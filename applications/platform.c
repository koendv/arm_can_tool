#include "general.h"
#include "platform.h"
#include "settings.h"
#include "serials.h"
#include <rtthread.h>

#define DBG_TAG "BMD"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#include "usb_cdc.h"

#define ADC_DEV_NAME    "adc1" /* ADC device name */
#define ADC_VIO_CHANNEL 13     /* ADC target voltage channel */

bool            running_status = false;
rt_adc_device_t adc_dev        = NULL;

void platform_init(void)
{
#ifdef LED_IDLE_RUN
    rt_pin_write(LED_IDLE_RUN, PIN_HIGH);
    rt_pin_mode(LED_IDLE_RUN, PIN_MODE_OUTPUT);
#endif

#ifdef NRST_OUT_PIN
    rt_pin_write(NRST_OUT_PIN, PIN_LOW);
    rt_pin_mode(NRST_OUT_PIN, PIN_MODE_OUTPUT);
#endif

#ifdef NRST_IN
    rt_pin_mode(NRST_IN_PIN, PIN_MODE_INPUT);
#endif

    rt_pin_write(SWCLK_PIN, PIN_HIGH);
    rt_pin_mode(SWCLK_PIN, PIN_MODE_OUTPUT);

    rt_pin_write(SWDIO_PIN, PIN_HIGH);
    rt_pin_mode(SWDIO_PIN, PIN_MODE_OUTPUT);

#ifdef SWCLK_DIR_PIN
    rt_pin_write(SWCLK_DIR_PIN, PIN_HIGH);
    rt_pin_mode(SWCLK_DIR_PIN, PIN_MODE_OUTPUT);
#endif

#ifdef SWDIO_DIR_PIN
    rt_pin_write(SWDIO_DIR_PIN, PIN_HIGH);
    rt_pin_mode(SWDIO_DIR_PIN, PIN_MODE_OUTPUT);
#endif

#ifdef TDI_DIR_PIN
    rt_pin_write(TDI_DIR_PIN, PIN_HIGH);
    rt_pin_mode(TDI_DIR_PIN, PIN_MODE_OUTPUT);
#endif

#ifdef TDO_DIR_PIN
    rt_pin_write(TDO_DIR_PIN, PIN_LOW);
    rt_pin_mode(TDO_DIR_PIN, PIN_MODE_OUTPUT);
#endif

#ifdef TARGET_POWER_PIN
    if (settings.tpower_enable)
        rt_pin_write(TARGET_POWER_PIN, PIN_HIGH); // switch target power on
    else
        rt_pin_write(TARGET_POWER_PIN, PIN_LOW);  // switch target power off
    rt_pin_mode(TARGET_POWER_PIN, PIN_MODE_OUTPUT);
#endif

#ifdef TARGET_OE_PIN
    rt_pin_write(TARGET_OE_PIN, PIN_LOW); // enable SWD/JTAG output
    rt_pin_mode(TARGET_OE_PIN, PIN_MODE_OUTPUT);
#endif
}

void platform_nrst_set_val(bool assert)
{
#ifdef NRST_OUT_PIN
    if (assert)
    {
        rt_pin_write(NRST_OUT_PIN, PIN_HIGH);
    }
    else
    {
        rt_pin_write(NRST_OUT_PIN, PIN_LOW);
    }
#else
    (void)assert;
#endif
}

bool platform_nrst_get_val(void)
{
#ifdef NRST_IN_PIN
    return (rt_pin_read(NRST_IN_PIN) ? false : true);
#else
    return 1;
#endif
}

/* return target voltage in millivolt */
int32_t platform_target_millivolt(void)
{
    int32_t target_vio_mv = 0;

    if (adc_dev == NULL)
    {
        adc_dev = (rt_adc_device_t)rt_device_find(ADC_DEV_NAME);
        if (adc_dev != NULL)
            rt_adc_enable(adc_dev, ADC_VIO_CHANNEL);
    }

    if (adc_dev == NULL)
        return -1;

    /* ADC_REF_VOLTAGE / 4095 * (10k + 4.7k) / 10k; calibrated */
    target_vio_mv = rt_adc_read(adc_dev, ADC_VIO_CHANNEL) * 4902 / 4095; /* millivolt */

    return target_vio_mv;
}

/* return target voltage as string */
const char *platform_target_voltage(void)
{
    static char target_vio_str[8];
    static char err_str[8] = "?";
    int32_t     target_vio_mv;

    target_vio_mv = platform_target_millivolt();
    if (target_vio_mv < 0)
        return err_str;
    snprintf(target_vio_str, sizeof(target_vio_str), "%d.%03dV", target_vio_mv / 1000, target_vio_mv % 1000);
    return target_vio_str;
}


int platform_hwversion(void)
{
    return 0;
}

void platform_target_clk_output_enable(bool enable)
{
#ifdef TARGET_SWCLK_DIR_PIN
    if (enable)
        rt_pin_write(TARGET_SWCLK_DIR_PIN, PIN_HIGH); // SWCLK out
    else
        rt_pin_write(TARGET_SWCLK_DIR_PIN, PIN_LOW);  // SWCLK in
#else
    (void)enable;
#endif
}

/* return whether target power is switched on or off */
bool platform_target_get_power(void)
{
    return (rt_pin_read(TARGET_POWER_PIN) ? true : false);
}

/* switch target power on/off */
bool platform_target_set_power(const bool power)
{
    if (power)
        rt_pin_write(TARGET_POWER_PIN, PIN_HIGH); // SWCLK out
    else
        rt_pin_write(TARGET_POWER_PIN, PIN_LOW);  // SWCLK in
    return true;
}

/* return target voltage in 1/10 volt */
uint32_t platform_target_voltage_sense(void)
{
    int32_t target_vio_10;

    target_vio_10 = (platform_target_millivolt() + 50) / 100;
    if (target_vio_10 < 0) return 0;
    return target_vio_10;
}

bool platform_spi_init(const spi_bus_e bus)
{
    (void)bus;
    return false;
}

bool platform_spi_deinit(const spi_bus_e bus)
{
    (void)bus;
    return false;
}

bool platform_spi_chip_select(const uint8_t device_select)
{
    (void)device_select;
    return false;
}

uint8_t platform_spi_xfer(const spi_bus_e bus, const uint8_t value)
{
    (void)bus;
    return value;
}

void debug_serial_send_stdout(const uint8_t * const data, const size_t len)
{
    cdc1_write((uint8_t *)data, len);
}

size_t debug_serial_debug_write(const char *buf, const size_t len)
{
    cdc1_write((uint8_t *)buf, len);
    return len;
}

void vtarget(int argc, char **argv)
{
    rt_kprintf("vio: %s\r\n", platform_target_voltage());
}

MSH_CMD_EXPORT(vtarget, target voltage);

void swdptap_platform_init()
{
    /* in swd mode TDO and TDI are used as serial port */
    rt_pin_write(TARGET_TDI_DIR_PIN, PIN_HIGH);
    rt_pin_write(TARGET_TDO_DIR_PIN, PIN_LOW);
    rt_pin_mode(TARGET_TDI_DIR_PIN, PIN_MODE_OUTPUT);
    rt_pin_mode(TARGET_TDO_DIR_PIN, PIN_MODE_OUTPUT);

    at32_msp_usart_init(USART2);
    serial0_enable(settings.serial0_enable);
}

void jtagtap_platform_init()
{
    /* in jtag mode TDO and TDI are GPIO pins */
    serial0_enable(false);

    rt_pin_write(TARGET_TDI_DIR_PIN, PIN_HIGH);
    rt_pin_write(TARGET_TDO_DIR_PIN, PIN_LOW);
    rt_pin_mode(TARGET_TDI_DIR_PIN, PIN_MODE_OUTPUT);
    rt_pin_mode(TARGET_TDO_DIR_PIN, PIN_MODE_OUTPUT);

    rt_pin_write(TARGET_TDI_PIN, PIN_HIGH);
    rt_pin_mode(TARGET_TDI_PIN, PIN_MODE_OUTPUT);

    rt_pin_mode(TARGET_TDO_PIN, PIN_MODE_INPUT);

    rt_pin_write(TMS_DIR_PIN, PIN_HIGH);
    rt_pin_write(TCK_DIR_PIN, PIN_HIGH);
    rt_pin_mode(TMS_DIR_PIN, PIN_MODE_OUTPUT);
    rt_pin_mode(TCK_DIR_PIN, PIN_MODE_OUTPUT);

    rt_pin_write(TMS_PIN, PIN_HIGH);
    rt_pin_write(TCK_PIN, PIN_HIGH);
    rt_pin_mode(TMS_PIN, PIN_MODE_OUTPUT);
    rt_pin_mode(TCK_PIN, PIN_MODE_OUTPUT);
}

void target_power_enable(bool on_off)
{
    LOG_I("target power %d", on_off);
    rt_pin_write(TARGET_POWER_PIN, on_off ? PIN_HIGH : PIN_LOW); // switch power on/off
};

void target_output_enable(bool on_off)
{
    LOG_I("target output enable %d", on_off);
    rt_pin_write(TARGET_OE_PIN, on_off ? PIN_LOW : PIN_HIGH); // enable output on/off
};
