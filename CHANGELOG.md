# Changelog

## V1.3 — 2026-08-30

- New: Black Magic Debug updated to v2.1.0
- New: GDB NoAckMode advertised by default
- Changed: Docker build runs as non-root user

## V1.2 — 2026-08-26

- New: ARM MTB (Micro Trace Buffer) instruction trace (`mon mtb`, `tools/mtb/mtb.py`)

## V1.1 — 2026-08-08

- New: ARM DWT program counter and exception trace (`swo top` / `swo graph`)
- Fix: AT32 SPI driver busy-waited on DMA transfer completion

## V1.0 — 2026-07-17

Initial release.
