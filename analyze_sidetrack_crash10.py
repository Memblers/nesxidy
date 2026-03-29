"""Analyze the 4GB exidy.txt trace log for the Side Track crash."""
import os, sys

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\exidy.txt"
fsize = os.path.getsize(TRACE)
print(f"Trace file size: {fsize:,} bytes ({fsize/1024**3:.2f} GB)")

# 1) Read last ~8KB to see the crash point
print("\n=== LAST 40 LINES ===")
with open(TRACE, 'rb') as f:
    f.seek(max(0, fsize - 8192))
    tail = f.read().decode('utf-8', errors='replace')
lines = tail.splitlines()
for line in lines[-40:]:
    print(line[:160])

# 2) Search backward for the compilation phase.
#    Our new ir_emit_direct_branch_placeholder is at $DE39 in CPU space.
#    The OLD trampoline (that shouldn't exist) was at $DD98 calling JSR $843E.
#    Search for both to see which one is actually being called.
print("\n=== SEARCHING FOR ir_emit_direct_branch_placeholder CALLS ===")
print("Looking for $DE39 (new fixed-bank) and $DD98/$843E (old trampoline)...")

# Read last 20MB to find compilation context
chunk_size = 20 * 1024 * 1024
with open(TRACE, 'rb') as f:
    f.seek(max(0, fsize - chunk_size))
    chunk = f.read().decode('utf-8', errors='replace')

chunk_lines = chunk.splitlines()
print(f"Read {len(chunk_lines):,} lines from last {chunk_size//1024//1024}MB")

de39_count = 0
dd98_count = 0
jsr_843e_count = 0
brk_9a72 = []

for i, line in enumerate(chunk_lines):
    if 'DE39' in line and ('JSR' in line or 'DE39' in line[:8]):
        de39_count += 1
        if de39_count <= 3:
            print(f"  DE39 hit #{de39_count}: {line[:160]}")
    if 'DD98' in line and ('JSR' in line or 'DD98' in line[:8]):
        dd98_count += 1
        if dd98_count <= 3:
            print(f"  DD98 hit #{dd98_count}: {line[:160]}")
    if '$843E' in line and 'JSR' in line:
        jsr_843e_count += 1
        if jsr_843e_count <= 3:
            print(f"  JSR $843E #{jsr_843e_count}: {line[:160]}")
    if '9A72' in line[:8]:
        brk_9a72.append(i)

print(f"\nDE39 (new func) calls: {de39_count}")
print(f"DD98 (old trampoline) calls: {dd98_count}")
print(f"JSR $843E (bank17 _b17) calls: {jsr_843e_count}")
print(f"Execution at $9A72 (crash addr): {len(brk_9a72)} times")

# 3) Show context around the crash
if brk_9a72:
    idx = brk_9a72[-1]
    print(f"\n=== CONTEXT AROUND CRASH (line {idx} in chunk) ===")
    start = max(0, idx - 20)
    end = min(len(chunk_lines), idx + 5)
    for j in range(start, end):
        marker = " >>>" if j == idx else "    "
        print(f"{marker} {chunk_lines[j][:160]}")

# 4) Find writes to cache_code ($6CB4+) in the last 20MB
print("\n=== WRITES TO cache_code ($6CB4-$6CF0) ===")
cache_writes = 0
for i, line in enumerate(chunk_lines):
    if 'STA' in line and ('$6CB' in line or '$6CC' in line or '$6CD' in line):
        if '[$6C' in line:  # indirect write
            cache_writes += 1
            if cache_writes <= 10:
                print(f"  {line[:160]}")
print(f"Total indirect writes to $6CB4+ range: {cache_writes}")
