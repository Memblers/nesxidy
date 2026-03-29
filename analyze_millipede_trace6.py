"""Look at the actual trace to understand the per-frame flow.
Instead of trying to track _pc from epilogues, look at what addresses 
run in flash ($8000-$BFFF) vs fixed bank vs WRAM, and identify the
transitions that represent dispatch boundaries.

Also look at the interpret_6502 calls - when dispatch returns 2,
the game falls to interpret. Track where these happen."""
import re

f = r'C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\millipede.txt'

# Let's look at ONE complete frame in detail
# Pick frame 100 and show the high-level flow

target_frame = 100
frame_lines = []
current_frame = 0

with open(f, 'r') as fh:
    for i, line in enumerate(fh):
        m = re.search(r'Fr:(\d+)', line)
        if m:
            fr = int(m.group(1))
            if fr == target_frame:
                frame_lines.append((i, line.rstrip()))
            elif fr > target_frame and len(frame_lines) > 0:
                break

print(f"Frame {target_frame}: {len(frame_lines)} trace lines")
print()

# Classify each line by address range
regions = []
prev_region = None
region_start = 0
region_lines = 0

for idx, (lineno, line) in enumerate(frame_lines):
    parts = line.split()
    if not parts:
        continue
    try:
        addr = int(parts[0], 16)
    except:
        continue
    
    if 0x8000 <= addr <= 0xBFFF:
        region = "FLASH"
    elif 0xC000 <= addr <= 0xFFFF:
        region = "FIXED"
    elif 0x6000 <= addr <= 0x7FFF:
        region = "WRAM"
    else:
        region = f"OTHER"
    
    if region != prev_region:
        if prev_region is not None:
            regions.append((prev_region, region_start, region_lines, frame_lines[max(0,idx-1)]))
        prev_region = region
        region_start = lineno
        region_lines = 1
    else:
        region_lines += 1

if prev_region is not None:
    regions.append((prev_region, region_start, region_lines, frame_lines[-1]))

print(f"Region transitions: {len(regions)}")
print()

# Show transitions with context
for i, (region, start, count, last_line) in enumerate(regions[:150]):
    # Get first line of region
    first_idx = None
    for idx, (lineno, line) in enumerate(frame_lines):
        if lineno >= start:
            first_idx = idx
            break
    
    if first_idx is not None:
        first_line = frame_lines[first_idx][1][:100]
    else:
        first_line = "???"
    
    last = last_line[1][:100] if last_line else "???"
    
    label = ""
    if region == "FLASH":
        # Get the first flash address
        parts = first_line.split()
        if parts:
            try:
                a = int(parts[0], 16)
                label = f" @ ${a:04X}"
            except:
                pass
    
    print(f"  [{i:>3}] {region:6s} ({count:>5} lines){label}")
    if count <= 3:
        for j in range(min(count, 3)):
            if first_idx + j < len(frame_lines):
                print(f"        {frame_lines[first_idx+j][1][:120]}")

print()
print("=== INTERPRET EVENTS ===")
# Look for interpret_6502 calls. These happen in bank 0 ($8000-$BFFF).
# Actually, interpret is called from run_6502 in the fixed bank.
# When dispatch returns 2, run_6502 calls bankswitch_prg(0) then interpret_6502().
# interpret_6502 is in bank 0, so it runs at $8000+ when bank 0 is active.
# But actually, ALL flash code shows as $8000-$BFFF in the trace.
# We need to distinguish between compiled-code-flash and interpreter-flash.
# The interpreter code would have different patterns than compiled blocks.

# Actually, the key question: how many cycles does the game spend in:
# 1. Flash (compiled code) vs 2. Flash (interpreter) vs 3. Fixed bank (dispatch/host)
# vs 4. WRAM (flash programming)

# Let me look at what addresses are in the FLASH region.
# Compiled blocks contain the guest opcodes. Interpreter code has different patterns.
# Let me just count regions.
region_counts = {}
for region, _, count, _ in regions:
    region_counts[region] = region_counts.get(region, 0) + count

for k, v in sorted(region_counts.items(), key=lambda x: -x[1]):
    pct = v * 100 / len(frame_lines)
    print(f"  {k:8s}: {v:>6} lines ({pct:.1f}%)")
