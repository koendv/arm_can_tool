#ifndef _AT24C256_H
#define _AT24C256_H

/*
 * rt-thread driver for AT24C256C 256 kbit eeprom
 * used to save settings struct
 */

/* at24c64 */
#define EEPROM_PAGE_SIZE   32
#define EEPROM_MEMORY_SIZE 8192

int32_t at24_write(uint32_t data_address, const uint8_t *data, uint32_t data_size);
int32_t at24_read(uint32_t data_address, uint8_t *data, uint32_t data_size);

#endif


