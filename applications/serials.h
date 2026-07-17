#ifndef SERIALS_H
#define SERIALS_H
#include <rtthread.h>
#include <stdint.h>
#include <stdbool.h>

/* event definitions, bit masks and names - keep in sync */

typedef enum
{
    EVENT_CAN1_TX_DONE = 0,    /*!< canbus: transmit complete event */
    EVENT_CAN1_RX0_INDIC,      /*!< canbus: receive fifo 0 event */
    EVENT_CAN1_BUS_OFF,        /*!< canbus: bus-off event */
    EVENT_CAN1_RX_OVERFLOW,    /*!< canbus: receive overflow */
    EVENT_CAN1_TX_OVERFLOW,    /*!< canbus: transmit overflow */
    EVENT_GSUSB_BULK_OUT,      /*!< socketcan: usb bulk out event */
    EVENT_GSUSB_STOP,          /*!< socketcan: usb control request */
    EVENT_GSUSB_START,         /*!< socketcan: usb control request */
    EVENT_GSUSB_TX_DONE,       /*!< socketcan: usb bulk in transfer complete */
    EVENT_CDC0_DTR,            /*!< usb: cdc0 dtr change */
    EVENT_CDC1_DTR,            /*!< usb: cdc1 dtr change */
    EVENT_CDC0_RX,             /*!< usb: cdc0 receive event */
    EVENT_CDC1_RX,             /*!< usb: cdc1 receive event */
    EVENT_SERIAL0_RX,          /*!< uart: serial 0 receive event */
    EVENT_SERIAL1_RX,          /*!< uart: serial 1 receive event */
    EVENT_SERIAL2_RX,          /*!< uart: serial 2 receive event */
    EVENT_TARGET_HALT_REQUEST, /*!< gdb: message to gdb server: halt target */
    EVENT_TARGET_HALTED,       /*!< gdb: message from gdb server: target halted */
    EVENT_MAX
} event_id_t;

#define EVENT_MASK(id) (1UL << (id))

#define EVENT_MASK_CAN1_TX_DONE        EVENT_MASK(EVENT_CAN1_TX_DONE)
#define EVENT_MASK_CAN1_RX0_INDIC      EVENT_MASK(EVENT_CAN1_RX0_INDIC)
#define EVENT_MASK_CAN1_BUS_OFF        EVENT_MASK(EVENT_CAN1_BUS_OFF)
#define EVENT_MASK_CAN1_RX_OVERFLOW    EVENT_MASK(EVENT_CAN1_RX_OVERFLOW)
#define EVENT_MASK_CAN1_TX_OVERFLOW    EVENT_MASK(EVENT_CAN1_TX_OVERFLOW)
#define EVENT_MASK_GSUSB_BULK_OUT      EVENT_MASK(EVENT_GSUSB_BULK_OUT)
#define EVENT_MASK_GSUSB_STOP          EVENT_MASK(EVENT_GSUSB_STOP)
#define EVENT_MASK_GSUSB_START         EVENT_MASK(EVENT_GSUSB_START)
#define EVENT_MASK_GSUSB_TX_DONE       EVENT_MASK(EVENT_GSUSB_TX_DONE)
#define EVENT_MASK_CDC0_DTR            EVENT_MASK(EVENT_CDC0_DTR)
#define EVENT_MASK_CDC1_DTR            EVENT_MASK(EVENT_CDC1_DTR)
#define EVENT_MASK_CDC0_RX             EVENT_MASK(EVENT_CDC0_RX)
#define EVENT_MASK_CDC1_RX             EVENT_MASK(EVENT_CDC1_RX)
#define EVENT_MASK_SERIAL0_RX          EVENT_MASK(EVENT_SERIAL0_RX)
#define EVENT_MASK_SERIAL1_RX          EVENT_MASK(EVENT_SERIAL1_RX)
#define EVENT_MASK_SERIAL2_RX          EVENT_MASK(EVENT_SERIAL2_RX)
#define EVENT_MASK_TARGET_HALT_REQUEST EVENT_MASK(EVENT_TARGET_HALT_REQUEST)
#define EVENT_MASK_TARGET_HALTED       EVENT_MASK(EVENT_TARGET_HALTED)

/* event_names: keep in sync with enum */
extern const char * const event_name[EVENT_MAX];

extern const uint32_t serial_speeds[];

extern rt_event_t  serial_event;
extern rt_device_t cdc1_dev;
extern rt_device_t serial0_dev;
extern rt_device_t serial1_dev;
extern rt_bool_t   serial2_ready;
extern bool        cdc0_dtr;
extern bool        cdc1_dtr;

void serials_init();

void hardware_serials_init();
void serial_set_dtr(uint8_t intf, bool dtr);

void serial0_enable(bool ena);
void serial1_enable(bool ena);
void serial2_enable(bool ena);

void serial0_set_speed(uint32_t speed);
void serial1_set_speed(uint32_t speed);
void serial2_set_speed(uint32_t speed);

void serial0_swap_pins(bool swap_pins);

void usb_serials_init();
void cdc0_write(const char *buf, const size_t len);
void cdc1_write(const char *buf, const size_t len);
void cdc0_printf(const char *fmt, ...);
void cdc1_printf(const char *fmt, ...);

void cdc0_receive(void);
void serial0_receive();
void serial1_receive();
void serial2_receive();

#endif
