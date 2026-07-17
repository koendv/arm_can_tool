/*
 * RT-Thread CAN bit timing calculation test command
 * 
 * This shell command verifies that can_calc_bittiming() produces identical
 * results to the Linux can-calc-bit-timing tool for bxcan with 108 MHz clock.
 */

#include <rtthread.h>
#include <stdint.h>
#include <string.h>
#include "can_calc_bittiming.h"

/* Expected results from Linux "can-calc-bit-timing --alg=v6.3 -c 108000000 bxcan" */
static const struct expected_result
{
    uint32_t             bitrate; /* Nominal bitrate */
    struct can_bittiming bt;      /* Expected calculated parameters */
    uint32_t             can_btr; /* Expected CAN_BTR register value */
} expected_results[] = {
    {.bitrate = 1000000,    .bt = {.bitrate = 1000000, .sample_point = 750, .tq = 83, .prop_seg = 4, .phase_seg1 = 4, .phase_seg2 = 3, .sjw = 1, .brp = 9}, .can_btr = 0x00270008},
    { .bitrate = 800000,     .bt = {.bitrate = 800000, .sample_point = 800, .tq = 83, .prop_seg = 5, .phase_seg1 = 6, .phase_seg2 = 3, .sjw = 1, .brp = 9}, .can_btr = 0x002a0008},
    { .bitrate = 666666,     .bt = {.bitrate = 666666, .sample_point = 777, .tq = 83, .prop_seg = 6, .phase_seg1 = 7, .phase_seg2 = 4, .sjw = 2, .brp = 9}, .can_btr = 0x013c0008},
    { .bitrate = 500000,   .bt = {.bitrate = 500000, .sample_point = 875, .tq = 250, .prop_seg = 3, .phase_seg1 = 3, .phase_seg2 = 1, .sjw = 1, .brp = 27}, .can_btr = 0x0005001a},
    { .bitrate = 250000,   .bt = {.bitrate = 250000, .sample_point = 875, .tq = 250, .prop_seg = 6, .phase_seg1 = 7, .phase_seg2 = 2, .sjw = 1, .brp = 27}, .can_btr = 0x001c001a},
    { .bitrate = 125000,   .bt = {.bitrate = 125000, .sample_point = 875, .tq = 500, .prop_seg = 6, .phase_seg1 = 7, .phase_seg2 = 2, .sjw = 1, .brp = 54}, .can_btr = 0x001c0035},
    { .bitrate = 100000, .bt = {.bitrate = 100000, .sample_point = 875, .tq = 1250, .prop_seg = 3, .phase_seg1 = 3, .phase_seg2 = 1, .sjw = 1, .brp = 135}, .can_btr = 0x00050086},
    {  .bitrate = 83333,    .bt = {.bitrate = 83333, .sample_point = 875, .tq = 750, .prop_seg = 6, .phase_seg1 = 7, .phase_seg2 = 2, .sjw = 1, .brp = 81}, .can_btr = 0x001c0050},
    {  .bitrate = 50000,  .bt = {.bitrate = 50000, .sample_point = 875, .tq = 1250, .prop_seg = 6, .phase_seg1 = 7, .phase_seg2 = 2, .sjw = 1, .brp = 135}, .can_btr = 0x001c0086},
    {  .bitrate = 33333,  .bt = {.bitrate = 33333, .sample_point = 875, .tq = 3750, .prop_seg = 3, .phase_seg1 = 3, .phase_seg2 = 1, .sjw = 1, .brp = 405}, .can_btr = 0x00050194},
    {  .bitrate = 20000,  .bt = {.bitrate = 20000, .sample_point = 875, .tq = 6250, .prop_seg = 3, .phase_seg1 = 3, .phase_seg2 = 1, .sjw = 1, .brp = 675}, .can_btr = 0x000502a2},
    {  .bitrate = 10000,  .bt = {.bitrate = 10000, .sample_point = 875, .tq = 6250, .prop_seg = 6, .phase_seg1 = 7, .phase_seg2 = 2, .sjw = 1, .brp = 675}, .can_btr = 0x001c02a2}
};

#define NUM_EXPECTED_RESULTS (sizeof(expected_results) / sizeof(expected_results[0]))

/**
 * Calculate CAN_BTR register value for bxcan
 */
static uint32_t calc_bxcan_btr(const struct can_bittiming *bt)
{
    return (((bt->brp - 1) & 0x3ff) << 0) | (((bt->prop_seg + bt->phase_seg1 - 1) & 0xf) << 16) | (((bt->phase_seg2 - 1) & 0x7) << 20) | (((bt->sjw - 1) & 0x3) << 24);
}

/**
 * Compare two can_bittiming structures
 * Returns RT_TRUE if all fields match, RT_FALSE otherwise
 */
static int compare_bittiming(const struct can_bittiming *expected,
                             const struct can_bittiming *actual)
{
    if (expected->bitrate != actual->bitrate) return RT_FALSE;
    if (expected->sample_point != actual->sample_point) return RT_FALSE;
    if (expected->tq != actual->tq) return RT_FALSE;
    if (expected->prop_seg != actual->prop_seg) return RT_FALSE;
    if (expected->phase_seg1 != actual->phase_seg1) return RT_FALSE;
    if (expected->phase_seg2 != actual->phase_seg2) return RT_FALSE;
    if (expected->sjw != actual->sjw) return RT_FALSE;
    if (expected->brp != actual->brp) return RT_FALSE;

    return RT_TRUE;
}

/**
 * RT-Thread shell command: can_calc_verify
 * 
 * Verifies CAN bit timing calculation against known-good Linux results from can-utils
 * can-calc-bit-timing --alg=v6.3 -c 108000000 bxcan
 */

static void can_calc_verify(int argc, char **argv)
{
    struct can_bittiming bt;
    uint32_t             btr_calc;
    uint32_t             i;
    uint32_t             pass_count = 0;
    uint32_t             fail_count = 0;

    rt_kprintf("\n");
    rt_kprintf(" nominal                                  real  Bitrt    nom   real  SampP\n");
    rt_kprintf(" Bitrate TQ[ns] PrS PhS1 PhS2 SJW BRP  Bitrate  Error  SampP  SampP  Error    CAN_BTR\n");

    for (i = 0; i < NUM_EXPECTED_RESULTS; i++)
    {
        const struct expected_result *exp = &expected_results[i];

        /* Clear bt and set input parameters */
        memset(&bt, 0, sizeof(bt));
        bt.bitrate      = exp->bitrate;
        bt.sample_point = 0;
        bt.sjw          = 0;

        /* Calculate timing */
        if (can_calc_bittiming(&bt) != RT_EOK)
        {
            rt_kprintf("%8d *** calculation failed ***\n", exp->bitrate);
            fail_count++;
            continue;
        }

        /* Calculate CAN_BTR */
        btr_calc = calc_bxcan_btr(&bt);

        /* Calculate errors */
        uint32_t bitrate_error, sample_point_nominal, sample_point_error;
        bitrate_error = bt.bitrate > exp->bitrate ? bt.bitrate - exp->bitrate : exp->bitrate - bt.bitrate;
        bitrate_error = 1000 * bitrate_error / bt.bitrate;
        if (bt.bitrate > 800000)
            sample_point_nominal = 750;
        else if (bt.bitrate > 500000)
            sample_point_nominal = 800;
        else
            sample_point_nominal = 875;
        sample_point_error = bt.sample_point > sample_point_nominal ? bt.sample_point - sample_point_nominal : sample_point_nominal - bt.sample_point;
        sample_point_error = 1000 * sample_point_error / bt.sample_point;

        /* Print table row (matching Linux format) */
        rt_kprintf("%8d %6d %3d %4d %4d %3d %3d %8d %3d.%d%% %3d.%d%% %3d.%d%% %3d.%d%% 0x%08x %s\n",
                   exp->bitrate,
                   bt.tq,
                   bt.prop_seg,
                   bt.phase_seg1,
                   bt.phase_seg2,
                   bt.sjw,
                   bt.brp,
                   bt.bitrate,
                   bitrate_error / 10, bitrate_error % 10,
                   exp->bt.sample_point / 10, exp->bt.sample_point % 10,
                   bt.sample_point / 10, bt.sample_point % 10,
                   sample_point_error / 10, sample_point_error % 10,
                   btr_calc,
                   (compare_bittiming(&exp->bt, &bt) && (btr_calc == exp->can_btr)) ? "PASS" : "FAIL");

        if (compare_bittiming(&exp->bt, &bt) && (btr_calc == exp->can_btr))
        {
            pass_count++;
        }
        else
        {
            fail_count++;
        }
    }

    rt_kprintf("\n%d PASS, %d FAIL\n", pass_count, fail_count);
}

MSH_CMD_EXPORT(can_calc_verify, CAN timing verification);
