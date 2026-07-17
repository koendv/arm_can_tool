/*
 * sync - flush open files.
 *
 * Walks the global DFS fd table and calls fsync() on every open fd.
 */

#include <rtthread.h>
#include <unistd.h>
#include <dfs.h>

#include "ulog.h"
#include "logger.h"

int sync_all_files(void)
{
    struct dfs_fdtable *fdt = dfs_fdtable_get();
    if (fdt == NULL)
        return -1;

#ifdef RT_USING_ULOG
    ulog_flush();
#endif

    logger_flush();

    for (int fd = 0; fd < (int)fdt->maxfd; fd++)
    {
        if (fdt->fds[fd] != NULL)
            fsync(fd);
    }
    return 0;
}

#ifdef RT_USING_FINSH
static int cmd_sync(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    return sync_all_files();
}
MSH_CMD_EXPORT_ALIAS(cmd_sync, sync, flush open files to sdcard and flash);
#endif /* RT_USING_FINSH */
