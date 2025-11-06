#ifndef PLATFORM_GPIO_H
#define PLATFORM_GPIO_H

/* speed optimized gpio, direct to hardware registers */

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


/* switch pins from read to write */
#define TMS_SET_MODE()          \
    {                           \
        GPIOA_OUTPUT(TMS_BIT);  \
        GPIOA_SET(TMS_DIR_BIT); \
    }
#define SWDIO_MODE_FLOAT()        \
    {                             \
        GPIOA_INPUT(SWDIO_BIT);   \
        GPIOA_CLEAR(TMS_DIR_BIT); \
    }
#define SWDIO_MODE_DRIVE()       \
    {                            \
        GPIOA_OUTPUT(SWDIO_BIT); \
        GPIOA_SET(TMS_DIR_BIT);  \
    }

#define gpio_clear(port, pin)           \
    {                                   \
        switch (pin)                    \
        {                               \
        case SWCLK_PIN:                 \
            GPIOA_CLEAR(SWCLK_BIT);     \
            break;                      \
        case SWDIO_PIN:                 \
            GPIOA_CLEAR(SWDIO_BIT);     \
            break;                      \
        case TDO_PIN:                   \
            GPIOA_CLEAR(TDO_BIT);       \
            break;                      \
        case TDI_PIN:                   \
            GPIOA_CLEAR(TDI_BIT);       \
            break;                      \
        default:                        \
            rt_pin_write(pin, PIN_LOW); \
            break;                      \
        }                               \
    }

#define gpio_set(port, pin)              \
    {                                    \
        switch (pin)                     \
        {                                \
        case SWCLK_PIN:                  \
            GPIOA_SET(SWCLK_BIT);        \
            break;                       \
        case SWDIO_PIN:                  \
            GPIOA_SET(SWDIO_BIT);        \
            break;                       \
        case TDO_PIN:                    \
            GPIOA_SET(TDO_BIT);          \
            break;                       \
        case TDI_PIN:                    \
            GPIOA_SET(TDI_BIT);          \
            break;                       \
        default:                         \
            rt_pin_write(pin, PIN_HIGH); \
            break;                       \
        }                                \
    }

#define gpio_set_val(port, pin, value)                     \
    {                                                      \
        switch (pin)                                       \
        {                                                  \
        case SWCLK_PIN:                                    \
            GPIOA_WRITE(SWCLK_BIT, value);                 \
            break;                                         \
        case SWDIO_PIN:                                    \
            GPIOA_WRITE(SWDIO_BIT, value);                 \
            break;                                         \
        case TDO_PIN:                                      \
            GPIOA_WRITE(TDO_BIT, value);                   \
            break;                                         \
        case TDI_PIN:                                      \
            GPIOA_WRITE(TDI_BIT, value);                   \
            break;                                         \
        default:                                           \
            rt_pin_write(pin, value ? PIN_HIGH : PIN_LOW); \
            break;                                         \
        }                                                  \
    }

#define gpio_get(port, pin)                                                                         \
    (pin == SWCLK_PIN ? GPIOA_READ(SWCLK_BIT)                                                       \
                      : (pin == SWDIO_PIN ? GPIOA_READ(SWDIO_BIT)                                   \
                                          : (pin == TDO_PIN ? GPIOA_READ(TDO_BIT)                   \
                                                            : (pin == TDI_PIN ? GPIOA_READ(TDI_BIT) \
                                                                              : rt_pin_read(pin)))))

#endif
