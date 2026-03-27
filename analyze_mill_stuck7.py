"""
Search backwards through the trace to find when $7672 yields FIRST appear.
Look at a 100MB window to cover many frames.
"""
import os, re

TRACE_FILE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\millipede.txt"
file_size = os.path.getsize(TRACE_FILE)

# Read 100MB from the end
read_size = min(100 * 1024 * 1024, file_size)
offset = max(0, file_size - read_size)

with open(TRACE_FILE, 'rb') as f:
    f.seek(offset)
    data = f.read(read_size)

text = data.decode('utf-8', errors='replace')
lines = text.split('\n')
if offset > 0:
    lines = lines[1:]

print(f"File: {file_size:,} bytes. Read {read_size:,} bytes ({len(lines)} lines) from offset {offset:,}")

# Find all yield points (LDA #$72 at A3A8)
first_yield = None
last_yield = None
yield_count = 0
frame_set = set()

for i, line in enumerate(lines):
    if 'A3A8' in line and 'LDA #$72' in line:
        m = re.search(r'Fr:(\d+)', line)
        fr = int(m.group(1)) if m else 0
        if first_yield is None:
            first_yield = (i, fr)
        last_yield = (i, fr)
        yield_count += 1
        frame_set.add(fr)

if first_yield:
    print(f"\nFirst $7672 yield: line {first_yield[0]}, Frame {first_yield[1]}")
    print(f"Last $7672 yield: line {last_yield[0]}, Frame {last_yield[1]}")
    print(f"Total yields: {yield_count}")
    print(f"Frames with yields: {sorted(frame_set)}")
    
    # Also check what frame the trace starts with
    for line in lines[:10]:
        m = re.search(r'Fr:(\d+)', line)
        if m:
            print(f"Earliest frame in window: {m.group(1)}")
            break
else:
    print("No $7672 yields found in window!")
