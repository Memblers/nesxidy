#!/usr/bin/env python3
"""
Root cause analysis v2: Find what sets guest PC to $6A76 before compilation.
Search for raw ZP address writes and the code executing at $DA00-$DC00 area.
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

# Find the flag write (STA to $AA76)
flag_idx = None
for i in range(len(lines)-1, -1, -1):
    if 'AA76' in lines[i]:
        flag_idx = i
        break
if flag_idx is None:
    print("ERROR: no $AA76 ref found"); exit(1)
print(f"Flag write at line {flag_idx}")

# Just dump 600 lines before the flag write - raw trace
# This should show us:
# 1. The compiled block epilogue that sets _pc = $6A76
# 2. The dispatch_on_pc call that returns 1 (compile)
# 3. The C compile path entry
# 4. The setup_flash_pc_tables / flash_cache_pc_update calls

# First, let's find the _pc ZP address by searching for the label
# In Mesen traces, ZP accesses show the label if known
pc_zp_lo = None
for i in range(max(0, flag_idx - 5000), flag_idx):
    m = re.search(r'(STA|LDA)\s+_pc\s*=', lines[i])
    if m:
        # Get the opcode bytes to determine ZP address
        bc = re.search(r'BC:([0-9A-Fa-f ]+)', lines[i])
        if bc:
            bytes_str = bc.group(1).strip().split()
            if len(bytes_str) >= 2:
                pc_zp_lo = int(bytes_str[1], 16)
                print(f"_pc is at ZP ${pc_zp_lo:02X} (from line {i})")
                break

# Also search for _pc+1
for i in range(max(0, flag_idx - 5000), flag_idx):
    if '_pc+1' in lines[i] or '_pc + 1' in lines[i]:
        bc = re.search(r'BC:([0-9A-Fa-f ]+)', lines[i])
        if bc:
            bytes_str = bc.group(1).strip().split()
            if len(bytes_str) >= 2:
                pc_zp_hi = int(bytes_str[1], 16)
                print(f"_pc+1 is at ZP ${pc_zp_hi:02X}")
                break

# Now dump 800 lines before the flag write with annotations
print(f"\n=== 800 lines before flag write (annotated) ===")
start = max(0, flag_idx - 800)
for i in range(start, min(len(lines), flag_idx + 20)):
    line = lines[i].rstrip()
    
    # Extract address
    m = re.match(r'\s*\d+:\s*([0-9A-Fa-f]{4})', line)
    addr = int(m.group(1), 16) if m else 0
    
    tags = []
    if '_pc+1' in line and 'STA' in line:
        tags.append("*** _pc HI WRITE ***")
    if '_pc' in line and '_pc+1' not in line and '_pc_' not in line and 'STA' in line:
        tags.append("*** _pc LO WRITE ***")
    if 'dispatch_on_pc' in line.lower() or 'dispatch' in line.lower():
        tags.append("[DISPATCH]")
    if 'not_recompiled' in line.lower() or 'needs_compile' in line.lower():
        tags.append("[NEEDS_COMPILE]")
    if 'cache_misses' in line:
        tags.append("[COMPILE ENTRY]")
    if 'cache_interpret' in line:
        tags.append("[INTERPRET]")
    if 'flash_sector_alloc' in line:
        tags.append("[ALLOC]")
    if 'interpret_6502' in line:
        tags.append("[INTERPRET_6502]")
    if 'RTS' in line and 0x6200 <= addr <= 0x6500:
        tags.append("[WRAM RTS]")
    if 'JMP' in line and ('$FFF0' in line or '$FFF6' in line):
        tags.append("[FFF0 DISPATCH]")
    if 'flash_dispatch_return' in line:
        tags.append("[BLOCK RETURN]")
    if 'AA76' in line:
        tags.append("[*** FLAG WRITE TARGET ***]")
    if 'setup_flash_pc' in line:
        tags.append("[SETUP PC TABLES]")
    if 'pc_jump_flag' in line:
        tags.append("[PC FLAG]")
    if 0xDA00 <= addr <= 0xDC00:
        if 'STA' in line and ('A:6A' in line or 'A:76' in line):
            tags.append("*** SUSPICIOUS STORE ***")
    
    tag_str = " " + " ".join(tags) if tags else ""
    
    # Only print annotated lines or every 10th line for context
    if tags or (i - start) % 10 == 0 or i >= flag_idx - 5:
        print(f"{i:06d}: {line}{tag_str}")

# --- Also: Find ALL writes where A=$6A to any ZP location in the 1000 lines before ---
print(f"\n=== All STA with A=$6A within 1000 lines before flag write ===")
for i in range(max(0, flag_idx - 1000), flag_idx):
    if 'STA' in lines[i] and 'A:6A' in lines[i]:
        print(f"  {i:06d}: {lines[i].rstrip()}")

print(f"\n=== All STA with A=$76 within 1000 lines before flag write ===")
for i in range(max(0, flag_idx - 1000), flag_idx):
    if 'STA' in lines[i] and 'A:76' in lines[i]:
        print(f"  {i:06d}: {lines[i].rstrip()}")

# Look for the entry_pc variable being set = the moment pc is saved as entry_pc
# In VBCC generated code, entry_pc = pc would be LDA _pc / STA somewhere
# entry_pc_lo and entry_pc_hi are stored in cache_entry_pc_lo[0] and cache_entry_pc_hi[0]
print(f"\n=== Writes to cache_entry_pc within 1000 lines before flag write ===")
for i in range(max(0, flag_idx - 1000), flag_idx):
    if 'cache_entry_pc' in lines[i]:
        print(f"  {i:06d}: {lines[i].rstrip()}")
