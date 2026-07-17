#include <rtthread.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <stdbool.h>
#include <string.h>

#include "lua_local.h"

/* black magic debug includes */
#include "general.h"
#include "gdb_main.h"
#include "target.h"
#include "target_internal.h"

/* lua bmd library */

/* push error message and nil */
static int push_error(lua_State *L, const char *msg)
{
    lua_pushnil(L);
    lua_pushstring(L, msg);
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
    const char  *data = luaL_checklstring(L, 2, &len);
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

static int lua_bmd_target_running(lua_State *L)
{
    if (!cur_target)
        return push_error(L, "not attached");

    lua_pushboolean(L, gdb_target_running);
    return 1;
}

/* registers bmd library as rotable */

static const rotable_Reg bmd_lib[] = {
    {          "attach",           lua_bmd_attach, 0},
    {"breakwatch_clear", lua_bmd_breakwatch_clear, 0},
    {  "breakwatch_set",   lua_bmd_breakwatch_set, 0},
    {  "flash_complete",   lua_bmd_flash_complete, 0},
    {     "flash_erase",      lua_bmd_flash_erase, 0},
    {"flash_mass_erase", lua_bmd_flash_mass_erase, 0},
    {     "flash_write",      lua_bmd_flash_write, 0},
    {       "halt_poll",        lua_bmd_halt_poll, 0},
    {    "halt_request",     lua_bmd_halt_request, 0},
    {     "halt_resume",      lua_bmd_halt_resume, 0},
    {      "mem32_read",       lua_bmd_mem32_read, 0},
    {     "mem32_write",      lua_bmd_mem32_write, 0},
    {        "reg_read",         lua_bmd_reg_read, 0},
    {       "reg_write",        lua_bmd_reg_write, 0},
    {       "regs_read",        lua_bmd_regs_read, 0},
    {      "regs_write",       lua_bmd_regs_write, 0},
    {           "reset",            lua_bmd_reset, 0},
    {  "target_running",   lua_bmd_target_running, 0},
    {              NULL,                     NULL, 0}
};

int luaopen_bmd_rotable(lua_State *L)
{
    rotable_newlib(L, bmd_lib);
    lua_setglobal(L, "bmd");
    return 0;
}
