## font

What to do if some characters do not display?


This procedure updates the font file so it contains all characters used in the display.

compile the u8g2 tool "bdfconv"

```
cd packages/u8g2-official-latest/tools/font/bdfconv
make
```
download unifont in bdf format from https://www.unifoundry.com/unifont/
```
wget https://www.unifoundry.com/pub/unifont/unifont-16.0.04/font-builds/unifont-16.0.04.bdf.gz
```

update the font file _unifont.h_ to contain all ascii characters, as well as all characters in source file _mui.c_:
```
cd arm_can_tool/
./packages/u8g2-official-latest/tools/font/bdfconv/bdfconv -v -f 1 -m '32-127' -u applications/mui_form.c  -n u8g2_font_unifont -o  applications/unifont.h font/unifont-16.0.04.bdf
```

Note: to convert ttf and otf files to bdf format, use _otf2bdf_.
An example: Fontawesome is an icon collection.
```
otf2bdf -r 72 -p 32 'Font Awesome 6 Free-Solid-900.otf' > fontawesome.bdf
```
