"""
analyze_dk_loop11.py
Trace what happens AFTER sa_run's first flash_byte_program (past frame 19→20 boundary).
Key questions:
1. Does sa_run continue with sa_write_header (7 more flash_byte_program calls)?
2. Does the BFS walk actually execute?
3. Does the NMI handler hijack execution?
4. Does sa_run ever return (look for $C60F - instruction after JSR $E7B3)?
"""
import re

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"

# Read lines 160989 to 165000 (after frame 19→20 boundary, ~4000 more lines)
START_LINE = 160989
END_LINE = 170000
CHUNK = END_LINE - START_LINE

print(f"Reading lines {START_LINE}-{END_LINE}...")

lines = []
with open(TRACE, 'r', errors='replace') as f:
    for i, line in enumerate(f, 1):
        if i >= START_LINE:
            lines.append((i, line.rstrip()))
        if i >= END_LINE:
            break

print(f"Read {len(lines)} lines")

# Key addresses to watch for
SA_RUN_RETURN = "C60F"   # instruction after JSR sa_run in nes.c
SA_COMPILE_COMPLETED = "60E1"  # sa_compile_completed address
NMI_HANDLER = "C192"     # NMI handler entry (from earlier analysis)
DISPATCH = "623A"         # dispatch_on_pc (game loop)

flash_byte_program_count = 0
flash_sector_erase_count = 0
nmi_count = 0
dispatch_count = 0
sa_run_return_found = False
sa_compile_write_found = False

# Track address distribution
addr_regions = {}
frame_transitions = []
last_frame = None

# Look for key events
key_events = []

for lineno, text in lines:
    # Extract address
    m = re.match(r'\s*(\d+):\s+([0-9A-Fa-f]{4})\s+(.+)', text)
    if not m:
        continue
    
    addr = m.group(2).upper()
    rest = m.group(3)
    
    # Track frame
    fm = re.search(r'Fr:(\d+)', text)
    if fm:
        frame = int(fm.group(1))
        if last_frame is not None and frame != last_frame:
            frame_transitions.append((lineno, last_frame, frame))
        last_frame = frame
    
    # Address distribution
    region = addr[0] + "000"
    addr_regions[region] = addr_regions.get(region, 0) + 1
    
    # Check for flash operations
    if addr == "6000" and "LDX" in rest:
        flash_byte_program_count += 1
    if addr == "6060":
        flash_sector_erase_count += 1
    
    # Check for NMI handler
    if addr == NMI_HANDLER:
        nmi_count += 1
        key_events.append((lineno, f"NMI handler at Fr:{frame}"))
    
    # Check for dispatch_on_pc
    if addr == DISPATCH:
        dispatch_count += 1
        if dispatch_count <= 5:
            key_events.append((lineno, f"dispatch_on_pc at Fr:{frame}"))
    
    # Check for sa_run return
    if addr == SA_RUN_RETURN:
        sa_run_return_found = True
        key_events.append((lineno, f"sa_run RETURNS at Fr:{frame}"))
    
    # Check for write to sa_compile_completed
    if SA_COMPILE_COMPLETED in text and "STA" in rest:
        sa_compile_write_found = True
        key_events.append((lineno, f"STA to sa_compile_completed at Fr:{frame}"))

print(f"\n=== SUMMARY (lines {START_LINE}-{END_LINE}) ===")
print(f"flash_byte_program entries: {flash_byte_program_count}")
print(f"flash_sector_erase entries: {flash_sector_erase_count}")
print(f"NMI handler entries: {nmi_count}")
print(f"dispatch_on_pc entries: {dispatch_count}")
print(f"sa_run returns (${SA_RUN_RETURN}): {sa_run_return_found}")
print(f"sa_compile_completed written: {sa_compile_write_found}")

print(f"\n=== FRAME TRANSITIONS ===")
for lineno, old_fr, new_fr in frame_transitions:
    print(f"  Line {lineno}: Fr:{old_fr} → Fr:{new_fr}")

print(f"\n=== KEY EVENTS ===")
for lineno, desc in key_events[:30]:
    print(f"  Line {lineno}: {desc}")

print(f"\n=== ADDRESS DISTRIBUTION ===")
for region in sorted(addr_regions.keys()):
    print(f"  ${region}: {addr_regions[region]} instructions")

# Show first 100 lines after the frame boundary
print(f"\n=== FIRST 100 INSTRUCTIONS AFTER LINE {START_LINE} ===")
count = 0
for lineno, text in lines:
    m = re.match(r'\s*(\d+):\s+([0-9A-Fa-f]{4})\s+(.+)', text)
    if m:
        print(f"    {text[:120]}")
        count += 1
        if count >= 100:
            break
