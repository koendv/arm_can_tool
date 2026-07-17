/*
 * canfilter_cmd.c
 *
 * RT-Thread MSH command: canfilter
 *
 * Parses CAN IDs and ranges from the command line, builds a bxCAN
 * hardware filter configuration (struct canfilter_bxcan_f0 / can_filter_t),
 * and programs it via can_set_filter().
 *
 * Usage:
 *   canfilter [OPTIONS] [IDs/RANGES]
 *
 *   IDs/RANGES   Decimal or hex (0x prefix), single or range (start-end).
 *                Comma or space separated. Standard if <= 0x7FF, else extended.
 *
 *   -a           Allow all packets (standard + extended).
 *   -v           Verbose; repeat up to three times for more detail.
 *   -h           Print help.
 */

#include <rtthread.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include "canfilter.h"
#include "canfilter_bxcan_f0.h"

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static void print_help(void)
{
    rt_kprintf("Usage: canfilter [OPTIONS] [IDs/RANGES]\n");
    rt_kprintf("Build and program a bxCAN hardware filter (14 banks, F0/F1/F3).\n\n");
    rt_kprintf("IDs/RANGES  0x100   single ID (hex or decimal)\n");
    rt_kprintf("            0x100-0x1FF   ID range\n");
    rt_kprintf("            IDs <= 0x7FF are standard, else extended.\n\n");
    rt_kprintf("Options:\n");
    rt_kprintf("  -a   Allow all packets\n");
    rt_kprintf("  -d   Dry run: print filter but do not program hardware\n");
    rt_kprintf("  -v   Verbose (repeat up to three times for more detail)\n");
    rt_kprintf("  -h   Show this help\n\n");
    rt_kprintf("Examples:\n");
    rt_kprintf("  canfilter 0x100\n");
    rt_kprintf("  canfilter 0x100-0x1FF\n");
    rt_kprintf("  canfilter -a\n");
    rt_kprintf("  canfilter -v 0x100 0x200-0x2FF\n");
}

static void print_cf_err(cf_err_t err)
{
    switch (err)
    {
    case CF_OK:
        rt_kprintf("canfilter: ok\n");
        break;
    case CF_PARAM:
        rt_kprintf("canfilter: invalid parameter or out of range\n");
        break;
    case CF_FULL:
        rt_kprintf("canfilter: no more filter banks available\n");
        break;
    default:
        rt_kprintf("canfilter: unknown error\n");
        break;
    }
}

/* Parse one token which may be a single ID ("0x100") or a range
 * ("0x100-0x1FF").  Tokens may also contain comma-separated lists,
 * e.g. "0x100,0x200-0x2FF".
 * Returns RT_EOK on success, -RT_ERROR on parse failure. */
static rt_err_t parse_token(cf_bxcan_f0_t *cf, const char *token)
{
    const char *p   = token;
    size_t      len = strlen(token);
    size_t      pos = 0;

    while (pos < len)
    {
        /* skip whitespace / comma separators */
        while (pos < len && (isspace((unsigned char)p[pos]) || p[pos] == ','))
            pos++;
        if (pos >= len)
            break;

        /* parse first number */
        char *end1;
        errno        = 0;
        uint32_t id1 = (uint32_t)strtoul(p + pos, &end1, 0);
        if (end1 == p + pos || errno == ERANGE)
        {
            rt_kprintf("canfilter: cannot parse '%s'\n", p + pos);
            return -RT_ERROR;
        }
        pos += (size_t)(end1 - (p + pos));

        /* skip whitespace */
        while (pos < len && isspace((unsigned char)p[pos]))
            pos++;

        if (pos < len && p[pos] == '-')
        {
            /* range: id1-id2 */
            pos++;
            while (pos < len && isspace((unsigned char)p[pos]))
                pos++;

            char *end2;
            errno        = 0;
            uint32_t id2 = (uint32_t)strtoul(p + pos, &end2, 0);
            if (end2 == p + pos || errno == ERANGE)
            {
                rt_kprintf("canfilter: cannot parse range end in '%s'\n", p + pos);
                return -RT_ERROR;
            }
            pos += (size_t)(end2 - (p + pos));

            cf_err_t err;
            if (id1 <= 0x7FFU && id2 <= 0x7FFU)
                err = cf_add_std_range(cf, id1, id2);
            else if (id1 <= 0x1FFFFFFFU && id2 <= 0x1FFFFFFFU)
                err = cf_add_ext_range(cf, id1, id2);
            else
            {
                rt_kprintf("canfilter: ID out of range\n");
                return -RT_ERROR;
            }
            if (err != CF_OK)
            {
                print_cf_err(err);
                return -RT_ERROR;
            }
        }
        else
        {
            /* single ID */
            cf_err_t err;
            if (id1 <= 0x7FFU)
                err = cf_add_std_id(cf, id1);
            else if (id1 <= 0x1FFFFFFFU)
                err = cf_add_ext_id(cf, id1);
            else
            {
                rt_kprintf("canfilter: ID 0x%x out of range\n", id1);
                return -RT_ERROR;
            }
            if (err != CF_OK)
            {
                print_cf_err(err);
                return -RT_ERROR;
            }
        }
    }

    return RT_EOK;
}

/* ------------------------------------------------------------------ */
/* MSH command entry point                                             */
/* ------------------------------------------------------------------ */

static int canfilter_cmd(int argc, char *argv[])
{
    cf_bxcan_f0_t cf;
    int           verbose   = 0;
    int           allow_all = 0;
    int           dry_run   = 0;
    int           has_ids   = 0;

    cf_begin(&cf);

    /* ----- option pass ----- */
    for (int i = 1; i < argc; i++)
    {
        const char *arg = argv[i];

        if (arg[0] == '-')
        {
            if (strcmp(arg, "-a") == 0 || strcmp(arg, "--allow-all") == 0)
            {
                allow_all = 1;
            }
            else if (strcmp(arg, "-d") == 0 || strcmp(arg, "--dry-run") == 0)
            {
                dry_run = 1;
            }
            else if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0)
            {
                print_help();
                return RT_EOK;
            }
            else if (strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0)
            {
                verbose++;
            }
            else
            {
                rt_kprintf("canfilter: unknown option '%s'\n", arg);
                return -RT_ERROR;
            }
        }
    }

    cf.verbose = (uint8_t)(verbose > 0 ? 1 : 0);

    /* ----- filter construction ----- */
    if (allow_all)
    {
        cf_err_t err = cf_allow_all(&cf);
        if (err != CF_OK)
        {
            print_cf_err(err);
            return -RT_ERROR;
        }
        has_ids = 1;
    }

    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
            continue; /* already handled */
        if (parse_token(&cf, argv[i]) != RT_EOK)
            return -RT_ERROR;
        has_ids = 1;
    }

    if (!has_ids)
    {
        rt_kprintf("canfilter: no IDs specified. Use -h for help.\n");
        return -RT_ERROR;
    }

    cf_err_t err = cf_end(&cf);
    if (err != CF_OK)
    {
        print_cf_err(err);
        return -RT_ERROR;
    }

    /* ----- diagnostics ----- */
    if (verbose >= 3)
        cf_debug_print_reg(&cf);
    if (verbose >= 2)
        cf_debug_print(&cf);

    cf_print_usage(&cf);

    /* ----- program hardware ----- */
    if (dry_run)
    {
        if (verbose)
            rt_kprintf("canfilter: dry run, not programming hardware\n");
        return RT_EOK;
    }

    rt_err_t ret = can_set_filter(&cf.hw);
    if (ret != RT_EOK)
    {
        rt_kprintf("canfilter: can_set_filter failed (%d)\n", ret);
        return -RT_ERROR;
    }

    if (verbose)
        rt_kprintf("canfilter: filter programmed successfully\n");

    return RT_EOK;
}

MSH_CMD_EXPORT_ALIAS(canfilter_cmd, canfilter, program bxCAN hardware filter);
