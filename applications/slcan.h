#ifndef SLCAN_H
#define SLCAN_H

#include <rtthread.h>
#include <rtdevice.h>

/* process frame from canbus and send slcan text to usb */
rt_err_t slcan_parse_frame(struct rt_can_msg *msg);

/* process slcan command from usb and send frame to canbus */
rt_err_t slcan_parse_str(uint8_t *buf, uint8_t len);

#endif
