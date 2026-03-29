"""
Binary search: find the earliest frame with $7672 yields.
Read chunks and check for the pattern.
"""
import os, re

TRACE_FILE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\millipede.txt"
file_size = os.path.getsize(TRACE_FILE)
PATTERN = b'A3A8'  # The yield PC

# Check at various offsets
offsets_to_check = [0, file_size // 4, file_size // 2, file_size * 3 // 4]

for off in offsets_to_check:
    with open(TRACE_FILE, 'rb') as f:
        f.seek(off)
        chunk = f.read(5 * 1024 * 1024)  # 5MB chunks
    
    text = chunk.decode('utf-8', errors='replace')
    lines = text.split('\n')
    
    # Find frame range
    first_fr = last_fr = None
    has_yield = False
    for line in lines:
        m = re.search(r'Fr:(\d+)', line)
        if m:
            fr = int(m.group(1))
            if first_fr is None:
                first_fr = fr
            last_fr = fr
        if 'A3A8' in line and 'LDA #$72' in line:
            has_yield = True
    
    print(f"Offset {off:>12,} ({off*100//file_size:3d}%): Frames {first_fr}-{last_fr}, Yields: {'YES' if has_yield else 'no'}")

# Now do finer search around the transition point
# From the above, find where "no" switches to "YES"
print("\n--- Fine search ---")
# Read 200MB from end to find exact start
read_size = 200 * 1024 * 1024
offset = max(0, file_size - read_size)
with open(TRACE_FILE, 'rb') as f:
    f.seek(offset)
    data = f.read(read_size)
    
# Search for first occurrence of the yield pattern
idx = data.find(b'A3A8  LDA #$72')
if idx >= 0:
    # Find the frame number near this position
    context_start = max(0, idx - 500)
    context = data[context_start:idx + 500].decode('utf-8', errors='replace')
    frames = re.findall(r'Fr:(\d+)', context)
    print(f"First yield at byte offset {offset + idx:,} (frame ~{frames[0] if frames else '?'})")
    # Print surrounding lines
    line_start = data.rfind(b'\n', 0, idx) + 1
    line_end = data.find(b'\n', idx + 50)
    print(data[line_start:line_end].decode('utf-8', errors='replace')[:200])
    
    # Now read 5MB BEFORE this point to verify no earlier yields
    earlier_offset = max(0, offset + idx - 5 * 1024 * 1024)
    with open(TRACE_FILE, 'rb') as f:
        f.seek(earlier_offset)
        earlier_data = f.read(5 * 1024 * 1024)
    earlier_text = earlier_data.decode('utf-8', errors='replace')
    count = earlier_text.count('A3A8  LDA #$72')
    frames_e = re.findall(r'Fr:(\d+)', earlier_text[:1000])
    print(f"\n5MB before first yield (starting frame ~{frames_e[0] if frames_e else '?'}): {count} yields found")
