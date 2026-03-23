"""
analyze_dk_invalid.py - Find invalid opcode crash in new trace.
Check the end of the trace for the crash, and look for soft resets / abnormal flow.
"""
import os, re

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"

size = os.path.getsize(TRACE)
print(f"File size: {size:,} bytes ({size/1024/1024:.1f} MB)")

# Read last 500 lines to see the crash
print("\n=== LAST 200 LINES ===")
# Efficiently read last portion of file
tail_size = min(size, 200000)  # last ~200KB
last_lines = []
with open(TRACE, 'r', errors='replace') as f:
    f.seek(max(0, size - tail_size))
    f.readline()  # skip partial line
    for line in f:
        last_lines.append(line.rstrip())

print(f"Read {len(last_lines)} lines from tail")
for line in last_lines[-200:]:
    print(f"  {line[:150]}")

# Also count total lines and check first few lines format
print("\n=== FIRST 10 LINES ===")
with open(TRACE, 'r', errors='replace') as f:
    for i, line in enumerate(f):
        if i < 10:
            print(f"  L{i}: {line.rstrip()[:150]}")
        if i > 10:
            break
