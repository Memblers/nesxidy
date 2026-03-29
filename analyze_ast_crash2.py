"""Analyze the crash at the end of asteroids.txt host trace (v2)."""
import re, os

f = r'C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\asteroids.txt'
size = os.path.getsize(f)
print(f'Size: {size:,} bytes ({size/1024/1024:.1f} MB)')

# Read last 500KB
with open(f, 'rb') as fh:
    fh.seek(max(0, size - 500000))
    tail = fh.read().decode('utf-8', errors='replace')

lines = tail.split('\n')
print(f'Lines in tail chunk: {len(lines)}')

# Parse each line
parsed = []
for i, line in enumerate(lines):
    m = re.match(
        r'\s*([0-9A-Fa-f]{4})\s+(.+?)\s{2,}'
        r'A:([0-9A-Fa-f]{2})\s+X:([0-9A-Fa-f]{2})\s+'
        r'Y:([0-9A-Fa-f]{2})\s+S:([0-9A-Fa-f]{2})\s+'
        r'P:(\S+)\s+.*Fr:(\d+)', line)
    if m:
        parsed.append({
            'idx': i,
            'addr': int(m.group(1), 16),
            'asm': m.group(2).strip(),
            'a': int(m.group(3), 16),
            'x': int(m.group(4), 16),
            'y': int(m.group(5), 16),
            'sp': int(m.group(6), 16),
            'flags': m.group(7),
            'frame': int(m.group(8))
        })

print(f'Parsed entries: {len(parsed)}')

# Find crash - execution below $0100 (zero page / stack page)
crash_idx = None
for i, p in enumerate(parsed):
    if p['addr'] < 0x0100:
        crash_idx = i
        break

if crash_idx is None:
    # Try: last entry with unusual address pattern
    last = parsed[-1] if parsed else None
    print(f'No zero-page execution found.')
    print(f'Last 20 entries:')
    for p in parsed[-20:]:
        print(f'  ${p["addr"]:04X}  {p["asm"]:<48s} A:{p["a"]:02X} X:{p["x"]:02X} Y:{p["y"]:02X} S:{p["sp"]:02X} Fr:{p["frame"]}')
    exit()

print(f'\n=== CRASH at parsed entry {crash_idx}, addr ${parsed[crash_idx]["addr"]:04X} ===')

# Show 80 entries before crash
start = max(0, crash_idx - 80)
print(f'\n--- Last {crash_idx - start} entries before crash + crash ---')
for p in parsed[start:min(crash_idx + 5, len(parsed))]:
    marker = ' <<<' if p['addr'] < 0x0100 else ''
    print(f'  ${p["addr"]:04X}  {p["asm"]:<48s} A:{p["a"]:02X} X:{p["x"]:02X} Y:{p["y"]:02X} S:{p["sp"]:02X} P:{p["flags"]:12s} Fr:{p["frame"]}{marker}')

# Stack ops in last 200 entries
print(f'\n=== Stack ops in last 200 entries before crash ===')
jsr_start = max(0, crash_idx - 200)
for p in parsed[jsr_start:crash_idx + 3]:
    asm_up = p['asm'].upper()
    if any(op in asm_up for op in ['JSR', 'RTS', 'RTI', 'PHA', 'PLA', 'PHP', 'PLP']):
        marker = ' <<<' if p['addr'] < 0x0100 else ''
        print(f'  ${p["addr"]:04X}  {p["asm"]:<48s} S:{p["sp"]:02X} Fr:{p["frame"]}{marker}')

# SP transitions in last 100 entries
print(f'\n=== SP transitions in last 100 entries ===')
sp_start = max(0, crash_idx - 100)
prev_sp = None
for p in parsed[sp_start:min(crash_idx + 5, len(parsed))]:
    if p['sp'] != prev_sp:
        change = f' (from {prev_sp:02X}, delta={p["sp"] - prev_sp:+d})' if prev_sp is not None else ''
        print(f'  ${p["addr"]:04X}  {p["asm"]:<48s} S:{p["sp"]:02X}{change}')
        prev_sp = p['sp']

# Find the RTS that caused the crash
for i in range(crash_idx - 1, max(0, crash_idx - 5), -1):
    if 'RTS' in parsed[i]['asm'].upper():
        sp = parsed[i]['sp']
        print(f'\nCrash RTS at ${parsed[i]["addr"]:04X} with SP={sp:02X}')
        print(f'Return address pulled from $01{sp+1:02X}/$01{sp+2:02X}')
        print(f'Next PC = ${parsed[crash_idx]["addr"]:04X}')
        break

# Block entries ($8000) near crash
print(f'\n=== $8000 entries near crash ===')
for p in parsed[max(0, crash_idx - 300):crash_idx + 3]:
    if p['addr'] == 0x8000:
        print(f'  entry: ${p["addr"]:04X}  {p["asm"]:<48s} S:{p["sp"]:02X} Fr:{p["frame"]}')

# Find the bankswitch (STA $C000) nearest to crash
print(f'\n=== Last bankswitches (STA $C000) before crash ===')
count = 0
for p in reversed(parsed[:crash_idx]):
    if '$C000' in p['asm']:
        print(f'  ${p["addr"]:04X}  {p["asm"]:<48s} A:{p["a"]:02X} S:{p["sp"]:02X} Fr:{p["frame"]}')
        count += 1
        if count >= 5:
            break

# What flash bank was active? Look for mapper register write
print(f'\n=== Flash bank at crash ===')
for p in reversed(parsed[:crash_idx]):
    if '_mapper_prg_bank' in p['asm'] and 'STA' in p['asm']:
        print(f'  Last STA _mapper_prg_bank: A={p["a"]:02X} at ${p["addr"]:04X}')
        break
    if '_mapper_register' in p['asm'] and 'STA' in p['asm']:
        print(f'  Last STA _mapper_register: A={p["a"]:02X} at ${p["addr"]:04X}')
        break

# Check: is the crash address from the branch resolution bug?
# Look for branches with large forward offsets in the crashing block
print(f'\n=== Forward branches in last 100 entries ===')
for p in parsed[max(0, crash_idx - 100):crash_idx]:
    asm = p['asm']
    # Match BCC/BCS/BEQ/BNE/BPL/BMI/BVC/BVS with target > current addr + 50
    bm = re.match(r'(B\w+)\s+\$([0-9A-Fa-f]{4})', asm)
    if bm:
        tgt = int(bm.group(2), 16)
        if tgt > p['addr'] + 20:  # significant forward jump
            print(f'  ${p["addr"]:04X}  {asm:<48s} (offset +{tgt - p["addr"]})')
