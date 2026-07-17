#include <rtthread.h>
#include "canbus_event.h"
#include "candump.h"
#include "timestamp_us.h"
#include "usb_cdc0.h"

#define CANDUMP_MTU (64)

static inline void u32_to_hex(char *dst, uint32_t value, uint32_t digits)
{
    static const char hex[] = "0123456789ABCDEF";

    // from right to left
    for (unsigned i = 0; i < digits; i++)
    {
        dst[digits - 1 - i]   = hex[value & 0xF];
        value               >>= 4;
    }
}

void candump_frame(const can_stored_frame_t *frame)
{
    if (!frame)
        return;

    char buf[CANDUMP_MTU + 2];

    /* DWT->CYCCNT wraps every ~20s. Use rt_tick_get() to determine DWT cycle */
    int64_t wrap_period_us = (int64_t)timestamp_us_scale_factor;
    int64_t raw            = (int64_t)frame->timestamp_us;
    int64_t tick_us        = (int64_t)rt_tick_get() * (1000000LL / RT_TICK_PER_SECOND);
    int64_t diff           = tick_us - raw;
    int64_t lap_base       = ((diff + wrap_period_us / 2) / wrap_period_us) * wrap_period_us;

    int64_t usec = lap_base + raw;
    long    sec  = (long)(usec / 1000000LL);
    long    frac = (long)(usec % 1000000LL);

    uint32_t pos = rt_snprintf(buf, CANDUMP_MTU, "(%ld.%06ld) can0 ", sec, frac);

    uint32_t id_len = (frame->frame.id_type == CAN_ID_STANDARD) ? 3 : 8;
    uint32_t id     = (frame->frame.id_type == CAN_ID_STANDARD) ? frame->frame.standard_id : frame->frame.extended_id;

    u32_to_hex(buf + pos, id, id_len);
    pos        += id_len;
    buf[pos++]  = '#';

    /* frame.dlc is the raw hardware DLC (0-15, straight from the 4-bit controller register).
       disp_len is the actual byte count. */

    uint32_t dlc      = frame->frame.dlc;
    uint32_t disp_len = (dlc > 8) ? 8 : dlc;

    if (frame->frame.frame_type == CAN_TFT_REMOTE)
    {
        buf[pos++] = 'R';
        if (disp_len > 0)
        {
            u32_to_hex(buf + pos, disp_len, 1);
            pos += 1;
        }
    }
    else
    {
        for (uint32_t i = 0; i < disp_len; i++)
        {
            u32_to_hex(buf + pos, frame->frame.data[i], 2);
            pos += 2;
        }
    }

    /* a dlc above 8 is a classic-CAN controller reporting more bytes than possible; log */

    if (disp_len == 8 && dlc > 8)
    {
        buf[pos++] = '_';
        u32_to_hex(buf + pos, dlc, 1);
        pos += 1;
    }

    buf[pos++] = '\r';
    buf[pos++] = '\n';

    cdc0_write(buf, pos);
}
