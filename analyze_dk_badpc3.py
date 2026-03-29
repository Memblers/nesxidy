"""
Search the ENTIRE 10MB tail for the moment _pc was set to $C805 
BEFORE the compile. The compile of $C805 started thousands of lines
before the crash. 

Also, trace the chain: what block's epilogue produced exit_pc=$C805?
And what set _pc=$C802 (which would have exit_pc=$C805 if compiled
as LDX abs from the operand byte)?
"""

import os

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"
TAIL_SIZE = 10 * 1024 * 1024

file_size = os.path.getsize(TRACE)
offset = max(0, file_size - TAIL_SIZE)

lines = []
with open(TRACE, 'r', encoding='utf-8', errors='replace') as f:
    f.seek(offset)
    if offset > 0:
        f.readline()
    lines = f.readlines()

print(f"Read {len(lines)} lines from tail")

# Find crash
crash_idx = None
for i in range(len(lines) - 1, max(0, len(lines) - 5000), -1):
    if 'LAX' in lines[i] and '$70CB' in lines[i]:
        crash_idx = i
        break
print(f"Crash at T{crash_idx}")

# Find ALL STA _pc (STA $51) stores in the entire tail where A=$05
# These are the places where _pc low byte was set to $05
print(f"\n=== ALL STA _pc with A=$05 in entire tail ===")
sta_pc_05_indices = []
for i in range(len(lines)):
    line = lines[i]
    if 'STA _pc' in line or 'STA $51' in line:
        if ' A:05 ' in line:
            sta_pc_05_indices.append(i)
            print(f"T{i}: {line.rstrip()[:130]}")

print(f"\nTotal STA _pc with A=$05: {len(sta_pc_05_indices)}")

# For each one, check if the NEXT store to $52 is $C8
print(f"\n=== Checking which have _pc+1 = $C8 ===")
for idx in sta_pc_05_indices:
    for j in range(idx+1, min(idx+10, len(lines))):
        line = lines[j]
        if ('STA $52' in line or 'STA _pc+1' in line):
            if ' A:C8 ' in line:
                print(f"T{idx}: _pc=$C805 (confirmed _pc+1=$C8 at T{j})")
                # Show context: 30 lines before
                print(f"  Context (T{max(0,idx-30)} to T{idx+5}):")
                for k in range(max(0, idx-30), min(idx+5, len(lines))):
                    print(f"    T{k}: {lines[k].rstrip()[:140]}")
                print()
            break

# Also find ALL dispatch_on_pc calls to see the compile/execute pattern
print(f"\n=== All dispatch_on_pc ($623A) entries ===")
dispatch_indices = []
for i in range(len(lines)):
    line = lines[i]
    if line[:4] == '623A' or line[:5] == ' 623A':
        # Check more precisely
        parts = line.split()
        if parts and parts[0] == '623A':
            dispatch_indices.append(i)

print(f"Total dispatch_on_pc calls: {len(dispatch_indices)}")
for d in dispatch_indices[-20:]:  # Last 20
    # Find _pc value
    pc_lo = '??'
    pc_hi = '??'
    for j in range(d, min(d+25, len(lines))):
        line = lines[j]
        if '6256' in line[:6] and '_pc =' in line:
            idx2 = line.find('_pc =')
            if idx2 >= 0:
                pc_lo = line[idx2+5:idx2+9].strip().lstrip('$')
        if '6240' in line[:6] and '$52 =' in line:
            idx2 = line.find('$52 =')
            if idx2 >= 0:
                pc_hi = line[idx2+5:idx2+9].strip().lstrip('$')
    
    # Also find what dispatch returned
    # After dispatch, not_recompiled ($630C) means compile needed
    # If block ran, flash_dispatch_return ($62B4) is hit
    result = "?"
    for j in range(d+1, min(d+200, len(lines))):
        line = lines[j]
        if line[:4] == '630C':
            result = "NOT_RECOMPILED"
            break
        if line[:4] == '62B4' or '62B4' in line[:6]:
            result = "EXECUTED"
            break
        # Check for the JMP to native code
        if '62B1' in line[:6] and 'JMP' in line:
            result = "JMP_TO_NATIVE"
            break
    
    print(f"  T{d}: _pc=${pc_hi}{pc_lo} → {result}")
