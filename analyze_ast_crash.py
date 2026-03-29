"""Analyze the crash at the end of asteroids.txt host trace."""
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

# Find crash - execution from zero page
crash_idx = None
for i, p in enumerate(parsed):
    if p['addr'] < 0x0010:
        crash_idx = i
        break

if crash_idx is None:
    print('No crash found (no execution from $0000-$000F)')
    exit()

print(f'\n=== CRASH at parsed entry {crash_idx}, addr ${parsed[crash_idx]["addr"]:04X} ===')

# Show 80 entries before crash
start = max(0, crash_idx - 80)
print(f'\n--- Last {crash_idx - start} entries before crash + crash ---')
for p in parsed[start:min(crash_idx + 5, len(parsed))]:
    marker = ' <<<' if p['addr'] < 0x0010 else ''
    print(f'  ${p["addr"]:04X}  {p["asm"]:<48s} A:{p["a"]:02X} X:{p["x"]:02X} Y:{p["y"]:02X} S:{p["sp"]:02X} P:{p["flags"]:12s} Fr:{p["frame"]}{marker}')

# JSR/RTS/PHA/PLA in last 200 entries
print(f'\n=== Stack ops in last 200 entries before crash ===')
jsr_start = max(0, crash_idx - 200)
for p in parsed[jsr_start:crash_idx + 3]:
    asm_up = p['asm'].upper()
    if any(op in asm_up for op in ['JSR', 'RTS', 'RTI', 'PHA', 'PLA', 'PHP', 'PLP']):
        marker = ' <<<' if p['addr'] < 0x0010 else ''
        print(f'  ${p["addr"]:04X}  {p["asm"]:<48s} S:{p["sp"]:02X} Fr:{p["frame"]}{marker}')

# SP changes in last 100 entries
print(f'\n=== SP transitions in last 100 entries ===')
sp_start = max(0, crash_idx - 100)
prev_sp = None
for p in parsed[sp_start:min(crash_idx + 5, len(parsed))]:
    if p['sp'] != prev_sp:
        change = f' (from {prev_sp:02X}, delta={p["sp"] - prev_sp:+d})' if prev_sp is not None else ''
        print(f'  ${p["addr"]:04X}  {p["asm"]:<48s} S:{p["sp"]:02X}{change}')
        prev_sp = p['sp']

# Look for the block entry ($8000) nearest to crash
print(f'\n=== Block entries ($8000) in tail ===')
for p in parsed[max(0, crash_idx - 300):crash_idx + 3]:
    if p['addr'] == 0x8000:
        print(f'  entry {p["idx"]}: ${p["addr"]:04X}  {p["asm"]:<48s} S:{p["sp"]:02X} Fr:{p["frame"]}')

# Find the specific block that crashed - trace from $8000 to crash
block_start = None
for i in range(crash_idx - 1, max(0, crash_idx - 300), -1):
    if parsed[i]['addr'] == 0x8000:
        block_start = i
        break

if block_start:
    print(f'\n=== Full crashing block (entry {block_start} to {crash_idx}) ===')
    print(f'Block length: {crash_idx - block_start} entries')
    # Show unique addresses in the block
    addrs = [parsed[i]['addr'] for i in range(block_start, min(crash_idx + 3, len(parsed)))]
    print(f'Address range: ${min(addrs):04X} - ${max(addrs):04X}')
    print(f'Unique addresses: {len(set(addrs))}')

# Show last 5 blocks' entry SP values
print(f'\n=== Last 5 block entries and their SP ===')
block_entries = [p for p in parsed[:crash_idx] if p['addr'] == 0x8000]
for p in block_entries[-5:]:
    print(f'  ${p["addr"]:04X}  S:{p["sp"]:02X}  Fr:{p["frame"]}')

# Check: what address does the RTS at $815F return to?
# The SP before RTS is the key - the return addr is at stack[SP+1], stack[SP+2]
for i in range(crash_idx - 1, max(0, crash_idx - 5), -1):
    if 'RTS' in parsed[i]['asm']:
        sp = parsed[i]['sp']
        print(f'\nRTS at ${parsed[i]["addr"]:04X} with SP={sp:02X}')
        print(f'Return address pulled from $01{sp+1:02X}/$01{sp+2:02X}')
        print(f'Next PC = ${parsed[crash_idx]["addr"]:04X} (expected from stack)')
        break

# Look backwards from crash for the last bankswitch or dispatch
print(f'\n=== Last 10 accesses to $C000 region (bankswitch/dispatch) ===')
count = 0
for p in reversed(parsed[:crash_idx]):
    if 0xC000 <= p['addr'] <= 0xCFFF:
        print(f'  ${p["addr"]:04X}  {p["asm"]:<48s} S:{p["sp"]:02X} Fr:{p["frame"]}')
        count += 1
        if count >= 10:
            break
