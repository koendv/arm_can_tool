# u8g2-rtl-strings 1

## Name

u8g2-rtl-strings - extract and reshape right-to-left strings for u8g2

## Synopsis

`u8g2-rtl-strings` [OPTIONS] FILE...

`u8g2-rtl-strings` [OPTIONS] < FILE

## Description

u8g2-rtl-strings reads C source files and extracts macros of the form

    U8G2_RTL_NAME("text")

The contained string is reshaped using Arabic shaping rules and then
reordered according to the Unicode bidirectional algorithm.

The resulting visual-order UTF-8 string is emitted as a C preprocessor
macro suitable for use with u8g2_DrawUTF8().

Input files must be UTF-8 encoded.

Lines beginning with // or /* are ignored.

Macros must appear entirely on a single line.

## Options

`-e, --escape`
:   Output Unicode escape sequences (\uXXXX) instead of raw UTF-8.

## Output

The program writes a C header file to standard output containing
preprocessed strings:

    #define U8G2_RTL_NAME(x) "visual text" /* original text */

When the `-e` option is used, the visual text is output as
Unicode escape sequences:

    #define U8G2_RTL_NAME(x) "\u0645\u0631\u062D\u0628\u0627" /* مرحبا */

## Font Pipeline

The tool is intended for use with u8g2 and GNU Unifont.

Typical workflow:

1. Extract and reshape RTL strings

       u8g2-rtl-strings src/*.c > rtl_strings.h

2. Generate a subset font using bdfconv

       bdfconv -u rtl_strings.h \
           -n u8g2_font_unifont \
           -o unifont.h \
           unifont.bdf

The -u option instructs bdfconv to scan the input file and include all
UTF-8 Unicode codepoints encountered.

Because u8g2 does not perform Arabic shaping at runtime, the text must
be reshaped offline. GNU Unifont already contains the required Arabic
presentation forms.

## Examples

Basic usage with UTF-8 output:

    u8g2-rtl-strings rtl_test_128x128.c > rtl_strings.h

Using escape sequences for maximum portability:

    u8g2-rtl-strings -e rtl_test_128x128.c > rtl_strings.h

Process multiple files from stdin:

    cat *.c | u8g2-rtl-strings > rtl_strings.h

## See Also

bdfconv(1), u8g2