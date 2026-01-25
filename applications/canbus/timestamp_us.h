#ifndef TIMESTAMP_US_H
#define TIMESTAMP_US_H

#include <stdint.h>
#include "at32f402_405.h"

/* time in microseconds. requires CONFIG_RT_USING_CPUTIME_CORTEXM=y */

extern uint64_t timestamp_us_scale_factor;

static inline uint32_t get_timestamp_us(void)
{
    uint32_t cycles = DWT->CYCCNT;
    uint64_t result = (uint64_t)cycles * timestamp_us_scale_factor;
    return (uint32_t)(result >> 32);
}

#endif
