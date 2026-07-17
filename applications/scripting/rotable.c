/*
** rotable.c — read-only library table for Lua 5.5 / RT-Thread.
**
** Ported from LuatOS (Lua 5.3) to Lua 5.5:
**   - lua_newuserdatauv(L, sz, 0): nuvalue=0, reg pointer stored in struct
**   - No lua_setuservalue/lua_getuservalue needed
**   - No luaC_fix: libraries anchored as globals via lua_setglobal
**   - All LUA_VERSION_NUM < 503 branches removed
**
** Tab-completion support (lua_completion.c):
**   - rotable_isrotable()
**   - rotable_entries()
*/

#include <stddef.h>
#include <string.h>
#include "lua.h"
#include "rotable.h"


/* -----------------------------------------------------------------------
** Internal types
** --------------------------------------------------------------------- */

/*
** Userdata payload.  Stores the reg pointer directly so nuvalue=0 —
** no Lua-managed user value slot needed, saving one TValue per instance.
*/
typedef struct {
    const rotable_Reg *p;
} rotable_ud;

/*
** Unique address used as registry key for the shared metatable.
** Its address is the key; we never dereference it.
*/
static const char unique_address = 0;


/* -----------------------------------------------------------------------
** Key lookup
** --------------------------------------------------------------------- */

static const rotable_Reg *find_key(const rotable_Reg *p, const char *s)
{
    if (s) {
        for (; p->name != NULL; ++p) {
            if (strcmp(s, p->name) == 0)
                return p;
        }
    }
    return NULL;
}

static void push_entry(lua_State *L, const rotable_Reg *p)
{
    if (p->func)
        lua_pushcfunction(L, p->func);
    else
        lua_pushinteger(L, p->value);
}


/* -----------------------------------------------------------------------
** Metatable validation
** --------------------------------------------------------------------- */

/*
** Returns the userdata pointer if idx carries our shared metatable,
** NULL otherwise.  Does not raise an error.
*/
static rotable_ud *test_rotable(lua_State *L, int idx)
{
    rotable_ud *t = (rotable_ud *)lua_touserdata(L, idx);
    if (!t)
        return NULL;
    if (!lua_getmetatable(L, idx))
        return NULL;
    lua_pushlightuserdata(L, (void *)&unique_address);
    lua_rawget(L, LUA_REGISTRYINDEX);
    int eq = lua_rawequal(L, -1, -2);
    lua_pop(L, 2);
    return eq ? t : NULL;
}

/*
** Like test_rotable but raises a Lua error on failure.
*/
static rotable_ud *check_rotable(lua_State *L, int idx, const char *func)
{
    rotable_ud *t = test_rotable(L, idx);
    if (!t) {
        const char *type = lua_typename(L, lua_type(L, idx));
        if (lua_type(L, idx) == LUA_TLIGHTUSERDATA) {
            type = "light userdata";
        } else if (lua_getmetatable(L, idx)) {
            lua_getfield(L, -1, "__name");
            lua_replace(L, -2);
            if (lua_type(L, -1) == LUA_TSTRING)
                type = lua_tostring(L, -1);
        }
        lua_pushfstring(L, "bad argument #%d to '%s' "
                        "(rotable expected, got %s)", idx, func, type);
        lua_error(L);
    }
    return t;
}


/* -----------------------------------------------------------------------
** Metamethods
** --------------------------------------------------------------------- */

static int rotable_udata_index(lua_State *L)
{
    rotable_ud     *t  = (rotable_ud *)lua_touserdata(L, 1);
    const char     *s  = lua_tostring(L, 2);
    const rotable_Reg *p  = t->p;
    const rotable_Reg *p2 = p;

    p = find_key(p, s);
    if (p) {
        push_entry(L, p);
    } else {
        /* If first entry is "__index" function, use it as fallback. */
        if (p2->name && strcmp(p2->name, "__index") == 0 && p2->func) {
            lua_pushcfunction(L, p2->func);
            lua_pushvalue(L, 2);
            lua_call(L, 1, 1);
            return 1;
        }
        lua_pushnil(L);
    }
    return 1;
}

static int rotable_udata_len(lua_State *L)
{
    (void)L;
    lua_pushinteger(L, 0);
    return 1;
}

static int rotable_iter(lua_State *L)
{
    rotable_ud        *t   = check_rotable(L, 1, "__pairs iterator");
    const char        *key = lua_tostring(L, 2);  /* nil on first call */
    const rotable_Reg *p   = t->p;
    const rotable_Reg *q;

    if (!key) {
        q = p;  /* first call */
    } else {
        for (q = p; q->name != NULL; ++q) {
            if (strcmp(key, q->name) == 0) {
                ++q;
                break;
            }
        }
    }

    if (q == NULL || q->name == NULL)
        return 0;

    lua_pushstring(L, q->name);
    push_entry(L, q);
    return 2;
}

static int rotable_udata_pairs(lua_State *L)
{
    lua_pushcfunction(L, rotable_iter);
    lua_pushvalue(L, 1);
    lua_pushnil(L);
    return 3;
}


/* -----------------------------------------------------------------------
** Shared metatable
** --------------------------------------------------------------------- */

static void push_shared_mt(lua_State *L)
{
    lua_pushlightuserdata(L, (void *)&unique_address);
    lua_rawget(L, LUA_REGISTRYINDEX);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_createtable(L, 0, 5);
        lua_pushcfunction(L, rotable_udata_index);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, rotable_udata_len);
        lua_setfield(L, -2, "__len");
        lua_pushcfunction(L, rotable_udata_pairs);
        lua_setfield(L, -2, "__pairs");
        lua_pushboolean(L, 0);
        lua_setfield(L, -2, "__metatable");  /* protect metatable */
        lua_pushliteral(L, "rotable");
        lua_setfield(L, -2, "__name");
        /* cache in registry */
        lua_pushlightuserdata(L, (void *)&unique_address);
        lua_pushvalue(L, -2);
        lua_rawset(L, LUA_REGISTRYINDEX);
    }
}


/* -----------------------------------------------------------------------
** __index closure for rotable_newidx
** upvalue 1: lightuserdata -> rotable_Reg array
** --------------------------------------------------------------------- */

static int rotable_func_index(lua_State *L)
{
    const char        *s  = lua_tostring(L, 2);
    const rotable_Reg *p  =
        (const rotable_Reg *)lua_touserdata(L, lua_upvalueindex(1));
    const rotable_Reg *p2 = p;

    p = find_key(p, s);
    if (p) {
        push_entry(L, p);
    } else {
        if (p2->name && strcmp(p2->name, "__index") == 0 && p2->func) {
            lua_pushcfunction(L, p2->func);
            lua_pushvalue(L, 2);
            lua_call(L, 1, 1);
            return 1;
        }
        lua_pushnil(L);
    }
    return 1;
}


/* -----------------------------------------------------------------------
** Public API
** --------------------------------------------------------------------- */

void rotable_newlib(lua_State *L, const rotable_Reg *reg)
{
    rotable_ud *t = (rotable_ud *)lua_newuserdatauv(L, sizeof(rotable_ud), 0);
    t->p = reg;
    push_shared_mt(L);
    lua_setmetatable(L, -2);
}

void rotable_newidx(lua_State *L, const rotable_Reg *reg)
{
    lua_pushlightuserdata(L, (void *)reg);
    lua_pushcclosure(L, rotable_func_index, 1);
}

int rotable_isrotable(lua_State *L, int idx)
{
    return test_rotable(L, idx) != NULL;
}

const rotable_Reg *rotable_entries(lua_State *L, int idx)
{
    rotable_ud *t = test_rotable(L, idx);
    return t ? t->p : NULL;
}
