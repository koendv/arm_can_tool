#ifndef JTAG_TAP_H
#define JTAG_TAP_H

#include "tap_swdp.h"

#define TCK_DELAY SWCLK_DELAY
#define TCK_HIGH  SWCLK_HIGH
#define TCK_LOW   SWCLK_LOW

#define TMS_MASK TAP_SWDIO_MASK

#define TDI_HIGH()                 \
    do {                           \
        GPIOA->scr = TAP_TDI_MASK; \
    } while (0)

#define TDI_LOW()                        \
    do {                                 \
        GPIOA->scr = TAP_TDI_MASK << 16; \
    } while (0)

#define TDI_SET(x)                           \
    do {                                     \
        if (x)                               \
            GPIOA->scr = TAP_TDI_MASK;       \
        else                                 \
            GPIOA->scr = TAP_TDI_MASK << 16; \
    } while (0)

#define TDO_GET() ((GPIOA->idt & TAP_TDO_MASK) != 0)

#define TMS_SET(x)                       \
    do {                                 \
        if (x)                           \
            GPIOA->scr = TMS_MASK;       \
        else                             \
            GPIOA->scr = TMS_MASK << 16; \
    } while (0)

#define TMS_LOW()                    \
    do {                             \
        GPIOA->scr = TMS_MASK << 16; \
    } while (0)

#endif
