"""Find the compilation of the block for guest PC $32DF by searching for
bank 2 usage (recompile_opcode_b2_inner) and the FFF0 template or 
direct branch placeholder emission.

The flash write is around line 9489700. Compilation would be earlier.
Let's search for bank switches to bank 2 in a wider window.
"""
TRACE_FILE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\exidy.txt"

# Search from line 9400000 to 9489000 for bank 2 switches
# Also look for bank 17 (BANK_COMPILE) which is used by ir_emit_direct_branch_placeholder_b17
TARGET_START = 9400000
TARGET_END = 9490000

print(f"=== Searching lines {TARGET_START} to {TARGET_END} for bank 2 and bank 17 switches ===")

bank2_switches = []
bank17_switches = []
all_bank_switches = []

with open(TRACE_FILE, 'r') as f:
    for i, line in enumerate(f):
        if i < TARGET_START:
            continue
        if i > TARGET_END:
            break
        
        if 'STA $C000' in line:
            a_idx = line.find('A:')
            if a_idx >= 0:
                try:
                    a_val = int(line[a_idx+2:a_idx+4], 16)
                    all_bank_switches.append((i, a_val))
                    if a_val == 2:
                        bank2_switches.append((i, line.strip()[:140]))
                    if a_val == 17 or a_val == 0x11:
                        bank17_switches.append((i, line.strip()[:140]))
                except:
                    pass

print(f"\nTotal bank switches: {len(all_bank_switches)}")
print(f"Bank 2 switches: {len(bank2_switches)}")
print(f"Bank 17 switches: {len(bank17_switches)}")

# Show all bank values
unique_banks = {}
for _, val in all_bank_switches:
    unique_banks[val] = unique_banks.get(val, 0) + 1
print(f"\nBank switch distribution:")
for bank in sorted(unique_banks.keys()):
    print(f"  Bank {bank:2d} (${bank:02X}): {unique_banks[bank]} times")

if bank2_switches:
    print(f"\nBank 2 switches (last 10):")
    for ln, txt in bank2_switches[-10:]:
        print(f"  Line {ln}: {txt}")
        
if bank17_switches:
    print(f"\nBank 17 switches:")
    for ln, txt in bank17_switches[-10:]:
        print(f"  Line {ln}: {txt}")

# If no bank 2 found, search even further back
if not bank2_switches:
    print("\nNo bank 2 found in this range! Searching earlier...")
    # Binary search approach: check every 100K lines going backwards
    import sys
    for chunk_start in range(9300000, 9000000, -100000):
        found = False
        with open(TRACE_FILE, 'r') as f:
            for i, line in enumerate(f):
                if i < chunk_start:
                    continue
                if i >= chunk_start + 100000:
                    break
                if 'STA $C000' in line:
                    a_idx = line.find('A:')
                    if a_idx >= 0:
                        try:
                            a_val = int(line[a_idx+2:a_idx+4], 16)
                            if a_val == 2:
                                print(f"  Found bank 2 at line {i}")
                                bank2_switches.append((i, line.strip()[:140]))
                                found = True
                        except:
                            pass
        if found:
            print(f"  (in range {chunk_start}-{chunk_start+100000})")
            break
    else:
        print("  Still not found! The compilation might be much earlier.")
