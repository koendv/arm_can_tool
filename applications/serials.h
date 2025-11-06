#ifndef AUX_UART_H
#define AUX_UART_H
#include <stdint.h>
#include <stdbool.h>

extern const uint32_t serial_speeds[];

int32_t serial0_write(uint8_t *buf, uint32_t len);
int32_t serial1_write(uint8_t *buf, uint32_t len);
void    serial0_swap_pins(bool swap_pins);
void    serial0_set_speed(uint32_t speed);
void    serial1_set_speed(uint32_t speed);
void    serial2_set_speed(uint32_t speed);
void    serial0_enable(bool ena);
void    serial1_enable(bool ena);
void    serial2_enable(bool ena);

#endif
