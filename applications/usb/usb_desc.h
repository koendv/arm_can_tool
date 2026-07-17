#ifndef USB_DESC_H
#define USB_DESC_H

#include <rtthread.h>
#include <stdbool.h>
#include <usbd_core.h>

#define CONFIG_USB_HS 1

/*!< usb packet size */
#define HS_PACKET_SIZE (512)
#define FS_PACKET_SIZE (64)

/*!< usb bus number */
#define BUSID0 0

/*!< endpoint address */
#define CANBUS_IN_EP  0x81
#define CANBUS_OUT_EP 0x01
#define DAP_IN_EP     0x81
#define DAP_OUT_EP    0x01
#define MSC_IN_EP     0x81
#define MSC_OUT_EP    0x01
#define CDC0_IN_EP    0x82
#define CDC0_OUT_EP   0x02
#define CDC0_INT_EP   0x83
#define CDC1_IN_EP    0x84
#define CDC1_OUT_EP   0x04
#define CDC1_INT_EP   0x85

/*!< interface number */
#define CANBUS_INTF 0x00
#define DAP_INTF    0x00
#define MSC_INTF    0x00
#define CDC0_INTF   0x01
#define CDC1_INTF   0x03

/*!< usb operating mode */
typedef enum
{
    MODE_CMSIS_DAP = 0,
    MODE_GDB_SERVER,
    MODE_MASS_STORAGE,
    MODE_SCRIPT,
} usb_mode_t;

/*!< string descriptors */
enum
{
    STRID_LANG = 0,
    STRID_MANUFACTURER,
    STRID_PRODUCT,
    STRID_SERIAL,
    STRID_WEBUSB,
    STRID_CANBUS,
    STRID_SLCAN,
    STRID_CDC0,
    STRID_CDC1,
    STRID_DAP,
    STRID_MSC,
    STRID_CONSOLE,
    STRID_USER,
    STRID_SCRIPT,
};

void usb_composite_init(uint8_t busid, uintptr_t reg_base, usb_mode_t mode);

/*!< External callbacks */

/* gsusb */
void gsusb_bulk_out_callback(uint8_t busid, uint8_t ep, uint32_t nbytes);
void gsusb_bulk_in_callback(uint8_t busid, uint8_t ep, uint32_t nbytes);
int  gsusb_control_request_handler(uint8_t                  busid,
                                   struct usb_setup_packet *setup,
                                   uint8_t                **data,
                                   uint32_t                *len);

void dap_on_configured(uint8_t busid);
void dap_bulk_out_callback(uint8_t busid, uint8_t ep, uint32_t nbytes);
void dap_bulk_in_callback(uint8_t busid, uint8_t ep, uint32_t nbytes);

void slcan_on_configured(uint8_t busid);

void gdb_on_configured(uint8_t busid);

void msc_on_configured(uint8_t busid);

void script_on_configured(uint8_t busid);

void msc_set_dtr(uint8_t intf, bool dtr);

#endif
