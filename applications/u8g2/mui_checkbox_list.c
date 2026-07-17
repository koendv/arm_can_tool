/*

  mui_checkbox_list.c

  Scrollable list of independent checkbox (toggle) fields for MUI.

  Universal 8bit Graphics Library (https://github.com/olikraus/u8g2/)

  Copyright (c) 2021, olikraus@gmail.com
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

*/

#include "mui.h"
#include "mui_u8g2.h"
#include "mui_checkbox_list.h"
#include <stdbool.h>

#if 1
/* copied from mui_u8g2.c */
static void u8g2_DrawCheckbox(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w, u8g2_uint_t is_checked) MUI_NOINLINE;
static void u8g2_DrawCheckbox(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w, u8g2_uint_t is_checked)
{
  u8g2_DrawFrame(u8g2, x, y-w, w, w);
  if ( is_checked )
  {
    w-=4;
    u8g2_DrawBox(u8g2, x+2, y-w-2, w, w);
  }
}
#endif

/*=========================================================================*/
/*

  uint8_t mui_checkbox_list_w1_pi(mui_t *ui, uint8_t msg)

  Description:
    One visible row of a scrollable list of independent checkbox fields.
    Multiple instances are placed on a form via MUI_XYA, each with a unique
    arg value 0, 1, 2, ... identifying its position in the visible window.

    The row draws the label from the item table and a checkbox glyph for
    the item at index (form_scroll_top + arg).  When focused, the full
    display width is inverted (pi style).

  Message Handling:
    FORM_START
      Update form_scroll_visible and form_scroll_total from arg and the item
      count.  Called once per MUI_XYA row instance during form entry.
      Mirrors mui_u8g2_u8_opt_child_mse_common FORM_START exactly.

    CURSOR_ENTER
      Return 255 to skip this row if it maps beyond the item table.
      Mirrors mui_u8g2_handle_scroll_next_prev_events CURSOR_ENTER.

    DRAW
      Draw label and checkbox for item at index (form_scroll_top + arg).

    CURSOR_SELECT / VALUE_INCREMENT / VALUE_DECREMENT
      Toggle *(items[form_scroll_top + arg].value).

    EVENT_NEXT
      Called on the last visible row (arg + 1 == form_scroll_visible).
      Scroll down if more items exist; return 1 to stay on this field.
      Return 0 at end of list so MUI moves focus to the next field.
      Mirrors mui_u8g2_handle_scroll_next_prev_events EVENT_NEXT.

    EVENT_PREV
      Called on the first visible row (arg == 0).
      Scroll up if possible; return 1 to stay on this field.
      Return 0 at top of list so MUI moves focus to the previous field.
      Mirrors mui_u8g2_handle_scroll_next_prev_events EVENT_PREV.

  User interface field list (muif):
    MUIF_CHECKBOX_LIST(id, items_array, count, mui_checkbox_list_w1_pi)
    flags: MUIF_CFLAG_IS_CURSOR_SELECTABLE

  Field definition string (fds):
    MUI_XYA("id", x, y, n)
      x  left margin for the label
      y  baseline of this visible row
      n  visible row index, starting at 0 for the topmost row

*/
/*=========================================================================*/

uint8_t mui_checkbox_list_w1_pi(mui_t *ui, uint8_t msg)
{
    u8g2_t *u8g2 = mui_get_U8g2(ui);
    mui_checkbox_list_t *list = (mui_checkbox_list_t *)muif_get_data(ui->uif);
    uint8_t arg = ui->arg;  /* capture before any call that may overwrite it */

    switch (msg)
    {
        /* ---------------------------------------------------------------- */
        case MUIF_MSG_FORM_START:
            /*
             * Accumulate the visible window size from the highest arg seen
             * across all MUI_XYA row instances on this form.
             * Set form_scroll_total from the item count once (it starts at 0
             * after mui_EnterForm clears all form_scroll_* fields).
             */
            if (ui->form_scroll_visible <= arg)
                ui->form_scroll_visible = arg + 1;
            if (ui->form_scroll_total == 0)
                ui->form_scroll_total = list->count;
            break;

        /* ---------------------------------------------------------------- */
        case MUIF_MSG_CURSOR_ENTER:
            /*
             * Skip this row if it maps beyond the item table, so the cursor
             * does not stop on an empty row at the bottom of the visible window.
             */
            if ((arg > 0) && (ui->form_scroll_top + arg >= list->count))
                return 255;
            break;

        /* ---------------------------------------------------------------- */
        case MUIF_MSG_DRAW:
        {
            uint8_t idx = ui->form_scroll_top + arg;
            if (idx >= list->count)
                break;

            u8g2_uint_t x       = mui_get_x(ui);
            u8g2_uint_t y       = mui_get_y(ui);
            u8g2_uint_t dw      = u8g2_GetDisplayWidth(u8g2);
            u8g2_uint_t ascent  = u8g2_GetAscent(u8g2);
            bool        checked = *(list->items[idx].value);

            /* label on the left */
            u8g2_DrawUTF8(u8g2, x, y, list->items[idx].label);

            /* checkbox on the right, inset one ascent-width from the edge */
            u8g2_DrawCheckbox(u8g2, dw - ascent - 2, y, ascent, checked);

            /* pi style: invert the full row width when focused.
             * Must come after drawing content so XOR font mode inverts
             * the pixels correctly (same pattern as goto_form_w1_pi). */
            if (mui_IsCursorFocus(ui))
                u8g2_DrawButtonFrame(u8g2, 0, y, U8G2_BTN_INV, dw, 0, 1);
            break;
        }

        /* ---------------------------------------------------------------- */
        case MUIF_MSG_CURSOR_SELECT:
        case MUIF_MSG_VALUE_INCREMENT:
        case MUIF_MSG_VALUE_DECREMENT:
        {
            uint8_t idx = ui->form_scroll_top + arg;
            if (idx < list->count)
                *(list->items[idx].value) = !*(list->items[idx].value);
            break;
        }

        /* ---------------------------------------------------------------- */
        case MUIF_MSG_EVENT_NEXT:
            /*
             * On the last visible row: scroll down if items remain below,
             * returning 1 so MUI keeps focus on this field (the cursor
             * stays at the last visible row and the window shifts).
             * At the end of the list return 0 so MUI advances to the next
             * field on the form (typically a Back button).
             */
            if (arg + 1 == ui->form_scroll_visible)
            {
                if (ui->form_scroll_top + ui->form_scroll_visible < list->count)
                {
                    ui->form_scroll_top++;
                    return 1;
                }
            }
            break;

        /* ---------------------------------------------------------------- */
        case MUIF_MSG_EVENT_PREV:
            /*
             * On the first visible row: scroll up if possible, returning 1
             * so MUI keeps focus on this field.
             * At the top of the list return 0 so MUI moves to the previous
             * field on the form.
             */
            if (arg == 0)
            {
                if (ui->form_scroll_top > 0)
                {
                    ui->form_scroll_top--;
                    return 1;
                }
            }
            break;

        /* ---------------------------------------------------------------- */
        case MUIF_MSG_CURSOR_LEAVE:
        case MUIF_MSG_FORM_END:
        case MUIF_MSG_TOUCH_DOWN:
        case MUIF_MSG_TOUCH_UP:
            break;
    }

    return 0;
}
