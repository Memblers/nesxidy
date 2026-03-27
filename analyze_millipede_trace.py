"""Analyze Millipede trace log to understand recompiler hang."""
import re

f = r'C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\millipede.txt'

# Track key metrics across the trace  
frame_changes = []  # (line_num, frame_num)
addr_ranges = {}    # range -> count
last_frame = None
unique_addrs_8000 = set()

# Scan through the trace
with open(f, 'r') as fh:
    for i, line in enumerate(fh):
        # Extract address
        parts = line.split()
        if not parts:
            continue
        try:
            addr = int(parts[0], 16)
        except:
            continue
        
        # Track frame changes
        m = re.search(r'Fr:(\d+)', line)
        if m:
            fr = int(m.group(1))
            if fr != last_frame:
                frame_changes.append((i, fr))
                last_frame = fr
        
        # Track address ranges
        if 0x8000 <= addr <= 0xBFFF:
            unique_addrs_8000.add(addr)
            bank = (addr - 0x8000) >> 12  # 4KB sectors
            key = f'flash_{bank}'
        elif 0xC000 <= addr <= 0xFFFF:
            key = 'fixed_bank'
        elif 0x6000 <= addr <= 0x7FFF:
            key = 'wram'
        else:
            key = f'other_{addr>>12:X}'
        addr_ranges[key] = addr_ranges.get(key, 0) + 1

        if i % 5000000 == 0 and i > 0:
            print(f'Progress: {i:,} lines, frame {last_frame}')

total = i + 1
print(f'\nTotal lines: {total:,}')
print(f'\nFrame changes ({len(frame_changes)} total):')
for ln, fr in frame_changes[:20]:
    print(f'  Line {ln:>10,}: Frame {fr}')
if len(frame_changes) > 30:
    print('  ...')
    for ln, fr in frame_changes[-10:]:
        print(f'  Line {ln:>10,}: Frame {fr}')

print(f'\nAddress range distribution:')
for k, v in sorted(addr_ranges.items(), key=lambda x: -x[1]):
    pct = v * 100 / total
    print(f'  {k:20s}: {v:>12,} ({pct:.1f}%)')

print(f'\nUnique addresses in flash ($8000-$BFFF): {len(unique_addrs_8000)}')
if unique_addrs_8000:
    sorted_addrs = sorted(unique_addrs_8000)
    print(f'  Range: ${sorted_addrs[0]:04X} - ${sorted_addrs[-1]:04X}')
    print(f'  First 30: {" ".join(f"${a:04X}" for a in sorted_addrs[:30])}')
    print(f'  Last 30:  {" ".join(f"${a:04X}" for a in sorted_addrs[-30:])}')
