import os, re

fpath = r'C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\millipede.txt'
fsize = os.path.getsize(fpath)

# Read around frame 62990 to see how the game recovers from $7672 spin
read_size = 20_000_000
with open(fpath, 'rb') as f:
    f.seek(max(0, fsize - read_size))
    tail = f.read().decode('utf-8', errors='replace')

lines = tail.split('\n')
lines = lines[1:]
print(f'Read {len(lines)} lines')

# Find the first $7672 dispatch and trace what happens AFTER the yield
# until the next dispatch or until the guest escapes the spin

# Find all instances where _pc is set to $7672
pc_7672_sets = []
for i in range(len(lines) - 3):
    line = lines[i]
    if 'STA _pc' in line:
        m = re.search(r'A:([0-9A-Fa-f]{2})', line)
        if m and m.group(1).upper() == '72':
            for j in range(i+1, min(i+4, len(lines))):
                if 'STA $68' in lines[j] or 'STA _pc+1' in lines[j]:
                    m2 = re.search(r'A:([0-9A-Fa-f]{2})', lines[j])
                    if m2 and m2.group(1).upper() == '76':
                        pc_7672_sets.append(i)
                        break

print(f'Found {len(pc_7672_sets)} PC=$7672 dispatches')

# For the first occurrence in frame 62990, show the full dispatch cycle:
# from the yield back through the dispatcher until the NEXT compiled block runs
if len(pc_7672_sets) >= 2:
    # Get the second one (first full cycle)
    idx = pc_7672_sets[0]
    # Show from the yield (JMP $FFF0) through dispatch until next compiled code
    # The yield at $FFF0 jumps to _flash_dispatch_return ($631E)
    # Then the dispatch loop runs
    # Until either: another dispatch to $7672, or dispatch to a different guest PC
    print(f'\n=== FULL DISPATCH CYCLE AFTER FIRST $7672 YIELD (line {idx}) ===')
    # Show from yield through to next compiled block entry
    start = idx  # the STA _pc line
    # Find the next JMP to compiled code (JMP $xxxx where xxxx is in $8000-$BFFF or $A000-$BFFF)
    end = min(idx + 300, len(lines))
    for i in range(start, end):
        stripped = lines[i].strip()
        print(f'  [{i}] {stripped[:140]}')
        # Stop after we see the next JMP to flash or the end
        if i > idx + 5 and ('JMP $A3' in stripped or 'JMP $A' in stripped or 
            'JMP $8' in stripped or 'JMP $9' in stripped or 'JMP $B' in stripped):
            # Show a few more lines of the compiled code
            for j in range(i+1, min(i+20, len(lines))):
                print(f'  [{j}] {lines[j].strip()[:140]}')
            break
    
    # Now find the LAST $7672 dispatch in frame 62990 and see what comes AFTER
    # (how does the game escape the spin?)
    frame_62990_start = None
    frame_62991_start = None
    for i, line in enumerate(lines):
        m = re.search(r'Fr:(\d+)', line)
        if m:
            fr = int(m.group(1))
            if fr == 62990 and frame_62990_start is None:
                frame_62990_start = i
            if fr == 62991 and frame_62991_start is None:
                frame_62991_start = i
                break
    
    print(f'\nFrame 62990: lines {frame_62990_start} to {frame_62991_start}')
    
    # Find the last $7672 set in frame 62990
    last_7672_in_frame = None
    for idx in pc_7672_sets:
        if frame_62990_start <= idx < (frame_62991_start or len(lines)):
            last_7672_in_frame = idx
    
    if last_7672_in_frame:
        print(f'\nLast $7672 in frame 62990 at line {last_7672_in_frame}')
        # After this last yield, what happens? Show 200 lines after
        print('\n=== AFTER LAST $7672 YIELD IN FRAME 62990 ===')
        start = last_7672_in_frame
        end = min(last_7672_in_frame + 200, len(lines))
        for i in range(start, end):
            stripped = lines[i].strip()
            print(f'  [{i}] {stripped[:140]}')
            # Stop when we see a guest address in $40xx range (main loop) 
            # or a new guest dispatch that's not $7672

# Also: in frame 63007, show the FULL sequence from the $7672 yield to the end
print('\n\n=== FRAME 63007: FROM $7672 TO END ===')
frame_63007_7672 = [idx for idx in pc_7672_sets 
                     if any('Fr:63007' in lines[j] for j in range(max(0,idx-5), min(idx+5, len(lines))))]
if frame_63007_7672:
    start = frame_63007_7672[0]
    end = min(start + 200, len(lines))
    for i in range(start, end):
        stripped = lines[i].strip()
        print(f'  [{i}] {stripped[:140]}')
