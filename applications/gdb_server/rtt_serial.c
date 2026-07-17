#include <rtthread.h>
#define DBG_TAG "GDB"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#include "settings.h"
#include "general.h"
#include "platform.h"
#include "gdb_main.h"
#include "exception.h"
#include "gdb_packet.h"
#include "gdb_server.h"
#include "rtt_serial.h"

#include "usb_cdc0.h"
#include "rtt.h"

static const uint8_t *rtt_down_buf = NULL;
static uint32_t       rtt_down_len = 0;
static uint32_t       rtt_down_idx = 0;

/* rtt target to host: write string */
uint32_t rtt_write(const uint32_t channel, const char *buf, uint32_t len)
{
    cdc0_write(buf, len);
    return len;
}

/* rtt host to target: read one character */
int32_t rtt_getchar(const uint32_t channel)
{
    if (rtt_down_buf != NULL && rtt_down_idx < rtt_down_len)
        return rtt_down_buf[rtt_down_idx++];
    return -1;
}

/* rtt host to target: true if no characters available for reading */
bool rtt_nodata(const uint32_t channel)
{
    return rtt_down_buf == NULL || rtt_down_idx >= rtt_down_len;
}

/* rtt host to target: send string to target */
void rtt_host_to_target(const uint8_t *buf, uint32_t len)
{
    if (buf == NULL || len == 0)
        return;

    if (!cur_target || !rtt_enabled || !rtt_found || !rtt_cbaddr)
        return;

    rtt_down_buf = buf;
    rtt_down_len = len;
    rtt_down_idx = 0;

    TRY(EXCEPTION_ALL)
    {
        poll_rtt(cur_target);
    }
    CATCH()
    {
    default:
        LOG_I("Uncaught exception: %s\n", exception_frame.msg);
    }

    rtt_down_buf = NULL;
    rtt_down_len = 0;
    rtt_down_idx = 0;
}

