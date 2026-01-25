/* handles gs_usb/socketcan control requests */

#include "gsusb.h"
#include "usb_desc.h"
#include "canbus.h"
#include "canfilter.h"
#include "timestamp_us.h"
#include "bxcan.h"

#ifdef DBG_TAG
#undef DBG_TAG
#undef DBG_LVL
#endif

#define DBG_TAG "GSUSB"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#define LITTLE_ENDIAN 0x0000beefU
#define WORK_QUEUE    8
#define RESPONSE(X)            \
    {                          \
        *data = (uint8_t *)&X; \
        *len  = sizeof(X);     \
    }

enum gsusb_work_enum
{
    BXCAN_RESET,
    BXCAN_DISABLE,
    BXCAN_ENABLE,
};

static struct gs_device_bittiming can_bittiming     = {0};
static bool                       can_timestamp     = false;
static uint32_t                   can_mode          = RT_CAN_MODE_NORMAL;
static struct canfilter_bxcan_f0  can_filter        = {0};
static bool                       bittiming_changed = false;
static bool                       timestamp_changed = false;
static bool                       mode_changed      = false;
static bool                       filter_changed    = false;

static rt_mailbox_t gsusb_work_mb = NULL;

static const struct gs_device_bt_const bt_const = {
    .feature   = GS_CAN_FEATURE_LISTEN_ONLY | GS_CAN_FEATURE_LOOP_BACK | GS_CAN_FEATURE_HW_TIMESTAMP | GS_CAN_FEATURE_FILTER,
    .fclk_can  = BXCAN_CLOCK_SPEED,
    .tseg1_min = 1,
    .tseg1_max = 16,
    .tseg2_min = 1,
    .tseg2_max = 8,
    .sjw_max   = 4,
    .brp_min   = 1,
    .brp_max   = 1024,
    .brp_inc   = 1,
};

static const struct gs_device_config device_config = {
    .reserved1  = 0,
    .reserved2  = 0,
    .reserved3  = 0,
    .icount     = 0, /* 0 = 1 channel */
    .sw_version = 2, /* GS_USB v2 */
    .hw_version = 1, /* Hardware v1 */
};

static const struct canfilter_info filter_info = {
    .dev = CANFILTER_DEV_BXCAN_F0,
};

static uint32_t timestamp_us = 0;

static inline uint32_t gs_usb_request_host_format(uint8_t **data, uint32_t *len)
{
    const struct gs_host_config *host_config;
    uint32_t                     byte_order;

    host_config = (const struct gs_host_config *)*data;

    if (*len != sizeof(struct gs_host_config))
    {
        LOG_E("invalid length for host format request (%d)", len);
        return -1;
    }

    /* Read byte order (little-endian in USB) */
    byte_order = host_config->byte_order;

    if (byte_order != LITTLE_ENDIAN)
    {
        LOG_E("unsupported host byte order (0x%08x)", byte_order);
        return -1;
    }

    LOG_I("host format accepted");

    return 0;
}

static inline uint32_t gs_usb_request_bittiming(uint8_t **data, uint32_t *len)
{
    const struct gs_device_bittiming *new_bittiming;
    can_baudrate_type                 new_baudrate;

    new_bittiming = (const struct gs_device_bittiming *)*data;

    if (*len != sizeof(struct gs_device_bittiming))
    {
        LOG_E("invalid length for bittiming request (%d)", len);
        return -1;
    }

    /* calculate speed */
    const uint8_t tseg1 = new_bittiming->prop_seg + new_bittiming->phase_seg1;
    const uint8_t tseg2 = new_bittiming->phase_seg2;
    uint32_t      speed = BXCAN_CLOCK_SPEED / (new_bittiming->brp * (1 + tseg1 + tseg2));
    speed               = ((speed + 500) / 1000) * 1000; // rounding
    LOG_I("can speed %d", speed);

    /* validate */
    if (new_bittiming->brp < bt_const.brp_min || new_bittiming->brp > bt_const.brp_max || new_bittiming->sjw < 1 || new_bittiming->sjw > bt_const.sjw_max || tseg1 < bt_const.tseg1_min || tseg1 > bt_const.tseg1_max || tseg2 < bt_const.tseg2_min || tseg2 > bt_const.tseg2_max)
    {
        LOG_E("canbus timing out of range");
        return -1;
    }

    if (can_bittiming.prop_seg == new_bittiming->prop_seg && can_bittiming.phase_seg1 == new_bittiming->phase_seg1 && can_bittiming.phase_seg2 == new_bittiming->phase_seg2 && can_bittiming.sjw == new_bittiming->sjw && can_bittiming.brp == new_bittiming->brp)
        return 0;

    can_bittiming     = *new_bittiming;
    bittiming_changed = true;

    return 0;
}

static inline uint32_t gs_usb_request_mode(uint8_t **data, uint32_t *len)
{
    const struct gs_device_mode *mode;

    mode = (const struct gs_device_mode *)*data;

    LOG_D("MODE request: mode=%u flags=0x%08x", mode->mode, mode->flags);

    if (*len != sizeof(struct gs_device_mode))
    {
        LOG_E("invalid length for mode request (%d)", len);
        return -1;
    }

    if (mode->mode == GS_CAN_MODE_RESET)
    {
        if (gsusb_work_mb)
            rt_mb_send(gsusb_work_mb, BXCAN_DISABLE);

        LOG_I("can disable");
    }
    else if (mode->mode == GS_CAN_MODE_START)
    {
        uint32_t new_mode = CAN_MODE_COMMUNICATE;
        if (mode->flags & GS_CAN_MODE_LISTEN_ONLY)
        {
            new_mode = RT_CAN_MODE_LISTEN;
            LOG_I("can mode listen");
        }
        else if (mode->flags & GS_CAN_MODE_LOOP_BACK)
        {
            new_mode = RT_CAN_MODE_LOOPBACK;
            LOG_I("can mode loopback");
        }
        else
        {
            new_mode = RT_CAN_MODE_NORMAL;
            LOG_I("can mode normal");
        }

        if (can_mode != new_mode)
        {
            can_mode     = new_mode;
            mode_changed = true;
        }

        bool new_timestamp = mode->flags & GS_CAN_MODE_HW_TIMESTAMP;

        if (can_timestamp != new_timestamp)
        {
            can_timestamp     = new_timestamp;
            timestamp_changed = true;
        }

        LOG_I("timestamp %s", can_timestamp ? "on" : "off");

        if (gsusb_work_mb)
        {
            if (bittiming_changed)
            {
                bxcan_set_bittiming(can_bittiming);
                bittiming_changed = false;
            }

            if (mode_changed)
            {
                bxcan_set_mode(can_mode);
                mode_changed = false;
            }

            if (timestamp_changed)
            {
                bxcan_set_timestamp(can_timestamp);
                timestamp_changed = false;
            }

            if (filter_changed)
            {
                bxcan_set_filter(can_filter);
                filter_changed = false;
            }

            rt_mb_send(gsusb_work_mb, BXCAN_ENABLE);
        }

        LOG_I("can enable");
    }

    return 0;
}

static inline uint32_t gs_usb_request_set_filter(uint8_t **data, uint32_t *len)
{
    const struct canfilter_bxcan_f0 *new_filter;

    new_filter = (const struct canfilter_bxcan_f0 *)*data;

    if (*len != sizeof(struct canfilter_bxcan_f0))
    {
        LOG_E("invalid length for set filter request (%d)", len);
        return -1;
    }

    if (new_filter->dev != CANFILTER_DEV_BXCAN_F0)
    {
        LOG_E("invalid hardware type for set filter request (%d)", new_filter->dev);
        return -1;
    }

    memcpy(&can_filter, new_filter, sizeof(bxcan_filter));
    filter_changed = 1;
    LOG_I("can set filter");

    return 0;
}

void gsusb_reset()
{
    if (gsusb_work_mb)
        rt_mb_send(gsusb_work_mb, BXCAN_RESET);

    LOG_I("can reset");
}

int gsusb_control_request_handler(uint8_t                  busid,
                                  struct usb_setup_packet *setup,
                                  uint8_t                **data,
                                  uint32_t                *len)
{
    if (!setup || !data || !len)
    {
        LOG_E("NULL pointer request");
        return -1;
    }

    LOG_D("CTRL: bReq=0x%02X bmReqType=0x%02X wValue=0x%04X wIndex=0x%04X len=%d",
          setup->bRequest, setup->bmRequestType, setup->wValue, setup->wIndex, *len);

    /* Check recipient */
    uint8_t recipient = setup->bmRequestType & USB_REQUEST_RECIPIENT_MASK;
    if (recipient != USB_REQUEST_RECIPIENT_INTERFACE)
        return -1;

    /* Check interface */
    uint8_t iface = setup->wIndex & 0xFF;
    if (iface != GS_USB_INTF)
    {
        LOG_D("Not our interface: %d (expected %d)", iface, GS_USB_INTF);
        return 0; /* Let CDC handle it */
    }

    uint16_t ch = setup->wValue;

    /* Check direction */
    uint8_t dir                    = setup->bmRequestType & USB_REQUEST_DIR_MASK;
    bool    usb_reqtype_is_to_host = (dir == USB_REQUEST_DIR_IN);

    if (usb_reqtype_is_to_host)
    {
        /* Interface to host */
        switch (setup->bRequest)
        {
        case GS_USB_BREQ_BT_CONST:
            RESPONSE(bt_const);
            LOG_I("bt_const");
            return 0;
        case GS_USB_BREQ_DEVICE_CONFIG:
            RESPONSE(device_config);
            LOG_I("device_config");
            return 0;
        case GS_USB_BREQ_TIMESTAMP:
            timestamp_us = get_timestamp_us();
            RESPONSE(timestamp_us);
            LOG_I("timestamp_us %d", timestamp_us);
            return 0;
        case GS_USB_BREQ_GET_FILTER:
            RESPONSE(filter_info);
            LOG_I("filter_info");
            return 0;
        default:
            LOG_D("invalid get request (%d)", setup->bRequest);
            break;
        }
    }
    else
    {
        /* Host to interface */
        switch (setup->bRequest)
        {
        case GS_USB_BREQ_HOST_FORMAT:
            return gs_usb_request_host_format(data, len);
        case GS_USB_BREQ_BITTIMING:
            return gs_usb_request_bittiming(data, len);
        case GS_USB_BREQ_MODE:
            return gs_usb_request_mode(data, len);
        case GS_USB_BREQ_SET_FILTER:
            return gs_usb_request_set_filter(data, len);
        default:
            LOG_D("invalid set request (%d)", setup->bRequest);
            break;
        }
    }

    LOG_E("bmRequestType 0x%02x bRequest 0x%02x not supported", setup->bmRequestType,
          setup->bRequest);

    return -ENOTSUP;
}

/* gsusb control message worker thread
   for tasks that cannot run in irq context,
   require too much stack or take too long
   to run in an usb callback */
static void gsusb_ctrl_worker(void *param)
{
    enum gsusb_work_enum work;
    while (1)
    {
        rt_thread_yield();
        if (rt_mb_recv(gsusb_work_mb, (rt_ubase_t *)&work, RT_WAITING_FOREVER) != RT_EOK)
            continue;
        switch (work)
        {
        case BXCAN_RESET:
            bxcan_reset();
            break;
        case BXCAN_DISABLE:
            bxcan_disable();
            break;
        case BXCAN_ENABLE:
            bxcan_enable();
            break;
        default:
            LOG_E("what work?");
            break;
        }
    }
}

int gsusb_init()
{
    if (bxcan_init() != RT_EOK)
        return -RT_ERROR;

    /* create gsusb work mailbox */
    gsusb_work_mb = rt_mb_create("can_ctrl", WORK_QUEUE, RT_IPC_FLAG_FIFO);
    if (gsusb_work_mb == RT_NULL)
    {
        LOG_E("create canbus work mailbox failed!");
        return -RT_ERROR;
    }

    /* create gsusb work thread */
    rt_thread_t thread = rt_thread_create("can_ctrl", gsusb_ctrl_worker, RT_NULL, 1024, 25, 10);
    if (thread == RT_NULL)
    {
        LOG_E("create work thread fail");
        return -RT_ERROR;
    }
    rt_thread_startup(thread);

    return RT_EOK;
}

INIT_COMPONENT_EXPORT(gsusb_init);

