
/* form definition string (fds) - defines all forms and the fields on those forms. */

/* UTF-8 support
   The menu supports UTF-8 characters.
   To conserve flash, the font only contains the characters needed.
   When this file is changed, and new characters are introduced, execute the following command:

   update_unifont.sh

   This ensures the file unifont.h contains all characters used in the menu.
 */

/*
   简体中文菜单

   由于屏幕分辨率限制(128x128像素，16点阵字体)，
   部分翻译使用了简化表达。

   翻译错误或不妥之处，恳请指正！
   反馈格式：页面名称 | 英文原文 | 当前中文 | 修改建议
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

#define USB_CDC0_OUT "  串口0|  串口1|RTT调试"

/* for MUIF_CHECKBOX_LIST */

const checkbox_item_t startup_items_zh[STARTUP_ITEMS_COUNT] = {
    {      "CAN总线",     &settings.can1_enable},
    {      "调试器",   &settings.attach_enable},
    {   "内存监视", &settings.memwatch_enable},
    {"自动运行Lua",    &settings.lua_autoexec},
    {         "日志",  &settings.logging_enable},
    {      "RTT调试",      &settings.rtt_enable},
    {      "SWO解码",      &settings.swo_decode},
    {      "看门狗", &settings.watchdog_enable},
    {         "触发",  &settings.trigger_enable},
};

/* clang-format off */

/* fds_data[] in simplified chinese. each user interface language has its own fds_data[] */

fds_t fds_data_zh[] =

/* top level main menu */
MUI_FORM(0)
MUI_STYLE(0)
MUI_DATA("GP",
    MUI_25 "模式|"
    MUI_10 "启动|"
    MUI_15 "目标|"
    MUI_20 "串口|"
    MUI_30 "CAN总线|"
    MUI_80 "显示|"
    MUI_90 "设置|"
    MUI_95 "关于")
MUI_XYA("GC", 0, 19, 0)
MUI_XYA("GC", 0, 37, 1)
MUI_XYA("GC", 0, 55, 2)
MUI_XYA("GC", 0, 73, 3)
MUI_XYA("GC", 0, 91, 4)
MUI_XYA("GC", 0, 109, 5)
MUI_XYA("GC", 0, 127, 6)

/* debug mode */
MUI_FORM(25)
MUI_STYLE(0)
MUI_LABEL(0, 19, "模式")
MUI_XYAT("U0", 0, 37, 0, "CMSIS-DAP|GDB服务器|U盘模式|LUA脚本")
MUI_GOTO(0, 109, 99, "确认")
MUI_GOTO(0, 127, 0, "返回")

/* services at boot */
MUI_FORM(10)
MUI_STYLE(0)
MUI_LABEL(0, 19, "启动")
MUI_XYA("CZ", 0, 37,  0)
MUI_XYA("CZ", 0, 55,  1)
MUI_XYA("CZ", 0, 73,  2)
MUI_XYA("CZ", 0, 91,  3)
MUI_XYA("CZ", 0, 109,  4)
MUI_XYT("BK", 0, 127, "返回")

/* target */
MUI_FORM(15)
MUI_STYLE(0)
MUI_LABEL(0, 19, "目标")
MUI_LABEL(0, 37, "轮询ms")
MUI_XYAT("T2", 95, 37, 0, POLLING_INTERVAL_MS)
MUI_LABEL(0, 55, "输出使能")
MUI_XY("T1", 107, 55)
MUI_LABEL(0, 73, "3.3V供电")
MUI_XY("T0", 107, 73)
MUI_LABEL(0, 91, "文件读写")
MUI_XY("T3", 107, 91)
MUI_LABEL(0, 109, "系统命令")
MUI_XY("T4", 107, 109)
MUI_XYT("BK", 0, 127, "返回")

/* serials */
MUI_FORM(20)
MUI_STYLE(0)
MUI_LABEL(0, 19, "串口")
MUI_LABEL(0, 37, "USB输出")
MUI_XYAT("U1", 63, 37, 0, USB_CDC0_OUT)
MUI_LABEL(0, 55, "串口0")
MUI_XYAT("S1", 63, 55, 0, SERIAL_SPEEDS)
MUI_LABEL(0, 73, "串口1")
MUI_XYAT("S2", 63, 73, 0, SERIAL_SPEEDS)
MUI_LABEL(0, 91, "串口2")
MUI_XYAT("S3", 63, 91, 0, SERIAL_SPEEDS)
MUI_GOTO(0, 109, 21, "串口使能")
MUI_XYT("BK", 0, 127, "返回")

/* serials enable */
MUI_FORM(21)
MUI_STYLE(0)
MUI_LABEL(0, 19, "串口使能")
MUI_LABEL(0, 37, "串口0")
MUI_XY("U2", 107, 37)
MUI_LABEL(0, 55, "串口1")
MUI_XY("U3", 107, 55)
MUI_LABEL(0, 73, "串口2")
MUI_XY("U4", 107, 73)
MUI_LABEL(0, 91, "交换RXD/TXD")
MUI_XY("S5", 107, 91)
MUI_XYT("BK", 0, 127, "返回")

/* canbus */
MUI_FORM(30)
MUI_STYLE(0)
MUI_LABEL(0, 19, "CAN总线")
MUI_LABEL(0, 37, "波特率")
MUI_XYAT("C0", 63, 37, 0, CANBUS_SPEEDS)
MUI_LABEL(0, 55, "日志")
MUI_XY("C1", 107, 55)
MUI_LABEL(0, 73, "CAN过滤")
MUI_XY("C2", 107, 73)
MUI_XYT("BK", 0, 127, "返回")

/* display settings become active after the next boot */
MUI_FORM(80)
MUI_STYLE(0)
MUI_LABEL(0, 19, "显示")
MUI_LABEL(0, 37, "语言")
MUI_XYAT("D0", 87, 37, 0, "  en|中文")
MUI_LABEL(0, 55, "亮度")
MUI_XY("D1", 99, 55)
MUI_LABEL(0, 73, "旋转")
MUI_XY("D2", 115, 73)
MUI_LABEL(0, 91, "休眠")
MUI_XY("D3", 107, 91)
MUI_LABEL(0, 109, "互换按键")
MUI_XY("D4", 107, 109)
MUI_GOTO(0, 127, 99, "确认")

/* settings */
MUI_FORM(90)
MUI_STYLE(0)
MUI_LABEL(0, 19, "设置")
MUI_GOTO(0, 37, 96, "时钟")
MUI_LABEL(0, 55, "预设")
MUI_XY("XS", 108, 55)
MUI_GOTO(0, 73, 91, "恢复")
MUI_GOTO(0, 91, 92, "保存")
MUI_GOTO(0, 109, 93, "重置")
MUI_XYT("BK", 0, 127, "返回")

/* recall settings action */
MUI_FORM(91)
MUI_STYLE(0)
MUI_AUX("XA")
MUI_LABEL(0, 19, "设定")
MUI_LABEL(0, 37, "已恢复")
MUI_GOTO(0, 127, 0, "确认")

/* store settings action */
MUI_FORM(92)
MUI_STYLE(0)
MUI_AUX("XB")
MUI_LABEL(0, 19, "设定")
MUI_LABEL(0, 37, "已保存")
MUI_GOTO(0, 127, 0, "确认")

/* reset settings action */
MUI_FORM(93)
MUI_STYLE(0)
MUI_AUX("XC")
MUI_LABEL(0, 19, "设定")
MUI_LABEL(0, 37, "已重置")
MUI_GOTO(0, 127, 0, "确认")

/* utf8 test page */
MUI_FORM(94)
MUI_STYLE(0)
MUI_LABEL(0, 15, "Hello")                  // Latin baseline
MUI_LABEL(0, 29, "Привет")                 // Cyrillic alphabet
MUI_LABEL(0, 47, "你好")                   // CJK characters
MUI_LABEL(0, 63, "こんにちは")             // Japanese hiragana
MUI_LABEL(0, 79, "한국어")                 // Korean Hangul
MUI_XYHB_NAMASTE("HB", 0, 95, "नमस्ते")      // Devanagari
MUI_LABEL(104, 108, U8G2_RTL_HELLO("سلام")) // right-to-left RTL + cursive + lam-alif ligature
MUI_XYT("BK", 0, 127, "返回")

/* about */
MUI_FORM(95)
MUI_STYLE(0)
MUI_LABEL(0, 19, "关于")
MUI_LABEL(0, 37, "ARM CAN TOOL")
MUI_LABEL(0, 55, __DATE__) /* compilation date */
MUI_LABEL(0, 73, "剩余内存")
MUI_XYT("XE", 72, 73, "KB") /* print free ram */
MUI_GOTO(0, 91, 94, "UTF8测试")
MUI_XYT("BK", 0, 127, "返回")

/* date and time */
MUI_FORM(96)
MUI_STYLE(0)
MUI_LABEL(0, 19, "时钟")
MUI_AUX("YA")
MUI_LABEL(0, 37, "年")
MUI_XY("Y0", 19, 37)
MUI_LABEL(37, 37, "月")
MUI_XY("Y1", 55, 37)
MUI_LABEL(73, 37, "日")
MUI_XY("Y2", 91, 37)
MUI_LABEL(0, 55, "时")
MUI_XY("Y3", 19, 55)
MUI_LABEL(37, 55, "分")
MUI_XY("Y4", 55, 55)
MUI_GOTO(0, 109, 97, "设定时钟")
MUI_XYT("BK", 0, 127, "返回")

MUI_FORM(97)
MUI_STYLE(0)
MUI_LABEL(0, 19, "设定时钟")
MUI_LABEL(0, 37, "松开按键")
MUI_LABEL(0, 55, "后重启")
MUI_AUX("YB")

/* store settings and reboot */
MUI_FORM(99)
MUI_STYLE(0)
MUI_LABEL(0, 19, "重启")
MUI_LABEL(0, 37, "松开按键")
MUI_LABEL(0, 55, "后重启")
MUI_AUX("XD")

;

/* clang-format on */
