/*
 * Copyright (c) 2024 RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * CAN bit timing calculation for bxcan (STM32) with 108 MHz clock.
 * Based on Linux kernel v6.3 algorithm.
 */

#ifndef __CAN_CALC_BITTIMING_H__
#define __CAN_CALC_BITTIMING_H__

#include <rtthread.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief CAN bit timing parameters structure
 * 
 * This structure holds both input and output parameters for bit timing calculation.
 * When calling can_calc_bittiming():
 * - Input:  bitrate must be set, sample_point may be set (0 for CiA recommended)
 * - Output: All fields are filled with calculated values
 */
struct can_bittiming
{
    uint32_t bitrate;      /**< Bit-rate in bits/second (input/output) */
    uint32_t sample_point; /**< Sample point in one-tenth of a percent (input/output)
                                Input: 0 = use CiA recommended, otherwise desired sample point
                                Output: Actual achievable sample point */
    uint32_t tq;           /**< Time quantum in nanoseconds (output) */
    uint32_t prop_seg;     /**< Propagation segment in time quanta (output) */
    uint32_t phase_seg1;   /**< Phase buffer segment 1 in time quanta (output) */
    uint32_t phase_seg2;   /**< Phase buffer segment 2 in time quanta (output) */
    uint32_t sjw;          /**< Synchronization jump width in time quanta (output) */
    uint32_t brp;          /**< Bit-rate prescaler (output) */
};

/**
 * @brief Calculate CAN bit timing parameters for bxcan with 108 MHz clock
 * 
 * This function computes optimal CAN bit timing parameters for the given bitrate
 * using the algorithm from Linux kernel v6.3. It is specifically configured for
 * the bxcan controller (STM32) with a 108 MHz clock.
 * 
 * @param bt      Pointer to can_bittiming structure
 *                - On input: bt->bitrate must be set, bt->sample_point may be set, bt->sjw may be set
 *                - On output: All fields are filled with computed values
 * 
 * @return RT_EOK              Success
 * @return -RT_EINVAL          Invalid parameter (bt == NULL)
 * @return -RT_ERROR           Calculation failed (no valid timing found, error too high, or SJW check failed)
 * 
 * @note The desired nominal bit rate in bits/second (e.g., 500000 for 500 kbit/s)
 * @note The sample point is in one-tenth of a percent (e.g., 875 = 87.5%)
 * @note If sample_point is 0 on input, CiA recommended values are used:
 *       - >800 kbit/s: 75.0%
 *       - >500 kbit/s: 80.0%
 *       - otherwise:   87.5%
 * 
 * @see https://www.can-cia.org/can-knowledge/can/can-bit-timing/
 */
rt_err_t can_calc_bittiming(struct can_bittiming *bt);

#ifdef __cplusplus
}
#endif

#endif /* __CAN_CALC_BITTIMING_H__ */
