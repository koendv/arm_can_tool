# ARM CAN TOOL — Lua Library Reference

---

## `bmd` — Black Magic Debug (SWD target control)

> `nil, err` if not attached or on error · `true` on success

### Connection

| Function | Returns | Description |
|---|---|---|
| `bmd.attach()` | `true, err` | SWD scan and attach to first target |
| `bmd.reset()` | `true, err` | Reset the attached target |
| `bmd.target_running()` | `boolean` | True if target is currently executing |

### Halt / Resume

| Function | Returns | Description |
|---|---|---|
| `bmd.halt_request()` | `true, err` | Request target halt |
| `bmd.halt_poll()` | `true, reason, watch` | Poll halt status; `reason`=int, `watch`=addr |
| `bmd.halt_resume()` | `true, err` | Resume target execution |

### Flash

| Function | Returns | Description |
|---|---|---|
| `bmd.flash_mass_erase()` | `true, err` | Erase entire target flash |
| `bmd.flash_erase(addr, len)` | `true, err` | Erase flash region |
| `bmd.flash_write(addr, data)` | `true, err` | Write binary string to flash (erase first) |
| `bmd.flash_complete()` | `true, err` | Flush pending flash write buffers |

### Memory

| Function | Returns | Description |
|---|---|---|
| `bmd.mem32_read(addr, len)` | `data, err` | Read `len` bytes from target RAM |
| `bmd.mem32_write(addr, data)` | `true, err` | Write binary string to target RAM |

### Registers

| Function | Returns | Description |
|---|---|---|
| `bmd.regs_read()` | `blob, err` | Read all registers as binary blob |
| `bmd.regs_write(blob)` | `true, err` | Write all registers from binary blob |
| `bmd.reg_read(reg)` | `bytes, err` | Read single register by index (4 or 8 bytes) |
| `bmd.reg_write(reg, data)` | `true, err` | Write single register by index |

### Breakpoints / Watchpoints

| Function | Returns | Description |
|---|---|---|
| `bmd.breakwatch_set(t, a, l)` | `true, err` | Set breakpoint or watchpoint |
| `bmd.breakwatch_clear(t, a, l)` | `true, err` | Clear breakpoint or watchpoint |

---

## `can` — AT32F405 CAN1 Peripheral

> Errors return `nil` or `nil, err`

### Functions

| Function | Returns | Description |
|---|---|---|
| `can.init(enable)` | `true, err` |Initialise CAN; `true` = bus active, `false` = frozen |
| `can.speed(freq_hz)` | `true, err` | Set CAN bitrate e.g. `500000` |
| `can.transmit(id, data, id_t, frame_t)` | `mailbox (0–2)` or `nil` | Queue frame; `data` ≤ 8 bytes; `nil` = no mailbox |
| `can.receive()` | `id, data, id_t, frame_t, ts` | Pop frame from RX ring; `nil` if empty |
| `can.tx_state()` | `s0, ts0, s1, ts1, s2, ts2` | Status + µs timestamp per TX mailbox |

### Constants

| Constant | Value | Description |
|---|---|---|
| `ID_STD` | `0` | Standard 11-bit ID |
| `ID_EXT` | `1` | Extended 29-bit ID |
| `FRAME_DATA` | `0` | Data frame |
| `FRAME_RTR` | `1` | Remote request frame |
| `TX_MAILBOX0` | `0` | Mailbox 0 |
| `TX_MAILBOX1` | `1` | Mailbox 1 |
| `TX_MAILBOX2` | `2` | Mailbox 2 |
| `TX_FAILED` | `0` | Transmission failed |
| `TX_OK` | `1` | Transmission successful |
| `TX_PENDING` | `2` | Transmission pending |
| `TX_NO_EMPTY` | `4` | No mailbox free |

---

## `flash` — Lua Flash Storage (2 KB sectors, XIP)

> Errors return `nil, err`

### Read / Execute

| Function | Returns | Description |
|---|---|---|
| `flash.exec(sector [, ...])` | `chunk returns, err` | Load + execute bytecode; extra args = varargs |
| `flash.load(sector)` | `function, err` | Load bytecode as function, do not execute |
| `flash.autoexec()` | *(nothing)* | Execute sector 0; errors logged, not raised |

### Write

| Function | Returns | Description |
|---|---|---|
| `flash.write(func, sector [, name])` | `true, byte_size, n_sectors` | Dump + write bytecode; erases sectors first |
| `flash.receive()` | `true` | Begin serial bytecode receive to flash (CDC1) |

### Erase / List

| Function | Returns | Description |
|---|---|---|
| `flash.erase(sector)` | `true, err` | Erase single sector |
| `flash.eraseall()` | `true, err` | Erase entire LFS region |
| `flash.list()` | *(prints to CDC1)* | Print sector index, size, name; free space |

---

## `dap` — CMSIS-DAP Raw HID Layer

| Function | Returns | Description |
|---|---|---|
| `dap.init()` | *(nothing)* | Initialise DAP layer |
| `dap.process_request(req)` | `response string` | `req` must be 64 bytes; returns response bytes |

---

## `sys` — System Utilities (memory · I/O · files · EEPROM · events)

> Errors return `nil, err`

### Memory

| Function | Returns | Description |
|---|---|---|
| `sys.mem_total()` | `bytes` | Total Lua heap size |
| `sys.mem_used()` | `bytes` | Current heap usage |
| `sys.mem_free()` | `bytes` | Free heap bytes |
| `sys.mem_max_used()` | `bytes` | High-water mark since boot |

### I/O

| Function | Returns | Description |
|---|---|---|
| `sys.write(s)` | *(nothing)* | Write string to CDC0 |
| `sys.log(s)` | *(nothing)* | INFO log via RTdbg (debug console) |

### Files (SD Card)

| Function | Returns | Description |
|---|---|---|
| `sys.load(filename)` | `true, err` | Load + exec Lua source or bytecode from SD |
| `sys.dump(func, filename)` | `true, err` | Print bytecode as xxd heredoc |

### EEPROM (24C64)

| Function | Returns | Description |
|---|---|---|
| `sys.eeprom_write(addr, data)` | `bytes_written` | Write binary string to EEPROM |
| `sys.eeprom_read(addr, len)` | `string, err` | Read bytes from EEPROM |

### Serial

| Function | Returns | Description |
|---|---|---|
| `sys.serial0_receive(count)` | `string` | Read up to `count` bytes from serial0 |
| `sys.serial1_receive(count)` | `string` | Read up to `count` bytes from serial1 |
| `sys.serial0_write(s)` | *(nothing)* | Write string to serial0 |
| `sys.serial1_write(s)` | *(nothing)* | Write string to serial1 |

### Timer

Use the timer to avoid busy waiting.

| Function | Returns | Description |
|---|---|---|
| `sys.timer_oneshot(delay_ms)` | `true, err` | Send `EVENT_TIMER` once, after `delay_ms` |
| `sys.timer_periodic(period_ms)` | `true, err` | Send `EVENT_TIMER` every `period_ms` |
| `sys.timer_cancel()` | *(nothing)* | Stop timer |

### Event Constants

| Constant | Description |
|---|---|
| `EVENT_CAN1_TX_DONE` | CAN1 frame transmitted |
| `EVENT_CAN1_RX0_INDIC` | CAN1 frame received |
| `EVENT_CAN1_BUS_OFF` | CAN1 bus-off condition |
| `EVENT_CAN1_RX_OVERFLOW` | CAN1 RX ring overflow |
| `EVENT_CAN1_TX_OVERFLOW` | CAN1 TX overflow |
| `EVENT_GSUSB_BULK_OUT` | GS_USB bulk-out packet |
| `EVENT_GSUSB_STOP` | GS_USB interface stopped |
| `EVENT_GSUSB_START` | GS_USB interface started |
| `EVENT_GSUSB_TX_DONE` | GS_USB frame transmitted |
| `EVENT_CDC0_DTR` | CDC0 DTR line changed |
| `EVENT_CDC1_DTR` | CDC1 DTR line changed |
| `EVENT_CDC0_RX` | CDC0 data received |
| `EVENT_CDC1_RX` | CDC1 data received |
| `EVENT_SERIAL0_RX` | UART0 data received |
| `EVENT_SERIAL1_RX` | UART1 data received |
| `EVENT_SERIAL2_RX` | UART2 data received |
| `EVENT_TARGET_HALT_REQUEST` | Debug target halt requested |
| `EVENT_TARGET_HALTED` | Debug target halted |
| `EVENT_TIMER` | Lua timer expired |
