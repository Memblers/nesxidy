"""Track the guest PC (_pc) values throughout the trace to understand
what Millipede code is actually executing and whether it makes progress.
We need to identify what _pc values are being dispatched."""
import re

f = r'C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\millipede.txt'

# The dispatch mechanism: when a compiled block exits, its epilogue
# stores exit_pc to _pc ($67-$68), then JMPs to dispatch return.
# When interpretation happens, interpret_6502 advances _pc.
# 
# We can track _pc by looking for:
# 1. STA $67 / STA $68 (stores to _pc lo/hi) in the epilogue
# 2. The dispatch entry - when fixed bank code loads _pc
#
# Actually, the easiest way is to look at what addresses execute in 
# the dispatch function. The dispatch reads _pc and looks up the 
# PC table. We can see _pc values from the LDA/STA patterns.
#
# Better: look at the interpret_6502 calls. When dispatch returns 2,
# interpret_6502 runs. The interpreter reads opcodes from ROM.
# In the trace, interpreter code runs in bank 0 (interpreting)
# or in the fixed bank (dispatch). 
#
# Let's look for a different pattern: the flash_dispatch_return at $6219.
# When a compiled block ends, it JMPs to $6219. Before that, it
# stored the exit_pc. We can track the LDA #imm / STA $67 / LDA #imm / STA $68
# pattern right before JMP $6219.

# Let me just look at _pc values by finding the dispatch entry patterns
# Track what guest PCs the game visits over time

# Strategy: find all "STA $67" and "STA $68" pairs in flash code ($8000-$BFFF)
# These set _pc before returning to dispatch

# Actually, the simplest approach: look at what the trace shows for 
# the ZP locations. The trace shows register names like "PLATFORM_MILLIPEDE = $xx"
# and "GAME_NUMBER = $xx". These are ZP labels from the debug symbols.
# 
# _pc is at ZP $67-$68. Let's find what label maps to those.

# Actually let me just scan for patterns in the trace that reveal _pc.
# When dispatch starts, it reads _pc. Let's find the dispatch_on_pc entry.

# Let me look at a section around frame 100 to see the steady-state behavior
print("=== SCANNING AROUND FRAME 100-105 FOR DISPATCH PATTERNS ===")
print()

target_frame = 100
lines_in_frame = []
current_frame = 0
found_target = False

with open(f, 'r') as fh:
    for i, line in enumerate(fh):
        m = re.search(r'Fr:(\d+)', line)
        if m:
            fr = int(m.group(1))
            if fr >= target_frame and not found_target:
                found_target = True
                current_frame = fr
            if found_target and fr <= target_frame + 5:
                current_frame = fr
                lines_in_frame.append((i, line.rstrip()))
            elif found_target and fr > target_frame + 5:
                break

print(f"Captured {len(lines_in_frame)} lines from frames {target_frame}-{target_frame+5}")
print()

# Find dispatch patterns - look for the dispatch_on_pc code
# The dispatch starts at a known fixed bank address and reads _pc
# Let's look for patterns that show what guest addresses are being processed

# Track addresses and look for _pc updates
# The epilogue pattern: STA $6A / LDA #xx / STA $67 / LDA #xx / STA $68 / JMP $6219
pc_updates = []
for idx in range(len(lines_in_frame) - 5):
    lineno, line = lines_in_frame[idx]
    parts = line.split()
    if not parts:
        continue
    try:
        addr = int(parts[0], 16)
    except:
        continue
    
    # Look for "STA $67" or similar  
    if 'STA' in line and '$67' in line and 0x8000 <= addr <= 0xBFFF:
        # This is in flash code - check the LDA before it
        if idx > 0:
            prev_line = lines_in_frame[idx-1][1]
            m = re.search(r'LDA #\$([0-9A-Fa-f]{2})', prev_line)
            if m:
                pc_lo = int(m.group(1), 16)
                # Look for the next LDA/STA $68
                if idx + 2 < len(lines_in_frame):
                    next_line1 = lines_in_frame[idx+1][1]
                    next_line2 = lines_in_frame[idx+2][1]
                    m2 = re.search(r'LDA #\$([0-9A-Fa-f]{2})', next_line1)
                    if m2:
                        pc_hi = int(m2.group(1), 16)
                        guest_pc = (pc_hi << 8) | pc_lo
                        pc_updates.append((lineno, guest_pc, lines_in_frame[idx-1][0]))

print(f"Guest PC updates (epilogue _pc stores): {len(pc_updates)}")
for lineno, gpc, prev in pc_updates[:80]:
    print(f"  Line {lineno:>10,}: guest_pc=${gpc:04X}")

print()

# Also count unique guest PCs
unique_gpcs = set(gpc for _, gpc, _ in pc_updates)
print(f"Unique guest PCs visited: {len(unique_gpcs)}")
for gpc in sorted(unique_gpcs):
    count = sum(1 for _, g, _ in pc_updates if g == gpc)
    print(f"  ${gpc:04X}: {count} times")
