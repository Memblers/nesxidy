#!/usr/bin/env python3
"""Read DK game PRG from correct bank (20) in the nesxidy mapper 30 ROM."""

ROM_PATH = r"c:\proj\c\NES\nesxidy-co\nesxidy\nes_donkeykong.nes"
BANK_NES_PRG_LO = 20
BANK_SIZE = 16384  # 16KB

with open(ROM_PATH, 'rb') as f:
    data = f.read()

# Skip 16-byte iNES header
prg_offset = 16 + (BANK_NES_PRG_LO * BANK_SIZE)
prg_data = data[prg_offset:prg_offset + BANK_SIZE]
print(f"Reading bank {BANK_NES_PRG_LO} at file offset ${prg_offset:X}")
print(f"Bank data size: {len(prg_data)} bytes")

# For DK (NROM-128): $C000-$FFFF maps to this 16KB bank
# In the bank, offset 0 = $C000
BASE_ADDR = 0xC000

# Check vectors at end of bank
vec_off = 0xFFFA - BASE_ADDR  # offset in bank
nmi = prg_data[vec_off] | (prg_data[vec_off+1] << 8)
reset = prg_data[vec_off+2] | (prg_data[vec_off+3] << 8)
irq = prg_data[vec_off+4] | (prg_data[vec_off+5] << 8)
print(f"\nVectors: NMI=${nmi:04X} RESET=${reset:04X} IRQ=${irq:04X}")

# 6502 opcodes (abbreviated)
OPCODES = {
    0x00: ("BRK", 1), 0x01: ("ORA (ind,X)", 2), 0x05: ("ORA zpg", 2), 0x06: ("ASL zpg", 2),
    0x08: ("PHP", 1), 0x09: ("ORA #imm", 2), 0x0A: ("ASL A", 1), 0x0D: ("ORA abs", 3),
    0x0E: ("ASL abs", 3), 0x10: ("BPL rel", 2), 0x11: ("ORA (ind),Y", 2), 0x15: ("ORA zpg,X", 2),
    0x16: ("ASL zpg,X", 2), 0x18: ("CLC", 1), 0x19: ("ORA abs,Y", 3), 0x1D: ("ORA abs,X", 3),
    0x1E: ("ASL abs,X", 3), 0x20: ("JSR abs", 3), 0x21: ("AND (ind,X)", 2), 0x24: ("BIT zpg", 2),
    0x25: ("AND zpg", 2), 0x26: ("ROL zpg", 2), 0x28: ("PLP", 1), 0x29: ("AND #imm", 2),
    0x2A: ("ROL A", 1), 0x2C: ("BIT abs", 3), 0x2D: ("AND abs", 3), 0x2E: ("ROL abs", 3),
    0x30: ("BMI rel", 2), 0x31: ("AND (ind),Y", 2), 0x35: ("AND zpg,X", 2), 0x36: ("ROL zpg,X", 2),
    0x38: ("SEC", 1), 0x39: ("AND abs,Y", 3), 0x3D: ("AND abs,X", 3), 0x3E: ("ROL abs,X", 3),
    0x40: ("RTI", 1), 0x41: ("EOR (ind,X)", 2), 0x45: ("EOR zpg", 2), 0x46: ("LSR zpg", 2),
    0x48: ("PHA", 1), 0x49: ("EOR #imm", 2), 0x4A: ("LSR A", 1), 0x4C: ("JMP abs", 3),
    0x4D: ("EOR abs", 3), 0x4E: ("LSR abs", 3), 0x50: ("BVC rel", 2), 0x51: ("EOR (ind),Y", 2),
    0x55: ("EOR zpg,X", 2), 0x56: ("LSR zpg,X", 2), 0x58: ("CLI", 1), 0x59: ("EOR abs,Y", 3),
    0x5D: ("EOR abs,X", 3), 0x5E: ("LSR abs,X", 3), 0x60: ("RTS", 1), 0x61: ("ADC (ind,X)", 2),
    0x65: ("ADC zpg", 2), 0x66: ("ROR zpg", 2), 0x68: ("PLA", 1), 0x69: ("ADC #imm", 2),
    0x6A: ("ROR A", 1), 0x6C: ("JMP (ind)", 3), 0x6D: ("ADC abs", 3), 0x6E: ("ROR abs", 3),
    0x70: ("BVS rel", 2), 0x71: ("ADC (ind),Y", 2), 0x75: ("ADC zpg,X", 2), 0x76: ("ROR zpg,X", 2),
    0x78: ("SEI", 1), 0x79: ("ADC abs,Y", 3), 0x7D: ("ADC abs,X", 3), 0x7E: ("ROR abs,X", 3),
    0x81: ("STA (ind,X)", 2), 0x84: ("STY zpg", 2), 0x85: ("STA zpg", 2), 0x86: ("STX zpg", 2),
    0x88: ("DEY", 1), 0x8A: ("TXA", 1), 0x8C: ("STY abs", 3), 0x8D: ("STA abs", 3),
    0x8E: ("STX abs", 3), 0x90: ("BCC rel", 2), 0x91: ("STA (ind),Y", 2), 0x94: ("STY zpg,X", 2),
    0x95: ("STA zpg,X", 2), 0x96: ("STX zpg,Y", 2), 0x98: ("TYA", 1), 0x99: ("STA abs,Y", 3),
    0x9A: ("TXS", 1), 0x9D: ("STA abs,X", 3), 0xA0: ("LDY #imm", 2), 0xA1: ("LDA (ind,X)", 2),
    0xA2: ("LDX #imm", 2), 0xA4: ("LDY zpg", 2), 0xA5: ("LDA zpg", 2), 0xA6: ("LDX zpg", 2),
    0xA8: ("TAY", 1), 0xA9: ("LDA #imm", 2), 0xAA: ("TAX", 1), 0xAC: ("LDY abs", 3),
    0xAD: ("LDA abs", 3), 0xAE: ("LDX abs", 3), 0xB0: ("BCS rel", 2), 0xB1: ("LDA (ind),Y", 2),
    0xB4: ("LDY zpg,X", 2), 0xB5: ("LDA zpg,X", 2), 0xB6: ("LDX zpg,Y", 2), 0xB8: ("CLV", 1),
    0xB9: ("LDA abs,Y", 3), 0xBA: ("TSX", 1), 0xBC: ("LDY abs,X", 3), 0xBD: ("LDA abs,X", 3),
    0xBE: ("LDX abs,Y", 3), 0xC0: ("CPY #imm", 2), 0xC1: ("CMP (ind,X)", 2), 0xC4: ("CPY zpg", 2),
    0xC5: ("CMP zpg", 2), 0xC6: ("DEC zpg", 2), 0xC8: ("INY", 1), 0xC9: ("CMP #imm", 2),
    0xCA: ("DEX", 1), 0xCC: ("CPY abs", 3), 0xCD: ("CMP abs", 3), 0xCE: ("DEC abs", 3),
    0xD0: ("BNE rel", 2), 0xD1: ("CMP (ind),Y", 2), 0xD5: ("CMP zpg,X", 2), 0xD6: ("DEC zpg,X", 2),
    0xD8: ("CLD", 1), 0xD9: ("CMP abs,Y", 3), 0xDD: ("CMP abs,X", 3), 0xDE: ("DEC abs,X", 3),
    0xE0: ("CPX #imm", 2), 0xE1: ("SBC (ind,X)", 2), 0xE4: ("CPX zpg", 2), 0xE5: ("SBC zpg", 2),
    0xE6: ("INC zpg", 2), 0xE8: ("INX", 1), 0xE9: ("SBC #imm", 2), 0xEA: ("NOP", 1),
    0xEC: ("CPX abs", 3), 0xED: ("SBC abs", 3), 0xEE: ("INC abs", 3), 0xF0: ("BEQ rel", 2),
    0xF1: ("SBC (ind),Y", 2), 0xF5: ("SBC zpg,X", 2), 0xF6: ("INC zpg,X", 2), 0xF8: ("SED", 1),
    0xF9: ("SBC abs,Y", 3), 0xFD: ("SBC abs,X", 3), 0xFE: ("INC abs,X", 3),
}

# The 43 interpreted PCs from the trace analysis
INTERPRETED_PCS = sorted([
    0xF4BE, 0xF52A, 0xF52D, 0xF52F, 0xF530, 0xFBF6, 0xF228, 0xFDDF,
    0xFE31, 0xFE34, 0xFE36, 0xFE39, 0xFE3B, 0xF519, 0xF51B, 0xF51E,
    0xF520, 0xF22B, 0xF22D, 0xF22F, 0xF231, 0xF233, 0xF236, 0xF23F,
    0xC1C3, 0xC1C6, 0xC1C9, 0xC1CC, 0xC1CE, 0xFBF9, 0xFBFB, 0xFBFD,
    0xFBFF, 0xFC01, 0xFC04, 0xFC06, 0xC47B, 0xC47D, 0xF4C1, 0xF4C3,
    0xF4C5, 0xFE2E, 0xF527,
])

INTERP_SET = set(INTERPRETED_PCS)

# Disassemble contiguous groups
print(f"\n=== Interpreted PCs (from bank {BANK_NES_PRG_LO}, base=${BASE_ADDR:04X}) ===\n")

# Group into contiguous ranges
groups = []
current_group = [INTERPRETED_PCS[0]]
for pc in INTERPRETED_PCS[1:]:
    if pc - current_group[-1] <= 6:
        current_group.append(pc)
    else:
        groups.append(current_group)
        current_group = [pc]
groups.append(current_group)

for group in groups:
    start = group[0]
    end = group[-1]
    # Disassemble from a few bytes before to a few after
    dis_start = max(BASE_ADDR, start - 8)
    dis_end = min(BASE_ADDR + BANK_SIZE - 1, end + 8)
    
    print(f"--- Group ${start:04X}-${end:04X} ({len(group)} interpreted PCs) ---")
    
    off = dis_start - BASE_ADDR
    addr = dis_start
    while addr <= dis_end and off < len(prg_data):
        opcode = prg_data[off]
        info = OPCODES.get(opcode, (f"???({opcode:02X})", 1))
        name, size = info
        
        operand = ""
        raw = f"{opcode:02X}"
        if size >= 2 and off + 1 < len(prg_data):
            raw += f" {prg_data[off+1]:02X}"
            if "rel" in name:
                # branch target
                rel = prg_data[off+1]
                if rel >= 0x80: rel -= 256
                target = addr + 2 + rel
                operand = f"${target:04X}"
            elif "#" in name:
                operand = f"#${prg_data[off+1]:02X}"
            else:
                operand = f"${prg_data[off+1]:02X}"
        if size >= 3 and off + 2 < len(prg_data):
            raw += f" {prg_data[off+2]:02X}"
            abs_addr = prg_data[off+1] | (prg_data[off+2] << 8)
            operand = f"${abs_addr:04X}"
        
        marker = ""
        if addr in INTERP_SET:
            marker = " *** INTERPRETED ***"
        
        # I/O check
        if size == 3:
            tgt = prg_data[off+1] | (prg_data[off+2] << 8)
            if 0x2000 <= tgt <= 0x2007:
                marker += " [PPU]"
            elif 0x4000 <= tgt <= 0x4017:
                marker += " [APU/IO]"
        
        # Always-interpreted instructions
        if opcode in (0x58, 0x78, 0x08, 0x28, 0xF8):
            marker += " [ALWAYS-INTERP]"
            
        print(f"  ${addr:04X}: {raw:<10} {name:<18} {operand}{marker}")
        off += size
        addr += size
    print()
