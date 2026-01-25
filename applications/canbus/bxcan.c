//#include <rtthread.h>
#include <limits.h>
//#include <string.h>
#include <usbd_core.h>
#include <at32f402_405_can.h>
#include <at32_msp.h>
#include "bxcan.h"
#include "timestamp_us.h"
//#include "usb_gsusb.h"
#include "usb_desc.h"

#ifdef DBG_TAG
#undef DBG_TAG
#undef DBG_LVL
#endif

#define DBG_TAG "BXCAN"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#define PRIORITY_HIGH   20
#define PRIORITY_MEDIUM 25
#define PRIORITY_LOW    30

#define USB_TX_SIZE (24)                              /* classic can with timestamp */
#define USB_RX_SIZE (128)                             /* big enough for FD */

#define BXCAN_RX_MQ_SIZE  32                          /* receive message queue size */
#define BXCAN_FILTER_MASK (0x3FFFU)                   /* 14 filter banks */

#define BXCAN_DEV           "can1"                    /* rt-thread canbus device name */
#define BXCAN_DEFAULT_SPEED 500000                    /* default canbus bit rate */

static can_type * const bxcan     = (can_type *)CAN1; /* at32 HAL: canbus controller memory address */
static rt_device_t      bxcan_dev = RT_NULL;          /* rt-thread: canbus controller device */

/* usb interface */
static uint32_t usb_busid   = 0;
static bool     usb_enabled = false;     // gsusb connection started
static uint32_t usb_errors  = 0;         // count of usb errors; reset usb of too large
static uint8_t  usb_rx_buf[USB_RX_SIZE] __aligned(4); // bulk from host to canbus
static uint8_t  usb_tx_buf[USB_TX_SIZE] __aligned(4); // bulk from canbus to host
static rt_sem_t usb_tx_sem = RT_NULL;    // semaphore released when usb transmit complete
static rt_sem_t usb_rx_sem = RT_NULL;    // semaphore released when usb receive complete

/* external interface with usb_gsusb.c and settings.c */
static uint32_t bxcan_mode      = RT_CAN_MODE_NORMAL;
static bool     bxcan_timestamp = false; /* true if timestamps enabled */
bxcan_filter_t  bxcan_filter    = {
        .fs1r  = 0x1,                    // 32-bit for filter bank 0
        .fm1r  = 0x0,                    // Mask mode for filter 0
        .ffa1r = 0x0,                    // Assign to FIFO 0
        .fa1r  = 0x1,                    // Enable filter bank 0
};
static rt_sem_t bxcan_rx_sem = RT_NULL;  // semaphore released when canbus packet received
static rt_mq_t  bxcan_rx_mq  = RT_NULL;  // receives CAN RX frames, TX echo frames, monitor error frames

/* error monitoring */
static volatile bool rx_over_error  = false;
static volatile bool tx_abort_error = false;
static volatile bool bus_off_error  = false;


static inline void LOG_FRAME(char *msg, struct gs_usb_host_frame *frame)
{
    if (frame)
        LOG_I("%s echo_id: 0x%08x can_id: 0x%08x len: %d chan: %d %s%s data: 0x%02x%02x%02x%02x%02x%02x%02x%02x", msg, frame->echo_id, frame->can_id, frame->can_dlc, frame->channel, (frame->can_id & CAN_EFF_FLAG) ? " EXT" : "", (frame->can_id & CAN_RTR_FLAG) ? " RTR" : "", frame->data[0], frame->data[1], frame->data[2], frame->data[3], frame->data[4], frame->data[5], frame->data[6], frame->data[7]);
    else
        LOG_E("null frame");
}

rt_err_t bxcan_reset()
{
    LOG_I("reset");
    //can_reset(bxcan);
    return RT_EOK;
}

rt_err_t bxcan_disable()
{
    LOG_I("disable");
    if (rt_device_close(bxcan_dev) != RT_EOK)
    {
        LOG_E("close device %s failed!", BXCAN_DEV);
        return -RT_ERROR;
    }

#if 0
    rt_device_control(bxcan_dev, RT_DEVICE_CTRL_SUSPEND, RT_NULL);
    can_doze_mode_enter(bxcan);
#endif
    usb_enabled = false;
    return RT_EOK;
}

rt_err_t bxcan_enable()
{
    LOG_I("enable");
#if 0
    can_doze_mode_exit(bxcan);
    rt_device_control(bxcan_dev, RT_DEVICE_CTRL_RESUME, RT_NULL);
#endif
    if (rt_device_open(bxcan_dev, RT_DEVICE_FLAG_INT_TX | RT_DEVICE_FLAG_INT_RX) != RT_EOK)
    {
        LOG_E("open device %s failed!", BXCAN_DEV);
        return -RT_ERROR;
    }

    usb_enabled = true;
    return RT_EOK;
}

rt_err_t bxcan_set_speed(uint32_t new_speed)
{
    LOG_I("speed %d", new_speed);
    if (rt_device_control(bxcan_dev, RT_CAN_CMD_SET_BAUD, (void *)new_speed) != RT_EOK)
    {
        LOG_E("set speed %d failed!", new_speed);
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t bxcan_set_bittiming(struct gs_device_bittiming bittiming)
{
    can_baudrate_type new_baudrate;

    /* calculate speed */
    const uint8_t tseg1 = bittiming.prop_seg + bittiming.phase_seg1;
    const uint8_t tseg2 = bittiming.phase_seg2;
    uint32_t      speed = BXCAN_CLOCK_SPEED / (bittiming.brp * (1 + tseg1 + tseg2));
    speed               = ((speed + 500) / 1000) * 1000; // rounding
    LOG_I("can speed %d", speed);

    /* set new bit timing */
    can_baudrate_default_para_init(&new_baudrate);
    new_baudrate.baudrate_div = bittiming.brp;
    new_baudrate.rsaw_size    = (can_rsaw_type)(bittiming.sjw - 1);
    new_baudrate.bts1_size    = (can_bts1_type)(tseg1 - 1);
    new_baudrate.bts2_size    = (can_bts2_type)(tseg2 - 1);

    error_status status = can_baudrate_set(bxcan, &new_baudrate);

    return (status == SUCCESS ? RT_EOK : -RT_ERROR);
}

rt_err_t bxcan_set_timestamp(bool new_timestamp)
{
    bxcan_timestamp = new_timestamp;
    return RT_EOK;
}

rt_err_t bxcan_set_mode(uint32_t new_mode)
{
    LOG_I("mode");
    if ((new_mode != RT_CAN_MODE_NORMAL) && (new_mode != RT_CAN_MODE_LISTEN) && (new_mode != RT_CAN_MODE_LOOPBACK))
        return -RT_ERROR;
    if (rt_device_control(bxcan_dev, RT_CAN_CMD_SET_MODE, (void *)new_mode) != RT_EOK)
    {
        LOG_E("set mode %d failed!", new_mode);
        return -RT_ERROR;
    }

    return RT_EOK;
}

rt_err_t bxcan_set_filter(bxcan_filter_t bxcan_filter)
{
    /* bxcan filter configuration process */
    if (!bxcan)
        return -RT_ERROR;

    // Enter filter initialization mode
    // stm32: FMR at32: fctrl
    bxcan->fctrl |= 0x1U;

    // Disable Filters
    // stm32: FA1R at32: facfg
    bxcan->facfg = 0x0U;

    // Configure filter scale (32-bit vs 16-bit)
    // stm32: FS1R at32: fbwcfg
    bxcan->fbwcfg = bxcan_filter.fs1r & BXCAN_FILTER_MASK;

    // Configure filter mode (mask vs list)
    // stm32: FM1R at32: fmcfg
    bxcan->fmcfg = bxcan_filter.fm1r & BXCAN_FILTER_MASK;

    // Configure filter FIFO assignment
    // stm32: FFA1R at32: frf
    bxcan->frf = bxcan_filter.ffa1r & BXCAN_FILTER_MASK;

    // Configure filter bank registers
    // stm32: FR1, FR2 at32: ffdb1, ffdb2
    for (uint32_t bank = 0; bank < 14; bank++)
    {
        bxcan->ffb[bank].ffdb1 = bxcan_filter.fr1[bank];
        bxcan->ffb[bank].ffdb2 = bxcan_filter.fr2[bank];
    }

    // Activate filters
    // stm32: FA1R at32: facfg
    bxcan->facfg = bxcan_filter.fa1r & BXCAN_FILTER_MASK;

    // Exit filter initialization mode
    bxcan->fctrl &= ~0x1U; // Clear bit 0 (FCS/FINIT equivalent)

    return RT_EOK;
}

/* usb bulk connection */

/* next read from host */
static void inline gsusb_next_read()
{
    usbd_ep_start_read(usb_busid, GS_USB_OUT_EP, usb_rx_buf, sizeof(usb_rx_buf));
}

void gsusb_configured(uint8_t busid)
{
    LOG_I("gsusb_configured");
    usb_busid   = busid;
    usb_enabled = false;
    /* first read from host */
    gsusb_next_read();
}

void gsusb_out_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    if (nbytes == sizeof(struct gs_usb_host_frame))
    {
        // LOG_FRAME("gsusb_out_callback", (struct gs_usb_host_frame *)&usb_rx_buf);
        if (usb_enabled && usb_rx_sem)
        {
            rt_sem_release(usb_rx_sem);
            LOG_I("usb_rx_sem released");
        }
    }
    gsusb_next_read(); /* next read from host */
}

void gsusb_in_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    LOG_I("usb write end");
    if (usb_tx_sem)
        rt_sem_release(usb_tx_sem);
    else
        LOG_E("no usb_tx_sem");
}

/* canbus callbacks */

/* receive callback */
static rt_err_t bxcan_rx_callback(rt_device_t dev, rt_size_t size)
{
    LOG_I("canbus receive");
    if (bxcan_rx_sem)
        rt_sem_release(bxcan_rx_sem);
    else
        LOG_E("no bxcan_rx_sem");
    return RT_EOK;
}

/**
 * @brief Process ALL available CAN frames in one call
 * @param can AT32 CAN peripheral base address
 * @return Number of frames processed
 */
static inline uint32_t bxcan_receive_all(can_type *can)
{
    uint32_t       frames_processed = 0;
    const uint32_t max_to_process   = 3;

    static struct gs_usb_host_frame_timestamp rx_frame;

    if (!bxcan_dev || !bxcan_rx_mq)
    {
        LOG_E("no bxcan_dev or no bxcan_rx_mq");
        return 0;
    }

    // Process all messages
    while ((can->rf0 & 0x03U) != 0 && frames_processed < max_to_process)
    {
        can_fifo_mailbox_type *fifo = &can->fifo_mailbox[0];

        uint32_t rfi   = fifo->rfi;
        uint32_t rfc   = fifo->rfc;
        uint32_t rfdtl = fifo->rfdtl;
        uint32_t rfdth = fifo->rfdth;

        // Fill frame efficiently
        rx_frame.echo_id = -1;

        // CAN ID
        uint32_t can_id;
        if (rfi & (1U << 2))
        {
            can_id = CAN_EFF_FLAG | ((rfi >> 3) & 0x1FFFFFFFU); // extended id
        }
        else
        {
            can_id = (rfi >> 21) & 0x7FFU; // standard id
        }
        if (rfi & (1U << 1))
        {
            can_id |= CAN_RTR_FLAG; // rtr packet
        }
        rx_frame.can_id = can_id;

        rx_frame.can_dlc  = rfc & 0x0FU;
        rx_frame.channel  = 0;
        rx_frame.flags    = 0;
        rx_frame.reserved = 0;

        // Copy data
        uint32_t *data_32 = (uint32_t *)rx_frame.data;
        data_32[0]        = rfdtl;
        data_32[1]        = rfdth;

        // Timestamp with microsecond resolution
        rx_frame.timestamp_us = get_timestamp_us();

        // Release FIFO
        can->rf0 |= CAN_RF0_RF0R_VAL;

        // push frame to message queue
        LOG_FRAME("bxcan_receive_all", (struct gs_usb_host_frame *)&rx_frame);
        if (bxcan_rx_mq && rt_mq_send(bxcan_rx_mq, &rx_frame, sizeof(rx_frame)) != RT_EOK)
            rx_over_error = true;

        frames_processed++;
    }

    return frames_processed;
}

static void bxcan_rx_thread(void *parameter)
{
    /* create bxcan receive indicate semaphore */
    bxcan_rx_sem = rt_sem_create("bxcan_rx", 0, RT_IPC_FLAG_FIFO);

    while (1)
    {
        rt_sem_take(bxcan_rx_sem, RT_WAITING_FOREVER);
        uint32_t recvd = bxcan_receive_all(bxcan);
        LOG_I("received (%d)", recvd);
    }
}

/* bxcan transmit thread: send frame from usb to can*/
static void bxcan_tx_thread(void *param)
{
    static struct gs_usb_host_frame           tx_frame;
    static struct gs_usb_host_frame_timestamp echo_frame;
    struct rt_can_msg                         tx_msg = {0};
    rt_size_t                                 written;

    /* create usb receive indicate semaphore */
    usb_rx_sem = rt_sem_create("gsusb_rx", 0, RT_IPC_FLAG_FIFO);

    if (!usb_rx_sem)
    {
        LOG_E("no usb receive semaphore");
        return;
    }

    if (!bxcan_dev)
    {
        LOG_E("no bxcan device");
        return;
    }

    while (1)
    {
        LOG_I("bxcan_tx_thread rt_sem_take");

        if (rt_sem_take(usb_rx_sem, RT_WAITING_FOREVER) != RT_EOK)
        {
            //rt_thread_yield();
            LOG_I("bxcan_tx_thread mdelay");
            rt_thread_mdelay(250);
            continue;
        }

        LOG_I("bxcan_tx_thread memcpy");
        memcpy(&tx_frame, usb_rx_buf, sizeof(struct gs_usb_host_frame));
        LOG_FRAME("bxcan_tx_thread", &tx_frame);

        ulog_flush();

        memset(&tx_msg, 0, sizeof(tx_msg));
        tx_msg.ide = (tx_frame.can_id & CAN_EFF_FLAG) ? RT_CAN_EXTID : RT_CAN_STDID;
        tx_msg.rtr = (tx_frame.can_id & CAN_RTR_FLAG) ? RT_CAN_RTR : RT_CAN_DTR;
        if (tx_frame.can_id & CAN_EFF_FLAG)
        tx_msg.id  = tx_frame.can_id & 0x1FFFFFFFU;
        else
        tx_msg.id  = tx_frame.can_id & 0x7FFU;
        tx_msg.len = tx_frame.can_dlc;
        for (uint32_t i = 0; i < sizeof(tx_frame.data); i++)
            tx_msg.data[i] = tx_frame.data[i];
        tx_msg.nonblocking = 1;

        LOG_I("bxcan_tx_thread test corrupt");
        if (((tx_msg.ide == RT_CAN_EXTID) && (tx_msg.id & 0x1FFFFFFFU != tx_msg.id))
            || ((tx_msg.ide == RT_CAN_STDID) && (tx_msg.id & 0x7FFU != tx_msg.id))
            || (tx_msg.len > 8))
        {
            LOG_I("corrupt packet");
            continue;
        }

        LOG_I("packet tx try");
#if 0
        written = rt_device_write(bxcan_dev, 0, &tx_msg, sizeof(struct rt_can_msg));
        if (written != sizeof(struct rt_can_msg))
        {
            tx_abort_error = true;
            LOG_E("packet tx fail (%d)", written);
            continue;
        }
#endif

        /* successful canbus write - now send a copy of the transmitted packet back to the sender */
        LOG_I("send echo packet");
        memcpy(&echo_frame, &tx_frame, sizeof(tx_frame));
        echo_frame.timestamp_us = get_timestamp_us();
        if (rt_mq_send(bxcan_rx_mq, &echo_frame, sizeof(struct gs_usb_host_frame_timestamp)) != RT_EOK)
        {
            LOG_E("echo packet send fail");
            rx_over_error = true;
            continue;
        }

        LOG_I("packet tx success");
    }
    LOG_E("bxcan_tx_thread exit");
}

// lightweight bus monitoring thread

static void bxcan_mon_thread(void *parameter)
{
    static bool busoff; // remember bus state
    bool        old_busoff;
    uint32_t    esr;
    uint8_t     tec, rec;

    static struct gs_usb_host_frame_timestamp err_frame;

    while (1)
    {
        rt_thread_mdelay(317); // polling three times per second, adjusted to be a prime number

        // read ESR Error Status Register
        // AT32: ests register instead of ESR
        esr = bxcan->ests;

        // determine current bus-off state
        // In AT32, BOF flag is bit 2 of ests register
        old_busoff = busoff;
        busoff     = (esr & (0x1U << 2)) != 0; // Check bit 2 for bus-off

        // if bus state changed
        if (busoff != old_busoff)
            bus_off_error = true;

        if (!(rx_over_error || tx_abort_error || bus_off_error))
            continue;

        /* send error frame */

        memset(&err_frame, 0, sizeof(err_frame));

        // fill CAN error frame
        err_frame.can_id = CAN_ERR_FLAG;
        if (bus_off_error)
            err_frame.can_id |= (busoff ? CAN_ERR_BUSOFF : CAN_ERR_RESTARTED);
        if (rx_over_error)
        {
            err_frame.can_id  |= CAN_ERR_CRTL;
            err_frame.data[0] |= CAN_ERR_CRTL_RX_OVERFLOW;
        }
        if (tx_abort_error)
        {
            err_frame.can_id  |= CAN_ERR_CRTL;
            err_frame.data[0] |= CAN_ERR_CRTL_TX_OVERFLOW;
        }
        err_frame.can_dlc = CAN_ERR_DLC;

        // Transmission Error Counter / Reception Error Counter
        // AT32: TEC is bits 23:16, REC is bits 31:24
        // Using same extraction method as at32 drv_can.c
        tec = (esr >> 16) & 0xFF;
        rec = (esr >> 24) & 0xFF;

        // fill frame data
        err_frame.data[6] = tec;
        err_frame.data[7] = rec;

        // set timestamp
        err_frame.timestamp_us = get_timestamp_us();

        // send frame over usb
        if (rt_mq_send(bxcan_rx_mq, &err_frame, sizeof(err_frame)) == RT_EOK)
        {
            /* error frame sent, clear error flags */
            bus_off_error  = false;
            rx_over_error  = false;
            tx_abort_error = false;
        }
    }
}

rt_err_t bxcan_init()
{
    rt_thread_t thread = NULL;

    RT_ASSERT(USB_TX_SIZE == sizeof(struct gs_usb_host_frame_timestamp));

    /* set up device */
    bxcan_dev = rt_device_find(BXCAN_DEV);
    if (!bxcan_dev)
    {
        LOG_E("device %s not found!", BXCAN_DEV);
        return -RT_ERROR;
    }

    /* create usb transmit complete semaphore */
    usb_tx_sem = rt_sem_create("gsusb_tx", 0, RT_IPC_FLAG_FIFO);

    /* create bxcan receive message queue */
    bxcan_rx_mq = rt_mq_create("bxcan_rx", sizeof(struct gs_usb_host_frame_timestamp), BXCAN_RX_MQ_SIZE, RT_IPC_FLAG_FIFO);
    if (bxcan_rx_mq == RT_NULL)
    {
        LOG_E("create receive message queue failed!");
        return -RT_ERROR;
    }

    /* set receive callback */
    if (rt_device_set_rx_indicate(bxcan_dev, bxcan_rx_callback) != RT_EOK)
    {
        LOG_E("%s canbus rx indicate fail", BXCAN_DEV);
        return -RT_ERROR;
    }

    /* start bxcan receive thread */
    thread = rt_thread_create("can_rx", bxcan_rx_thread, RT_NULL, 1024, PRIORITY_MEDIUM, 10);
    if (thread == RT_NULL)
    {
        LOG_E("create bxcan receive thread fail");
        return -RT_ERROR;
    }
    rt_thread_startup(thread);

    /* start bxcan transmit thread */
    thread = rt_thread_create("can_tx", bxcan_tx_thread, RT_NULL, 2048 /*XXX*/, PRIORITY_HIGH, 10);
    if (thread == RT_NULL)
    {
        LOG_E("create bxcan transmit thread fail");
        return -RT_ERROR;
    }
    rt_thread_startup(thread);

#if 0
    /* start canbus monitoring thread */
    thread = rt_thread_create("can_mon", bxcan_mon_thread, RT_NULL, 1024, PRIORITY_LOW, 10);
    if (thread == RT_NULL)
    {
        LOG_E("create monitor thread fail");
        return -RT_ERROR;
    }
    rt_thread_startup(thread);
#endif

    return RT_EOK;
}

