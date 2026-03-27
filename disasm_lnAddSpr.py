#!/usr/bin/env python3
"""Disassemble _lnAddSpr, _lnSync, RESET, NMI from the millipede NES ROM."""

rom_path = r'c:\proj\c\NES\nesxidy-co\nesxidy\nes_millipede.nes'
with open(rom_path, 'rb') as f:
    data = f.read()

prg_banks = data[4]
print(f'PRG banks: {prg_banks} (each 16KB = {prg_banks*16}KB total)')
bank31_offset = 16 + (prg_banks - 1) * 16384
bank31 = data[bank31_offset:bank31_offset + 16384]

opcodes = {
    0x48: ('PHA', 1), 0x8A: ('TXA', 1), 0x98: ('TYA', 1),
    0x68: ('PLA', 1), 0xAA: ('TAX', 1), 0xA8: ('TAY', 1),
    0x60: ('RTS', 1), 0x40: ('RTI', 1), 0xEA: ('NOP', 1),
    0x18: ('CLC', 1), 0x38: ('SEC', 1), 0xE8: ('INX', 1),
    0xCA: ('DEX', 1), 0xC8: ('INY', 1), 0x88: ('DEY', 1),
    0x78: ('SEI', 1), 0xD8: ('CLD', 1), 0xBA: ('TSX', 1),
    0x9A: ('TXS', 1), 0x0A: ('ASL A', 1),
    0x4A: ('LSR A', 1), 0x2A: ('ROL A', 1), 0x6A: ('ROR A', 1),
    0xA9: ('LDA #', 2), 0xA2: ('LDX #', 2), 0xA0: ('LDY #', 2),
    0x09: ('ORA #', 2), 0x29: ('AND #', 2), 0x49: ('EOR #', 2),
    0xC9: ('CMP #', 2), 0xE9: ('SBC #', 2), 0x69: ('ADC #', 2),
    0xE0: ('CPX #', 2), 0xC0: ('CPY #', 2),
    0xA5: ('LDA zp', 2), 0xA6: ('LDX zp', 2), 0xA4: ('LDY zp', 2),
    0x85: ('STA zp', 2), 0x86: ('STX zp', 2), 0x84: ('STY zp', 2),
    0x05: ('ORA zp', 2), 0x25: ('AND zp', 2), 0x45: ('EOR zp', 2),
    0xC5: ('CMP zp', 2), 0xE5: ('SBC zp', 2), 0x65: ('ADC zp', 2),
    0xE6: ('INC zp', 2), 0xC6: ('DEC zp', 2), 0x06: ('ASL zp', 2),
    0x46: ('LSR zp', 2), 0x26: ('ROL zp', 2), 0x66: ('ROR zp', 2),
    0x24: ('BIT zp', 2), 0x64: ('STZ zp', 2),
    0xB5: ('LDA zp,X', 2), 0x95: ('STA zp,X', 2),
    0xB4: ('LDY zp,X', 2), 0x94: ('STY zp,X', 2),
    0x15: ('ORA zp,X', 2), 0x35: ('AND zp,X', 2),
    0x55: ('EOR zp,X', 2), 0x75: ('ADC zp,X', 2),
    0xF5: ('SBC zp,X', 2), 0xD5: ('CMP zp,X', 2),
    0x16: ('ASL zp,X', 2), 0x56: ('LSR zp,X', 2),
    0x36: ('ROL zp,X', 2), 0x76: ('ROR zp,X', 2),
    0xF6: ('INC zp,X', 2), 0xD6: ('DEC zp,X', 2),
    0xA1: ('LDA (zp,X)', 2), 0x81: ('STA (zp,X)', 2),
    0xB1: ('LDA (zp),Y', 2), 0x91: ('STA (zp),Y', 2),
    0xB2: ('LDA (zp)', 2), 0x92: ('STA (zp)', 2),
    0x12: ('ORA (zp)', 2), 0x32: ('AND (zp)', 2),
    0x0D: ('ORA abs', 3), 0x2D: ('AND abs', 3), 0x4D: ('EOR abs', 3),
    0xAD: ('LDA abs', 3), 0xAE: ('LDX abs', 3), 0xAC: ('LDY abs', 3),
    0x8D: ('STA abs', 3), 0x8E: ('STX abs', 3), 0x8C: ('STY abs', 3),
    0x2C: ('BIT abs', 3), 0x4C: ('JMP abs', 3), 0x20: ('JSR abs', 3),
    0xCC: ('CPY abs', 3), 0xCD: ('CMP abs', 3), 0xEC: ('CPX abs', 3),
    0x9C: ('STZ abs', 3), 0xFC: ('NOP abs', 3), 0xEE: ('INC abs', 3),
    0xCE: ('DEC abs', 3), 0x6C: ('JMP (abs)', 3),
    0x7C: ('JMP (abs,X)', 3), 0x6D: ('ADC abs', 3),
    0xED: ('SBC abs', 3),
    0xF0: ('BEQ', 2), 0xD0: ('BNE', 2), 0x10: ('BPL', 2),
    0x30: ('BMI', 2), 0x50: ('BVC', 2), 0x70: ('BVS', 2),
    0x90: ('BCC', 2), 0xB0: ('BCS', 2), 0x80: ('BRA', 2),
    0x9D: ('STA abs,X', 3), 0xBD: ('LDA abs,X', 3), 0x9E: ('STZ abs,X', 3),
    0x99: ('STA abs,Y', 3), 0xB9: ('LDA abs,Y', 3),
    0xBE: ('LDX abs,Y', 3), 0xBC: ('LDY abs,X', 3),
    0x1D: ('ORA abs,X', 3), 0x3D: ('AND abs,X', 3),
    0x5D: ('EOR abs,X', 3), 0x79: ('ADC abs,Y', 3),
    0x7D: ('ADC abs,X', 3), 0xFD: ('SBC abs,X', 3),
    0xDD: ('CMP abs,X', 3), 0xD9: ('CMP abs,Y', 3),
    0xDE: ('DEC abs,X', 3), 0xFE: ('INC abs,X', 3),
    0x19: ('ORA abs,Y', 3), 0x39: ('AND abs,Y', 3),
    0x59: ('EOR abs,Y', 3),
}

zp_names = {
    0x00: 'r0', 0x01: 'r1', 0x02: 'r2', 0x03: 'r3',
    0x04: 'r4', 0x05: 'r5', 0x06: 'r6', 0x07: 'r7',
    0x10: 'nfScroll', 
    0x20: 'nfOamFull(=$20)', 0x21: 'nfOamFull+1(=$21)',
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
    0x4014: 'OAMDMA', 0xC000: 'BANKSEL',
}

def disasm_range(start_cpu, end_cpu, highlight_zp=None):
    i = start_cpu - 0xC000
    end_i = end_cpu - 0xC000
    while i < end_i and i < len(bank31):
        addr = 0xC000 + i
        b = bank31[i]
        if b in opcodes:
            name, size = opcodes[b]
            raw = bank31[i:i+size]
            hex_s = ' '.join(f'{x:02X}' for x in raw)
            
            marker = ''
            if size == 1:
                op_str = name
            elif size == 2:
                if 'zp' in name:
                    zn = zp_names.get(raw[1], f'${raw[1]:02X}')
                    if raw[1] in (0x20, 0x21):
                        marker = '  <<<< ZP $20/$21!'
                    suffix = name.split('zp')[-1]
                    base = name.split(' ')[0]
                    if '(' in name:
                        op_str = f'{base} ({zn}){suffix}'
                    else:
                        op_str = f'{base} {zn}{suffix}'
                elif name in ('BEQ','BNE','BPL','BMI','BVC','BVS','BCC','BCS','BRA'):
                    offset = raw[1] if raw[1] < 128 else raw[1] - 256
                    target = addr + 2 + offset
                    op_str = f'{name} ${target:04X}'
                elif '#' in name:
                    op_str = f'{name}${raw[1]:02X}'
                else:
                    op_str = f'{name} ${raw[1]:02X}'
            elif size == 3:
                t = raw[1] | (raw[2] << 8)
                an = abs_names.get(t, f'${t:04X}')
                base = name.split(' ')[0]
                suffix = ''
                if ',X' in name: suffix = ',X'
                elif ',Y' in name: suffix = ',Y'
                if '(' in name:
                    op_str = f'{base} ({an}){suffix}'
                else:
                    op_str = f'{base} {an}{suffix}'
            else:
                op_str = name
            print(f'  ${addr:04X}: {hex_s:12s}  {op_str}{marker}')
            i += size
        else:
            print(f'  ${addr:04X}: {bank31[i]:02X}            .db ${bank31[i]:02X}')
            i += 1

# Vectors
nmi_vec = bank31[0x3FFA] | (bank31[0x3FFB] << 8)
reset_vec = bank31[0x3FFC] | (bank31[0x3FFD] << 8)
irq_vec = bank31[0x3FFE] | (bank31[0x3FFF] << 8)
print(f'Vectors: NMI=${nmi_vec:04X} RESET=${reset_vec:04X} IRQ=${irq_vec:04X}')

# 1. _lnAddSpr at $C354
print()
print('='*60)
print('_lnAddSpr ($C354):')
print('='*60)
disasm_range(0xC354, 0xC400)

# 2. _lnSync at $C24F
print()
print('='*60)
print('_lnSync ($C24F):')
print('='*60)
disasm_range(0xC24F, 0xC2E0)

# 3. _lnList at $C222
print()
print('='*60)
print('_lnList ($C222):')
print('='*60)
disasm_range(0xC222, 0xC24F)

# 4. RESET handler
print()
print('='*60)
print(f'RESET handler (${reset_vec:04X}):')
print('='*60)
disasm_range(reset_vec, reset_vec + 0xA0)

# 5. NMI handler
print()
print('='*60)
print(f'NMI handler (${nmi_vec:04X}):')
print('='*60)
disasm_range(nmi_vec, nmi_vec + 0x90)

# 6. Around $C080-$C098 (main call / startup epilogue)
print()
print('='*60)
print('Around $C080-$C0A0:')
print('='*60)
disasm_range(0xC080, 0xC0A0)

# 7. Check what's around _lnScroll ($C2D8)
print()
print('='*60)
print('_lnScroll ($C2D8):')
print('='*60)
disasm_range(0xC2D8, 0xC354)

# 8. Find ALL ZP $20 and $21 accesses in fixed bank
print()
print('='*60)
print('ALL ZP $20/$21 accesses in fixed bank:')
print('='*60)
zp_ops_2byte = {
    0xA5: 'LDA', 0x85: 'STA', 0xA6: 'LDX', 0x86: 'STX',
    0xA4: 'LDY', 0x84: 'STY', 0x05: 'ORA', 0x25: 'AND',
    0x45: 'EOR', 0xC5: 'CMP', 0xE4: 'CPX', 0xC4: 'CPY',
    0xE6: 'INC', 0xC6: 'DEC', 0x06: 'ASL', 0x46: 'LSR',
    0x26: 'ROL', 0x66: 'ROR', 0x24: 'BIT', 0x64: 'STZ',
    0x65: 'ADC', 0xE5: 'SBC',
    0xB1: 'LDA (zp),Y', 0x91: 'STA (zp),Y',
    0xA1: 'LDA (zp,X)', 0x81: 'STA (zp,X)',
    0xB2: 'LDA (zp)', 0x92: 'STA (zp)',
}
for target_zp in (0x20, 0x21):
    print(f'\n  --- ZP ${target_zp:02X} ---')
    for i in range(len(bank31) - 1):
        if bank31[i] in zp_ops_2byte and bank31[i+1] == target_zp:
            addr = 0xC000 + i
            op = zp_ops_2byte[bank31[i]]
            rw = 'W' if op.startswith(('STA','STX','STY','STZ','INC','DEC','ASL','LSR','ROL','ROR')) else 'R'
            print(f'    ${addr:04X}: {bank31[i]:02X} {target_zp:02X}  {op} ${target_zp:02X}  [{rw}]')
