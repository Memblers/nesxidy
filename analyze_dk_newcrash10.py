"""Get context around PC table write #5 (Fr:4513) that writes na=$9BA0.
Line 42033805 writes $A0 to [$975C], line 42033860 writes $9B to [$975D].
Need to see: what block is being compiled, what guest PC, and why na=$9BA0.
Also check the flag guard — was the flag $FF before this write?"""
import os, re

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"
fsize = os.path.getsize(TRACE)

# Line 42033805 is approximately at byte offset:
# Average ~115 bytes/line, so ~4.83 GB into the file
# Let me binary search for it.

# Actually, we know:
# L42033805 at Fr:4513 - this is in the middle of a compile.
# Let me find the byte offset by counting.

# Better approach: read a window around the target.
# The trace has ~73M lines total (8.45GB / ~115 bytes per line).
# L42033805 / 73M * 8.45GB ≈ 4.87GB

target_offset = int(42033805 / 73_000_000 * fsize)
print(f"Estimated byte offset for L42033805: {target_offset:,}")

# Read a 2MB window around that offset
window_size = 2_000_000
with open(TRACE, 'rb') as f:
    f.seek(max(0, target_offset - window_size))
    raw = f.read(window_size * 2)

lines = raw.decode('utf-8', errors='replace').split('\n')
print(f"Read {len(lines)} lines")

# Find the line with STA (r2),Y [$975C] = $FF  A:A0
target_line_idx = None
for i, line in enumerate(lines):
    if '[$975C]' in line and 'A:A0' in line and 'STA' in line:
        target_line_idx = i
        print(f"Found target write at local index {i}")
        break

if target_line_idx is None:
    print("Target write not found in window! Trying wider search...")
    # Try adjusting offset
    for adj in [-500_000_000, -200_000_000, 0, 200_000_000, 500_000_000]:
        offset = target_offset + adj
        if offset < 0 or offset >= fsize:
            continue
        with open(TRACE, 'rb') as f:
            f.seek(offset)
            raw = f.read(5_000_000)
        chunk = raw.decode('utf-8', errors='replace')
        if '[$975C]' in chunk and 'A:A0' in chunk:
            print(f"Found at offset ~{offset:,}")
            lines = chunk.split('\n')
            for i, line in enumerate(lines):
                if '[$975C]' in line and 'A:A0' in line:
                    target_line_idx = i
                    break
            break
else:
    # Show 200 lines before and 50 lines after
    print(f"\n=== CONTEXT: 200 lines before and 50 lines after write ===")
    start = max(0, target_line_idx - 200)
    end = min(len(lines), target_line_idx + 50)
    
    for i in range(start, end):
        marker = " >>>" if i == target_line_idx else "    "
        line = lines[i].rstrip()[:180]
        # Highlight key events
        highlight = ""
        if '[$975C]' in line or '[$975D]' in line or '[$8BAE]' in line:
            highlight = " *** PC TABLE WRITE ***"
        if 'STA $C000' in line:
            m = re.search(r'A:([0-9A-Fa-f]{2})', line)
            if m:
                highlight = f" [bank={m.group(1)}]"
        if 'flash_sector_erase' in line.lower() or 'erase' in line.lower():
            highlight = " *** SECTOR ERASE ***"
        if '_cache_entry_pc' in line.lower() or 'STA $94' in line or 'STA $95' in line:
            m = re.search(r'A:([0-9A-Fa-f]{2})', line)
            if m:
                highlight = f" [entry_pc byte={m.group(1)}]"
        
        print(f"{marker} {line}{highlight}")
