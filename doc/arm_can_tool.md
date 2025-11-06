# arm can tool

[![hello, world!](hello_world_small.jpg)](hello_world.jpg)

"arm can tool" is an arm/riscv debugger with canbus interface.

## use

The target can be debugged

- with openocd, using CMSIS-DAP
- with gdb, using black magic debug.
- standalone, scripted

"arm can tool" solves the question whether to use openocd or black magic debug by providing both. Additionally, the probe can be used standalone, calling CMSIS-DAP or black magic debug from a lua script.

### openocd

Using [openocd](https://github.com/ArteryTek/openocd) with CMSIS-DAP. Sample command line:

```sh
openocd -f interface/cmsis-dap.cfg -f target/stm32f1x.cfg
```

### black magic probe

Black magic probe is a gdb server.

Using [black magic probe](https://black-magic.org/index.html). Command line:

```sh
$ arm-none-eabi-gdb -q
(gdb) tar ext /dev/ttyACM0
Remote debugging using /dev/ttyACM0
(gdb) mon swd
Target voltage: 3.325V
Available Targets:
No. Att Driver
 1      STM32F1 L/M density M3
(gdb) at 1
```

## scripting

The firmware contains a lua interpreter. A lua program running in the probe has access to the debug target using CMSIS-DAP and black magic debug. An example of using CMSIS-DAP from lua:

```
msh />lua
> dap.init()
> request=string.char(0) .. string.char(2) .. string.rep("\0", 62)
> dap.process_request(request)
Generic CMSIS-DAP Adapter
```

Black magic debug can be used to read or write target memory from lua.

```
msh />lua
> bmd.attach()
true
> bmd.mem32_write(0x20001000, "0123456789ABCDEF")
true
> bmd.mem32_read(0x20001000, 16)
0123456789ABCDEF
```

The following functions are available:

```
bmd.help()
bmd.attach()
bmd.mem32_read(addr, len)
bmd.mem32_write(addr, str)
bmd.flash_mass_erase()
bmd.flash_erase(addr, len)
bmd.flash_write(addr, str)
bmd.flash_complete()
bmd.regs_read()
bmd.regs_write(str)
bmd.reg_read(reg)
bmd.reg_write(reg, str)
bmd.halt_request()
bmd.halt_resume()
bmd.halt_poll()
bmd.breakwatch_set(typ, addr, len)
bmd.breakwatch_clear(typ, addr, len)
bmd.reset()
```

This is basic, but sufficient for many things.

For the same task, lua uses more memory than C. Free ram drops by 32 kbyte after starting up lua.

lua is useful as a rapid prototyping tool.

## hardware block diagram

![block diagram](block_diagram.svg)

Note there are two spi flash chips. The 16 Mbyte qspi flash is used to store the program; the 16 Mbyte spi flash is used to store data (files). The qspi flash is written by the bootloader; the spi flash is written the operating system (rt-thread).

## software block diagram

![software block](software_block.svg)

"arm can tool" contains two debuggers:

- black magic debug, a gdb server
- free-dap, a cmsis-dap

The difference between black magic debug and free-dap (cmsis-dap) is the debugger software on the pc. gdb on the pc can use black magic debug directly. free-dap requires openocd and gdb on the pc.

within "arm can tool", cherry usb connects black magic debug and free-dap to different usb endpoints. The interaction between black magic debug and free-dap is minimal.

mui (minimal user interface) runs a small text menu on the oled display. The difference between "black magic debug" and "arm can tool" is that

- "black magic debug" settings are gdb "monitor" commands, and lost when power is removed.
- "arm can tool" settings are through the display menu, and saved in eeprom. The saved settings are restored at boot.

## processor

The AT32F405 has:

- 216 MHz Cortex-M4
- 96/102 kbyte RAM
- 256 kbyte zero-wait state flash
- high-speed and full-speed usb
- four-wire qspi in LQFP64

The processor is extended by various chips around it:

- 16 mbyte qspi flash at 108 MHz to store the program
- external i2c eeprom to store settings
- external i2c rtc with battery backup

### voltage translators

The probe uses level translators to convert from target voltage to probe 3.3V. Accepts logic voltages from 1.1V to 3.6V if using 74AVC4T77. Other level translators can be used to provide different voltage ranges.

The translators need to know the target voltage to work.

Either

- connect VIO to target VCC
- connect VIO to 3.3V internally

Connecting VIO to 3.3V can be done from the menu "Target" or from the command line, using gdb.
From the "Target" menu:

![target menu](target_menu.png)

From the command line:

```bash
 $ arm-none-eabi-gdb
(gdb) tar ext /dev/ttyACM0
Remote debugging using /dev/ttyACM0
(gdb) mon tpwr ena
Enabling target power
(gdb) mon tpwr dis
Disabling target power
```

## canbus

![canbus menu](canbus_menu.png)

The canbus interface is classic CAN, up to 1mbit/s. The canbus interface is isolated. At this moment the software just writes all canbus packets to the usb serial in slcan format.

#### canbus connector

The CAN bus connector is GH1.25, 6pin.

| pin | signal          |
| --- | --------------- |
| 1   | ISOLATED GROUND |
| 2   | CAN_HIGH        |
| 3   | CAN_LOW         |
| 4   | CAN_HIGH        |
| 5   | CAN_LOW         |
| 6   | ISOLATED GROUND |

A small cable then goes from connector to the canbus.
An example cable from 6 pin GH1.25 to two 4 pin GH1.25:

[![canbus cable](canbus_cable_small.jpg)](canbus_cable.jpg)

cable [wiring diagram](canbus_cable.pdf)

Note the debugger has no internal terminating resistors. If the tool is the last device on the bus, and a terminating resistor is needed, put a 120R resistor on the JST GH connector.

### canbus isolation

- 8 mm spacing between CAN and logic sides provides ample margin for typical CAN bus voltages.
- The FR4 material (CTI ≥ 175 V) is suitable for standard applications, but not rated for high-voltage isolation. For high voltage you would prefer High-CTI FR4, (CTI ≥ 600 V).
- Flux residue remains on the PCB as part of the production process; this can create leakage paths, especially in the presence of humidity or condensation.
- No conformal coating is applied. The board is not protected against moisture, dust, corrosion, or environmental contaminants such as oils, salts, or airborne chemicals.
- ⚠️ This design is not intended for high-voltage use. Use only in clean, controlled environments.

High voltages require High-CTI FR4 (CTI ≥ 600 V) and conformal coating. This would make the board prohibitively expensive. If high-voltage isolation is needed, perhaps it is more suitable to put the canbus interface in a separate, small two-layer board with few components. XXX link to "canbus isolator" project XXX

## pinout

The target jtag connector is GH1.25, 6 pin.

| Pin | SWD   | JTAG |
| --- | ----- | ---- |
| 1   | VIO   | VIO  |
| 2   | TXD   | TDI  |
| 3   | RXD   | TDO  |
| 4   | SWDIO | TMS  |
| 5   | SWCLK | TCK  |
| 6   | GND   | GND  |

In SWD mode pin 2 and 3 are a serial port, serial 0.
In JTAG mode pin 2 and 3 are TDI/TDO jtag signals.

The auxiliary connector is GH1.25, 6 pin.

| Pin | Signal |
| --- | ------ |
| 1   | VIO    |
| 2   | TXD1   |
| 3   | RXD1   |
| 4   | SWO    |
| 5   | NRST   |
| 6   | GND    |

Pin 2 and 3 are a serial port, serial 1.
Pin 4 is a receive-only serial, serial 2.
Pin 5 connects to the target reset signal.

The connector marked "SWD" is the swd connector of the AT32F405.

| Pin | SWD         |
| --- | ----------- |
| 1   | 3.3V        |
| 2   | CONSOLE TXD |
| 3   | CONSOLE RXD |
| 4   | SWDIO       |
| 5   | SWCLK       |
| 6   | GND         |

Pins 2 and 3 are the rt-thread console. The console has a shell prompt.

## serials

![serial menu](serial_menu.png)

There are three serials: serial0, serial1 and serial2.

- serial0 is a high-speed serial. serial0 is typically used to connect to the target console.
  The checkbox "swap rxd txd" can be used to swap serial port pins, in case a crossed cable is needed.

- serial1 is a high-speed serial.
  The checkbox "swo decode" can be used to decode a SWO stream on the RXD1 pin.
  The decoded SWO stream is printed on usb cdc1.

- serial2 is low speed.
  Use e.g. as 115200 baud logger.

## logging

To switch on logging, go to menu "Mode" and select "logging".
Next, go to menu "Settings" and select "Store".
Reboot.

Logging can be switched on in menu "Mode".
Log files are written to sdcard. Please insert a dos formatted sdcard before switching logging on. Make sure the sd card has a directory "log".

There are two log files, one for rt-thread and one for blackmagic.
 
The rt-thread system log is written to /sdcard/log/rtthread.log
If the system log grows too large, the logfile is rotated: rtthread.log.1

The output from black magic debug (auxiliary port) is written to /sdcard/log/log_0000.txt
If the log grows too large, logging switches to another file: log_0001.txt

Console commands:
The `ulog` command logs its arguments to the rt-thread system log.
```
ulog hello, world!
```
The `ulog -c` command switches console logging off. Logging to file is unchanged.

The `logger` command logs its arguments to the black magic log.
```
logger hello, world!
```

The `logger -f` command flushes the log from ram to sdcard.

```
msh />logger -f                                                                 
I/LOG: flush 
```

## watchdog

To switch on the watchdog, go to menu "Mode" and select "watchdog".
Next, go to menu "Settings" and select "Store".
Reboot.

If the system is unresponsive for 20 seconds, and the watchdog is switched on, the system will reboot.

## user interface

The user interface should be frugal with cpu and memory.

![main menu](main_menu.png)

Display controllers were tested for speed and memory. Color controllers use too much memory. Greyscale controllers are slower than black and white controllers. SPI controllers were preferred over I2C controllers, as less likely to hang, even if the display is not plugged in. Various black and white OLED display controllers were tested, and the fastest chosen.

The display is 128\*128 black and white only, sh1107 SPI controller. This combines speed and using as little ram as possible.

The display is square, which makes changing display orientation easy.

The small multi-direction switch is debounced in both hardware and software, and generates one clean interrupt per press. There is no need for polling in software.

Graphics software is u8g2 with [minimal user interface](https://github.com/olikraus/u8g2/wiki/muimanual#mui) (mui). u8g2 was originally written for a 16MHz 8-bit processor. On a 216MHz 32-bit processor the software flies.

Yes, the probe has an interactive display - but everything has been done to make the display as lightweight as possible.

## UTF8

![utf8 test](utf8_test.png)

The user interface supports displaying UTF8 characters. The font used is [GNU Unifont](https://www.unifoundry.com/unifont/). Latin characters are variable width; Chinese characters are 16x16. To conserve flash memory, the font only contains the characters used in the menus.

After editing the menu, if new characters have been introduced, please run the command `update_unifont.sh`

u8g2 issues: [#2627](https://github.com/olikraus/u8g2/pull/2627) [#2656](https://github.com/olikraus/u8g2/issues/2656)

## screenshots

To make a screenshot, connect to the serial console, and type `printscreen`. The `printscreen`command outputs the current display in .pbm format. Copy paste from the console into a text document, and post-process. Example:

```bash
$ convert input.pbm -rotate 90 -bordercolor white -border 1 -bordercolor black -border 1 output.png
```

## manufacturing

XXX picture with all components

The board can be made in small quantity (2 or 5) using jlcpcb "economic" pcb assembly.

### PCB assembly

[![pcb front](pcb_front_small.jpg)](pcb_front.jpg)

[![pcb front](pcb_back_small.jpg)](pcb_back.jpg)

The board is assembled at jlcpcb using "economic" assembly. All components are smd components. No manual soldering is needed.

During the ordering process:

- Specify Stackup: JLC04161H-7628
- Mark on PCB: Order Number (Specify Position).
- PCBA Qty: 2 or 5, choose.
- PCBA Remark: NO SOLDER PASTE ON MIDDLE PAD OF BATTERY HOLDER C964818

The position of the order number on the board is specified to avoid the order number accidentally being placed in the canbus isolation barrier.

Do not request ultrasonic board cleaning. The board contains the DS3231 real-time clock with built-in crystal. The DS3231 datasheet says ultrasonic cleaning should be avoided to prevent damage to the crystal.

PCB assembly cost (07/2025): \\$115 for 2, \\$153 for 5.

The processor AT32F405RCT7-7 C9900094935 is a consigned part. A consigned part is a part supplied by the customer to jlcpcb.
The AT32F405RCT7-7 was consigned as follows: superbuy.com, a Chinese shopping agent, bought the processors at taobao.com, and forwarded the parcel to the jlcpcb warehouse. Please [follow jlcpcb instructions](https://jlcpcb.com/help/article/how-to-consign-parts-to-jlcpcb) exactly. Cost was \\$17.5 for 10 processors.

### OLED display

[![oled](oled128x128_sh1107_small.jpg)](oled128x128_sh1107.jpg)

The OLED display used is [ZJY150-2828KSWKG03](ZJY150-2828KSWKG03.pdf). This is a 1.5", 128x128 pixel, SH1107 driver, SPI interface, 12 pin OLED. The display can be ordered from

- [alibaba](https://www.alibaba.com/product-detail/1-5-inch-OLED-display-module_1601054555841.html)
- [aliexpress](https://www.aliexpress.com/item/1005007579159330.html)
- [taobao](https://item.taobao.com/item.htm?id=756962244579)

Choose the 12pin connector with SPI.

### case

[![3d printed case](3d_printed_case_small.jpg)](3d_printed_case.jpg)

The Easyeda project includes a case. The case consists of two parts, top and bottom, connected using 4 M3x12 nylon screws and nuts. For the case design files, in the Easyeda project open the PCB; see layers "3D Top", "3D Bottom", "3D Outline". For the STL files, choose Export->3D Shell File. The case was printed at [jlc3dp](https://jlc3dp.com).

The case can be 3D printed from different materials. Each material has its use.

#### prototype for testing

The case for the prototype was made in clear, transparent plastic (SLA, 8001 resin). A transparent case allows seeing whether there is sufficient space between case, pcb and connectors.

Assume any electronics device will fall at least once from a table to a hard floor. A prototype was intentionally dropped from a table. The clear case allowed checking there was no damage, without opening the case.

#### production

For production, choose the cheapest case that works.
The board has two sides: display and components.
Every connector has the names of the signals printed on the display side of the board. The display side of the case is transparent to allow reading the signal names.
The component side of the case is white, because white resin is cheaper than transparent.
The case is printed in SLA. Cost is about $5.

| Side      | color       | jlc3dp     | pcbway   |
| --------- | ----------- | ---------- | -------- |
| display   | transparent | 8100 resin | UTR-8100 |
| component | white       | 9600 resin | UTR-8360 |

#### heatproof

The above resins may be sufficient for office use. But these resins begin to deform at 53°C to 59°C. This is roughly the temperature of the dashboard of a car parked in the sun.
If a heatproof case is desired, perhaps choose "JLC Temp Resin" (jlc3dp, 101°C) or "Somos Perform" (pcbway, 122℃). Nylon also works.

### assembly

Assembly is straightforward:

- thread OLED flexible cable through PCB
- plug OLED flexible cable in PCB connector
- fix OLED display to PCB using double-sided tape.
- close box using 4 nylon M3 screws and nuts

Do _not_ first glue the OLED display and then try to plug the OLED cable in the PCB connector.

If using double-sided removable adhesive tape with color-coded liner, pink on one side, and blue on the other:

- stick the side with the pink liner to the PCB
- stick the side with the blue liner to the OLED.

## firmware

The AT32F405 firmware consists of two binaries:

- bootloader in internal flash
- application in external qspi flash

Both have to be installed.

### install bootloader

The bootloader binary is `cherryuf2_arm_can_tool.bin`

The bootloader source is at [at32f405-uf2boot: Bootloader for Artery AT32F405 with QSPI flash](https://github.com/koendv/at32f405-uf2boot)

To install the bootloader using dfu-util:

- connect the board to usb
- push reset and boot0 buttons
- wait one second
- release the reset button
- wait one second
- release the boot0 button

At this point, the at32f405 ought to be in DFU mode:

```
$ lsusb
...
Bus 003 Device 012: ID 2e3c:df11 Artery-Tech DFU in FS Mode
```

With the firmware `cherryuf2_arm_can_tool.bin` in your current directory, execute the following command:

```
sudo dfu-util -a 0 -d 2e3c:df11 --dfuse-address 0x08000000 -D cherryuf2_arm_can_tool.bin
...
Erase       [=========================] 100%        23692 bytes
Erase    done.
Download    [=========================] 100%        23692 bytes
Download done.
File downloaded successfully
```

This completes installing the bootloader.

Linux users: If dfu-util does not see the at32, execute dfu-util as root with `sudo` or set up udev-rules:

```
# Artery AT32 DFU
# Copy this file to /etc/udev/rules.d/99-at32.rules and run: sudo udevadm control -R
SUBSYSTEMS=="usb", ATTRS{idVendor}=="2e3c", ATTRS{idProduct}=="df11", TAG+="uaccess", MODE="0664", GROUP="plugdev"
```

Windows users: If dfu-util does not see the at32, download [zadig](https://zadig.akeo.ie/), install the WinUSB driver and run dfu-util again.

The boot loader prints either "app" or "dfu"

#### dfu

dfu is the abbreviation of _Device Firmware Update_. When the multi-direction switch is pressed during boot, the boot loader waits for a uf2 file. Console output:

```
dfu
[SFUD]Found a Winbond flash chip. Size is 16777216 bytes.
[SFUD]qspi1 flash device initialized successfully.
```

#### app

App is the abbreviation of _APPlication_. When the multi-direction switch is not pressed during boot, the boot loader starts the operating system. Console output:

```
app
 \ | /
- RT -     Thread Operating System
 / | \     5.2.1 build Jul 14 2025 11:29:50
```

### install application

The bootloader is used to install the application.

The application binary is `rtthread.uf2`

The application source is at XXX

To install the application using the bootloader:

- unplug arm_can_tool from usb
- press the multi-direction switch
- plug arm_can_tool in usb. The green led switches on. On the pc a usb mass storage appears. In the mass storage there is a file `CURRENT.UF2`
- copy the file `rt-thread.uf2` to the file `CURRENT.UF2`. The led blinks during firmware update.
  `cp rtthread.uf2 /media/$USER/CherryUF2/CURRENT.UF2`
- unplug the arm_can_tool from usb and plug it back in. The oled display now shows the menu.

This completes installing the application.

### ram parity

The at32f405 cpu has 96k ram if ram parity check is enabled, 102k ram if ram parity check  is not enabled (default).
The software has been compiled for 102kbyte ram.
If the bootloader prints "96k ram", the option bits need a reset.

## software

The software consists of:

- [rt-thread](https://github.com/RT-Thread/rt-thread) operating system
- [u8g2](https://github.com/olikraus/u8g2) graphics, with [mui](https://github.com/olikraus/u8g2/wiki/muimanual#introduction) menu system
- [black magic debug](https://github.com/blackmagic-debug/blackmagic) gdb server
- [free-dap](https://github.com/ataradov/free-dap) cmsis-dap server
- [cherry dap](https://github.com/cherry-embedded/CherryDAP) usb stack
- [lua](https://github.com/lua/lua) programming language

The software can be further optimized:

- writing directly to GPIO registers instead of going through operating system calls
- putting "hot" routines in zero-wait-state memory
- using dma to gpio for SWD/JTAG

but at the moment bitbanging is done using the operating system gpio calls.

## black magic debug extensions

Compared to upstream black magic debug, two additions: memwatch and rtt halt.

## memwatch - read memory while target running

This introduces a new gdb monitor command, _memwatch_.
The memwatch command reads variables in target memory while the target is running, and prints variable and value when value changes. Inspecting memory without halting the target is useful when debugging hard real time systems.

The arguments to "mon memwatch" are names, formats (/d, /u, /f, /x, /t), and memory addresses. Up to 8 memory addresses may be monitored at the same time. The "mon memwatch" command can be applied to Arm Cortex-M targets only.

### Example

Assume target firmware contains a variable called "counter". In gdb:

```
(gdb) p &counter
$1 = (<data variable, no debug info> *) 0x20000224 <counter>
(gdb) mon memwatch counter /d 0x20000224
0x20000224
(gdb) r
```

When the variable changes, output is written to the usb serial:

```
counter 0
counter 1
counter 2
counter 3
counter 4
```

See [Using memwatch](UsingMemwatch.md) for more information.

## rtt halt

On some target processors it may be necessary to halt the target while rtt data is copied from target to debugger.

`mon rtt halt enable` always halt the target processor during rtt.
`mon rtt halt disable` never halt the target processor during rtt
`mon rtt halt auto` default is never halt the target processor during rtt. only halt target processor on

- risc-v that have no system bus
- TM4C123GH6PM

Default is `mon rtt halt auto`. Note many rtt problems are cache problems.

## compiling

To compile the firmware on linux:

```
git clone https://github.com/RT-Thread/rt-thread
cd rt-thread/bsp/at32
# XXX need to add package blackmagic
# XXX need to put git url
git clone ~/src/repos/arm_can_tool
cd arm_can_tool
pkgs --update
patch -p1 -d ../../../ <  patches/dev_can.patch
patch -p1 -d ../../../ <  patches/dev_rtc.patch
patch -p1 -d ../../../ <  patches/dma_config.patch
patch -p1 -d packages/Lua-latest/ <  patches/lua.patch
scons -c
scons
arm-none-eabi-objcopy -O binary rtthread.elf rtthread.bin
# uf2 family id 0xf35c900d load address 0x90000000
bin2uf2 -f 0xf35c900d -o rtthread.uf2 0x90000000 rtthread.bin
```

This creates the uf2 binary rtthread.uf2.

## hardware

The EDA software used for designing schematics and board is Chinese. The processor is Chinese. The operating system is Chinese. Board assembly is done in China.

- [Schematics](SCH_Schematic1_2025-07-18.pdf)
- [PCB](PCB_PCB1_2025-07-18.pdf)

Schematics and pcb design files are at oshwlab [arm_can_tool](https://oshwlab.com/koendv/arm_can_tool).

The board has been designed using [EasyEDA Pro](https://pro.easyeda.com/).

### 3D view

[![3D front view](3D_PCB1_front_small.png)](3D_PCB1_front.png)
[![3D back view](3D_PCB1_back_small.png)](3D_PCB1_back.png)

To see a 3D model of the board:

- open the project in EasyEDA Pro.
- open the pcb "PCB1".
- click "3D" in the menu bar. This shows the board in the 3D shell.
- click "Layers"
- click the eye icon labelled "3D Shell". The 3D Shell disappears, and the board itself is shown.

## hardware tests

Hardware tests for:

- power supply
- crystals
- i2c
- spi flash

and others

### power supplies

Test pads on the back of the board allow checking the power supplies:

| Voltage   | Range       | Reference | Note                                                                                                                                              |
| --------- | ----------- | --------- | ------------------------------------------------------------------------------------------------------------------------------------------------- |
| 5V USB    | 4.75-5.25V  | GND       | from USB                                                                                                                                          |
| 3.3V      | 3.3V        | GND       | main power                                                                                                                                        |
| 12V OLED  | 11.7-12.4V  | GND       | 12.1V typical                                                                                                                                     |
| 3.3V CARD | 3.3V        | GND       | SD card voltage range is 2.7-3.6 V. Inrush current limit. Switched on by firmware when a SD card is present. Switched off when a card is removed. |
| VIO       | 1.1V - 3.6V | GND       | Target VCC for voltage translators. Powered by target or probe. See "voltage translators" how to power VIO from the probe.                        |
| ISO 5V    | 5.1V        | ISO GND   | 5V isolated, for CANBUS transceiver. transceiver voltage range is 4.5-5.5V.                                                                       |

### spare pins

There is room for future expansion.

Between usb connector and switching power supply there is room for a battery charger ic.

Spare pins for hardware expansion.

- pin PB1 is free, to be used as the CS of a new SPI device.
- if two more pins are needed, free up pin PC0 and PC1 by connecting the I2C connector to pin PB6 and PB7.

### console

Install firmware. Connect a vt100 terminal emulation to the connector "SWD", pin RXD and TXD, 115200 8N1. Check the console prompt appears.

### boot

Check the boot message for errors. This is a clean boot with sdcard inserted:

```
102k ram
app
 \ | /
- RT -     Thread Operating System
 / | \     5.2.1 build Jul 14 2025 11:29:50
 2006 - 2024 Copyright by RT-Thread team
[u8g2] Attach device to spi22
D/I2C_S: Software simulation i2c1 init done, SCL pin: 0x16, SDA pin: 0x17
D/I2C_S: Software simulation i2c3 init done, SCL pin: 0x20, SDA pin: 0x21
I/EEPROM: settings loaded
msh />I/MULTI: init
I/CAN: canbus speed 1000000
I/CAN: can1 canbus init ok
I/SFUD: rom mount to '/'
I/SFUD: Found a flash chip. Size is 16777216 bytes.
I/SFUD: norflash0 flash device initialized successfully.
I/SFUD: Probe SPI flash norflash0 by SPI device spi20 success.
D/FAL: Flash device |                norflash0 | addr: 0x00000000 | len: 0x01000
000 | blk_size: 0x00001000 |initialized f
I/FAL: ==================== FAL partition table ====================
I/FAL: | name  | flash_dev |   offset   |    length  |
I/FAL: -------------------------------------------------------------
I/FAL: | part0 | norflash0 | 0x00000000 | 0x01000000 |
I/FAL: =============================================================
I/FAL: RT-Thread Flash Abstraction Layer initialize success.
I/FAL: The FAL MTD NOR device (part0) created successfully
I/SFUD: spi flash mount to /flash
I/SD: sdcard mount
I/SD: sd card mounted on /sdcard
I/MAIN: ready
msh />
```

### crystals

Do not measure oscillator frequency touching the crystal with an oscilloscope probe; the capacitance of the probe will change the oscillator frequency slightly.

To accurately measure HSE and LSE oscillator frequency, use the `clkout_12m` and `clkout_32k` commands.

#### 12 MHz crystal

from the console prompt, execute

```
msh />clkout_12m
```

A 1MHz square wave appears on pin PB13. Measure on R24. The signal on PB13 is the system clock divided by 12.

#### 32768 kHz crystal

from the console prompt, execute

```
msh />clkout_32k
```

A 32768 Hz square wave appears on pin PB13. Measure on R24.

### real-time clock

Insert a CR1220 battery in the battery holder. Set the time.

Example: for 2025, July 14, 12:07:00 hours:

```
msh />ds3231 date 2025 07 14 12 07 00
msh />ds3231 date
2025 7 14 12 7 35
```

Reboot. With the external rtc set, the internal rtc should now have the same time as the external rtc:

```
msh />date
local time: Mon Jul 14 12:08:49 2025
timestamps: 1752466129
timezone: UTC+08:00:00
```

### I2C

Run i2c_scan to see devices on the internal I2C bus:

```shell
msh />i2c_scan
    00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F
00:                         -- -- -- -- -- -- -- --
10: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
20: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
30: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
40: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
50: 50 -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
60: -- -- -- -- -- -- -- -- 68 -- -- -- -- -- -- --
70: -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
```

These devices are:

- AT24C256 EEPROM (0x50)
- DS3231 RTC (0x68)

Plug an i2c device in the qwiic connector for the external i2c bus. Run `i2c_scan i2c3` and check the device appears.

### flash

The board has 16MB spi flash. Benchmark the flash from the command prompt. Running the benchmark erases the flash completely.

```shell
Probed a flash partition | flash | flash_dev: norflash0 | offset: 0 | len: 16777216 |.
msh />fal bench 4096 yes
Erasing 16777216 bytes data, waiting...
Erase benchmark success, total time: 39.097S.
Writing 16777216 bytes data, waiting...
Write benchmark success, total time: 157.395S.
Reading 16777216 bytes data, waiting...
Read benchmark success, total time: 17.782S.
```

### sd card

Insert an sd card formatted in FAT32.

```
msh />
I/SD: sdcard mount
I/SD: sd card mounted on /sdcard
msh />ls /sdcard/
Directory /sdcard/:
```

Remove the sd card. Check the console logs this.

```
I/SD: sdcard unmount
```

### target reset

The target reset pin can be brought low by the debugger, or by pushing the reset button on the target. Because of this, there are two pins for the target reset:

- target reset out: driven low when the debugger wants to reset the target

- target reset in: low when the target is in reset

A small command, `trst`, allows testing the target reset hardware.

```
msh />trs
trst
msh />trst high
trst_out high
trst_in low
msh />trst low
trst_out low
trst_in high
```

### canbus isolation

With unpowered, unconnected board, using a multimeter, check the resistance between the test pads GND and ISO GND. Multimeter should show OL.

### canbus

Install firmware. Go to the canbus menu.

![menu canbus](menu_canbus.png)

Set canbus speed in the menu. Check _slcan output_ is selected.

Connect to the _second_ usb serial. On Windows, use putty. On linux:

```
minicom -D /dev/ttyACM1
```

Connect the tool to a canbus. Check that canbus packets are printed in slcan format on the usb serial port.

From the rt-thread console shell prompt, type

```
msh> canbus send
```

This sends a packet, ID=001, Data=01 23 45 67 89 AB CD EF. Verify the nodes on the bus see the packet.

#### EMI

A very hands-on approach to electro-magnetic interference (EMI).

The QSPI flash is used for program storage and runs at 108 MHz. 108 MHz is the highest frequency of the FM radio band.

Take an FM radio, tune to 108 MHz or slightly below. Put the antenna on the component side of the board, 1 to 2 cm above the QSPI traces. When the "arm can tool" is connected to usb, a local radio station at 107.8 Mhz is replaced with a shrill noise.

The effect decreases rapidly with distance. At 10-30 cm distance there is already no interference. In most cases EMI will not be a problem.

XXX If necessary
- cover the inside of the back of the enclosure with copper foil 3M 1181
- or add EMI [shielding clips](https://jlcpcb.com/parts/componentSearch?searchTxt=shielding%20clip) to the board, to easily attach and detach EMI shield cans.

## designed for repair

The device has been designed to allow manual repair.

If you can repair mobile phones you can repair this.

If a voltage level translator, canbus transceiver, or one of the SOIC or LQFP components fails, try the following:

- under microscope, using a scalpel or sharp cutter, with patience, cut the component pins until the ic body is free
- using a generous amount of flux, unsolder the pins one by one
- remove excess solder using copper braid
- clean with isopropyl alcohol
- apply flux, solder new ic in place

If needed, the processor can also be replaced this way. You should find there is enough room around these components to swipe a soldering iron across the pins.

This method fails for the 4x33R resistor arrays, or the H5VU25U ESD protection, as there are no pins to cut through. For these components, use hot air.

_not truncated_

It is more economical to assemble several boards at a time. Manufacturing 5 boards, and selling the boards one by one would be ideal. This requires a service on top of jlcpcb / pcbway, to

- buy consigned parts and ship them to jlcpcb
- order 5 pcb's from jlcpcb, 5 3d shells from jlc3dp, 5 oled displays from taobao, cables, screws, ...
- make packages with 1 enclosure, 1 pcb, 1 display
- sell product on aliexpress
- perhaps also offer a more expensive version, ready for use, with the display already connected, the pcb placed in the 3d shell, the firmware installed, and GH1.25 to Dupont cables.


