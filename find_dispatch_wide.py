#!/usr/bin/env python3
"""Find ALL dispatch_on_pc ($623B) calls in the trace and the first compile trigger."""

import re

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_excitebike.txt"

# Search from start to past the first compile
END = 260000

print(f"Scanning lines 1 to {END} for $623B (dispatch_on_pc)...")

dispatch_lines = []
first_92f8 = None

# Also search for any execution in $6200-$62FF range
wram_62xx = []

with open(TRACE, 'r') as f:
    for i, line in enumerate(f, 1):
        if i > END:
            break
        
        text = line.rstrip()
        
        # Check for 623B as PC
        if text.lstrip().startswith('623B') or ' 623B ' in text[:20]:
            dispatch_lines.append((i, text[:140]))
        
        # Check for any $62xx WRAM execution
        m = re.match(r'\s*([0-9A-Fa-f]{4})\s', text)
        if m:
            pc = int(m.group(1), 16)
            if 0x6200 <= pc <= 0x62FF:
                wram_62xx.append((i, pc, text[:140]))
        
        # Track first $92F8 write
        if first_92f8 is None and '$92F8' in text:
            first_92f8 = (i, text[:140])
        
        # Also check for the string "dispatch" in case labels are in the trace
        if 'dispatch' in text.lower():
            dispatch_lines.append((i, text[:140]))

print(f"\nFirst $92F8 write: L{first_92f8[0] if first_92f8 else 'None'}")
print(f"\ndispatch_on_pc entries ($623B or 'dispatch'): {len(dispatch_lines)}")
for ln, text in dispatch_lines[:30]:
    print(f"  L{ln}: {text}")
if len(dispatch_lines) > 30:
    print(f"  ... {len(dispatch_lines)-30} more")

print(f"\nWRAM $62xx execution: {len(wram_62xx)}")
for ln, pc, text in wram_62xx[:50]:
    print(f"  L{ln}: ${pc:04X} {text}")
if len(wram_62xx) > 50:
    print(f"  ... {len(wram_62xx)-50} more")
    for ln, pc, text in wram_62xx[-10:]:
        print(f"  L{ln}: ${pc:04X} {text}")
