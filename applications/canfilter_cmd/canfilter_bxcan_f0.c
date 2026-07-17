/*
 * canfilter_bxcan_f0.c
 *
 * Plain-C bxCAN filter builder for STM32F0/F1/F3 (14 filter banks).
 * Ported from canfilter_bxcan.cpp / canfilter.cpp (C++ original).
 *
 * See:
 *   STM RM0431  31.7.4 Identifier filtering
 *   https://github.com/koendv/canfilter
 */

#include <string.h>
#include <rtthread.h>
#include "canfilter_bxcan_f0.h"

#define MAX_BANKS  14U
#define MAX_STD_ID 0x7FFU
#define MAX_EXT_ID 0x1FFFFFFFU

/* ------------------------------------------------------------------ */
/* Internal emit helpers                                               */
/* ------------------------------------------------------------------ */

/* 4 std IDs into one bank, 16-bit list mode */
static cf_err_t emit_std_list(cf_bxcan_f0_t *cf,
                              uint32_t id1, uint32_t id2,
                              uint32_t id3, uint32_t id4)
{
    if (cf->bank >= MAX_BANKS)
        return CF_FULL;
    if (id1 > MAX_STD_ID || id2 > MAX_STD_ID || id3 > MAX_STD_ID || id4 > MAX_STD_ID)
        return CF_PARAM;

    uint32_t b     = cf->bank;
    cf->hw.fr1[b]  = (id2 << 21) | (id1 << 5);
    cf->hw.fr2[b]  = (id4 << 21) | (id3 << 5);
    cf->hw.fs1r   &= ~(1U << b); /* 16-bit */
    cf->hw.fm1r   |= (1U << b);  /* list   */
    cf->hw.fa1r   |= (1U << b);  /* enable */
    cf->bank++;
    return CF_OK;
}

/* 2 std (id,mask) pairs into one bank, 16-bit mask mode */
static cf_err_t emit_std_mask(cf_bxcan_f0_t *cf,
                              uint32_t id1, uint32_t mask1,
                              uint32_t id2, uint32_t mask2)
{
    if (cf->bank >= MAX_BANKS)
        return CF_FULL;
    if (id1 > MAX_STD_ID || mask1 > MAX_STD_ID || id2 > MAX_STD_ID || mask2 > MAX_STD_ID)
        return CF_PARAM;

    uint32_t b     = cf->bank;
    cf->hw.fr1[b]  = (mask1 << 21) | (id1 << 5);
    cf->hw.fr2[b]  = (mask2 << 21) | (id2 << 5);
    cf->hw.fs1r   &= ~(1U << b); /* 16-bit */
    cf->hw.fm1r   &= ~(1U << b); /* mask   */
    cf->hw.fa1r   |= (1U << b);  /* enable */
    cf->bank++;
    return CF_OK;
}

/* 2 ext IDs into one bank, 32-bit list mode */
static cf_err_t emit_ext_list(cf_bxcan_f0_t *cf, uint32_t id1, uint32_t id2)
{
    if (cf->bank >= MAX_BANKS)
        return CF_FULL;
    if (id1 > MAX_EXT_ID || id2 > MAX_EXT_ID)
        return CF_PARAM;

    uint32_t b     = cf->bank;
    cf->hw.fr1[b]  = (id1 << 3) | (1U << 2); /* IDE bit */
    cf->hw.fr2[b]  = (id2 << 3) | (1U << 2);
    cf->hw.fs1r   |= (1U << b);              /* 32-bit */
    cf->hw.fm1r   |= (1U << b);              /* list   */
    cf->hw.fa1r   |= (1U << b);              /* enable */
    cf->bank++;
    return CF_OK;
}

/* 1 ext (id,mask) into one bank, 32-bit mask mode */
static cf_err_t emit_ext_mask(cf_bxcan_f0_t *cf, uint32_t id1, uint32_t mask1)
{
    if (cf->bank >= MAX_BANKS)
        return CF_FULL;
    if (id1 > MAX_EXT_ID || mask1 > MAX_EXT_ID)
        return CF_PARAM;

    uint32_t b     = cf->bank;
    cf->hw.fr1[b]  = (id1 << 3) | (1U << 2); /* IDE bit */
    cf->hw.fr2[b]  = (mask1 << 3);           /* no IDE in mask */
    cf->hw.fs1r   |= (1U << b);              /* 32-bit */
    cf->hw.fm1r   &= ~(1U << b);             /* mask   */
    cf->hw.fa1r   |= (1U << b);              /* enable */
    cf->bank++;
    return CF_OK;
}

/* ------------------------------------------------------------------ */
/* Accumulator helpers (batch before emitting)                         */
/* ------------------------------------------------------------------ */

static cf_err_t add_std_list(cf_bxcan_f0_t *cf, uint32_t id)
{
    if (cf->std_list_count > 3)
        return CF_PARAM;

    cf->std_list[cf->std_list_count++] = id;
    if (cf->std_list_count == 1)
    {
        /* pad duplicates so emit always has 4 valid slots */
        cf->std_list[1] = id;
        cf->std_list[2] = id;
        cf->std_list[3] = id;
    }
    else if (cf->std_list_count == 4)
    {
        cf_err_t err       = emit_std_list(cf,
                                           cf->std_list[0], cf->std_list[1],
                                           cf->std_list[2], cf->std_list[3]);
        cf->std_list_count = 0;
        return err;
    }
    return CF_OK;
}

static cf_err_t add_std_mask(cf_bxcan_f0_t *cf, uint32_t id, uint32_t mask)
{
    if (cf->std_mask_count > 1)
        return CF_PARAM;

    cf->std_mask[cf->std_mask_count].id   = id;
    cf->std_mask[cf->std_mask_count].mask = mask;
    cf->std_mask_count++;
    if (cf->std_mask_count == 1)
    {
        /* pad duplicate */
        cf->std_mask[1].id   = id;
        cf->std_mask[1].mask = mask;
    }
    else if (cf->std_mask_count == 2)
    {
        cf_err_t err       = emit_std_mask(cf,
                                           cf->std_mask[0].id, cf->std_mask[0].mask,
                                           cf->std_mask[1].id, cf->std_mask[1].mask);
        cf->std_mask_count = 0;
        return err;
    }
    return CF_OK;
}

static cf_err_t add_ext_list(cf_bxcan_f0_t *cf, uint32_t id)
{
    if (cf->ext_list_count > 1)
        return CF_PARAM;

    cf->ext_list[cf->ext_list_count++] = id;
    if (cf->ext_list_count == 1)
    {
        cf->ext_list[1] = id; /* pad duplicate */
    }
    else if (cf->ext_list_count == 2)
    {
        cf_err_t err       = emit_ext_list(cf, cf->ext_list[0], cf->ext_list[1]);
        cf->ext_list_count = 0;
        return err;
    }
    return CF_OK;
}

static cf_err_t add_ext_mask(cf_bxcan_f0_t *cf, uint32_t id, uint32_t mask)
{
    return emit_ext_mask(cf, id, mask);
}

/* ------------------------------------------------------------------ */
/* CIDR largest-prefix helpers                                         */
/* ------------------------------------------------------------------ */

/* Return the prefix length of the largest power-of-two aligned block
 * starting at 'begin' that still fits within [begin, end].
 * Standard IDs are 11 bits wide. */
static int std_largest_prefix(uint32_t begin, uint32_t end)
{
    int prefix = 11;
    /* reduce prefix until begin is aligned to block boundary */
    while (prefix > 0)
    {
        uint32_t mask_bit = 1U << (11 - prefix);
        if (begin & mask_bit)
            break;
        prefix--;
    }
    /* increase prefix until block fits within [begin, end] */
    while (prefix < 11)
    {
        uint32_t block_size = 1U << (11 - prefix);
        if (begin + block_size - 1 > end)
            prefix++;
        else
            break;
    }
    return prefix;
}

/* Same as above for extended IDs (29 bits wide). */
static int ext_largest_prefix(uint32_t begin, uint32_t end)
{
    int prefix = 29;
    while (prefix > 0)
    {
        uint32_t mask_bit = 1U << (29 - prefix);
        if (begin & mask_bit)
            break;
        prefix--;
    }
    while (prefix < 29)
    {
        uint32_t block_size = 1U << (29 - prefix);
        if (begin + block_size - 1 > end)
            prefix++;
        else
            break;
    }
    return prefix;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

void cf_begin(cf_bxcan_f0_t *cf)
{
    memset(cf, 0, sizeof(*cf));
    cf->hw.dev = CANFILTER_DEV_BXCAN_F0;
}

cf_err_t cf_end(cf_bxcan_f0_t *cf)
{
    cf_err_t err = CF_OK;

    if (cf->std_list_count != 0)
        err = emit_std_list(cf,
                            cf->std_list[0], cf->std_list[1],
                            cf->std_list[2], cf->std_list[3]);

    if (cf->std_mask_count != 0 && err == CF_OK)
        err = emit_std_mask(cf,
                            cf->std_mask[0].id, cf->std_mask[0].mask,
                            cf->std_mask[1].id, cf->std_mask[1].mask);

    if (cf->ext_list_count != 0 && err == CF_OK)
        err = emit_ext_list(cf, cf->ext_list[0], cf->ext_list[1]);

    return err;
}

cf_err_t cf_add_std_range(cf_bxcan_f0_t *cf, uint32_t begin, uint32_t end)
{
    if (begin > MAX_STD_ID || end > MAX_STD_ID)
        return CF_PARAM;

    if (begin > end)
    {
        uint32_t tmp = end;
        end          = begin;
        begin        = tmp;
    }

    while (begin <= end)
    {
        int      prefix = std_largest_prefix(begin, end);
        uint32_t mask   = (~0U << (11 - prefix)) & MAX_STD_ID;
        uint32_t id     = begin;
        cf_err_t err;

        if (mask == MAX_STD_ID)
        {
            if (cf->verbose)
                rt_kprintf("bxcan std list id 0x%03x\n", id);
            err = add_std_list(cf, id);
        }
        else
        {
            if (cf->verbose)
                rt_kprintf("bxcan std mask id 0x%03x mask 0x%03x\n", id, mask);
            err = add_std_mask(cf, id, mask);
        }

        if (err != CF_OK)
        {
            rt_kprintf("canfilter: std filter error %d\n", err);
            return err;
        }

        begin += 1U << (11 - prefix);
    }

    return CF_OK;
}

cf_err_t cf_add_ext_range(cf_bxcan_f0_t *cf, uint32_t begin, uint32_t end)
{
    if (begin > MAX_EXT_ID || end > MAX_EXT_ID)
        return CF_PARAM;

    if (begin > end)
    {
        uint32_t tmp = end;
        end          = begin;
        begin        = tmp;
    }

    while (begin <= end)
    {
        int      prefix = ext_largest_prefix(begin, end);
        uint32_t mask   = (~0U << (29 - prefix)) & MAX_EXT_ID;
        uint32_t id     = begin;
        cf_err_t err;

        if (mask == MAX_EXT_ID)
        {
            if (cf->verbose)
                rt_kprintf("bxcan ext list id 0x%08x\n", id);
            err = add_ext_list(cf, id);
        }
        else
        {
            if (cf->verbose)
                rt_kprintf("bxcan ext mask id 0x%08x mask 0x%08x\n", id, mask);
            err = add_ext_mask(cf, id, mask);
        }

        if (err != CF_OK)
        {
            rt_kprintf("canfilter: ext filter error %d\n", err);
            return err;
        }

        begin += 1U << (29 - prefix);
    }

    return CF_OK;
}

cf_err_t cf_add_std_id(cf_bxcan_f0_t *cf, uint32_t id)
{
    return cf_add_std_range(cf, id, id);
}

cf_err_t cf_add_ext_id(cf_bxcan_f0_t *cf, uint32_t id)
{
    return cf_add_ext_range(cf, id, id);
}

cf_err_t cf_allow_all(cf_bxcan_f0_t *cf)
{
    cf_err_t err = cf_add_std_range(cf, 0, MAX_STD_ID);
    if (err != CF_OK)
        return err;
    return cf_add_ext_range(cf, 0, MAX_EXT_ID);
}

/* ------------------------------------------------------------------ */
/* Debug / diagnostics                                                 */
/* ------------------------------------------------------------------ */

void cf_print_usage(const cf_bxcan_f0_t *cf)
{
    uint32_t percent = (cf->bank * 100U + MAX_BANKS / 2U) / MAX_BANKS;
    rt_kprintf("Filter usage: %u/%u (%u%%)\n",
               (unsigned)cf->bank, (unsigned)MAX_BANKS, (unsigned)percent);
}

void cf_debug_print(const cf_bxcan_f0_t *cf)
{
    rt_kprintf("\nbxcan debug:\n");
    for (uint32_t i = 0; i < MAX_BANKS; i++)
    {
        if (!(cf->hw.fa1r & (1U << i)))
            continue;

        int is_32bit = (cf->hw.fs1r & (1U << i)) != 0;
        int is_list  = (cf->hw.fm1r & (1U << i)) != 0;

        rt_kprintf("bank[%u]: ", (unsigned)i);

        if (is_32bit)
        {
            uint32_t id1 = (cf->hw.fr1[i] >> 3) & MAX_EXT_ID;
            uint32_t id2 = (cf->hw.fr2[i] >> 3) & MAX_EXT_ID;
            if (is_list)
            {
                rt_kprintf("ext list 0x%08x, 0x%08x\n", id1, id2);
            }
            else
            {
                uint32_t begin1 = id1 & id2;
                uint32_t end1   = (begin1 | ~id2) & MAX_EXT_ID;
                rt_kprintf("ext mask 0x%08x-0x%08x\n", begin1, end1);
            }
        }
        else
        {
            uint32_t id1 = (cf->hw.fr1[i] >> 5) & MAX_STD_ID;
            uint32_t id2 = (cf->hw.fr1[i] >> 21) & MAX_STD_ID;
            uint32_t id3 = (cf->hw.fr2[i] >> 5) & MAX_STD_ID;
            uint32_t id4 = (cf->hw.fr2[i] >> 21) & MAX_STD_ID;
            if (is_list)
            {
                rt_kprintf("std list 0x%03x, 0x%03x, 0x%03x, 0x%03x\n",
                           id1, id2, id3, id4);
            }
            else
            {
                uint32_t begin1 = id1 & id2;
                uint32_t end1   = (begin1 | ~id2) & MAX_STD_ID;
                uint32_t begin2 = id3 & id4;
                uint32_t end2   = (begin2 | ~id4) & MAX_STD_ID;
                rt_kprintf("std mask 0x%03x-0x%03x, 0x%03x-0x%03x\n",
                           begin1, end1, begin2, end2);
            }
        }
    }
}

void cf_debug_print_reg(const cf_bxcan_f0_t *cf)
{
    rt_kprintf("\nbxcan registers:\n");
    rt_kprintf("FS1R:  0x%08x\n", cf->hw.fs1r);
    rt_kprintf("FM1R:  0x%08x\n", cf->hw.fm1r);
    rt_kprintf("FFA1R: 0x%08x\n", cf->hw.ffa1r);
    rt_kprintf("FA1R:  0x%08x\n", cf->hw.fa1r);
    for (uint32_t i = 0; i < MAX_BANKS; i++)
    {
        if (cf->hw.fr1[i] != 0 || cf->hw.fr2[i] != 0)
            rt_kprintf("FR1[%u]: 0x%08x  FR2[%u]: 0x%08x\n",
                       (unsigned)i, cf->hw.fr1[i],
                       (unsigned)i, cf->hw.fr2[i]);
    }
}
