#ifndef CANFILTER_BXCAN_F0_H
#define CANFILTER_BXCAN_F0_H

/*
 * canfilter_bxcan_f0.h
 *
 * Plain-C bxCAN filter builder for STM32F0/F1/F3 (14 filter banks).
 * Ported from canfilter_bxcan.cpp / canfilter.cpp (C++ original).
 *
 * Produces a struct canfilter_bxcan_f0 as defined in canfilter.h.
 * No dynamic allocation, no C++ dependencies.
 */

#include <stdint.h>
#include "canfilter.h"

/* Error codes (mirrors canfilter_error_t) */
typedef enum
{
    CF_OK = 0,
    CF_PARAM, /* id out of range or invalid range */
    CF_FULL,  /* no more filter banks */
} cf_err_t;

/* Internal builder state - zero-initialise before use */
typedef struct
{
    struct canfilter_bxcan_f0 hw; /* output: hardware filter image       */

    uint32_t bank;                /* next free bank index (0-13)         */

    /* pending std IDs (batched 4-per-bank in list mode) */
    uint32_t std_list[4];
    uint32_t std_list_count;

    /* pending std masks (batched 2-per-bank in mask mode) */
    struct
    {
        uint32_t id;
        uint32_t mask;
    } std_mask[2];
    uint32_t std_mask_count;

    /* pending ext IDs (batched 2-per-bank in list mode) */
    uint32_t ext_list[2];
    uint32_t ext_list_count;

    uint8_t verbose; /* 0 = silent, 1 = verbose             */
} cf_bxcan_f0_t;

/* Lifecycle */
void     cf_begin(cf_bxcan_f0_t *cf);
cf_err_t cf_end(cf_bxcan_f0_t *cf);

/* Add individual IDs */
cf_err_t cf_add_std_id(cf_bxcan_f0_t *cf, uint32_t id);
cf_err_t cf_add_ext_id(cf_bxcan_f0_t *cf, uint32_t id);

/* Add ranges */
cf_err_t cf_add_std_range(cf_bxcan_f0_t *cf, uint32_t begin, uint32_t end);
cf_err_t cf_add_ext_range(cf_bxcan_f0_t *cf, uint32_t begin, uint32_t end);

/* Convenience: allow all standard + extended IDs */
cf_err_t cf_allow_all(cf_bxcan_f0_t *cf);

/* Debug / diagnostics (rt_kprintf) */
void cf_print_usage(const cf_bxcan_f0_t *cf);
void cf_debug_print(const cf_bxcan_f0_t *cf);
void cf_debug_print_reg(const cf_bxcan_f0_t *cf);

#endif /* CANFILTER_BXCAN_F0_H */
