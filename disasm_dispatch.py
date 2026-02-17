#!/usr/bin/env python3
"""Disassemble dispatch_on_pc from the ROM binary to find key addresses."""

with open('exidy.nes', 'rb') as f:
    # dispatch_on_pc at $620D, data section starts at VMA $6000
    # Found in ROM at file offset 0x19BA
    f.seek(0x19BA)
    code = f.read(160)
    base = 0x620D
    i = 0
    opcodes = {
        0x4C: ('JMP', 'abs'), 0x20: ('JSR', 'abs'), 0x6C: ('JMP', 'ind'),
        0x85: ('STA', 'zp'), 0xA5: ('LDA', 'zp'), 0x86: ('STX', 'zp'), 0xA6: ('LDX', 'zp'),
        0x84: ('STY', 'zp'), 0xA4: ('LDY', 'zp'),
        0x8D: ('STA', 'abs'), 0xAD: ('LDA', 'abs'), 0x8E: ('STX', 'abs'), 0xAE: ('LDX', 'abs'),
        0x8C: ('STY', 'abs'), 0xAC: ('LDY', 'abs'),
        0xA9: ('LDA', 'imm'), 0xA2: ('LDX', 'imm'), 0xA0: ('LDY', 'imm'),
        0x09: ('ORA', 'imm'), 0x29: ('AND', 'imm'), 0x49: ('EOR', 'imm'),
        0x69: ('ADC', 'imm'), 0xC9: ('CMP', 'imm'), 0xE0: ('CPX', 'imm'), 0xC0: ('CPY', 'imm'),
        0x48: ('PHA', 'imp'), 0x68: ('PLA', 'imp'), 0x08: ('PHP', 'imp'), 0x28: ('PLP', 'imp'),
        0x60: ('RTS', 'imp'), 0x40: ('RTI', 'imp'),
        0x0A: ('ASL A', 'imp'), 0x2A: ('ROL A', 'imp'), 0x4A: ('LSR A', 'imp'), 0x6A: ('ROR A', 'imp'),
        0x38: ('SEC', 'imp'), 0x18: ('CLC', 'imp'), 0xF8: ('SED', 'imp'), 0xD8: ('CLD', 'imp'),
        0x78: ('SEI', 'imp'), 0x58: ('CLI', 'imp'), 0xB8: ('CLV', 'imp'),
        0xCA: ('DEX', 'imp'), 0xE8: ('INX', 'imp'), 0x88: ('DEY', 'imp'), 0xC8: ('INY', 'imp'),
        0xAA: ('TAX', 'imp'), 0x8A: ('TXA', 'imp'), 0xA8: ('TAY', 'imp'), 0x98: ('TYA', 'imp'),
        0xBA: ('TSX', 'imp'), 0x9A: ('TXS', 'imp'), 0xEA: ('NOP', 'imp'),
        0xB1: ('LDA', 'izy'), 0x91: ('STA', 'izy'), 0x11: ('ORA', 'izy'),
        0xA1: ('LDA', 'izx'), 0x81: ('STA', 'izx'), 0x01: ('ORA', 'izx'),
        0xE6: ('INC', 'zp'), 0xC6: ('DEC', 'zp'),
    }
    branches = {0x10:'BPL', 0x30:'BMI', 0x50:'BVC', 0x70:'BVS', 0x90:'BCC', 0xB0:'BCS', 0xD0:'BNE', 0xF0:'BEQ'}
    
    while i < 160:
        addr = base + i
        b = code[i]
        if b in branches:
            offset = code[i+1]
            if offset > 127: offset -= 256
            target = addr + 2 + offset
            print(f"  {addr:04X}: {branches[b]} ${target:04X}")
            i += 2
        elif b in opcodes:
            name, mode = opcodes[b]
            if mode == 'imp':
                print(f"  {addr:04X}: {name}")
                i += 1
            elif mode == 'imm':
                print(f"  {addr:04X}: {name} #${code[i+1]:02X}")
                i += 2
            elif mode == 'zp':
                print(f"  {addr:04X}: {name} ${code[i+1]:02X}")
                i += 2
            elif mode == 'abs':
                t = code[i+1] | (code[i+2] << 8)
                print(f"  {addr:04X}: {name} ${t:04X}")
                i += 3
            elif mode == 'ind':
                t = code[i+1] | (code[i+2] << 8)
                print(f"  {addr:04X}: {name} (${t:04X})")
                i += 3
            elif mode == 'izy':
                print(f"  {addr:04X}: {name} (${code[i+1]:02X}),Y")
                i += 2
            elif mode == 'izx':
                print(f"  {addr:04X}: {name} (${code[i+1]:02X},X)")
                i += 2
        else:
            print(f"  {addr:04X}: .byte ${b:02X}")
            i += 1
