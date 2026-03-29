#!/usr/bin/env python3
"""
Root cause v5: Find the dispatch_on_pc call that triggered compilation for $6A76.
Key: _pc at ZP $5B (lo), $5C (hi). dispatch_on_pc at $6263.
The flag write is at line ~73448. The byte-write loop (FIX_D8 ↔ WRAM_FLASH)
starts at ~71948. The compile path entry is BEFORE that.
Search for the dispatch_on_pc entry where _pc = $6A76.
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

# _pc is at ZP $5B (lo), $5C (hi)
# dispatch_on_pc entry reads _pc+1: "LDA $5C" at the start of the function
# The dispatch_on_pc function starts at ~$6263 (WRAM)

# Find ALL dispatch_on_pc entries where _pc+1 = $6A
# These are: lines with address ~$626x and instruction reading $5C
print("=== All dispatch_on_pc entries with _pc+1=$6A (before flag write at ~73448) ===")
for i in range(len(lines)):
    line = lines[i]
    # dispatch_on_pc reads _pc+1 early in its body
    # Pattern: address 626x-627x, instruction "LDA $5C" or "LDA _pc+1"
    if ('A5 5C' in line or 'LDA _pc+1' in line) and '$5C' in line:
        # Check if A contains $6A after this load  
        # Actually, we need to look at the NEXT line to see what A becomes
        # Or look at the current value: the trace shows the value AT the address
        # "LDA $5C = $XX" shows the value at $5C before the load
        if '= $6A' in line:
            addr_match = re.match(r'([0-9A-Fa-f]{4})', line.strip())
            if addr_match:
                addr = int(addr_match.group(1), 16)
                if 0x6260 <= addr <= 0x62A0:
                    print(f"  {i:06d}: {line.rstrip()}")

# Also look for ALL "JSR _dispatch_on_pc" or "JSR $6263" calls
print("\n=== All JSR to dispatch_on_pc ===")
dispatch_calls = []
for i in range(len(lines)):
    if 'dispatch_on_pc' in lines[i] and 'JSR' in lines[i]:
        dispatch_calls.append(i)
    elif 'JSR' in lines[i] and '$6263' in lines[i]:
        dispatch_calls.append(i)

print(f"  Total JSR to dispatch_on_pc: {len(dispatch_calls)}")
for idx in dispatch_calls[-20:]:
    # Show the _pc value by looking at surrounding context
    print(f"  {idx:06d}: {lines[idx].rstrip()}")

# Find the compile-path entry: where does the code transition from dispatch
# to the compile code in the fixed bank? Look for the switch(result) code.
# On Exidy (non-batch): case 1 breaks, then falls through to compile code.
# The entry guard is at ~$D??? in the fixed bank.

# Look for the transition: WRAM_DISP ($62xx) → FIX_D8 or FIX_DC ($D8xx-$DCxx)
# This marks the return from dispatch to the C compile path.
print("\n=== dispatch_on_pc returns (WRAM → FIX_D8/DC) ===")
for i in range(max(0, len(lines)-10000), len(lines)):
    line = lines[i]
    addr_m = re.match(r'([0-9A-Fa-f]{4})', line.strip())
    if not addr_m:
        continue
    addr = int(addr_m.group(1), 16)
    
    # Look for RTS from dispatch_on_pc area
    if 0x6260 <= addr <= 0x6300 and 'RTS' in line:
        # Check what the return value (A register) is
        a_match = re.search(r'A:([0-9A-Fa-f]{2})', line)
        if a_match:
            a_val = int(a_match.group(1), 16)
            tag = ""
            if a_val == 0: tag = " [executed]"
            elif a_val == 1: tag = " [COMPILE]"
            elif a_val == 2: tag = " [interpret]"
            print(f"  {i:06d}: {line.rstrip()}{tag}")

# Look specifically for the WRAM_DISP region showing the asm guard for $6A76
# The .needs_compile guard: LDA _pc+1 / CMP #$28 / BCC / CMP #$40 / BCS
# At addresses around $62D0-$62F0 (approximate)
print("\n=== .needs_compile guard hits (CMP #$40 in dispatch area) ===")
for i in range(len(lines)):
    line = lines[i]
    addr_m = re.match(r'([0-9A-Fa-f]{4})', line.strip())
    if not addr_m:
        continue
    addr = int(addr_m.group(1), 16)
    if 0x6260 <= addr <= 0x6310 and 'CMP #$40' in line:
        print(f"  {i:06d}: {line.rstrip()}")
        # Show a few lines after for context (BCS result)
        for j in range(i+1, min(i+5, len(lines))):
            print(f"  {j:06d}: {lines[j].rstrip()}")
        print()
