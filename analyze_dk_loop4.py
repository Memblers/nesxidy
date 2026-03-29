"""Deep dive into frame 19 (SA compile) and early game frames.
Find evidence of two-pass compile vs BFS-only walk."""
import re, sys

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"

fr_re = re.compile(r'Fr:(\d+)')

# Read frame 18-22 in detail - look for flash_sector_erase and flash_byte_program patterns
# flash_byte_program is at WRAM $6000
# flash_sector_erase is also WRAM
# The key: during SA compile pass 2, we should see heavy flash_byte_program calls
# followed by game execution. During BFS walk only, we see mostly $Cxxx code.

# Also look for the recompile_opcode function calls - they happen during compile.
# The compile loop calls recompile_opcode from bank 2 ($8xxx when bank2 is mapped).

print("=== DETAILED ANALYSIS OF FRAMES 17-25 ===")
print("Looking at first 50 lines per frame transition, plus WRAM call counts\n")

target_frames = set(range(2, 30))  # frames we care about
frame_wram_calls = {}  # frame -> count of WRAM ($6xxx) instructions
frame_flash_programs = {}  # frame -> count of flash_byte_program pattern
frame_sector_erases = {}  # frame -> count of flash_sector_erase pattern

# Patterns for flash operations:
# flash_byte_program at $6000: LDA/STA sequence writing to $8000-$BFFF range
# The SA PPU effect: STA to $2001 address

prev_frame = -1
frame_transitions = []  # (line, from_frame, to_frame)
total_wram = 0
frame_2001_writes = {}

with open(TRACE, 'r', buffering=4*1024*1024) as f:
    for i, line in enumerate(f):
        if i > 3_000_000:  # first ~300 frames
            break
        
        m = fr_re.search(line)
        if not m:
            continue
        frame = int(m.group(1))
        
        if frame != prev_frame:
            frame_transitions.append((i, prev_frame, frame))
            prev_frame = frame
        
        try:
            addr = int(line[:4], 16)
        except (ValueError, IndexError):
            continue
        
        # Count WRAM calls per frame
        if 0x6000 <= addr <= 0x7FFF:
            frame_wram_calls[frame] = frame_wram_calls.get(frame, 0) + 1
        
        # STA to $2001 (PPU mask writes) - SA compile effect
        if 'STA' in line and '2001' in line:
            frame_2001_writes[frame] = frame_2001_writes.get(frame, 0) + 1

print("Frame transitions (first 30):")
for ln, fr_from, fr_to in frame_transitions[:35]:
    wram = frame_wram_calls.get(fr_from, 0)
    ppu = frame_2001_writes.get(fr_from, 0)
    print(f"  Line {ln:>8,}: Frame {fr_from:>4} -> {fr_to:>4}  (WRAM calls in prev: {wram:>5,}, PPU $2001 writes: {ppu})")

print(f"\n=== WRAM activity per frame (first 30 frames) ===")
for fr in sorted(frame_wram_calls.keys())[:35]:
    wram = frame_wram_calls[fr]
    ppu = frame_2001_writes.get(fr, 0)
    bar = '#' * min(80, wram // 50)
    print(f"  Frame {fr:>5}: WRAM={wram:>6,} PPU2001={ppu:>3} {bar}")

# Now look at the actual instructions in frame 19
print(f"\n=== FIRST 100 LINES OF FRAME 19 ===")
with open(TRACE, 'r', buffering=4*1024*1024) as f:
    in_frame_19 = False
    count = 0
    for i, line in enumerate(f):
        m = fr_re.search(line)
        if m:
            frame = int(m.group(1))
            if frame == 19:
                in_frame_19 = True
            elif frame > 19:
                break
        if in_frame_19:
            if count < 100:
                print(f"  {i:>8}: {line.rstrip()[:140]}")
            count += 1
    print(f"  ... total lines in frame 19: {count}")

# Look for the BFS walk start/end indicators
# sa_walk_b2 starts with q_init() then seeds vectors, then BFS loop
# The BFS walk reads from the NES PRG ROM (bank 20, mapped at $8000-$BFFF)
# We should see heavy reads from $Cxxx (fixed bank) during BFS
print(f"\n=== LOOKING FOR SA COMPILE INDICATORS ===")
print("Searching for flash_sector_erase ($AA,$55 sequence) and compile PPU effect...")

with open(TRACE, 'r', buffering=4*1024*1024) as f:
    prev_a = None
    erase_seq = 0
    for i, line in enumerate(f):
        if i > 500_000:  # first ~50 frames
            break
        
        m = fr_re.search(line)
        if not m:
            continue
        frame = int(m.group(1))
        
        # Look for A register values typical of flash erase sequence
        a_match = re.search(r'A:([0-9A-F]{2})', line)
        if a_match:
            a_val = int(a_match.group(1), 16)
            if a_val == 0xAA and prev_a != 0xAA:
                erase_seq = 1
            elif erase_seq == 1 and a_val == 0x55:
                erase_seq = 2
            elif erase_seq == 2 and a_val == 0x80:
                print(f"  Line {i:>8} Frame {frame}: Possible flash erase start (AA,55,80 sequence)")
                erase_seq = 0
            else:
                if erase_seq > 0 and a_val not in (0xAA, 0x55, 0x80):
                    erase_seq = 0
            prev_a = a_val
