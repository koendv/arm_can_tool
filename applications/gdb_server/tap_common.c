#include <rtthread.h>
#define DBG_TAG "SWD"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>
#include <drv_gpio.h>

#include "tap_common.h"

uint32_t target_clk_divider = SYSTEM_CORE_CLOCK / 2 / SWD_DEFAULT_CLOCK;

#define SWD_MAX_CLOCK 10000000

/* set bit-banging delay */
uint32_t platform_max_frequency_get(void)
{
    uint32_t f;
    if (target_clk_divider == 0)
        f = SWD_MAX_CLOCK;
    else
        f = SYSTEM_CORE_CLOCK / (2 * target_clk_divider);
    return f;
}

void platform_max_frequency_set(const uint32_t frequency)
{
    if (frequency == 0)
        return;
    if (frequency >= SWD_MAX_CLOCK)
    {
        target_clk_divider = 0;
        return;
    }
    target_clk_divider = SYSTEM_CORE_CLOCK / (2 * frequency);
}

/* log number of bytes of firmware running from zero-wait-state ram */
int tap_init()
{
    extern char _sramfunc;
    extern char _eramfunc;

    LOG_I("ramfunc %d byte", &_eramfunc - &_sramfunc);
    return RT_EOK;
}

INIT_DEVICE_EXPORT(tap_init);
