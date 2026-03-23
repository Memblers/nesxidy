import sys

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"

# We know the [$975C] write is at byte offset ~4,785,552,061 (from newcrash11).
# The code writes BEFORE the PC table publish should be within ~500KB before that.
# Let's scan 5MB before and 1MB after that offset for ALL flash writes to bank 4.

CENTER = 4_785_552_061
START = max(0, CENTER - 5_000_000)
LENGTH = 6_000_000

print(f"Scanning {LENGTH/1e6:.1f}MB around byte offset {CENTER} for bank-4 flash writes")
print(f"  Range: {START} to {START+LENGTH}")
sys.stdout.flush()

hits = []
with open(TRACE, 'rb') as f:
    f.seek(START)
    raw = f.read(LENGTH)

lines = raw.split(b'\n')
print(f"  Read {len(lines)} lines")
sys.stdout.flush()

for i, line in enumerate(lines):
    if b'6041' not in line:
        continue
    line_str = line.decode('utf-8', errors='replace')
    
    if '6041' not in line_str[:8]:
        continue
    if 'STA (r2),Y' not in line_str:
        continue
    
    # Extract X register
    x_pos = line_str.find('X:')
    if x_pos < 0:
        continue
    try:
        x_val = int(line_str[x_pos+2:x_pos+4], 16)
    except:
        continue
    
    if x_val != 0x04:
        continue
    
    # Extract effective address
    bracket_pos = line_str.find('[$')
    if bracket_pos < 0:
        continue
    try:
        eff_addr = int(line_str[bracket_pos+2:bracket_pos+6], 16)
    except:
        continue
    
    # Extract A (data byte)
    a_pos = line_str.find('A:')
    try:
        a_val = int(line_str[a_pos+2:a_pos+4], 16)
    except:
        continue
    
    # Extract frame
    fr_pos = line_str.find('Fr:')
    frame = "?"
    if fr_pos >= 0:
        fr_end = line_str.find(' ', fr_pos+3)
        if fr_end < 0: fr_end = len(line_str)
        frame = line_str[fr_pos+3:fr_end]
    
    hits.append((i, frame, eff_addr, a_val, line_str.strip()[:140]))

print(f"\nFound {len(hits)} bank-4 flash writes in window")

# Show writes near $9B80-$9BC0
near_hits = [(i, f, a, d, l) for i, f, a, d, l in hits if 0x9B00 <= a <= 0x9C00]
print(f"\nWrites to $9B00-$9C00 in bank 4: {len(near_hits)}")
for idx, frame, addr, data, line in near_hits:
    print(f"  line {idx:6d} Fr:{frame}  [{addr:04X}] = ${data:02X}")

# Show ALL writes, grouped by address high byte
from collections import defaultdict
groups = defaultdict(list)
for i, f, a, d, l in hits:
    groups[a >> 8].append((i, f, a, d, l))

print(f"\nAll bank-4 writes by address page:")
for page in sorted(groups.keys()):
    writes = groups[page]
    print(f"  ${page:02X}xx: {len(writes)} writes")
    if page == 0x9B:
        for idx, frame, addr, data, line in writes:
            print(f"    line {idx:6d} Fr:{frame}  [{addr:04X}] = ${data:02X}")

print("\nDone.")
