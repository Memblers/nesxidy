import sys

f = open(r'C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\exidy.txt', 'rb')
f.seek(0, 2)
size = f.tell()

# Read last 16KB
f.seek(max(0, size - 16384))
f.readline()
data = f.read().decode('utf-8', errors='replace')
f.close()
lines = data.splitlines()
print(f'Size: {size:,} bytes ({size/1048576:.1f} MB)')
print(f'Last {len(lines)} lines')

# Find crash
crash_line = -1
for i, line in enumerate(lines):
    if 'JMP PLATFORM_NES' in line or ('BRK' in line and line.strip().startswith('0000')):
        crash_line = i
        break

if crash_line >= 0:
    start = max(0, crash_line - 50)
    end = min(len(lines), crash_line + 5)
    print(f'\n=== Crash at line {crash_line} ===')
    for i in range(start, end):
        print(f'{i:06d}: {lines[i][:150]}')
else:
    print('\n=== Last 60 lines (no JMP $0000 / BRK crash found) ===')
    for i, line in enumerate(lines[-60:]):
        print(f'{len(lines)-60+i:06d}: {line[:150]}')

# Find dispatch_on_pc calls
print('\n=== dispatch_on_pc calls in tail ===')
for i, line in enumerate(lines):
    if 'JSR _dispatch_on_pc' in line:
        pc_lo = pc_hi = '??'
        for j in range(max(0, i-8), i):
            if 'STA _pc' in lines[j]:
                idx = lines[j].find('A:')
                if idx >= 0: pc_lo = lines[j][idx+2:idx+4]
            if 'STA $5C' in lines[j]:
                idx = lines[j].find('A:')
                if idx >= 0: pc_hi = lines[j][idx+2:idx+4]
        frame = '?'
        idx = line.find('Fr:')
        if idx >= 0: frame = line[idx+3:].split()[0]
        
        # Trace the flag byte, flag bank, jt bank, native addr
        flag_byte = flag_bank = jt_bank = '??'
        native_lo = native_hi = '??'
        sta_c000_count = 0
        for j in range(i+1, min(i+70, len(lines))):
            if 'STA $C000' in lines[j]:
                sta_c000_count += 1
                idx2 = lines[j].find('A:')
                if idx2 >= 0:
                    val = lines[j][idx2+2:idx2+4]
                    if sta_c000_count == 1: flag_bank = val
                    elif sta_c000_count == 2: jt_bank = val
            elif 'LDA (addr_lo),Y' in lines[j] and flag_byte == '??':
                idx2 = lines[j].find('= $')
                if idx2 >= 0: flag_byte = lines[j][idx2+3:idx2+5]
            elif 'STA $62DB' in lines[j]:
                idx2 = lines[j].find('A:')
                if idx2 >= 0: native_lo = lines[j][idx2+2:idx2+4]
            elif 'STA $62DC' in lines[j]:
                idx2 = lines[j].find('A:')
                if idx2 >= 0: native_hi = lines[j][idx2+2:idx2+4]
            elif 'not_recompiled' in lines[j] or 'RTS' in lines[j][:20]:
                break
        
        print(f'  guest=${pc_hi}{pc_lo} Fr:{frame} flag_bank=${flag_bank} flag=${flag_byte} jt_bank=${jt_bank} native=${native_hi}{native_lo}')
