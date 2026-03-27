#!/usr/bin/env python3
"""Parse vicemap.map for all symbols in fixed bank $C000-$FFFF."""
import re

map_path = r'c:\proj\c\NES\nesxidy-co\nesxidy\vicemap.map'
syms = {}
with open(map_path) as f:
    for line in f:
        m = re.match(r'al C:([0-9a-fA-F]{4})\s+\.(\S+)', line)
        if m:
            addr = int(m.group(1), 16)
            name = m.group(2)
            if 0xC000 <= addr <= 0xFFFF:
                syms[addr] = name

# Print ALL symbols sorted by address
for addr in sorted(syms.keys()):
    print(f'  ${addr:04X}: {syms[addr]}')

# Now identify which ZP $20/$21 accesses belong to which function
print()
print('='*60)
print('ZP $20/$21 accesses by function:')
print('='*60)

rom_path = r'c:\proj\c\NES\nesxidy-co\nesxidy\nes_millipede.nes'
with open(rom_path, 'rb') as f:
    data = f.read()
prg_banks = data[4]
bank31_offset = 16 + (prg_banks - 1) * 16384
bank31 = data[bank31_offset:bank31_offset + 16384]

zp_ops = {
    0xA5: 'LDA', 0x85: 'STA', 0xA6: 'LDX', 0x86: 'STX',
    0xA4: 'LDY', 0x84: 'STY', 0x05: 'ORA', 0x25: 'AND',
    0x45: 'EOR', 0xC5: 'CMP', 0xE4: 'CPX', 0xC4: 'CPY',
    0xE6: 'INC', 0xC6: 'DEC', 0x06: 'ASL', 0x46: 'LSR',
    0x26: 'ROL', 0x66: 'ROR', 0x24: 'BIT', 0x64: 'STZ',
    0x65: 'ADC', 0xE5: 'SBC',
    0xB1: 'LDA(ind),Y', 0x91: 'STA(ind),Y',
    0xA1: 'LDA(ind,X)', 0x81: 'STA(ind,X)',
    0xB2: 'LDA(ind)', 0x92: 'STA(ind)',
}

sorted_addrs = sorted(syms.keys())

def find_function(addr):
    """Find which function contains this address."""
    best = None
    for sa in sorted_addrs:
        if sa <= addr:
            best = syms[sa]
        else:
            break
    return best or '???'

for target_zp in (0x20, 0x21):
    print(f'\n  --- ZP ${target_zp:02X} ---')
    for i in range(len(bank31) - 1):
        if bank31[i] in zp_ops and bank31[i+1] == target_zp:
            addr = 0xC000 + i
            op = zp_ops[bank31[i]]
            fn = find_function(addr)
            is_indirect = '(ind)' in op
            rw = 'W' if op.startswith(('STA','STX','STY','STZ','INC','DEC','ASL','LSR','ROL','ROR')) else 'R'
            kind = 'sp-indirect' if is_indirect else 'DIRECT'
            print(f'    ${addr:04X}: {op:12s} ${target_zp:02X}  [{rw}] [{kind:11s}]  in {fn}')
