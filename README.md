# ARM CAN TOOL — Overview

|[![English](doc/pictures/front_en_small.jpg)](doc/pictures/front_en_big.jpg)|[![Chinese](doc/pictures/front_zh_small.jpg)](doc/pictures/front_zh_big.jpg)|
|---|---|
|English|Chinese|


| Model | ARM CAN Tool |
|-|-|
| Hardware Version | V1.0 |
| Firmware Version | V1.3 |

- SWD/JTAG debug probe for ARM and RISC-V embedded systems
- Electrically isolated CAN 2.0A/B interface
- Debug output and CAN bus traffic merged into single USB serial log stream
- Standalone semihosting: target stdout, file I/O, and clock — no PC required
- ARM DWT program counter and exception trace
- Lua scripting for standalone use
- Automated logging to SD card
- User interface in English and simplified Chinese
- Documentation covers Linux host

---

## Quick Start

| Situation | Start here |
|-----------|------------|
| Board has no firmware | [INSTALL](doc/INSTALL.md) |
| Board has firmware, want to use it | [TUTORIAL](doc/TUTORIAL.md) |
| Want to reproduce the board | [MANUFACTURING](doc/MANUFACTURING.md) |
| Want to sell the board | [COMMERCIAL](doc/COMMERCIAL.md) |
| Want to build or modify the firmware | [DEVELOPER](doc/DEVELOPER.md) |

---

## Document Map

This documentation is published as a set of Markdown files, and as an epub.

- [INSTALL](doc/INSTALL.md) — Install the UF2 bootloader and application firmware. Begin here if the board came from PCB assembly with no firmware.
- [TUTORIAL](doc/TUTORIAL.md) — A first session: wire a target, flash firmware, read the console, capture CAN frames, and log to SD card. Begin here if the board already has firmware installed.
- [OPERATION](doc/OPERATION.md) — How the device interface works: operating modes, OLED menu, serial ports, target power, and startup services.
- [DEBUG](doc/DEBUG.md) — Debug workflows using Black Magic Debug or CMSIS-DAP, plus memwatch, RTT, SWO, and MTB.
- [TARGETS](doc/TARGETS.md) — Supported targets.
- [CANBUS](doc/CANBUS.md) — The CAN bus interface: isolation, protocols, hardware filters, and logging.
- [SCRIPT](doc/SCRIPT.md) — Lua scripting: event model, deployment workflows, and flash sector layout.
- [LOGGING](doc/LOGGING.md) — SD card logging: file formats, retrieval, and combined log streams.
- [HARDWARE](doc/HARDWARE.md) — Hardware design, connectors, isolation, power, EMI, and repair.
- [SCHEMATIC](doc/SCHEMATIC.md) — PCB schematic.
- [AI](doc/AI.md) — How to use AI-assisted PCB design review and repair.
- [REMOTE](doc/REMOTE.md) — Console port use.
- [DEVELOPER](doc/DEVELOPER.md) — Build the firmware from source, software architecture, and internals.
- [MANUFACTURING](doc/MANUFACTURING.md) — Order PCBs, source components, assemble the enclosure, and install firmware.
- [LICENSE](LICENSE.md) — License terms for original work (CC0) and third-party components.
- [COMMERCIAL](doc/COMMERCIAL.md) — License terms, estimated cost, supply chain notes, and reseller information.
- [REFERENCE](doc/REFERENCE.md) — Static facts: connector pinouts, electrical parameters, and PCB parameters.
- [LUA REFERENCE](doc/LUA_REF.md) — Lua function reference.

---

## Specifications

| Parameter | Value |
|-----------|-------|
| MCU | AT32F405, ARM Cortex-M4, 216 MHz |
| USB | USB 2.0 High Speed, 480 Mbit/s |
| Debug interfaces | SWD, JTAG |
| Target logic voltage range | 1.1 V – 3.6 V |
| Target power | 3.3 V, up to 100 mA |
| CAN interface | CAN 2.0A/B, up to 1,000,000 bit/s, electrically isolated |
| Storage | SD card, 16 MB SPI flash, 16 MB QSPI flash, EEPROM |
| Display | 1.5 inch OLED, 128×128, monochrome |
| Input | Multi-direction switch (up / down / press) |
| Firmware update | UF2 via USB mass storage |
| License | CC0 (Public Domain) |

---

## License

[CC0](https://creativecommons.org/publicdomain/zero/1.0/). Use, modify, and sell without restriction.

## Online Resources

| Resource | Location |
|----------|----------|
| Firmware source | [github arm_can_tool](https://github.com/koendv/arm_can_tool) |
| Bootloader source | [github at32f405-uf2boot](https://github.com/koendv/at32f405-uf2boot) |
| Hardware design | [oshwlab arm_can_tool](https://oshwlab.com/koendv/arm_can_tool) |
