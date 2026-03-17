import sys

f = open(r'C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\exidy.txt', 'rb')
f.seek(0, 2)
size = f.tell()

# Read last 50MB to get more context
chunk = 50 * 1024 * 1024
f.seek(max(0, size - chunk))
f.readline()  # skip partial line
data = f.read().decode('utf-8', errors='replace')
f.close()
lines = data.splitlines()
print(f'Read {len(lines)} lines from last {chunk//1024//1024}MB of {size:,} bytes')

# Find all dispatch_on_pc calls and analyze them
dispatches = []
for i, line in enumerate(lines):
    if 'JSR _dispatch_on_pc' in line:
        pc_lo = pc_hi = '??'
        for j in range(max(0, i-6), i):
            if 'STA _pc' in lines[j]:
                idx = lines[j].find('A:')
                if idx >= 0:
                    pc_lo = lines[j][idx+2:idx+4]
            if 'STA $5C' in lines[j]:
                idx = lines[j].find('A:')
                if idx >= 0:
                    pc_hi = lines[j][idx+2:idx+4]
        # Find native address
        native = '????'
        for j in range(i+1, min(i+80, len(lines))):
            if '62DA' in lines[j][:6] and 'JMP' in lines[j]:
                if j+1 < len(lines):
                    addr = lines[j+1].strip()[:4]
                    native = addr
                break
        # Extract frame number
        frame = '????'
        idx = line.find('Fr:')
        if idx >= 0:
            frame = line[idx+3:].split()[0]
        dispatches.append((i, pc_hi + pc_lo, native, frame))

print(f'Total dispatch_on_pc calls: {len(dispatches)}')
print(f'\nFirst 20 dispatches:')
for idx, (linenum, pc, native, frame) in enumerate(dispatches[:20]):
    print(f'  #{idx+1}: guest=${pc} -> native=${native} Fr:{frame} (line {linenum})')

print(f'\nLast 20 dispatches:')
for idx, (linenum, pc, native, frame) in enumerate(dispatches[-20:]):
    print(f'  #{len(dispatches)-19+idx}: guest=${pc} -> native=${native} Fr:{frame} (line {linenum})')

# Find any dispatches to bad addresses (outside valid Exidy ROM range)
print(f'\nDispatches outside $0000-$3FFF range:')
for idx, (linenum, pc, native, frame) in enumerate(dispatches):
    try:
        pc_val = int(pc, 16)
        if pc_val > 0x3FFF and pc_val < 0xFF00:
            print(f'  #{idx+1}: guest=${pc} -> native=${native} Fr:{frame} (line {linenum})')
    except:
        print(f'  #{idx+1}: guest=${pc} (parse error) Fr:{frame}')

# Find any dispatches resolving to $0000
print(f'\nDispatches resolving to native $0000:')
for idx, (linenum, pc, native, frame) in enumerate(dispatches):
    if native == '0000':
        print(f'  #{idx+1}: guest=${pc} -> native=${native} Fr:{frame} (line {linenum})')
        # Print surrounding context
        start = max(0, linenum - 10)
        for j in range(start, min(linenum + 5, len(lines))):
            print(f'    {j:06d}: {lines[j][:130]}')
