#ifndef BXCAN_H
#define BXCAN_H

#include <rtthread.h>
#include <rtdevice.h>
#include <stdbool.h>
#include "canfilter.h"
#include "canbus.h"

#define BXCAN_CLOCK_SPEED 108000000U

typedef struct gs_usb_host_frame gs_usb_host_frame_t;

typedef struct canfilter_bxcan_f0 bxcan_filter_t;

extern bxcan_filter_t bxcan_filter; /* hardware filter for settings.c */

rt_err_t bxcan_init();
rt_err_t bxcan_reset();
rt_err_t bxcan_disable();
rt_err_t bxcan_enable();
rt_err_t bxcan_set_speed(uint32_t new_speed);
rt_err_t bxcan_set_bittiming(struct gs_device_bittiming new_bittiming);
rt_err_t bxcan_set_timestamp(bool new_timestamp);
rt_err_t bxcan_set_mode(uint32_t new_mode);
rt_err_t bxcan_set_filter(bxcan_filter_t new_filter);

#ifdef __cplusplus
}
#endif

#endif /* BXCAN_H */
