/*
 restore saved settings at boot.
 startup debugger in standalone mode, not connected to usb.
 */

#include <rtthread.h>
#include <stdbool.h>
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

#define ATTACH_RETRY_COUNT    5
#define ATTACH_RETRY_DELAY_MS 1000

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

static bool startup_attach_with_retry(void)
{
    for (int attempt = 0; attempt < ATTACH_RETRY_COUNT; attempt++)
    {
        if (startup_attach())
            return true;
        rt_thread_mdelay(ATTACH_RETRY_DELAY_MS);
        LOG_I("attach retry %d/%d", attempt + 1, ATTACH_RETRY_COUNT);
    }

    LOG_E("attach failed");
    return false;
}

static void startup_memwatch()
{
    memcpy(memwatch_table, settings.memwatch_table, sizeof(memwatch_table));
    memwatch_cnt       = settings.memwatch_cnt;
    memwatch_timestamp = settings.memwatch_timestamp;
    LOG_I("memwatch");
}

static void startup_rtt()
{
    rtt_enabled = true;
    rtt_found   = false;
    rt_memset(rtt_channel, 0, sizeof(rtt_channel));
    LOG_I("rtt");
}

static bool target_run()
{
    /* gdb 'run' command */
    if (!cur_target)
    {
        LOG_E("no current target");
        return false;
    }

    target_reset(cur_target);
    target_halt_resume(cur_target, false);
    SET_RUN_STATE(true);
    gdb_target_running = true;
    LOG_I("target running");
    return true;
}

int gdb_startup(void)
{
    bool attached = false;

    if (settings.mode != MODE_GDB_SERVER)
        return RT_EOK;

    /* allow power to settle */
    if (settings.tpower_enable)
    {
        target_power_enable(true);
        rt_thread_mdelay(5000);
    }

    /* attach gdb server */
    if (settings.attach_enable)
        attached = startup_attach_with_retry();

    /* setup rtt */
    if (settings.rtt_enable)
        startup_rtt();

    /* restore memwatch settings */
    if (settings.memwatch_enable)
        startup_memwatch();

    /* gdb "run" command */
    if (attached)
        target_run();

    return RT_EOK;
}

INIT_APP_EXPORT(gdb_startup);

