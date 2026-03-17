import os, time

# Analyze how the crash unfolds by reading a larger portion
f = open(r'C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\exidy.txt', 'rb')
f.seek(0, 2)
size = f.tell()

# Read last 200MB to find the FIRST time $6A76 appears as a guest PC
chunk = 200 * 1024 * 1024
f.seek(max(0, size - chunk))
f.readline()  # skip partial
data = f.read().decode('utf-8', errors='replace')
f.close()
lines = data.splitlines()
print(f'Read {len(lines)} lines from last {chunk//1024//1024}MB')

# Find ALL places where _pc is set to $76 and _pc+1 ($5C) to $6A
print('\n=== Searching for _pc = $6A76 (STA _pc with A:76, STA $5C with A:6A) ===')
bad_pc_count = 0
for i in range(len(lines)):
    if 'STA _pc' in lines[i]:
        idx = lines[i].find('A:')
        if idx >= 0 and lines[i][idx+2:idx+4] == '76':
            # Check if the previous STA $5C had A:6A
            for j in range(max(0, i-4), i):
                if 'STA $5C' in lines[j]:
                    idx2 = lines[j].find('A:')
                    if idx2 >= 0 and lines[j][idx2+2:idx2+4] == '6A':
                        bad_pc_count += 1
                        frame = '?'
                        idx3 = lines[i].find('Fr:')
                        if idx3 >= 0:
                            frame = lines[i][idx3+3:].split()[0]
                        if bad_pc_count <= 10:
                            print(f'  #{bad_pc_count}: line {i} Fr:{frame}')
                            # Print context
                            start = max(0, i - 10)
                            end = min(len(lines), i + 5)
                            for k in range(start, end):
                                print(f'    {k}: {lines[k][:130]}')
                            print()

print(f'Total _pc=$6A76 occurrences: {bad_pc_count}')

# Also search for 'not_recompiled' or compile-related activity near $6A76
print('\n=== Searching for compile activity at PC $6A76 ===')
# Look for flash_byte_program or write_pc_flag calls near $6A76
compile_count = 0
for i, line in enumerate(lines):
    if '$6A76' in line or '6A76' in line[:6]:
        compile_count += 1
        if compile_count <= 5:
            print(f'  line {i}: {line[:130]}')

print(f'\nTotal references to $6A76: {compile_count}')

# Find the first dispatch to any PC in $6000-$7FFF range
print('\n=== First dispatch_on_pc with PC in $6000-$7FFF range ===')
found = False
for i, line in enumerate(lines):
    if 'JSR _dispatch_on_pc' in line:
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
        try:
            pc = int(pc_hi + pc_lo, 16)
            if 0x6000 <= pc <= 0x7FFF:
                frame = '?'
                idx = line.find('Fr:')
                if idx >= 0:
                    frame = line[idx+3:].split()[0]
                print(f'  guest=${pc_hi}{pc_lo} Fr:{frame} (line {i})')
                start = max(0, i-15)
                end = min(len(lines), i+3)
                for k in range(start, end):
                    print(f'    {k}: {lines[k][:130]}')
                found = True
                break
        except:
            pass
if not found:
    print('  None found (parsing may have missed some)')
