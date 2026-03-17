"""Analyze the flash writes for the crashing block at $9A68-$9ABF"""
import os

f = r'C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\exidy.txt'
size = os.path.getsize(f)

# Read a large chunk - 10MB from end
with open(f, 'rb') as fh:
    fh.seek(max(0, size - 10_000_000))
    data = fh.read()
lines = data.decode('utf-8', errors='replace').splitlines()
print("Got %d lines from last 10MB" % len(lines))

# Find ALL flash byte program writes (STA (r2),Y [$xxxx]) to the $9A00-$9AFF range
# These are the actual flash writes happening at address $6041
flash_writes = {}  # addr -> (line_idx, value, line_text)
for i, line in enumerate(lines):
    # Flash byte program write pattern: STA (r2),Y [$xxxx] = $yy  A:zz
    if '6041' in line and 'STA (r2)' in line:
        # Extract target address and value
        try:
            bracket_start = line.index('[$')
            bracket_end = line.index(']', bracket_start)
            addr_str = line[bracket_start+2:bracket_end]
            addr = int(addr_str, 16)
            
            # Extract A register value (the byte being written)
            a_idx = line.index('A:')
            a_val = int(line[a_idx+2:a_idx+4], 16)
            
            if 0x9A00 <= addr <= 0x9AFF:
                flash_writes[addr] = (i, a_val, line.rstrip()[:160])
        except (ValueError, IndexError):
            pass

print("\nFlash writes to $9A00-$9AFF range:")
for addr in sorted(flash_writes.keys()):
    idx, val, txt = flash_writes[addr]
    print("  $%04X <- $%02X  (line %d)" % (addr, val, idx))

# Reconstruct the compiled block
print("\n\n=== Reconstructed flash content $9A60-$9AFF ===")
block_bytes = {}
for addr in sorted(flash_writes.keys()):
    _, val, _ = flash_writes[addr]
    block_bytes[addr] = val

# Print as hex dump
for base in range(0x9A60, 0x9AB0, 16):
    hex_str = ""
    ascii_str = ""
    for off in range(16):
        a = base + off
        if a in block_bytes:
            hex_str += "%02X " % block_bytes[a]
        else:
            hex_str += "-- "
    print("$%04X: %s" % (base, hex_str))

# Disassemble the code starting at $9A70
print("\n\n=== Disassembly from $9A70 (block code entry) ===")
opcodes_1byte = {0x00: 'BRK', 0x08: 'PHP', 0x28: 'PLP', 0x48: 'PHA', 0x68: 'PLA',
                 0x0A: 'ASL A', 0x2A: 'ROL A', 0x4A: 'LSR A', 0x6A: 'ROR A',
                 0x18: 'CLC', 0x38: 'SEC', 0x58: 'CLI', 0x78: 'SEI',
                 0xAA: 'TAX', 0xA8: 'TAY', 0xBA: 'TSX', 0x8A: 'TXA',
                 0x9A: 'TXS', 0x98: 'TYA', 0xCA: 'DEX', 0xE8: 'INX',
                 0x88: 'DEY', 0xC8: 'INY', 0xD8: 'CLD', 0xF8: 'SED',
                 0xB8: 'CLV', 0xEA: 'NOP', 0x40: 'RTI', 0x60: 'RTS'}
opcodes_2byte = {0xA9: 'LDA', 0xA2: 'LDX', 0xA0: 'LDY', 0xC9: 'CMP',
                 0xE0: 'CPX', 0xC0: 'CPY', 0x09: 'ORA', 0x29: 'AND',
                 0x49: 'EOR', 0x69: 'ADC', 0xE9: 'SBC',
                 0x90: 'BCC', 0xB0: 'BCS', 0xF0: 'BEQ', 0xD0: 'BNE',
                 0x10: 'BPL', 0x30: 'BMI', 0x50: 'BVC', 0x70: 'BVS',
                 0xA5: 'LDA', 0xA6: 'LDX', 0xA4: 'LDY', 0x85: 'STA',
                 0x86: 'STX', 0x84: 'STY', 0xC5: 'CMP', 0xC6: 'DEC',
                 0xE6: 'INC', 0x05: 'ORA', 0x25: 'AND', 0x45: 'EOR',
                 0x65: 'ADC', 0xE5: 'SBC', 0x24: 'BIT', 0x06: 'ASL',
                 0x26: 'ROL', 0x46: 'LSR', 0x66: 'ROR'}
opcodes_3byte = {0x4C: 'JMP', 0x20: 'JSR', 0xAD: 'LDA', 0xAE: 'LDX',
                 0xAC: 'LDY', 0x8D: 'STA', 0x8E: 'STX', 0x8C: 'STY',
                 0xCD: 'CMP', 0xCE: 'DEC', 0xEE: 'INC', 0x0D: 'ORA',
                 0x2D: 'AND', 0x4D: 'EOR', 0x6D: 'ADC', 0xED: 'SBC',
                 0x2C: 'BIT', 0x0E: 'ASL', 0x2E: 'ROL', 0x4E: 'LSR',
                 0x6E: 'ROR', 0x6C: 'JMP()', 0xB1: 'LDA', 0x91: 'STA'}

addr = 0x9A70
while addr < 0x9AA0:
    if addr not in block_bytes:
        print("  $%04X: --" % addr)
        addr += 1
        continue
    
    b = block_bytes[addr]
    if b in opcodes_1byte:
        print("  $%04X: %02X        %s" % (addr, b, opcodes_1byte[b]))
        addr += 1
    elif b in opcodes_2byte:
        if (addr+1) in block_bytes:
            b2 = block_bytes[addr+1]
            name = opcodes_2byte[b]
            if b in (0x90, 0xB0, 0xF0, 0xD0, 0x10, 0x30, 0x50, 0x70):
                target = addr + 2 + ((b2 - 256) if b2 >= 128 else b2)
                print("  $%04X: %02X %02X     %s $%04X" % (addr, b, b2, name, target))
            elif b in (0xA5, 0xA6, 0xA4, 0x85, 0x86, 0x84, 0xC5, 0xC6, 0xE6, 0x05, 0x25, 0x45, 0x65, 0xE5, 0x24, 0x06, 0x26, 0x46, 0x66):
                print("  $%04X: %02X %02X     %s $%02X" % (addr, b, b2, name, b2))
            else:
                print("  $%04X: %02X %02X     %s #$%02X" % (addr, b, b2, name, b2))
        else:
            print("  $%04X: %02X --     %s ??" % (addr, b, opcodes_2byte[b]))
        addr += 2
    elif b in opcodes_3byte:
        if (addr+1) in block_bytes and (addr+2) in block_bytes:
            lo = block_bytes[addr+1]
            hi = block_bytes[addr+2]
            target = lo | (hi << 8)
            name = opcodes_3byte[b]
            print("  $%04X: %02X %02X %02X  %s $%04X" % (addr, b, lo, hi, name, target))
        else:
            print("  $%04X: %02X ?? ??  %s ???" % (addr, b, opcodes_3byte[b]))
        addr += 3
    else:
        print("  $%04X: %02X        ???" % (addr, b))
        addr += 1

# Also find what was happening right before the compile of this block
# Look for the outer compile loop calling recompile_opcode
print("\n\n=== Compile context: looking for the block compilation ===")
# The block header at $9A68 stores guest PC $32DF
# Find where the flash writes to $9A68 start
if 0x9A68 in flash_writes:
    first_write_idx = flash_writes[0x9A68][0]
    print("First write to $9A68 at line %d" % first_write_idx)
    # Show context before the first write
    start = max(0, first_write_idx - 100)
    end = min(len(lines), first_write_idx + 5)
    print("\n=== Context before block compilation ===")
    for j in range(start, end):
        print("  %d: %s" % (j, lines[j].rstrip()[:160]))
