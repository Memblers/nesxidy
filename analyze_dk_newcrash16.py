import sys

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"

# We know:
# - 56 writes to bank 4 $9B80-$9BC0 at ~2.1GB (previous cycle)
# - 0 writes from 2.1GB to 4.7GB
# - PC table write for $CBAE at ~4.8GB (pre-populate)
# - Need to find pass 2 code writes at >4.8GB
#
# Scan from 4.7GB to end of trace for bank-4 writes to $9B80-$9BC0

START = 4_700_000_000
CHUNK = 200 * 1024 * 1024

print(f"Scanning from {START/1e9:.1f}GB to end for bank-4 writes to $9B80-$9BC0")
sys.stdout.flush()

hits = []
with open(TRACE, 'rb') as f:
    f.seek(0, 2)
    file_size = f.tell()
    f.seek(START)
    
    leftover = b""
    offset = START
    
    while offset < file_size:
        raw = f.read(CHUNK)
        if not raw:
            break
        data = leftover + raw
        last_nl = data.rfind(b'\n')
        if last_nl < 0:
            leftover = data
            offset += len(raw)
            continue
        leftover = data[last_nl+1:]
        
        for line in data[:last_nl].split(b'\n'):
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
            if eff_addr < 0x9B80 or eff_addr > 0x9BC0: continue
            
            a_pos = line_str.find('A:')
            try: a_val = int(line_str[a_pos+2:a_pos+4], 16)
            except: continue
            
            fr_pos = line_str.find('Fr:')
            frame = "?"
            if fr_pos >= 0:
                fr_end = line_str.find(' ', fr_pos+3)
                if fr_end < 0: fr_end = len(line_str)
                frame = line_str[fr_pos+3:fr_end]
            
            hits.append((frame, eff_addr, a_val))
        
        offset += len(raw)
        gb = offset / 1e9
        print(f"  {gb:.1f}GB ({len(hits)} hits)")
        sys.stdout.flush()

print(f"\nFound {len(hits)} bank-4 writes to $9B80-$9BC0 after 4.7GB")
for frame, addr, data in hits:
    print(f"  Fr:{frame}  [{addr:04X}] = ${data:02X}")

if hits:
    from collections import defaultdict
    addr_map = defaultdict(list)
    for frame, addr, data in hits:
        addr_map[addr].append((frame, data))
    print(f"\nAddress summary:")
    for addr in sorted(addr_map.keys()):
        writes = addr_map[addr]
        vals = ' '.join(f"Fr:{f}=${d:02X}" for f, d in writes)
        print(f"  ${addr:04X}: {len(writes)} writes: {vals}")

print("\nDone.")
