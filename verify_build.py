#!/usr/bin/env python3
"""Verify the built ROM has the fix applied."""

with open(r'c:\proj\c\NES\nesxidy-co\nesxidy\nes_donkeykong.nes', 'rb') as f:
    data = f.read()

bank31_off = 16 + 31 * 16384
bank31 = data[bank31_off:bank31_off + 16384]

nmi_off = 0x192
nmi_bytes = bank31[nmi_off:nmi_off+5]
print(f'___nmi at $C192: {" ".join(f"{b:02X}" for b in nmi_bytes)}')
print(f'  Expected: 48 8A 48 98 48 (PHA TXA PHA TYA PHA)')

sync_off = 0x24F
sync_bytes = bank31[sync_off:sync_off+5]
print(f'_lnSync at $C24F: {" ".join(f"{b:02X}" for b in sync_bytes)}')

nmi_vec = bank31[0x3FFA] | (bank31[0x3FFB] << 8)
print(f'NMI vector: ${nmi_vec:04X}')

print(f'ROM: {len(data)} bytes, {(len(data)-16)//16384} PRG banks')
