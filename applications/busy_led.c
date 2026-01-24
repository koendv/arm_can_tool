/*
 * busy_led.c
 * switches LED ON when busy
 *
 * two versions to switch the led on/off.
 * - one using rt_thread primitives. more portable, but slower.
 * - one using AT32 HAL direct register writes. not portable, but minimum cpu use.
 * remember this function gets called on every task switch.
 * 
 * possible improvement: switch led on when entering interrupt handler
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <drv_gpio.h>
#include <at32f402_405_gpio.h>
//#include "pins.h"

#define DBG_TAG "LED"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#define LED_PIN GET_PIN(C, 2)

#ifdef LED_USE_RT_PIN
/* use rt-thread system calls, portable */
#define LED_ON()  rt_pin_write(LED_PIN, PIN_LOW)
#define LED_OFF() rt_pin_write(LED_PIN, PIN_HIGH)
#else
/* use direct register access, not portable but minimum cpu use */
#define LED_ON()  GPIOC->clr = GPIO_PINS_2
#define LED_OFF() GPIOC->scr = GPIO_PINS_2
#endif

static rt_thread_t idle_thread = NULL;

static void scheduler_hook(struct rt_thread *from, struct rt_thread *to)
{
    if (from == idle_thread)
        LED_ON();  /* LED ON when busy */
    else if (to == idle_thread)
        LED_OFF(); /* LED OFF when idle */
}

static int busy_led_init(void)
{
    rt_pin_mode(LED_PIN, PIN_MODE_OUTPUT);
    rt_pin_write(LED_PIN, PIN_HIGH);
    idle_thread = rt_thread_find("tidle0");
    if (idle_thread)
    {
        rt_scheduler_sethook(scheduler_hook);
        LOG_I("init");
    }
    else
    {
        LOG_E("fail");
    }
    return RT_EOK;
}

INIT_APP_EXPORT(busy_led_init);
