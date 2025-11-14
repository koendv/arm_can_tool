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
#include <at32f402_405_can.h>

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


rt_err_t canbus_set_filter(uint8_t bank, uint32_t id, uint32_t mask, uint8_t mode, uint8_t scale, uint8_t ide, uint8_t rtr)
{
    if (can_hw_filter.count >= MAX_CAN_HW_FILTER)
    {
        LOG_E("filter bank full");
        return -RT_ERROR;
    }

    for (int i = 0; i < can_hw_filter.count; i++)
    {
        if (can_hw_filter.filter[i].bank == bank)
        {
            LOG_E("filter bank %d already configured", bank);
            return -RT_ERROR;
        }
    }

    can_hw_filter.filter[can_hw_filter.count].bank  = bank;
    can_hw_filter.filter[can_hw_filter.count].id    = id;
    can_hw_filter.filter[can_hw_filter.count].mask  = mask;
    can_hw_filter.filter[can_hw_filter.count].mode  = mode;
    can_hw_filter.filter[can_hw_filter.count].scale = scale;
    can_hw_filter.filter[can_hw_filter.count].ide   = ide;
    can_hw_filter.filter[can_hw_filter.count].rtr   = rtr;
    can_hw_filter.count++;

    LOG_D("filter added: bank=%d, id=0x%08lx, mask=0x%08lx, ide=%s, rtr=%s", bank, id, mask, ide ? "ext" : "std", rtr ? "rtr" : "data");

    return RT_EOK;
}

rt_err_t canbus_begin_filter(void)
{
    memset(&can_hw_filter, 0, sizeof(can_hw_filter));
    return RT_EOK;
}

#if 1

/* implement filters in rt-thread - 32-bit filters */
rt_err_t canbus_end_filter(void)
{
    rt_err_t res = RT_EOK;

    if (!can_dev) return -RT_ERROR;

    // Use RT-Thread filter configuration
    struct rt_can_filter_item   items[MAX_CAN_HW_FILTER] = {0};
    struct rt_can_filter_config cfg                      = {0, 1, items};

    // Configure each filter using RT-Thread API
    for (int i = 0; i < can_hw_filter.count; i++)
    {
        can_hw_filter_t *filter = &can_hw_filter.filter[i];

        if (filter->bank >= 14)
        {
            LOG_E("Invalid filter bank %d, skipping", filter->bank);
            continue;
        }

        items[i].hdr_bank = filter->bank;
        items[i].id       = filter->id;
        items[i].mask     = filter->mask;
        items[i].mode     = filter->mode ? 1 : 0; // 0=mask, 1=list
        items[i].ide      = filter->ide ? 1 : 0;  // 0=std, 1=ext
        items[i].rtr      = filter->rtr ? 1 : 0;  // 0=data, 1=remote

        LOG_D("Filter bank %d: id=0x%08lx mask=0x%08lx mode=%s ide=%s rtr=%s",
              filter->bank, filter->id, filter->mask,
              filter->mode ? "LIST" : "MASK",
              filter->ide ? "EXT" : "STD",
              filter->rtr ? "REMOTE" : "DATA");
    }

    cfg.count = can_hw_filter.count;
    res       = rt_device_control(can_dev, RT_CAN_CMD_SET_FILTER, &cfg);

    if (res == RT_EOK)
    {
        LOG_I("Hardware filters configured: %d banks", can_hw_filter.count);
    }
    else
    {
        LOG_E("Failed to set hardware filters: %d", res);
    }

    return res;
}

#else
/* implement filters using at32 hal: 32-bit filters and 16-bit filters */
/* 16-bit filters gives double the number of filters, but only for standard id's, not extended */

rt_err_t canbus_end_filter(void)
{
    can_filter_init_type filter_init;

    // Initialize with default parameters
    can_filter_default_para_init(&filter_init);

    // Configure each filter bank using direct AT32 HAL
    for (int i = 0; i < can_hw_filter.count; i++)
    {
        can_hw_filter_t *filter = &can_hw_filter.filter[i];

        // Validate bank number
        if (filter->bank >= 14)
        {
            LOG_E("Invalid filter bank %d, skipping", filter->bank);
            continue;
        }

        // Configure filter parameters
        filter_init.filter_activate_enable = TRUE;
        filter_init.filter_number          = filter->bank;
        filter_init.filter_mode            = filter->mode ? CAN_FILTER_MODE_ID_LIST : CAN_FILTER_MODE_ID_MASK;
        filter_init.filter_bit             = filter->scale ? CAN_FILTER_16BIT : CAN_FILTER_32BIT;
        filter_init.filter_fifo            = CAN_FILTER_FIFO0;

        // Calculate filter ID and mask with proper IDE/RTR bit handling
        // This matches RT-Thread's bit shifting for compatibility
        uint32_t filter_id   = 0;
        uint32_t filter_mask = 0;

        if (filter->ide == CAN_ID_STANDARD)
        {
            // Standard ID: shift left by 21 bits for filter register
            // RT-Thread uses: id << 18 for high16, then additional shifting
            filter_id   = filter->id << 21;
            filter_mask = filter->mask << 21;

            // Set IDE bit to 0 (standard) and RTR bit
            filter_id |= (filter->rtr << 1); // RTR at bit 1
            // IDE bit is 0 for standard (already 0)

            // Mask the IDE bit to filter by frame type
            filter_mask |= (1 << 2); // Mask IDE bit
            filter_mask |= (1 << 1); // Mask RTR bit
        }
        else
        {
            // Extended ID: shift left by 3 bits for filter register
            // RT-Thread uses: id << 3
            filter_id   = filter->id << 3;
            filter_mask = filter->mask << 3;

            // Set IDE bit to 1 (extended) and RTR bit
            filter_id |= (1 << 2);           // IDE at bit 2
            filter_id |= (filter->rtr << 1); // RTR at bit 1

            // Mask the IDE and RTR bits
            filter_mask |= (1 << 2); // Mask IDE bit
            filter_mask |= (1 << 1); // Mask RTR bit
        }

        // Set filter ID and mask based on 16-bit vs 32-bit mode
        if (filter_init.filter_bit == CAN_FILTER_32BIT)
        {
            // 32-bit filter: single ID and mask
            filter_init.filter_id_high   = (filter_id >> 16) & 0xFFFF;
            filter_init.filter_id_low    = filter_id & 0xFFFF;
            filter_init.filter_mask_high = (filter_mask >> 16) & 0xFFFF;
            filter_init.filter_mask_low  = filter_mask & 0xFFFF;
        }
        else
        {
            // 16-bit filter: two separate filters in one bank
            // For 16-bit mode, we configure two 16-bit filters
            // Filter 1 uses high 16 bits, Filter 2 uses low 16 bits
            filter_init.filter_id_high   = (filter_id >> 16) & 0xFFFF;
            filter_init.filter_id_low    = filter_id & 0xFFFF;
            filter_init.filter_mask_high = (filter_mask >> 16) & 0xFFFF;
            filter_init.filter_mask_low  = filter_mask & 0xFFFF;
        }

        // Apply the filter configuration
        can_filter_init(CAN1, &filter_init);

        LOG_D("Filter bank %d: id=0x%08lx mask=0x%08lx mode=%s scale=%s ide=%s rtr=%s",
              filter->bank, filter->id, filter->mask,
              filter->mode ? "LIST" : "MASK",
              filter->scale ? "16BIT" : "32BIT",
              filter->ide ? "EXT" : "STD",
              filter->rtr ? "REMOTE" : "DATA");
    }

    // Disable any unused filter banks to save power
    for (uint8_t bank = 0; bank < 14; bank++)
    {
        uint8_t bank_used = 0;
        for (int i = 0; i < can_hw_filter.count; i++)
        {
            if (can_hw_filter.filter[i].bank == bank)
            {
                bank_used = 1;
                break;
            }
        }

        if (!bank_used)
        {
            filter_init.filter_activate_enable = FALSE;
            filter_init.filter_number          = bank;
            can_filter_init(CAN1, &filter_init);
        }
    }

    LOG_I("Hardware filters configured: %d banks using AT32 HAL", can_hw_filter.count);
    return RT_EOK;
}

#endif

/* pass all frames - disable filtering */
rt_err_t canbus_pass_all(void)
{
    /* ID=0, MASK=0, MODE=MASK passes everything */
    canbus_begin_filter();
    canbus_set_filter(0, 0x0, 0x0, 0, 0, 0, 0);
    canbus_end_filter();
    LOG_I("All frames accepted");
    return RT_EOK;
}

/* block all frames - enable filtering but allow nothing */
rt_err_t canbus_block_all(void)
{
    /* ID=0, MASK=0xFFFFFFFF, MODE=MASK blocks everything */
    canbus_begin_filter();
    canbus_set_filter(0, 0x0, 0xFFFFFFFF, 0, 0, 0, 0);
    canbus_end_filter();
    LOG_I("All frames blocked");
    return RT_EOK;
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

#if 0
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
#endif

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

#if 0
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

#endif
