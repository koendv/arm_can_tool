#ifndef LUACONF_PLATFORM_H
#define LUACONF_PLATFORM_H

#include "serials.h"

#define LUA_32BITS
#define LUA_NOCVTN2S

#define LUA_PATH_DEFAULT      \
    "/sdcard/lua/?.lua;"      \
    "/sdcard/lua/?/init.lua;" \
    "/flash/lua/?.lua;"       \
    "/flash/lua/?/init.lua"

#define LUA_CPATH_DEFAULT ""

/* print a string */
#define lua_writestring(s, l) cdc1_write((s), (l))

/* print a newline and flush the output */
#define lua_writeline() cdc1_write("\r\n", 2)

/* print an error message */
#define lua_writestringerror(s, p) cdc1_printf((s), (p))

#endif
