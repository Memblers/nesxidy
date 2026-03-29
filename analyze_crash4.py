import sys

f = open(r'C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\exidy.txt', 'rb')
f.seek(0, 2)
size = f.tell()

# Read last 16KB
f.seek(max(0, size - 16384))
f.readline()  # skip partial
data = f.read().decode('utf-8', errors='replace')
f.close()
lines = data.splitlines()
print(f'Size: {size:,} bytes ({size/1048576:.1f} MB)')
print(f'Last {len(lines)} lines')

# Find the crash - JMP $0000 or BRK at 0000
crash_line = -1
for i, line in enumerate(lines):
    if 'JMP PLATFORM_NES' in line or ('BRK' in line and line.strip().startswith('0000')):
        crash_line = i
        break

if crash_line >= 0:
    # Print 30 lines before crash and a few after
    start = max(0, crash_line - 40)
    end = min(len(lines), crash_line + 5)
    print(f'\n=== Crash context (line {crash_line}) ===')
    for i in range(start, end):
        print(f'{i:06d}: {lines[i][:150]}')
else:
    # Just print last 40 lines
    print('\n=== Last 40 lines (no crash pattern found) ===')
    for line in lines[-40:]:
        print(line[:150])

# Also search for dispatch_on_pc in last portion
print('\n=== dispatch_on_pc calls in last portion ===')
for i, line in enumerate(lines):
    if 'JSR _dispatch_on_pc' in line:
        # Get guest PC
        pc_lo = pc_hi = '??'
        for j in range(max(0, i-8), i):
            if 'STA _pc' in lines[j]:
                idx = lines[j].find('A:')
                if idx >= 0:
                    pc_lo = lines[j][idx+2:idx+4]
            if 'STA $5C' in lines[j]:
                idx = lines[j].find('A:')
                if idx >= 0:
                    pc_hi = lines[j][idx+2:idx+4]
        # Get frame
        frame = '?'
        idx = line.find('Fr:')
        if idx >= 0:
            frame = line[idx+3:].split()[0]
        
        # Find the flag byte read and bank switched
        flag_byte = '??'
        flag_bank = '??'
        jt_bank = '??'
        native_lo = '??'
        native_hi = '??'
        for j in range(i+1, min(i+60, len(lines))):
            # Flag bank switch: first STA $C000 after dispatch
            if 'STA $C000' in lines[j] and flag_bank == '??':
                idx2 = lines[j].find('A:')
                if idx2 >= 0:
                    flag_bank = lines[j][idx2+2:idx2+4]
            # Flag byte read: first LDA (addr_lo),Y after dispatch  
            elif 'LDA (addr_lo),Y' in lines[j] and flag_byte == '??':
                idx2 = lines[j].find('= $')
                if idx2 >= 0:
                    flag_byte = lines[j][idx2+3:idx2+5]
            # Jump table bank: second STA $C000
            elif 'STA $C000' in lines[j] and flag_bank != '??' and jt_bank == '??':
                idx2 = lines[j].find('A:')
                if idx2 >= 0:
                    jt_bank = lines[j][idx2+2:idx2+4]
            # Native address reads
            elif 'STA $62DB' in lines[j]:
                idx2 = lines[j].find('A:')
                if idx2 >= 0:
                    native_lo = lines[j][idx2+2:idx2+4]
            elif 'STA $62DC' in lines[j]:
                idx2 = lines[j].find('A:')
                if idx2 >= 0:
                    native_hi = lines[j][idx2+2:idx2+4]
        
        print(f'  guest=${pc_hi}{pc_lo} Fr:{frame} flag_bank={flag_bank} flag={flag_byte} jt_bank={jt_bank} native=${native_hi}{native_lo} (line {i})')
