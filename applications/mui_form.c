
/* form definition string (fds) - defines all forms and the fields on those forms. */

/* UTF-8 support
   The menu supports UTF-8 characters.
   To conserve flash, the font only contains the characters needed.
   When this file is changed, and new characters are introduced, execute the following command:

   update_unifont.sh

   This ensures the file unifont.h contains all characters used in the menu.
 */

#include "mui_form.h"

#define CANBUS_SPEEDS "1000000| 800000| 500000| 250000| 125000| 100000|  50000|  20000|  10000"

#define SERIAL_SPEEDS                                          \
    "   2400|   4800|   9600|  19200|  38400|  57600| 115200|" \
    " 230400| 460800| 500000| 576000| 921600|1000000|1152000|" \
    "1500000|2000000|2500000|3000000|3500000|4000000"

#define USB_CDC1_INPUT "serial0|serial1|    rtt|   can1"

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
MUI_XY("XF", 0, 15) /* print status line */
MUI_DATA("GP",
    MUI_10 "Startup|"
    MUI_15 "Target|"
    MUI_20 "Serial|"
    MUI_30 "Canbus|"
    MUI_80 "Display|"
    MUI_90 "Settings|"
    MUI_95 "About")
MUI_XYA("GC", 0, 31, 0)
MUI_XYA("GC", 0, 47, 1)
MUI_XYA("GC", 0, 63, 2)
MUI_XYA("GC", 0, 79, 3)
MUI_XYA("GC", 0, 95, 4)
MUI_XYA("GC", 0, 111, 5)
MUI_XYA("GC", 0, 127, 6)

/* debug mode */
MUI_FORM(10)
MUI_STYLE(0)
MUI_LABEL(0, 15, "STARTUP")
MUI_LABEL(0, 31, "attach")
MUI_XY("M0", 107, 31)
MUI_LABEL(0, 47, "memwatch")
MUI_XY("M1", 107, 47)
MUI_LABEL(0, 63, "lua")
MUI_XY("M2", 107, 63)
MUI_LABEL(0, 79, "logging")
MUI_XY("M3", 107, 79)
MUI_LABEL(0, 95, "rtt")
MUI_XY("M4", 107, 95)
MUI_LABEL(0, 111, "watchdog")
MUI_XY("M5", 107, 111)
MUI_XYT("BK", 0, 127, "Back")

/* target */
MUI_FORM(15)
MUI_STYLE(0)
MUI_LABEL(0, 15, "TARGET")
MUI_LABEL(0, 31, "3.3V power")
MUI_XY("T0", 107, 31)
MUI_LABEL(0, 47, "output enable")
MUI_XY("T1", 107, 47)
MUI_XYT("BK", 0, 127, "Back")

/* serials */
MUI_FORM(20)
MUI_STYLE(0)
MUI_LABEL(0, 15, "SERIAL")
MUI_LABEL(0, 31, "input")
MUI_XYAT("U1", 63, 31, 0, USB_CDC1_INPUT)
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
MUI_LABEL(0, 79, "swo decode")
MUI_XY("S4", 107, 79)
MUI_LABEL(0, 95, "swap rxd txd")
MUI_XY("S5", 107, 95)
MUI_XYT("BK", 0, 127, "Back")

/* canbus */
MUI_FORM(30)
MUI_STYLE(0)
MUI_LABEL(0, 15, "CANBUS")
MUI_LABEL(0, 31, "speed")
MUI_XYAT("C0", 63, 31, 0, CANBUS_SPEEDS)
MUI_LABEL(0, 47, "slcan output")
MUI_XY("C1", 107, 47)
MUI_XYT("BK", 0, 127, "Back")

/* display settings become active after the next boot */
MUI_FORM(80)
MUI_STYLE(0)
MUI_LABEL(0, 15, "DISPLAY")
MUI_LABEL(0, 31, "Language")
MUI_XYAT("D0", 95, 31, 0, " en| ??")
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
MUI_GOTO(0, 47, 91, "Recall")
MUI_GOTO(0, 63, 92, "Store")
MUI_GOTO(0, 79, 93, "Reset")
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
MUI_FORM(94)
MUI_STYLE(0)
MUI_LABEL(0, 15, "UTF8 TEST")
MUI_LABEL(0, 31, "Hello, world!")
MUI_LABEL(0, 47, "大家好")
MUI_GOTO(0, 127, 0, "OK")

/* about */
MUI_FORM(95)
MUI_STYLE(0)
MUI_LABEL(0, 15, "ABOUT")
MUI_LABEL(0, 31, "arm can tool")
MUI_LABEL(0, 47, __DATE__) /* compilation date */
MUI_XYT("XE", 0, 63, "k ram free") /* print free ram */
MUI_GOTO(0, 79, 94, "utf8 test")
MUI_GOTO(0, 127, 0, "OK")

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
