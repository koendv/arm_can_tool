#include "settings.h"
#include "at24c256.h"
#include <rtthread.h>
#define DBG_TAG "EEPROM"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>
#include <drv_gpio.h>
#include "pins.h"

_Static_assert(SETTINGS_PRESET_SIZE >= sizeof(settings_struct),
               "SETTINGS_PRESET_SIZE too small for settings_struct");
_Static_assert(SETTINGS_PRESET_SIZE % EEPROM_PAGE_SIZE == 0,
               "SETTINGS_PRESET_SIZE must be a multiple of EEPROM_PAGE_SIZE");
_Static_assert(SETTINGS_PRESET_SIZE *(SETTINGS_MAX_PRESET + 1) <= EEPROM_MEMORY_SIZE,
               "presets exceed EEPROM capacity");

/* field descriptor table settings print and set */
typedef struct
{
    const char *name;
    const void *ptr;
    bool        is_bool;
} field_desc_t;

#define FIELD_U8(f)   {#f, &settings.f, false}
#define FIELD_BOOL(f) {#f, &settings.f, true}

static const field_desc_t field_table[] =
    {
        FIELD_U8(version),
        FIELD_U8(language),
        FIELD_U8(mode),
        FIELD_U8(polling_interval),
        FIELD_BOOL(attach_enable),
        FIELD_BOOL(memwatch_enable),
        FIELD_BOOL(trigger_enable),
        FIELD_BOOL(lua_autoexec),
        FIELD_BOOL(logging_enable),
        FIELD_BOOL(watchdog_enable),
        FIELD_BOOL(fileio_enable),
        FIELD_BOOL(shell_enable),
        FIELD_BOOL(rtt_enable),
        FIELD_BOOL(tpower_enable),
        FIELD_BOOL(toutput_enable),
        FIELD_BOOL(swo_decode),
        FIELD_BOOL(swap_rxd_txd),
        FIELD_U8(serial0_speed),
        FIELD_U8(serial1_speed),
        FIELD_U8(serial2_speed),
        FIELD_BOOL(serial0_enable),
        FIELD_BOOL(serial1_enable),
        FIELD_BOOL(serial2_enable),
        FIELD_BOOL(canfilter_enable),
        FIELD_BOOL(can1_enable),
        FIELD_U8(can1_speed),
        FIELD_BOOL(can1_log),
        FIELD_U8(cdc0_out),
        FIELD_U8(screen_brightness),
        FIELD_U8(screen_sleep_time),
        FIELD_U8(screen_rotation),
        FIELD_BOOL(swap_buttons),
        FIELD_U8(memwatch_cnt),
        FIELD_BOOL(memwatch_timestamp),
};

#define FIELD_TABLE_SIZE (sizeof(field_table) / sizeof(field_table[0]))

static uint32_t crc32(const uint8_t *buf, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    while (len--)
    {
        crc ^= *buf++;
        for (int i = 0; i < 8; i++)
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
    }
    return crc ^ 0xFFFFFFFF;
}

uint32_t calc_crc()
{
    return crc32((const uint8_t *)&settings, sizeof(settings));
}

static const settings_struct default_settings =
    {
        .timestamp          = 0,
        .version            = SETTINGS_VERSION,
        .language           = LANG_EN,
        .mode               = MODE_GDB_SERVER,
        .polling_interval   = DEFAULT_POLLING,
        .attach_enable      = false,
        .memwatch_enable    = false,
        .trigger_enable     = false,
        .lua_autoexec       = false,
        .logging_enable     = false,
        .watchdog_enable    = false,
        .fileio_enable      = false,
        .shell_enable       = false,
        .rtt_enable         = false,
        .tpower_enable      = false,
        .toutput_enable     = true,
        .swo_decode         = false,
        .swap_rxd_txd       = false,
        .serial0_speed      = 6,  /* 115200 */
        .serial1_speed      = 6,  /* 115200 */
        .serial2_speed      = 12, /* 1000000 */
        .serial0_enable     = true,
        .serial1_enable     = true,
        .serial2_enable     = true,
        .canfilter_enable   = false,
        .can1_enable        = false,
        .can1_speed         = 8, /* 500000 */
        .can1_log           = false,
        .can1_hw_filter     = {0},
        .cdc0_out           = CDC0_OUT_SERIAL0,
        .screen_brightness  = 192,
        .screen_sleep_time  = 1,
        .screen_rotation    = 3,
        .swap_buttons       = false,
        .memwatch_table     = {0},
        .memwatch_cnt       = 0,
        .memwatch_timestamp = false,
};

settings_struct settings = default_settings;

uint8_t settings_preset = 0;     /* index in settings eeprom */
bool    settings_done   = false; /* true when settings read from eeprom */

static void set_preset(const char *s)
{
    if (!s) return;

    uint32_t n = (uint8_t)strtoul(s, NULL, 0);
    if (n > SETTINGS_MAX_PRESET)
        n = SETTINGS_MAX_PRESET;
    rt_kprintf("preset %u\r\n", n);
    settings_preset = n;
}

void reset_settings()
{
    settings = default_settings;
}

void store_settings(uint8_t preset)
{
    if (preset > SETTINGS_MAX_PRESET)
        preset = SETTINGS_MAX_PRESET;

    uint32_t eeprom_address = preset * SETTINGS_PRESET_SIZE;
    if (eeprom_address % EEPROM_PAGE_SIZE != 0)
        LOG_E("eeprom_address page error");

    /* update timestamp */
    settings.timestamp = time(RT_NULL);

    /* copy memwatch settings */
    memcpy(settings.memwatch_table, memwatch_table, sizeof(memwatch_table));
    for (uint32_t i = 0; i < MEMWATCH_NUM; i++)
        settings.memwatch_table[i].value = 0;
    settings.memwatch_cnt       = memwatch_cnt;
    settings.memwatch_timestamp = memwatch_timestamp;

    /* copy can bus hardware filter settings */
    can_get_filter(&settings.can1_hw_filter);

    /* calculate crc */
    settings.crc = 0;
    settings.crc = calc_crc();

    /* save settings to eeprom */
    at24_write(eeprom_address, (uint8_t *)&settings, sizeof(settings));
}


void recall_settings(uint8_t preset)
{
    uint32_t saved_crc, calculated_crc;

    if (preset > SETTINGS_MAX_PRESET)
        preset = SETTINGS_MAX_PRESET;

    uint32_t eeprom_address = preset * SETTINGS_PRESET_SIZE;
    if (eeprom_address % EEPROM_PAGE_SIZE != 0)
        LOG_E("eeprom_address page error");

    /* restore settings from eeprom */
    at24_read(eeprom_address, (uint8_t *)&settings, sizeof(settings));

    /* restore memwatch settings */
    if (settings.memwatch_enable)
    {
        memcpy(memwatch_table, settings.memwatch_table, sizeof(memwatch_table));
        memwatch_cnt       = settings.memwatch_cnt;
        memwatch_timestamp = settings.memwatch_timestamp;
        LOG_I("memwatch init");
    }
    else
    {
        memset(memwatch_table, 0, sizeof(memwatch_table));
        memwatch_cnt       = 0;
        memwatch_timestamp = 0;
    }

    /* check crc */
    saved_crc      = settings.crc;
    settings.crc   = 0;
    calculated_crc = calc_crc();
    if ((calculated_crc != saved_crc) || (settings.version != SETTINGS_VERSION))
    {
        LOG_I("first run, resetting settings");
        reset_settings();
        store_settings(settings_preset);
    }
}

static bool is_valid_string(const char *s, size_t maxlen)
{
    if (s == NULL) return 0;

    for (size_t i = 0; i < maxlen; i++)
    {
        if (s[i] == '\0')
        {
            return true; // properly terminated and all previous chars checked
        }
        if (!isprint((unsigned char)s[i]))
        {
            return false; // found non-printable character
        }
    }
    return false; // no null terminator found within maxlen
}

void list_settings()
{
    uint8_t val;

    /* settings */
    rt_kprintf("[arm_can_tool]\r\n");
    for (uint32_t i = 0; i < FIELD_TABLE_SIZE; i++)
    {
        val = *(uint8_t *)field_table[i].ptr;
        rt_kprintf("%s=%u\r\n", field_table[i].name, val);
    }

    /* can filter */
    rt_kprintf("[canfilter]\r\n");
    rt_kprintf("dev=%u\r\n", settings.can1_hw_filter.dev);
    rt_kprintf("fs1r=0x%08X\r\n", settings.can1_hw_filter.fs1r);
    rt_kprintf("fm1r=0x%08X\r\n", settings.can1_hw_filter.fm1r);
    rt_kprintf("ffa1r=0x%08X\r\n", settings.can1_hw_filter.ffa1r);
    rt_kprintf("fa1r=0x%08X\r\n", settings.can1_hw_filter.fa1r);

    for (int i = 0; i < 14; i++)
    {
        rt_kprintf("fr1_%02d=0x%08X\r\n", i, settings.can1_hw_filter.fr1[i]);
        rt_kprintf("fr2_%02d=0x%08X\r\n", i, settings.can1_hw_filter.fr2[i]);
    }

    /* memwatch */
    rt_kprintf("[memwatch]\r\n");
    rt_kprintf("enable=%u\r\n", settings.memwatch_enable);
    rt_kprintf("timestamp=%u\r\n", settings.memwatch_timestamp);
    rt_kprintf("count=%u\r\n", settings.memwatch_cnt);

    for (uint32_t i = 0; i < MEMWATCH_NUM && i < settings.memwatch_cnt; i++)
    {
        rt_kprintf("addr%u=0x%08X\r\n", i, memwatch_table[i].addr);
        rt_kprintf("format%u=%u\r\n", i, memwatch_table[i].format);
        rt_kprintf("precision%u=%u\r\n", i, memwatch_table[i].precision);
        char *s;
        if (is_valid_string(memwatch_table[i].name, MEMWATCH_STRLEN))
            s = memwatch_table[i].name;
        else
            s = "?";
        rt_kprintf("name%u=%s\r\n", i, s);
    }
    rt_kprintf("[end]\r\n");
}

static void set_value(const char *name, const char *valstr)
{
    uint32_t val = strtoul(valstr, NULL, 0);

    for (uint32_t i = 0; i < FIELD_TABLE_SIZE; i++)
    {
        if (!strcmp(field_table[i].name, name))
        {
            if (field_table[i].is_bool)
            {
                *((bool *)(uintptr_t)field_table[i].ptr) = val ? true : false;
                rt_kprintf("%s=%u\r\n", name, (uint8_t) * ((const bool *)field_table[i].ptr));
            }
            else
            {
                *((uint8_t *)(uintptr_t)field_table[i].ptr) = (uint8_t)val;
                rt_kprintf("%s=%u\r\n", name, *((const uint8_t *)field_table[i].ptr));
            }
            return;
        }
    }

    rt_kprintf("unknown field: %s\r\n", name);
    return;
}


int init_settings()
{
    /* use default settings if multi-direction switch is rotated clockwise at boot */
    rt_pin_mode(SW_PREV_PIN, PIN_MODE_INPUT_PULLUP);
    bool boot_with_default = !rt_pin_read(SW_PREV_PIN);

    if (boot_with_default)
    {
        reset_settings();
        LOG_I("using default settings");
        return RT_EOK;
    }
    recall_settings(0);
    settings_done = true;
    LOG_I("settings %d byte", sizeof(settings));

    /* canbus initialization at boot */
    if (settings.can1_enable)
        can_init(settings.can1_enable);

    return RT_EOK;
}
INIT_DEVICE_EXPORT(init_settings);

#ifdef RT_USING_FINSH
static int cmd_settings(int argc, char **argv)
{
    if (argc == 2 && !strncmp(argv[1], "read", strlen(argv[1])))
        recall_settings(settings_preset);
    else if (argc == 2 && !strncmp(argv[1], "write", strlen(argv[1])))
        store_settings(settings_preset);
    else if (argc == 2 && !strncmp(argv[1], "default", strlen(argv[1])))
        reset_settings();
    else if (argc == 2 && !strncmp(argv[1], "list", strlen(argv[1])))
        list_settings();
    else if (argc == 4 && !strncmp(argv[1], "set", strlen(argv[1])))
        set_value(argv[2], argv[3]);
    else if (argc == 3 && !strncmp(argv[1], "preset", strlen(argv[1])))
        set_preset(argv[2]);
    else
        rt_kprintf("%s (read|write|default|list|set <field> <value>|preset [0-15])\r\n", argv[0]);
    return RT_EOK;
}

MSH_CMD_EXPORT_ALIAS(cmd_settings, settings, save settings in eeprom);
#endif
