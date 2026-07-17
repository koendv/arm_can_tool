#!/usr/bin/env python3
#
# Requires linux can-utils built from source.
# Usage:
#   ~/src/can-utils/can-calc-bit-timing --alg=v6.3 -c 108000000 bxcan | python3 ./tools/canbus/canbus_bitrates.py
#
# The bxCAN BTS1 register field encodes the full TSEG1 = PrS + PhS1.
# can-calc-bit-timing splits TSEG1 into PrS and PhS1 columns; we must sum them.
#
# | can-calc column | Meaning                    | Hardware register concept          | Struct field   |
# | --------------- | -------------------------- | ---------------------------------- | -------------- |
# | BRP             | Baud Rate Prescaler        | Divides peripheral clock           | baudrate_div   |
# | SJW             | Synchronization Jump Width | Resynchronization correction limit | rsaw_size      |
# | PrS + PhS1      | TSEG1 (full)               | Timing segment before sample point | bts1_size      |
# | PhS2            | Phase Segment 2            | Timing segment after sample point  | bts2_size      |

import sys

def die(msg, line=None):
    if line is not None:
        print(f"error: {msg}: {line}", file=sys.stderr)
    else:
        print(f"error: {msg}", file=sys.stderr)
    sys.exit(1)

def enum_rsaw(v): return f"CAN_RSAW_{v}TQ"
def enum_bts1(v): return f"CAN_BTS1_{v}TQ"
def enum_bts2(v): return f"CAN_BTS2_{v}TQ"

lines = [l.rstrip("\n") for l in sys.stdin]
if not lines:
    die("no input received")

entries = []
parsing_started = False

for lineno, line in enumerate(lines, 1):
    stripped = line.strip()

    if not stripped:
        continue

    cols = stripped.split()

    if not cols[0].isdigit():
        if parsing_started:
            die("unexpected non-data line after table started", line)
        continue

    parsing_started = True

    if len(cols) < 13:
        die("unexpected column count", line)

    try:
        bitrate_nom = int(cols[0])
        prs         = int(cols[2])   # propagation segment
        phs1        = int(cols[3])   # phase segment 1
        phs2        = int(cols[4])   # phase segment 2
        sjw         = int(cols[5])
        brp         = int(cols[6])
        bitrate_real= int(cols[7])
        sample_real = cols[10]
    except ValueError:
        die("invalid numeric field", line)

    bts1 = prs + phs1   # full TSEG1 for bxCAN BTS1 field

    # strict validation (bxCAN limits)
    if not (1 <= sjw <= 4):
        die("SJW out of range", line)
    if not (1 <= bts1 <= 16):
        die("BTS1 (PrS+PhS1) out of range", line)
    if not (1 <= phs2 <= 8):
        die("PhS2 out of range", line)
    if not (1 <= brp <= 1024):
        die("BRP out of range", line)

    entries.append((bitrate_nom, brp, sjw, bts1, phs2, bitrate_real, sample_real))

if not entries:
    die("no timing rows detected")

entries.sort(key=lambda e: e[0])

print("static const can_bitrate_config_t bitrate_configs[] = {")
for bitrate, brp, sjw, bts1, phs2, real, samp in entries:
    print(
        f"{{.bitrate={bitrate},"
        f".config={{.baudrate_div={brp},"
        f".rsaw_size={enum_rsaw(sjw)},"
        f".bts1_size={enum_bts1(bts1)},"
        f".bts2_size={enum_bts2(phs2)}}}}},"
        f" /* real={real}Hz sample={samp} */"
    )
print("};")
