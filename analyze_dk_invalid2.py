"""
Deeper analysis of the invalid opcode crash.
Look at the broader context around the crash:
- What was the guest PC being dispatched?
- What flash bank/address was targeted?
- Were there recent flash_byte_program calls?
- What compilation was happening before the crash?
"""

trace_file = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"

# Read a larger tail - 500KB to see compilation context before the crash
import os
size = os.path.getsize(trace_file)
tail_bytes = 500_000

with open(trace_file, 'r', encoding='utf-8', errors='replace') as f:
    f.seek(max(0, size - tail_bytes))
    if f.tell() > 0:
        f.readline()  # skip partial
    tail_lines = f.readlines()

print(f"Read {len(tail_lines)} tail lines")

# Find the crash line (LAX / $BF / invalid opcode)
crash_idx = None
for i, line in enumerate(tail_lines):
    if 'LAX' in line or 'BC:BF' in line.upper():
        crash_idx = i
        print(f"Found crash at tail line {i}: {line.rstrip()[:120]}")

if crash_idx is None:
    print("No LAX/BF found in tail, checking for other invalid opcodes...")
    # Look for common illegal opcodes
    for i, line in enumerate(tail_lines):
        bc_pos = line.find('BC:')
        if bc_pos >= 0:
            bc_str = line[bc_pos+3:bc_pos+5].strip()
            if bc_str in ['02','12','22','32','42','52','62','72','92','B2','D2','F2',
                          '03','13','23','33','43','53','63','73','83','93','A3','B3','C3','D3','E3','F3',
                          '07','17','27','37','47','57','67','77','87','97','A7','B7','C7','D7','E7','F7',
                          '0B','1B','2B','3B','4B','5B','6B','7B','8B','9B','AB','BB','CB','DB','EB','FB',
                          '0F','1F','2F','3F','4F','5F','6F','7F','8F','9F','AF','BF','CF','DF','EF','FF']:
                crash_idx = i
                print(f"Found potential illegal opcode at tail line {i}: {line.rstrip()[:120]}")

if crash_idx is not None:
    print(f"\n=== 50 LINES BEFORE CRASH ===")
    start = max(0, crash_idx - 50)
    for i in range(start, crash_idx + 1):
        print(f"  T{i}: {tail_lines[i].rstrip()[:200]}")

# Look for flash_byte_program calls ($6000) in the tail
print(f"\n=== FLASH BYTE PROGRAM CALLS (last 200 before crash) ===")
search_start = max(0, crash_idx - 3000) if crash_idx else max(0, len(tail_lines) - 3000)
search_end = crash_idx if crash_idx else len(tail_lines)
fbp_count = 0
for i in range(search_start, search_end):
    line = tail_lines[i]
    if '6000' in line and ('JSR' in line or 'JMP' in line):
        fbp_count += 1
        if fbp_count <= 5 or fbp_count % 50 == 0:
            print(f"  T{i}: {line.rstrip()[:200]}")
print(f"Total flash_byte_program calls in window: {fbp_count}")

# Look for dispatch_on_pc calls ($623A) near crash
print(f"\n=== DISPATCH_ON_PC CALLS NEAR CRASH ===")
search_start2 = max(0, crash_idx - 500) if crash_idx else max(0, len(tail_lines) - 500)
for i in range(search_start2, crash_idx + 1 if crash_idx else len(tail_lines)):
    line = tail_lines[i]
    if '623A' in line:
        print(f"  T{i}: {line.rstrip()[:200]}")

# Show the last instruction and decode the dispatch
print(f"\n=== CRASH ANALYSIS ===")
if crash_idx is not None:
    line = tail_lines[crash_idx]
    print(f"Crash line: {line.rstrip()[:200]}")
    
    # Find the JMP that brought us here
    for i in range(crash_idx - 1, max(0, crash_idx - 20), -1):
        l = tail_lines[i]
        if 'JMP' in l or 'JSR $62B1' in l:
            print(f"  Dispatch JMP at T{i}: {l.rstrip()[:200]}")
            break
    
    # Find _pc store
    for i in range(crash_idx - 1, max(0, crash_idx - 200), -1):
        l = tail_lines[i]
        if '_pc' in l and 'STA' in l:
            print(f"  PC store at T{i}: {l.rstrip()[:200]}")
            break
    for i in range(crash_idx - 1, max(0, crash_idx - 200), -1):
        l = tail_lines[i]
        if '$52' in l and 'STA' in l:
            print(f"  PC hi store at T{i}: {l.rstrip()[:200]}")
            break

# Look for the recompile_opcode calls to see what was being compiled
print(f"\n=== RECOMPILE_OPCODE CALLS ($E21F) NEAR CRASH ===")
recomp_lines = []
for i in range(search_start, search_end):
    line = tail_lines[i]
    if 'E21F' in line and ('JSR' in line or 'JMP' in line):
        recomp_lines.append((i, line))
print(f"Found {len(recomp_lines)} recompile_opcode calls")
if recomp_lines:
    for idx, (i, l) in enumerate(recomp_lines[-10:]):
        print(f"  T{i}: {l.rstrip()[:200]}")
