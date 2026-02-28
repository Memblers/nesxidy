#!/usr/bin/python
"""
split_nes_rom.py — Split an iNES (.nes) file into raw PRG and CHR files.

Usage:
    python split_nes_rom.py <input.nes> [output_dir]

If output_dir is omitted, files are written alongside the input.
Only NROM (mapper 0) with 1×16KB PRG + 1×8KB CHR is supported
(the standard NROM-128 format used by DynaMoS NES targets).

Output:
    <basename>.prg  — 16384 bytes (PRG-ROM)
    <basename>.chr  —  8192 bytes (CHR-ROM)
"""

import sys
import os

INES_HEADER = 16
PRG_BANK    = 16384   # 16 KB
CHR_BANK    =  8192   #  8 KB

def main():
    if len(sys.argv) < 2:
        print("Usage: python split_nes_rom.py <input.nes> [output_dir]")
        sys.exit(1)

    nes_path = sys.argv[1]
    out_dir  = sys.argv[2] if len(sys.argv) > 2 else os.path.dirname(nes_path)
    if not out_dir:
        out_dir = "."

    with open(nes_path, "rb") as f:
        data = f.read()

    # Validate iNES header
    if data[:4] != b"NES\x1a":
        print(f"ERROR: {nes_path} is not a valid iNES file")
        sys.exit(1)

    prg_banks = data[4]
    chr_banks = data[5]
    mapper    = ((data[6] >> 4) | (data[7] & 0xF0))

    if mapper != 0:
        print(f"ERROR: mapper {mapper} not supported (only NROM/mapper 0)")
        sys.exit(1)

    if prg_banks != 1:
        print(f"ERROR: {prg_banks}×16KB PRG not supported (need exactly 1 for NROM-128)")
        sys.exit(1)

    if chr_banks != 1:
        print(f"ERROR: {chr_banks}×8KB CHR not supported (need exactly 1)")
        sys.exit(1)

    basename = os.path.splitext(os.path.basename(nes_path))[0].lower()
    prg_path = os.path.join(out_dir, basename + ".prg")
    chr_path = os.path.join(out_dir, basename + ".chr")

    prg_data = data[INES_HEADER : INES_HEADER + PRG_BANK]
    chr_data = data[INES_HEADER + PRG_BANK : INES_HEADER + PRG_BANK + CHR_BANK]

    with open(prg_path, "wb") as f:
        f.write(prg_data)
    with open(chr_path, "wb") as f:
        f.write(chr_data)

    print(f"  {basename}: PRG={len(prg_data)} CHR={len(chr_data)} -> {prg_path}, {chr_path}")

if __name__ == "__main__":
    main()
