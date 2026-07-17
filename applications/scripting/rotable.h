#ifndef ROTABLE_H_
#define ROTABLE_H_

#include "lua.h"

/*
** rotable — read-only library table for Lua 5.5 on RT-Thread.
**
** A rotable wraps a const rotable_Reg[] array (flash/ROM) as a Lua
** userdata disguised as a table via a shared metatable.  Only a small
** userdata header (sizeof(rotable_ud) bytes) is allocated in the Lua
** heap, regardless of how many entries the library has.
**
** Each entry is either a CFunction (func != NULL) or an integer
** constant (func == NULL, value field used).
**
** Usage:
**
**   static const rotable_Reg mylib[] = {
**       { "foo", my_foo, 0  },   // CFunction entry
**       { "BAR", NULL,   42 },   // integer constant
**       { NULL,  NULL,   0  }    // sentinel
**   };
**
**   rotable_newlib(L, mylib);    // push rotable onto stack
**   lua_setglobal(L, "mylib");
**
** For use as __index closure on a custom userdata metatable:
**   rotable_newidx(L, mylib);    // push C closure onto stack
**   lua_setfield(L, meta, "__index");
**
** Tab-completion support (lua_completion.c):
**   rotable_isrotable(L, idx)
**   rotable_entries(L, idx)      // returns pointer to flash array; caller walks with entry++
*/

typedef struct rotable_Reg {
    const char    *name;
    lua_CFunction  func;
    lua_Integer    value;
} rotable_Reg;

#ifndef ROTABLE_EXPORT
#  define ROTABLE_EXPORT extern
#endif

/* Push a new rotable onto the stack. */
ROTABLE_EXPORT void rotable_newlib(lua_State *L, const rotable_Reg *reg);

/* Push a C closure for use as __index on a custom userdata metatable. */
ROTABLE_EXPORT void rotable_newidx(lua_State *L, const rotable_Reg *reg);

/* Returns 1 if the value at idx is a rotable, 0 otherwise. */
ROTABLE_EXPORT int rotable_isrotable(lua_State *L, int idx);

/*
** Return the underlying flash array for tab-completion iteration.
** Returns NULL if idx is not a rotable.
** Caller walks: for (e = rotable_entries(L, idx); e && e->name; e++) ...
*/
ROTABLE_EXPORT const rotable_Reg *rotable_entries(lua_State *L, int idx);

#endif /* ROTABLE_H_ */
