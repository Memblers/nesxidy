#!/usr/bin/env python3
"""Find the dispatch_on_pc call that triggered the first $D364 compile.
Search backwards from the first flash write to $92F8 (line ~251480) to find
the JSR dispatch_on_pc at WRAM $623B."""

import re

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_excitebike.txt"

# The first flash write to $92F8 is around line 251480
# dispatch_on_pc is at WRAM $623B (CPU $623B)
# The compile path takes thousands of instructions, so search 10K lines back

START = 241000
END = 251500

print(f"Reading lines {START} to {END}...")

dispatch_entries = []
compile_related = []

with open(TRACE, 'r') as f:
    for i, line in enumerate(f, 1):
        if i < START:
            continue
        if i > END:
            break
        
        # Look for dispatch_on_pc entry (JSR to $623B or code at $623B)
        if '623B' in line or '623b' in line:
            dispatch_entries.append((i, line.rstrip()))
        
        # Look for _pc variable access patterns
        # _pc is a ZP variable. Look for loads that set it to $D3/$64
        if '= $D3' in line and ('LDA' in line or 'STA' in line):
            if '_pc' in line.lower() or 'r0' in line or 'r1' in line:
                compile_related.append((i, line.rstrip()))
        
        # Look for the compile entry - dynamos.c compile function entry
        # The compile function is in bank 2. Look for bankswitch to bank 2
        # Also look for JSR/JMP to compile-related addresses
        if '$92F8' in line:
            compile_related.append((i, line.rstrip()))

print(f"\n=== dispatch_on_pc entries ($623B) ===")
for ln, text in dispatch_entries:
    print(f"  L{ln}: {text[:120]}")

print(f"\n=== Compile-related ($D3 loads, $92F8 writes) ===")
for ln, text in compile_related:
    print(f"  L{ln}: {text[:120]}")

# Now let's do a focused dump: find the LAST dispatch_on_pc call before line 251480
if dispatch_entries:
    last_dispatch = dispatch_entries[-1]
    print(f"\n=== Last dispatch before compile at line {last_dispatch[0]} ===")
    
    # Dump 100 lines starting from the last dispatch
    dump_start = last_dispatch[0]
    dump_end = min(dump_start + 300, END)
    
    print(f"Dumping lines {dump_start} to {dump_end}...")
    with open(TRACE, 'r') as f:
        for i, line in enumerate(f, 1):
            if i < dump_start:
                continue
            if i > dump_end:
                break
            text = line.rstrip()
            # Highlight interesting lines
            prefix = "  "
            if '623B' in text or '623b' in text:
                prefix = ">>"
            elif 'STA $C000' in text:
                prefix = "$$"
            elif 'RTS' in text and ('62' in text[:10] or '63' in text[:10]):
                prefix = "RT"
            elif '_pc' in text.lower():
                prefix = "PC"
            elif 'dispatch' in text.lower():
                prefix = "DI"
            print(f"  {prefix} L{i}: {text[:140]}")
