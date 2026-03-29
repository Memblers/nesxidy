#!/usr/bin/env python3
"""Analyze Raid on Bungeling Bay trace log to understand boot sequence."""
import sys, re

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_raidbung.txt"

def analyze_boot():
    """Find key events in boot: after VBlank waits, bank switches, sub-$C000 accesses."""
    print("=== Raid on Bungeling Bay Boot Trace Analysis ===\n")
    
    vblank_wait_count = 0
    in_vblank_wait = False
    unique_pcs = set()
    writes_to_8000_plus = []
    reads_from_below_c000 = []
    jsr_targets = []
    jmp_targets = []
    interesting_lines = []
    nmi_count = 0
    last_pc = None
    line_count = 0
    max_lines = 200000  # First ~200K lines should cover boot
    
    # Track first N non-vblank-loop instructions
    non_loop_instructions = []
    passed_first_vblank = False
    passed_second_vblank = False
    collecting_boot = True
    boot_lines_collected = 0
    
    with open(TRACE, 'r', encoding='utf-8', errors='replace') as f:
        for i, line in enumerate(f):
            if i >= max_lines:
                break
            line_count = i
            
            # Parse PC address
            m = re.match(r'^([0-9A-Fa-f]{4})\s+(.+?)(?:\s+A:|$)', line)
            if not m:
                continue
            
            pc = int(m.group(1), 16)
            instr = m.group(2).strip()
            
            # Detect VBlank wait loop (BIT $2002 / BPL pattern)
            if 'BIT' in instr and '2002' in instr:
                in_vblank_wait = True
                continue
            if in_vblank_wait and 'BPL' in instr:
                continue
            if in_vblank_wait and 'BPL' not in instr:
                in_vblank_wait = False
                vblank_wait_count += 1
                if vblank_wait_count <= 3:
                    print(f"  VBlank wait #{vblank_wait_count} ended at line {i}, PC=${pc:04X}: {instr}")
            
            # Track unique PCs
            unique_pcs.add(pc)
            
            # Detect writes to $8000+ (bank switching!)
            if 'STA' in instr or 'STX' in instr or 'STY' in instr:
                # Look for writes to $8000+
                addr_match = re.search(r'\$([0-9A-Fa-f]{4})', instr)
                if addr_match:
                    write_addr = int(addr_match.group(1), 16)
                    if write_addr >= 0x8000:
                        writes_to_8000_plus.append((i, pc, instr.strip(), line.strip()[:120]))
            
            # Detect reads from $8000-$BFFF (switchable bank region)
            if pc >= 0x8000 and pc < 0xC000:
                if pc not in [p for _, p, _, _ in reads_from_below_c000[:100]]:
                    reads_from_below_c000.append((i, pc, instr.strip(), ''))
            
            # Detect JSR/JMP targets
            if 'JSR' in instr:
                addr_match = re.search(r'\$([0-9A-Fa-f]{4})', instr)
                if addr_match:
                    target = int(addr_match.group(1), 16)
                    jsr_targets.append((i, pc, target, instr.strip()))
            
            if instr.startswith('JMP') and '(' not in instr:
                addr_match = re.search(r'\$([0-9A-Fa-f]{4})', instr)
                if addr_match:
                    target = int(addr_match.group(1), 16)
                    jmp_targets.append((i, pc, target, instr.strip()))
            
            # Collect first 200 non-loop boot instructions
            if collecting_boot and boot_lines_collected < 500:
                interesting_lines.append((i, pc, instr.strip()))
                boot_lines_collected += 1

    print(f"\n=== Summary (first {line_count} lines) ===")
    print(f"  VBlank waits detected: {vblank_wait_count}")
    print(f"  Unique PC addresses: {len(unique_pcs)}")
    
    # PC range analysis
    pcs_below_8000 = [p for p in unique_pcs if p < 0x8000]
    pcs_8000_bfff = [p for p in unique_pcs if 0x8000 <= p < 0xC000]
    pcs_c000_ffff = [p for p in unique_pcs if 0xC000 <= p <= 0xFFFF]
    print(f"  PCs below $8000: {len(pcs_below_8000)}")
    print(f"  PCs $8000-$BFFF: {len(pcs_8000_bfff)}")
    print(f"  PCs $C000-$FFFF: {len(pcs_c000_ffff)}")
    
    if pcs_below_8000:
        print(f"    Below $8000 PCs: {sorted(['${:04X}'.format(p) for p in pcs_below_8000])[:20]}")
    if pcs_8000_bfff:
        print(f"    $8000-$BFFF PCs (SWITCHABLE BANK!): {sorted(['${:04X}'.format(p) for p in pcs_8000_bfff])[:30]}")
    
    print(f"\n=== Writes to $8000+ (mapper bank switches!) ===")
    if writes_to_8000_plus:
        for line_no, pc, instr, full in writes_to_8000_plus[:30]:
            print(f"  Line {line_no:>8}: PC=${pc:04X}  {instr}")
    else:
        print("  NONE found (game may not be bank-switched, or writes happen later)")
    
    print(f"\n=== Code execution in $8000-$BFFF (switchable bank) ===")
    if reads_from_below_c000:
        print(f"  {len(reads_from_below_c000)} unique PCs in switchable bank region")
        for line_no, pc, instr, _ in reads_from_below_c000[:30]:
            print(f"  Line {line_no:>8}: PC=${pc:04X}  {instr}")
    else:
        print("  NONE - game runs entirely in $C000-$FFFF fixed bank")
    
    print(f"\n=== JSR targets ===")
    unique_jsr = set()
    for line_no, pc, target, instr in jsr_targets[:100]:
        if target not in unique_jsr:
            unique_jsr.add(target)
            bank_region = "FIXED" if target >= 0xC000 else "SWITCHABLE" if target >= 0x8000 else "RAM/IO"
            print(f"  Line {line_no:>8}: PC=${pc:04X}  {instr}  [{bank_region}]")
    
    print(f"\n=== First 100 boot instructions (non-VBlank-loop) ===")
    for line_no, pc, instr in interesting_lines[:100]:
        print(f"  Line {line_no:>8}: ${pc:04X}  {instr}")


if __name__ == '__main__':
    analyze_boot()
