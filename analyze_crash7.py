#!/usr/bin/env python3
"""Analyze the Exidy crash trace - read more context before the crash."""
import os, re

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\exidy.txt"

# Read last 512KB for much more context
tail_bytes = 512 * 1024
fsize = os.path.getsize(TRACE)
print(f"Trace file size: {fsize:,} bytes ({fsize/1024/1024:.1f} MB)")

with open(TRACE, 'r', errors='replace') as f:
    f.seek(max(0, fsize - tail_bytes))
    if fsize > tail_bytes:
        f.readline()  # skip partial line
    lines = f.readlines()

print(f"Read {len(lines)} lines from tail")

# Find JMP PLATFORM_NES (crash) or BRK at 0000
crash_indices = []
for i, line in enumerate(lines):
    if 'JMP PLATFORM_NES' in line or ('0000' in line[:20] and 'BRK' in line):
        crash_indices.append(i)

if not crash_indices:
    # Look for JMP $0000
    for i, line in enumerate(lines):
        if '4C 00 00' in line:
            crash_indices.append(i)

print(f"Found {len(crash_indices)} crash points")

if crash_indices:
    ci = crash_indices[0]
    # Show 200 lines before crash for more context
    start = max(0, ci - 200)
    end = min(len(lines), ci + 10)
    print(f"\n=== Context before crash (lines {start}-{end}) ===")
    for i in range(start, end):
        print(f"{i:06d}: {lines[i].rstrip()}")

# Also find where _pc gets set to $6A76
# Look for stores to ZP $5B and $5C (the _pc bytes)
print("\n=== Writes to _pc ($5B/$5C) in last 500 lines before crash ===")
if crash_indices:
    ci = crash_indices[0]
    search_start = max(0, ci - 500)
    for i in range(search_start, ci):
        line = lines[i]
        # Look for STA $5B or STA $5C or STX $5B etc.
        if re.search(r'ST[AXY]\s+\$5[BC]\b', line) or re.search(r'ST[AXY]\s+_pc\b', line):
            print(f"{i:06d}: {line.rstrip()}")

# Look for any compiled block execution (addresses in $8000-$BFFF range which is where compiled code lives)
print("\n=== Last 20 compiled block entries (JMP/JSR to $8000+) before crash ===")
if crash_indices:
    ci = crash_indices[0]
    search_start = max(0, ci - 2000)
    entries = []
    for i in range(search_start, ci):
        line = lines[i]
        # dispatch_on_pc ends with JSR $62DA; JMP addr
        # Look for the JSR $62DA pattern or transition from $62xx to $8xxx/$9xxx
        if '62DA' in line and 'JSR' in line:
            entries.append(i)
        # Or look for execution at $62D6 (JSR $62DA) 
        if '62D6' in line[:20]:
            entries.append(i)
    for idx in entries[-20:]:
        # Show the JSR and next few lines
        for j in range(idx, min(idx + 5, ci)):
            print(f"{j:06d}: {lines[j].rstrip()}")
        print("---")

# Find dispatch_on_pc entry points (start of dispatch is around $6255 or wherever LDA _pc starts)
print("\n=== dispatch_on_pc calls (flag reads) in last 500 lines ===")
if crash_indices:
    ci = crash_indices[0]
    search_start = max(0, ci - 500)
    for i in range(search_start, ci):
        line = lines[i]
        # The flag read: LDA (addr_lo),Y [$xxxx] = $xx at address $6285
        if '6285' in line[:20] and 'LDA' in line:
            # Extract the flag value and indirect address
            m = re.search(r'\[(\$[0-9A-Fa-f]+)\]\s*=\s*\$([0-9A-Fa-f]+)', line)
            if m:
                addr = m.group(1)
                val = m.group(2)
                print(f"{i:06d}: flag_read addr={addr} val=${val}  {line.rstrip()[:100]}")

# Check what happens BEFORE the crashing dispatch - is there a return from compiled code?
print("\n=== Last 50 lines showing $62xx addresses (dispatch/return area) ===")
if crash_indices:
    ci = crash_indices[0]
    search_start = max(0, ci - 300)
    matches = []
    for i in range(search_start, ci):
        line = lines[i]
        # Check if address starts with 62
        if line.strip() and len(line) > 10:
            parts = line.split()
            if len(parts) >= 2:
                addr = parts[0].rstrip(':')
                if addr.startswith('62') and len(addr) == 4:
                    matches.append(i)
    for idx in matches[-50:]:
        print(f"{idx:06d}: {lines[idx].rstrip()[:120]}")
