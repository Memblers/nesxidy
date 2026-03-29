#!/usr/bin/env python3
"""Quick bank size report from NES ROM binary."""
import sys

rom = open(r'c:\proj\c\NES\nesxidy-co\nesxidy\exidy2.nes', 'rb').read()
header = 16
bank_size = 16384

# Bank 31 analysis
s = header + 31 * bank_size
chunk = rom[s:s+bank_size]
nmi = chunk[0x3FFA] | (chunk[0x3FFB] << 8)
reset = chunk[0x3FFC] | (chunk[0x3FFD] << 8)
irq = chunk[0x3FFE] | (chunk[0x3FFF] << 8)
print(f"Vectors: NMI=${nmi:04X} RESET=${reset:04X} IRQ=${irq:04X}")

# Find code end in bank31 (before trampoline at $FFF0 = offset 0x3FF0)
fill_start = 0x3FF0
for i in range(0x3FEF, -1, -1):
    if chunk[i] != 0x00 and chunk[i] != 0xFF:
        fill_start = i + 1
        break
print(f"Bank31 code ends at offset 0x{fill_start:04X} (addr ${0xC000+fill_start:04X}), {fill_start} bytes used")
print(f"Bank31 free: {0x3FF0 - fill_start} bytes before trampoline")
