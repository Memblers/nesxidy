"""Analyze DK trace log to find the compile-erase-compile loop."""
import sys

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"

# Key patterns to look for:
# 1. JMP ($FFFC) - soft reset (trigger_soft_reset)
# 2. STA $C000  - bankswitch (flash_byte_program, flash_sector_erase, etc)
# 3. Addresses in $6000-$7FFF = WRAM code (flash_byte_program, dispatch_on_pc, etc)
# 4. flash_sector_erase writes $AA,$55,$80,$AA,$55,$30 sequence to flash
# 5. Look for the SA compile PPU effect: STA $2001 with greyscale/emphasis bits

# First pass: find resets, erase patterns, and SA compile boundaries
resets = []
erase_count = 0
sa_ppu_writes = []  # STA $2001 during compile (greyscale toggle)
sta_c000_count = 0
first_flash_write = None
last_flash_write = None

# Track address ranges to understand execution flow
addr_ranges = {}  # bucket by 4K page

print(f"Reading {TRACE}...")
with open(TRACE, 'r', buffering=1024*1024) as f:
    for i, line in enumerate(f):
        if i % 5_000_000 == 0 and i > 0:
            print(f"  ...{i:,} lines processed", file=sys.stderr)
        
        # Quick filter - only process lines with interesting content
        # Mesen trace format: ADDR OPCODE OPERAND ... 
        if len(line) < 10:
            continue
        
        # Look for JMP ($FFFC) - soft reset
        if 'FFFC' in line and 'JMP' in line:
            resets.append((i, line.rstrip()[:120]))
        
        # Look for STA $2001 (PPU mask writes - SA compile effect)
        if '$2001' in line and 'STA' in line:
            if len(sa_ppu_writes) < 50:
                sa_ppu_writes.append((i, line.rstrip()[:120]))
        
        # Count bankswitches
        if '$C000' in line and 'STA' in line:
            sta_c000_count += 1
        
        # Track first 2M lines by address range
        if i < 2_000_000:
            try:
                addr = int(line[:4], 16)
                page = addr >> 12
                addr_ranges[page] = addr_ranges.get(page, 0) + 1
            except (ValueError, IndexError):
                pass
        
        # Stop after enough data
        if i >= 20_000_000:
            break

total = i + 1
print(f"\nProcessed {total:,} lines")
print(f"\n=== RESETS (JMP ($FFFC)) ===")
if resets:
    for ln, text in resets:
        print(f"  Line {ln:>10,}: {text}")
else:
    print("  None found!")

print(f"\n=== STA $C000 (bankswitches) === {sta_c000_count:,} total")

print(f"\n=== STA $2001 (PPU writes, first 50) ===")
for ln, text in sa_ppu_writes[:30]:
    print(f"  Line {ln:>10,}: {text}")

print(f"\n=== Address page distribution (first 2M lines) ===")
for page in sorted(addr_ranges.keys()):
    count = addr_ranges[page]
    pct = count * 100.0 / min(total, 2_000_000)
    bar = '#' * int(pct)
    print(f"  ${page:X}xxx: {count:>8,} ({pct:5.1f}%) {bar}")
