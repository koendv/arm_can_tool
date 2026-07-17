/*

  mui_checkbox_list.h

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

  Reference Manual:
    https://github.com/olikraus/u8g2/wiki/muiref


  Description
  -----------
  Provides a scrollable list of independent checkbox (toggle) fields.
  Each row displays a label on the left and a checkbox on the right.
  The currently focused row is drawn inverted across the full display width
  (pi style).  Scrolling and selection are handled internally; when
  navigation reaches the end of the list, focus passes naturally to the
  next or previous field on the form (e.g. a Back button).

  This follows the same pattern as MUIF_U8G2_U16_LIST: the item data lives
  entirely in a C struct array; the FDS rows carry only their visible index
  as the arg value.


  Usage
  -----

  1. Define an item table:

       static const checkbox_item_t my_items[] = {
           { "option A", &settings.option_a },
           { "option B", &settings.option_b },
           { "option C", &settings.option_c },
       };
       #define MY_ITEMS_COUNT  (sizeof(my_items) / sizeof(my_items[0]))

  2. Register in muif_list[]:

       MUIF_CHECKBOX_LIST("CL", my_items, MY_ITEMS_COUNT,
                          mui_checkbox_list_w1_pi),

  3. Place rows in the FDS — one MUI_XYA per visible row,
     arg = visible row index 0, 1, 2, ...:

       MUI_FORM(10)
       MUI_STYLE(0)
       MUI_LABEL(0, 15, "OPTIONS")
       MUI_XYA("CL", 0, 31,  0)
       MUI_XYA("CL", 0, 47,  1)
       MUI_XYA("CL", 0, 63,  2)
       MUI_XYA("CL", 0, 79,  3)
       MUI_XYA("CL", 0, 95,  4)
       MUI_XYA("CL", 0, 111, 5)
       MUI_XYT("BK", 0, 127, "Back")

  The number of MUI_XYA rows determines the visible window size.
  The total scrollable count comes from the MUIF_CHECKBOX_LIST macro.

*/

#ifndef MUI_CHECKBOX_LIST_H
#define MUI_CHECKBOX_LIST_H

#include "mui.h"
#include "mui_u8g2.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=========================================================================*/
/* Data structures                                                         */
/*=========================================================================*/

/*
 * One entry in the item table.
 *   label  text displayed to the left of the checkbox (UTF-8 ok)
 *   value  pointer to the bool variable (false = unchecked, true = checked)
 */
typedef struct
{
    const char *label;
    bool       *value;
} checkbox_item_t;

/*
 * Bundled into the MUIF data pointer by MUIF_CHECKBOX_LIST.
 * Do not fill this by hand; use the macro below.
 */
struct mui_checkbox_list_struct
{
    const checkbox_item_t *items;
    uint8_t                count;
};

typedef struct mui_checkbox_list_struct mui_checkbox_list_t;

/*=========================================================================*/
/* MUIF registration macro                                                 */
/*=========================================================================*/

/*
 * MUIF_CHECKBOX_LIST(id, items_array, count_val, cb)
 *
 * Mirrors the style of MUIF_U8G2_U16_LIST.
 * Uses a compound literal to embed the mui_checkbox_list_t inline,
 * extending its lifetime for the duration of the program — same technique
 * as MUIF_U8G2_U8_MIN_MAX and friends.
 *
 * Example:
 *   MUIF_CHECKBOX_LIST("CL", my_items, MY_ITEMS_COUNT,
 *                      mui_checkbox_list_w1_pi),
 */
#define MUIF_CHECKBOX_LIST(id, items_arr, count_val, cb)                \
    MUIF(id, MUIF_CFLAG_IS_CURSOR_SELECTABLE,                           \
    (void *)((mui_checkbox_list_t []){ { (items_arr), (count_val) } }), \
    (cb))

/*=========================================================================*/
/* Callback                                                                */
/*=========================================================================*/

/*
 * mui_checkbox_list_w1_pi
 *
 * GIF, MUIF_CHECKBOX_LIST, MUI_XYA
 *
 * One visible row of the scrollable checkbox list.
 * w1 = full display width highlight bar.
 * pi = plain when unfocused, inverted when focused.
 */
uint8_t mui_checkbox_list_w1_pi(mui_t *ui, uint8_t msg);

#ifdef __cplusplus
}
#endif

#endif /* MUI_CHECKBOX_LIST_H */
