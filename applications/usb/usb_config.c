#include <rtthread.h>
#include <stdarg.h>
#define LOG_TAG "USB"
#define LOG_LVL LOG_LVL_DBG
#include <rtdbg.h>
#include "usb_config.h"

static char buf[128];

// log cherryusb using rt-thread log

void usb_logger(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    rt_vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    LOG_E("%s", buf);
}

// usb fifo size sanity check

enum
{
    // force compiler to calculate FIFO sizes in bytes
    USB_RX_FIFO_BYTES  = CONFIG_USB_DWC2_RXALL_FIFO_SIZE * 4,
    USB_TX0_FIFO_BYTES = CONFIG_USB_DWC2_TX0_FIFO_SIZE * 4,
    USB_TX1_FIFO_BYTES = CONFIG_USB_DWC2_TX1_FIFO_SIZE * 4,
    USB_TX2_FIFO_BYTES = CONFIG_USB_DWC2_TX2_FIFO_SIZE * 4,
    USB_TX3_FIFO_BYTES = CONFIG_USB_DWC2_TX3_FIFO_SIZE * 4,
    USB_FIFO_BYTES =
        USB_RX_FIFO_BYTES
        + USB_TX0_FIFO_BYTES
        + USB_TX1_FIFO_BYTES
        + USB_TX2_FIFO_BYTES
        + USB_TX3_FIFO_BYTES
};

static int usb_fifo_check(void)
{
    // FIFO sanity assertions
    _Static_assert(USB_RX_FIFO_BYTES >= 768, "RX FIFO too small");
    _Static_assert(USB_TX0_FIFO_BYTES >= 64, "EP0 FIFO too small");
    _Static_assert(USB_TX1_FIFO_BYTES >= 128, "GSUSB FIFO too small for CAN-FD");
    _Static_assert(USB_TX1_FIFO_BYTES >= 512, "MSC FIFO too small");
    _Static_assert(USB_TX2_FIFO_BYTES >= 64, "CDC console FIFO too small");
    _Static_assert(USB_TX3_FIFO_BYTES >= 64, "CDC GDB FIFO too small");
    _Static_assert(USB_FIFO_BYTES <= 4096, "usb ram overflow");

    return RT_EOK;
}

INIT_COMPONENT_EXPORT(usb_fifo_check);

