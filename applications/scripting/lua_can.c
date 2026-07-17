/*
 * lua_can.c — Lua 5.5 rotable binding for AT32F405 CAN bus
 *
 * Functions:
 *   can.speed(freq)
 *   mailbox = can.transmit(id, data, id_type, frame_type)
 *   id, data, id_type, frame_type, timestamp = can.receive()
 *   s0,ts0, s1,ts1, s2,ts2 = can.tx_state()
 *
 * Constants (integer, in rotable):
 *   can.ID_STD      = 0   (CAN_ID_STANDARD)
 *   can.ID_EXT      = 1   (CAN_ID_EXTENDED)
 *   can.FRAME_DATA  = 0   (CAN_TFT_DATA)
 *   can.FRAME_RTR   = 1   (CAN_TFT_REMOTE)
 *   can.TX_MAILBOX0 = 0   (CAN_TX_MAILBOX0)
 *   can.TX_MAILBOX1 = 1   (CAN_TX_MAILBOX1)
 *   can.TX_MAILBOX2 = 2   (CAN_TX_MAILBOX2)
 *   can.TX_FAILED   = 0   (CAN_TX_STATUS_FAILED)
 *   can.TX_OK       = 1   (CAN_TX_STATUS_SUCCESSFUL)
 *   can.TX_PENDING  = 2   (CAN_TX_STATUS_PENDING)
 *   can.TX_NO_EMPTY = 4   (CAN_TX_STATUS_NO_EMPTY)
 */

#include "lua.h"
#include "lauxlib.h"
#include "rotable.h"
#include "at32f402_405_can.h"
#include "canbus.h"
#include "canbus_event.h"
#include "lua_local.h"

/* ------------------------------------------------------------------ */
/* can.speed(freq)                                                     */
/*   freq: bitrate in Hz, e.g. 125000, 250000, 500000, 1000000        */
/*   returns: true on success, or nil + error message on failure       */
/* ------------------------------------------------------------------ */
static int lua_can_speed(lua_State *L)
{
    lua_Integer freq = luaL_checkinteger(L, 1);

    if (freq <= 0)
        return luaL_argerror(L, 1, "frequency must be positive");

    rt_err_t err = can_set_bitrate_freq((uint32_t)freq);

    if (err != RT_EOK)
    {
        lua_pushnil(L);
        lua_pushfstring(L, "can_set_bitrate_freq failed (err=%d)", (int)err);
        return 2;
    }

    lua_pushboolean(L, 1);
    return 1;
}

/* ------------------------------------------------------------------ */
/* can.init(enable)                                                   */
/*   enable: true  -> start CAN controller (bus active)               */
/*           false -> initialise but leave bus off (frozen)           */
/*   returns: true on success, or nil + error message on failure      */
/* ------------------------------------------------------------------ */

static int lua_can_init(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TBOOLEAN);
    bool enable = (bool)lua_toboolean(L, 1);

    rt_err_t err = can_init(enable);

    if (err != RT_EOK)
    {
        lua_pushnil(L);
        lua_pushfstring(L, "can_init failed (err=%d)", (int)err);
        return 2;
    }

    lua_pushboolean(L, 1);
    return 1;
}

/* ------------------------------------------------------------------ */
/* can.transmit(id, data, id_type, frame_type)                        */
/*   returns: mailbox (0/1/2) or nil if no mailbox free               */
/* ------------------------------------------------------------------ */
static int lua_can_transmit(lua_State *L)
{
    lua_Integer id = luaL_checkinteger(L, 1);
    size_t      data_len;
    const char *data       = luaL_checklstring(L, 2, &data_len);
    lua_Integer id_type    = luaL_checkinteger(L, 3);
    lua_Integer frame_type = luaL_checkinteger(L, 4);

    if (data_len > 8)
        return luaL_argerror(L, 2, "data too long (max 8 bytes)");

    can_tx_message_type tx = {0};

    if (id_type == CAN_ID_EXTENDED)
    {
        if (id < 0 || id > 0x1FFFFFFFL)
            return luaL_argerror(L, 1, "extended id out of range (0..0x1FFFFFFF)");
        tx.id_type     = CAN_ID_EXTENDED;
        tx.extended_id = (uint32_t)id;
    }
    else
    {
        if (id < 0 || id > 0x7FFL)
            return luaL_argerror(L, 1, "standard id out of range (0..0x7FF)");
        tx.id_type     = CAN_ID_STANDARD;
        tx.standard_id = (uint32_t)id;
    }

    tx.frame_type = (frame_type == CAN_TFT_REMOTE) ? CAN_TFT_REMOTE : CAN_TFT_DATA;
    tx.dlc        = (uint8_t)data_len;

    for (size_t i = 0; i < data_len; i++)
        tx.data[i] = (uint8_t)data[i];

    uint8_t mailbox = can_message_transmit(CAN1, &tx);

    if (mailbox == CAN_TX_STATUS_NO_EMPTY)
    {
        lua_pushnil(L);
        return 1;
    }

    lua_pushinteger(L, mailbox); /* 0, 1, or 2 */
    return 1;
}

/* ------------------------------------------------------------------ */
/* can.receive()                                                       */
/*   returns: id, data, id_type, frame_type, timestamp                */
/*        or: nil  (ring buffer empty)                                 */
/* ------------------------------------------------------------------ */
static int lua_can_receive(lua_State *L)
{
    can_stored_frame_t rx;
    can_rx_result_t    rx_result;

    while ((rx_result = can_rx_get(&rx)) == CAN_RX_SKIP);

    if (rx_result == CAN_RX_EMPTY)
    {
        lua_pushnil(L);
        return 1;
    }

    can_rx_message_type *f = &rx.frame;

    /* id */
    if (f->id_type == CAN_ID_EXTENDED)
        lua_pushinteger(L, (lua_Integer)f->extended_id);
    else
        lua_pushinteger(L, (lua_Integer)f->standard_id);

    /* data as Lua string */
    lua_pushlstring(L, (const char *)f->data, f->dlc);

    /* id_type  (0 = STD, 1 = EXT — matches rotable constants) */
    lua_pushinteger(L, (lua_Integer)f->id_type);

    /* frame_type  (0 = DATA, 1 = REMOTE — matches rotable constants) */
    lua_pushinteger(L, (lua_Integer)f->frame_type);

    /* microsecond timestamp */
    lua_pushinteger(L, (lua_Integer)rx.timestamp_us);

    return 5;
}

/* ------------------------------------------------------------------ */
/* can.tx_state()                                                      */
/*   returns: s0, ts0, s1, ts1, s2, ts2                               */
/*   status values: TX_FAILED=0  TX_OK=1  TX_PENDING=2                */
/* ------------------------------------------------------------------ */
static int lua_can_tx_state(lua_State *L)
{
    can_tx_state_t state;

    if (can_tx_get_state(&state) != RT_EOK)
    {
        lua_pushnil(L);
        return 1;
    }

    for (int i = 0; i < CAN_TX_NUM; i++)
    {
        lua_pushinteger(L, (lua_Integer)state.status[i]);
        lua_pushinteger(L, (lua_Integer)state.timestamp_us[i]);
    }

    return CAN_TX_NUM * 2; /* 6 values */
}

/* ------------------------------------------------------------------ */
/* rotable                                                             */
/* ------------------------------------------------------------------ */
static const rotable_Reg can_lib[] = {
    /* functions */
    {      "speed",    lua_can_speed,                                     0},
    {       "init",     lua_can_init,                                     0},
    {   "transmit", lua_can_transmit,                                     0},
    {    "receive",  lua_can_receive,                                     0},
    {   "tx_state", lua_can_tx_state,                                     0},

    /* id_type constants */
    {     "ID_STD",             NULL,          (lua_Integer)CAN_ID_STANDARD},
    {     "ID_EXT",             NULL,          (lua_Integer)CAN_ID_EXTENDED},

    /* frame_type constants */
    { "FRAME_DATA",             NULL,             (lua_Integer)CAN_TFT_DATA},
    {  "FRAME_RTR",             NULL,           (lua_Integer)CAN_TFT_REMOTE},

    /* mailbox numbers */
    {"TX_MAILBOX0",             NULL,          (lua_Integer)CAN_TX_MAILBOX0},
    {"TX_MAILBOX1",             NULL,          (lua_Integer)CAN_TX_MAILBOX1},
    {"TX_MAILBOX2",             NULL,          (lua_Integer)CAN_TX_MAILBOX2},

    /* transmit status */
    {  "TX_FAILED",             NULL,     (lua_Integer)CAN_TX_STATUS_FAILED},
    {      "TX_OK",             NULL, (lua_Integer)CAN_TX_STATUS_SUCCESSFUL},
    { "TX_PENDING",             NULL,    (lua_Integer)CAN_TX_STATUS_PENDING},
    {"TX_NO_EMPTY",             NULL,   (lua_Integer)CAN_TX_STATUS_NO_EMPTY},

    {         NULL,             NULL,                                     0}  /* sentinel */
};

/* Call from Lua init code: */
int luaopen_can_rotable(lua_State *L)
{
    rotable_newlib(L, can_lib);
    lua_setglobal(L, "can");
    return 0;
}
