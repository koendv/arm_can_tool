#include <rtthread.h>
#include <rtdevice.h>
#include <string.h>
#include "romdisk.h"

#define DBG_TAG "romdisk"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#define ROMDISK_SECTOR_SIZE 512

/* romdisk */
struct rt_romdisk_device
{
    struct rt_device parent; /* RT-Thread device (must be first) */

    /* ROM Disk specific data */
    const struct romdisk_sparse_sector *sparse_table;
    uint32_t                            sparse_count;
    uint32_t                            total_sectors;
};

static rt_err_t   rt_romdisk_init(rt_device_t dev);
static rt_err_t   rt_romdisk_open(rt_device_t dev, rt_uint16_t oflag);
static rt_err_t   rt_romdisk_close(rt_device_t dev);
static rt_ssize_t rt_romdisk_write(rt_device_t dev, rt_off_t pos, const void *buffer, rt_size_t size);
static rt_ssize_t rt_romdisk_read(rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size);
static rt_err_t   rt_romdisk_control(rt_device_t dev, int cmd, void *args);


#ifdef RT_USING_DEVICE_OPS
const static struct rt_device_ops romdisk_ops =
    {
        rt_romdisk_init,
        rt_romdisk_open,
        rt_romdisk_close,
        rt_romdisk_read,
        rt_romdisk_write,
        rt_romdisk_control};
#endif

/* RT-Thread Device Driver Interface */
static rt_err_t rt_romdisk_init(rt_device_t dev)
{
    return RT_EOK;
}

static rt_err_t rt_romdisk_open(rt_device_t dev, rt_uint16_t oflag)
{
    return RT_EOK;
}

static rt_err_t rt_romdisk_close(rt_device_t dev)
{
    return RT_EOK;
}

/* read sparse romdisk */
static rt_ssize_t rt_romdisk_read(rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size)
{
    struct rt_romdisk_device *romdisk      = (struct rt_romdisk_device *)dev;
    uint32_t                  sector       = (uint32_t)pos;
    uint32_t                  sector_count = (uint32_t)size;
    uint8_t                  *buf          = (uint8_t *)buffer;

    /* Range checking (similar to romdisk) */
    if (sector >= romdisk->total_sectors)
    {
        LOG_E("read sector %u out of range", sector);
        return 0;
    }

    if (sector + sector_count > romdisk->total_sectors)
    {
        sector_count = romdisk->total_sectors - sector;
    }

    /* Zero buffer first */
    rt_memset(buf, 0, sector_count * ROMDISK_SECTOR_SIZE);

    /* Sparse sector lookup */
    for (uint32_t i = 0; i < sector_count; i++)
    {
        uint32_t lba = sector + i;
        uint8_t *dst = buf + i * ROMDISK_SECTOR_SIZE;

        for (uint32_t j = 0; j < romdisk->sparse_count; j++)
        {
            if (romdisk->sparse_table[j].lba == lba)
            {
                rt_memcpy(dst, romdisk->sparse_table[j].data, ROMDISK_SECTOR_SIZE);
                break;
            }
        }
    }

    LOG_D("read sector: %u size: %u sectors", sector, sector_count);

    return sector_count;
}

static rt_ssize_t rt_romdisk_write(rt_device_t dev, rt_off_t pos, const void *buffer, rt_size_t size)
{
    /* silently drop writes */
    return size;
}


static rt_err_t rt_romdisk_control(rt_device_t dev, int cmd, void *args)
{
    struct rt_romdisk_device *romdisk = (struct rt_romdisk_device *)dev;

    RT_ASSERT(dev != RT_NULL);

    switch (cmd)
    {
    case RT_DEVICE_CTRL_BLK_GETGEOME: {
        struct rt_device_blk_geometry *geometry;

        geometry = (struct rt_device_blk_geometry *)args;
        if (geometry == RT_NULL) return -RT_ERROR;

        geometry->bytes_per_sector = ROMDISK_SECTOR_SIZE;
        geometry->block_size       = ROMDISK_SECTOR_SIZE;
        geometry->sector_count     = romdisk->total_sectors;
        return RT_EOK;
    }
    case RT_DEVICE_CTRL_BLK_SYNC:
        /* Nothing to sync for read-only */
        return RT_EOK;

    case RT_DEVICE_CTRL_BLK_ERASE:
        /* Read-only - cannot erase */
        return -RT_ERROR;

    case RT_DEVICE_CTRL_BLK_PARTITION:
        /* Report no partitions */
        return -RT_ENOSYS;

    default:
        /* Let parent handle other commands */
        return -RT_ENOSYS;
    }
}

/* public API */
int rt_romdisk_create(const char *name, const struct romdisk_sparse_sector *sparse_table, uint32_t sparse_count, uint32_t total_sectors)
{
    rt_err_t                  result = RT_EOK;
    rt_device_t               device;
    struct rt_romdisk_device *romdisk_dev;

    RT_ASSERT(name != RT_NULL);
    RT_ASSERT(sparse_table != RT_NULL);
    RT_ASSERT(total_sectors > 0);

    /* Check if device already exists */
    if (rt_device_find(name) != RT_NULL)
    {
        LOG_E("Device '%s' already exists", name);
        return -RT_ERROR;
    }

    romdisk_dev = (struct rt_romdisk_device *)rt_malloc(sizeof(struct rt_romdisk_device));
    if (romdisk_dev == RT_NULL)
    {
        LOG_E("%s: no memory for romdisk control block", name);
        return -RT_ENOMEM;
    }
    rt_memset(romdisk_dev, 0, sizeof(struct rt_romdisk_device));

    /* device type */
    romdisk_dev->parent.type = RT_Device_Class_Block;

    /* set up ops */
#ifdef RT_USING_DEVICE_OPS
    romdisk_dev->parent.ops = &romdisk_ops;
#else
    romdisk_dev->parent.init    = rt_romdisk_init;
    romdisk_dev->parent.open    = rt_romdisk_open;
    romdisk_dev->parent.read    = rt_romdisk_read;
    romdisk_dev->parent.write   = rt_romdisk_write;
    romdisk_dev->parent.close   = rt_romdisk_close;
    romdisk_dev->parent.control = rt_romdisk_control;
#endif

    /* no callback no private data */
    romdisk_dev->parent.user_data   = RT_NULL;
    romdisk_dev->parent.rx_indicate = RT_NULL;
    romdisk_dev->parent.tx_complete = RT_NULL;

    /* romdisk data */
    romdisk_dev->sparse_table  = sparse_table;
    romdisk_dev->sparse_count  = sparse_count;
    romdisk_dev->total_sectors = total_sectors;

    /* register romdisk as a block device */
    result = rt_device_register((rt_device_t)romdisk_dev, name, RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_STANDALONE);
    if (result != RT_EOK)
    {
        LOG_E("Failed to register ROM disk '%s'", name);
    }

    return result;
}
