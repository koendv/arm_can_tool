#include <rtthread.h>
#define DBG_TAG "LUA"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#include <string.h>
#include <stdint.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <lua_local.h>

#include "at32f402_405_conf.h"
#include "lua_flash.h"
#include "lua_lfs.h"
#include "rotable.h"
#include "serials.h"

/* -----------------------------------------------------------------------
** Compile-time check
** --------------------------------------------------------------------- */

RT_STATIC_ASSERT(lfs_header_size, sizeof(lfs_header_t) == LFS_HEADER_SIZE);

/* -----------------------------------------------------------------------
** Helpers
** --------------------------------------------------------------------- */

static int push_error(lua_State *L, const char *msg)
{
    lua_pushnil(L);
    lua_pushstring(L, msg);
    return 2;
}

static int check_sector(lua_State *L, int arg)
{
    lua_Integer s = luaL_checkinteger(L, arg);
    if (s < 0 || s >= LFS_NUM_SECTORS)
        luaL_error(L, "sector must be 0..%d", LFS_NUM_SECTORS - 1);
    return (int)s;
}

/* number of sectors needed for header + bytecode of given size */
static int lfs_sectors_needed(uint32_t size)
{
    return (int)((size + LFS_HEADER_SIZE + LFS_SECTOR_SIZE - 1)
                 / LFS_SECTOR_SIZE);
}

/* -----------------------------------------------------------------------
** flash.exec(sector [, args...]) — load and execute bytecode from sector.
** Extra args are passed to the chunk as varargs (...).
** --------------------------------------------------------------------- */

static int lfs_lua_exec(lua_State *L)
{
    int sector = check_sector(L, 1);
    int nargs  = lua_gettop(L) - 1; /* arguments after sector number */

    const lfs_header_t *h = lfs_sector_header(sector);

    if (h->magic == LFS_BLANK || h->size == 0)
        return push_error(L, "empty slot");

    if (h->size > lfs_max_size(sector))
        return push_error(L, "invalid size");

    const char *bc   = (const char *)(lfs_sector_addr(sector) + LFS_HEADER_SIZE);
    const char *name = h->name[0] ? h->name : "flash";

    /* mode "B": fixed buffer — code/lineinfo stay as XIP flash pointers,
    ** not copied to RAM. Proto structs and constants still allocated in RAM. */
    if (luaL_loadbufferx(L, bc, h->size, name, "B") != LUA_OK)
        return push_error(L, lua_tostring(L, -1));

    /* Stack before pcall: [sector, arg1, arg2, ..., chunk]
    ** lua_pcall expects:  [sector, chunk, arg1, arg2, ...]
    ** lua_rotate(L, 2, 1) rotates positions 2..top one step forward,
    ** moving chunk from top to position 2, sliding args up by one. */
    if (nargs > 0)
        lua_rotate(L, 2, 1);

    if (lua_pcall(L, nargs, LUA_MULTRET, 0) != LUA_OK)
        return push_error(L, lua_tostring(L, -1));

    return lua_gettop(L) - 1; /* return all results (minus sector arg) */
}

/* flash.load(sector) — load bytecode from sector, return as Lua function.
** Does not execute. The returned function can be stored and called later.
** Uses mode "B": instruction array stays as XIP flash pointer, not copied. */
static int lfs_lua_load(lua_State *L)
{
    int sector = check_sector(L, 1);

    const lfs_header_t *h = lfs_sector_header(sector);

    if (h->magic == LFS_BLANK || h->size == 0)
        return push_error(L, "empty slot");

    if (h->size > lfs_max_size(sector))
        return push_error(L, "invalid size");

    const char *bc   = (const char *)(lfs_sector_addr(sector) + LFS_HEADER_SIZE);
    const char *name = h->name[0] ? h->name : "flash";

    /* mode "B": instruction array stays as XIP flash pointer, not copied.
    ** Returns the loaded chunk as a function on the stack — not called. */
    if (luaL_loadbufferx(L, bc, h->size, name, "B") != LUA_OK)
        return push_error(L, lua_tostring(L, -1));

    return 1; /* return the loaded function */
}

/* -----------------------------------------------------------------------
** lua_run_autoexec() — load and execute bytecode from sector 0.
** Delegates to lfs_lua_exec() with sector 0.
** On failure, logs the error message left on the stack by lfs_lua_exec() before cleaning up.
** --------------------------------------------------------------------- */

int lfs_run_autoexec(lua_State *L)
{
    lua_pushinteger(L, 0);
    int rc = lfs_lua_exec(L);
    if (rc < 0)
    {
        LOG_E("autoexec: %s", lua_tostring(L, -1));
        lua_settop(L, 0);
        return 1;
    }
    lua_settop(L, 0);
    LOG_I("autoexec");
    return 0;
}

/* -----------------------------------------------------------------------
** Management functions
** --------------------------------------------------------------------- */

/* -----------------------------------------------------------------------
** HAL flash operations — direct AT32 HAL, no drv_flash wrapper.
** unlock once / operate / lock once per logical operation.
** Both functions verify the target address is within the LFS region
** before unlocking flash. A violation is logged and returns -1.
** --------------------------------------------------------------------- */

#define LFS_END (LFS_BASE + (uint32_t)LFS_NUM_SECTORS * LFS_SECTOR_SIZE)

int lfs_hal_erase(uint32_t addr)
{
    if (addr < LFS_BASE || addr + LFS_SECTOR_SIZE > LFS_END)
    {
        LOG_E("lfs_hal_erase: 0x%08lx outside LFS region", (unsigned long)addr);
        return -1;
    }
    flash_unlock();
    flash_status_type st = flash_sector_erase(addr);
    flash_lock();
    return (st == FLASH_OPERATE_DONE) ? 0 : -1;
}

/*
 * Write a word-aligned buffer to flash.
 * addr and size must both be multiples of 4.
 * Flash must already be erased.
 * unlock/lock wraps the entire loop.
 */
int lfs_hal_write(uint32_t addr, const uint8_t *buf, size_t size)
{
    if (addr < LFS_BASE || addr + (uint32_t)size > LFS_END)
    {
        LOG_E("lfs_hal_write: 0x%08lx+%u outside LFS region",
              (unsigned long)addr, (unsigned)size);
        return -1;
    }
    flash_unlock();
    flash_status_type st  = FLASH_OPERATE_DONE;
    const uint8_t    *end = buf + size;

    while (buf < end)
    {
        uint32_t word;
        memcpy(&word, buf, 4);
        st = flash_word_program(addr, word);
        if (st != FLASH_OPERATE_DONE)
        {
            LOG_E("lfs_hal_write: flash_word_program failed at 0x%08lx status=%d",
                  (unsigned long)addr, (int)st);
            break;
        }
        addr += 4;
        buf  += 4;
    }

    flash_lock();
    return (st == FLASH_OPERATE_DONE) ? 0 : -1;
}

/* -----------------------------------------------------------------------
** Two-pass dump writer state — lives on C stack, no heap.
**
** Pass 1: count bytecode size via null writer.
** Pass 2: erase required sectors, write header, write bytecode.
** Two passes avoids needing to re-erase the header sector after
** learning the size, and keeps the writer simple.
** --------------------------------------------------------------------- */

#define DUMP_BUF_SIZE 128 /* must be multiple of 4 */

typedef struct
{
    uint32_t flash_addr;
    uint8_t  buf[DUMP_BUF_SIZE];
    size_t   buf_used;
    size_t   total;
    int      error;
} DumpState;

/* pass 1: count bytes only */
static int lfs_count_writer(lua_State *L, const void *p, size_t sz, void *ud)
{
    (void)L;
    (void)p;
    size_t *total  = (size_t *)ud;
    *total        += sz;
    return 0;
}

/* pass 2: accumulate into buffer, flush full buffers to flash */
static int lfs_flash_writer(lua_State *L, const void *p, size_t sz, void *ud)
{
    (void)L;
    DumpState     *ds  = (DumpState *)ud;
    const uint8_t *src = (const uint8_t *)p;

    if (ds->error) return 1;

    while (sz > 0)
    {
        size_t space = DUMP_BUF_SIZE - ds->buf_used;
        size_t chunk = sz < space ? sz : space;
        memcpy(ds->buf + ds->buf_used, src, chunk);
        ds->buf_used += chunk;
        src          += chunk;
        sz           -= chunk;
        ds->total    += chunk;

        if (ds->buf_used == DUMP_BUF_SIZE)
        {
            int rc = lfs_hal_write(ds->flash_addr, ds->buf, DUMP_BUF_SIZE);
            if (rc != 0)
            {
                ds->error = rc;
                return 1;
            }
            ds->flash_addr += DUMP_BUF_SIZE;
            ds->buf_used    = 0;
        }
    }
    return 0;
}

static int lfs_flash_flush(DumpState *ds)
{
    if (ds->error) return ds->error;
    if (ds->buf_used == 0) return 0;
    /* pad to word boundary with 0x00 */
    size_t padded = (ds->buf_used + 3) & ~3u;
    memset(ds->buf + ds->buf_used, 0, padded - ds->buf_used);
    int rc = lfs_hal_write(ds->flash_addr, ds->buf, padded);
    if (rc != 0) ds->error = rc;
    return rc;
}

/* flash.write(func, sector [, name]) */
static int lfs_lua_write(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TFUNCTION);
    int         sector = check_sector(L, 2);
    const char *name   = luaL_optstring(L, 3, "");

    /* --- pass 1: count bytecode size --- */
    size_t bc_size = 0;
    lua_pushvalue(L, 1);
    if (lua_dump(L, lfs_count_writer, &bc_size, 1) != 0)
    {
        lua_pop(L, 1);
        return push_error(L, "dump (count) failed");
    }
    lua_pop(L, 1);

    if (bc_size == 0)
        return push_error(L, "empty bytecode");

    if ((uint32_t)bc_size > lfs_max_size(sector))
        return push_error(L, "bytecode too large for sector");

    /* --- erase required sectors --- */
    int n_sectors = lfs_sectors_needed((uint32_t)bc_size);
    for (int i = 0; i < n_sectors; i++)
    {
        if (lfs_hal_erase(lfs_sector_addr(sector + i)) != 0)
        {
            LOG_W("flash.write: erase failed at sector 0x%02x; "
                  "sectors 0x%02x..0x%02x may be partially erased",
                  sector + i, sector, sector + i - 1);
            return push_error(L, "erase failed");
        }
    }

    /* --- write header --- */
    lfs_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = LFS_MAGIC;
    hdr.size  = (uint32_t)bc_size;
    rt_strncpy(hdr.name, name, LFS_NAME_SIZE - 1);
    hdr.name[LFS_NAME_SIZE - 1] = '\0';

    if (lfs_hal_write(lfs_sector_addr(sector),
                      (const uint8_t *)&hdr, sizeof(hdr))
        != 0)
        return push_error(L, "header write failed");

    /* --- pass 2: write bytecode --- */
    DumpState ds;
    memset(&ds, 0, sizeof(ds));
    ds.flash_addr = lfs_sector_addr(sector) + LFS_HEADER_SIZE;

    lua_pushvalue(L, 1);
    int rc = lua_dump(L, lfs_flash_writer, &ds, 1 /* strip debug */);
    lua_pop(L, 1);

    if (rc != 0 || ds.error)
        return push_error(L, "dump (write) failed");

    if (lfs_flash_flush(&ds) != 0)
        return push_error(L, "flash write failed");

    lua_pushboolean(L, 1);
    lua_pushinteger(L, (lua_Integer)bc_size);
    lua_pushinteger(L, (lua_Integer)n_sectors);
    return 3; /* true, bytecode_size, sectors_used */
}

/* flash.erase(sector) */
static int lfs_lua_erase(lua_State *L)
{
    int sector = check_sector(L, 1);

    if (lfs_hal_erase(lfs_sector_addr(sector)) != 0)
        return push_error(L, "erase failed");

    lua_pushboolean(L, 1);
    return 1;
}

/* flash.eraseall() */
static int lfs_lua_eraseall(lua_State *L)
{
    for (int i = 0; i < LFS_NUM_SECTORS; i++)
    {
        if (lfs_hal_erase(lfs_sector_addr(i)) != 0)
            return push_error(L, "erase failed");
    }
    lua_pushboolean(L, 1);
    return 1;
}

/* flash.list() */
static int lfs_lua_list(lua_State *L)
{
    char line[80];
    int  i    = 0;
    int  free = 0;

    while (i < LFS_NUM_SECTORS)
    {
        const lfs_header_t *h = lfs_sector_header(i);
        int                 n;

        if (h->magic == LFS_BLANK)
        {
            free++;
            i++;
            continue;
        }

        if (h->magic != LFS_MAGIC || h->size == 0)
        {
            n = rt_snprintf(line, sizeof(line),
                            "0x%02x          invalid (magic=0x%08lx)\r\n",
                            i, (unsigned long)h->magic);
            cdc1_write(line, n);
            i++;
            continue;
        }

        if (h->size > lfs_max_size(i))
        {
            n = rt_snprintf(line, sizeof(line),
                            "0x%02x          error size=%lu exceeds flash\r\n",
                            i, (unsigned long)h->size);
            cdc1_write(line, n);
            i++;
            continue;
        }

        int n_sectors = lfs_sectors_needed(h->size);
        n             = rt_snprintf(line, sizeof(line),
                                    "0x%02x  %5lu  %s\r\n",
                                    i,
                                    (unsigned long)h->size,
                        h->name[0] ? h->name : "(unnamed)");
        cdc1_write(line, n);

        for (int j = 1; j < n_sectors; j++)
        {
            n = rt_snprintf(line, sizeof(line), "0x%02x  (continued)\r\n", i + j);
            cdc1_write(line, n);
        }

        i += n_sectors;
    }

    /* free sector count */
    int n = rt_snprintf(line, sizeof(line), "%d/%d free\r\n", free, LFS_NUM_SECTORS);
    cdc1_write(line, n);

    return 0;
}

/* receive bytecode from serial port and write to flash */

static int lfs_lua_receive(lua_State *L)
{
    const char msg[] = "Waiting for data\r\n";

    if (!lfs_receive_init())
    {
        return push_error(L, "out of memory");
    }

    cdc1_write(msg, strlen(msg));

    lua_pushboolean(L, 1);

    return 1;
}

/* -----------------------------------------------------------------------
** Static rotable — 5 entries, fully in QSPI flash.
** Zero RAM except the rotable userdata header.
** --------------------------------------------------------------------- */

static const rotable_Reg flash_lib[] = {
    {    "exec",     lfs_lua_exec, 0},
    {    "load",     lfs_lua_load, 0},
    {"autoexec", lfs_run_autoexec, 0},
    {   "write",    lfs_lua_write, 0},
    {   "erase",    lfs_lua_erase, 0},
    {"eraseall", lfs_lua_eraseall, 0},
    {    "list",     lfs_lua_list, 0},
    { "receive",  lfs_lua_receive, 0},
    {      NULL,             NULL, 0},
};

/* -----------------------------------------------------------------------
** luaopen
** --------------------------------------------------------------------- */

int luaopen_flash_rotable(lua_State *L)
{
    rotable_newlib(L, flash_lib);
    lua_setglobal(L, "flash");
    return 0;
}
