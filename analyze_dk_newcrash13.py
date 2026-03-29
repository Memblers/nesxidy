import sys

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"

target_range_lo = 0x9B80
target_range_hi = 0x9BC0
target_bank_x = 0x04

CHUNK = 200 * 1024 * 1024
hits = []

print(f"Scanning for flash writes to bank 4, addresses ${target_range_lo:04X}-${target_range_hi:04X}")
sys.stdout.flush()

with open(TRACE, 'rb') as f:
    f.seek(0, 2)
    file_size = f.tell()
    f.seek(0)
    
    leftover = b""
    offset = 0
    
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
        chunk_text = data[:last_nl]
        
        for line in chunk_text.split(b'\n'):
            if b'6041' not in line:
                continue
            line_str = line.decode('utf-8', errors='replace')
            
            if '6041' not in line_str[:8]:
                continue
            if 'STA (r2),Y' not in line_str:
                continue
            
            x_pos = line_str.find('X:')
            if x_pos < 0:
                continue
            try:
                x_val = int(line_str[x_pos+2:x_pos+4], 16)
            except:
                continue
            
            if x_val != target_bank_x:
                continue
            
            bracket_pos = line_str.find('[$')
            if bracket_pos < 0:
                continue
            try:
                eff_addr = int(line_str[bracket_pos+2:bracket_pos+6], 16)
            except:
                continue
            
            if eff_addr < target_range_lo or eff_addr > target_range_hi:
                continue
            
            a_pos = line_str.find('A:')
            if a_pos < 0:
                continue
            try:
                a_val = int(line_str[a_pos+2:a_pos+4], 16)
            except:
                continue
            
            fr_pos = line_str.find('Fr:')
            frame = "?"
            if fr_pos >= 0:
                fr_end = line_str.find(' ', fr_pos+3)
                if fr_end < 0:
                    fr_end = len(line_str)
                frame = line_str[fr_pos+3:fr_end]
            
            hits.append((frame, eff_addr, a_val, line_str.strip()))
        
        offset += len(raw)
        gb = offset / (1024*1024*1024)
        print(f"  Scanned {gb:.1f}GB... ({len(hits)} hits so far)")
        sys.stdout.flush()

print(f"\nFound {len(hits)} writes to bank 4, ${target_range_lo:04X}-${target_range_hi:04X}")
for frame, addr, data, line in hits:
    print(f"  Fr:{frame}  [{addr:04X}] = ${data:02X}  | {line[:120]}")

if hits:
    addr_map = {}
    for frame, addr, data, line in hits:
        if addr not in addr_map:
            addr_map[addr] = []
        addr_map[addr].append((frame, data))
    
    print(f"\nAddress summary:")
    for addr in sorted(addr_map.keys()):
        writes = addr_map[addr]
        vals = ' '.join(f"Fr:{f}=${d:02X}" for f, d in writes)
        print(f"  ${addr:04X}: {len(writes)} writes: {vals}")

print("\nDone.")
