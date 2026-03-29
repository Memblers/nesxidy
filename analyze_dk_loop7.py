"""Trace the exact boot flow between lnSync exit and game start.
We know:
- Line ~151942: VBlank wait exits (frame 19)
- Lines 152390+: flash_sector_erase calls begin  
- Question: what happens AFTER the last flash_sector_erase?
  Does sa_run get called? Does the BFS walk start?"""
import re

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"

# First find the LAST flash_sector_erase call and what follows
print("=== FINDING LAST FLASH_SECTOR_ERASE IN FRAME 19 AND WHAT FOLLOWS ===")

last_erase_line = 0
last_erase_text = ""
post_erase_lines = []
found_last = False

with open(TRACE, 'r', buffering=4*1024*1024) as f:
    for i, line in enumerate(f):
        if i > 200000:
            break
        
        if line.startswith('6060'):
            last_erase_line = i
            last_erase_text = line.rstrip()[:150]
            post_erase_lines = []
            found_last = False
        elif last_erase_line > 0 and not found_last:
            post_erase_lines.append((i, line.rstrip()[:150]))
            if len(post_erase_lines) >= 500:
                found_last = True

print(f"Last flash_sector_erase at line {last_erase_line}: {last_erase_text}")
print(f"\n=== FIRST 100 LINES AFTER LAST FLASH_SECTOR_ERASE ===")
for ln, text in post_erase_lines[:100]:
    print(f"  {ln:>8}: {text}")

# Also find the exact transition point between frame 19 and frame 20
print(f"\n=== TRANSITION FROM FRAME 19 TO 20 (last 20 lines of Fr:19, first 20 of Fr:20) ===")
fr19_end = []
fr20_start = []
with open(TRACE, 'r', buffering=4*1024*1024) as f:
    for i, line in enumerate(f):
        if i > 200000:
            break
        m = re.search(r'Fr:(\d+)', line)
        if not m:
            continue
        fr = int(m.group(1))
        if fr == 19:
            fr19_end.append((i, line.rstrip()[:150]))
            if len(fr19_end) > 20:
                fr19_end.pop(0)
        elif fr == 20:
            fr20_start.append((i, line.rstrip()[:150]))
            if len(fr20_start) >= 20:
                break

print("\nLast 20 lines of frame 19:")
for ln, text in fr19_end:
    print(f"  {ln:>8}: {text}")
print("\nFirst 20 lines of frame 20:")
for ln, text in fr20_start:
    print(f"  {ln:>8}: {text}")

# Count all unique $8xxx addresses in frame 19 (to see what banks are active)
print(f"\n=== UNIQUE $8xxx-$9xxx ADDRESSES IN FRAME 19 (banked code) ===")
banked_addrs = set()
banked_count = 0
with open(TRACE, 'r', buffering=4*1024*1024) as f:
    in_fr19 = False
    for i, line in enumerate(f):
        if i > 200000:
            break
        m = re.search(r'Fr:(\d+)', line)
        if m:
            fr = int(m.group(1))
            if fr == 19:
                in_fr19 = True
            elif fr > 19:
                break
            else:
                in_fr19 = False
        if in_fr19:
            addr_m = re.match(r'([0-9A-Fa-f]{4})\s', line)
            if addr_m:
                addr = int(addr_m.group(1), 16)
                if 0x8000 <= addr <= 0xBFFF:
                    banked_addrs.add(addr)
                    banked_count += 1

print(f"Total banked instructions in frame 19: {banked_count}")
print(f"Unique banked addresses: {len(banked_addrs)}")
if banked_addrs:
    sorted_addrs = sorted(banked_addrs)
    print(f"Address range: ${sorted_addrs[0]:04X} - ${sorted_addrs[-1]:04X}")
    print(f"First 30 unique addresses: {', '.join(f'${a:04X}' for a in sorted_addrs[:30])}")
