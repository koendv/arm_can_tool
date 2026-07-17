# Firmware for ARM CAN Tool V1.0

## Governing Principle

The AT32F405 processor uses two separate memory regions for firmware. The internal flash (256 kB) contains the UF2 bootloader, installed once via DFU. The QSPI flash (16 MB) contains the application firmware, updated by copying a UF2 file to a USB mass storage device.

## Prerequisites

- `dfu-util` installed (`sudo apt install dfu-util` on Debian-based systems)
- USB cable (data-capable)
- Ability to identify and press [RESET] and [BOOT0] buttons on the board

## File Overview

| File | Purpose | Usage Frequency |
|------|---------|-----------------|
| `bootloader/cherryuf2_arm_can_tool.bin` | UF2 bootloader | Once (first-time setup) |
| `application/rtthread.uf2` | Application firmware | Every firmware update |

---

## Installing the UF2 Bootloader (One Time Only)

**Governing Principle:**
The AT32F405 contains a factory DFU bootloader accessible by holding [BOOT0] during reset. The UF2 bootloader is written to internal flash using `dfu-util`.

**Prerequisites:**
- `dfu-util` installed
- Board connected via USB

**Procedure:**
1. Press and hold [RESET] and [BOOT0] simultaneously.
2. Release [RESET] after one second.
3. Release [BOOT0] after one further second.

The board is now in DFU mode.

**Verification:**
```bash
$ lsusb | grep Artery
Bus 003 Device 012: ID 2e3c:df11 Artery-Tech DFU in FS Mode
```

**Procedure (continued):**
4. Write the bootloader binary:
```bash
dfu-util -a 0 -d 2e3c:df11 --dfuse-address 0x08000000 \
         -D bootloader/cherryuf2_arm_can_tool.bin
```

**Expected output:**
```
Erase       [=========================] 100%        23692 bytes
Erase    done.
Download    [=========================] 100%        23692 bytes
Download done.
File downloaded successfully
```

**Verification:** The bootloader requires installation only once. Normal boot (without [BOOT0] held) starts the application.

⚠️ **Warning: Linux USB Access**

**If ignored:** `dfu-util` reports "No DFU capable USB device found"

**Correct practice:** Add udev rule:
```bash
echo 'SUBSYSTEMS=="usb", ATTRS{idVendor}=="2e3c", ATTRS{idProduct}=="df11", TAG+="uaccess", MODE="0664", GROUP="plugdev"' | sudo tee /etc/udev/rules.d/99-at32.rules
sudo udevadm control -R
```

**Complete precautions:** See [HARDWARE.md#usb-dfu-setup]

---

## Installing the Application Firmware (Routine)

**Governing Principle:**
The UF2 bootloader presents the QSPI flash as a USB mass storage device. The application firmware installs by copying a UF2 file to this device.

**Prerequisites:**
- UF2 bootloader installed (see previous section)
- Board connected via USB

**Procedure:**
1. Press and hold [RESET] and the multi-direction switch simultaneously.
2. Release [RESET] after one second.
3. Release the multi-direction switch after one further second.

The green LED illuminates. A USB mass storage device named `CherryUF2` appears.

**Verification:**
```bash
$ ls /media/$USER/CherryUF2/
CURRENT.UF2
```

**Procedure (continued):**
4. Copy the application firmware:
```bash
cp application/rtthread.uf2 /media/$USER/CherryUF2/CURRENT.UF2
```

**Expected behavior:**
- LED blinks during programming
- Board resets automatically when complete
- OLED display shows the main menu

**Verification:** The serial console (SWD connector, 115200 baud) displays:
```
I/MAIN: ready
```

---

## Troubleshooting

| Observed Result | Possible Cause | Corrective Action |
|-----------------|----------------|--------------------|
| `CherryUF2` drive does not appear | UF2 bootloader not installed | Install bootloader (see previous section) |
| `dfu-util` reports no device | Missing udev rule or driver | Apply udev rule (Linux) or use Zadig (Windows) |
| Copy operation fails | Loose USB connection | Use different cable or USB port |
| OLED remains blank after reset | Application firmware corrupt | Re-enter UF2 mode and copy `rtthread.uf2` again |

---

## Building from Source

- **UF2 bootloader:** [at32f405-uf2boot](https://github.com/koendv/at32f405-uf2boot)
- **Application firmware:** See `../../../applications/` (RT-Thread)

**Complete documentation:** See [DEVELOPER.md](../../../doc/DEVELOPER.md)
