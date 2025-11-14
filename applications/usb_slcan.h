#ifndef USB_SLCAN_H
#define USB_SLCAN_H

#include "usb_cdc.h"
#include <string.h>

/* called each time a usb cdc packet is received */
void slcan_process(uint8_t *buf, uint32_t len);

/* send slcan reply to usb */
static void inline slcan_send_reply(uint8_t *buf, uint32_t len)
{
    cdc1_write(buf, len);
}

#endif
