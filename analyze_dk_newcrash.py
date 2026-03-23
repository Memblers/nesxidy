"""Analyze the new DK trace crash - read the tail to find what happened."""
import os

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"

# Get file size
fsize = os.path.getsize(TRACE)
print(f"Trace file size: {fsize:,} bytes ({fsize/1e9:.2f} GB)")

# Read last 5MB to find crash
tail_bytes = 5_000_000
with open(TRACE, 'rb') as f:
    f.seek(max(0, fsize - tail_bytes))
    raw = f.read()

# Find line boundary
idx = raw.find(b'\n')
if idx >= 0:
    raw = raw[idx+1:]

lines = raw.decode('utf-8', errors='replace').split('\n')
print(f"Read {len(lines)} lines from last {tail_bytes/1e6:.0f}MB")

# Show last 80 lines
print(f"\n=== LAST 80 LINES ===")
for i, line in enumerate(lines[-80:]):
    print(f"  {len(lines)-80+i:>8}: {line.rstrip()[:160]}")

# Look for unusual patterns in last 2000 lines
print(f"\n=== KEY PATTERNS IN LAST 2000 LINES ===")
xbank_hits = []
jmp_ffff = []
bad_ops = []
dispatch_hits = []
for i, line in enumerate(lines[-2000:]):
    ln = len(lines) - 2000 + i
    text = line.strip()
    if 'xbank' in text.lower() or '$FFFF' in text:
        jmp_ffff.append((ln, text[:160]))
    if 'JMP $FFFF' in text:
        xbank_hits.append((ln, text[:160]))
    # Look for dispatch_on_pc ($623A) or cross_bank_dispatch ($621B)
    parts = text.split()
    if parts:
        try:
            addr = int(parts[0][:4], 16)
            if addr == 0x623A:
                dispatch_hits.append((ln, text[:160]))
            if addr == 0x6231:  # xbank_trampoline
                xbank_hits.append((ln, text[:160]))
        except:
            pass

print(f"JMP $FFFF occurrences: {len(jmp_ffff)}")
for ln, t in jmp_ffff[-20:]:
    print(f"  L{ln}: {t}")

print(f"\nxbank_trampoline ($6231) hits: {len(xbank_hits)}")
for ln, t in xbank_hits[-20:]:
    print(f"  L{ln}: {t}")

print(f"\ndispatch_on_pc ($623A) hits: {len(dispatch_hits)}")
for ln, t in dispatch_hits[-10:]:
    print(f"  L{ln}: {t}")

# Also check: what address ranges are being executed in the last 500 lines?
print(f"\n=== ADDRESS DISTRIBUTION (LAST 500 LINES) ===")
ranges = {'$0000-$5FFF': 0, '$6000-$7FFF': 0, '$8000-$BFFF': 0, '$C000-$FFFF': 0}
for line in lines[-500:]:
    parts = line.strip().split()
    if not parts: continue
    try:
        addr = int(parts[0][:4], 16)
        if addr < 0x6000: ranges['$0000-$5FFF'] += 1
        elif addr < 0x8000: ranges['$6000-$7FFF'] += 1
        elif addr < 0xC000: ranges['$8000-$BFFF'] += 1
        else: ranges['$C000-$FFFF'] += 1
    except:
        pass
for r, c in ranges.items():
    print(f"  {r}: {c}")
