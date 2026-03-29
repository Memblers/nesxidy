"""
Look for ALL writes to ZP $51 and $52 in the trace around the critical regions.
Also search BROADLY for any STA to $51 with value $05, using raw byte matching.
"""
import os, re

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"
TAIL_BYTES = 20 * 1024 * 1024  # 20MB

with open(TRACE, 'rb') as f:
    f.seek(0, 2)
    sz = f.tell()
    start = max(0, sz - TAIL_BYTES)
    f.seek(start)
    if start > 0:
        f.readline()
    lines = f.readlines()

print(f"Total lines: {len(lines)}")

# Search for ANY instruction that writes to $51 or $52 with relevant values
# Be very broad: look for "85 51" (STA $51) or "85 52" (STA $52) in bytecodes
# Also look for "86 51" (STX $51) or "84 51" (STY $51)
# The trace format includes "BC:xx xx" at the end of each line

pc_lo_writes = []  # writes to $51
pc_hi_writes = []  # writes to $52

for i, raw in enumerate(lines):
    try:
        line = raw.decode('utf-8', errors='replace').rstrip()
    except:
        continue
    
    # Look for BC:85 51 (STA $51) with A:05
    if 'BC:85 51' in line and ' A:05' in line:
        pc_lo_writes.append((i, 'STA$51_A05', line))
    
    # Look for BC:85 52 (STA $52) with A:C8
    if 'BC:85 52' in line and ' A:C8' in line:
        pc_hi_writes.append((i, 'STA$52_AC8', line))
    
    # Also look for STA $51 with any value (to see pattern)
    if 'BC:85 51' in line:
        # Extract A value
        m = re.search(r'A:([0-9A-Fa-f]{2})', line)
        if m:
            a_val = m.group(1).upper()
            if a_val == '05':
                pass  # already captured above
    
    # Look for stores from flash banks (addresses $8000-$BFFF)
    if ('BC:85 51' in line or 'BC:85 52' in line):
        m = re.match(r'\s*([0-9A-Fa-f]{4})', line)
        if m:
            addr = int(m.group(1), 16)
            if 0x8000 <= addr <= 0xBFFF:
                # This is a flash-bank store to _pc
                if 'BC:85 51' in line:
                    pc_lo_writes.append((i, 'FLASH_STA$51', line))
                else:
                    pc_hi_writes.append((i, 'FLASH_STA$52', line))

print(f"\n=== Writes to $51 (pc_lo) with A=$05 ===")
for i, tag, line in pc_lo_writes:
    print(f"  L{i} [{tag}]: {line[:250]}")

print(f"\n=== Writes to $52 (pc_hi) with A=$C8 ===")
for i, tag, line in pc_hi_writes:
    print(f"  L{i} [{tag}]: {line[:250]}")

# Now look at 30 lines around each flash STA $51/$52 write
flash_writes = [(i, tag, line) for i, tag, line in pc_lo_writes + pc_hi_writes if 'FLASH' in tag]
flash_writes.sort()
print(f"\n=== Context around flash-bank STA $51/$52 writes ===")
for wi, (flash_i, tag, line) in enumerate(flash_writes):
    print(f"\n--- Flash write #{wi}: L{flash_i} [{tag}] ---")
    for j in range(max(0, flash_i-5), min(len(lines), flash_i+10)):
        try:
            ctx = lines[j].decode('utf-8', errors='replace').rstrip()
            marker = " <<<" if j == flash_i else ""
            print(f"  L{j}: {ctx[:200]}{marker}")
        except:
            pass
