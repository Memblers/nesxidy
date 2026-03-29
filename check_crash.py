# Check convert_chr_b2 in bank 25 specifically
rom = open('exidy.nes', 'rb').read()
header = 16
bank_size = 0x4000

# convert_chr is at $C989 in fixed bank (trampoline)
# The trampoline calls bankswitch_prg(BANK_INIT_CODE=25), then JSR convert_chr_b2
# convert_chr_b2's address should be in $8000-$BFFF range (bank25)

# Let's find convert_chr_b2's map address - it's static so may not be in vicemap.map
# The crash showed JSR $81E1 - so convert_chr_b2 is at $81E1

# Let's check what's at $81E1 in bank 25
offset = 0x81E1 - 0x8000
rom_offset = header + 25 * bank_size + offset
data = rom[rom_offset:rom_offset+32]
print(f"Bank 25 @ $81E1: {' '.join(f'{b:02X}' for b in data)}")

# For comparison, what's the start of bank 25 code?
# Let's find the first non-FF non-00 byte in bank 25
bank25_start = header + 25 * bank_size
bank25_data = rom[bank25_start:bank25_start + bank_size]
first_code = -1
for i, b in enumerate(bank25_data):
    if b != 0xFF and b != 0x00:
        first_code = i
        break
print(f"\nFirst non-FF/non-00 byte in bank 25: offset ${first_code:04X} (addr ${0x8000+first_code:04X})")
print(f"  Bytes: {' '.join(f'{b:02X}' for b in bank25_data[first_code:first_code+32])}")

# Let's also dump the actual convert_chr trampoline from the fixed bank
# $C989 is in bank 31 (fixed)
fixed_offset = 0xC989 - 0xC000
rom_offset = header + 31 * bank_size + fixed_offset
data = rom[rom_offset:rom_offset+64]

# Simple disassembly
print(f"\nDisassembly of convert_chr trampoline at $C989:")
pc = 0xC989
i = 0
mnemonics = {
    0xA5: ("LDA zp", 2), 0x85: ("STA zp", 2), 0xA9: ("LDA #", 2),
    0x20: ("JSR", 3), 0x60: ("RTS", 1), 0x48: ("PHA", 1), 0x68: ("PLA", 1),
    0x4C: ("JMP", 3), 0x38: ("SEC", 1), 0x18: ("CLC", 1),
    0xAD: ("LDA abs", 3), 0x8D: ("STA abs", 3),
}
while i < 48:
    op = data[i]
    if op in mnemonics:
        name, size = mnemonics[op]
        if size == 1:
            print(f"  ${pc:04X}: {op:02X}           {name}")
        elif size == 2:
            print(f"  ${pc:04X}: {op:02X} {data[i+1]:02X}        {name} ${data[i+1]:02X}")
        elif size == 3:
            addr = data[i+1] | (data[i+2] << 8)
            print(f"  ${pc:04X}: {op:02X} {data[i+1]:02X} {data[i+2]:02X}     {name} ${addr:04X}")
        i += size
        pc += size
        if op == 0x60:  # RTS
            print()
            break
    else:
        print(f"  ${pc:04X}: {op:02X}           ???")
        i += 1
        pc += 1

# Now let's look at what's actually AT the address convert_chr_b2 lands on in bank25
# The trampoline will show us the target address
