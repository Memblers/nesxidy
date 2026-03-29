#!/usr/bin/env python3
"""Trace the full call chain leading to the flash_cache_pc_update at $AA76."""
import os, re

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\exidy.txt"

tail_bytes = 4 * 1024 * 1024
fsize = os.path.getsize(TRACE)

with open(TRACE, 'r', errors='replace') as f:
    f.seek(max(0, fsize - tail_bytes))
    if fsize > tail_bytes:
        f.readline()
    lines = f.readlines()

print(f"Read {len(lines)} lines from tail")

# Find the flag write to $AA76
flag_write_idx = None
for i, line in enumerate(lines):
    if '6041' in line[:8] and 'STA' in line and '$AA76' in line:
        flag_write_idx = i
        break

if not flag_write_idx:
    print("Flag write not found!")
    exit()

print(f"Flag write at line {flag_write_idx}")

# The call chain: flash_cache_pc_update_b17 is called via:
# flash_cache_pc_update() wrapper (fixed bank) → bankswitch_prg(BANK_COMPILE) → flash_cache_pc_update_b17()
# OR setup_and_update_pc() → similar
# OR direct from bank17 code

# flash_cache_pc_update_b17 starts around $83xx or $84xx
# Look for the JSR that called into bank17 (the bankswitch + call)
# The wrapper is: flash_cache_pc_update at ~$DC62 in fixed bank
# Which calls bankswitch_prg then calls b17 version then bankswitch back

# Let's trace JSR/RTS backwards from the flag write
print("\n=== Full JSR/RTS chain 300 lines before flag write ===")
start = max(0, flag_write_idx - 300)
for i in range(start, flag_write_idx + 5):
    line = lines[i]
    if 'JSR' in line or 'RTS' in line or 'JMP' in line:
        addr_match = re.match(r'\s*(\w{4}):', line.strip())
        if addr_match:
            print(f"  {i:06d}: {line.rstrip()[:140]}")

# Also show the setup_flash_pc_tables call - look for the function that sets
# pc_jump_flag_bank to $1C and pc_jump_flag_address to match $AA76
print("\n=== setup_flash_pc_tables call (sets pc_jump* variables) ===")
for i in range(max(0, flag_write_idx - 200), flag_write_idx):
    line = lines[i]
    if 'setup_flash_pc_tables' in line or '_pc_jump' in line:
        print(f"  {i:06d}: {line.rstrip()[:140]}")

# Find what CALLED the compile function - what was _pc when compile started?
# Look for INC _cache_misses (compile path entry) or flash_sector_alloc
print("\n=== flash_sector_alloc / cache_misses (compile entry) ===")
for i in range(max(0, flag_write_idx - 500), flag_write_idx):
    line = lines[i]
    if 'flash_sector_alloc' in line or 'cache_misses' in line:
        print(f"  {i:06d}: {line.rstrip()[:140]}")

# Show 30 lines before the peek_bank_byte check (line ~17988)
# which is the AND-corruption guard in flash_cache_pc_update_b17
peek_idx = None
for i in range(max(0, flag_write_idx - 200), flag_write_idx):
    if '_peek_bank_byte' in lines[i]:
        peek_idx = i
        break

if peek_idx:
    print(f"\n=== 50 lines before peek_bank_byte (AND-corruption guard) ===")
    for i in range(max(0, peek_idx - 50), peek_idx + 5):
        print(f"  {i:06d}: {lines[i].rstrip()[:140]}")
