#!/usr/bin/env python3
"""Decode the vbcc startup code from exidy.nes to understand initialization."""

with open('exidy.nes', 'rb') as f:
    base = 16 + 31 * 16384  # bank 31 offset in iNES
    
    # Read full startup area
    f.seek(base)
    code = f.read(256)
    
    print("=== vbcc startup code (bank 31, $C000+) ===")
    print()
    
    # Print raw hex in 16-byte rows
    for row in range(0, 256, 16):
        addr = 0xC000 + row
        hex_str = ' '.join(f'{code[row+i]:02X}' for i in range(16))
        ascii_str = ''.join(chr(code[row+i]) if 32 <= code[row+i] < 127 else '.' for i in range(16))
        print(f'  {addr:04X}: {hex_str}  {ascii_str}')
    
    print()
    print("=== Manual disassembly of key sections ===")
    print()
    
    # Decode instruction by instruction
    pc = 0
    opcodes_1byte = {0x78: 'SEI', 0xD8: 'CLD', 0x9A: 'TXS', 0xE8: 'INX', 0xAA: 'TAX', 
                     0x60: 'RTS', 0x00: 'BRK', 0x88: 'DEY', 0xCA: 'DEX', 0xC8: 'INY',
                     0x18: 'CLC', 0x38: 'SEC', 0x48: 'PHA', 0x68: 'PLA', 0x08: 'PHP',
                     0x28: 'PLP', 0x8A: 'TXA', 0x98: 'TYA', 0xA8: 'TAY'}
    
    while pc < 176:
        op = code[pc]
        addr = 0xC000 + pc
        
        if op in opcodes_1byte:
            print(f'  {addr:04X}: {op:02X}          {opcodes_1byte[op]}')
            pc += 1
        elif op == 0xA9:  # LDA imm
            print(f'  {addr:04X}: A9 {code[pc+1]:02X}       LDA #${code[pc+1]:02X}')
            pc += 2
        elif op == 0xA2:  # LDX imm
            print(f'  {addr:04X}: A2 {code[pc+1]:02X}       LDX #${code[pc+1]:02X}')
            pc += 2
        elif op == 0xA0:  # LDY imm
            print(f'  {addr:04X}: A0 {code[pc+1]:02X}       LDY #${code[pc+1]:02X}')
            pc += 2
        elif op == 0xC9:  # CMP imm
            print(f'  {addr:04X}: C9 {code[pc+1]:02X}       CMP #${code[pc+1]:02X}')
            pc += 2
        elif op == 0xE0:  # CPX imm
            print(f'  {addr:04X}: E0 {code[pc+1]:02X}       CPX #${code[pc+1]:02X}')
            pc += 2
        elif op == 0xE9:  # SBC imm
            print(f'  {addr:04X}: E9 {code[pc+1]:02X}       SBC #${code[pc+1]:02X}')
            pc += 2
        elif op == 0x69:  # ADC imm
            print(f'  {addr:04X}: 69 {code[pc+1]:02X}       ADC #${code[pc+1]:02X}')
            pc += 2
        elif op == 0x09:  # ORA imm
            print(f'  {addr:04X}: 09 {code[pc+1]:02X}       ORA #${code[pc+1]:02X}')
            pc += 2
        elif op == 0x29:  # AND imm
            print(f'  {addr:04X}: 29 {code[pc+1]:02X}       AND #${code[pc+1]:02X}')
            pc += 2
        elif op == 0x49:  # EOR imm
            print(f'  {addr:04X}: 49 {code[pc+1]:02X}       EOR #${code[pc+1]:02X}')
            pc += 2
        elif op == 0x85:  # STA zp
            print(f'  {addr:04X}: 85 {code[pc+1]:02X}       STA ${code[pc+1]:02X}')
            pc += 2
        elif op == 0x86:  # STX zp
            print(f'  {addr:04X}: 86 {code[pc+1]:02X}       STX ${code[pc+1]:02X}')
            pc += 2
        elif op == 0x84:  # STY zp
            print(f'  {addr:04X}: 84 {code[pc+1]:02X}       STY ${code[pc+1]:02X}')
            pc += 2
        elif op == 0xA5:  # LDA zp
            print(f'  {addr:04X}: A5 {code[pc+1]:02X}       LDA ${code[pc+1]:02X}')
            pc += 2
        elif op == 0xA6:  # LDX zp
            print(f'  {addr:04X}: A6 {code[pc+1]:02X}       LDX ${code[pc+1]:02X}')
            pc += 2
        elif op == 0xA4:  # LDY zp
            print(f'  {addr:04X}: A4 {code[pc+1]:02X}       LDY ${code[pc+1]:02X}')
            pc += 2
        elif op == 0xE6:  # INC zp
            print(f'  {addr:04X}: E6 {code[pc+1]:02X}       INC ${code[pc+1]:02X}')
            pc += 2
        elif op == 0xC6:  # DEC zp
            print(f'  {addr:04X}: C6 {code[pc+1]:02X}       DEC ${code[pc+1]:02X}')
            pc += 2
        elif op == 0x8D:  # STA abs
            lo, hi = code[pc+1], code[pc+2]
            print(f'  {addr:04X}: 8D {lo:02X} {hi:02X}    STA ${hi:02X}{lo:02X}')
            pc += 3
        elif op == 0x8E:  # STX abs
            lo, hi = code[pc+1], code[pc+2]
            print(f'  {addr:04X}: 8E {lo:02X} {hi:02X}    STX ${hi:02X}{lo:02X}')
            pc += 3
        elif op == 0x8C:  # STY abs
            lo, hi = code[pc+1], code[pc+2]
            print(f'  {addr:04X}: 8C {lo:02X} {hi:02X}    STY ${hi:02X}{lo:02X}')
            pc += 3
        elif op == 0xAD:  # LDA abs
            lo, hi = code[pc+1], code[pc+2]
            print(f'  {addr:04X}: AD {lo:02X} {hi:02X}    LDA ${hi:02X}{lo:02X}')
            pc += 3
        elif op == 0xAE:  # LDX abs
            lo, hi = code[pc+1], code[pc+2]
            print(f'  {addr:04X}: AE {lo:02X} {hi:02X}    LDX ${hi:02X}{lo:02X}')
            pc += 3
        elif op == 0xAC:  # LDY abs
            lo, hi = code[pc+1], code[pc+2]
            print(f'  {addr:04X}: AC {lo:02X} {hi:02X}    LDY ${hi:02X}{lo:02X}')
            pc += 3
        elif op == 0x2C:  # BIT abs
            lo, hi = code[pc+1], code[pc+2]
            print(f'  {addr:04X}: 2C {lo:02X} {hi:02X}    BIT ${hi:02X}{lo:02X}')
            pc += 3
        elif op == 0x20:  # JSR
            lo, hi = code[pc+1], code[pc+2]
            print(f'  {addr:04X}: 20 {lo:02X} {hi:02X}    JSR ${hi:02X}{lo:02X}')
            pc += 3
        elif op == 0x4C:  # JMP abs
            lo, hi = code[pc+1], code[pc+2]
            print(f'  {addr:04X}: 4C {lo:02X} {hi:02X}    JMP ${hi:02X}{lo:02X}')
            pc += 3
        elif op == 0x6C:  # JMP indirect
            lo, hi = code[pc+1], code[pc+2]
            print(f'  {addr:04X}: 6C {lo:02X} {hi:02X}    JMP (${hi:02X}{lo:02X})')
            pc += 3
        elif op == 0x9D:  # STA abs,X
            lo, hi = code[pc+1], code[pc+2]
            print(f'  {addr:04X}: 9D {lo:02X} {hi:02X}    STA ${hi:02X}{lo:02X},X')
            pc += 3
        elif op == 0x99:  # STA abs,Y
            lo, hi = code[pc+1], code[pc+2]
            print(f'  {addr:04X}: 99 {lo:02X} {hi:02X}    STA ${hi:02X}{lo:02X},Y')
            pc += 3
        elif op == 0xBD:  # LDA abs,X
            lo, hi = code[pc+1], code[pc+2]
            print(f'  {addr:04X}: BD {lo:02X} {hi:02X}    LDA ${hi:02X}{lo:02X},X')
            pc += 3
        elif op == 0xB9:  # LDA abs,Y
            lo, hi = code[pc+1], code[pc+2]
            print(f'  {addr:04X}: B9 {lo:02X} {hi:02X}    LDA ${hi:02X}{lo:02X},Y')
            pc += 3
        elif op == 0x91:  # STA (zp),Y
            print(f'  {addr:04X}: 91 {code[pc+1]:02X}       STA (${code[pc+1]:02X}),Y')
            pc += 2
        elif op == 0xB1:  # LDA (zp),Y
            print(f'  {addr:04X}: B1 {code[pc+1]:02X}       LDA (${code[pc+1]:02X}),Y')
            pc += 2
        elif op == 0x81:  # STA (zp,X)
            print(f'  {addr:04X}: 81 {code[pc+1]:02X}       STA (${code[pc+1]:02X},X)')
            pc += 2
        elif op == 0xA1:  # LDA (zp,X)
            print(f'  {addr:04X}: A1 {code[pc+1]:02X}       LDA (${code[pc+1]:02X},X)')
            pc += 2
        elif op == 0xD0:  # BNE
            offset = code[pc+1]
            if offset >= 128: offset -= 256
            target = addr + 2 + offset
            print(f'  {addr:04X}: D0 {code[pc+1]:02X}       BNE ${target:04X}')
            pc += 2
        elif op == 0xF0:  # BEQ
            offset = code[pc+1]
            if offset >= 128: offset -= 256
            target = addr + 2 + offset
            print(f'  {addr:04X}: F0 {code[pc+1]:02X}       BEQ ${target:04X}')
            pc += 2
        elif op == 0x90:  # BCC
            offset = code[pc+1]
            if offset >= 128: offset -= 256
            target = addr + 2 + offset
            print(f'  {addr:04X}: 90 {code[pc+1]:02X}       BCC ${target:04X}')
            pc += 2
        elif op == 0xB0:  # BCS
            offset = code[pc+1]
            if offset >= 128: offset -= 256
            target = addr + 2 + offset
            print(f'  {addr:04X}: B0 {code[pc+1]:02X}       BCS ${target:04X}')
            pc += 2
        elif op == 0x10:  # BPL
            offset = code[pc+1]
            if offset >= 128: offset -= 256
            target = addr + 2 + offset
            print(f'  {addr:04X}: 10 {code[pc+1]:02X}       BPL ${target:04X}')
            pc += 2
        elif op == 0x30:  # BMI
            offset = code[pc+1]
            if offset >= 128: offset -= 256
            target = addr + 2 + offset
            print(f'  {addr:04X}: 30 {code[pc+1]:02X}       BMI ${target:04X}')
            pc += 2
        elif op == 0xEE:  # INC abs
            lo, hi = code[pc+1], code[pc+2]
            print(f'  {addr:04X}: EE {lo:02X} {hi:02X}    INC ${hi:02X}{lo:02X}')
            pc += 3
        elif op == 0xCE:  # DEC abs
            lo, hi = code[pc+1], code[pc+2]
            print(f'  {addr:04X}: CE {lo:02X} {hi:02X}    DEC ${hi:02X}{lo:02X}')
            pc += 3
        elif op == 0x95:  # STA zp,X
            print(f'  {addr:04X}: 95 {code[pc+1]:02X}       STA ${code[pc+1]:02X},X')
            pc += 2
        elif op == 0xB5:  # LDA zp,X
            print(f'  {addr:04X}: B5 {code[pc+1]:02X}       LDA ${code[pc+1]:02X},X')
            pc += 2
        elif op == 0xCD:  # CMP abs
            lo, hi = code[pc+1], code[pc+2]
            print(f'  {addr:04X}: CD {lo:02X} {hi:02X}    CMP ${hi:02X}{lo:02X}')
            pc += 3
        elif op == 0xEC:  # CPX abs
            lo, hi = code[pc+1], code[pc+2]
            print(f'  {addr:04X}: EC {lo:02X} {hi:02X}    CPX ${hi:02X}{lo:02X}')
            pc += 3
        elif op == 0xCC:  # CPY abs
            lo, hi = code[pc+1], code[pc+2]
            print(f'  {addr:04X}: CC {lo:02X} {hi:02X}    CPY ${hi:02X}{lo:02X}')
            pc += 3
        elif op == 0xC5:  # CMP zp
            print(f'  {addr:04X}: C5 {code[pc+1]:02X}       CMP ${code[pc+1]:02X}')
            pc += 2
        elif op == 0x4A:  # LSR A
            print(f'  {addr:04X}: 4A          LSR A')
            pc += 1
        elif op == 0x0A:  # ASL A
            print(f'  {addr:04X}: 0A          ASL A')
            pc += 1
        elif op == 0x6A:  # ROR A
            print(f'  {addr:04X}: 6A          ROR A')
            pc += 1
        elif op == 0x2A:  # ROL A
            print(f'  {addr:04X}: 2A          ROL A')
            pc += 1
        elif op == 0xEA:  # NOP
            print(f'  {addr:04X}: EA          NOP')
            pc += 1
        else:
            print(f'  {addr:04X}: {op:02X}          ??? (unknown)')
            pc += 1
    
    print()
    print("=== Checking if startup writes to $C000 mapper register ===")
    found = False
    for i in range(len(code) - 2):
        if code[i] == 0x8D and code[i+1] == 0x00 and code[i+2] == 0xC0:
            print(f"  Found STA $C000 at offset {0xC000+i:04X}")
            found = True
        if code[i] == 0x8E and code[i+1] == 0x00 and code[i+2] == 0xC0:
            print(f"  Found STX $C000 at offset {0xC000+i:04X}")
            found = True
    if not found:
        print("  *** NO WRITES TO $C000 FOUND IN STARTUP ***")
    
    # Show vectors
    f.seek(base + 0x3FFA)
    vectors = f.read(6)
    print()
    print(f"=== Vectors ===")
    print(f"  NMI:   ${vectors[1]:02X}{vectors[0]:02X}")
    print(f"  RESET: ${vectors[3]:02X}{vectors[2]:02X}")
    print(f"  IRQ:   ${vectors[5]:02X}{vectors[4]:02X}")
    
    # Show trampoline area
    f.seek(base + 0x3FF0)
    tramp = f.read(16)
    print()
    print(f"=== $FFF0-$FFFF ===")
    print(f"  {' '.join(f'{b:02X}' for b in tramp)}")
