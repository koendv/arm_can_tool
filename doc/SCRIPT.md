# ARM CAN TOOL — Lua Scripting

> Add a feature once, support forever.

Scripting automates repetitive tasks.

Lua scripting is event-driven and runs from flash.

- *event driven:* When an event occurs, Lua briefly runs the registered event handler, then returns to sleep until the next event. Avoid busy waiting.
- *run from flash:* The Lua bytecode for the event handlers runs from processor internal flash. Store user functions in flash, avoid using ram.
- *runs untethered:* Once stored in flash and autoexec is enabled, event handlers execute with no PC connected.

RAM and CPU are scarce resources - allocate them wisely.

Adding features increases resource usage. For long-lived products, aim for no more than 50% RAM, CPU and flash use at first release.

## Lua Shell

Set Mode -> Lua Script. Reboot.

Connect a terminal emulator to CDC serial 1 (`/dev/ttyBmpLua`). Prompt is `lua>`. CDC serial 0 (`/dev/ttyBmpUser`) is available for script I/O.

The shell is single-line expressions only. Line length maximum 80 characters. Command history is 128 characters. Lua integers and floats are 32-bit.

Object-method syntax is not available. Use `string.upper(s)`, not `s:upper()`.

Implicit string-to-number conversion is disabled. `"10" + 5` produces a runtime error. Use `tonumber()` for explicit conversion.

Tab completion lists the first 8 candidates, globals and first level of table members only.

A line beginning with `=` is shorthand for `return`:

```
lua> =1+1
2
lua> =sys.mem_free()
24840
```

## Lua Libraries

Lua libraries are ro-tables (read only tables) and stored in 16 Mbyte QSPI flash. The ro-tables are limited to c functions and integer constants. Float constants and strings in libraries are zero-argument functions.

| Library | Description |Note|
|---------|-------------|---|
| bmd | Black Magic Debug | target memory, target flash, registers, breakpoints, halt/resume |
| dap | CMSIS-DAP |  |
| can | CAN bus | transmit, receive, speed |
| sys | System functions | memory info, I/O, events |
| flash | Internal flash | store and execute user functions |
| base | Standard Lua library | `_VERSION()` is a zero-argument function |
| coroutine | Standard Lua library | |
| math | Standard Lua library | Float constants are zero-argument functions. (`math.pi()`, `math.huge()`, etc.)  |
| string | Standard Lua library | |
| table | Standard Lua library | |
| utf8 | Standard Lua library | `utf8.charpattern()` is a zero-argument function. |

What is not used does not get fixed.

## Lua System Library

| Function | Description |
|----------|-------------|
| `sys.mem_total()` | Returns total heap memory size in bytes. |
| `sys.mem_used()` | Returns currently used heap memory in bytes. |
| `sys.mem_free()` | Returns free heap memory in bytes. |
| `sys.mem_max_used()` | Returns peak (maximum) heap memory used in bytes. |
| `sys.write(string)` | Write a string to usb serial port. |
| `sys.log(string)` | Log a message to the system logger. |
| `sys.load(filename)` | Load and execute Lua bytecode or source from SD card file. Returns true on success or (nil, error_string) on failure. |
| `sys.dump(func, filename)` | Compile function to bytecode and output as hex dump to serial terminal. Output can be piped to `xxd -r` to recreate the binary. Returns true or (nil, error_string). |
| `sys.eeprom_write(address, string)` | Write data to settings EEPROM at specified address. Returns bytes written or (nil, error_string) on failure. |
| `sys.eeprom_read(address, length)` | Read data from settings EEPROM at specified address and length. Returns string or (nil, error_string) on failure. |
| `sys.serial0_receive(count)` | Read up to `count` bytes from serial0; returns empty string if no data available. |
| `sys.serial1_receive(count)` | Read up to `count` bytes from serial1 |
| `sys.serial0_write(string)` | Write a string to serial0 |
| `sys.serial1_write(string)` | Write a string to serial1 |

**Event Constants**: The library also defines event flag constants (e.g., `sys.EVENT_CAN1_TX_DONE`, `sys.EVENT_CDC1_RX`, `sys.EVENT_TARGET_HALTED`) used with event handlers. See [events](LUA_REF.md#event-constants).

The difference between `print()` and `sys.write()` is that `print()` writes to the Lua console `/dev/ttyBmpLua`. `sys.write()` writes to the user serial `/dev/ttyBmpUser`, and from there to SD card if logging is set.

## Lua Flash Store

Lua compiles user functions to bytecode. Bytecode is stored in ram or flash.

The AT32F405 has 256 kbyte internal flash. The upper 128 kbyte of internal flash is used to store Lua user functions, in 64 sectors of 2 kbyte each. Sector 0 is the autoexec function. A function whose bytecode exceeds 2 kB occupies consecutive sectors automatically. The user is responsible for sector allocation. Leave a gap of one or more sectors between functions to allow for growth.

## Lua Flash Library

| Function | Description |
|----------|-------------|
| `flash.exec(sector [, args...])` | Load and execute bytecode from flash sector. Extra arguments are passed to bytecode as varargs (`...`). Returns execution results. |
| `flash.load(sector)` | Load bytecode from sector and return the chunk as a callable Lua function, without executing it. Calling the returned chunk runs the stored code's top level — which may itself return a function, depending on how the script was written. Uses XIP mode to keep instructions in flash. |
| `flash.autoexec()` | Internal function that loads and executes bytecode from sector 0 automatically. Logs success or error to system console. |
| `flash.write(func, sector [, name])` | Write a Lua function's compiled bytecode to flash at specified sector. Returns success flag, bytecode size (bytes), and number of sectors used. |
| `flash.erase(sector)` | Erase a single flash sector. Returns success flag. |
| `flash.eraseall()` | Erase all LFS sectors. Returns success flag. |
| `flash.list()` | Display all LFS sectors, their sizes, function names, and free space. |
| `flash.receive()` | Receive bytecode from serial port and write to flash. |

The difference between `flash.exec(0)` and `flash.autoexec()` is that `autoexec()` logs errors to the system console, `exec()` logs errors to usb serial. Lua autoexec may occur before usb startup.

The difference between `sys.load()` and `flash.load()` is that `sys.load()` loads source from file; `flash.load()` loads bytecode from flash.

## Compiling User Functions

Clear all stored functions:

```
lua> flash.eraseall()
lua> flash.list()
64/64 free
```

Compile a user function at the lua prompt:

```
lua> function hello() print("hello, world!") end
lua> hello()
hello, world!
```

Display the bytecode:

```
lua> sys.dump(hello, "hello")
xxd -r << 'EOF' > hello
00000000: 1b4c 7561 5500 1993 0d0a 1a0a 0488 a9ff  .LuaU...........
(output truncated)
EOF
```

Store the function in processor flash:

```
lua> flash.write(hello, 1, "hello")
lua> flash.list()
0x01     92  hello
63/64 free
```

Execute the function from flash:

```
lua> flash.exec(1)
hello, world!
```

**Verification:** Turn arm can tool off and on. `flash.exec(1)` prints "hello, world!".

## Compiling from File

Change arm can tool mode to "Mass Storage". In the usb filesystem, create the following file:

```
$ more mem.lua
function show_memory()
    print("ram: " .. tostring(sys.mem_used()) .. "/" .. tostring(sys.mem_total()) .. " bytes")
end
```

Carefully eject the usb file system.

Change arm can tool mode back to "Lua Script". At the lua prompt, type:

```
lua> sys.load("/sdcard/mem.lua")
lua> show_memory()
ram: 17688/32768 bytes
lua> flash.write(show_memory, 2, "show_memory")
lua> flash.list()
0x01     92  hello
0x02    189  show_memory
62/64 free
lua> flash.exec(2)
ram: 14120/32768 bytes
```

## Cross-Compiling

Go to `arm_can_tool/tools/lua32`.

On the PC, create a file:

```
$ more hello.lua
print("hello, world!")
```

Write executable statements at the top level of the file, not wrapped in `function ... end`.
`flash.exec(sector)` and `flash.load(sector)()` load the bytecode as a chunk and call the chunk once; only the chunk's top-level statements run on that call.

A file written as `function hello() ... end` or `return function() ... end` just builds a closure and hands it back without running it.
`flash.exec()` then silently discards that returned function, producing no output and no error.

Compile:

```
$ luac32 -s -o hello.luac hello.lua
```
`luac32` is luac, same lua version as embedded, compiled with LUA_32BITS.

At the lua prompt, type:

```
lua> flash.receive()
Waiting for data
```

Close the terminal.


On the PC, type:

```
$ lfs_send.py -p /dev/ttyBmpLua -s 3 hello.luac
  hello.luac: 96 bytes, 1 sector(s), base sector 3, name 'hello'
Sending 1 sector(s) via /dev/ttyBmpLua
  sector   3 [last] 2058 framed bytes ... ACK
Transfer complete.
```

**Consequence of not closing the terminal**: the ACK is sent to the terminal, and not to `lfs_send.py`. Transfer does not complete.

Open the lua console `/dev/ttyBmpLua` again. At the lua prompt, type:

```
lua> flash.list()
0x01     92  hello
0x02    195  show_memory
0x03     96  hello
61/64 free
lua> flash.exec(3)
hello, world!
```

⚠️ **Warning — 32-bit bytecode required.** 64-bit `luac` produces incompatible bytecode. Error message when using 64-bit `luac`: `luac: bad binary format (version mismatch)`

### Cross-Compiling a Function with Arguments

`flash.exec(sector, args...)` passes any arguments after the sector number into the chunk as varargs (...). Example: function to add two numbers.

On PC:

```
$ more add.lua
local a, b = ...
print(a + b)
$ luac32 -s -o add.luac add.lua
```

On arm can tool:

```
lua> flash.receive()
Waiting for data
```
Exit terminal.
On PC:

```
$ lfs_send.py -p /dev/ttyBmpLua -s 4 add.luac
```

On arm can tool:

```
lua> flash.list()
0x01     92  hello
0x02    195  show_memory
0x03     96  hello
0x04     88  add
60/64 free
lua> flash.exec(4, 2, 3)
5
```

### Trading RAM For CPU

`flash.exec(sector)` and `flash.load(sector)` re-parse the chunk's bytecode on every call, allocating a fresh `Proto`, constants, and closure in RAM. RAM usage spikes during the call and is freed by garbage collection afterward. Default for infrequently-called handlers — no RAM held between calls.

For a frequently-called handler, the repeated parse-and-allocate is the dominant cost. Load the chunk once, keep a reference to the resulting function, and call the function directly on later calls:

On PC:

```
$ more add2.lua
return function(a, b)
    return a + b
end
$ luac32 -s -o add2.luac add2.lua
```

On arm can tool:

```
lua> flash.receive()
Waiting for data
```

On PC:

```
$ lfs_send.py -p /dev/ttyBmpLua -s 5 add2.luac
```

Calling the chunk once returns the inner function:

```
lua> add2=flash.exec(5)
lua> =add2(1, 1)
2
lua> =add2(2, 3)
5
```

CPU cost per call is lower. RAM usage is higher — the function is held in RAM between calls.

## Lua Events

Lua reacts to hardware or serial activity autonomously, using event handlers.
An event handler is a Lua function that is executed when an event occurs.
Examples of events are:

- CAN bus frame received
- CAN bus off
- target program halts

Lua wakes up when an event occurs. The corresponding event handler is executed. Then Lua goes back to sleep.
There is no polling loop to write.

### Registering Event Handlers


Event handlers are registered once, at boot, from flash sector 0, by defining a global table named `event_handler`.

Pattern: `event_handler[sys.EVENT_X] = flash.load(N)`

The index of `event_handler` is an integer constant in the `sys` library (`sys.EVENT_CAN1_RX0_INDIC`, `sys.EVENT_CDC1_RX`, etc.)
The value of `event_handler` is a function taking no arguments and returning nothing.

To use event handlers:

- Store a script in flash sector 0 that sets `event_handler` table.
- Set Startup menu → lua autoexec → on
- Store settings: Settings → Store.
- Reset arm can tool.

**Registering event handlers only happens once, at boot, from sector 0.**

Setting or changing `event_handler` from the interactive shell after boot does not change event handlers.

An event handler stored in flash is written the same way as any other cross-compiled script: statements at the top level, not wrapped in `function ... end`.

### Demo: CAN Bus Logging

Three pieces:

- an event handler that prints received CAN frames
- an event handler that reports a bus-off condition.
- autoexec script that registers the two event handlers, and a function to switch logging on and off.

**Sector 0 — autoexec.**

On PC:

```
$ more autoexec.lua
can_logging = true

function toggle()
    can_logging = not can_logging
    print(can_logging and "on" or "off")
end

event_handler = {}
event_handler[sys.EVENT_CAN1_RX0_INDIC] = flash.load(1)
event_handler[sys.EVENT_CAN1_BUS_OFF]   = flash.load(2)
```

```
$ luac32 -s -o autoexec.luac autoexec.lua
```

**Sector 1 — CAN frame handler.**

Runs on `EVENT_CAN1_RX0_INDIC`.
Drains the receive ring buffer unconditionally, even when logging is off.

On PC:

```
$ more can_log.lua
while true do
    local id, data, id_type, frame_type, ts = can.receive()
    if not id then break end
    if can_logging then
        local hex = string.format(string.rep("%02X ", #data), string.byte(data, 1, #data))
        sys.write(string.format("CAN %s id=0x%X dlc=%d frame=%s ts=%d\r\n",
            id_type == can.ID_EXT and "ext" or "std", id, #data, hex, ts))
    end
end
```

```
$ luac32 -s -o can_log.luac can_log.lua
```

**Sector 2 — bus-off handler.**

Runs on `EVENT_CAN1_BUS_OFF`.

On PC:

```
$ more can_busoff.lua
sys.write("CAN1 bus off\r\n")
```

```
$ luac32 -s -o can_busoff.luac can_busoff.lua
```

On arm can tool:

```
lua> flash.eraseall()
lua> flash.receive()
Waiting for data
```

On PC:

```
$  ./lfs_send.py -p /dev/ttyBmpLua -s 0 autoexec.luac -s 1 can_log.luac -s 2 can_busoff.luac
  autoexec.luac: 348 bytes, 1 sector(s), base sector 0, name 'autoexec'
  can_log.luac: 279 bytes, 1 sector(s), base sector 1, name 'can_log'
  can_busoff.luac: 95 bytes, 1 sector(s), base sector 2, name 'can_busoff'
Sending 3 sector(s) via /dev/ttyBmpLua
  sector   0 [cont] 2058 framed bytes ... ACK
  sector   1 [cont] 2058 framed bytes ... ACK
  sector   2 [last] 2058 framed bytes ... ACK
Transfer complete.
```

Note sending multiple sectors in one command.

On arm can tool:

```
lua> flash.list()
0x00    348  autoexec
0x01    381  can_log
0x02    107  can_busoff
61/64 free
```

Enable `lua autoexec`:

- Startup → CAN bus
- Startup → lua autoexec
- Settings → Store
- reboot

Console boot log:

```
I/LUA: heap 32 kbyte
I/LUA: autoexec
I/LUA: 2 event handlers
```

Connect to user terminal /dev/ttyBmpUser.

CAN traffic on the bus prints one line per frame.

```
CAN std id=0x0 dlc=4 frame=12 34 56 78  ts=17492945
CAN std id=0x0 dlc=4 frame=12 34 56 78  ts=15137684
```

Connect to lua console /dev/ttyBmpLua.

Switch logging on/off:

```
lua> toggle()
off
lua> toggle()
on
```

---

## Standalone Logging

Once logging is enabled and settings stored, the tool continues logging with
no PC attached. ARM CAN Tool needs only power, not a live usb connection.

- Startup → logging → on
- Settings → Store
- Disconnect USB from PC; connect USB to a USB charger
- Logging continues, writing to the SD card

**Verification:** Reconnect to a PC after 5 minutes unattached. Set Mode -> Mass Storage. `xxxxxxxx.log` contains CAN bus log.

---
