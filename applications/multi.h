/*
 5-way navigation switch with debouncing and auto-repeat.
 a long press on "select" jumps to the main screen.
 screen blanking if inactive for 30 seconds.
 */

#ifndef _MULTI_H_
#define _MULTI_H_

#include <rtthread.h>
#include <u8x8.h>

/*
   interface with mui.
   returns 0 0 if no key has been pressed; or
   U8X8_MSG_GPIO_MENU_HOME, U8X8_MSG_GPIO_MENU_SELECT,
   U8X8_MSG_GPIO_MENU_NEXT, U8X8_MSG_GPIO_MENU_PREV,
   U8X8_MSG_GPIO_MENU_UP, U8X8_MSG_GPIO_MENU_DOWN.
 */

uint8_t u8x8_GetMenuEvent(u8x8_t *u8x8);

/*
 * send a message to wake up mui
 */

rt_err_t mui_refresh();

#endif
