u8g2-rtl-strings rtl_test_128x128.c > rtl_strings.h

bdfconv -u rtl_strings.h \
    -n u8g2_font_unifont \
    -o unifont.h \
    unifont-16.0.04.bdf
