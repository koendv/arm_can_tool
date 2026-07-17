#include <rtthread.h>
#define DBG_TAG "GDB"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#include "settings.h"
#include "general.h"
#include "platform.h"

#include "gdb_if.h"
#include "gdb_main.h"
#include "target.h"
#include "exception.h"
#include "gdb_packet.h"
#include "memwatch.h"
#include "logger.h"
#include "gdb_server.h"
#include "serials.h"
#include "usb_cdc0.h"
#ifdef ENABLE_RTT
#include "rtt.h"
#endif

#ifndef GDB_STACK_SIZE
#define GDB_STACK_SIZE 4096
#endif

#ifndef GDB_PRIORITY
#define GDB_PRIORITY 20
#endif

#ifndef GDB_TICK
#define GDB_TICK 20
#endif

static uint32_t    gdb_busid         = 0;
static rt_thread_t gdb_server_thread = RT_NULL;
static uint8_t     gdb_write_buffer[HS_PACKET_SIZE - 1] __attribute__((aligned(4)));
static uint32_t    gdb_write_idx          = 0;
static uint32_t    polling_interval_ticks = RT_TICK_PER_SECOND / 20;

/* gdb polling interval */
rt_err_t gdb_set_polling_interval_ms(uint32_t timeout_ms)
{
    if (timeout_ms < 1 || timeout_ms > 500)
        return -RT_ERROR;

    polling_interval_ticks = rt_tick_from_millisecond(timeout_ms);
    return RT_EOK;
}

/* ISR-safe hook to halt running target */
void gdb_target_halt_request(void)
{
    if (serial_event)
        rt_event_send(serial_event, EVENT_MASK_TARGET_HALT_REQUEST); /* schedule halt target later */
}

void gdb_flush(void)
{
    if ((cdc1_dev != NULL) && cdc1_dtr && (gdb_write_idx != 0))
    {
        int32_t written = rt_device_write(cdc1_dev, 0, gdb_write_buffer, gdb_write_idx);
        if (written >= 0)
        {
            if (gdb_write_idx < written)
                written = gdb_write_idx;
            gdb_write_idx -= written;
            if (gdb_write_idx > 0) memmove(gdb_write_buffer, gdb_write_buffer + written, gdb_write_idx);
        }
        else
        {
            LOG_E("gdb write failed");
        }
    }
}

/* write one character to gdb server port. send usb packet if "flush" */
void gdb_if_putchar(char c, bool flush)
{
    static bool logged = false; /* avoid log spam */

    if (cdc1_dev == RT_NULL)
        return;

    gdb_write_buffer[gdb_write_idx++] = c;

    if (flush || gdb_write_idx >= sizeof(gdb_write_buffer))
    {
        if (cdc1_dev != RT_NULL)
            gdb_flush();
        if (gdb_write_idx >= sizeof(gdb_write_buffer))
        {
            if (!logged)
            {
                LOG_E("write error - gdb_write_buffer cleared");
                logged = true;
            }
            gdb_write_idx = 0;
        }
    }
}

/* non-blocking character read */
bool gdb_if_getchar_nonblock(char *c)
{
    if (cdc1_dev == RT_NULL)
        return false;

    return (rt_device_read(cdc1_dev, 0, c, 1) == 1);
}

/* read one character from gdb port, no time-out */
char gdb_if_getchar()
{
    char ch;

    if (cdc1_dev != RT_NULL && (rt_device_read(cdc1_dev, 0, &ch, 1) == 1))
        return ch;
    return '\x04';
}

/* read one character from gdb port with time-out */
char gdb_if_getchar_to(uint32_t timeout_ms)
{
    char ch;

    if (cdc1_dev == RT_NULL)
        return '\x04';

    rt_tick_t start   = rt_tick_get();
    rt_tick_t timeout = rt_tick_from_millisecond(timeout_ms);

    while (1)
    {
        /* try immediate read */
        if (rt_device_read(cdc1_dev, 0, &ch, 1) == 1)
            return ch;

        rt_tick_t now     = rt_tick_get();
        rt_tick_t elapsed = now - start;

        if (elapsed >= timeout)
            break;

        rt_tick_t remaining = timeout - elapsed;

        /* wait for new characters */
        if (rt_event_recv(serial_event,
                          EVENT_MASK_CDC1_RX,
                          RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR,
                          remaining,
                          RT_NULL)
            != RT_EOK)
        {
            /* timed out */
            break;
        }
    }

    return '\x04';
}

static void cdc1_dtr_change()
{
    if (cdc1_dtr) /* new gdb client */
        gdb_packet_reset();
    gdb_write_idx = 0;
    LOG_D("gdb dtr %s", cdc1_dtr ? "on" : "off");
}

static void cdc0_dtr_change()
{
    LOG_I("usb cdc dtr %s", cdc0_dtr ? "on" : "off");
}

static void bmp_loop(void)
{
    uint32_t recv_set;
    int32_t  poll_ticks;
    bool     timed_out;

    if (serial_event == NULL)
    {
        LOG_E("null serial_event");
        rt_thread_delay(RT_TICK_PER_SECOND);
        return;
    }

    while (1)
    {
        if (gdb_target_running && cur_target)
        {
            poll_ticks = polling_interval_ticks;
        }
        else
        {
            poll_ticks = RT_TICK_PER_SECOND /* RT_WAITING_FOREVER */;
        }

        timed_out = false;

        if (rt_event_recv(serial_event,
                          EVENT_MASK_CDC1_DTR
                              | EVENT_MASK_CDC0_DTR
                              | EVENT_MASK_CDC1_RX
                              | EVENT_MASK_CDC0_RX
                              | EVENT_MASK_SERIAL0_RX
                              | EVENT_MASK_SERIAL1_RX
                              | EVENT_MASK_SERIAL2_RX
                              | EVENT_MASK_TARGET_HALT_REQUEST,
                          RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR,
                          poll_ticks,
                          &recv_set)
            != RT_EOK)
        {
            /* timed out */
            recv_set  = 0;
            timed_out = true;
        }

        // if target running, check if user typed ctrl-c
        if (gdb_target_running)
        {
            char c;
            while (gdb_if_getchar_nonblock(&c))
                if (c == '\x03')
                    gdb_halt_target();
        }

        // check if target running
        if (gdb_target_running && cur_target)
        {
            gdb_poll_target();
        }

        // Check again, as `gdb_poll_target()` may
        // alter gdb_target_running and cur_target
        if (gdb_target_running && cur_target)
        {
#ifdef ENABLE_RTT
            if (rtt_enabled)
                poll_rtt(cur_target);
#endif
#ifdef ENABLE_MEMWATCH
            if (memwatch_cnt != 0)
                poll_memwatch(cur_target);
#endif
        }

        if (recv_set & EVENT_MASK_TARGET_HALT_REQUEST)
        {
            if (gdb_target_running && cur_target)
                target_halt_request(cur_target);
        }

        if (recv_set & EVENT_MASK_CDC1_RX)
        {
            gdb_packet_process();
        }

        if (recv_set & EVENT_MASK_CDC0_RX)
        {
            cdc0_receive();
        }

        if (recv_set & EVENT_MASK_SERIAL0_RX)
        {
            serial0_receive();
        }

        if (recv_set & EVENT_MASK_SERIAL1_RX)
        {
            serial1_receive();
        }

        /* uart7/swo has no idle-line interrupt */
        if ((recv_set & EVENT_MASK_SERIAL2_RX) || timed_out)
        {
            serial2_receive();
        }

        if (recv_set & EVENT_MASK_CDC1_DTR)
        {
            cdc1_dtr_change();
        }

        if (recv_set & EVENT_MASK_CDC0_DTR)
        {
            cdc0_dtr_change();
        }

        /* push usb IN */
        cdc0_flush();
        gdb_flush();
    }
}

static void gdb_server_task(void *param)
{
    (void)param;

    platform_init();
    while (1)
    {
        TRY(EXCEPTION_ALL)
        {
            bmp_loop();
        }
        CATCH()
        {
        default:
            gdb_put_packet_error(0xFFU);
            target_list_free();
            cur_target         = NULL;
            gdb_target_running = false;
            SET_RUN_STATE(false);
            gdb_outf("Uncaught exception: %s\n", exception_frame.msg);
        }
    }
}

void gdb_on_configured(uint8_t busid)
{
    gdb_busid = busid;

    serials_init();

    gdb_server_thread = rt_thread_create("gdb_server",
                                         gdb_server_task,
                                         RT_NULL,
                                         GDB_STACK_SIZE,
                                         GDB_PRIORITY,
                                         GDB_TICK);

    if (gdb_server_thread != RT_NULL)
        rt_thread_startup(gdb_server_thread);
}
