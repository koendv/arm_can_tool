#ifndef _MUI_FORM_H
#define _MUI_FORM_H

#include <mui.h>
#include "mui_checkbox_list.h"

/* mui form definition string (fds) - defines all forms and the fields on those forms. */
extern fds_t fds_data_en[]; /* english */
extern fds_t fds_data_zh[]; /* simplified chinese */

/* for MUIF_CHECKBOX_LIST */
#define STARTUP_ITEMS_COUNT 9
extern const checkbox_item_t startup_items_en[STARTUP_ITEMS_COUNT];
extern const checkbox_item_t startup_items_zh[STARTUP_ITEMS_COUNT];

#endif

