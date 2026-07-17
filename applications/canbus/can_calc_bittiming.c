/*
 * RT-Thread CAN bit timing calculation for bxcan (STM32) with 108 MHz clock.
 * Based on Linux kernel v6.3 algorithm (can-calc-bit-timing-v6_3.c).
 * 
 * This file is self-contained and can be used in RT-Thread drivers.
 */

#include "can_calc_bittiming.h"
#include <stdbool.h>
#include <limits.h>

/* -------------------------------------------------------------------------
 * Arithmetic helpers (from Linux kernel compat.h)
 * ------------------------------------------------------------------------- */

#define NSEC_PER_SEC       1000000000UL
#define CAN_CALC_MAX_ERROR 50 /* in one-tenth of a percent */
#define CAN_SYNC_SEG       1
#define KILO               1000UL

/* Absolute value (works for signed types) */
#define abs(x) ({           \
    long __x = (x);         \
    (__x < 0) ? -__x : __x; \
})

/* min/max with type checking (simplified for embedded) */
#define min(x, y) ({               \
    typeof(x) _min1 = (x);         \
    typeof(y) _min2 = (y);         \
    _min1 < _min2 ? _min1 : _min2; \
})

#define max(x, y) ({               \
    typeof(x) _max1 = (x);         \
    typeof(y) _max2 = (y);         \
    _max1 > _max2 ? _max1 : _max2; \
})

#define clamp(val, lo, hi) min((typeof(val))max(val, lo), hi)

/* 64-bit division helpers */
#define do_div(n, base) ({            \
    uint32_t __base = (base);         \
    uint32_t __rem;                   \
    __rem = ((uint64_t)(n)) % __base; \
    (n)   = ((uint64_t)(n)) / __base; \
    __rem;                            \
})

static inline uint64_t div_u64_rem(uint64_t dividend, uint32_t divisor,
                                   uint32_t *remainder)
{
    *remainder = dividend % divisor;
    return dividend / divisor;
}

static inline uint64_t div_u64(uint64_t dividend, uint32_t divisor)
{
    uint32_t remainder;
    return div_u64_rem(dividend, divisor, &remainder);
}

static inline uint64_t mul_u32_u32(uint32_t a, uint32_t b)
{
    return (uint64_t)a * b;
}

#define DIV_U64_ROUND_CLOSEST(dividend, divisor) \
    ({ uint32_t _tmp = (divisor);            \
           div_u64((uint64_t)(dividend) + _tmp / 2, _tmp); })

/* -------------------------------------------------------------------------
 * CAN bit timing constants for bxcan (stm32f072 etc.) with 108 MHz clock
 * ------------------------------------------------------------------------- */

#define BXCAN_CLOCK_FREQ 108000000UL /* 108 MHz */

#define BXCAN_TSEG1_MIN 1
#define BXCAN_TSEG1_MAX 16
#define BXCAN_TSEG2_MIN 1
#define BXCAN_TSEG2_MAX 8
#define BXCAN_SJW_MAX   4
#define BXCAN_BRP_MIN   1
#define BXCAN_BRP_MAX   1024
#define BXCAN_BRP_INC   1

static const struct can_bittiming_const
{
    uint32_t tseg1_min, tseg1_max;
    uint32_t tseg2_min, tseg2_max;
    uint32_t sjw_max;
    uint32_t brp_min, brp_max, brp_inc;
} bxcan_bittiming_const = {
    .tseg1_min = BXCAN_TSEG1_MIN,
    .tseg1_max = BXCAN_TSEG1_MAX,
    .tseg2_min = BXCAN_TSEG2_MIN,
    .tseg2_max = BXCAN_TSEG2_MAX,
    .sjw_max   = BXCAN_SJW_MAX,
    .brp_min   = BXCAN_BRP_MIN,
    .brp_max   = BXCAN_BRP_MAX,
    .brp_inc   = BXCAN_BRP_INC,
};

/* -------------------------------------------------------------------------
 * Helper functions (from v6.3)
 * ------------------------------------------------------------------------- */

static inline unsigned int can_bit_time(const struct can_bittiming *bt)
{
    return CAN_SYNC_SEG + bt->prop_seg + bt->phase_seg1 + bt->phase_seg2;
}

static void can_sjw_set_default(struct can_bittiming *bt)
{
    if (bt->sjw)
        return;
    /* If no sjw provided, use sane default of phase_seg2 / 2 */
    bt->sjw = max(1U, min(bt->phase_seg1, bt->phase_seg2 / 2));
}

static int can_sjw_check(const struct can_bittiming       *bt,
                         const struct can_bittiming_const *btc)
{
    if (bt->sjw > btc->sjw_max)
        return -1; /* error */
    if (bt->sjw > bt->phase_seg1)
        return -1;
    if (bt->sjw > bt->phase_seg2)
        return -1;
    return 0;
}

/*
 * can_update_sample_point - find nearest achievable sample point
 */
static unsigned int
can_update_sample_point(const struct can_bittiming_const *btc,
                        unsigned int                      sample_point_nominal,
                        unsigned int                      tseg,
                        unsigned int                     *tseg1_ptr,
                        unsigned int                     *tseg2_ptr,
                        unsigned int                     *sample_point_error_ptr)
{
    unsigned int sample_point_error, best_sample_point_error = UINT_MAX;
    unsigned int sample_point, best_sample_point             = 0;
    unsigned int tseg1, tseg2;
    int          i;

    for (i = 0; i <= 1; i++)
    {
        tseg2 = tseg + CAN_SYNC_SEG - (sample_point_nominal * (tseg + CAN_SYNC_SEG)) / 1000 - i;
        tseg2 = clamp(tseg2, btc->tseg2_min, btc->tseg2_max);
        tseg1 = tseg - tseg2;
        if (tseg1 > btc->tseg1_max)
        {
            tseg1 = btc->tseg1_max;
            tseg2 = tseg - tseg1;
        }

        sample_point       = 1000 * (tseg + CAN_SYNC_SEG - tseg2) / (tseg + CAN_SYNC_SEG);
        sample_point_error = abs((int)sample_point_nominal - (int)sample_point);

        if (sample_point <= sample_point_nominal && sample_point_error < best_sample_point_error)
        {
            best_sample_point       = sample_point;
            best_sample_point_error = sample_point_error;
            *tseg1_ptr              = tseg1;
            *tseg2_ptr              = tseg2;
        }
    }

    if (sample_point_error_ptr)
        *sample_point_error_ptr = best_sample_point_error;

    return best_sample_point;
}

/* -------------------------------------------------------------------------
 * Main calculation function (RT-Thread API)
 * ------------------------------------------------------------------------- */

rt_err_t can_calc_bittiming(struct can_bittiming *bt)
{
    const struct can_bittiming_const *btc        = &bxcan_bittiming_const;
    uint32_t                          clock_freq = BXCAN_CLOCK_FREQ;

    unsigned int sample_point_nominal; /* desired sample point */
    unsigned int best_tseg = 0;        /* best tseg value (tseg1+tseg2) */
    unsigned int best_brp  = 0;        /* best brp */
    unsigned int tseg, tsegall, tseg1, tseg2;
    unsigned int brp;
    uint64_t     v64;

    if (bt == RT_NULL)
        return -RT_EINVAL;

    if (bt->bitrate == 0)
        return -RT_EINVAL;

    /* Determine nominal sample point if not given */
    if (bt->sample_point)
    {
        sample_point_nominal = bt->sample_point;
    }
    else
    {
        if (bt->bitrate > 800 * KILO)
            sample_point_nominal = 750;
        else if (bt->bitrate > 500 * KILO)
            sample_point_nominal = 800;
        else
            sample_point_nominal = 875;
    }

    /* Search over possible tseg values (tseg = tseg1 + tseg2) */
    unsigned int best_bitrate_error      = UINT_MAX;
    unsigned int best_sample_point_error = UINT_MAX;

    /* tseg even = round down, odd = round up */
    for (tseg = (btc->tseg1_max + btc->tseg2_max) * 2 + 1;
         tseg >= (btc->tseg1_min + btc->tseg2_min) * 2; tseg--)
    {
        unsigned int bitrate_error, sample_point_error;

        tsegall = CAN_SYNC_SEG + tseg / 2;

        /* Compute possible brp */
        brp = clock_freq / (tsegall * bt->bitrate) + (tseg % 2);

        /* Align to brp increment */
        brp = (brp / btc->brp_inc) * btc->brp_inc;
        if (brp < btc->brp_min || brp > btc->brp_max)
            continue;

        /* Resulting bitrate */
        uint32_t bitrate_calc = clock_freq / (brp * tsegall);
        bitrate_error         = abs((int)bt->bitrate - (int)bitrate_calc);

        if (bitrate_error > best_bitrate_error)
            continue;

        /* Reset sample point error if we have a better bitrate */
        if (bitrate_error < best_bitrate_error)
            best_sample_point_error = UINT_MAX;

        /* Evaluate sample point for this tseg */
        unsigned int tseg1_tmp, tseg2_tmp;
        unsigned int sample_point =
            can_update_sample_point(btc, sample_point_nominal,
                                    tseg / 2,
                                    &tseg1_tmp, &tseg2_tmp,
                                    &sample_point_error);

        if (sample_point_error >= best_sample_point_error)
            continue;

        best_sample_point_error = sample_point_error;
        best_bitrate_error      = bitrate_error;
        best_tseg               = tseg / 2;
        best_brp                = brp;
        tseg1                   = tseg1_tmp;
        tseg2                   = tseg2_tmp;

        if (bitrate_error == 0 && sample_point_error == 0)
            break;
    }

    if (best_bitrate_error == UINT_MAX)
    {
        /* No valid timing found */
        return -RT_ERROR;
    }

    /* Check bitrate error tolerance */
    if (best_bitrate_error)
    {
        v64 = (uint64_t)best_bitrate_error * 1000;
        do_div(v64, bt->bitrate);
        unsigned int bitrate_error_tenths = (uint32_t)v64;
        if (bitrate_error_tenths > CAN_CALC_MAX_ERROR)
            return -RT_ERROR; /* error too high */
    }

    /* Compute final sample point (may differ slightly from candidate) */
    bt->sample_point = can_update_sample_point(btc, sample_point_nominal,
                                               best_tseg, &tseg1, &tseg2,
                                               NULL);

    /* Time quantum in ns */
    v64 = mul_u32_u32(best_brp, NSEC_PER_SEC);
    do_div(v64, clock_freq);
    bt->tq = (uint32_t)v64;

    /* Split tseg1 into prop_seg and phase_seg1 */
    bt->prop_seg   = tseg1 / 2;
    bt->phase_seg1 = tseg1 - bt->prop_seg;
    bt->phase_seg2 = tseg2;

    /* Set SJW */
    can_sjw_set_default(bt);
    if (can_sjw_check(bt, btc) < 0)
        return -RT_ERROR;

    bt->brp = best_brp;

    /* Real bitrate */
    bt->bitrate = clock_freq / (bt->brp * can_bit_time(bt));

    return RT_EOK;
}
