/* This file implements the jtag interface */

#include <rtthread.h>
#define DBG_TAG "JTAG"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#include "tap_common.h"

#include "jtag_tap.h"
#include "adiv5.h"

jtag_proc_s jtag_proc;

OPTIMIZE RAMFUNC void jtagtap_reset(void);
OPTIMIZE RAMFUNC bool jtagtap_next(bool tms, bool tdi);
OPTIMIZE RAMFUNC void jtagtap_tms_seq(uint32_t tms_states, size_t clock_cycles);
OPTIMIZE RAMFUNC void jtagtap_tdi_tdo_seq(uint8_t *data_out, bool final_tms, const uint8_t *data_in, size_t clock_cycles);
OPTIMIZE RAMFUNC void jtagtap_tdi_seq(bool final_tms, const uint8_t *data_in, size_t clock_cycles);
OPTIMIZE RAMFUNC void jtagtap_cycle(bool tms, bool tdi, size_t clock_cycles);

void jtagtap_init(void)
{
    jtagtap_platform_init();

    /* tms, tck are now in output mode */

    jtag_proc.jtagtap_reset       = jtagtap_reset;
    jtag_proc.jtagtap_next        = jtagtap_next;
    jtag_proc.jtagtap_tms_seq     = jtagtap_tms_seq;
    jtag_proc.jtagtap_tdi_tdo_seq = jtagtap_tdi_tdo_seq;
    jtag_proc.jtagtap_tdi_seq     = jtagtap_tdi_seq;
    jtag_proc.jtagtap_cycle       = jtagtap_cycle;
    jtag_proc.tap_idle_cycles     = 1;

    /* Ensure we're in JTAG mode. Start by issuing a complete SWD reset of at least 50 reset cycles */
    jtagtap_cycle(true, false, 51U);
    /* Having achieved reset, try the deprecated 16-bit SWD-to-JTAG sequence */
    jtagtap_tms_seq(ADIV5_SWD_TO_JTAG_SELECT_SEQUENCE, 16U);
    /* Next, to complete that sequence, do a full 50+ cycle reset again */
    jtagtap_cycle(true, false, 51U);
    /*
	 * For parts that implement the old sequence, we're done.. however, for parts that do not, we
	 * now need to do SWD-to-Dormant-State
	 */
    jtagtap_tms_seq(ADIV5_SWD_TO_DORMANT_SEQUENCE, 16U);
    /* Having achieved this state, we now have to signal we want to change states with the alert sequence */
    jtagtap_tms_seq(0xffU, 8U); /* 8 reset cycles used to ensure the target's in a happy place */
    /* 128-bit Selection Alert sequence */
    jtagtap_tms_seq(ADIV5_SELECTION_ALERT_SEQUENCE_0, 32U);
    jtagtap_tms_seq(ADIV5_SELECTION_ALERT_SEQUENCE_1, 32U);
    jtagtap_tms_seq(ADIV5_SELECTION_ALERT_SEQUENCE_2, 32U);
    jtagtap_tms_seq(ADIV5_SELECTION_ALERT_SEQUENCE_3, 32U);
    /*
	 * Now ask for JTAG please
	 * We combine the last two sequences in a single jtagtap_tms_seq as an optimization
	 *
	 * Send 4 SWCLKTCK cycles with SWDIOTMS LOW
	 * Send the required 8 bit activation code sequence on SWDIOTMS
	 *
	 * The bits are shifted out to the right, so we shift the second sequence left by the size of the first sequence
	 * The first sequence is 4 bits and the second 8 bits, totaling 12 bits in the combined sequence
	 */
    jtagtap_tms_seq(ADIV5_ACTIVATION_CODE_ARM_JTAG_DP << 4U, 12U);
    /* At this point we are definitely in JTAG mode - let the scan logic reset the state machine into a good state. */
}

OPTIMIZE RAMFUNC void jtagtap_reset(void)
{
    jtagtap_soft_reset();
}

static bool INLINE OPTIMIZE RAMFUNC jtagtap_next_delay(const bool tms, const bool tdi)
{
    TMS_SET(tms);
    TDI_SET(tdi);
    TCK_HIGH();
    TCK_DELAY();
    const uint16_t result = TDO_GET();
    TCK_LOW();
    TCK_DELAY();
    return result != 0;
}

bool OPTIMIZE RAMFUNC jtagtap_next(const bool tms, const bool tdi)
{
    INTERRUPTS_OFF();
    bool retval = jtagtap_next_delay(tms, tdi);
    INTERRUPTS_ON();
    return retval;
}

void OPTIMIZE RAMFUNC jtagtap_tms_seq(uint32_t tms_states, const size_t clock_cycles)
{
    INTERRUPTS_OFF();
    TDI_HIGH();
    for (size_t cycle = 0; cycle < clock_cycles; ++cycle)
    {
        const bool state = tms_states & 1U;
        TMS_SET(state);
        TCK_HIGH();
        TCK_DELAY();
        tms_states >>= 1U;
        TCK_LOW();
        TCK_DELAY();
    }
    INTERRUPTS_ON();
}

void OPTIMIZE RAMFUNC jtagtap_tdi_tdo_seq(uint8_t * const data_out, const bool final_tms, const uint8_t * const data_in, size_t clock_cycles)
{
    INTERRUPTS_OFF();
    uint8_t value = 0;
    for (size_t cycle = 0; cycle < clock_cycles; ++cycle)
    {
        /* Calculate the next bit and byte to consume data from */
        const uint8_t bit  = cycle & 7U;
        const size_t  byte = cycle >> 3U;
        /* On the last cycle, assert final_tms to TMS_PIN */
        TMS_SET(cycle + 1U >= clock_cycles && final_tms);
        /* Set up the TDI pin and start the clock cycle */
        TDI_SET(data_in[byte] & (1U << bit));
        /* Start the clock cycle */
        TCK_HIGH();
        TCK_DELAY();
        /* If TDO is high, store a 1 in the appropriate position in the value being accumulated */
        if (TDO_GET())
            value |= 1U << bit;
        if (bit == 7U)
        {
            if (data_out)
                data_out[byte] = value;
            value = 0;
        }
        /* Finish the clock cycle */
        TCK_LOW();
        TCK_DELAY();
    }
    /* If clock_cycles is not divisible by 8, we have some extra data to write back here. */
    if (clock_cycles & 7U)
    {
        const size_t byte = (clock_cycles - 1U) >> 3U;
        if (data_out)
            data_out[byte] = value;
    }
    INTERRUPTS_ON();
}


void OPTIMIZE RAMFUNC jtagtap_tdi_seq(const bool final_tms, const uint8_t * const data_in, const size_t clock_cycles)
{
    INTERRUPTS_OFF();
    TMS_LOW();
    for (size_t cycle = 0; cycle < clock_cycles; ++cycle)
    {
        const uint8_t bit  = cycle & 7U;
        const size_t  byte = cycle >> 3U;
        /* On the last tick, assert final_tms to TMS_PIN */
        TMS_SET(cycle + 1U >= clock_cycles && final_tms);
        /* Set up the TDI pin and start the clock cycle */
        TDI_SET(data_in[byte] & (1U << bit));
        TCK_HIGH();
        TCK_DELAY();
        /* Finish the clock cycle */
        TCK_LOW();
        TCK_DELAY();
    }
    INTERRUPTS_ON();
}

void OPTIMIZE RAMFUNC jtagtap_cycle(const bool tms, const bool tdi, const size_t clock_cycles)
{
    INTERRUPTS_OFF();
    jtagtap_next_delay(tms, tdi);

    for (size_t cycle = 0; cycle < clock_cycles; ++cycle)
    {
        TCK_HIGH();
        TCK_DELAY();
        TCK_LOW();
        TCK_DELAY();
    }
    INTERRUPTS_ON();
}
