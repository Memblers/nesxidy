"""
Look at the trace between consecutive $7672 yields:
- Between last yield in fr:63006 and the yield in fr:63007
- Between 2nd-to-last and last yield in fr:63006
Focus on what happens to make the game progress.
"""
import os, re

TRACE_FILE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\millipede.txt"

file_size = os.path.getsize(TRACE_FILE)
read_size = 15 * 1024 * 1024  # 15MB
offset = max(0, file_size - read_size)

with open(TRACE_FILE, 'rb') as f:
    f.seek(offset)
    data = f.read(read_size)
    
text = data.decode('utf-8', errors='replace')
lines = text.split('\n')
if offset > 0:
    lines = lines[1:]

# Find all $7672 yield points (LDA #$72 at $A3A8)
yield_points = []
for i, line in enumerate(lines):
    if 'A3A8' in line and 'LDA #$72' in line:
        # Extract frame number
        m = re.search(r'Fr:(\d+)', line)
        fr = int(m.group(1)) if m else 0
        # Extract scanline
        m2 = re.search(r'V:(\d+)', line)
        v = int(m2.group(1)) if m2 else 0
        yield_points.append((i, fr, v))

print(f"Found {len(yield_points)} yield points in last {read_size//1024//1024}MB")
for idx, (line_idx, fr, v) in enumerate(yield_points[-15:]):
    print(f"  #{idx}: line {line_idx}, Frame {fr}, V:{v}")

# Now look at what happens between the last two yields
if len(yield_points) >= 2:
    prev = yield_points[-2]
    last = yield_points[-1]
    print(f"\n=== BETWEEN LAST TWO YIELDS ===")
    print(f"From line {prev[0]} (Fr:{prev[1]} V:{prev[2]}) to line {last[0]} (Fr:{last[1]} V:{last[2]})")
    print(f"Gap: {last[0] - prev[0]} lines")
    
    # Print all lines between them (limit to 300)
    start = prev[0] + 10  # skip the yield itself
    end = min(last[0], prev[0] + 300)
    print(f"\nLines {start}-{end}:")
    for i in range(start, end):
        if i < len(lines):
            print(lines[i].rstrip()[:160])
    
    if last[0] - prev[0] > 300:
        print(f"\n... ({last[0] - prev[0] - 290} more lines) ...")
        # Print last 30 lines before the yield
        for i in range(max(start, last[0]-30), last[0]):
            if i < len(lines):
                print(lines[i].rstrip()[:160])
