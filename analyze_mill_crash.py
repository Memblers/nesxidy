"""Analyze end of Millipede trace — seek from end of huge file."""
import os, re, sys

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\millipede.txt"
TAIL_BYTES = 500_000  # read last 500KB

size = os.path.getsize(TRACE)
print(f"File size: {size:,} bytes ({size/1024**3:.2f} GB)")

with open(TRACE, 'rb') as f:
    f.seek(max(0, size - TAIL_BYTES))
    raw = f.read()

text = raw.decode('utf-8', errors='replace')
lines = text.split('\n')
# Drop first partial line
if lines:
    lines = lines[1:]
# Drop trailing empty
while lines and not lines[-1].strip():
    lines.pop()

print(f"Read {len(lines)} lines from tail")
print()

# === LAST 80 LINES ===
print("=== LAST 80 LINES ===")
for line in lines[-80:]:
    print(f"  {line.rstrip()[:160]}")
print()

# === Find BRK ===
print("=== BRK OCCURRENCES (last 2000 lines) ===")
brk_lines = []
for i, line in enumerate(lines[-2000:], len(lines)-2000):
    if re.match(r'\s*[0-9A-F]{4}\s+BRK\b', line.strip()):
        brk_lines.append((i, line.strip()[:160]))
print(f"Found {len(brk_lines)} BRK instructions")
for idx, (i, txt) in enumerate(brk_lines[-10:]):
    print(f"  [{i}] {txt}")
print()

# === Context around the LAST BRK — 30 lines before ===
if brk_lines:
    last_brk_idx = brk_lines[-1][0]
    print(f"=== 40 LINES BEFORE LAST BRK (at index {last_brk_idx}) ===")
    start = max(0, last_brk_idx - 40)
    for line in lines[start:last_brk_idx+3]:
        print(f"  {line.rstrip()[:160]}")
    print()

# === Frame number at end ===
print("=== FRAME INFO ===")
last_frame = None
for line in lines[-200:]:
    m = re.search(r'Fr:(\d+)', line)
    if m:
        last_frame = int(m.group(1))
if last_frame is not None:
    print(f"Last frame seen: {last_frame}")

# === Address distribution in last 5000 lines ===
print()
print("=== ADDRESS REGIONS (last 5000 lines) ===")
regions = {"$0000-$7FFF (WRAM/RAM)": 0, "$8000-$BFFF (flash)": 0,
           "$C000-$FFFF (fixed)": 0}
for line in lines[-5000:]:
    m = re.match(r'\s*([0-9A-F]{4})\s', line.strip())
    if m:
        addr = int(m.group(1), 16)
        if addr < 0x8000:
            regions["$0000-$7FFF (WRAM/RAM)"] += 1
        elif addr < 0xC000:
            regions["$8000-$BFFF (flash)"] += 1
        else:
            regions["$C000-$FFFF (fixed)"] += 1
for k, v in regions.items():
    print(f"  {k}: {v}")

# === Unique addresses around the BRK ===
if brk_lines:
    last_brk_idx = brk_lines[-1][0]
    print()
    print("=== INSTRUCTION FLOW LEADING TO BRK ===")
    # Show addresses + mnemonics for 60 lines before BRK
    for line in lines[max(0,last_brk_idx-60):last_brk_idx+3]:
        m = re.match(r'\s*([0-9A-F]{4})\s+(\S+)', line.strip())
        if m:
            addr_s, mnem = m.group(1), m.group(2)
            # Also extract A/X/Y/S/P
            regs = re.search(r'A:([0-9A-F]{2}) X:([0-9A-F]{2}) Y:([0-9A-F]{2}) S:([0-9A-F]{2}) P:(\S+)', line)
            reg_str = ""
            if regs:
                reg_str = f"  A:{regs.group(1)} X:{regs.group(2)} Y:{regs.group(3)} S:{regs.group(4)} P:{regs.group(5)}"
            # Get full instruction
            full = re.match(r'\s*[0-9A-F]{4}\s+(.+?)(?:\s{2,})', line.strip())
            instr = full.group(1) if full else mnem
            print(f"  {addr_s}  {instr:<40s}{reg_str}")
