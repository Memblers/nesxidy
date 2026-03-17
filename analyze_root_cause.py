#!/usr/bin/env python3
"""
Root-cause analysis: trace what sets _pc = $6A76 and how it reaches compilation.

Strategy:
1. Find the flash_byte_program that writes the bogus flag (at $AA76 in bank $1C)
2. Walk backwards to find the setup_flash_pc_tables call that set pc_jump_flag_address=$76
3. Keep walking backwards to find what C function called setup_flash_pc_tables
4. Find the moment _pc was loaded with $6A76 (STA _pc / STA _pc+1)
5. Find the previous block epilogue that wrote _pc = $6A76 (or whatever set it)
"""

import re, os

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\exidy.txt"
TAIL_BYTES = 8 * 1024 * 1024  # 8MB

def read_tail(path, nbytes):
    sz = os.path.getsize(path)
    start = max(0, sz - nbytes)
    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        f.seek(start)
        if start > 0:
            f.readline()  # discard partial
        return f.readlines()

lines = read_tail(TRACE, TAIL_BYTES)
print(f"Read {len(lines)} lines from tail")

# --- Step 1: Find the flag write for $AA76 (pc_jump_flag_address=$76, bank $1C) ---
# flash_byte_program writes to $AA76 = flash_cache_pc_flags[$76] in bank $1C
# This is the last flash_byte_program call in flash_cache_pc_update_b17
# Look for "STA" to $AA76 (the flag table entry)
flag_write_idx = None
for i in range(len(lines)-1, -1, -1):
    if 'AA76' in lines[i] and ('STA' in lines[i] or 'flash_byte' in lines[i].lower()):
        flag_write_idx = i
        break

if flag_write_idx is None:
    # Try broader: look for writes to _pc_jump_flag_address when value is $76
    for i in range(len(lines)-1, -1, -1):
        if 'pc_jump_flag_address' in lines[i] and '$76' in lines[i]:
            flag_write_idx = i
            print(f"Found pc_jump_flag_address=$76 ref at line {i}")
            break

if flag_write_idx is None:
    print("ERROR: Could not find flag write for $AA76")
    exit(1)

print(f"\nFlag write area at line {flag_write_idx}: {lines[flag_write_idx].rstrip()}")

# --- Step 2: Walk backwards to find the compile entry ---
# Look for: the moment _pc gets set to $6A76
# _pc is at some zero page address. Look for STA patterns that write $6A and $76
# to consecutive ZP locations.
# Also look for: the entry to run_6502's compile path (cache_misses increment)
# and the dispatch_on_pc return value

# First, find what ZP address _pc is at by looking for "STA _pc" in the trace
pc_zp = None
for i in range(max(0, flag_write_idx-2000), flag_write_idx):
    m = re.search(r'(STA|LDA|LDX|LDY)\s+_pc\b\s*=\s*\$([0-9A-Fa-f]+)', lines[i])
    if m:
        print(f"  _pc reference at line {i}: {lines[i].rstrip()}")
        # The "= $XX" shows current value, the ZP address is in the opcode bytes
        # Let's look at the BC: bytes to find the address
        break

# Look for _pc+1 reference
for i in range(max(0, flag_write_idx-2000), flag_write_idx):
    if '_pc+1' in lines[i] or '_pc + 1' in lines[i]:
        print(f"  _pc+1 reference at line {i}: {lines[i].rstrip()}")
        break

# --- Step 3: Find the compile-path entry ---
# Look for INC _cache_misses (indicates compile path was entered)
# Or look for flash_sector_alloc (JSR to the allocator)
# Or look for the compile entry marker
print("\n=== Searching for compile path entry markers ===")
search_start = max(0, flag_write_idx - 5000)
for i in range(flag_write_idx, search_start, -1):
    line = lines[i]
    if 'cache_misses' in line or 'INC' in line and '_cache_misses' in line:
        print(f"  cache_misses at line {i}: {line.rstrip()}")
    if 'cache_interpret' in line:
        print(f"  cache_interpret at line {i}: {line.rstrip()}")

# --- Step 4: Find the PREVIOUS dispatch_on_pc call and its return ---
# dispatch_on_pc is at $6263 (WRAM). Look for JSR $6263 or the entry.
# The return value tells us: 0=executed, 1=compile, 2=interpret
print("\n=== dispatch_on_pc calls before flag write ===")
dispatch_calls = []
for i in range(search_start, flag_write_idx):
    # dispatch_on_pc entry (it's a global label, look for the address)
    # From the asm, _dispatch_on_pc is in section "data" (WRAM)
    # The actual address depends on linker, but let's look for the pattern
    if '_dispatch_on_pc' in lines[i] or (re.search(r'\b62[0-9A-F]{2}\b.*_dispatch', lines[i], re.I)):
        dispatch_calls.append(i)

# Let's also look for the specific address pattern
# From earlier traces, dispatch_on_pc is around $6263
for i in range(search_start, flag_write_idx):
    # Look for JMP/JSR to the dispatch address
    if re.search(r'(JSR|JMP)\s+\$62[456][0-9A-Fa-f]', lines[i]):
        dispatch_calls.append(i)

dispatch_calls = sorted(set(dispatch_calls))
if dispatch_calls:
    # Show the last few
    for idx in dispatch_calls[-5:]:
        print(f"  Line {idx}: {lines[idx].rstrip()}")
else:
    print("  (none found by specific address, trying broader search)")

# --- Step 5: Look for what set _pc to contain $6A in the high byte ---
# The epilogue of a compiled block typically does:
#   LDA #imm / STA _pc   (low byte)
#   LDA #imm / STA _pc+1 (high byte)
# Look for STA to the _pc ZP location with A=$76 or A=$6A
print("\n=== Writes to _pc (ZP $5C/$5D based on typical layout) ===")
# Let's find _pc ZP address from the opcode bytes
# Look for any line with "_pc" and "STA" to determine ZP addr
pc_writes_6a = []
pc_writes_76 = []
for i in range(search_start, flag_write_idx):
    line = lines[i]
    # Look for stores with A containing $6A or $76
    if 'STA' in line and '_pc' in line:
        if 'A:6A' in line or 'A:76' in line:
            if '_pc+1' in line and 'A:6A' in line:
                pc_writes_6a.append(i)
            elif '_pc' in line and '_pc+1' not in line and 'A:76' in line:
                pc_writes_76.append(i)

print(f"  STA _pc+1 with A=$6A: {len(pc_writes_6a)} occurrences")
for idx in pc_writes_6a[-10:]:
    print(f"    Line {idx}: {lines[idx].rstrip()}")

print(f"  STA _pc with A=$76: {len(pc_writes_76)} occurrences")
for idx in pc_writes_76[-10:]:
    print(f"    Line {idx}: {lines[idx].rstrip()}")

# --- Step 6: For the LAST write of _pc=$6A76 before the flag write,
# show extended context to see what code produced it ---
if pc_writes_6a:
    last_6a = pc_writes_6a[-1]
    # Find the matching $76 write (should be nearby, within a few lines)
    matching_76 = None
    for idx in pc_writes_76:
        if abs(idx - last_6a) < 20:
            matching_76 = idx
    
    ctx_start = max(0, min(last_6a, matching_76 or last_6a) - 50)
    ctx_end = min(len(lines), max(last_6a, matching_76 or last_6a) + 20)
    
    print(f"\n=== Context around last _pc=$6A76 write (lines {ctx_start}-{ctx_end}) ===")
    for i in range(ctx_start, ctx_end):
        marker = ">>>" if i == last_6a or i == matching_76 else "   "
        print(f"  {marker} {i:06d}: {lines[i].rstrip()}")

# --- Step 7: Find what address the code was executing from ---
# The address at the left of the trace line tells us what native code set _pc
# If it's in $D000-$DFFF range, it's likely compiled block code or C code in fixed bank
# If it's in $8000-$BFFF, it's switchable bank code
# If it's in $6000-$7FFF, it's WRAM code (dispatch, etc.)
if pc_writes_6a:
    last_6a = pc_writes_6a[-1]
    # Extract the address from the trace line
    m = re.match(r'\s*(\w+):\s*([0-9A-Fa-f]{4})', lines[last_6a])
    if m:
        addr = int(m.group(2), 16)
        print(f"\n=== Code that writes _pc+1=$6A is at native address ${addr:04X} ===")
        if 0x8000 <= addr < 0xC000:
            print(f"  → Switchable bank ($8000-$BFFF) — compiled block or bank 17 code")
        elif 0xC000 <= addr <= 0xFFFF:
            print(f"  → Fixed bank ($C000-$FFFF) — C runtime / fixed code")
        elif 0x6000 <= addr < 0x8000:
            print(f"  → WRAM ($6000-$7FFF) — dispatch or hand-written asm")

# --- Step 8: Look for the COMPILED BLOCK that set _pc = $6A76 ---
# A compiled block epilogue typically looks like:
#   LDA #$76 / STA _pc       (or STA _a + other regs)
#   LDA #$6A / STA _pc+1
#   ...
#   JMP _flash_dispatch_return
# Show 100 lines before the _pc write to see the whole block epilogue
if pc_writes_6a:
    last_6a = pc_writes_6a[-1]
    ctx_start = max(0, last_6a - 100)
    print(f"\n=== 100 lines before _pc+1=$6A write (block epilogue context) ===")
    for i in range(ctx_start, last_6a + 5):
        line = lines[i].rstrip()
        # Highlight interesting instructions
        highlight = ""
        if 'STA _pc' in line:
            highlight = " <<<< _pc WRITE"
        elif 'STA _a' in line:
            highlight = " <<<< _a WRITE"
        elif 'JMP' in line and ('dispatch' in line.lower() or 'flash' in line.lower() or '$FFF' in line):
            highlight = " <<<< DISPATCH JUMP"
        elif 'RTS' in line or 'JMP' in line:
            highlight = " <<<< CONTROL FLOW"
        elif '_flash_dispatch_return' in line:
            highlight = " <<<< DISPATCH RETURN"
        # Show address ranges to identify which block this is
        print(f"  {i:06d}: {line}{highlight}")

# --- Step 9: Trace the compile entry path ---
# After the _pc write and dispatch return, the C code checks the return value.
# Find the transition from dispatch → compile
if pc_writes_6a:
    last_6a = pc_writes_6a[-1]
    # After the _pc write, look for the dispatch_on_pc return and then
    # the compile path entry
    print(f"\n=== After _pc=$6A76: dispatch return → compile entry ===")
    end_search = min(len(lines), last_6a + 500)
    for i in range(last_6a, end_search):
        line = lines[i].rstrip()
        interesting = False
        if any(kw in line for kw in ['dispatch', 'cache_misses', 'cache_interpret',
                                      'flash_sector_alloc', 'sector_free', 'interpret_6502',
                                      '_bankswitch', 'ROM_ADDR', 'rom_addr',
                                      'batch_exit', 'JMP ($FFFC)', 'not_recompiled',
                                      '.needs_compile', '.out_of_range']):
            interesting = True
        if re.search(r'\b62[456][0-9A-F]\b', line):  # dispatch_on_pc area
            interesting = True
        if 'RTS' in line:
            interesting = True
        if 'JSR' in line:
            interesting = True
        if interesting:
            print(f"  {i:06d}: {line}")

print("\n=== Analysis complete ===")
