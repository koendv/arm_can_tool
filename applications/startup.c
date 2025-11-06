#include <rtthread.h>
#include <stdbool.h>
#include "lua_script.h"
#include "general.h"
#include "exception.h"
#include "gdb_main.h"
#include "rtt.h"
#include "memwatch.h"
#include "platform.h"
#include "settings.h"

#define DBG_TAG "STARTUP"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

static bool startup_attach()
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
        LOG_E("%s", msg);
        return false;
    }

    platform_target_clk_output_enable(false);

    /* Attach to remote target processor */
    extern target_controller_s gdb_controller;
    cur_target = target_attach_n(1, &gdb_controller);
    if (!cur_target)
    {
        LOG_E("attach failed");
        return false;
    }

    LOG_I("attached");
    return true;
}

static void startup_memwatch()
{
    memcpy(memwatch_table, settings.memwatch_table, sizeof(memwatch_table));
    memwatch_cnt       = settings.memwatch_cnt;
    memwatch_timestamp = settings.memwatch_timestamp;
}

static void startup_rtt()
{
    rtt_enabled = true;
    rtt_found   = false;
    rt_memset(rtt_channel, 0, sizeof(rtt_channel));
}

int startup(void)
{
    /* allow power to settle */
    if (settings.tpower_enable)
    {
        target_power_enable(true);
        rt_thread_mdelay(5000);
    }
    /* attach gdb server */
    if (settings.attach_enable)
        startup_attach();
    /* setup rtt */
    if (settings.rtt_enable)
        startup_rtt();
    /* restore memwatch settings */
    if (settings.memwatch_enable)
        startup_memwatch();
    /* run lua autoexec script */
    if (settings.lua_enable)
        lua_autoexec();
    return RT_EOK;
}

//XXX INIT_APP_EXPORT(startup);

