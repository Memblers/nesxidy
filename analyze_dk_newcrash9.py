"""Find the flash data write instruction and search for $CBAE PC table writes.
flash_byte_program at $6000 uses STA (r2),Y for IO8(addr) = data.
The effective address appears in the trace as [$xxxx].
Search for effective addresses $975C, $975D, $8BAE."""
import os, re

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"
fsize = os.path.getsize(TRACE)

# First, find the exact STA (r2),Y instruction address
# Read around the first flash_byte_program call
with open(TRACE, 'rb') as f:
    f.seek(18_509_000)  # near first call found earlier
    raw = f.read(5000)
text = raw.decode('utf-8', errors='replace')
lines_sample = text.split('\n')

sta_r2_pc = None
for line in lines_sample:
    stripped = line.strip()
    if 'STA (r2)' in stripped or 'STA (r0)' in stripped:
        parts = stripped.split()
        if parts:
            try:
                addr = int(parts[0][:4], 16)
                if 0x6000 <= addr <= 0x60FF:
                    sta_r2_pc = addr
                    print(f"Flash data write at PC=${addr:04X}: {stripped[:160]}")
                    break
            except:
                pass

if sta_r2_pc is None:
    # Try searching more context
    print("STA (r2) not found in initial context, searching wider...")
    with open(TRACE, 'rb') as f:
        f.seek(18_500_000)
        raw = f.read(50000)
    text = raw.decode('utf-8', errors='replace')
    for line in text.split('\n'):
        stripped = line.strip()
        if 'STA (' in stripped:
            parts = stripped.split()
            if parts:
                try:
                    addr = int(parts[0][:4], 16)
                    if 0x6000 <= addr <= 0x60FF:
                        print(f"  STA indirect at ${addr:04X}: {stripped[:180]}")
                except:
                    pass

# Now search for writes where the effective address is $975C, $975D, or $8BAE
# The trace format shows: STA (r2),Y [$975C] = $xx
print(f"\nSearching entire trace for flash writes to $975C/$975D/$8BAE...")
print("(Looking for '[$975C]', '[$975D]', '[$8BAE]' in STA lines from WRAM)")

targets = {'[$975C]': [], '[$975D]': [], '[$8BAE]': []}
chunk_size = 100_000_000
line_offset = 0

for file_offset in range(0, fsize, chunk_size):
    with open(TRACE, 'rb') as f:
        f.seek(file_offset)
        raw = f.read(chunk_size + 500)
    
    text = raw.decode('utf-8', errors='replace')
    chunk_lines = text.split('\n')
    
    for i, line in enumerate(chunk_lines):
        stripped = line.strip()
        if 'STA' not in stripped:
            continue
        # Check if it's from WRAM and targets our addresses
        parts = stripped.split()
        if not parts:
            continue
        try:
            pc = int(parts[0][:4], 16)
        except:
            continue
        if pc < 0x6000 or pc > 0x60FF:
            continue
        
        for t in targets:
            if t in stripped:
                targets[t].append((line_offset + i, stripped[:180]))
    
    line_offset += len(chunk_lines)
    progress = (file_offset + chunk_size) / fsize * 100
    print(f"  {progress:.0f}% ({file_offset/1e9:.1f}GB)... hits: {sum(len(v) for v in targets.values())}")

for t, hits in targets.items():
    print(f"\n=== {t} writes: {len(hits)} ===")
    for ln, text in hits:
        print(f"  L{ln}: {text}")
