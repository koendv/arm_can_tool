## github

Release source (done, 06 nov 2025).

## oshwlab

Write text to share oshwlab hardware project. Close hardware development.

## SLCAN

Add SLCAN input with CAN bus filters, and setting baud rate. (begun)

## SWD speedup

- focus on SWD, not JTAG.
- Try DMA to GPIO for swdptap_seq_out().

## Black Magic Debug

Split gdb_main in two:
- a callback from usb for running gdb commands. Perhaps use flex to parse gdb fileio?
- a task for polling target state when target is running.
  No way to avoid polling here; need to run SWD to see if target halted, in breakpoint, fault, waiting for semihosting, etc.

End result ought to be a debugger that acts responsably in the operating system: 
- gdb server not polling usb; but waiting for usb callback. Message queue?
- target task does rt_delay() between polls.
- semaphore to avoid gdb server and task accessing target at the same time. Timeout on sempahore in case SWD hangs?

## Target Power Good

- Add target power low and target power high voltage thresholds to menu.
- Display and log if target voltage out of range.
- No polling, use ADC voltage monitoring interrupt.
- Maybe do the same for temperature?

## SWO

More testing. Using rt-thread serials. Do stress tests. If load is too high, run swo_decode() directly on dma buffers. Use code from rt_device_read(), replacing rt_memcpy() with swo_decode().

## Logging

Log in text format or in swo format? Put option in menu to switch between text and swo format?
Logging in SWO format gives each source (rtt, can bus, serial0, serial1, swo) its own channel.
Text format can be read with simple editor; SWO format needs viewer but clearer if different channels output at same time.

