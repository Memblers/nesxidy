"""Broader search: find any mention of $975C, $8BAE, $9BA0 in the trace,
and find flash_byte_program execution patterns."""
import os, re

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"
fsize = os.path.getsize(TRACE)

# Read first 100MB 
read_bytes = 100_000_000
with open(TRACE, 'rb') as f:
    raw = f.read(read_bytes)

lines = raw.decode('utf-8', errors='replace').split('\n')
print(f"Read {len(lines)} lines from first {read_bytes/1e6:.0f}MB")

# 1. Search for any line containing "975C" or "8BAE" or "9BA0"
targets = ['975C', '975D', '8BAE', '9BA0', '9BA4', '9BA9', '9BAE']
hits = {t: [] for t in targets}

for i, line in enumerate(lines[:200000]):
    for t in targets:
        if t in line or t.lower() in line:
            hits[t].append((i, line.strip()[:160]))

for t in targets:
    print(f"\n=== '{t}' occurrences: {len(hits[t])} ===")
    for ln, text in hits[t][:10]:
        print(f"  L{ln}: {text}")

# 2. Find flash_byte_program - where is it in WRAM? Look for the $5555/$AA pattern
print(f"\n=== Looking for flash command sequence ($5555 writes) in first 50K lines ===")
for i, line in enumerate(lines[:50000]):
    text = line.strip()
    if '$D555' in text and 'STA' in text:
        print(f"  L{i}: {text[:160]}")
        if i < 50000:
            # Show a few lines of context
            for j in range(max(0,i-2), min(len(lines), i+8)):
                print(f"    L{j}: {lines[j].strip()[:160]}")
            print()
        break

# 3. Find the SA compile start markers
print(f"\n=== Looking for SA compile markers ===")
for i, line in enumerate(lines[:200000]):
    text = line.strip()
    # SA compile typically starts after reset, look for sa_do_compile or similar
    # Actually let's find flash_byte_program calls by looking for the WRAM routine
    # The routine at $6000 starts with a bankswitch
    parts = text.split()
    if not parts:
        continue
    try:
        addr = int(parts[0][:4], 16)
    except:
        continue
    
    if addr == 0x6000 and i < 5000:
        print(f"  L{i}: flash_byte_program entry: {text[:160]}")

# 4. Check what's at various WRAM addresses during flash writes
# The flash_byte_program routine uses self-modifying code
# Let's find its STA instruction that writes the actual data
print(f"\n=== WRAM STA instructions in $6000-$60FF range (first 50K lines) ===")
wram_stas = {}
for i, line in enumerate(lines[:50000]):
    text = line.strip()
    parts = text.split()
    if not parts:
        continue
    try:
        addr = int(parts[0][:4], 16)
    except:
        continue
    if 0x6000 <= addr <= 0x60FF and 'STA' in text:
        key = parts[0][:4]
        if key not in wram_stas:
            wram_stas[key] = (i, text[:160])

for key in sorted(wram_stas.keys()):
    ln, text = wram_stas[key]
    print(f"  {key}: L{ln}: {text}")
