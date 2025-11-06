#include <rtthread.h>
#include <ff.h>
#include <logger_elmfat.h>

#define DBG_TAG "ELM"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

/*
 * find index of last log file.
 */

extern int32_t find_last_log(const char *log_dir_path, const char *log_file_fmt)
{
    DIR     dir;
    FILINFO fno;
    FRESULT res;
    int     max_index = -1;
    int     index;

    res = f_opendir(&dir, log_dir_path);

    if (res != FR_OK)
    {
        LOG_E("failed to open directory %s (err %d)\n", log_dir_path, res);
        return -1;
    }

    while (1)
    {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0)
            break; // end of directory

        if (fno.fattrib & AM_DIR)
            continue; // skip subdirectory

        if ((rt_sscanf(fno.fname, log_file_fmt, &index) == 1) && (index > max_index))
            max_index = index; // highest-numbered log file so far
    }

    f_closedir(&dir);

    return max_index;
}

