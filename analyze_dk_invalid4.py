"""
Search for flash_byte_program by label name instead of address.
Also search for general patterns that indicate flash writes.
"""
import os

trace_file = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"
size = os.path.getsize(trace_file)

# Read 5MB tail
tail_bytes = 5_000_000
with open(trace_file, 'r', encoding='utf-8', errors='replace') as f:
    f.seek(max(0, size - tail_bytes))
    if f.tell() > 0:
        f.readline()
    tail_lines = f.readlines()

print(f"Read {len(tail_lines)} tail lines")

# Find crash
crash_idx = len(tail_lines) - 1

# Search for flash_byte_program by various patterns
patterns = ['flash_byte_program', '_flash_byte', 'JSR $6000', '6000', 'BC:20 00 60']
for pat in patterns:
    count = 0
    last_idx = None
    for i in range(max(0, crash_idx - 40000), crash_idx):
        if pat.lower() in tail_lines[i].lower():
            count += 1
            last_idx = i
    print(f"Pattern '{pat}': {count} matches in last 40K lines" + 
          (f", last at T{last_idx}" if last_idx else ""))

# Let's look at the actual compile path - search for recompile_opcode ($E21F)
print(f"\n=== RECOMPILE_OPCODE CALLS ===")
recomp_calls = []
for i in range(max(0, crash_idx - 40000), crash_idx):
    line = tail_lines[i]
    if 'E21F' in line or 'recompile_opcode' in line.lower():
        recomp_calls.append(i)
        if len(recomp_calls) <= 5:
            print(f"  T{i}: {line.rstrip()[:180]}")

print(f"Total recompile_opcode calls: {len(recomp_calls)}")

# Search for ir_lower or ir_optimize calls
print(f"\n=== IR CALLS ===")
ir_patterns = ['ir_lower', 'ir_optimize', 'ir_resolve', 'ir_compute']
for pat in ir_patterns:
    for i in range(max(0, crash_idx - 40000), crash_idx):
        if pat in tail_lines[i].lower():
            print(f"  T{i} [{pat}]: {tail_lines[i].rstrip()[:180]}")
            break

# Search for flash_code_address reads near entry_pc
print(f"\n=== FLASH CODE ADDRESS NEAR COMPILE ===")
for i in range(max(0, crash_idx - 40000), crash_idx):
    line = tail_lines[i]
    if 'flash_code_address' in line.lower() or '_flash_code_address' in line:
        if i > crash_idx - 10000:
            print(f"  T{i}: {line.rstrip()[:180]}")

# Look at what address $6000 contains - check if any execution hits $6000
print(f"\n=== ANY EXECUTION AT $6000 ===")
for i in range(max(0, crash_idx - 40000), crash_idx):
    line = tail_lines[i]
    # Check if the PC (first field) is $6000
    stripped = line.lstrip()
    if stripped.startswith('6000') or stripped.startswith('6001') or stripped.startswith('6002'):
        print(f"  T{i}: {line.rstrip()[:180]}")
        break

# Check for sector erase or alloc
print(f"\n=== SECTOR ALLOC/ERASE ===")
for i in range(max(0, crash_idx - 40000), crash_idx):
    line = tail_lines[i]
    if 'flash_sector' in line.lower() or 'sector_alloc' in line.lower():
        if i > crash_idx - 5000:
            print(f"  T{i}: {line.rstrip()[:180]}")

# Try raw search: what are the last 20 unique address ranges executed before crash?
print(f"\n=== EXECUTION ADDRESS RANGES (last 500 lines) ===")
addr_ranges = set()
for i in range(max(0, crash_idx - 500), crash_idx + 1):
    line = tail_lines[i].lstrip()
    # Extract PC address (first 4 hex chars)
    if len(line) >= 4:
        try:
            addr = int(line[:4], 16)
            addr_range = addr & 0xFF00
            addr_ranges.add(addr_range)
        except ValueError:
            pass
sorted_ranges = sorted(addr_ranges)
for r in sorted_ranges:
    print(f"  ${r:04X}-${r+0xFF:04X}")
