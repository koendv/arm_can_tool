#!/usr/bin/env python3
"""
lfs_send.py — send pre-compiled Lua bytecode to the ARM CAN Tool flash loader.

Usage:
    lfs_send.py -p <port> -s <sector> <file.luac> [-s <sector> <file.luac>] ...

Options:
    -p <port>               Serial port (e.g. /dev/ttyACM1, COM3)
    -s <sector> <file>      Write <file.luac> starting at <sector> (0..127)
                            May be repeated for multiple files.

The flash name is derived from the filename stem (init.luac -> "init").

Examples:
    lfs_send.py -p /dev/ttyACM1 -s 0 init.luac
    lfs_send.py -p /dev/ttyACM1 -s 0 init.luac -s 4 handler.luac

On the embedded side, type flash.receive() before running this tool.

Protocol (see lua_flash.h, lua_flash_additions.h):
    One HDLC frame per 1KB flash sector.
    Frame payload (2056 bytes):
        uint8_t  version    — always 1
        uint8_t  command    — CMD_WRITE (0x01) | CMD_CONTINUATION (0x80) if more follow
        uint16_t sector     — destination sector, little-endian
        uint8_t  data[2048] — raw sector content
        uint32_t crc32      — IEEE 802.3 CRC over preceding 2052 bytes, little-endian
    Sector 0 of each file: data = lfs_header_t (24 bytes) + bytecode, zero-padded.
    Subsequent sectors: data = bytecode continuation, zero-padded.
    HDLC: 0x7E frame flag, 0x7D escape, escaped byte XOR 0x20.
    After each frame: wait for ACK (0x06) or NAK (0x15).

Dependencies:
    pip install pyserial
"""

import sys
import struct
import serial
import time
import zlib
import os

# -----------------------------------------------------------------------
# Constants — from lua_flash.h and lua_flash_additions.h
# -----------------------------------------------------------------------

LFS_MAGIC        = 0x4C465300   # "LFS\0"  — lua_flash.h
LFS_HEADER_SIZE  = 24           # sizeof(lfs_header_t) — lua_flash.h
LFS_NAME_SIZE    = 16           # 15 chars + NUL — lua_flash.h
LFS_NUM_SECTORS  = 64           # lua_flash.h
LFS_SECTOR_SIZE  = 2048         # lua_flash.h

PROTO_VERSION    = 1            # lua_flash_additions.h
CMD_WRITE        = 0x01         # lua_flash_additions.h
CMD_CONTINUATION = 0x80         # lua_flash_additions.h

HDLC_FLAG        = 0x7E         # lua_flash_additions.h
HDLC_ESCAPE      = 0x7D         # lua_flash_additions.h
HDLC_ESCAPE_XOR  = 0x20         # lua_flash_additions.h

ACK              = 0x06         # lua_flash_additions.h
NAK              = 0x15         # lua_flash_additions.h

PACKET_CRC_LEN   = 1 + 1 + 2 + LFS_SECTOR_SIZE     # 2052
PACKET_SIZE      = PACKET_CRC_LEN + 4              # 2056

ACK_TIMEOUT_S    = 5.0

# -----------------------------------------------------------------------
# CRC-32 — zlib uses IEEE 802.3, matches embedded crc32_update()
# -----------------------------------------------------------------------

def crc32(data: bytes) -> int:
    return zlib.crc32(data) & 0xFFFFFFFF

# -----------------------------------------------------------------------
# HDLC
# -----------------------------------------------------------------------

def hdlc_stuff(data: bytes) -> bytes:
    out = bytearray()
    for b in data:
        if b == HDLC_FLAG or b == HDLC_ESCAPE:
            out.append(HDLC_ESCAPE)
            out.append(b ^ HDLC_ESCAPE_XOR)
        else:
            out.append(b)
    return bytes(out)

def hdlc_frame(payload: bytes) -> bytes:
    return bytes([HDLC_FLAG]) + hdlc_stuff(payload) + bytes([HDLC_FLAG])

# -----------------------------------------------------------------------
# lfs_header_t
# -----------------------------------------------------------------------

def build_lfs_header(bc_size: int, name: str) -> bytes:
    name_bytes = name.encode('ascii', errors='replace')[:LFS_NAME_SIZE - 1]
    name_bytes = name_bytes + b'\x00' * (LFS_NAME_SIZE - len(name_bytes))
    return struct.pack('<II', LFS_MAGIC, bc_size) + name_bytes

# -----------------------------------------------------------------------
# Packet builder
# -----------------------------------------------------------------------

def build_packet(sector: int, data: bytes, continuation: bool) -> bytes:
    assert len(data) == LFS_SECTOR_SIZE
    assert 0 <= sector < LFS_NUM_SECTORS
    command        = CMD_WRITE | (CMD_CONTINUATION if continuation else 0)
    payload_no_crc = struct.pack('<BBH', PROTO_VERSION, command, sector) + data
    assert len(payload_no_crc) == PACKET_CRC_LEN
    return payload_no_crc + struct.pack('<I', crc32(payload_no_crc))

# -----------------------------------------------------------------------
# File -> list of (sector, data[1024]) pairs
# -----------------------------------------------------------------------

def file_to_sector_data(base_sector: int, bytecode: bytes, name: str) -> list:
    n_sectors = (len(bytecode) + LFS_HEADER_SIZE + LFS_SECTOR_SIZE - 1) // LFS_SECTOR_SIZE

    if base_sector + n_sectors > LFS_NUM_SECTORS:
        raise ValueError(
            f"'{name}' needs {n_sectors} sector(s) from base {base_sector}, "
            f"exceeds LFS_NUM_SECTORS ({LFS_NUM_SECTORS})"
        )

    payload = build_lfs_header(len(bytecode), name) + bytecode

    remainder = len(payload) % LFS_SECTOR_SIZE
    if remainder:
        payload += b'\x00' * (LFS_SECTOR_SIZE - remainder)

    result = []
    for i, offset in enumerate(range(0, len(payload), LFS_SECTOR_SIZE)):
        result.append((base_sector + i, payload[offset:offset + LFS_SECTOR_SIZE]))

    return result

# -----------------------------------------------------------------------
# Transfer
# -----------------------------------------------------------------------

def send_transfers(port_name: str, transfers: list) -> bool:
    all_sectors = []
    for base_sector, bytecode, name in transfers:
        all_sectors.extend(file_to_sector_data(base_sector, bytecode, name))

    total = len(all_sectors)
    print(f"Sending {total} sector(s) via {port_name}")

    port = serial.Serial(port_name, baudrate=115200, timeout=0.1)
    time.sleep(0.1)

    try:
        for idx, (sector, data) in enumerate(all_sectors):
            continuation = idx < total - 1
            packet       = build_packet(sector, data, continuation)
            frame        = hdlc_frame(packet)

            label = "[cont]" if continuation else "[last]"
            print(f"  sector {sector:3d} {label} {len(frame):4d} framed bytes ... ",
                  end='', flush=True)

            port.write(frame)

            deadline = time.monotonic() + ACK_TIMEOUT_S
            result   = None
            while time.monotonic() < deadline:
                b = port.read(1)
                if not b:
                    continue
                if b[0] in (ACK, NAK):
                    result = b[0]
                    break

            if result == ACK:
                print("ACK")
            elif result == NAK:
                print("NAK")
                print(f"Transfer failed at sector {sector}.", file=sys.stderr)
                return False
            else:
                print("TIMEOUT")
                print(f"No response for sector {sector}.", file=sys.stderr)
                return False

    finally:
        port.close()

    print("Transfer complete.")
    return True

# -----------------------------------------------------------------------
# Argument parsing
# -----------------------------------------------------------------------

def usage():
    print("Usage: lfs_send.py -p <port> -s <sector> <file.luac> "
          "[-s <sector> <file.luac>] ...", file=sys.stderr)
    sys.exit(1)

def parse_args(argv: list) -> tuple:
    port_name = None
    transfers = []
    i         = 1

    while i < len(argv):
        opt = argv[i]

        if opt == '-p':
            if i + 1 >= len(argv):
                print("-p requires an argument", file=sys.stderr)
                usage()
            port_name = argv[i + 1]
            i += 2

        elif opt == '-s':
            if i + 2 >= len(argv):
                print("-s requires sector and filename", file=sys.stderr)
                usage()

            try:
                sector = int(argv[i + 1])
            except ValueError:
                print(f"-s: expected sector number, got '{argv[i + 1]}'",
                      file=sys.stderr)
                usage()

            if not (0 <= sector < LFS_NUM_SECTORS):
                print(f"-s: sector {sector} out of range (0..{LFS_NUM_SECTORS - 1})",
                      file=sys.stderr)
                sys.exit(1)

            filename = argv[i + 2]
            i += 3

            try:
                with open(filename, 'rb') as f:
                    bytecode = f.read()
            except OSError as e:
                print(f"Cannot read '{filename}': {e}", file=sys.stderr)
                sys.exit(1)

            if len(bytecode) == 0:
                print(f"'{filename}' is empty", file=sys.stderr)
                sys.exit(1)

            if not bytecode.startswith(b'\x1bLua'):
                print(f"Warning: '{filename}' does not start with Lua bytecode magic",
                      file=sys.stderr)

            name      = os.path.splitext(os.path.basename(filename))[0]
            n_sectors = (len(bytecode) + LFS_HEADER_SIZE + LFS_SECTOR_SIZE - 1) // LFS_SECTOR_SIZE
            print(f"  {filename}: {len(bytecode)} bytes, {n_sectors} sector(s), "
                  f"base sector {sector}, name '{name}'")

            transfers.append((sector, bytecode, name))

        else:
            print(f"Unknown argument '{opt}'", file=sys.stderr)
            usage()

    if port_name is None:
        print("Missing -p <port>", file=sys.stderr)
        usage()

    if not transfers:
        print("At least one -s <sector> <file> required", file=sys.stderr)
        usage()

    return port_name, transfers

# -----------------------------------------------------------------------
# Entry point
# -----------------------------------------------------------------------

if __name__ == '__main__':
    port_name, transfers = parse_args(sys.argv)
    ok = send_transfers(port_name, transfers)
    sys.exit(0 if ok else 1)
