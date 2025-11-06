#include <rtdevice.h>
#include <rtthread.h>

#define DBG_TAG "WDT"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>
#include <settings.h>

#define WDT_NAME    "wdt"
#define WDT_TIMEOUT 20 /* watchdog timeout from 1 to 26 seconds */

/* watchdog timer
   resets system if feed_watchdog() is not called for WDT_TIMEOUT seconds.
   see also: syswatch component.
 */

static rt_device_t wdt_dev = RT_NULL;

static int init_watchdog(void)
{
    rt_err_t ret;

    if (!settings.watchdog_enable)
        return RT_EOK;

    wdt_dev = rt_device_find(WDT_NAME);
    if (wdt_dev == RT_NULL)
    {
        LOG_E("Cannot find watchdog device");
        return -RT_ERROR;
    }

    ret = rt_device_init(wdt_dev);
    if (ret != RT_EOK)
    {
        LOG_E("initialize watchdog failed");
        return -RT_ERROR;
    }

    int timeout = WDT_TIMEOUT;
    rt_device_control(wdt_dev, RT_DEVICE_CTRL_WDT_SET_TIMEOUT, &timeout);
    if (ret != RT_EOK)
    {
        LOG_E("set watchdog timeout failed");
        return -RT_ERROR;
    }

    rt_device_control(wdt_dev, RT_DEVICE_CTRL_WDT_START, RT_NULL);
    if (ret != RT_EOK)
    {
        LOG_E("start watchdog failed");
        return -RT_ERROR;
    }

    LOG_I("watchdog timer %ds", timeout);
    return RT_EOK;
}
INIT_APP_EXPORT(init_watchdog);

// in main loop or thread, feed the watchdog:

void feed_watchdog(void)
{
    if (wdt_dev)
    {
        rt_device_control(wdt_dev, RT_DEVICE_CTRL_WDT_KEEPALIVE, RT_NULL);
    }
}
