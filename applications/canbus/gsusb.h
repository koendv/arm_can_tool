/*
 * GS_USB USB Class Implementation
 * CherryUSB interface for GS_USB protocol
 */

#ifndef USB_GSUSB_H
#define USB_GSUSB_H

#include <stdint.h>
#include <stdbool.h>
#include "usbd_core.h"
#include "canbus.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief GS_USB configured callback
 */
void gsusb_configured(uint8_t busid);

/**
 * @brief GS_USB reset callback  
 */
void gsusb_reset(void);

/**
 * @brief GS_USB bulk OUT endpoint callback (host → device)
 */
void gsusb_out_callback(uint8_t busid, uint8_t ep, uint32_t nbytes);

/**
 * @brief GS_USB bulk IN endpoint callback (device → host)
 */
void gsusb_in_callback(uint8_t busid, uint8_t ep, uint32_t nbytes);

/**
 * @brief GS_USB control request handler (vendor handler)
 * Direct port from cannectivity's gs_usb_vendor_request_handler
 */
int gsusb_control_request_handler(uint8_t                  busid,
                                  struct usb_setup_packet *setup,
                                  uint8_t                **data,
                                  uint32_t                *len);

#ifdef __cplusplus
}
#endif

#endif /* USB_GSUSB_H */

