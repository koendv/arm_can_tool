#include <rtthread.h>
#define DBG_TAG "CAN"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>
#include <finsh.h>
#include "at32f402_405.h"
#include "gsusb.h"
#include "canbus_event.h"
#include "timestamp_us.h"

/* handle canbus interrupts as rt-thread events */

can_ringbuffer_t can_rx_buffer = {0};
can_tx_state_t   can_tx_state  = {0};

void CAN1_TX_IRQHandler(void)
{
    uint32_t events    = 0;
    uint32_t timestamp = get_timestamp_us();

    LOG_D("CAN TX");

    if (can_flag_get(CAN1, CAN_TM0TCF_FLAG) == SET)
    {
        can_tx_state.timestamp_us[0]  = timestamp;
        can_tx_state.status[0]        = can_transmit_status_get(CAN1, CAN_TX_MAILBOX0);
        events                       |= EVENT_MASK_CAN1_TX_DONE;
        /* clear flag */
        can_flag_clear(CAN1, CAN_TM0TCF_FLAG);
    }

    if (can_flag_get(CAN1, CAN_TM1TCF_FLAG) == SET)
    {
        can_tx_state.timestamp_us[1]  = timestamp;
        can_tx_state.status[1]        = can_transmit_status_get(CAN1, CAN_TX_MAILBOX1);
        events                       |= EVENT_MASK_CAN1_TX_DONE;
        /* clear flag */
        can_flag_clear(CAN1, CAN_TM1TCF_FLAG);
    }

    if (can_flag_get(CAN1, CAN_TM2TCF_FLAG) == SET)
    {
        can_tx_state.timestamp_us[2]  = timestamp;
        can_tx_state.status[2]        = can_transmit_status_get(CAN1, CAN_TX_MAILBOX2);
        events                       |= EVENT_MASK_CAN1_TX_DONE;
        /* clear flag */
        can_flag_clear(CAN1, CAN_TM2TCF_FLAG);
    }
    if (serial_event && events)
    {
        __DSB();
        rt_event_send(serial_event, events);
    }
}

/* bxcan fifo0 receive interrupt */
void CAN1_RX0_IRQHandler(void)
{
    uint32_t events       = 0;
    uint32_t current_head = can_rx_buffer.head;

    LOG_D("CAN RX");

    if (can_interrupt_flag_get(CAN1, CAN_RF0MN_FLAG) != RESET)
    {
        while (can_receive_message_pending_get(CAN1, CAN_RX_FIFO0))
        {
            uint32_t next_slot = (current_head + 1) & (CAN_RX_NUM - 1);
            if (next_slot == can_rx_buffer.tail)
            {
                can_receive_fifo_release(CAN1, CAN_RX_FIFO0);
                /* count of dropped packets may be inaccurate if multiple packets in fifo */
                can_rx_buffer.dropped_cnt++;
                events |= EVENT_MASK_CAN1_RX_OVERFLOW;
                continue;
            }

            can_message_receive(CAN1, CAN_RX_FIFO0, &can_rx_buffer.frames[current_head].frame);
            can_rx_buffer.frames[current_head].timestamp_us = get_timestamp_us();
            can_rx_buffer.received_cnt++;
            events |= EVENT_MASK_CAN1_RX0_INDIC;

            current_head = next_slot;
        }
        __DMB();
        can_rx_buffer.head = current_head;
        if (serial_event && events)
            rt_event_send(serial_event, events);
    }
}

/* bxcan error handler interrupt */
void CAN1_SE_IRQHandler(void)
{
    uint32_t events = 0;
    if (can_interrupt_flag_get(CAN1, CAN_BOF_FLAG) != RESET)
    {
        can_flag_clear(CAN1, CAN_BOF_FLAG);
        events |= EVENT_MASK_CAN1_BUS_OFF;
    }
    if (serial_event && events)
        rt_event_send(serial_event, events);
}

/* canbus receive hook - override with handler */
__attribute__((weak)) bool can_rx_consumed(can_stored_frame_t *stored_frame)
{
    (void)stored_frame;
    return false; /* default: claim nothing */
}

/* receive canbus frame */
can_rx_result_t can_rx_get(can_stored_frame_t *stored_frame)
{
    uint32_t tail = can_rx_buffer.tail;
    uint32_t head = can_rx_buffer.head;

    if (!stored_frame)
        return CAN_RX_EMPTY;
    if (tail == head)
        return CAN_RX_EMPTY;
    rt_memcpy(stored_frame, &can_rx_buffer.frames[tail], sizeof(can_stored_frame_t));
    can_rx_buffer.tail = (tail + 1) & (CAN_RX_NUM - 1);
    return can_rx_consumed(stored_frame) ? CAN_RX_SKIP : CAN_RX_OK;
}

/* get state of all 3 tx mailboxes */
rt_err_t can_tx_get_state(can_tx_state_t *tx_state_dest)
{
    if (!tx_state_dest)
        return -RT_EINVAL;

    rt_base_t level = rt_hw_interrupt_disable();

    /* bulk copy the entire TX state array */
    rt_memcpy(tx_state_dest, &can_tx_state, sizeof(can_tx_state));

    /* reset all data */
    for (uint32_t i = 0; i < CAN_TX_NUM; i++)
    {
        can_tx_state.timestamp_us[i] = 0;
        can_tx_state.status[i]       = CAN_TX_STATUS_PENDING;
    }

    rt_hw_interrupt_enable(level);

    return RT_EOK;
}

int cmd_canstat(int argc, void **argv)
{
    rt_kprintf("CAN1\r\n");
    rt_kprintf("hardware receive error:  %d\r\n", can_receive_error_counter_get(CAN1));
    rt_kprintf("hardware transmit error: %d\r\n", can_transmit_error_counter_get(CAN1));
    rt_kprintf("receive count:   %d\r\n", can_rx_buffer.received_cnt); /* received in software ringbuffer */
    rt_kprintf("receive dropped: %d\r\n", can_rx_buffer.dropped_cnt);  /* software rx ringbuffer full */
    rt_kprintf("transmit count:  %d\r\n", transmit_count);             /* hardware confirms transmission success */
    rt_kprintf("transmit errors: %d\r\n", transmit_errors);            /* hardware confirms transmission fail */
    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(cmd_canstat, canstat, stat can device status);

rt_err_t canbus_event_init(void)
{
    /* Force compile error if ringbuffer size not power of 2 */
    RT_ASSERT((CAN_RX_NUM > 0) && ((CAN_RX_NUM & (CAN_RX_NUM - 1)) == 0));

    /* set up canbus receive ringbuffer */
    can_rx_buffer.head         = 0;
    can_rx_buffer.tail         = 0;
    can_rx_buffer.received_cnt = 0;
    can_rx_buffer.dropped_cnt  = 0;

    if (serial_event == RT_NULL)
        return -RT_ERROR;

    /* set up canbus transmit status */
    for (uint32_t i = 0; i < CAN_TX_NUM; i++)
    {
        can_tx_state.timestamp_us[i] = 0;
        can_tx_state.status[i]       = CAN_TX_STATUS_PENDING;
    }

    /* can interrupt config */
    nvic_irq_enable(CAN1_RX0_IRQn, 0x1, 0x0);
    nvic_irq_enable(CAN1_TX_IRQn, 0x2, 0x0);
    nvic_irq_enable(CAN1_SE_IRQn, 0x3, 0x0);

    /* receive interrupt enable */
    can_interrupt_enable(CAN1, CAN_RF0MIEN_INT, TRUE);

    /* error interrupt enable */
    can_interrupt_enable(CAN1, CAN_BOIEN_INT, TRUE);

    /* transmit interrupt enable */
    can_interrupt_enable(CAN1, CAN_TCIEN_INT, TRUE);

    LOG_I("event init");

    return RT_EOK;
}

