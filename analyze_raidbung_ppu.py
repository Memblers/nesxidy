#!/usr/bin/env python3
"""Focused analysis: sprite-0 hit, controller reads, guest NMI handler behavior."""
import sys, re

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_raidbung.txt"

def analyze_ppu_and_io():
    print("=== PPU & IO Analysis for Raid on Bungeling Bay ===\n")
    
    # Track specific patterns
    bvc_lines = []       # BVC instructions (sprite-0 hit wait)
    bvs_lines = []       # BVS instructions
    bit_2002 = []        # BIT $2002 reads (PPU status)
    write_2000 = []      # Writes to $2000 (PPUCTRL)
    write_4016 = []      # Writes to $4016 (controller strobe)
    read_4016 = []       # Reads from $4016 (controller data)
    write_4014 = []      # OAM DMA triggers
    guest_nmi_entry = [] # Guest NMI handler entry (PC=$C04C)
    jmp_indirect = []    # JMP ($xxxx) instructions
    
    # Track the longest stretch of BVC/BVS loops
    consecutive_bvc_count = 0
    max_bvc_streak = 0
    bvc_streak_start = 0
    current_streak_start = 0
    
    max_lines = 2000000
    
    with open(TRACE, 'r', encoding='utf-8', errors='replace') as f:
        prev_was_bit2002 = False
        for i, line in enumerate(f):
            if i >= max_lines:
                break
            
            m = re.match(r'^([0-9A-Fa-f]{4})\s+(.+?)(?:\s+A:|$)', line)
            if not m:
                continue
            
            pc = int(m.group(1), 16)
            instr = m.group(2).strip()
            
            # BVC/BVS (sprite-0 hit check)
            if 'BVC' in instr:
                bvc_lines.append((i, pc, instr))
                consecutive_bvc_count += 1
                if consecutive_bvc_count == 1:
                    current_streak_start = i
            elif 'BVS' in instr:
                bvs_lines.append((i, pc, instr))
                consecutive_bvc_count += 1
                if consecutive_bvc_count == 1:
                    current_streak_start = i
            else:
                if consecutive_bvc_count > max_bvc_streak:
                    max_bvc_streak = consecutive_bvc_count
                    bvc_streak_start = current_streak_start
                if 'BIT' not in instr:  # BIT $2002 is part of the sprite-0 check
                    consecutive_bvc_count = 0
            
            # BIT $2002
            if 'BIT' in instr and '2002' in instr:
                bit_2002.append((i, pc, instr, line.strip()[:150]))
                prev_was_bit2002 = True
                continue
            else:
                prev_was_bit2002 = False
            
            # Writes to $2000
            if ('STA' in instr or 'STX' in instr) and '2000' in instr:
                a_match = re.search(r'A:([0-9A-Fa-f]{2})', line)
                a_val = int(a_match.group(1), 16) if a_match else -1
                write_2000.append((i, pc, instr, a_val, line.strip()[:150]))
            
            # Controller strobe/read
            if '4016' in instr:
                if 'STA' in instr or 'STX' in instr:
                    write_4016.append((i, pc, instr))
                elif 'LDA' in instr or 'LDX' in instr:
                    read_4016.append((i, pc, instr))
            
            # OAM DMA
            if '4014' in instr and ('STA' in instr or 'STX' in instr):
                write_4014.append((i, pc, instr))
            
            # JMP indirect
            if instr.startswith('JMP') and '(' in instr:
                jmp_indirect.append((i, pc, instr, line.strip()[:150]))
    
    print(f"=== BIT $2002 reads: {len(bit_2002)} ===")
    # Show first few and categorize by what follows (BPL=vblank, BVC=sprite0)
    for line_no, pc, instr, full in bit_2002[:5]:
        print(f"  Line {line_no:>8}: PC=${pc:04X}  {full[:120]}")
    if len(bit_2002) > 5:
        print(f"  ... ({len(bit_2002)} total)")
    
    print(f"\n=== BVC instructions (sprite-0 wait): {len(bvc_lines)} ===")
    if bvc_lines:
        for line_no, pc, instr in bvc_lines[:20]:
            print(f"  Line {line_no:>8}: PC=${pc:04X}  {instr}")
        if len(bvc_lines) > 20:
            print(f"  ... ({len(bvc_lines)} total)")
        print(f"  Longest consecutive BVC/BIT streak: {max_bvc_streak} at line {bvc_streak_start}")
    
    print(f"\n=== BVS instructions: {len(bvs_lines)} ===")
    if bvs_lines:
        for line_no, pc, instr in bvs_lines[:10]:
            print(f"  Line {line_no:>8}: PC=${pc:04X}  {instr}")
    
    print(f"\n=== Writes to $2000 (PPUCTRL): {len(write_2000)} ===")
    for line_no, pc, instr, a_val, full in write_2000[:20]:
        nmi_enable = "NMI_ON" if (a_val & 0x80) else "NMI_OFF"
        print(f"  Line {line_no:>8}: PC=${pc:04X}  {instr}  A=${a_val:02X} [{nmi_enable}]")
    if len(write_2000) > 20:
        print(f"  ... ({len(write_2000)} total)")
    
    print(f"\n=== Controller strobe ($4016 writes): {len(write_4016)} ===")
    for line_no, pc, instr in write_4016[:10]:
        print(f"  Line {line_no:>8}: PC=${pc:04X}  {instr}")
    
    print(f"\n=== Controller reads ($4016 reads): {len(read_4016)} ===")
    for line_no, pc, instr in read_4016[:10]:
        print(f"  Line {line_no:>8}: PC=${pc:04X}  {instr}")
    
    print(f"\n=== OAM DMA ($4014 writes): {len(write_4014)} ===")
    for line_no, pc, instr in write_4014[:20]:
        print(f"  Line {line_no:>8}: PC=${pc:04X}  {instr}")
    
    print(f"\n=== JMP indirect: {len(jmp_indirect)} ===")
    for line_no, pc, instr, full in jmp_indirect[:20]:
        print(f"  Line {line_no:>8}: PC=${pc:04X}  {full[:120]}")


if __name__ == '__main__':
    analyze_ppu_and_io()
