"""Find ALL callers of ir_emit_direct_branch_placeholder and the old trampoline."""
import os

# The trace shows calls from:
#   $DA9F -> JSR $DCF8  (fixed bank)
#   $CE1D -> JSR $DCF8  (fixed bank)  
#   $CFEC -> JSR $DCF8  (fixed bank)
#
# But we also found JSR $DCF8 at:
#   bank 24 ($8A88, $8B20, $932B)
#   bank 31 ($8DE6, $8E1D, $8FEC, $9A9F, $9B29) -- wait, bank 31 IS the fixed bank
#
# Fixed bank = last bank = bank 31 for 32 banks. 
# Bank 31 maps to $8000-$BFFF? No -- fixed bank maps to $C000-$FFFF.
# In the 512KB ROM, the fixed bank is PRG offset $7C000-$7FFFF.
# $7CDE6 = offset in PRG. Bank 31 at $C000+offset = $C000 + ($7CDE6-$7C000) = $C000+$DE6 = $CDE6
# But the trace shows $CE1D calling JSR $DCF8... let me recalculate.
# 
# PRG ROM is 512KB = $80000 bytes. 32 banks of 16KB each.
# Fixed bank (last 16KB) = PRG $7C000-$7FFFF, maps to CPU $C000-$FFFF.
# So PRG offset $7CDE6: CPU addr = $C000 + ($7CDE6 - $7C000) = $C000 + $DE6 = $CDE6
# PRG offset $7CE1D: CPU addr = $C000 + $E1D = $CE1D  ✓ matches trace!
# PRG offset $7CFEC: CPU addr = $C000 + $FEC = $CFEC  ✓ matches trace!
# PRG offset $7DA9F: CPU addr = $C000 + $1A9F = $DA9F ✓ matches trace!
# PRG offset $7DB29: CPU addr = $C000 + $1B29 = $DB29

# So there are 5 JSR $DCF8 calls in the FIXED BANK itself, plus 3 in bank 24.
# Our fix only created JSR $DE39 in bank 2.
# 
# The fixed-bank callers must be the trampolines for OTHER functions that
# also call ir_emit_direct_branch_placeholder, OR they are the
# recompile_opcode_b2_inner calls that got compiled differently.
#
# Wait. The function is called from recompile_opcode_b2_inner() which is
# in bank 2 (#pragma section bank2). But the compiler might inline or
# the linker might have placed code in both banks.
#
# Actually: recompile_opcode_b2_inner is in bank 2, but recompile_opcode()
# is in the fixed bank as a trampoline. The call chain is:
#   recompile_opcode() [fixed] -> bankswitch to bank2 -> recompile_opcode_b2() [bank2]
#   -> recompile_opcode_b2_inner() [bank2] -> ir_emit_direct_branch_placeholder() [fixed]
#
# When bank 2 calls ir_emit_direct_branch_placeholder at $DE39, it works
# because the fixed bank is always visible at $C000-$FFFF.
#
# But the fixed-bank callers at $CDE6, $CE1D, etc. -- what are those?
# Those must be DIFFERENT functions also calling something at $DCF8.
# $DCF8 in the new build is _setup_flash_pc_tables!

# Let me check: in the NEW build, what is $DCF8?
with open("exidy.nes", "rb") as f:
    f.seek(16 + 0x7C000)  # fixed bank start in PRG
    fixed = f.read(16384)

# $DCF8 = $C000 + $CF8 = offset $CF8 in fixed bank
print("=== Fixed bank at offset $CF8 ($DCF8) - new build ===")
ctx = fixed[0xCF8:0xCF8+32]
print(f"Hex: {ctx.hex(' ')}")

# $DD1B = offset $D1B in fixed bank  
print("\n=== Fixed bank at offset $D1B ($DD1B) - contains JSR $843E ===")
ctx2 = fixed[0xD1B:0xD1B+32]
print(f"Hex: {ctx2.hex(' ')}")

# The old trampoline at $DD1B has: 20 3E 84 (JSR $843E)
# What function is this part of? Let me find the sub start before $DD1B.
# Search backward for a pattern indicating function entry
# Actually let me check what label is just before $DD1B
# $DD1B in PRG = $7C000 + $D1B = $7CD1B -> PRG offset $5CD1B? No.
# Label file uses PRG ROM offsets directly.
# $DD1B - $C000 = $1D1B, in the fixed bank. PRG offset = $7C000 + $1D1B = $7DD1B
# But labels use a different scheme: P:XXXX is the PRG ROM address.
# $7DD1B won't fit in 16 bits. Let me check the label format.
# 
# Actually in the mlb, P:5E39 = _ir_emit_direct_branch_placeholder
# $5E39 as PRG offset would be at file offset 16 + $5E39 = $5E49
# But our function is at CPU $DE39 which is fixed bank offset $1E39
# PRG file offset of fixed bank start = total - 16384 = 524304 - 16 - 16384 = 507904 = $7C000
# So $DE39 = $7C000 + $1E39 = $7DE39 in PRG
# But the label says $5E39. Hmm, $7DE39 != $5E39.
# 
# I think the label file P: values are within the bank's local range.
# Or maybe they map differently. Let me just look at the trace directly.

# The KEY question: does the trace show calls from BANK 2 code?
# Bank 2 code is at CPU $8000-$BFFF when bank 2 is mapped.
# In the trace, the callers were at $DA9F, $CE1D, $CFEC -- all in $C000-$FFFF.
# These are ALL in the fixed bank!
# 
# So the bank 2 callers (JSR $DE39) are never seen in the trace for this block.
# Instead, the fixed bank callers (JSR $DCF8) handle it.
# 
# But why would fixed-bank code call ir_emit_direct_branch_placeholder?
# Unless... the compiler inlined recompile_opcode_b2_inner into the fixed bank?
# Or the function is called from somewhere else too?

# Let me check what label is at $DA9F (in the trace).
# $DA9F - $C000 = $1A9F = offset in fixed bank
# PRG address for label = ?
# Let me search labels around that area

with open("exidy.mlb", "r") as f:
    labels = {}
    for line in f:
        line = line.strip()
        if line.startswith("P:"):
            parts = line.split(":")
            if len(parts) >= 3:
                try:
                    addr = int(parts[1], 16)
                    name = parts[2]
                    labels[addr] = name
                except:
                    pass

# Find labels near the caller addresses
# Need to figure out the mapping. Let me just look for recognizable names.
print("\n=== Labels in range $5A00-$5F00 ===")
for addr in sorted(labels.keys()):
    if 0x5A00 <= addr <= 0x5F00:
        print(f"  P:{addr:04X}:{labels[addr]}")

# Let's also check: what does the compile use for the call?
# The callers in bank 2 use JSR $DE39.
# The callers in fixed bank use JSR $DCF8.
# $DCF8 is _setup_flash_pc_tables in the NEW label map.
# But the OLD binary had ir_emit_direct_branch_placeholder's trampoline there.
# 
# Conclusion: THE USER DIDN'T RELOAD THE ROM IN MESEN.
# The trace was recorded with the OLD ROM.

# Let's verify: check if the ROM bytes at $DCF8 in the new build match 
# _setup_flash_pc_tables or the old trampoline
print("\n=== Verifying: what is at $DCF8 in new ROM? ===")
# Disassemble manually
import struct
pos = 0xCF8
print(f"Bytes at $DCF8: {fixed[pos:pos+20].hex(' ')}")
# If it's setup_flash_pc_tables, first byte should be part of that function
# If it's the old trampoline, it would start with register saves (PHA etc.)

# The old trampoline started with: LDA r16; PHA; LDA r18; PHA; LDA BANK_PC; PHA...
# Which is: A5 10 48 A5 12 48 A5 13 48...
print(f"Expected old trampoline: A5 10 48 A5 12 48 A5 13 48...")
print(f"Actual first 10 bytes:   {fixed[pos:pos+10].hex(' ')}")

# Check what the new build actually has at the CPU addresses the trace calls
# The trace caller at $DA9F does JSR $DCF8
# In the new build, $DCF8 would be... let's decode
print(f"\n=== Disassembling $DCF8 in new ROM ===")
i = 0xCF8
for _ in range(20):
    if i >= len(fixed):
        break
    b = fixed[i]
    cpu = 0xC000 + i
    if b == 0xA5:  # LDA zp
        print(f"  ${cpu:04X}: LDA ${fixed[i+1]:02X}")
        i += 2
    elif b == 0x48:  # PHA
        print(f"  ${cpu:04X}: PHA")
        i += 1
    elif b == 0x85:  # STA zp
        print(f"  ${cpu:04X}: STA ${fixed[i+1]:02X}")
        i += 2
    elif b == 0xAD:  # LDA abs
        addr16 = fixed[i+1] | (fixed[i+2] << 8)
        print(f"  ${cpu:04X}: LDA ${addr16:04X}")
        i += 3
    elif b == 0x8D:  # STA abs
        addr16 = fixed[i+1] | (fixed[i+2] << 8)
        print(f"  ${cpu:04X}: STA ${addr16:04X}")
        i += 3
    elif b == 0xA9:  # LDA #imm
        print(f"  ${cpu:04X}: LDA #${fixed[i+1]:02X}")
        i += 2
    elif b == 0x20:  # JSR
        addr16 = fixed[i+1] | (fixed[i+2] << 8)
        print(f"  ${cpu:04X}: JSR ${addr16:04X}")
        i += 3
    elif b == 0x68:  # PLA
        print(f"  ${cpu:04X}: PLA")
        i += 1
    elif b == 0x60:  # RTS
        print(f"  ${cpu:04X}: RTS")
        i += 1
        break
    else:
        print(f"  ${cpu:04X}: ${b:02X} (?)")
        i += 1
