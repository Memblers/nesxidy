"""Look at the actual instructions in key blocks ($8E7B, $9075, etc.)
and the dispatch overhead (122-line FIXED sections) to understand
what's being compiled vs interpreted."""
import re

f = r'C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\millipede.txt'

# Read frame 100 data
target_frame = 100
frame_lines = []

with open(f, 'r') as fh:
    for i, line in enumerate(fh):
        m = re.search(r'Fr:(\d+)', line)
        if m:
            fr = int(m.group(1))
            if fr == target_frame:
                frame_lines.append((i, line.rstrip()))
            elif fr > target_frame and len(frame_lines) > 0:
                break

# Show the first FLASH @ $8E7B block in detail
print("=== FLASH BLOCK @ $8E7B ===")
in_block = False
block_count = 0
for idx, (lineno, line) in enumerate(frame_lines):
    parts = line.split()
    if not parts:
        continue
    try:
        addr = int(parts[0], 16)
    except:
        continue
    
    if addr == 0x8E7B and not in_block:
        in_block = True
        block_count += 1
        if block_count > 1:
            continue  # Skip to show first occurrence only
        print(f"  Start at line {lineno}:")
    
    if in_block and block_count == 1:
        if 0x8000 <= addr <= 0xBFFF:
            print(f"    {line[:150]}")
        else:
            print(f"    --- EXIT to ${addr:04X} ---")
            # Show the dispatch code that follows
            for j in range(min(30, len(frame_lines) - idx)):
                l2 = frame_lines[idx+j][1]
                print(f"    {l2[:150]}")
            break

print()
print("=== FLASH BLOCK @ $9075 ===")
in_block = False
block_count = 0
for idx, (lineno, line) in enumerate(frame_lines):
    parts = line.split()
    if not parts:
        continue
    try:
        addr = int(parts[0], 16)
    except:
        continue
    
    if addr == 0x9075 and not in_block:
        in_block = True
        block_count += 1
        if block_count > 1:
            continue
        print(f"  Start at line {lineno}:")
    
    if in_block and block_count == 1:
        if 0x8000 <= addr <= 0xBFFF:
            print(f"    {line[:150]}")
        else:
            print(f"    --- EXIT to ${addr:04X} ---")
            for j in range(min(20, len(frame_lines) - idx)):
                l2 = frame_lines[idx+j][1]
                print(f"    {l2[:150]}")
            break

print()
print("=== FLASH BLOCK @ $8FFD ===")
in_block = False
block_count = 0
for idx, (lineno, line) in enumerate(frame_lines):
    parts = line.split()
    if not parts:
        continue
    try:
        addr = int(parts[0], 16)
    except:
        continue
    
    if addr == 0x8FFD and not in_block:
        in_block = True
        block_count += 1
        if block_count > 1:
            continue
        print(f"  Start at line {lineno}:")
    
    if in_block and block_count == 1:
        if 0x8000 <= addr <= 0xBFFF:
            print(f"    {line[:150]}")
        else:
            print(f"    --- EXIT to ${addr:04X} ---")
            for j in range(min(20, len(frame_lines) - idx)):
                l2 = frame_lines[idx+j][1]
                print(f"    {l2[:150]}")
            break

print()
print("=== FLASH BLOCK @ $84F6 ===")
in_block = False
block_count = 0
for idx, (lineno, line) in enumerate(frame_lines):
    parts = line.split()
    if not parts:
        continue
    try:
        addr = int(parts[0], 16)
    except:
        continue
    
    if addr == 0x84F6 and not in_block:
        in_block = True
        block_count += 1
        if block_count > 1:
            continue
        print(f"  Start at line {lineno}:")
    
    if in_block and block_count == 1:
        if 0x8000 <= addr <= 0xBFFF:
            print(f"    {line[:150]}")
        else:
            print(f"    --- EXIT to ${addr:04X} ---")
            for j in range(min(20, len(frame_lines) - idx)):
                l2 = frame_lines[idx+j][1]
                print(f"    {l2[:150]}")
            break

print()
print("=== 122-LINE FIXED SECTION (DISPATCH/INTERPRET) ===")
# Find a 122-line FIXED section and show it
prev_region = None
region_start_idx = 0
region_count = 0
shown = False
for idx, (lineno, line) in enumerate(frame_lines):
    parts = line.split()
    if not parts:
        continue
    try:
        addr = int(parts[0], 16)
    except:
        continue
    
    is_fixed = 0xC000 <= addr <= 0xFFFF
    
    if is_fixed:
        if prev_region != "FIXED":
            region_start_idx = idx
            region_count = 1
        else:
            region_count += 1
    else:
        if prev_region == "FIXED" and region_count >= 100 and not shown:
            shown = True
            print(f"  {region_count}-line FIXED section at line {frame_lines[region_start_idx][0]}:")
            for j in range(min(region_count, 130)):
                l2 = frame_lines[region_start_idx+j][1]
                print(f"    {l2[:150]}")
    
    prev_region = "FIXED" if is_fixed else "OTHER"
