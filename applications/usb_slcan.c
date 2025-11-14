#include <ctype.h>
#include "usb_slcan.h"
#include "slcan.h"

#define DBG_TAG "SLCAN"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#define SLCAN_LINE_MAX 32

/* split usb packet into slcan commands */

void slcan_process(uint8_t *buf, uint32_t len)
{
    static char     slcan_buffer[SLCAN_LINE_MAX];
    static uint32_t slcan_pos = 0;

    for (uint32_t i = 0; i < len; i++)
    {
        char c = buf[i];

        if (c == '\r' || c == '\n')
        {
            if (slcan_pos > 0 && slcan_pos < SLCAN_LINE_MAX)
            {
                slcan_buffer[slcan_pos] = '\0';
                LOG_D("cmd: %s", slcan_buffer);
                slcan_parse_str((uint8_t *)slcan_buffer, slcan_pos);
            }
            else if (slcan_pos >= SLCAN_LINE_MAX)
                LOG_E("buffer overflow");
            slcan_pos = 0;
        }
        else if (!isprint(c))
        {
            slcan_pos = SLCAN_LINE_MAX;
        }
        else if (slcan_pos < SLCAN_LINE_MAX - 1)
        {
            slcan_buffer[slcan_pos++] = c;
        }
        else
        {
            slcan_pos = SLCAN_LINE_MAX;
        }
    }
}
