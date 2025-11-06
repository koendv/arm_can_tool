#ifndef PINS_H
#define PINS_H

#include <rtthread.h>

#define DISP_CS_PIN          GET_PIN(D, 2)
#define DISP_DC_PIN          GET_PIN(F, 7)
#define DISP_RST_PIN         GET_PIN(C, 12)
#define LED0_PIN             GET_PIN(C, 2)
#define SD_CS_PIN            GET_PIN(A, 11)
#define SD_DETECT_PIN        GET_PIN(C, 9)
#define SD_ON_PIN            GET_PIN(C, 8)
#define SW_NEXT_PIN          GET_PIN(B, 12)
#define SW_PREV_PIN          GET_PIN(B, 2)
#define SW_SELECT_PIN        GET_PIN(B, 10)
#define TARGET_RST_IN_PIN    GET_PIN(B, 0)
#define TARGET_RST_OUT_PIN   GET_PIN(B, 13)
#define TARGET_SWCLK_DIR_PIN GET_PIN(A, 7)
#define TARGET_SWCLK_PIN     GET_PIN(A, 5)
#define TARGET_SWDIO_DIR_PIN GET_PIN(A, 6)
#define TARGET_SWDIO_PIN     GET_PIN(A, 4)
#define TARGET_TDI_DIR_PIN   GET_PIN(A, 0)
#define TARGET_TDI_PIN       GET_PIN(A, 2)
#define TARGET_TDO_DIR_PIN   GET_PIN(A, 1)
#define TARGET_TDO_PIN       GET_PIN(A, 3)
#define TARGET_OE_PIN        GET_PIN(F, 4)
#define TARGET_POWER_PIN     GET_PIN(F, 5)
#define TARGET_TXD_DIR_PIN   TARGET_TDI_DIR_PIN
#define TARGET_RXD_DIR_PIN   TARGET_TDO_DIR_PIN

#endif
