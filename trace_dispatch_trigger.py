#!/usr/bin/env python3
"""Trace the dispatch_on_pc call that triggers the first $D364 compile.
Focus on lines 245000-251500 to find the last dispatch before the flash write."""

import re

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_excitebike.txt"

START = 148000
END = 252000

print(f"Scanning lines {START} to {END}...")

# Track all dispatch calls and their outcomes
dispatch_calls = []  # (line, text) for JSR _dispatch_on_pc
wram_62xx = []       # (line, pc, text) for any $6200+ execution
returns_from_dispatch = []  # After RTS from dispatch routine

# Track the last compile trigger
compile_start = 251480  # first flash write

with open(TRACE, 'r') as f:
    for i, line in enumerate(f, 1):
        if i < START:
            continue
        if i > END:
            break
        
        text = line.rstrip()
        
        # JSR _dispatch_on_pc
        if 'dispatch_on_pc' in text and 'JSR' in text:
            dispatch_calls.append((i, text[:160]))
        
        # Any code in dispatch routine area ($623B-$62FF)
        m = re.match(r'\s*([0-9A-Fa-f]{4})\s', text)
        if m:
            pc = int(m.group(1), 16)
            if 0x623B <= pc <= 0x62FF:
                wram_62xx.append((i, pc, text[:160]))

print(f"\n=== JSR _dispatch_on_pc calls ({len(dispatch_calls)} total) ===")
# Show the last 20 before the compile
last_before = [(l,t) for l,t in dispatch_calls if l < compile_start]
print(f"Last {min(20,len(last_before))} before compile at L{compile_start}:")
for ln, text in last_before[-20:]:
    print(f"  L{ln}: {text}")

# Show first 5 after
first_after = [(l,t) for l,t in dispatch_calls if l >= compile_start]
if first_after:
    print(f"\nFirst 5 after compile:")
    for ln, text in first_after[:5]:
        print(f"  L{ln}: {text}")

# Now for the LAST dispatch before compile, dump the full dispatch execution
if last_before:
    last_dispatch_line = last_before[-1][0]
    print(f"\n=== Full dispatch execution from L{last_dispatch_line} ===")
    
    # Dump ALL $62xx execution lines between the last dispatch and the compile
    dispatch_exec = [(l,p,t) for l,p,t in wram_62xx if l >= last_dispatch_line and l < compile_start]
    print(f"({len(dispatch_exec)} lines in dispatch routine)")
    for ln, pc, text in dispatch_exec[:60]:
        print(f"  L{ln}: ${pc:04X} {text}")
    if len(dispatch_exec) > 60:
        print(f"  ... {len(dispatch_exec)-60} more lines ...")
        for ln, pc, text in dispatch_exec[-10:]:
            print(f"  L{ln}: ${pc:04X} {text}")
    
    # Also dump the 50 lines right after the dispatch JSR to see the flag read
    print(f"\n=== 80 trace lines from L{last_dispatch_line} ===")
    with open(TRACE, 'r') as f:
        for j, line in enumerate(f, 1):
            if j < last_dispatch_line:
                continue
            if j > last_dispatch_line + 80:
                break
            text = line.rstrip()
            prefix = "  "
            m2 = re.match(r'\s*([0-9A-Fa-f]{4})\s', text)
            if m2:
                pc2 = int(m2.group(1), 16)
                if 0x6200 <= pc2 <= 0x62FF:
                    prefix = ">>"
                elif 0x6000 <= pc2 <= 0x61FF:
                    prefix = "FL"
            if 'dispatch' in text.lower():
                prefix = "DI"
            if 'STA $C000' in text:
                prefix = "BK"
            if 'RTS' in text:
                prefix = "RT"
            print(f"  {prefix} L{j}: {text[:160]}")
