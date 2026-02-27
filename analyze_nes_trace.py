"""Analyze nes_dk.txt trace from Mesen2 debugger."""
import re
from collections import Counter

TRACE = r'C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_dk.txt'

brk_count = 0
low_addr_count = 0
frame_set = set()
addr_list = []
dispatch_entries = 0
nmi_count = 0
rti_count = 0

with open(TRACE, 'r') as f:
    for line in f:
        m = re.match(r'\s*([0-9A-Fa-f]{4})\s+(\w+)', line)
        if m:
            addr = int(m.group(1), 16)
            instr = m.group(2)
            addr_list.append(addr)
            if instr == 'BRK':
                brk_count += 1
            if instr == 'RTI':
                rti_count += 1
            if addr < 0x0200:
                low_addr_count += 1
        m2 = re.search(r'Fr:(\d+)', line)
        if m2:
            frame_set.add(int(m2.group(1)))

print("=== NES DK Trace Summary ===")
print("Total frames: %d (%d to %d)" % (len(frame_set), min(frame_set), max(frame_set)))
print("Duration: ~%.1f seconds at 60fps" % ((max(frame_set) - min(frame_set)) / 60.0))
print("Total instructions: %d" % len(addr_list))
print("BRK instructions: %d" % brk_count)
print("RTI instructions: %d" % rti_count)
print("Execution below 0x0200: %d" % low_addr_count)
print()

# Address range distribution
ranges = Counter()
for a in addr_list:
    if a < 0x2000:
        ranges['RAM (0000-1FFF)'] += 1
    elif a < 0x8000:
        ranges['IO/mid (2000-7FFF)'] += 1
    elif a < 0xC000:
        ranges['Banked (8000-BFFF)'] += 1
    else:
        ranges['Fixed (C000-FFFF)'] += 1

print("Address ranges:")
for rng, cnt in sorted(ranges.items()):
    print("  %s: %d (%.1f%%)" % (rng, cnt, 100.0 * cnt / len(addr_list)))
print()

# Top addresses overall
print("Top 20 most-hit addresses:")
addr_counts = Counter(addr_list)
for addr, cnt in addr_counts.most_common(20):
    print("  $%04X: %d" % (addr, cnt))
print()

# Check last 500 instructions for loops
print("Last 500 instructions - top addresses:")
last_counts = Counter(addr_list[-500:])
for addr, cnt in last_counts.most_common(10):
    print("  $%04X: %d" % (addr, cnt))
