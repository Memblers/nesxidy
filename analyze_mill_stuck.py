import os, re

fpath = r'C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\millipede.txt'
fsize = os.path.getsize(fpath)
print(f'File size: {fsize:,} bytes ({fsize/1024/1024:.1f} MB)')

# Read last 5MB to find the transition into the stuck state
read_size = 5_000_000
with open(fpath, 'rb') as f:
    f.seek(max(0, fsize - read_size))
    tail = f.read().decode('utf-8', errors='replace')

lines = tail.split('\n')
lines = lines[1:]  # skip partial first line
print(f'Read {len(lines)} lines from last {read_size//1_000_000}MB')

# Find frame transitions
frame_changes = []
prev_frame = None
for i, line in enumerate(lines):
    m = re.search(r'Fr:(\d+)', line)
    if m:
        fr = int(m.group(1))
        if fr != prev_frame:
            frame_changes.append((i, fr))
            prev_frame = fr

print(f'\nFrame transitions found: {len(frame_changes)}')
for idx, fr in frame_changes:
    print(f'  Line {idx}: Frame {fr}')

# Show context around the last frame transition
if len(frame_changes) >= 2:
    last_trans = frame_changes[-1][0]
    print(f'\n=== AROUND LAST FRAME TRANSITION (line {last_trans}) ===')
    start = max(0, last_trans - 40)
    end = min(len(lines), last_trans + 40)
    for i in range(start, end):
        marker = '>>>' if i == last_trans else '   '
        print(f'{marker} [{i}] {lines[i].rstrip()[:140]}')

# The game is stuck at guest $7672 = BCS $7672 (self-spin, watchdog trigger)
# This means vblank_flag ($9A) reached >= 8.
# vblank_flag is incremented each vblank and reset when the game processes it.
# If the game can't process vblanks fast enough (or they're not being delivered),
# the counter accumulates and hits 8.

# Look for the vblank_flag address in writes - on the host side,
# guest ZP $9A is mapped to _RAM_BASE + $9A.
# In compiled code, writes to $9A would appear as STA to some host address.

# Also look for the dispatch loop or interpreter handling $766E-$7672
# Let's search for $766E, $7670, $7672 as guest PCs that might appear
# in dispatcher context (e.g. _pc being loaded/stored)

# Search for the interpreted path - dispatcher reads _pc and executes
# Look for patterns: the guest code LDA $9A / CMP #$08 / BCS
# These would be at $766E/$7670/$7672 in the guest.
# The compiled version would execute from flash.

# Let's look for the OAM fill loop at $CC85 and what calls it
print('\n=== SEARCHING FOR CC85 LOOP START ===')
found_cc85_start = None
for i, line in enumerate(lines):
    stripped = line.strip()
    if stripped.startswith('CC85') and found_cc85_start is None:
        found_cc85_start = i
        print(f'First CC85 at line {i}')
        # Show 30 lines before
        print('\n--- 30 lines before first CC85 ---')
        start = max(0, i - 30)
        for j in range(start, i + 5):
            marker = '>>>' if j == i else '   '
            print(f'{marker} [{j}] {lines[j].rstrip()[:140]}')
        break

# Count how many times CC85 appears
cc85_count = sum(1 for line in lines if line.strip().startswith('CC85'))
print(f'\nCC85 appearances in last 5MB: {cc85_count}')

# Look for any address in the $76xx range (guest vblank check area)
# These would appear as host PCs if the code is compiled to flash
# OR as guest PCs if being interpreted
print('\n=== LOOKING FOR GUEST $76xx ADDRESSES ===')
for i, line in enumerate(lines[-10000:]):
    stripped = line.strip()
    if len(stripped) >= 4:
        try:
            pc = int(stripped[:4], 16)
            if 0x7660 <= pc <= 0x7690:
                real_i = len(lines) - 10000 + i
                print(f'  [{real_i}] {stripped[:140]}')
        except:
            pass

# Also look for _pc being set to $76xx values
print('\n=== LOOKING FOR _pc = 76xx ===')
count = 0
for i, line in enumerate(lines[-10000:]):
    if '_pc' in line and '76' in line:
        real_i = len(lines) - 10000 + i
        if count < 20:
            print(f'  [{real_i}] {line.strip()[:140]}')
        count += 1
print(f'Total: {count}')
