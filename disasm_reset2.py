#!/usr/bin/env python3
"""Disassemble lazyNES RESET handler area around $C080-$C09A."""

opcodes = {
    0x48: ('PHA', 1, 'imp'), 0x8A: ('TXA', 1, 'imp'), 0x98: ('TYA', 1, 'imp'),
    0x68: ('PLA', 1, 'imp'), 0xAA: ('TAX', 1, 'imp'), 0xA8: ('TAY', 1, 'imp'),
    0x60: ('RTS', 1, 'imp'), 0x40: ('RTI', 1, 'imp'), 0xEA: ('NOP', 1, 'imp'),
    0x18: ('CLC', 1, 'imp'), 0x38: ('SEC', 1, 'imp'), 0xE8: ('INX', 1, 'imp'),
    0xCA: ('DEX', 1, 'imp'), 0xC8: ('INY', 1, 'imp'), 0x88: ('DEY', 1, 'imp'),
    0x78: ('SEI', 1, 'imp'), 0xD8: ('CLD', 1, 'imp'), 0xBA: ('TSX', 1, 'imp'),
    0x9A: ('TXS', 1, 'imp'), 0x0A: ('ASL', 1, 'imp'),
    0xA9: ('LDA', 2, 'imm'), 0xA2: ('LDX', 2, 'imm'), 0xA0: ('LDY', 2, 'imm'),
    0x09: ('ORA', 2, 'imm'), 0x29: ('AND', 2, 'imm'), 0x49: ('EOR', 2, 'imm'),
    0xC9: ('CMP', 2, 'imm'), 0xE0: ('CPX', 2, 'imm'), 0xC0: ('CPY', 2, 'imm'),
    0x69: ('ADC', 2, 'imm'),
    0xA5: ('LDA', 2, 'zp'), 0xA6: ('LDX', 2, 'zp'), 0xA4: ('LDY', 2, 'zp'),
    0x85: ('STA', 2, 'zp'), 0x86: ('STX', 2, 'zp'), 0x84: ('STY', 2, 'zp'),
    0x05: ('ORA', 2, 'zp'), 0x25: ('AND', 2, 'zp'), 0x45: ('EOR', 2, 'zp'),
    0xE6: ('INC', 2, 'zp'), 0xC6: ('DEC', 2, 'zp'), 0x06: ('ASL', 2, 'zp'),
    0x65: ('ADC', 2, 'zp'), 0x64: ('STZ', 2, 'zp'), 0x46: ('LSR', 2, 'zp'),
    0x66: ('ROR', 2, 'zp'),
    0x0D: ('ORA', 3, 'abs'), 0x2D: ('AND', 3, 'abs'),
    0xAD: ('LDA', 3, 'abs'), 0xAE: ('LDX', 3, 'abs'), 0xAC: ('LDY', 3, 'abs'),
    0x8D: ('STA', 3, 'abs'), 0x8E: ('STX', 3, 'abs'), 0x8C: ('STY', 3, 'abs'),
    0x2C: ('BIT', 3, 'abs'), 0x4C: ('JMP', 3, 'abs'), 0x20: ('JSR', 3, 'abs'),
    0xCC: ('CPY', 3, 'abs'), 0xCD: ('CMP', 3, 'abs'), 0xEC: ('CPX', 3, 'abs'),
    0x9C: ('STZ', 3, 'abs'), 0x4D: ('EOR', 3, 'abs'), 0xFC: ('NOP', 3, 'abs'),
    0x6C: ('JMP', 3, 'ind'),
    0xF0: ('BEQ', 2, 'rel'), 0xD0: ('BNE', 2, 'rel'), 0x10: ('BPL', 2, 'rel'),
    0x30: ('BMI', 2, 'rel'), 0x50: ('BVC', 2, 'rel'), 0x70: ('BVS', 2, 'rel'),
    0x90: ('BCC', 2, 'rel'), 0xB0: ('BCS', 2, 'rel'),
    0xB1: ('LDA', 2, 'iny'), 0x91: ('STA', 2, 'iny'),
    0x9D: ('STA', 3, 'abx'), 0xBD: ('LDA', 3, 'abx'), 0x9E: ('STZ', 3, 'abx'),
    0x99: ('STA', 3, 'aby'), 0xB9: ('LDA', 3, 'aby'),
    0x95: ('STA', 2, 'zpx'), 0xB5: ('LDA', 2, 'zpx'),
}

zp_names = {
    0x00: 'r0', 0x01: 'r1', 0x02: 'r2', 0x03: 'r3',
    0x10: 'nfScroll', 0x20: 'nfOamFull', 0x21: 'nfOamFull+1',
    0x22: 'lnPPUCTRL', 0x23: 'lnPPUMASK', 0x24: 'lnSpr0Wait',
    0x25: 'nmiFlags', 0x26: 'nmiCounter', 0x27: 'oamIdx',
    0x28: 'nmiScrollT1', 0x29: 'nmiScrollT2',
    0x2A: 'sT1', 0x2B: 'sT2hi', 0x2C: 'sT2lo',
    0x2D: 'sT2Y', 0x2E: 'sT2namehi',
    0x2F: 'splitActive', 0x30: 'listPtrLo', 0x31: 'listPtrHi',
}

rom_path = r'c:\proj\c\NES\nesxidy-co\nesxidy\nes_donkeykong.nes'
with open(rom_path, 'rb') as f:
    data = f.read()

bank31_offset = 16 + 31 * 16384
bank31 = data[bank31_offset:bank31_offset + 16384]


def disasm(start_cpu, end_cpu):
    i = start_cpu - 0xC000
    end_i = end_cpu - 0xC000
    while i < end_i:
        addr = 0xC000 + i
        b = bank31[i]
        if b in opcodes:
            name, size, mode = opcodes[b]
            raw = bank31[i:i+size]
            hex_s = ' '.join(f'{x:02X}' for x in raw)
            if mode == 'imp':
                op = ''
            elif mode == 'imm':
                op = f' #${raw[1]:02X}'
            elif mode in ('zp', 'zpx'):
                zn = zp_names.get(raw[1], f'${raw[1]:02X}')
                sfx = ',X' if mode == 'zpx' else ''
                op = f' {zn}{sfx}'
            elif mode == 'abs':
                t = raw[1] | (raw[2] << 8)
                op = f' ${t:04X}'
            elif mode in ('abx', 'aby'):
                t = raw[1] | (raw[2] << 8)
                sfx = ',X' if mode == 'abx' else ',Y'
                op = f' ${t:04X}{sfx}'
            elif mode == 'rel':
                offset = raw[1] if raw[1] < 128 else raw[1] - 256
                target = addr + 2 + offset
                op = f' ${target:04X}'
            elif mode == 'iny':
                zn = zp_names.get(raw[1], f'${raw[1]:02X}')
                op = f' ({zn}),Y'
            elif mode == 'ind':
                t = raw[1] | (raw[2] << 8)
                op = f' (${t:04X})'
            else:
                op = f' ???'
            print(f'  ${addr:04X}: {hex_s:12s}  {name}{op}')
            i += size
        else:
            print(f'  ${addr:04X}: {bank31[i]:02X}            .db ${bank31[i]:02X}')
            i += 1


print('===== RESET / init code ($C060-$C0A0) =====')
disasm(0xC060, 0xC0A0)
