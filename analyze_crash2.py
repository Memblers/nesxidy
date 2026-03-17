import sys

f = open(r'C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\exidy.txt', 'rb')
f.seek(0, 2)
size = f.tell()
# Read last 2MB
f.seek(max(0, size - 2*1024*1024))
f.readline()  # skip partial line
data = f.read().decode('utf-8', errors='replace')
f.close()
lines = data.splitlines()
print(f'Read {len(lines)} lines from last 2MB of {size:,} bytes')

# Find all JMP $0000 / BRK events and dispatch_on_pc calls
for i, line in enumerate(lines):
    if 'JMP PLATFORM_NES' in line or ('BRK' in line and line.strip()[:4] == '0000'):
        start = max(0, i-5)
        for j in range(start, min(i+3, len(lines))):
            print(f'{j:06d}: {lines[j][:140]}')
        print('---')

# Also find all dispatch_on_pc entries and what they resolved to
print("\n=== All dispatch_on_pc calls ===")
count = 0
for i, line in enumerate(lines):
    if 'JSR _dispatch_on_pc' in line:
        count += 1
        # Find the _pc and $5C values from nearby lines
        pc_lo = pc_hi = '??'
        for j in range(max(0,i-6), i):
            if 'STA _pc' in lines[j]:
                idx = lines[j].find('A:')
                if idx >= 0:
                    pc_lo = lines[j][idx+2:idx+4]
            if 'STA $5C' in lines[j]:
                idx = lines[j].find('A:')
                if idx >= 0:
                    pc_hi = lines[j][idx+2:idx+4]
        # Find what native address was resolved (look for JMP after dispatch)
        native = '????'
        for j in range(i+1, min(i+80, len(lines))):
            if 'JMP PLATFORM_NES' in lines[j] or ('62DA' in lines[j][:4] and 'JMP' in lines[j]):
                # Next line has the jump target
                if j+1 < len(lines):
                    addr = lines[j+1].strip()[:4]
                    native = addr
                break
        if count <= 20 or native == '0000' or pc_hi not in ['00','01','02','03']:
            print(f'  #{count}: guest=${pc_hi}{pc_lo} -> native=${native} (line {i})')

print(f'\nTotal dispatch_on_pc calls: {count}')
