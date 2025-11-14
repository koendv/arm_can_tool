/*
 * canfilter.c - CAN Bus Hardware Filter Generator
 *
 * Generates optimal hardware filters using CIDR-style decomposition
 * Supports STM32, SLCAN, and embedded output formats
 *
 * Copyright (c) 2025 Koen De Vleeschauwer
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <stdarg.h>
#include "canfilter.h"

/* Platform detection - MUST COME FIRST */
#if defined(__RTTHREAD__) || defined(RT_THREAD)
#define USE_EMBEDDED
#define USE_RTTHREAD
#elif defined(__ZEPHYR__)
#define USE_EMBEDDED
#endif

/* Configuration - Platform Specific */
#ifdef USE_EMBEDDED
/* Embedded (RT-Thread) - Conservative limits */
#define MAX_FILTERS 14
#define MAX_TEST_IDS 16
#define MAX_RANGES 8
#else
/* Desktop - Generous limits */
#define MAX_FILTERS 64
#define MAX_TEST_IDS 256
#define MAX_RANGES 128
#endif

/* RT-Thread specific includes and configuration */
#ifdef USE_RTTHREAD
#include <rtdevice.h>
#include <rtthread.h>
#ifndef MAX_CAN_HW_FILTER
#define MAX_CAN_HW_FILTER 28  /* Typical STM32F4 has 14-28 filters */
#endif
#endif

/* ============================================================================
 * SAFETY CHECKS FOR EMBEDDED SYSTEMS
 * ============================================================================ */

#ifdef USE_RTTHREAD
/* Buffer boundary checks */
#define CHECK_BOUNDS(index, max) do { \
    if ((index) < 0 || (index) >= (max)) { \
        printf("Error: Buffer overflow at %s:%d\n", __FILE__, __LINE__); \
        return CANFILTER_ERROR; \
    } \
} while(0)
#else
#define CHECK_BOUNDS(index, max) ((void)0)
#endif

/* ============================================================================
 * RT-THREAD HARDWARE INTEGRATION
 * ============================================================================ */

#ifdef USE_RTTHREAD
int canfilter_apply_to_hardware(const can_filter_t* filters, int filter_count, const char* device_name) {
    rt_device_t can_dev = rt_device_find(device_name);
    if (!can_dev) {
        printf("Error: CAN device '%s' not found\n", device_name);
        return CANFILTER_HW_ERROR;
    }

    if (filter_count > MAX_CAN_HW_FILTER) {
        printf("Error: %d filters exceed hardware limit of %d\n", filter_count, MAX_CAN_HW_FILTER);
        return CANFILTER_HW_ERROR;
    }

    struct rt_can_filter_item items[MAX_CAN_HW_FILTER];
    struct rt_can_filter_config cfg = {0, 1, items};

    for (int i = 0; i < filter_count; i++) {
        uint32_t id_limit = (filters[i].mode == MODE_EXT) ? 0x1FFFFFFF : 0x7FF;

        /* Direct use - CIDR mask format matches STM32 mask format */
        items[i].mask = filters[i].mask & id_limit;

        items[i].id = filters[i].id;
        items[i].ide = (filters[i].mode == MODE_EXT) ? 1 : 0;
        items[i].rtr = (filters[i].frame_type == FRAME_RTR) ? 1 : 0;

        /* Use hardware-optimized mode selection */
        uint32_t full_mask = (filters[i].mode == MODE_STD) ? 0x7FF : 0x1FFFFFFF;
        items[i].mode = (filters[i].mask == full_mask) ? 1 : 0;  /* 1=list, 0=mask */

        items[i].hdr_bank = -1;  // Auto-assign hardware banks
    }

    cfg.count = filter_count;
    rt_err_t res = rt_device_control(can_dev, RT_CAN_CMD_SET_FILTER, &cfg);

    if (res == RT_EOK) {
        printf("Successfully applied %d filters to CAN hardware\n", filter_count);
        return CANFILTER_SUCCESS;
    } else {
        printf("Failed to apply filters: %d\n", (int)res);
        return CANFILTER_HW_ERROR;
    }
}
#else
/* Desktop version - no hardware access */
int canfilter_apply_to_hardware(const can_filter_t* filters, int filter_count, const char* device_name) {
    (void)filters; (void)filter_count; (void)device_name;
    printf("Error: Hardware integration only available on RT-Thread\n");
    return CANFILTER_HW_ERROR;
}
#endif

/* ============================================================================
 * FILTER ALGORITHM (CIDR-STYLE)
 * ============================================================================ */

/* Insertion sort for filters */
static void insertion_sort_filters(can_filter_t* filters, int count) {
    int i, j;
    can_filter_t key;

    for (i = 1; i < count; i++) {
        key = filters[i];
        j = i - 1;

        /* Sort by base ID, then by mask (more specific first) */
        while (j >= 0 && (filters[j].id > key.id ||
                          (filters[j].id == key.id && filters[j].mask < key.mask))) {
            filters[j + 1] = filters[j];
            j = j - 1;
        }
        filters[j + 1] = key;
    }
}

/* Parse CAN ID from string */
static int parse_can_id(const char* str, uint32_t* id, can_mode_t mode) {
    unsigned int temp;
    int bits = (mode == MODE_STD) ? 11 : 29;
    uint32_t max_id = (1UL << bits) - 1;

    if (sscanf(str, "%x", &temp) != 1) {
        return 0;
    }

    if (temp > max_id) {
        printf("Error: ID 0x%X exceeds %d-bit range\n", temp, bits);
        return 0;
    }

    *id = (uint32_t)temp;
    return 1;
}

/* Parse range specification */
static int parse_range(const char* str, can_range_t* range, can_mode_t default_mode, frame_type_t frame_type) {
    char* dash = strchr(str, '-');
    char start_str[32];
    char end_str[32];

    range->mode = default_mode;
    range->frame_type = frame_type;

    if (!dash) {
        /* Single ID */
        if (!parse_can_id(str, &range->start, default_mode)) return 0;
        range->end = range->start;
        return 1;
    }

    /* Range */
    if ((dash - str) >= (int)sizeof(start_str)) return 0;

    strncpy(start_str, str, dash - str);
    start_str[dash - str] = '\0';
    strcpy(end_str, dash + 1);

    if (!parse_can_id(start_str, &range->start, default_mode) ||
        !parse_can_id(end_str, &range->end, default_mode)) {
        return 0;
    }

    if (range->start > range->end) {
        uint32_t temp = range->start;
        range->start = range->end;
        range->end = temp;
    }

    return 1;
}

/* Convert range to optimal filters using CIDR-style decomposition */
static int range_to_filters(const can_range_t* range, can_filter_t* filters, int max_filters) {
    uint32_t current = range->start;
    uint32_t end = range->end;
    int count = 0;
    int bits = (range->mode == MODE_STD) ? 11 : 29;
    uint32_t max_mask = (1UL << bits) - 1;

    while (current <= end && count < max_filters) {
        /* Start with largest possible block and reduce until it fits */
        int shift = 0; /* Largest block */

        while (shift < bits) {
            uint32_t block_mask = (max_mask << shift) & max_mask;
            uint32_t base = current & block_mask; /* Aligned base */
            uint32_t block_end = base | (~block_mask & max_mask);

            /* Check if block fits entirely within range and starts at or after current */
            if (base >= current && block_end <= end) {
                /* This block works, try even larger block */
                shift++;
            } else {
                /* Block is too large, use previous size */
                shift--;
                break;
            }
        }

        /* Handle edge cases */
        if (shift < 0) shift = 0;
        if (shift >= bits) shift = bits - 1;

        uint32_t final_mask = (max_mask << shift) & max_mask;
        uint32_t base = current & final_mask;

        filters[count].id = base;
        filters[count].mask = final_mask;
        filters[count].mode = range->mode;
        filters[count].frame_type = range->frame_type;
        count++;

        /* Move to next address after this block */
        uint32_t block_end = base | (~final_mask & max_mask);
        current = block_end + 1;
        if (current <= block_end) break; /* Overflow protection */
    }

    return count;
}

/* Remove filters that are completely covered by other filters */
static void remove_subset_filters(can_filter_t* filters, int* count) {
    for (int i = 0; i < *count; i++) {
        for (int j = 0; j < *count; j++) {
            if (i == j) continue;

            /* Check if filter[i] is completely covered by filter[j] */
            if (filters[i].mode == filters[j].mode &&
                filters[i].frame_type == filters[j].frame_type &&
                (filters[i].mask & filters[j].mask) == filters[j].mask &&
                (filters[i].id & filters[j].mask) == (filters[j].id & filters[j].mask)) {
                /* Remove the subset filter */
                for (int k = i; k < *count - 1; k++) {
                    filters[k] = filters[k + 1];
                }
                (*count)--;
                i--; /* Restart check for this position */
                break;
            }
        }
    }
}

/* Check if two filters can be aggregated */
static int can_aggregate(const can_filter_t* a, const can_filter_t* b) {
    if (a->mode != b->mode || a->frame_type != b->frame_type) {
        return 0;
    }

    /* Only aggregate filters with the same mask size */
    if (a->mask != b->mask) {
        return 0;
    }

    int bits = (a->mode == MODE_STD) ? 11 : 29;
    uint32_t block_size = (~a->mask + 1) & ((1UL << bits) - 1);
    uint32_t expected_next = a->id + block_size;

    return (b->id == expected_next) &&
           ((a->id & (a->mask << 1)) == (b->id & (b->mask << 1)));
}

/* Aggregate two filters */
static void aggregate_filters(const can_filter_t* a, can_filter_t* result) {
    int bits = (a->mode == MODE_STD) ? 11 : 29;
    *result = *a;
    result->mask = a->mask << 1;
    result->id = a->id & result->mask;

    if ((result->mask >> bits) != 0) {
        result->mask = (1UL << bits) - 1;
    }
}

/* Main filter generation with aggregation */
int canfilter_generate_filters(can_range_t* ranges, int range_count, can_filter_t* filters, int max_filters) {
#ifdef USE_EMBEDDED
    can_filter_t temp_filters[MAX_FILTERS * 2];
#else
    can_filter_t temp_filters[MAX_FILTERS * 4];
#endif
    int temp_count = 0;
    int i, j;

    memset(temp_filters, 0, sizeof(temp_filters));

    /* Convert ranges to filters */
    int max_temp_filters = (int)(sizeof(temp_filters) / sizeof(temp_filters[0]));
    for (i = 0; i < range_count && temp_count < max_temp_filters; i++) {
        CHECK_BOUNDS(i, range_count);
        int count = range_to_filters(&ranges[i], &temp_filters[temp_count], max_temp_filters - temp_count);
        if (count > 0) {
            temp_count += count;
        }
    }

    if (temp_count == 0) return 0;

    /* Sort and aggregate filters */
    int changed;
    do {
        changed = 0;
        insertion_sort_filters(temp_filters, temp_count);

        for (i = 0; i < temp_count - 1; i++) {
            CHECK_BOUNDS(i, temp_count - 1);
            if (can_aggregate(&temp_filters[i], &temp_filters[i + 1])) {
                aggregate_filters(&temp_filters[i], &temp_filters[i]);

                /* Remove the second filter */
                for (j = i + 1; j < temp_count - 1; j++) {
                    CHECK_BOUNDS(j + 1, temp_count);
                    temp_filters[j] = temp_filters[j + 1];
                }
                temp_count--;
                changed = 1;
                break;
            }
        }
    } while (changed && temp_count > 1);

    /* Remove subset filters (like single ID covered by range) */
    remove_subset_filters(temp_filters, &temp_count);

    /* Copy to output with truncation warning */
    int output_count = (temp_count > max_filters) ? max_filters : temp_count;

    /* ADD TRUNCATION WARNING */
    if (temp_count > max_filters) {
        printf("Warning: Filter list truncated from %d to %d filters\n", temp_count, max_filters);
        printf("Some IDs may not be filtered correctly. Consider using fewer ranges.\n");
    }

    for (i = 0; i < output_count; i++) {
        CHECK_BOUNDS(i, output_count);
        filters[i] = temp_filters[i];
    }

    return output_count;
}

/* ============================================================================
 * TESTING AND VERIFICATION
 * ============================================================================ */

/* Test if ID passes through filters */
static int test_filter(const can_filter_t* filter, uint32_t id, can_mode_t mode, frame_type_t frame_type) {
    if (filter->mode != mode || filter->frame_type != frame_type) {
        return 0;
    }
    return (id & filter->mask) == (filter->id & filter->mask);
}

/* Test multiple filters */
int canfilter_test_filters(const can_filter_t* filters, int filter_count,
                       uint32_t id, can_mode_t mode, frame_type_t frame_type) {
    int i;
    for (i = 0; i < filter_count; i++) {
        if (test_filter(&filters[i], id, mode, frame_type)) {
            return 1;
        }
    }
    return 0;
}

/* Check if this is a "pass all" configuration */
static int is_pass_all_filter(const can_filter_t* filters, int count) {
    return (count == 1 && filters[0].id == 0 && filters[0].mask == 0);
}

/* Comprehensive self-test implementation */
int canfilter_run_selftest(void) {
    printf("Running canfilter self-test...\n");
    int passed = 0;
    int total = 0;

    /* Test 1: Single ID - Data frame */
    {
        can_range_t test_ranges[] = {{0x100, 0x100, MODE_STD, FRAME_DATA}};
        can_filter_t filters[MAX_FILTERS];
        int count = canfilter_generate_filters(test_ranges, 1, filters, MAX_FILTERS);

        if (count == 1 && filters[0].id == 0x100 && filters[0].mask == 0x7FF &&
            filters[0].frame_type == FRAME_DATA) {
            passed++;
        } else {
            printf("FAIL: Single ID Data frame test\n");
        }
        total++;
    }

    /* Test 2: Single ID - RTR frame */
    {
        can_range_t test_ranges[] = {{0x200, 0x200, MODE_STD, FRAME_RTR}};
        can_filter_t filters[MAX_FILTERS];
        int count = canfilter_generate_filters(test_ranges, 1, filters, MAX_FILTERS);

        if (count == 1 && filters[0].id == 0x200 && filters[0].mask == 0x7FF &&
            filters[0].frame_type == FRAME_RTR) {
            passed++;
        } else {
            printf("FAIL: Single ID RTR frame test\n");
        }
        total++;
    }

    /* Test 3: Range with Data frames */
    {
        can_range_t test_ranges[] = {{0x300, 0x30F, MODE_STD, FRAME_DATA}};
        can_filter_t filters[MAX_FILTERS];
        int count = canfilter_generate_filters(test_ranges, 1, filters, MAX_FILTERS);

        int coverage_ok = 1;
        coverage_ok &= canfilter_test_filters(filters, count, 0x300, MODE_STD, FRAME_DATA);
        coverage_ok &= canfilter_test_filters(filters, count, 0x30F, MODE_STD, FRAME_DATA);
        coverage_ok &= !canfilter_test_filters(filters, count, 0x300, MODE_STD, FRAME_RTR);
        coverage_ok &= !canfilter_test_filters(filters, count, 0x310, MODE_STD, FRAME_DATA);

        if (coverage_ok && count > 0) {
            passed++;
        } else {
            printf("FAIL: Data frame range test\n");
        }
        total++;
    }

    /* Test 4: Mixed Standard and Extended IDs */
    {
        can_range_t test_ranges[] = {
            {0x100, 0x100, MODE_STD, FRAME_DATA},
            {0x1FFFF, 0x1FFFF, MODE_EXT, FRAME_DATA}
        };
        can_filter_t filters[MAX_FILTERS];
        int count = canfilter_generate_filters(test_ranges, 2, filters, MAX_FILTERS);

        int coverage_ok = 1;
        coverage_ok &= canfilter_test_filters(filters, count, 0x100, MODE_STD, FRAME_DATA);
        coverage_ok &= canfilter_test_filters(filters, count, 0x1FFFF, MODE_EXT, FRAME_DATA);
        coverage_ok &= !canfilter_test_filters(filters, count, 0x100, MODE_EXT, FRAME_DATA);
        coverage_ok &= !canfilter_test_filters(filters, count, 0x1FFFF, MODE_STD, FRAME_DATA);

        if (coverage_ok && count >= 2) {
            passed++;
        } else {
            printf("FAIL: Mixed Standard/Extended test\n");
        }
        total++;
    }

    /* Test 5: Mixed Data and RTR frames */
    {
        can_range_t test_ranges[] = {
            {0x400, 0x400, MODE_STD, FRAME_DATA},
            {0x400, 0x400, MODE_STD, FRAME_RTR}
        };
        can_filter_t filters[MAX_FILTERS];
        int count = canfilter_generate_filters(test_ranges, 2, filters, MAX_FILTERS);

        int coverage_ok = 1;
        coverage_ok &= canfilter_test_filters(filters, count, 0x400, MODE_STD, FRAME_DATA);
        coverage_ok &= canfilter_test_filters(filters, count, 0x400, MODE_STD, FRAME_RTR);
        coverage_ok &= !canfilter_test_filters(filters, count, 0x401, MODE_STD, FRAME_DATA);

        if (coverage_ok && count >= 2) {
            passed++;
        } else {
            printf("FAIL: Mixed Data/RTR test\n");
        }
        total++;
    }

    /* Test 6: Complex mixed case */
    {
        can_range_t test_ranges[] = {
            {0x500, 0x50F, MODE_STD, FRAME_DATA},
            {0x600, 0x600, MODE_STD, FRAME_RTR},
            {0x100000, 0x100000, MODE_EXT, FRAME_DATA}
        };
        can_filter_t filters[MAX_FILTERS];
        int count = canfilter_generate_filters(test_ranges, 3, filters, MAX_FILTERS);

        int coverage_ok = 1;
        coverage_ok &= canfilter_test_filters(filters, count, 0x500, MODE_STD, FRAME_DATA);
        coverage_ok &= canfilter_test_filters(filters, count, 0x50F, MODE_STD, FRAME_DATA);
        coverage_ok &= canfilter_test_filters(filters, count, 0x600, MODE_STD, FRAME_RTR);
        coverage_ok &= canfilter_test_filters(filters, count, 0x100000, MODE_EXT, FRAME_DATA);
        coverage_ok &= !canfilter_test_filters(filters, count, 0x500, MODE_STD, FRAME_RTR);
        coverage_ok &= !canfilter_test_filters(filters, count, 0x600, MODE_STD, FRAME_DATA);
        coverage_ok &= !canfilter_test_filters(filters, count, 0x100000, MODE_STD, FRAME_DATA);

        if (coverage_ok && count >= 3) {
            passed++;
        } else {
            printf("FAIL: Complex mixed test\n");
        }
        total++;
    }

    printf("Self-test: %d/%d passed\n", passed, total);

    if (passed == total) {
        printf("All self-tests passed!\n");
        return CANFILTER_SUCCESS;
    } else {
        printf("Some self-tests failed!\n");
        return CANFILTER_TEST_FAILED;
    }
}

/* ============================================================================
 * OUTPUT FORMATS
 * ============================================================================ */

/* Calculate FR1 register value for hardware */
static uint32_t calculate_fr1(const can_filter_t* filter) {
    if (filter->mode == MODE_STD) {
        /* Standard ID: [IDE=0][RTR=0/1][STD_ID=11bits] */
        return ((filter->frame_type & 0x1) << 31) |  /* RTR bit */
               ((filter->id & 0x7FF) << 21);         /* Standard ID */
    } else {
        /* Extended ID: [IDE=1][EXT_ID=29bits] */
        return (1 << 31) |                           /* IDE bit */
               (filter->id & 0x1FFFFFFF);            /* Extended ID */
    }
}

/* Calculate FR2 register value for hardware */
static uint32_t calculate_fr2(const can_filter_t* filter) {
    if (filter->mode == MODE_STD) {
        /* Standard ID mask mode: [RTR=0/1][MASK=11bits] */
        /* Standard ID list mode: second ID goes here */
        return ((filter->frame_type & 0x1) << 31) |  /* RTR bit */
               ((filter->mask & 0x7FF) << 21);       /* Mask or second ID */
    } else {
        /* Extended ID: mask or second ID */
        return filter->mask & 0x1FFFFFFF;            /* Mask or second ID */
    }
}

/* Determine optimal mode for filter */
static uint8_t determine_mode(const can_filter_t* filter) {
    uint32_t full_mask = (filter->mode == MODE_STD) ? 0x7FF : 0x1FFFFFFF;
    return (filter->mask == full_mask) ? 1 : 0;  /* 1=list, 0=mask */
}

/* Determine optimal scale for filter */
static uint8_t determine_scale(const can_filter_t* filter) {
    /* Use 32-bit scale for extended IDs, 16-bit for standard when in list mode */
    uint8_t mode = determine_mode(filter);
    if (filter->mode == MODE_EXT) {
        return 1;  /* 32-bit scale for extended IDs */
    } else {
        return (mode == 1) ? 0 : 1;  /* 16-bit for list mode, 32-bit for mask mode */
    }
}

/* Output filters in STM32 format */
void canfilter_output_stm32(const can_filter_t* filters, int count, int use_list_optimization) {
    printf("STM32 Hardware Format:\n");
    for (int i = 0; i < count; i++) {
        const char* mode_str = "MASK";
        const char* scale_str = "32BIT";

        if (use_list_optimization && filters[i].mask == 0x7FF) {
            mode_str = "LIST";
            scale_str = "16BIT";
        }

        printf("ID=0x%08lX MASK=0x%08lX IDE=%d RTR=%d MODE=%s SCALE=%s FILTER=%s\n",
               (unsigned long)filters[i].id,
               (unsigned long)filters[i].mask,
               filters[i].mode,
               filters[i].frame_type,
               (filters[i].mode == MODE_STD) ? "16BIT" : "32BIT",
               scale_str,
               mode_str);
    }
}

/* Output filters in SLCAN format - SINGLE COMMAND APPROACH */
void canfilter_output_slcan(const can_filter_t* filters, int count, int use_list_optimization) {
    printf("SLCAN Hardware Register Format:\n");

    if (count == 0) {
        printf("FB\n");  /* Block all - no filters */
        return;
    } else if (is_pass_all_filter(filters, count)) {
        printf("FA\n");  /* Pass all - special case */
        return;
    }

    printf("F0\n"); /* Begin filter configuration */

    for (int i = 0; i < count; i++) {
        /* Calculate complete hardware values */
        uint32_t fr1 = calculate_fr1(&filters[i]);
        uint32_t fr2 = calculate_fr2(&filters[i]);
        uint8_t mode = use_list_optimization ? determine_mode(&filters[i]) : 0;
        uint8_t scale = determine_scale(&filters[i]);

        /* Output single complete command (21 characters) */
        printf("F%02X%08lX%08lX%01X%01X%01X%01X\n",
               i,                                   /* bank */
               (unsigned long)fr1,                  /* fr1 */
               (unsigned long)fr2,                  /* fr2 */
               mode,                                /* mode */
               scale,                               /* scale */
               filters[i].mode,                     /* ide */
               filters[i].frame_type);              /* rtr */
    }

    printf("F1\n"); /* End filter configuration */
}

/* Output filters in HAL format */
void canfilter_output_hal(const can_filter_t* filters, int count, int use_list_optimization) {
    printf("STM32 HAL Library Format:\n");
    for (int i = 0; i < count; i++) {
        const char* mode_str = "CAN_FILTERMODE_IDMASK";
        const char* scale_str = "CAN_FILTERSCALE_32BIT";

        if (use_list_optimization && filters[i].mask == 0x7FF) {
            mode_str = "CAN_FILTERMODE_IDLIST";
            scale_str = "CAN_FILTERSCALE_16BIT";
        }

        printf("CAN_FilterTypeDef filter%d = {\n", i);
        printf("  .FilterIdHigh = 0x%04lX,\n", (unsigned long)filters[i].id >> 16);
        printf("  .FilterIdLow = 0x%04lX,\n", (unsigned long)filters[i].id & 0xFFFF);
        printf("  .FilterMaskIdHigh = 0x%04lX,\n", (unsigned long)filters[i].mask >> 16);
        printf("  .FilterMaskIdLow = 0x%04lX,\n", (unsigned long)filters[i].mask & 0xFFFF);
        printf("  .FilterFIFOAssignment = CAN_FILTER_FIFO0,\n");
        printf("  .FilterBank = %d,\n", i);
        printf("  .FilterMode = %s,\n", mode_str);
        printf("  .FilterScale = %s,\n", scale_str);
        printf("  .FilterActivation = ENABLE\n");
        printf("};\n");
        printf("HAL_CAN_ConfigFilter(&hcan1, &filter%d);\n\n", i);
    }
}

/* ============================================================================
 * COMMAND LINE INTERFACE
 * ============================================================================ */

/* Print usage information */
static void print_usage(const char* progname) {
    printf("Usage: %s [OPTIONS] RANGE...\n", progname);
    printf("Generate optimal CAN bus hardware filters using CIDR route aggregation\n\n");

    printf("Filter Definition Options:\n");
    printf("  --std          Use 11-bit standard IDs (default)\n");
    printf("  --ext          Use 29-bit extended IDs\n");
    printf("  --data         Filter data frames only (default)\n");
    printf("  --rtr          Filter remote frames only\n");

    printf("\nOutput Control Options:\n");
    printf("  --output FORMAT  Output format: stm, slcan, hal, embedded\n");
    printf("  --mask          Force mask mode for all filters\n");
    printf("  --list          Enable list mode optimization (default)\n");
    printf("  --max N         Maximum number of filters (default: platform dependent)\n");
    printf("  --verbose       Verbose output showing algorithm details\n");

    printf("\nTesting and Verification Options:\n");
    printf("  --test ID...    Test specific IDs against generated filters\n");
    printf("  --selftest      Run built-in self-test\n");

    printf("\nInformation Options:\n");
    printf("  -h, --help      Show this help\n");

    printf("\nSLCAN Format:\n");
    printf("  Single 21-character commands: F<bank><fr1><fr2><mode><scale><ide><rtr>\n");
    printf("  Complete hardware programming with one command per filter bank\n");

    printf("\nOption Abbreviations:\n");
    printf("  Options can be abbreviated to the shortest non-ambiguous prefix.\n");
    printf("  Example: --stm, --std, --emb, --ext are valid.\n");
    printf("  Avoid: --s (ambiguous), --e (ambiguous), --st (ambiguous).\n");

    printf("\nRanges can be: 0x100 (single ID) or 0x100-0x10F (range)\n");
    printf("Example: %s --std --output stm 0x100 0x200-0x20F\n", progname);
    printf("Example: %s --ext --output slcan 0x1000000\n", progname);
#ifdef USE_EMBEDDED
    printf("\nEmbedded limits: %d filters, %d test IDs, %d ranges\n", MAX_FILTERS, MAX_TEST_IDS, MAX_RANGES);
#endif
}

/* RT-Thread wrapper that always returns 0 to prevent crash dumps */
#ifdef USE_RTTHREAD
static int canfilter_wrapper(int argc, char* argv[]) {
    int result = canfilter_cmd(argc, argv);

    /* Print terse error message on failure */
    if (result != CANFILTER_SUCCESS) {
        switch (result) {
            case CANFILTER_ERROR:
                printf("error\n");
                break;
            case CANFILTER_USAGE_ERROR:
                printf("usage error\n");
                break;
            case CANFILTER_HW_ERROR:
                printf("hw error\n");
                break;
            case CANFILTER_TEST_FAILED:
                printf("test failed\n");
                break;
        }
    }

    /* Always return 0 to RT-Thread shell */
    return 0;
}
#endif

/* Main command function - compatible with both desktop and RT-Thread */
int canfilter_cmd(int argc, char* argv[]) {
    can_range_t ranges[MAX_RANGES];
    uint32_t test_ids[MAX_TEST_IDS];
    can_filter_t filters[MAX_FILTERS];

    memset(ranges, 0, sizeof(ranges));
    memset(test_ids, 0, sizeof(test_ids));
    memset(filters, 0, sizeof(filters));

    config_t config = {
        .output_format = OUTPUT_STM32,
        .default_mode = MODE_STD,
        .frame_type = FRAME_DATA,
        .max_filters = MAX_FILTERS,
        .verbose = 0,
        .selftest_mode = 0,
        .use_list_optimization = 1  // Default to list optimization
    };

    int range_count = 0;
    int test_count = 0;
    int i;

    /* Parse command line arguments */
    for (i = 1; i < argc; i++) {
        CHECK_BOUNDS(i, argc);

        if (argv[i][0] == '-') {
            /* Check for ambiguous abbreviations */
            if (strcmp(argv[i], "--s") == 0 || strcmp(argv[i], "--e") == 0 || strcmp(argv[i], "--st") == 0) {
                fprintf(stderr, "Error: Ambiguous option '%s'\n", argv[i]);
                fprintf(stderr, "Use more characters to disambiguate\n");
                return CANFILTER_USAGE_ERROR;
            }
            /* Handle short options */
            else if (strcmp(argv[i], "-v") == 0) {
                config.verbose = 1;
            } else if (strcmp(argv[i], "-h") == 0) {
                print_usage(argv[0]);
                return CANFILTER_SUCCESS;
            }
            /* Handle long options with proper abbreviation support */
            else if (strncmp(argv[i], "--output", strlen(argv[i])) == 0) {
                if (++i < argc) {
                    if (strcmp(argv[i], "stm") == 0) config.output_format = OUTPUT_STM32;
                    else if (strcmp(argv[i], "slcan") == 0) config.output_format = OUTPUT_SLCAN;
                    else if (strcmp(argv[i], "hal") == 0) config.output_format = OUTPUT_HAL;
                    else if (strcmp(argv[i], "embedded") == 0) config.output_format = OUTPUT_EMBEDDED;
                    else {
                        fprintf(stderr, "Error: Unknown output format: %s\n", argv[i]);
                        fprintf(stderr, "Valid formats: stm, slcan, hal, embedded\n");
                        return CANFILTER_USAGE_ERROR;
                    }
                }
            } else if (strncmp(argv[i], "--std", strlen(argv[i])) == 0) {
                config.default_mode = MODE_STD;
            } else if (strncmp(argv[i], "--ext", strlen(argv[i])) == 0) {
                config.default_mode = MODE_EXT;
            } else if (strncmp(argv[i], "--data", strlen(argv[i])) == 0) {
                config.frame_type = FRAME_DATA;
            } else if (strncmp(argv[i], "--rtr", strlen(argv[i])) == 0) {
                config.frame_type = FRAME_RTR;
            } else if (strncmp(argv[i], "--mask", strlen(argv[i])) == 0) {
                config.use_list_optimization = 0;
            } else if (strncmp(argv[i], "--list", strlen(argv[i])) == 0) {
                config.use_list_optimization = 1;
            } else if (strncmp(argv[i], "--max", strlen(argv[i])) == 0) {
                if (++i < argc) {
                    config.max_filters = atoi(argv[i]);
                    if (config.max_filters <= 0 || config.max_filters > MAX_FILTERS) {
                        fprintf(stderr, "Error: Invalid max filters value (1-%d)\n", MAX_FILTERS);
                        return CANFILTER_USAGE_ERROR;
                    }
                }
            } else if (strncmp(argv[i], "--test", strlen(argv[i])) == 0) {
                /* Parse test IDs */
                while (++i < argc && test_count < MAX_TEST_IDS) {
                    CHECK_BOUNDS(i, argc);
                    CHECK_BOUNDS(test_count, MAX_TEST_IDS);

                    if (argv[i][0] == '-') {
                        i--; /* Back to option */
                        break;
                    }
                    if (parse_can_id(argv[i], &test_ids[test_count], config.default_mode)) {
                        test_count++;
                    }
                }
            } else if (strncmp(argv[i], "--selftest", strlen(argv[i])) == 0) {
                config.selftest_mode = 1;
            } else if (strncmp(argv[i], "--verbose", strlen(argv[i])) == 0) {
                config.verbose = 1;
            } else if (strncmp(argv[i], "--help", strlen(argv[i])) == 0) {
                print_usage(argv[0]);
                return CANFILTER_SUCCESS;
            } else {
                fprintf(stderr, "Error: Unknown option: %s\n", argv[i]);
                return CANFILTER_USAGE_ERROR;
            }
        } else if (range_count < MAX_RANGES) {
            /* Parse range */
            if (parse_range(argv[i], &ranges[range_count], config.default_mode, config.frame_type)) {
                range_count++;
            }
        }
    }

    /* Run self-test if requested */
    if (config.selftest_mode) {
        int result = canfilter_run_selftest();
        return result; /* Return actual test result */
    }

    /* Check for valid input */
    if (range_count == 0) {
        fprintf(stderr, "Error: No ranges specified\n");
        print_usage(argv[0]);
        return CANFILTER_USAGE_ERROR;
    }

    /* Generate filters */
    int filter_count = canfilter_generate_filters(ranges, range_count, filters, config.max_filters);

    if (filter_count <= 0) {
        printf("No filters generated\n");
        return CANFILTER_ERROR;
    }

    /* ADD HARDWARE LIMIT CHECK FOR EMBEDDED MODE */
    if (config.output_format == OUTPUT_EMBEDDED) {
#ifdef USE_RTTHREAD
        if (filter_count > MAX_CAN_HW_FILTER) {
            printf("Error: %d filters exceed hardware limit of %d\n", filter_count, MAX_CAN_HW_FILTER);
            printf("Filters not applied to hardware. Use --max %d or reduce filter ranges.\n", MAX_CAN_HW_FILTER);
            return CANFILTER_HW_ERROR;
        }
#endif
    }

    /* Output filters */
    switch (config.output_format) {
        case OUTPUT_STM32:
            canfilter_output_stm32(filters, filter_count, config.use_list_optimization);
            break;
        case OUTPUT_SLCAN:
            canfilter_output_slcan(filters, filter_count, config.use_list_optimization);
            break;
        case OUTPUT_HAL:
            canfilter_output_hal(filters, filter_count, config.use_list_optimization);
            break;
        case OUTPUT_EMBEDDED:
#ifdef USE_RTTHREAD
            if (canfilter_apply_to_hardware(filters, filter_count, "can1") != CANFILTER_SUCCESS) {
                return CANFILTER_HW_ERROR;
            }
#else
            fprintf(stderr, "Error: embedded output only available on RT-Thread\n");
            return CANFILTER_HW_ERROR;
#endif
            break;
    }

    /* Test if requested */
    if (test_count > 0) {
        printf("\nTest Results:\n");
        int passed = 0;
        for (i = 0; i < test_count; i++) {
            CHECK_BOUNDS(i, test_count);
            int result = canfilter_test_filters(filters, filter_count, test_ids[i], config.default_mode, config.frame_type);
            printf("  ID 0x%lX: %s\n", (unsigned long)test_ids[i], result ? "PASS" : "FAIL");
            if (result) passed++;
        }
        printf("Test summary: %d/%d passed\n", passed, test_count);
    }

    return CANFILTER_SUCCESS;
}

/* ============================================================================
 * PLATFORM-SPECIFIC ENTRY POINTS
 * ============================================================================ */

#ifdef USE_RTTHREAD
/* RT-Thread shell command registration
 * RECOMMENDED STACK SIZE: 2.0 kB (increased for enhanced SLCAN output)
 *
 * Stack usage breakdown (ARM32):
 * - Base algorithm: ~800 bytes
 * - SLCAN output buffers: ~400 bytes
 * - Local variables (banks, positions): ~200 bytes
 * - Function call overhead: ~400 bytes
 * - Safety margin: ~200 bytes
 * TOTAL: ~2000 bytes
 */
MSH_CMD_EXPORT_ALIAS(canfilter_wrapper, canfilter, CAN bus hardware filter generator);
#else
/* Desktop main function */
int main(int argc, char **argv) {
    return canfilter_cmd(argc, argv);
}
#endif
