#!/usr/bin/env python3
"""Check the SA bitmap in the compiled ROM."""
import sys

with open('exidy.nes', 'rb') as f:
    # Bank 3 starts at offset 16 (header) + 3*16384
    f.seek(16 + 3 * 16384)
    bank3 = f.read(16384)

# The bitmap is at the start of bank3 (sa_code_bitmap)
# Check the linker map for exact offset
with open('vicemap.map', 'r') as f:
    for line in f:
        if 'sa_code_bitmap' in line:
            print(f"Map entry: {line.strip()}")
        if 'sa_header' in line:
            print(f"Map entry: {line.strip()}")
        if 'flash_block_flags' in line:
            print(f"Map entry: {line.strip()}")

# The bitmap is SA_BITMAP_SIZE = 4096 bytes (32K addresses / 8)
# In the ROM, bank3 maps to $8000-$BFFF
# flash_block_flags is also in bank3
# Need to find the offset within bank3

# Let's just scan the entire bank3 for non-FF bytes
print(f"\nBank3 non-FF regions:")
for i in range(0, len(bank3), 256):
    chunk = bank3[i:i+256]
    non_ff = sum(1 for b in chunk if b != 0xFF)
    if non_ff > 0:
        print(f"  ${0x8000+i:04X}-${0x8000+i+255:04X}: {non_ff} non-FF bytes")

# Also check bitmap area specifically
# SA_BITMAP_SIZE = 4096 = 0x1000
# ROM_ADDR_MIN = 0x2800, ROM_ADDR_MAX = 0x3FFF
# Bitmap byte for address A = A >> 3
# So $2800 >> 3 = $500, $3FFF >> 3 = $7FF
# These would be at bitmap offsets $500-$7FF
print(f"\nIn compiled ROM (flash is all FF initially):")
print("The bitmap starts as all FF (erased). Walker clears bits for known code.")
print("But in the ROM file, bank3 is whatever the linker puts there (probably zeros for reserve).")
print()

# Check actual bitmap content in the ROM
# The bitmap will be at the linker-assigned address within bank3
# Let's check what we see
bitmap_area = bank3[0x500:0x800]  # bytes covering $2800-$3FFF
non_ff = sum(1 for b in bitmap_area if b != 0xFF)
non_zero = sum(1 for b in bitmap_area if b != 0x00)
print(f"Bitmap bytes $500-$7FF (covers $2800-$3FFF):")
print(f"  All-FF bytes: {sum(1 for b in bitmap_area if b == 0xFF)}")
print(f"  All-00 bytes: {sum(1 for b in bitmap_area if b == 0x00)}")
print(f"  Other bytes: {sum(1 for b in bitmap_area if b != 0xFF and b != 0x00)}")
print()

# The ROM file has the initial flash state (before any runtime writes)
# flash_block_flags: reserve FLASH_CACHE_BLOCKS (960 bytes)
# sa_code_bitmap: reserve SA_BITMAP_SIZE (4096 bytes)  
# Both are 'reserve' in bank3, so they'll be zeros or FF in the ROM
print("Note: The bitmap is populated at RUNTIME by the BFS walker writing to flash.")
print("The ROM file just has the initial (erased/reserved) state.")
