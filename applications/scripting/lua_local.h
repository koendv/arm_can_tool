#ifndef LUA_LOCAL_H
#define LUA_LOCAL_H

#include "luaconf.h"

#ifndef LUA_MINSTACK
#error "LUA_MINSTACK not defined - check include order"
#endif

/* lua */
#define LUA_STACK_SIZE  (8 * 1024)
#define LUA_HEAP_SIZE   (28 * 1024)
#define LUA_PRIORITY    20
#define LUA_TICK        20
#define LUA_INIT_SCRIPT "/sdcard/init.lua"

/*
 * limit nested Lua/c calls to bound worst-case c-stack use.
 * worst case is MAXCCALLS nested calls each claiming LUA_MINSTACK slots.
 * RT-Thread lua_task stack is LUA_STACK_SIZE
 */

#define LUAI_MAXCCALLS 20
#define LUAI_MAXSTACK  (LUAI_MAXCCALLS * LUA_MINSTACK + 100)

int   lua_heap_init(void);
void *lua_alloc(void *ud, void *ptr, size_t osize, size_t nsize);
void  lua_memheap_info(rt_size_t *total, rt_size_t *used, rt_size_t *max_used);

/* rotable versions of standard libraries */
LUAMOD_API int luaopen_base_rotable(lua_State *L);
LUAMOD_API int luaopen_math_rotable(lua_State *L);
LUAMOD_API int luaopen_string_rotable(lua_State *L);
LUAMOD_API int luaopen_table_rotable(lua_State *L);
LUAMOD_API int luaopen_coroutine_rotable(lua_State *L);
LUAMOD_API int luaopen_utf8_rotable(lua_State *L);

/* lua base library print */
int lua_print(lua_State *L);

/* local libraries */
int luaopen_bmd_rotable(lua_State *L);
int luaopen_dap_rotable(lua_State *L);
int luaopen_can_rotable(lua_State *L);
int luaopen_sys_rotable(lua_State *L);
int luaopen_flash_rotable(lua_State *L);

#endif
