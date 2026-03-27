"""
Examine what code runs between two $7672 yield groups.
The yields cluster at V:216-249 each frame, with ~8363 lines
of other code between clusters. What are those 8363 lines?

Look at a TYPICAL frame (63000) to see the full cycle.
"""
import os, re

TRACE_FILE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\millipede.txt"
file_size = os.path.getsize(TRACE_FILE)

# Find frame 63000 area - it's near the end
# 63007 is at the very end. 63000 is about 7 frames earlier.
# Each frame is about 100KB of trace, so ~700KB before end
read_size = 20 * 1024 * 1024  # 20MB
offset = max(0, file_size - read_size)

with open(TRACE_FILE, 'rb') as f:
    f.seek(offset)
    data = f.read(read_size)

text = data.decode('utf-8', errors='replace')
lines = text.split('\n')
if offset > 0:
    lines = lines[1:]

# Find yields in frame 63000 and 63001
yields_63000 = []
yields_63001 = []
for i, line in enumerate(lines):
    if 'A3A8' in line and 'LDA #$72' in line:
        m = re.search(r'Fr:(\d+)', line)
        if m:
            fr = int(m.group(1))
            if fr == 63000:
                yields_63000.append(i)
            elif fr == 63001:
                yields_63001.append(i)

print(f"Frame 63000: {len(yields_63000)} yields at lines {yields_63000[:3]}...{yields_63000[-3:]}")
print(f"Frame 63001: {len(yields_63001)} yields at lines {yields_63001[:3]}...{yields_63001[-3:]}")

# Look at the gap between last yield of 63000 and first yield of 63001
if yields_63000 and yields_63001:
    gap_start = yields_63000[-1] + 15  # skip yield stub
    gap_end = yields_63001[0]
    print(f"\nGap between frames: lines {gap_start}-{gap_end} ({gap_end-gap_start} lines)")
    
    # Summarize what PCs appear in the gap
    pc_counts = {}
    for i in range(gap_start, gap_end):
        line = lines[i].strip()
        m = re.match(r'([0-9A-F]{4})\s', line)
        if m:
            pc = m.group(1)
            # Group by high nibble to see address ranges
            prefix = pc[:2]
            pc_counts[prefix] = pc_counts.get(prefix, 0) + 1
    
    print("\nPC address ranges in gap:")
    for prefix in sorted(pc_counts.keys()):
        print(f"  ${prefix}xx: {pc_counts[prefix]} instructions")
    
    # Also count unique full PCs to see distinct routines
    full_pcs = {}
    for i in range(gap_start, gap_end):
        line = lines[i].strip()
        m = re.match(r'([0-9A-F]{4})\s', line)
        if m:
            full_pcs[m.group(1)] = full_pcs.get(m.group(1), 0) + 1
    
    # Top 20 most frequent PCs
    top = sorted(full_pcs.items(), key=lambda x: -x[1])[:20]
    print("\nTop 20 PCs in gap:")
    for pc, cnt in top:
        print(f"  ${pc}: {cnt} times")
    
    # Print first 50 lines and last 50 lines of gap
    print(f"\n=== FIRST 50 LINES OF GAP ===")
    for i in range(gap_start, min(gap_start + 50, gap_end)):
        print(lines[i].rstrip()[:150])
    
    print(f"\n=== LAST 50 LINES BEFORE NEXT YIELD GROUP ===")
    for i in range(max(gap_start, gap_end - 50), gap_end):
        print(lines[i].rstrip()[:150])
