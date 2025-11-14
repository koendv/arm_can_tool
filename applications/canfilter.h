#ifndef CANFILTER_H
#define CANFILTER_H

#include <stdint.h>

/* Platform detection */
#if defined(__RTTHREAD__) || defined(RT_THREAD)
#define USE_EMBEDDED
#define USE_RTTHREAD
#endif

/* Public API - for embedded integration */
#ifdef __cplusplus
extern "C" {
#endif

/* Return code definitions */
#define CANFILTER_SUCCESS      0
#define CANFILTER_ERROR        1
#define CANFILTER_USAGE_ERROR  2
#define CANFILTER_HW_ERROR     3
#define CANFILTER_TEST_FAILED  4

/* Core data structures */
typedef enum {
    MODE_STD = 0,
    MODE_EXT = 1
} can_mode_t;

typedef enum {
    FRAME_DATA = 0,
    FRAME_RTR = 1
} frame_type_t;

typedef enum {
    OUTPUT_STM32,
    OUTPUT_SLCAN,
    OUTPUT_EMBEDDED,
    OUTPUT_HAL
} output_format_t;

typedef struct {
    uint32_t start;
    uint32_t end;
    can_mode_t mode;
    frame_type_t frame_type;
} can_range_t;

typedef struct {
    uint32_t id;
    uint32_t mask;
    can_mode_t mode;
    frame_type_t frame_type;
} can_filter_t;

typedef struct {
    output_format_t output_format;
    can_mode_t default_mode;
    frame_type_t frame_type;
    int max_filters;
    int verbose;
    int selftest_mode;
    int use_list_optimization;
} config_t;

/* Public API functions */

/**
 * @brief Generate optimal hardware filters from ID ranges using CIDR aggregation
 *
 * @param ranges Array of CAN ID ranges to filter
 * @param range_count Number of ranges in the array
 * @param filters Output array for generated filters
 * @param max_filters Maximum number of filters to generate
 * @return int Number of filters generated, or 0 on error
 */
int canfilter_generate_filters(can_range_t* ranges, int range_count,
                              can_filter_t* filters, int max_filters);

/**
 * @brief Test if a specific CAN ID passes through the generated filters
 *
 * @param filters Array of filters to test against
 * @param filter_count Number of filters in the array
 * @param id CAN ID to test
 * @param mode CAN ID mode (standard or extended)
 * @param frame_type Frame type (data or remote)
 * @return int 1 if ID passes filters, 0 if blocked
 */
int canfilter_test_filters(const can_filter_t* filters, int filter_count,
                          uint32_t id, can_mode_t mode, frame_type_t frame_type);

/**
 * @brief Run comprehensive self-test to verify algorithm correctness
 *
 * @return int CANFILTER_SUCCESS if all tests pass, CANFILTER_TEST_FAILED on failure
 */
int canfilter_run_selftest(void);

/**
 * @brief Output filters in STM32 hardware register format
 *
 * @param filters Array of filters to output
 * @param count Number of filters in array
 * @param use_list_optimization Enable hardware-aware list mode optimization
 */
void canfilter_output_stm32(const can_filter_t* filters, int count, int use_list_optimization);

/**
 * @brief Output filters in enhanced SLCAN protocol format
 *
 * @param filters Array of filters to output
 * @param count Number of filters in array
 * @param use_list_optimization Enable hardware-aware list mode optimization
 */
void canfilter_output_slcan(const can_filter_t* filters, int count, int use_list_optimization);

/**
 * @brief Output filters in STM32 HAL library code format
 *
 * @param filters Array of filters to output
 * @param count Number of filters in array
 * @param use_list_optimization Enable hardware-aware list mode optimization
 */
void canfilter_output_hal(const can_filter_t* filters, int count, int use_list_optimization);

/* RT-Thread specific hardware integration */
#ifdef USE_RTTHREAD
/**
 * @brief Apply generated filters directly to CAN hardware (RT-Thread only)
 *
 * @param filters Array of filters to apply
 * @param filter_count Number of filters in array
 * @param device_name CAN device name (e.g., "can1")
 * @return int CANFILTER_SUCCESS on success, CANFILTER_HW_ERROR on error
 */
int canfilter_apply_to_hardware(const can_filter_t* filters, int filter_count, const char* device_name);
#endif

/**
 * @brief Command line interface (also used by RT-Thread shell)
 *
 * @param argc Argument count
 * @param argv Argument vector
 * @return int CANFILTER_SUCCESS on success, error code on failure
 */
int canfilter_cmd(int argc, char* argv[]);

#ifdef __cplusplus
}
#endif

#endif /* CANFILTER_H */