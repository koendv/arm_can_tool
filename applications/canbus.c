/*
 * canbus.c canbus logger
 * receive can packets. write slcan to usb cdc.
 */

/*
 * test:
 * - set RT_CAN_MODE_LOOPBACK
 * - shell command: canbus_send
 * - console response: "t10080123456789ABCDEF"
 * - shell command: canstat can1
 * - console response: 1 package sent, 1 package received, no errors.
 */

#include <rtthread.h>
#include <rtdevice.h>

#if defined(RT_USING_CAN) && defined(BSP_USING_CAN1)

#include <drv_can.h>
#include <string.h>
#include <usb_cdc.h>
#include <canbus.h>
#include <slcan.h>

#define DBG_TAG "CAN"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>
#include "settings.h"

#define CAN_DEV   "can1"
#define SLCAN_MTU (sizeof("T1111222281122334455667788EA5F\r\n") + 1)

/* canbus hardware filter */
can_hw_filter_bank_t can_hw_filter;

static uint8_t     char_tx_buffer[SLCAN_MTU];
static rt_device_t can_dev          = RT_NULL;
static rt_thread_t can_rx_thread_id = RT_NULL;
static rt_sem_t    can_rx_sem       = RT_NULL;

/* Hardware-level CAN operations */

rt_err_t canbus_send_frame(struct rt_can_msg *msg)
{
    if (!can_dev) return -RT_ERROR;

    rt_size_t sent = rt_device_write(can_dev, 0, msg, sizeof(*msg));
    return (sent > 0) ? RT_EOK : -RT_ERROR;
}

rt_err_t canbus_set_baudrate(uint32_t baudrate)
{
    if (!can_dev) return -RT_ERROR;

    return rt_device_control(can_dev, RT_CAN_CMD_SET_BAUD, &baudrate);
}

rt_err_t canbus_set_autoretransmit(rt_bool_t mode)
{
    // Not implemented
    return -RT_ERROR;
}

rt_err_t canbus_set_std_filter(uint32_t id, uint32_t mask)
{
    if (can_hw_filter.count >= MAX_CAN_HW_FILTER) return -RT_ERROR;
    can_hw_filter.filter[can_hw_filter.count].mask     = mask;
    can_hw_filter.filter[can_hw_filter.count].value    = id;
    can_hw_filter.filter[can_hw_filter.count].extended = RT_FALSE;
    can_hw_filter.count++;
    return RT_EOK;
}

rt_err_t canbus_set_ext_filter(uint32_t id, uint32_t mask)
{
    if (can_hw_filter.count >= MAX_CAN_HW_FILTER) return -RT_ERROR;
    can_hw_filter.filter[can_hw_filter.count].mask     = mask;
    can_hw_filter.filter[can_hw_filter.count].value    = id;
    can_hw_filter.filter[can_hw_filter.count].extended = RT_TRUE;
    can_hw_filter.count++;
    return RT_EOK;
}

rt_err_t canbus_begin_filter(void)
{
    memset(&can_hw_filter, 0, sizeof(can_hw_filter));
    return RT_EOK;
}

rt_err_t canbus_end_filter(void)
{
    rt_err_t                    res                      = RT_EOK;
    struct rt_can_filter_item   items[MAX_CAN_HW_FILTER] = {0};
    struct rt_can_filter_config cfg                      = {0, 1, items};

    if (!can_dev) return -RT_ERROR;
    for (int i = 0; i < can_hw_filter.count; i++)
    {
        items[i].id       = can_hw_filter.filter[i].value;
        items[i].ide      = can_hw_filter.filter[i].extended ? 1 : 0;
        items[i].rtr      = 0;
        items[i].mode     = 0;
        items[i].mask     = can_hw_filter.filter[i].mask;
        items[i].hdr_bank = -1;
    }

    cfg.count = can_hw_filter.count;
    res       = rt_device_control(can_dev, RT_CAN_CMD_SET_FILTER, &cfg);

#if 0
    // Some drivers might require an explicit start command.
    // This is driver-specific.
    if (res == RT_EOK)
    {
        rt_uint32_t cmd_arg = 1;

        res = rt_device_control(can_dev, RT_CAN_CMD_START, &cmd_arg);
    }
#endif

    return res;
}

rt_err_t canbus_set_mode(int mode)
{
    if (!can_dev) return -RT_ERROR;

    if (mode == RT_CAN_MODE_NORMAL || mode == RT_CAN_MODE_LISTEN || mode == RT_CAN_MODE_LOOPBACK)
    {
        rt_device_control(can_dev, RT_CAN_CMD_SET_MODE, (void *)mode);
        return RT_EOK;
    }
    return -RT_ERROR;
}

/* CAN receive */

static rt_err_t can_rx_handler(rt_device_t dev, rt_size_t size)
{
    if (can_rx_sem)
        rt_sem_release(can_rx_sem);
    return RT_EOK;
}

static void can_rx_thread(void *param)
{
    rt_err_t          res;
    struct rt_can_msg rx_msg = {0};
    while (1)
    {
        rx_msg.hdr_index = -1;
        rt_sem_take(can_rx_sem, RT_WAITING_FOREVER);
        if (rt_device_read(can_dev, 0, &rx_msg, sizeof(rx_msg)) > 0 && settings.can1_slcan)
        {
            /* slcan output */
            slcan_parse_frame(&rx_msg);
        }
        rt_thread_yield();
    }
}

#if 1
/* send test packet */
static void canbus_send()
{
    struct rt_can_msg msg = {0};
    rt_size_t         size;

    msg.id      = 0x100;
    msg.ide     = RT_CAN_STDID;
    msg.rtr     = RT_CAN_DTR;
    msg.len     = 8;
    msg.data[0] = 0x01;
    msg.data[1] = 0x23;
    msg.data[2] = 0x45;
    msg.data[3] = 0x67;
    msg.data[4] = 0x89;
    msg.data[5] = 0xAB;
    msg.data[6] = 0xCD;
    msg.data[7] = 0xEF;

    size = rt_device_write(can_dev, 0, &msg, sizeof(msg));
    if (size == 0)
    {
        LOG_E("%s write failed!", CAN_DEV);
    }

    return;
}

#endif

/* filter canbus messages in hardware */
static rt_err_t canbus_set_filter()
{
    rt_err_t res = RT_EOK;

    struct rt_can_filter_item items[5] =
        {
            RT_CAN_FILTER_ITEM_INIT(0x100, 0, 0, 0, 0x700, RT_NULL, RT_NULL),
            RT_CAN_FILTER_ITEM_INIT(0x300, 0, 0, 0, 0x700, RT_NULL, RT_NULL),
            RT_CAN_FILTER_ITEM_INIT(0x211, 0, 0, 0, 0x7ff, RT_NULL, RT_NULL),
            RT_CAN_FILTER_STD_INIT(0x486, RT_NULL, RT_NULL),
            {0x555, 0, 0, 0, 0x7ff, 7}
    };
    struct rt_can_filter_config cfg = {5, 1, items};

    res = rt_device_control(can_dev, RT_CAN_CMD_SET_FILTER, &cfg);
    if (res != RT_EOK)
    {
        LOG_E("canbus %s set filter failed!", CAN_DEV);
        return -RT_ERROR;
    }
    return res;
}

void can1_set_speed(uint32_t speed)
{
    if (can_dev)
    {
        rt_device_control(can_dev, RT_CAN_CMD_SET_BAUD, (void *)speed);
        LOG_I("can1 speed %d", speed);
    }
}

int canbus_init(void)
{
    rt_err_t res;

    can_rx_sem = rt_sem_create("can1rx", 0, RT_IPC_FLAG_FIFO);

    can_dev = rt_device_find(CAN_DEV);
    if (!can_dev)
    {
        LOG_E("device %s not found!", CAN_DEV);
        return -RT_ERROR;
    }

    // set canbus speed before opening the device
    extern const enum CANBAUD can_speeds[]; // XXX
    can1_set_speed(can_speeds[settings.can1_speed]);

    res = rt_device_open(can_dev, RT_DEVICE_FLAG_INT_TX | RT_DEVICE_FLAG_INT_RX);
    if (res != RT_EOK)
    {
        LOG_E("open device %s failed!", CAN_DEV);
        return -RT_ERROR;
    }

    can_rx_thread_id = rt_thread_create("can rx", can_rx_thread, RT_NULL, 1024, 25, 10);
    if (can_rx_thread_id != RT_NULL)
    {
        rt_thread_startup(can_rx_thread_id);
    }
#if 0
    rt_device_control(can_dev, RT_CAN_CMD_SET_MODE, (void *)RT_CAN_MODE_LISTEN); // listen only
#endif
#if 0
    rt_device_control(can_dev, RT_CAN_CMD_SET_MODE, (void *)RT_CAN_MODE_LOOPBACK); // loopback test
#endif
    rt_device_set_rx_indicate(can_dev, can_rx_handler);
    LOG_D("%s canbus init ok", CAN_DEV);
    return RT_EOK;
}

INIT_APP_EXPORT(canbus_init);

#ifdef RT_USING_FINSH
static int canbus_cmd(int argc, char **argv)
{
    if (argc == 2 && !strncmp(argv[1], "send", strlen(argv[1])))
        canbus_send();
    else if (argc == 2 && !strncmp(argv[1], "filter", strlen(argv[1])))
        canbus_set_filter();
    else
    {
        rt_kprintf("canbus (send|filter)\r\n");
        rt_kprintf("canbus send : send canbus packet\r\n");
        rt_kprintf("canbus filter : set hardware filter\r\n");
    }
    return RT_EOK;
}

MSH_CMD_EXPORT_ALIAS(canbus_cmd, canbus, canbus send and filter);
#endif

#endif
