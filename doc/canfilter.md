# CANFILTER(1) - CAN Bus Filter Tool

## NAME

**canfilter** - generate and verify optimal CAN bus hardware filters

## SYNOPSIS

**canfilter** [*OPTIONS*] *RANGE*...

**canfilter** [*OPTIONS*] **--test** *TEST_ID*... *RANGE*...

**canfilter** **--output embedded** [*OPTIONS*] *RANGE*... *(RT-Thread only)*

## DESCRIPTION

**canfilter** is a sophisticated tool for generating optimal hardware filters for CAN bus controllers. It takes human-readable ID ranges and converts them into efficient hardware filter configurations using CIDR route aggregation algorithms with subset elimination.

The tool supports both standard (11-bit) and extended (29-bit) CAN identifiers and generates filters in multiple output formats including STM32-compatible register values, SLCAN protocol commands, HAL library code, and direct hardware configuration.

## ALGORITHM

**canfilter** uses CIDR (Classless Inter-Domain Routing) route aggregation algorithms adapted for CAN bus filters:

- **Single CAN IDs** treated as host routes (/32 equivalent)
- **CAN ID ranges** decomposed into optimal CIDR blocks  
- **CIDR aggregation** merges adjacent blocks to minimize filter count
- **Subset elimination** removes any filter completely covered by a broader filter
- **Hardware optimization** selects mask vs list mode based on efficiency

### Subset Elimination

The algorithm automatically eliminates redundant filters:
- Single IDs covered by ranges (e.g., `0x100` + `0x100-0x1FF` → single filter)
- Smaller ranges covered by larger ranges (e.g., `0x100-0x10F` + `0x100-0x1FF` → single filter)
- Any filter completely covered by a broader filter with the same mode and frame type

## OPTIONS

### Filter Definition Options

**--std**  
Process following ranges as standard 11-bit IDs

**--ext**  
Process following ranges as extended 29-bit IDs

**--data**  
Filter data frames only (default)

**--rtr**  
Filter remote frames only

### Output Control Options

**--output** *FORMAT*  
Output format: stm, slcan, hal, or embedded

**--mask**  
Force mask mode for all filters (debugging)

**--list**  
Enable list mode optimization where efficient (default)

**--max** *N*  
Maximum number of filters to generate (default: platform dependent)

**--verbose**  
Verbose output showing algorithm details

### Testing and Verification Options

**--test** *ID*...  
Test specific IDs against generated filters

**--selftest**  
Run built-in self-test

### Information Options

**-h**, **--help**  
Show this help message

### Option Abbreviations

Options can be abbreviated to the shortest non-ambiguous prefix.  
For example: --stm, --std, --emb, --ext are all valid.  
Avoid: --s (ambiguous), --e (ambiguous), --st (ambiguous).

## RANGE SPECIFICATION

Ranges can be specified as single IDs or ID ranges:

**Single ID**  
`0x100`  
Filters exactly one CAN ID

**ID Range**  
`0x100-0x10F`  
Filters all IDs from 0x100 to 0x10F inclusive

**Multiple Ranges**  
`0x100 0x200-0x20F 0x300`  
Filters multiple individual IDs and ranges

*Note: Overlapping ranges are automatically optimized through subset elimination*

## OUTPUT FORMATS

### STM32 Format

Outputs filters in STM32 hardware register format:

```
ID=0x100 MASK=0x7F0 IDE=0 RTR=0 MODE=16BIT SCALE=32BIT FILTER=MASK
```

Where:

**ID**  
Base identifier value

**MASK**  
Filter mask (1=must match, 0=don't care)

**IDE**  
0=standard ID, 1=extended ID

**RTR**  
0=data frame, 1=remote frame

**MODE**  
16BIT (standard) or 32BIT (extended)

**SCALE**  
16BIT or 32BIT hardware scale

**FILTER**  
MASK or LIST mode

### SLCAN Format

Outputs enhanced SLCAN protocol commands with complete hardware programming:

#### Control Commands

```
FA # Pass all frames (disable filtering)
FB # Block all frames (enable filtering, allow nothing)
F0 # Begin filter configuration (synchronization)
F1 # End filter configuration and apply
```

#### Single Complete Filter Programming Command

```
F<bank><fr1><fr2><mode><scale><ide><rtr>
```

**Command Fields (21 characters total):**

| Field | Size | Description |
|-------|------|-------------|
| **F** | 1 char | Filter command identifier |
| **bank** | 2 hex digits | Filter bank number (00-27) |
| **fr1** | 8 hex digits | FR1 hardware register value |
| **fr2** | 8 hex digits | FR2 hardware register value |
| **mode** | 1 hex digit | `0`=MASK mode, `1`=LIST mode |
| **scale** | 1 hex digit | `0`=16BIT scale, `1`=32BIT scale |
| **ide** | 1 hex digit | `0`=standard ID, `1`=extended ID |
| **rtr** | 1 hex digit | `0`=data frame, `1`=remote frame |

**Hardware Optimization:**
- **Standard ID List**: 4 filters per bank
- **Extended ID List**: 2 filters per bank  
- **Mask Mode**: 1-2 filters per bank

**Protocol Flow:**
```
F0 # Begin configuration
F000000001000000007FF01000 # Bank 0: complete filter setup
F010000002000000007F0001000 # Bank 1: complete filter setup
F1 # Apply to hardware
```

**Target Implementation:**
```
/* Simple target device implementation */
void handle_filter_command(const char* cmd) {
    uint8_t bank, mode, scale, ide, rtr;
    uint32_t fr1, fr2;
    
    sscanf(&cmd[1], "%2hhx%8lx%8lx%1hhx%1hhx%1hhx%1hhx", 
           &bank, &fr1, &fr2, &mode, &scale, &ide, &rtr);
    
    /* Direct hardware programming */
    CAN1->sFilterRegister[bank].FR1 = fr1;
    CAN1->sFilterRegister[bank].FR2 = fr2;
    
    /* Set mode and scale bits */
    if (mode == 1) CAN1->FM1R |= (1 << bank);
    else CAN1->FM1R &= ~(1 << bank);
    
    if (scale == 1) CAN1->FS1R |= (1 << bank);
    else CAN1->FS1R &= ~(1 << bank);
}
```

### HAL Format

Outputs STM32 HAL library configuration code for direct integration.

### Embedded Format

Directly applies filters to CAN hardware (RT-Thread only).

## TESTING AND VERIFICATION

**canfilter** includes comprehensive testing capabilities:

- **Built-in self-test** (`--selftest`) verifies algorithm correctness including subset elimination
- **User testing** (`--test`) validates specific IDs against generated filters  
- **Test results** show pass/fail counts: X/Y passed
- **Automatic boundary condition testing**

## EXAMPLES

**Basic single ID filter**
```
canfilter 0x100
```

**Range with optimal masking**
```
canfilter 0x100-0x1FF
```

**Subset elimination example** - single ID covered by range
```
canfilter 0x100 0x100-0x1FF
# Output: Single optimal filter covering both
```

**Nested range elimination** - smaller range covered by larger
```
canfilter 0x100-0x10F 0x100-0x1FF  
# Output: Single optimal filter covering both
```

**Extended ID filtering**
```
canfilter --ext 0x1ABCDE 0x1ABCDF-0x1ABCE0
```

**Apply filters directly to hardware (RT-Thread)**
```
canfilter --output embedded --std 0x100-0x10F
```

**Realistic automotive filter for powertrain and instrumentation**
```
canfilter --std 0x100-0x1FF 0x300-0x3FF
```

**Generate HAL library code**
```
canfilter --output hal --std 0x100-0x10F
```

**Generate SLCAN commands for hardware programming**
```
canfilter --output slcan --std 0x100 0x200-0x20F
```

## DEVELOPER INFORMATION

This section contains technical details for developers and integrators.

### Algorithm Implementation

Uses CIDR (Classless Inter-Domain Routing) route aggregation algorithms adapted for CAN bus filters:

- **Single CAN IDs** treated as host routes (/32 equivalent)
- **CAN ID ranges** decomposed into optimal CIDR blocks
- **CIDR aggregation** minimizes filter count
- **Subset elimination** removes any filter completely covered by a broader filter
- **Hardware optimization** selects mask vs list mode

### Hardware Optimization

**canfilter** optimizes filter implementation based on hardware capabilities:

- **List mode** for host routes (exact matches)
- **Mask mode** for CIDR blocks (range coverage)
- **Automatic bank assignment** and packing
- **Hardware-aware filter grouping** by (IDE × RTR) combinations

### SLCAN Protocol Enhancement

The enhanced SLCAN protocol provides:

- **Single Command**: Complete hardware programming in 21 characters
- **No Calculations**: Target device just copies values to registers  
- **Transaction Safety**: F0/F1 prevent partial configurations
- **Quick Control**: FA/FB for instant enable/disable
- **Hardware Direct**: 1:1 mapping to hardware registers

### Platform Specifications

**Linux/Windows**
Filter design and verification tool

**RT-Thread**
Embedded deployment with hardware integration

### Embedded System Requirements

On RT-Thread systems:

- Static memory allocation only
- Conservative limits: 14 filters, 16 test IDs, 8 ranges
- Available as shell command: canfilter
- Direct hardware filter application
- Recommended stack size: 2.0 kB (enhanced SLCAN output)

## BUG REPORTING PROCEDURE

If you find a bug:

1. Reproduce with `--verbose` and `--selftest`
2. Test with different output formats
3. Use AI tools for initial analysis
4. Open GitHub issue with complete information

## AUTHOR

Koen De Vleeschauwer

## COPYRIGHT

This project (including all software, hardware designs, documentation, and other creative content) is dedicated to the public domain under the Creative Commons Zero ([CC0](https://creativecommons.org/publicdomain/zero/1.0/)) 1.0 Universal Public Domain Dedication.

### What this means

- Use this project for **any purpose**, personal or commercial
- Modify, adapt, and build upon the work
- Distribute your modifications **without any requirements** to:
  - Provide attribution
  - Share your changes
  - Use the same license
- No permission is needed - **just use it!**

### No Warranty

This project is provided **"as is"** without any warranties. The authors accept no liability for any damages resulting from its use.
