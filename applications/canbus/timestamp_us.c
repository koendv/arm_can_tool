#include <rtthread.h>
#include <rtconfig.h>
#include "timestamp_us.h"

#ifndef RT_USING_CPUTIME_CORTEXM
#error RT_USING_CPUTIME_CORTEXM needed
#endif

uint64_t timestamp_us_scale_factor = 0;

static int timestamp_us_init(void)
{
    if (system_core_clock == 0)
        return -RT_ERROR;
    /* calculate scale factor once, at boot */
    timestamp_us_scale_factor = ((uint64_t)1 << 32) * 1000000ULL / system_core_clock;
    return RT_EOK;
}

INIT_PREV_EXPORT(timestamp_us_init);
