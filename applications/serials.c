#include <rtthread.h>
#include "platform.h"
#include "usb_cdc.h"
#include "serials.h"
#include "settings.h"
#include "swo.h"

#define DBG_TAG "UART"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

/* uart will run at any baud rate between 1647 bit/s and 6750000 bit/s */
#define BAUD_MIN 1647
#define BAUD_MAX 6750000

#define SERIAL_RX_STACK    1024
#define SERIAL_RX_PRIORITY 25

/* TODO to improve speed, avoid copying dma buffer with rt_device_read()
   but instead do swo_decode() directly on dma buffer */

static rt_device_t serial0_dev = RT_NULL;
static rt_device_t serial1_dev = RT_NULL;
static rt_device_t serial2_dev = RT_NULL;

static rt_sem_t serial0_rx_sem = RT_NULL;
static rt_sem_t serial1_rx_sem = RT_NULL;
static rt_sem_t serial2_rx_sem = RT_NULL;

static bool serial0_ena = true;
static bool serial1_ena = true;
static bool serial2_ena = true;

/* serial0 stop receive */
void serial0_enable(bool ena)
{
    serial0_ena = ena;
    if (serial0_dev)
        usart_receiver_mute_enable(USART2, !ena);
}

void serial1_enable(bool ena)
{
    serial1_ena = ena;
    if (serial1_dev)
        usart_receiver_mute_enable(USART3, !ena);
}

void serial2_enable(bool ena)
{
    serial2_ena = ena;
    if (serial2_dev)
        usart_receiver_mute_enable(UART7, !ena);
}

/* set serial port speed */
static rt_err_t serial_set_speed(const char *name, rt_device_t *serial, uint32_t speed)
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

    return RT_EOK;
}

/* set serial0 speed in bit/s */
void serial0_set_speed(uint32_t speed)
{
    serial_set_speed(AUX_UART, &serial0_dev, speed);
}

void serial1_set_speed(uint32_t speed)
{
    serial_set_speed(AUX1_UART, &serial1_dev, speed);
}

void serial2_set_speed(uint32_t speed)
{
    serial_set_speed(AUX2_UART, &serial2_dev, speed);
}

/* serial0 transmit */
int32_t serial0_write(uint8_t *buf, uint32_t len)
{
    int32_t ret = -RT_ERROR;
    if (serial0_dev && serial0_ena)
    {
        ret = rt_device_write(serial0_dev, 0, buf, len);
        LOG_D("serial0 tx %*.s", len, buf);
    }
    return ret;
}

int32_t serial1_write(uint8_t *buf, uint32_t len)
{
    int32_t ret = -RT_ERROR;
    if (serial1_dev && serial1_ena)
    {
        ret = rt_device_write(serial1_dev, 0, buf, len);
        LOG_D("serial1 tx %*.s", len, buf);
    }
    return ret;
}

/* serial0 special feature: swap usart receive and transmit pins */
void serial0_swap_pins(bool swap_pins)
{
#if 0
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
#endif
}

/* serial0 receive interrupt handler */
static rt_err_t serial0_rx_cb(rt_device_t dev, rt_size_t size)
{
    if (serial0_rx_sem)
        rt_sem_release(serial0_rx_sem);
    return RT_EOK;
}

/* serial1 receive interrupt handler */
static rt_err_t serial1_rx_cb(rt_device_t dev, rt_size_t size)
{
    if (serial1_rx_sem)
        rt_sem_release(serial1_rx_sem);
    return RT_EOK;
}

/* serial2 receive interrupt handler */
static rt_err_t serial2_rx_cb(rt_device_t dev, rt_size_t size)
{
    if (serial2_rx_sem)
        rt_sem_release(serial2_rx_sem);
    return RT_EOK;
}

/* input from serial0 to usb cdc1 */
static void serial0_rx_thread(void *parameter)
{
    (void)parameter;
    uint16_t    len;
    static char serial0_rx_buf[256];

    while (1)
    {
        rt_sem_take(serial0_rx_sem, RT_WAITING_FOREVER);
        if (serial0_dev && serial0_ena)
            while (len = rt_device_read(serial0_dev, 0, serial0_rx_buf, sizeof(serial0_rx_buf)))
            {
                LOG_D("serial0 rx %*.s", len, serial0_rx_buf);
                cdc1_write(serial0_rx_buf, len);
#if 0
                serial0_write(serial0_rx_buf, len); /* echo for debugging */
#endif
            }
        rt_thread_yield();
    }
}

/* input from serial1 to usb cdc1 */
static void serial1_rx_thread(void *parameter)
{
    (void)parameter;
    uint16_t    len;
    static char serial1_rx_buf[256];

    while (1)
    {
        rt_sem_take(serial1_rx_sem, RT_WAITING_FOREVER);
        if (serial1_dev && serial1_ena)
            while (len = rt_device_read(serial1_dev, 0, serial1_rx_buf, sizeof(serial1_rx_buf)))
            {
                LOG_D("serial1 rx %*.s", len, serial1_rx_buf);
                cdc1_write(serial1_rx_buf, len);
#if 0
                serial1_write(serial1_rx_buf, len); /* echo for debugging */
#endif
            }
        rt_thread_yield();
    }
}

/* input from serial2 to usb cdc1 */
static void serial2_rx_thread(void *parameter)
{
    (void)parameter;
    uint16_t    len;
    static char serial2_rx_buf[256];

    while (1)
    {
        rt_sem_take(serial2_rx_sem, RT_WAITING_FOREVER);
        if (serial2_dev && serial2_ena)
            while (len = rt_device_read(serial2_dev, 0, serial2_rx_buf, sizeof(serial2_rx_buf)))
            {
                LOG_D("serial2 rx %*.s", len, serial2_rx_buf);
                if (settings.swo_decode)
                    swo_itm_decode(serial2_rx_buf, len);
                else
                    cdc1_write(serial2_rx_buf, len);
#if 0
                serial1_write(serial2_rx_buf, len); /* echo for debugging */
#endif
            }
        rt_thread_yield();
    }
}

static void serial0_init(uint32_t speed)
{
    static rt_thread_t serial_thread = RT_NULL;

    serial0_rx_sem = rt_sem_create(AUX_UART, 0, RT_IPC_FLAG_FIFO);
    serial0_set_speed(speed);
    serial_thread = rt_thread_create(AUX_UART, serial0_rx_thread, RT_NULL, SERIAL_RX_STACK, SERIAL_RX_PRIORITY, 10);
    if (serial_thread)
        rt_thread_startup(serial_thread);
    else
        LOG_E("serial0 thread fail");
    if (serial0_dev)
        rt_device_set_rx_indicate(serial0_dev, serial0_rx_cb);
}

static void serial1_init(uint32_t speed)
{
    static rt_thread_t serial_thread = RT_NULL;

    serial1_rx_sem = rt_sem_create(AUX1_UART, 0, RT_IPC_FLAG_FIFO);
    serial1_set_speed(speed);
    serial_thread = rt_thread_create(AUX1_UART, serial1_rx_thread, RT_NULL, SERIAL_RX_STACK, SERIAL_RX_PRIORITY, 10);
    if (serial_thread)
        rt_thread_startup(serial_thread);
    else
        LOG_E("serial1 thread fail");
    if (serial1_dev)
        rt_device_set_rx_indicate(serial1_dev, serial1_rx_cb);
}

static void serial2_init(uint32_t speed)
{
    static rt_thread_t serial_thread = RT_NULL;

    serial2_rx_sem = rt_sem_create(AUX2_UART, 0, RT_IPC_FLAG_FIFO);
    serial2_set_speed(speed);
    serial_thread = rt_thread_create(AUX2_UART, serial2_rx_thread, RT_NULL, SERIAL_RX_STACK, SERIAL_RX_PRIORITY, 10);
    if (serial_thread)
        rt_thread_startup(serial_thread);
    else
        LOG_E("serial2 thread fail");
    if (serial2_dev)
        rt_device_set_rx_indicate(serial2_dev, serial2_rx_cb);
}

static int serials_init()
{
    serial0_init(serial_speeds[settings.serial0_speed]);
    serial1_init(serial_speeds[settings.serial1_speed]);
    serial2_init(serial_speeds[settings.serial2_speed]);
    return RT_EOK;
}

INIT_COMPONENT_EXPORT(serials_init);
