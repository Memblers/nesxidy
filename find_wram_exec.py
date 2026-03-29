#!/usr/bin/env python3
"""Find any WRAM code execution ($6000-$7FFF) near the first $D364 compile.
Also find the actual compile entry point."""

import re

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_excitebike.txt"

START = 249000
END = 251500

print(f"Reading lines {START} to {END}...")

wram_exec = []
rts_lines = []
jsr_lines = []
bank_switch = []
interesting = []

with open(TRACE, 'r') as f:
    for i, line in enumerate(f, 1):
        if i < START:
            continue
        if i > END:
            break
        
        text = line.rstrip()
        
        # Extract PC address (first hex field after line marker)
        # Format: "  ADDR  INSTRUCTION"
        # The PC is the first hex number on the line
        m = re.match(r'\s*([0-9A-Fa-f]{4})\s', text)
        if m:
            pc = int(m.group(1), 16)
            
            # WRAM execution ($6000-$7FFF)
            if 0x6000 <= pc <= 0x7FFF:
                wram_exec.append((i, pc, text[:140]))
            
            # Look for JSR instructions
            if 'JSR' in text:
                jsr_lines.append((i, pc, text[:140]))
            
            # Look for STA $C000 (bankswitch)
            if 'STA $C000' in text:
                bank_switch.append((i, pc, text[:140]))
                
            # Look for compile entry - the compile code is in bank 2
            # After bankswitch to bank 2, we'd see code at $8000-$BFFF
            
            # Look for $92F8 (first flash write target)
            if '$92F8' in text:
                interesting.append((i, pc, text[:140]))

print(f"\n=== WRAM execution ($6000-$7FFF) [{len(wram_exec)} entries] ===")
for ln, pc, text in wram_exec[:50]:
    print(f"  L{ln}: ${pc:04X} {text}")
if len(wram_exec) > 50:
    print(f"  ... {len(wram_exec)-50} more ...")
    for ln, pc, text in wram_exec[-10:]:
        print(f"  L{ln}: ${pc:04X} {text}")

print(f"\n=== Bank switches (STA $C000) [{len(bank_switch)} entries] ===")
for ln, pc, text in bank_switch[:30]:
    print(f"  L{ln}: ${pc:04X} {text}")

print(f"\n=== JSR instructions [{len(jsr_lines)} entries] ===")
# Show unique JSR targets
jsr_targets = {}
for ln, pc, text in jsr_lines:
    m2 = re.search(r'JSR \$([0-9A-Fa-f]{4})', text)
    if m2:
        target = m2.group(1)
        if target not in jsr_targets:
            jsr_targets[target] = []
        jsr_targets[target].append(ln)

for target, lines in sorted(jsr_targets.items()):
    print(f"  JSR ${target}: {len(lines)} calls, first at L{lines[0]}")

print(f"\n=== $92F8 references [{len(interesting)} entries] ===")
for ln, pc, text in interesting[:10]:
    print(f"  L{ln}: ${pc:04X} {text}")
