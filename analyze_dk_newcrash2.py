"""Trace the xbank crash: find the block at $9BA0, its epilogue, and how it got patched.
Also find all xbank_trampoline calls in the trace to see if any succeed or all fail."""
import os

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"
fsize = os.path.getsize(TRACE)

# Read last 30MB for more context 
tail_bytes = 30_000_000
with open(TRACE, 'rb') as f:
    f.seek(max(0, fsize - tail_bytes))
    raw = f.read()
idx = raw.find(b'\n')
if idx >= 0:
    raw = raw[idx+1:]
lines = raw.decode('utf-8', errors='replace').split('\n')
print(f"Read {len(lines)} lines from last {tail_bytes/1e6:.0f}MB")

# 1. Find ALL executions of xbank_trampoline ($6231)
xbank_exec = []
# 2. Find ALL executions going to $9BA0 (the unpatched xbank setup)
xbank_setup = []
# 3. Find the dispatch that led to this — JSR _dispatch_on_pc before $9BA0
dispatches = []
# 4. Find flash_byte_program calls near the xbank addresses
# The xbank setup is at $9BA0. The block it belongs to would have
# a header 8 bytes before the code start. Let's find what block this is.
# 5. Find STA at $9BA5 (STA xbank_addr) - what values get written?
xbank_addr_writes = []

for i, line in enumerate(lines):
    text = line.strip()
    if not text:
        continue
    parts = text.split()
    if not parts:
        continue
    try:
        addr = int(parts[0][:4], 16)
    except:
        continue
    
    if addr == 0x6231:
        xbank_exec.append((i, text[:180]))
    
    if addr == 0x9BA0:
        xbank_setup.append((i, text[:180]))
    
    if addr == 0x623A:  # dispatch_on_pc
        dispatches.append((i, text[:180]))
    
    # STA _xbank_addr ($6238) - look at the value being written
    if addr == 0x9BA5 and 'STA' in text:
        xbank_addr_writes.append((i, text[:180]))

print(f"\n=== xbank_trampoline ($6231) executions: {len(xbank_exec)} ===")
for ln, t in xbank_exec:
    print(f"  L{ln}: {t}")

print(f"\n=== xbank setup at $9BA0 executions: {len(xbank_setup)} ===")
for ln, t in xbank_setup:
    print(f"  L{ln}: {t}")

print(f"\n=== STA xbank_addr at $9BA5: {len(xbank_addr_writes)} ===")
for ln, t in xbank_addr_writes:
    print(f"  L{ln}: {t}")

# Now find the block containing the xbank setup code at $9BA0.
# The patchable epilogue layout is:
#   +0: PHP
#   +1: CLC
#   +2: BCC
#   +3: offset (0 when patched, 4 when unpatched)
#   +4: PLP (fast path)
#   +5: JMP (fast path)
#   +6: JMP lo
#   +7: JMP hi
#   +8: STA _a (regular path)
#   ...
#   +20: JMP cross_bank_dispatch
#   +21: PHP (xbank setup start)
#   +22: STA _a
#   +24: LDA #xx (addr lo)
#   +25: addr lo operand
#   +26: STA xbank_addr
#   +29: LDA #xx (addr hi)  
#   +30: addr hi operand
#   +31: STA xbank_addr+1
#   +34: LDA #xx (bank)
#   +35: bank operand
#   +36: JMP xbank_trampoline
#
# $9BA0 = xbank setup at +21 from epilogue start
# So epilogue starts at $9BA0 - 21 = $9B8B
# Block header at $9B8B - 8 = $9B83 (if header is before code)
# Actually header is BEFORE the code. The block code starts at header + 8.
# Let's look at the execution around $9BA0

# Show 40 lines of context before the crash
crash_line = None
for i, line in enumerate(lines):
    if '9BA0' in line and 'PHP' in line:
        crash_line = i
        # Don't break - get the LAST one

if crash_line is not None:
    print(f"\n=== 60 lines before crash at $9BA0 (L{crash_line}) ===")
    start = max(0, crash_line - 60)
    for i in range(start, min(len(lines), crash_line + 20)):
        print(f"  L{i}: {lines[i].rstrip()[:180]}")

# Find the JMP that brought us to $9BA0 - it should be in the epilogue fast path
print(f"\n=== Looking for JMP to $9BA0 pattern (BCC fast path) ===")
for i, line in enumerate(lines):
    text = line.strip()
    if 'JMP $9BA0' in text or 'JMP $9B' in text:
        print(f"  L{i}: {text[:180]}")
    # Also look for the BCC that activates the fast path at the epilogue
    if '$9B8B' in text or '$9B8C' in text or '$9B8D' in text or '$9B8E' in text:
        parts = text.split()
        if parts:
            try:
                a = int(parts[0][:4], 16)
                if 0x8000 <= a <= 0xBFFF:
                    print(f"  L{i}: {text[:180]}")
            except:
                pass
