#!/usr/bin/env python3
"""Disassemble key areas of the Lunar Pool NES PRG ROM."""

import sys

# Complete 6502 opcode table: opcode -> (mnemonic, mode, total_bytes)
OPCODES = {
    0x00: ('BRK','imp',1), 0x01: ('ORA','izx',2), 0x05: ('ORA','zp',2),
    0x06: ('ASL','zp',2), 0x08: ('PHP','imp',1), 0x09: ('ORA','imm',2),
    0x0A: ('ASL','acc',1), 0x0D: ('ORA','abs',3), 0x0E: ('ASL','abs',3),
    0x10: ('BPL','rel',2), 0x11: ('ORA','izy',2), 0x15: ('ORA','zpx',2),
    0x16: ('ASL','zpx',2), 0x18: ('CLC','imp',1), 0x19: ('ORA','aby',3),
    0x1D: ('ORA','abx',3), 0x1E: ('ASL','abx',3),
    0x20: ('JSR','abs',3), 0x21: ('AND','izx',2), 0x24: ('BIT','zp',2),
    0x25: ('AND','zp',2), 0x26: ('ROL','zp',2), 0x28: ('PLP','imp',1),
    0x29: ('AND','imm',2), 0x2A: ('ROL','acc',1), 0x2C: ('BIT','abs',3),
    0x2D: ('AND','abs',3), 0x2E: ('ROL','abs',3),
    0x30: ('BMI','rel',2), 0x31: ('AND','izy',2), 0x35: ('AND','zpx',2),
    0x36: ('ROL','zpx',2), 0x38: ('SEC','imp',1), 0x39: ('AND','aby',3),
    0x3D: ('AND','abx',3), 0x3E: ('ROL','abx',3),
    0x40: ('RTI','imp',1), 0x41: ('EOR','izx',2), 0x45: ('EOR','zp',2),
    0x46: ('LSR','zp',2), 0x48: ('PHA','imp',1), 0x49: ('EOR','imm',2),
    0x4A: ('LSR','acc',1), 0x4C: ('JMP','abs',3), 0x4D: ('EOR','abs',3),
    0x4E: ('LSR','abs',3),
    0x50: ('BVC','rel',2), 0x51: ('EOR','izy',2), 0x55: ('EOR','zpx',2),
    0x56: ('LSR','zpx',2), 0x58: ('CLI','imp',1), 0x59: ('EOR','aby',3),
    0x5D: ('EOR','abx',3), 0x5E: ('LSR','abx',3),
    0x60: ('RTS','imp',1), 0x61: ('ADC','izx',2), 0x65: ('ADC','zp',2),
    0x66: ('ROR','zp',2), 0x68: ('PLA','imp',1), 0x69: ('ADC','imm',2),
    0x6A: ('ROR','acc',1), 0x6C: ('JMP','ind',3), 0x6D: ('ADC','abs',3),
    0x6E: ('ROR','abs',3),
    0x70: ('BVS','rel',2), 0x71: ('ADC','izy',2), 0x75: ('ADC','zpx',2),
    0x76: ('ROR','zpx',2), 0x78: ('SEI','imp',1), 0x79: ('ADC','aby',3),
    0x7D: ('ADC','abx',3), 0x7E: ('ROR','abx',3),
    0x81: ('STA','izx',2), 0x84: ('STY','zp',2), 0x85: ('STA','zp',2),
    0x86: ('STX','zp',2), 0x88: ('DEY','imp',1), 0x8A: ('TXA','imp',1),
    0x8C: ('STY','abs',3), 0x8D: ('STA','abs',3), 0x8E: ('STX','abs',3),
    0x90: ('BCC','rel',2), 0x91: ('STA','izy',2), 0x94: ('STY','zpx',2),
    0x95: ('STA','zpx',2), 0x96: ('STX','zpy',2), 0x98: ('TYA','imp',1),
    0x99: ('STA','aby',3), 0x9A: ('TXS','imp',1), 0x9D: ('STA','abx',3),
    0xA0: ('LDY','imm',2), 0xA1: ('LDA','izx',2), 0xA2: ('LDX','imm',2),
    0xA4: ('LDY','zp',2), 0xA5: ('LDA','zp',2), 0xA6: ('LDX','zp',2),
    0xA8: ('TAY','imp',1), 0xA9: ('LDA','imm',2), 0xAA: ('TAX','imp',1),
    0xAC: ('LDY','abs',3), 0xAD: ('LDA','abs',3), 0xAE: ('LDX','abs',3),
    0xB0: ('BCS','rel',2), 0xB1: ('LDA','izy',2), 0xB4: ('LDY','zpx',2),
    0xB5: ('LDA','zpx',2), 0xB6: ('LDX','zpy',2), 0xB8: ('CLV','imp',1),
    0xB9: ('LDA','aby',3), 0xBA: ('TSX','imp',1), 0xBC: ('LDY','abx',3),
    0xBD: ('LDA','abx',3), 0xBE: ('LDX','aby',3),
    0xC0: ('CPY','imm',2), 0xC1: ('CMP','izx',2), 0xC4: ('CPY','zp',2),
    0xC5: ('CMP','zp',2), 0xC6: ('DEC','zp',2), 0xC8: ('INY','imp',1),
    0xC9: ('CMP','imm',2), 0xCA: ('DEX','imp',1), 0xCC: ('CPY','abs',3),
    0xCD: ('CMP','abs',3), 0xCE: ('DEC','abs',3),
    0xD0: ('BNE','rel',2), 0xD1: ('CMP','izy',2), 0xD5: ('CMP','zpx',2),
    0xD6: ('DEC','zpx',2), 0xD8: ('CLD','imp',1), 0xD9: ('CMP','aby',3),
    0xDD: ('CMP','abx',3), 0xDE: ('DEC','abx',3),
    0xE0: ('CPX','imm',2), 0xE1: ('SBC','izx',2), 0xE4: ('CPX','zp',2),
    0xE5: ('SBC','zp',2), 0xE6: ('INC','zp',2), 0xE8: ('INX','imp',1),
    0xE9: ('SBC','imm',2), 0xEA: ('NOP','imp',1), 0xEC: ('CPX','abs',3),
    0xED: ('SBC','abs',3), 0xEE: ('INC','abs',3),
    0xF0: ('BEQ','rel',2), 0xF1: ('SBC','izy',2), 0xF5: ('SBC','zpx',2),
    0xF6: ('INC','zpx',2), 0xF8: ('SED','imp',1), 0xF9: ('SBC','aby',3),
    0xFD: ('SBC','abx',3), 0xFE: ('INC','abx',3),
}

def prg_to_guest(offset):
    """NROM-128: 16KB mirrored. Maps PRG offset to $C000-based guest address."""
    return 0xC000 + offset

def guest_to_prg(addr):
    """Guest address to PRG offset (mask off to 16KB)."""
    return addr & 0x3FFF

def format_operand(prg, pos, mnem, mode, size, guest_addr):
    """Format the operand string for a given addressing mode."""
    if mode == 'imp':
        return ''
    elif mode == 'acc':
        return 'A'
    elif mode == 'imm':
        return '#$%02X' % prg[pos+1]
    elif mode == 'zp':
        return '$%02X' % prg[pos+1]
    elif mode == 'zpx':
        return '$%02X,X' % prg[pos+1]
    elif mode == 'zpy':
        return '$%02X,Y' % prg[pos+1]
    elif mode == 'abs':
        val = prg[pos+1] | (prg[pos+2] << 8)
        return '$%04X' % val
    elif mode == 'abx':
        val = prg[pos+1] | (prg[pos+2] << 8)
        return '$%04X,X' % val
    elif mode == 'aby':
        val = prg[pos+1] | (prg[pos+2] << 8)
        return '$%04X,Y' % val
    elif mode == 'ind':
        val = prg[pos+1] | (prg[pos+2] << 8)
        return '($%04X)' % val
    elif mode == 'izx':
        return '($%02X,X)' % prg[pos+1]
    elif mode == 'izy':
        return '($%02X),Y' % prg[pos+1]
    elif mode == 'rel':
        offset_val = prg[pos+1]
        if offset_val >= 0x80:
            offset_val -= 256
        target = guest_addr + 2 + offset_val
        return '$%04X' % target
    return '???'

def disasm(prg, prg_offset, count=20, base_addr=None):
    """Disassemble `count` instructions starting at prg_offset."""
    if base_addr is None:
        base_addr = prg_to_guest(prg_offset)
    results = []
    pos = prg_offset
    n = 0
    while n < count and pos < len(prg):
        op = prg[pos]
        addr = base_addr + (pos - prg_offset)
        if op in OPCODES:
            mnem, mode, size = OPCODES[op]
            if pos + size > len(prg):
                break
            raw = prg[pos:pos+size]
            raw_hex = ' '.join('%02X' % b for b in raw)
            operand = format_operand(prg, pos, mnem, mode, size, addr)
            line = '  $%04X: %-10s  %s %s' % (addr, raw_hex, mnem, operand)
            results.append(line)
            pos += size
        else:
            line = '  $%04X: %02X          .db $%02X  ; unknown opcode' % (addr, op, op)
            results.append(line)
            pos += 1
        n += 1
    return results

def main():
    rom_path = r'c:\proj\c\NES\nesxidy-co\nesxidy\roms\nes\lunarpool.prg'
    with open(rom_path, 'rb') as f:
        prg = f.read()

    print('PRG ROM size: %d bytes (0x%04X)' % (len(prg), len(prg)))
    print('NROM-128: 16KB mirrored at $8000-$BFFF and $C000-$FFFF')
    print()

    # =========================================================
    # 1. Read interrupt vectors
    # =========================================================
    nmi_addr   = prg[0x3FFA] | (prg[0x3FFB] << 8)
    reset_addr = prg[0x3FFC] | (prg[0x3FFD] << 8)
    irq_addr   = prg[0x3FFE] | (prg[0x3FFF] << 8)

    print('=' * 64)
    print('INTERRUPT VECTORS (PRG offset $3FFA-$3FFF)')
    print('=' * 64)
    print('  NMI vector:   $%04X  (PRG offset $%04X)' % (nmi_addr, guest_to_prg(nmi_addr)))
    print('  RESET vector: $%04X  (PRG offset $%04X)' % (reset_addr, guest_to_prg(reset_addr)))
    print('  IRQ vector:   $%04X  (PRG offset $%04X)' % (irq_addr, guest_to_prg(irq_addr)))
    print()

    # =========================================================
    # 2. NMI handler disassembly
    # =========================================================
    nmi_prg = guest_to_prg(nmi_addr)
    print('=' * 64)
    print('NMI HANDLER at $%04X (PRG offset $%04X)' % (nmi_addr, nmi_prg))
    print('=' * 64)
    for line in disasm(prg, nmi_prg, count=50):
        mark = ''
        if '01C8' in line:
            mark = '  <-- VBlank flag $01C8!'
        if '$2002' in line:
            mark += '  <-- PPU STATUS'
        if '$2005' in line:
            mark += '  <-- PPU SCROLL'
        if '$2006' in line:
            mark += '  <-- PPU ADDR'
        if '$2007' in line:
            mark += '  <-- PPU DATA'
        if '$4014' in line:
            mark += '  <-- OAM DMA'
        print(line + mark)
    print()

    # =========================================================
    # 3. Spin loop area at $FF5B
    # =========================================================
    spin_guest = 0xFF5B
    spin_prg = guest_to_prg(spin_guest)
    # Start a few instructions before for context
    start_prg = max(0, spin_prg - 16)
    print('=' * 64)
    print('SPIN LOOP AREA around $FF5B (PRG offset $%04X)' % spin_prg)
    print('=' * 64)
    for line in disasm(prg, start_prg, count=25, base_addr=prg_to_guest(start_prg)):
        mark = ''
        if '$FF5B' in line and 'BEQ' in line:
            mark = '  <-- SPIN LOOP (BEQ to self)'
        if '01C8' in line:
            mark += '  <-- VBlank flag!'
        print(line + mark)
    print()

    # =========================================================
    # 4. Code at $C000 (PRG offset $0000) - including $C003
    # =========================================================
    print('=' * 64)
    print('CODE AT $C000 (PRG offset $0000) - includes $C003 entry')
    print('=' * 64)
    for line in disasm(prg, 0x0000, count=35):
        print(line)
    print()

    # =========================================================
    # 5. All references to $01C8 in entire PRG
    # =========================================================
    print('=' * 64)
    print('ALL REFERENCES TO $01C8 (VBlank flag) IN ENTIRE PRG')
    print('=' * 64)
    pos = 0
    while pos < len(prg):
        op = prg[pos]
        if op in OPCODES:
            mnem, mode, size = OPCODES[op]
            if size == 3 and pos + 2 < len(prg):
                val = prg[pos+1] | (prg[pos+2] << 8)
                if val == 0x01C8:
                    addr = prg_to_guest(pos)
                    raw = prg[pos:pos+size]
                    raw_hex = ' '.join('%02X' % b for b in raw)
                    print('  $%04X: %s  %s $01C8' % (addr, raw_hex, mnem))
            pos += size
        else:
            pos += 1
    print()

    # =========================================================
    # 6. RESET handler
    # =========================================================
    reset_prg = guest_to_prg(reset_addr)
    print('=' * 64)
    print('RESET HANDLER at $%04X (PRG offset $%04X)' % (reset_addr, reset_prg))
    print('=' * 64)
    for line in disasm(prg, reset_prg, count=30):
        print(line)
    print()

    # =========================================================
    # 7. Follow JSR targets from NMI handler
    # =========================================================
    print('=' * 64)
    print('JSR TARGETS CALLED FROM NMI HANDLER')
    print('=' * 64)
    pos = nmi_prg
    seen_targets = []
    for i in range(60):
        if pos >= len(prg):
            break
        op = prg[pos]
        if op in OPCODES:
            mnem, mode, size = OPCODES[op]
            addr = prg_to_guest(pos)
            if mnem == 'JSR' and size == 3:
                target = prg[pos+1] | (prg[pos+2] << 8)
                seen_targets.append((addr, target))
            if mnem == 'RTI':
                print('  (RTI reached at $%04X - end of NMI handler)' % addr)
                break
            pos += size
        else:
            pos += 1

    for caller, target in seen_targets:
        tprg = guest_to_prg(target)
        print()
        print('  --- JSR $%04X (called from $%04X, PRG offset $%04X) ---' % (target, caller, tprg))
        for line in disasm(prg, tprg, count=20):
            mark = ''
            if '01C8' in line:
                mark = '  <-- VBlank flag!'
            if '$2002' in line:
                mark += '  <-- PPU STATUS'
            if '$4014' in line:
                mark += '  <-- OAM DMA'
            print(line + mark)

if __name__ == '__main__':
    main()
