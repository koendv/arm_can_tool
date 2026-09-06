#include <rtthread.h>
#include <rtdevice.h>
#define DBG_TAG "USB"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>
#include "drv_common.h"
#include "at32_msp.h"
#include "usb_desc.h"
#include "usb_device.h"
#include "settings.h"

void usb_dc_low_level_init(void)
{
    at32_msp_usb_init(NULL);
    crm_periph_clock_enable(CRM_OTGHS_PERIPH_CLOCK, TRUE);
    nvic_irq_enable(OTGHS_IRQn, 0, 0);
}

void OTGHS_IRQHandler(void)
{
    extern void USBD_IRQHandler(uint8_t busid);
    USBD_IRQHandler(0);
}

int usbd_app_init(void)
{
    LOG_I("init");
    usb_composite_init(0, OTGHS_BASE, settings.mode);
    return 0;
}
