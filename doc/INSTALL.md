# ARM CAN TOOL — Firmware Installation

## Memory Map

| Memory | Capacity | Contents | Installed |
|---|---|---|---|
| Processor ROM | Factory | DFU bootloader | Factory — read-only |
| Internal flash | 256 kB | UF2 bootloader | Once, using dfu-util |
| QSPI flash | 16 MB | Application firmware | First use and every update |

Board received from PCB assembly has no UF2 bootloader and no application firmware installed.
Complete both steps before first use.

Download required files:

| File | Source |
|---|---|
| `cherryuf2_arm_can_tool.bin` | [at32f405-uf2boot releases](https://github.com/koendv/at32f405-uf2boot/releases) |
| `rtthread.uf2` | [arm_can_tool releases](https://github.com/koendv/arm_can_tool/releases) |

---

## Step 1 — Install UF2 Bootloader

Perform this step once. UF2 bootloader installs to internal flash via the factory DFU bootloader.

### Prerequisites

- `dfu-util` installed.
  - Linux: `sudo apt install dfu-util`
  - Windows: [dfu-util](https://dfu-util.sourceforge.net/)
- USB data cable connected to board.
- Windows only: WinUSB driver installed via Zadig for the Artery DFU device.

### Linux — USB Access Without sudo

Install udev rule:

```
$ echo 'SUBSYSTEMS=="usb", ATTRS{idVendor}=="2e3c", ATTRS{idProduct}=="df11", TAG+="uaccess", MODE="0664", GROUP="plugdev"' \
  | sudo tee /etc/udev/rules.d/99-at32.rules
$ sudo udevadm control -R
```

### Enter DFU Mode

![DFU button sequence](pictures/buttons_dfu.svg)

Figure 1 — BOOT0 and RESET button locations.

1. Press and hold BOOT0 and RESET simultaneously.
2. Release RESET after about 1 second.
3. Release BOOT0 after about 1 further second.

Verification: Board enumerates as Artery DFU device.

```
$ lsusb | grep Artery
Bus 003 Device 012: ID 2e3c:df11 Artery-Tech DFU in FS Mode
```

If device does not appear:

- Check cable is a data cable, not a charging-only cable.
- Repeat the button sequence.

### Write UF2 Bootloader

```
$ sudo dfu-util -a 0 -d 2e3c:df11 --dfuse-address 0x08000000 -D cherryuf2_arm_can_tool.bin
```

Expected output:

```
Erase   [=========================] 100%        23692 bytes
Erase done.
Download [=========================] 100%        23692 bytes
Download done.
File downloaded successfully
```

Verification: Output shows `File downloaded successfully`.

On normal boot, UF2 bootloader starts application automatically.

---

## Step 2 — Install Application Firmware

UF2 bootloader exposes QSPI flash as USB mass storage. No additional tools required.

### Enter UF2 Bootloader Mode

![UF2 bootloader button sequence](pictures/buttons_uf2.svg)

Figure 2 — Multi-direction switch and RESET button locations.

1. Press and hold the multi-direction switch and RESET simultaneously.
2. Release RESET after about 1 second.
3. Release multi-direction switch after about 1 further second.

Green LED illuminates. USB mass storage device named CherryUF2 appears on host PC.

Verification (Linux):

```
$ ls /media/$USER/CherryUF2/
CURRENT.UF2
```

### Copy Firmware

**Linux:**

```
$ cp rtthread.uf2 /media/$USER/CherryUF2/CURRENT.UF2
```

**Windows — Command Prompt:**

```
COPY rtthread.uf2 D:\CURRENT.UF2 /Y
```

Replace `D:` with the drive letter assigned to CherryUF2.

LED blinks during programming. Programming completes, board resets, and boots into application.

Verification: OLED displays main menu after reset.
OLED: About shows new compilation date.

---

## Updating Firmware

Repeat Step 2. UF2 bootloader does not require reinstallation.

---

## Linux — Persistent Device Names

By default, Linux assigns CDC serial ports as `/dev/ttyACM0`, `/dev/ttyACM1`, etc.
Assigned number depends on connection order and may change between sessions.

Install udev rules file from repository to create persistent symlinks:

```
$ sudo cp tools/udev/99-arm-can-tool.rules /etc/udev/rules.d/
$ sudo udevadm control -R
$ sudo udevadm trigger
```

Add user account to plugdev group:

```
$ sudo usermod -a -G plugdev $USER
```

Log out and back in for group change to take effect.

Verification: Depending upon _arm can tool_ mode, one or more symlinks listed below appear in `/dev/`.

**Table 2 — Persistent Device Symlinks**

| Symlink | Interface | Mode |
|---|---|---|
| `/dev/ttyBmpGdb` | GDB server | GDB Server |
| `/dev/ttyBmpTarg` | Target console (log output) | GDB Server, CMSIS-DAP |
| `/dev/ttyBmpSlcan` | SLCAN | CMSIS-DAP |
| `/dev/ttyBmpLua` | Lua script console | Lua Script |
| `/dev/ttyBmpUser` | Lua script user I/O | Lua Script |

