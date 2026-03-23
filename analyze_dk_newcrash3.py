"""Trace how native address $9BA0 was published for guest PC $CBAE.

The PC table for $CBAE:
  Flag bank: ($CBAE >> 14) + $1B = $1E, address: $CBAE & $3FFF = $0BAE → $8BAE
  Native addr bank: ($CBAE >> 13) + $13 = $19, address: ($CBAE << 1) & $3FFF = $175C → $975C

flash_byte_program writes: STA $C000 (bankswitch) then 5-byte flash command sequence.
Look for the writes that program the PC table entries for $CBAE.

Also find the block header at $9B98 (or wherever) and the compile of $CBAE.
"""
import os, re

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"
fsize = os.path.getsize(TRACE)

# We need to search a LOT of the trace to find compilation.
# The SA compile happens early. Let's read the first 100MB.
read_bytes = 100_000_000
with open(TRACE, 'rb') as f:
    raw = f.read(read_bytes)

lines = raw.decode('utf-8', errors='replace').split('\n')
print(f"Read {len(lines)} lines from first {read_bytes/1e6:.0f}MB")

# Look for patterns indicating compilation of $CBAE:
# 1. _pc being set to $CBAE (STA $51 with A=$AE, then STA $52 with A=$CB)
# 2. flash_byte_program writes to PC table for $CBAE
#    - The flag write to $8BAE in bank $1E  
#    - The addr write to $975C/$975D in bank $19
# 3. Cache entry PC = $CBAE (LDA #$AE / STA cache_entry_pc_lo)

# flash_byte_program is in WRAM ($6000-$7FFF). It does:
# STA $C000 (bankswitch to target bank)
# LDA #$AA / STA $D555 / LDA #$55 / STA $AAAA / LDA #$A0 / STA $D555
# LDA data / STA addr
# So the final STA writes the data byte to the target address.

# Strategy: Find all STA to $975C, $975D (native addr table) and $8BAE (flag table)
# These would be the flash_byte_program writes for $CBAE's PC table entries.

pc_table_writes = []  # (line, addr_written_to, text)
bankswitch_to_19 = []
bankswitch_to_1E = []
compile_cbae = []

current_bank = None  # track last STA $C000 value

for i, line in enumerate(lines):
    text = line.strip()
    if not text:
        continue
    
    # Track bankswitches
    if 'STA $C000' in text:
        # Extract A register value
        m = re.search(r'A:([0-9A-Fa-f]{2})', text)
        if m:
            current_bank = int(m.group(1), 16)
    
    # Look for STA to $975C or $975D (in bank $19)
    if ('STA $975C' in text or 'STA $975D' in text) and current_bank == 0x19:
        m = re.search(r'A:([0-9A-Fa-f]{2})', text)
        aval = m.group(1) if m else '??'
        pc_table_writes.append((i, text[:160], f"bank=${current_bank:02X} A=${aval}"))
    
    # Look for STA to $8BAE (flag for $CBAE) in bank $1E
    if 'STA $8BAE' in text and current_bank == 0x1E:
        m = re.search(r'A:([0-9A-Fa-f]{2})', text)
        aval = m.group(1) if m else '??'
        pc_table_writes.append((i, text[:160], f"FLAG bank=${current_bank:02X} A=${aval}"))
    
    # Look for _pc being set to $CBAE
    if 'STA _pc' in text or 'STA $51' in text or 'STA $0051' in text:
        m = re.search(r'A:([0-9A-Fa-f]{2})', text)
        if m and m.group(1).upper() == 'AE':
            compile_cbae.append((i, 'pc_lo', text[:160]))
    
    # Look for cache_entry_pc_lo being set with $AE
    if ('$94' in text or 'cache_entry' in text.lower()) and 'STA' in text:
        m = re.search(r'A:([0-9A-Fa-f]{2})', text)
        if m and m.group(1).upper() == 'AE':
            compile_cbae.append((i, 'cache_entry', text[:160]))

print(f"\n=== PC TABLE WRITES FOR $CBAE ===")
for ln, text, info in pc_table_writes:
    print(f"  L{ln}: {info} | {text}")

print(f"\n=== _pc = $xxAE writes (first 30) ===")
for ln, typ, text in compile_cbae[:30]:
    print(f"  L{ln} [{typ}]: {text}")

# Also look for STA $9BA0 or nearby in bank 4 (the actual code write)
print(f"\n=== Looking for writes near $9BA0 in flash ===")
writes_9bxx = []
for i, line in enumerate(lines):
    text = line.strip()
    if not text:
        continue
    if 'STA $9B' in text:
        parts = text.split()
        if parts:
            try:
                addr = int(parts[0][:4], 16)
                if 0x6000 <= addr <= 0x7FFF:  # from WRAM (flash_byte_program)
                    writes_9bxx.append((i, text[:160]))
            except:
                pass

print(f"STA $9Bxx from WRAM: {len(writes_9bxx)}")
for ln, text in writes_9bxx[:50]:
    print(f"  L{ln}: {text}")
