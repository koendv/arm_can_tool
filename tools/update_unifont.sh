#!/bin/bash
#
# Running this shell script from the "arm_can_tool" directory ensures the file unifont.h contains all characters used in the menu.
#
# Use:
# cd arm_can_tool
# ./tools/update_unifont.sh
#
APP_DIR="applications/u8g2"

UTF8_FILES="$APP_DIR/mui_app.c $APP_DIR/mui_form_en.c $APP_DIR/mui_form_zh.c $APP_DIR/u8g2_rtl_strings.h $APP_DIR/mui_hb_strings.h"
ALL_UTF8=$(mktemp)

# harfbuzz rewriting

tools/hbpp/hbpp.py font/unifont-17.0.04.otf 16 $APP_DIR/mui_form_en.c $APP_DIR/mui_hb_strings.h

# arabic right-to-left rewriting

tools/u8g2-rtl-strings/u8g2-rtl-strings.py $APP_DIR/mui_form_en.c > $APP_DIR/u8g2_rtl_strings.h

# create font file with all characters/glyphs that are used in the source

cat $UTF8_FILES > $ALL_UTF8

make -C packages/u8g2-official-latest/tools/font/bdfconv

packages/u8g2-official-latest/tools/font/bdfconv/bdfconv -v -f 1 -m '32-127' \
-u $ALL_UTF8 \
-n u8g2_font_unifont \
-o $APP_DIR/unifont.h \
font/unifont-17.0.04.bdf

echo "/* run update_unifont.sh to update font */" >> $APP_DIR/unifont.h
