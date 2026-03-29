#!/usr/bin/env python3
"""Find which guest PCs are being interpreted in DK trace.
dispatch_on_pc is at $623B. When it returns 2 (interpret), the code
goes to interpret_6502. Track _pc values at interpret time."""
import re
from collections import Counter, defaultdict

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"

# dispatch_on_pc at $623B reads flag at $625D: LDA (addr_lo),Y
# When flag has bit7 SET and INTERPRETED bit ($40) clear → returns 2
# When flag = $FF or flag has INTERPRETED bit → returns 1 (compile)
# When flag = $00 → returns 1 (compile)
# When flag has bit7 CLEAR → executes from flash (returns 0)

# The interpret path eventually calls interpret_6502 after setting bank 0.
# Let's track: at interpret_6502 call time, what is _pc ($51:$52)?
# Or better: track the flag reads at $625D and their outcomes.

# Actually, the simplest: find the not_recompiled path.
# After dispatch reads flag at $625D:
#   BEQ not_recompiled ($00 → compile)
#   BMI not_recompiled (bit7 set → check INTERPRETED)
# At not_recompiled:
#   BEQ .needs_compile ($00)
#   AND #INTERPRETED ($40) → BNE .needs_compile (INTERPRETED bit set → compile)
#   LDA #2 / RTS (interpret)

# Track: which PCs get dispatched and what flag values they have
# Focus on the interpret path (returns 2)

# Strategy: Find all dispatch_on_pc calls, read the _pc value,
# then track which path they take.

# _pc is at ZP $51 (lo) and $52 (hi)
# dispatch reads $52 first at $623F: LDA $52

dispatch_pcs = []  # (line, frame, pc_lo, pc_hi, flag_val, outcome)
interpret_pcs = Counter()
compile_pcs = Counter()
flash_pcs = Counter()

# Track state through dispatch
in_dispatch = False
current_dispatch_line = 0
current_frame = 0
current_pc_hi = 0
current_pc_lo = 0
current_flag = 0

print("Scanning trace for dispatch outcomes...")
with open(TRACE, 'r') as f:
    for i, line in enumerate(f):
        text = line.rstrip()
        
        m = re.match(r'\s*([0-9A-Fa-f]{4})\s', text)
        if not m:
            continue
        pc = int(m.group(1), 16)
        
        fm = re.search(r'Fr:(\d+)', text)
        frame = int(fm.group(1)) if fm else 0
        
        # dispatch_on_pc entry
        if pc == 0x623B:
            in_dispatch = True
            current_dispatch_line = i
            current_frame = frame
        
        # Read _pc+1 ($52) at $623F
        if in_dispatch and pc == 0x623F:
            vm = re.search(r'\$52\s*=\s*\$([0-9A-Fa-f]{2})', text)
            if vm:
                current_pc_hi = int(vm.group(1), 16)
        
        # Read _pc ($51) at $6257
        if in_dispatch and pc == 0x6257:
            vm = re.search(r'_pc\s*=\s*\$([0-9A-Fa-f]{2})', text)
            if vm:
                current_pc_lo = int(vm.group(1), 16)
        
        # Flag read at $625D
        if in_dispatch and pc == 0x625D:
            vm = re.search(r'=\s*\$([0-9A-Fa-f]{2})', text)
            if vm:
                current_flag = int(vm.group(1), 16)
        
        # Outcomes:
        # Flash hit: reaches $62AE (JSR $62B2) or $62B2 (JMP to native)
        if in_dispatch and pc == 0x62B2:
            guest_pc = current_pc_lo | (current_pc_hi << 8)
            flash_pcs[guest_pc] += 1
            in_dispatch = False
        
        # not_recompiled path: reaches $630D (from JMP not_recompiled)
        # or the LDA #2/RTS at the interpret return, or LDA #1/RTS at compile return
        
        # Interpret return: LDA #$02 at not_recompiled area
        # The exact address depends on assembly. Let's track the RTS from dispatch
        # by watching for the return: after $62B1 RTS or $62E5 (JSR interpret)
        
        if in_dispatch and pc == 0x62E5:  # JSR _interpret_6502
            guest_pc = current_pc_lo | (current_pc_hi << 8)
            interpret_pcs[guest_pc] += 1
            in_dispatch = False
        
        # Compile return (should not happen in this trace)
        # The .needs_compile path does LDA #1 / RTS
        # But we'd see this reflected in the C code, not easy to track in asm
        
        if i % 2000000 == 0 and i > 0:
            print(f"  ...{i//1000000}M lines, {sum(interpret_pcs.values())} interprets, {sum(flash_pcs.values())} flash hits")

total_interpret = sum(interpret_pcs.values())
total_flash = sum(flash_pcs.values())
total_compile = sum(compile_pcs.values())

print(f"\n=== Dispatch outcomes ===")
print(f"Flash hits (case 0): {total_flash}")
print(f"Interprets (case 2): {total_interpret}")
print(f"Compile needed (case 1): {total_compile}")
print(f"Unique interpreted PCs: {len(interpret_pcs)}")
print(f"Unique flash-hit PCs: {len(flash_pcs)}")

print(f"\n=== Top 30 interpreted PCs ===")
for guest_pc, count in interpret_pcs.most_common(30):
    print(f"  ${guest_pc:04X}: {count}x interpreted")

print(f"\n=== All interpreted PCs (sorted by address) ===")
for guest_pc in sorted(interpret_pcs.keys()):
    count = interpret_pcs[guest_pc]
    print(f"  ${guest_pc:04X}: {count}x")

# Check: are any of these PCs also getting flash hits?
both = set(interpret_pcs.keys()) & set(flash_pcs.keys())
if both:
    print(f"\n=== PCs with BOTH flash hits and interprets ({len(both)}) ===")
    for guest_pc in sorted(both):
        print(f"  ${guest_pc:04X}: {flash_pcs[guest_pc]} hits, {interpret_pcs[guest_pc]} interprets")
