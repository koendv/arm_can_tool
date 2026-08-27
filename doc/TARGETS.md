# ARM CAN TOOL — Targets

In CMSIS-DAP mode, target support depends on the software running on PC (OpenOCD, BMDA, or pyOCD).

In GDB server mode, target support depends upon the internal GDB server, Black Magic Debug.

This list is derived from Black Magic Debug firmware source.

To update this list after a firmware upgrade: See [Update Targets](DEVELOPER.md#update-targets).

To add a new target: See [New Target](DEVELOPER.md#adding-a-new-target).

---

## ARM Cortex-M

| Family / Series | Specific Targets |
|---|---|
| STMicroelectronics STM32F0 | STM32F03, F04/F070x6, F05/F030x8, F07, F09/F030xC |
| STMicroelectronics STM32F1 | STM32F1 low/medium/VL/XL density |
| STMicroelectronics STM32F2 | STM32F2 |
| STMicroelectronics STM32F3 | STM32F3 |
| STMicroelectronics STM32F4/F7 | STM32F40x, F42x, F47x, F446, F401C/E, F410, F411, F412, F413, F74x, F76x, F72x |
| STMicroelectronics STM32G0 | STM32G03/4, G05/6, G07/8, G0B/C |
| STMicroelectronics STM32H5 | STM32H5 |
| STMicroelectronics STM32H7 | STM32H7 |
| STMicroelectronics STM32L0/L1 | STM32L0, STM32L1 |
| STMicroelectronics STM32L4/L5/G4/U5 | STM32L41x, L43x, L45x, L47x, L49x, L4Rx, L55, G43, G47, G49, U535/545, U575/585, U59x/5Ax, U5Fx/5Gx |
| STMicroelectronics STM32WL/WB | STM32WLxx, STM32WBxx, WB35/55, WB1x |
| STMicroelectronics STM32WB0 | STM32WB0 |
| STMicroelectronics STM32MP15 (CM4 core) | STM32MP15 Cortex-M4 core |
| NXP LPC8xx | LPC802, LPC804, LPC81x, LPC82x, LPC832, LPC834, LPC844, LPC845, LPC8N04 |
| NXP LPC11xx / LPC13xx | LPC1112, LPC112x, LPC11U3x, LPC11U6x, LPC11xx, LPC11xx-XL, LPC1343 |
| NXP LPC15xx | LPC15xx |
| NXP LPC17xx | LPC17xx |
| NXP LPC40xx | LPC40xx |
| NXP LPC43xx | LPC43xx |
| NXP LPC546xx | LPC546xxJ256, LPC546xxJ512 |
| NXP LPC55xx | LPC5502/04/06/26/28, LPC55S04/06/26/28/69 |
| NXP LPC55 Debug Mailbox | LPC55 Debug Mailbox AP (recovery mode) |
| NXP i.MXRT | i.MXRT500, i.MXRT600, i.MXRT1011, i.MXRT1021, i.MXRT1052, i.MXRT1062, i.MXRT1176 |
| NXP / Freescale Kinetis K/KL | K22F, K64, KL02x8/16/32, KL03, KL16Z32/64/128/256, KL25, KL27x32/64/128, MK12DX128/256, MK12DN512, MK20DX256 |
| NXP / Freescale Kinetis KE | KE04Z8Vxxxx, KE04Z64Vxxxx, KE04Z128Vxxxx |
| NXP / Freescale Kinetis MDM-AP | Kinetis Recovery (MDM-AP) |
| NXP S32K1xx | S32K118, S32K14x |
| NXP S32K3xx | S32K344 |
| Nordic Semiconductor nRF51/52 | nRF51 series, nRF52 series |
| Nordic Semiconductor nRF52 Access Port | nRF52 Access Port, nRF52 Access Port (protected) |
| Nordic Semiconductor nRF54L | nRF54L series |
| Nordic Semiconductor nRF54L Access Port | nRF54L Access Port, nRF54L Access Port (protected) |
| Nordic Semiconductor nRF91 | nRF9160 |
| Raspberry Pi RP2040 | RP2040 |
| Raspberry Pi RP2040 Rescue | RP2040 Rescue AP (attach to reset) |
| Raspberry Pi RP2350 (Cortex-M33) | RP2350 Cortex-M33 core |
| Microchip / Atmel SAM3x | SAM3X, SAM3N/S, SAM3U, SAM4S |
| Microchip / Atmel SAM4L | SAM4L |
| Microchip / Atmel SAMD/C/L | SAMD21, SAMC21, SAML21, SAML22 |
| Microchip / Atmel SAMD5x/E5x | SAM D5x / E5x series |
| Microchip / Atmel SAMX7X | SAM S70, E70, V70, V71 (Cortex-M7) |
| Silicon Labs EFM32 | Gecko, Giant Gecko (11/12), Tiny Gecko (11), Leopard Gecko, Wonder Gecko, Zero Gecko, Happy Gecko, Pearl Gecko (12), Jade Gecko (12) |
| Silicon Labs EFR32 | EFR32BG/FG/MG 1x/12x/13x/14x (B/P/V variants) |
| Silicon Labs EZR32 | EZR32LG, EZR32WG, EZR32HG |
| Silicon Labs EFM32 AAP | EFM32 Authentication Access Port (protected devices) |
| Texas Instruments Stellaris / Tiva-C | LM3S (Stellaris), TM4C (Tiva-C) |
| Texas Instruments MSP432E4 | MSP432E4 |
| Texas Instruments MSP432P4 | MSP432P401R (256 KB / 64 KB), MSP432P401M (128 KB / 32 KB) |
| Texas Instruments MSPM0 | MSPM0 series |
| Renesas RA | RA2A1, RA2E1/E2, RA2L1, RA4E1/E2, RA4M1/M2/M3, RA4W1, RA6E1/E2, RA6M1–M5, RA6T1/T2 |
| GigaDevice GD32F1/F3/E2 | GD32E230, GD32F1, GD32F2, GD32F3, GD32E5 |
| GigaDevice GD32F4 | GD32F405, GD32F450, GD32F470 |
| ArteryTek AT32F40x | AT32F403, AT32F403A/407, AT32F405, AT32F413, AT32F415, AT32F421, AT32F423, AT32F425 |
| ArteryTek AT32F43x | AT32F435, AT32F437 |
| STMicroelectronics STM32C5 | STM32C53x, STM32C55x, STM32C59x |
| MindMotion MM32 (Star-MC1) | MM32F327, MM32F52 |
| MindMotion MM32 (Cortex-M3) | MM32SPIN05, MM32SPIN27 |
| MindMotion MM32L (Cortex-M0) | MM32L07x |
| WCH CH32F1 | CH32F1 medium density |
| WCH CH579 | CH579 (Cortex-M0 BLE SoC) |
| Ambiq Micro Apollo 3 | Apollo 3 Blue |
| HDSC HC32 | HC32L110 |
| Puya Semiconductor PY32 | PY32Fxxx |

---

## ARM Cortex-A/R

| Family / Series | Specific Targets |
|---|---|
| STMicroelectronics STM32MP15 (CA7 core) | STM32MP15 Cortex-A7 core |
| Renesas RZ | RZ/A1L, RZ/A1LC, RZ/A1 (covers A1LU / A1H) |
| AMD / Xilinx Zynq-7000 | Zynq-7020 (Cortex-A9 cores) |
| Texas Instruments Sitara AM335x | AM335x (Cortex-A8) — groundwork only, no flash/debug support |
| Generic ARMv8-A | ARMv8-A core (e.g. Cortex-A53/A55/A72-class) |

---

## RISC-V

| Family / Series | Specific Targets |
|---|---|
| GigaDevice GD32VF | GD32VF103 |
| GigaDevice/Nuclei GD32VW5 | GD32VW5 (Wi-Fi RISC-V SoC) |
| WCH CH32V003 | CH32V003 |
| WCH CH32V2/V3 | CH32V203, CH32V208, CH32V303, CH32V305, CH32V307 |
| Raspberry Pi RP2350 (Hazard3) | RP2350 RISC-V core |
| Generic RISC-V 32-bit | Any RISC-V debug v0.13 target via JTAG DTM or ADI DTM |
| Generic RISC-V 64-bit | Scaffolding only |

---

## JTAG Infrastructure

| Family / Series | Specific Targets |
|---|---|
| ARM ADIv5 JTAG-DP | Generic ARM ADIv5 JTAG debug port |
| Xilinx FPGA | XCVU440 and broad Xilinx FPGA family — JTAG chain traversal only, no debug |
| Texas Instruments ICEPICK | JTAG router — enables TAPs behind an ICEPICK-C/D scan-chain controller |
| Lattice ECP5 FPGA | SRAM/eFuse configuration programming over JTAG |
