"""Trace the exact transition from frame 18 to frame 19.
The BFS walk ends in frame 18 (VBlank wait at C2AA).
The SA compile should follow, but we need to see what actually happens."""
import re, sys

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"

# Key addresses from map:
# _flash_sector_erase = $6060 (WRAM)
# _flash_byte_program = $6000 (WRAM)
# _sa_compile_completed = $60E1 (WRAM var)
# _sa_do_compile = $6202 (WRAM var)
# _dispatch_on_pc = $623A (WRAM)
# _main = $C51B (fixed)
# _recompile_opcode = $E21F (fixed)

# We want to see:
# 1. When does the VBlank wait at C2AA finally exit (BNE not taken)?
# 2. What code runs after that?
# 3. Is there ANY STA to $60E1 (sa_compile_completed)?

print("=== SEARCHING FOR VBLANK WAIT EXIT POINTS (BNE C2AA NOT TAKEN) ===")
print("=== AND STA TO $60E1 (sa_compile_completed) ===")
print("=== First 200K lines ===")

vblank_exits = []
sa_completed_writes = []
interesting = []
prev_was_vblank = False

with open(TRACE, 'r', buffering=4*1024*1024) as f:
    for i, line in enumerate(f):
        if i > 200000:
            break
        
        addr_m = re.match(r'([0-9A-Fa-f]{4})\s', line)
        if not addr_m:
            continue
        addr = addr_m.group(1).upper()
        
        # Track VBlank wait exits (when C2AE BNE doesn't branch back to C2AA)
        if addr == 'C2AE' and 'BNE' in line:
            prev_was_vblank = True
            continue
        
        if prev_was_vblank:
            prev_was_vblank = False
            if addr != 'C2AA':  # VBlank wait exited!
                vblank_exits.append((i, line.rstrip()[:150]))
                # Print context: next 30 lines after exit
                interesting.append(('VBLANK_EXIT', i))
        
        # Look for writes to $60E1 (sa_compile_completed)
        if '60E1' in line and ('STA' in line or 'STX' in line or 'STY' in line):
            sa_completed_writes.append((i, line.rstrip()[:150]))

print(f"VBlank exits found: {len(vblank_exits)}")
for ln, text in vblank_exits:
    print(f"  Line {ln}: {text}")

print(f"\nSTA to $60E1 (sa_compile_completed): {len(sa_completed_writes)}")
for ln, text in sa_completed_writes:
    print(f"  Line {ln}: {text}")

# Now get 50 lines after each VBlank exit
print("\n=== CONTEXT AFTER EACH VBLANK EXIT (next 40 lines) ===")
for exit_type, exit_line in interesting:
    print(f"\n--- After {exit_type} at line {exit_line} ---")
    with open(TRACE, 'r', buffering=4*1024*1024) as f:
        for i, line in enumerate(f):
            if i >= exit_line and i < exit_line + 40:
                print(f"  {i:>8}: {line.rstrip()[:150]}")
            if i >= exit_line + 40:
                break

# Also: look for any access to $6060 (flash_sector_erase) in first 200K lines
# This would indicate SA compile Pass 1/2 or between-passes erase
print("\n=== CALLS TO flash_sector_erase ($6060) in first 200K lines ===")
erase_calls = []
with open(TRACE, 'r', buffering=4*1024*1024) as f:
    for i, line in enumerate(f):
        if i > 200000:
            break
        if line.startswith('6060') or (len(line) > 4 and line[4:8] == '6060'):
            erase_calls.append((i, line.rstrip()[:150]))

print(f"flash_sector_erase entries: {len(erase_calls)}")
for ln, text in erase_calls[:20]:
    print(f"  Line {ln}: {text}")

# Also check for any calls to flash_sector_erase across frames 2-25
print("\n=== FLASH_SECTOR_ERASE ($6060) and FLASH_BYTE_PROGRAM ($6000) counts per frame ===")
frame_erase = {}
frame_program = {}
with open(TRACE, 'r', buffering=4*1024*1024) as f:
    for i, line in enumerate(f):
        if i > 200000:
            break
        fr_m = re.search(r'Fr:(\d+)', line)
        if not fr_m:
            continue
        fr = int(fr_m.group(1))
        if fr > 30:
            break
        
        addr_m = re.match(r'([0-9A-Fa-f]{4})\s', line)
        if not addr_m:
            continue
        addr = addr_m.group(1).upper()
        
        if addr == '6060':
            frame_erase[fr] = frame_erase.get(fr, 0) + 1
        if addr == '6000':
            frame_program[fr] = frame_program.get(fr, 0) + 1

for fr in sorted(set(list(frame_erase.keys()) + list(frame_program.keys()))):
    e = frame_erase.get(fr, 0)
    p = frame_program.get(fr, 0)
    print(f"  Frame {fr:3d}: erase={e:5d}  program={p:5d}")
