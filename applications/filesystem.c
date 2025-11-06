/* 
 * file systems.
 * /flash : 16 mbyte spi flash W25Q128
 * /sdcard : sd card, if plugged in
 */

#include <rtthread.h>
#include <dfs_fs.h>
#include <dfs_romfs.h>
#include <drv_spi.h>
#include <dev_spi_flash_sfud.h>
#include <fal.h>
#include "sdcard.h"

#if DFS_FILESYSTEMS_MAX < 4
#error "Please define DFS_FILESYSTEMS_MAX more than 4"
#endif
#if DFS_FILESYSTEM_TYPES_MAX < 4
#error "Please define DFS_FILESYSTEM_TYPES_MAX more than 4"
#endif

#define FLASH0_SPI_BUS     "spi2"
#define FLASH0_SPI_DEV     "spi20"
#define FLASH0_CS_GPIO     GPIOA
#define FLASH0_CS_GPIO_PIN GPIO_PINS_8
#define FS_PARTITION_NAME  "part0"
#define FS_TYPE            "lfs"
#define FS_DIR             "/flash"

const struct romfs_dirent _romfs_root[] =
    {
        {ROMFS_DIRENT_DIR, "sdcard", RT_NULL, 0},
        {ROMFS_DIRENT_DIR,  "flash", RT_NULL, 0},
};

const struct romfs_dirent romfs_root =
    {
        ROMFS_DIRENT_DIR,
        "/",
        (rt_uint8_t *)_romfs_root,
        sizeof(_romfs_root) / sizeof(_romfs_root[0]),
};

static int rom_mount()
{
    if (dfs_mount(RT_NULL, "/", "rom", 0, &(romfs_root)) != 0)
    {
        LOG_E("rom mount to '/' failed!");
        return -RT_ERROR;
    };
    LOG_I("rom mount to '/'");
    return RT_EOK;
}

static int flash_mount()
{
    struct rt_device *part0_dev = RT_NULL;
    rt_hw_spi_device_attach(FLASH0_SPI_BUS, FLASH0_SPI_DEV, FLASH0_CS_GPIO, FLASH0_CS_GPIO_PIN);
    if (RT_NULL == rt_sfud_flash_probe("norflash0", "spi20"))
    {
        LOG_E("flash not found\r\n");
        return -RT_ERROR;
    };
    fal_init();
    struct rt_device *flash_dev = fal_mtd_nor_device_create(FS_PARTITION_NAME);
    if (!flash_dev)
    {
        LOG_E("create block device failed on partition " FS_PARTITION_NAME);
        return -RT_ERROR;
    };
    if (dfs_mount(flash_dev->parent.name, FS_DIR, FS_TYPE, 0, 0) == 0)
    {
        LOG_I("spi flash mount to " FS_DIR);
    }
    else
    {
        /* first time run. create filesystem */
        LOG_E("Initializing " FS_PARTITION_NAME " filesystem");
        dfs_mkfs(FS_TYPE, FS_PARTITION_NAME);
        if (dfs_mount(FS_PARTITION_NAME, FS_DIR, FS_TYPE, 0, 0) == RT_EOK)
        {
            LOG_I("spi flash mount to " FS_DIR);
        }
        else
        {
            LOG_E("spi flash failed to mount to " FS_DIR);
            LOG_E("run mkfs -t lfs part0");
            return -RT_ERROR;
        }
    }
    return RT_EOK;
}

static int filesystem_mount(void)
{
    rom_mount();
    flash_mount();
    sdcard_init();
    return RT_EOK;
}

INIT_APP_EXPORT(filesystem_mount);

