"""Trace much further back from the flash write to find the actual compilation
of the BNE opcode that produces the $00 byte.

We know:
- Flash write loop starts around line 9489700 (at host $DA1E)
- _cache_code[2] = $00 (the bad byte)
- Before flash write, there's ir_lower + ir_resolve

Let's find the IR emission and recompilation phase by looking at what host
addresses are executing. The recompile_opcode_b2_inner is in bank 2 ($8000-$BFFF),
and the IR functions are in various banks.

Key approach: look for the code_index changes. Each time a byte is emitted
to the code buffer, code_index increments. We need to find when code_index
goes from 2 to 3 (that's when the $00 byte is written).
"""
TRACE_FILE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\exidy.txt"

# The flash write loop starts at ~line 9489700.
# Let's look 10000 lines back to find the compilation phase
# Look for patterns that indicate recompilation:
# - Addresses in $8000-$BFFF range (banked code)
# - Addresses in $C000-$FFFF range (fixed bank)
# - Writes to _code_index or code_index ZP locations

# First, let's find the boundaries of the compilation.
# The flash write loop is preceded by a section where the host builds the code buffer.
# The build involves:
# 1. recompile_block() calls recompile_opcode_b2() in a loop
# 2. Each call emits bytes + IR nodes
# 3. After all opcodes: ir_optimize, ir_lower, ir_resolve, flash write

# Let's look at lines 9480000-9489700 for the compilation

TARGET_START = 9480000
TARGET_END = 9489700

print("=== Reading compilation phase ===")
print(f"Lines {TARGET_START} to {TARGET_END}")

# Instead of reading all lines (too many), let's look for specific patterns:
# 1. Bank switches during compilation (STA $C000)
# 2. Host code in different address ranges
# 3. The ir_emit_direct_branch_placeholder function calls

# Track address ranges to understand program flow
addr_ranges = {}  # (range_start) -> count
bank_switches = []
interesting_lines = []

with open(TRACE_FILE, 'r') as f:
    for i, line in enumerate(f):
        if i < TARGET_START:
            continue
        if i > TARGET_END:
            break
        
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
        
        # Track address ranges (granularity: 256 bytes)
        page = addr & 0xFF00
        addr_ranges[page] = addr_ranges.get(page, 0) + 1
        
        # Bank switches
        if 'STA $C000' in line:
            # Extract A value
            a_val = None
            a_idx = line.find('A:')
            if a_idx >= 0:
                try:
                    a_val = int(line[a_idx+2:a_idx+4], 16)
                except:
                    pass
            bank_switches.append((i, a_val, line.strip()[:120]))

print(f"\n=== Address range distribution ===")
for page in sorted(addr_ranges.keys()):
    count = addr_ranges[page]
    if count > 100:
        label = ""
        if 0x6000 <= page < 0x8000: label = "(WRAM)"
        elif 0x8000 <= page < 0xC000: label = "(banked PRG)"
        elif 0xC000 <= page <= 0xFF00: label = "(fixed bank)"
        elif page < 0x0100: label = "(ZP indirect?)"
        print(f"  ${page:04X}: {count:>6} instructions {label}")

print(f"\n=== Bank switches ({len(bank_switches)}) ===")
# Just show first/last few and unique values
if bank_switches:
    unique_vals = set(v for _, v, _ in bank_switches if v is not None)
    print(f"  Unique bank values: {sorted(unique_vals)}")
    print(f"  First 5:")
    for ln, val, txt in bank_switches[:5]:
        print(f"    Line {ln}: bank={val} - {txt[:100]}")
    print(f"  Last 5:")
    for ln, val, txt in bank_switches[-5:]:
        print(f"    Line {ln}: bank={val} - {txt[:100]}")
