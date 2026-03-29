"""Find callers of the trampoline and our function. Map the call chain."""
import os

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\exidy.txt"
fsize = os.path.getsize(TRACE)

# Read last 20MB
chunk_size = 20 * 1024 * 1024
with open(TRACE, 'rb') as f:
    f.seek(max(0, fsize - chunk_size))
    chunk = f.read().decode('utf-8', errors='replace')
lines = chunk.splitlines()

# The call chain is:
#   Some caller (in bank 2) -> JSR $DCF8 (trampoline)
#   $DCF8: saves regs, bankswitches to bank 17, JSR $843E
#   $843E: the old dead _b17 function
#
# Our new function is at $DE39. The label file says that's _ir_emit_direct_branch_placeholder.
# But the callers are going through a different trampoline at $DCF8.
#
# The callers are in bank 2. The JSR would be to the fixed bank address.
# In the source, the call sites do: ir_emit_direct_branch_placeholder(target_pc, op_buffer_0)
# The linker resolved that to... let's check.

# Find all JSR targets that call the trampoline
print("=== ALL JSR $DCF8 calls (with 2 lines before) ===")
count = 0
for i, line in enumerate(lines):
    if 'JSR $DCF8' in line:
        count += 1
        if count <= 5:
            start = max(0, i - 2)
            for j in range(start, i + 1):
                print(f"  {lines[j][:160]}")
            print()
print(f"Total JSR $DCF8: {count}")

# Also check for direct JSR $DE39 (our new function)  
print("\n=== ALL JSR $DE39 calls ===")
count2 = 0
for i, line in enumerate(lines):
    if 'JSR $DE39' in line:
        count2 += 1
        if count2 <= 5:
            print(f"  {lines[i][:160]}")
print(f"Total JSR $DE39: {count2}")

# Check what labels correspond to what addresses
# The trace shows the trampoline at $DCF8. Our source has:
# - ir_emit_direct_branch_placeholder (new, no bankswitch) should be the one callers use
# - But there's clearly STILL a trampoline at $DCF8 doing the bankswitch
#
# Let me check: does vbcc still generate a bank17 version even though we removed the pragma?
# The source callers do `ir_emit_direct_branch_placeholder(target_pc, op_buffer_0)`
# which links to $DE39. But the trace shows them calling $DCF8 instead.
# 
# Wait - maybe the callers are in bank 2, and they can't call $DE39 (fixed bank) directly?
# No, fixed bank ($C000-$FFFF) is always mapped.
# 
# OR: maybe the ROM hasn't been rebuilt / loaded correctly. Let's verify.

# Check the label file for $DCF8 = $5CF8
print("\n=== LABEL MAP around $5CF0-$5D10 ===")
with open("exidy.mlb", "r") as f:
    for line in f:
        line = line.strip()
        for prefix in ["P:5CF", "P:5D0", "P:5D1", "P:5D2", "P:5D3", "P:5D4",
                       "P:5D5", "P:5D6", "P:5D7", "P:5D8", "P:5D9", "P:5DA",
                       "P:5DB", "P:5DC", "P:5DD", "P:5DE", "P:5DF", "P:5E0",
                       "P:5E1", "P:5E2", "P:5E3", "P:5E4"]:
            if line.startswith(prefix):
                print(f"  {line}")
                break

# Now let's verify: is the ROM in Mesen the one we just built?
# Check the exidy.nes file timestamp
import time
stat = os.stat("exidy.nes")
print(f"\nexidy.nes last modified: {time.ctime(stat.st_mtime)}")
print(f"exidy.nes size: {stat.st_size}")

# Check if there's a backup copy that Mesen might be using
for path in ["exidy.nes", "exidy2.nes"]:
    if os.path.exists(path):
        s = os.stat(path)
        print(f"{path}: {s.st_size} bytes, modified {time.ctime(s.st_mtime)}")
