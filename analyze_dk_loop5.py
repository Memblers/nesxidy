"""Find the SA walk/compile phases by looking for flash_sector_erase patterns.
flash_sector_erase writes specific byte sequences to flash via WRAM code.
Also look for the BFS walk start (seeds from vectors), and pass transitions."""
import re, sys

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"

# The key insight: flash_sector_erase lives in WRAM ($6xxx).
# It writes a command sequence: $AA to $5555, $55 to $2AAA, $80 to $5555,
# $AA to $5555, $55 to $2AAA, $30 to sector address.
# But those are flash-space addresses, not CPU addresses.
# On the CPU side, it does bankswitch + STA to $8xxx-$Bxxx addresses.

# Better: look at the map file to find exact addresses
# For now, let's check what happens in frames 2-19 by looking at
# address patterns more carefully.

# Specifically: does flash_cache_init_sectors() run (zeros sector_free_offset)?
# Does sa_compile_one_block() run? It lives in BANK_SA_CODE (bank 19).

# When bank 19 is mapped at $8000-$BFFF, SA code runs from there.
# BFS walk is in bank 2 (BANK_RECOMPILE), also at $8000-$BFFF when mapped.

# Let's look at the label map to find key function addresses
import os

# Read the label map
labels = {}
mlb_file = r"c:\proj\c\NES\nesxidy-co\nesxidy\nes_donkeykong.mlb"
map_file = r"c:\proj\c\NES\nesxidy-co\nesxidy\vicemap.map"

if os.path.exists(map_file):
    print("=== KEY ADDRESSES FROM MAP FILE ===")
    with open(map_file, 'r') as f:
        for line in f:
            line = line.strip()
            # Look for key function addresses
            for name in ['sa_run_b2', 'sa_walk_b2', 'sa_compile_one_block',
                         'flash_format_b19', 'flash_cache_init_sectors_b17',
                         'flash_sector_erase', 'flash_byte_program',
                         'check_recompile_signature', 'check_recompile_triggers',
                         'sa_compile_completed', 'sa_do_compile',
                         'dispatch_on_pc', 'recompile_opcode',
                         'main', 'sa_run ']:
                if name in line:
                    print(f"  {line[:100]}")
                    break

# Now let's look for specific evidence in the trace:
# 1. The flash_format phase (frames 2-6ish) - should erase ALL code banks
# 2. The BFS walk phase - reads ROM via fixed bank, writes SA bitmap via WRAM
# 3. The SA compile phase - heavy WRAM writes for flash_byte_program
# 4. The game start - dispatch_on_pc in WRAM

# Actually the most useful thing: find which $8xxx addresses are being visited.
# During BFS walk, bank 2 (BANK_RECOMPILE) is mapped → sa_walk_b2 at $8000+
# During SA compile, bank 19 (BANK_SA_CODE) is mapped → sa_compile_one_block at $8000+
# During game, various cache banks (4-16) are mapped → compiled code at $8000+

# The $8xxx references in frames 2-18 should tell us which bank is active.
# In the trace, we see the INSTRUCTION addresses. If bank 2 is mapped,
# instructions at $8xxx are running sa_walk_b2. If bank 19, sa_run_b2 etc.

# Let's look at what's happening right before frame 19 (end of SA phase)
print("\n=== LAST 200 LINES OF FRAME 18 (end of SA walk/compile) ===")
frame_18_end = []
with open(TRACE, 'r', buffering=4*1024*1024) as f:
    for i, line in enumerate(f):
        m = re.search(r'Fr:(\d+)', line)
        if not m:
            continue
        fr = int(m.group(1))
        if fr == 18:
            frame_18_end.append((i, line.rstrip()[:150]))
            if len(frame_18_end) > 200:
                frame_18_end.pop(0)
        elif fr > 18:
            break

for ln, text in frame_18_end:
    print(f"  {ln:>8}: {text}")

# And the first 50 lines of frame 20 (first game frame after SA)
print("\n=== FIRST 50 LINES OF FRAME 20 ===")
with open(TRACE, 'r', buffering=4*1024*1024) as f:
    in_f20 = False
    count = 0
    for i, line in enumerate(f):
        m = re.search(r'Fr:(\d+)', line)
        if m:
            fr = int(m.group(1))
            if fr == 20:
                in_f20 = True
            elif fr > 20:
                break
        if in_f20:
            print(f"  {i:>8}: {line.rstrip()[:150]}")
            count += 1
            if count >= 50:
                break
