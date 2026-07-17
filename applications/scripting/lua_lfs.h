#ifndef LUA_FLASH_LOADER_H
#define LUA_FLASH_LOADER_H

/* lfs - lua flash store */

#include <stdbool.h>
#include <stdint.h>

/* Set true by flash.receive(), cleared by flash_receive() on last packet
** or error. Checked by lua_task() to route input. */
extern bool lfs_receiving;

/* allocate memory for protocol */
bool lfs_receive_init();

/* Feed one byte from CDC1 into the HDLC flash loader state machine. */
void lfs_receive(uint8_t c);

#endif /* LUA_FLASH_LOADER_H */
