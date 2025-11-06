/*
 5-way navigation switch with debouncing and auto-repeat.
 a long press on "select" jumps to the main screen.
 screen blanking if inactive for 30 seconds.
 */

#include <rthw.h>
#include <rtthread.h>
#include <rtdevice.h>
#include <u8x8.h>
#include <u8g2_port.h>
#include <mui.h>
#include "multi.h"
#include "pins.h"
#include "settings.h"

#define DEBOUNCE_TICK        (RT_TICK_PER_SECOND / 50)
#define AUTO_REPEAT_TICK     (RT_TICK_PER_SECOND / 2)
#define MIN_AUTO_REPEAT_TICK (RT_TICK_PER_SECOND / 20)
#define ACCELERATION         3
#define IDLE_WAIT_TICK       (RT_TICK_PER_SECOND)

/* arrays of buttons and their messages */

#if 0
/* five-way switch */
static uint32_t menu_gpio[] = {
    SW_UP_PIN,
    SW_DOWN_PIN,
    SW_LEFT_PIN,
    SW_RIGHT_PIN,
    SW_MID_PIN,
};
static uint32_t menu_msg[] = {
    U8X8_MSG_GPIO_MENU_UP,
    U8X8_MSG_GPIO_MENU_DOWN,
    U8X8_MSG_GPIO_MENU_PREV,
    U8X8_MSG_GPIO_MENU_NEXT,
    U8X8_MSG_GPIO_MENU_SELECT,
};
#endif

/* three-way switch */
static const uint32_t menu_gpio[] = {
    SW_PREV_PIN,
    SW_NEXT_PIN,
    SW_SELECT_PIN,
};
static uint32_t menu_msg[] = {
    U8X8_MSG_GPIO_MENU_NEXT,
    U8X8_MSG_GPIO_MENU_PREV,
    U8X8_MSG_GPIO_MENU_SELECT,
};

static struct rt_mailbox *mui_msg_mb         = RT_NULL;
static rt_timer_t         debounce_tim       = RT_NULL;
static rt_timer_t         auto_repeat_tim    = RT_NULL;
static rt_tick_t          auto_repeat_period = 0;
static int32_t            menu_index         = -1;
static rt_tick_t          last_activity      = 0;
static rt_bool_t          sleeping           = RT_FALSE;
static rt_uint32_t        sleep_ticks        = 0;

/* button is pressed or released.
   start 20ms debounce timer */

void button_changed(void *args)
{
    menu_index = (uint32_t)args;
    rt_timer_start(debounce_tim);
}

/* 100ms debounce timer ends.
   when button pressed, send message to mui, start auto-repeat timer
   when button released, stop auto-repeat timer */

static void button_debounce_thread(void *ptr)
{
    uint32_t pin_state = rt_pin_read(menu_gpio[menu_index]);
    if (pin_state == PIN_LOW)
    {
        if (mui_msg_mb != NULL)
            rt_mb_send(mui_msg_mb, menu_msg[menu_index]);
        if (menu_msg[menu_index] != U8X8_MSG_GPIO_MENU_SELECT)
        {
            auto_repeat_period = AUTO_REPEAT_TICK;
            rt_timer_control(auto_repeat_tim, RT_TIMER_CTRL_SET_TIME, (void *)&auto_repeat_period);
            rt_timer_start(auto_repeat_tim);
        }
    }
    else
    {
        rt_timer_stop(auto_repeat_tim);
    }
}

/* button auto-repeat timer.
   repeats the button twice per second */

static void button_auto_repeat_thread(void *ptr)
{
    if (mui_msg_mb != NULL)
    {
        auto_repeat_period -= auto_repeat_period >> ACCELERATION;
        if (auto_repeat_period < MIN_AUTO_REPEAT_TICK) auto_repeat_period = MIN_AUTO_REPEAT_TICK;
        rt_timer_control(auto_repeat_tim, RT_TIMER_CTRL_SET_TIME, (void *)&auto_repeat_period);
        rt_timer_start(auto_repeat_tim);
        rt_mb_send(mui_msg_mb, menu_msg[menu_index]);
    }
}

/* set up mailbox, timers, interrupts */

static int multi_direction_switch()
{
    /* mailbox for messages to mui */
    mui_msg_mb = rt_mb_create("mui_msg", 4, RT_IPC_FLAG_FIFO);

    /* button debounce timer. 20ms one-shot */
    debounce_tim = rt_timer_create("debounce",
                                   button_debounce_thread,
                                   RT_NULL,
                                   DEBOUNCE_TICK,
                                   RT_TIMER_FLAG_ONE_SHOT | RT_TIMER_FLAG_SOFT_TIMER);

    /* button repeat timer. initially 500ms one-shot, decreases to 50 ms one-shot. */
    auto_repeat_tim = rt_timer_create("auto repeat",
                                      button_auto_repeat_thread,
                                      RT_NULL,
                                      AUTO_REPEAT_TICK,
                                      RT_TIMER_FLAG_ONE_SHOT | RT_TIMER_FLAG_SOFT_TIMER);

    /* swap buttons ? */
    if (settings.swap_buttons)
    {
        menu_msg[0] = U8X8_MSG_GPIO_MENU_PREV;
        menu_msg[1] = U8X8_MSG_GPIO_MENU_NEXT;
    }

    /* idle time for screen blanking */
    sleep_ticks = settings.screen_sleep_time * 60 * RT_TICK_PER_SECOND; /* minutes to ticks */

    /* attach interrupts to the switch pins */
    for (uint32_t i = 0; i < sizeof(menu_gpio) / sizeof(menu_gpio[0]); i++)
    {
        rt_pin_mode(menu_gpio[i], PIN_MODE_INPUT_PULLUP);
        rt_pin_attach_irq(menu_gpio[i], PIN_IRQ_MODE_RISING_FALLING, button_changed, (void *)i);
        rt_pin_irq_enable(menu_gpio[i], PIN_IRQ_ENABLE);
    }
}

/*
   interface with mui.
   returns 0 if no key has been pressed; or
   U8X8_MSG_GPIO_MENU_HOME, U8X8_MSG_GPIO_MENU_SELECT,
   U8X8_MSG_GPIO_MENU_NEXT, U8X8_MSG_GPIO_MENU_PREV,
   U8X8_MSG_GPIO_MENU_UP, U8X8_MSG_GPIO_MENU_DOWN.
 */

uint8_t u8x8_GetMenuEvent(u8x8_t *u8x8)
{
    (void)u8x8;
    uint32_t  received_message = 0;
    rt_tick_t now              = rt_tick_get();

    if (rt_mb_recv(mui_msg_mb, (rt_uint32_t *)&received_message, IDLE_WAIT_TICK) != RT_EOK)
    {
        if (!sleeping && (sleep_ticks != 0)
            && (last_activity + sleep_ticks < now))
        {
            /* default: 1 minute inactivity switches screen blanking on */
            u8x8_SetPowerSave(u8x8, 1);
            sleeping = RT_TRUE;
        }
        return 0;
    }

    last_activity = now;
    if (sleeping == RT_TRUE)
    {
        /* first click when sleeping switches screen blanking off */
        u8x8_SetPowerSave(u8x8, 0);
        sleeping = RT_FALSE;
        return 0;
    }

    return received_message;
}

/*
 * send a message to wake up mui display
 */

rt_err_t mui_refresh()
{
    return rt_mb_send(mui_msg_mb, U8X8_MSG_DISPLAY_REFRESH);
}

INIT_COMPONENT_EXPORT(multi_direction_switch);

