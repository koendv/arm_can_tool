#include <rthw.h>
#include <rtthread.h>
#include <rtdevice.h>
#include <u8g2_port.h>
#include <mui_u8g2.h>
#include <stdio.h>

#include "pins.h"
#include "canbus.h"
#include "serials.h"
#include "ds3231_util.h"

#include "general.h"
#include "gdb_main.h"
#include "target.h"
#include "target_internal.h"

#include "unifont.h"
#include "mui_form.h"
#include "settings.h"
#include "platform.h"

#define DBG_TAG "MUI"
#define DBG_LVL DBG_ERR
#include <rtdbg.h>

/* if system reboots in menu, try increasing mui task stack size */
#define MUI_STACK 2048

mui_t              mui;
u8g2_t             u8g2;
const uint32_t     serial_speeds[] = {2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800, 500000, 576000, 921600, 1000000, 1152000, 1500000, 2000000, 2500000, 3000000, 3500000, 4000000};
const enum CANBAUD can_speeds[]    = {CAN1MBaud, CAN800kBaud, CAN500kBaud, CAN250kBaud, CAN125kBaud, CAN100kBaud, CAN50kBaud, CAN20kBaud, CAN10kBaud};
uint8_t            mui_year, mui_month, mui_mday, mui_hour, mui_minutes, mui_seconds; /* date and time */

/* reboot */

static void reboot_thread(void *parameter)
{
    /* wait until multi-direction switch released before rebooting, to avoid booting in bootloader */
    while (rt_pin_read(SW_SELECT_PIN) == PIN_LOW)
        rt_thread_mdelay(100);
    rt_hw_cpu_reset();
}

void reboot_system()
{
    rt_thread_t thread = rt_thread_create("reboot", reboot_thread, RT_NULL, 1024, 25, 10);
    if (thread != RT_NULL)
    {
        rt_kprintf("\nrebooting\n");
        rt_thread_startup(thread);
    }
    else
    {
        LOG_E("reboot thread failed!");
    }
}

/* horizontal line */
uint8_t mui_hrule(mui_t *ui, uint8_t msg)
{
    u8g2_t *u8g2 = mui_get_U8g2(ui);
    switch (msg)
    {
    case MUIF_MSG_DRAW:
        u8g2_DrawHLine(u8g2, 0, mui_get_y(ui), u8g2_GetDisplayWidth(u8g2));
        break;
    }
    return 0;
}

uint8_t mui_target_power_enable(mui_t *ui, uint8_t msg)
{
    uint8_t retval = mui_u8g2_u8_chkbox_wm_pi(ui, msg);
    if ((msg == MUIF_MSG_CURSOR_SELECT) || (msg == MUIF_MSG_VALUE_INCREMENT) || (msg == MUIF_MSG_VALUE_DECREMENT))
        target_power_enable(settings.tpower_enable);
    return retval;
}

uint8_t mui_target_output_enable(mui_t *ui, uint8_t msg)
{
    uint8_t retval = mui_u8g2_u8_chkbox_wm_pi(ui, msg);
    if ((msg == MUIF_MSG_CURSOR_SELECT) || (msg == MUIF_MSG_VALUE_INCREMENT) || (msg == MUIF_MSG_VALUE_DECREMENT))
        target_output_enable(settings.toutput_enable);
    return retval;
}

uint8_t mui_serial0_enable(mui_t *ui, uint8_t msg)
{
    uint8_t retval = mui_u8g2_u8_chkbox_wm_pi(ui, msg);
    serial0_enable(settings.serial0_enable);
    return retval;
}

uint8_t mui_serial1_enable(mui_t *ui, uint8_t msg)
{
    uint8_t retval = mui_u8g2_u8_chkbox_wm_pi(ui, msg);
    serial1_enable(settings.serial1_enable);
    return retval;
}

uint8_t mui_serial2_enable(mui_t *ui, uint8_t msg)
{
    uint8_t retval = mui_u8g2_u8_chkbox_wm_pi(ui, msg);
    serial2_enable(settings.serial2_enable);
    return retval;
}

uint8_t mui_serial0_swap_pins(mui_t *ui, uint8_t msg)
{
    uint8_t retval = mui_u8g2_u8_chkbox_wm_pi(ui, msg);
    if ((msg == MUIF_MSG_CURSOR_SELECT) || (msg == MUIF_MSG_VALUE_INCREMENT) || (msg == MUIF_MSG_VALUE_DECREMENT))
        serial0_swap_pins(settings.swap_rxd_txd);
    return retval;
}

uint8_t mui_serial0_set_speed(mui_t *ui, uint8_t msg)
{
    uint8_t retval = mui_u8g2_u8_opt_line_wa_mud_pi(ui, msg);
    if (settings.serial0_speed > sizeof(serial_speeds) / sizeof(serial_speeds[0])) settings.serial0_speed = 0;
    if ((msg == MUIF_MSG_EVENT_NEXT) || (msg == MUIF_MSG_EVENT_PREV))
        serial0_set_speed(serial_speeds[settings.serial0_speed]);
    return retval;
}

uint8_t mui_serial1_set_speed(mui_t *ui, uint8_t msg)
{
    uint8_t retval = mui_u8g2_u8_opt_line_wa_mud_pi(ui, msg);
    if (settings.serial1_speed > sizeof(serial_speeds) / sizeof(serial_speeds[0])) settings.serial1_speed = 0;
    if ((msg == MUIF_MSG_EVENT_NEXT) || (msg == MUIF_MSG_EVENT_PREV))
        serial1_set_speed(serial_speeds[settings.serial1_speed]);
    return retval;
}

uint8_t mui_serial2_set_speed(mui_t *ui, uint8_t msg)
{
    uint8_t retval = mui_u8g2_u8_opt_line_wa_mud_pi(ui, msg);
    if (settings.serial2_speed > sizeof(serial_speeds) / sizeof(serial_speeds[0])) settings.serial2_speed = 0;
    if ((msg == MUIF_MSG_EVENT_NEXT) || (msg == MUIF_MSG_EVENT_PREV))
        serial2_set_speed(serial_speeds[settings.serial2_speed]);
    return retval;
}

uint8_t mui_set_brightness(mui_t *ui, uint8_t msg)
{
    uint8_t retval = mui_u8g2_u8_min_max_wm_mud_pi(ui, msg);
    if ((msg == MUIF_MSG_EVENT_NEXT) || (msg == MUIF_MSG_EVENT_PREV))
        u8g2_SetContrast(&u8g2, settings.screen_brightness);
    return retval;
}

uint8_t mui_clock_get(mui_t *ui, uint8_t msg)
{
    if (msg == MUIF_MSG_FORM_START)
    {
        clock_get(&mui_year, &mui_month, &mui_mday, &mui_hour, &mui_minutes, &mui_seconds);
    }
    return 0;
}

uint8_t mui_clock_set(mui_t *ui, uint8_t msg)
{
    uint8_t retval = mui_u8g2_btn_back_w1_pi(ui, msg);
    if (msg == MUIF_MSG_FORM_START)
    {
        clock_set(mui_year, mui_month, mui_mday, mui_hour, mui_minutes, mui_seconds);
        reboot_system();
    }
    return retval;
}

uint8_t mui_can1_set_speed(mui_t *ui, uint8_t msg)
{
    uint8_t retval = mui_u8g2_u8_opt_line_wa_mud_pi(ui, msg);
    if (settings.can1_speed > sizeof(can_speeds) / sizeof(can_speeds[0])) settings.can1_speed = 0;
    if ((msg == MUIF_MSG_EVENT_NEXT) || (msg == MUIF_MSG_EVENT_PREV))
        can1_set_speed(can_speeds[settings.can1_speed]);
    return retval;
}

uint8_t mui_recall_settings(mui_t *ui, uint8_t msg)
{
    if (msg == MUIF_MSG_FORM_START)
    {
        recall_settings();
    }
    return 0;
}

uint8_t mui_store_settings(mui_t *ui, uint8_t msg)
{
    if (msg == MUIF_MSG_FORM_START)
    {
        store_settings();
    }
    return 0;
}

uint8_t mui_reset_settings(mui_t *ui, uint8_t msg)
{
    if (msg == MUIF_MSG_FORM_START)
    {
        reset_settings();
    }
    return 0;
}

uint8_t mui_store_settings_and_reboot(mui_t *ui, uint8_t msg)
{
    if (msg == MUIF_MSG_FORM_START)
    {
        store_settings();
        reboot_system();
    }
    return 0;
}

/* print free memory */
uint8_t mui_memory_info(mui_t *ui, uint8_t msg)
{
    rt_size_t mem_total = 0, mem_used = 0, mem_max_used = 0, mem_free = 0;
    char      buf[16];
    if (msg == MUIF_MSG_DRAW)
    {
        rt_memory_info(&mem_total, &mem_used, &mem_max_used);
        mem_free = mem_total - mem_used;
        mem_free = (mem_free + 1023) / 1024;
        char *s  = mui_get_text(ui);
        sprintf(buf, "%d%s", mem_free, s);
        u8g2_DrawStr(&u8g2, mui_get_x(ui), mui_get_y(ui), buf);
    }
    return 0;
}

uint8_t mui_status(mui_t *ui, uint8_t msg)
{
    if (msg == MUIF_MSG_DRAW)
    {
        /* print gdb server target */
        if (cur_target && cur_target->driver)
        {
            //u8g2_DrawStr(&u8g2, mui_get_x(ui), mui_get_y(ui), cur_target->driver);
            u8g2_DrawStr(&u8g2, 0, 15, cur_target->driver);
        }
    }
    return 0;
}

/* User interface fields list */

muif_t muif_list[] = {
    /* normal text style */
    MUIF_U8G2_FONT_STYLE(0, u8g2_font_unifont),
    /* horizontal line (hrule) */
    MUIF_RO("HR", mui_hrule),
    /* enable bmd auto attach */
    MUIF_VARIABLE("M0", &settings.attach_enable, mui_u8g2_u8_chkbox_wm_pi),
    /* enable memwatch */
    MUIF_VARIABLE("M1", &settings.memwatch_enable, mui_u8g2_u8_chkbox_wm_pi),
    /* enable lua program */
    MUIF_VARIABLE("M2", &settings.lua_enable, mui_u8g2_u8_chkbox_wm_pi),
    /* enable logging on sd card */
    MUIF_VARIABLE("M3", &settings.logging_enable, mui_u8g2_u8_chkbox_wm_pi),
    /* enable rtt */
    MUIF_VARIABLE("M4", &settings.rtt_enable, mui_u8g2_u8_chkbox_wm_pi),
    /* enable watchdog timer */
    MUIF_VARIABLE("M5", &settings.watchdog_enable, mui_u8g2_u8_chkbox_wm_pi),
    /* enable target power */
    MUIF_VARIABLE("T0", &settings.tpower_enable, mui_target_power_enable),
    /* voltage translators output enable */
    MUIF_VARIABLE("T1", &settings.toutput_enable, mui_target_output_enable),
    /* target serial speed */
    MUIF_VARIABLE("S1", &settings.serial0_speed, mui_serial0_set_speed),
    /* target aux serial1 speed */
    MUIF_VARIABLE("S2", &settings.serial1_speed, mui_serial1_set_speed),
    /* target aux serial2 speed */
    MUIF_VARIABLE("S3", &settings.serial2_speed, mui_serial2_set_speed),
    /* swo decode */
    MUIF_VARIABLE("S4", &settings.swo_decode, mui_u8g2_u8_chkbox_wm_pi),
    /* swap serial0 rxd txd */
    MUIF_VARIABLE("S5", &settings.swap_rxd_txd, mui_serial0_swap_pins),
    /* canbus speed */
    MUIF_VARIABLE("C0", &settings.can1_speed, mui_can1_set_speed),
    /* canbus slcan output enable */
    MUIF_VARIABLE("C1", &settings.can1_slcan, mui_u8g2_u8_chkbox_wm_pi),
    /* from usb cdc1 to target */
    MUIF_VARIABLE("U1", &settings.cdc1_output, mui_u8g2_u8_opt_line_wa_mud_pi),
    /* from serial0 to usb cdc1 */
    MUIF_VARIABLE("U2", &settings.serial0_enable, mui_serial0_enable),
    /* from serial1 to usb cdc1 */
    MUIF_VARIABLE("U3", &settings.serial1_enable, mui_serial1_enable),
    /* from serial2 to usb cdc1 */
    MUIF_VARIABLE("U4", &settings.serial2_enable, mui_serial2_enable),
    /* user interface language */
    MUIF_VARIABLE("D0", &settings.language, mui_u8g2_u8_opt_line_wa_mud_pi),
    /* display brightness */
    MUIF_U8G2_U8_MIN_MAX("D1", &settings.screen_brightness, 0, 255, mui_set_brightness),
    /* display rotation */
    MUIF_U8G2_U8_MIN_MAX("D2", &settings.screen_rotation, 0, 3, mui_u8g2_u8_min_max_wm_mud_pi),
    /* idle time before display blanking */
    MUIF_U8G2_U8_MIN_MAX("D3", &settings.screen_sleep_time, 0, 30, mui_u8g2_u8_min_max_wm_mud_pi),
    /* swap buttons checkbox */
    MUIF_VARIABLE("D4", &settings.swap_buttons, mui_u8g2_u8_chkbox_wm_pi),
    /* button */
    MUIF_GOTO(mui_u8g2_btn_goto_w1_pi),
    /* back button */
    MUIF_BUTTON("BK", mui_u8g2_btn_back_w1_pi),
    /* text */
    MUIF_U8G2_LABEL(),
    /* menu */
    MUIF_RO("GP", mui_u8g2_goto_data),
    MUIF_BUTTON("GC", mui_u8g2_goto_form_w1_pi),
    /* display settings */
    MUIF_RO("XA", mui_recall_settings),
    MUIF_RO("XB", mui_store_settings),
    MUIF_RO("XC", mui_reset_settings),
    MUIF_RO("XD", mui_store_settings_and_reboot),
    /* print free ram */
    MUIF_RO("XE", mui_memory_info),
    /* print status line */
    MUIF_RO("XF", mui_status),
    /* date and time */
    MUIF_U8G2_U8_MIN_MAX("Y0", &mui_year, 0, 99, mui_u8g2_u8_min_max_wm_mud_pi),
    MUIF_U8G2_U8_MIN_MAX("Y1", &mui_month, 1, 12, mui_u8g2_u8_min_max_wm_mud_pi),
    MUIF_U8G2_U8_MIN_MAX("Y2", &mui_mday, 1, 31, mui_u8g2_u8_min_max_wm_mud_pi),
    MUIF_U8G2_U8_MIN_MAX("Y3", &mui_hour, 0, 23, mui_u8g2_u8_min_max_wm_mud_pi),
    MUIF_U8G2_U8_MIN_MAX("Y4", &mui_minutes, 0, 59, mui_u8g2_u8_min_max_wm_mud_pi),
    MUIF_RO("YA", mui_clock_get),
    MUIF_BUTTON("YB", mui_clock_set),
};

static void mui_setup()
{
    const u8g2_cb_t *screen_rot[] = {&u8g2_cb_r0, &u8g2_cb_r1, &u8g2_cb_r2, &u8g2_cb_r3};
    const u8g2_cb_t *u8g2_rot     = screen_rot[settings.screen_rotation % 4];
    // Initialization
#if 1
    u8g2_Setup_sh1107_128x128_f(&u8g2, u8g2_rot, u8x8_byte_rtthread_4wire_hw_spi, u8x8_gpio_and_delay_rtthread);
    u8g2_GetU8x8(&u8g2)->x_offset = 0;
#endif
#if 0
    u8g2_Setup_sh1107_seeed_128x128_f(&u8g2, u8g2_rot, u8x8_byte_rtthread_4wire_hw_spi, u8x8_gpio_and_delay_rtthread);
#endif

    u8x8_SetPin(u8g2_GetU8x8(&u8g2), U8X8_PIN_CS, DISP_CS_PIN);
    u8x8_SetPin(u8g2_GetU8x8(&u8g2), U8X8_PIN_DC, DISP_DC_PIN);
    u8x8_SetPin(u8g2_GetU8x8(&u8g2), U8X8_PIN_RESET, DISP_RST_PIN);

    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);
    u8g2_SetFontMode(&u8g2, 1);
    u8g2_ClearBuffer(&u8g2);
    u8g2_SendBuffer(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_unifont);
    u8g2_SetFontDirection(&u8g2, 0);
    u8g2_SetFontRefHeightAll(&u8g2);
    /* restore display brightness from settings */
    u8g2_SetContrast(&u8g2, settings.screen_brightness);
    u8g2_FirstPage(&u8g2);
    do {
        u8g2_DrawStr(&u8g2, 0, 15, "Hello World!");
        u8g2_DrawUTF8(&u8g2, 0, 40, "大家好");
    } while (u8g2_NextPage(&u8g2));

    /* later: choose fds_data language according to settings.language */
    mui_Init(&mui, &u8g2, fds_data_en, muif_list, sizeof(muif_list) / sizeof(muif_t));
    mui_GotoForm(&mui, /* form_id= */ 0, /* initial_cursor_position= */ 0);
}

static uint8_t is_redraw = 1;

static void mui_loop()
{
    /* handle events */
    switch (u8x8_GetMenuEvent(u8g2_GetU8x8(&u8g2)))
    {
    case U8X8_MSG_GPIO_MENU_SELECT:
        mui_SendSelect(&mui);
        is_redraw = 1;
        break;
    case U8X8_MSG_GPIO_MENU_NEXT:
        mui_NextField(&mui);
        is_redraw = 1;
        break;
    case U8X8_MSG_GPIO_MENU_PREV:
        mui_PrevField(&mui);
        is_redraw = 1;
        break;
    case U8X8_MSG_GPIO_MENU_UP:
        mui_SendValueIncrement(&mui);
        is_redraw = 1;
        break;
    case U8X8_MSG_GPIO_MENU_DOWN:
        mui_SendValueDecrement(&mui);
        is_redraw = 1;
        break;
    case U8X8_MSG_GPIO_MENU_HOME:
        mui_GotoForm(&mui, 0, 0);
        is_redraw = 1;
        break;
    case U8X8_MSG_DISPLAY_REFRESH:
        is_redraw = 1;
        break;
    default:
        break;
    }
    /* check whether the menu is active */
    if (mui_IsFormActive(&mui))
    {
        /* menu is active: draw the menu */
        if (is_redraw)
        {
            u8g2_FirstPage(&u8g2);
            do {
                mui_Draw(&mui);
            } while (u8g2_NextPage(&u8g2));
            is_redraw = 0;
        }
    }
}

static void mui_thread(void *parameter)
{
    mui_setup();
    for (;;)
        mui_loop();
}

static int mui_init(void)
{
    rt_thread_t mui_threadid = RT_NULL;

    LOG_I("init");
    mui_threadid = rt_thread_create("mui", mui_thread, RT_NULL, MUI_STACK, 20, 10);
    if (mui_threadid != RT_NULL)
        rt_thread_startup(mui_threadid);
}

INIT_COMPONENT_EXPORT(mui_init);

#ifdef RT_USING_FINSH

static void u8g2_print_callback(const char *str)
{
    rt_kprintf("%s", str);
}

// shell command to print screen in pbm format
static void printscreen_cmd(int argc, char **argv)
{
    rt_kprintf("\n# PBM output begin\n");
    u8g2_WriteBufferPBM(&u8g2, u8g2_print_callback);
    rt_kprintf("\n# PBM output end\n");
}

MSH_CMD_EXPORT_ALIAS(printscreen_cmd, printscreen, screenshot in pbm format);

#endif
