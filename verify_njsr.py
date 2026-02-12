#!/usr/bin/env python3
"""Verify native JSR template size and patch offsets from the built ROM."""

with open(r"C:\proj\c\NES\nesxidy-co\nesxidy\exidy.nes", "rb") as f:
    rom = f.read()

HEADER = 16
BANK = 0x4000

def rom_off(bank, addr):
    return HEADER + bank * BANK + (addr & 0x3FFF)

# Template at $F4F4 (bank 31)
template_addr = 0xF4F4
end_addr = 0xF51C
off = rom_off(31, template_addr)
size = end_addr - template_addr
print(f"Native JSR template at ${template_addr:04X}, size={size} bytes")

template = rom[off:off+size]
print(f"Raw bytes:")
for i in range(0, size, 16):
    hex_str = ' '.join(f'{b:02X}' for b in template[i:i+16])
    print(f"  +{i:02X}: {hex_str}")

# Size byte
size_off = rom_off(31, 0xF51C)
size_byte = rom[size_off]
print(f"\n_opcode_6502_njsr_size byte: {size_byte}")

# Patch offsets
ret_hi_off = rom_off(31, 0xF51D)
ret_lo_off = rom_off(31, 0xF51E)
tgt_lo_off = rom_off(31, 0xF51F)
tgt_hi_off = rom_off(31, 0xF520)
print(f"Patch offsets:")
print(f"  ret_hi: {rom[ret_hi_off]} (should be 12 = offset of LDA #hi immediate)")
print(f"  ret_lo: {rom[ret_lo_off]} (should be 18 = offset of LDA #lo immediate)")
print(f"  tgt_lo: {rom[tgt_lo_off]} (should be 26 = offset of LDA #tgt_lo)")
print(f"  tgt_hi: {rom[tgt_hi_off]} (should be 30 = offset of LDA #tgt_hi)")

# Verify template structure
print(f"\nDecoded template:")
i = 0
while i < size:
    b = template[i]
    if b == 0x08:
        print(f"  +{i:02X}: PHP")
        i += 1
    elif b == 0x85:
        print(f"  +{i:02X}: STA ${template[i+1]:02X}")
        i += 2
    elif b == 0x86:
        print(f"  +{i:02X}: STX ${template[i+1]:02X}")
        i += 2
    elif b == 0x84:
        print(f"  +{i:02X}: STY ${template[i+1]:02X}")
        i += 2
    elif b == 0xA6:
        print(f"  +{i:02X}: LDX ${template[i+1]:02X}")
        i += 2
    elif b == 0xA9:
        print(f"  +{i:02X}: LDA #${template[i+1]:02X}")
        i += 2
    elif b == 0x9D:
        print(f"  +{i:02X}: STA ${template[i+2]:02X}{template[i+1]:02X},X")
        i += 3
    elif b == 0xCA:
        print(f"  +{i:02X}: DEX")
        i += 1
    elif b == 0x4C:
        print(f"  +{i:02X}: JMP ${template[i+2]:02X}{template[i+1]:02X}")
        i += 3
    elif b == 0x20:
        print(f"  +{i:02X}: JSR ${template[i+2]:02X}{template[i+1]:02X}")
        i += 3
    else:
        print(f"  +{i:02X}: ${b:02X} (???)")
        i += 1

# Check trampoline in WRAM at $626D
print(f"\n=== Native JSR trampoline at $626D ===")
# This is in the data section which gets loaded to WRAM
# Need to find it in ROM at its link address
# The "data" section maps to WRAM $6000-$7FFF
# But it's stored in ROM... let me check where

# The trampoline is at $626D. In the vicemap:
# _dispatch_on_pc = $61F4
# _native_jsr_trampoline = $626D
# These are WRAM addresses. The data section in ROM is loaded to WRAM at boot.
# Let me find the trampoline bytes in the ROM by searching near dispatch_on_pc
# Actually, the "data" section is stored in a specific ROM bank. Let me check.

# From previous analysis, dispatch_on_pc is in WRAM which is loaded from ROM.
# The mapper30 linker config determines where the data section lives in ROM.
# Let's just search for the trampoline bytes pattern in the ROM

# The trampoline should start with: JSR dispatch_on_pc ($20 $F4 $61)
search = bytes([0x20, 0xF4, 0x61])
pos = 0
while pos < len(rom):
    p = rom.find(search, pos)
    if p == -1:
        break
    bank = (p - HEADER) // BANK
    addr = 0x8000 + ((p - HEADER) % BANK)
    print(f"  Found JSR $61F4 at ROM offset 0x{p:X} (bank {bank}, ${addr:04X})")
    # Show context
    for j in range(0, 32, 16):
        hex_str = ' '.join(f'{b:02X}' for b in rom[p+j:p+j+16])
        print(f"    +{j:02X}: {hex_str}")
    pos = p + 1

# Verify saved_sp ZP at $B6
print(f"\n_native_jsr_saved_sp ZP address: $B6")
