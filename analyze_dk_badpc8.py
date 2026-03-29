"""
Analyze the context around L157389 (flash epilogue that writes _pc=$C800).
Also determine what's at $C800 in the DK ROM by looking at the disassembly.
And trace the full chain from $C800 to $C805.
"""
import os, re

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"
TAIL_BYTES = 25 * 1024 * 1024  # 25MB

with open(TRACE, 'rb') as f:
    f.seek(0, 2)
    sz = f.tell()
    start = max(0, sz - TAIL_BYTES)
    f.seek(start)
    if start > 0:
        f.readline()
    lines = f.readlines()

print(f"Total lines: {len(lines)}")

# Find the STA _pc at $9280 with A:00 (the C800 epilogue)
for i, raw in enumerate(lines):
    try:
        line = raw.decode('utf-8', errors='replace').rstrip()
    except:
        continue
    if '9280' in line and 'STA' in line and 'BC:85 51' in line:
        # Found it, show 40 lines of context
        print(f"\n=== Context around flash STA _pc at $9280 (L{i}) ===")
        for j in range(max(0, i-30), min(len(lines), i+30)):
            try:
                ctx = lines[j].decode('utf-8', errors='replace').rstrip()
                marker = " <<<" if j == i else ""
                print(f"  L{j}: {ctx[:250]}{marker}")
            except:
                pass
        break

# Also find what happens immediately after the $9284 STA $52 write
# (the dispatch and compile that follows)
for i, raw in enumerate(lines):
    try:
        line = raw.decode('utf-8', errors='replace').rstrip()
    except:
        continue
    if '9284' in line and 'STA' in line and 'BC:85 52' in line and 'A:C8' in line:
        print(f"\n=== Context after flash STA $52 at $9284 (L{i}) ===")
        for j in range(i, min(len(lines), i+100)):
            try:
                ctx = lines[j].decode('utf-8', errors='replace').rstrip()
                print(f"  L{j}: {ctx[:250]}")
            except:
                pass
        break

# Now, look for what happens with _pc after the C800 dispatch
# Find the first dispatch_on_pc after the $9284 write
found_9284 = False
for i, raw in enumerate(lines):
    try:
        line = raw.decode('utf-8', errors='replace').rstrip()
    except:
        continue
    if '9284' in line and 'STA' in line and 'BC:85 52' in line and 'A:C8' in line:
        found_9284 = True
    if found_9284 and 'dispatch_on_pc' in line:
        print(f"\n=== First dispatch_on_pc after $9284 write: L{i} ===")
        for j in range(i, min(len(lines), i+80)):
            try:
                ctx = lines[j].decode('utf-8', errors='replace').rstrip()
                print(f"  L{j}: {ctx[:250]}")
            except:
                pass
        break

# Also search for what the interpreter does with $C800
# Look for interpret_6502 call or bankswitch_prg near the dispatch
print("\n=== Looking for interpret/compile decision after C800 dispatch ===")
found_dispatch = False
for i, raw in enumerate(lines):
    try:
        line = raw.decode('utf-8', errors='replace').rstrip()
    except:
        continue
    if found_9284 and 'dispatch_on_pc' in line and not found_dispatch:
        found_dispatch = True
        # Look for "interpret" or "compile" keywords in the next 500 lines
        for j in range(i, min(len(lines), i+500)):
            try:
                ctx = lines[j].decode('utf-8', errors='replace').rstrip()
                if 'interpret' in ctx.lower() or 'STA _pc' in ctx or 'STA $51' in ctx or 'BC:85 51' in ctx:
                    print(f"  L{j}: {ctx[:250]}")
            except:
                pass
        break
