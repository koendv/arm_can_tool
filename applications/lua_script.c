#include <rtthread.h>
#include <dfs_file.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <sys/stat.h>

#define DBG_TAG "LUA"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#include "lua_script.h"

#define LUA_AUTOEXEC_FILE       "/sdcard/autoexec.lua"
#define LUA_AUTOEXEC_STACK_SIZE 10240
#define LUA_AUTOEXEC_PRIORITY   20
#define LUA_AUTOEXEC_TIMESLICE  5

static rt_thread_t lua_autoexec_thread = RT_NULL;

/* run lua script from file */
int lua_script(const char *filename)
{
    struct stat file_stat;

    // check file exists
    if (dfs_file_stat(filename, &file_stat) != 0 || !S_ISREG(file_stat.st_mode))
    {
        LOG_E("script %s not found", filename);
        return -RT_ERROR;
    }

    // create lua state
    lua_State *L = luaL_newstate();
    if (!L)
    {
        LOG_E("failed to create lua state");
        return -RT_ERROR;
    }

    // load standard lua libraries
    luaL_openlibs(L);

    // load script file
    if (luaL_loadfile(L, filename) != LUA_OK)
    {
        LOG_E("load error: %s", lua_tostring(L, -1));
        lua_pop(L, 1);
        lua_close(L);
        return -RT_ERROR;
    }

    // execute
    if (lua_pcall(L, 0, LUA_MULTRET, 0) != LUA_OK)
    {
        LOG_E("exec error: %s", lua_tostring(L, -1));
        lua_pop(L, 1);
        lua_close(L);
        return -RT_ERROR;
    }

    LOG_I("lua script %s success", filename);

    // garbage collection and close
    lua_gc(L, LUA_GCCOLLECT, 0);
    lua_close(L);
    return RT_EOK;
}


/* thread to run autoexec script */
static void lua_autoexec_thread_entry(void *parameter)
{
    (void)parameter;

    LOG_I(LUA_AUTOEXEC_FILE " start");
    int ret = lua_script(LUA_AUTOEXEC_FILE);
    if (ret != RT_EOK)
    {
        LOG_E(LUA_AUTOEXEC_FILE " fail");
    }
    else
    {
        LOG_I(LUA_AUTOEXEC_FILE " success");
    }

    lua_autoexec_thread = RT_NULL;
}

/* run lua autoexec script in background */
int lua_autoexec(void)
{
    if (lua_autoexec_thread != RT_NULL)
    {
        LOG_W("lua_autoexec thread already running");
        return -RT_ERROR;
    }

    lua_autoexec_thread = rt_thread_create("lua_autoexec",
                                           lua_autoexec_thread_entry,
                                           RT_NULL,
                                           LUA_AUTOEXEC_STACK_SIZE,
                                           LUA_AUTOEXEC_PRIORITY,
                                           LUA_AUTOEXEC_TIMESLICE);
    if (lua_autoexec_thread == RT_NULL)
    {
        LOG_E("Failed to create lua_autoexec thread");
        return -RT_ERROR;
    }

    rt_thread_startup(lua_autoexec_thread);
    return RT_EOK;
}

/* shell command to run lua autoexec script in background */
static void lua_autoexec_cmd(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    int ret = lua_autoexec();
    if (ret == RT_EOK)
    {
        LOG_I("lua_autoexec start");
    }
    else
    {
        LOG_E("lua_autoexec fail");
    }
}
MSH_CMD_EXPORT_ALIAS(lua_autoexec_cmd, lua_autoexec, run lua autoexec script in background);

/* shell command to run lua script */
static void lua_run_cmd(int argc, char **argv)
{
    if (argc < 2)
    {
        rt_kprintf("Usage: lua_run <script_path>\n");
        return;
    }

    const char *filename = argv[1];
    int         ret      = lua_script(filename);
    if (ret == RT_EOK)
    {
        rt_kprintf("%s success\n", filename);
    }
    else
    {
        rt_kprintf("%s fail\n", filename);
    }
}
MSH_CMD_EXPORT_ALIAS(lua_run_cmd, lua_run, run lua script from shell);

