# canfilter

**canfilter** is an RT-Thread MSH shell command for managing the bxCAN hardware acceptance filter. canfilter accepts human-readable CAN bus IDs and ID ranges and programs them into the canbus hardware filter configuration.

## Usage

```
canfilter [OPTIONS] [IDs/RANGES]
```

IDs: Single CAN IDs (0x100, 256)

RANGES: CAN ID ranges (0x100-0x1FF, 256-511)

| Option | Long form     | Description                                           |
| ------ | ------------- | ----------------------------------------------------- |
| -a     | --allow-all   | Allow all packets                                     |
| -d     | --dry-run     | Print filter configuration without programming hardware |
| -v     | --verbose     | Enable verbose output (repeat up to three times)      |
| -h     | --help        | Show help                                             |

- Single IDs are interpreted as standard if <= 0x7FF, extended if <= 0x1FFFFFFF.
- Hex numbers are supported (prefix `0x`).
- Ranges are interpreted as extended if either bound is an extended ID.
- Repeating `-v` up to three times increases verbosity:
  - `-v` prints filter usage and programming result.
  - `-v -v` also prints a human-readable decode of each filter bank.
  - `-v -v -v` also dumps the raw hardware registers.

The filter takes effect on the next CAN bus start. To make the filter persistent across reboots, store settings using the menu or the rt-thread `settings` command.

## Examples

Allow a single standard ID:

```
canfilter 0x100
```

Program a standard ID range:

```
canfilter 0x100-0x1FF
```

Program mixed standard and extended IDs:

```
canfilter 0x100 0x200-0x2FF 0x1000 -v
```

Print filter configuration without programming hardware:

```
canfilter -d -v -v -v 0x101-0x1fe
```

Allow all traffic:

```
canfilter -a
```

## Source files

| File                  | Description                                              |
| --------------------- | -------------------------------------------------------- |
| `canfilter_cmd.c`     | MSH command, argument parsing, calls `can_set_filter()`  |
| `canfilter_bxcan_f0.c` | bxCAN filter builder: CIDR decomposition, bank packing  |
| `canfilter_bxcan_f0.h` | Public API for the filter builder                       |
| `canfilter.h`         | Hardware filter struct definitions (`can_filter_t` etc.) |

## bxCAN filtering

bxCAN uses mask-based filtering. It has no native range support, so ranges are decomposed into a minimal set of (ID, mask) pairs using a CIDR aggregation algorithm — the same technique used for IP network prefix compression.

Each hardware filter bank holds either four standard IDs (16-bit list mode), two standard (ID, mask) pairs (16-bit mask mode), two extended IDs (32-bit list mode), or one extended (ID, mask) pair (32-bit mask mode). The builder packs entries as efficiently as possible.

The STM32F0/F1/F3 bxCAN peripheral has 14 filter banks. Filter usage is printed after every successful `canfilter` invocation. CIDR-aligned ranges use fewer banks:

```
canfilter -d 0x100-0x1FF
Filter usage: 1/14 (7%)

canfilter -d 0x101-0x1FE
Filter usage: 7/14 (50%)
```

The second range uses more banks because its boundaries are not aligned to a power-of-two block. If you run out of filter banks, consider adjusting your ranges to start and end on power-of-two boundaries.

## Notes

The core idea behind **canfilter** is that CAN bus hardware filters (ID + mask) are mathematically equivalent to IP network blocks (network + prefix). Because of this equivalence, CIDR aggregation — a mature algorithm for converting IP addresses and ranges into minimal sets of network blocks — can be applied directly to CAN identifiers.

The masks generated follow the same structure as IP network masks: a contiguous run of 1-bits from the most significant bit marking the portion of the CAN ID that must match, followed by 0-bits representing don't-care positions. A mask of all zeroes matches any CAN ID.


SEE ALSO

[canfilter](https://github.com/koendv/canfilter)

## License

Public domain — [CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/).
