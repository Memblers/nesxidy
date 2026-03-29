#!/usr/bin/env python3
"""Disassemble render_video from the built ROM to verify the IO8(0x2000) fix."""

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
    0x4D: ('EOR', 3, 'abs'), 0x6D: ('ADC', 3, 'abs'),
    0x0A: ('ASL', 1, 'imp'), 0x4A: ('LSR', 1, 'imp'),
    0x65: ('ADC', 2, 'zp'), 0xE5: ('SBC', 2, 'zp'),
    0x69: ('ADC', 2, 'imm'), 0xE9: ('SBC', 2, 'imm'),
}

rom_path = r'c:\proj\c\NES\nesxidy-co\nesxidy\nes_skydestroyer.nes'
with open(rom_path, 'rb') as f:
    data = f.read()

bank31_off = 16 + 31 * 16384
bank31 = data[bank31_off:bank31_off + 16384]

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
            elif mode == 'zp':
                op = f' ${raw[1]:02X}'
            elif mode == 'abs':
                t = raw[1] | (raw[2] << 8)
                op = f' ${t:04X}'
            elif mode == 'rel':
                offset = raw[1] if raw[1] < 128 else raw[1] - 256
                target = addr + 2 + offset
                op = f' ${target:04X}'
            elif mode == 'iny':
                op = f' (${raw[1]:02X}),Y'
            elif mode == 'abx':
                t = raw[1] | (raw[2] << 8)
                op = f' ${t:04X},X'
            else:
                op = ' ???'
            marker = ''
            if name == 'STA' and '2000' in op:
                marker = '  <<<< PPUCTRL WRITE'
            elif name == 'JSR' or name == 'JMP':
                if '$C24F' in op: marker = '  ; lnSync'
                elif '$C222' in op: marker = '  ; lnList'
            print(f'  ${addr:04X}: {hex_s:12s}  {name}{op}{marker}')
            i += size
        else:
            print(f'  ${addr:04X}: {bank31[i]:02X}            .db ${bank31[i]:02X}')
            i += 1

# render_video at $C9A4, render_video_noblock at $C97E
print('=== _render_video_noblock ($C97E) ===')
disasm(0xC97E, 0xC9A4)
print()
print('=== _render_video ($C9A4) ===')
disasm(0xC9A4, 0xC9E0)

# Also check if STA $2000 appears anywhere in the fixed bank
print()
print('=== Scanning fixed bank for STA $2000 (8D 00 20) ===')
pat = bytes([0x8D, 0x00, 0x20])
off = 0
while True:
    idx = bank31.find(pat, off)
    if idx == -1:
        break
    addr = 0xC000 + idx
    ctx = bank31[max(0,idx-3):idx+6]
    hex_ctx = ' '.join(f'{b:02X}' for b in ctx)
    print(f'  ${addr:04X}: ...{hex_ctx}...')
    off = idx + 1
