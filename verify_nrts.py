#!/usr/bin/env python3
"""Verify native RTS template from built ROM."""

with open(r"C:\proj\c\NES\nesxidy-co\nesxidy\exidy.nes", "rb") as f:
    rom = f.read()

HEADER = 16
BANK = 0x4000

def rom_off(bank, addr):
    return HEADER + bank * BANK + (addr & 0x3FFF)

# RTS template at $F621, size byte at $F641
off = rom_off(31, 0xF621)
size_off = rom_off(31, 0xF641)
size_byte = rom[size_off]
print(f"Native RTS template size byte: {size_byte}")

template = rom[off:off+size_byte]
print(f"Raw bytes ({size_byte} bytes):")
for i in range(0, size_byte, 16):
    hex_str = ' '.join(f'{b:02X}' for b in template[i:i+16])
    print(f"  +{i:02X}: {hex_str}")

# Decode
print(f"\nDecoded:")
i = 0
addr = 0xF621
while i < size_byte:
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
    elif b == 0xE8:
        print(f"  +{i:02X}: INX")
        i += 1
    elif b == 0xBD:
        print(f"  +{i:02X}: LDA ${template[i+2]:02X}{template[i+1]:02X},X")
        i += 3
    elif b == 0xE6:
        print(f"  +{i:02X}: INC ${template[i+1]:02X}")
        i += 2
    elif b == 0xD0:
        target = addr + i + 2 + (template[i+1] if template[i+1] < 128 else template[i+1] - 256)
        print(f"  +{i:02X}: BNE ${target:04X} (offset {template[i+1]:+d})")
        i += 2
    elif b == 0x4C:
        print(f"  +{i:02X}: JMP ${template[i+2]:02X}{template[i+1]:02X}")
        i += 3
    else:
        print(f"  +{i:02X}: ${b:02X} (???)")
        i += 1

# Verify the flow
print(f"\nExecution trace:")
print(f"  PHP            - save flags (matched by PLA in flash_dispatch_return_no_regs)")
print(f"  STA _a         - save A")
print(f"  STX _x         - save X") 
print(f"  STY _y         - save Y")
print(f"  LDX _sp        - load emulated SP")
print(f"  INX            - sp+1")
print(f"  LDA stack,X    - read lo byte of return addr")
print(f"  STA _pc        - store lo")
print(f"  INX            - sp+2")
print(f"  LDA stack,X    - read hi byte of return addr")
print(f"  STA _pc+1      - store hi")
print(f"  STX _sp        - save updated SP (sp+2)")
print(f"  INC _pc        - add 1 (6502 RTS convention: addr+1)")
print(f"  BNE skip       - skip if no page crossing")
print(f"  INC _pc+1      - handle carry")
print(f"  JMP no_regs    - exit (pops PHP, returns 0 to dispatcher/trampoline)")
