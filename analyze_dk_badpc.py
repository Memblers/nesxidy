"""
Trace why _pc ends up at $C805 (mid-instruction address).

$C804 = JMP $CBB7 ($4C $B7 $CB), so $C805 is the operand byte $B7.
$C801 = JSR $CBAE ($20 $AE $CB), so $C802 is the operand byte $AE.

We need to find:
1. What block set exit_pc = $C805 or $C802?
2. Was it the SA walk that enqueued these PCs?
3. Or was it an interpreter step that advanced PC incorrectly?

Strategy: Search the last 10MB of the trace for:
- STA to _pc / _pc+1 that writes $05/$C8 or $02/$C8
- The block that executed just before dispatch to $C805
- How the compile of $C802 and $C805 was triggered
"""

import os

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"
TAIL_SIZE = 10 * 1024 * 1024  # 10MB

file_size = os.path.getsize(TRACE)
offset = max(0, file_size - TAIL_SIZE)

lines = []
with open(TRACE, 'r', encoding='utf-8', errors='replace') as f:
    f.seek(offset)
    if offset > 0:
        f.readline()  # skip partial
    lines = f.readlines()

print(f"Read {len(lines)} lines from tail ({TAIL_SIZE // (1024*1024)}MB)")

# Find lines where _pc is being set to $C805 or $C802
# _pc is a ZP variable. We need to find its address.
# From the labels: _pc is at some ZP location.
# In the dispatch code: `lda _pc+1` followed by `lda _pc` 
# The epilogue writes exit_pc to _pc.
# Let's search for writes to _pc by looking for STA patterns near $C8xx PCs

# Strategy: Find the compilation of $C805 block by looking for 
# cache_entry_pc stores, then trace backward to find what set _pc=$C805

# First, find the crash line
crash_idx = None
for i in range(len(lines) - 1, max(0, len(lines) - 5000), -1):
    line = lines[i].rstrip()
    if 'LAX' in line and '$70CB' in line:
        crash_idx = i
        print(f"Crash at T{i}: {line[:120]}")
        break

if crash_idx is None:
    print("Crash line not found in tail!")
    exit(1)

# Find dispatch_on_pc entry just before the crash
# dispatch_on_pc is at $623A
for i in range(crash_idx, max(0, crash_idx - 200), -1):
    line = lines[i].rstrip()
    if '623A' in line[:6] or '623B' in line[:6]:
        print(f"\nDispatch entry at T{i}: {line[:120]}")
        break

# Now find what happened before dispatch - the previous block exit
# The previous block's epilogue would have stored exit_pc into _pc
# and then returned to dispatch. Let me look at what happened ~50-100 lines
# before the dispatch to $C805.

print(f"\n=== 100 lines before crash (T{crash_idx-100} to T{crash_idx}) ===")
start = max(0, crash_idx - 100)
for i in range(start, crash_idx + 1):
    line = lines[i].rstrip()
    # Show everything - we need to see the block exit and PC setting
    addr = line[:4].strip()
    print(f"T{i:>6}: {line[:140]}")

# Also search for what wrote entry_pc = $C805 during compilation
# Look for STA to cache_entry_pc addresses
print(f"\n\n=== Searching for entry_pc_lo=$05 writes near compile ===")
# The compile of $C805 would have cache_entry_pc_lo = $05
# and cache_entry_pc_hi = $C8. Let me find where these are stored.
# From earlier analysis, entry_pc setup stores to block header at 
# flash_code_address in bank 8.
# entry_pc lo = $05 written to $84A8, entry_pc hi = $C8 written to $84A9

# Let me find the first write to $84A8 (entry_pc_lo for the $C805 block)
for i in range(len(lines)):
    line = lines[i].rstrip()
    if 'addr=$84A8' in line or ('$84A8' in line and 'data=$05' in line):
        print(f"T{i}: {line[:140]}")

# Actually, let me look for the block that executed BEFORE the crash dispatch.
# The dispatch_on_pc at the crash returns to run_6502 which re-enters.
# The PREVIOUS dispatch would have executed a compiled block whose epilogue
# set _pc to $C805. Let me find that.

print(f"\n=== Looking for previous flash_dispatch_return ===")
# flash_dispatch_return is at $62B4 area
# The previous block's epilogue does: STA _pc (lo), STA _pc+1 (hi), then
# eventually returns to dispatch
for i in range(crash_idx - 1, max(0, crash_idx - 500), -1):
    line = lines[i].rstrip()
    # Look for the cross_bank_dispatch or flash_dispatch_return
    if '62B4' in line[:6] or '621B' in line[:6]:
        print(f"T{i}: {line[:140]}")
        # Show context
        for j in range(max(0, i-20), i):
            print(f"  T{j}: {lines[j].rstrip()[:140]}")
        break

# Let me also look for the interpret_6502 call that might have set bad PC
print(f"\n=== Searching for interpret_6502 calls near crash ===")
# interpret_6502 = _interpret_6502 in asm
for i in range(crash_idx - 1, max(0, crash_idx - 2000), -1):
    line = lines[i].rstrip()
    if 'interpret' in line.lower() or '_interpret_6502' in line:
        print(f"T{i}: {line[:140]}")
