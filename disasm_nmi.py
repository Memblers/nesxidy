#!/usr/bin/env python3
"""Disassemble NMI handler and lnList from the DK ROM fixed bank."""

opcodes = {
    0x48: ('PHA', 1, 'imp'), 0x8A: ('TXA', 1, 'imp'), 0x98: ('TYA', 1, 'imp'),
    0x68: ('PLA', 1, 'imp'), 0xAA: ('TAX', 1, 'imp'), 0xA8: ('TAY', 1, 'imp'),
    0x60: ('RTS', 1, 'imp'), 0x40: ('RTI', 1, 'imp'), 0xEA: ('NOP', 1, 'imp'),
    0x18: ('CLC', 1, 'imp'), 0x38: ('SEC', 1, 'imp'), 0xE8: ('INX', 1, 'imp'),
    0xCA: ('DEX', 1, 'imp'), 0xC8: ('INY', 1, 'imp'), 0x88: ('DEY', 1, 'imp'),
    0xA9: ('LDA', 2, 'imm'), 0xA2: ('LDX', 2, 'imm'), 0xA0: ('LDY', 2, 'imm'),
    0x09: ('ORA', 2, 'imm'), 0x29: ('AND', 2, 'imm'), 0x49: ('EOR', 2, 'imm'),
    0xC9: ('CMP', 2, 'imm'), 0xE0: ('CPX', 2, 'imm'), 0xC0: ('CPY', 2, 'imm'),
    0xA5: ('LDA', 2, 'zp'), 0xA6: ('LDX', 2, 'zp'), 0xA4: ('LDY', 2, 'zp'),
    0x85: ('STA', 2, 'zp'), 0x86: ('STX', 2, 'zp'), 0x84: ('STY', 2, 'zp'),
    0x05: ('ORA', 2, 'zp'), 0x25: ('AND', 2, 'zp'), 0x45: ('EOR', 2, 'zp'),
    0xE6: ('INC', 2, 'zp'), 0xC6: ('DEC', 2, 'zp'), 0x06: ('ASL', 2, 'zp'),
    0x0D: ('ORA', 3, 'abs'), 0x2D: ('AND', 3, 'abs'),
    0xAD: ('LDA', 3, 'abs'), 0xAE: ('LDX', 3, 'abs'), 0xAC: ('LDY', 3, 'abs'),
    0x8D: ('STA', 3, 'abs'), 0x8E: ('STX', 3, 'abs'), 0x8C: ('STY', 3, 'abs'),
    0x2C: ('BIT', 3, 'abs'), 0x4C: ('JMP', 3, 'abs'), 0x20: ('JSR', 3, 'abs'),
    0xCC: ('CPY', 3, 'abs'), 0xCD: ('CMP', 3, 'abs'), 0xEC: ('CPX', 3, 'abs'),
    0xF0: ('BEQ', 2, 'rel'), 0xD0: ('BNE', 2, 'rel'), 0x10: ('BPL', 2, 'rel'),
    0x30: ('BMI', 2, 'rel'), 0x50: ('BVC', 2, 'rel'), 0x70: ('BVS', 2, 'rel'),
    0x90: ('BCC', 2, 'rel'), 0xB0: ('BCS', 2, 'rel'),
    0xB1: ('LDA', 2, 'iny'), 0x91: ('STA', 2, 'iny'),
    0x9D: ('STA', 3, 'abx'), 0xBD: ('LDA', 3, 'abx'),
    0x9E: ('STZ', 3, 'abx'),  # 65C02
    0x4D: ('EOR', 3, 'abs'),
    0x0A: ('ASL', 1, 'imp'),
    0xFC: ('NOP', 3, 'abs'),
}

zp_names = {
    0x02: 'r2', 0x03: 'r3',
    0x10: 'nfScroll', 0x20: 'nfOamFull',
    0x22: 'lnPPUCTRL', 0x23: 'lnPPUMASK', 0x24: 'lnSpr0Wait',
    0x25: 'nmiFlags', 0x26: 'nmiCounter', 0x27: 'oamIdx',
    0x28: 'nmiScrollT1', 0x29: 'nmiScrollT2',
    0x2A: 'sT1', 0x2B: 'sT2hi', 0x2C: 'sT2lo',
    0x2D: 'sT2Y', 0x2E: 'sT2namehi',
    0x2F: 'splitActive', 0x30: 'listPtrLo', 0x31: 'listPtrHi',
}

abs_names = {
    0x2000: 'PPUCTRL', 0x2001: 'PPUMASK', 0x2002: 'PPUSTATUS',
    0x2005: 'PPUSCROLL', 0x2006: 'PPUADDR', 0x2007: 'PPUDATA',
    0x4014: 'OAMDMA',
}

labels = {
    0xC141: 'FlushVramUpdate', 0xC192: '___nmi', 0xC214: '_nmiCallback',
    0xC222: '_lnList', 0xC23B: '_lnPush', 0xC24F: '_lnSync',
    0xC2D8: '_lnScroll',
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
        # Print label if any
        if addr in labels:
            print(f'\n{labels[addr]}:')
        b = bank31[i]
        if b in opcodes:
            name, size, mode = opcodes[b]
            raw = bank31[i:i+size]
            hex_s = ' '.join(f'{x:02X}' for x in raw)
            if mode == 'imp':
                op = ''
            elif mode == 'imm':
                op = f' #${raw[1]:02X}'
            elif mode == 'zp':
                zn = zp_names.get(raw[1], f'${raw[1]:02X}')
                op = f' {zn}'
            elif mode == 'abs':
                t = raw[1] | (raw[2] << 8)
                an = abs_names.get(t, labels.get(t, f'${t:04X}'))
                op = f' {an}'
            elif mode == 'rel':
                offset = raw[1] if raw[1] < 128 else raw[1] - 256
                target = addr + 2 + offset
                tl = labels.get(target, f'${target:04X}')
                op = f' {tl}'
            elif mode == 'iny':
                zn = zp_names.get(raw[1], f'${raw[1]:02X}')
                op = f' ({zn}),Y'
            elif mode == 'abx':
                t = raw[1] | (raw[2] << 8)
                an = abs_names.get(t, f'${t:04X}')
                op = f' {an},X'
            else:
                op = f' ???'
            print(f'  ${addr:04X}: {hex_s:12s}  {name}{op}')
            i += size
        else:
            print(f'  ${addr:04X}: {bank31[i]:02X}            .db ${bank31[i]:02X}')
            i += 1


print('========== ___nmi handler ($C192) ==========')
disasm(0xC192, 0xC222)
print()
print('========== _lnList ($C222) ==========')
disasm(0xC222, 0xC2D8)
print()
print('========== FlushVramUpdate ($C141) ==========')
disasm(0xC141, 0xC192)

print()
print('========== Call chain return addresses ==========')
print('--- Around $C098 (return from main loop?) ---')
disasm(0xC090, 0xC0A0)
print('--- Around $C0D6 ---')
disasm(0xC0C8, 0xC0E0)
print('--- Around $C6DC ---')
disasm(0xC6D0, 0xC6E8)

# Also check what functions are in the area
print()
print('========== Labels near return addresses ==========')
for addr_name in sorted(labels.items()):
    if 0xC080 <= addr_name[0] <= 0xC100 or 0xC0C0 <= addr_name[0] <= 0xC0E0 or 0xC6C0 <= addr_name[0] <= 0xC6F0:
        print(f'  ${addr_name[0]:04X}: {addr_name[1]}')

# Find all labels in $C000-$C800
print()
print('========== All labels in fixed bank (from mlb scan) ==========')
for addr, name in sorted(labels.items()):
    print(f'  ${addr:04X}: {name}')
