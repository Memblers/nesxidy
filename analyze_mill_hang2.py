"""Analyze the host trace (millipede.txt) ending to understand why IRQs never fire.
Focus on the main loop flow: run_6502 → NMI check → IRQ delivery.
"""

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\millipede.txt"

import os
size = os.path.getsize(TRACE)
print(f"Host trace: {size:,} bytes")

# Read last 200 lines
lines = []
with open(TRACE, 'rb') as f:
    # Seek near end
    f.seek(max(0, size - 50000))
    chunk = f.read().decode('utf-8', errors='replace')
    lines = chunk.split('\n')

print(f"\n=== LAST 50 LINES ===")
for line in lines[-50:]:
    print(line[:160])

print(f"\n=== LINES -200 to -150 ===")
for line in lines[-200:-150]:
    print(line[:160])
