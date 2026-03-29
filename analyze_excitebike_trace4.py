"""
Fixed block analysis - filter out sentinel writes and ROM data copies.
Sentinel: single write of $AA to header_base+7, written AFTER the code.
"""

import sys
from collections import Counter, defaultdict

TRACE_FILE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_excitebike.txt"

def analyze():
    flash_writes = []
    current_frame = 0
    
    print(f"Reading {TRACE_FILE}...")
    with open(TRACE_FILE, "r") as f:
        for i, line in enumerate(f):
            if not line.startswith("60"):
                fr_idx = line.find("Fr:")
                if fr_idx >= 0:
                    try:
                        fr_str = ""
                        for c in line[fr_idx+3:fr_idx+12]:
                            if c.isdigit(): fr_str += c
                            else: break
                        if fr_str: current_frame = int(fr_str)
                    except: pass
                continue
            parts = line.split()
            if not parts: continue
            try: addr = int(parts[0], 16)
            except: continue
            fr_idx = line.find("Fr:")
            if fr_idx >= 0:
                try:
                    fr_str = ""
                    for c in line[fr_idx+3:fr_idx+12]:
                        if c.isdigit(): fr_str += c
                        else: break
                    if fr_str: current_frame = int(fr_str)
                except: pass
            if addr == 0x6041:
                bracket_idx = line.find("[$")
                if bracket_idx < 0: continue
                try: target = int(line[bracket_idx+2:bracket_idx+6], 16)
                except: continue
                a_idx = line.find("A:")
                try: data = int(line[a_idx+2:a_idx+4], 16)
                except: continue
                flash_writes.append((current_frame, target, data))
            if i % 1000000 == 0:
                print(f"  {i:,} lines, {len(flash_writes)} writes...")
    
    print(f"\nTotal flash writes: {len(flash_writes)}")
    
    # Split into contiguous write groups
    groups = []
    current_group = []
    prev = 0
    for fw in flash_writes:
        frame, target, data = fw
        if current_group and (target < prev or target > prev + 4):
            groups.append(current_group)
            current_group = []
        current_group.append(fw)
        prev = target
    if current_group:
        groups.append(current_group)
    
    # Classify groups: sentinel (1 byte, value $AA) vs block vs data copy
    blocks = []
    sentinels = 0
    data_copies = 0
    
    for group in groups:
        if len(group) == 1 and group[0][2] == 0xAA:
            sentinels += 1
            continue
        if len(group) >= 200:
            data_copies += 1
            continue
        
        # Decode block header
        frame = group[0][0]
        base_addr = group[0][1]
        data_map = {}
        for f, target, data in group:
            data_map[target - base_addr] = data
        
        entry_pc_lo = data_map.get(0)
        entry_pc_hi = data_map.get(1)
        exit_pc_lo = data_map.get(2)
        exit_pc_hi = data_map.get(3)
        code_len_hdr = data_map.get(4)
        epilogue_off = data_map.get(5)
        
        entry_pc = None
        if entry_pc_lo is not None and entry_pc_hi is not None:
            entry_pc = entry_pc_lo | (entry_pc_hi << 8)
        
        exit_pc = None
        if exit_pc_lo is not None and exit_pc_hi is not None:
            exit_pc = exit_pc_lo | (exit_pc_hi << 8)
        
        bank = (base_addr - 0x8000) // 0x4000
        sect_in_bank = ((base_addr - 0x8000) % 0x4000) // 0x1000
        sector = bank * 4 + sect_in_bank
        
        blocks.append({
            "frame": frame,
            "entry_pc": entry_pc,
            "exit_pc": exit_pc,
            "code_len": code_len_hdr,
            "epilogue_off": epilogue_off,
            "flash_addr": base_addr,
            "sector": sector,
            "write_count": len(group),
            "total_bytes": max(t - base_addr for _, t, _ in group) + 1 if group else 0,
        })
    
    print(f"\nGroups: {len(groups)} total")
    print(f"  Real blocks: {len(blocks)}")
    print(f"  Sentinels: {sentinels}")
    print(f"  Data copies (>=200 bytes): {data_copies}")
    
    # Report blocks
    print(f"\n{'#':>3} {'Frame':>6} {'Entry':>7} {'Exit':>7} {'CLen':>4} {'Epi':>4} {'Flash':>7} {'Writes':>6} {'Span':>5}")
    
    entry_pc_count = Counter()
    entry_pc_first_frame = {}
    
    for i, b in enumerate(blocks):
        ep = f"${b['entry_pc']:04X}" if b['entry_pc'] else "????"
        xp = f"${b['exit_pc']:04X}" if b['exit_pc'] else "????"
        cl = f"{b['code_len']:>4}" if b['code_len'] is not None else "  ??"
        eo = f"{b['epilogue_off']:>4}" if b['epilogue_off'] is not None else "  ??"
        print(f"{i:>3} {b['frame']:>6} {ep:>7} {xp:>7} {cl} {eo} ${b['flash_addr']:04X} {b['write_count']:>6} {b['total_bytes']:>5}")
        
        if b['entry_pc']:
            entry_pc_count[b['entry_pc']] += 1
            if b['entry_pc'] not in entry_pc_first_frame:
                entry_pc_first_frame[b['entry_pc']] = b['frame']
    
    # Duplicate analysis
    print(f"\n=== Guest PCs Compiled Multiple Times ===")
    duplicates = [(pc, cnt) for pc, cnt in entry_pc_count.items() if cnt > 1]
    duplicates.sort(key=lambda x: -x[1])
    
    if duplicates:
        print(f"{'GuestPC':>8} {'Count':>5} {'FirstFr':>7}")
        for pc, cnt in duplicates:
            print(f"  ${pc:04X}   {cnt:>5}   {entry_pc_first_frame[pc]:>7}")
        total_wasted = sum(cnt - 1 for _, cnt in duplicates)
        print(f"\nTotal block compilations: {len(blocks)}")
        print(f"Unique guest PCs: {len(entry_pc_count)}")
        print(f"Duplicates: {total_wasted}")
    else:
        print("No duplicates found.")
    
    # Flash space consumed
    total_flash_bytes = sum(b['total_bytes'] for b in blocks)
    print(f"\nTotal flash bytes consumed by blocks: {total_flash_bytes:,}")
    print(f"Average block size: {total_flash_bytes / len(blocks):.0f} bytes")
    
    # Show the repeating pattern
    print(f"\n=== Compilation Timeline ===")
    for i, b in enumerate(blocks):
        ep = f"${b['entry_pc']:04X}" if b['entry_pc'] else "????"
        dup_num = sum(1 for b2 in blocks[:i] if b2['entry_pc'] == b['entry_pc'])
        dup_str = f" (dup #{dup_num+1})" if dup_num > 0 else " (FIRST)"
        print(f"  Fr {b['frame']:>6}: {ep}{dup_str}")


if __name__ == "__main__":
    analyze()
