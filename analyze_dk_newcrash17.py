import sys

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"

# Look at the last 20MB of the trace (around the crash).
# Find ALL flash_byte_program writes to bank 4 (any address).
# This tells us if the dynamic JIT writes to bank 4 after the SA compile.

with open(TRACE, 'rb') as f:
    f.seek(0, 2)
    file_size = f.tell()

SIZE = 20_000_000
START = max(0, file_size - SIZE)

print(f"Scanning last {SIZE/1e6:.0f}MB of trace for ANY bank-4 flash writes")
sys.stdout.flush()

hits = []
with open(TRACE, 'rb') as f:
    f.seek(START)
    data = f.read(SIZE)

for line in data.split(b'\n'):
    if b'6041' not in line:
        continue
    line_str = line.decode('utf-8', errors='replace')
    if '6041' not in line_str[:8]:
        continue
    if 'STA (r2),Y' not in line_str:
        continue
    
    x_pos = line_str.find('X:')
    if x_pos < 0: continue
    try: x_val = int(line_str[x_pos+2:x_pos+4], 16)
    except: continue
    if x_val != 0x04: continue
    
    bracket_pos = line_str.find('[$')
    if bracket_pos < 0: continue
    try: eff_addr = int(line_str[bracket_pos+2:bracket_pos+6], 16)
    except: continue
    
    a_pos = line_str.find('A:')
    try: a_val = int(line_str[a_pos+2:a_pos+4], 16)
    except: continue
    
    fr_pos = line_str.find('Fr:')
    frame = "?"
    if fr_pos >= 0:
        fr_end = line_str.find(' ', fr_pos+3)
        if fr_end < 0: fr_end = len(line_str)
        frame = line_str[fr_pos+3:fr_end]
    
    hits.append((frame, eff_addr, a_val))

print(f"Found {len(hits)} bank-4 flash writes in last {SIZE/1e6:.0f}MB")

# Group by high byte of address
from collections import defaultdict
pages = defaultdict(int)
for frame, addr, data in hits:
    pages[addr >> 8] += 1

print(f"\nWrites by page:")
for page in sorted(pages.keys()):
    print(f"  ${page:02X}xx: {pages[page]} writes")

# Show any writes to $9B00-$9C00
near = [(f, a, d) for f, a, d in hits if 0x9B00 <= a <= 0x9C00]
print(f"\nWrites to $9B00-$9C00: {len(near)}")
for frame, addr, data in near:
    print(f"  Fr:{frame}  [{addr:04X}] = ${data:02X}")

# Show the last 20 writes overall
if hits:
    print(f"\nLast 20 bank-4 writes:")
    for frame, addr, data in hits[-20:]:
        print(f"  Fr:{frame}  [{addr:04X}] = ${data:02X}")

print("\nDone.")
