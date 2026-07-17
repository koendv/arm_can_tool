/* cdc0: tuned for throughput */

#include <rtthread.h>
#define DBG_TAG "CDC0"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#include "usb_cdc0.h"
#include "usb_desc.h"
#include "usbd_cdc_acm.h"
#include "serials.h"
#include "settings.h"
#include "logger.h"

void cdc0_start_read(void);

/* direction in: from device to host */

/* cdc throughput determined by size of this buffer */
#define CDC0_MPS (HS_PACKET_SIZE - 1)

typedef struct
{
    uint8_t  data[CDC0_MPS]; /* only 511 bytes: no zero-length packets needed */
    uint32_t len;            /* number bytes valid in data[]  */
} cdc0_in_buf_t;

/* word-align for DMA */
static cdc0_in_buf_t cdc0_in_buf[2] __attribute__((aligned(4)));
static rt_mutex_t    cdc0_buffer_mutex = RT_NULL; /* avoid two threads writing buffer at same time */

static rt_sem_t         cdc0_tx_sem    = RT_NULL; /* released when transmit idle */
static volatile uint8_t cdc0_write_idx = 0;       /* buffer being filled by thread  */
static volatile uint8_t cdc0_send_idx  = 0;       /* buffer being sent by usb */

/* direction out: from host to device */

uint8_t  cdc0_out_buf[HS_PACKET_SIZE] __attribute__((aligned(4))); /* full 512 bytes/packet */
uint32_t cdc0_out_len = 0;


static struct usbd_interface cdc0_ctrl_intf; /* cdc0 control interface     */
static struct usbd_interface cdc0_data_intf; /* cdc0 data interface        */

static void cdc0_bulk_in_cb(uint8_t busid, uint8_t ep, uint32_t nbytes);
static void cdc0_bulk_out_cb(uint8_t busid, uint8_t ep, uint32_t nbytes);

static struct usbd_endpoint cdc0_in_ep = {
    .ep_addr = CDC0_IN_EP,
    .ep_cb   = cdc0_bulk_in_cb,
};

static struct usbd_endpoint cdc0_out_ep = {
    .ep_addr = CDC0_OUT_EP,
    .ep_cb   = cdc0_bulk_out_cb,
};

void cdc0_init(uint8_t busid)
{
    /* reset buffer state */
    cdc0_in_buf[0].len = 0;
    cdc0_in_buf[1].len = 0;
    cdc0_write_idx     = 0;
    cdc0_send_idx      = 0;

    /* set up usb transmit semaphore */
    cdc0_tx_sem = rt_sem_create("cdc0_tx", 1, RT_IPC_FLAG_FIFO);
    if (cdc0_tx_sem == RT_NULL)
    {
        LOG_E("cdc0_tx_sem create fail");
    }
    rt_sem_control(cdc0_tx_sem, RT_IPC_CMD_SET_VLIMIT, (void *)1);

    /* set up transmit buffer mutex */
    cdc0_buffer_mutex = rt_mutex_create("cdc0_in", RT_IPC_FLAG_FIFO);
    if (cdc0_buffer_mutex == RT_NULL)
    {
        LOG_E("cdc0_buffer_mutex create fail");
    }

    /* register CDC ACM interfaces (control + data) */
    usbd_add_interface(busid, usbd_cdc_acm_init_intf(busid, &cdc0_ctrl_intf));
    usbd_add_interface(busid, usbd_cdc_acm_init_intf(busid, &cdc0_data_intf));

    /* register bulk endpoints with our own callbacks */
    usbd_add_endpoint(busid, &cdc0_in_ep);
    usbd_add_endpoint(busid, &cdc0_out_ep);
}

void cdc0_on_configured(uint8_t busid)
{
    (void)busid;

    cdc0_in_buf[0].len = 0;
    cdc0_in_buf[1].len = 0;
    cdc0_write_idx     = 0;
    cdc0_send_idx      = 0;

    if (cdc0_tx_sem)
    {
        rt_sem_release(cdc0_tx_sem); /* VLIMIT=1 prevents overflow */
    }

    cdc0_start_read();
}

static void cdc0_start_send(uint8_t idx)
{
    cdc0_in_buf_t *sb = &cdc0_in_buf[idx];
    usbd_ep_start_write(BUSID0, CDC0_IN_EP, sb->data, sb->len);
}

static void cdc0_buffer_flush()
{
    if (!cdc0_tx_sem)
    {
        LOG_E("no cdc0_tx_sem"); /* init failed? */
        return;
    }

    if (!cdc0_dtr)
        return; /* no connection, don't send */

    if (cdc0_in_buf[cdc0_write_idx].len == 0)
        return; /* no data, don't send */

    if (rt_sem_take(cdc0_tx_sem, RT_TICK_PER_SECOND) != RT_EOK)
    {
        /* should never happen — bail out */
        LOG_E("cdc0 tx timeout, discarding");
        cdc0_in_buf[cdc0_write_idx].len = 0;
        return;
    }

    /* flip buffers */
    cdc0_in_buf_t *wb = &cdc0_in_buf[cdc0_write_idx];
    cdc0_send_idx     = cdc0_write_idx;
    cdc0_write_idx    = 1 - cdc0_send_idx;

    /* reset new write buffer */
    cdc0_in_buf[cdc0_write_idx].len = 0;

    /* send old write buffer, releases cdc0_tx_sem when write done */
    cdc0_start_send(cdc0_send_idx);
}

void cdc0_buffer_write(const char *buf, size_t buf_len, bool flush)
{
    if (settings.logging_enable)
        logger(buf, buf_len);

    if (!cdc0_dtr)
        return; /* no connection, don't send */

    if (rt_mutex_take(cdc0_buffer_mutex, RT_TICK_PER_SECOND) != RT_EOK)
    {
        /* should never happen - log and continue */
        LOG_E("cdc0_buffer_mutex timeout");
    }

    while (buf_len > 0)
    {
        cdc0_in_buf_t *wb    = &cdc0_in_buf[cdc0_write_idx];
        uint16_t       space = CDC0_MPS - wb->len;
        size_t         len   = buf_len;
        if (len > space) len = space;
        memcpy(wb->data + wb->len, buf, len);
        wb->len += len;
        buf     += len;
        buf_len -= len;
        if (wb->len == CDC0_MPS)
            cdc0_buffer_flush();
    }

    if (flush)
        cdc0_buffer_flush();

    rt_mutex_release(cdc0_buffer_mutex);
}

void cdc0_write(const char *buf, const size_t len)
{
    cdc0_buffer_write(buf, len, false);
}

void cdc0_flush(void)
{
    cdc0_buffer_write(RT_NULL, 0, true);
}

/* tx-complete callback (usb isr) */

static void cdc0_bulk_in_cb(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    if ((nbytes % usbd_get_ep_mps(busid, ep)) == 0 && nbytes)
    {
        /* send zlp */
        usbd_ep_start_write(busid, CDC0_IN_EP, NULL, 0);
    }
    else if (cdc0_tx_sem)
    {
        rt_sem_release(cdc0_tx_sem);
    }
}

void cdc0_start_read(void)
{
    usbd_ep_start_read(BUSID0, CDC0_OUT_EP, cdc0_out_buf, sizeof(cdc0_out_buf));
}

/* rx-indicate callback */
static void cdc0_bulk_out_cb(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)busid;
    (void)ep;
    (void)nbytes;

    cdc0_out_len = nbytes;
    if (serial_event && nbytes > 0)
        /* caller must call cdc0_start_read() after consuming the buffer */
        rt_event_send(serial_event, EVENT_MASK_CDC0_RX);
    else
        cdc0_start_read();
}
