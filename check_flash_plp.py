"""Disassemble the JIT block at $AA40 and check for missing PLP"""
f = open(r'c:\proj\c\NES\nesxidy-co\nesxidy\nes_donkeykong.nes', 'rb')
f.seek(16 + 4 * 16384 + 0x2A40)  # bank 4, $AA40
block = f.read(128)
f.close()

# 6502 disassembler (simplified)
opcodes = {
    0x00: ('BRK', 1), 0x08: ('PHP', 1), 0x0A: ('ASL A', 1),
    0x18: ('CLC', 1), 0x28: ('PLP', 1), 0x2A: ('ROL A', 1),
    0x38: ('SEC', 1), 0x48: ('PHA', 1), 0x4C: ('JMP', 3),
    0x60: ('RTS', 1), 0x68: ('PLA', 1), 0x69: ('ADC #', 2),
    0x6A: ('ROR A', 1), 0x84: ('STY zp', 2), 0x85: ('STA zp', 2),
    0x86: ('STX zp', 2), 0x88: ('DEY', 1), 0x8C: ('STY abs', 3),
    0x8D: ('STA abs', 3), 0x8E: ('STX abs', 3), 0x90: ('BCC', 2),
    0x91: ('STA (zp),Y', 2), 0xA0: ('LDY #', 2), 0xA2: ('LDX #', 2),
    0xA4: ('LDY zp', 2), 0xA5: ('LDA zp', 2), 0xA6: ('LDX zp', 2),
    0xA9: ('LDA #', 2), 0xAD: ('LDA abs', 3), 0xB1: ('LDA (zp),Y', 2),
    0xB0: ('BCS', 2), 0xBD: ('LDA abs,X', 3), 0xC8: ('INY', 1),
    0xC9: ('CMP #', 2), 0xCD: ('CMP abs', 3), 0xD0: ('BNE', 2),
    0xD1: ('CMP (zp),Y', 2), 0xE0: ('CPX #', 2), 0xE6: ('INC zp', 2),
    0xE8: ('INX', 1), 0xE9: ('SBC #', 2), 0xEA: ('NOP', 1),
    0xEE: ('INC abs', 3), 0xF0: ('BEQ', 2),
}

addr = 0xAA40
pc = 0
php_without_plp = 0
php_count = 0
plp_count = 0
stack_depth = 0
in_php = False

print("Disassembly of JIT block at $AA40:")
print("=" * 60)
while pc < 80:
    op = block[pc]
    if op in opcodes:
        name, size = opcodes[op]
        if size == 1:
            print(f"  ${addr+pc:04X}: {op:02X}         {name}")
        elif size == 2:
            operand = block[pc+1]
            print(f"  ${addr+pc:04X}: {op:02X} {operand:02X}      {name}${operand:02X}")
        elif size == 3:
            lo = block[pc+1]
            hi = block[pc+2]
            print(f"  ${addr+pc:04X}: {op:02X} {lo:02X} {hi:02X}   {name}${hi:02X}{lo:02X}")
        
        if op == 0x08:  # PHP
            php_count += 1
            stack_depth += 1
            in_php = True
        elif op == 0x28:  # PLP
            plp_count += 1
            stack_depth -= 1
            in_php = False
        elif op == 0x48:  # PHA
            stack_depth += 1
        elif op == 0x68:  # PLA
            stack_depth -= 1
        elif in_php and op != 0xE6:  # Something other than INC after PHP
            if op != 0x28:
                print(f"    *** PHP NOT followed by PLP! Stack leak!")
                php_without_plp += 1
            in_php = False
            
        pc += size
        
        if op == 0x4C:  # JMP - end of block
            break
        if op == 0x60:  # RTS - end of block
            break
    else:
        print(f"  ${addr+pc:04X}: {op:02X}         ???")
        pc += 1

print(f"\n{'='*60}")
print(f"PHP count: {php_count}")
print(f"PLP count: {plp_count}")
print(f"Stack balance: {stack_depth} (should be 0)")
print(f"Missing PLPs after dirty flag: {php_without_plp}")

# Now scan ALL flash banks for PHP/INC $32 without PLP pattern
print(f"\n\n{'='*60}")
print("Scanning ALL flash banks for PHP INC $32 without PLP...")
print("Pattern: 08 E6 32 [not 28]")
f = open(r'c:\proj\c\NES\nesxidy-co\nesxidy\nes_donkeykong.nes', 'rb')
data = f.read()
f.close()

count = 0
for i in range(16, len(data) - 3):
    if data[i] == 0x08 and data[i+1] == 0xE6 and data[i+2] == 0x32:
        if i + 3 < len(data) and data[i+3] != 0x28:
            bank = (i - 16) // 16384
            offset_in_bank = (i - 16) % 16384
            mapped_addr = 0x8000 + offset_in_bank
            next_byte = data[i+3]
            count += 1
            if count <= 30:
                print(f"  Bank {bank:2d} ${mapped_addr:04X}: 08 E6 32 {next_byte:02X} (next={next_byte:02X}, NOT PLP)")
        
total_dirty = 0
for i in range(16, len(data) - 3):
    if data[i] == 0x08 and data[i+1] == 0xE6 and data[i+2] == 0x32:
        total_dirty += 1

with_plp = 0
for i in range(16, len(data) - 3):
    if data[i] == 0x08 and data[i+1] == 0xE6 and data[i+2] == 0x32:
        if i + 3 < len(data) and data[i+3] == 0x28:
            with_plp += 1

print(f"\nTotal dirty flag sequences (08 E6 32): {total_dirty}")
print(f"With PLP (08 E6 32 28): {with_plp}")
print(f"Without PLP: {count}")
