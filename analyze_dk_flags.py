#!/usr/bin/env python3
"""For the top interpreted DK PCs, trace dispatch to see what flag value they have.
Focus on $F4BE (1316x) and $F52A (1280x) as highest-frequency."""
import re

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"

# Track dispatch execution for specific PCs
TARGET_PCS = {0xF4BE, 0xF52A, 0xF52D, 0xF52F, 0xF530, 0xFBF6, 0xF228, 0xFDDF, 0xFE31}

# State machine
in_dispatch = False
current_pc_hi = 0
current_pc_lo = 0
current_flag = 0
current_line = 0

# Results: (guest_pc, flag_val, outcome, line, frame)
results = []

MAX_RESULTS = 50  # first N results per target PC
result_counts = {}

print("Scanning for dispatch flag values of interpreted PCs...")
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
            current_line = i
        
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
        
        # Interpret path: JSR _interpret_6502 at $62E5
        if in_dispatch and pc == 0x62E5:
            guest_pc = current_pc_lo | (current_pc_hi << 8)
            if guest_pc in TARGET_PCS:
                key = guest_pc
                if result_counts.get(key, 0) < 5:
                    results.append((guest_pc, current_flag, 'interpret', current_line, frame))
                    result_counts[key] = result_counts.get(key, 0) + 1
            in_dispatch = False
        
        # Flash hit: JMP to native at $62B2
        if in_dispatch and pc == 0x62B2:
            guest_pc = current_pc_lo | (current_pc_hi << 8)
            if guest_pc in TARGET_PCS:
                key = guest_pc
                if result_counts.get(key, 0) < 3:
                    results.append((guest_pc, current_flag, 'flash', current_line, frame))
                    result_counts[key] = result_counts.get(key, 0) + 1
            in_dispatch = False
        
        # Check if we have enough results
        if all(result_counts.get(pc, 0) >= 3 for pc in TARGET_PCS):
            break

print(f"\n=== Flag values for interpreted PCs ===")
for guest_pc, flag, outcome, line, frame in sorted(results, key=lambda x: (x[0], x[3])):
    flag_desc = ""
    if flag == 0xFF:
        flag_desc = "UNPROGRAMMED"
    elif flag == 0x00:
        flag_desc = "AND-CORRUPTED/UNINIT"
    elif flag & 0x80:
        if flag & 0x40:
            flag_desc = f"INTERPRETED (bit7+bit6)"
        else:
            flag_desc = f"bit7=1, NOT INTERPRETED (bits={flag:08b})"
    else:
        flag_desc = f"RECOMPILED bank {flag & 0x1F}"
    print(f"  ${guest_pc:04X} flag=${flag:02X} ({flag_desc}) -> {outcome} @ L{line} Fr:{frame}")

# Now let's check: what does the dispatch asm do with these flag values?
# From dynamos-asm.s not_recompiled:
#   BEQ .needs_compile ($00)
#   AND #INTERPRETED ($40) -> BNE .needs_compile (bit6 set -> compile)
#   LDA #2 / RTS (interpret)
# So:  flag=$80 -> not_recompiled -> $80 != 0, $80 & $40 = 0 -> LDA #2 -> interpret
# flag=$80 means: bit7 set (not recompiled), bit6 clear (NOT interpret-flagged)
# This is a BUG in the encoding! $80 has RECOMPILED bit set but nothing else.
# What writes $80?
