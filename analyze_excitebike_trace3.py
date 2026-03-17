"""
Deep analysis of Excitebike flash cache growth.

From analyze_excitebike_trace2.py results:
- 8,554 flash_byte_program calls across 619 frames
- After frame ~6553, cumulative new addresses plateau at 9,394
  but FBP calls CONTINUE every ~5-15 frames with 0 new addresses
- This means blocks are being recompiled to NEW flash locations
  for the SAME guest PCs = wasting flash space

Goal: identify WHICH guest PCs are triggering repeated compilation
by tracking the _pc ZP variable and the dispatch pattern.
"""

import sys
from collections import Counter, defaultdict

TRACE_FILE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_excitebike.txt"

def analyze():
    # Track what the dispatch code sees.
    # dispatch_on_pc is in the fixed bank ($C000-$FFFF).
    # When it decides to compile, it jumps to WRAM helpers.
    # The key is to find what _pc value is active when we enter compilation.
    #
    # Strategy: When we see flash_byte_program at $6000 (start of compile burst),
    # look BACKWARD at the recent guest PC values.
    # The _pc ZP variable is written by the epilogue: STA _pc (85 xx) / STA _pc+1 (85 xx).
    # Or by the dispatch code before entering compile.
    #
    # Better: track the block header writes. The block header at offset 0,1 = entry_pc lo/hi.
    # These are the first 2 bytes written by flash_byte_program for each new block.
    # But we need to decode which flash writes are header writes vs code writes.
    #
    # Simplest approach: track the flash write target addresses.
    # When a new block starts, the target address jumps to a new aligned position.
    # Header writes go to aligned_addr - 8 (BLOCK_HEADER_SIZE).
    # First 2 bytes of header = entry_pc lo, hi.
    
    flash_write_sequence = []  # (frame, target_addr, data_value)
    
    # Track: at $6041 STA (r2),Y, the A register has the data byte,
    # and the target is in [$xxxx]. Also r2:r3 has the target addr.
    
    print(f"Reading {TRACE_FILE}...")
    
    current_frame = 0
    
    with open(TRACE_FILE, 'r') as f:
        for i, line in enumerate(f):
            # Fast skip: only care about WRAM flash write instruction
            if not line.startswith('60'):  # $60xx = flash_byte_program addresses
                # Parse frame from other lines
                fr_idx = line.find('Fr:')
                if fr_idx >= 0:
                    try:
                        fr_str = ''
                        for c in line[fr_idx+3:fr_idx+12]:
                            if c.isdigit(): fr_str += c
                            else: break
                        if fr_str:
                            current_frame = int(fr_str)
                    except:
                        pass
                continue
            
            parts = line.split()
            if len(parts) < 2:
                continue
            try:
                addr = int(parts[0], 16)
            except ValueError:
                continue
            
            # Parse frame
            fr_idx = line.find('Fr:')
            if fr_idx >= 0:
                try:
                    fr_str = ''
                    for c in line[fr_idx+3:fr_idx+12]:
                        if c.isdigit(): fr_str += c
                        else: break
                    if fr_str:
                        current_frame = int(fr_str)
                except:
                    pass
            
            # The actual flash write: STA (r2),Y at $6041
            if addr == 0x6041:
                # Get target from [$xxxx]
                bracket_idx = line.find('[$')
                if bracket_idx < 0:
                    continue
                try:
                    target = int(line[bracket_idx+2:bracket_idx+6], 16)
                except:
                    continue
                
                # Get data value from A register
                a_idx = line.find('A:')
                if a_idx < 0:
                    continue
                try:
                    data = int(line[a_idx+2:a_idx+4], 16)
                except:
                    continue
                
                flash_write_sequence.append((current_frame, target, data))
            
            if i % 1000000 == 0:
                print(f"  {i:,} lines, frame {current_frame}, "
                      f"writes: {len(flash_write_sequence):,}")
    
    print(f"\nDone. {len(flash_write_sequence):,} flash writes captured.\n")
    
    # === Decode block compilations from the write sequence ===
    # Block header format:
    #   +0: entry_pc_lo  +1: entry_pc_hi  +2: exit_pc_lo  +3: exit_pc_hi
    #   +4: code_len  +5: epilogue_offset  +6: flags  +7: sentinel ($AA)
    # Code starts at header + 8, aligned to 16 bytes.
    #
    # Detection: look for writes where target address is 16-byte aligned - 8
    # (header start). The sentinel byte $AA at offset +7 confirms block completion.
    
    blocks = []  # (frame, entry_pc, exit_pc, code_len, flash_addr, sector)
    
    # Group writes by contiguous target address ranges
    prev_target = 0
    current_block_writes = []
    
    for frame, target, data in flash_write_sequence:
        # Detect block boundary: big jump in target address
        if current_block_writes and (target < prev_target or target > prev_target + 4):
            # Process the completed block
            block = decode_block(current_block_writes)
            if block:
                blocks.append(block)
            current_block_writes = []
        current_block_writes.append((frame, target, data))
        prev_target = target
    
    if current_block_writes:
        block = decode_block(current_block_writes)
        if block:
            blocks.append(block)
    
    print(f"Decoded {len(blocks)} block compilations.\n")
    
    # === Report ===
    print("=== Block Compilations (all) ===")
    print(f"{'#':>3} {'Frame':>6} {'Entry':>6} {'Exit':>6} {'Len':>4} {'FlAddr':>7} {'Sector':>6}")
    
    entry_pc_count = Counter()
    entry_pc_first_frame = {}
    
    for i, (frame, entry_pc, exit_pc, code_len, flash_addr, sector) in enumerate(blocks):
        entry_str = f"${entry_pc:04X}" if entry_pc else "????"
        exit_str = f"${exit_pc:04X}" if exit_pc else "????"
        flash_str = f"${flash_addr:04X}" if flash_addr else "????"
        print(f"{i:>3} {frame:>6} {entry_str:>6} {exit_str:>6} {code_len:>4} {flash_str:>7} {sector:>6}")
        
        if entry_pc:
            entry_pc_count[entry_pc] += 1
            if entry_pc not in entry_pc_first_frame:
                entry_pc_first_frame[entry_pc] = frame
    
    # === Duplicate analysis ===
    print(f"\n=== Guest PCs Compiled Multiple Times ===")
    duplicates = [(pc, cnt) for pc, cnt in entry_pc_count.items() if cnt > 1]
    duplicates.sort(key=lambda x: -x[1])
    
    print(f"{'GuestPC':>8} {'Count':>5} {'FirstFr':>7}")
    for pc, cnt in duplicates:
        print(f"  ${pc:04X}   {cnt:>5}   {entry_pc_first_frame[pc]:>7}")
    
    # === Unique guest PCs ===
    print(f"\n=== Summary ===")
    print(f"Total block compilations: {len(blocks)}")
    print(f"Unique guest PCs compiled: {len(entry_pc_count)}")
    print(f"Guest PCs compiled more than once: {len(duplicates)}")
    if duplicates:
        total_wasted = sum(cnt - 1 for _, cnt in duplicates)
        print(f"Total wasted compilations (duplicates): {total_wasted}")
    
    # === Flash space consumption by sector ===
    print(f"\n=== Flash Writes by Sector ===")
    sector_writes = Counter()
    for frame, target, data in flash_write_sequence:
        bank = (target - 0x8000) // 0x4000
        sect_in_bank = ((target - 0x8000) % 0x4000) // 0x1000
        sector = bank * 4 + sect_in_bank
        sector_writes[sector] += 1
    
    for sector in sorted(sector_writes.keys()):
        bank = sector // 4
        sect = sector % 4
        print(f"  Sector {sector:>2} (bank {bank} sect {sect}): {sector_writes[sector]:>5} bytes")
    
    # === Timeline: blocks compiled per 10-frame window ===
    print(f"\n=== Compilation Rate (10-frame windows) ===")
    if blocks:
        min_frame = min(b[0] for b in blocks)
        max_frame = max(b[0] for b in blocks)
        for window_start in range(min_frame, max_frame + 1, 10):
            window_blocks = [b for b in blocks if window_start <= b[0] < window_start + 10]
            if window_blocks:
                entry_pcs = [f"${b[1]:04X}" for b in window_blocks if b[1]]
                print(f"  Fr {window_start}-{window_start+9}: {len(window_blocks)} blocks: {', '.join(entry_pcs[:8])}")


def decode_block(writes):
    """Decode a block from its flash write sequence."""
    if len(writes) < 8:
        return None
    
    frame = writes[0][0]
    base_addr = writes[0][1]
    
    # Build a map of offset -> data
    data_map = {}
    for f, target, data in writes:
        offset = target - base_addr
        data_map[offset] = data
    
    # Read header fields
    entry_pc_lo = data_map.get(0, None)
    entry_pc_hi = data_map.get(1, None)
    exit_pc_lo = data_map.get(2, None)
    exit_pc_hi = data_map.get(3, None)
    code_len = data_map.get(4, None)
    epilogue_off = data_map.get(5, None)
    sentinel = data_map.get(7, None)
    
    entry_pc = None
    if entry_pc_lo is not None and entry_pc_hi is not None:
        entry_pc = entry_pc_lo | (entry_pc_hi << 8)
    
    exit_pc = None
    if exit_pc_lo is not None and exit_pc_hi is not None:
        exit_pc = exit_pc_lo | (exit_pc_hi << 8)
    
    # Compute sector
    bank = (base_addr - 0x8000) // 0x4000
    sect_in_bank = ((base_addr - 0x8000) % 0x4000) // 0x1000
    sector = bank * 4 + sect_in_bank
    
    return (frame, entry_pc, exit_pc, code_len or len(writes), base_addr, sector)


if __name__ == "__main__":
    analyze()
