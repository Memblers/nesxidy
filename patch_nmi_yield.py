#!/usr/bin/python
"""
patch_nmi_yield.py — Post-build patch to redirect lazyNES _nmiCallback.

LazyNES's NMI handler (___nmi at $C192) calls  JSR _nmiCallback  every
VBlank.  By default _nmiCallback ($C214) is a no-op RTS.  This script
redirects the JSR to our _nmi_yield_hook in WRAM, which sets bit 7 of
nmi_yield — signaling compiled backward-branch stubs to yield.

The patch finds the byte sequence  20 14 C2  (JSR $C214) within the
NMI handler region of the fixed bank (bank 31) and replaces the operand
with the address of _nmi_yield_hook from vicemap.map.

Usage:
    python patch_nmi_yield.py <rom.nes> [vicemap.map]
"""

import sys
import os
import re

# JSR _nmiCallback: opcode 20, lo C2 14 → 20 14 C2
PATTERN = bytes([0x20, 0x14, 0xC2])

# The NMI handler lives in the fixed bank (bank 31) at $C192-$C222.
# In the iNES file: header(16) + 31*16384 + (addr - $C000).
INES_HEADER = 16
BANK_SIZE   = 16384
FIXED_BANK  = 31
NMI_START   = 0xC192
NMI_END     = 0xC222

def rom_offset(bank, cpu_addr):
    """Convert (bank, cpu_addr) to iNES file offset."""
    base = 0xC000 if bank == 31 else 0x8000
    return INES_HEADER + bank * BANK_SIZE + (cpu_addr - base)

def find_symbol(mapfile, name):
    """Parse vicemap.map for a symbol address.  Returns int or None."""
    expr = re.compile(r"al C:([0-9a-fA-F]+) \." + re.escape(name) + r"$")
    with open(mapfile, "r") as f:
        for line in f:
            m = expr.match(line.rstrip())
            if m:
                return int(m.group(1), 16)
    return None

def main():
    if len(sys.argv) < 2:
        print("Usage: python patch_nmi_yield.py <rom.nes> [vicemap.map]")
        sys.exit(1)

    rom_path = sys.argv[1]
    map_path = sys.argv[2] if len(sys.argv) > 2 else "vicemap.map"

    if not os.path.exists(map_path):
        script_dir = os.path.dirname(os.path.abspath(__file__))
        map_path = os.path.join(script_dir, map_path)

    hook_addr = find_symbol(map_path, "_nmi_yield_hook")
    if hook_addr is None:
        print("ERROR: _nmi_yield_hook not found in " + map_path)
        sys.exit(1)

    print(f"_nmi_yield_hook = ${hook_addr:04X}")

    with open(rom_path, "rb") as f:
        data = bytearray(f.read())

    # Search within the NMI handler region only (bank 31, $C192-$C222)
    search_start = rom_offset(FIXED_BANK, NMI_START)
    search_end   = rom_offset(FIXED_BANK, NMI_END)

    idx = data.find(PATTERN, search_start, search_end)
    if idx == -1:
        # Wider search in the full fixed bank as fallback
        fb_start = rom_offset(FIXED_BANK, 0xC000)
        fb_end   = fb_start + BANK_SIZE
        idx = data.find(PATTERN, fb_start, fb_end)
        if idx == -1:
            print("ERROR: JSR $C214 pattern not found in fixed bank")
            sys.exit(1)
        cpu = 0xC000 + (idx - fb_start)
        print(f"WARNING: pattern found outside NMI handler at ${cpu:04X}")

    cpu_addr = 0xC000 + (idx - rom_offset(FIXED_BANK, 0xC000))
    print(f"Found JSR $C214 at ${cpu_addr:04X} (file offset 0x{idx:06X})")

    # Patch operand bytes (little-endian target address)
    old_lo = data[idx + 1]
    old_hi = data[idx + 2]
    new_lo = hook_addr & 0xFF
    new_hi = (hook_addr >> 8) & 0xFF

    if old_lo == new_lo and old_hi == new_hi:
        print("Already patched — no changes needed")
        return

    data[idx + 1] = new_lo
    data[idx + 2] = new_hi

    with open(rom_path, "wb") as f:
        f.write(data)

    print(f"Patched: JSR ${old_hi:02X}{old_lo:02X} -> JSR ${new_hi:02X}{new_lo:02X}")
    print("OK: NMI yield hook installed")

if __name__ == "__main__":
    main()
