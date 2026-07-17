#include <rtthread.h>
#include <rtdevice.h>
#define DBG_TAG "TRIGGER"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>
#include <drv_gpio.h>
#include "settings.h"
#include "serials.h"

#define TRIGGER_IN_PIN GET_PIN(C, 1)

/* request target halt on falling edge of trigger */
static void trigger_isr(void *args)
{
    if (serial_event != RT_NULL)
        rt_event_send(serial_event, EVENT_MASK_TARGET_HALT_REQUEST);
}

static int trigger_init(void)
{
    rt_err_t ret;

    if (!settings.trigger_enable)
        return RT_EOK;

    rt_pin_mode(TRIGGER_IN_PIN, PIN_MODE_INPUT);
    ret = rt_pin_attach_irq(TRIGGER_IN_PIN,
                            PIN_IRQ_MODE_FALLING,
                            trigger_isr,
                            RT_NULL);
    if (ret != RT_EOK)
    {
        LOG_E("failed to attach irq");
        return ret;
    }
    ret = rt_pin_irq_enable(TRIGGER_IN_PIN, PIN_IRQ_ENABLE);
    if (ret != RT_EOK)
    {
        LOG_E("failed to enable irq");
        return ret;
    }

    LOG_I("irq armed (falling edge)");
    return RT_EOK;
}

INIT_APP_EXPORT(trigger_init);

