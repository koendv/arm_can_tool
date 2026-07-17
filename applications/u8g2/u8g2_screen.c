/*
 * u8g2_braille.c
 *
 * Print the u8g2 framebuffer to the terminal as UTF-8 braille characters,
 * rotated 90 degrees clockwise to match physical display orientation.
 *
 * Dot/mask algorithm from braille-framebuffer (bfb.c) by Edwin Watkeys (MIT):
 *   https://github.com/edw/braille-framebuffer
 *
 * Each line is encoded into a per-line stack buffer, trailing blank cells are trimmed,
 * and the line (plus CR/LF) is written with one rt_device_write() call.
 *
 * Requires: u8g2 global, RT-Thread, full u8g2 frame buffer (_f constructor).
 */

#include <u8g2.h>
#include <rtthread.h>
#include <rtdevice.h>

extern u8g2_t u8g2;

/* all 8 dots, pattern 0x00-0xFF
 * Bit-position lookup from braille-framebuffer (bfb.c) by Edwin Watkeys.
 * Index: (y << 1) | x,  where x in {0,1}, y in {0,1,2,3} */
static const uint8_t bfb_offsets[8] = {0, 3, 1, 4, 2, 5, 6, 7};

static inline uint8_t bfb_mask(int x, int y)
{
    return (uint8_t)(1 << bfb_offsets[(y << 1) | x]);
}

/* Compute the 8-dot braille pattern for the cell at (cx, cy) */
static inline uint8_t braille_cell_pattern(uint16_t cx, uint16_t cy, uint16_t rot_h, uint16_t height, uint8_t *buf, uint8_t tile_w, uint8_t rows)
{
    uint8_t pattern = 0;
    int     dy;

    for (dy = 0; dy < rows; dy++)
    {
        uint16_t ry = cy + dy;
        if (ry >= rot_h) break;

        /* 90° CW: rotated (rx, ry), original (ry, height-1-rx) */
        if (u8x8_capture_get_pixel_1(ry, height - 1 - cx, buf, tile_w))
            pattern |= bfb_mask(0, dy);
        if (u8x8_capture_get_pixel_1(ry, height - 1 - (cx + 1), buf, tile_w))
            pattern |= bfb_mask(1, dy);
    }

    return pattern;
}

/* write a UTF-8 encoded codepoint to out. returns the number of bytes written (1-4). */

static int utf8_put_codepoint(uint32_t cp, uint8_t *out)
{
    if (cp < 0x80)
    {
        out[0] = (uint8_t)cp;
        return 1;
    }
    else if (cp < 0x800)
    {
        out[1]   = (uint8_t)(0x80 | (cp & 0x3F));
        cp     >>= 6;
        out[0]   = (uint8_t)(0xC0 | cp);
        return 2;
    }
    else if (cp < 0x10000)
    {
        out[2]   = (uint8_t)(0x80 | (cp & 0x3F));
        cp     >>= 6;
        out[1]   = (uint8_t)(0x80 | (cp & 0x3F));
        cp     >>= 6;
        out[0]   = (uint8_t)(0xE0 | cp);
        return 3;
    }
    else
    {
        out[3]   = (uint8_t)(0x80 | (cp & 0x3F));
        cp     >>= 6;
        out[2]   = (uint8_t)(0x80 | (cp & 0x3F));
        cp     >>= 6;
        out[1]   = (uint8_t)(0x80 | (cp & 0x3F));
        cp     >>= 6;
        out[0]   = (uint8_t)(0xF0 | cp);
        return 4;
    }
}

void u8g2_print_braille(void)
{
    uint8_t *buf    = u8g2_GetBufferPtr(&u8g2);
    uint8_t  tile_w = u8g2_GetBufferTileWidth(&u8g2);
    uint8_t  tile_h = u8g2_GetBufferTileHeight(&u8g2);
    uint16_t width  = (uint16_t)tile_w * 8;
    uint16_t height = (uint16_t)tile_h * 8;

    /* After 90° CW rotation: rotated_w = height, rotated_h = width */
    uint16_t      rot_w = height;
    uint16_t      rot_h = width;
    const uint8_t rows  = 4; /* pixel rows per braille cell */

    rt_device_t console = rt_console_get_device();
    if (console == RT_NULL) return;

    uint16_t cy, cx;

    for (cy = 0; cy < rot_h; cy += rows)
    {
        /* Worst case: every cell is non-blank and 3-byte UTF-8 encoded,
         * plus 2 bytes for the trailing CR/LF. */
        uint8_t  line[(rot_w / 2) * 3 + 2];
        uint8_t *p        = line;
        uint8_t *line_end = line; /* one past the last non-blank cell's bytes */

        for (cx = 0; cx < rot_w; cx += 2)
        {
            uint8_t pattern = braille_cell_pattern(cx, cy, rot_h, height, buf, tile_w, rows);

            p += utf8_put_codepoint(0x2800 + pattern, p);

            if (pattern != 0)
                line_end = p;
        }

        line_end[0] = '\r';
        line_end[1] = '\n';

        rt_device_write(console, 0, line, (size_t)(line_end - line) + 2);
    }
}

#ifdef RT_USING_FINSH
static void screendump_cmd(int argc, char **argv)
{
    (void)argv;
    if (argc > 1)
    {
        rt_kprintf("usage: screen\r\n");
        return;
    }
    u8g2_print_braille();
}
MSH_CMD_EXPORT_ALIAS(screendump_cmd, screen, print display);
#endif
