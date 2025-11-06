/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-10-18     shelton      first version
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <drv_common.h>
#include <drv_gpio.h>
#include "ds3231_util.h"
#include "watchdog.h"
#include "settings.h"
#include "logger.h"
#include "pins.h"

#define DBG_TAG "MAIN"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

int main(void)
{
    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOD_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOF_PERIPH_CLOCK, TRUE);
    LOG_I("ready");

    /* set led0 pin mode to output */
    rt_pin_mode(LED0_PIN, PIN_MODE_OUTPUT);

    ds3231_sync();
    logger("\r\nboot\r\n", 8);

    while (1)
    {
        rt_pin_write(LED0_PIN, PIN_LOW);
        rt_thread_mdelay(500);
        rt_pin_write(LED0_PIN, PIN_HIGH);
        rt_thread_mdelay(4500);
        feed_watchdog();
    }
}
