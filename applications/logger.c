#include <rtthread.h>
#include <rtdevice.h>
#include <stdbool.h>
#include <dfs_fs.h>
#include <dfs_file.h>

#define DBG_TAG "LOG"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>
#include "settings.h"
#include "logger.h"

/* logging to sdcard */

#define LOG_DIR          "/sdcard"
#define LOG_SIZE_MAX     (4 * 1024 * 1024)
#define LOG_FNAME_MAX    (128)
#define LOG_BUF_SIZE     2048 /* must be power of two */
#define LOG_BUF_MASK     (LOG_BUF_SIZE - 1)
#define LOG_SYNC_BYTES   512
#define LOG_SYNC_SECONDS 120

/* ringbuffer structure */
static struct
{
    uint8_t *buffer; /* dynamically allocated buffer, NULL when inactive */
    uint32_t head;   /* write index */
    uint32_t tail;   /* read index */
} log_rb = {0};      /* ensure buffer starts as NULL */

static rt_mutex_t      log_mutex  = RT_NULL;
static rt_sem_t        log_sem    = RT_NULL;
static rt_thread_t     log_thread = RT_NULL;
static bool            log_open   = false;
static struct dfs_file log_fd;
static uint32_t        log_file_size     = 0; /* size of current log file */
static uint32_t        log_bytes_logged  = 0; /* total bytes passed to logger() */
static uint32_t        log_bytes_written = 0; /* total bytes written to file */

/* inline function to get current data length in ringbuffer */
static inline uint32_t rb_data_len(void)
{
    if (log_rb.buffer == RT_NULL)
        return 0;

    return (log_rb.head - log_rb.tail) & LOG_BUF_MASK;
}

/* put data into ringbuffer */
static void rb_put(const uint8_t *data, uint32_t len)
{
    uint32_t space;
    uint32_t to_end;

    /* check if buffer is available */
    if (log_rb.buffer == RT_NULL)
        return;

    /* calculate available space (keep one byte free to detect full/empty) */
    space = LOG_BUF_SIZE - rb_data_len() - 1;

    if (len > space)
        len = space;

    if (len == 0)
        return;

    to_end = LOG_BUF_SIZE - log_rb.head;

    if (len <= to_end)
    {
        /* fits without wrapping */
        rt_memcpy(&log_rb.buffer[log_rb.head], data, len);
        log_rb.head += len;
    }
    else
    {
        /* wraps around */
        rt_memcpy(&log_rb.buffer[log_rb.head], data, to_end);
        rt_memcpy(log_rb.buffer, data + to_end, len - to_end);
        log_rb.head = len - to_end;
    }

    /* mask head to keep within bounds */
    log_rb.head &= LOG_BUF_MASK;

    return;
}

/* write ringbuffer contents to file */
static uint32_t rb_write_to_file(struct dfs_file *fd)
{
    uint32_t len;
    uint32_t to_end;
    uint32_t written = 0;

    /* check if buffer is available */
    if (log_rb.buffer == RT_NULL)
        return 0;

    len = rb_data_len();

    if (len == 0)
        return 0;

    to_end = LOG_BUF_SIZE - log_rb.tail;

    if (len <= to_end)
    {
        /* read without wrapping */
        written = dfs_file_write(fd, &log_rb.buffer[log_rb.tail], len);
        if (written > 0)
            log_rb.tail = (log_rb.tail + written) & LOG_BUF_MASK;
    }
    else
    {
        /* wraps around - write first part */
        written = dfs_file_write(fd, &log_rb.buffer[log_rb.tail], to_end);
        if (written > 0)
        {
            log_rb.tail = (log_rb.tail + written) & LOG_BUF_MASK;

            /* write second part if first part succeeded */
            if (written == to_end)
            {
                uint32_t second_written;
                second_written = dfs_file_write(fd, log_rb.buffer, len - to_end);
                if (second_written > 0)
                {
                    written     += second_written;
                    log_rb.tail  = (log_rb.tail + second_written) & LOG_BUF_MASK;
                }
            }
        }
    }

    return written;
}

void logger_flush(void)
{
    if (log_sem && log_rb.buffer != RT_NULL)
        rt_sem_release(log_sem);
}

void logger(const char *buf, const uint32_t len)
{
    if (buf == RT_NULL || len == 0 || log_mutex == RT_NULL || log_rb.buffer == RT_NULL)
        return;

    rt_mutex_take(log_mutex, RT_WAITING_FOREVER);

    /* update stats */
    log_bytes_logged += len;

    /* try to put data in ringbuffer */
    rb_put((const uint8_t *)buf, len);

    rt_mutex_release(log_mutex);

    /* trigger flush if buffer is getting full */
    if (rb_data_len() > LOG_SYNC_BYTES)
        logger_flush();
}

static void logger_rotate(void)
{
    char           fname[LOG_FNAME_MAX];
    struct timeval tv = {0};

    if (log_open)
    {
        dfs_file_flush(&log_fd);
        dfs_file_close(&log_fd);
        log_open = false;
    }

    /* log on sdcard */
    gettimeofday(&tv, RT_NULL);
    rt_snprintf(fname, sizeof(fname), LOG_DIR "/%08lx.log", (long)tv.tv_sec);
    log_open = dfs_file_open(&log_fd, fname, O_RDWR | O_CREAT | O_APPEND) == 0;

    if (log_open)
        LOG_I("logfile %s", fname);
    else
        LOG_E("open logfile %s fail", fname);
}

void logger_thread(void *parameter)
{
    uint32_t written;

    if (log_sem == RT_NULL || log_rb.buffer == RT_NULL)
    {
        LOG_E("logger not initialized");
        return;
    }

    logger_rotate();

    while (1)
    {
        rt_sem_take(log_sem, RT_TICK_PER_SECOND * LOG_SYNC_SECONDS);

        written = rb_write_to_file(&log_fd);
        if (written > 0)
        {
            dfs_file_flush(&log_fd);
            log_bytes_written += written;
            log_file_size     += written;
        }

        if (log_file_size >= LOG_SIZE_MAX)
        {
            logger_rotate();
            log_file_size = 0;
        }
    }
}

static int logger_init(void)
{
    /* verify buffer size is power of two */
    RT_ASSERT((LOG_BUF_SIZE & (LOG_BUF_SIZE - 1)) == 0);

    /* don't log to sd card when exporting the sd card over usb */
    if (!settings.logging_enable || settings.mode == MODE_MASS_STORAGE)
    {
        return RT_EOK;
    }

    /* check log directory exists */
    struct stat st;
    if (stat(LOG_DIR, &st) != 0)
    {
        LOG_E("directory " LOG_DIR " not found");
        return -RT_ERROR;
    }

    /* create synchronization objects first */
    log_mutex = rt_mutex_create("log_mutex", RT_IPC_FLAG_FIFO);
    if (log_mutex == RT_NULL)
    {
        LOG_E("create mutex fail");
        return -RT_ERROR;
    }

    log_sem = rt_sem_create("logger", 0, RT_IPC_FLAG_FIFO);
    if (log_sem == RT_NULL)
    {
        LOG_E("create sem fail");
        rt_mutex_delete(log_mutex);
        log_mutex = RT_NULL;
        return -RT_ERROR;
    }
    rt_sem_control(log_sem, RT_IPC_CMD_SET_VLIMIT, (void *)1);

    /* allocate buffer last - this activates the logger */
    uint8_t *newbuf = (uint8_t *)rt_malloc(LOG_BUF_SIZE);
    if (newbuf == RT_NULL)
    {
        LOG_E("failed to allocate buffer (%d bytes)", LOG_BUF_SIZE);
        rt_sem_delete(log_sem);
        log_sem = RT_NULL;
        rt_mutex_delete(log_mutex);
        log_mutex = RT_NULL;
        return -RT_ENOMEM;
    }

    /* initialize buffer and indices */
    rt_memset(newbuf, 0, LOG_BUF_SIZE);
    log_rb.head   = 0;
    log_rb.tail   = 0;
    log_rb.buffer = newbuf; /* logging starts as soon as buffer != RT_NULL */

    /* start thread to write to file. dfs needs 2kbyte stack typically */
    log_thread = rt_thread_create("logger", logger_thread, RT_NULL, 2048, 25, 10);
    if (log_thread == RT_NULL)
    {
        LOG_E("start thread fail");
        rt_free(log_rb.buffer);
        log_rb.buffer = RT_NULL;
        rt_sem_delete(log_sem);
        log_sem = RT_NULL;
        rt_mutex_delete(log_mutex);
        log_mutex = RT_NULL;
        return -RT_ERROR;
    }

    rt_thread_startup(log_thread);

    LOG_I("buffer %d byte", LOG_BUF_SIZE);
    return RT_EOK;
}

INIT_APP_EXPORT(logger_init);

#ifdef RT_USING_FINSH
static int cmd_logger(int argc, char **argv)
{
    uint32_t pending;
    uint32_t dropped;
    char     msg[128];

    if (log_rb.buffer == RT_NULL)
    {
        rt_kprintf("logging inactive\r\n");
        return -1;
    }

    if (argc == 2 && strlen(argv[1]) == 2 && argv[1][0] == '-')
    {
        switch (argv[1][1])
        {
        case 'f':
            /* flush log file */
            logger_flush();
            break;
        case 'r':
            /* rotate log file */
            log_file_size = LOG_SIZE_MAX;
            logger_flush();
            break;
        case 's':
            /* show statistics */
            pending = rb_data_len();
            dropped = log_bytes_logged - log_bytes_written - pending;
            rt_kprintf("logged: %u written: %u pending: %u dropped: %u\r\n",
                       log_bytes_logged,
                       log_bytes_written,
                       pending,
                       dropped);
            break;
        default:
            rt_kprintf("what?\r\n");
            break;
        }
    }
    else if (argc >= 2)
    {
        /* log command arguments */
        char    *s         = msg;
        uint32_t remaining = sizeof(msg);

        *s = '\0';
        for (int i = 1; i < argc; i++)
        {
            uint32_t arglen = strlen(argv[i]);
            if (arglen + 1 > remaining) break;
            memcpy(s, argv[i], arglen);
            s         += arglen;
            *s++       = ' ';
            remaining -= (arglen + 1);
        }
        if (s > msg)
            *--s = '\0';
        else
            *s = '\0';
        logger(msg, strlen(msg));
    }
    else
        rt_kprintf("%s (-f|-r|-s|\"message\")\r\n", argv[0]);

    return 0;
}

MSH_CMD_EXPORT_ALIAS(cmd_logger, logger, log arguments to file);
#endif
