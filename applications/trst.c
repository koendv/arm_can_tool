#include <rtthread.h>
#include <rtdevice.h>
#include <finsh.h>
#include <string.h>
#include <drv_gpio.h>
#include "pins.h"

/*
 * setting and clearing target reset form the rt-thread command line.
 *
 * if no target connected, vio is zero.
 * to enable vio if no target is connected:
 * (gdb) mon tpwr ena
 */


static int cmd_trst(int argc, char **argv)
{
    crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
    //rcc_periph_clock_enable(RCC_GPIOB);
    rt_pin_mode(TARGET_RST_OUT_PIN, PIN_MODE_OUTPUT);
    rt_pin_mode(TARGET_RST_IN_PIN, PIN_MODE_INPUT);
    if (argc == 2 && !strncmp(argv[1], "low", strlen(argv[1])))
    {
        rt_kprintf("trst_out low\r\n");
        rt_pin_write(TARGET_RST_OUT_PIN, PIN_LOW);
    }
    else if (argc == 2 && !strncmp(argv[1], "high", strlen(argv[1])))
    {
        rt_kprintf("trst_out high\r\n");
        rt_pin_write(TARGET_RST_OUT_PIN, PIN_HIGH);
    }
    rt_thread_delay(10);
    if (rt_pin_read(TARGET_RST_IN_PIN) == PIN_HIGH)
        rt_kprintf("trst_in high\r\n");
    else if (rt_pin_read(TARGET_RST_IN_PIN) == PIN_LOW)
        rt_kprintf("trst_in low\r\n");
    else
        rt_kprintf("trst_in ?\r\n");
}

MSH_CMD_EXPORT_ALIAS(cmd_trst, trst, reset target);
