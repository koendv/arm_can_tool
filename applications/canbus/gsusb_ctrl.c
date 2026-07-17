#include <rtthread.h>
#define DBG_TAG "GSUSB"
//#define DBG_LVL DBG_INFO
#define DBG_LVL DBG_DBG
#include <rtdbg.h>

#include "gsusb.h"
#include "usb_desc.h"
#include "canbus.h"
#include "canfilter.h"
#include "canbus_event.h"
#include "timestamp_us.h"
#include "settings.h"

#define BXCAN_CLOCK_SPEED (108000000U) /* apb1 bus frequency */
#define LITTLE_ENDIAN     0x0000beefU
#define RESPONSE(X)            \
    {                          \
        *data = (uint8_t *)&X; \
        *len  = sizeof(X);     \
    }

typedef struct
{
    can_baudrate_type       can_bittiming;
    can_filter_t            can_filter;
    can_mode_type           flags; /* from gs_usb protocol */
    can_operating_mode_type mode;  /* from at32 HAL */
    bool                    hw_timestamp;
} can_settings_t;

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

static uint32_t timestamp_us    = 0;
bool            gsusb_timestamp = false;

static can_settings_t can_settings = {0};

static inline uint32_t gs_usb_request_bittiming(uint8_t **data, uint32_t *len)
{
    const struct gs_device_bittiming *new_bittiming;
    can_baudrate_type                 new_baudrate;

    new_bittiming = (const struct gs_device_bittiming *)*data;

    if (*len != sizeof(struct gs_device_bittiming))
    {
        LOG_E("invalid length for bittiming request (%d)", *len);
        return -1;
    }

    /* calculate speed */
    const uint8_t tseg1 = new_bittiming->prop_seg + new_bittiming->phase_seg1;
    const uint8_t tseg2 = new_bittiming->phase_seg2;
    uint32_t      speed = BXCAN_CLOCK_SPEED / (new_bittiming->brp * (1 + tseg1 + tseg2));
    LOG_I("can bit rate %d", speed);

    /* validate */
    if (new_bittiming->brp < bt_const.brp_min
        || new_bittiming->brp > bt_const.brp_max
        || new_bittiming->sjw < 1
        || new_bittiming->sjw > bt_const.sjw_max
        || tseg1 < bt_const.tseg1_min
        || tseg1 > bt_const.tseg1_max
        || tseg2 < bt_const.tseg2_min
        || tseg2 > bt_const.tseg2_max)
    {
        LOG_E("canbus timing out of range");
        return -1;
    }

    can_settings.can_bittiming.baudrate_div = new_bittiming->brp;
    can_settings.can_bittiming.rsaw_size    = (can_rsaw_type)(new_bittiming->sjw - 1);
    can_settings.can_bittiming.bts1_size    = (can_bts1_type)(tseg1 - 1);
    can_settings.can_bittiming.bts2_size    = (can_bts2_type)(tseg2 - 1);

    return 0;
}

static inline uint32_t gs_usb_request_mode(uint8_t **data, uint32_t *len)
{
    const struct gs_device_mode *gs_mode;

    gs_mode = (const struct gs_device_mode *)*data;

    LOG_D("MODE request: mode=%u flags=0x%08x", gs_mode->mode, gs_mode->flags);

    if (*len != sizeof(struct gs_device_mode))
    {
        LOG_E("invalid length for mode request (%d)", *len);
        return -1;
    }

    can_mode_type           new_mode         = can_settings.mode;
    can_operating_mode_type new_flags        = can_settings.flags;
    bool                    new_hw_timestamp = false;

    if (gs_mode->mode == GS_CAN_MODE_RESET)
    {
        new_flags = CAN_OPERATINGMODE_FREEZE;
        LOG_I("can mode reset");
    }
    if (gs_mode->mode == GS_CAN_MODE_START)
    {
        new_flags = CAN_OPERATINGMODE_COMMUNICATE;
        LOG_I("can mode start");
    }
    if (gs_mode->flags & GS_CAN_MODE_LISTEN_ONLY && gs_mode->flags & GS_CAN_MODE_LOOP_BACK)
    {
        new_mode = CAN_MODE_LISTENONLY_LOOPBACK;
        LOG_I("can flag loopback listen only");
    }
    else if (gs_mode->flags & GS_CAN_MODE_LISTEN_ONLY)
    {
        new_mode = CAN_MODE_LISTENONLY;
        LOG_I("can flag listen only");
    }
    else if (gs_mode->flags & GS_CAN_MODE_LOOP_BACK)
    {
        new_mode = CAN_MODE_LOOPBACK;
        LOG_I("can flag loopback");
    }
    else
    {
        new_mode = CAN_MODE_COMMUNICATE;
        LOG_I("can flag normal");
    }

    new_hw_timestamp = (gs_mode->flags & GS_CAN_MODE_HW_TIMESTAMP) != 0;

    if ((can_settings.mode == new_mode) && (can_settings.flags == new_flags) && (can_settings.hw_timestamp == new_hw_timestamp))
        return 0;

    if (can_settings.mode != new_mode)
    {
        can_settings.mode = new_mode;
    }
    if (can_settings.flags != new_flags)
    {
        can_settings.flags = new_flags;
    }
    can_settings.hw_timestamp = new_hw_timestamp;

    if (serial_event)
    {
        if (can_settings.mode == CAN_MODE_COMMUNICATE)
            rt_event_send(serial_event, EVENT_MASK_GSUSB_START);
        else
            rt_event_send(serial_event, EVENT_MASK_GSUSB_STOP);
    }

    return 0;
}

static inline uint32_t gs_usb_request_set_filter(uint8_t **data, uint32_t *len)
{
    const struct canfilter_bxcan_f0 *new_filter;

    new_filter = (const struct canfilter_bxcan_f0 *)*data;

    if (*len != sizeof(struct canfilter_bxcan_f0))
    {
        LOG_E("invalid length for set filter request (%d)", *len);
        return -1;
    }

    if (new_filter->dev != CANFILTER_DEV_BXCAN_F0)
    {
        LOG_E("invalid hardware type for set filter request (%d)", new_filter->dev);
        return -1;
    }

    memcpy(&can_settings.can_filter, new_filter, sizeof(can_filter_t));

    LOG_I("can set filter");

    return 0;
}

static inline uint32_t gs_usb_request_host_format(uint8_t **data, uint32_t *len)
{
    const struct gs_host_config *host_config;
    uint32_t                     byte_order;

    host_config = (const struct gs_host_config *)*data;

    if (*len != sizeof(struct gs_host_config))
    {
        LOG_E("invalid length for host format request (%d)", *len);
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

rt_err_t can_set_filter(can_filter_t *new_filter)
{
    memcpy(&can_settings.can_filter, new_filter, sizeof(can_filter_t));

    /* save operating mode of can device */
    can_operating_mode_type saved_can_operating_mode;
    if (CAN1->mctrl_bit.fzen)
        saved_can_operating_mode = CAN_OPERATINGMODE_FREEZE;
    else if (CAN1->mctrl_bit.dzen)
        saved_can_operating_mode = CAN_OPERATINGMODE_DOZE;
    else
        saved_can_operating_mode = CAN_OPERATINGMODE_COMMUNICATE;

    /* enter freeze mode to program filter */
    if (saved_can_operating_mode != CAN_OPERATINGMODE_FREEZE)
        can_operating_mode_set(CAN1, CAN_OPERATINGMODE_FREEZE);

    /* program filter */
    bxcan_set_filter(can_settings.can_filter);

    /* return to saved operating mode */
    if (saved_can_operating_mode != CAN_OPERATINGMODE_FREEZE)
        can_operating_mode_set(CAN1, saved_can_operating_mode);

    return RT_EOK;
}

rt_err_t can_get_filter(can_filter_t *hw_filter)
{
    memcpy(hw_filter, &can_settings.can_filter, sizeof(can_filter_t));

    return RT_EOK;
}

static error_status can_mode_set(can_type *can_x, can_mode_type can_mode)
{
    can_base_type can_base_struct;

    can_default_para_init(&can_base_struct);
    can_base_struct.mode_selection   = can_mode;
    can_base_struct.ttc_enable       = FALSE;
    can_base_struct.aebo_enable      = TRUE;
    can_base_struct.aed_enable       = TRUE;
    can_base_struct.prsf_enable      = FALSE;
    can_base_struct.mdrsel_selection = CAN_DISCARDING_FIRST_RECEIVED;
    can_base_struct.mmssr_selection  = CAN_SENDING_BY_ID;
    return can_base_init(can_x, &can_base_struct);
}

void gsusb_stop()
{
    can_operating_mode_set(CAN1, CAN_OPERATINGMODE_FREEZE);
}

void gsusb_start()
{
    can_operating_mode_set(CAN1, CAN_OPERATINGMODE_FREEZE);

    can_mode_set(CAN1, can_settings.mode);

    if (settings.canfilter_enable)
        bxcan_set_filter(can_settings.can_filter);
    else
        bxcan_set_filter(canfilter_bxcan_f0_pass_all);

    can_baudrate_set(CAN1, &can_settings.can_bittiming);

    gsusb_timestamp = can_settings.hw_timestamp;

    can_operating_mode_set(CAN1, CAN_OPERATINGMODE_COMMUNICATE);
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
    if (iface != CANBUS_INTF)
    {
        LOG_D("Not our interface: %d (expected %d)", iface, CANBUS_INTF);
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
