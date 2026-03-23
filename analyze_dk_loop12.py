"""
analyze_dk_loop12.py - Trace sa_run execution through many frames.
Find: Does sa_run ever return? Does BFS walk execute? Does compile execute?
Scans from line 160990 (start of frame 20) through many more frames.
"""
import re

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"

# Read from frame 20 start through ~500K more lines
START_LINE = 160990
MAX_LINES = 500000

print(f"Reading {MAX_LINES} lines starting from line {START_LINE}...")

# Key addresses
SA_RUN_RETURN = "C60F"     # instruction after JSR sa_run in nes.c
NMI_VECTOR = "FFFA"        # NMI vector (if read)
DISPATCH = "623A"           # dispatch_on_pc (game started)
FLASH_BYTE_PROGRAM = "6000" # flash_byte_program entry
FLASH_SECTOR_ERASE = "6060" # flash_sector_erase entry

# Counters
flash_byte_program_count = 0
flash_sector_erase_count = 0
nmi_count = 0
dispatch_count = 0
sa_run_return_line = None
sa_compile_completed_line = None

# Address regions
addr_regions = {}
frame_set = set()
last_frame = None
frame_transitions = []

# Key events
key_events = []

# BFS walk indicators: q_init, q_push, q_pop involve cache_code[0] area
# Let's look for the sa_enqueue_if_valid pattern — writes to cache_code[0] area
# Also: the BFS walk reads from ROM via read6502 → peek_bank_byte

# Track banked ($8xxx-$Bxxx) address activity per frame
banked_per_frame = {}

lines_processed = 0
with open(TRACE, 'r', errors='replace') as f:
    for i, line in enumerate(f, 1):
        if i < START_LINE:
            continue
        if lines_processed >= MAX_LINES:
            break
        lines_processed += 1
        
        text = line.rstrip()
        
        # Parse: "ADDR  OPCODE ..."  (no line number prefix)
        m = re.match(r'([0-9A-Fa-f]{4})\s+(.+)', text)
        if not m:
            continue
        
        addr = m.group(1).upper()
        rest = m.group(2)
        
        # Track frame
        fm = re.search(r'Fr:(\d+)', text)
        if fm:
            frame = int(fm.group(1))
            frame_set.add(frame)
            if last_frame is not None and frame != last_frame:
                frame_transitions.append((i, last_frame, frame))
            last_frame = frame
        else:
            frame = last_frame or 0
        
        # Address distribution
        region = addr[0] + "000"
        addr_regions[region] = addr_regions.get(region, 0) + 1
        
        # Banked per frame
        if addr[0] in '89AB' and addr[0].isdigit() or addr[0] in 'ABab':
            val = int(addr, 16)
            if 0x8000 <= val <= 0xBFFF:
                banked_per_frame[frame] = banked_per_frame.get(frame, 0) + 1
        
        # Check for flash operations
        if addr == FLASH_BYTE_PROGRAM and "LDX" in rest:
            flash_byte_program_count += 1
            if flash_byte_program_count <= 20:
                # Extract what's being written (r4 value)
                key_events.append((i, f"flash_byte_program #{flash_byte_program_count} at Fr:{frame}"))
        
        if addr == FLASH_SECTOR_ERASE:
            flash_sector_erase_count += 1
            if flash_sector_erase_count <= 10:
                key_events.append((i, f"flash_sector_erase #{flash_sector_erase_count} at Fr:{frame}"))
        
        # Check for sa_run return
        if addr == SA_RUN_RETURN and sa_run_return_line is None:
            sa_run_return_line = i
            key_events.append((i, f"*** sa_run RETURNS at Fr:{frame} ***"))
        
        # Check for write to sa_compile_completed ($60E1)
        if "60E1" in text and "STA" in rest:
            sa_compile_completed_line = i
            key_events.append((i, f"*** STA to $60E1 (sa_compile_completed) at Fr:{frame} ***"))
        
        # Check for dispatch_on_pc (game started)
        if addr == DISPATCH:
            dispatch_count += 1
            if dispatch_count <= 3:
                key_events.append((i, f"dispatch_on_pc at Fr:{frame}"))
        
        # Check for RTI (NMI return) pattern or NMI entry
        # NMI entry: PC jumps to NMI handler address
        # Look for specific NMI handler address
        if addr == "C192":
            nmi_count += 1
            if nmi_count <= 10:
                key_events.append((i, f"NMI handler entry #{nmi_count} at Fr:{frame}"))

print(f"\nProcessed {lines_processed} lines (lines {START_LINE}-{START_LINE+lines_processed})")
print(f"Frames spanned: {min(frame_set) if frame_set else '?'} to {max(frame_set) if frame_set else '?'}")

print(f"\n=== SUMMARY ===")
print(f"flash_byte_program count: {flash_byte_program_count}")
print(f"flash_sector_erase count: {flash_sector_erase_count}")
print(f"NMI handler entries: {nmi_count}")
print(f"dispatch_on_pc entries: {dispatch_count}")
print(f"sa_run returns ($C60F): {'YES at line ' + str(sa_run_return_line) if sa_run_return_line else 'NO - never returns!'}")
print(f"sa_compile_completed written: {'YES at line ' + str(sa_compile_completed_line) if sa_compile_completed_line else 'NO'}")

print(f"\n=== FRAME TRANSITIONS (first 20) ===")
for lineno, old_fr, new_fr in frame_transitions[:20]:
    print(f"  Line {lineno}: Fr:{old_fr} → Fr:{new_fr}")

print(f"\n=== KEY EVENTS ===")
for lineno, desc in key_events[:50]:
    print(f"  Line {lineno}: {desc}")

print(f"\n=== ADDRESS DISTRIBUTION ===")
for region in sorted(addr_regions.keys()):
    print(f"  ${region}: {addr_regions[region]:,} instructions")

print(f"\n=== BANKED ($8xxx-$Bxxx) INSTRUCTIONS PER FRAME ===")
for frame in sorted(banked_per_frame.keys())[:30]:
    print(f"  Frame {frame}: {banked_per_frame[frame]:,} banked instructions")
