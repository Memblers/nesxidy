#!/usr/bin/env python3
"""Find all instructions accessing ZP $20 (nfOamFull) in the NES ROM fixed bank."""

rom_path = r'c:\proj\c\NES\nesxidy-co\nesxidy\nes_donkeykong.nes'
with open(rom_path, 'rb') as f:
    data = f.read()

bank31_offset = 16 + 31 * 16384
bank31 = data[bank31_offset:bank31_offset + 16384]

# ZP access opcodes with $20 operand
zp_opcodes = {
    0xA5: 'LDA', 0x85: 'STA', 0xA6: 'LDX', 0x86: 'STX',
    0xA4: 'LDY', 0x84: 'STY', 0x05: 'ORA', 0x25: 'AND',
    0x45: 'EOR', 0xC5: 'CMP', 0xE4: 'CPX', 0xC4: 'CPY',
    0xE6: 'INC', 0xC6: 'DEC', 0x06: 'ASL', 0x46: 'LSR',
    0x26: 'ROL', 0x66: 'ROR', 0x24: 'BIT',
}

print('=== Instructions accessing ZP $20 (nfOamFull) in fixed bank ===')
for i in range(len(bank31) - 1):
    if bank31[i] in zp_opcodes and bank31[i+1] == 0x20:
        addr = 0xC000 + i
        op = zp_opcodes[bank31[i]]
        print(f'  ${addr:04X}: {bank31[i]:02X} 20  {op} $20')

# Also check indirect-Y access to ZP $20
for i in range(len(bank31) - 1):
    if bank31[i] == 0xB1 and bank31[i+1] == 0x20:  # LDA ($20),Y
        addr = 0xC000 + i
        print(f'  ${addr:04X}: B1 20  LDA ($20),Y')
    if bank31[i] == 0x91 and bank31[i+1] == 0x20:  # STA ($20),Y
        addr = 0xC000 + i
        print(f'  ${addr:04X}: 91 20  STA ($20),Y')

# Also dump the NMI vector to confirm entry point
nmi_vec_lo = bank31[0x3FFA]  # $FFFA
nmi_vec_hi = bank31[0x3FFB]  # $FFFB
reset_vec_lo = bank31[0x3FFC]  # $FFFC
reset_vec_hi = bank31[0x3FFD]  # $FFFD
irq_vec_lo = bank31[0x3FFE]  # $FFFE
irq_vec_hi = bank31[0x3FFF]  # $FFFF

print(f'\n=== NES Vectors ===')
print(f'  NMI:   ${nmi_vec_hi:02X}{nmi_vec_lo:02X}')
print(f'  RESET: ${reset_vec_hi:02X}{reset_vec_lo:02X}')
print(f'  IRQ:   ${irq_vec_hi:02X}{irq_vec_lo:02X}')

# Now search specifically in the NMI handler range ($C192-$C213)
print(f'\n=== NMI handler ($C192-$C213) ZP accesses ===')
i = 0xC192 - 0xC000
end = 0xC214 - 0xC000
zp_all = {}
zp_all.update(zp_opcodes)
while i < end:
    b = bank31[i]
    if b in zp_all:
        zp_addr = bank31[i+1]
        op = zp_all[b]
        addr = 0xC000 + i
        rw = 'W' if op in ('STA','STX','STY','INC','DEC','ASL','LSR','ROL','ROR') else 'R'
        print(f'  ${addr:04X}: {b:02X} {zp_addr:02X}  {op} ${zp_addr:02X}  [{rw}]')
        i += 2
    else:
        # Skip instruction
        sizes = {
            1: [0x48,0x8A,0x98,0x68,0xAA,0xA8,0x60,0x40,0xEA,0x18,0x38,0xE8,0xCA,0xC8,0x88,0x0A],
            2: [0xA9,0xA2,0xA0,0x09,0x29,0x49,0xC9,0xE0,0xC0,0xF0,0xD0,0x10,0x30,0x50,0x70,0x90,0xB0],
            3: [0x0D,0x2D,0xAD,0xAE,0xAC,0x8D,0x8E,0x8C,0x2C,0x4C,0x20,0xCC,0xCD,0xEC,0x4D,0x9D,0xBD,0xFC,0x9E],
        }
        found_size = 1
        for sz, ops in sizes.items():
            if b in ops:
                found_size = sz
                break
        i += found_size
