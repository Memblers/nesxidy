"""Analyze where the Millipede trace gets stuck - look at the final frames and 
find the repeating pattern that causes the hang."""
import re

f = r'C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\millipede.txt'

# Read from near the end - look at the last ~500K lines to find the pattern
import os
size = os.path.getsize(f)

# Read the area around where the loop starts repeating
# First let's look at the transition - find where the game starts looping 
# by looking at what guest PCs are being dispatched to

# Look at lines around the last few frames
print("=== LOOKING AT FRAME TRANSITIONS NEAR END ===")
print()

# Read lines from ~29.1M to see what happens
target_start = 29_100_000
lines_buf = []
with open(f, 'r') as fh:
    for i, line in enumerate(fh):
        if i >= target_start:
            lines_buf.append((i, line.rstrip()))
        if i >= target_start + 50000:
            break

# Find the dispatch pattern - look for JMP to flash ($8000-$BFFF) from fixed bank ($C000-$FFFF)
# and the epilogue return pattern
print(f"Read {len(lines_buf)} lines starting from line {target_start}")
print()

# Find all blocks that execute - track PC flows 
# A block starts when we jump into $8000-$BFFF range from $C000+ range
block_entries = []
prev_addr = 0
for idx, (lineno, line) in enumerate(lines_buf):
    parts = line.split()
    if not parts:
        continue
    try:
        addr = int(parts[0], 16)
    except:
        continue
    
    # Detect transition from fixed bank to flash
    if 0x8000 <= addr <= 0xBFFF and 0xC000 <= prev_addr <= 0xFFFF:
        block_entries.append((lineno, addr, idx))
    
    prev_addr = addr

print(f"Block entries (flash from fixed bank): {len(block_entries)}")
if block_entries:
    # Show last 50 block entries
    print("Last 50 block entries:")
    for lineno, addr, idx in block_entries[-50:]:
        # Get the actual line
        line = lines_buf[idx][1]
        print(f"  Line {lineno:>10,}: ${addr:04X}  {line[:120]}")

print()

# Also look for what guest PCs the dispatch is handling
# Search for patterns that suggest guest PC loading (STA _pc / LDA _pc)
# In the dispatch, _pc is at some ZP location

# Look for the repeating loop - detect when the same sequence of addresses repeats
print("=== DETECTING REPEATED PATTERNS IN LAST 5000 LINES ===")
last_addrs = []
for lineno, line in lines_buf[-5000:]:
    parts = line.split()
    if not parts:
        continue
    try:
        addr = int(parts[0], 16)
        last_addrs.append(addr)
    except:
        continue

# Find the period of the repeat
for period in range(50, 500):
    if len(last_addrs) < period * 3:
        continue
    match = True
    for j in range(period):
        if last_addrs[-1-j] != last_addrs[-1-j-period]:
            match = False
            break
    if match:
        print(f"Found repeating pattern with period {period}")
        print("One cycle of the pattern:")
        start = len(last_addrs) - period
        for j in range(min(period, 200)):
            print(f"  ${last_addrs[start+j]:04X}")
        if period > 200:
            print(f"  ... ({period - 200} more)")
        break
else:
    print("No simple repeating pattern found in last 5000 lines")
    # Show last 100 unique address sequences
    print("Last 100 addresses:")
    for a in last_addrs[-100:]:
        print(f"  ${a:04X}")
