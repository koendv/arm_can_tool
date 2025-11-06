#ifndef _CANBUS_H_
#define _CANBUS_H_

int32_t ascii2can(uint8_t *buf, uint32_t nbytes);
void    can1_set_speed(uint32_t speed);

#endif
