"""Decode the compiled cache blocks from the hex dump provided by the user.
Parse block headers and native code to understand what's compiled."""

# The hex dump from ROM $10000 (flash bank 4, CPU $8000-$BFFF when bank 4 selected)
hex_data = """
FF FF FF FF FF FF FF FF 2B 79 2E 79 2B 04 FF AA
A2 FF 86 69 08 18 90 04 28 4C FF FF 85 6A A9 2E
85 67 A9 79 85 68 4C 19 62 08 85 6A A9 FF 8D 89
62 A9 FF 8D 8A 62 A9 FF 4C 82 62 FF FF FF FF FF
FF FF FF FF FF FF FF FF 30 79 3B 79 33 0C FF AA
9D 00 71 9D 00 72 9D 00 73 9D 00 74 08 18 90 04
28 4C FF FF 85 6A A9 3B 85 67 A9 79 85 68 4C 19
62 08 85 6A A9 FF 8D 89 62 A9 FF 8D 8A 62 A9 FF
4C 82 62 FF FF FF FF FF 48 79 4A 79 2C 05 FF AA
D0 BE EA EA EA 08 18 90 04 28 4C FF FF 85 6A A9
4A 85 67 A9 79 85 68 4C 19 62 08 85 6A A9 FF 8D
89 62 A9 FF 8D 8A 62 A9 FF 4C 82 62 FF FF FF FF
FF FF FF FF FF FF FF FF 56 79 58 79 29 02 FF AA
A2 07 08 18 90 04 28 4C FF FF 85 6A A9 58 85 67
A9 79 85 68 4C 19 62 08 85 6A A9 FF 8D 89 62 A9
FF 8D 8A 62 A9 FF 4C 82 62 FF FF FF FF FF FF FF
FF FF FF FF FF FF FF FF 62 79 66 79 3C 15 FF AA
85 6A 08 A9 58 85 67 A9 79 85 68 A5 6A 28 30 03
4C F0 FF A2 07 08 18 90 04 28 4C FF FF 85 6A A9
66 85 67 A9 79 85 68 4C 19 62 08 85 6A A9 FF 8D
89 62 A9 FF 8D 8A 62 A9 FF 4C 82 62 FF FF FF FF
FF FF FF FF FF FF FF FF 79 79 85 79 57 30 FF AA
85 6A 08 A9 66 85 67 A9 79 85 68 A5 6A 28 30 03
4C F0 FF A2 00 BD 00 71 85 6A 08 A9 D7 85 67 A9
79 85 68 A5 6A 28 F0 03 4C F0 FF A9 11 9D 00 71
08 18 90 04 28 4C FF FF 85 6A A9 85 85 67 A9 79
85 68 4C 19 62 08 85 6A A9 FF 8D 89 62 A9 FF 8D
8A 62 A9 FF 4C 82 62 FF 86 79 8A 79 3D 16 FF AA
5D 00 71 85 6A 08 A9 D7 85 67 A9 79 85 68 A5 6A
28 F0 03 4C F0 FF 08 18 90 04 28 4C FF FF 85 6A
A9 8A 85 67 A9 79 85 68 4C 19 62 08 85 6A A9 FF
8D 89 62 A9 FF 8D 8A 62 A9 FF 4C 82 62 FF FF FF
FF FF FF FF FF FF FF FF 8B 79 93 79 42 1B FF AA
49 FF 9D 00 71 5D 00 71 85 6A 08 A9 D7 85 67 A9
79 85 68 A5 6A 28 F0 03 4C F0 FF 08 18 90 04 28
4C FF FF 85 6A A9 93 85 67 A9 79 85 68 4C 19 62
08 85 6A A9 FF 8D 89 62 A9 FF 8D 8A 62 A9 FF 4C
82 62 FF FF FF FF FF FF 95 79 97 79 2C 05 FF AA
B0 03 4C 8D 81 08 18 90 04 28 4C FF FF 85 6A A9
97 85 67 A9 79 85 68 4C 19 62 08 85 6A A9 FF 8D
89 62 A9 FF 8D 8A 62 A9 FF 4C 82 62 FF FF FF FF
FF FF FF FF FF FF FF FF 98 79 9A 79 2C 05 FF AA
F0 03 4C 75 81 08 18 90 04 28 4C FF FF 85 6A A9
9A 85 67 A9 79 85 68 4C 19 62 08 85 6A A9 FF 8D
89 62 A9 FF 8D 8A 62 A9 FF 4C 82 62 FF FF FF FF
FF FF FF FF FF FF FF FF 9E 79 A0 79 2A 03 FF AA
8D 9B 71 08 18 90 04 28 4C FF FF 85 6A A9 A0 85
67 A9 79 85 68 4C 19 62 08 85 6A A9 FF 8D 89 62
A9 FF 8D 8A 62 A9 FF 4C 82 62 FF FF FF FF FF FF
FF FF FF FF FF FF FF FF A1 79 A7 79 2E 07 FF AA
8D 9C 71 A0 00 A2 11 08 18 90 04 28 4C FF FF 85
6A A9 A7 85 67 A9 79 85 68 4C 19 62 08 85 6A A9
FF 8D 89 62 A9 FF 8D 8A 62 A9 FF 4C 82 62 FF FF
FF FF FF FF FF FF FF FF A9 79 AB 79 3A 13 FF AA
85 6A 08 A9 DD 85 67 A9 79 85 68 A5 6A 28 F0 03
4C F0 FF 08 18 90 04 28 4C FF FF 85 6A A9 AB 85
67 A9 79 85 68 4C 19 62 08 85 6A A9 FF 8D 89 62
A9 FF 8D 8A 62 A9 FF 4C 82 62 FF FF FF FF FF FF
FF FF FF FF FF FF FF FF B0 79 B2 79 3A 13 FF AA
85 6A 08 A9 DD 85 67 A9 79 85 68 A5 6A 28 F0 03
4C F0 FF 08 18 90 04 28 4C FF FF 85 6A A9 B2 85
67 A9 79 85 68 4C 19 62 08 85 6A A9 FF 8D 89 62
A9 FF 8D 8A 62 A9 FF 4C 82 62 FF FF FF FF FF FF
FF FF FF FF FF FF FF FF B3 79 B5 79 29 02 FF AA
49 FF 08 18 90 04 28 4C FF FF 85 6A A9 B5 85 67
A9 79 85 68 4C 19 62 08 85 6A A9 FF 8D 89 62 A9
FF 8D 8A 62 A9 FF 4C 82 62 FF FF FF FF FF FF FF
FF FF FF FF FF FF FF FF B9 79 BB 79 3A 13 FF AA
85 6A 08 A9 DD 85 67 A9 79 85 68 A5 6A 28 F0 03
4C F0 FF 08 18 90 04 28 4C FF FF 85 6A A9 BB 85
67 A9 79 85 68 4C 19 62 08 85 6A A9 FF 8D 89 62
A9 FF 8D 8A 62 A9 FF 4C 82 62 FF FF FF FF FF FF
FF FF FF FF FF FF FF FF BE 79 C0 79 3A 13 FF AA
85 6A 08 A9 AB 85 67 A9 79 85 68 A5 6A 28 B0 03
4C F0 FF 08 18 90 04 28 4C FF FF 85 6A A9 C0 85
67 A9 79 85 68 4C 19 62 08 85 6A A9 FF 8D 89 62
A9 FF 8D 8A 62 A9 FF 4C 82 62 FF FF FF FF FF FF
FF FF FF FF FF FF FF FF C1 79 C3 79 2C 05 FF AA
F0 03 4C 25 83 08 18 90 04 28 4C FF FF 85 6A A9
C3 85 67 A9 79 85 68 4C 19 62 08 85 6A A9 FF 8D
89 62 A9 FF 8D 8A 62 A9 FF 4C 82 62 FF FF FF FF
"""

# Parse hex data
import re
bytes_data = []
for line in hex_data.strip().split('\n'):
    line = line.strip()
    if not line:
        continue
    for b in line.split():
        bytes_data.append(int(b, 16))

print(f"Total bytes: {len(bytes_data)}")

# 6502 opcode table for disassembly
opcodes = {
    0x00: ("BRK", 1, "impl"), 0x01: ("ORA", 2, "(zp,X)"),
    0x05: ("ORA", 2, "zp"), 0x06: ("ASL", 2, "zp"),
    0x08: ("PHP", 1, "impl"), 0x09: ("ORA", 2, "#imm"),
    0x0A: ("ASL", 1, "A"), 0x10: ("BPL", 2, "rel"),
    0x11: ("ORA", 2, "(zp),Y"), 0x15: ("ORA", 2, "zp,X"),
    0x18: ("CLC", 1, "impl"), 0x19: ("ORA", 3, "abs,Y"),
    0x1D: ("ORA", 3, "abs,X"),
    0x20: ("JSR", 3, "abs"), 0x21: ("AND", 2, "(zp,X)"),
    0x24: ("BIT", 2, "zp"), 0x25: ("AND", 2, "zp"),
    0x26: ("ROL", 2, "zp"), 0x28: ("PLP", 1, "impl"),
    0x29: ("AND", 2, "#imm"), 0x2A: ("ROL", 1, "A"),
    0x2C: ("BIT", 3, "abs"), 0x2D: ("AND", 3, "abs"),
    0x30: ("BMI", 2, "rel"), 0x31: ("AND", 2, "(zp),Y"),
    0x35: ("AND", 2, "zp,X"), 0x38: ("SEC", 1, "impl"),
    0x39: ("AND", 3, "abs,Y"), 0x3D: ("AND", 3, "abs,X"),
    0x40: ("RTI", 1, "impl"), 0x41: ("EOR", 2, "(zp,X)"),
    0x45: ("EOR", 2, "zp"), 0x46: ("LSR", 2, "zp"),
    0x48: ("PHA", 1, "impl"), 0x49: ("EOR", 2, "#imm"),
    0x4A: ("LSR", 1, "A"), 0x4C: ("JMP", 3, "abs"),
    0x4D: ("EOR", 3, "abs"), 0x50: ("BVC", 2, "rel"),
    0x51: ("EOR", 2, "(zp),Y"), 0x55: ("EOR", 2, "zp,X"),
    0x58: ("CLI", 1, "impl"), 0x59: ("EOR", 3, "abs,Y"),
    0x5D: ("EOR", 3, "abs,X"),
    0x60: ("RTS", 1, "impl"), 0x61: ("ADC", 2, "(zp,X)"),
    0x65: ("ADC", 2, "zp"), 0x66: ("ROR", 2, "zp"),
    0x68: ("PLA", 1, "impl"), 0x69: ("ADC", 2, "#imm"),
    0x6A: ("ROR", 1, "A"), 0x6C: ("JMP", 3, "(abs)"),
    0x6D: ("ADC", 3, "abs"), 0x70: ("BVS", 2, "rel"),
    0x71: ("ADC", 2, "(zp),Y"), 0x75: ("ADC", 2, "zp,X"),
    0x78: ("SEI", 1, "impl"), 0x79: ("ADC", 3, "abs,Y"),
    0x7D: ("ADC", 3, "abs,X"),
    0x81: ("STA", 2, "(zp,X)"), 0x84: ("STY", 2, "zp"),
    0x85: ("STA", 2, "zp"), 0x86: ("STX", 2, "zp"),
    0x88: ("DEY", 1, "impl"), 0x8A: ("TXA", 1, "impl"),
    0x8C: ("STY", 3, "abs"), 0x8D: ("STA", 3, "abs"),
    0x8E: ("STX", 3, "abs"), 0x90: ("BCC", 2, "rel"),
    0x91: ("STA", 2, "(zp),Y"), 0x94: ("STY", 2, "zp,X"),
    0x95: ("STA", 2, "zp,X"), 0x96: ("STX", 2, "zp,Y"),
    0x98: ("TYA", 1, "impl"), 0x99: ("STA", 3, "abs,Y"),
    0x9A: ("TXS", 1, "impl"), 0x9D: ("STA", 3, "abs,X"),
    0xA0: ("LDY", 2, "#imm"), 0xA1: ("LDA", 2, "(zp,X)"),
    0xA2: ("LDX", 2, "#imm"), 0xA4: ("LDY", 2, "zp"),
    0xA5: ("LDA", 2, "zp"), 0xA6: ("LDX", 2, "zp"),
    0xA8: ("TAY", 1, "impl"), 0xA9: ("LDA", 2, "#imm"),
    0xAA: ("TAX", 1, "impl"), 0xAC: ("LDY", 3, "abs"),
    0xAD: ("LDA", 3, "abs"), 0xAE: ("LDX", 3, "abs"),
    0xB0: ("BCS", 2, "rel"), 0xB1: ("LDA", 2, "(zp),Y"),
    0xB4: ("LDY", 2, "zp,X"), 0xB5: ("LDA", 2, "zp,X"),
    0xB6: ("LDX", 2, "zp,Y"), 0xB9: ("LDA", 3, "abs,Y"),
    0xBA: ("TSX", 1, "impl"), 0xBD: ("LDA", 3, "abs,X"),
    0xC0: ("CPY", 2, "#imm"), 0xC1: ("CMP", 2, "(zp,X)"),
    0xC4: ("CPY", 2, "zp"), 0xC5: ("CMP", 2, "zp"),
    0xC6: ("DEC", 2, "zp"), 0xC8: ("INY", 1, "impl"),
    0xC9: ("CMP", 2, "#imm"), 0xCA: ("DEX", 1, "impl"),
    0xCC: ("CPY", 3, "abs"), 0xCD: ("CMP", 3, "abs"),
    0xCE: ("DEC", 3, "abs"), 0xD0: ("BNE", 2, "rel"),
    0xD1: ("CMP", 2, "(zp),Y"), 0xD5: ("CMP", 2, "zp,X"),
    0xD8: ("CLD", 1, "impl"), 0xD9: ("CMP", 3, "abs,Y"),
    0xDD: ("CMP", 3, "abs,X"), 0xDE: ("DEC", 3, "abs,X"),
    0xE0: ("CPX", 2, "#imm"), 0xE1: ("SBC", 2, "(zp,X)"),
    0xE4: ("CPX", 2, "zp"), 0xE5: ("SBC", 2, "zp"),
    0xE6: ("INC", 2, "zp"), 0xE8: ("INX", 1, "impl"),
    0xE9: ("SBC", 2, "#imm"), 0xEA: ("NOP", 1, "impl"),
    0xEC: ("CPX", 3, "abs"), 0xED: ("SBC", 3, "abs"),
    0xEE: ("INC", 3, "abs"), 0xF0: ("BEQ", 2, "rel"),
    0xF1: ("SBC", 2, "(zp),Y"), 0xF5: ("SBC", 2, "zp,X"),
    0xF8: ("SED", 1, "impl"), 0xF9: ("SBC", 3, "abs,Y"),
    0xFD: ("SBC", 3, "abs,X"), 0xFE: ("INC", 3, "abs,X"),
}

def disasm(data, offset, base_addr):
    """Disassemble a few instructions starting at offset."""
    result = []
    pos = offset
    while pos < len(data) and pos < offset + 64:
        if pos >= len(data):
            break
        op = data[pos]
        if op not in opcodes:
            result.append((pos - offset + base_addr, f".db ${op:02X}"))
            pos += 1
            continue
        name, size, mode = opcodes[op]
        if size == 1:
            result.append((pos - offset + base_addr, f"{name}"))
        elif size == 2:
            if pos + 1 >= len(data):
                break
            operand = data[pos + 1]
            if 'rel' in mode:
                target = (pos - offset + base_addr) + 2 + (operand if operand < 128 else operand - 256)
                result.append((pos - offset + base_addr, f"{name} ${target:04X}"))
            elif '#' in mode:
                result.append((pos - offset + base_addr, f"{name} #${operand:02X}"))
            else:
                result.append((pos - offset + base_addr, f"{name} ${operand:02X}"))
        elif size == 3:
            if pos + 2 >= len(data):
                break
            lo = data[pos + 1]
            hi = data[pos + 2]
            addr16 = (hi << 8) | lo
            result.append((pos - offset + base_addr, f"{name} ${addr16:04X}"))
        pos += size
    return result

# Parse blocks
print("=== DECODED BLOCKS ===\n")
block_num = 0
i = 0
while i < len(bytes_data) - 8:
    # Look for block headers - sentinel $AA at offset +7
    # Headers are 8-byte aligned (we look at 8-byte boundaries in the data)
    # But actually blocks may start at different alignments depending on the allocator
    
    # Find next header by looking for $AA sentinel
    if bytes_data[i + 7] != 0xAA:
        i += 8  # Skip to next potential header position (8-byte aligned within sectors)
        continue
    
    # Verify it's likely a header (entry_pc should be in $4000-$7FFF range for Millipede)
    entry_pc = bytes_data[i] | (bytes_data[i+1] << 8)
    exit_pc = bytes_data[i+2] | (bytes_data[i+3] << 8)
    code_len = bytes_data[i+4]
    epi_off = bytes_data[i+5]
    cycles = bytes_data[i+6]
    sentinel = bytes_data[i+7]
    
    if not (0x4000 <= entry_pc <= 0x7FFF):
        i += 8
        continue
    
    block_num += 1
    code_start = i + 8
    base_addr = 0x8000 + code_start  # CPU address in flash
    
    print(f"--- Block #{block_num} at flash offset ${i:04X} (CPU ${0x8000+i:04X}) ---")
    print(f"  entry_pc=${entry_pc:04X}  exit_pc=${exit_pc:04X}  code_len=${code_len:02X}({code_len})")
    print(f"  epilogue_offset=${epi_off:02X}({epi_off})  cycles=${cycles:02X}  sentinel=${sentinel:02X}")
    print(f"  Guest code bytes (entry..exit): {exit_pc - entry_pc} bytes")
    print(f"  Code at CPU ${base_addr:04X}:")
    
    # Disassemble the native code
    end = min(code_start + code_len, len(bytes_data))
    code_data = bytes_data[code_start:end]
    disasm_result = disasm(bytes_data, code_start, base_addr)
    
    # Mark epilogue boundary
    for addr, inst in disasm_result:
        epi_addr = base_addr + epi_off
        marker = " <-- EPILOGUE" if addr == epi_addr else ""
        marker2 = " <-- XBANK" if addr == base_addr + epi_off + 21 else ""  
        print(f"    ${addr:04X}: {inst}{marker}{marker2}")
    
    print()
    
    # Move past this block (code_len from the code start, but round up to alignment)
    # Blocks seem to be at multiples of some size. Let's just advance by the block span.
    i = code_start + code_len
    # Align to next 8-byte boundary
    while i % 8 != 0 and i < len(bytes_data):
        i += 1
    
    if block_num >= 20:
        print("(showing first 20 blocks only)")
        break

print(f"\nTotal blocks found: {block_num}")
