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

/* transmission counters */
extern uint32_t transmit_count;  /* hardware confirms transmission success */
extern uint32_t transmit_errors; /* hardware confirms transmission fail */

/* gsusb_timestamp is true if gsusb packets from adapter to host need timestamp */
extern bool gsusb_timestamp;

/**
 * @brief GS_USB configured callback
 */
void gsusb_on_configured(uint8_t busid);

/**
 * @brief GS_USB bulk OUT endpoint callback (host to device)
 */
void gsusb_bulk_out_callback(uint8_t busid, uint8_t ep, uint32_t nbytes);

/**
 * @brief GS_USB bulk IN endpoint callback (device to host)
 */
void gsusb_bulk_in_callback(uint8_t busid, uint8_t ep, uint32_t nbytes);

/**
 * @brief GS_USB control request handler (vendor handler)
 * Direct port from cannectivity's gs_usb_vendor_request_handler
 */
int gsusb_control_request_handler(uint8_t                  busid,
                                  struct usb_setup_packet *setup,
                                  uint8_t                **data,
                                  uint32_t                *len);

/* @brief stop gsusb usb bulk */
void gsusb_stop(void);

/* @brief start gsusb usb bulk */
void gsusb_start(void);

#ifdef __cplusplus
}
#endif

#endif /* USB_GSUSB_H */

