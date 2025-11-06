#include <rtthread.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <stdbool.h>
#include <string.h>

/* black magic debug includes */
#include "general.h"
#include "gdb_main.h"
#include "target.h"
#include "target_internal.h"

/* lua bmd library

Example:
msh />lua
> bmd.attach()
true
> bmd.mem32_write(0x20001000, "0123456789ABCDEF")
true
> bmd.mem32_read(0x20001000, 16)
0123456789ABCDEF

To obtain register and memory mapping, connect using gdb:
 $ arm-none-eabi-gdb
(gdb) tar ext /dev/ttyACM0
(gdb) mon swd
(gdb) at 1
(gdb) info mem
(gdb) info reg

stack overflow and lua data corruption may occur if LUA2RTT_THREAD_STACK_SIZE is too small.
*/

/* push error message and nil */
static int push_error(lua_State *L, const char *msg)
{
    lua_pushnil(L);
    lua_pushstring(L, msg);
    return 2;
}

static const char lua_bmd_help_str[] = "\n\
bmd.help()\n\
bmd.attach()\n\
bmd.mem32_read(addr, len)\n\
bmd.mem32_write(addr, str)\n\
bmd.flash_mass_erase()\n\
bmd.flash_erase(addr, len)\n\
bmd.flash_write(addr, str)\n\
bmd.flash_complete()\n\
bmd.regs_read()\n\
bmd.regs_write(str)\n\
bmd.reg_read(reg)\n\
bmd.reg_write(reg, str)\n\
bmd.halt_request()\n\
bmd.halt_resume()\n\
bmd.halt_poll()\n\
bmd.breakwatch_set(typ, addr, len)\n\
bmd.breakwatch_clear(typ, addr, len)\n\
bmd.reset()\
";

static int lua_bmd_help(lua_State *L)
{
    lua_pushboolean(L, 1);
    lua_pushstring(L, lua_bmd_help_str);
    return 2;
}

static int lua_bmd_reset(lua_State *L)
{
    if (!cur_target)
        return push_error(L, "not attached");

    char *msg = NULL;
    TRY(EXCEPTION_ALL)
    {
        target_reset(cur_target);
    }
    CATCH()
    {
    case EXCEPTION_TIMEOUT:
        msg = "timeout";
        break;
    case EXCEPTION_ERROR:
        msg = (char *)exception_frame.msg;
        break;
    default:
        break;
    }

    if (msg)
        return push_error(L, msg);

    lua_pushboolean(L, 1);
    return 1;
}

static int lua_bmd_halt_request(lua_State *L)
{
    if (!cur_target)
        return push_error(L, "not attached");

    char *msg = NULL;
    TRY(EXCEPTION_ALL)
    {
        target_halt_request(cur_target);
    }
    CATCH()
    {
    case EXCEPTION_TIMEOUT:
        msg = "timeout";
        break;
    case EXCEPTION_ERROR:
        msg = (char *)exception_frame.msg;
        break;
    default:
        break;
    }

    if (msg)
        return push_error(L, msg);

    lua_pushboolean(L, 1);
    return 1;
}

static int lua_bmd_halt_poll(lua_State *L)
{
    target_addr64_t      watch  = 0;
    target_halt_reason_e reason = TARGET_HALT_ERROR;

    if (!cur_target)
        return push_error(L, "not attached");

    char *msg = NULL;
    TRY(EXCEPTION_ALL)
    {
        reason = target_halt_poll(cur_target, &watch);
    }
    CATCH()
    {
    case EXCEPTION_TIMEOUT:
        msg = "timeout";
        break;
    case EXCEPTION_ERROR:
        msg = (char *)exception_frame.msg;
        break;
    default:
        break;
    }

    if (msg)
        return push_error(L, msg);

    lua_pushboolean(L, 1);
    lua_pushinteger(L, reason);
    lua_pushinteger(L, watch);
    return 3;
}

static int lua_bmd_halt_resume(lua_State *L)
{
    if (!cur_target)
        return push_error(L, "not attached");

    char *msg = NULL;
    TRY(EXCEPTION_ALL)
    {
        target_halt_resume(cur_target, false);
    }
    CATCH()
    {
    case EXCEPTION_TIMEOUT:
        msg = "timeout";
        break;
    case EXCEPTION_ERROR:
        msg = (char *)exception_frame.msg;
        break;
    default:
        break;
    }

    if (msg)
        return push_error(L, msg);

    lua_pushboolean(L, 1);
    return 1;
}

/* erase and write target flash */

static int lua_bmd_flash_mass_erase(lua_State *L)
{
    if (!cur_target)
        return push_error(L, "not attached");

    char *msg = NULL;
    TRY(EXCEPTION_ALL)
    {
        if (!target_flash_mass_erase(cur_target))
            msg = "flash mass erase failed";
    }
    CATCH()
    {
    case EXCEPTION_TIMEOUT:
        msg = "timeout";
        break;
    case EXCEPTION_ERROR:
        msg = (char *)exception_frame.msg;
        break;
    default:
        break;
    }

    if (msg)
        return push_error(L, msg);

    lua_pushboolean(L, 1);
    return 1;
}

static int lua_bmd_flash_erase(lua_State *L)
{
    unsigned int addr = luaL_checkinteger(L, 1);
    size_t       len  = luaL_checkinteger(L, 2);

    if (!cur_target)
        return push_error(L, "not attached");

    char *msg = NULL;
    TRY(EXCEPTION_ALL)
    {
        if (!target_flash_erase(cur_target, addr, len))
            msg = "flash erase failed";
    }
    CATCH()
    {
    case EXCEPTION_TIMEOUT:
        msg = "timeout";
        break;
    case EXCEPTION_ERROR:
        msg = (char *)exception_frame.msg;
        break;
    default:
        break;
    }

    if (msg)
        return push_error(L, msg);

    lua_pushboolean(L, 1);
    return 1;
}

static int lua_bmd_flash_write(lua_State *L)
{
    unsigned int addr = luaL_checkinteger(L, 1);
    size_t       len;
    const char  *data = luaL_checklstring(L, 2, &len);

    if (!cur_target)
        return push_error(L, "not attached");

    char *msg = NULL;
    TRY(EXCEPTION_ALL)
    {
        if (!target_flash_write(cur_target, addr, data, len))
            msg = "flash write failed";
    }
    CATCH()
    {
    case EXCEPTION_TIMEOUT:
        msg = "timeout";
        break;
    case EXCEPTION_ERROR:
        msg = (char *)exception_frame.msg;
        break;
    default:
        break;
    }

    if (msg)
        return push_error(L, msg);

    lua_pushboolean(L, 1);
    return 1;
}

static int lua_bmd_flash_complete(lua_State *L)
{
    if (!cur_target)
        return push_error(L, "not attached");

    char *msg = NULL;
    TRY(EXCEPTION_ALL)
    {
        if (!target_flash_complete(cur_target))
            msg = "flash complete failed";
    }
    CATCH()
    {
    case EXCEPTION_TIMEOUT:
        msg = "timeout";
        break;
    case EXCEPTION_ERROR:
        msg = (char *)exception_frame.msg;
        break;
    default:
        break;
    }

    if (msg)
        return push_error(L, msg);

    lua_pushboolean(L, 1);
    return 1;
}

/* read and write target ram */

static int lua_bmd_mem32_read(lua_State *L)
{
    unsigned int addr = luaL_checkinteger(L, 1);
    size_t       len  = luaL_checkinteger(L, 2);
    if (!cur_target)
        return push_error(L, "not attached");
    luaL_Buffer b;
    // allocate lua string buffer
    uint8_t *p = (uint8_t *)luaL_buffinitsize(L, &b, len);
    if (!p)
        return push_error(L, "lua out of memory");
    if (target_check_error(cur_target))
        return push_error(L, "target error");

    char *msg = NULL;
    TRY(EXCEPTION_ALL)
    {
        if (target_mem32_read(cur_target, p, addr, len))
            msg = "mem32 read error";
    }
    CATCH()
    {
    case EXCEPTION_TIMEOUT:
        msg = "timeout";
        break;
    case EXCEPTION_ERROR:
        msg = (char *)exception_frame.msg;
        break;
    default:
        break;
    }

    if (msg)
        return push_error(L, msg);

    luaL_pushresultsize(&b, len);
    return 1;
}

static int lua_bmd_mem32_write(lua_State *L)
{
    unsigned int addr = luaL_checkinteger(L, 1);
    size_t       len;
    const char  *data = luaL_checklstring(L, 2, &len);
    if (!cur_target)
        return push_error(L, "not attached");
    if (target_check_error(cur_target))
        return push_error(L, "target error");

    char *msg = NULL;
    TRY(EXCEPTION_ALL)
    {
        if (target_mem32_write(cur_target, addr, data, len))
            msg = "mem32 write error";
    }
    CATCH()
    {
    case EXCEPTION_TIMEOUT:
        msg = "timeout";
        break;
    case EXCEPTION_ERROR:
        msg = (char *)exception_frame.msg;
        break;
    default:
        break;
    }

    if (msg)
        return push_error(L, msg);

    lua_pushboolean(L, 1);
    return 1;
}

/* read and write target registers */

static int lua_bmd_regs_read(lua_State *L)
{
    if (!cur_target)
        return push_error(L, "not attached");

    size_t len = target_regs_size(cur_target);
    if (len == 0)
        return push_error(L, "no regs");

    // allocate lua string buffer
    luaL_Buffer b;
    uint8_t    *p = (uint8_t *)luaL_buffinitsize(L, &b, len);
    if (!p)
        return push_error(L, "lua out of memory");

    char *msg = NULL;
    TRY(EXCEPTION_ALL)
    {
        target_regs_read(cur_target, p);
    }
    CATCH()
    {
    case EXCEPTION_TIMEOUT:
        msg = "timeout";
        break;
    case EXCEPTION_ERROR:
        msg = (char *)exception_frame.msg;
        break;
    default:
        break;
    }

    if (msg)
        return push_error(L, msg);

    luaL_pushresultsize(&b, len);
    return 1;
}

static int lua_bmd_regs_write(lua_State *L)
{
    size_t      len;
    const char *data = luaL_checklstring(L, 1, &len);

    if (!cur_target)
        return push_error(L, "not attached");

    if (len != target_regs_size(cur_target))
        return push_error(L, "wrong size");

    char *msg = NULL;
    TRY(EXCEPTION_ALL)
    {
        target_regs_write(cur_target, data);
    }
    CATCH()
    {
    case EXCEPTION_TIMEOUT:
        msg = "timeout";
        break;
    case EXCEPTION_ERROR:
        msg = (char *)exception_frame.msg;
        break;
    default:
        break;
    }

    if (msg)
        return push_error(L, msg);

    lua_pushboolean(L, 1);
    return 1;
}

static int lua_bmd_reg_read(lua_State *L)
{
    uint8_t      val[8];
    unsigned int reg = luaL_checkinteger(L, 1);
    size_t       len = 0;

    if (!cur_target)
        return push_error(L, "not attached");

    char *msg = NULL;
    TRY(EXCEPTION_ALL)
    {
        len = target_reg_read(cur_target, reg, val, sizeof(val));
        if (len == 0)
            msg = "reg read error";
    }
    CATCH()
    {
    case EXCEPTION_TIMEOUT:
        msg = "timeout";
        break;
    case EXCEPTION_ERROR:
        msg = (char *)exception_frame.msg;
        break;
    default:
        break;
    }

    if (msg)
        return push_error(L, msg);

    lua_pushlstring(L, (const char *)val, len);
    return 1;
}

static int lua_bmd_reg_write(lua_State *L)
{
    unsigned int reg = luaL_checkinteger(L, 1);
    size_t       len;
    const char  *data = luaL_checklstring(L, 1, &len);
    size_t       written;

    if (!cur_target)
        return push_error(L, "not attached");

    char *msg = NULL;
    TRY(EXCEPTION_ALL)
    {
        written = target_reg_write(cur_target, reg, data, len);
        if (written != len)
            msg = "wrong size";
    }
    CATCH()
    {
    case EXCEPTION_TIMEOUT:
        msg = "timeout";
        break;
    case EXCEPTION_ERROR:
        msg = (char *)exception_frame.msg;
        break;
    default:
        break;
    }

    if (msg)
        return push_error(L, msg);

    lua_pushboolean(L, 1);
    return 1;
}

static int lua_bmd_breakwatch_set(lua_State *L)
{
    unsigned int typ  = luaL_checkinteger(L, 1);
    unsigned int addr = luaL_checkinteger(L, 2);
    size_t       len  = luaL_checkinteger(L, 3);

    if (!cur_target)
        return push_error(L, "not attached");

    char *msg = NULL;
    TRY(EXCEPTION_ALL)
    {
        if (target_breakwatch_set(cur_target, typ, addr, len) != 0)
            msg = "breakwatch set failed";
    }
    CATCH()
    {
    case EXCEPTION_TIMEOUT:
        msg = "timeout";
        break;
    case EXCEPTION_ERROR:
        msg = (char *)exception_frame.msg;
        break;
    default:
        break;
    }

    if (msg)
        return push_error(L, msg);

    lua_pushboolean(L, 1);
    return 1;
}

static int lua_bmd_breakwatch_clear(lua_State *L)
{
    unsigned int typ  = luaL_checkinteger(L, 1);
    unsigned int addr = luaL_checkinteger(L, 2);
    size_t       len  = luaL_checkinteger(L, 3);

    if (!cur_target)
        return push_error(L, "not attached");

    char *msg = NULL;
    TRY(EXCEPTION_ALL)
    {
        if (target_breakwatch_clear(cur_target, typ, addr, len) != 0)
            msg = "breakwatch clear failed";
    }
    CATCH()
    {
    case EXCEPTION_TIMEOUT:
        msg = "timeout";
        break;
    case EXCEPTION_ERROR:
        msg = (char *)exception_frame.msg;
        break;
    default:
        break;
    }

    if (msg)
        return push_error(L, msg);

    lua_pushboolean(L, 1);
    return 1;
}

static int lua_bmd_attach(lua_State *L)
{
    target_s *target;
    char     *msg = NULL;
    if (connect_assert_nrst)
        platform_nrst_set_val(true); /* will be deasserted after attach */

    bool scan_result = false;
    TRY(EXCEPTION_ALL)
    {
        scan_result = adiv5_swd_scan(0);
    }
    CATCH()
    {
    case EXCEPTION_TIMEOUT:
        msg = "Timeout during scan. Is target stuck in WFI?";
        break;
    case EXCEPTION_ERROR:
        msg = (char *)exception_frame.msg;
        break;
    default:
        break;
    }

    if (!scan_result)
        msg = "swd scan failed";

    if (msg)
    {
        platform_target_clk_output_enable(false);
        platform_nrst_set_val(false);
        return push_error(L, msg);
    }

    platform_target_clk_output_enable(false);

    /* Attach to remote target processor */
    extern target_controller_s gdb_controller;
    cur_target = target_attach_n(1, &gdb_controller);
    if (!cur_target)
        return push_error(L, "attach failed");

    lua_pushboolean(L, 1);
    return 1;
}

/* bmd library */
static const struct luaL_Reg bmd_lib[] = {
    {            "help",             lua_bmd_help},
    {          "attach",           lua_bmd_attach},
    {      "mem32_read",       lua_bmd_mem32_read},
    {     "mem32_write",      lua_bmd_mem32_write},
    {"flash_mass_erase", lua_bmd_flash_mass_erase},
    {     "flash_erase",      lua_bmd_flash_erase},
    {     "flash_write",      lua_bmd_flash_write},
    {  "flash_complete",   lua_bmd_flash_complete},
    {       "regs_read",        lua_bmd_regs_read},
    {      "regs_write",       lua_bmd_regs_write},
    {        "reg_read",         lua_bmd_reg_read},
    {       "reg_write",        lua_bmd_reg_write},
    {    "halt_request",     lua_bmd_halt_request},
    {       "halt_poll",        lua_bmd_halt_poll},
    {     "halt_resume",      lua_bmd_halt_resume},
    {           "reset",            lua_bmd_reset},
    {  "breakwatch_set",   lua_bmd_breakwatch_set},
    {"breakwatch_clear", lua_bmd_breakwatch_clear},
    {              NULL,                     NULL}
};

/* called from lua init, registers bmd library */
int luaopen_bmd(lua_State *L)
{
    luaL_newlib(L, bmd_lib);
    return 1;
}

