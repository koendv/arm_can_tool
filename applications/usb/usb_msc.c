#include <rtthread.h>
#define DBG_TAG "MSC"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>
#include <string.h>
#include <stdbool.h>
#include <usb_core.h>
#include <usbd_msc.h>
#include <usb_msc.h>
#include <usb_desc.h>
#include <serials.h>
#include "platform.h"

#define USB_SHELL_DEVICE_NAME "usb-acm0"

static rt_device_t block_device = NULL;
static uint32_t    block_num    = 0;
static uint32_t    block_size   = 512;
static bool        shell_dtr    = false;

/* redirect shell from hardware serial to usb serial */
static void usb_shell_enable(bool onoff)
{
    finsh_set_device(onoff ? USB_SHELL_DEVICE_NAME : RT_CONSOLE_DEVICE_NAME);
    rt_console_set_device(onoff ? USB_SHELL_DEVICE_NAME : RT_CONSOLE_DEVICE_NAME);
    return;
}

/* called when connected/disconnected changes */
void msc_set_dtr(uint8_t intf, bool dtr)
{
    if (intf == CDC0_INTF)
    {
        shell_dtr = dtr;
        if (serial_event)
            rt_event_send(serial_event, EVENT_MASK_CDC0_DTR);
    }
}

static void msc_thread(void *parameter)
{
    rt_uint32_t recv_set;

    (void)parameter;

    while (1)
    {
        if (rt_event_recv(serial_event,
                          EVENT_MASK_CDC0_DTR,
                          RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR,
                          rt_tick_from_millisecond(5000),
                          &recv_set)
            == RT_EOK)
        {
            if (recv_set & EVENT_MASK_CDC0_DTR)
                usb_shell_enable(shell_dtr);
        }
    }
}

void msc_on_configured(uint8_t busid)
{
    (void)busid;

    rt_thread_t thread = rt_thread_create("msc_thread",
                                          msc_thread,
                                          RT_NULL,
                                          1024,
                                          25,
                                          10);
    if (!thread)
    {
        LOG_E("msc_thread fail");
        return;
    }
    rt_thread_startup(thread);

    LOG_D("msc_on_configured");
}

void usbd_msc_get_cap(uint8_t busid, uint8_t lun, uint32_t *block_num_out, uint32_t *block_size_out)
{
    struct rt_device_blk_geometry geo;
    LOG_D("usbd_msc_get_cap");

    if (block_num_out == NULL || block_size_out == NULL)
    {
        LOG_E("NULL block_num, block_size");
        return;
    }

    *block_num_out  = block_num;
    *block_size_out = block_size;

    LOG_I("%d blocks of %d bytes", block_num, block_size);
}

int usbd_msc_sector_read(uint8_t busid, uint8_t lun,
                         uint32_t sector, uint8_t *buffer,
                         uint32_t length)
{
    rt_size_t sectors_read = 0;

    LOG_D("usbd_msc_sector_read sector: %d length : %d", sector, length);

    /* zero the buffer first */
    memset(buffer, 0, length);

    if (!block_device)
    {
        LOG_E("NULL device");
        return 0;
    }

    if (!buffer)
    {
        LOG_E("NULL buffer");
        return 0;
    }

    if (block_size == 0)
    {
        LOG_E("0 block size");
        return 0;
    }

    rt_size_t sector_count = length / block_size;

    if (length % block_size != 0)
        LOG_E("read length %d not multiple of %d", length, block_size);

    if (sector + sector_count > block_num)
        sector_count = block_num - sector;

    if (sector_count != 0)
        sectors_read = rt_device_read(block_device, sector, buffer, sector_count);

    if (sectors_read != sector_count)
        LOG_E("partial read at sector %u: got %u of %u sectors", sector, sectors_read, sector_count);

    /* always return 0, else stack times out */
    return 0;
}

int usbd_msc_sector_write(uint8_t busid, uint8_t lun, uint32_t sector, uint8_t *buffer, uint32_t length)
{
    rt_size_t sectors_written = 0;

    LOG_D("usbd_msc_sector_write sector: %d length : %d", sector, length);

    if (!block_device)
    {
        LOG_E("NULL device");
        return 0;
    }

    if (!buffer)
    {
        LOG_E("NULL buffer");
        return 0;
    }

    if (block_size == 0)
    {
        LOG_E("0 block size");
        return 0;
    }

    rt_size_t sector_count = length / block_size;

    if (length % block_size != 0)
        LOG_E("write length %d not multiple of %d", length, block_size);

    if (sector + sector_count > block_num)
        sector_count = block_num - sector;

    if (sector_count != 0)
        sectors_written = rt_device_write(block_device, sector, buffer, sector_count);

    if (sectors_written != sector_count)
        LOG_E("partial write at sector %u: got %u of %u sectors", sector, sectors_written, sector_count);

    /* always return 0, else stack times out */
    return 0;
}

/* public API: set MSC device name */
int usb_msc_device(char *dev_name)
{
    struct rt_device_blk_geometry geo;

    if (dev_name == NULL)
    {
        block_device = NULL;
        return RT_EOK;
    }

    rt_device_t dev = rt_device_find(dev_name);
    if (dev == NULL)
    {
        LOG_E("device '%s' not found", dev_name);
        return -RT_ERROR;
    }

    if (dev->type != RT_Device_Class_Block)
    {
        LOG_E("device '%s' not a block device", dev_name);
        return -RT_ERROR;
    }

    if ((dev->ref_count == 0) && (rt_device_open(dev, RT_DEVICE_OFLAG_RDWR) != RT_EOK))
    {
        LOG_E("device '%s' open fail", dev_name);
        return -RT_ERROR;
    }

    /* get geometry from block device */
    if (rt_device_control(dev, RT_DEVICE_CTRL_BLK_GETGEOME, &geo) == RT_EOK)
    {
        if (geo.bytes_per_sector == 0)
        {
            LOG_E("device reported block size 0");
            return -RT_ERROR;
        }
        if ((geo.bytes_per_sector & (geo.bytes_per_sector - 1)) != 0)
        {
            LOG_E("device reported block size %d not a power of 2", geo.bytes_per_sector);
            return -RT_ERROR;
        }
        block_num  = geo.sector_count;
        block_size = geo.bytes_per_sector;
    }

    block_device = dev;

    LOG_I("using %s block device", dev_name);

    return RT_EOK;
}
