#ifndef _CANBUS_H_
#define _CANBUS_H_

#include <rtthread.h>
#include <rtdevice.h>

#define MAX_CAN_HW_FILTER 14

typedef struct
{
    uint32_t  mask;
    uint32_t  value;
    rt_bool_t extended;
} can_hw_filter_t;

typedef struct
{
    can_hw_filter_t filter[MAX_CAN_HW_FILTER];
    uint8_t         count;
} can_hw_filter_bank_t;

extern can_hw_filter_bank_t can_hw_filter;

rt_err_t canbus_send_frame(struct rt_can_msg *msg);
rt_err_t canbus_set_baudrate(uint32_t baudrate);
rt_err_t canbus_begin_filter(void);
rt_err_t canbus_set_std_filter(uint32_t id, uint32_t mask);
rt_err_t canbus_set_ext_filter(uint32_t id, uint32_t mask);
rt_err_t canbus_end_filter(void);
rt_err_t canbus_set_autoretransmit(rt_bool_t mode);
rt_err_t canbus_set_mode(int mode);

#endif /* _CANBUS_H_ */
