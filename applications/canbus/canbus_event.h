#ifndef CANBUS_EVENT_H
#define CANBUS_EVENT_H

#include <rtthread.h>
#include "at32f402_405.h"
#include "serials.h"

/* handle canbus interrupts as rt-thread events */

#define CAN_RX_NUM (128) /* ringbuffer size, has to be power of two */
#define CAN_TX_NUM (3)   /* number of transmit mailboxes */

typedef enum
{
    CAN_RX_EMPTY = 0, /* no frame available */
    CAN_RX_OK,        /* frame available */
    CAN_RX_SKIP,      /* frame consumed elsewhere */
} can_rx_result_t;

typedef struct
{
    volatile uint32_t                 timestamp_us[CAN_TX_NUM]; /* timestamp when mailbox status changed */
    volatile can_transmit_status_type status[CAN_TX_NUM];       /* status of each mailbox */
} can_tx_state_t;

typedef struct
{
    uint32_t            timestamp_us; /* microsecond timestamp */
    can_rx_message_type frame;        /* AT32 HAL structure */
} can_stored_frame_t;

/* Simple 128-frame ring buffer for received frames */
typedef struct
{
    volatile uint32_t  head;               /* ISR writes here */
    volatile uint32_t  tail;               /* thread reads here */
    uint32_t           received_cnt;       /* count of received frames */
    uint32_t           dropped_cnt;        /* count of dropped frames */
    can_stored_frame_t frames[CAN_RX_NUM]; /* 128-frame buffer */
} can_ringbuffer_t;

extern can_ringbuffer_t can_rx_buffer;

/*
   all canbus frames dequeued with can_rx_get() are passed to can_rx_consumed() first.
   implement protocol decoders in can_rx_consumed():
   - write output using cdc0_write()
   - return true if frame consumed by protocol decoder
   - return false if frame to be processed by gsusb/slcan/lua
 */

bool            can_rx_consumed(can_stored_frame_t *stored_frame); /* canbus receive hook */
can_rx_result_t can_rx_get(can_stored_frame_t *frame);             /* receive canbus frame with timestamp */
rt_err_t        can_tx_get_state(can_tx_state_t *tx_state_dest);   /* get state and timestamp for all transmit mailboxes */
rt_err_t        canbus_event_init(void);

#endif
