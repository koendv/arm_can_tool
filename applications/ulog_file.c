#include <rtthread.h>
#include <rtconfig.h>
#include <dfs_file.h>

#define DBG_TAG "ULOG"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>
#include <ulog_be.h>

#include "ulog_file.h"
#include "settings.h"

/* ulog logging to sdcard */

#define ULOG_FILE_SIZE (1024 * 1024)
#define ULOG_FILE_NUM  10
#define ULOG_BUF_SIZE  1024

static struct ulog_file_be file_be;

static bool is_dir(const char *path)
{
    struct stat sb;
    if (dfs_file_stat(path, &sb) == 0)
        return S_ISDIR(sb.st_mode);
    return false;
}

/*
   ulog to file. 
   ulog_dir: directory where logs are kept.
   ulog_file_name: log file name
 */

void ulog_file(const char *ulog_dir, const char *ulog_file_name)
{
    RT_ASSERT(ulog_dir != RT_NULL);

    if (!is_dir(ulog_dir))
    {
        LOG_E("%s not a directory", ulog_dir);
        return;
    }

    ulog_file_backend_init(&file_be,
                           ulog_file_name, // 日志文件名前缀
                           ulog_dir,       // 目录
                           ULOG_FILE_NUM,  // 保留多少个旧文件
                           ULOG_FILE_SIZE, // 每个文件最大字节
                           ULOG_BUF_SIZE); // 缓存大小

    /* switch on logging to file */
    ulog_file_backend_enable(&file_be);

    LOG_I("logging to %s/%s", ulog_dir, ulog_file_name);
}

#ifdef RT_USING_FINSH
static int cmd_ulog(int argc, char **argv)
{
    uint32_t length;
    char     msg[ULOG_LINE_BUF_SIZE];

    if (argc == 2 && !strncmp(argv[1], "-c", strlen(argv[1])))
    {
        /* switch off logging to console */
        ulog_backend_t console_be = ulog_backend_find("console");
        if (console_be)
            ulog_backend_unregister(console_be);
    }
    else if (argc >= 2)
    {
        /* log command arguments */
        char *s = msg;
        for (int i = 1; i < argc; i++)
        {
            if (strlen(s) + strlen(argv[i]) >= sizeof(msg)) break;
            strcpy(s, argv[i]);
            s    += strlen(argv[i]);
            *s++  = ' ';
        }
        *--s = '\0';
        LOG_I("%s", msg);
    }
    else
        rt_kprintf("%s (-c|\"message\")\n", argv[0]);
    return RT_EOK;
}

MSH_CMD_EXPORT_ALIAS(cmd_ulog, ulog, write arguments to system log);
#endif

static int ulog_file_init(void)
{
    if (settings.logging_enable)
    {
        ulog_file("/sdcard/log", "rtthread");
    }
    return RT_EOK;
}

INIT_APP_EXPORT(ulog_file_init);
