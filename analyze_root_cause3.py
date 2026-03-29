#!/usr/bin/env python3
"""
Root cause v3: Full trace from _pc=$6A76 write backward to find the CALLER.
Focus: What C function is executing in the $DA00-$DC00/$E600-$E700 range?
Is this the SA batch compile path?
"""
import re, os

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\exidy.txt"
TAIL_BYTES = 8 * 1024 * 1024

def read_tail(path, nbytes):
    sz = os.path.getsize(path)
    start = max(0, sz - nbytes)
    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        f.seek(start)
        if start > 0:
            f.readline()
        return f.readlines()

lines = read_tail(TRACE, TAIL_BYTES)
print(f"Read {len(lines)} lines from tail")

# Find the flag write
flag_idx = None
for i in range(len(lines)-1, -1, -1):
    if 'AA76' in lines[i] and 'STA' in lines[i]:
        flag_idx = i
        break
print(f"Flag write at line {flag_idx}")

# Now: dump EVERY line from 1500 lines before the flag write up to the flag write
# Annotate with address ranges to identify which function is executing
# Key: $8000-$BFFF = switchable bank (bank 17 for compile functions)
#      $C000-$DFFF = fixed bank C code
#      $E000-$FFFF = fixed bank C code (higher)
#      $6000-$7FFF = WRAM (asm dispatch, flash_byte_program, etc.)

# Also track the compiled block's boundaries: find flash_sector_alloc
# and the entry_pc setup
start = max(0, flag_idx - 2000)

# First, look for flash_sector_alloc (marks the start of compilation)
# flash_sector_alloc_b17 is called via the trampoline which does bankswitch_prg(BANK_COMPILE)
# The trampoline is at a fixed address. Let's look for "sector_free" or "next_free_sector"
print("\n=== Markers in 2000 lines before flag write ===")
for i in range(start, flag_idx):
    line = lines[i]
    if any(kw in line for kw in ['sector_free', 'next_free_sector', 'cache_misses',
                                  'cache_interpret', 'entry_pc', '_entry_list',
                                  'sa_compile', 'sa_batch', 'sa_bitmap',
                                  'block_count', 'BLOCK_SENTINEL']):
        print(f"  {i:06d}: {line.rstrip()}")

# Now dump all lines showing the NATIVE ADDRESS of each instruction
# Group by address range to identify functions
print(f"\n=== Address range flow (last 1200 lines before flag write) ===")
range_start = max(0, flag_idx - 1200)
prev_range = ""
range_count = 0
for i in range(range_start, flag_idx + 10):
    m = re.match(r'\s*\d+:\s*([0-9A-Fa-f]{4})', lines[i])
    if not m:
        continue
    addr = int(m.group(1), 16)
    
    if 0x8000 <= addr < 0xC000:
        curr_range = "BANK17"
    elif 0xC000 <= addr < 0xD000:
        curr_range = "FIXED_C0"
    elif 0xD000 <= addr < 0xDA00:
        curr_range = "FIXED_D0"  
    elif 0xDA00 <= addr < 0xDC00:
        curr_range = "FIXED_DA"
    elif 0xDC00 <= addr < 0xDE00:
        curr_range = "FIXED_DC"
    elif 0xDE00 <= addr < 0xE000:
        curr_range = "FIXED_DE"
    elif 0xE000 <= addr < 0xE400:
        curr_range = "FIXED_E0"
    elif 0xE400 <= addr < 0xE800:
        curr_range = "FIXED_E4"
    elif 0xE800 <= addr < 0xF000:
        curr_range = "FIXED_E8"
    elif 0xF000 <= addr <= 0xFFFF:
        curr_range = "FIXED_F0"
    elif 0x6000 <= addr < 0x6100:
        curr_range = "WRAM_60"
    elif 0x6100 <= addr < 0x6200:
        curr_range = "WRAM_61"
    elif 0x6200 <= addr < 0x6400:
        curr_range = "WRAM_DISP"
    elif 0x6400 <= addr < 0x6600:
        curr_range = "WRAM_64"
    else:
        curr_range = f"OTHER_{addr>>12:X}"
    
    if curr_range != prev_range:
        if prev_range:
            print(f"  ... ({range_count} instructions)")
        print(f"  [{i:06d}] {curr_range}: ${addr:04X}  ← {lines[i].rstrip()[:100]}")
        prev_range = curr_range
        range_count = 1
    else:
        range_count += 1

print(f"  ... ({range_count} instructions)")

# Now look specifically for the FIRST bank17 call that starts the compilation
# (flash_sector_alloc or setup_flash_pc_tables)
print(f"\n=== First bank17 call (compilation start) ===")
for i in range(start, flag_idx):
    m = re.match(r'\s*\d+:\s*([0-9A-Fa-f]{4})', lines[i])
    if not m:
        continue
    addr = int(m.group(1), 16)
    if 0x8375 <= addr <= 0x8380 and 'LDA' in lines[i]:
        # Entry of setup_flash_pc_tables_b17 or flash_cache_pc_update_b17
        print(f"  {i:06d}: {lines[i].rstrip()}")
    if 0x8300 <= addr <= 0x8310 and 'LDA' in lines[i]:
        # Possible entry of flash_sector_alloc_b17
        print(f"  {i:06d}: {lines[i].rstrip()}")

# Finally: look for the run_6502 / sa_batch_compile distinction
# sa_compile_batch calls recompile functions differently
# Look for any reference to 'sa_' or the SA entry list
print(f"\n=== SA-related references ===")
for i in range(start, flag_idx):
    if 'sa_' in lines[i].lower() or 'entry_list' in lines[i].lower() or 'bitmap' in lines[i].lower():
        print(f"  {i:06d}: {lines[i].rstrip()}")
