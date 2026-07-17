# ARM CAN TOOL — Reference

![ARM CAN TOOL](pictures/buttons_none.svg)


Hardware revision: v1.0

All connectors: JST GH 1.25 mm pitch, 6-pin.

---

## ARM Pinout

| Pin | SWD Signal | JTAG Signal | Direction | Description |
|-----|------------|-------------|-----------|-------------|
| 1 | VIO | VIO | Input | Target logic supply voltage. Connect to target VCC. |
| 2 | TXD0 | TDI | Output | serial0 TXD (SWD) / JTAG TDI (JTAG) |
| 3 | RXD0 | TDO | Input | serial0 RXD (SWD) / JTAG TDO (JTAG) |
| 4 | SWDIO | TMS | Bidirectional | SWD data / JTAG TMS |
| 5 | SWCLK | TCK | Output | SWD clock / JTAG TCK |
| 6 | GND | GND | — | Ground |

In SWD mode: pins 2 and 3 carry serial0 (target console).

In JTAG mode: pins 2 and 3 carry TDI and TDO.

⚠️ **Warning — serial0 in JTAG mode:** In JTAG mode, pins 2 and 3 carry TDI and TDO. Do not enable serial0 in JTAG mode. Disable serial0 before switching to JTAG mode: `Serial → Serial Enable → serial0 → Off`.

In JTAG mode connect target console via AUX connector serial1.

---

## AUX Pinout

| Pin | Signal | Direction | Description |
|-----|--------|-----------|-------------|
| 1 | VIO | Input | Target logic supply voltage |
| 2 | TXD1 | Output | serial1 TXD |
| 3 | RXD1 | Input | serial1 RXD |
| 4 | RXD2 | Input | serial2 / SWO (receive only) |
| 5 | NRST | Output | Target reset (active low) |
| 6 | GND | — | Ground |

---

## Probe SWD Pinout

Probe self-debug interface. Console: 115200 baud, 8N1.

| Pin | Signal | Direction | Description |
|-----|--------|-----------|-------------|
| 1 | 3.3V | Output | Probe 3.3V supply |
| 2 | CONSOLE TXD | Output | rt-thread console transmit |
| 3 | CONSOLE RXD | Input | rt-thread console receive |
| 4 | SWDIO | Bidirectional | Probe SWD data |
| 5 | SWCLK | Input | Probe SWD clock |
| 6 | GND | — | Ground |

---

## CAN Bus Pinout

Isolated. CAN_HIGH and CAN_LOW duplicated on two pin pairs for daisy-chain wiring.

| Pin | Signal | Description |
|-----|--------|-------------|
| 1 | ISOLATED GND | CAN bus ground (isolated from probe ground) |
| 2 | CAN_HIGH | CAN bus HIGH |
| 3 | CAN_LOW | CAN bus LOW |
| 4 | CAN_HIGH | CAN bus HIGH (second connection point) |
| 5 | CAN_LOW | CAN bus LOW (second connection point) |
| 6 | ISOLATED GND | CAN bus ground (isolated from probe ground) |

No internal terminating resistors. If probe is last device on bus: connect 120 Ω across CAN_HIGH and CAN_LOW.

---

## I2C Pinout

Pins 1–4 follow QWIIC connector pinout.

| Pin | Signal | MCU Pin | Description |
|-----|--------|---------|-------------|
| 1 | GND | — | Ground |
| 2 | 3.3V | — | 3.3V supply |
| 3 | I2C1_SDA | PB7 | I2C bus 1 data (internal bus — RTC and EEPROM) |
| 4 | I2C1_SCL | PB6 | I2C bus 1 clock (internal bus — RTC and EEPROM) |
| 5 | I2C3_SDA / TRIG | PC1 | I2C bus 3 data / external trigger input |
| 6 | I2C3_SCL | PC0 | I2C bus 3 clock |

I2C1 carries internal RTC (address 0x68) and EEPROM (address 0x50). Use I2C3 for external devices to avoid address conflicts.

Pin 5 doubles as external trigger input when trigger is enabled in Startup menu.

⚠️ **Note — I2C3 unavailable when trigger active:** External trigger uses PC1. I2C3 is not available when trigger is enabled.


---

## Electrical Parameters

| Parameter | Value |
|-----------|-------|
| Nominal input voltage | 5.0 V |
| Operating range | 4.5 V – 5.5 V |
| Absolute maximum | 6.0 V |
| Target voltage range (VIO) | 1.1 V – 3.6 V |
| Target power supply capacity | approximately 100 mA |
| CAN bus fault protection | ±58 V |
| CAN max data rate | 1 Mbit/s |

### Power Consumption

| Condition | Current |
|-----------|---------|
| No SD card, display blanked | 60 – 65 mA |
| No SD card, display showing text | 80 – 85 mA |
| Writing to SD card, display 100% white | 200 mA |

---

## DFU Log

On the system console:

```
102k ram
dfu
[SFUD] Found a Winbond flash chip. Size is 16777216 bytes.
[SFUD] qspi1 flash device initialized successfully.
........
```

One dot is printed to the console for every 4kbyte of firmware written to flash.

## Boot Log

Boot log after `Settings->Reset` and `Settings->Store`.
On the system console:

```
102k ram
app

 \ | /
- RT -     Thread Operating System
 / | \     5.3.0 build Jul 20 2026 09:39:27
 2006 - 2024 Copyright by RT-Thread team
[u8g2] Attach device to spi22
I/SWD: ramfunc 3700 byte
I/EEPROM: settings 404 byte
I/SFUD: rom mount to '/'
I/SFUD: Found a flash chip. Size is 16777216 bytes.
I/SFUD: norflash0 flash device initialized successfully.
I/SFUD: Probe SPI flash norflash0 by SPI device spi20 success.
I/SFUD: spi flash mount to /flash
I/SD: sd card mount to /sdcard
I/USB: init
I/LED: init
I/MAIN: ready
I/CAN: speed 500000
I/CAN: event init
I/CAN: init
I/GSUSB: init
I/GSUSB: waiting
I/UART: uart2 speed 115200
I/UART: uart3 speed 115200
I/UART: uart7 speed 1000000
msh />
```

The `msh />` prompt may appear at any point during the boot.

## PCB Parameters

Parameters for manufacturing 5 PCBs at JLCPCB.

| Parameter | Value | Parameter | Value |
|-----------|-------|-----------|-------|
| Gerber file | Gerber_PCB1_2026-04-18_Y86 | Build Time | 3 days (PCBA Only) |
| Base Material | FR-4 | Layers | 4 |
| Dimension | 60 mm × 100 mm | PCB Qty | 5 |
| Product Type | Industrial/Consumer electronics | Different Design | 1 |
| Delivery Format | Single PCB | PCB Thickness | 1.6 mm |
| Specify Stackup | Yes — JLC04161H-7628 | Impedance Control | No requirement |
| Layer Sequence | — | PCB Color | Green |
| Silkscreen | White | Material Type | TG135 |
| Via Covering | Plugged | Surface Finish | ENIG Gold Thickness: 1U” |
| Deburring/Edge Rounding | No | Outer Copper Weight | 1 oz |
| Inner Copper Weight | 0.5 oz | Gold Fingers | No |
| Electrical Test | Flying Probe Fully Test | Castellated Holes | No |
| Press-Fit Hole | No | Edge Plating | No |
| Mark on PCB | Order Number (Specify Position) | Blind Slot | No |
| Min Via Hole Size/Diameter | 0.3 mm / (0.4/0.45 mm) | Via Plating Method | Not Specified |
| 4-Wire Kelvin Test | No | Paper between PCBs | No |
| Appearance Quality | IPC Class 2 Standard | Confirm Production File | No |
| Silkscreen Technology | Ink-jet/Screen Printing | Silkscreen Package Box | With JLCPCB logo |
| Inspection Report | No | Board Outline Tolerance | ±0.2 mm (Regular) |
| UL Marking | No | Countersink Hole | No |
| Backdrill | No | | |

---

## PCB Assembly Parameters

Parameters for assembling 5 PCBs at JLCPCB.


| Parameter | Value |
|-----------|-------|
| PCBA Type | Economic |
| Assembly Side | Top Side |
| PCBA Qty | 5 |
| Tooling Holes | Added by JLCPCB |
| Confirm Parts Placement | No |
| Bake Components | No |
| Photo Confirmation | No |
| Board Cleaning | No |
| Conformal Coating (cleaning included) | No |
| Special Stencil | No |
| Packaging | Antistatic bubble film |
| Depanel Boards & Edge Rail Before Delivery | No |
| Solder Paste | High temp. |
| Flying Probe Test | No |
| Nitrogen Reflow Soldering | Yes (for Economic) |
| Function Test | No |
| PCBA Remark | No |
| File Provided As | Complete File, just proceed with my own files |
| Panel Format | 1 × 1 |
| Build Time | 3 – 4 days |
| Stencil Storage | No |
| Fixture Storage | No |

⚠️ **Warning — battery holder solder paste:**

**REMARK:** NO SOLDER PASTE ON MIDDLE PAD OF BATTERY HOLDER C964818
