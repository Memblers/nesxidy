"""Get detailed trace context around the write of $00 to cache_code[0][2].
The write happens at trace offset ~4,172,521,278."""
import os

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\exidy.txt"

# The corrupting write is at offset 4,172,521,278
# Let's read 10KB before and 2KB after
TARGET = 4172521278
BEFORE = 20000
AFTER = 5000

with open(TRACE, 'rb') as f:
    start = max(0, TARGET - BEFORE)
    f.seek(start)
    data = f.read(BEFORE + AFTER)

text = data.decode('ascii', errors='replace')
lines = text.split('\n')

# Find the line containing the write
target_line_idx = None
for i, line in enumerate(lines):
    if '9A89' in line and '6CB6' in line:
        target_line_idx = i
        break

if target_line_idx is None:
    print("Target line not found in this range!")
    # Print last 50 lines anyway
    for line in lines[-50:]:
        print(line)
else:
    # Print 100 lines before and 30 after
    start_idx = max(0, target_line_idx - 100)
    end_idx = min(len(lines), target_line_idx + 30)
    
    print(f"=== Context around the corrupting write (line {target_line_idx}) ===")
    print(f"=== Showing lines {start_idx} to {end_idx} ===\n")
    
    for i in range(start_idx, end_idx):
        marker = " >>>" if i == target_line_idx else "    "
        print(f"{marker} {lines[i]}")
