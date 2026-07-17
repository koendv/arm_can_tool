#include <rtthread.h>
#define DBG_TAG "CAN"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>
#include "at32f402_405.h"
#include "canbus.h"
#include "canbus_event.h"
#include "can_calc_at32timing.h"
#include "settings.h"

/* CAN bitrate configuration for PCLK = 108 MHz */
typedef struct
{
    uint32_t          bitrate;
    can_baudrate_type config;
} can_bitrate_config_t;

/* 
 * at32f405 requires 12MHz crystal, SYSCLK 216MHz for USB-2.0 HS, AHB /1 = 216 MHz, APB1 /2 = 108 MHz.
 *
 * generated using linux shell command:
 * $ ~/src/can-utils/can-calc-bit-timing --alg=v6.3 -c 108000000 bxcan | python3 ./tools/canbus/canbus_bitrates.py
 * verified using rt-thread shell command:
 * msh> can_calc_verify
 * keep in sync with CANBUS_SPEEDS in mui_form.c
 */

static const can_bitrate_config_t bitrate_configs[] = {
    {  .bitrate = 10000, .config = {.baudrate_div = 675, .rsaw_size = CAN_RSAW_1TQ, .bts1_size = CAN_BTS1_13TQ, .bts2_size = CAN_BTS2_2TQ}}, /* real=10000Hz sample=87.5% */
    {  .bitrate = 20000,  .config = {.baudrate_div = 675, .rsaw_size = CAN_RSAW_1TQ, .bts1_size = CAN_BTS1_6TQ, .bts2_size = CAN_BTS2_1TQ}}, /* real=20000Hz sample=87.5% */
    {  .bitrate = 33333,  .config = {.baudrate_div = 405, .rsaw_size = CAN_RSAW_1TQ, .bts1_size = CAN_BTS1_6TQ, .bts2_size = CAN_BTS2_1TQ}}, /* real=33333Hz sample=87.5% */
    {  .bitrate = 50000, .config = {.baudrate_div = 135, .rsaw_size = CAN_RSAW_1TQ, .bts1_size = CAN_BTS1_13TQ, .bts2_size = CAN_BTS2_2TQ}}, /* real=50000Hz sample=87.5% */
    {  .bitrate = 83333,  .config = {.baudrate_div = 81, .rsaw_size = CAN_RSAW_1TQ, .bts1_size = CAN_BTS1_13TQ, .bts2_size = CAN_BTS2_2TQ}}, /* real=83333Hz sample=87.5% */
    { .bitrate = 100000,  .config = {.baudrate_div = 135, .rsaw_size = CAN_RSAW_1TQ, .bts1_size = CAN_BTS1_6TQ, .bts2_size = CAN_BTS2_1TQ}}, /* real=100000Hz sample=87.5% */
    { .bitrate = 125000,  .config = {.baudrate_div = 54, .rsaw_size = CAN_RSAW_1TQ, .bts1_size = CAN_BTS1_13TQ, .bts2_size = CAN_BTS2_2TQ}}, /* real=125000Hz sample=87.5% */
    { .bitrate = 250000,  .config = {.baudrate_div = 27, .rsaw_size = CAN_RSAW_1TQ, .bts1_size = CAN_BTS1_13TQ, .bts2_size = CAN_BTS2_2TQ}}, /* real=250000Hz sample=87.5% */
    { .bitrate = 500000,   .config = {.baudrate_div = 27, .rsaw_size = CAN_RSAW_1TQ, .bts1_size = CAN_BTS1_6TQ, .bts2_size = CAN_BTS2_1TQ}}, /* real=500000Hz sample=87.5% */
    { .bitrate = 666666,   .config = {.baudrate_div = 9, .rsaw_size = CAN_RSAW_2TQ, .bts1_size = CAN_BTS1_13TQ, .bts2_size = CAN_BTS2_4TQ}}, /* real=666666Hz sample=77.7% */
    { .bitrate = 800000,   .config = {.baudrate_div = 9, .rsaw_size = CAN_RSAW_1TQ, .bts1_size = CAN_BTS1_11TQ, .bts2_size = CAN_BTS2_3TQ}}, /* real=800000Hz sample=80.0% */
    {.bitrate = 1000000,    .config = {.baudrate_div = 9, .rsaw_size = CAN_RSAW_1TQ, .bts1_size = CAN_BTS1_8TQ, .bts2_size = CAN_BTS2_3TQ}}, /* real=1000000Hz sample=75.0% */
};

void can_gpio_config(void)
{
    gpio_init_type gpio_init_struct;

    /* enable the gpio clock */
    crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);

    gpio_default_para_init(&gpio_init_struct);

    /* configure the can tx, rx pin */
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init_struct.gpio_out_type       = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_mode           = GPIO_MODE_MUX;
    gpio_init_struct.gpio_pins           = GPIO_PINS_9 | GPIO_PINS_8;
    gpio_init_struct.gpio_pull           = GPIO_PULL_NONE;
    gpio_init(GPIOB, &gpio_init_struct);

    gpio_pin_mux_config(GPIOB, GPIO_PINS_SOURCE9, GPIO_MUX_9);
    gpio_pin_mux_config(GPIOB, GPIO_PINS_SOURCE8, GPIO_MUX_9);
}

rt_err_t can_configure_device(void)
{
    can_base_type        can_base_struct;
    can_filter_init_type can_filter_init_struct;

    /* as specified in CAN protocol, the maximum allowable oscillator tolerance is 1.58%. 
     The HICK accuracy does not meet the clock requirements in CAN protocol. to guarantee normal 
     communication, it is recommended to use HEXT as the system clock source. */
    if (crm_flag_get(CRM_HEXT_STABLE_FLAG) != SET)
    {
        return -RT_ERROR;
    }

    /* enable the can clock */
    crm_periph_clock_enable(CRM_CAN1_PERIPH_CLOCK, TRUE);

    /* can base init */
    can_default_para_init(&can_base_struct);
    can_base_struct.mode_selection   = CAN_MODE_COMMUNICATE;
    can_base_struct.ttc_enable       = FALSE;
    can_base_struct.aebo_enable      = TRUE;
    can_base_struct.aed_enable       = TRUE;
    can_base_struct.prsf_enable      = FALSE;
    can_base_struct.mdrsel_selection = CAN_DISCARDING_FIRST_RECEIVED;
    can_base_struct.mmssr_selection  = CAN_SENDING_BY_ID;
    can_base_init(CAN1, &can_base_struct);

    /* can baudrate, set baudrate from settings */
    if (can_set_bitrate_index(settings.can1_speed) != RT_EOK)
    {
        LOG_E("can_baudrate_set() fail");
        return -RT_ERROR;
    }

    /* can filter init */
    can_filter_init_struct.filter_activate_enable = TRUE;
    can_filter_init_struct.filter_mode            = CAN_FILTER_MODE_ID_MASK;
    can_filter_init_struct.filter_fifo            = CAN_FILTER_FIFO0;
    can_filter_init_struct.filter_number          = 0;
    can_filter_init_struct.filter_bit             = CAN_FILTER_32BIT;
    can_filter_init_struct.filter_id_high         = 0;
    can_filter_init_struct.filter_id_low          = 0;
    can_filter_init_struct.filter_mask_high       = 0;
    can_filter_init_struct.filter_mask_low        = 0;
    can_filter_init(CAN1, &can_filter_init_struct);

    return RT_EOK;
}

/* set bitrate using index i = 0, 1, 2, ... */
rt_err_t can_set_bitrate_index(uint32_t i)
{
    can_baudrate_type can_baudrate_struct;
    uint32_t          max_index = sizeof(bitrate_configs) / sizeof(bitrate_configs[0]);

    if (i >= max_index)
    {
        LOG_E("can_set_bitrate_index(%d) out of range", i);
        return -RT_EINVAL;
    }

    if (can_baudrate_set(CAN1, (can_baudrate_type *)&bitrate_configs[i].config) == SUCCESS)
    {
        LOG_I("speed %d", bitrate_configs[i].bitrate);
        return RT_EOK;
    }
    LOG_E("can_set_bitrate_index(%d) f=%d fail", i, bitrate_configs[i].bitrate);
    return -RT_ERROR;
}

/* set bitrate using frequency f = 10000, 20000, ... , 800000, 1000000 */
rt_err_t can_set_bitrate_freq(uint32_t f)
{
    can_baudrate_type  cbt_calc;
    can_baudrate_type *cbt       = NULL;
    uint32_t           max_index = sizeof(bitrate_configs) / sizeof(bitrate_configs[0]);

    /* look up bitrate in table */
    for (uint32_t i = 0; i < max_index; i++)
    {
        if (f == bitrate_configs[i].bitrate)
        {
            cbt = (can_baudrate_type *)&bitrate_configs[i].config;
            break;
        }
    }

    /* if bitrate not in table, calculate */
    if (cbt == NULL)
    {
        if (can_calc_at32_timing(f, &cbt_calc) == RT_EOK)
            cbt = &cbt_calc;
    }

    /* error if bitrate not in table, and calculation fails */
    if (cbt == NULL)
    {
        LOG_E("can_set_bitrate_freq(%d) value invalid", f);
        return -RT_EINVAL;
    }

    /* set canbus speed */
    if (can_baudrate_set(CAN1, cbt) == SUCCESS)
    {
        LOG_I("bitrate %d", f);
        return RT_EOK;
    }

    /* set canbus speed fail */
    LOG_E("can_set_bitrate_freq(%d) fail", f);
    return -RT_ERROR;
}

/* can_init() called by settings.c when settings have been read */

rt_err_t can_init(bool enable)
{
    can_operating_mode_type can1_op_mode;

    /* set pin alternate function */
    can_gpio_config();

    /* set speed */
    if (can_configure_device() != RT_EOK)
    {
        LOG_E("can configure speed fail");
        return -RT_ERROR;
    }

    /* set operating mode */
    if (enable)
        can1_op_mode = CAN_OPERATINGMODE_COMMUNICATE;
    else
        can1_op_mode = CAN_OPERATINGMODE_FREEZE;
    if (can_operating_mode_set(CAN1, can1_op_mode) != SUCCESS)
    {
        LOG_E("can_operating_mode_set() fail");
        return -RT_ERROR;
    }

    /* set filter */
    if (settings.canfilter_enable)
        bxcan_set_filter(settings.can1_hw_filter);
    else
        bxcan_set_filter(canfilter_bxcan_f0_pass_all);

    /* set up canbus interrupts  */
    if (canbus_event_init() != RT_EOK)
    {
        LOG_E("canbus event init fail");
        return -RT_ERROR;
    }

    LOG_I("init");
    return RT_EOK;
}
