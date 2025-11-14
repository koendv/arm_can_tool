#ifndef _CANBUS_H_
#define _CANBUS_H_

#include <rtthread.h>
#include <rtdevice.h>

#define MAX_CAN_HW_FILTER 14

typedef struct
{
    uint8_t  bank;
    uint32_t id;
    uint32_t mask;
    uint8_t  mode;
    uint8_t  scale;
    uint8_t  ide;
    uint8_t  rtr;
} can_hw_filter_t;

typedef struct
{
    can_hw_filter_t filter[MAX_CAN_HW_FILTER];
    uint8_t         count;
} can_hw_filter_bank_t;

extern can_hw_filter_bank_t can_hw_filter;

rt_err_t canbus_send_frame(struct rt_can_msg *msg);
rt_err_t canbus_set_baudrate(uint32_t baudrate);
rt_err_t canbus_set_autoretransmit(rt_bool_t mode);
/* modes: normal, loopback, listen-only */
rt_err_t canbus_set_mode(int mode);
/* canbus filters */
rt_err_t canbus_pass_all(void);  /* pass all frames - disable filtering */
rt_err_t canbus_block_all(void); /* block all frames - enable filtering but allow nothing */
rt_err_t canbus_begin_filter(void);
rt_err_t canbus_set_filter(uint8_t bank, uint32_t fr1, uint32_t fr2, uint8_t mode, uint8_t scale, uint8_t ide, uint8_t rtr);
rt_err_t canbus_end_filter(void);

#endif /* _CANBUS_H_ */
