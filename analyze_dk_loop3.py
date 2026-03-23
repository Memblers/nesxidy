"""Analyze DK trace: find SA compile phases, resets, and the loop pattern.
Mesen format: ADDR  OPCODE OPERAND  ... Fr:NNN ..."""
import re, sys

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"

# trigger_soft_reset is at WRAM, does: LDA mapper_chr_bank; STA $C000; JMP ($FFFC)
# The JMP ($FFFC) would show as address in $6xxx range with "JMP ($FFFC)"
# But since it's indirect, Mesen shows it differently. Let's search more broadly.

# flash_sector_erase sequence involves writing to $5555 and $2AAA on SST39SF040
# but those are flash commands, not CPU addresses.

# The key indicator: look at frame numbers. SA compile will span many frames
# with heavy $6xxx (WRAM) activity. Then game runs. Then if it resets, 
# we'll see $C0xx again (the VBlank wait loop at boot).

# Let's track:
# 1. Frame transitions (Fr: field)
# 2. Per-frame address distribution (which 4K page dominates)
# 3. Specific patterns: $C012 BIT $2002 (VBlank wait = boot), trigger_soft_reset

fr_re = re.compile(r'Fr:(\d+)')

frame_data = {}  # frame_num -> {pages: Counter, line_start, line_end, instr_count}
boot_loops = []  # (line, frame) where we see $C012 BIT PpuStatus_2002
resets = []      # lines matching trigger_soft_reset pattern

prev_frame = -1
cur_frame_pages = {}
cur_frame_start = 0
cur_frame_count = 0

MAX_LINES = 30_000_000

print(f"Reading {TRACE}...")
with open(TRACE, 'r', buffering=4*1024*1024) as f:
    for i, line in enumerate(f):
        if i >= MAX_LINES:
            break
        if i % 5_000_000 == 0 and i > 0:
            print(f"  ...{i:,} lines", file=sys.stderr)
        
        # Extract frame number
        m = fr_re.search(line)
        if not m:
            continue
        frame = int(m.group(1))
        
        # Extract address (first 4 hex chars)
        try:
            addr = int(line[:4], 16)
        except (ValueError, IndexError):
            continue
        
        page = addr >> 12
        
        # Frame transition
        if frame != prev_frame:
            if prev_frame >= 0:
                # Save previous frame data
                frame_data[prev_frame] = {
                    'pages': dict(cur_frame_pages),
                    'start': cur_frame_start,
                    'end': i - 1,
                    'count': cur_frame_count
                }
            cur_frame_pages = {}
            cur_frame_start = i
            cur_frame_count = 0
            prev_frame = frame
        
        cur_frame_pages[page] = cur_frame_pages.get(page, 0) + 1
        cur_frame_count += 1
        
        # Look for boot VBlank wait loop ($C012 BIT $2002)
        if addr == 0xC012 and 'BIT' in line and '2002' in line:
            if not boot_loops or boot_loops[-1][1] != frame:
                boot_loops.append((i, frame))
        
        # Look for trigger_soft_reset: JMP ($FFFC) from WRAM
        if 0x6000 <= addr <= 0x7FFF and 'JMP' in line and 'FFFC' in line:
            resets.append((i, frame, line.rstrip()[:120]))

# Save last frame
if prev_frame >= 0:
    frame_data[prev_frame] = {
        'pages': dict(cur_frame_pages),
        'start': cur_frame_start,
        'end': i,
        'count': cur_frame_count
    }

total_lines = i + 1
print(f"\nProcessed {total_lines:,} lines, {len(frame_data)} frames")

# Report resets
print(f"\n=== SOFT RESETS (JMP ($FFFC) from WRAM) ===")
if resets:
    for ln, fr, text in resets:
        print(f"  Line {ln:>10,} Frame {fr:>5}: {text}")
else:
    print("  None found!")

# Report boot loops
print(f"\n=== BOOT VBLANK WAIT ($C012 BIT $2002) - first/last occurrences ===")
if boot_loops:
    print(f"  Total: {len(boot_loops)} occurrences")
    for ln, fr in boot_loops[:10]:
        print(f"  Line {ln:>10,} Frame {fr:>5}")
    if len(boot_loops) > 10:
        print(f"  ...")
        for ln, fr in boot_loops[-5:]:
            print(f"  Line {ln:>10,} Frame {fr:>5}")
else:
    print("  None found!")

# Classify frames: "compile" (heavy WRAM $6xxx) vs "game" (heavy flash $8/$9xxx)
print(f"\n=== FRAME CLASSIFICATION (first 100 frames) ===")
print(f"{'Frame':>6} {'Lines':>8} {'$6xxx':>7} {'$8xxx':>7} {'$9xxx':>7} {'$Cxxx':>7} {'$Exxx':>7} {'Type':>10}")
frames_sorted = sorted(frame_data.keys())
for fr in frames_sorted[:100]:
    d = frame_data[fr]
    p = d['pages']
    total = d['count']
    wram = p.get(6, 0)
    flash8 = p.get(8, 0)
    flash9 = p.get(9, 0)
    fixedC = p.get(0xC, 0)
    fixedE = p.get(0xE, 0)
    
    # Classify
    if total < 10:
        ftype = "tiny"
    elif wram > total * 0.3:
        ftype = "COMPILE"
    elif flash8 + flash9 > total * 0.3:
        ftype = "GAME"
    elif fixedC + fixedE > total * 0.5:
        ftype = "host"
    else:
        ftype = "mixed"
    
    print(f"{fr:>6} {total:>8,} {wram:>7,} {flash8:>7,} {flash9:>7,} {fixedC:>7,} {fixedE:>7,} {ftype:>10}")

# Also show around any transition from GAME->COMPILE which would indicate loop
print(f"\n=== ALL COMPILE FRAMES ===")
compile_frames = []
for fr in frames_sorted:
    d = frame_data[fr]
    total = d['count']
    wram = d['pages'].get(6, 0)
    if total > 100 and wram > total * 0.3:
        compile_frames.append(fr)
        
if compile_frames:
    print(f"  Total compile frames: {len(compile_frames)}")
    # Group consecutive
    groups = []
    cur_group = [compile_frames[0]]
    for cf in compile_frames[1:]:
        if cf <= cur_group[-1] + 2:  # allow 1-frame gap
            cur_group.append(cf)
        else:
            groups.append(cur_group)
            cur_group = [cf]
    groups.append(cur_group)
    
    print(f"  Compile phases: {len(groups)}")
    for gi, g in enumerate(groups):
        print(f"    Phase {gi+1}: frames {g[0]}-{g[-1]} ({len(g)} frames)")
else:
    print("  None found!")
