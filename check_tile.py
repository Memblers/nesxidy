#!/usr/bin/env python3
"""Check space tile data in ROM."""
data = open('exidy.nes', 'rb').read()
# chr_sidetrac at $9900 in bank1
# bank1 = ROM offset 16 + 1*16384 + ($9900 - $8000)
offset = 16 + 1*16384 + (0x9900 - 0x8000)
print(f'chr_sidetrac ROM offset: 0x{offset:05X}')
# Space character = tile $20 = offset 32*8 = 256 = $100
for tile in range(0x1E, 0x24):
    t_off = offset + tile * 8
    t_data = data[t_off:t_off+8]
    hex_str = ' '.join(f'{b:02X}' for b in t_data)
    all_zero = all(b == 0 for b in t_data)
    print(f'Tile ${tile:02X} at ROM 0x{t_off:05X}: {hex_str} {"(all zero)" if all_zero else "HAS DATA"}')
