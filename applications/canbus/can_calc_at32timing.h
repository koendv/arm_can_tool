#ifndef CAN_CALC_AT32_TIMING_H
#define CAN_CALC_AT32_TIMING_H

#include <rtthread.h>

/*
  given bitrate, calculates at32 hal config for bxcan canbus controller.
  Usage:
    uint32_t bitrate = 500000;
    can_baudrate_type cbt;

    can_calc_at32_timing(bitrate, &cbt);
    can_baudrate_set(CAN1, &cbt);
    
 */

rt_err_t can_calc_at32_timing(uint32_t bitrate, can_baudrate_type *config);

#endif

