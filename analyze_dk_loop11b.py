"""
analyze_dk_loop11b.py - Check raw trace content around line 160989
"""
TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"

print("Reading lines 160985-161010...")
with open(TRACE, 'r', errors='replace') as f:
    for i, line in enumerate(f, 1):
        if 160985 <= i <= 161010:
            print(f"L{i}: [{line.rstrip()[:140]}]")
        if i > 161010:
            break

print("\nChecking file size and total line count (sample)...")
import os
size = os.path.getsize(TRACE)
print(f"File size: {size:,} bytes ({size/1024/1024/1024:.2f} GB)")

# Count total lines (quick sample)
with open(TRACE, 'r', errors='replace') as f:
    count = 0
    for _ in f:
        count += 1
        if count > 200000:
            break
print(f"Line count (first 200K check): {'> 200K' if count > 200000 else count}")
