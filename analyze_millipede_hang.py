"""Analyze the end-of-trace hang loop in detail.
Look at the actual DEY counter to see if the loop is infinite.
Also look at what guest PC is being processed when the hang occurs.
"""
import re

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\millipede.txt"

# Read last 2000 lines to see the hang loop pattern
lines = []
with open(TRACE, 'r') as f:
    # Read from the end
    f.seek(0, 2)
    size = f.tell()
    # Read last 200KB
    read_size = min(200000, size)
    f.seek(size - read_size)
    chunk = f.read()
    lines = chunk.split('\n')
    if lines and not lines[-1]:
        lines = lines[:-1]

print(f"Read {len(lines)} lines from end of trace")
print()

# Show the last 100 lines with Y register values highlighted
print("=== LAST 100 LINES (focus on Y register) ===")
for line in lines[-100:]:
    # Extract Y value
    m = re.search(r'Y:([0-9A-F]{2})', line)
    y_val = m.group(1) if m else '??'
    # Extract address
    addr_m = re.match(r'\s*([0-9A-F]{4})\s', line)
    addr = addr_m.group(1) if addr_m else '????'
    # Highlight DEY and BNE lines
    if 'DEY' in line or 'BNE' in line or 'BCC' in line:
        print(f"  >>> {line.strip()[:120]}")
    elif addr in ('ED5D', 'ED93'):
        print(f"  * {line.strip()[:120]}")
    else:
        pass  # Skip non-interesting lines for brevity

print()

# Now track Y values at the DEY instruction to see if it's counting down
print("=== Y REGISTER VALUES AT DEY INSTRUCTIONS (last 500 lines) ===")
dey_y_values = []
for line in lines[-500:]:
    if 'DEY' in line:
        m = re.search(r'Y:([0-9A-F]{2})', line)
        if m:
            dey_y_values.append(int(m.group(1), 16))

print(f"DEY Y values (before DEY): {dey_y_values}")
print(f"Number of DEY instructions: {len(dey_y_values)}")
if dey_y_values:
    print(f"Y range: {min(dey_y_values):#04x} to {max(dey_y_values):#04x}")
    # Check if Y is monotonically decreasing
    is_decreasing = all(dey_y_values[i] >= dey_y_values[i+1] or dey_y_values[i] == 0 for i in range(len(dey_y_values)-1))
    print(f"Monotonically decreasing: {is_decreasing}")

print()

# Look further back for the context BEFORE the hang loop starts
# Find where $ED5D first appears
print("=== SEARCHING FOR HANG LOOP START ===")
first_ed5d = None
for i, line in enumerate(lines):
    if line.strip().startswith('ED5D'):
        first_ed5d = i
        break

if first_ed5d is not None:
    print(f"First ED5D in last {len(lines)} lines: line index {first_ed5d}")
    print(f"Lines before ED5D: {first_ed5d}")
    print()
    # Show 30 lines before the hang loop starts
    print("=== 30 LINES BEFORE HANG LOOP ===")
    start = max(0, first_ed5d - 30)
    for line in lines[start:first_ed5d+5]:
        print(f"  {line.strip()[:140]}")
else:
    print("ED5D not found in last lines - hang loop may start earlier")
    # Show the first few lines we have
    print("=== FIRST 20 LINES OF OUR WINDOW ===")
    for line in lines[:20]:
        print(f"  {line.strip()[:140]}")

print()

# Count iterations of the outer loop (DEY/BNE)
print("=== COUNTING HANG LOOP ITERATIONS ===")
outer_loops = 0
inner_loops = 0
for line in lines:
    stripped = line.strip()
    if stripped.startswith('ED5D'):
        inner_loops += 1
    if 'DEY' in stripped and stripped.startswith('ED'):
        outer_loops += 1

print(f"Inner loop entries (ED5D): {inner_loops}")
print(f"DEY instructions: {outer_loops}")
if outer_loops > 0:
    print(f"Avg inner iterations per outer: {inner_loops / outer_loops:.1f}")

print()

# Look for the LAST few instructions before ED5D to understand caller
print("=== LOOKING FOR CALLER OF HANG LOOP ===")
# Find the transition INTO the hang
for i, line in enumerate(lines):
    stripped = line.strip()
    if stripped.startswith('ED5D'):
        # Show what came before
        if i > 0:
            prev = lines[i-1].strip()
            if not prev.startswith('ED'):
                print(f"Transition into loop from: {prev[:140]}")
                if i > 5:
                    print("Context before:")
                    for j in range(max(0, i-10), i):
                        print(f"    {lines[j].strip()[:140]}")
                print()
        break
