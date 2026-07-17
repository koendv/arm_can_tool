# ARM CAN TOOL — Debug Guide

Debug workflows, memwatch, RTT, SWO for ARM and CAN bus targets.

For initial setup (firmware verification, factory reset, display orientation, target power): See [OPERATION](OPERATION.md).

For a complete first-session walkthrough: See [TUTORIAL](TUTORIAL.md).

---

## Choosing a Mode

- Mode → GDB Server runs Black Magic Debug directly on the probe.
- Mode → CMSIS-DAP hands off to an intermediary — OpenOCD, BMDA, or pyOCD — running on the PC.

Switching between Black Magic Debug and OpenOCD is a menu setting, not a wiring change — both modes use the same SWD/JTAG connection to the target.

---

## GDB Server Mode — Black Magic Debug

In GDB Server mode, the probe runs Black Magic Debug firmware and exposes a GDB server as a USB serial port. `arm-none-eabi-gdb` connects directly to the probe. No intermediary software runs on the PC.

The probe also exposes the CAN bus as GS-USB / SocketCAN.

1. Navigate to Mode → GDB Server. Device reboots.

2. Start GDB and connect to the probe:

```
$ arm-none-eabi-gdb
(gdb) target extended-remote /dev/ttyBmpGdb
```

Device path `/dev/ttyBmpGdb` requires udev rules (See INSTALL.md). Without udev rules: `/dev/ttyACMx`

| OS | Device path |
|---|---|
| Linux | `/dev/ttyACMx` — the second of the two ttyACMx ports ARM CAN Tool exposes |
| Windows | COM port assigned to GDB server interface |
| macOS | `/dev/cu.usbmodemXXXX` |

3. Scan for targets:

```
(gdb) monitor swd_scan
```

Expected output:

```
Target voltage: 3.325V
Available Targets:
No. Att Driver
 1      STM32F1  L/M density M3
```

If `Target voltage: 0.000V`:

- Target is not powered
- If target is powered: VIO is not connected.
- If probe supplies target power: Target → 3.3V Power is not set.

4. Attach, load firmware, and run:

```
(gdb) attach 1
(gdb) file firmware.elf
(gdb) load
(gdb) run
```

Verification: GDB reports `Attaching to Remote target`. Target executes loaded firmware.

The `monitor` prefix sends commands to Black Magic Debug firmware on the probe.

---

## CMSIS-DAP Mode — Intermediary Required

In CMSIS-DAP mode, the probe exposes a CMSIS-DAP interface. `arm-none-eabi-gdb` does not connect directly to the probe — an intermediary program runs on the PC between GDB and the probe:

```
arm-none-eabi-gdb → intermediary (PC) → ARM CAN Tool (CMSIS-DAP)
```

The probe also exposes the CAN bus as SLCAN.

Three intermediaries are supported:

### OpenOCD

1. Navigate to Mode → CMSIS-DAP. Device reboots.

2. In a first terminal, start OpenOCD:

```
$ openocd -f interface/cmsis-dap.cfg -f target/stm32f1x.cfg
```

3. In a second terminal, start GDB and connect to OpenOCD:

```
$ arm-none-eabi-gdb
(gdb) target extended-remote :3333
(gdb) file ~/firmware/build/firmware.elf
(gdb) load
(gdb) run
```

Verification: OpenOCD reports `Listening on port 3333 for gdb connections`. GDB connects and target executes loaded firmware.

Prebuilt OpenOCD binaries: [xPack project](https://xpack-dev-tools.github.io/openocd-xpack/).

### Black Magic Debug App (BMDA)

BMDA is the hosted (PC-side) version of Black Magic Debug. See [codeberg.org/blackmagic-debug/blackmagic](https://codeberg.org/blackmagic-debug/blackmagic).

1. Navigate to Mode → CMSIS-DAP. Device reboots.

2. In a first terminal, start BMDA:

```
$ blackmagic
Listening on TCP port: 2000
```

Use `-v 3` to confirm ARM CAN Tool is detected:

```
$ blackmagic -v 3
Black Magic Debug App v2.0.0-rc1-4-gab6d92e8
 for Black Magic Probe, ST-Link v2 and v3, CMSIS-DAP, J-Link and FTDI (MPSSE)
Using 1209:8816 338472C52178 open hardware
 arm can tool (CMSIS-DAP/slcan) ---
Using bulk transfer
CMSIS-DAP v2.0.0, Capabilities: 03 (JTAG/SWD)
Adaptor supports DAP SWD sequences
Setting V6ONLY to off for dual stack listening.
Listening on TCP port: 2000
```

`1209:8816` is the VID:PID of ARM CAN Tool. `arm can tool (CMSIS-DAP/slcan)` confirms the probe is detected.

3. In a second terminal, start GDB and connect to BMDA:

```
$ arm-none-eabi-gdb
(gdb) target extended-remote :2000
(gdb) file ~/firmware/build/firmware.elf
(gdb) load
(gdb) run
```

Verification: BMDA reports `Listening on TCP port: 2000`. GDB connects and target executes loaded firmware.

### pyOCD

See [pyocd.io](https://pyocd.io).

1. Navigate to Mode → CMSIS-DAP. Device reboots.

2. In a first terminal, start pyOCD gdbserver:

```
$ pyocd gdbserver
```

3. In a second terminal, start GDB and connect to pyOCD:

```
$ arm-none-eabi-gdb
(gdb) target extended-remote localhost:3333
(gdb) file ~/firmware/build/firmware.elf
(gdb) load
(gdb) run
```

Verification: pyOCD reports GDB server listening on port 3333. GDB connects and target executes loaded firmware.

---

## Target Reset

AUX connector pin 5 (NRST) provides target reset.

Some processors require reset asserted during SWD or JTAG connection.

Manual reset: Press target reset button briefly while initiating debugger connection.

Automated reset via NRST: Connect AUX connector pin 5 to target reset line, then configure debugger.

For Black Magic Debug (GDB Server mode) and BMDA (CMSIS-DAP mode):

```
(gdb) monitor connect_rst enable
```

For OpenOCD:

```
reset_config srst_only srst_push_pull connect_assert_srst
```

---

## Memwatch — Live Memory Monitoring

Memwatch reads target memory while target is running, without halting execution. Available in GDB Server mode only, on ARM Cortex-M targets.

Up to 8 addresses monitored simultaneously. Output appears on CDC serial 0.

Useful for observing variables in hard real-time systems where halting alters behaviour.

### Command Syntax

```
(gdb) mon memwatch [/t] [[NAME] [/d|/u|/f[N]|/x] ADDRESS]...
(gdb) mon memwatch status
(gdb) mon memwatch
```

| Argument | Description |
|---|---|
| NAME | Optional label printed on each output line. Must begin with a letter |
| /d | Signed 32-bit integer |
| /u | Unsigned 32-bit integer |
| /f | IEEE 754 32-bit float, 6 decimal places |
| /fN | IEEE 754 32-bit float, N decimal places |
| /x | Hexadecimal 32-bit integer (default) |
| /t | Prefix each line with timestamp in milliseconds |
| status | Display current watchpoint configuration |
| (no arguments) | Disable all watchpoints |

### Single Variable

```
(gdb) p &counter
$1 = (<data variable, no debug info> *) 0x20000224 <counter>
(gdb) mon memwatch counter /d 0x20000224
(gdb) run
```

Output on CDC serial 0:

```
counter 0
counter 1
counter 2
```

### Multiple Variables with Timestamp

```
(gdb) mon memwatch /t counter /d 0x20000224 root /f4 0x20000248
```

Output:

```
20601 counter 0
20601 root 0.0000
20613 counter 1
20613 root 1.0000
21613 counter 2
21613 root 1.4142
```

---

## RTT

Real-Time Transfer (RTT) reads debug output from target RTT control block in memory while target is running.

RTT output destination depends on mode:

| Mode | RTT output |
|---|---|
| GDB Server | CDC serial 0 |
| Lua Script | CDC serial 0 |
| CMSIS-DAP / BMDA | BMDA stdout |

Enable RTT:

```
(gdb) monitor rtt enable
```

Verification:

```
(gdb) mon rtt stat
rtt: on found: yes ident: off halt: off channels: auto
max poll ms: 256 min poll ms: 8 max errs: 10
```

### Halt Control

On most targets, RTT control block is read without halting target. On some targets this is not possible.

| Command | Behaviour |
|---|---|
| `mon rtt halt enable` | Always halt target during RTT reads |
| `mon rtt halt disable` | Never halt target during RTT reads |
| `mon rtt halt auto` | Default. Halt only where required |

Many RTT connection problems are caused by CPU cache configuration, not by halt setting.

### Slow RTT Connection

If RTT is slow to connect, specify RTT control block address explicitly using GDB scripts. These scripts require `arm-none-eabi-gdb-py3`:

- `tools/rtt/rtt.bmd` — Black Magic Debug
- `tools/rtt/rtt.openocd` — OpenOCD

---

## SWO

SWO (Serial Wire Output) data arrives on serial2 via AUX connector pin 4. Receive only.

SWO is a direct hardware connection from target SWO pin to probe RXD2. It operates independently of the debug interface mode (GDB Server or CMSIS-DAP).

SWO requires Arm ITM. ITM is present on Cortex-M3, M4, M33, and others. ITM is not present on Cortex-M0.

1. Connect target SWO pin to AUX connector pin 4.
2. Set speed: Serial → serial2 speed.
3. Enable SWO decode: Startup → swo decode → On.
4. Enable serial2: Serial → Serial Enable → serial2 → On.
5. Connect terminal emulator to `/dev/ttyBmpTarg` (CDC serial 0).

Verification: Decoded SWO output appears in terminal emulator.

In gdb server:
To switch swo on, serial port speed 1000000, swo ports 1 and 3:

```
(gdb) mon swo enable 1000000 decode 1 3
Channel mask: 00000000000000000000000000001010
```

to switch swo off:

```
(gdb) mon swo disable
Trace disabled
```

## Semihosting

Using semihosting a target program can write to stdout, read from stdin, and do basic file input/output.

To enable semihosting, compile the target with a library that implements semihosting:

```
arm-none-eabi-gcc ... --specs=rdimon.specs -lrdimon
```

For each semihosting system call, the target halts, the debugger completes the semihosting system call, and the target continues program execution.

⚠️ **Warning — always run a semihosting program with a debugger:** Without debugger, the target waits forever.

Where semihosting calls are serviced depends upon arm can tool mode:

| Semihosting     | GDB Server mode           | CMSIS-DAP mode   |
| --------------- | ------------------------- | ---------------- |
| runs on         | arm can tool              | pc               |
| file read/write | sdcard or flash           | pc filesystem    |
| stdout, stderr  | usb serial and sdcard log | pc terminal      |
| stdin           | always EOF                | pc terminal      |
| time()          | arm can tool RTC          | pc clock         |
| system()        | rt-thread msh shell       | pc shell command |
| runs without pc | yes                       | no               |

⚠️ **Warning — security:** In GDB Server mode, a semihosting program can read or overwrite files on the SD card or in SPI flash, execute rt-thread console commands, and modify arm can tool settings. In CMSIS-DAP mode, a semihosting program can read or overwrite any file, and execute any program, on the PC.

### CMSIS-DAP Mode

Using arm can tool in CMSIS-DAP mode, together with [OpenOCD](https://xpack-dev-tools.github.io/openocd-xpack/docs/user/#using-openocd-in-testing) on PC. Files are created on the PC where OpenOCD runs. stdin, stdout and stderr connect to the OpenOCD console. `time()` returns pc time. `system()` runs a shell command on the pc. See OpenOCD [arm semihosting](https://openocd.org/doc/html/Architecture-and-Core-Commands.html) commands.

**Example:**

Set arm can tool in CMSIS-DAP mode. Start openocd.

```
./bin/openocd -f openocd/scripts/interface/cmsis-dap.cfg -f openocd/scripts/target/stm32f4x.cfg  -c "init" -c "arm semihosting enable"
...
Info : [stm32f4x.cpu] Cortex-M4 r0p1 processor detected
Info : Listening on port 3333 for gdb connections
semihosting is enabled
```

Using arduino, compile and run `tools/Arduino/Hello`. Open a gdb session in another window:

```
$ arm-none-eabi-gdb
(gdb) tar ext :3333
Remote debugging using :3333
(gdb) file ~/Arduino/Hello/build/STMicroelectronics.stm32.GenF4/Hello.ino.elf
(gdb) lo
(gdb) compare-sections
(gdb) r
Starting program:
```

Verification: `Hello from ARM CAN Tool!` appears in the window where openocd runs.

⚠️ **Warning — security:** Only enable semihosting for trusted programs. When enabled, a target program can open any file and execute any program on the PC.

### GDB Server Mode

In gdb server mode, arm can tool executes semihosting calls.

Always enabled are terminal output (writec, write0) and time (clock, time, elapsed). stdout and stderr are written to usb serial cdc0 `/dev/ttyBmpTarg`, and to sd card if `Startup->logging` is enabled. stdin always returns EOF.

File input/output is disabled by default. To enable, switch on `Target -> file i/o`. Filesystems are `/sdcard` for sdcard, `/flash` for 16 Mbyte SPI flash. Working directory is sdcard.

|filename|file system|
|---|---|
|/sdcard/output.csv|sdcard |
|/flash/output.csv|SPI flash |
|output.csv|sdcard (same as /sdcard/output.csv)|

⚠️ **Warning — security:** Only enable file i/o for trusted programs. File paths are not confined to a working directory. When enabled, a target program can open any file on the arm can tool.

System commands are disabled by default. To enable, switch on `Target -> shell command`. Commands are executed using rt-thread msh. Example: system("sync").

⚠️ **Warning — security:** Only enable shell command for trusted programs. When enabled, a target program can execute any rt-thread console command.

**Example:**

Set arm can tool in gdb server mode. Connect a terminal emulator to `/dev/ttyBmpTarg`.

Using arduino, compile and run `tools/Arduino/Hello`:

```
 $ arm-none-eabi-gdb
(gdb) tar ext /dev/ttyBmpGdb
Remote debugging using /dev/ttyBmpGdb
(gdb) mon swd
Target voltage: 3.336V
Available Targets:
No. Att Driver
 1      STM32F412 M4
(gdb) at 1
(gdb) file ~/Arduino/Hello/build/STMicroelectronics.stm32.GenF4/Hello.ino.elf
(gdb) lo
(gdb) compare-sections
(gdb) r
Starting program:
```

Verification: `Hello from ARM CAN Tool!` appears on `/dev/ttyBmpTarg`.

## Online Book

A practical guide to Black Magic Debug:
[Black Magic Debug Book](https://github.com/compuphase/Black-Magic-Probe-Book/releases)

