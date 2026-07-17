#include "u8g2.h"
#include "rtl_strings.h"

#define DISPLAY_WIDTH 128

void draw_demo(u8g2_t *u8g2)
{
    u8g2_ClearBuffer(u8g2);

    u8g2_SetFont(u8g2, u8g2_font_unifont);

    u8g2_DrawUTF8(u8g2, DISPLAY_WIDTH - 8 * U8G2_LEN_OK, 16, U8G2_RTL_OK("موافق"));
    u8g2_DrawUTF8(u8g2, DISPLAY_WIDTH - 8 * U8G2_LEN_CANCEL, 32, U8G2_RTL_CANCEL("إلغاء"));
    u8g2_DrawUTF8(u8g2, DISPLAY_WIDTH - 8 * U8G2_LEN_BACK, 48, U8G2_RTL_BACK("رجوع"));
    u8g2_DrawUTF8(u8g2, DISPLAY_WIDTH - 8 * U8G2_LEN_NEXT, 64, U8G2_RTL_NEXT("التالي"));
    u8g2_DrawUTF8(u8g2, DISPLAY_WIDTH - 8 * U8G2_LEN_PREVIOUS, 80, U8G2_RTL_PREVIOUS("السابق"));

    u8g2_SendBuffer(u8g2);
}
