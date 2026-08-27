# ARM CAN TOOL — License

## Original Work

The hardware design, original firmware code, and documentation authored by koendv are dedicated to the public domain under
[Creative Commons Zero (CC0) 1.0](https://creativecommons.org/publicdomain/zero/1.0/).

- No license fees.
- No attribution required.
- No permission needed.
- May be used, modified, manufactured, and sold without restriction.

This work is provided without warranty of any kind. To the extent permitted by law, the authors accept no liability for damages resulting from its use.

## Third-Party Components

This project incorporates third-party software packages. Each package retains its own license. The table below lists each component, its license, and the obligations it places on distributors.

| Component | License | Distributor Obligation |
| --- | --- | --- |
| [Black Magic Debug](https://codeberg.org/blackmagic-debug/blackmagic) | GPL v3 | Source code must be available to recipients |
| [RT-Thread](https://github.com/RT-Thread/rt-thread) | Apache 2.0 | Preserve copyright and license notices |
| [CMSIS-Core](https://github.com/ARM-software/CMSIS_6) | Apache 2.0 | Preserve copyright and license notices |
| [Lua](https://www.lua.org/) | MIT | Preserve copyright notice |
| [free-DAP](https://github.com/ataradov/free-dap) | BSD 2-Clause | Preserve copyright notice |
| [MicroRL-remaster](https://github.com/dimmykar/microrl-remaster) | Apache 2.0 | Preserve copyright and license notices |
| [CherryUSB](https://github.com/cherry-embedded/CherryUSB) | Apache 2.0 | Preserve copyright and license notices |
| [u8g2](https://github.com/olikraus/u8g2) | BSD 2-Clause | Preserve copyright notice |
| [LittleFS](https://github.com/littlefs-project/littlefs) | BSD 3-Clause | Preserve copyright notice |
| [AT32F405 HAL Driver](https://github.com/ArteryTek/AT32F402_405_Firmware_Library) | BSD 3-Clause (ArteryTek) | Preserve copyright notice; do not use ArteryTek name for endorsement |
| [AT32F405 CMSIS Driver](https://github.com/ArteryTek/AT32F402_405_Firmware_Library) | BSD 3-Clause (ArteryTek) | Preserve copyright notice; do not use ArteryTek name for endorsement |
| [CmBacktrace](https://github.com/armink/CmBacktrace) | MIT | Preserve copyright notice |
| [rt_kprintf_threadsafe](https://github.com/mysterywolf/rt-thread-kprintf-threadsafe) | MIT | Preserve copyright notice |
| [Bootstrap loader (at32f405-uf2boot)](https://github.com/koendv/at32f405-uf2boot) | MIT | Preserve copyright notice |

License texts for each component are found in the corresponding subdirectory under `packages/` or `applications/`.

## Difference Between Bootloader and Firmware

The bootloader is used to install the firmware.
Bootloader and firmware carry different licenses.

**Bootloader**: Permissive license. Preload.

- MIT Permissive license
- Preloading bootloader doubles as a production test - tests power supply, USB, MCU.
- With the bootloader preloaded, installing firmware is a simple USB mass-storage file copy. No programmer, no host tools, no special drivers.

**Firmware**: Non-permissive license. Do not preload it.

- Includes GPLv3 components, non-permissive license.
- Shipping with firmware preloaded triggers license obligation: offer source code to every recipient.
- Ship hardware with bootloader preloaded, firmware not preloaded.
- The customer downloads firmware from the project repository and installs firmware using file copy.
