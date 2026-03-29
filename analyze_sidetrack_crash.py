"""Analyze Side Track crash from exidy.txt trace log."""
import os

f = r'C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\exidy.txt'
size = os.path.getsize(f)
print(f"File size: {size:,} bytes")

# Read last 5MB to find the compilation of the crashing block
with open(f, 'rb') as fh:
    fh.seek(max(0, size - 5_000_000))
    data = fh.read()
lines = data.decode('utf-8', errors='replace').splitlines()
print(f"Got {len(lines)} lines from last 5MB")

# The crash: dispatch jumps to $9A70 in bank 4 flash
# $9A70 has CMP #$23 (C9 23) then BRK (00) - uninitialized flash
# Guest PC = $32DF

# 1. Look for the compilation of block at guest PC $32DF
# The compile path stores the guest PC and writes flash bytes
compile_refs = []
for i, line in enumerate(lines):
    if '32DF' in line:
        compile_refs.append((i, line.rstrip()[:160]))

print(f"\nReferences to $32DF: {len(compile_refs)}")
for idx, l in compile_refs[-30:]:
    print(f"  {idx}: {l}")

# 2. Look for flash writes to addresses near $9A70
# flash_byte_program writes individual bytes - look for STA to $8xxx-$Bxxx 
# preceded by specific flash unlock sequence
print("\n\n=== Looking for writes to $9A6x-$9A7x ===")
flash_9a_writes = []
for i, line in enumerate(lines):
    if '9A7' in line or '9A6' in line:
        if 'STA' in line or 'STX' in line or 'STY' in line:
            flash_9a_writes.append((i, line.rstrip()[:160]))

print(f"Flash area writes near $9A70: {len(flash_9a_writes)}")
for idx, l in flash_9a_writes[:30]:
    print(f"  {idx}: {l}")

# 3. Find ALL BRK instructions 
print("\n\n=== BRK instructions ===")
brk_hits = []
for i, line in enumerate(lines):
    if '\tBRK' in line or ' BRK' in line:
        # Extract address
        parts = line.split()
        if parts and 'BRK' in parts:
            brk_hits.append((i, line.rstrip()[:160]))

print(f"Total BRK hits: {len(brk_hits)}")
for idx, l in brk_hits[-10:]:
    print(f"  {idx}: {l}")

# 4. Look at what blocks were compiled recently
# The compile function stores block info - look for _flash_code_address updates
print("\n\n=== Recent _flash_code_address updates ===")
fca_refs = []
for i, line in enumerate(lines):
    if '_flash_code_address' in line:
        fca_refs.append((i, line.rstrip()[:160]))

print(f"Total _flash_code_address refs: {len(fca_refs)}")
for idx, l in fca_refs[-20:]:
    print(f"  {idx}: {l}")

# 5. Check what was the last block compiled before the crash
print("\n\n=== Recent _opt2_blocks_compiled increments ===")
compiled_refs = []
for i, line in enumerate(lines):
    if '_opt2_blocks_compiled' in line:
        compiled_refs.append((i, line.rstrip()[:160]))
        
print(f"Total: {len(compiled_refs)}")
for idx, l in compiled_refs[-10:]:
    print(f"  {idx}: {l}")

# 6. Look at the block map area - specifically what was stored for guest PC $32DF
# The block map is accessed via the dispatch code 
# addr = ($32DF * 4) with high bits creating bank + address
print("\n\n=== Context around the last compile before crash ===")
if fca_refs:
    last_fca = fca_refs[-1][0]
    start = max(0, last_fca - 100)
    end = min(len(lines), last_fca + 200)
    for j in range(start, end):
        print(f"  {j}: {lines[j].rstrip()[:160]}")
