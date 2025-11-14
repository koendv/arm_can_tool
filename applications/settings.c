#include "settings.h"
#include "at24c256.h"
#include <rtthread.h>
#define DBG_TAG "EEPROM"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>
#include <string.h>

static uint32_t simplehash32(const uint8_t *data, uint32_t len)
{
    uint32_t hash = 0, seed = 0x30393235;
    while (len--) hash = (hash ^ seed) + *data++;
    return hash;
}

uint32_t calc_crc()
{
    return simplehash32((const uint8_t *)&settings, sizeof(settings));
}

static const settings_struct default_settings =
    {
        .version            = SETTINGS_VERSION,
        .language           = LANG_EN,
        .attach_enable      = false,
        .memwatch_enable    = false,
        .lua_enable         = false,
        .logging_enable     = false,
        .watchdog_enable    = false,
        .rtt_enable         = false,
        .tpower_enable      = false,
        .toutput_enable     = true,
        .swo_decode         = false,
        .swap_rxd_txd       = false,
        .serial0_speed      = 6,
        .serial1_speed      = 6,
        .serial2_speed      = 6,
        .serial0_enable     = true,
        .serial1_enable     = true,
        .serial2_enable     = true,
        .can1_speed         = 0,
        .can1_slcan         = true,
        .can1_hw_filter     = {0},
        .cdc1_output        = CDC1_SERIAL0,
        .screen_brightness  = 192,
        .screen_sleep_time  = 1,
        .screen_rotation    = 3,
        .swap_buttons       = false,
        .memwatch_table     = {0},
        .memwatch_cnt       = 0,
        .memwatch_timestamp = false,
};

settings_struct settings = default_settings;

void reset_settings()
{
    settings = default_settings;
}

void store_settings()
{
    /* copy memwatch settings */
    memcpy(settings.memwatch_table, memwatch_table, sizeof(memwatch_table));
    for (uint32_t i = 0; i < MEMWATCH_NUM; i++)
        settings.memwatch_table[i].value = 0;
    settings.memwatch_cnt       = memwatch_cnt;
    settings.memwatch_timestamp = memwatch_timestamp;

    /* copy can bus hardware filter settings *
    memcpy(&settings.can1_hw_filter, &can_hw_filter, sizeof(can_hw_filter));

    /* apply canbus filter */
    if (can_hw_filter.count)
        canbus_end_filter();

    /* calculate crc */
    settings.crc = 0;
    settings.crc = calc_crc();

    /* save settings to eeprom */
    at24_write(0, (uint8_t *)&settings, sizeof(settings));
}


void recall_settings()
{
    uint32_t saved_crc, calculated_crc;

    /* restore settings from eeprom */
    at24_read(0, (uint8_t *)&settings, sizeof(settings));

    /* restore memwatch settings */
    memcpy(memwatch_table, settings.memwatch_table, sizeof(memwatch_table));
    memwatch_cnt       = settings.memwatch_cnt;
    memwatch_timestamp = settings.memwatch_timestamp;

    /* restore can filter settings */
    memcpy(&can_hw_filter, &settings.can1_hw_filter, sizeof(can_hw_filter));

    /* check crc */
    saved_crc      = settings.crc;
    settings.crc   = 0;
    calculated_crc = calc_crc();
    if ((calculated_crc != saved_crc) || (settings.version != SETTINGS_VERSION))
    {
        LOG_I("first run, resetting settings");
        reset_settings();
        store_settings();
    }
}

void print_settings()
{
    rt_kprintf("crc               : %d\r\n", settings.crc);
    rt_kprintf("version           : %d\r\n", settings.version);
    rt_kprintf("language          : %d\r\n", settings.language);
    rt_kprintf("attach_enable     : %d\r\n", settings.attach_enable);
    rt_kprintf("memwatch_enable   : %d\r\n", settings.memwatch_enable);
    rt_kprintf("memwatch_cnt      : %d\r\n", settings.memwatch_cnt);
    rt_kprintf("lua_enable        : %d\r\n", settings.lua_enable);
    rt_kprintf("logging_enable    : %d\r\n", settings.logging_enable);
    rt_kprintf("rtt_enable        : %d\r\n", settings.rtt_enable);
    rt_kprintf("tpower_enable     : %d\r\n", settings.tpower_enable);
    rt_kprintf("toutput_enable    : %d\r\n", settings.toutput_enable);
    rt_kprintf("swo_decode        : %d\r\n", settings.swo_decode);
    rt_kprintf("swap_rxd_txd      : %d\r\n", settings.swap_rxd_txd);
    rt_kprintf("serial0_speed     : %d\r\n", settings.serial0_speed);
    rt_kprintf("serial1_speed     : %d\r\n", settings.serial1_speed);
    rt_kprintf("serial2_speed     : %d\r\n", settings.serial2_speed);
    rt_kprintf("serial0_enable    : %d\r\n", settings.serial0_enable);
    rt_kprintf("serial1_enable    : %d\r\n", settings.serial1_enable);
    rt_kprintf("serial2_enable    : %d\r\n", settings.serial2_enable);
    rt_kprintf("can1_speed        : %d\r\n", settings.can1_speed);
    rt_kprintf("can1_slcan        : %d\r\n", settings.can1_slcan);
    rt_kprintf("cdc1_output       : %d\r\n", settings.cdc1_output);
    rt_kprintf("screen_brightness : %d\r\n", settings.screen_brightness);
    rt_kprintf("screen_sleep_time : %d\r\n", settings.screen_sleep_time);
    rt_kprintf("screen_rotation   : %d\r\n", settings.screen_rotation);
    rt_kprintf("swap_buttons      : %d\r\n", settings.swap_buttons);
}

int init_settings()
{
    recall_settings();
    LOG_I("settings loaded");
    return RT_EOK;
}
INIT_DEVICE_EXPORT(init_settings);

#ifdef RT_USING_FINSH
static int cmd_settings(int argc, char **argv)
{
    if (argc == 2 && !strncmp(argv[1], "read", strlen(argv[1])))
        recall_settings();
    else if (argc == 2 && !strncmp(argv[1], "write", strlen(argv[1])))
        store_settings();
    else if (argc == 2 && !strncmp(argv[1], "reset", strlen(argv[1])))
        reset_settings();
    else if (argc == 2 && !strncmp(argv[1], "print", strlen(argv[1])))
        print_settings();
    else
        rt_kprintf("%s (read|write|reset|print)\r\n", argv[0]);
    return RT_EOK;
}

MSH_CMD_EXPORT_ALIAS(cmd_settings, settings, save settings in eeprom);
#endif
