#!/usr/bin/python
"""
patch_lnlist.py — Post-build patch: make lnList set nmiFlags bit 6 (sync)
along with bit 2 (VRU pending).

The LazyNES lnList function ($C222) sets nmiFlags bit 2 to mark VRU data
as pending for the next NMI.  The NMI handler gates VRU processing on
bit 6 (sync request), which is normally set by lnSync/render_video.

On DynaMoS, render_video and the NMI handler may race: if render_video
sets bit 2 via lnList but NMI fires before bit 6 is set, the handler
skips VRU entirely, yet increments nmiCounter — causing the blocking
loop to exit prematurely and the PPU data to be lost.

Fix: patch lnList's  ORA #$04  to  ORA #$44  so both bits 2 and 6 are
set atomically in a single STA.

Usage:
    python patch_lnlist.py <rom.nes>
"""

import sys
import os

INES_HEADER = 16
BANK_SIZE   = 16384
FIXED_BANK  = 31

# lnList is in the fixed bank at $C222.
# The relevant sequence at $C22D-$C232:
#   A5 25       LDA $25      (nmiFlags)
#   09 04       ORA #$04     (set bit 2)
#   85 25       STA $25      (nmiFlags)
# We patch 09 04 → 09 44 to set bits 2+6.

PATTERN = bytes([0xA5, 0x25, 0x09, 0x04, 0x85, 0x25])
LNLIST_REGION_START = 0xC222
LNLIST_REGION_END   = 0xC23F

def rom_offset(bank, cpu_addr):
    base = 0xC000 if bank == 31 else 0x8000
    return INES_HEADER + bank * BANK_SIZE + (cpu_addr - base)

def main():
    if len(sys.argv) < 2:
        print("Usage: python patch_lnlist.py <rom.nes>")
        sys.exit(1)

    rom_path = sys.argv[1]

    with open(rom_path, "rb") as f:
        data = bytearray(f.read())

    search_start = rom_offset(FIXED_BANK, LNLIST_REGION_START)
    search_end   = rom_offset(FIXED_BANK, LNLIST_REGION_END)

    idx = data.find(PATTERN, search_start, search_end)
    if idx == -1:
        # Wider fallback: full fixed bank
        fb_start = rom_offset(FIXED_BANK, 0xC000)
        fb_end   = fb_start + BANK_SIZE
        idx = data.find(PATTERN, fb_start, fb_end)
        if idx == -1:
            print("ERROR: lnList ORA #$04 pattern not found")
            sys.exit(1)
        cpu = 0xC000 + (idx - fb_start)
        print(f"WARNING: pattern found at ${cpu:04X} (outside expected region)")

    cpu_addr = 0xC000 + (idx - rom_offset(FIXED_BANK, 0xC000))
    patch_offset = idx + 3  # offset of the $04 immediate byte

    old_val = data[patch_offset]
    if old_val == 0x44:
        print(f"Already patched at ${cpu_addr:04X} — ORA #$44")
        return

    if old_val != 0x04:
        print(f"ERROR: unexpected byte ${old_val:02X} at patch offset (expected $04)")
        sys.exit(1)

    data[patch_offset] = 0x44
    with open(rom_path, "wb") as f:
        f.write(data)

    print(f"Patched lnList at ${cpu_addr + 2:04X}: ORA #$04 -> ORA #$44")
    print("OK: lnList now sets nmiFlags bits 2+6 atomically")

if __name__ == "__main__":
    main()
