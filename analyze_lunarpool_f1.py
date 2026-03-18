#!/usr/bin/env python3
"""Analyze Lunar Pool NES guest ROM: RESET handler and PPUMASK shadow ($F1) writes."""

import struct
import sys

NES_FILE = r"nes_lunarpool.nes"

# Guest PRG bank 20, 16KB, NROM-128 mirrored
BANK_NUM = 20
BANK_SIZE = 16384
FILE_HEADER = 16
BANK_FILE_OFFSET = FILE_HEADER + BANK_NUM * BANK_SIZE  # 327696
GUEST_BASE = 0xC000  # guest $C000 maps to bank offset 0

def guest_to_file(addr):
    return BANK_FILE_OFFSET + (addr - GUEST_BASE)

def file_to_guest(foff):
    return GUEST_BASE + (foff - BANK_FILE_OFFSET)

# 6502 instruction table: (mnemonic, addressing_mode, size)
# Addressing modes: imp, acc, imm, zp, zpx, zpy, abs, abx, aby, ind, izx, izy, rel
OPCODES = {}

def _add(op, mnem, mode, size):
    OPCODES[op] = (mnem, mode, size)

# Build opcode table (common 6502 instructions)
_instrs = [
    (0x00, "BRK", "imp", 1), (0x01, "ORA", "izx", 2), (0x05, "ORA", "zp", 2),
    (0x06, "ASL", "zp", 2), (0x08, "PHP", "imp", 1), (0x09, "ORA", "imm", 2),
    (0x0A, "ASL", "acc", 1), (0x0D, "ORA", "abs", 3), (0x0E, "ASL", "abs", 3),
    (0x10, "BPL", "rel", 2), (0x11, "ORA", "izy", 2), (0x15, "ORA", "zpx", 2),
    (0x16, "ASL", "zpx", 2), (0x18, "CLC", "imp", 1), (0x19, "ORA", "aby", 3),
    (0x1D, "ORA", "abx", 3), (0x1E, "ASL", "abx", 3),
    (0x20, "JSR", "abs", 3), (0x21, "AND", "izx", 2), (0x24, "BIT", "zp", 2),
    (0x25, "AND", "zp", 2), (0x26, "ROL", "zp", 2), (0x28, "PLP", "imp", 1),
    (0x29, "AND", "imm", 2), (0x2A, "ROL", "acc", 1), (0x2C, "BIT", "abs", 3),
    (0x2D, "AND", "abs", 3), (0x2E, "ROL", "abs", 3),
    (0x30, "BMI", "rel", 2), (0x31, "AND", "izy", 2), (0x35, "AND", "zpx", 2),
    (0x36, "ROL", "zpx", 2), (0x38, "SEC", "imp", 1), (0x39, "AND", "aby", 3),
    (0x3D, "AND", "abx", 3), (0x3E, "ROL", "abx", 3),
    (0x40, "RTI", "imp", 1), (0x41, "EOR", "izx", 2), (0x45, "EOR", "zp", 2),
    (0x46, "LSR", "zp", 2), (0x48, "PHA", "imp", 1), (0x49, "EOR", "imm", 2),
    (0x4A, "LSR", "acc", 1), (0x4C, "JMP", "abs", 3), (0x4D, "EOR", "abs", 3),
    (0x4E, "LSR", "abs", 3),
    (0x50, "BVC", "rel", 2), (0x51, "EOR", "izy", 2), (0x55, "EOR", "zpx", 2),
    (0x56, "LSR", "zpx", 2), (0x58, "CLI", "imp", 1), (0x59, "EOR", "aby", 3),
    (0x5D, "EOR", "abx", 3), (0x5E, "LSR", "abx", 3),
    (0x60, "RTS", "imp", 1), (0x61, "ADC", "izx", 2), (0x65, "ADC", "zp", 2),
    (0x66, "ROR", "zp", 2), (0x68, "PLA", "imp", 1), (0x69, "ADC", "imm", 2),
    (0x6A, "ROR", "acc", 1), (0x6C, "JMP", "ind", 3), (0x6D, "ADC", "abs", 3),
    (0x6E, "ROR", "abs", 3),
    (0x70, "BVS", "rel", 2), (0x71, "ADC", "izy", 2), (0x75, "ADC", "zpx", 2),
    (0x76, "ROR", "zpx", 2), (0x78, "SEI", "imp", 1), (0x79, "ADC", "aby", 3),
    (0x7D, "ADC", "abx", 3), (0x7E, "ROR", "abx", 3),
    (0x81, "STA", "izx", 2), (0x84, "STY", "zp", 2), (0x85, "STA", "zp", 2),
    (0x86, "STX", "zp", 2), (0x88, "DEY", "imp", 1), (0x8A, "TXA", "imp", 1),
    (0x8C, "STY", "abs", 3), (0x8D, "STA", "abs", 3), (0x8E, "STX", "abs", 3),
    (0x90, "BCC", "rel", 2), (0x91, "STA", "izy", 2), (0x94, "STY", "zpx", 2),
    (0x95, "STA", "zpx", 2), (0x96, "STX", "zpy", 2), (0x98, "TYA", "imp", 1),
    (0x99, "STA", "aby", 3), (0x9A, "TXS", "imp", 1), (0x9D, "STA", "abx", 3),
    (0xA0, "LDY", "imm", 2), (0xA1, "LDA", "izx", 2), (0xA2, "LDX", "imm", 2),
    (0xA4, "LDY", "zp", 2), (0xA5, "LDA", "zp", 2), (0xA6, "LDX", "zp", 2),
    (0xA8, "TAY", "imp", 1), (0xA9, "LDA", "imm", 2), (0xAA, "TAX", "imp", 1),
    (0xAC, "LDY", "abs", 3), (0xAD, "LDA", "abs", 3), (0xAE, "LDX", "abs", 3),
    (0xB0, "BCS", "rel", 2), (0xB1, "LDA", "izy", 2), (0xB4, "LDY", "zpx", 2),
    (0xB5, "LDA", "zpx", 2), (0xB6, "LDX", "zpy", 2), (0xB8, "CLV", "imp", 1),
    (0xB9, "LDA", "aby", 3), (0xBA, "TSX", "imp", 1), (0xBC, "LDY", "abx", 3),
    (0xBD, "LDA", "abx", 3), (0xBE, "LDX", "aby", 3),
    (0xC0, "CPY", "imm", 2), (0xC1, "CMP", "izx", 2), (0xC4, "CPY", "zp", 2),
    (0xC5, "CMP", "zp", 2), (0xC6, "DEC", "zp", 2), (0xC8, "INY", "imp", 1),
    (0xC9, "CMP", "imm", 2), (0xCA, "DEX", "imp", 1), (0xCC, "CPY", "abs", 3),
    (0xCD, "CMP", "abs", 3), (0xCE, "DEC", "abs", 3),
    (0xD0, "BNE", "rel", 2), (0xD1, "CMP", "izy", 2), (0xD5, "CMP", "zpx", 2),
    (0xD6, "DEC", "zpx", 2), (0xD8, "CLD", "imp", 1), (0xD9, "CMP", "aby", 3),
    (0xDD, "CMP", "abx", 3), (0xDE, "DEC", "abx", 3),
    (0xE0, "CPX", "imm", 2), (0xE1, "SBC", "izx", 2), (0xE4, "CPX", "zp", 2),
    (0xE5, "SBC", "zp", 2), (0xE6, "INC", "zp", 2), (0xE8, "INX", "imp", 1),
    (0xE9, "SBC", "imm", 2), (0xEA, "NOP", "imp", 1), (0xEC, "CPX", "abs", 3),
    (0xED, "SBC", "abs", 3), (0xEE, "INC", "abs", 3),
    (0xF0, "BEQ", "rel", 2), (0xF1, "SBC", "izy", 2), (0xF5, "SBC", "zpx", 2),
    (0xF6, "INC", "zpx", 2), (0xF8, "SED", "imp", 1), (0xF9, "SBC", "aby", 3),
    (0xFD, "SBC", "abx", 3), (0xFE, "INC", "abx", 3),
]
for op, mnem, mode, size in _instrs:
    _add(op, mnem, mode, size)

def disasm_one(data, offset, guest_addr):
    """Disassemble one instruction. Returns (text, size). guest_addr is the actual guest address."""
    if offset >= len(data):
        return ("???", 1)
    op = data[offset]
    if op not in OPCODES:
        return (f".db ${op:02X}", 1)
    mnem, mode, size = OPCODES[op]
    if offset + size > len(data):
        return (f".db ${op:02X}", 1)
    
    addr = guest_addr
    raw = ' '.join(f'{data[offset+i]:02X}' for i in range(size))
    
    if mode == "imp":
        operand = ""
    elif mode == "acc":
        operand = " A"
    elif mode == "imm":
        operand = f" #${data[offset+1]:02X}"
    elif mode == "zp":
        operand = f" ${data[offset+1]:02X}"
    elif mode == "zpx":
        operand = f" ${data[offset+1]:02X},X"
    elif mode == "zpy":
        operand = f" ${data[offset+1]:02X},Y"
    elif mode == "abs":
        val = data[offset+1] | (data[offset+2] << 8)
        operand = f" ${val:04X}"
    elif mode == "abx":
        val = data[offset+1] | (data[offset+2] << 8)
        operand = f" ${val:04X},X"
    elif mode == "aby":
        val = data[offset+1] | (data[offset+2] << 8)
        operand = f" ${val:04X},Y"
    elif mode == "ind":
        val = data[offset+1] | (data[offset+2] << 8)
        operand = f" (${val:04X})"
    elif mode == "izx":
        operand = f" (${data[offset+1]:02X},X)"
    elif mode == "izy":
        operand = f" (${data[offset+1]:02X}),Y"
    elif mode == "rel":
        rel = data[offset+1]
        if rel >= 0x80:
            rel -= 0x100
        target = addr + size + rel
        operand = f" ${target:04X}"
    else:
        operand = ""
    
    return (f"${addr:04X}: {raw:<12s} {mnem}{operand}", size)

def disasm_range(data, bank_offset, count, base_addr):
    """Disassemble 'count' bytes from bank_offset."""
    lines = []
    pos = bank_offset
    end = bank_offset + count
    while pos < end and pos < len(data):
        text, sz = disasm_one(data, pos, base_addr + (pos - bank_offset))
        lines.append(text)
        pos += sz
    return lines

def main():
    with open(NES_FILE, "rb") as f:
        rom = f.read()
    
    print(f"ROM size: {len(rom)} bytes")
    
    # Extract 16KB PRG bank
    prg = rom[BANK_FILE_OFFSET : BANK_FILE_OFFSET + BANK_SIZE]
    print(f"PRG bank 20: file offset {BANK_FILE_OFFSET} (0x{BANK_FILE_OFFSET:X}), {len(prg)} bytes")
    print(f"Guest address range: ${GUEST_BASE:04X}-${GUEST_BASE + BANK_SIZE - 1:04X}")
    
    # Verify RESET vector
    reset_lo = prg[0x3FFC]  # $FFFC - $C000 = $3FFC
    reset_hi = prg[0x3FFD]
    reset_vec = reset_lo | (reset_hi << 8)
    print(f"\nRESET vector at $FFFC: ${reset_vec:04X}")
    
    nmi_lo = prg[0x3FFA]
    nmi_hi = prg[0x3FFB]
    nmi_vec = nmi_lo | (nmi_hi << 8)
    print(f"NMI vector at $FFFA: ${nmi_vec:04X}")
    
    irq_lo = prg[0x3FFE]
    irq_hi = prg[0x3FFF]
    irq_vec = irq_lo | (irq_hi << 8)
    print(f"IRQ vector at $FFFE: ${irq_vec:04X}")
    
    # ========== PART 1: Disassemble RESET handler ==========
    print("\n" + "="*70)
    print("RESET HANDLER at $E80C (256 bytes)")
    print("="*70)
    reset_bank_off = reset_vec - GUEST_BASE
    lines = disasm_range(prg, reset_bank_off, 256, reset_vec)
    for l in lines:
        print(l)
    
    # ========== PART 2: Search for writes to zeropage $F1 ==========
    print("\n" + "="*70)
    print("SEARCHING FOR WRITES TO ZEROPAGE $F1")
    print("="*70)
    
    patterns = [
        (0x85, 0xF1, "STA $F1"),
        (0x86, 0xF1, "STX $F1"),
        (0x84, 0xF1, "STY $F1"),
        (0x95, 0xF1, "STA $F1,X"),  # zpx store
        (0x96, 0xF1, "STX $F1,Y"),  # zpy store
        (0x94, 0xF1, "STY $F1,X"),  # zpx store
    ]
    
    # Also check INC/DEC $F1
    extra_patterns = [
        (0xE6, 0xF1, "INC $F1"),
        (0xC6, 0xF1, "DEC $F1"),
        (0xF6, 0xF1, "INC $F1,X"),
        (0xD6, 0xF1, "DEC $F1,X"),
        (0x46, 0xF1, "LSR $F1"),
        (0x06, 0xF1, "ASL $F1"),
        (0x26, 0xF1, "ROL $F1"),
        (0x66, 0xF1, "ROR $F1"),
    ]
    
    all_patterns = patterns + extra_patterns
    
    hits = []
    for i in range(len(prg) - 1):
        byte0 = prg[i]
        byte1 = prg[i + 1]
        for pat0, pat1, desc in all_patterns:
            if byte0 == pat0 and byte1 == pat1:
                guest_addr = GUEST_BASE + i
                hits.append((guest_addr, i, desc))
    
    print(f"\nFound {len(hits)} writes/modifications to $F1:\n")
    
    for guest_addr, bank_off, desc in hits:
        print(f"\n--- {desc} at ${guest_addr:04X} (bank offset 0x{bank_off:04X}) ---")
        # Show context: 16 bytes before and 16 bytes after
        ctx_start = max(0, bank_off - 16)
        ctx_end = min(len(prg), bank_off + 16)
        ctx_guest = GUEST_BASE + ctx_start
        lines = disasm_range(prg, ctx_start, ctx_end - ctx_start, ctx_guest)
        for l in lines:
            # Mark the target instruction
            if f"${guest_addr:04X}:" in l:
                print(f"  >>> {l}")
            else:
                print(f"      {l}")
    
    # ========== PART 3: Track what values get stored to $F1 ==========
    print("\n" + "="*70)
    print("ANALYSIS: Values written to $F1")
    print("="*70)
    
    # For each STA $F1, look backwards for the most recent LDA
    for guest_addr, bank_off, desc in hits:
        if desc != "STA $F1":
            continue
        # Scan backwards up to 32 bytes for LDA #imm
        print(f"\n  STA $F1 at ${guest_addr:04X}:")
        found_lda = False
        scan_start = max(0, bank_off - 32)
        # Disassemble forward from scan_start to find instructions leading up
        pos = scan_start
        instructions = []
        while pos < bank_off + 2:
            text, sz = disasm_one(prg, pos, GUEST_BASE + pos)
            instructions.append((pos, text, sz))
            pos += sz
        
        # Show last 8 instructions before the STA
        show_instrs = instructions[-10:]
        for ipos, itext, isz in show_instrs:
            marker = ">>>" if ipos == bank_off else "   "
            print(f"    {marker} {itext}")
    
    # ========== PART 4: Search for LDA #xx / STA $F1 pairs ==========
    print("\n" + "="*70)
    print("SEARCHING FOR LDA #imm ... STA $F1 PATTERNS")
    print("="*70)
    
    # Look for LDA #xx followed relatively soon by STA $F1
    for guest_addr, bank_off, desc in hits:
        if desc != "STA $F1":
            continue
        # Check if 2 bytes before is LDA #imm ($A9 xx)
        if bank_off >= 2 and prg[bank_off - 2] == 0xA9:
            val = prg[bank_off - 1]
            print(f"  ${guest_addr-2:04X}: LDA #${val:02X}")
            print(f"  ${guest_addr:04X}: STA $F1    ; $F1 = ${val:02X} = {val:08b}b")
            bits = []
            if val & 0x01: bits.append("Greyscale")
            if val & 0x02: bits.append("ShowBgLeft8")  
            if val & 0x04: bits.append("ShowSprLeft8")
            if val & 0x08: bits.append("ShowBg")
            if val & 0x10: bits.append("ShowSpr")
            if val & 0x20: bits.append("EmpRed")
            if val & 0x40: bits.append("EmpGreen")
            if val & 0x80: bits.append("EmpBlue")
            rendering = "ENABLED" if (val & 0x18) else "DISABLED"
            print(f"         PPUMASK bits: {', '.join(bits) if bits else 'None'}")
            print(f"         Rendering: {rendering}")
            print()

    # ========== PART 5: Check NMI handler for PPUMASK write ==========
    print("\n" + "="*70)
    print("NMI HANDLER (first 128 bytes)")
    print("="*70)
    if nmi_vec >= GUEST_BASE and nmi_vec < GUEST_BASE + BANK_SIZE:
        nmi_off = nmi_vec - GUEST_BASE
        lines = disasm_range(prg, nmi_off, 128, nmi_vec)
        for l in lines:
            print(l)
    else:
        print(f"NMI vector ${nmi_vec:04X} outside PRG bank range")
    
    # ========== PART 6: Search for writes to PPUMASK $2001 ==========
    print("\n" + "="*70)
    print("WRITES TO PPUMASK ($2001)")
    print("="*70)
    
    ppumask_patterns = [
        (0x8D, 0x01, 0x20, "STA $2001"),  # STA $2001
        (0x8E, 0x01, 0x20, "STX $2001"),  # STX $2001
        (0x8C, 0x01, 0x20, "STY $2001"),  # STY $2001
    ]
    
    for i in range(len(prg) - 2):
        for pat0, pat1, pat2, desc in ppumask_patterns:
            if prg[i] == pat0 and prg[i+1] == pat1 and prg[i+2] == pat2:
                guest_addr = GUEST_BASE + i
                print(f"\n--- {desc} at ${guest_addr:04X} ---")
                ctx_start = max(0, i - 16)
                ctx_end = min(len(prg), i + 16)
                ctx_guest = GUEST_BASE + ctx_start
                lines = disasm_range(prg, ctx_start, ctx_end - ctx_start, ctx_guest)
                for l in lines:
                    if f"${guest_addr:04X}:" in l:
                        print(f"  >>> {l}")
                    else:
                        print(f"      {l}")
    
    # ========== PART 7: Also check JSR targets from RESET ==========
    print("\n" + "="*70)
    print("JSR TARGETS FROM RESET HANDLER")
    print("="*70)
    reset_off = reset_vec - GUEST_BASE
    pos = reset_off
    for _ in range(60):  # first ~60 instructions
        if pos >= len(prg):
            break
        op = prg[pos]
        if op not in OPCODES:
            pos += 1
            continue
        mnem, mode, sz = OPCODES[op]
        if mnem == "JSR" and sz == 3:
            target = prg[pos+1] | (prg[pos+2] << 8)
            caller = GUEST_BASE + pos
            print(f"\n  JSR ${target:04X} (called from ${caller:04X})")
            # Disassemble first 48 bytes of target
            if target >= GUEST_BASE and target < GUEST_BASE + BANK_SIZE:
                tgt_off = target - GUEST_BASE
                lines = disasm_range(prg, tgt_off, 48, target)
                for l in lines:
                    print(f"    {l}")
        if mnem in ("RTS", "RTI", "JMP") and mode != "abs":
            break
        if mnem == "JMP":
            # Follow JMP
            target = prg[pos+1] | (prg[pos+2] << 8)
            if target >= GUEST_BASE and target < GUEST_BASE + BANK_SIZE:
                pos = target - GUEST_BASE
                continue
            else:
                break
        pos += sz

if __name__ == "__main__":
    main()
