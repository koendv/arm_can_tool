#ifndef RT_CONFIG_H__
#define RT_CONFIG_H__

/* RT-Thread Kernel */

/* klibc options */

/* rt_vsnprintf options */

/* end of rt_vsnprintf options */

/* rt_vsscanf options */

/* end of rt_vsscanf options */

/* rt_memset options */

/* end of rt_memset options */

/* rt_memcpy options */

/* end of rt_memcpy options */

/* rt_memmove options */

/* end of rt_memmove options */

/* rt_memcmp options */

/* end of rt_memcmp options */

/* rt_strstr options */

/* end of rt_strstr options */

/* rt_strcasecmp options */

/* end of rt_strcasecmp options */

/* rt_strncpy options */

/* end of rt_strncpy options */

/* rt_strcpy options */

/* end of rt_strcpy options */

/* rt_strncmp options */

/* end of rt_strncmp options */

/* rt_strcmp options */

/* end of rt_strcmp options */

/* rt_strlen options */

/* end of rt_strlen options */

/* rt_strnlen options */

/* end of rt_strnlen options */
/* end of klibc options */
#define RT_NAME_MAX 16
#define RT_CPUS_NR 1
#define RT_ALIGN_SIZE 8
#define RT_THREAD_PRIORITY_32
#define RT_THREAD_PRIORITY_MAX 32
#define RT_TICK_PER_SECOND 1000
#define RT_USING_OVERFLOW_CHECK
#define RT_USING_HOOK
#define RT_HOOK_USING_FUNC_PTR
#define RT_USING_IDLE_HOOK
#define RT_IDLE_HOOK_LIST_SIZE 4
#define IDLE_THREAD_STACK_SIZE 512
#define RT_USING_TIMER_SOFT
#define RT_TIMER_THREAD_PRIO 4
#define RT_TIMER_THREAD_STACK_SIZE 512

/* kservice options */

/* end of kservice options */
#define RT_USING_DEBUG
#define RT_DEBUGING_ASSERT
#define RT_DEBUGING_COLOR
#define RT_DEBUGING_CONTEXT

/* Inter-Thread communication */

#define RT_USING_SEMAPHORE
#define RT_USING_MUTEX
#define RT_USING_EVENT
#define RT_USING_MAILBOX
#define RT_USING_MESSAGEQUEUE
/* end of Inter-Thread communication */

/* Memory Management */

#define RT_USING_MEMPOOL
#define RT_USING_SMALL_MEM
#define RT_USING_MEMHEAP
#define RT_MEMHEAP_FAST_MODE
#define RT_USING_SMALL_MEM_AS_HEAP
#define RT_USING_HEAP
/* end of Memory Management */
#define RT_USING_DEVICE
#define RT_USING_CONSOLE
#define RT_CONSOLEBUF_SIZE 128
#define RT_CONSOLE_DEVICE_NAME "uart6"
#define RT_VER_NUM 0x50201
#define RT_BACKTRACE_LEVEL_MAX_NR 32
/* end of RT-Thread Kernel */
#define RT_USING_HW_ATOMIC
#define RT_USING_CPU_FFS
#define ARCH_ARM
#define ARCH_ARM_CORTEX_M
#define ARCH_ARM_CORTEX_M4

/* RT-Thread Components */

#define RT_USING_COMPONENTS_INIT
#define RT_USING_USER_MAIN
#define RT_MAIN_THREAD_STACK_SIZE 2048
#define RT_MAIN_THREAD_PRIORITY 10
#define RT_USING_MSH
#define RT_USING_FINSH
#define FINSH_USING_MSH
#define FINSH_THREAD_NAME "tshell"
#define FINSH_THREAD_PRIORITY 20
#define FINSH_THREAD_STACK_SIZE 4096
#define FINSH_USING_HISTORY
#define FINSH_HISTORY_LINES 5
#define FINSH_USING_SYMTAB
#define FINSH_CMD_SIZE 80
#define MSH_USING_BUILT_IN_COMMANDS
#define FINSH_USING_DESCRIPTION
#define FINSH_ARG_MAX 10
#define FINSH_USING_OPTION_COMPLETION

/* DFS: device virtual file system */

#define RT_USING_DFS
#define DFS_USING_POSIX
#define DFS_USING_WORKDIR
#define DFS_FD_MAX 16
#define RT_USING_DFS_V1
#define DFS_FILESYSTEMS_MAX 4
#define DFS_FILESYSTEM_TYPES_MAX 4
#define RT_USING_DFS_ELMFAT

/* elm-chan's FatFs, Generic FAT Filesystem Module */

#define RT_DFS_ELM_CODE_PAGE 437
#define RT_DFS_ELM_WORD_ACCESS
#define RT_DFS_ELM_USE_LFN_3
#define RT_DFS_ELM_USE_LFN 3
#define RT_DFS_ELM_LFN_UNICODE_0
#define RT_DFS_ELM_LFN_UNICODE 0
#define RT_DFS_ELM_MAX_LFN 255
#define RT_DFS_ELM_DRIVES 2
#define RT_DFS_ELM_MAX_SECTOR_SIZE 512
#define RT_DFS_ELM_USE_ERASE
#define RT_DFS_ELM_REENTRANT
#define RT_DFS_ELM_MUTEX_TIMEOUT 3000
#define RT_DFS_ELM_USE_EXFAT
/* end of elm-chan's FatFs, Generic FAT Filesystem Module */
#define RT_USING_DFS_DEVFS
#define RT_USING_DFS_ROMFS
/* end of DFS: device virtual file system */
#define RT_USING_FAL
#define FAL_PART_HAS_TABLE_CFG
#define FAL_USING_SFUD_PORT
#define FAL_USING_NOR_FLASH_DEV_NAME "norflash0"

/* Device Drivers */

#define RT_USING_DEVICE_IPC
#define RT_UNAMED_PIPE_NUMBER 64
#define RT_USING_SERIAL
#define RT_USING_SERIAL_V2
#define RT_SERIAL_BUF_STRATEGY_OVERWRITE
#define RT_SERIAL_USING_DMA
#define RT_USING_CAN
#define RT_CAN_USING_HDR
#define RT_CANMSG_BOX_SZ 16
#define RT_CANSND_BOX_NUM 1
#define RT_CANSND_MSG_TIMEOUT 100
#define RT_CAN_NB_TX_FIFO_SIZE 256
#define RT_USING_I2C
#define RT_USING_I2C_BITOPS
#define RT_USING_SOFT_I2C
#define RT_USING_SOFT_I2C1
#define RT_SOFT_I2C1_SCL_PIN 22
#define RT_SOFT_I2C1_SDA_PIN 23
#define RT_SOFT_I2C1_BUS_NAME "i2c1"
#define RT_SOFT_I2C1_TIMING_DELAY 10
#define RT_SOFT_I2C1_TIMING_TIMEOUT 10
#define RT_USING_SOFT_I2C3
#define RT_SOFT_I2C3_SCL_PIN 32
#define RT_SOFT_I2C3_SDA_PIN 33
#define RT_SOFT_I2C3_BUS_NAME "i2c3"
#define RT_SOFT_I2C3_TIMING_DELAY 10
#define RT_SOFT_I2C3_TIMING_TIMEOUT 10
#define RT_USING_ADC
#define RT_USING_MTD_NOR
#define RT_USING_RTC
#define RT_USING_SPI
#define RT_USING_SPI_MSD
#define RT_USING_SFUD
#define RT_SFUD_USING_SFDP
#define RT_SFUD_USING_FLASH_INFO_TABLE
#define RT_SFUD_SPI_MAX_HZ 50000000
#define RT_USING_WDT
#define RT_USING_PIN
#define RT_USING_HWTIMER
#define RT_USING_CHERRYUSB
#define RT_CHERRYUSB_DEVICE
#define RT_CHERRYUSB_DEVICE_SPEED_HS
#define RT_CHERRYUSB_DEVICE_DWC2_AT
#define RT_CHERRYUSB_DEVICE_CDC_ACM
#define CONFIG_USBDEV_REQUEST_BUFFER_LEN 512
#define CONFIG_USBDEV_MSC_MAX_BUFSIZE 512
#define RT_CHERRYUSB_DEVICE_TEMPLATE_NONE
/* end of Device Drivers */

/* C/C++ and POSIX layer */

/* ISO-ANSI C layer */

/* Timezone and Daylight Saving Time */

#define RT_LIBC_USING_LIGHT_TZ_DST
#define RT_LIBC_TZ_DEFAULT_HOUR 8
#define RT_LIBC_TZ_DEFAULT_MIN 0
#define RT_LIBC_TZ_DEFAULT_SEC 0
/* end of Timezone and Daylight Saving Time */
/* end of ISO-ANSI C layer */

/* POSIX (Portable Operating System Interface) layer */

#define RT_USING_POSIX_FS

/* Interprocess Communication (IPC) */


/* Socket is in the 'Network' category */

/* end of Interprocess Communication (IPC) */
/* end of POSIX (Portable Operating System Interface) layer */
/* end of C/C++ and POSIX layer */

/* Network */

/* end of Network */

/* Memory protection */

/* end of Memory protection */

/* Utilities */

#define RT_USING_ULOG
#define ULOG_OUTPUT_LVL_D
#define ULOG_OUTPUT_LVL 7
#define ULOG_USING_ISR_LOG
#define ULOG_ASSERT_ENABLE
#define ULOG_LINE_BUF_SIZE 128
#define ULOG_USING_ASYNC_OUTPUT
#define ULOG_ASYNC_OUTPUT_BUF_SIZE 2048
#define ULOG_ASYNC_OUTPUT_BY_THREAD
#define ULOG_ASYNC_OUTPUT_THREAD_STACK 2048
#define ULOG_ASYNC_OUTPUT_THREAD_PRIORITY 30

/* log format */

#define ULOG_USING_COLOR
#define ULOG_OUTPUT_LEVEL
#define ULOG_OUTPUT_TAG
/* end of log format */
#define ULOG_BACKEND_USING_CONSOLE
#define ULOG_BACKEND_USING_FILE
/* end of Utilities */

/* Using USB legacy version */

/* end of Using USB legacy version */
/* end of RT-Thread Components */

/* RT-Thread Utestcases */

/* end of RT-Thread Utestcases */

/* RT-Thread online packages */

/* IoT - internet of things */


/* Wi-Fi */

/* Marvell WiFi */

/* end of Marvell WiFi */

/* Wiced WiFi */

/* end of Wiced WiFi */

/* CYW43012 WiFi */

/* end of CYW43012 WiFi */

/* BL808 WiFi */

/* end of BL808 WiFi */

/* CYW43439 WiFi */

/* end of CYW43439 WiFi */
/* end of Wi-Fi */

/* IoT Cloud */

/* end of IoT Cloud */
/* end of IoT - internet of things */

/* security packages */

/* end of security packages */

/* language packages */

/* JSON: JavaScript Object Notation, a lightweight data-interchange format */

/* end of JSON: JavaScript Object Notation, a lightweight data-interchange format */

/* XML: Extensible Markup Language */

/* end of XML: Extensible Markup Language */
#define PKG_USING_LUA
#define PKG_USING_LUA_LATEST_VERSION
#define LUA_USING_PORTING_V534
/* end of language packages */

/* multimedia packages */

/* LVGL: powerful and easy-to-use embedded GUI library */

/* end of LVGL: powerful and easy-to-use embedded GUI library */

/* u8g2: a monochrome graphic library */

#define PKG_USING_U8G2_OFFICIAL
#define U8G2_USE_HW_SPI
#define U8G2_SPI_BUS_NAME "spi2"
#define U8G2_SPI_DEVICE_NAME "spi22"

/* U8G2 Examples */

/* end of U8G2 Examples */
#define PKG_USING_U8G2_OFFICIAL_LATEST_VERSION
#define PKG_U8G2_OFFICIAL_VER_NUM 0x99999
/* end of u8g2: a monochrome graphic library */
/* end of multimedia packages */

/* tools packages */

#define PKG_USING_BLACKMAGIC
#define BLACKMAGIC_ENABLE_GPIO
#define BLACKMAGIC_ENABLE_RTT
#define BLACKMAGIC_ENABLE_DEBUG
#define PKG_USING_BLACKMAGIC_LATEST_VERSION
#define BLACKMAGIC_VERSION "latest"
#define PKG_USING_CMBACKTRACE
#define PKG_CMBACKTRACE_PLATFORM_M4
#define PKG_CMBACKTRACE_DUMP_STACK
#define PKG_CMBACKTRACE_PRINT_ENGLISH
#define PKG_USING_CMBACKTRACE_LATEST_VERSION
#define PKG_CMBACKTRACE_VER_NUM 0x99999
#define PKG_USING_CPU_USAGE
#define PKG_USING_CPU_USAGE_LATEST_VERSION
/* end of tools packages */

/* system packages */

/* enhanced kernel services */

#define PKG_USING_RT_KPRINTF_THREADSAFE
#define PKG_USING_RT_KPRINTF_THREADSAFE_LATEST_VERSION
/* end of enhanced kernel services */

/* acceleration: Assembly language or algorithmic acceleration packages */

/* end of acceleration: Assembly language or algorithmic acceleration packages */

/* CMSIS: ARM Cortex-M Microcontroller Software Interface Standard */

#define PKG_USING_CMSIS_CORE
#define PKG_USING_CMSIS_CORE_LATEST_VERSION
/* end of CMSIS: ARM Cortex-M Microcontroller Software Interface Standard */

/* Micrium: Micrium software products porting for RT-Thread */

/* end of Micrium: Micrium software products porting for RT-Thread */
#define PKG_USING_LITTLEFS
#define PKG_USING_LITTLEFS_LATEST_VERSION
#define LFS_READ_SIZE 256
#define LFS_PROG_SIZE 256
#define LFS_BLOCK_SIZE 4096
#define LFS_CACHE_SIZE 256
#define LFS_BLOCK_CYCLES -1
#define LFS_THREADSAFE
#define LFS_LOOKAHEAD_MAX 128
#define RT_DEF_LFS_DRIVERS 1
/* end of system packages */

/* peripheral libraries and drivers */

/* HAL & SDK Drivers */

/* STM32 HAL & SDK Drivers */

/* end of STM32 HAL & SDK Drivers */

/* Infineon HAL Packages */

/* end of Infineon HAL Packages */

/* Kendryte SDK */

/* end of Kendryte SDK */

/* WCH HAL & SDK Drivers */

/* end of WCH HAL & SDK Drivers */

/* AT32 HAL & SDK Drivers */

#define PKG_USING_AT32F402_405_HAL_DRIVER
#define PKG_USING_AT32F402_405_HAL_DRIVER_LATEST_VERSION
#define PKG_USING_AT32F402_405_CMSIS_DRIVER
#define PKG_USING_AT32F402_405_CMSIS_DRIVER_LATEST_VERSION
/* end of AT32 HAL & SDK Drivers */

/* HC32 DDL Drivers */

/* end of HC32 DDL Drivers */

/* NXP HAL & SDK Drivers */

/* end of NXP HAL & SDK Drivers */
/* end of HAL & SDK Drivers */

/* sensors drivers */

/* end of sensors drivers */

/* touch drivers */

/* end of touch drivers */
/* end of peripheral libraries and drivers */

/* AI packages */

/* end of AI packages */

/* Signal Processing and Control Algorithm Packages */

/* end of Signal Processing and Control Algorithm Packages */

/* miscellaneous packages */

/* project laboratory */

/* end of project laboratory */

/* samples: kernel and components samples */

/* end of samples: kernel and components samples */

/* entertainment: terminal games and other interesting software packages */

/* end of entertainment: terminal games and other interesting software packages */
/* end of miscellaneous packages */

/* Arduino libraries */


/* Projects and Demos */

/* end of Projects and Demos */

/* Sensors */

/* end of Sensors */

/* Display */

/* end of Display */

/* Timing */

/* end of Timing */

/* Data Processing */

/* end of Data Processing */

/* Data Storage */

/* Communication */

/* end of Communication */

/* Device Control */

/* end of Device Control */

/* Other */

/* end of Other */

/* Signal IO */

/* end of Signal IO */

/* Uncategorized */

/* end of Arduino libraries */
/* end of RT-Thread online packages */
#define SOC_FAMILY_AT32
#define SOC_SERIES_AT32F405

/* Hardware Drivers Config */

#define SOC_AT32F405RCT7

/* Onboard Peripheral Drivers */

#define BSP_USING_SERIAL
/* end of Onboard Peripheral Drivers */

/* On-chip Peripheral Drivers */

#define BSP_USING_GPIO
#define BSP_USING_RTC
#define BSP_RTC_USING_LEXT
#define BSP_USING_UART
#define BSP_USING_UART2
#define BSP_UART2_RX_USING_DMA
#define BSP_UART2_TX_USING_DMA
#define BSP_UART2_RX_BUFSIZE 256
#define BSP_UART2_TX_BUFSIZE 128
#define BSP_UART2_DMA_PING_BUFSIZE 64
#define BSP_USING_UART3
#define BSP_UART3_RX_USING_DMA
#define BSP_UART3_TX_USING_DMA
#define BSP_UART3_RX_BUFSIZE 256
#define BSP_UART3_TX_BUFSIZE 256
#define BSP_UART3_DMA_PING_BUFSIZE 64
#define BSP_USING_UART6
#define BSP_UART6_RX_USING_DMA
#define BSP_UART6_TX_USING_DMA
#define BSP_UART6_RX_BUFSIZE 256
#define BSP_UART6_TX_BUFSIZE 256
#define BSP_UART6_DMA_PING_BUFSIZE 64
#define BSP_USING_UART7
#define BSP_UART7_RX_USING_DMA
#define BSP_UART7_RX_BUFSIZE 4096
#define BSP_UART7_TX_BUFSIZE 0
#define BSP_UART7_DMA_PING_BUFSIZE 1024
#define BSP_USING_HWTIMER
#define BSP_USING_HWTMR3
#define BSP_USING_SPI
#define BSP_USING_SPI2
#define BSP_SPI2_TX_USING_DMA
#define BSP_SPI2_RX_USING_DMA
#define BSP_USING_ADC
#define BSP_USING_ADC1
#define BSP_USING_CAN
#define BSP_USING_CAN1
/* end of On-chip Peripheral Drivers */
/* end of Hardware Drivers Config */

#endif
