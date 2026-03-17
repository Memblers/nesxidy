"""Examine each bank 17 switch (ir_emit_direct_branch_placeholder_b17 call)
to see what branch_opcode is being used and whether any corruption occurs.

Bank 17 switches found at lines:
9448440, 9449310, 9450602, 9451538, 9451693, 9451751, 9451804, 9451859, 9451913, 9471618

Let's look at the context around each one: 30 lines before and 100 lines after.
Focus on what registers hold when entering bank 17, and what bytes get written.
"""
TRACE_FILE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\exidy.txt"

bank17_lines = [9448440, 9449310, 9450602, 9451538, 9451693]

for target in bank17_lines:
    print(f"\n{'='*80}")
    print(f"=== Bank 17 switch at line {target} ===")
    print(f"{'='*80}")
    
    lines = []
    with open(TRACE_FILE, 'r') as f:
        for i, line in enumerate(f):
            if i >= target - 50 and i <= target + 150:
                lines.append((i, line.rstrip()))
            if i > target + 150:
                break
    
    # Print context showing key operations:
    # - Bank switches (STA $C000)
    # - Writes to code buffer (STA at addresses in code buffer area)
    # - JSR/RTS calls
    # - Register state at key points
    for ln, txt in lines:
        # Highlight important lines
        marker = ""
        if ln == target:
            marker = " <<< BANK 17 SWITCH"
        elif 'STA $C000' in txt:
            marker = " <<< BANK SWITCH"
        elif 'JSR' in txt and ('$C3' in txt or '$C4' in txt or '$62' in txt):
            marker = " <<< JSR"
        elif 'RTS' in txt:
            marker = " <<< RTS"
            
        # Only print lines with markers, or within ±15 of the bank switch
        if marker or abs(ln - target) <= 15:
            print(f"  {ln:>10}: {txt[:155]}{marker}")
    
    # Look for the inverted branch opcode write
    # After bank 17 switch, the _b17 function writes to cache_code
    # The first write is the inverted branch: p[0] = branch_opcode ^ 0x20
    print(f"\n  --- Writes after bank switch ---")
    write_count = 0
    for ln, txt in lines:
        if ln <= target:
            continue
        if ('STA' in txt) and ('$6C' in txt or '$6D' in txt or 'cache' in txt.lower()):
            print(f"  {ln:>10}: {txt[:155]}")
            write_count += 1
            if write_count >= 10:
                break

print("\n\n" + "="*80)
print("=== Now let's look at what comes BEFORE the first bank 17 switch ===")
print("=== to see the guest opcode being compiled ===")
print("="*80)

# Look 200 lines before the first bank 17 switch
target = 9448440
lines = []
with open(TRACE_FILE, 'r') as f:
    for i, line in enumerate(f):
        if i >= target - 200 and i <= target + 5:
            lines.append((i, line.rstrip()))
        if i > target + 5:
            break

print(f"\nLines {target-200} to {target+5}:")
for ln, txt in lines:
    # Look for interesting patterns
    marker = ""
    if 'STA $C000' in txt:
        marker = " <<< BANK"
    if ln == target:
        marker = " <<< BANK 17"
    # Show all lines in this range that involve:
    # - address in $8000-$BFFF (banked code)
    # - bank switches
    # - the opcode/PC values
    parts = txt.split()
    if parts:
        try:
            addr = int(parts[0][:4], 16)
            # Show lines in bank 2 code, or with markers
            if (0x8000 <= addr < 0xC000) or (0xC000 <= addr <= 0xFFFF) or marker:
                print(f"  {ln:>10}: {txt[:155]}{marker}")
        except:
            pass
