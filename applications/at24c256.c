/*
   rt-thread driver for AT24C256C 256 kbit eeprom

   this driver will also work for AT24C32, AT24C64, AT24C128
   (after changing the #defines) but not for AT32C16 and smaller -
   below 16kbit the address is one byte,
   from 32kbit up the address is two bytes.
 */

#include <rtthread.h>
#include <rtconfig.h>
#include <rtdevice.h>
#include <finsh.h>
#include <at24c256.h>
#include <string.h>

#define DBG_TAG "AT24"
#define DBG_LVL DBG_ERR
#include <rtdbg.h>

#ifdef BSP_USING_HARD_I2C1
#define I2C_BUS "hwi2c1" /* i2c bus */
#else
#define I2C_BUS "i2c1"   /* i2c bus */
#endif

#define I2C_ADDRESS 0x50
#define WRITE_MS    10

int32_t at24_write(uint32_t data_address, const uint8_t *data, uint32_t data_size)
{
    struct rt_i2c_bus_device *i2c_bus = RT_NULL;
    struct rt_i2c_msg         msg[1];
    uint8_t                   buf[EEPROM_PAGE_SIZE + 2];
    uint32_t                  data_idx = 0;
    uint32_t                  bytes_this_write;
    uint32_t                  page_remain;

    if (data == NULL)
    {
        LOG_E("null pointer");
        return -RT_ERROR;
    }

    if (data_address >= EEPROM_MEMORY_SIZE)
    {
        LOG_E("address out of range");
        return -RT_EINVAL;
    }

    if (data_address + data_size > EEPROM_MEMORY_SIZE)
    {
        LOG_E("write truncated");
        data_size = EEPROM_MEMORY_SIZE - data_address;
    }

    i2c_bus = rt_i2c_bus_device_find(I2C_BUS);
    if (i2c_bus == RT_NULL)
    {
        LOG_E("no bus");
        return -RT_ERROR;
    }

    while (data_idx < data_size)
    {
        /* bytes left in current page */
        page_remain      = EEPROM_PAGE_SIZE - (data_address % EEPROM_PAGE_SIZE);
        bytes_this_write = data_size - data_idx;

        /* write at most one page at a time */
        if (bytes_this_write > page_remain)
            bytes_this_write = page_remain;

        /* prepare buffer with address */
        buf[0] = data_address >> 8 & 0xff;
        buf[1] = data_address & 0xff;

        /* copy up to one page of data */
        memcpy(&buf[2], &data[data_idx], bytes_this_write);

        /* write to i2c */
        msg[0].addr  = I2C_ADDRESS;
        msg[0].flags = RT_I2C_WR;
        msg[0].len   = bytes_this_write + 2; /* address bytes + data */
        msg[0].buf   = buf;

        if (rt_i2c_transfer(i2c_bus, msg, 1) != 1)
        {
            LOG_E("i2c write error");
            return data_idx;
        }

        /* update pointers */
        data_idx     += bytes_this_write;
        data_address += bytes_this_write;

        rt_thread_mdelay(WRITE_MS);
    }
    return data_idx;
}

int32_t at24_read(uint32_t data_address, uint8_t *data, uint32_t data_size)
{
    struct rt_i2c_bus_device *i2c_bus = RT_NULL;
    struct rt_i2c_msg         msg[2];
    uint8_t                   buf[2];

    if (data == NULL)
    {
        LOG_E("null pointer");
        return -RT_ERROR;
    }

    memset(data, 0, data_size);

    if (data_address >= EEPROM_MEMORY_SIZE)
    {
        return 0;
    }

    if (data_address + data_size > EEPROM_MEMORY_SIZE)
        data_size = EEPROM_MEMORY_SIZE - data_address;

    i2c_bus = rt_i2c_bus_device_find(I2C_BUS);
    if (i2c_bus == RT_NULL)
    {
        LOG_E(" no bus");
        return -RT_ERROR;
    }

    buf[0] = data_address >> 8 & 0xff;
    buf[1] = data_address & 0xff;

    msg[0].addr  = I2C_ADDRESS;
    msg[0].flags = RT_I2C_WR;
    msg[0].len   = 2;
    msg[0].buf   = buf;
    msg[1].addr  = I2C_ADDRESS;
    msg[1].flags = RT_I2C_RD;
    msg[1].len   = data_size;
    msg[1].buf   = data;

    if (rt_i2c_transfer(i2c_bus, msg, 2) != 2)
    {
        LOG_E("i2c read error");
        return -RT_ERROR;
    }

    return data_size;
}

void at24c256_test()
{
    char     wr_str[256] = {"the quick brown fox jumps over a lazy dog"};
    char     rd_str[256];
    uint32_t retval;

    rt_kprintf("at24c%d\r\n", EEPROM_MEMORY_SIZE / 128);
    rt_kprintf("page Size: %d, memory Size: %d\r\n", EEPROM_PAGE_SIZE, EEPROM_MEMORY_SIZE);

    rt_kprintf("write\r\n");
    retval = at24_write(0, wr_str, sizeof(wr_str));
    rt_kprintf("retval: %d\r\n", retval);

    rt_kprintf("read\r\n");
    retval = at24_read(0, rd_str, sizeof(rd_str));
    rt_kprintf("retval: %d\r\n", retval);

    int32_t comp = strncmp(wr_str, rd_str, sizeof(wr_str));
    rt_kprintf("%s\r\n", rd_str);
    if (comp == 0)
        rt_kprintf("ok\r\n");

    return;
}

MSH_CMD_EXPORT(at24c256_test, test eeprom);
