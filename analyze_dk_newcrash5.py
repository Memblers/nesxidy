"""Track bank register and find PC table writes for $CBAE.
The flash_byte_program sequence in WRAM does:
  STA $C000 (bank select)
  LDA #$AA / STA $D555
  LDA #$55 / STA $AAAA  
  LDA #$A0 / STA $D555
  LDA <data> / STA <target_addr>

We need to find STA <target> where target=$975C when bank $19 is mapped,
or STA $8BAE when bank $1E is mapped.

Actually, let me just look at the flash_byte_program routine itself.
First, find the WRAM code. flash_byte_program is copied to WRAM at boot.
"""
import os, re

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"
fsize = os.path.getsize(TRACE)

# Read first 200MB
read_bytes = 200_000_000
with open(TRACE, 'rb') as f:
    raw = f.read(read_bytes)
lines = raw.decode('utf-8', errors='replace').split('\n')
print(f"Read {len(lines)} lines from first {read_bytes/1e6:.0f}MB")

# Step 1: Find flash_byte_program.
# It lives in WRAM $6000-$7FFF. Look for the unlock sequence:
# STA $D555 with A=$AA, then STA $AAAA with A=$55
# But the addresses $D555 and $AAAA are in the $8000-$FFFF range.
# The flash unlock is: $5555=$AA, $2AAA=$55, $5555=$A0
# But the NES mapper mirrors these: with $8000 base, $5555 → $D555, $2AAA → $AAAA

# Find first occurrence of STA $D555 from WRAM
print("=== Finding flash_byte_program routine ===")
found_fbp = False
fbp_addr = None
for i, line in enumerate(lines[:50000]):
    text = line.strip()
    if not text: continue
    parts = text.split()
    if not parts: continue
    try:
        addr = int(parts[0][:4], 16)
    except:
        continue
    if 0x6000 <= addr <= 0x7FFF and 'STA $D555' in text:
        print(f"  Found STA $D555 at PC=${addr:04X}: L{i}: {text[:160]}")
        # Show context
        for j in range(max(0, i-5), min(len(lines), i+15)):
            print(f"    L{j}: {lines[j].strip()[:160]}")
        fbp_addr = addr
        found_fbp = True
        break

if not found_fbp:
    # Try searching for any WRAM code that writes to $D555 or $AAAA
    print("  No STA $D555 from WRAM found in first 50K lines")
    print("  Looking for WRAM execution at all...")
    wram_exec = 0
    for i, line in enumerate(lines[:50000]):
        text = line.strip()
        parts = text.split()
        if not parts: continue
        try:
            addr = int(parts[0][:4], 16)
        except:
            continue
        if 0x6000 <= addr <= 0x7FFF:
            wram_exec += 1
            if wram_exec <= 5:
                print(f"    L{i}: {text[:160]}")
    print(f"  Total WRAM executions in first 50K: {wram_exec}")

# Step 2: Track bank register and find writes to PC table addresses
# When bank $19 is mapped and data is written to $975C/$975D, that's
# a PC table native address write for $CBAE.
print("\n=== Tracking bank register for PC table writes ===")
current_bank = 0
pc_table_writes = []  # (line, bank, addr, data_val)

for i, line in enumerate(lines):
    text = line.strip()
    if not text: continue
    
    # Track bankswitches
    if 'STA $C000' in text:
        m = re.search(r'A:([0-9A-Fa-f]{2})', text)
        if m:
            current_bank = int(m.group(1), 16)
        continue
    
    # Look for STA to $975C or $975D when bank 0x19 is mapped
    if current_bank == 0x19:
        if 'STA $975C' in text or 'STA $975D' in text:
            m = re.search(r'A:([0-9A-Fa-f]{2})', text)
            aval = int(m.group(1), 16) if m else -1
            addr_str = '$975C' if '$975C' in text else '$975D'
            pc_table_writes.append((i, 0x19, addr_str, aval, text[:160]))
    
    # Look for STA to $8BAE when bank 0x1E is mapped (flag)
    if current_bank == 0x1E:
        if 'STA $8BAE' in text:
            m = re.search(r'A:([0-9A-Fa-f]{2})', text)
            aval = int(m.group(1), 16) if m else -1
            pc_table_writes.append((i, 0x1E, '$8BAE', aval, text[:160]))

print(f"PC table writes for $CBAE: {len(pc_table_writes)}")
for ln, bank, addr, val, text in pc_table_writes:
    print(f"  L{ln}: bank=${bank:02X} {addr}←${val:02X} | {text}")

# Step 3: Find the compile of guest PC $CBAE
# Look for cache_entry_pc being set to $CBAE
# In the compile path: cache_entry_pc_lo[0] = lo, cache_entry_pc_hi[0] = hi
# These are ZP globals. Look for stores to them.
print("\n=== Looking for _cache_entry_pc stores for $CBAE ===")
# cache_entry_pc_lo is at ZP $94, cache_entry_pc_hi at ZP $95
# Look for STA $94 with A=$AE, STA $95 with A=$CB
for i, line in enumerate(lines[:500000]):
    text = line.strip()
    if 'STA $94' in text or 'STA $0094' in text:
        m = re.search(r'A:([0-9A-Fa-f]{2})', text)
        if m and m.group(1).upper() == 'AE':
            # Check next line for STA $95 with A=$CB
            if i + 5 < len(lines):
                for j in range(i, min(len(lines), i+10)):
                    next_text = lines[j].strip()
                    if ('STA $95' in next_text or 'STA $0095' in next_text):
                        m2 = re.search(r'A:([0-9A-Fa-f]{2})', next_text)
                        if m2 and m2.group(1).upper() == 'CB':
                            print(f"  L{i}: cache_entry_pc = $CBAE")
                            for k in range(max(0,i-3), min(len(lines), i+15)):
                                print(f"    L{k}: {lines[k].strip()[:160]}")
                            break
