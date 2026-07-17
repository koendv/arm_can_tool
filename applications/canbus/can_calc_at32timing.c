#include <rtthread.h>
#include "can_calc_bittiming.h"
#include "at32f402_405_can.h"
#include "can_calc_at32timing.h"

rt_err_t can_calc_at32_timing(uint32_t bitrate, can_baudrate_type *config)
{
    struct can_bittiming bt;

    if (config == NULL)
        return -RT_ERROR;

    can_baudrate_default_para_init(config);

    rt_memset(&bt, 0, sizeof(bt));
    bt.bitrate      = bitrate;
    bt.sample_point = 0;
    bt.sjw          = 0;

    if (can_calc_bittiming(&bt) != RT_EOK)
    {
        return -RT_ERROR;
    }

    config->baudrate_div = bt.brp;
    config->rsaw_size    = bt.sjw - 1;
    config->bts1_size    = (bt.prop_seg + bt.phase_seg1) - 1;
    config->bts2_size    = bt.phase_seg2 - 1;

    return RT_EOK;
}
