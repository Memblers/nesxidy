"""
Analyze the Millipede trace to understand the $7672 stuck state.
Focus on: what guest PCs are being dispatched, and what host code
executes around yield points. Look at _pc stores/loads.
"""
import os

TRACE_FILE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\millipede.txt"

# Read last 2MB of trace
file_size = os.path.getsize(TRACE_FILE)
read_size = 2 * 1024 * 1024
offset = max(0, file_size - read_size)

with open(TRACE_FILE, 'rb') as f:
    f.seek(offset)
    data = f.read(read_size)
    
text = data.decode('utf-8', errors='replace')
lines = text.split('\n')
# Skip first (possibly truncated) line
if offset > 0:
    lines = lines[1:]

print(f"File size: {file_size:,} bytes")
print(f"Read {len(lines)} lines from offset {offset:,}")

# Look for the yield pattern: LDA #$xx / STA _pc pattern
# and the dispatch_on_pc entry
# Also look for frame boundaries (NMI counter changes)

# First, let's see what PCs appear in the last 200 lines
print("\n=== LAST 100 LINES ===")
for line in lines[-100:]:
    print(line.rstrip()[:160])
