#ifndef USB_CDC0_H
#define USB_CDC0_H

#include <stdint.h>
#include <stddef.h>

/* data from host */
extern uint8_t  cdc0_out_buf[];
extern uint32_t cdc0_out_len;

void cdc0_init(uint8_t busid);
void cdc0_on_configured(uint8_t busid);
void cdc0_write(const char *buf, const size_t len);
void cdc0_flush(void);
void cdc0_start_read(void);

#endif /* USB_CDC0_H */
