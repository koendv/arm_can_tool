#include <rtthread.h>
#include <rtdevice.h>
#define DBG_TAG "SLCAN"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>
#include "settings.h"
#include "serials.h"
#include "canbus_event.h"
#include "usb_cdc0.h"
#include "usb_serial_number.h"
#include "timestamp_us.h"

#define SLCAN_STD_ID_LEN 3
#define SLCAN_EXT_ID_LEN 8
#define SLCAN_MTU 40

static uint8_t        usb_rx_buf[SLCAN_MTU] = {0};
static uint32_t       usb_rx_len            = 0;
static uint8_t        usb_tx_buf[SLCAN_MTU] = {0};
static bool           slcan_timestamp       = false;
static const uint32_t slcan_bitrate[]       = {10000, 20000, 50000, 100000, 125000, 250000, 500000, 800000, 1000000};

static inline void u32_to_hex(char *dst, uint32_t value, uint32_t digits)
{
    static const char hex[] = "0123456789ABCDEF";

    // from right to left
    for (unsigned i = 0; i < digits; i++)
    {
        dst[digits - 1 - i]   = hex[value & 0xF];
        value               >>= 4;
    }
}

static inline bool hex_to_u32(uint32_t *out, const uint8_t *src, uint32_t digits)
{
    if (!src || !out || digits == 0 || digits > 8)
        return false;

    uint32_t v = 0;

    for (uint32_t i = 0; i < digits; i++)
    {
        uint8_t  c = (uint8_t)src[i];
        uint32_t nib;

        if (c >= '0' && c <= '9')
            nib = c - '0';
        else if (c >= 'A' && c <= 'F')
            nib = c - 'A' + 10;
        else if (c >= 'a' && c <= 'f')
            nib = c - 'a' + 10;
        else
            return false; // invalid character

        v = (v << 4) | nib;
    }

    *out = v;
    return true;
}

static void inline usb_send_str(uint8_t *str)
{
    cdc1_write(str, strlen(str));
}

static rt_err_t slcan_parse_string(uint8_t *line, uint32_t len)
{
    usb_tx_buf[0] = '\r';
    usb_tx_buf[1] = '\0';

    if (line == NULL || len == 0)
        return -RT_ERROR;

    switch (line[0])
    {
    case 't': /* transmit standard data frame */
    case 'r': /* transmit standard remote frame */
    case 'T': /* transmit extended data frame */
    case 'R': /* transmit extended remote frame */
    {
        uint32_t            id;
        uint32_t            dlc;
        can_tx_message_type tx_msg = {0};

        bool is_extended = (line[0] == 'T' || line[0] == 'R');
        bool is_remote   = (line[0] == 'r' || line[0] == 'R');

        uint8_t id_len  = is_extended ? SLCAN_EXT_ID_LEN : SLCAN_STD_ID_LEN;
        uint8_t min_len = 1 + id_len + 1; /* command character + id + dlc */

        if (len < min_len)
            return -RT_ERROR;

        if (!hex_to_u32(&id, &line[1], id_len))
            return -RT_ERROR;

        if (!hex_to_u32(&dlc, &line[1 + id_len], 1))
            return -RT_ERROR;

        if (dlc > 8)
            return -RT_ERROR;

        uint32_t expected_len = min_len + (is_remote ? 0 : 2 * dlc);
        if (len < expected_len)
            return -RT_ERROR;

        /* set up can frame */
        tx_msg.id_type = is_extended ? CAN_ID_EXTENDED : CAN_ID_STANDARD;
        if (is_extended)
            tx_msg.extended_id = id;
        else
            tx_msg.standard_id = id;
        tx_msg.frame_type = is_remote ? CAN_TFT_REMOTE : CAN_TFT_DATA;
        tx_msg.dlc        = dlc;

        if (!is_remote)
        {
            for (uint32_t i = 0; i < dlc; i++)
            {
                uint32_t data_byte;
                if (!hex_to_u32(&data_byte, &line[min_len + i * 2], 2))
                    return -RT_ERROR;
                tx_msg.data[i] = data_byte;
            }
        }

        if (can_message_transmit(CAN1, &tx_msg) != CAN_TX_STATUS_NO_EMPTY)
            return RT_EOK;
        return -RT_ERROR;
    }

    case 'O': /* open */
        if (can_operating_mode_set(CAN1, CAN_OPERATINGMODE_COMMUNICATE) == SUCCESS)
            return RT_EOK;
        return -RT_ERROR;

    case 'C': /* close */
        if (can_operating_mode_set(CAN1, CAN_OPERATINGMODE_FREEZE) == SUCCESS)
            return RT_EOK;
        return -RT_ERROR;

    case 'S': /* set bitrate */
    {
        uint32_t bitrate_index;
        if (len < 2)
            return -RT_ERROR;

        if (!hex_to_u32(&bitrate_index, &line[1], 1))
            return -RT_ERROR;

        if (bitrate_index >= sizeof(slcan_bitrate) / sizeof(slcan_bitrate[0]))
            return -RT_ERROR;

        if (can_set_bitrate_freq(slcan_bitrate[bitrate_index]) == RT_EOK)
            return RT_EOK;

        return -RT_ERROR;
    }

    case 'Z': /* only microsecond timestamps implemented */
    {
        if (len < 2)
            return -RT_ERROR;
        slcan_timestamp = line[1] == '2';
        if (line[1] == '2')
            return RT_EOK;
        return -RT_ERROR;
    }

    case 'f': /* get error status */ {
        strncpy(usb_tx_buf, "F00\r", sizeof(usb_tx_buf));
        return RT_EOK;
    }

    case 'V':
        strncpy(usb_tx_buf, "V0100\r", sizeof(usb_tx_buf));
        return RT_EOK;

    case 'N':
        rt_snprintf(usb_tx_buf, sizeof(usb_tx_buf), "N%s\r", usb_serial_number);
        return RT_EOK;

    default:
        return -RT_ERROR;
    }
    return RT_EOK;
}

static void slcan_usb_receive()
{
    uint8_t ch;

    while (rt_device_read(cdc1_dev, 0, &ch, 1) == 1)
    {
        if ((ch == '\r' || ch == '\n') && (usb_rx_len > 0))
        {
            usb_rx_buf[usb_rx_len] = '\0';
            if (slcan_parse_string(usb_rx_buf, usb_rx_len) == RT_EOK)
                usb_send_str(usb_tx_buf);
            else
                usb_send_str("\a");
            usb_rx_len = 0;
        }
        else if (usb_rx_len < sizeof(usb_rx_buf) - 1)
        {
            usb_rx_buf[usb_rx_len++] = ch;
        }
        else
        {
            LOG_D("line too long, discarding");
            usb_rx_len = 0;
            /* skip rest of line */
            while (ch != '\r' && ch != '\n' && rt_device_read(cdc1_dev, 0, &ch, 1) == 1);
        }
    }
}

uint32_t slcan_format_frame(char buf[SLCAN_MTU], const can_stored_frame_t *frame, bool timestamp)
{
    uint32_t pos = 0;

    if (!frame)
        return 0;

    if (frame->frame.id_type == CAN_ID_STANDARD)
    {
        buf[pos++] = (frame->frame.frame_type == CAN_TFT_DATA) ? 't' : 'r';
        u32_to_hex(buf + pos, frame->frame.standard_id, SLCAN_STD_ID_LEN);
        pos += SLCAN_STD_ID_LEN;
    }
    else
    {
        buf[pos++] = (frame->frame.frame_type == CAN_TFT_DATA) ? 'T' : 'R';
        u32_to_hex(buf + pos, frame->frame.extended_id, SLCAN_EXT_ID_LEN);
        pos += SLCAN_EXT_ID_LEN;
    }

    uint32_t dlc = frame->frame.dlc;
    if (dlc > 8) dlc = 8;
    u32_to_hex(buf + pos, dlc, 1);
    pos++;

    if (frame->frame.frame_type != CAN_TFT_REMOTE)
    {
        for (uint32_t i = 0; i < dlc; i++)
        {
            u32_to_hex(buf + pos, frame->frame.data[i], 2);
            pos += 2;
        }
    }

    if (timestamp)
    {
        u32_to_hex(buf + pos, frame->timestamp_us, 8);
        pos += 8;
    }

    buf[pos++] = '\r';
    buf[pos]   = '\0';

    return pos;
}

static void slcan_can_receive(void)
{
    char               fmt_buf[SLCAN_MTU];
    can_stored_frame_t rx_frame;
    can_rx_result_t    rx_result;
    uint32_t           len;

    while ((rx_result = can_rx_get(&rx_frame)) != CAN_RX_EMPTY)
    {
        /* skip if already consumed elsewhere */
        if (rx_result == CAN_RX_SKIP)
            continue;

        /* slcan output */
        len = slcan_format_frame(fmt_buf, &rx_frame, slcan_timestamp);
        if (len > 0)
            cdc1_write((uint8_t *)fmt_buf, len);
    }
}

static void slcan_bus_off()
{
    static bool previous_bus_off = false;

    bool current_bus_off = can_flag_get(CAN1, CAN_BOF_FLAG);
    if (previous_bus_off == current_bus_off)
        return;

    uint32_t status = current_bus_off ? 0x80 : 0x0;
    if (slcan_timestamp)
        rt_snprintf(usb_tx_buf, sizeof(usb_tx_buf), "E%02X%08X\r", status, get_timestamp_us());
    else
        rt_snprintf(usb_tx_buf, sizeof(usb_tx_buf), "E%02X\r", status);
    usb_send_str(usb_tx_buf);
    previous_bus_off = current_bus_off;
    return;
}

static void cdc0_dtr_change()
{
    /* empty */
}

static void slcan_dtr_change()
{
    /* empty */
}

/* main SLCAN thread */
static void slcan_thread(void *arg)
{
    uint32_t recv_set;

    if (serial_event == NULL)
    {
        LOG_E("null serial_event");
        return;
    }
    LOG_I("init");
    while (1)
    {
        /* wait for USB or CAN event */
        rt_err_t err = rt_event_recv(serial_event,
                                     EVENT_MASK_CAN1_RX0_INDIC
                                         | EVENT_MASK_CAN1_BUS_OFF
                                         | EVENT_MASK_CDC1_DTR
                                         | EVENT_MASK_CDC1_RX
                                         | EVENT_MASK_CDC0_RX
                                         | EVENT_MASK_SERIAL0_RX
                                         | EVENT_MASK_SERIAL1_RX
                                         | EVENT_MASK_SERIAL2_RX,
                                     RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR,
                                     RT_WAITING_FOREVER,
                                     &recv_set);

        if (err != RT_EOK)
        {
            /* what? */
            recv_set = 0;
        }

        if (recv_set & EVENT_MASK_CAN1_BUS_OFF)
        {
            LOG_D("EVENT_CAN1_BUS_OFF");
            slcan_bus_off();
        }

        if (recv_set & EVENT_MASK_CDC1_RX)
        {
            LOG_D("EVENT_CDC1_RX");
            slcan_usb_receive();
        }

        if (recv_set & EVENT_MASK_CAN1_RX0_INDIC)
        {
            LOG_D("EVENT_CAN1_RX0_INDIC");
            slcan_can_receive();
        }

        if (recv_set & EVENT_MASK_CDC0_RX)
        {
            cdc0_receive();
        }

        if (recv_set & EVENT_MASK_SERIAL0_RX)
        {
            serial0_receive();
        }

        if (recv_set & EVENT_MASK_SERIAL1_RX)
        {
            serial1_receive();
        }

        if (recv_set & EVENT_MASK_SERIAL2_RX)
        {
            serial2_receive();
        }

        if (recv_set & EVENT_MASK_CDC0_DTR)
        {
            cdc0_dtr_change();
        }

        if (recv_set & EVENT_MASK_CDC1_DTR)
        {
            slcan_dtr_change();
        }

        cdc0_flush();
    }
}

/* USB configured callback */
void slcan_on_configured(uint8_t busid)
{
    rt_err_t ret;

    (void)busid;

    if (settings.mode != MODE_CMSIS_DAP)
    {
        LOG_D("slcan disabled - not in mode CMSIS-DAP");
        return;
    }

    /* configure canbus device */
    if (can_init(true) != RT_EOK)
    {
        LOG_E("canbus init fail");
        return;
    }

    /* get hardware serials and usb serials */
    serials_init();

    /* events, can and usb configured - start slcan thread */
    rt_thread_t thread = rt_thread_create("slcan",
                                          slcan_thread,
                                          NULL,
                                          2048,
                                          15,
                                          10);
    if (!thread)
    {
        LOG_E("slcan thread fail");
        return;
    }
    rt_thread_startup(thread);

    return;
}


