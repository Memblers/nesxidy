"""
Trace back from crash to find who set _pc = $C805.
Search for ALL writes to ZP $51 (pc lo) in the last 10K lines.
Also search for STA $51 without relying on label names.
"""
import os, re

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"
TAIL_BYTES = 10 * 1024 * 1024  # 10MB

with open(TRACE, 'rb') as f:
    f.seek(0, 2)
    sz = f.tell()
    start = max(0, sz - TAIL_BYTES)
    f.seek(start)
    if start > 0:
        f.readline()  # skip partial
    lines = f.readlines()

print(f"Total lines: {len(lines)}")

# Find any write to $51 (ZP address of _pc)
# Patterns: STA $51, STX $51, STY $51, or with label STA _pc
# Also look for any store that includes "= $" followed by the old value and "A:05"
# In trace format: "STA _pc = $XX  A:05" or "STA $51 = $XX  A:05"

pc_writes = []
dispatch_calls = []

for i, raw in enumerate(lines):
    try:
        line = raw.decode('utf-8', errors='replace').rstrip()
    except:
        continue
    
    # Look for any store to $51 or _pc (regardless of value)
    # The trace format for ZP stores: "ST[AXY] _pc = $XX" or "ST[AXY] $51 = $XX"
    if ('$51' in line or '_pc' in line) and ('STA' in line or 'STX' in line or 'STY' in line):
        # Exclude _pc_jump, _pc+1, etc but keep _pc = or $51 =
        if ' _pc =' in line or ' _pc=' in line or ' $51 =' in line or ' $51=' in line:
            # Check if this is writing pc_lo with value $05
            if ' A:05' in line or ' X:05' in line or ' Y:05' in line:
                pc_writes.append((i, line))
    
    # Also look for $52 or _pc+1 with value $C8
    if ('$52' in line or '_pc+1' in line) and ('STA' in line or 'STX' in line or 'STY' in line):
        if ' A:C8' in line or ' X:C8' in line or ' Y:C8' in line:
            pc_writes.append((i, 'PC_HI: ' + line))
    
    # Look for dispatch_on_pc calls
    if 'dispatch_on_pc' in line.lower() or '$623A' in line or '$623a' in line:
        dispatch_calls.append((i, line))

print(f"\n=== Writes to _pc/$51 with value $05 (lo) or $52 with $C8 (hi) ===")
print(f"Found {len(pc_writes)} matches")
for idx, (i, line) in enumerate(pc_writes[-50:]):
    dist = len(lines) - i
    print(f"  L{i} (end-{dist}): {line[:200]}")

print(f"\n=== dispatch_on_pc calls ===")
print(f"Found {len(dispatch_calls)} total")
for idx, (i, line) in enumerate(dispatch_calls[-10:]):
    dist = len(lines) - i
    print(f"  L{i} (end-{dist}): {line[:200]}")

# Now let's look at the last 500 lines for ANY trace of what happens
print(f"\n=== Last 200 lines of trace ===")
for i in range(max(0, len(lines)-200), len(lines)):
    try:
        line = lines[i].decode('utf-8', errors='replace').rstrip()
        if line:
            print(f"  L{i}: {line[:250]}")
    except:
        pass
