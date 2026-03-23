"""
Trace back MUCH further to find who set _pc = $C805.

The crash block has entry_pc=$C805. This came from _pc being $C805 when
run_6502 entered compile mode. Something BEFORE set _pc to $C805.

The previous block's epilogue wrote exit_pc = $C805 into _pc.
Or an interpreted instruction left _pc = $C805.

Strategy: Search backward from crash for the PREVIOUS block execution
that set _pc. Look for:
1. The previous dispatch_on_pc call and what block ran before
2. Any STA to _pc ($51) that stored $05
3. The previous block's epilogue
"""

import os

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"
TAIL_SIZE = 10 * 1024 * 1024

file_size = os.path.getsize(TRACE)
offset = max(0, file_size - TAIL_SIZE)

lines = []
with open(TRACE, 'r', encoding='utf-8', errors='replace') as f:
    f.seek(offset)
    if offset > 0:
        f.readline()
    lines = f.readlines()

print(f"Read {len(lines)} lines from tail")

# Find crash
crash_idx = None
for i in range(len(lines) - 1, max(0, len(lines) - 5000), -1):
    if 'LAX' in lines[i] and '$70CB' in lines[i]:
        crash_idx = i
        break

print(f"Crash at T{crash_idx}")

# The compile of $C805 starts somewhere before the crash.
# The compile loop writes flash bytes. The first flash write for this
# block was at entry #15 in the earlier analysis (addr=$84A8, data=$05).
# Let me find the compile START — look for recompile_opcode at $E21F
# or the entry into run_6502's compile path.

# First, let me find where the compile of $C805 began by looking for
# the flash_sector_alloc call or the setup_flash_pc_tables call.
# 
# Actually, the SIMPLEST approach: find the PREVIOUS dispatch that 
# returned non-zero (compile needed), which triggered the compile.
# 
# run_6502 calls dispatch_on_pc. If it returns 1, we compile.
# dispatch_on_pc at $623A, returns via RTS.
# After dispatch returns, run_6502 checks the return value.
#
# Let me look for the dispatch_on_pc that was called before the
# compile of the $C805 block.

# The deferred publish code at $DD58 reads _cache_entry_pc.
# Before that, the compile loop ran. Before THAT, dispatch_on_pc
# was called and returned "not recompiled".
#
# Let me find all dispatch_on_pc calls (JSR $623A or at $623A) 
# in the last 2000 lines before crash, and trace what happened.

print(f"\n=== All dispatch_on_pc entries in last 2000 lines ===")
search_start = max(0, crash_idx - 2000)
dispatches = []
for i in range(search_start, crash_idx + 1):
    line = lines[i].rstrip()
    if '623A' in line[:6]:
        dispatches.append(i)

print(f"Found {len(dispatches)} dispatch_on_pc calls")
for d in dispatches:
    print(f"  T{d}: {lines[d].rstrip()[:100]}")

# For each dispatch, what was _pc?
# _pc is at ZP $51/$52. dispatch_on_pc reads it at:
# LDA _pc at $6256 (lo), LDA $52 at $6240 (hi)
for d in dispatches:
    pc_lo = None
    pc_hi = None
    for j in range(d, min(d+30, len(lines))):
        line = lines[j].rstrip()
        if '6256' in line[:6] and 'LDA _pc' in line:
            # Extract value after = sign
            idx = line.find('_pc =')
            if idx >= 0:
                pc_lo = line[idx+5:idx+9].strip().lstrip('$')
        if '6240' in line[:6] and 'LDA $52' in line:
            idx = line.find('$52 =')
            if idx >= 0:
                pc_hi = line[idx+5:idx+9].strip().lstrip('$')
    if pc_lo and pc_hi:
        print(f"  T{d}: _pc = ${pc_hi}{pc_lo}")
    
# Now let me look MUCH further back to find the block that set _pc=$C805.
# The block that set _pc=$C805 would have an epilogue that writes to _pc ($51).
# Let me search for STA $51 (STA _pc) with value $05 combined with 
# a previous store of $C8 to $52.

# Actually, let me look at the ENTIRE compile sequence for this block.
# The compile happens between the dispatch returning "not_recompiled" 
# and the deferred publish. Let me find not_recompiled ($630C).

print(f"\n=== not_recompiled at $630C in last 2000 lines ===")
for i in range(search_start, crash_idx + 1):
    line = lines[i].rstrip()
    if '630C' in line[:6]:
        print(f"T{i}: {line[:120]}")

# The flow should be:
# 1. Previous block epilogue sets _pc=$C805, returns to dispatch
# 2. run_6502 gets control, checks _pc, starts compile for $C805
# 3. Compile writes flash bytes
# 4. Deferred publish writes PC tables
# 5. Dispatch $C805 → crash

# Let me find the BLOCK that set _pc=$C802 (which then got compiled
# and set exit_pc=$C805). Look at the compile for $C802.

# The compile writes for $C802 happened at fr=23702 (earlier).
# The $C805 compile is at fr=23703. So there's a frame boundary between.

# Let me search much further back for the compile of $C802.
# Search backward from crash for cache_entry_pc stores with $02/$C8

print(f"\n=== Searching for earlier compile chain ===")
# Look for stores to _cache_entry_pc_lo ($94) with value $02
# and _cache_entry_pc_hi ($95) with value $C8
for i in range(search_start, crash_idx):
    line = lines[i].rstrip()
    if '_cache_entry_pc_lo' in line:
        # Extract value
        idx = line.find('_cache_entry_pc_lo =')
        if idx >= 0:
            val = line[idx+20:idx+25].strip().lstrip('$')
            if val in ('02', '$02', '05', '$05'):
                print(f"T{i}: {line[:140]}")
                # Show next few lines for context
                for j in range(i+1, min(i+5, len(lines))):
                    print(f"  T{j}: {lines[j].rstrip()[:140]}")

# Now let me look at the block that PREVIOUSLY ran and set _pc.
# The key: before dispatch at T90337, something set _pc=$C805.
# Let me look at what the last compiled block's epilogue did.
# The epilogue stores exit_pc to _pc. 

# Search backward from T90337 for STA _pc ($85 $51 in bytecode)
# or STA $51 pattern
print(f"\n=== STA _pc stores before crash dispatch ===")
for i in range(crash_idx - 1, max(0, crash_idx - 5000), -1):
    line = lines[i].rstrip()
    if 'STA _pc' in line or 'STA $51' in line:
        # Get the value being stored (A register)
        if ' A:' in line:
            a_idx = line.index(' A:')
            a_val = line[a_idx+3:a_idx+5]
            print(f"T{i}: A={a_val} {line[:130]}")
            if a_val == '05':
                # Show surrounding context
                print("  *** Found STA _pc with A=$05! Context: ***")
                for j in range(max(0, i-15), i+5):
                    print(f"    T{j}: {lines[j].rstrip()[:140]}")
                break
