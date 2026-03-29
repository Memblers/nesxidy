"""Find PC table writes for $CBAE using correct flash_byte_program addresses.
Flash command: STA $9555 ($AA), STA $AAAA ($55), STA $9555 ($A0), 
then STA $C000 (bank select), STA addr (data).

For $CBAE's PC table:
  Native addr: bank $19, addr $975C/$975D
  Flag: bank $1E, addr $8BAE
  
Bank register is: bank | mapper_chr_bank (written to $C000).
For bank $19: value = $19 | chr_bits (typically chr=0, so $19)
For bank $1E: value = $1E | chr_bits

Search the SA compile region for these writes.
"""
import os, re

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"
fsize = os.path.getsize(TRACE)

# SA compile happens around frames 4-16. Need to find the right region.
# Let's search first 500MB which should cover the early compile phase.
# Read in chunks and search for "STA $975C" or "STA $975D"

print("Searching for STA $975C, STA $975D, STA $8BAE in first 2GB...")

targets = {
    '975C': [],  # native addr lo for $CBAE  
    '975D': [],  # native addr hi for $CBAE
    '8BAE': [],  # flag for $CBAE
}

chunk_size = 100_000_000
line_offset = 0

for file_offset in range(0, min(fsize, 2_000_000_000), chunk_size):
    with open(TRACE, 'rb') as f:
        f.seek(file_offset)
        raw = f.read(chunk_size + 1000)  # overlap for line boundary
    
    text = raw.decode('utf-8', errors='replace')
    chunk_lines = text.split('\n')
    
    found_any = False
    for i, line in enumerate(chunk_lines):
        for target in targets:
            if f'STA ${target}' in line:
                global_line = line_offset + i
                targets[target].append((global_line, line.strip()[:180]))
                found_any = True
    
    line_offset += len(chunk_lines)
    progress = (file_offset + chunk_size) / 1e9
    if found_any or int(progress * 10) != int((progress - chunk_size/1e9) * 10):
        hit_count = sum(len(v) for v in targets.values())
        print(f"  Scanned {progress:.1f}GB, hits so far: {hit_count}")

print(f"\n=== STA $975C (native addr lo) — {len(targets['975C'])} hits ===")
for ln, text in targets['975C']:
    print(f"  L{ln}: {text}")

print(f"\n=== STA $975D (native addr hi) — {len(targets['975D'])} hits ===")
for ln, text in targets['975D']:
    print(f"  L{ln}: {text}")

print(f"\n=== STA $8BAE (flag) — {len(targets['8BAE'])} hits ===")
for ln, text in targets['8BAE']:
    print(f"  L{ln}: {text}")
