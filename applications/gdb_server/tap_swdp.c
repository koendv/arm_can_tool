/* This file implements the SW-DP interface. */

#include <rtthread.h>
#define DBG_TAG "SWD"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#include "tap_common.h"
#include "swd.h"
#include "tap_swdp.h"

typedef enum swdio_status_e
{
    SWDIO_STATUS_FLOAT = 0,
    SWDIO_STATUS_DRIVE
} swdio_status_t;

swd_proc_s swd_proc;

OPTIMIZE RAMFUNC uint32_t swdptap_seq_in(size_t clock_cycles);
OPTIMIZE RAMFUNC bool     swdptap_seq_in_parity(uint32_t *ret, size_t clock_cycles);
OPTIMIZE RAMFUNC void     swdptap_seq_out(uint32_t tms_states, size_t clock_cycles);
OPTIMIZE RAMFUNC void     swdptap_seq_out_parity(uint32_t tms_states, size_t clock_cycles);

/*
 * Overall strategy for timing consistency:
 *
 * - Each primitive ends with a falling clock edge
 * - Output is driven after the falling clock edge
 * - Input is read immediately before the rising clock edge
 * - Each primitive assumes it was immediately preceded by a falling clock edge
 *
 * This increases the chances of meeting setup and hold times when the target
 * connection is lower bandwidth (with adequately slower clocks configured).
 */

void swdptap_init(void)
{
    swdptap_platform_init();

    swd_proc.seq_in         = swdptap_seq_in;
    swd_proc.seq_in_parity  = swdptap_seq_in_parity;
    swd_proc.seq_out        = swdptap_seq_out;
    swd_proc.seq_out_parity = swdptap_seq_out_parity;
}

static INLINE OPTIMIZE RAMFUNC void swdptap_turnaround_delay(const swdio_status_t dir)
{
    static swdio_status_t olddir = SWDIO_STATUS_FLOAT;
    /* Don't turnaround if direction not changing */
    if (dir == olddir)
        return;
    olddir = dir;

    if (dir == SWDIO_STATUS_FLOAT)
    {
        SWDIO_MODE_FLOAT();
        SWDIO_GUARD();
    }
    SWCLK_DELAY();

    SWCLK_HIGH();
    SWCLK_DELAY();

    SWCLK_LOW();
    SWCLK_DELAY(); /* guard cycle */
    if (dir == SWDIO_STATUS_DRIVE)
    {
        SWDIO_MODE_DRIVE();
    }
}

static INLINE OPTIMIZE RAMFUNC void swdptap_turnaround_nodelay(const swdio_status_t dir)
{
    static swdio_status_t olddir = SWDIO_STATUS_FLOAT;
    /* Don't turnaround if direction not changing */
    if (dir == olddir)
        return;
    olddir = dir;

    if (dir == SWDIO_STATUS_FLOAT)
    {
        SWDIO_MODE_FLOAT();
        SWDIO_GUARD();
    }
    NOP();

    SWCLK_HIGH();
    NOP();
    NOP();
    SWCLK_LOW();
    NOP(); /* guard cycle */
    if (dir == SWDIO_STATUS_DRIVE)
    {
        SWDIO_MODE_DRIVE();
    }
}

static INLINE OPTIMIZE RAMFUNC uint32_t swdptap_seq_in_delay(size_t clock_cycles)
{
    uint32_t value = 0;

    swdptap_turnaround_delay(SWDIO_STATUS_FLOAT);

    if (!clock_cycles)
        return 0;

    /*
	 * Count down instead of up, because with an up-count, some ARM-GCC
	 * versions use an explicit CMP, missing the optimization of converting
	 * to a faster down-count that uses SUBS followed by BCS/BCC.
	 */
    for (size_t cycle = clock_cycles; cycle--;)
    {
        SWCLK_DELAY();
        const bool bit = SWDIO_READ();
        SWCLK_HIGH();
        SWCLK_DELAY();
        value >>= 1U;
        value  |= (uint32_t)bit << 31U;
        /* Reordering barrier */
        BARRIER();
        SWCLK_LOW();
        /* Reordering barrier */
        BARRIER();
    }
    value >>= (32U - clock_cycles);
    return value;
}

static INLINE OPTIMIZE RAMFUNC uint32_t swdptap_seq_in_nodelay(size_t clock_cycles)
{
    uint32_t value = 0;

    swdptap_turnaround_nodelay(SWDIO_STATUS_FLOAT);

    if (!clock_cycles)
        return 0;

    /*
	 * Count down instead of up, because with an up-count, some ARM-GCC
	 * versions use an explicit CMP, missing the optimization of converting
	 * to a faster down-count that uses SUBS followed by BCS/BCC.
	 */
    for (size_t cycle = clock_cycles; cycle--;)
    {
        NOP();
        const bool bit = SWDIO_READ();
        SWCLK_HIGH();
        NOP();
        value >>= 1U;
        value  |= (uint32_t)bit << 31U;
        /* Reordering barrier */
        BARRIER();
        SWCLK_LOW();
        /* Reordering barrier */
        BARRIER();
    }
    value >>= (32U - clock_cycles);
    return value;
}


OPTIMIZE RAMFUNC uint32_t swdptap_seq_in(size_t clock_cycles)
{
    uint32_t retval;

    INTERRUPTS_OFF();
    if (target_clk_divider != 0)
        retval = swdptap_seq_in_delay(clock_cycles);
    else
        retval = swdptap_seq_in_nodelay(clock_cycles);
    INTERRUPTS_ON();
    return retval;
}

static INLINE OPTIMIZE RAMFUNC bool swdptap_seq_in_parity_delay(uint32_t *ret, size_t clock_cycles)
{
    const uint32_t result = swdptap_seq_in_delay(clock_cycles);

    SWCLK_DELAY();
    const bool bit = SWDIO_READ();
    SWCLK_HIGH();
    SWCLK_DELAY();
    SWCLK_LOW();

    /* Terminate the read cycle now */
    swdptap_turnaround_delay(SWDIO_STATUS_DRIVE);

    const bool parity = __builtin_parity(result);

    *ret = result;
    return parity == bit;
}

static INLINE OPTIMIZE RAMFUNC bool swdptap_seq_in_parity_nodelay(uint32_t *ret, size_t clock_cycles)
{
    const uint32_t result = swdptap_seq_in_nodelay(clock_cycles);

    NOP();
    const bool bit = SWDIO_READ();
    SWCLK_HIGH();
    NOP();
    NOP();
    SWCLK_LOW();

    /* Terminate the read cycle now */
    swdptap_turnaround_nodelay(SWDIO_STATUS_DRIVE);

    const bool parity = __builtin_parity(result);

    *ret = result;
    return parity == bit;
}

OPTIMIZE RAMFUNC bool swdptap_seq_in_parity(uint32_t *ret, size_t clock_cycles)
{
    bool retval;

    INTERRUPTS_OFF();
    if (target_clk_divider != 0)
        retval = swdptap_seq_in_parity_delay(ret, clock_cycles);
    else
        retval = swdptap_seq_in_parity_nodelay(ret, clock_cycles);
    INTERRUPTS_ON();
    return retval;
}

#if 0
static INLINE OPTIMIZE RAMFUNC void swdptap_seq_out_delay(const uint32_t tms_states, const size_t clock_cycles)
{
    uint32_t value = tms_states;
    bool     bit   = value & 1U;

    swdptap_turnaround(SWDIO_STATUS_DRIVE);

    if (!clock_cycles)
        return;
    /*
	 * Count down instead of up, because with an up-count, some ARM-GCC
	 * versions use an explicit CMP, missing the optimization of converting
	 * to a faster down-count that uses SUBS followed by BCS/BCC.
	 */
    for (size_t cycle = clock_cycles; cycle--;)
    {
        /* Reordering barrier */
        BARRIER();
        SWDIO_SET(bit);
        SWCLK_DELAY();
        SWCLK_HIGH();
        SWCLK_DELAY();
        BARRIER();
        value >>= 1U;
        bit     = value & 1U;
        /* Reordering barrier */
        BARRIER();
        SWCLK_LOW();
    }
}

#else

#define SWDIO_BIT_OUT_DELAY()    \
    do {                         \
        /* Reordering barrier */ \
        BARRIER();               \
        SWDIO_SET(bit);          \
        SWCLK_DELAY();           \
        SWCLK_HIGH();            \
        SWCLK_DELAY();           \
        BARRIER();               \
        value >>= 1U;            \
        bit     = value & 1U;    \
        /* Reordering barrier */ \
        BARRIER();               \
        SWCLK_LOW();             \
    } while (0)

static INLINE OPTIMIZE RAMFUNC void swdptap_seq_out_delay(const uint32_t tms_states, const size_t clock_cycles)
{
    uint32_t value = tms_states;
    bool     bit   = value & 1U;

    swdptap_turnaround_delay(SWDIO_STATUS_DRIVE);

    size_t n = (clock_cycles + 7) / 8;
    /* clang-format off */
    switch (clock_cycles % 8)
    {
        case 0: do { SWDIO_BIT_OUT_DELAY();
        case 7: SWDIO_BIT_OUT_DELAY();
        case 6: SWDIO_BIT_OUT_DELAY();
        case 5: SWDIO_BIT_OUT_DELAY();
        case 4: SWDIO_BIT_OUT_DELAY();
        case 3: SWDIO_BIT_OUT_DELAY();
        case 2: SWDIO_BIT_OUT_DELAY();
        case 1: SWDIO_BIT_OUT_DELAY();
        } while (--n > 0);
    }
    /* clang-format on */
}
#endif

#if 0
static INLINE OPTIMIZE RAMFUNC void swdptap_seq_out_nodelay(const uint32_t tms_states, const size_t clock_cycles)
{
    uint32_t value = tms_states;
    bool     bit   = value & 1U;

    swdptap_turnaround(SWDIO_STATUS_DRIVE);

    if (!clock_cycles)
        return;
    /*
	 * Count down instead of up, because with an up-count, some ARM-GCC
	 * versions use an explicit CMP, missing the optimization of converting
	 * to a faster down-count that uses SUBS followed by BCS/BCC.
	 */
    for (size_t cycle = clock_cycles; cycle--;)
    {
        /* Reordering barrier */
        BARRIER();
        SWDIO_SET(bit);
        NOP();
        SWCLK_HIGH();
        NOP();
        BARRIER();
        value >>= 1U;
        bit     = value & 1U;
        /* Reordering barrier */
        BARRIER();
        SWCLK_LOW();
    }
}

#else

#define SWDIO_BIT_OUT_NODELAY()  \
    do {                         \
        /* Reordering barrier */ \
        BARRIER();               \
        SWDIO_SET(bit);          \
        NOP();                   \
        SWCLK_HIGH();            \
        NOP();                   \
        BARRIER();               \
        value >>= 1U;            \
        bit     = value & 1U;    \
        /* Reordering barrier */ \
        BARRIER();               \
        SWCLK_LOW();             \
    } while (0)

static INLINE OPTIMIZE RAMFUNC void swdptap_seq_out_nodelay(const uint32_t tms_states, const size_t clock_cycles)
{
    uint32_t value = tms_states;
    bool     bit   = value & 1U;

    swdptap_turnaround_nodelay(SWDIO_STATUS_DRIVE);

    size_t n = (clock_cycles + 7) / 8;
    /* clang-format off */
    switch (clock_cycles % 8)
    {
        case 0: do { SWDIO_BIT_OUT_NODELAY();
        case 7: SWDIO_BIT_OUT_NODELAY();
        case 6: SWDIO_BIT_OUT_NODELAY();
        case 5: SWDIO_BIT_OUT_NODELAY();
        case 4: SWDIO_BIT_OUT_NODELAY();
        case 3: SWDIO_BIT_OUT_NODELAY();
        case 2: SWDIO_BIT_OUT_NODELAY();
        case 1: SWDIO_BIT_OUT_NODELAY();
        } while (--n > 0);
    }
    /* clang-format on */
}
#endif

OPTIMIZE RAMFUNC void swdptap_seq_out(const uint32_t tms_states, const size_t clock_cycles)
{
    INTERRUPTS_OFF();
    if (target_clk_divider != 0)
        swdptap_seq_out_delay(tms_states, clock_cycles);
    else
        swdptap_seq_out_nodelay(tms_states, clock_cycles);
    INTERRUPTS_ON();
}

static INLINE OPTIMIZE RAMFUNC void swdptap_seq_out_parity_delay(const uint32_t tms_states, const size_t clock_cycles)
{
    const bool parity = __builtin_parity(tms_states);
    swdptap_seq_out_delay(tms_states, clock_cycles);
    SWDIO_SET(parity);
    SWCLK_DELAY();
    SWCLK_HIGH();
    SWCLK_DELAY();
    SWCLK_LOW();
}

static INLINE OPTIMIZE RAMFUNC void swdptap_seq_out_parity_nodelay(const uint32_t tms_states, const size_t clock_cycles)
{
    const bool parity = __builtin_parity(tms_states);
    swdptap_seq_out_nodelay(tms_states, clock_cycles);
    SWDIO_SET(parity);
    NOP();
    SWCLK_HIGH();
    NOP();
    NOP();
    SWCLK_LOW();
}


OPTIMIZE RAMFUNC void swdptap_seq_out_parity(const uint32_t tms_states, const size_t clock_cycles)
{
    INTERRUPTS_OFF();
    if (target_clk_divider != 0)
        swdptap_seq_out_parity_delay(tms_states, clock_cycles);
    else
        swdptap_seq_out_parity_nodelay(tms_states, clock_cycles);
    INTERRUPTS_ON();
}
