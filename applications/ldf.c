/* linux-style df "disk free" */

#include <rtthread.h>
#include <rtconfig.h>
#include <finsh.h>
#include <dfs.h>
#include <dfs_fs.h>
#include <dfs_file.h>

/* display a single filesystem given its mount point */
static void df_display_fs(const char *mountpoint)
{
    struct dfs_filesystem *fs;
    struct statfs          fs_stat;
    const char            *fs_name;

    /* look up the filesystem for this path */
    fs = dfs_filesystem_lookup(mountpoint);
    if (fs == NULL || fs->ops == NULL || fs->ops->statfs == NULL)
        return;

    /* get filesystem stats */
    if (fs->ops->statfs(fs, &fs_stat) != 0)
        return;

    /* determine name */
    if (fs->dev_id != NULL)
        fs_name = fs->dev_id->parent.name;
    else
        fs_name = fs->ops->name;

    /* calculate sizes in 1K blocks */
    uint64_t l_blocks = fs_stat.f_blocks;
    uint64_t l_bfree  = fs_stat.f_bfree;
    uint64_t l_bsize  = fs_stat.f_bsize;

    uint64_t l_total_kb = (l_blocks * l_bsize) / 1024ULL;
    uint64_t l_free_kb  = (l_bfree * l_bsize) / 1024ULL;
    uint64_t l_used_kb  = ((l_blocks - l_bfree) * l_bsize) / 1024ULL;

    uint32_t total_kb = l_total_kb;
    uint32_t free_kb  = l_free_kb;
    uint32_t used_kb  = l_used_kb;

    int percent = (total_kb > 0) ? (int)((uint64_t)used_kb * 100 / total_kb) : 0;

    /* output in linux format */
    rt_kprintf("%-12s %9d %9d %9d %3d%% %s\n", fs_name, total_kb, used_kb, free_kb, percent, mountpoint);
}

/* linux df command */
static int cmd_ldf(int argc, char **argv)
{
    int i;

#ifdef RT_USING_DFS_V2
    rt_kprintf("dfs v2 not supported\r\n");
    return -1;
#endif

    /* print header */
    rt_kprintf("Filesystem   1K-blocks      Used Available Use%% Mounted on\n");

    if (argc == 1)
    {
        /* no arguments - show all mounted filesystems */
        extern struct dfs_filesystem filesystem_table[];
        struct dfs_filesystem       *iter;

        for (iter = &filesystem_table[0];
             iter < &filesystem_table[DFS_FILESYSTEMS_MAX]; iter++)
        {
            if (iter->path != NULL && iter->ops != NULL && iter->ops->statfs != NULL)
                df_display_fs(iter->path);
        }
    }
    else
    {
        /* arguments given - show each specified mount point */
        for (i = 1; i < argc; i++)
            df_display_fs(argv[i]);
    }

    return 0;
}

MSH_CMD_EXPORT_ALIAS(cmd_ldf, ldf, show filesystem disk usage);
