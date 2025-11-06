#include <rtthread.h>
#include <rtdevice.h>
#include <stdbool.h>
#include <dfs_fs.h>
#include <dfs_file.h>
#include <logger_elmfat.h>

#define DBG_TAG "LOG"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>
#include "settings.h"

/* usb serial cdc1 logging to sdcard */

#define LOG_DIR             "/sdcard/log"
#define LOG_ELM_DIR         "/log"
#define LOG_FILENAME_FORMAT "log_%04d.txt"
#define LOG_SIZE_MAX        (4 * 1024 * 1024)
#define LOG_BUF_SIZE        2048
#define LOG_SYNC_BYTES      512
#define LOG_NAME_MAX        128

static struct rt_ringbuffer *log_rb        = RT_NULL;
static rt_sem_t              log_sync_sem  = RT_NULL;
static rt_thread_t           log_thread_id = RT_NULL;
static bool                  log_open      = false;
static bool                  log_flush     = false;
static int32_t               log_index     = -1;
static struct dfs_file       fd;

static bool is_dir(const char *path)
{
    struct stat sb;
    if (dfs_file_stat(path, &sb) == 0)
        return S_ISDIR(sb.st_mode);
    return false;
}

/* write a ringbuffer to file. */

rt_size_t ringbuffer_write(struct dfs_file *fd, struct rt_ringbuffer *rb)
{
    /* this is rt_ringbuffer_get(), 
     * modified to use dfs_file_write() instead of rt_memcpy() 
     * avoids copying data. */

    rt_uint32_t length;

    RT_ASSERT(fd != RT_NULL);
    RT_ASSERT(rb != RT_NULL);

    /* whether has enough data  */
    length = rt_ringbuffer_data_len(rb);

    /* no data */
    if (length == 0)
        return 0;

    if (rb->buffer_size - rb->read_index > length)
    {
        /* write all of data */
        dfs_file_write(fd,
                       &rb->buffer_ptr[rb->read_index],
                       length);

        /* this should not cause overflow because there is enough space for
         * length of data in current mirror */
        rb->read_index += length;
        return length;
    }

    dfs_file_write(fd,
                   &rb->buffer_ptr[rb->read_index],
                   rb->buffer_size - rb->read_index);
    dfs_file_write(fd,
                   &rb->buffer_ptr[0],
                   length - (rb->buffer_size - rb->read_index));

    /* we are going into the other side of the mirror */
    rb->read_mirror = ~rb->read_mirror;
    rb->read_index  = length - (rb->buffer_size - rb->read_index);

    return length;
}

static void log_rotate()
{
    char fname[LOG_NAME_MAX];

    if (log_open)
    {
        dfs_file_flush(&fd); // to be sure
        dfs_file_close(&fd);
        log_open = false;

        /* next log file */
        log_index += 1;
        if (log_index < 0) log_index = 0;
        if (log_index > 9999) log_index = 9999;
    }

    /* log on sdcard */
    rt_snprintf(fname, sizeof(fname), LOG_DIR "/" LOG_FILENAME_FORMAT, log_index);
    log_open = dfs_file_open(&fd, fname, O_RDWR | O_CREAT | O_APPEND) >= 0;

    if (log_open)
        LOG_I("logfile %s", fname);
    else
        LOG_E("open logfile %s fail", fname);

    return;
}

static void log_sync_thread(void *parameter)
{
    while (1)
    {
        rt_sem_take(log_sync_sem, RT_WAITING_FOREVER);
        ringbuffer_write(&fd, log_rb);
        if (log_flush)
        {
            dfs_file_flush(&fd);
            log_flush = false;
            LOG_I("flush");
        }
        if (fd.vnode && fd.vnode->size >= LOG_SIZE_MAX) log_rotate;
    }
}

void logger(char *buf, uint32_t len)
{
    static uint32_t bytes_written = 0;

    if (log_rb == RT_NULL || buf == RT_NULL) return;

    rt_ringbuffer_put(log_rb, (const rt_uint8_t *)buf, (rt_uint16_t)len);
    bytes_written += len;

    if (log_sync_sem && bytes_written > LOG_SYNC_BYTES)
    {
        rt_sem_release(log_sync_sem);
        bytes_written = 0;
    }
}

static int logger_init(void)
{
    if (!settings.logging_enable)
        return RT_EOK;

    log_rb        = rt_ringbuffer_create(LOG_BUF_SIZE);
    log_sync_sem  = rt_sem_create("log sync", 0, RT_IPC_FLAG_FIFO);
    log_thread_id = rt_thread_create("log_sync", log_sync_thread, RT_NULL, 1024, 25, 10);
    log_index     = find_last_log(LOG_ELM_DIR, LOG_FILENAME_FORMAT);
    if (log_index < 0)
        log_index = 0;
    log_rotate();
    if (log_thread_id)
    {
        rt_thread_startup(log_thread_id);
        return RT_EOK;
    }
    else
    {
        LOG_E("log sync thread fail");
        return -RT_ERROR;
    }
}

INIT_APP_EXPORT(logger_init);

#ifdef RT_USING_FINSH
static int cmd_logger(int argc, char **argv)
{
    uint32_t length;
    char     msg[128];

    if (argc == 2 && !strncmp(argv[1], "-f", strlen(argv[1])))
    {
        /* flush log file */
        log_flush = true;
        if (log_sync_sem)
            rt_sem_release(log_sync_sem);
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
        logger(msg, strlen(msg));
    }
    else
        rt_kprintf("%s (-f|\"message\")\r\n", argv[0]);
}

MSH_CMD_EXPORT_ALIAS(cmd_logger, logger, log arguments to file);
#endif
