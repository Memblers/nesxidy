"""
Look at the trace around the previous dispatch (L79655) and the crash dispatch (L90337).
Focus on:
1. What block was dispatched at L79655?
2. What exit_pc was set?
3. How did _pc end up as $C805?
Also check ALL JMP _dispatch_on_pc (not just JSR) in the gap.
"""
import os, re

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"
TAIL_BYTES = 15 * 1024 * 1024  # 15MB

with open(TRACE, 'rb') as f:
    f.seek(0, 2)
    sz = f.tell()
    start = max(0, sz - TAIL_BYTES)
    f.seek(start)
    if start > 0:
        f.readline()
    lines = f.readlines()

print(f"Total lines: {len(lines)}")

# Find ALL dispatch_on_pc references (JSR, JMP, or any mention)
dispatches = []
for i, raw in enumerate(lines):
    try:
        line = raw.decode('utf-8', errors='replace').rstrip()
    except:
        continue
    if 'dispatch_on_pc' in line.lower() or 'JSR $623A' in line or 'JMP $623A' in line:
        dispatches.append((i, line))

print(f"\n=== ALL dispatch_on_pc references ({len(dispatches)} total) ===")
for idx, (i, line) in enumerate(dispatches[-20:]):
    dist = len(lines) - i
    print(f"  L{i} (end-{dist}): {line[:200]}")

# Now look at the 200 lines around L79655 equivalent (prev dispatch)
# and the 200 lines before the crash dispatch
# Find the last two JSR/JMP _dispatch_on_pc
jsr_dispatches = [(i, l) for i, l in dispatches if 'JSR' in l or 'JMP' in l]
print(f"\n=== JSR/JMP _dispatch_on_pc ({len(jsr_dispatches)} total) ===")
for idx, (i, line) in enumerate(jsr_dispatches[-10:]):
    print(f"  L{i}: {line[:200]}")

if len(jsr_dispatches) >= 2:
    prev_idx = jsr_dispatches[-2][0]
    crash_idx = jsr_dispatches[-1][0]
    
    # Show 100 lines after the previous dispatch
    print(f"\n=== 200 lines after previous dispatch (L{prev_idx}) ===")
    for i in range(prev_idx, min(prev_idx+200, len(lines))):
        try:
            line = lines[i].decode('utf-8', errors='replace').rstrip()
            if line:
                print(f"  L{i}: {line[:250]}")
        except:
            pass
    
    # Show 200 lines before the crash dispatch
    print(f"\n=== 200 lines before crash dispatch (L{crash_idx}) ===")
    for i in range(max(0, crash_idx-200), crash_idx+1):
        try:
            line = lines[i].decode('utf-8', errors='replace').rstrip()
            if line:
                print(f"  L{i}: {line[:250]}")
        except:
            pass

# Also search for ALL STA _pc or STA $51 between the two dispatches
if len(jsr_dispatches) >= 2:
    prev_idx = jsr_dispatches[-2][0]
    crash_idx = jsr_dispatches[-1][0]
    pc_stores = []
    for i in range(prev_idx, crash_idx):
        try:
            line = lines[i].decode('utf-8', errors='replace').rstrip()
        except:
            continue
        if ('STA _pc' in line or 'STA $51' in line or 'STA $52' in line) and ('=' in line):
            pc_stores.append((i, line))
    
    print(f"\n=== ALL STA _pc/$51/$52 between dispatches ({len(pc_stores)}) ===")
    for i, line in pc_stores[-50:]:
        print(f"  L{i}: {line[:200]}")
