"""Find exactly what address calls the old trampoline and what code is at DD98."""
import os

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\exidy.txt"
fsize = os.path.getsize(TRACE)

# Read last 20MB
chunk_size = 20 * 1024 * 1024
with open(TRACE, 'rb') as f:
    f.seek(max(0, fsize - chunk_size))
    chunk = f.read().decode('utf-8', errors='replace')
lines = chunk.splitlines()

# Find the first JSR $843E and show 30 lines of context before it
print("=== FIRST JSR $843E IN LAST 20MB (with context) ===")
for i, line in enumerate(lines):
    if '$843E' in line and 'JSR' in line:
        start = max(0, i - 30)
        for j in range(start, min(len(lines), i + 20)):
            marker = ">>>" if j == i else "   "
            print(f"{marker} {lines[j][:160]}")
        break

# Also find what's at DD98 in the trace - show the full trampoline
print("\n=== TRAMPOLINE CODE (entries at $DD98) ===")
dd98_seen = False
for i, line in enumerate(lines):
    if 'DD98' in line[:8] and not dd98_seen:
        dd98_seen = True
        for j in range(i, min(len(lines), i + 30)):
            print(f"   {lines[j][:160]}")
        break

# Find what calls our new function DE39
print("\n=== CALLS TO $DE39 (our new function) ===")
de39_count = 0
for i, line in enumerate(lines):
    if 'DE39' in line[:8]:
        de39_count += 1
        if de39_count <= 5:
            start = max(0, i - 3)
            for j in range(start, min(len(lines), i + 3)):
                print(f"   {lines[j][:160]}")
            print()

if de39_count == 0:
    print("   NO execution at $DE39 found!")

# Show what the mlb file says about $DD98
print("\n=== CHECKING LABEL MAP FOR DD98/DE39 ===")
with open("exidy.mlb", "r") as f:
    for line in f:
        for addr in ["5D98", "5E39", "5D28", "5D08"]:
            if addr in line:
                print(f"  {line.rstrip()}")
