#include <rtthread.h>
#include <rtdevice.h>
#include <rtconfig.h>
#include "drv_common.h"
#include "drv_gpio.h"

#ifdef BSP_USING_HARD_I2C1
#define I2C_BUS "hwi2c1" /* i2c bus */
#else
#define I2C_BUS "i2c1"   /* i2c bus */
#endif

struct rt_i2c_bus_device *i2c_bus;


int i2c_probe(char addr)
{
    unsigned char cmd[1];
    cmd[0] = 0;

    struct rt_i2c_msg msgs;
    msgs.addr  = addr;
    msgs.flags = RT_I2C_WR;
    msgs.buf   = cmd;
    msgs.len   = 0;

    return rt_i2c_transfer(i2c_bus, &msgs, 1);
}


void i2c_scan(int argc, char **argv)
{
    char *i2c_bus_name = I2C_BUS;
    if (argc == 2) i2c_bus_name = argv[1];
    i2c_bus = rt_i2c_bus_device_find(i2c_bus_name);
    if (i2c_bus == RT_NULL)
    {
        rt_kprintf(I2C_BUS " not found\r\n");
        return;
    }
    rt_uint8_t start_addr = 0x08;
    rt_uint8_t stop_addr  = 0x7f;
    rt_kputs("    00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F");
    rt_uint8_t pos = start_addr <= 0xF ? 0x00 : start_addr & 0xF;
    for (; pos < stop_addr; pos++)
    {
        if ((pos & 0xF) == 0)
        {
            rt_kprintf("\n%02X: ", pos);
        }
        if (pos < start_addr)
        {
            rt_kputs("   ");
            continue;
        }
        if (i2c_probe(pos) == 1)
        {
            rt_kprintf("%02X", pos);
        }
        else
        {
            rt_kputs("--");
        }
        rt_kputs(" ");
    }
    rt_kputs("\n");
}

MSH_CMD_EXPORT(i2c_scan, i2c tools);

