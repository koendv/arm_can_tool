#include <rtthread.h>
#define DBG_TAG "LUA"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <lua_local.h>
#include <dfs_file.h>
#include "serials.h"
#include <at24c256.h>

/* push error message and nil */
static int push_error(lua_State *L, const char *msg)
{
    lua_pushnil(L);
    lua_pushstring(L, msg);
    return 2;
}

/* memory */

static int lua_mem_total(lua_State *L)
{
    rt_size_t total = 0, used = 0, max_used = 0;
    lua_memheap_info(&total, &used, &max_used);
    lua_pushinteger(L, total);
    return 1;
}

static int lua_mem_used(lua_State *L)
{
    rt_size_t total = 0, used = 0, max_used = 0;
    lua_memheap_info(&total, &used, &max_used);
    lua_pushinteger(L, (lua_Integer)used);
    return 1;
}

static int lua_mem_free(lua_State *L)
{
    rt_size_t total = 0, used = 0, max_used = 0;
    lua_memheap_info(&total, &used, &max_used);
    lua_pushinteger(L, (lua_Integer)(total - used));
    return 1;
}
static int lua_mem_max_used(lua_State *L)
{
    rt_size_t total = 0, used = 0, max_used = 0;
    lua_memheap_info(&total, &used, &max_used);
    lua_pushinteger(L, (lua_Integer)max_used);
    return 1;
}

/* input/output */

static int lua_sys_write(lua_State *L)
{
    size_t      len;
    const char *s = luaL_checklstring(L, 1, &len);
    cdc0_write(s, len);
    return 0;
}

static int lua_sys_log(lua_State *L)
{
    const char *s = luaL_checkstring(L, 1);
    LOG_I("%s", s);
    return 0;
}


/* file i/o */

/* sys.load() reader callback state — lives on the C stack, no heap allocation */
#define LOAD_CHUNK 512

typedef struct
{
    struct dfs_file *fd;
    char             buf[LOAD_CHUNK];
} LoadState;

static const char *lua_load_reader(lua_State *L, void *ud, size_t *sz)
{
    (void)L;
    LoadState *ls = (LoadState *)ud;
    ssize_t    n  = dfs_file_read(ls->fd, ls->buf, LOAD_CHUNK);
    if (n <= 0)
    {
        *sz = 0;
        return NULL;
    }
    *sz = (size_t)n;
    return ls->buf;
}

/* sys.load(filename) - load bytecode or source from SD card, execute once to register handlers.
 *
 * On success: executes the chunk (to register event handlers etc.), returns true.
 * On error:   returns nil, error_string.
 *
 * Uses lua_load() with a reader callback — file is fed directly into Lua
 * chunk by chunk via a stack-allocated buffer. No system heap, no double
 * allocation.
 */

static int lua_sys_load(lua_State *L)
{
    const char *filename = luaL_checkstring(L, 1);

    /* stat: confirm file exists and is regular */
    struct stat file_stat;
    if (dfs_file_stat(filename, &file_stat) != 0)
        return push_error(L, "file not found");

    if (!S_ISREG(file_stat.st_mode))
        return push_error(L, "not a regular file");

    if (file_stat.st_size == 0)
        return push_error(L, "file is empty");

    struct dfs_file fd;
    if (dfs_file_open(&fd, filename, O_RDONLY) != 0)
        return push_error(L, "open failed");

    /* load via reader callback — mode NULL: auto-detect source vs bytecode */
    LoadState ls;
    ls.fd  = &fd;
    int rc = lua_load(L, lua_load_reader, &ls, filename, NULL);
    dfs_file_close(&fd);

    if (rc != LUA_OK)
    {
        const char *err = lua_tostring(L, -1);
        lua_pop(L, 1);
        return push_error(L, err ? err : "load failed");
    }

    /* execute the chunk once — registers event handlers, sets globals, etc. */
    if (lua_pcall(L, 0, 0, 0) != LUA_OK)
    {
        const char *err = lua_tostring(L, -1);
        lua_pop(L, 1);
        return push_error(L, err ? err : "exec failed");
    }

    lua_pushboolean(L, 1);
    return 1;
}


/* sys.dump(func, filename) - compile func to bytecode, print as a shell heredoc
 * on the Lua shell terminal (CDC1).
 *
 * Output can be copy/pasted directly into a Linux shell to produce the binary:
 *
 *   xxd -r << 'EOF' > printNumbers.luac
 *   00000000: 1b4c 7561 5500 1993 0d0a 1a0a 0488 a9ff  .LuaU...........
 *   ...
 *   EOF
 *
 * strip_debug = true: no debug info, appropriate for embedded target.
 */

#define XXD_COLS 16 /* bytes per line, must be 16 for xxd -r compatibility */

typedef struct
{
    uint8_t line[XXD_COLS]; /* accumulate one output line worth of bytes */
    size_t  col;            /* current position within line */
    size_t  offset;         /* byte offset into bytecode stream */
} XxdState;

/* format and write one xxd line to the Lua shell terminal */
static void xxd_flush_line(XxdState *xs)
{
    if (xs->col == 0)
        return;

    char buf[80]; /* one xxd line: 8 + 2 + 4*5 + 1 + 16 + 1 = ~70 chars */
    int  pos = 0;

    /* offset */
    pos += rt_snprintf(buf + pos, sizeof(buf) - pos,
                       "%08zx: ", xs->offset - xs->col);

    /* hex pairs in groups of 2, space after each group of 4 bytes */
    for (size_t i = 0; i < XXD_COLS; i++)
    {
        if (i < xs->col)
            pos += rt_snprintf(buf + pos, sizeof(buf) - pos,
                               "%02x", xs->line[i]);
        else
            pos += rt_snprintf(buf + pos, sizeof(buf) - pos, "  ");

        if ((i & 1) == 1)
            pos += rt_snprintf(buf + pos, sizeof(buf) - pos, " ");
    }

    /* ascii column */
    pos += rt_snprintf(buf + pos, sizeof(buf) - pos, " ");
    for (size_t i = 0; i < xs->col; i++)
    {
        uint8_t c  = xs->line[i];
        pos       += rt_snprintf(buf + pos, sizeof(buf) - pos,
                                 "%c", (c >= 0x20 && c < 0x7f) ? c : '.');
    }

    pos += rt_snprintf(buf + pos, sizeof(buf) - pos, "\r\n");

    cdc1_write(buf, pos);
}

static int lua_dump_xxd_writer(lua_State *L, const void *p, size_t sz, void *ud)
{
    (void)L;
    XxdState      *xs  = (XxdState *)ud;
    const uint8_t *src = (const uint8_t *)p;

    for (size_t i = 0; i < sz; i++)
    {
        xs->line[xs->col++] = src[i];
        xs->offset++;

        if (xs->col == XXD_COLS)
        {
            xxd_flush_line(xs);
            xs->col = 0;
        }
    }

    return 0;
}

static int lua_sys_dump(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TFUNCTION);
    const char *filename = luaL_checkstring(L, 2);

    const bool strip_debug = true;

    /* heredoc header — one write, one line */
    char header[128];
    int  hlen = rt_snprintf(header, sizeof(header),
                            "xxd -r << 'EOF' > %s\r\n", filename);
    cdc1_write(header, hlen);

    /* dump bytecode as xxd lines */
    XxdState xs;
    memset(&xs, 0, sizeof(xs));

    lua_pushvalue(L, 1);
    int rc = lua_dump(L, lua_dump_xxd_writer, &xs, strip_debug);
    lua_pop(L, 1);

    /* flush any remaining bytes in the last partial line */
    xxd_flush_line(&xs);

    /* heredoc terminator */
    cdc1_write("EOF\r\n", 5);

    if (rc != 0)
        return push_error(L, "dump failed");

    lua_pushboolean(L, 1);
    return 1;
}


/* eeprom */

/* sys.eeprom_write(address, string). returns bytes_written or nil, err */
static int lua_sys_eeprom_write(lua_State *L)
{
    lua_Integer address = luaL_checkinteger(L, 1);
    size_t      len;
    const char *data = luaL_checklstring(L, 2, &len);

    if (address < 0)
        return push_error(L, "invalid address");

    int32_t ret = at24_write((uint32_t)address, (const uint8_t *)data, (uint32_t)len);

    if (ret < 0)
        return push_error(L, "write failed");

    lua_pushinteger(L, ret);
    return 1;
}

/* sys.eeprom_read(address, length). returns string or nil, err */
static int lua_sys_eeprom_read(lua_State *L)
{
    lua_Integer address = luaL_checkinteger(L, 1);
    lua_Integer length  = luaL_checkinteger(L, 2);

    if (address < 0 || length <= 0)
        return push_error(L, "invalid args");

    luaL_Buffer b;
    uint8_t    *buf = (uint8_t *)luaL_buffinitsize(L, &b, (size_t)length);

    int32_t ret = at24_read((uint32_t)address, buf, (uint32_t)length);

    if (ret < 0)
    {
        lua_pop(L, 1);
        return push_error(L, "read failed");
    }

    luaL_pushresultsize(&b, ret);
    return 1;
}

/* serial */

static int lua_serial_receive(lua_State *L, rt_device_t dev)
{
    lua_Integer count = luaL_checkinteger(L, 1);

    if (!dev || count <= 0)
    {
        lua_pushliteral(L, "");
        return 1;
    }

    luaL_Buffer b;
    uint8_t    *buf = (uint8_t *)luaL_buffinitsize(L, &b, (size_t)count);

    rt_size_t n = rt_device_read(dev, 0, buf, (rt_size_t)count);

    luaL_pushresultsize(&b, (n > 0) ? (size_t)n : 0);
    return 1;
}

static int lua_serial0_receive(lua_State *L)
{
    return lua_serial_receive(L, serial0_dev);
}
static int lua_serial1_receive(lua_State *L)
{
    return lua_serial_receive(L, serial1_dev);
}

static int lua_serial_write(lua_State *L, rt_device_t dev)
{
    size_t      len;
    const char *buf = luaL_checklstring(L, 1, &len);

    if (dev && (len > 0))
        rt_device_write(dev, 0, (uint8_t *)buf, len);

    return 0;
}

static int lua_serial0_write(lua_State *L)
{
    return lua_serial_write(L, serial0_dev);
}
static int lua_serial1_write(lua_State *L)
{
    return lua_serial_write(L, serial1_dev);
}

/* sys library as rotable */

static const rotable_Reg sys_lib[] = {
    /* memory */
    {                "mem_total",        lua_mem_total,                         0},
    {                 "mem_used",         lua_mem_used,                         0},
    {                 "mem_free",         lua_mem_free,                         0},
    {             "mem_max_used",     lua_mem_max_used,                         0},
    /* i/o */
    {                      "log",          lua_sys_log,                         0},
    {                    "write",        lua_sys_write,                         0},
    /* files */
    {                     "dump",         lua_sys_dump,                         0},
    {                     "load",         lua_sys_load,                         0},
    /* eeprom */
    {             "eeprom_write", lua_sys_eeprom_write,                         0},
    {              "eeprom_read",  lua_sys_eeprom_read,                         0},
    /* serial */
    {          "serial0_receive",  lua_serial0_receive,                         0},
    {          "serial1_receive",  lua_serial1_receive,                         0},
    {            "serial0_write",    lua_serial0_write,                         0},
    {            "serial1_write",    lua_serial1_write,                         0},
    /* event names */
    {       "EVENT_CAN1_TX_DONE",                 NULL,        EVENT_CAN1_TX_DONE},
    {     "EVENT_CAN1_RX0_INDIC",                 NULL,      EVENT_CAN1_RX0_INDIC},
    {       "EVENT_CAN1_BUS_OFF",                 NULL,        EVENT_CAN1_BUS_OFF},
    {   "EVENT_CAN1_RX_OVERFLOW",                 NULL,    EVENT_CAN1_RX_OVERFLOW},
    {   "EVENT_CAN1_TX_OVERFLOW",                 NULL,    EVENT_CAN1_TX_OVERFLOW},
    {     "EVENT_GSUSB_BULK_OUT",                 NULL,      EVENT_GSUSB_BULK_OUT},
    {         "EVENT_GSUSB_STOP",                 NULL,          EVENT_GSUSB_STOP},
    {        "EVENT_GSUSB_START",                 NULL,         EVENT_GSUSB_START},
    {      "EVENT_GSUSB_TX_DONE",                 NULL,       EVENT_GSUSB_TX_DONE},
    {           "EVENT_CDC0_DTR",                 NULL,            EVENT_CDC0_DTR},
    {           "EVENT_CDC1_DTR",                 NULL,            EVENT_CDC1_DTR},
    {            "EVENT_CDC0_RX",                 NULL,             EVENT_CDC0_RX},
    {            "EVENT_CDC1_RX",                 NULL,             EVENT_CDC1_RX},
    {         "EVENT_SERIAL0_RX",                 NULL,          EVENT_SERIAL0_RX},
    {         "EVENT_SERIAL1_RX",                 NULL,          EVENT_SERIAL1_RX},
    {         "EVENT_SERIAL2_RX",                 NULL,          EVENT_SERIAL2_RX},
    {"EVENT_TARGET_HALT_REQUEST",                 NULL, EVENT_TARGET_HALT_REQUEST},
    {      "EVENT_TARGET_HALTED",                 NULL,       EVENT_TARGET_HALTED},
    {                       NULL,                 NULL,                         0}
};

int luaopen_sys_rotable(lua_State *L)
{
    rotable_newlib(L, sys_lib);
    lua_setglobal(L, "sys");
    return 0;
}

