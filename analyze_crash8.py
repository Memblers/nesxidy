#!/usr/bin/env python3
"""Deeper trace analysis - find what called flash_cache_pc_update for PC $6A76."""
import os, re

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\exidy.txt"

# Read last 2MB for much more context
tail_bytes = 2 * 1024 * 1024
fsize = os.path.getsize(TRACE)
print(f"Trace file size: {fsize:,} bytes ({fsize/1024/1024:.1f} MB)")

with open(TRACE, 'r', errors='replace') as f:
    f.seek(max(0, fsize - tail_bytes))
    if fsize > tail_bytes:
        f.readline()  # skip partial line
    lines = f.readlines()

print(f"Read {len(lines)} lines from tail")

# Find the crash point
crash_idx = None
for i, line in enumerate(lines):
    if 'JMP PLATFORM_NES' in line or ('4C 00 00' in line and '62DA' in line[:20]):
        crash_idx = i
        break

if crash_idx is None:
    for i, line in enumerate(lines):
        if '0000' in line[:8] and ('BRK' in line or 'ROR' in line):
            crash_idx = i
            break

if crash_idx:
    print(f"Crash at line {crash_idx}")
else:
    print("No crash found!")
    exit()

# Find ALL flash_byte_program calls (JSR $6000 or JSR _flash_byte_program)
# that write to bank $1C (28) - these are PC flag writes for $4000-$7FFF
print("\n=== flash_byte_program calls writing to bank $1C in last 2000 lines ===")
search_start = max(0, crash_idx - 2000)
for i in range(search_start, crash_idx):
    line = lines[i]
    # Look for the pattern: STX r2 or STA r2 with value $1C right before JSR $6000
    # Or LDA _pc_jump_flag_bank = $1C
    if '$1C' in line and ('flag_bank' in line.lower() or 'r2' in line):
        # Show context: this line + next 5
        for j in range(max(0,i-3), min(len(lines), i+8)):
            print(f"  {j:06d}: {lines[j].rstrip()[:120]}")
        print("  ---")

# Find what function is executing right before the flag write
# The flag write at address ~$8428 is flash_cache_pc_update_b17
# The caller should be at an address in fixed bank ($C000-$FFFF) or compile bank ($8xxx)
# Look for JSR/RTS patterns around the flag write
print("\n=== Code flow before the flag write (JSR/RTS patterns) ===")
# Find the flash_byte_program call for the flag write
flag_write_idx = None
for i in range(search_start, crash_idx):
    line = lines[i]
    if '6041' in line[:8] and 'STA' in line and '$AA76' in line:
        flag_write_idx = i
        break

if flag_write_idx:
    print(f"Flag write at line {flag_write_idx}")
    # Show 100 lines before the flag write
    start = max(0, flag_write_idx - 100)
    for i in range(start, flag_write_idx + 5):
        addr_match = re.match(r'(\w{4}):', lines[i].strip())
        if addr_match:
            addr = int(addr_match.group(1), 16)
            op = lines[i].strip()
            # Highlight JSR, RTS, JMP
            if 'JSR' in op or 'RTS' in op or 'JMP' in op:
                print(f"  {i:06d}: {lines[i].rstrip()[:140]}")
        # Also show lines that reference $AA76 or PC $6A76
        if '$AA76' in lines[i] or '$6A76' in lines[i] or '_pc_jump_flag' in lines[i]:
            print(f"  {i:06d}: {lines[i].rstrip()[:140]}")
else:
    print("Flag write to $AA76 not found, searching for STA (r2),Y with $1C...")
    for i in range(search_start, crash_idx):
        if '91 02' in lines[i] and 'AA76' in lines[i]:
            print(f"  Found at line {i}: {lines[i].rstrip()[:120]}")

# Also look for the very first dispatch_on_pc result=1 that starts the compile
print("\n=== dispatch_on_pc returning 1 (compile) near the crash ===")
# dispatch_on_pc returns 1 at .needs_compile: LDA #1 / RTS
# The caller (run_6502) checks result and goes to compile
# Look for the compile start: entry to run_6502 compile path
for i in range(search_start, crash_idx):
    line = lines[i]
    # After dispatch_on_pc returns 1, run_6502 increments cache_misses
    # The assembly is: INC _cache_misses / ... / LDA _pc+1 / STA r27
    if '_cache_misses' in line and 'INC' in line:
        # Show context
        for j in range(max(0,i-5), min(len(lines), i+15)):
            print(f"  {j:06d}: {lines[j].rstrip()[:120]}")
        print("  ---")

# What is the ENTRY PC for this block compile?
print("\n=== Entry PC values (writes to cache_entry_pc_lo/hi) ===")
for i in range(search_start, crash_idx):
    line = lines[i]
    if 'cache_entry_pc' in line and 'STA' in line:
        print(f"  {i:06d}: {lines[i].rstrip()[:120]}")
