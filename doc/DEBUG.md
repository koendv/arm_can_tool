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

**Verification:** GDB reports `Attaching to Remote target`. Target executes loaded firmware.

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

**Verification:** OpenOCD reports `Listening on port 3333 for gdb connections`. GDB connects and target executes loaded firmware.

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

**Verification:** BMDA reports `Listening on TCP port: 2000`. GDB connects and target executes loaded firmware.

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

**Verification:** pyOCD reports GDB server listening on port 3333. GDB connects and target executes loaded firmware.

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

**Verification:**

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

**Verification:** Decoded SWO output appears in terminal emulator.

Arm ITM uses 32 software channels (0-31) for application debug output, selected with `decode`.

In gdb server:
To switch swo on, serial port speed 1000000, swo channels 1 and 3:

```
(gdb) mon swo enable 1000000 decode 1 3
Channel mask: 00000000000000000000000000001010
```

to switch swo off:

```
(gdb) mon swo disable
Trace disabled
```

---

## DWT

DWT (Data Watchpoint and Trace) prints

- PC (program counter) samples
- exceptions
- data watchpoints
- timestamps

of a running target.

DWT trace and ITM printing use the same SWO output pin.
Because the SWO clock is slower than the MCU clock, it is not possible to send the program counter of every instruction.
Instead, the program counter is sampled. Use the program counter samples for statistical analysis, to see where the MCU spends most of its time.

DWT hardware trace packets (PC samples, exceptions, data watchpoints, timestamps) are separate from ITM packets. DWT trace packets are decoded, regardless of which channels `mon swo decode` selects.

DWT packets are sent over the same output pin as SWO.
Therefore, SWO must be enabled before you can see any DWT output.
First set up SWO, then set up DWT.

- `mon swo` to set up how ARM Can Tool prints.
- `mon dwt` to set up what the target sends.

### monitor swo

`mon swo` configures ARM CAN Tool how to print.

```
(gdb) mon swo [enable [BAUDRATE] [decode [CHANNEL_NR ...]]|disable|log|[top|graph] <low_addr> <high_addr> <bucket_bits> <interval_seconds>|selftest]
```

| Option | Meaning |
| --- | --- |
| enable | switch ITM decoding on |
| disable | switch ITM decoding off |
| BAUDRATE | speed of the serial port |
| decode [CHANNEL_NR ...] | ITM channels to show |
| log | log individual PC trace packets (default)|
| top <low_addr> <high_addr> <bucket_bits> <interval_seconds> | PC trace histogram, text format |
| graph <low_addr> <high_addr> <bucket_bits> <interval_seconds> | PC trace histogram, ANSI terminal |
| selftest | test SWO with known good packets |

The SWO speed is a few megabit/s, and depends on the length of the wire.

| Speed | Comment |
| --- | --- |
| 1 Mbit/s | safe |
| 2 to 2.5 Mbit/s | typical |
| 6.75 Mbit/s | ARM CAN Tool maximum |

When printing over SWO, a channel number has to be specified. Using different channel numbers for different subsystems allows separate logs for separate subsystems without recompiling.

For `top` and `graph`, the command shows

- the 25 most used PC addresses
- between `low_addr` and `high_addr`
- in groups of 2**`bucket_bits`
- every `interval_seconds`.

`top` outputs for logfiles, `graph` outputs for ansi terminal.

Example: 32 kbyte of code, beginning at 0x08000000, in blocks of 512 bytes (9 bit), displaying statistics every every 5 seconds:

```
(gdb) mon swo top 0x08000000 0x08008000 9 5
```

### monitor dwt

`mon dwt` configures the target what to send.

```
(gdb) mon dwt [enable|disable|status|clear|0..31|exception|lts <0..3>|gts <0..3>|timestamp]...
```

| Option | Meaning |
| --- | --- |
| enable | switch DWT trace on |
| disable | switch DWT trace off |
| status | show target DWT registers |
| clear | switch PC, exception, timestamp trace off |
| 0..31 | switch PC trace on, 0 = slowest, 31 = fastest |
| exception | switch exception trace on |
| lts 0..3 | local timestamp clock |
| gts 0..3 | global timestamp source |
| timestamp | switch timestamp trace on |

The slowest PC trace is 1 sample every 16384 cycles, the fastest 1 sample every 64 cycles.
If the number of samples is too high for the SWO speed, some packets will be dropped, and the log shows overflow ("OVF").
For a typical Cortex-M4, `mon dwt enable <rate>` where rate >= 15 requests PC samples faster than the link can carry, and samples are dropped.

### Example

```
$ arm-none-eabi-gdb
(gdb) target extended /dev/ttyBmpGdb
(gdb) monitor swd_scan
Target voltage: 3.326V
Available Targets:
No. Att Driver
 1      STM32F412 M4
```

**Verification:** gdb lists target

```
(gdb) attach 1
(gdb) file ~/Arduino/FiveWay/build/FiveWay.ino.elf
Reading symbols from ~/Arduino/FiveWay/build/FiveWay.ino.elf...
(gdb) load
(gdb) compare-sections
```

**Verification:** gdb outputs: "Section ... matched", firmware flashed ok.

```
(gdb) monitor swo enable 1000000 decode
Channel mask: 11111111111111111111111111111111
(gdb) monitor swo log
(gdb) break main
```

**Verification:** gdb outputs: "Breakpoint ... at ..."

The `mon dwt` command is only available if the processor is Cortex-M, and after `attach`.

```
(gdb) run
Breakpoint 1, main () at ...
(gdb) mon dwt enable 0
(gdb) mon dwt status
DWT_CTRL: 0x400013ff
PC sampling on
ITM_TCR: 0x00000019
trace forwarding to SWO: on

```

**Verification:** gdb outputs include the lines "PC sampling on" and "trace forwarding to SWO: on".

Open a terminal window on /dev/ttyBmpTarg:

```
minicom -D /dev/ttyBmpTarg
```

Continue the target program:

```
(gdb) continue
Continuing.
```

**Verification:** /dev/ttyBmpTarg outputs swo text and program counter trace:

```
serial: 1
swo: 1
PC:0x080054CA
PC:0x08001798
PC:0x080054BC
PC:0x080054BC
PC:0x08003AD6
PC:0x08003AD6
...
```

The output is suitable for saving and postprocessing on pc. Example of postprocessing: Look up function at address.

```
$  arm-none-eabi-addr2line -sfe ~/Arduino/FiveWay/build/FiveWay.ino.elf 0x080054BC 0x08003AD6
delay
wiring_time.c:43 (discriminator 1)
getCurrentMillis
clock.c:52
```
Interrupt the program with ctrl-c, and change DWT output format:

```
Program received signal SIGINT, Interrupt.
(gdb) mon swo top 0x08000000 0x08008000 9 5
(gdb) continue
Continuing.
```

/dev/ttyBmpTarg now outputs PC addresses from 0x08000000 to 0x08008000 only, sorted in blocks of 512byte (9 bit), every 5 seconds:

```
serial: 5
swo: 5
serial: 6
swo: 6
12375 0x08005400
11345 0x08003A00
 6168 0x08001600
    1 0x08000000
```

The output is suitable for logging to SD card.

Each line represents a PC address range of 512 bytes. E.g. 0x08000000 represents addresses from 0x08000000 to 0x080001ff. To set up smaller ranges, e.g. 256 bytes:

```
(gdb) mon swo top 0x08000000 0x08008000 8 5
```

The total number of ranges is limited to 4096: `(high_addr - low_addr)/2**bucket_bits <= 4096`.

Stop target program, and choose output format for ansi terminal:

```
ctrl-c
Program received signal SIGINT, Interrupt.
(gdb) mon swo graph 0x08000000 0x08008000 9 5
(gdb) continue
Continuing.
```

**Verification:** Graph of PC samples

```
   12348   0x08005400                                    █
   11344   0x08003A00                        █
    6191   0x08001600       █
       1   0x08003200                    █
       1   0x08000000   █
serial: 57
swo: 57
serial: 58
swo: 58
...
```

In this graph, the vertical axis is increasing number of samples, the horizontal axis is program counter.

The output is suitable for seeing target status at a glance.

Stop target program, and add tracing exceptions:

```
^C
Program received signal SIGINT, Interrupt.
(gdb) mon dwt enable 0 exception
(gdb) continue
Continuing.
```

The terminal now shows:

```
12309 0x08005400                                        █
11299 0x08003A00                            █
 6171 0x08001600           █
   80 0x08002000               █
 5096 SysTick
   30 IRQ 37
OVERFLOW
serial: 29
swo: 29
serial: 30
swo: 30
serial: 31
swo: 31
```

This display contains:

- PC activity
- exceptions (here: SysTick and interrupt 37)
- OVERFLOW: some packets have been dropped because SWO was too slow.
- console output (serial: ) and SWO output (swo: )

The `mon dwt` command is convenient, but not necessary: a target can also set it's own DWT register from code.

`Settings->Save` also saves SWO settings.
If `Startup->swo` is configured, the SWO settings are restored at startup.
Decoding SWO does not require an SWD connection.

---

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

**Verification:** `Hello from ARM CAN Tool!` appears in the window where openocd runs.

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

**Verification:** `Hello from ARM CAN Tool!` appears on `/dev/ttyBmpTarg`.

---

## Online Book

A practical guide to Black Magic Debug:
[Black Magic Debug Book](https://github.com/compuphase/Black-Magic-Probe-Book/releases)

