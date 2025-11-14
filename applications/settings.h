#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#include <stdint.h>
#include <stdbool.h>
#include <memwatch.h>
#include <canbus.h>

#define SETTINGS_VERSION 1
#define LANG_EN          0
#define CDC1_SERIAL0     0
#define CDC1_SERIAL1     1
#define CDC1_RTT         2
#define CDC1_CAN         3

typedef struct
{
    uint32_t             crc;                          /* checksum */
    uint8_t              version;                      /* settings version */
    uint8_t              language;                     /* user interface language */
    bool                 attach_enable;                /* enable bmd attach */
    bool                 memwatch_enable;              /* enable memwatch */
    bool                 lua_enable;                   /* enable lua program */
    bool                 logging_enable;               /* enable logging on sd card */
    bool                 watchdog_enable;              /* enable watchdog timer */
    bool                 rtt_enable;                   /* enable rtt */
    bool                 tpower_enable;                /* enable target power */
    bool                 toutput_enable;               /* voltage translators output enable */
    bool                 swo_decode;                   /* swo to ascii decoding */
    bool                 swap_rxd_txd;                 /* swap target serial rxd txd */
    uint8_t              serial0_speed;                /* serial0 speed */
    uint8_t              serial1_speed;                /* serial1 speed */
    uint8_t              serial2_speed;                /* serial2 speed */
    bool                 serial0_enable;               /* from serial0 to usb cdc1 enable */
    bool                 serial1_enable;               /* from serial1 to usb cdc1 enable */
    bool                 serial2_enable;               /* from serial2 to usb cdc1 enable */
    uint8_t              can1_speed;                   /* canbus speed, in Hz */
    bool                 can1_slcan;                   /* canbus slcan output enable */
    can_hw_filter_bank_t can1_hw_filter;               /* canbus hardware filter */
    uint8_t              cdc1_output;                  /* from usb cdc1 to target */
    uint8_t              screen_brightness;            /* brightness, 0 .. 255 */
    uint8_t              screen_sleep_time;            /* sleep time in minutes */
    uint8_t              screen_rotation;              /* 0 = 0, 1 = 90, 2 = 180, 3 = 270 */
    bool                 swap_buttons;                 /* swap 'next' and 'previous' buttons */
    memwatch_s           memwatch_table[MEMWATCH_NUM]; /* memwatch settings */
    uint32_t             memwatch_cnt;                 /* max number of variables being watched */
    bool                 memwatch_timestamp;           /* whether memwatch prints a timestamp */
} settings_struct;

extern settings_struct settings;

void reset_settings();
void store_settings();
void recall_settings();
void print_settings();

#endif

