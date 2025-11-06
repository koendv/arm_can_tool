#!/bin/bash
#
# Running this shell script from the "arm_can_tool" directory ensures the file unifont.h contains all characters used in the menu.
#
# Use:
# cd arm_can_tool
# ./tools/update_unifont.sh
#

( cd packages/u8g2-official-latest/tools/font/bdfconv/; make )

packages/u8g2-official-latest/tools/font/bdfconv/bdfconv -v -f 1 -m '32-127' \
-u applications/mui_form.c \
-n u8g2_font_unifont \
-o applications/unifont.h \
font/unifont-16.0.04.bdf

echo "/* run update_unifont.sh to update font */" >> applications/unifont.h
