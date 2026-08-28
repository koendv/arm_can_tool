#include <rtthread.h>
#include <rtconfig.h>
#include <stdarg.h>
#include <board.h>
#include "platform.h"
#include "pins.h"
#include "serials.h"
#include "settings.h"
#include "logger.h"
#include "swo.h"
#include "usb_cdc0.h"
#include "rtt_serial.h"

#define DBG_TAG "UART"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#define PRINTF_BUFSIZE 256

/* uart will run at any baud rate between 1647 bit/s and 6750000 bit/s */
#define BAUD_MIN 1647
#define BAUD_MAX 6750000

/* 
 note cdc0 and cdc1 are not equal;
 cdc0 has been tuned for more output. (swo, rtt, serial)
 cdc1 is fine for input. (gdb, slcan)
 */

#define CDC1_DEVICE_NAME "usb-acm0"

const char * const event_name[EVENT_MAX] = {
    "EVENT_CAN1_TX_DONE",
    "EVENT_CAN1_RX0_INDIC",
    "EVENT_CAN1_BUS_OFF",
    "EVENT_CAN1_RX_OVERFLOW",
    "EVENT_CAN1_TX_OVERFLOW",
    "EVENT_GSUSB_BULK_OUT",
    "EVENT_GSUSB_STOP",
    "EVENT_GSUSB_START",
    "EVENT_GSUSB_TX_DONE",
    "EVENT_CDC0_DTR",
    "EVENT_CDC1_DTR",
    "EVENT_CDC0_RX",
    "EVENT_CDC1_RX",
    "EVENT_SERIAL0_RX",
    "EVENT_SERIAL1_RX",
    "EVENT_SERIAL2_RX",
    "EVENT_TARGET_HALT_REQUEST",
    "EVENT_TARGET_HALTED",
};

static uint8_t serial_rx_buf[HS_PACKET_SIZE - 1] __attribute__((aligned(4)));
rt_event_t     serial_event = RT_NULL;
rt_device_t    cdc1_dev     = RT_NULL;
rt_device_t    serial0_dev  = RT_NULL;
rt_device_t    serial1_dev  = RT_NULL;
bool           cdc0_dtr     = false;
bool           cdc1_dtr     = false;

/* forward declarations */
static rt_err_t serial0_rx_indic(rt_device_t dev, rt_size_t size);
static rt_err_t serial1_rx_indic(rt_device_t dev, rt_size_t size);
static rt_err_t serial2_rx_indic(rt_device_t dev, rt_size_t size);

/* serial0 start/stop receive */
void serial0_enable(bool ena)
{
    if (serial0_dev)
        usart_receiver_mute_enable(USART2, !ena);
}

void serial1_enable(bool ena)
{
    if (serial1_dev)
        usart_receiver_mute_enable(USART3, !ena);
}

/* set serial port speed */
static rt_err_t serial_set_speed(const char *name, rt_device_t *serial, uint32_t speed, rt_err_t (*rx_indic)(rt_device_t dev, rt_size_t size))
{
    rt_err_t                err;
    struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;

    if (!name || !serial || (speed == 0)) return -RT_ERROR;
    if (speed < BAUD_MIN) speed = BAUD_MIN;
    if (speed > BAUD_MAX) speed = BAUD_MAX;

    if (*serial)
    {
        rt_device_close(*serial);
    }
    else
    {
        *serial = rt_device_find(name);
        if (!*serial)
        {
            LOG_E("%s find failed!", name);
            return -RT_ERROR;
        }
    }
    config.baud_rate = speed;
    config.data_bits = DATA_BITS_8;
    config.stop_bits = STOP_BITS_1;
    config.parity    = PARITY_NONE;
    err              = rt_device_control(*serial, RT_DEVICE_CTRL_CONFIG, &config);
    if (err != RT_EOK)
    {
        LOG_E("%s set speed failed!", name);
        return -RT_ERROR;
    }
    err = rt_device_open(*serial, RT_DEVICE_FLAG_RX_NON_BLOCKING | RT_DEVICE_FLAG_TX_BLOCKING);
    if (err != RT_EOK)
    {
        LOG_E("%s open failed!", name);
        return -RT_ERROR;
    }
    LOG_I("%s speed %d", name, speed);

    /* set callback */
    if (serial)
        rt_device_set_rx_indicate(*serial, rx_indic);
    else
        LOG_E("no serial %s", name);

    return RT_EOK;
}

/* set serial0 speed in bit/s */
void serial0_set_speed(uint32_t speed)
{
    serial_set_speed(AUX_UART, &serial0_dev, speed, serial0_rx_indic);
}

void serial1_set_speed(uint32_t speed)
{
    serial_set_speed(AUX1_UART, &serial1_dev, speed, serial1_rx_indic);
}

/* serial0 special feature: swap usart receive and transmit pins */
void serial0_swap_pins(bool swap_pins)
{
    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
    rt_pin_mode(TARGET_TXD_DIR_PIN, PIN_MODE_OUTPUT);
    rt_pin_mode(TARGET_RXD_DIR_PIN, PIN_MODE_OUTPUT);
    if (swap_pins)
    {
        rt_pin_write(TARGET_TXD_DIR_PIN, PIN_HIGH);
        rt_pin_write(TARGET_RXD_DIR_PIN, PIN_LOW);
        usart_transmit_receive_pin_swap(USART2, false);
    }
    else
    {
        rt_pin_write(TARGET_TXD_DIR_PIN, PIN_LOW);
        rt_pin_write(TARGET_RXD_DIR_PIN, PIN_HIGH);
        usart_transmit_receive_pin_swap(USART2, true);
    }
}

/* callback when usb cdc acm port connects/disconnects */
void serial_set_dtr(uint8_t intf, bool dtr)
{
    LOG_D("serial_set_dtr(%d, %d)", intf, dtr);

    if (intf == CDC0_INTF)
    {
        if (cdc0_dtr == dtr)
            return;
        cdc0_dtr = dtr;
        if (serial_event)
            rt_event_send(serial_event, EVENT_MASK_CDC0_DTR);
    }
    else if (intf == CDC1_INTF)
    {
        if (cdc1_dtr == dtr)
            return;
        cdc1_dtr = dtr;
        if (serial_event)
            rt_event_send(serial_event, EVENT_MASK_CDC1_DTR);
    }
}

/* formatted output */

void cdc0_printf(const char *fmt, ...)
{
    char    buffer[PRINTF_BUFSIZE];
    va_list args;

    va_start(args, fmt);
    int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    if (len < 0)
    {
        return; // formatting error
    }

    if ((size_t)len > sizeof(buffer))
    {
        len = sizeof(buffer); // truncated output
    }

    cdc0_write(buffer, (size_t)len);
}

void cdc1_printf(const char *fmt, ...)
{
    char    buffer[PRINTF_BUFSIZE];
    va_list args;

    va_start(args, fmt);
    int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    if (len < 0)
    {
        return; // formatting error
    }

    if ((size_t)len > sizeof(buffer))
    {
        len = sizeof(buffer); // truncated output
    }

    cdc1_write(buffer, (size_t)len);
}

/* rtt from host to target */


/* callbacks */

/* cdc0_write is in usb_cdc0.c */

void cdc1_write(const char *buf, const size_t len)
{
    if ((cdc1_dev != NULL) && cdc1_dtr && (len != 0))
        rt_device_write(cdc1_dev, 0, (uint8_t *)buf, len);
}

void cdc0_receive()
{
    switch (settings.cdc0_out)
    {
    case CDC0_OUT_SERIAL0:
        if (serial0_dev)
            rt_device_write(serial0_dev, 0, cdc0_out_buf, cdc0_out_len);
        break;
    case CDC0_OUT_SERIAL1:
        if (serial1_dev)
            rt_device_write(serial1_dev, 0, cdc0_out_buf, cdc0_out_len);
        break;
    case CDC0_OUT_RTT:
        rtt_host_to_target(cdc0_out_buf, cdc0_out_len);
        break;
    default:
        break;
    }
    /* read next cdc0 usb packet */
    cdc0_start_read();
}

void serial0_receive()
{
    uint32_t len = 0;

    if (!serial0_dev)
        return;

    while ((len = rt_device_read(serial0_dev, 0, serial_rx_buf, sizeof(serial_rx_buf))) > 0)
        cdc0_write(serial_rx_buf, len);
}

void serial1_receive()
{
    uint32_t len = 0;

    if (!serial1_dev)
        return;

    while ((len = rt_device_read(serial1_dev, 0, serial_rx_buf, sizeof(serial_rx_buf))) > 0)
        cdc0_write(serial_rx_buf, len);
}

/* interrupt handlers */

static rt_err_t cdc0_rx_indic(rt_device_t dev, rt_size_t size)
{
    (void)dev;
    (void)size;
    if (serial_event && (size != 0))
        rt_event_send(serial_event, EVENT_MASK_CDC0_RX);
    return RT_EOK;
}

static rt_err_t cdc1_rx_indic(rt_device_t dev, rt_size_t size)
{
    (void)dev;
    (void)size;
    if (serial_event && (size != 0))
        rt_event_send(serial_event, EVENT_MASK_CDC1_RX);
    return RT_EOK;
}

static rt_err_t serial0_rx_indic(rt_device_t dev, rt_size_t size)
{
    (void)dev;
    (void)size;
    if (serial_event && (size != 0))
        rt_event_send(serial_event, EVENT_MASK_SERIAL0_RX);
    return RT_EOK;
}

static rt_err_t serial1_rx_indic(rt_device_t dev, rt_size_t size)
{
    (void)dev;
    (void)size;
    if (serial_event && (size != 0))
        rt_event_send(serial_event, EVENT_MASK_SERIAL1_RX);
    return RT_EOK;
}

void usb_serials_init()
{
    if (serial_event == RT_NULL)
    {
        LOG_E("null serial_event");
    }

    if (!cdc1_dev)
        cdc1_dev = rt_device_find(CDC1_DEVICE_NAME);
    if (!cdc1_dev)
        LOG_E("no " CDC1_DEVICE_NAME);
    if (cdc1_dev)
    {
        rt_device_set_rx_indicate(cdc1_dev, cdc1_rx_indic);
        if (rt_device_open(cdc1_dev, RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_INT_RX) != RT_EOK)
            LOG_E(CDC1_DEVICE_NAME " open failed!");
    }
}

/* called when settings available */
void hardware_serials_init()
{
    if (serial_event == RT_NULL)
    {
        LOG_E("null serial_event");
    }

    if (settings.serial0_enable && !serial0_dev)
        serial0_set_speed(serial_speeds[settings.serial0_speed]);
    if (!serial0_dev)
        LOG_E("no serial0");

    if (settings.serial1_enable && !serial1_dev)
        serial1_set_speed(serial_speeds[settings.serial1_speed]);
    if (!serial1_dev)
        LOG_E("no serial1");

    if (settings.serial2_enable && !serial2_ready)
        serial2_set_speed(serial_speeds[settings.serial2_speed]);
    if (!serial2_ready)
        LOG_E("no serial2");

    return;
}

void serials_init()
{
    hardware_serials_init();
    usb_serials_init();
}

/* called early on */
static int serial_event_init(void)
{
    if (serial_event == RT_NULL)
        serial_event = rt_event_create("serial", RT_IPC_FLAG_FIFO);
    return RT_EOK;
}

INIT_PREV_EXPORT(serial_event_init);
