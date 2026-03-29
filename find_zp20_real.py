#!/usr/bin/env python3
"""Properly disassemble to find REAL ZP $20 accesses (instruction-boundary-aware)."""

rom_path = r'c:\proj\c\NES\nesxidy-co\nesxidy\nes_donkeykong.nes'
with open(rom_path, 'rb') as f:
    data = f.read()

bank31_offset = 16 + 31 * 16384
bank31 = data[bank31_offset:bank31_offset + 16384]

# Full 6502 instruction size table
inst_sizes = [0]*256
# 1-byte (implied/accumulator)
for op in [0x00,0x08,0x18,0x28,0x38,0x40,0x48,0x58,0x60,0x68,0x78,
           0x88,0x8A,0x98,0x9A,0xA8,0xAA,0xB8,0xBA,0xC8,0xCA,
           0xD8,0xE8,0xEA,0xF8,0x0A,0x2A,0x4A,0x6A]:
    inst_sizes[op] = 1
# 2-byte (immediate, zp, zp,x, zp,y, (zp,x), (zp),y, relative)
for op in [0x09,0x29,0x49,0x69,0x89,0xA0,0xA2,0xA9,0xC0,0xC9,0xE0,0xE9,
           0x05,0x06,0x15,0x16,0x24,0x25,0x26,0x35,0x36,0x45,0x46,0x55,0x56,
           0x65,0x66,0x75,0x76,0x84,0x85,0x86,0x94,0x95,0x96,0xA4,0xA5,0xA6,
           0xB4,0xB5,0xB6,0xC4,0xC5,0xC6,0xD5,0xD6,0xE4,0xE5,0xE6,0xF5,0xF6,
           0x01,0x11,0x21,0x31,0x41,0x51,0x61,0x71,0x81,0x91,0xA1,0xB1,0xC1,0xD1,0xE1,0xF1,
           0x10,0x30,0x50,0x70,0x90,0xB0,0xD0,0xF0,
           0x64,0x74]:  # STZ zp, STZ zp,X (65C02)
    inst_sizes[op] = 2
# 3-byte (absolute, abs,x, abs,y, indirect)
for op in [0x0C,0x0D,0x0E,0x19,0x1D,0x1E,0x20,0x2C,0x2D,0x2E,0x39,0x3D,0x3E,
           0x4C,0x4D,0x4E,0x59,0x5D,0x5E,0x6C,0x6D,0x6E,0x79,0x7D,0x7E,
           0x8C,0x8D,0x8E,0x99,0x9C,0x9D,0x9E,0xAC,0xAD,0xAE,0xB9,0xBC,0xBD,0xBE,
           0xCC,0xCD,0xCE,0xD9,0xDD,0xDE,0xEC,0xED,0xEE,0xF9,0xFC,0xFD,0xFE]:
    inst_sizes[op] = 3
# Fill unknown as 1 (safe default)
for i in range(256):
    if inst_sizes[i] == 0:
        inst_sizes[i] = 1

# ZP opcodes that access the operand byte as a ZP address
zp_access_opcodes = {
    0x05:'ORA zp', 0x06:'ASL zp', 0x15:'ORA zp,X', 0x16:'ASL zp,X',
    0x24:'BIT zp', 0x25:'AND zp', 0x26:'ROL zp', 0x35:'AND zp,X', 0x36:'ROL zp,X',
    0x45:'EOR zp', 0x46:'LSR zp', 0x55:'EOR zp,X', 0x56:'LSR zp,X',
    0x65:'ADC zp', 0x66:'ROR zp', 0x75:'ADC zp,X', 0x76:'ROR zp,X',
    0x84:'STY zp', 0x85:'STA zp', 0x86:'STX zp', 0x94:'STY zp,X', 0x95:'STA zp,X', 0x96:'STX zp,Y',
    0xA4:'LDY zp', 0xA5:'LDA zp', 0xA6:'LDX zp', 0xB4:'LDY zp,X', 0xB5:'LDA zp,X', 0xB6:'LDX zp,Y',
    0xC4:'CPY zp', 0xC5:'CMP zp', 0xC6:'DEC zp', 0xD5:'CMP zp,X', 0xD6:'DEC zp,X',
    0xE4:'CPX zp', 0xE5:'SBC zp', 0xE6:'INC zp', 0xF5:'SBC zp,X', 0xF6:'INC zp,X',
    0x01:'ORA (zp,X)', 0x11:'ORA (zp),Y', 0x21:'AND (zp,X)', 0x31:'AND (zp),Y',
    0x41:'EOR (zp,X)', 0x51:'EOR (zp),Y', 0x61:'ADC (zp,X)', 0x71:'ADC (zp),Y',
    0x81:'STA (zp,X)', 0x91:'STA (zp),Y', 0xA1:'LDA (zp,X)', 0xB1:'LDA (zp),Y',
    0xC1:'CMP (zp,X)', 0xD1:'CMP (zp),Y', 0xE1:'SBC (zp,X)', 0xF1:'SBC (zp),Y',
    0x64:'STZ zp', 0x74:'STZ zp,X',
}

# Track which addresses are instruction starts
inst_starts = set()
i = 0
while i < len(bank31):
    inst_starts.add(i)
    i += inst_sizes[bank31[i]]

print('=== REAL ZP $20 accesses in fixed bank (instruction-boundary-aware) ===')
real_accesses = []
for offset in sorted(inst_starts):
    if offset + 1 >= len(bank31):
        continue
    opcode = bank31[offset]
    if opcode in zp_access_opcodes and bank31[offset + 1] == 0x20:
        addr = 0xC000 + offset
        desc = zp_access_opcodes[opcode]
        # Show context: 3 bytes before and after
        ctx_start = max(0, offset - 6)
        ctx_end = min(len(bank31), offset + 6)
        real_accesses.append((addr, opcode, desc))
        print(f'  ${addr:04X}: {opcode:02X} 20  {desc}')

print(f'\nTotal real ZP $20 accesses: {len(real_accesses)}')

# Now show context around each in the NMI handler / lazyNES region ($C000-$C2D8)
print('\n=== ZP $20 accesses in lazyNES code ($C000-$C2D8) with context ===')
for addr, opcode, desc in real_accesses:
    if 0xC000 <= addr <= 0xC2D8:
        # Disassemble a few instructions around it
        print(f'\n--- ${addr:04X}: {desc} $20 ---')
        # Find a good starting point
        start = addr - 0xC000 - 10
        if start < 0: start = 0
        # Walk forward from start to align to instruction boundary
        ii = 0
        while ii < start:
            ii += inst_sizes[bank31[ii]]
        for _ in range(8):
            if ii >= len(bank31): break
            a = 0xC000 + ii
            sz = inst_sizes[bank31[ii]]
            raw = ' '.join(f'{bank31[ii+j]:02X}' for j in range(sz))
            marker = ' <<<' if a == addr else ''
            print(f'    ${a:04X}: {raw:12s}{marker}')
            ii += sz
