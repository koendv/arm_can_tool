#ifndef RT_BLACKMAGIC_PLATFORM_H
#define RT_BLACKMAGIC_PLATFORM_H

#include <sys/times.h>
#include <time.h>
#include <strings.h>
#include <stdio.h>
#include <stdbool.h>

#include <rtthread.h>
#include <rtdevice.h>

extern bool running_status;

#define PLATFORM_IDENT "AT32F405 "

#define SYSTEM_CORE_CLOCK 216000000
#define SWD_DEFAULT_CLOCK (2000000)

/* request halt running target */
void gdb_halt_target(void);

/* aux serial port - connect to target console */
#define AUX_UART          "uart2"
#define AUX_DEFAULT_SPEED 115200U
#define AUX_RX_BUFSIZE    BSP_UART2_RX_BUFSIZE

/* aux1 serial port - receive and transmit */
#define AUX1_UART          "uart3"
#define AUX1_DEFAULT_SPEED 115200U
#define AUX1_RX_BUFSIZE    BSP_UART3_RX_BUFSIZE

/* aux2 serial port - uart7, receive only, used for swo */

/* rtt input and output via usb cdc1 */
#define RTT_UP_BUF_SIZE   (2048U + 8U)
#define RTT_DOWN_BUF_SIZE 256U

#define PLATFORM_HAS_TRACESWO
#define SWO_ENCODING 2

#define PLATFORM_HAS_POWER_SWITCH

/* target reset pin */
#define NRST_IN_PIN  TARGET_RST_IN_PIN
#define NRST_OUT_PIN TARGET_RST_OUT_PIN

/* target swd pins */
#define SWCLK_PIN TARGET_SWCLK_PIN
#define SWDIO_PIN TARGET_SWDIO_PIN

/* direction for the level shifters */
#define SWCLK_DIR_PIN TARGET_SWCLK_DIR_PIN
#define SWDIO_DIR_PIN TARGET_SWDIO_DIR_PIN

/* jtag port */
#define TCK_PIN     TARGET_SWCLK_PIN
#define TMS_PIN     TARGET_SWDIO_PIN
#define TDI_PIN     TARGET_TDI_PIN
#define TDO_PIN     TARGET_TDO_PIN
#define TCK_DIR_PIN TARGET_SWCLK_DIR_PIN
#define TMS_DIR_PIN TARGET_SWDIO_DIR_PIN
#define TDI_DIR_PIN TARGET_TDI_DIR_PIN
#define TDO_DIR_PIN TARGET_TDO_DIR_PIN

/* dummy port definitions */
#define NRST_PORT  0
#define SWDIO_PORT 0
#define SWCLK_PORT 0
#define TCK_PORT   0
#define TMS_PORT   0
#define TDI_PORT   0
#define TDO_PORT   0

/* activity led */
#ifdef LED_IDLE_RUN
#define SET_RUN_STATE(state)                                    \
    {                                                           \
        rt_pin_write(LED_IDLE_RUN, state ? PIN_LOW : PIN_HIGH); \
        running_status = (state);                               \
    }
#define SET_IDLE_STATE(state)                                   \
    {                                                           \
        rt_pin_write(LED_IDLE_RUN, state ? PIN_HIGH : PIN_LOW); \
    }
#else
#define SET_RUN_STATE(state)      \
    {                             \
        running_status = (state); \
    }
#define SET_IDLE_STATE(state) \
    {                         \
    }
#endif
#define SET_ERROR_STATE(state)

#define strnlen(s, maxlen) rt_strnlen((s), (maxlen))

#define SWDPTAP_PLATFORM_INIT
#define JTAGTAP_PLATFORM_INIT

void swdptap_platform_init();
void jtagtap_platform_init();

void target_power_enable(bool on_off);
void target_output_enable(bool on_off);

#endif
