"""Read lines 160440-160989 to trace boot flow between flash_format and game start.
This should show flash_cache_init_sectors, then sa_run (BFS walk + compile)."""
import re

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"

# Read lines 160440-160989 and categorize
print("=== BOOT FLOW: FLASH_FORMAT EXIT TO GAME START ===")
print("=== Lines 160440-160989 ===")

addr_ranges = {'fixed': 0, 'banked': 0, 'wram': 0, 'ppu': 0}
unique_banked = set()
flash_ops = []

with open(TRACE, 'r', buffering=4*1024*1024) as f:
    for i, line in enumerate(f):
        if i < 160440:
            continue
        if i > 160989:
            break
        
        text = line.rstrip()[:150]
        
        addr_m = re.match(r'([0-9A-Fa-f]{4})\s', line)
        if addr_m:
            addr = int(addr_m.group(1), 16)
            if 0xC000 <= addr <= 0xFFFF:
                addr_ranges['fixed'] += 1
            elif 0x8000 <= addr <= 0xBFFF:
                addr_ranges['banked'] += 1
                unique_banked.add(addr)
            elif 0x6000 <= addr <= 0x7FFF:
                addr_ranges['wram'] += 1
        
        # Track flash operations
        if line.startswith('6060') or line.startswith('6000'):
            flash_ops.append((i, text))
        
        # Print every 10th line for overview
        if (i - 160440) % 10 == 0 or i < 160510 or i > 160950:
            print(f"  {i:>8}: {text}")

print(f"\n=== SUMMARY ===")
print(f"Fixed bank ($C000+): {addr_ranges['fixed']}")
print(f"Banked ($8000-$BFFF): {addr_ranges['banked']}")
print(f"WRAM ($6000-$7FFF): {addr_ranges['wram']}")
print(f"Unique banked addrs: {len(unique_banked)}")
if unique_banked:
    sorted_b = sorted(unique_banked)
    print(f"Banked addr range: ${sorted_b[0]:04X} - ${sorted_b[-1]:04X}")
    # Show grouped by $100 range
    ranges = {}
    for a in sorted_b:
        r = a & 0xFF00
        ranges[r] = ranges.get(r, 0) + 1
    print("Banked address distribution:")
    for r in sorted(ranges.keys()):
        print(f"  ${r:04X}: {ranges[r]} unique addrs")

print(f"\nFlash operations in this range:")
for ln, text in flash_ops:
    print(f"  Line {ln}: {text}")
