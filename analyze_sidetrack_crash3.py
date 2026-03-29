"""Analyze the updated Side Track trace to understand why BRK still appears at $9A72.
Trace flash byte program writes to the $9A70 region, and look at what's being compiled.
"""
import sys

TRACE_FILE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\exidy.txt"

# Flash byte program: the recompiler writes individual bytes to flash
# We want to find writes near $9A70-$9A7F
# Also find what guest PC is being compiled when those writes happen

# The flash_byte_program routine lives in WRAM ($6000-$7FFF)
# It programs a byte to flash by writing to the SST39SF040
# The target address will be in the $8000-$BFFF range

# Let's look for:
# 1. All STA to addresses $9A70-$9A7F (direct flash writes)
# 2. The compilation context around those writes
# 3. Also check if $9A70 block was compiled at all in this trace, or if it's stale

# First pass: find writes to flash around $9A70
print("=== Searching for flash writes near $9A70 ===")
flash_writes_9a = []  # (line_num, line_text)
flash_writes_all_count = 0
context_before = []
CONTEXT_SIZE = 20

# Also track bank switches to see which bank is active
current_bank = None

# Look for STA $9A70..9A7F patterns and also byte program sequences
found_count = 0
total_lines = 0

# Track what's happening during compilation - look for the compile context
# The recompiler stores guest PC in ZP $5B/$5C before compile

with open(TRACE_FILE, 'r') as f:
    for i, line in enumerate(f):
        total_lines = i
        
        # Track bank switches 
        if 'STA $C000' in line:
            # Extract the value being stored
            parts = line.split('A:')
            if len(parts) > 1:
                try:
                    a_val = int(parts[1][:2], 16)
                    current_bank = a_val
                except:
                    pass
        
        # Look for any instruction at address $9A70-$9A7F  
        stripped = line.strip()
        if not stripped:
            continue
        parts = stripped.split()
        if not parts:
            continue
        try:
            addr = int(parts[0][:4], 16)
        except:
            continue
            
        # Flash byte program writes to $8000-$BFFF range
        # Look for STA instructions that write to $9A70-$9A80
        if 'STA' in line:
            # Check if the target is in our range
            for target_str in ['$9A70', '$9A71', '$9A72', '$9A73', '$9A74', 
                              '$9A75', '$9A76', '$9A77', '$9A78', '$9A79',
                              '$9A7A', '$9A7B', '$9A7C', '$9A7D', '$9A7E', '$9A7F']:
                if target_str in line:
                    flash_writes_9a.append((i, line.rstrip()[:160]))
                    break
        
        # Also catch writes via (indirect),Y addressing that land on $9A7x
        # These show up as [$9A7x] in the trace
        for target_str in ['[$9A70]', '[$9A71]', '[$9A72]', '[$9A73]', '[$9A74]',
                          '[$9A75]', '[$9A76]', '[$9A77]', '[$9A78]', '[$9A79]',
                          '[$9A7A]', '[$9A7B]', '[$9A7C]', '[$9A7D]', '[$9A7E]', '[$9A7F]']:
            if target_str in line:
                flash_writes_9a.append((i, line.rstrip()[:160]))
                break

        if i % 5000000 == 0 and i > 0:
            print(f"  ...scanned {i:,} lines...", file=sys.stderr)

print(f"\nTotal lines scanned: {total_lines:,}")
print(f"\nFlash writes to $9A70-$9A7F: {len(flash_writes_9a)}")
for ln, txt in flash_writes_9a:
    print(f"  Line {ln:>10}: {txt}")

if not flash_writes_9a:
    print("\nNo direct writes found to $9A70-$9A7F!")
    print("The block at $9A70 may be stale from a previous run,")
    print("or the writes use a different addressing mode.")
    print("\nLet's search for flash_byte_program calls writing nearby addresses...")
