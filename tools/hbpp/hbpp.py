#!/usr/bin/env python3
r"""
hbpp.py - HarfBuzz Preprocessor for u8g2

Scans a C source file for HB_STR_NAME and MUI_XYHB_NAME macros, shapes each
UTF-8 string using HarfBuzz, and emits a C header with pre-shaped glyph arrays
for use with u8g2_DrawHB().

Usage:
    python3 hbpp.py [-v] FONT.ttf SIZE INPUT.c OUTPUT.h

Arguments:
    FONT.ttf    OpenType or TrueType font file (relative to cwd)
    SIZE        Render size in pixels (integer)
    INPUT.c     C source file to scan (read-only)
    OUTPUT.h    C header file to generate

Recognised source patterns (one per line, no multiline, no escaped quotes):

    HB_STR_NAME("utf-8 string")
        Generates: #define HB_STR_NAME(z)  <index>
        Use for direct draw: u8g2_DrawHB(&u8g2, x, y, stem_strings[HB_STR_NAME("")])
        or as a plain integer where needed.

    MUI_XYHB_NAME(id, x, y, "utf-8 string")
        Generates: #define MUI_XYHB_NAME(id, x, y, s)  "A" "HB" "\xXX" "\xYY" "\xII"
        where XX=x, YY=y, II=index. All parameters consumed; expansion is a
        raw MUI_XYA-equivalent byte string, bypassing MUI_## token-paste issues.
        Use directly in MUI form strings.

Both patterns contribute to the same stem_strings[] table and may share a NAME
(same array, same index, both macros generated).

NAME must be unique — a NAME may not appear twice in the same pattern.
"""

import sys
import re
import os
import argparse
from pathlib import Path

try:
    import uharfbuzz as hb
except ImportError:
    print("Error: uharfbuzz not installed.  pip install uharfbuzz", file=sys.stderr)
    sys.exit(1)


# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------

parser = argparse.ArgumentParser(
    description="HarfBuzz preprocessor for u8g2: shapes UTF-8 strings at build time."
)
parser.add_argument("font",   help="Font file (.ttf/.otf), relative to cwd")
parser.add_argument("size",   type=int, help="Render size in pixels")
parser.add_argument("input",  help="C source file to scan")
parser.add_argument("output", help="C header file to generate")
parser.add_argument("-v", "--verbose", action="store_true",
                    help="Print progress to stderr")
args = parser.parse_args()


# ---------------------------------------------------------------------------
# Regex patterns
# ---------------------------------------------------------------------------

# HB_STR_NAME("utf-8 string")
HB_STR_RE = re.compile(
    r'\bHB_STR_([A-Za-z0-9_]+)\s*\(\s*"([^"]*)"\s*\)'
)

# MUI_XYHB_NAME(id, x, y, "utf-8 string")
# x and y must be integer literals (0-255)
MUI_XYHB_RE = re.compile(
    r'\bMUI_XYHB_([A-Za-z0-9_]+)\s*\(\s*"[^"]*"\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*"([^"]*)"\s*\)'
)


# ---------------------------------------------------------------------------
# Font loading
# ---------------------------------------------------------------------------

def load_font(font_path, size_px):
    with open(font_path, "rb") as f:
        font_data = f.read()
    face = hb.Face(font_data)
    font = hb.Font(face)
    upem = face.upem
    font.scale = (upem, upem)
    if args.verbose:
        print(f"  font: {os.path.basename(font_path)}, upem={upem}, size={size_px}px",
              file=sys.stderr)
    return font, upem, size_px


# ---------------------------------------------------------------------------
# Shaping
# ---------------------------------------------------------------------------

def shape_string(font_tuple, text):
    """
    Returns list of dicts: glyph_id, cluster, ax, ay, dx, dy, codepoint.
    ax/ay/dx/dy in upem units.
    """
    font, upem, size_px = font_tuple

    buf = hb.Buffer()
    buf.add_str(text)
    buf.guess_segment_properties()
    hb.shape(font, buf)

    infos        = buf.glyph_infos
    positions    = buf.glyph_positions
    unicode_list = [ord(c) for c in text]

    cl_last   = -1
    cl_offset =  0
    glyphs    = []

    for info, pos in zip(infos, positions):
        cl = info.cluster
        if cl != cl_last:
            cl_last   = cl
            cl_offset = 0
        else:
            cl_offset += 1

        idx       = cl_last + cl_offset
        codepoint = unicode_list[idx] if idx < len(unicode_list) else 0

        glyphs.append({
            'glyph_id':  info.codepoint,
            'cluster':   cl,
            'ax':        pos.x_advance,
            'ay':        pos.y_advance,
            'dx':        pos.x_offset,
            'dy':        pos.y_offset,
            'codepoint': codepoint,
        })

    return glyphs


def to_px(val, upem, size_px):
    return int(round(val * size_px / upem))


# ---------------------------------------------------------------------------
# Binary array generation  (replicates hbshape2u8g2.c pen logic exactly)
# ---------------------------------------------------------------------------

def glyphs_to_entries(glyphs, upem, size_px):
    """
    Returns list of (codepoint, delta_x_px, delta_y_px).
    Warns if any delta exceeds int8_t range.
    """
    entries   = []
    x = 0;  y = 0
    cl_last   = -1
    cl_offset =  0
    cl_x = 0;  cl_y = 0
    old_pen_x = 0;  old_pen_y = 0
    pen_x     = 0;  pen_y     = 0

    for g in glyphs:
        cl = g['cluster']
        if cl != cl_last:
            cl_last   = cl
            cl_offset = 0
            cl_x = x;  cl_y = y
        else:
            cl_offset += 1

        old_pen_x = pen_x
        old_pen_y = pen_y
        pen_x = cl_x + to_px(g['dx'], upem, size_px)
        pen_y = cl_y + to_px(g['dy'], upem, size_px)

        dx = pen_x - old_pen_x
        dy = pen_y - old_pen_y

        if not (-128 <= dx <= 127) or not (-128 <= dy <= 127):
            print(
                f"Warning: delta ({dx},{dy}) for U+{g['codepoint']:04X} "
                f"exceeds int8_t range — glyph will render incorrectly.",
                file=sys.stderr
            )

        entries.append((g['codepoint'], dx, dy))

        x += to_px(g['ax'], upem, size_px)
        y += to_px(g['ay'], upem, size_px)

    return entries


# ---------------------------------------------------------------------------
# Cluster comment
# ---------------------------------------------------------------------------

def cluster_comment(glyphs):
    parts = []
    for g in glyphs:
        cp = g['codepoint']
        try:
            ch = chr(cp)
            parts.append(ch if ch.isprintable() else f"U+{cp:04X}")
        except (ValueError, OverflowError):
            parts.append(f"U+{cp:04X}")
    return "// " + " ".join(parts)


# ---------------------------------------------------------------------------
# Array formatting
# ---------------------------------------------------------------------------

def format_array(name, entries):
    lines = [f"static const uint8_t _hb_{name}_data[] U8X8_PROGMEM = {{"]
    for enc, dx, dy in entries:
        hi  = (enc >> 8) & 0xFF
        lo  =  enc       & 0xFF
        dx8 =  dx        & 0xFF
        dy8 =  dy        & 0xFF
        try:
            label = chr(enc) if 32 <= enc <= 126 else f"U+{enc:04X}"
        except (ValueError, OverflowError):
            label = f"U+{enc:04X}"
        lines.append(
            f"  0x{hi:02x}, 0x{lo:02x}, 0x{dx8:02x}, 0x{dy8:02x},"
            f"  // '{label}'"
        )
    lines.append("  0x00, 0x00  // end")
    lines.append("};")
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# MUI_XYHB expansion
# MUI_XYA(id, x, y, a) expands to: "A" id MUI_##x MUI_##y MUI_##a
# We replicate this at the byte level to avoid MUI_## token-paste issues.
# ---------------------------------------------------------------------------

def mui_xyhb_expansion(x, y, index):
    """
    Returns the string literal expansion equivalent to MUI_XYA("HB", x, y, index),
    encoded as raw hex byte strings.
    'A' is the MUI command byte for XYA fields.
    """
    if not (0 <= x <= 255):
        print(f"Warning: MUI_XYHB x={x} out of range 0-255.", file=sys.stderr)
    if not (0 <= y <= 255):
        print(f"Warning: MUI_XYHB y={y} out of range 0-255.", file=sys.stderr)
    if not (0 <= index <= 255):
        print(f"Warning: MUI_XYHB index={index} out of range 0-255.", file=sys.stderr)
    return f'"A" "HB" "\\x{x:02x}" "\\x{y:02x}" "\\x{index:02x}"'


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    font_path   = args.font
    size_px     = args.size
    input_path  = args.input
    output_path = args.output

    if not os.path.exists(font_path):
        print(f"Error: font not found: {font_path}", file=sys.stderr)
        sys.exit(1)

    with open(input_path, "r", encoding="utf-8") as f:
        lines = f.readlines()

    font_tuple = load_font(font_path, size_px)
    stem       = Path(output_path).stem
    guard      = stem.upper().replace("-", "_") + "_H"
    table_name = stem + "_strings"

    out    = []
    errors = 0

    # name -> { 'lineno', 'index', 'entries', 'glyphs',
    #           'hb_str': bool, 'mui_xyhb': [(x,y), ...] }
    seen  = {}
    names = []   # insertion order

    # ---- one pass ----
    for lineno, line in enumerate(lines, 1):

        # HB_STR_NAME("string")
        for mm in HB_STR_RE.finditer(line):
            name = mm.group(1)
            text = mm.group(2)

            if name in seen and seen[name]['hb_str']:
                print(
                    f"Error line {lineno}: HB_STR_{name} already defined "
                    f"at line {seen[name]['lineno']}.",
                    file=sys.stderr
                )
                errors += 1
                continue

            entry = _ensure_name(seen, names, name, text, lineno,
                                 font_tuple, out)
            if entry:
                entry['hb_str'] = True

        # MUI_XYHB_NAME(id, x, y, "string")
        for mm in MUI_XYHB_RE.finditer(line):
            name = mm.group(1)
            x    = int(mm.group(2))
            y    = int(mm.group(3))
            text = mm.group(4)

            if name in seen and (x, y) in seen[name]['mui_xyhb']:
                print(
                    f"Error line {lineno}: MUI_XYHB_{name} with x={x},y={y} "
                    f"already defined.",
                    file=sys.stderr
                )
                errors += 1
                continue

            entry = _ensure_name(seen, names, name, text, lineno,
                                 font_tuple, out)
            if entry:
                entry['mui_xyhb'].append((x, y))

    if errors:
        print(f"{errors} error(s). Output not written.", file=sys.stderr)
        sys.exit(1)

    if not names:
        print("Warning: no HB_STR_ or MUI_XYHB_ macros found.", file=sys.stderr)

    # ---- emit header ----
    header = (
        f"/* Generated by hbpp.py — do not edit manually */\n"
        f"/* Source:  {os.path.basename(input_path)} */\n"
        f"/* Command: {' '.join(sys.argv)} */\n"
        f"\n"
        f"#ifndef {guard}\n"
        f"#define {guard}\n"
        f"\n"
        f"#include <stdint.h>\n"
        f"\n"
    )

    # string table
    table_lines = [f"static const uint8_t *{table_name}[] = {{"]
    for name in names:
        i = seen[name]['index']
        table_lines.append(f"    _hb_{name}_data,  /* {i} */")
    table_lines.append("};\n")

    # defines
    define_lines = []
    for name in names:
        entry = seen[name]
        i     = entry['index']
        if entry['hb_str']:
            define_lines.append(f"#define HB_STR_{name}(z)  {i}")
        for (x, y) in entry['mui_xyhb']:
            exp = mui_xyhb_expansion(x, y, i)
            define_lines.append(
                f"#define MUI_XYHB_{name}(id, x, y, s)  {exp}"
            )
    define_lines.append("")

    # callback hint
    callback = (
        f"/*\n"
        f" * MUI callback — implement once in your project:\n"
        f" *\n"
        f" *   MUIF_RO(\"HB\", mui_draw_hb)\n"
        f" *\n"
        f" *   uint8_t mui_draw_hb(mui_t *ui, uint8_t msg) {{\n"
        f" *       if (msg == MUIF_MSG_DRAW)\n"
        f" *           u8g2_DrawHB(mui_get_U8g2(ui), ui->x, ui->y,\n"
        f" *                       {table_name}[ui->arg]);\n"
        f" *       return 0;\n"
        f" *   }}\n"
        f" */\n"
        f"\n"
        f"#endif /* {guard} */\n"
    )

    with open(output_path, "w", encoding="utf-8") as f:
        f.write(header)
        f.writelines(out)          # arrays (emitted during scan)
        f.write("\n".join(table_lines) + "\n")
        f.write("\n".join(define_lines) + "\n")
        f.write(callback)

    if args.verbose:
        print(f"Written: {output_path}", file=sys.stderr)
    print(f"{len(names)} string(s) processed → {output_path}")


# ---------------------------------------------------------------------------
# Helper — ensure a name is registered and its array emitted exactly once
# ---------------------------------------------------------------------------

def _ensure_name(seen, names, name, text, lineno, font_tuple, out):
    if name not in seen:
        _, upem, size_px = font_tuple
        glyphs  = shape_string(font_tuple, text)
        entries = glyphs_to_entries(glyphs, upem, size_px)
        index   = len(names)
        names.append(name)
        seen[name] = {
            'lineno':   lineno,
            'index':    index,
            'glyphs':   glyphs,
            'entries':  entries,
            'hb_str':   False,
            'mui_xyhb': [],
        }
        out.append(cluster_comment(glyphs) + "\n")
        out.append(format_array(name, entries) + "\n")
        if args.verbose:
            print(f"  {name}: \"{text}\", {len(glyphs)} glyph(s)", file=sys.stderr)
    return seen[name]


if __name__ == "__main__":
    main()
