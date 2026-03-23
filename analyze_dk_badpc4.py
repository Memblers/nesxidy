"""
The _pc=$C805 was NOT set within the 10MB tail. Let's try 50MB.
We need to find the block that executed and set exit_pc=$C805.
That block would have been compiled from $C802 (byte $AE = LDX abs,
3-byte instruction, so exit_pc = $C802+3 = $C805).

And the block at $C802 was compiled because some previous block
set exit_pc=$C802. That one was compiled from $C7FF or whatever.

Let's trace the chain by finding when _pc was set to $C802 and $C805.
The _pc write might be from flash code (epilogue).

Strategy: Read 50MB from tail. Search for ALL writes to _pc ($51)
where value is $02, $05, or matches $C8xx pattern.
Also search for dispatch_on_pc results for $C802 and $C805.
"""

import os

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"
TAIL_SIZE = 50 * 1024 * 1024  # 50MB

file_size = os.path.getsize(TRACE)
offset = max(0, file_size - TAIL_SIZE)

print(f"File size: {file_size / (1024*1024):.0f} MB, reading last {TAIL_SIZE // (1024*1024)} MB")

# Read in chunks to avoid memory issues - just search for key patterns
pc_writes = []  # (line_num, line_text) for STA _pc or STA $51
dispatches_c805 = []  # dispatch_on_pc calls for _pc=$C805
dispatches_c802 = []
compile_c802 = []
compile_c805 = []

line_num = 0
with open(TRACE, 'r', encoding='utf-8', errors='replace') as f:
    f.seek(offset)
    if offset > 0:
        f.readline()
    
    for line in f:
        line_num += 1
        
        # Look for STA _pc (ZP $51) where A matches our targets
        # The epilogue writes _pc. In the trace, it shows as:
        # "xxxx  STA _pc = $yy  A:zz ..."
        # We want A:05 (for _pc_lo=$05) or A:02 (for _pc_lo=$02)
        if 'STA _pc ' in line and 'STA _pc_' not in line:
            if ' A:05 ' in line or ' A:02 ' in line:
                pc_writes.append((line_num, line.rstrip()[:140]))
                if len(pc_writes) <= 50:
                    pass  # collect all
        
        # Look for STA $52 (pc_hi) with A:C8
        if 'STA $52 ' in line and ' A:C8 ' in line:
            pc_writes.append((line_num, 'PC_HI: ' + line.rstrip()[:130]))
        
        # Look for dispatch reading _pc=$05 followed by $52=$C8
        if line[:4] == '623A':
            # This is dispatch_on_pc entry
            pass
        
        if line_num % 500000 == 0:
            print(f"  Scanned {line_num:,} lines...", flush=True)

print(f"\nTotal lines scanned: {line_num:,}")
print(f"Total STA _pc/STA $52 matches: {len(pc_writes)}")

# Now pair up: find STA _pc with A:02 or A:05 followed by STA $52 with A:C8
print(f"\n=== STA _pc lo=$02 or $05 entries ===")
for ln, text in pc_writes:
    print(f"  L{ln:>8}: {text}")

# Look for the FIRST STA _pc with A=$05 or A=$02 — this should be the
# block epilogue that set _pc to the mid-instruction address
print(f"\n=== Pairing _pc writes ===")
# Group by proximity: STA _pc (lo) + STA $52 (hi) within 5 lines
lo_writes = [(ln, t) for ln, t in pc_writes if 'STA _pc' in t and 'PC_HI' not in t]
hi_writes = [(ln, t) for ln, t in pc_writes if 'PC_HI' in t]

for lo_ln, lo_text in lo_writes:
    for hi_ln, hi_text in hi_writes:
        if abs(hi_ln - lo_ln) <= 5:
            a_val = '05' if ' A:05 ' in lo_text else '02'
            print(f"  _pc=${a_val}, $52=$C8 at L{lo_ln} (hi at L{hi_ln})")
            print(f"    LO: {lo_text}")
            print(f"    HI: {hi_text}")
