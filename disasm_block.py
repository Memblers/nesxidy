#!/usr/bin/env python3
"""Disassemble the compiled native code block for guest PCs $3487-$3494"""

data = bytes([
    0xAC, 0xBB, 0x6D, 0xF0, 0x03, 0x4C, 0xF0, 0xAE,
    0x08, 0x86, 0x62, 0xA6, 0x60, 0x9D, 0xB8, 0x6E,
    0xC6, 0x60, 0xA6, 0x62, 0x28, 0xAC, 0xBB, 0x6D,
    0xB9, 0xFE, 0x6D, 0xA8, 0x08, 0x86, 0x62, 0xE6,
    0x60, 0xA6, 0x60, 0xBD, 0xB8, 0x6E, 0xA6, 0x62,
    0x08, 0x18, 0x90, 0x04, 0x28, 0x4C, 0xFF, 0xFF,
    0x85, 0x61, 0xA9, 0x94, 0x85, 0x5E, 0xA9, 0x34,
    0x85, 0x5F, 0x4C, 0x00, 0x62, 0x08, 0x85, 0x61,
    0xA9, 0xFF, 0x8D, 0x13, 0x62, 0xA9, 0xFF, 0x8D,
    0x14, 0x62, 0xA9, 0xFF, 0x4C, 0x0C, 0x62
])

# Header: entry=$3487, exit=$3494, code_len=79, epi_off=40

# Guest sequence:
# $3487: LDY $03          → LDY abs (ZP translated to abs)
# $3489: BNE $34C9        → forward branch (21-byte patchable OR direct)
# $348B: NOP              → skip (0 bytes emitted)
# $348C: PHA              → template (PHP..PLP)
# $348D: LDY $03          → LDY abs
# $348F: LDA $0046,Y      → LDA abs,Y
# $3492: TAY              → TAY (1 byte)
# $3493: PLA              → template (PHP..PLP)
# exit $3494              → epilogue

opcodes = {
    0x08: ('PHP', 'imp', 1), 0x28: ('PLP', 'imp', 1), 0x48: ('PHA', 'imp', 1), 0x68: ('PLA', 'imp', 1),
    0x18: ('CLC', 'imp', 1), 0x38: ('SEC', 'imp', 1), 0xEA: ('NOP', 'imp', 1), 0xA8: ('TAY', 'imp', 1),
    0x85: ('STA', 'zp', 2), 0x86: ('STX', 'zp', 2), 0xA6: ('LDX', 'zp', 2), 0xA5: ('LDA', 'zp', 2),
    0xA9: ('LDA', 'imm', 2), 0xC6: ('DEC', 'zp', 2), 0xE6: ('INC', 'zp', 2),
    0xAC: ('LDY', 'abs', 3), 0x8D: ('STA', 'abs', 3), 0xAD: ('LDA', 'abs', 3), 0xCD: ('CMP', 'abs', 3),
    0x4C: ('JMP', 'abs', 3), 0x9D: ('STA', 'abx', 3), 0xBD: ('LDA', 'abx', 3), 0xB9: ('LDA', 'aby', 3),
    0xF0: ('BEQ', 'rel', 2), 0xD0: ('BNE', 'rel', 2), 0x90: ('BCC', 'rel', 2), 0xB0: ('BCS', 'rel', 2),
    0x10: ('BPL', 'rel', 2), 0x30: ('BMI', 'rel', 2),
    0x20: ('JSR', 'abs', 3),
}

# Known ZP addresses
zp_names = {
    0x5E: '_pc', 0x5F: '_pc+1', 0x60: '_sp', 0x61: '_a', 0x62: '_x', 0x63: '_y', 0x64: '_status',
    0xBA: '_njsr_saved_sp', 0xBB: '_sp_mirror?',
}

# Known abs addresses
abs_names = {
    0x6200: '_cross_bank_dispatch', 0x6215: '_dispatch_on_pc',
    0x62AC: '_flash_dispatch_return', 0x62B0: '_flash_dispatch_return_no_regs',
    0x62BF: '_native_jsr_trampoline', 0x620C: '_xbank_trampoline',
    0x6DBB: 'RAM_BASE+$03', 0x6EB8: 'RAM_BASE+$100 (emul stack base)',
}

print("=== Disassembly of block entry=$3487 exit=$3494 ===")
print(f"Header: code_len=79, epi_off=40 (epilogue at +40 = $A698)")
print()

i = 0
base = 0xA670
while i < len(data):
    addr = base + i
    b = data[i]
    
    # Annotate epilogue
    if i == 40:
        print(f"  ---- epilogue starts here (offset {i}) ----")
    
    if b in opcodes:
        name, mode, sz = opcodes[b]
        raw = ' '.join(f'{data[i+j]:02X}' for j in range(min(sz, len(data)-i)))
        
        if mode == 'imp':
            print(f'  ${addr:04X}: {raw:8s}  {name}')
            i += 1
        elif mode == 'zp':
            op = data[i+1]
            zn = zp_names.get(op, '')
            print(f'  ${addr:04X}: {raw:8s}  {name} ${op:02X}  {zn}')
            i += 2
        elif mode == 'imm':
            op = data[i+1]
            print(f'  ${addr:04X}: {raw:8s}  {name} #${op:02X}')
            i += 2
        elif mode == 'abs':
            lo, hi = data[i+1], data[i+2]
            val = hi*256+lo
            an = abs_names.get(val, '')
            print(f'  ${addr:04X}: {raw:8s}  {name} ${val:04X}  {an}')
            i += 3
        elif mode == 'abx':
            lo, hi = data[i+1], data[i+2]
            val = hi*256+lo
            an = abs_names.get(val, '')
            print(f'  ${addr:04X}: {raw:8s}  {name} ${val:04X},X  {an}')
            i += 3
        elif mode == 'aby':
            lo, hi = data[i+1], data[i+2]
            val = hi*256+lo
            print(f'  ${addr:04X}: {raw:8s}  {name} ${val:04X},Y')
            i += 3
        elif mode == 'rel':
            off = data[i+1]
            if off > 127: off -= 256
            target = addr + 2 + off
            print(f'  ${addr:04X}: {raw:8s}  {name} ${target:04X}  (offset {off:+d})')
            i += 2
    else:
        print(f'  ${addr:04X}: {data[i]:02X}        .db ${b:02X}')
        i += 1

print()
print("=== ANALYSIS ===")
print("Guest: $3487 LDY $03 / $3489 BNE $34C9 / $348B NOP / $348C PHA / $348D LDY $03")
print("       $348F LDA $0046,Y / $3492 TAY / $3493 PLA / $3494 CMP... (exit)")
print()
print("Native code should be:")
print("  LDY abs  (LDY $03 → LDY $6DBB)")
print("  BNE handling (forward branch)")
print("  NOP (skip)")
print("  PHA template: PHP STX_x LDX_sp STA_stack,X DEC_sp LDX_x [PLP?]")
print("  LDY abs  (LDY $03)")
print("  LDA abs,Y ($0046,Y → $6DFE,Y)")
print("  TAY")
print("  PLA template: PHP STX_x INC_sp LDX_sp LDA_stack,X LDX_x [PLP?]")
print("  epilogue")
