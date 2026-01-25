#ifndef CANFILTER_H
#define CANFILTER_H

/**
 * CAN hardware filter
 * from https://github.com/koendv/canfilter
 */

/* Controller types */
typedef enum
{
    CANFILTER_DEV_NONE = 0, /* no hardware filter */
    CANFILTER_DEV_BXCAN_F0, /* bxcan on F0/F1/F3, 14 filters */
    CANFILTER_DEV_BXCAN_F4, /* bxcan on F4/F7, 28 filters */
    CANFILTER_DEV_FDCAN_G0, /* bosch m_can, 28 standard, 8 extended filters */
    CANFILTER_DEV_FDCAN_H7, /* bosch m_can, 128 standard, 64 extended filters */
} canfilter_hardware_t;

/* hardware filter type */

struct canfilter_info
{
    uint8_t dev; /* one of canfilter_hardware_t */
    uint8_t reserved[3];
} __attribute__((packed)) __attribute__((aligned(4)));

/* hardware filter for bxcan hardware, stm32f0/f1/f3 */

struct canfilter_bxcan_f0
{
    uint8_t  dev; // CANFILTER_DEV_BXCAN_F0
    uint8_t  reserved[3];
    uint32_t fs1r;
    uint32_t fm1r;
    uint32_t ffa1r;
    uint32_t fa1r;
    uint32_t fr1[14];
    uint32_t fr2[14];
} __attribute__((packed, aligned(4)));

/* hardware filter for bxcan hardware, stm32f4/f7 */

struct canfilter_bxcan_f4
{
    uint8_t  dev; // CANFILTER_DEV_BXCAN_F4
    uint8_t  reserved[3];
    uint32_t fs1r;
    uint32_t fm1r;
    uint32_t ffa1r;
    uint32_t fa1r;
    uint32_t fr1[28];
    uint32_t fr2[28];
} __attribute__((packed, aligned(4)));

/* hardware filter for fdcan hardware, stm32g0 */

struct canfilter_fdcan_g0
{
    uint8_t  dev;              // CANFILTER_DEV_FDCAN_G0
    uint8_t  std_filter_nbr;   // number of standard filters
    uint8_t  ext_filter_nbr;   // number of extended filters
    uint8_t  reserved[1];
    uint32_t std_filter[28];   // Fixed-size array for standard filters
    uint32_t ext_filter[8][2]; // Fixed-size array for extended filters
} __attribute__((packed, aligned(4)));

/* hardware filter for fdcan hardware, stm32h7 */

struct canfilter_fdcan_h7
{
    uint8_t  dev;               // CANFILTER_DEV_FDCAN_H7
    uint8_t  std_filter_nbr;    // number of standard filters
    uint8_t  ext_filter_nbr;    // number of extended filters
    uint8_t  reserved[1];
    uint32_t std_filter[128];   // Fixed-size array for standard filters
    uint32_t ext_filter[64][2]; // Fixed-size array for extended filters
} __attribute__((packed, aligned(4)));

#endif
