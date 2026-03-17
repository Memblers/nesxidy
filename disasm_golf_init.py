#!/usr/bin/env python3
"""Disassemble NES Golf ROM init code."""

rom = open(r'C:\proj\c\NES\nesxidy-co\nesxidy\roms\nes\golf.nes', 'rb').read()
prg = rom[0x10:0x4010]  # 16KB PRG

opcodes = {
    0x78: ('SEI', '', 1), 0xD8: ('CLD', '', 1), 0x9A: ('TXS', '', 1),
    0xE8: ('INX', '', 1), 0xCA: ('DEX', '', 1), 0x88: ('DEY', '', 1),
    0xC8: ('INY', '', 1), 0xEA: ('NOP', '', 1),
    0xA9: ('LDA', '#$%02X', 2), 0xA2: ('LDX', '#$%02X', 2), 0xA0: ('LDY', '#$%02X', 2),
    0x8D: ('STA', '$%04X', 3), 0x8E: ('STX', '$%04X', 3), 0x8C: ('STY', '$%04X', 3),
    0xAD: ('LDA', '$%04X', 3), 0xAE: ('LDX', '$%04X', 3), 0xAC: ('LDY', '$%04X', 3),
    0x85: ('STA', '$%02X', 2), 0x86: ('STX', '$%02X', 2), 0x84: ('STY', '$%02X', 2),
    0xA5: ('LDA', '$%02X', 2), 0xA6: ('LDX', '$%02X', 2), 0xA4: ('LDY', '$%02X', 2),
    0x10: ('BPL', 'br', 2), 0x30: ('BMI', 'br', 2), 0xF0: ('BEQ', 'br', 2),
    0xD0: ('BNE', 'br', 2), 0x90: ('BCC', 'br', 2), 0xB0: ('BCS', 'br', 2),
    0x4C: ('JMP', '$%04X', 3), 0x20: ('JSR', '$%04X', 3), 0x60: ('RTS', '', 1),
    0x40: ('RTI', '', 1), 0x6C: ('JMP', '($%04X)', 3),
    0x29: ('AND', '#$%02X', 2), 0x09: ('ORA', '#$%02X', 2), 0x49: ('EOR', '#$%02X', 2),
    0xC9: ('CMP', '#$%02X', 2), 0xE0: ('CPX', '#$%02X', 2), 0xC0: ('CPY', '#$%02X', 2),
    0x48: ('PHA', '', 1), 0x68: ('PLA', '', 1), 0x08: ('PHP', '', 1), 0x28: ('PLP', '', 1),
    0x8A: ('TXA', '', 1), 0xAA: ('TAX', '', 1), 0x98: ('TYA', '', 1), 0xA8: ('TAY', '', 1),
    0x18: ('CLC', '', 1), 0x38: ('SEC', '', 1),
    0xE6: ('INC', '$%02X', 2), 0xC6: ('DEC', '$%02X', 2),
    0xEE: ('INC', '$%04X', 3), 0xCE: ('DEC', '$%04X', 3),
    0xBD: ('LDA', '$%04X,X', 3), 0xB9: ('LDA', '$%04X,Y', 3),
    0x9D: ('STA', '$%04X,X', 3), 0x99: ('STA', '$%04X,Y', 3),
    0xB1: ('LDA', '($%02X),Y', 2), 0x91: ('STA', '($%02X),Y', 2),
    0xA1: ('LDA', '($%02X,X)', 2), 0x81: ('STA', '($%02X,X)', 2),
    0x24: ('BIT', '$%02X', 2), 0x2C: ('BIT', '$%04X', 3),
    0x4A: ('LSR', 'A', 1), 0x0A: ('ASL', 'A', 1), 0x6A: ('ROR', 'A', 1), 0x2A: ('ROL', 'A', 1),
    0x69: ('ADC', '#$%02X', 2), 0xE9: ('SBC', '#$%02X', 2),
    0x65: ('ADC', '$%02X', 2), 0xE5: ('SBC', '$%02X', 2),
    0x6D: ('ADC', '$%04X', 3), 0xED: ('SBC', '$%04X', 3),
    0xB5: ('LDA', '$%02X,X', 2), 0x95: ('STA', '$%02X,X', 2),
    0xB4: ('LDY', '$%02X,X', 2), 0x94: ('STY', '$%02X,X', 2),
    0x16: ('ASL', '$%02X,X', 2), 0x56: ('LSR', '$%02X,X', 2),
    0x36: ('ROL', '$%02X,X', 2), 0x76: ('ROR', '$%02X,X', 2),
    0x06: ('ASL', '$%02X', 2), 0x46: ('LSR', '$%02X', 2),
    0x26: ('ROL', '$%02X', 2), 0x66: ('ROR', '$%02X', 2),
    0x0E: ('ASL', '$%04X', 3), 0x4E: ('LSR', '$%04X', 3),
    0x2E: ('ROL', '$%04X', 3), 0x6E: ('ROR', '$%04X', 3),
    0xF6: ('INC', '$%02X,X', 2), 0xD6: ('DEC', '$%02X,X', 2),
    0xFE: ('INC', '$%04X,X', 3), 0xDE: ('DEC', '$%04X,X', 3),
    0xDD: ('CMP', '$%04X,X', 3), 0xD9: ('CMP', '$%04X,Y', 3),
    0xC5: ('CMP', '$%02X', 2), 0xCD: ('CMP', '$%04X', 3),
    0xE4: ('CPX', '$%02X', 2), 0xEC: ('CPX', '$%04X', 3),
    0xC4: ('CPY', '$%02X', 2), 0xCC: ('CPY', '$%04X', 3),
}

def disasm(start_off, count):
    i = start_off
    lines = 0
    while i < len(prg) and lines < count:
        addr = 0xC000 + i
        op = prg[i]
        if op in opcodes:
            mnem, fmt, sz = opcodes[op]
            bytez = ' '.join('%02X' % prg[i+j] for j in range(sz))
            if sz == 1:
                print(f'  {addr:04X}: {bytez:8s}  {mnem} {fmt}')
            elif sz == 2:
                val = prg[i+1]
                if fmt == 'br':
                    offset = val if val < 128 else val - 256
                    target = addr + 2 + offset
                    print(f'  {addr:04X}: {bytez:8s}  {mnem} ${target:04X}')
                else:
                    print(f'  {addr:04X}: {bytez:8s}  {mnem} {fmt % val}')
            elif sz == 3:
                lo, hi = prg[i+1], prg[i+2]
                val = lo | (hi << 8)
                print(f'  {addr:04X}: {bytez:8s}  {mnem} {fmt % val}')
            i += sz
        else:
            print(f'  {addr:04X}: {prg[i]:02X}        .byte ${prg[i]:02X}')
            i += 1
        lines += 1

print('=== Golf ROM init from RESET ($C000) ===')
disasm(0x0000, 80)

print()
print('=== Code around $C120-$C150 (possible NMI enable) ===')
disasm(0x0120, 50)

print()
print('=== Code at $CFC0-$D000 (NMI sub-handler setup area) ===')
disasm(0x0FC0, 60)

print()
print('=== Code at $C8E0-$C920 (main wait loop area) ===')
disasm(0x08E0, 50)
