"""Look at the actual instructions in the hang loop $ED5D-$ED93 and 
understand the transition into it."""
import re, os

f = r'C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\millipede.txt'
size = os.path.getsize(f)

# Read the very last portion of the file
lines = []
with open(f, 'r') as fh:
    fh.seek(max(0, size - 200000))
    fh.readline()  # discard partial
    for line in fh:
        lines.append(line.rstrip())

# Show the last 200 lines with full instruction detail
print("=== LAST 200 LINES (HANG LOOP) ===")
for line in lines[-200:]:
    print(line[:200])

print()
print("=== LOOKING FOR ENTRY INTO THE HANG LOOP ===")
# Find the first occurrence of $ED5D in this buffer
for i, line in enumerate(lines):
    if line.startswith('ED5D') or line.startswith('ED5D '):
        # Show 50 lines before this
        start = max(0, i - 100)
        print(f"First ED5D at buffer offset {i}, showing context from {start}:")
        for j in range(start, min(i + 30, len(lines))):
            print(f"  {lines[j][:200]}")
        break
