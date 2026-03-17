#!/usr/bin/env python3
"""Root cause v4: trace address flow with correct parsing."""
import re, os

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\exidy.txt"
TAIL_BYTES = 8 * 1024 * 1024

def read_tail(path, nbytes):
    sz = os.path.getsize(path)
    start = max(0, sz - nbytes)
    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        f.seek(start)
        if start > 0:
            f.readline()
        return f.readlines()

lines = read_tail(TRACE, TAIL_BYTES)
print(f"Read {len(lines)} lines from tail")

# Find flag write
flag_idx = None
for i in range(len(lines)-1, -1, -1):
    if 'AA76' in lines[i] and 'STA' in lines[i]:
        flag_idx = i
        break
print(f"Flag write at line {flag_idx}")

# Parse address from line (format: "XXXX  INSTR ...")
def get_addr(line):
    m = re.match(r'([0-9A-Fa-f]{4})\s', line.strip())
    if m:
        return int(m.group(1), 16)
    return None

def addr_range(addr):
    if addr is None: return "???"
    if 0x8000 <= addr < 0xC000: return "BANK17"
    elif 0xC000 <= addr < 0xD800: return "FIX_C"
    elif 0xD800 <= addr < 0xDC00: return "FIX_D8"
    elif 0xDC00 <= addr < 0xE000: return "FIX_DC"
    elif 0xE000 <= addr < 0xE500: return "FIX_E0"
    elif 0xE500 <= addr < 0xE800: return "FIX_E5"
    elif 0xE800 <= addr < 0xF000: return "FIX_E8"
    elif 0xF000 <= addr <= 0xFFFF: return "FIX_F"
    elif 0x6000 <= addr < 0x6100: return "WRAM_FLASH"
    elif 0x6100 <= addr < 0x6200: return "WRAM_61"
    elif 0x6200 <= addr < 0x6400: return "WRAM_DISP"
    elif 0x6400 <= addr < 0x6600: return "WRAM_PEEK"
    else: return f"OTHER_{addr:04X}"

# Show address range transitions in the 1500 lines before the flag write
start = max(0, flag_idx - 1500)
print(f"\n=== Address flow transitions (line {start} to {flag_idx}) ===")
prev_range = ""
range_start_line = start
range_start_addr = 0
for i in range(start, flag_idx + 5):
    addr = get_addr(lines[i])
    curr = addr_range(addr)
    if curr != prev_range:
        if prev_range:
            print(f"  [{range_start_line:06d}-{i-1:06d}] {prev_range:12s} (${range_start_addr:04X}..)")
        prev_range = curr
        range_start_line = i
        range_start_addr = addr if addr else 0
    # Also print interesting specific lines regardless
    line = lines[i].rstrip()
    if any(kw in line for kw in ['_pc =', '_pc+1', 'dispatch_on_pc', 'sector_alloc',
                                  'cache_misses', 'cache_interpret', 'entry_list',
                                  'sa_compile', 'sa_bitmap', 'entry_pc',
                                  'BLOCK_SENTINEL', 'interpret_6502']):
        print(f"    *** {i:06d}: {line[:120]}")

if prev_range:
    print(f"  [{range_start_line:06d}-{flag_idx:06d}] {prev_range:12s}")

# Now: dump the FULL 300 lines immediately before the first BANK17 call
# This is the C code that initiated the compile
print(f"\n=== Full trace: 300 lines before flag write ===")
dump_start = max(0, flag_idx - 300)
for i in range(dump_start, flag_idx + 5):
    addr = get_addr(lines[i])
    line = lines[i].rstrip()
    rng = addr_range(addr)
    
    tag = ""
    if 'STA' in line and ('A:6A ' in line or 'A:6A\t' in line) and '$5C' in line:
        tag = " <<<<< _pc HI = $6A"
    if 'STA' in line and 'A:76 ' in line and ('$5B' in line or '_pc =' in line):
        tag = " <<<<< _pc LO = $76"
    if 'JSR' in line and ('dispatch' in line.lower() or '$62' in line):
        tag = " <<<<< DISPATCH"
    if 'AA76' in line:
        tag = " <<<<< FLAG TARGET"
    
    print(f"{i:06d} [{rng:12s}] {line[:120]}{tag}")
