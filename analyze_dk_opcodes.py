#!/usr/bin/env python3
"""Read the DK ROM and disassemble the 43 interpreted PCs to see what instructions are there."""

ROM_PATH = r"c:\proj\c\NES\nesxidy-co\nesxidy\nes_donkeykong.nes"

# DK is NROM-128: 16KB PRG ($C000-$FFFF) + 8KB CHR
# iNES header is 16 bytes, then PRG ROM starts
# $C000 maps to PRG offset 0

# 6502 opcode names
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

# The 43 interpreted PCs from the trace analysis (sorted by frequency)
INTERPRETED_PCS = [
    0xF4BE, 0xF52A, 0xF52D, 0xF52F, 0xF530, 0xFBF6, 0xF228, 0xFDDF,
    0xFE31, 0xFE34, 0xFE36, 0xFE39, 0xFE3B, 0xF519, 0xF51B, 0xF51E,
    0xF520, 0xF22B, 0xF22D, 0xF22F, 0xF231, 0xF233, 0xF236, 0xF23F,
    0xC1C3, 0xC1C6, 0xC1C9, 0xC1CC, 0xC1CE, 0xFBF9, 0xFBFB, 0xFBFD,
    0xFBFF, 0xFC01, 0xFC04, 0xFC06, 0xC47B, 0xC47D, 0xF4C1, 0xF4C3,
    0xF4C5, 0xFE2E, 0xF527,
]

# Read ROM
with open(ROM_PATH, 'rb') as f:
    data = f.read()

# iNES header: 16 bytes
# PRG ROM starts at offset 16
# DK is NROM-128: 16KB PRG, $C000-$FFFF
PRG_START = 16
PRG_SIZE = 16384  # 16KB

prg = data[PRG_START:PRG_START + PRG_SIZE]

print(f"PRG ROM size: {len(prg)} bytes")
print(f"\n{'PC':>6}  {'Opcode':>4}  {'Instruction':<16}  {'Context (surrounding bytes)'}")
print("-" * 70)

# Group PCs into contiguous ranges for context
sorted_pcs = sorted(INTERPRETED_PCS)

for pc in sorted_pcs:
    offset = pc - 0xC000
    if 0 <= offset < len(prg):
        opcode = prg[offset]
        info = OPCODES.get(opcode, ("???", 1))
        name, size = info
        
        # Get operand bytes
        operand = ""
        if size == 2 and offset + 1 < len(prg):
            operand = f"${prg[offset+1]:02X}"
        elif size == 3 and offset + 2 < len(prg):
            operand = f"${prg[offset+2]:02X}{prg[offset+1]:02X}"
        
        # Get surrounding context (5 bytes before, instruction bytes, 5 bytes after)
        ctx_start = max(0, offset - 3)
        ctx_end = min(len(prg), offset + size + 3)
        ctx_bytes = " ".join(f"{prg[j]:02X}" for j in range(ctx_start, ctx_end))
        
        # Check if this is specifically an always-interpreted instruction
        always_interp = opcode in (0x58, 0x78, 0x08, 0x28, 0xF8)  # CLI, SEI, PHP, PLP, SED
        io_flag = ""
        if always_interp:
            io_flag = " <-- ALWAYS INTERPRETED"
        
        # Check if operand targets PPU/APU/I/O range ($2000-$401F)
        if size == 3:
            addr = prg[offset+1] | (prg[offset+2] << 8)
            if 0x2000 <= addr <= 0x401F:
                io_flag = f" <-- I/O ${addr:04X}"
        
        print(f"${pc:04X}  ${opcode:02X}    {name:<16} {operand:>6}  [{ctx_bytes}]{io_flag}")
    else:
        print(f"${pc:04X}  -- out of PRG range --")

# Now show contiguous groups
print("\n\n=== Contiguous groups ===")
groups = []
current_group = [sorted_pcs[0]]
for pc in sorted_pcs[1:]:
    # Check if this PC is within a few bytes of the last one  
    if pc - current_group[-1] <= 4:
        current_group.append(pc)
    else:
        groups.append(current_group)
        current_group = [pc]
groups.append(current_group)

for group in groups:
    start = group[0]
    end = group[-1]
    # Disassemble from start to end+3
    print(f"\nGroup ${start:04X}-${end:04X} ({len(group)} PCs):")
    off = start - 0xC000
    end_off = end - 0xC000 + 4
    addr = start
    while off < end_off and off < len(prg):
        opcode = prg[off]
        info = OPCODES.get(opcode, ("???", 1))
        name, size = info
        
        operand = ""
        raw = f"{opcode:02X}"
        if size >= 2 and off + 1 < len(prg):
            raw += f" {prg[off+1]:02X}"
            operand = f"${prg[off+1]:02X}"
        if size >= 3 and off + 2 < len(prg):
            raw += f" {prg[off+2]:02X}"
            operand = f"${prg[off+2]:02X}{prg[off+1]:02X}"

        marker = " <-- INTERPRETED" if addr in INTERPRETED_PCS else ""
        
        # Check for I/O
        if size == 3:
            tgt = prg[off+1] | (prg[off+2] << 8)
            if 0x2000 <= tgt <= 0x401F:
                marker += f" I/O"
        if opcode in (0x58, 0x78, 0x08, 0x28, 0xF8):
            marker += " ALWAYS-INTERP"
            
        print(f"  ${addr:04X}: {raw:<10} {name:<16} {operand}{marker}")
        off += size
        addr += size
