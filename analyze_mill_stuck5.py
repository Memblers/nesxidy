"""
Analyze the Millipede trace: look at what happens BEFORE the CC85 OAM loop.
Search for _pc stores (guest address $7672) and dispatch pattern.
"""
import os, re

TRACE_FILE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\millipede.txt"

file_size = os.path.getsize(TRACE_FILE)
read_size = 10 * 1024 * 1024  # 10MB
offset = max(0, file_size - read_size)

with open(TRACE_FILE, 'rb') as f:
    f.seek(offset)
    data = f.read(read_size)
    
text = data.decode('utf-8', errors='replace')
lines = text.split('\n')
if offset > 0:
    lines = lines[1:]

print(f"File size: {file_size:,} bytes, read {len(lines)} lines")

# Find first line NOT in CC85-CC8F range (working backwards from end)
last_non_oam = len(lines) - 1
for i in range(len(lines)-1, -1, -1):
    line = lines[i].strip()
    if not line:
        continue
    # Extract PC (first 4 hex chars)
    m = re.match(r'([0-9A-F]{4})\s', line)
    if m:
        pc = int(m.group(1), 16)
        if pc < 0xCC85 or pc > 0xCC8F:
            last_non_oam = i
            break

print(f"Last non-OAM line at index {last_non_oam}/{len(lines)}")
print(f"OAM loop started at line {last_non_oam+1}, runs for {len(lines)-last_non_oam-1} lines")

# Print 200 lines before the OAM loop starts
start = max(0, last_non_oam - 200)
print(f"\n=== 200 LINES BEFORE OAM LOOP (lines {start}-{last_non_oam}) ===")
for i in range(start, last_non_oam + 1):
    print(lines[i].rstrip()[:160])

# Also search for LDA #$72 / STA patterns (guest PC store to _pc)
print("\n\n=== SEARCHING FOR GUEST $7672 PATTERNS (LDA #$72) ===")
count = 0
for i in range(len(lines)-1, max(0, len(lines)-50000), -1):
    line = lines[i].strip()
    if 'LDA #$72' in line or 'STA $' in line and '76' in line:
        if 'LDA #$72' in line:
            # Print context: this line and next 3
            print(f"\n--- LDA #$72 at line {i} ---")
            for j in range(max(0,i-2), min(len(lines), i+5)):
                print(f"  {lines[j].rstrip()[:160]}")
            count += 1
            if count >= 10:
                break
