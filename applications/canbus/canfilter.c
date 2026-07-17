#include <rtthread.h>
#define DBG_TAG "BXCAN"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>
#include <at32f402_405_can.h>
#include "canfilter.h"

#define BXCAN_FILTER_MASK (0x3FFFU) /* 14 filter banks */

/* pass-all filter for bxcan */
const struct canfilter_bxcan_f0 canfilter_bxcan_f0_pass_all = {
    .dev   = CANFILTER_DEV_BXCAN_F0,
    .fs1r  = 0x1,
    .fm1r  = 0x0,
    .ffa1r = 0x0,
    .fa1r  = 0x3,
    .fr1   = {[0] = 0x00000004},
    .fr2   = {[0] = 0x00000000},
};

static can_type * const bxcan = (can_type *)CAN1; /* at32 HAL: canbus controller memory address */

rt_err_t bxcan_set_filter(struct canfilter_bxcan_f0 bxcan_filter)
{
    /* bxcan filter configuration process */
    if (!bxcan)
        return -RT_ERROR;

    // Enter filter initialization mode
    // stm32: FMR at32: fctrl
    bxcan->fctrl |= 0x1U;

    // Disable Filters
    // stm32: FA1R at32: facfg
    bxcan->facfg = 0x0U;

    // Configure filter scale (32-bit vs 16-bit)
    // stm32: FS1R at32: fbwcfg
    bxcan->fbwcfg = bxcan_filter.fs1r & BXCAN_FILTER_MASK;

    // Configure filter mode (mask vs list)
    // stm32: FM1R at32: fmcfg
    bxcan->fmcfg = bxcan_filter.fm1r & BXCAN_FILTER_MASK;

    // Configure filter FIFO assignment
    // stm32: FFA1R at32: frf
    bxcan->frf = bxcan_filter.ffa1r & BXCAN_FILTER_MASK;

    // Configure filter bank registers
    // stm32: FR1, FR2 at32: ffdb1, ffdb2
    for (uint32_t bank = 0; bank < 14; bank++)
    {
        bxcan->ffb[bank].ffdb1 = bxcan_filter.fr1[bank];
        bxcan->ffb[bank].ffdb2 = bxcan_filter.fr2[bank];
    }

    // Activate filters
    // stm32: FA1R at32: facfg
    bxcan->facfg = bxcan_filter.fa1r & BXCAN_FILTER_MASK;

    // Exit filter initialization mode
    bxcan->fctrl &= ~0x1U; // Clear bit 0 (FCS/FINIT equivalent)

    return RT_EOK;
}

void bxcan_filter_pass_all(void)
{
    can_filter_init_type filter_conf;

    /* Fill struct with HAL defaults (id=0, mask=0, mode=ID_MASK, fifo=0) */
    can_filter_default_para_init(&filter_conf);

    /* Select filter bank 0 */
    filter_conf.filter_number = 0;

    /* Use 32-bit mask mode — id=0 / mask=0 means no bits are checked: pass all */
    filter_conf.filter_bit       = CAN_FILTER_32BIT;
    filter_conf.filter_mode      = CAN_FILTER_MODE_ID_MASK;
    filter_conf.filter_id_high   = 0x0000;
    filter_conf.filter_id_low    = 0x0000;
    filter_conf.filter_mask_high = 0x0000;
    filter_conf.filter_mask_low  = 0x0000;

    /* Route matching frames to RX FIFO 0 */
    filter_conf.filter_fifo = CAN_FILTER_FIFO0;

    /* Activate the filter bank */
    filter_conf.filter_activate_enable = TRUE;

    /* Write to hardware */
    can_filter_init(CAN1, &filter_conf);

    LOG_I("pass all");
}
