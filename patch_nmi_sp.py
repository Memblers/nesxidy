#!/usr/bin/python
"""
patch_nmi_sp.py — Post-build patch to guard vbcc sp ($20-$21) from
the lazyNES NMI handler's destructive ASL $20 (nfOamFull processing).

Two patches:
  1. Redirect NMI vector ($FFFA) to _nmi_sp_trampoline in WRAM
  2. Replace the NMI handler's final TAX;PLA;RTI (AA 68 40) with
     JMP _nmi_sp_restore

Usage:
    python patch_nmi_sp.py <rom.nes> [vicemap.map]
"""

import sys
import os
import re

INES_HEADER = 16
BANK_SIZE   = 16384
FIXED_BANK  = 31

# NMI handler region in fixed bank
NMI_HANDLER_START = 0xC192
NMI_HANDLER_END   = 0xC230

# Byte pattern for TAX;PLA;RTI at end of NMI handler
NMI_TAIL_PATTERN = bytes([0xAA, 0x68, 0x40])  # TAX PLA RTI


def rom_offset(bank, cpu_addr):
    """Convert (bank, cpu_addr) to iNES file offset."""
    base = 0xC000 if bank == FIXED_BANK else 0x8000
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
        print("Usage: python patch_nmi_sp.py <rom.nes> [vicemap.map]")
        sys.exit(1)

    rom_path = sys.argv[1]
    map_path = sys.argv[2] if len(sys.argv) > 2 else "vicemap.map"

    if not os.path.exists(map_path):
        script_dir = os.path.dirname(os.path.abspath(__file__))
        map_path = os.path.join(script_dir, map_path)

    # Find WRAM trampoline symbols
    trampoline_addr = find_symbol(map_path, "_nmi_sp_trampoline")
    restore_addr    = find_symbol(map_path, "_nmi_sp_restore")

    if trampoline_addr is None:
        print("ERROR: _nmi_sp_trampoline not found in " + map_path)
        sys.exit(1)
    if restore_addr is None:
        print("ERROR: _nmi_sp_restore not found in " + map_path)
        sys.exit(1)

    print(f"_nmi_sp_trampoline = ${trampoline_addr:04X}")
    print(f"_nmi_sp_restore    = ${restore_addr:04X}")

    with open(rom_path, "rb") as f:
        data = bytearray(f.read())

    patched = False

    # --- Patch 1: Redirect NMI vector ---
    nmi_vec_offset = rom_offset(FIXED_BANK, 0xFFFA)
    old_nmi_lo = data[nmi_vec_offset]
    old_nmi_hi = data[nmi_vec_offset + 1]
    old_nmi = old_nmi_lo | (old_nmi_hi << 8)
    print(f"NMI vector at file offset 0x{nmi_vec_offset:06X}: ${old_nmi:04X}")

    new_lo = trampoline_addr & 0xFF
    new_hi = (trampoline_addr >> 8) & 0xFF
    if old_nmi_lo != new_lo or old_nmi_hi != new_hi:
        data[nmi_vec_offset]     = new_lo
        data[nmi_vec_offset + 1] = new_hi
        print(f"  -> Patched NMI vector to ${trampoline_addr:04X}")
        patched = True
    else:
        print("  NMI vector already patched")

    # --- Patch 2: Replace TAX;PLA;RTI with JMP _nmi_sp_restore ---
    search_start = rom_offset(FIXED_BANK, NMI_HANDLER_START)
    search_end   = rom_offset(FIXED_BANK, NMI_HANDLER_END)

    idx = data.find(NMI_TAIL_PATTERN, search_start, search_end)
    if idx == -1:
        # Wider search as fallback
        fb_start = rom_offset(FIXED_BANK, 0xC000)
        fb_end   = fb_start + BANK_SIZE
        idx = data.find(NMI_TAIL_PATTERN, fb_start, fb_end)
        if idx == -1:
            print("ERROR: TAX;PLA;RTI pattern (AA 68 40) not found in NMI handler")
            sys.exit(1)
        cpu = 0xC000 + (idx - fb_start)
        print(f"WARNING: pattern found outside NMI handler at ${cpu:04X}")

    cpu_addr = 0xC000 + (idx - rom_offset(FIXED_BANK, 0xC000))
    print(f"Found TAX;PLA;RTI at ${cpu_addr:04X} (file offset 0x{idx:06X})")

    # Replace 3 bytes: AA 68 40 -> 4C lo hi (JMP _nmi_sp_restore)
    jmp_lo = restore_addr & 0xFF
    jmp_hi = (restore_addr >> 8) & 0xFF
    if data[idx] == 0x4C and data[idx+1] == jmp_lo and data[idx+2] == jmp_hi:
        print("  Already patched")
    else:
        data[idx]     = 0x4C   # JMP opcode
        data[idx + 1] = jmp_lo
        data[idx + 2] = jmp_hi
        print(f"  -> Patched to JMP ${restore_addr:04X}")
        patched = True

    if patched:
        with open(rom_path, "wb") as f:
            f.write(data)
        print("ROM patched successfully")
    else:
        print("No changes needed")


if __name__ == "__main__":
    main()
