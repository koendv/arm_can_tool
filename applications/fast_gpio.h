/*
 * fast_gpio.h - Fast GPIO operations for AT32F405
 *
 * this is a header-only replacement for rt-thread 
 * rt_pin_write(), rt_pin_read(), rt_pin_mode() 
 * in the common case that pin is a compile-time constant
 *
 * Assumptions:
 *  - GPIO clock already enabled
 *  - pin encoding from RT-Thread drv_gpio.h
 *  - pin is valid
 *  - only PIN_MODE_INPUT and PIN_MODE_OUTPUT supported
 *
 * Initialize the pin once using rt_pin_mode(), 
 * then use fast_pin_write(), fast_pin_read(), fast_pin_mode().
 */

#ifndef FAST_GPIO_H
#define FAST_GPIO_H

#include <rtthread.h>
#include "at32f402_405_gpio.h"

#define PIN_PORT(pin)                   ((uint8_t)(((pin) >> 4) & 0xFu))
#define PIN_NO(pin)                     ((uint8_t)((pin) & 0xFu))

#define PIN_ATPORT(pin)                 ((gpio_type *)(GPIOA_BASE + (0x400u * PIN_PORT(pin))))
#define PIN_ATPIN(pin)                  ((uint16_t)(1u << PIN_NO(pin)))

static inline void fast_pin_set(rt_base_t pin)
{
    PIN_ATPORT(pin)->scr = PIN_ATPIN(pin);
}

static inline void fast_pin_clear(rt_base_t pin)
{
    PIN_ATPORT(pin)->clr = PIN_ATPIN(pin);
}

static inline void fast_pin_toggle(rt_base_t pin)
{
    PIN_ATPORT(pin)->togr = PIN_ATPIN(pin);
}

static inline void fast_pin_write(rt_base_t pin, rt_base_t value)
{
    gpio_type *gpio = PIN_ATPORT(pin);
    uint32_t mask = PIN_ATPIN(pin);

    if (value)
        gpio->scr = mask;
    else
        gpio->clr = mask;
}

static inline rt_base_t fast_pin_read(rt_base_t pin)
{
    return (PIN_ATPORT(pin)->idt & PIN_ATPIN(pin)) ? 1 : 0;
}

static inline void fast_pin_mode(rt_base_t pin, rt_base_t mode)
{
    gpio_type *gpio = PIN_ATPORT(pin);
    uint32_t index = PIN_NO(pin);
    uint32_t shift = index * 2;
    uint32_t mask  = 0x3u << shift;

    uint32_t cfgr = gpio->cfgr;

    if (mode == PIN_MODE_OUTPUT)
        cfgr = (cfgr & ~mask) | (1u << shift);
    else
        cfgr = (cfgr & ~mask);   /* input = 0 */

    gpio->cfgr = cfgr;
}

#endif
