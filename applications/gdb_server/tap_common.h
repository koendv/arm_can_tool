#ifndef TAP_COMMON_H
#define TAP_COMMON_H

/*
 * AT32F405 SWD/JTAG Transport GPIO Layer
 *
 * REQUIREMENTS
 * ------------
 *
 * MCU:
 *   AT32F405
 *
 * Level shifter:
 *   SN74AVC4T774
 *
 * Wiring (MCU = A-side of level shifter)
 *
 *   PA7  -> DIR1  (SWCLK direction)
 *   PA6  -> DIR2  (SWDIO direction)
 *   PA5  -> A1    (SWCLK / TCK)
 *   PA4  -> A2    (SWDIO / TMS)
 *   PA3  -> A3    (TDO)
 *   PA2  -> A4    (TDI)
 *   PA1  -> DIR3  (TDO direction)
 *   PA0  -> DIR4  (TDI direction)
 *
 * DIR polarity:
 *   DIR = 1  A -> B  (MCU drives target pin)
 *   DIR = 0  B -> A  (target drives MCU pin)
 *
 * GPIO Ownership:
 *   This module assumes exclusive control of GPIOA pins 0..7.
 *
 * TIMING REQUIREMENT:
 *   DWT cycle counter must be enabled before using this code.
 *
 * INTERRUPT REQUIREMENT:
 *   Interrupts are switched off during gpio bitbanging.
 *
 * Example DWT configuration:
 *
 *   CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
 *   DWT->CYCCNT = 0;
 *   DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
 *
 * RT-Thread Logging:
 *   Uses LOG_E / LOG_W / LOG_I / LOG_D
 *   rt-thread logging is ulog() with async output.
 *
 */

#include <at32f402_405_gpio.h>
#include <stdint.h>

#include "platform.h"

/* gcc attributes */
//#define OPTIMIZE  __attribute__((optimize("Os")))
#define OPTIMIZE  __attribute__((optimize(3)))
#define RAMFUNC   __attribute__((section(".ramfunc")))
#define INLINE    inline __attribute__((always_inline))
#define BARRIER() __asm__ volatile("" ::: "memory")
#define NOP()     __asm__("nop")
#if 1
#define INTERRUPTS_OFF()
#define INTERRUPTS_ON()
#else
#define INTERRUPTS_OFF()    \
    uint32_t irq_level;     \
    __asm volatile(         \
        "MRS %0, PRIMASK\n" \
        "CPSID i"           \
        : "=r"(irq_level)::"memory")

#define INTERRUPTS_ON() \
    __asm volatile(     \
        "MSR PRIMASK, %0" ::"r"(irq_level) : "memory")
#endif

#define BIT(n) (1U << (n))

#define SWD_PORT GPIOA

#define TAP_SWCLK_DIR_PIN 7
#define TAP_SWDIO_DIR_PIN 6
#define TAP_SWCLK_PIN     5
#define TAP_SWDIO_PIN     4
#define TAP_TDO_PIN       3
#define TAP_TDI_PIN       2
#define TAP_TDO_DIR_PIN   1
#define TAP_TDI_DIR_PIN   0

#define TAP_SWCLK_DIR_MASK BIT(TAP_SWCLK_DIR_PIN)
#define TAP_SWDIO_DIR_MASK BIT(TAP_SWDIO_DIR_PIN)
#define TAP_SWCLK_MASK     BIT(TAP_SWCLK_PIN)
#define TAP_SWDIO_MASK     BIT(TAP_SWDIO_PIN)
#define TAP_TDO_MASK       BIT(TAP_TDO_PIN)
#define TAP_TDI_MASK       BIT(TAP_TDI_PIN)
#define TAP_TDO_DIR_MASK   BIT(TAP_TDO_DIR_PIN)
#define TAP_TDI_DIR_MASK   BIT(TAP_TDI_DIR_PIN)

/* DWT delay */
extern uint32_t target_clk_divider;
extern uint32_t half_target_clk_divider;

extern void swdptap_platform_init(void);
extern void jtagtap_platform_init(void);

#endif
