import sys

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"

# The [$975C] write is at byte offset ~4,785,552,061 (from newcrash11).
# The code writes to bank 4 might be much earlier if the SA compile writes
# many blocks' code before publishing any PC tables.
# Let's scan a 50MB window before the PC table write.

CENTER = 4_785_552_061
START = max(0, CENTER - 50_000_000)
LENGTH = 51_000_000

print(f"Scanning {LENGTH/1e6:.1f}MB window for bank-4 flash writes to $9B00-$9C00")
print(f"  Range: byte {START} to {START+LENGTH}")
sys.stdout.flush()

hits = []
line_count = 0
with open(TRACE, 'rb') as f:
    f.seek(START)
    leftover = b""
    bytes_read = 0
    
    while bytes_read < LENGTH:
        chunk = f.read(min(10_000_000, LENGTH - bytes_read))
        if not chunk:
            break
        data = leftover + chunk
        last_nl = data.rfind(b'\n')
        if last_nl < 0:
            leftover = data
            bytes_read += len(chunk)
            continue
        leftover = data[last_nl+1:]
        
        for line in data[:last_nl].split(b'\n'):
            line_count += 1
            if b'6041' not in line:
                continue
            line_str = line.decode('utf-8', errors='replace')
            if '6041' not in line_str[:8]:
                continue
            if 'STA (r2),Y' not in line_str:
                continue
            
            x_pos = line_str.find('X:')
            if x_pos < 0: continue
            try: x_val = int(line_str[x_pos+2:x_pos+4], 16)
            except: continue
            
            if x_val != 0x04: continue
            
            bracket_pos = line_str.find('[$')
            if bracket_pos < 0: continue
            try: eff_addr = int(line_str[bracket_pos+2:bracket_pos+6], 16)
            except: continue
            
            if eff_addr < 0x9B00 or eff_addr > 0x9C00: continue
            
            a_pos = line_str.find('A:')
            try: a_val = int(line_str[a_pos+2:a_pos+4], 16)
            except: continue
            
            fr_pos = line_str.find('Fr:')
            frame = "?"
            if fr_pos >= 0:
                fr_end = line_str.find(' ', fr_pos+3)
                if fr_end < 0: fr_end = len(line_str)
                frame = line_str[fr_pos+3:fr_end]
            
            hits.append((line_count, frame, eff_addr, a_val))
        
        bytes_read += len(chunk)
        print(f"  {bytes_read/1e6:.0f}MB scanned, {line_count} lines, {len(hits)} hits")
        sys.stdout.flush()

print(f"\nFound {len(hits)} bank-4 writes to $9B00-$9C00")
for idx, frame, addr, data in hits:
    print(f"  line ~{idx} Fr:{frame}  [{addr:04X}] = ${data:02X}")

print("\nDone.")
