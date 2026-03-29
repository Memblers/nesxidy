"""Trace exactly what sa_run does — it enters at line 159650 and exits before line 160989.
Show every instruction to see why it returns so quickly."""
import re

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"

print("=== sa_run() EXECUTION (lines 159650-160989) ===")
print("=== Showing first 200 instructions, then last 100 ===")

lines_buf = []
with open(TRACE, 'r', buffering=4*1024*1024) as f:
    for i, line in enumerate(f):
        if i < 159650:
            continue
        if i > 160989:
            break
        lines_buf.append((i, line.rstrip()[:160]))

print(f"Total lines in sa_run: {len(lines_buf)}")

# Show first 200
print(f"\n--- First 200 lines ---")
for ln, text in lines_buf[:200]:
    print(f"  {ln:>8}: {text}")

# Show last 100
print(f"\n--- Last 100 lines ---")
for ln, text in lines_buf[-100:]:
    print(f"  {ln:>8}: {text}")

# Categorize addresses
addr_hist = {}
for ln, text in lines_buf:
    m = re.match(r'([0-9A-Fa-f]{4})\s', text)
    if m:
        addr = int(m.group(1), 16)
        region = addr & 0xF000
        addr_hist[region] = addr_hist.get(region, 0) + 1

print(f"\n=== ADDRESS DISTRIBUTION IN sa_run ===")
for region in sorted(addr_hist.keys()):
    print(f"  ${region:04X}: {addr_hist[region]} instructions")

# Look for any $8xxx addresses and show what they do
print(f"\n=== ALL BANKED ($8xxx) INSTRUCTIONS IN sa_run ===")
for ln, text in lines_buf:
    m = re.match(r'([0-9A-Fa-f]{4})\s', text)
    if m and int(m.group(1), 16) >= 0x8000 and int(m.group(1), 16) < 0xC000:
        print(f"  {ln:>8}: {text}")
