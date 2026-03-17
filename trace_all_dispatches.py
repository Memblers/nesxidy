#!/usr/bin/env python3
"""Trace ALL dispatch entries (including JMP re-entries) between L239000 and L252000.
Find the one that triggers compile by looking for flag=$FF or the not_recompiled path."""

import re

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_excitebike.txt"

START = 239000
END = 252000

print(f"Scanning lines {START} to {END}...")

# Track ALL entries to dispatch_on_pc ($623B)
# This includes both JSR and JMP entries
dispatch_entries = []
flag_reads = []   # LDA (addr_lo),Y at $625D
interpret_calls = []
not_recompiled = []

with open(TRACE, 'r') as f:
    for i, line in enumerate(f, 1):
        if i < START:
            continue
        if i > END:
            break
        
        text = line.rstrip()
        
        m = re.match(r'\s*([0-9A-Fa-f]{4})\s', text)
        if not m:
            continue
        pc = int(m.group(1), 16)
        
        # Entry to dispatch_on_pc at $623B (first instruction is LDA #$00)
        if pc == 0x623B:
            dispatch_entries.append((i, text[:160]))
        
        # Flag read at $625D: LDA (addr_lo),Y 
        if pc == 0x625D:
            # Extract the effective address and value
            flag_reads.append((i, text[:160]))
        
        # interpret_6502 call
        if pc == 0x62E5:
            interpret_calls.append((i, text[:160]))
        
        # not_recompiled path indicators
        # BNE at $625F (flag != 0 means has value)
        # BMI at $6264 (flag bit7 set means INTERPRETED)
        if pc == 0x6262:  # This might be the fall-through for flag==0
            not_recompiled.append((i, 'flag_zero', text[:160]))
        if pc == 0x6266 or pc == 0x6267:  # BMI taken = INTERPRETED path
            not_recompiled.append((i, 'interpreted', text[:160]))
        
        # Look for NMI yield / return from dispatch with result
        if pc == 0x62EE:  # LDA #$01 (NMI detected result)
            not_recompiled.append((i, 'nmi_yield', text[:160]))
        if pc == 0x62F3:
            not_recompiled.append((i, 'nmi_same', text[:160]))

print(f"\n=== Dispatch entries ($623B) [{len(dispatch_entries)}] ===")
for ln, text in dispatch_entries:
    # Show what _pc was (check $52 which is _pc+1) 
    print(f"  L{ln}: {text}")

print(f"\n=== Flag reads ($625D) [{len(flag_reads)}] ===")
for ln, text in flag_reads:
    # The effective address and value tell us which PC and what flag
    print(f"  L{ln}: {text}")

print(f"\n=== Interpret calls ($62E5) [{len(interpret_calls)}] ===")
for ln, text in interpret_calls:
    print(f"  L{ln}: {text}")

print(f"\n=== Not-recompiled/NMI indicators [{len(not_recompiled)}] ===")
for ln, typ, text in not_recompiled:
    print(f"  L{ln} [{typ}]: {text}")
