"""Deeper analysis - understand what guest PCs are being dispatched and 
what the compiled blocks at $8000-$BFFF are doing.
Focus on the dispatch pattern and what guest PCs are being executed."""
import re

f = r'C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\millipede.txt'

# The dispatch writes _pc (guest PC) before jumping to flash.
# After a block returns, the epilogue sets _pc to the exit PC.
# Let's look for the dispatch mechanism and track guest PC values.

# In the Millipede setup:
# ROM_ADDR_MIN = 0x4000, ROM_ADDR_MAX = 0x7FFF
# Mirrors: $C000-$FFFF mirrors $4000-$7FFF
# The dispatch canonicalizes to $4000-$7FFF

# Let's trace the first few hundred frames to understand the game flow.
# Key question: is the game stuck in its own loop, or is the recompiler miscoding something?

# Look at the first 500K lines - understand the boot process
print("=== BOOT SEQUENCE ANALYSIS ===")
print()

# Track transitions between fixed bank ($C000+) and flash ($8000-$BFFF)
# Each time we enter flash, it means dispatch succeeded -> compiled block runs
# Each time we're in fixed bank, it's either dispatch logic or host code

guest_pc_dispatches = []  # (line_num, guest_pc) from epilogue _pc stores
frame_at_line = {}  # line -> frame
compile_events = []  # lines where compilation happens (flash_byte_program at $6000+)

last_frame = 0
block_executions = []  # (line_num, flash_addr, frame)
prev_addr = 0
prev_was_fixed = False

with open(f, 'r') as fh:
    for i, line in enumerate(fh):
        if i > 2000000:  # First ~200 frames
            break
        
        parts = line.split()
        if not parts:
            continue
        try:
            addr = int(parts[0], 16)
        except:
            continue
        
        m = re.search(r'Fr:(\d+)', line)
        if m:
            last_frame = int(m.group(1))
        
        is_flash = 0x8000 <= addr <= 0xBFFF
        is_fixed = 0xC000 <= addr <= 0xFFFF
        is_wram = 0x6000 <= addr <= 0x7FFF
        
        # Track block entries
        if is_flash and prev_was_fixed:
            block_executions.append((i, addr, last_frame))
        
        # Track WRAM execution (flash_byte_program is at $6000)
        if is_wram and addr == 0x6000:
            compile_events.append((i, last_frame))
        
        prev_was_fixed = is_fixed
        prev_addr = addr

print(f"Analyzed {i+1:,} lines up to frame {last_frame}")
print(f"Block executions (dispatch->flash): {len(block_executions)}")
print(f"Compile events (flash_byte_program): {len(compile_events)}")

print()
print("=== FIRST 50 BLOCK EXECUTIONS ===")
for ln, addr, fr in block_executions[:50]:
    print(f"  Line {ln:>8,}: ${addr:04X} (frame {fr})")

print()
print("=== FIRST 30 COMPILE EVENTS ===")
for ln, fr in compile_events[:30]:
    print(f"  Line {ln:>8,}: frame {fr}")

print()
print("=== BLOCK EXECUTION FREQUENCY PER FRAME (first 50 frames) ===")
from collections import Counter
frame_blocks = Counter()
frame_addrs = {}
for ln, addr, fr in block_executions:
    frame_blocks[fr] += 1
    if fr not in frame_addrs:
        frame_addrs[fr] = []
    frame_addrs[fr].append(addr)

for fr in sorted(frame_blocks.keys())[:50]:
    addrs = frame_addrs[fr]
    unique = len(set(addrs))
    print(f"  Frame {fr:>5}: {frame_blocks[fr]:>4} block entries, {unique} unique entry addrs")
    # Show first few unique addresses
    seen = []
    for a in addrs:
        if a not in seen:
            seen.append(a)
        if len(seen) >= 10:
            break
    print(f"           First entries: {' '.join(f'${a:04X}' for a in seen)}")
