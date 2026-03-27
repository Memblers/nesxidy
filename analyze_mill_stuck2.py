import os, re

fpath = r'C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\millipede.txt'
fsize = os.path.getsize(fpath)
print(f'File size: {fsize:,} bytes ({fsize/1024/1024:.1f} MB)')

# Read last 20MB to see more history before the hang
read_size = 20_000_000
with open(fpath, 'rb') as f:
    f.seek(max(0, fsize - read_size))
    tail = f.read().decode('utf-8', errors='replace')

lines = tail.split('\n')
lines = lines[1:]
print(f'Read {len(lines)} lines from last {read_size//1_000_000}MB')

# The guest code at $766E is LDA $9A (inside IRQ handler)
# The guest code at $7651 is INC $9A (IRQ handler increments vblank_flag)
# The guest code at $4026 is LSR $9A (main loop clears vblank_flag)
# These execute as compiled code in flash, not at those guest addresses.
# But we can track _pc values to find dispatch cycles.

# Find all dispatch_on_pc calls and their guest PCs
# The pattern is: the dispatcher loads _pc, then JSR dispatch_on_pc
# After dispatch returns, it checks the result.

# Actually, let me look for the guest PC being set to $76xx (IRQ handler area)
# The pattern is: STA _pc = $XX where the value is $72 (lo) or $76 (hi)

# Better: look for the backwards branch yield at $7672 pattern.
# The yield stub sets _pc to the branch target, then yields.
# For $7672 (BCS $7672), it sets _pc = $72, $68 = $76.

# Let me find the first occurrence of _pc being set to values in the $76xx range
# which indicates the IRQ handler is running.

# First, let me find frame boundaries and understand the execution pattern
frame_starts = {}
for i, line in enumerate(lines):
    m = re.search(r'Fr:(\d+)', line)
    if m:
        fr = int(m.group(1))
        if fr not in frame_starts:
            frame_starts[fr] = i

print(f'\nFrames in trace: {sorted(frame_starts.keys())}')
for fr in sorted(frame_starts.keys()):
    print(f'  Frame {fr} starts at line {frame_starts[fr]}')

# Now search for the _pc = $7672 pattern (the stuck address)
# It appears as: STA _pc with A=$72 followed by STA $68 with A=$76
stuck_pcs = []
for i in range(len(lines) - 1):
    line = lines[i]
    if '_pc' in line and 'STA' in line:
        # Check if A register value suggests $72 (lo of $7672)
        m = re.search(r'A:([0-9A-Fa-f]{2})', line)
        if m and m.group(1).upper() == '72':
            # Check next few lines for $68 = $76
            for j in range(i+1, min(i+5, len(lines))):
                if '$68' in lines[j] and 'STA' in lines[j]:
                    m2 = re.search(r'A:([0-9A-Fa-f]{2})', lines[j])
                    if m2 and m2.group(1).upper() == '76':
                        stuck_pcs.append(i)
                        break

print(f'\nGuest PC set to $7672 occurrences: {len(stuck_pcs)}')
if stuck_pcs:
    print(f'First at line {stuck_pcs[0]}, last at line {stuck_pcs[-1]}')

# Show the first occurrence and context
if stuck_pcs:
    first = stuck_pcs[0]
    print(f'\n=== CONTEXT AROUND FIRST $7672 SET (line {first}) ===')
    start = max(0, first - 60)
    end = min(len(lines), first + 10)
    for i in range(start, end):
        marker = '>>>' if i == first else '   '
        # Get frame number
        m = re.search(r'Fr:(\d+)', lines[i])
        fr_str = f' [Fr:{m.group(1)}]' if m else ''
        print(f'{marker} [{i}]{fr_str} {lines[i].rstrip()[:130]}')

# Also look for how many dispatch cycles happen at $7672 per frame
if len(stuck_pcs) > 1:
    print(f'\n=== DISTRIBUTION OF $7672 DISPATCHES ===')
    # Group by frame
    pc_by_frame = {}
    for idx in stuck_pcs:
        # Find frame for this line
        fr = None
        for j in range(idx, max(idx-20, -1), -1):
            m = re.search(r'Fr:(\d+)', lines[j])
            if m:
                fr = int(m.group(1))
                break
        if fr:
            pc_by_frame[fr] = pc_by_frame.get(fr, 0) + 1
    for fr in sorted(pc_by_frame.keys()):
        print(f'  Frame {fr}: {pc_by_frame[fr]} dispatches to $7672')
