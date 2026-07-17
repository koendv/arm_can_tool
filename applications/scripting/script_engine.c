#include <rtthread.h>
#define DBG_TAG "LUA"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>
#include <dfs_file.h>

#include "canbus.h"
#include "canbus_event.h"
#include "usb_desc.h"
#include "usb_cdc0.h"
#include "serials.h"
#include "settings.h"
#include "script_engine.h"

#include "general.h"
#include "platform.h"
#include "gdb_if.h"
#include "gdb_main.h"
#include "target.h"
#include "rtt.h"
#include "memwatch.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <lua_local.h>
#include <lua_completion.h>
#include <lua_lfs.h>
#include <lua_flash.h>

#include <microrl.h>

#define FLASH_RECEIVE_TIMEOUT_S 30 /* seconds */

static uint32_t    lua_busid              = 0;
static rt_thread_t lua_thread             = RT_NULL;
static uint32_t    polling_interval_ticks = RT_TICK_PER_SECOND / 20;
static bool        target_running         = false;
static int         lua_event_ref[EVENT_MAX]; // references to lua event handlers
lua_State         *L = RT_NULL;
static microrl_t   rl;

/* lua shell input */

static void cdc1_dtr_change(void)
{
    if (cdc1_dtr)
        microrl_redraw_terminal(&rl);
}

static void cdc1_receive(void)
{
    char ch;
    while (rt_device_read(cdc1_dev, 0, &ch, 1) == 1)
    {
        if (lfs_receiving)
            lfs_receive(ch);
        else
            microrl_processing_input(&rl, &ch, 1);
    }
}

/* register lua events */

static void lua_register_events(void)
{
    uint32_t handler_count = 0;

    /* initialize array */
    for (int i = 0; i < EVENT_MAX; i++)
        lua_event_ref[i] = LUA_NOREF;

    /* get lua event handler table */
    lua_getglobal(L, "event_handler");
    if (!lua_istable(L, -1))
    {
        LOG_D("no event_handler table");
        lua_pop(L, 1);
        return;
    }

    for (int i = 0; i < EVENT_MAX; i++)
    {
        lua_rawgeti(L, -1, i);
        if (lua_isnil(L, -1))
        {
            /* leave lua_event_ref[i] as LUA_NOREF */
            lua_pop(L, 1);
            continue;
        }
        lua_event_ref[i] = luaL_ref(L, LUA_REGISTRYINDEX);
        handler_count++;
    }

    lua_pop(L, 1); /* pop event_handler table */

    LOG_I("%d event handlers", handler_count);
}

static void inline run_event_handler(int evt)
{
    if (lua_event_ref[evt] != LUA_NOREF && lua_event_ref[evt] != LUA_REFNIL)
    {
        int top_before = lua_gettop(L);
        lua_rawgeti(L, LUA_REGISTRYINDEX, lua_event_ref[evt]);
        if (lua_pcall(L, 0, 0, 0) != LUA_OK)
        {
            const char *err = lua_tostring(L, -1);
            LOG_E("%s", err);
        }
        int leaked = lua_gettop(L) - top_before;
        if (leaked != 0)
        {
            LOG_W("event %d: handler leaked stack", evt);
            lua_settop(L, top_before);
        }
    }
}

/* lua rt-thread thread */

static void lua_task(void *param)
{
    uint32_t recv_set;
    int32_t  poll_ticks;
    rt_err_t err;

    (void)param;

    if (serial_event == NULL)
    {
        LOG_E("null serial_event");
        return;
    }

    platform_init();

    /* run autoexec script */
    if (settings.lua_autoexec)
        lfs_run_autoexec(L);

    /* register lua event handlers */
    lua_register_events();

    /* garbage collection */
    lua_gc(L, LUA_GCCOLLECT, 0);

    while (1)
    {
        if (lfs_receiving)
            poll_ticks = RT_TICK_PER_SECOND * FLASH_RECEIVE_TIMEOUT_S;
        else if (gdb_target_running && cur_target)
        {
            poll_ticks = polling_interval_ticks;
        }
        else
        {
            poll_ticks = RT_WAITING_FOREVER;
        }

        err = rt_event_recv(serial_event,
                            EVENT_MASK_CDC0_DTR
                                | EVENT_MASK_CDC1_DTR
                                | EVENT_MASK_CDC0_RX
                                | EVENT_MASK_CDC1_RX
                                | EVENT_MASK_SERIAL0_RX
                                | EVENT_MASK_SERIAL1_RX
                                | EVENT_MASK_SERIAL2_RX
                                | EVENT_MASK_TARGET_HALT_REQUEST
                                | EVENT_MASK_CAN1_TX_DONE
                                | EVENT_MASK_CAN1_RX0_INDIC
                                | EVENT_MASK_CAN1_BUS_OFF
                                | EVENT_MASK_CAN1_RX_OVERFLOW
                                | EVENT_MASK_CAN1_TX_OVERFLOW,
                            RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR,
                            poll_ticks,
                            &recv_set);

        if (err != RT_EOK)
        {
            recv_set = 0;
            if (lfs_receiving)
            {
                const char msg[] = "\r\nflash loader timeout, aborted";
                cdc1_write(msg, strlen(msg));
                LOG_E(msg + 2);
                lfs_receiving = false;
            }
        }

        if (gdb_target_running && cur_target)
        {
            gdb_poll_target();
        }

        // Check again, as `gdb_poll_target()` may
        // alter gdb_target_running and cur_target
        if (gdb_target_running && cur_target)
        {
            if (rtt_enabled)
                poll_rtt(cur_target);
            if (memwatch_cnt != 0)
                poll_memwatch(cur_target);
        }

        if (target_running && !gdb_target_running && cur_target)
        {
            /* tell lua target is halted */
            run_event_handler(EVENT_TARGET_HALTED);
        }
        target_running = gdb_target_running && cur_target;

        if (recv_set & EVENT_MASK_TARGET_HALT_REQUEST)
        {
            /* ask gdb server to halt target */
            if (gdb_target_running && cur_target)
                target_halt_request(cur_target);
        }

        /* event handlers in lua */
        for (int evt = 0; evt < EVENT_MAX; evt++)
        {
            if (recv_set & (0x1 << evt))
                run_event_handler(evt);
        }

        /* lua shell */
        if (recv_set & EVENT_MASK_CDC1_RX)
        {
            cdc1_receive();
        }

        if (recv_set & EVENT_MASK_CDC1_DTR)
        {
            cdc1_dtr_change();
        }

        /* hardware serials */
        if (recv_set & EVENT_MASK_SERIAL0_RX)
        {
            serial0_receive();
        }

        if (recv_set & EVENT_MASK_SERIAL1_RX)
        {
            serial1_receive();
        }

        if (recv_set & EVENT_MASK_SERIAL2_RX)
        {
            serial2_receive();
        }

        cdc0_flush();

        /* step garbage collection while idle, to avoid having to do garbage collection inside a handler */
        lua_gc(L, LUA_GCSTEP, 0);
    }
}

/* console output */

/* micro-readline library */

int microrl_print(microrl_t *mrl, const char *str)
{
    (void)mrl;
    cdc1_write(str, strlen(str));
    return 0;
}

static int microrl_execute(microrl_t *mrl, const char *buf, uint32_t buflen)
{
    char tmp[MICRORL_CFG_CMDLINE_LEN + 8];

    if (buf[0] == '=')
    {
        memcpy(tmp, "return ", 7);
        memcpy(tmp + 7, buf + 1, buflen); // includes terminating nul.
        buf = tmp;
    }

    if (luaL_loadstring(L, buf) == LUA_OK)
        lua_pcall(L, 0, LUA_MULTRET, 0);

    if (lua_gettop(L) > 0)
        lua_print(L);

    lua_settop(L, 0);

    return 0;
}

static int lua_panic(lua_State *L)
{
    LOG_E("PANIC: %s\n", lua_tostring(L, -1));
    return 0;
}

void script_on_configured(uint8_t busid)
{
    lua_busid = busid;

    /* cdc0 and cdc1 */
    usb_serials_init();
}

void script_init()
{
    /* hardware serials */
    hardware_serials_init();

    /* configure canbus device */
    can_gpio_config();

    if (can_configure_device() != RT_EOK)
    {
        LOG_E("canbus init fail");
        return;
    }

    /* set up canbus interrupts  */
    if (canbus_event_init() != RT_EOK)
    {
        LOG_E("canbus event init fail");
        return;
    }

    /* initialize readline library */
    microrl_init(&rl, microrl_print, microrl_execute);

    /* tab completion */
    microrl_set_complete_callback(&rl, microrl_get_completion);

    /* allocate lua heap */
    if (lua_heap_init())
    {
        LOG_E("heap init fail");
        return;
    }

    /* initialize lua */
    L = lua_newstate(lua_alloc, NULL, time(NULL));

    if (!L)
    {
        LOG_E("lua_newstate failed");
        return;
    }

    /* panic handler */
    lua_atpanic(L, lua_panic);

    /* standard libraries */
    //luaopen_base(L); /* ram */
    luaopen_base_rotable(L); /* flash */
    //luaopen_package(L);
    //luaopen_coroutine(L);
    luaopen_coroutine_rotable(L);
    //luaopen_debug(L);
    //luaopen_io(L);
    //luaopen_math(L);
    luaopen_math_rotable(L);
    //luaopen_os(L);
    //luaopen_string(L);
    luaopen_string_rotable(L);
    //luaopen_table(L);
    luaopen_table_rotable(L);
    //luaopen_utf8(L);
    luaopen_utf8_rotable(L);

    luaopen_bmd_rotable(L);
    luaopen_dap_rotable(L);
    luaopen_can_rotable(L);
    luaopen_sys_rotable(L);
    luaopen_flash_rotable(L);

    /* start up thread */
    lua_thread = rt_thread_create("lua",
                                  lua_task,
                                  RT_NULL,
                                  LUA_STACK_SIZE,
                                  LUA_PRIORITY,
                                  LUA_TICK);

    if (lua_thread != RT_NULL)
        rt_thread_startup(lua_thread);

    LOG_I("heap %d kbyte", LUA_HEAP_SIZE >> 10);

    return;
}
