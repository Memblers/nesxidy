#!/usr/bin/env python3
"""Find NES CPU register sections in Mesen2 savestate."""
import zlib

with open(r"C:\Users\membl\OneDrive\Documents\Mesen2\SaveStates\exidy_1.mss", "rb") as f:
    data = f.read()

pos = 0
zblocks = []
while pos < len(data) - 2:
    if data[pos] == 0x78 and data[pos+1] in (0x01, 0x5E, 0x9C, 0xDA):
        try:
            d = zlib.decompressobj()
            r = d.decompress(data[pos:])
            c = len(data[pos:]) - len(d.unused_data)
            zblocks.append((pos, bytearray(r)))
            pos += c
            continue
        except:
            pass
    pos += 1

blk1 = zblocks[1][1]

# List ALL sections with 'cpu' in the name
print("=== All CPU-related sections ===")
pos = 0
while pos < len(blk1) - 10:
    # Look for null-terminated strings containing 'cpu'
    if blk1[pos:pos+3] == b'cpu' or blk1[pos:pos+3] == b'Cpu':
        # Find the null terminator
        end = blk1.find(b'\x00', pos)
        if end > pos and end - pos < 100:
            name = blk1[pos:end].decode('ascii', errors='replace')
            sz = int.from_bytes(blk1[end+1:end+5], 'little')
            val_bytes = blk1[end+5:end+5+min(sz, 8)]
            if sz <= 8:
                val = int.from_bytes(val_bytes, 'little')
                print(f"  offset=0x{pos:X} [{name}] size={sz}: value=0x{val:X} ({val})")
            else:
                print(f"  offset=0x{pos:X} [{name}] size={sz}: {val_bytes[:16].hex()}")
    pos += 1

# Also search for the section that has the NES 6502 state
# Mesen2 uses "NesCpu" or "nesCpu" as prefix
print("\n=== All 'nes' sections ===")
pos = 0
while pos < len(blk1) - 10:
    if blk1[pos:pos+3].lower() == b'nes':
        end = blk1.find(b'\x00', pos)
        if end > pos and end - pos < 100:
            name = blk1[pos:end].decode('ascii', errors='replace')
            sz = int.from_bytes(blk1[end+1:end+5], 'little')
            val_bytes = blk1[end+5:end+5+min(sz, 8)]
            if sz <= 8:
                val = int.from_bytes(val_bytes, 'little')
                print(f"  offset=0x{pos:X} [{name}] size={sz}: value=0x{val:X} ({val})")
            else:
                print(f"  offset=0x{pos:X} [{name}] size={sz}: {val_bytes[:16].hex()}")
    pos += 1

# Also dump a region around the first 'cpu' marker
first_cpu = blk1.find(b'cpu')
if first_cpu >= 0:
    print(f"\n=== Context around first 'cpu' (offset 0x{first_cpu:X}) ===")
    start = max(0, first_cpu - 32)
    for i in range(start, min(len(blk1), first_cpu + 200), 16):
        hex_str = ' '.join(f'{b:02X}' for b in blk1[i:i+16])
        ascii_str = ''.join(chr(b) if 32 <= b < 127 else '.' for b in blk1[i:i+16])
        print(f"  {i:06X}: {hex_str}  {ascii_str}")
