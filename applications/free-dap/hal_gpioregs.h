#ifndef FREEDAP_HAL_GPIOREGS_H
#define FREEDAP_HAL_GPIOREGS_H

#include <rtthread.h>
#include <rtconfig.h>
#include <drv_gpio.h>
#include <pins.h>

#define SWCLK_PIN     TARGET_SWCLK_PIN
#define SWDIO_PIN     TARGET_SWDIO_PIN
#define TDI_DIR_PIN   TARGET_TDI_DIR_PIN
#define TDI_PIN       TARGET_TDI_PIN
#define TDO_DIR_PIN   TARGET_TDO_DIR_PIN
#define TDO_PIN       TARGET_TDO_PIN
#define TCK_DIR_PIN   TARGET_SWCLK_DIR_PIN
#define SWCLK_DIR_PIN TARGET_SWCLK_DIR_PIN
#define TMS_DIR_PIN   TARGET_SWDIO_DIR_PIN
#define NRST_IN_PIN   TARGET_RST_IN_PIN
#define NRST_OUT_PIN  TARGET_RST_OUT_PIN

/* GPIO Register Definitions */
#define REG_GPIOA_CLEAR (*((volatile uint32_t *)0x4002001A)) /* Bit clear register */
#define REG_GPIOA_SET   (*((volatile uint32_t *)0x40020018)) /* Bit set register */
#define REG_GPIOA_IDT   (*((volatile uint32_t *)0x40020010)) /* Input status */
#define REG_GPIOA_CFGR  (*((volatile uint32_t *)0x40020000)) /* Configuration low */

/* Pin Bit Positions */
#define TDI_DIR_BIT   0U /* PA0 */
#define TDO_DIR_BIT   1U /* PA1 */
#define TDI_BIT       2U /* PA2 */
#define TDO_BIT       3U /* PA3 */
#define SWDIO_BIT     4U /* PA4 */
#define SWCLK_BIT     5U /* PA5 */
#define SWDIO_DIR_BIT 6U /* PA6 */
#define SWCLK_DIR_BIT 7U /* PA7 */
#define TMS_BIT       SWDIO_BIT
#define TCK_BIT       SWCLK_BIT
#define TMS_DIR_BIT   SWDIO_DIR_BIT
#define TCK_DIR_BIT   SWCLK_DIR_BIT

/* GPIO Mode Configuration */
/* Assumes the pin is in MODE_INPUT or MODE_OUTPUT */
static inline void GPIOA_OUTPUT(uint32_t pin)
{
    REG_GPIOA_CFGR = REG_GPIOA_CFGR & ~((uint32_t)0x3 << (pin * 2)) | ((uint32_t)0x1 << (pin * 2));
}

static inline void GPIOA_INPUT(uint32_t pin)
{
    REG_GPIOA_CFGR &= ~((uint32_t)0x3 << (pin * 2));
}

/* GPIO Access Functions */
static inline void GPIOA_SET(uint32_t pin)
{
    REG_GPIOA_SET = (1U << pin);
}

static inline void GPIOA_CLEAR(uint32_t pin)
{
    REG_GPIOA_CLEAR = (1U << pin);
}

static inline void GPIOA_WRITE(uint32_t pin, int value)
{
    if (value)
        REG_GPIOA_SET = (1U << pin);
    else
        REG_GPIOA_CLEAR = (1U << pin);
}

static inline int GPIOA_READ(uint32_t pin)
{
    return (REG_GPIOA_IDT & (1U << pin)) != 0;
}

static inline void HAL_GPIO_SWCLK_TCK_write(int value)
{
    GPIOA_WRITE(SWCLK_BIT, value);
}

static inline void HAL_GPIO_SWCLK_TCK_set()
{
    GPIOA_SET(SWCLK_BIT);
}

static inline void HAL_GPIO_SWCLK_TCK_clr()
{
    GPIOA_CLEAR(SWCLK_BIT);
}

static inline int HAL_GPIO_SWCLK_TCK_read()
{
    return GPIOA_READ(SWCLK_BIT);
}

static inline void HAL_GPIO_SWCLK_TCK_out()
{
    GPIOA_SET(SWCLK_DIR_BIT);
    GPIOA_OUTPUT(SWCLK_BIT);
}

static inline void HAL_GPIO_SWCLK_TCK_in()
{
    GPIOA_INPUT(SWCLK_BIT);
    GPIOA_CLEAR(SWCLK_DIR_BIT);
}

static inline void HAL_GPIO_SWDIO_TMS_write(int value)
{
    GPIOA_WRITE(SWDIO_BIT, value);
}

static inline void HAL_GPIO_SWDIO_TMS_set()
{
    GPIOA_SET(SWDIO_BIT);
}

static inline int HAL_GPIO_SWDIO_TMS_read()
{
    return GPIOA_READ(SWDIO_BIT);
}

static inline void HAL_GPIO_SWDIO_TMS_out()
{
    GPIOA_SET(SWDIO_DIR_BIT);
    GPIOA_OUTPUT(SWDIO_BIT);
}

static inline void HAL_GPIO_SWDIO_TMS_in()
{
    GPIOA_CLEAR(SWDIO_DIR_BIT);
    GPIOA_INPUT(SWDIO_BIT);
}

#ifdef DAP_CONFIG_ENABLE_JTAG
static inline void HAL_GPIO_TDI_write(int value)
{
    GPIOA_WRITE(TDI_BIT, value);
}

static inline void HAL_GPIO_TDI_set()
{
    GPIOA_SET(TDI_BIT);
}

static inline int HAL_GPIO_TDI_read()
{
    return GPIOA_READ(TDI_BIT);
}

static inline void HAL_GPIO_TDI_in()
{
    GPIOA_CLEAR(TDI_DIR_BIT);
    GPIOA_INPUT(TDI_BIT);
}

static inline void HAL_GPIO_TDI_out()
{
    GPIOA_SET(TDI_DIR_BIT);
    GPIOA_OUTPUT(TDI_BIT);
}

static inline void HAL_GPIO_TDO_write(int value)
{
    GPIOA_WRITE(TDO_BIT, value);
}

static inline int HAL_GPIO_TDO_read()
{
    return GPIOA_READ(TDO_BIT);
}

static inline void HAL_GPIO_TDO_in()
{
    GPIOA_CLEAR(TDO_DIR_BIT);
    GPIOA_INPUT(TDO_BIT);
}

static inline void HAL_GPIO_TDO_out()
{
    GPIOA_SET(TDO_DIR_BIT);
    GPIOA_OUTPUT(TDO_BIT);
}
#endif

static inline void HAL_GPIO_nRESET_write(int value)
{
    /* pull-down transistor inverts logic */
    rt_pin_write(NRST_OUT_PIN, value ? PIN_LOW : PIN_HIGH);
}

static inline void HAL_GPIO_nRESET_set()
{
    /* pull-down transistor inverts logic */
    rt_pin_write(NRST_OUT_PIN, PIN_LOW);
}

static inline int HAL_GPIO_nRESET_read()
{
    return rt_pin_read(NRST_IN_PIN);
}

static inline void HAL_GPIO_nRESET_out()
{
    // arm_can_tool has different NRST_OUT and NRST_IN pins
}

static inline void HAL_GPIO_nRESET_in()
{
    // arm_can_tool has different NRST_OUT and NRST_IN pins
}

static inline void DAP_config(void)
{
#ifdef TARGET_OE_PIN
    rt_pin_write(TARGET_OE_PIN, PIN_LOW); // enable SWD/JTAG output
    rt_pin_mode(TARGET_OE_PIN, PIN_MODE_OUTPUT);
#endif

#ifdef SWCLK_DIR_PIN
    rt_pin_write(SWCLK_DIR_PIN, PIN_LOW);
    rt_pin_mode(SWCLK_DIR_PIN, PIN_MODE_OUTPUT);
#endif

#ifdef SWDIO_DIR_PIN
    rt_pin_write(SWDIO_DIR_PIN, PIN_LOW);
    rt_pin_mode(SWDIO_DIR_PIN, PIN_MODE_OUTPUT);
#endif

#ifdef NRST_OUT_PIN
    rt_pin_mode(NRST_OUT_PIN, PIN_MODE_OUTPUT);
#endif

#ifdef NRST_IN_PIN
    rt_pin_mode(NRST_IN_PIN, PIN_MODE_INPUT);
#endif

#ifdef TARGET_IO0_PIN
    rt_pin_mode(TARGET_IO0_PIN, PIN_MODE_INPUT);
#endif
}

#endif
