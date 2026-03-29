#!/usr/bin/python
"""
patch_oam_dma.py — Post-build patch for lazynes NMI sprite DMA page.

LazyNES's built-in NMI handler does  LDA #$02 / STA $4014  to trigger
OAM DMA from page $02 (the standard NES OAM buffer at $0200).  DynaMoS
relocates guest RAM to RAM_BASE ($6C00+), so the OAM buffer the guest
game populates is actually at RAM_BASE+$200, not $0200.

This script reads vicemap.map to find _RAM_BASE, computes the correct
DMA page byte (high byte of RAM_BASE+$200), and binary-patches the
.nes ROM in place.

Usage:
    python patch_oam_dma.py <rom.nes> [vicemap.map]

If vicemap.map is omitted it defaults to vicemap.map in the script's
directory (or cwd).
"""

import sys
import os
import re

# The exact byte sequence in lazynes's NMI handler:
#   A9 02      LDA #$02
#   8D 14 40   STA $4014
PATTERN = bytes([0xA9, 0x02, 0x8D, 0x14, 0x40])

def find_ram_base(mapfile):
    """Parse vicemap.map for _RAM_BASE address."""
    expr = re.compile(r"al C:([0-9a-fA-F]+) \._RAM_BASE$")
    with open(mapfile, "r") as f:
        for line in f:
            m = expr.match(line.rstrip())
            if m:
                return int(m.group(1), 16)
    return None

def main():
    if len(sys.argv) < 2:
        print("Usage: python patch_oam_dma.py <rom.nes> [vicemap.map]")
        sys.exit(1)

    rom_path = sys.argv[1]
    map_path = sys.argv[2] if len(sys.argv) > 2 else "vicemap.map"

    # Resolve map path relative to script dir if not found in cwd
    if not os.path.exists(map_path):
        script_dir = os.path.dirname(os.path.abspath(__file__))
        map_path = os.path.join(script_dir, map_path)

    ram_base = find_ram_base(map_path)
    if ram_base is None:
        print(f"ERROR: _RAM_BASE not found in {map_path}")
        sys.exit(1)

    dma_page = ((ram_base + 0x200) >> 8) & 0xFF
    print(f"RAM_BASE=${ram_base:04X}  OAM=${ram_base+0x200:04X}  DMA page=${dma_page:02X}")

    with open(rom_path, "rb") as f:
        data = bytearray(f.read())

    # Find all occurrences of the LDA #$02 / STA $4014 pattern
    matches = []
    offset = 0
    while True:
        idx = data.find(PATTERN, offset)
        if idx == -1:
            break
        matches.append(idx)
        offset = idx + 1

    if len(matches) == 0:
        print("ERROR: LDA #$02 / STA $4014 pattern not found in ROM")
        sys.exit(1)

    if len(matches) > 1:
        locs = ", ".join(f"0x{m:06X}" for m in matches)
        print(f"WARNING: {len(matches)} matches found at [{locs}], patching all")

    if dma_page == 0x02:
        print("DMA page is already $02 — no patch needed")
        return

    patched = 0
    for idx in matches:
        old = data[idx + 1]
        if old != dma_page:
            data[idx + 1] = dma_page
            patched += 1
            print(f"  Patched offset 0x{idx:06X}: LDA #${old:02X} -> LDA #${dma_page:02X}")
        else:
            print(f"  Offset 0x{idx:06X}: already ${dma_page:02X}")

    if patched > 0:
        with open(rom_path, "wb") as f:
            f.write(data)
        print(f"OK: {patched} patch(es) applied to {rom_path}")
    else:
        print("No changes needed")

if __name__ == "__main__":
    main()
