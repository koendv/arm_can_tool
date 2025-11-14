/*********************************************************************
*
*       rtt terminal i/o
*
**********************************************************************
*/

#include "rtthread.h"
#include "general.h"
#include "platform.h"

/* rtt input from and output to usb cdc1 */

#include "rtt.h"
#include "rtt_if.h"
#include "usb_desc.h"
#include "usb_cdc.h"

#define RTT_READ_BUF_SIZE 80

static struct rt_ringbuffer *rtt_read_rb = RT_NULL;

/* from usb to rtt */
int32_t rtt_read(uint8_t *buf, uint32_t len)
{
    if (rtt_read_rb == RT_NULL)
        rtt_read_rb = rt_ringbuffer_create(RTT_READ_BUF_SIZE);

    return rt_ringbuffer_put(rtt_read_rb, buf, len);
}

/* rtt host to target: read one character */
int32_t rtt_getchar(const uint32_t channel)
{
    char ch;

    if (channel == 0 && rtt_read_rb && rt_ringbuffer_getchar(rtt_read_rb, &ch))
        return ch;
    return -1;
}

/* rtt host to target: true if no characters available for reading */
bool rtt_nodata(const uint32_t channel)
{
    if (channel == 0 && rtt_read_rb)
        return rt_ringbuffer_data_len(rtt_read_rb) == 0;
    return true;
}

/* rtt target to host: write string */
uint32_t rtt_write(const uint32_t channel, const char *buf, uint32_t len)
{
    cdc1_write((uint8_t *)buf, len);
    return len;
}
