/* rt-thread driver for AT24C256C 256 kbit eeprom */

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
#define PAGE_SIZE   64
#define MEMORY_SIZE 32768
#define WRITE_MS    10

int32_t at24_write(uint32_t data_address, const uint8_t *data, uint32_t data_size)
{
    struct rt_i2c_bus_device *i2c_bus = RT_NULL;
    struct rt_i2c_msg         msg[1];
    uint8_t                   buf[PAGE_SIZE + 2];
    uint32_t                  data_idx;
    uint32_t                  buf_idx;

    if (data_address + data_size > MEMORY_SIZE)
        data_size = MEMORY_SIZE - data_address;

    i2c_bus = rt_i2c_bus_device_find(I2C_BUS);
    if (i2c_bus == RT_NULL)
    {
        LOG_E("no bus");
        return -RT_ERROR;
    }

    rt_thread_mdelay(WRITE_MS);
    data_idx = 0;
    while (data_idx < data_size)
    {
        buf[0]  = data_address >> 8 & 0xff;
        buf[1]  = data_address & 0xff;
        buf_idx = 2;

        while ((buf_idx < sizeof(buf)) && (data_idx < data_size))
        {
            buf[buf_idx++] = data[data_idx++];
            data_address++;
        }
        /* write to i2c */
        msg[0].addr  = I2C_ADDRESS;
        msg[0].flags = RT_I2C_WR;
        msg[0].len   = buf_idx;
        msg[0].buf   = buf;
        if (rt_i2c_transfer(i2c_bus, msg, 1) != 1)
        {
            LOG_E("i2c write error");
            return data_idx;
        }
        rt_thread_mdelay(WRITE_MS);
    }
    return data_idx;
}

int32_t at24_read(uint32_t data_address, uint8_t *data, uint32_t data_size)
{
    struct rt_i2c_bus_device *i2c_bus = RT_NULL;
    struct rt_i2c_msg         msg[2];
    uint8_t                   buf[2];

    memset(data, 0, data_size);
    if (data_address + data_size > MEMORY_SIZE)
        data_size = MEMORY_SIZE - data_address;

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
        return 0;
    }

    return data_size;
}

void at24c256_test()
{
    char     wr_str[256] = {"the quick brown fox jumps over a lazy dog"};
    char     rd_str[256];
    uint32_t retval;

    rt_kprintf("at24c256\r\n");

    rt_kprintf("write\r\n");
    retval = at24_write(0, wr_str, sizeof(wr_str));
    rt_kprintf("retval: %d\r\n", retval);

    rt_kprintf("read\r\n");
    retval = at24_read(0, rd_str, sizeof(rd_str));
    rt_kprintf("retval: %d\r\n", retval);

    int32_t comp = strncmp(wr_str, rd_str, sizeof(wr_str));
    rt_kprintf("%d %s\r\n", comp, rd_str);
    if (comp == 0) rt_kprintf("ok\r\n");

    return;
}

MSH_CMD_EXPORT(at24c256_test, test eeprom);
