#
# GDB command that dumps the CoreSight Micro Trace Buffer and resolves each
# packet's source/destination address to function/file/line, using the debug
# symbols GDB already has loaded for the running image.
#
# Load from the GDB prompt:
#   source tools/mtb/mtb.py
# or add to .gdbinit. Then, with a target attached:
#   (gdb) mtb
#
# Wraps `mon mtb dump`.
#

import os.path
import re

import gdb

# Tracks cortexm_mtb.c's cortexm_mtb_dump() output format. If that format
# drifts, this falls back to printing raw, unresolved lines.
_PACKET_RE = re.compile(r"^([A-]) 0x([0-9a-f]{8}) ([S-]) 0x([0-9a-f]{8})$")


def _function_at(pc):
    block = gdb.block_for_pc(pc)
    while block is not None and block.function is None:
        block = block.superblock
    return block.function.name if block is not None and block.function is not None else None


def _resolve(pc):
    try:
        func = _function_at(pc)
    except gdb.error:
        func = None
    try:
        sal = gdb.find_pc_line(pc)
    except gdb.error:
        sal = None

    if func and sal is not None and sal.symtab is not None:
        return "%s at %s:%d" % (func, os.path.basename(sal.symtab.filename), sal.line)
    if func:
        return func
    return "??"


class MTBCommand(gdb.Command):
    """Dump the CoreSight Micro Trace Buffer with addresses resolved to symbols."""

    def __init__(self):
        super(MTBCommand, self).__init__("mtb", gdb.COMMAND_USER)

    @staticmethod
    def _flush(text, repeats):
        if text is None:
            return
        if repeats > 0:
            print("%s  (%d times)" % (text, repeats + 1))
        else:
            print(text)

    def invoke(self, arg, from_tty):
        raw = gdb.execute("mon mtb dump", to_string=True)
        prev_text = None
        repeats = 0
        for line in raw.splitlines():
            match = _PACKET_RE.match(line)
            if match is None:
                text = line
            else:
                source_flag, source_hex, dest_flag, dest_hex = match.groups()
                source_addr = int(source_hex, 16)
                dest_addr = int(dest_hex, 16)
                text = "%s 0x%08x %-40s -> %s 0x%08x %s" % (
                    source_flag, source_addr, _resolve(source_addr), dest_flag, dest_addr, _resolve(dest_addr))

            if text == prev_text:
                repeats += 1
                continue

            self._flush(prev_text, repeats)
            prev_text = text
            repeats = 0
        self._flush(prev_text, repeats)


MTBCommand()
