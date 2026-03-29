"""Find when compilation starts - search for first WRAM execution and 
first flash_byte_program in a larger window."""
import os, re

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"
fsize = os.path.getsize(TRACE)
print(f"Trace: {fsize:,} bytes ({fsize/1e9:.2f} GB)")

# Binary search: find first WRAM execution ($6000-$7FFF)
# Read in chunks to find the first occurrence
chunk_size = 50_000_000
found_wram = False
found_offset = 0

for offset in range(0, min(fsize, 500_000_000), chunk_size):
    with open(TRACE, 'rb') as f:
        f.seek(offset)
        raw = f.read(chunk_size)
    
    # Quick check: does this chunk contain "6000" or "60" at line start?
    # Look for pattern where a line starts with 6xxx
    idx = raw.find(b'\n6')
    while idx >= 0:
        # Check if the next chars form a WRAM address (6000-7FFF)
        try:
            addr_bytes = raw[idx+1:idx+5]
            addr = int(addr_bytes, 16)
            if 0x6000 <= addr <= 0x7FFF:
                # Found it!
                found_wram = True
                found_offset = offset + idx
                # Get some context
                line_start = raw.rfind(b'\n', 0, idx) + 1
                line_end = raw.find(b'\n', idx + 1)
                if line_end < 0:
                    line_end = len(raw)
                context = raw[max(0,idx-500):min(len(raw),idx+500)]
                context_lines = context.decode('utf-8', errors='replace').split('\n')
                print(f"\nFirst WRAM execution found at byte offset {found_offset:,}")
                print(f"Context:")
                for cl in context_lines:
                    print(f"  {cl.rstrip()[:160]}")
                break
        except:
            pass
        idx = raw.find(b'\n6', idx + 1)
    
    if found_wram:
        break
    print(f"  Scanned {offset + chunk_size:,} bytes, no WRAM exec yet...")

if not found_wram:
    print("No WRAM execution found in first 500MB!")
else:
    # Now read a region around the WRAM start to find flash_byte_program
    print(f"\n=== Reading around first WRAM execution ===")
    with open(TRACE, 'rb') as f:
        f.seek(max(0, found_offset - 1000))
        raw = f.read(10_000_000)
    
    lines = raw.decode('utf-8', errors='replace').split('\n')
    print(f"Read {len(lines)} lines")
    
    # Find first WRAM exec line
    for i, line in enumerate(lines):
        parts = line.strip().split()
        if not parts: continue
        try:
            addr = int(parts[0][:4], 16)
        except:
            continue
        if 0x6000 <= addr <= 0x7FFF:
            print(f"\nFirst WRAM line at index {i}:")
            for j in range(max(0,i-5), min(len(lines), i+30)):
                print(f"  {lines[j].rstrip()[:160]}")
            break
    
    # Find the flash_byte_program STA to $D555 (flash command)
    print(f"\n=== Flash command sequences (STA $D555) ===")
    count = 0
    for i, line in enumerate(lines):
        text = line.strip()
        if 'STA $D555' in text:
            count += 1
            if count <= 3:
                print(f"  L{i}: {text[:160]}")
                for j in range(max(0,i-3), min(len(lines), i+10)):
                    print(f"    L{j}: {lines[j].rstrip()[:160]}")
                print()
    print(f"Total STA $D555: {count}")
