#!/usr/bin/env python3
"""Disassemble Millipede ROM around $7672 to find the stuck loop."""

import struct

# 6502 instruction table: (mnemonic, addressing_mode, length)
OPCODES = {}
def op(code, mne, mode, length):
    OPCODES[code] = (mne, mode, length)

# Populate basic 6502 opcodes
op(0x00,'BRK','imp',1); op(0x01,'ORA','indx',2); op(0x05,'ORA','zp',2)
op(0x06,'ASL','zp',2); op(0x08,'PHP','imp',1); op(0x09,'ORA','imm',2)
op(0x0A,'ASL','acc',1); op(0x0D,'ORA','abs',3); op(0x0E,'ASL','abs',3)
op(0x10,'BPL','rel',2); op(0x11,'ORA','indy',2); op(0x15,'ORA','zpx',2)
op(0x16,'ASL','zpx',2); op(0x18,'CLC','imp',1); op(0x19,'ORA','absy',3)
op(0x1D,'ORA','absx',3); op(0x1E,'ASL','absx',3)
op(0x20,'JSR','abs',3); op(0x21,'AND','indx',2); op(0x24,'BIT','zp',2)
op(0x25,'AND','zp',2); op(0x26,'ROL','zp',2); op(0x28,'PLP','imp',1)
op(0x29,'AND','imm',2); op(0x2A,'ROL','acc',1); op(0x2C,'BIT','abs',3)
op(0x2D,'AND','abs',3); op(0x2E,'ROL','abs',3)
op(0x30,'BMI','rel',2); op(0x31,'AND','indy',2); op(0x35,'AND','zpx',2)
op(0x36,'ROL','zpx',2); op(0x38,'SEC','imp',1); op(0x39,'AND','absy',3)
op(0x3D,'AND','absx',3); op(0x3E,'ROL','absx',3)
op(0x40,'RTI','imp',1); op(0x41,'EOR','indx',2); op(0x45,'EOR','zp',2)
op(0x46,'LSR','zp',2); op(0x48,'PHA','imp',1); op(0x49,'EOR','imm',2)
op(0x4A,'LSR','acc',1); op(0x4C,'JMP','abs',3); op(0x4D,'EOR','abs',3)
op(0x4E,'LSR','abs',3)
op(0x50,'BVC','rel',2); op(0x51,'EOR','indy',2); op(0x55,'EOR','zpx',2)
op(0x56,'LSR','zpx',2); op(0x58,'CLI','imp',1); op(0x59,'EOR','absy',3)
op(0x5D,'EOR','absx',3); op(0x5E,'LSR','absx',3)
op(0x60,'RTS','imp',1); op(0x61,'ADC','indx',2); op(0x65,'ADC','zp',2)
op(0x66,'ROR','zp',2); op(0x68,'PLA','imp',1); op(0x69,'ADC','imm',2)
op(0x6A,'ROR','acc',1); op(0x6C,'JMP','ind',3); op(0x6D,'ADC','abs',3)
op(0x6E,'ROR','abs',3)
op(0x70,'BVS','rel',2); op(0x71,'ADC','indy',2); op(0x75,'ADC','zpx',2)
op(0x76,'ROR','zpx',2); op(0x78,'SEI','imp',1); op(0x79,'ADC','absy',3)
op(0x7D,'ADC','absx',3); op(0x7E,'ROR','absx',3)
op(0x81,'STA','indx',2); op(0x84,'STY','zp',2); op(0x85,'STA','zp',2)
op(0x86,'STX','zp',2); op(0x88,'DEY','imp',1); op(0x8A,'TXA','imp',1)
op(0x8C,'STY','abs',3); op(0x8D,'STA','abs',3); op(0x8E,'STX','abs',3)
op(0x90,'BCC','rel',2); op(0x91,'STA','indy',2); op(0x94,'STY','zpx',2)
op(0x95,'STA','zpx',2); op(0x96,'STX','zpy',2); op(0x98,'TYA','imp',1)
op(0x99,'STA','absy',3); op(0x9A,'TXS','imp',1); op(0x9D,'STA','absx',3)
op(0xA0,'LDY','imm',2); op(0xA1,'LDA','indx',2); op(0xA2,'LDX','imm',2)
op(0xA4,'LDY','zp',2); op(0xA5,'LDA','zp',2); op(0xA6,'LDX','zp',2)
op(0xA8,'TAY','imp',1); op(0xA9,'LDA','imm',2); op(0xAA,'TAX','imp',1)
op(0xAC,'LDY','abs',3); op(0xAD,'LDA','abs',3); op(0xAE,'LDX','abs',3)
op(0xB0,'BCS','rel',2); op(0xB1,'LDA','indy',2); op(0xB4,'LDY','zpx',2)
op(0xB5,'LDA','zpx',2); op(0xB6,'LDX','zpy',2); op(0xB8,'CLV','imp',1)
op(0xB9,'LDA','absy',3); op(0xBA,'TSX','imp',1); op(0xBC,'LDY','absx',3)
op(0xBD,'LDA','absx',3); op(0xBE,'LDX','absy',3)
op(0xC0,'CPY','imm',2); op(0xC1,'CMP','indx',2); op(0xC4,'CPY','zp',2)
op(0xC5,'CMP','zp',2); op(0xC6,'DEC','zp',2); op(0xC8,'INY','imp',1)
op(0xC9,'CMP','imm',2); op(0xCA,'DEX','imp',1); op(0xCC,'CPY','abs',3)
op(0xCD,'CMP','abs',3); op(0xCE,'DEC','abs',3)
op(0xD0,'BNE','rel',2); op(0xD1,'CMP','indy',2); op(0xD5,'CMP','zpx',2)
op(0xD6,'DEC','zpx',2); op(0xD8,'CLD','imp',1); op(0xD9,'CMP','absy',3)
op(0xDD,'CMP','absx',3); op(0xDE,'DEC','absx',3)
op(0xE0,'CPX','imm',2); op(0xE1,'SBC','indx',2); op(0xE4,'CPX','zp',2)
op(0xE5,'SBC','zp',2); op(0xE6,'INC','zp',2); op(0xE8,'INX','imp',1)
op(0xE9,'SBC','imm',2); op(0xEA,'NOP','imp',1); op(0xEC,'CPX','abs',3)
op(0xED,'SBC','abs',3); op(0xEE,'INC','abs',3)
op(0xF0,'BEQ','rel',2); op(0xF1,'SBC','indy',2); op(0xF5,'SBC','zpx',2)
op(0xF6,'INC','zpx',2); op(0xF8,'SED','imp',1); op(0xF9,'SBC','absy',3)
op(0xFD,'SBC','absx',3); op(0xFE,'INC','absx',3)

def fmt_operand(mode, lo, hi, pc):
    if mode == 'imp' or mode == 'acc': return ''
    if mode == 'imm': return f'#${lo:02X}'
    if mode == 'zp': return f'${lo:02X}'
    if mode == 'zpx': return f'${lo:02X},X'
    if mode == 'zpy': return f'${lo:02X},Y'
    if mode == 'abs': return f'${hi:02X}{lo:02X}'
    if mode == 'absx': return f'${hi:02X}{lo:02X},X'
    if mode == 'absy': return f'${hi:02X}{lo:02X},Y'
    if mode == 'ind': return f'(${hi:02X}{lo:02X})'
    if mode == 'indx': return f'(${lo:02X},X)'
    if mode == 'indy': return f'(${lo:02X}),Y'
    if mode == 'rel':
        target = pc + 2 + (lo if lo < 128 else lo - 256)
        return f'${target:04X}'
    return '???'

def disasm(rom, base_addr, start, count):
    """Disassemble 'count' instructions starting at offset 'start' in rom."""
    pc = base_addr + start
    offset = start
    lines = []
    for _ in range(count):
        if offset >= len(rom):
            break
        b = rom[offset]
        if b in OPCODES:
            mne, mode, length = OPCODES[b]
            lo = rom[offset+1] if length > 1 and offset+1 < len(rom) else 0
            hi = rom[offset+2] if length > 2 and offset+2 < len(rom) else 0
            operand = fmt_operand(mode, lo, hi, pc)
            raw = ' '.join(f'{rom[offset+j]:02X}' for j in range(min(length, len(rom)-offset)))
            lines.append(f'${pc:04X}: {raw:10s} {mne} {operand}')
            offset += length
            pc += length
        else:
            lines.append(f'${pc:04X}: {b:02X}         .db ${b:02X}')
            offset += 1
            pc += 1
    return lines

# Read the NES ROM
with open('millipede.nes', 'rb') as f:
    nes = f.read()

# Bank 23 contains the Millipede ROM, mapped at $4000-$7FFF
# iNES header = 16 bytes, each bank = 16384 bytes
bank23_offset = 16 + 23 * 16384
rom = nes[bank23_offset:bank23_offset + 16384]
print(f"ROM bank 23 at file offset {bank23_offset:#x}, size={len(rom)}")

# ROM base address is $4000
BASE = 0x4000

# Disassemble around $7672
target = 0x7672
rom_off = target - BASE
print(f"\n=== Disassembly around ${target:04X} (ROM offset ${rom_off:04X}) ===")
# Start a bit before
start_off = max(0, rom_off - 32)
for line in disasm(rom, BASE, start_off, 40):
    marker = " <<<<" if line.startswith(f'${target:04X}') else ""
    print(f"  {line}{marker}")

# Also check $7643 (IRQ vector) and $7929 (Reset)
print(f"\n=== IRQ handler at $7643 ===")
for line in disasm(rom, BASE, 0x7643 - BASE, 30):
    print(f"  {line}")

print(f"\n=== Vectors ===")
# Vectors are at the end: FFFA-FFFF, but mirrored from $7FFA-$7FFF
vec_off = 0x7FFA - BASE
nmi = rom[vec_off] | (rom[vec_off+1] << 8)
rst = rom[vec_off+2] | (rom[vec_off+3] << 8)
irq = rom[vec_off+4] | (rom[vec_off+5] << 8)
print(f"  NMI=${nmi:04X}  Reset=${rst:04X}  IRQ=${irq:04X}")

# Look for loops that reference $7672
print(f"\n=== Scanning for branches to/from $7672 area ===")
for off in range(len(rom)):
    b = rom[off]
    pc = BASE + off
    if b in OPCODES:
        mne, mode, length = OPCODES[b]
        if mode == 'rel' and length == 2 and off+1 < len(rom):
            lo = rom[off+1]
            branch_target = pc + 2 + (lo if lo < 128 else lo - 256)
            if abs(branch_target - target) <= 16 or abs(pc - target) <= 16:
                operand = fmt_operand(mode, lo, 0, pc)
                print(f"  ${pc:04X}: {mne} {operand}")

# Check what I/O addresses are accessed near $7672
print(f"\n=== I/O accesses near $7672 ===")
for off in range(max(0, rom_off - 64), min(len(rom), rom_off + 64)):
    b = rom[off]
    pc = BASE + off
    if b in OPCODES:
        mne, mode, length = OPCODES[b]
        if mode == 'abs' and length == 3 and off+2 < len(rom):
            addr = rom[off+1] | (rom[off+2] << 8)
            if addr >= 0x0400 and addr < 0x4000:
                operand = fmt_operand(mode, rom[off+1], rom[off+2], pc)
                print(f"  ${pc:04X}: {mne} {operand}")
