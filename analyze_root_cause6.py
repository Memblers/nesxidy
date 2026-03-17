#!/usr/bin/env python3
"""Root cause v6: What PC triggered the compile that produced entry_pc=$6A76?"""
import re, os

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\exidy.txt"
TAIL_BYTES = 8 * 1024 * 1024

def read_tail(path, nbytes):
    sz = os.path.getsize(path)
    start = max(0, sz - nbytes)
    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        f.seek(start)
        if start > 0: f.readline()
        return f.readlines()

lines = read_tail(TRACE, TAIL_BYTES)
print(f"Read {len(lines)} lines from tail")

# Last pre-compile dispatch call is at line 36486
# Show 30 lines before and after to see _pc value and return
print("=== Context around last pre-compile dispatch (line 36486) ===")
for i in range(max(0,36476), min(len(lines), 36530)):
    line = lines[i].rstrip()
    tag = ""
    if '_pc' in line and ('STA' in line or 'LDA' in line):
        tag = " <<<"
    if 'dispatch' in line.lower():
        tag = " <<< DISPATCH"
    if 'RTS' in line:
        tag = " <<< RTS"
    if '$5B' in line or '$5C' in line:
        tag += " [_pc ZP]"
    print(f"  {i:06d}: {line}{tag}")

# Now look at what _pc was when dispatch was called
# _pc is at $5B (lo) and $5C (hi)  
# dispatch_on_pc reads _pc+1 ($5C) early - find it
print("\n=== _pc+1 value at each dispatch entry ===")
for i in range(len(lines)):
    line = lines[i]
    # Find where dispatch reads _pc+1 ($5C)
    if '6267' in line[:6] and 'A5 5C' in line:
        val_m = re.search(r'= \$([0-9A-Fa-f]{2})', line)
        val = val_m.group(1) if val_m else "??"
        print(f"  {i:06d}: _pc+1 = ${val}  {line.rstrip()[:80]}")

# Show the transition from dispatch return to compile
# After line 36486, find the switch/case handling and compile entry
print("\n=== Post-dispatch compile entry ===")
for i in range(36486, min(36700, len(lines))):
    line = lines[i].rstrip()
    addr_m = re.match(r'([0-9A-Fa-f]{4})', line.strip())
    addr = int(addr_m.group(1), 16) if addr_m else 0
    # Print ALL lines to see the exact flow
    print(f"  {i:06d} [${addr:04X}]: {line[:110]}")

# Now look at the compile loop to see if _pc overruns ROM
# Find where _pc transitions from $3Fxx to $40xx or higher
print("\n=== _pc transitions past ROM boundary ($3FFF -> $4000+) ===")
for i in range(36500, min(73000, len(lines))):
    line = lines[i]
    # Look for STA $5C where A goes from $3F to $40+
    if '$5C' in line and 'STA' in line:
        a_m = re.search(r'A:([0-9A-Fa-f]{2})', line)
        if a_m:
            a_val = int(a_m.group(1), 16)
            if a_val >= 0x40:
                print(f"  {i:06d}: STA $5C, A=${a_val:02X} - _pc hi byte past ROM!")
                # Show context
                for j in range(max(0,i-3), min(len(lines), i+3)):
                    print(f"    {j:06d}: {lines[j].rstrip()[:110]}")
                break
    # Also check for STA _pc+1 label
    if '_pc+1' in line and 'STA' in line:
        a_m = re.search(r'A:([0-9A-Fa-f]{2})', line)
        if a_m:
            a_val = int(a_m.group(1), 16)
            if a_val >= 0x40:
                print(f"  {i:06d}: STA _pc+1, A=${a_val:02X} - _pc hi byte past ROM!")
                for j in range(max(0,i-3), min(len(lines), i+3)):
                    print(f"    {j:06d}: {lines[j].rstrip()[:110]}")
                break
