/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/cherry-embedded/CherryDAP
 */

#include <rtthread.h>
#define DBG_TAG "USB"
#define DBG_LVL DBG_ERR
#include <rtdbg.h>

#include "usbd_core.h"
#include "usbd_cdc_acm.h"
#include "usb_cdc0.h"
#include "usbd_msc.h"
#include "usb_desc.h"
#include "gsusb.h"
#include "serials.h"
#include "usb_ms.h"
#include "usb_serial_number.h"
extern void usbd_cdc_acm_serial_init(uint8_t busid, uint8_t in_ep, uint8_t out_ep);

#define USBD_VID           0x1209
#define USBD_PID           0x8816
#define USBD_MAX_POWER     100
#define USBD_LANGID_STRING 1033

/* event definitions */
#define EVENT_USB_RESET        (1 << 0)
#define EVENT_USB_CONNECTED    (1 << 1)
#define EVENT_USB_DISCONNECTED (1 << 2)
#define EVENT_USB_CONFIGURED   (1 << 3)

/* usb operating mode */
usb_mode_t usb_mode = MODE_CMSIS_DAP;

/* usb event handler */
static rt_event_t usb_event = RT_NULL; /* event for configuration */
static uint32_t   usb_busid = 0;

/*!< string descriptors */
static char product_name[48] = "?";

static const char *string_descriptors[] = {
    [STRID_LANG]         = (const char[]){0x09, 0x04},
    [STRID_MANUFACTURER] = "open hardware",
    [STRID_PRODUCT]      = product_name,
    [STRID_SERIAL]       = usb_serial_number,
    [STRID_WEBUSB]       = "https://github.com/koendv/arm_can_tool",
    [STRID_CANBUS]       = "CANBUS GSUSB",
    [STRID_SLCAN]        = "CANBUS SLCAN",
    [STRID_CDC0]         = "TARGET CONSOLE",
    [STRID_CDC1]         = "GDB SERVER",
    [STRID_DAP]          = "CMSIS-DAP",
    [STRID_MSC]          = "SDCARD",
    [STRID_CONSOLE]      = "RT-THREAD CONSOLE",
    [STRID_USER]         = "SCRIPT USER I/O",
    [STRID_SCRIPT]       = "SCRIPT CONSOLE",
};

/*!< config descriptor size */
#define BULK_DESCRIPTOR_LEN (9 + 7 + 7)

#define DAP_CONFIG_SIZE (9 + BULK_DESCRIPTOR_LEN + 2 * CDC_ACM_DESCRIPTOR_LEN)
#define DAP_INTF_NUM    (1 + 2 * 2)
#define DAP_BCD_DEVICE  0x100

static uint8_t dap_device_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_1, 0xEF, 0x02, 0x01, USBD_VID, USBD_PID, DAP_BCD_DEVICE, 0x01),
};

static const uint8_t dap_config_descriptor[] = {
    USB_CONFIG_DESCRIPTOR_INIT(DAP_CONFIG_SIZE, DAP_INTF_NUM, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    USB_INTERFACE_DESCRIPTOR_INIT(DAP_INTF, 0x00, 0x02, 0xFF, 0x00, 0x00, STRID_DAP),
    USB_ENDPOINT_DESCRIPTOR_INIT(DAP_OUT_EP, USB_ENDPOINT_TYPE_BULK, HS_PACKET_SIZE, 0x00),
    USB_ENDPOINT_DESCRIPTOR_INIT(DAP_IN_EP, USB_ENDPOINT_TYPE_BULK, HS_PACKET_SIZE, 0x00),
    CDC_ACM_DESCRIPTOR_INIT(CDC0_INTF, CDC0_INT_EP, CDC0_OUT_EP, CDC0_IN_EP, HS_PACKET_SIZE, STRID_CDC0),
    CDC_ACM_DESCRIPTOR_INIT(CDC1_INTF, CDC1_INT_EP, CDC1_OUT_EP, CDC1_IN_EP, HS_PACKET_SIZE, STRID_SLCAN),
};

static const uint8_t dap_other_speed_config_descriptor[] = {
    USB_CONFIG_DESCRIPTOR_INIT(DAP_CONFIG_SIZE, DAP_INTF_NUM, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    USB_INTERFACE_DESCRIPTOR_INIT(DAP_INTF, 0x00, 0x02, 0xFF, 0x00, 0x00, STRID_DAP),
    USB_ENDPOINT_DESCRIPTOR_INIT(DAP_OUT_EP, USB_ENDPOINT_TYPE_BULK, FS_PACKET_SIZE, 0x00),
    USB_ENDPOINT_DESCRIPTOR_INIT(DAP_IN_EP, USB_ENDPOINT_TYPE_BULK, FS_PACKET_SIZE, 0x00),
    CDC_ACM_DESCRIPTOR_INIT(CDC0_INTF, CDC0_INT_EP, CDC0_OUT_EP, CDC0_IN_EP, FS_PACKET_SIZE, STRID_CDC0),
    CDC_ACM_DESCRIPTOR_INIT(CDC1_INTF, CDC1_INT_EP, CDC1_OUT_EP, CDC1_IN_EP, FS_PACKET_SIZE, STRID_SLCAN),
};

_Static_assert(DAP_CONFIG_SIZE == sizeof(dap_config_descriptor), "cmsis-dap usb config size mismatch");
_Static_assert(DAP_CONFIG_SIZE == sizeof(dap_other_speed_config_descriptor), "cmsis-dap usb other speed config size mismatch");

#define GDB_CONFIG_SIZE (9 + BULK_DESCRIPTOR_LEN + 2 * CDC_ACM_DESCRIPTOR_LEN)
#define GDB_INTF_NUM    (1 + 2 * 2)
#define GDB_BCD_DEVICE  0x101

static uint8_t gdb_device_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_1, 0xEF, 0x02, 0x01, USBD_VID, USBD_PID, GDB_BCD_DEVICE, 0x01),
};

static const uint8_t gdb_config_descriptor[] = {
    USB_CONFIG_DESCRIPTOR_INIT(GDB_CONFIG_SIZE, GDB_INTF_NUM, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    USB_INTERFACE_DESCRIPTOR_INIT(CANBUS_INTF, 0x00, 0x02, 0xFF, 0x00, 0x00, STRID_CANBUS),
    USB_ENDPOINT_DESCRIPTOR_INIT(CANBUS_OUT_EP, USB_ENDPOINT_TYPE_BULK, HS_PACKET_SIZE, 0x00),
    USB_ENDPOINT_DESCRIPTOR_INIT(CANBUS_IN_EP, USB_ENDPOINT_TYPE_BULK, HS_PACKET_SIZE, 0x00),
    CDC_ACM_DESCRIPTOR_INIT(CDC0_INTF, CDC0_INT_EP, CDC0_OUT_EP, CDC0_IN_EP, HS_PACKET_SIZE, STRID_CDC0),
    CDC_ACM_DESCRIPTOR_INIT(CDC1_INTF, CDC1_INT_EP, CDC1_OUT_EP, CDC1_IN_EP, HS_PACKET_SIZE, STRID_CDC1),
};

static const uint8_t gdb_other_speed_config_descriptor[] = {
    USB_CONFIG_DESCRIPTOR_INIT(GDB_CONFIG_SIZE, GDB_INTF_NUM, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    USB_INTERFACE_DESCRIPTOR_INIT(CANBUS_INTF, 0x00, 0x02, 0xFF, 0x00, 0x00, STRID_CANBUS),
    USB_ENDPOINT_DESCRIPTOR_INIT(CANBUS_OUT_EP, USB_ENDPOINT_TYPE_BULK, FS_PACKET_SIZE, 0x00),
    USB_ENDPOINT_DESCRIPTOR_INIT(CANBUS_IN_EP, USB_ENDPOINT_TYPE_BULK, FS_PACKET_SIZE, 0x00),
    CDC_ACM_DESCRIPTOR_INIT(CDC0_INTF, CDC0_INT_EP, CDC0_OUT_EP, CDC0_IN_EP, FS_PACKET_SIZE, STRID_CDC0),
    CDC_ACM_DESCRIPTOR_INIT(CDC1_INTF, CDC1_INT_EP, CDC1_OUT_EP, CDC1_IN_EP, FS_PACKET_SIZE, STRID_CDC1),
};

_Static_assert(GDB_CONFIG_SIZE == sizeof(gdb_config_descriptor), "gdb server usb config size mismatch");
_Static_assert(GDB_CONFIG_SIZE == sizeof(gdb_other_speed_config_descriptor), "gdb server usb other speed config size mismatch");

#define MSC_CONFIG_SIZE (9 + MSC_DESCRIPTOR_LEN + CDC_ACM_DESCRIPTOR_LEN)
#define MSC_INTF_NUM    (1 + 2)
#define MSC_BCD_DEVICE  0x102

static uint8_t msc_device_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_1, 0xEF, 0x02, 0x01, USBD_VID, USBD_PID, MSC_BCD_DEVICE, 0x01),
};

static const uint8_t msc_config_descriptor[] = {
    USB_CONFIG_DESCRIPTOR_INIT(MSC_CONFIG_SIZE, MSC_INTF_NUM, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    MSC_DESCRIPTOR_INIT(MSC_INTF, MSC_OUT_EP, MSC_IN_EP, HS_PACKET_SIZE, STRID_MSC),
    CDC_ACM_DESCRIPTOR_INIT(CDC0_INTF, CDC0_INT_EP, CDC0_OUT_EP, CDC0_IN_EP, HS_PACKET_SIZE, STRID_CONSOLE),
};

static const uint8_t msc_other_speed_config_descriptor[] = {
    USB_CONFIG_DESCRIPTOR_INIT(MSC_CONFIG_SIZE, MSC_INTF_NUM, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    MSC_DESCRIPTOR_INIT(MSC_INTF, MSC_OUT_EP, MSC_IN_EP, FS_PACKET_SIZE, STRID_MSC),
    CDC_ACM_DESCRIPTOR_INIT(CDC0_INTF, CDC0_INT_EP, CDC0_OUT_EP, CDC0_IN_EP, FS_PACKET_SIZE, STRID_CONSOLE),
};

_Static_assert(MSC_CONFIG_SIZE == sizeof(msc_config_descriptor), "mass storage usb config size mismatch");
_Static_assert(MSC_CONFIG_SIZE == sizeof(msc_other_speed_config_descriptor), "mass storage usb other speed config size mismatch");

#define SCRIPT_CONFIG_SIZE (9 + 2 * CDC_ACM_DESCRIPTOR_LEN)
#define SCRIPT_INTF_NUM    (2 * 2)
#define SCRIPT_BCD_DEVICE  0x103

static uint8_t script_device_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_1, 0xEF, 0x02, 0x01, USBD_VID, USBD_PID, SCRIPT_BCD_DEVICE, 0x01),
};

static const uint8_t script_config_descriptor[] = {
    USB_CONFIG_DESCRIPTOR_INIT(SCRIPT_CONFIG_SIZE, SCRIPT_INTF_NUM, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    CDC_ACM_DESCRIPTOR_INIT(CDC0_INTF, CDC0_INT_EP, CDC0_OUT_EP, CDC0_IN_EP, HS_PACKET_SIZE, STRID_USER),
    CDC_ACM_DESCRIPTOR_INIT(CDC1_INTF, CDC1_INT_EP, CDC1_OUT_EP, CDC1_IN_EP, HS_PACKET_SIZE, STRID_SCRIPT),
};

static const uint8_t script_other_speed_config_descriptor[] = {
    USB_CONFIG_DESCRIPTOR_INIT(SCRIPT_CONFIG_SIZE, SCRIPT_INTF_NUM, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    CDC_ACM_DESCRIPTOR_INIT(CDC0_INTF, CDC0_INT_EP, CDC0_OUT_EP, CDC0_IN_EP, FS_PACKET_SIZE, STRID_USER),
    CDC_ACM_DESCRIPTOR_INIT(CDC1_INTF, CDC1_INT_EP, CDC1_OUT_EP, CDC1_IN_EP, FS_PACKET_SIZE, STRID_SCRIPT),
};

_Static_assert(SCRIPT_CONFIG_SIZE == sizeof(script_config_descriptor), "script usb config size mismatch");
_Static_assert(SCRIPT_CONFIG_SIZE == sizeof(script_other_speed_config_descriptor), "script usb other speed config size mismatch");

static const uint8_t device_quality_descriptor[] = {
    USB_DEVICE_QUALIFIER_DESCRIPTOR_INIT(USB_2_1, 0x00, 0x00, 0x00, 0x01),
};

static const uint8_t *device_descriptor_callback(uint8_t speed)
{
    (void)speed;
    switch (usb_mode)
    {
    case MODE_CMSIS_DAP:
        return dap_device_descriptor;
        break;
    case MODE_GDB_SERVER:
        return gdb_device_descriptor;
        break;
    case MODE_MASS_STORAGE:
        return msc_device_descriptor;
        break;
    case MODE_SCRIPT:
        return script_device_descriptor;
        break;
    default:
        LOG_E("unknown usb mode %d", usb_mode);
        break;
    }
    return msc_device_descriptor;
}

static const uint8_t *config_descriptor_callback(uint8_t speed)
{
    (void)speed;
    switch (usb_mode)
    {
    case MODE_CMSIS_DAP:
        return dap_config_descriptor;
        break;
    case MODE_GDB_SERVER:
        return gdb_config_descriptor;
        break;
    case MODE_MASS_STORAGE:
        return msc_config_descriptor;
        break;
    case MODE_SCRIPT:
        return script_config_descriptor;
        break;
    default:
        LOG_E("unknown usb mode %d", usb_mode);
        break;
    }
    return msc_config_descriptor;
}

static const uint8_t *other_speed_config_descriptor_callback(uint8_t speed)
{
    (void)speed;
    switch (usb_mode)
    {
    case MODE_CMSIS_DAP:
        return dap_other_speed_config_descriptor;
        break;
    case MODE_GDB_SERVER:
        return gdb_other_speed_config_descriptor;
        break;
    case MODE_MASS_STORAGE:
        return msc_other_speed_config_descriptor;
        break;
    case MODE_SCRIPT:
        return script_other_speed_config_descriptor;
        break;
    default:
        LOG_E("unknown usb mode %d", usb_mode);
        break;
    }
    return msc_other_speed_config_descriptor;
}

static const uint8_t *device_quality_descriptor_callback(uint8_t speed)
{
    (void)speed;
    return device_quality_descriptor;
}

static const char *string_descriptor_callback(uint8_t speed, uint8_t index)
{
    (void)speed;

    if (index >= (sizeof(string_descriptors) / sizeof(char *)))
    {
        return NULL;
    }
    return string_descriptors[index];
}

static const struct usb_descriptor usb_desc = {
    .device_descriptor_callback         = device_descriptor_callback,
    .config_descriptor_callback         = config_descriptor_callback,
    .device_quality_descriptor_callback = device_quality_descriptor_callback,
    .string_descriptor_callback         = string_descriptor_callback,
    .msosv2_descriptor                  = &msosv2_desc,
    .bos_descriptor                     = &bos_desc,
};

static void usbd_event_handler(uint8_t busid, uint8_t event)
{
    usb_busid = busid;

    if (usb_event == NULL)
    {
        LOG_E("NULL usb_event");
        return;
    }

    switch (event)
    {
    case USBD_EVENT_RESET:
        rt_event_send(usb_event, EVENT_USB_RESET);
        break;
    case USBD_EVENT_CONNECTED:
        rt_event_send(usb_event, EVENT_USB_CONNECTED);
        break;
    case USBD_EVENT_DISCONNECTED:
        rt_event_send(usb_event, EVENT_USB_DISCONNECTED);
        break;
    case USBD_EVENT_RESUME:
        break;
    case USBD_EVENT_SUSPEND:
        break;
    case USBD_EVENT_CONFIGURED:
        rt_event_send(usb_event, EVENT_USB_CONFIGURED);
        break;
    case USBD_EVENT_SET_REMOTE_WAKEUP:
        break;
    case USBD_EVENT_CLR_REMOTE_WAKEUP:
        break;
    default:
        break;
    }
}

/*!< endpoint call back */

static struct usbd_endpoint canbus_out_ep = {
    .ep_addr = CANBUS_OUT_EP,
    .ep_cb   = gsusb_bulk_out_callback};

static struct usbd_endpoint canbus_in_ep = {
    .ep_addr = CANBUS_IN_EP,
    .ep_cb   = gsusb_bulk_in_callback};

static struct usbd_interface canbus_intf = {
    .class_interface_handler   = NULL,
    .class_endpoint_handler    = NULL,
    .vendor_handler            = gsusb_control_request_handler,
    .notify_handler            = NULL,
    .hid_report_descriptor     = NULL,
    .hid_report_descriptor_len = 0,
    .intf_num                  = CANBUS_INTF};

static struct usbd_endpoint dap_out_ep = {
    .ep_addr = DAP_OUT_EP,
    .ep_cb   = dap_bulk_out_callback};

static struct usbd_endpoint dap_in_ep = {
    .ep_addr = DAP_IN_EP,
    .ep_cb   = dap_bulk_in_callback};

static struct usbd_interface dap_intf;

static struct usbd_interface msc_intf;

static void dap_usb_init(uint8_t busid, uintptr_t reg_base)
{
    const char product_name_dap[] = "arm can tool (CMSIS-DAP/slcan)";

    _Static_assert(sizeof(product_name_dap) < sizeof(product_name), "dap product name too long");

    strncpy(product_name, product_name_dap, sizeof(product_name) - 1);
    product_name[sizeof(product_name) - 1] = '\0';

    usbd_desc_register(busid, &usb_desc);

    /* bulk for dap */
    usbd_add_interface(busid, &dap_intf);
    usbd_add_endpoint(busid, &dap_out_ep);
    usbd_add_endpoint(busid, &dap_in_ep);

    /* cdc0 in usb_cdc0.h */
    cdc0_init(busid);

    /* cdc1 maps to rt-thread character device usb-acm1 */
    usbd_cdc_acm_serial_init(busid, CDC1_IN_EP, CDC1_OUT_EP);

    usbd_initialize(busid, reg_base, usbd_event_handler);
}

static void gdb_usb_init(uint8_t busid, uintptr_t reg_base)
{
    const char product_name_gdb[] = "arm can tool (gdb server/gsusb)";

    _Static_assert(sizeof(product_name_gdb) < sizeof(product_name), "gdb product name too long");

    strncpy(product_name, product_name_gdb, sizeof(product_name) - 1);
    product_name[sizeof(product_name) - 1] = '\0';

    usbd_desc_register(busid, &usb_desc);

    /* bulk for canbus gsusb */
    usbd_add_interface(busid, &canbus_intf);
    usbd_add_endpoint(busid, &canbus_out_ep);
    usbd_add_endpoint(busid, &canbus_in_ep);

    /* cdc0 in usb_cdc0.h */
    cdc0_init(busid);

    /* cdc1 maps to rt-thread character device usb-acm1 */
    usbd_cdc_acm_serial_init(busid, CDC1_IN_EP, CDC1_OUT_EP);

    usbd_initialize(busid, reg_base, usbd_event_handler);
}

static void msc_usb_init(uint8_t busid, uintptr_t reg_base)
{
    const char product_name_msc[] = "arm can tool (mass storage)";

    _Static_assert(sizeof(product_name_msc) < sizeof(product_name), "msc product name too long");

    strncpy(product_name, product_name_msc, sizeof(product_name) - 1);
    product_name[sizeof(product_name) - 1] = '\0';

    usbd_desc_register(busid, &usb_desc);

    /* mass storage for sdcard */
    usbd_add_interface(busid, usbd_msc_init_intf(busid, &msc_intf, MSC_OUT_EP, MSC_IN_EP));

    /* cdc0 maps to rt-thread character device usb-acm0 */
    usbd_cdc_acm_serial_init(busid, CDC0_IN_EP, CDC0_OUT_EP);

    usbd_initialize(busid, reg_base, usbd_event_handler);
}

static void script_usb_init(uint8_t busid, uintptr_t reg_base)
{
    const char product_name_script[] = "arm can tool (script)";

    _Static_assert(sizeof(product_name_script) < sizeof(product_name), "script product name too long");

    strncpy(product_name, product_name_script, sizeof(product_name) - 1);
    product_name[sizeof(product_name) - 1] = '\0';

    usbd_desc_register(busid, &usb_desc);

    /* cdc0 in usb_cdc0.h */
    cdc0_init(busid);

    /* cdc1 maps to rt-thread character device usb-acm1 */
    usbd_cdc_acm_serial_init(busid, CDC1_IN_EP, CDC1_OUT_EP);

    usbd_initialize(busid, reg_base, usbd_event_handler);
}

/*
   thread to call event handlers.
   this takes event handling out of interrupt context.
   used only for configuration events now,
   but easily extended for other events.
 */

static void usb_event_thread(void *arg)
{
    rt_uint32_t recv_set;

    if (!usb_event)
    {
        LOG_E("no usb event");
        return;
    }

    while (1)
    {
        /* Wait for USB event */
        if (rt_event_recv(usb_event,
                          EVENT_USB_RESET
                              | EVENT_USB_CONNECTED
                              | EVENT_USB_DISCONNECTED
                              | EVENT_USB_CONFIGURED,
                          RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR,
                          RT_WAITING_FOREVER,
                          &recv_set)
            == RT_EOK)
        {
            /* Process USB event */
            if (recv_set & EVENT_USB_RESET)
            {
                LOG_I("usb reset");
            }
            if (recv_set & EVENT_USB_CONNECTED)
            {
                LOG_I("usb connected");
            }
            if (recv_set & EVENT_USB_DISCONNECTED)
            {
                LOG_I("usb disconnected");
            }
            if (recv_set & EVENT_USB_CONFIGURED)
            {
                LOG_I("usb configured");
                switch (usb_mode)
                {
                case MODE_CMSIS_DAP:
                    dap_on_configured(usb_busid);
                    cdc0_on_configured(usb_busid);
                    slcan_on_configured(usb_busid);
                    LOG_I("dap configured");
                    break;
                case MODE_GDB_SERVER:
                    gsusb_on_configured(usb_busid);
                    cdc0_on_configured(usb_busid);
                    gdb_on_configured(usb_busid);
                    LOG_I("gdb server configured");
                    break;
                case MODE_MASS_STORAGE:
                    msc_on_configured(usb_busid);
                    LOG_I("msc configured");
                    break;
                case MODE_SCRIPT:
                    cdc0_on_configured(usb_busid);
                    script_on_configured(usb_busid);
                    LOG_I("script configured");
                    break;
                default:
                    break;
                }
            }
        }
    }
}

void usbd_cdc_acm_set_dtr(uint8_t busid, uint8_t intf, bool dtr)
{
    if (busid != usb_busid)
        return;

    switch (usb_mode)
    {
    case MODE_CMSIS_DAP:
    case MODE_GDB_SERVER:
        serial_set_dtr(intf, dtr);
        break;
    case MODE_MASS_STORAGE:
        msc_set_dtr(intf, dtr);
        break;
    case MODE_SCRIPT:
        serial_set_dtr(intf, dtr);
        break;
    default:
        break;
    }

    return;
}

void usb_composite_init(uint8_t busid, uintptr_t reg_base, usb_mode_t mode)
{
    /* usb mode remains the same after boot */
    usb_mode = mode;

    usb_event = rt_event_create("usb", RT_IPC_FLAG_FIFO);
    if (!usb_event)
    {
        LOG_E("failed to create usb event");
        return;
    }

    rt_thread_t thread = rt_thread_create("usb_event",
                                          usb_event_thread,
                                          NULL,
                                          1024,
                                          15,
                                          10);
    if (!thread)
    {
        LOG_E("failed to create usb event thread");
        return;
    }
    rt_thread_startup(thread);

    switch (usb_mode)
    {
    case MODE_CMSIS_DAP:
        dap_usb_init(busid, reg_base);
        break;
    case MODE_GDB_SERVER:
        gdb_usb_init(busid, reg_base);
        break;
    case MODE_MASS_STORAGE:
        msc_usb_init(busid, reg_base);
        break;
    case MODE_SCRIPT:
        script_usb_init(busid, reg_base);
        break;
    default:
        LOG_E("unknown usb mode %d", usb_mode);
        usb_mode = MODE_MASS_STORAGE;
        msc_usb_init(busid, reg_base);
        break;
    }
    return;
}
