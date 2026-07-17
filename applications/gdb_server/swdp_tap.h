#ifndef SWDP_TAP_H
#define SWDP_TAP_H

#include "tap_common.h"

#define SWCLK_HIGH()             \
    do {                         \
        GPIOA->scr = SWCLK_MASK; \
    } while (0)

#define SWCLK_LOW()                    \
    do {                               \
        GPIOA->scr = SWCLK_MASK << 16; \
    } while (0)

#define SWDIO_READ() ((GPIOA->idt & SWDIO_MASK) != 0)

#define SWDIO_MODE_DRIVE()                      \
    do                                          \
    {                                           \
        GPIOA->scr   = SWDIO_DIR_MASK;          \
        GPIOA->cfgr |= (1U << (SWDIO_PIN * 2)); \
    } while (0)

#define SWDIO_MODE_FLOAT()                       \
    do                                           \
    {                                            \
        GPIOA->scr   = SWDIO_DIR_MASK << 16;     \
        GPIOA->cfgr &= ~(3U << (SWDIO_PIN * 2)); \
    } while (0)

#if 0
#define SWDIO_SET(x)                       \
    do {                                   \
        if (x)                             \
            GPIOA->scr = SWDIO_MASK;       \
        else                               \
            GPIOA->scr = SWDIO_MASK << 16; \
    } while (0)
#else
/* branchless */
#define SWDIO_SET(x) GPIOA->scr = SWDIO_MASK << ((1 - !!(x)) << 4)
#endif

/* guard read */
#define SWDIO_GUARD() ((void)GPIOA->idt)

#define SWCLK_DELAY()                                       \
    {                                                       \
        uint32_t start = DWT->CYCCNT;                       \
        while ((DWT->CYCCNT - start) < target_clk_divider); \
    }

#endif
