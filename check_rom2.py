import os

rom_path = r'c:\proj\c\NES\nesxidy-co\nesxidy\millipede.nes'
with open(rom_path, 'rb') as f:
    rom = f.read()

header_size = 16
bank_size = 0x4000

# Disassemble bank 20 to find function entry points
bank20_offset = header_size + 20 * bank_size
bank20 = rom[bank20_offset:bank20_offset + bank_size]

# Search for vbcc function prologues: SEC (38) LDA sp (A5 20) SBC # (E9)
print("=== Function entry points in bank 20 (SEC; LDA sp; SBC #xx pattern) ===")
for i in range(len(bank20) - 4):
    if bank20[i] == 0x38 and bank20[i+1] == 0xA5 and bank20[i+2] == 0x20 and bank20[i+3] == 0xE9:
        frame_size = bank20[i+4]
        addr = 0x8000 + i
        print(f"  ${addr:04X}: SEC; LDA sp; SBC #${frame_size:02X}  (frame size = {frame_size})")

# Also look for CLC; LDA sp; ADC # pattern (frame deallocation / epilogues)
print("\n=== Function epilogues in bank 20 (CLC; LDA sp; ADC #xx pattern) ===")
for i in range(len(bank20) - 4):
    if bank20[i] == 0x18 and bank20[i+1] == 0xA5 and bank20[i+2] == 0x20 and bank20[i+3] == 0x69:
        frame_size = bank20[i+4]
        addr = 0x8000 + i
        print(f"  ${addr:04X}: CLC; LDA sp; ADC #${frame_size:02X}  (frame size = {frame_size})")

# Look for RTS instructions to find function boundaries
print("\n=== RTS instructions in bank 20 ===")
rts_addrs = []
for i in range(len(bank20)):
    if bank20[i] == 0x60:
        addr = 0x8000 + i
        rts_addrs.append(addr)
        
print(f"  Total RTS: {len(rts_addrs)}")
for a in rts_addrs[:30]:
    print(f"  ${a:04X}", end="")
print()

# Full disassembly around $810D
print("\n=== Disassembly around $810D ===")
# Simple 6502 disassembler for the relevant opcodes
oplen = {
    0x00: 1, 0x01: 2, 0x05: 2, 0x06: 2, 0x08: 1, 0x09: 2, 0x0A: 1, 0x0D: 3, 0x0E: 3,
    0x10: 2, 0x11: 2, 0x15: 2, 0x16: 2, 0x18: 1, 0x19: 3, 0x1D: 3, 0x1E: 3,
    0x20: 3, 0x21: 2, 0x24: 2, 0x25: 2, 0x26: 2, 0x28: 1, 0x29: 2, 0x2A: 1, 0x2C: 3, 0x2D: 3, 0x2E: 3,
    0x30: 2, 0x31: 2, 0x35: 2, 0x36: 2, 0x38: 1, 0x39: 3, 0x3D: 3, 0x3E: 3,
    0x40: 1, 0x41: 2, 0x45: 2, 0x46: 2, 0x48: 1, 0x49: 2, 0x4A: 1, 0x4C: 3, 0x4D: 3, 0x4E: 3,
    0x50: 2, 0x51: 2, 0x55: 2, 0x56: 2, 0x58: 1, 0x59: 3, 0x5D: 3, 0x5E: 3,
    0x60: 1, 0x61: 2, 0x65: 2, 0x66: 2, 0x68: 1, 0x69: 2, 0x6A: 1, 0x6C: 3, 0x6D: 3, 0x6E: 3,
    0x70: 2, 0x71: 2, 0x75: 2, 0x76: 2, 0x78: 1, 0x79: 3, 0x7D: 3, 0x7E: 3,
    0x81: 2, 0x84: 2, 0x85: 2, 0x86: 2, 0x88: 1, 0x8A: 1, 0x8C: 3, 0x8D: 3, 0x8E: 3,
    0x90: 2, 0x91: 2, 0x94: 2, 0x95: 2, 0x96: 2, 0x98: 1, 0x99: 3, 0x9A: 1, 0x9D: 3,
    0xA0: 2, 0xA1: 2, 0xA2: 2, 0xA4: 2, 0xA5: 2, 0xA6: 2, 0xA8: 1, 0xA9: 2, 0xAA: 1, 0xAC: 3, 0xAD: 3, 0xAE: 3,
    0xB0: 2, 0xB1: 2, 0xB4: 2, 0xB5: 2, 0xB6: 2, 0xB8: 1, 0xB9: 3, 0xBA: 1, 0xBC: 3, 0xBD: 3, 0xBE: 3,
    0xC0: 2, 0xC1: 2, 0xC4: 2, 0xC5: 2, 0xC6: 2, 0xC8: 1, 0xC9: 2, 0xCA: 1, 0xCC: 3, 0xCD: 3, 0xCE: 3,
    0xD0: 2, 0xD1: 2, 0xD5: 2, 0xD6: 2, 0xD8: 1, 0xD9: 3, 0xDD: 3, 0xDE: 3,
    0xE0: 2, 0xE1: 2, 0xE4: 2, 0xE5: 2, 0xE6: 2, 0xE8: 1, 0xE9: 2, 0xEA: 1, 0xEC: 3, 0xED: 3, 0xEE: 3,
    0xF0: 2, 0xF1: 2, 0xF5: 2, 0xF6: 2, 0xF8: 1, 0xF9: 3, 0xFD: 3, 0xFE: 3,
}

i = 0
# Start from $80F0 to get context before $810D
start = 0xF0
end_off = 0x140
i = start
while i < end_off and i < len(bank20):
    addr = 0x8000 + i
    op = bank20[i]
    length = oplen.get(op, 1)
    bytes_str = ' '.join(f'{bank20[i+j]:02X}' for j in range(min(length, end_off - i)))
    marker = " <-- JSR target $810D" if addr == 0x810D else ""
    marker = " <-- JSR at $810C" if addr == 0x810C else marker
    print(f"  ${addr:04X}: {bytes_str:10s}{marker}")
    i += length
