#include <rtthread.h>
#include <rtdevice.h>
#define DBG_TAG "GSUSB"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>
#include "settings.h"
#include "gsusb.h"
#include "candump.h"
#include "canbus.h"
#include "canbus_event.h"
#include "canfilter.h"
#include "usb_desc.h"
#include "usb_cdc0.h"
#include "usb_serial_number.h"
#include "timestamp_us.h"

#define USB_RX_SIZE     (128) /* big enough for FD */
#define USB_TX_BUF_SIZE (512) /* USB HS max packet size */
#define TX_QUEUE_SIZE   (32)  /* queue up to 32 gsusb packets for canbus transmission */

static uint32_t usb_busid = 0;
static uint8_t  usb_rx_buf[USB_RX_SIZE] __aligned(4); // bulk from host to canbus
extern bool     gsusb_timestamp;

/*
 * ping-pong usb tx (in) buffers.
 * write_buf: frames are appended here by can_receive(), send_error_frame(), can_transmit_done()
 * send_buf:  currently being transmitted over usb
 * both are allocated at init. send_buf_len == 0 means usb is idle.
 * all access is from the gsusb thread (single-threaded).
 */

static uint8_t *write_buf     = NULL;
static uint8_t *send_buf      = NULL;
static uint32_t write_buf_len = 0;
static uint32_t send_buf_len  = 0; /* usb idle if 0 */

static bool                 tx_mailbox_valid[CAN_TX_NUM] = {false};
static gs_usb_host_frame_t  tx_gsusb_frame[CAN_TX_NUM];
uint32_t                    transmit_count   = 0; /* hardware confirms transmission success */
uint32_t                    transmit_errors  = 0; /* hardware confirms transmission fail */
uint32_t                    transmit_dropped = 0; /* dropped because ringbuffer full */
static struct rt_ringbuffer tx_rb;                /* queue of gsusb packets waiting to be transmitted */
static uint8_t             *tx_rb_buffer = NULL;  /* memory pool for tx_rb ringbuffer */

static bool previous_bus_off      = false;
static bool gsusb_tx_buf_overflow = false; /* write buffer was full; if true send error frame on next tx done */

/* next read from host */
static void inline gsusb_next_read()
{
    usbd_ep_start_read(usb_busid, CANBUS_OUT_EP, usb_rx_buf, sizeof(usb_rx_buf));
}

/*
 * gsusb_kick_tx - start usb transfer from send_buf. 
 * Caller must ensure send_buf_len > 0 XXX really?
 */
static void gsusb_kick_tx(void)
{
    usbd_ep_start_write(usb_busid, CANBUS_IN_EP, send_buf, send_buf_len);
}

/*
 * gsusb_append - append a gsusb frame to the write buffer.
 * If the write buffer has no room, the frame is dropped and overflow is signalled.
 * If USB is currently idle, swaps buffers and kicks TX immediately.
 * Called only from the gsusb thread.
 */
static void gsusb_append(const gs_usb_host_frame_timestamp_t *frame)
{
    uint32_t frame_size = gsusb_timestamp ? sizeof(gs_usb_host_frame_timestamp_t)
                                          : sizeof(gs_usb_host_frame_t);

    /* drop if no room */
    if (write_buf_len + frame_size > USB_TX_BUF_SIZE)
    {
        gsusb_tx_buf_overflow = true;
        return;
    }

    rt_memcpy(write_buf + write_buf_len, frame, frame_size);
    write_buf_len += frame_size;

    /* if usb is idle, swap and kick immediately */
    if (send_buf_len == 0)
    {
        uint8_t *tmp  = send_buf;
        send_buf      = write_buf;
        send_buf_len  = write_buf_len;
        write_buf     = tmp;
        write_buf_len = 0;
        gsusb_kick_tx();
    }
}

/* cherryusb usb callbacks */
void gsusb_bulk_out_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    /* signal usb bulk data available */
    if (nbytes == sizeof(struct gs_usb_host_frame) && serial_event != NULL)
        rt_event_send(serial_event, EVENT_MASK_GSUSB_BULK_OUT);
    else
        gsusb_next_read();
    return;
}

/* signal usb bulk data has been sent */
void gsusb_bulk_in_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    if ((nbytes % USB_TX_BUF_SIZE) == 0 && nbytes > 0)
    {
        /* exact multiple of max packet size: send ZLP to terminate transfer */
        usbd_ep_start_write(busid, CANBUS_IN_EP, NULL, 0);
    }
    else
    {
        /* transfer complete (sub-512 or ZLP acknowledgement) */
        if (serial_event)
            rt_event_send(serial_event, EVENT_MASK_GSUSB_TX_DONE);
    }
}

/*
   send error notification.
   can_err is an "or" of error flags CAN_ERR_CRTL_*
 */

void send_error_frame(uint8_t can_err)
{
    gs_usb_host_frame_timestamp_t err_frame = {0};
    bool                          current_bus_off;
    bool                          bus_off_error;

    current_bus_off  = can_flag_get(CAN1, CAN_BOF_FLAG);
    bus_off_error    = previous_bus_off != current_bus_off;
    previous_bus_off = current_bus_off;

    err_frame.echo_id      = 0xFFFFFFFF;
    err_frame.can_id       = CAN_ERR_FLAG;
    err_frame.can_dlc      = CAN_ERR_DLC;
    err_frame.data[0]      = CAN_ERR_LOSTARB_UNSPEC;
    err_frame.data[1]      = CAN_ERR_CRTL_UNSPEC;
    err_frame.timestamp_us = get_timestamp_us();

    if (bus_off_error)
        err_frame.can_id |= current_bus_off ? CAN_ERR_BUSOFF : CAN_ERR_RESTARTED;

    if (can_err)
    {
        err_frame.can_id  |= CAN_ERR_CRTL;
        err_frame.data[0] |= can_err;
    }

    err_frame.data[6] = can_transmit_error_counter_get(CAN1);
    err_frame.data[7] = can_receive_error_counter_get(CAN1);

    gsusb_append(&err_frame);
}

/* send a gsusb frame to the canbus */
static bool send_can_frame(const gs_usb_host_frame_t *gsusb_frame)
{
    can_tx_message_type hal_frame = {0};

    /* convert gsusb frame to hal */
    if (gsusb_frame->can_id & CAN_EFF_FLAG)
    {
        hal_frame.id_type     = CAN_ID_EXTENDED;
        hal_frame.extended_id = gsusb_frame->can_id & 0x1FFFFFFFU;
    }
    else
    {
        hal_frame.id_type     = CAN_ID_STANDARD;
        hal_frame.standard_id = gsusb_frame->can_id & 0x7FF;
    }

    hal_frame.frame_type = (gsusb_frame->can_id & CAN_RTR_FLAG) ? CAN_TFT_REMOTE : CAN_TFT_DATA;

    if (gsusb_frame->can_dlc > 8)
        return false;
    hal_frame.dlc = gsusb_frame->can_dlc;

    for (uint32_t i = 0; i < gsusb_frame->can_dlc; i++)
        hal_frame.data[i] = gsusb_frame->data[i];

    /* try to transmit */
    uint8_t retval = can_message_transmit(CAN1, &hal_frame);

    switch (retval)
    {
    case CAN_TX_MAILBOX0:
    case CAN_TX_MAILBOX1:
    case CAN_TX_MAILBOX2:
        /* Frame accepted by hardware - store for echo */
        memcpy(&tx_gsusb_frame[retval], gsusb_frame, sizeof(*gsusb_frame));
        tx_mailbox_valid[retval] = true;
        return true; /* sent */

    case CAN_TX_STATUS_NO_EMPTY:
        /* no mailbox available - queue for later */
        if (rt_ringbuffer_put(&tx_rb,
                              (uint8_t *)gsusb_frame,
                              sizeof(*gsusb_frame))
            == 0)
        {
            transmit_dropped++;
            if (serial_event)
                rt_event_send(serial_event, EVENT_MASK_CAN1_TX_OVERFLOW);
            return false; /* dropped */
        }
        return false;     /* queued, not dropped */

    default:
        LOG_E("unexpected can_message_transmit() : %d", retval);
        return false; /* unexpected return value */
    }
}

/* gsusb usb frame received. send canbus frame */
void gsusb_receive()
{
    struct gs_usb_host_frame *gsusb_frame = (struct gs_usb_host_frame *)usb_rx_buf;

    /* send gsusb frame to canbus */
    if (gsusb_frame->can_dlc <= 8)
        send_can_frame(gsusb_frame);

    /* schedule next usb read */
    gsusb_next_read();
}

/* canbus transmit finished, send echo packet */
void can_transmit_done()
{
    can_tx_state_t                tx_state;
    gs_usb_host_frame_timestamp_t gsusb_frame;
    gs_usb_host_frame_t           queued_frame;

    if (can_tx_get_state(&tx_state) != RT_EOK)
    {
        LOG_E("no tx state?");
        return;
    }

    for (uint32_t i = 0; i < CAN_TX_NUM; i++)
    {
        if ((tx_state.status[i] == CAN_TX_STATUS_FAILED)
            || (tx_state.status[i] == CAN_TX_STATUS_SUCCESSFUL))
        {
            if (tx_mailbox_valid[i])
            {
                /* transmission completed, send echo frame to host */
                memcpy(&gsusb_frame, &tx_gsusb_frame[i], sizeof(struct gs_usb_host_frame));
                gsusb_frame.timestamp_us = tx_state.timestamp_us[i];
                gsusb_append(&gsusb_frame);

                if (tx_state.status[i] == CAN_TX_STATUS_SUCCESSFUL)
                    transmit_count++;
                else
                    transmit_errors++;

                tx_mailbox_valid[i] = false;
            }
        }
    }
    /* drain queued frames into available mailboxes */
    while (rt_ringbuffer_get(&tx_rb,
                             (uint8_t *)&queued_frame,
                             sizeof(queued_frame))
           == sizeof(queued_frame))
    {
        /* queue had a frame - try to send it */
        if (!send_can_frame(&queued_frame))
            break;
    }
}

void can_receive()
{
    can_stored_frame_t            rx_frame;
    can_rx_result_t               rx_result;
    gs_usb_host_frame_timestamp_t gsusb_frame;
    bool                          packet_logged = false;

    while ((rx_result = can_rx_get(&rx_frame)) != CAN_RX_EMPTY)
    {
        /* skip if already consumed elsewhere */
        if (rx_result == CAN_RX_SKIP)
            continue;

        /* if can1_log enabled, log received canbus packets to cdc0 in candump format */
        if (settings.can1_log)
        {
            candump_frame(&rx_frame);
            packet_logged = true;
        }

        memset(&gsusb_frame, 0, sizeof(gsusb_frame));
        gsusb_frame.echo_id = 0xFFFFFFFF; /* mark as received frame */

        if (rx_frame.frame.id_type == CAN_ID_STANDARD)
            gsusb_frame.can_id = rx_frame.frame.standard_id;
        else
            gsusb_frame.can_id = rx_frame.frame.extended_id | CAN_EFF_FLAG;

        if (rx_frame.frame.frame_type == CAN_TFT_REMOTE)
            gsusb_frame.can_id |= CAN_RTR_FLAG;


        gsusb_frame.can_dlc = rx_frame.frame.dlc;

        for (uint32_t i = 0; i < gsusb_frame.can_dlc; i++)
            gsusb_frame.data[i] = rx_frame.frame.data[i];

        gsusb_frame.timestamp_us = rx_frame.timestamp_us;

        gsusb_append(&gsusb_frame);
    }
    if (packet_logged)
        cdc0_flush();
}


/* called by cherryusb when gsusb usb packet has been sent to host */
static void gsusb_tx_done(void)
{
    /* send buffer has been fully transmitted (including any ZLP) */
    send_buf_len = 0;

    /* report any write buffer overflow that occurred since last tx done */
    if (gsusb_tx_buf_overflow)
    {
        gsusb_tx_buf_overflow = false;
        send_error_frame(CAN_ERR_CRTL_RX_OVERFLOW);
    }

    /* if write buffer has accumulated frames, swap and send */
    if (write_buf_len > 0)
    {
        uint8_t *tmp  = send_buf;
        send_buf      = write_buf;
        send_buf_len  = write_buf_len;
        write_buf     = tmp;
        write_buf_len = 0;
        gsusb_kick_tx();
    }
}

/* main GSUSB thread */
static void gsusb_thread(void *arg)
{
    uint32_t recv_set;

    if (serial_event == NULL)
    {
        LOG_E("null serial_event");
        return;
    }
    LOG_I("waiting");

    while (1)
    {
        /* wait for USB or CAN event */
        if (rt_event_recv(serial_event,
                          EVENT_MASK_CAN1_TX_DONE
                              | EVENT_MASK_CAN1_RX0_INDIC
                              | EVENT_MASK_CAN1_BUS_OFF
                              | EVENT_MASK_CAN1_RX_OVERFLOW
                              | EVENT_MASK_CAN1_TX_OVERFLOW
                              | EVENT_MASK_GSUSB_BULK_OUT
                              | EVENT_MASK_GSUSB_STOP
                              | EVENT_MASK_GSUSB_START
                              | EVENT_MASK_GSUSB_TX_DONE,
                          RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR,
                          RT_WAITING_FOREVER,
                          &recv_set)
            == RT_EOK)
        {
            if (recv_set & EVENT_MASK_GSUSB_STOP)
            {
                LOG_D("EVENT_GSUSB_STOP");
                gsusb_stop();
            }

            if (recv_set & EVENT_MASK_GSUSB_START)
            {
                LOG_D("EVENT_GSUSB_START");
                gsusb_start();
            }

            if (recv_set & EVENT_MASK_CAN1_RX0_INDIC)
            {
                LOG_D("EVENT_CAN1_RX0_INDIC");
                can_receive();
            }

            if (recv_set & EVENT_MASK_CAN1_TX_DONE)
            {
                LOG_D("EVENT_CAN1_TX_DONE");
                can_transmit_done();
            }

            if (recv_set & EVENT_MASK_GSUSB_BULK_OUT)
            {
                LOG_D("EVENT_GSUSB_BULK_OUT");
                gsusb_receive();
            }

            if (recv_set & EVENT_MASK_GSUSB_TX_DONE)
            {
                LOG_D("EVENT_GSUSB_TX_DONE");
                gsusb_tx_done();
            }

            /* error notifications */
            if (recv_set & EVENT_MASK_CAN1_BUS_OFF)
            {
                LOG_D("EVENT_CAN1_BUS_OFF");
                send_error_frame(CAN_ERR_CRTL_UNSPEC); /* send bus off notification */
            }

            if (recv_set & EVENT_MASK_CAN1_RX_OVERFLOW)
            {
                LOG_D("EVENT_CAN1_RX_OVERFLOW");
                send_error_frame(CAN_ERR_CRTL_RX_OVERFLOW);
            }

            if (recv_set & EVENT_MASK_CAN1_TX_OVERFLOW)
            {
                LOG_D("EVENT_CAN1_TX_OVERFLOW");
                send_error_frame(CAN_ERR_CRTL_TX_OVERFLOW);
            }
        }
    }
}

/* USB configured callback */
void gsusb_on_configured(uint8_t busid)
{
    usb_busid = busid;

    if (settings.mode != MODE_GDB_SERVER)
    {
        LOG_D("gsusb disabled - not in mode GDB_SERVER");
        return;
    }

    if (write_buf != NULL)
        return; /* re-enumeration */

    /* allocate ping-pong TX buffers */
    if (write_buf == NULL)
        write_buf = rt_malloc(USB_TX_BUF_SIZE);
    if (send_buf == NULL)
        send_buf = rt_malloc(USB_TX_BUF_SIZE);
    if (write_buf == NULL || send_buf == NULL)
    {
        LOG_E("ping-pong buffer malloc fail");
        rt_free(write_buf);
        rt_free(send_buf);
        write_buf = send_buf = NULL;
        return;
    }
    write_buf_len = 0;
    send_buf_len  = 0;

    /* configure canbus device */
    if (can_init(true) != RT_EOK)
    {
        LOG_E("canbus init fail");
        return;
    }

    /* allocate ringbuffer for canbus tx queue */
    tx_rb_buffer = rt_malloc(TX_QUEUE_SIZE * sizeof(gs_usb_host_frame_t));
    if (tx_rb_buffer == NULL)
    {
        LOG_E("malloc canbus tx ringbuffer fail");
        return;
    }
    rt_ringbuffer_init(&tx_rb, tx_rb_buffer, TX_QUEUE_SIZE * sizeof(gs_usb_host_frame_t));

    /* events, can and usb configured - start gsusb thread */
    rt_thread_t thread = rt_thread_create("gsusb",
                                          gsusb_thread,
                                          NULL,
                                          2048,
                                          15,
                                          10);
    if (!thread)
    {
        LOG_E("thread fail");
        return;
    }
    rt_thread_startup(thread);

    /* first read from host */
    gsusb_next_read();

    LOG_I("init");

    return;
}
