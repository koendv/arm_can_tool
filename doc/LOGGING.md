# ARM CAN TOOL — Logging

SD card logging of all active output channels.

---

## Prerequisites

- SD card, FAT32 or exFAT format.

---

## Enable Logging

- Insert the SD card into the ARM CAN Tool.
- Navigate to Startup and enable logging: Startup → logging
- Navigate to Settings → Store.
- Reboot.

Verification: All active output channels are recorded to the SD card.

---

## Log Files

| File | Contents |
|------|----------|
| `/sdcard/rtthread.log` | rt-thread system log. |
| `/sdcard/xxxxxxxx.log` | Filename `xxxxxxxx` is a number of seconds since January 1, 1970, in hex. Debug output (RTT, SWO, serial, memwatch, CAN log).

rt-thread log rotates to `rtthread.log.1` when maximum size (ULOG_FILE_SIZE, 1 Mbyte) is reached.

Debug log is changes to a new file when maximum size (LOG_SIZE_MAX, 4 MByte) is reached.

---

## Shell Commands

| Command | Description |
|---------|-------------|
| `ulog <message>` | Append a message to the rt-thread system log. |
| `ulog -c` | Suppress rt-thread log output to console. File logging continues. |
| `logger <message>` | Append a message to the debug log. |
| `logger -f` | Flush the debug log from RAM buffers to SD card. |

Example:

```
msh />logger hello, world!
msh />logger -f
```

Expected output:

```
I/LOG: flush
```

Verification: The message appears in `/sdcard/xxxxxxxx.log`.

From the rt-thread console prompt:

```
msh />ls /sdcard/
Directory /sdcard/:
6a41eadf.log         5409
rtthread.log         158
msh />tail /sdcard/6a41eadf.log
```

---

## Combined Log Stream

The following sources contribute to CDC serial 0 and to the debug log file:

- serial0, serial1, serial2 output
- SWO decoded output
- RTT output (GDB Server and Lua Script mode)
- memwatch output (GDB Server and Lua Script mode)
- CAN bus frames in candump format (when Canbus → logging is enabled)

The following do **not** appear in the combined log stream:

- GDB protocol traffic on CDC serial 1
- SLCAN protocol traffic on CDC serial 1 in CMSIS-DAP mode

---

## Retrieve Logs

- Navigate to Mode → MASS STORAGE.
- Choose Mode → OK. The SD card appears as a USB mass storage device on the host PC.
- Copy the desired files from the USB device to the host PC.
- In the PC, eject the mass storage device.
- In the arm can tool, navigate to another operating mode.

Verification: Log files are present on the host PC.

---

## Log Format

Each line in the debug log file is a plain text string.
