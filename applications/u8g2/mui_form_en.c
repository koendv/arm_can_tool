
/* form definition string (fds) - defines all forms and the fields on those forms. */

/* UTF-8 support
   The menu supports UTF-8 characters.
   To conserve flash, the font only contains the characters needed.
   When this file is changed, and new characters are introduced, execute the following command:

   update_unifont.sh

   This ensures the file unifont.h contains all characters used in the menu.
 */

#define U8X8_PROGMEM
#include "mui_hb_strings.h"
#include "mui_form.h"
#include "settings.h"
#include "u8g2_rtl_strings.h" /* for languages written right to left */

#define DISPLAY_WIDTH (128)

#define CANBUS_SPEEDS "  10000|  20000|  33333|  50000|  83333| 100000|" \
                      " 125000| 250000| 500000| 666666| 800000|1000000"

#define SERIAL_SPEEDS                                          \
    "   2400|   4800|   9600|  19200|  38400|  57600| 115200|" \
    " 230400| 460800| 500000| 576000| 921600|1000000|1152000|" \
    "1500000|2000000|2500000|3000000|3500000|4000000"

#define POLLING_INTERVAL_MS "  1|  2|  5| 10| 20| 50|100|200|500"

#define USB_CDC0_OUT "serial0|serial1|    rtt"

/*
 * for MUIF_CHECKBOX_LIST
 * A single item table per language.  Labels differ; value pointers are
 * identical — they both point into the same settings struct.
 */

const checkbox_item_t startup_items_en[STARTUP_ITEMS_COUNT] = {
    {     "CAN bus",     &settings.can1_enable},
    {    "debugger",   &settings.attach_enable},
    {    "memwatch", &settings.memwatch_enable},
    {"lua autoexec",    &settings.lua_autoexec},
    {     "logging",  &settings.logging_enable},
    {         "rtt",      &settings.rtt_enable},
    {         "swo",      &settings.swo_decode},
    {    "watchdog", &settings.watchdog_enable},
    {     "trigger",  &settings.trigger_enable},
};

/*
8 lines per screen. 16 pixels per line.
top line: title and status.
line 1-6: menu and content.
last line: action item, e.g. Back button

MUI_FORM(10)
MUI_STYLE(0)
MUI_LABEL(0, 15, "TITLE")
MUI_LABEL(0, 31, "line 1")
MUI_LABEL(0, 47, "line 2")
MUI_LABEL(0, 63, "line 3")
MUI_LABEL(0, 79, "line 4")
MUI_LABEL(0, 95, "line 5")
MUI_LABEL(0, 111, "line 6")
MUI_XYT("BK", 0, 127, "Back")

*/

/* clang-format off */

/* fds_data[] in english. each user interface language has its own fds_data[] */

fds_t fds_data_en[] =

/* top level main menu */
MUI_FORM(0)
MUI_STYLE(0)
MUI_DATA("GP",
    MUI_25 "Mode|"
    MUI_10 "Startup|"
    MUI_15 "Target|"
    MUI_20 "Serial|"
    MUI_30 "CAN bus|"
    MUI_80 "Display|"
    MUI_90 "Settings|"
    MUI_95 "About")
MUI_XYA("GC", 0, 15, 0)
MUI_XYA("GC", 0, 31, 1)
MUI_XYA("GC", 0, 47, 2)
MUI_XYA("GC", 0, 63, 3)
MUI_XYA("GC", 0, 79, 4)
MUI_XYA("GC", 0, 95, 5)
MUI_XYA("GC", 0, 111, 6)
MUI_XYA("GC", 0, 127, 7)

/* debug mode */
MUI_FORM(25)
MUI_STYLE(0)
MUI_LABEL(0, 15, "MODE")
MUI_XYAT("U0", 0, 31, 0, "CMSIS-DAP|GDB SERVER|MASS STORAGE|LUA SCRIPT")
MUI_GOTO(0, 111, 99, "OK")
MUI_GOTO(0, 127, 0, "Back")

/* services at boot */
MUI_FORM(10)
MUI_STYLE(0)
MUI_LABEL(0, 15, "STARTUP")
MUI_XYA("CE", 0, 31,  0)
MUI_XYA("CE", 0, 47,  1)
MUI_XYA("CE", 0, 63,  2)
MUI_XYA("CE", 0, 79,  3)
MUI_XYA("CE", 0, 95,  4)
MUI_XYA("CE", 0, 111, 5)
MUI_XYT("BK", 0, 127, "Back")

/* target */
MUI_FORM(15)
MUI_STYLE(0)
MUI_LABEL(0, 15, "TARGET")
MUI_LABEL(0, 31, "polling ms")
MUI_XYAT("T2", 95, 31, 0, POLLING_INTERVAL_MS)
MUI_LABEL(0, 47, "output enable")
MUI_XY("T1", 107, 47)
MUI_LABEL(0, 63, "3.3V power")
MUI_XY("T0", 107, 63)
MUI_LABEL(0, 79, "file i/o")
MUI_XY("T3", 107, 79)
MUI_LABEL(0, 95, "shell command")
MUI_XY("T4", 107, 95)
MUI_XYT("BK", 0, 127, "Back")

/* serials */
MUI_FORM(20)
MUI_STYLE(0)
MUI_LABEL(0, 15, "SERIAL")
MUI_LABEL(0, 31, "usb out")
MUI_XYAT("U1", 63, 31, 0, USB_CDC0_OUT)
MUI_LABEL(0, 47, "serial0")
MUI_XYAT("S1", 63, 47, 0, SERIAL_SPEEDS)
MUI_LABEL(0, 63, "serial1")
MUI_XYAT("S2", 63, 63, 0, SERIAL_SPEEDS)
MUI_LABEL(0, 79, "serial2")
MUI_XYAT("S3", 63, 79, 0, SERIAL_SPEEDS)
MUI_GOTO(0, 95, 21, "Serial Enable")
MUI_XYT("BK", 0, 127, "Back")

/* serials enable */
MUI_FORM(21)
MUI_STYLE(0)
MUI_LABEL(0, 15, "SERIAL ENABLE")
MUI_LABEL(0, 31, "serial0")
MUI_XY("U2", 107, 31)
MUI_LABEL(0, 47, "serial1")
MUI_XY("U3", 107, 47)
MUI_LABEL(0, 63, "serial2")
MUI_XY("U4", 107, 63)
MUI_LABEL(0, 79, "swap rxd txd")
MUI_XY("S5", 107, 79)
MUI_XYT("BK", 0, 127, "Back")

/* canbus */
MUI_FORM(30)
MUI_STYLE(0)
MUI_LABEL(0, 15, "CANBUS")
MUI_LABEL(0, 31, "speed")
MUI_XYAT("C0", 63, 31, 0, CANBUS_SPEEDS)
MUI_LABEL(0, 47, "logging")
MUI_XY("C1", 107, 47)
MUI_LABEL(0, 63, "canfilter")
MUI_XY("C2", 107, 63)
MUI_XYT("BK", 0, 127, "Back")

/* display settings become active after the next boot */
MUI_FORM(80)
MUI_STYLE(0)
MUI_LABEL(0, 15, "DISPLAY")
MUI_LABEL(0, 31, "Language")
MUI_XYAT("D0", 87, 31, 0, "  en|中文")
MUI_LABEL(0, 47, "Brightness")
MUI_XY("D1", 99, 47)
MUI_LABEL(0, 63, "Rotate")
MUI_XY("D2", 115, 63)
MUI_LABEL(0, 79, "Sleep")
MUI_XY("D3", 107, 79)
MUI_LABEL(0, 95, "Swap buttons")
MUI_XY("D4", 107, 95)
MUI_GOTO(0, 127, 99, "OK")

/* settings */
MUI_FORM(90)
MUI_STYLE(0)
MUI_LABEL(0, 15, "SETTINGS")
MUI_GOTO(0, 31, 96, "Clock")
MUI_LABEL(0, 47, "Preset")
MUI_XY("XS", 108, 47)
MUI_GOTO(0, 63, 91, "Recall")
MUI_GOTO(0, 79, 92, "Store")
MUI_GOTO(0, 95, 93, "Reset")
MUI_XYT("BK", 0, 127, "Back")

/* recall settings action */
MUI_FORM(91)
MUI_STYLE(0)
MUI_AUX("XA")
MUI_LABEL(0, 15, "SETTINGS")
MUI_LABEL(0, 31, "recalled")
MUI_GOTO(0, 127, 0, "OK")

/* store settings action */
MUI_FORM(92)
MUI_STYLE(0)
MUI_AUX("XB")
MUI_LABEL(0, 15, "SETTINGS")
MUI_LABEL(0, 31, "stored")
MUI_GOTO(0, 127, 0, "OK")

/* reset settings action */
MUI_FORM(93)
MUI_STYLE(0)
MUI_AUX("XC")
MUI_LABEL(0, 15, "SETTINGS")
MUI_LABEL(0, 31, "reset")
MUI_GOTO(0, 127, 0, "OK")

/* utf8 test page */

/* use tool hbpp for libharfbuzz rewriting in Devanagari */

/* use tool utf8-rtl-strings for right-to-left strings in Arabic */

MUI_FORM(94)
MUI_STYLE(0)
MUI_LABEL(0, 15, "Hello")                  // Latin baseline
MUI_LABEL(0, 29, "Привет")                 // Cyrillic alphabet
MUI_LABEL(0, 47, "你好")                   // CJK characters
MUI_LABEL(0, 63, "こんにちは")             // Japanese hiragana
MUI_LABEL(0, 79, "한국어")                 // Korean Hangul
MUI_XYHB_NAMASTE("HB", 0, 95, "नमस्ते")      // Devanagari
MUI_LABEL(104, 108, U8G2_RTL_HELLO("سلام")) // right-to-left RTL + cursive + lam-alif ligature
MUI_XYT("BK", 0, 127, "Back")

/* about */
MUI_FORM(95)
MUI_STYLE(0)
MUI_LABEL(0, 15, "ABOUT")
MUI_LABEL(0, 31, "ARM CAN TOOL")
MUI_LABEL(0, 47, __DATE__) /* compilation date */
MUI_LABEL(0, 63, "free ram:")
MUI_XYT("XE", 95, 63, "k") /* print free ram */
MUI_GOTO(0, 79, 94, "utf8 test")
MUI_XYT("BK", 0, 127, "Back")

/* date and time */
MUI_FORM(96)
MUI_STYLE(0)
MUI_LABEL(0, 15, "CLOCK")
MUI_AUX("YA")
MUI_LABEL(0, 31, "Year")
MUI_XY("Y0", 107, 31)
MUI_LABEL(0, 47, "Month")
MUI_XY("Y1", 107, 47)
MUI_LABEL(0, 63, "Day")
MUI_XY("Y2", 107, 63)
MUI_LABEL(0, 79, "Hour")
MUI_XY("Y3", 107, 79)
MUI_LABEL(0, 95, "Minutes")
MUI_XY("Y4", 107, 95)
MUI_GOTO(0, 111, 97, "Set clock")
MUI_XYT("BK", 0, 127, "Back")

MUI_FORM(97)
MUI_STYLE(0)
MUI_LABEL(0, 15, "SET CLOCK")
MUI_LABEL(0, 31, "Release button")
MUI_LABEL(0, 47, "to reboot")
MUI_AUX("YB")

/* store settings and reboot */
MUI_FORM(99)
MUI_STYLE(0)
MUI_LABEL(0, 15, "REBOOT")
MUI_LABEL(0, 31, "Release button")
MUI_LABEL(0, 47, "to reboot")
MUI_AUX("XD")

;

/* clang-format on */
