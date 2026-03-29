#!/usr/bin/env python3
"""Analyze Donkey Kong trace log for flash cache misses.
Decode block compilations from flash_byte_program ($6000) calls.
Reuse the same approach as analyze_excitebike_trace4.py."""

import re
from collections import Counter, defaultdict

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"

# Read first 10 lines to understand format
print("=== First 10 lines ===")
with open(TRACE, 'r') as f:
    for i in range(10):
        line = f.readline().rstrip()
        print(f"  {line[:160]}")

# Scan for flash_byte_program ($6000) calls and frame boundaries
print("\n=== Scanning for flash writes and frame info ===")

flash_write_count = 0
frame_set = set()
wram_exec = 0
first_flash = None
last_flash = None
compile_blocks = []  # (line, frame, guest_pc_lo, guest_pc_hi, target_bank, target_addr)

# State machine for decoding block header writes
# flash_byte_program writes: first 2 bytes of block header are entry_pc_lo, entry_pc_hi
# We track consecutive flash writes to detect block boundaries
current_block_writes = []
current_block_start_line = 0
current_block_frame = 0

MAX_LINES = 2000000  # Analyze first 2M lines

with open(TRACE, 'r') as f:
    for i, line in enumerate(f):
        if i >= MAX_LINES:
            break
        
        text = line.rstrip()
        
        # Extract frame number
        fm = re.search(r'Fr:(\d+)', text)
        if fm:
            frame_set.add(int(fm.group(1)))
        
        # Check for WRAM execution ($6000+)
        m = re.match(r'\s*([0-9A-Fa-f]{4})\s', text)
        if m:
            pc = int(m.group(1), 16)
            
            # flash_byte_program entry at $6000
            if pc == 0x6000:
                flash_write_count += 1
                frame = int(fm.group(1)) if fm else -1
                if first_flash is None:
                    first_flash = (i, frame)
                last_flash = (i, frame)
            
            # Track the STA that writes the actual byte to flash
            # In flash_byte_program: the final STA is to the target address
            # Look for the STA (r2),Y pattern at $6041
            if pc == 0x6041:
                # Extract target address and value from the effective address
                ea_m = re.search(r'\[\$([0-9A-Fa-f]{4})\]', text)
                val_m = re.search(r'A:([0-9A-Fa-f]{2})', text)
                bank_m = None
                if ea_m and val_m:
                    ea = int(ea_m.group(1), 16)
                    val = int(val_m.group(1), 16)
                    frame = int(fm.group(1)) if fm else -1
                    current_block_writes.append((i, ea, val, frame))
                    if len(current_block_writes) == 1:
                        current_block_start_line = i
                        current_block_frame = frame

# Now decode the flash writes into blocks
print(f"\nTotal flash_byte_program calls ($6000): {flash_write_count}")
print(f"First flash write: line {first_flash[0] if first_flash else 'N/A'}, frame {first_flash[1] if first_flash else 'N/A'}")
print(f"Last flash write: line {last_flash[0] if last_flash else 'N/A'}, frame {last_flash[1] if last_flash else 'N/A'}")
print(f"Frames in trace: {min(frame_set)} to {max(frame_set)} ({len(frame_set)} unique)")

# Group consecutive writes by target address proximity (same sector = same block)
# A block starts when the target address jumps to a new location
blocks = []
current_writes = []
last_addr = -1

for (line_no, ea, val, frame) in current_block_writes:
    if last_addr >= 0 and (ea != last_addr + 1 and ea != last_addr):
        # New block or non-consecutive write
        if len(current_writes) >= 8:  # Min block = 8 byte header
            blocks.append(current_writes)
        current_writes = []
    current_writes.append((line_no, ea, val, frame))
    last_addr = ea

if len(current_writes) >= 8:
    blocks.append(current_writes)

print(f"\n=== Decoded {len(blocks)} blocks ===")

# Extract block info: first 2 bytes = entry_pc, byte[7] = sentinel
block_pcs = []
for bi, writes in enumerate(blocks):
    if len(writes) < 8:
        continue
    entry_pc_lo = writes[0][2]  # val of first write
    entry_pc_hi = writes[1][2]  # val of second write
    exit_pc_lo = writes[2][2]
    exit_pc_hi = writes[3][2]
    code_len = writes[4][2]
    sentinel = writes[7][2] if len(writes) > 7 else 0xFF
    
    entry_pc = entry_pc_lo | (entry_pc_hi << 8)
    exit_pc = exit_pc_lo | (exit_pc_hi << 8)
    
    target_addr = writes[0][1]  # flash address of first byte
    target_bank = (target_addr >> 14) + 4  # approximate bank from address
    
    frame = writes[0][3]
    line = writes[0][0]
    
    block_pcs.append({
        'entry_pc': entry_pc,
        'exit_pc': exit_pc,
        'code_len': code_len,
        'sentinel': sentinel,
        'target_addr': target_addr,
        'frame': frame,
        'line': line,
        'size': len(writes),
    })

# Filter: only blocks with sentinel $AA and valid-looking PCs
real_blocks = [b for b in block_pcs if b['sentinel'] == 0xAA and 0x8000 <= b['entry_pc'] <= 0xFFFF]

# Also include blocks without sentinel that look like they might be ROM data copies
rom_copy_blocks = [b for b in block_pcs if b['sentinel'] != 0xAA]

print(f"\nReal compiled blocks (sentinel=$AA): {len(real_blocks)}")
print(f"Other blocks (ROM copies/incomplete): {len(rom_copy_blocks)}")

# Count unique vs duplicate PCs
pc_counts = Counter(b['entry_pc'] for b in real_blocks)
unique_pcs = len(pc_counts)
duplicates = sum(1 for b in real_blocks if pc_counts[b['entry_pc']] > 1)
total_dup_compiles = sum(c - 1 for c in pc_counts.values() if c > 1)

print(f"\nUnique guest PCs compiled: {unique_pcs}")
print(f"Total duplicate compilations: {total_dup_compiles}")
print(f"Duplicate rate: {total_dup_compiles}/{len(real_blocks)} = {100*total_dup_compiles/max(1,len(real_blocks)):.0f}%")

# Top compiled PCs
print(f"\n=== Top 30 most-compiled guest PCs ===")
for pc_val, count in pc_counts.most_common(30):
    # Find first and last frame for this PC
    frames = [b['frame'] for b in real_blocks if b['entry_pc'] == pc_val]
    print(f"  ${pc_val:04X}: {count}x  (frames {min(frames)}-{max(frames)})")

# Timeline: compilations per frame
frame_compiles = Counter(b['frame'] for b in real_blocks)
print(f"\n=== Compilations per frame (top 20) ===")
for frame, count in frame_compiles.most_common(20):
    pcs = [f"${b['entry_pc']:04X}" for b in real_blocks if b['frame'] == frame]
    print(f"  Frame {frame}: {count} blocks  {' '.join(pcs[:10])}")

# Show first 20 blocks in order
print(f"\n=== First 30 compiled blocks ===")
for bi, b in enumerate(real_blocks[:30]):
    dup = "DUP" if pc_counts[b['entry_pc']] > 1 else "   "
    print(f"  #{bi:3d} L{b['line']:>8d} Fr:{b['frame']:5d} ${b['entry_pc']:04X}-${b['exit_pc']:04X} len={b['code_len']:3d} @${b['target_addr']:04X} {dup}")
