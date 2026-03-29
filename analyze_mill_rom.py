import os

rom_path = r'c:\proj\c\NES\nesxidy-co\nesxidy\millipede.nes'
with open(rom_path, 'rb') as f:
    rom = f.read()

header = 16
bank_size = 0x4000
# Bank 23 = Millipede ROM
bank23_off = header + 23 * bank_size
mill_rom = rom[bank23_off:bank23_off + bank_size]

# The Millipede ROM is at $4000-$7FFF in guest address space
# Reset vector at $7FFC-$7FFD (offset $3FFC within ROM)
reset_lo = mill_rom[0x3FFC]
reset_hi = mill_rom[0x3FFD]
reset_vec = reset_lo | (reset_hi << 8)
print(f'Reset vector: ${reset_vec:04X}')

# IRQ/NMI vector at $7FFE-$7FFF
irq_lo = mill_rom[0x3FFE]
irq_hi = mill_rom[0x3FFF]
irq_vec = irq_lo | (irq_hi << 8)
print(f'IRQ vector: ${irq_vec:04X}')

# NMI vector at $7FFA-$7FFB
nmi_lo = mill_rom[0x3FFA]
nmi_hi = mill_rom[0x3FFB]
nmi_vec = nmi_lo | (nmi_hi << 8)
print(f'NMI vector: ${nmi_vec:04X}')

# Show first ~60 bytes of code at reset vector
off = reset_vec - 0x4000
print(f'\n=== Code at reset vector ${reset_vec:04X} (offset 0x{off:04X}) ===')
for i in range(min(96, len(mill_rom) - off)):
    if i % 16 == 0:
        addr = reset_vec + i
        print(f'\n  ${addr:04X}: ', end='')
    print(f'{mill_rom[off + i]:02X} ', end='')
print()

# Show first ~60 bytes at IRQ vector
off_irq = irq_vec - 0x4000
print(f'\n=== Code at IRQ vector ${irq_vec:04X} (offset 0x{off_irq:04X}) ===')
for i in range(min(64, len(mill_rom) - off_irq)):
    if i % 16 == 0:
        addr = irq_vec + i
        print(f'\n  ${addr:04X}: ', end='')
    print(f'{mill_rom[off_irq + i]:02X} ', end='')
print()

# Check what the ROM does with video RAM ($1000-$13BF)
# Look for writes in the $10xx-$13xx range in the ROM
# STA abs pattern: 8D xx 1y where y = 0,1,2,3
print('\n=== STA $1xxx instructions in ROM ===')
count = 0
for i in range(len(mill_rom) - 2):
    if mill_rom[i] == 0x8D:  # STA absolute
        hi = mill_rom[i+2]
        lo = mill_rom[i+1]
        if hi >= 0x10 and hi <= 0x13:
            addr = lo | (hi << 8)
            rom_addr = 0x4000 + i
            if count < 20:
                print(f'  ${rom_addr:04X}: STA ${addr:04X}')
            count += 1
print(f'  Total STA $1xxx: {count}')

# Check for palette writes ($2480-$249F)
print('\n=== STA $24xx instructions in ROM ===')
count = 0
for i in range(len(mill_rom) - 2):
    if mill_rom[i] == 0x8D:  # STA absolute
        hi = mill_rom[i+2]
        lo = mill_rom[i+1]
        if hi == 0x24:
            addr = lo | (hi << 8)
            rom_addr = 0x4000 + i
            if count < 20:
                print(f'  ${rom_addr:04X}: STA ${addr:04X}')
            count += 1
print(f'  Total STA $24xx: {count}')

# Check for $20xx writes (IRQ ack etc)
print('\n=== STA $20xx instructions in ROM ===')
count = 0
for i in range(len(mill_rom) - 2):
    if mill_rom[i] == 0x8D:
        hi = mill_rom[i+2]
        lo = mill_rom[i+1]
        if hi == 0x20:
            addr = lo | (hi << 8)
            rom_addr = 0x4000 + i
            if count < 20:
                print(f'  ${rom_addr:04X}: STA ${addr:04X}')
            count += 1
print(f'  Total STA $20xx: {count}')
