#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#include <stdint.h>
#include <stdbool.h>
#include <memwatch.h>
#include <canbus.h>
#include <canfilter.h>
#include <usb_desc.h>

#define SETTINGS_VERSION 1
#define LANG_EN          0
#define LANG_ZH          1
#define CDC0_OUT_SERIAL0 0
#define CDC0_OUT_SERIAL1 1
#define CDC0_OUT_RTT     2
#define CDC0_OUT_SHELL   3
#define DEFAULT_POLLING  5

typedef struct
{
    uint32_t     crc;                          /* checksum */
    uint32_t     timestamp;                    /* when written to eeprom */
    uint8_t      version;                      /* settings version */
    uint8_t      language;                     /* user interface language */
    uint8_t      mode;                         /* boot mode */
    uint8_t      polling_interval;             /* target halt/breakpoint polling interval */
    bool         attach_enable;                /* enable bmd attach */
    bool         memwatch_enable;              /* enable memwatch */
    bool         trigger_enable;               /* enable external trigger */
    bool         lua_autoexec;                 /* enable lua autoexec script */
    bool         logging_enable;               /* enable logging on sd card */
    bool         watchdog_enable;              /* enable watchdog timer */
    bool         fileio_enable;                /* enable semihosting file i/o */
    bool         shell_enable;                 /* enable semihosting shell commands */
    bool         rtt_enable;                   /* enable rtt */
    bool         tpower_enable;                /* enable target power */
    bool         toutput_enable;               /* voltage translators output enable */
    bool         swo_decode;                   /* swo to ascii decoding */
    bool         swap_rxd_txd;                 /* swap target serial rxd txd */
    uint8_t      serial0_speed;                /* serial0 speed */
    uint8_t      serial1_speed;                /* serial1 speed */
    uint8_t      serial2_speed;                /* serial2 speed */
    bool         serial0_enable;               /* from serial0 to usb cdc1 enable */
    bool         serial1_enable;               /* from serial1 to usb cdc1 enable */
    bool         serial2_enable;               /* from serial2 to usb cdc1 enable */
    bool         canfilter_enable;             /* canbus filtering */
    bool         can1_enable;                  /* if set: canbus running at boot */
    uint8_t      can1_speed;                   /* canbus speed, index in bitrate_configs */
    bool         can1_log;                     /* canbus logging on cdc0 enable */
    can_filter_t can1_hw_filter;               /* canbus hardware filter */
    uint8_t      cdc0_out;                     /* from usb cdc0 to target */
    uint8_t      screen_brightness;            /* brightness, 0 .. 255 */
    uint8_t      screen_sleep_time;            /* sleep time in minutes */
    uint8_t      screen_rotation;              /* 0 = 0, 1 = 90, 2 = 180, 3 = 270 */
    bool         swap_buttons;                 /* swap 'next' and 'previous' buttons */
    memwatch_s   memwatch_table[MEMWATCH_NUM]; /* memwatch settings */
    uint8_t      memwatch_cnt;                 /* max number of variables being watched */
    bool         memwatch_timestamp;           /* whether memwatch prints a timestamp */
} settings_struct;

#include "at24c256.h"

#define SETTINGS_MAX_PRESET  15
#define SETTINGS_PRESET_SIZE 512 /* must be >= sizeof(settings_struct) and a multiple of EEPROM_PAGE_SIZE */

extern settings_struct settings;
extern bool            settings_done;
extern uint8_t         settings_preset;


void reset_settings();
void store_settings(uint8_t preset);
void recall_settings(uint8_t preset);
void print_settings();

#endif

