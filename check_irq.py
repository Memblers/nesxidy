#!/usr/bin/env python3
"""Check IRQ-related stuff in the ROM."""
with open('exidy.nes', 'rb') as f:
    data = f.read()

# NES header is 16 bytes, then PRG ROM
# 32 banks of 16KB each = 512KB
# Fixed bank = bank 31 at $C000-$FFFF
header = 16

# Check vectors at end of ROM
vec_offset = header + 32 * 0x4000 - 6  # last 6 bytes
nmi = data[vec_offset] | (data[vec_offset+1] << 8)
reset = data[vec_offset+2] | (data[vec_offset+3] << 8)
irq = data[vec_offset+4] | (data[vec_offset+5] << 8)
print(f"NMI vector:   ${nmi:04X}")
print(f"RESET vector: ${reset:04X}")
print(f"IRQ vector:   ${irq:04X}")

# Check what's at the IRQ handler
irq_rom_offset = header + 31 * 0x4000 + (irq - 0xC000)
print(f"\nIRQ handler at ${irq:04X} (ROM offset {irq_rom_offset:#x}):")
for i in range(8):
    print(f"  ${irq+i:04X}: ${data[irq_rom_offset+i]:02X}")
print(f"  First byte: ${data[irq_rom_offset]:02X} = {'RTI' if data[irq_rom_offset]==0x40 else 'NOT RTI'}")

# Search for $4017 writes in fixed bank  
fixed_offset = header + 31 * 0x4000
fixed = data[fixed_offset:fixed_offset + 0x4000]
print(f"\nSearching for $4017 references in fixed bank...")
pos = 0
count = 0
while True:
    pos = fixed.find(bytes([0x17, 0x40]), pos)
    if pos == -1:
        break
    # Show context
    start = max(0, pos - 3)
    end = min(len(fixed), pos + 5)
    ctx = ' '.join(f'{fixed[i]:02X}' for i in range(start, end))
    print(f"  Fixed+${pos:04X} (=${0xC000+pos:04X}): ...{ctx}...")
    count += 1
    pos += 1
if count == 0:
    print("  None found!")

# Search for STA $4017 (8D 17 40) specifically
print(f"\nSearching for STA $4017 (8D 17 40)...")
pos = 0
while True:
    pos = fixed.find(bytes([0x8D, 0x17, 0x40]), pos)
    if pos == -1:
        break
    print(f"  ${0xC000+pos:04X}: STA $4017")
    pos += 1

# Check startup code around reset vector
reset_offset = header + 31 * 0x4000 + (reset - 0xC000) if reset >= 0xC000 else header + (reset - 0x8000)
print(f"\nReset handler at ${reset:04X}:")
for i in range(0, 32, 1):
    print(f"  ${reset+i:04X}: ${data[reset_offset+i]:02X}", end='')
    if (i+1) % 8 == 0:
        print()

# Check the lazynes startup init to see if it sets $4017
print(f"\nSearching for LDA #$40 / STA $4017 pattern...")
for i in range(len(fixed) - 5):
    if fixed[i] == 0xA9 and fixed[i+1] == 0x40 and fixed[i+2] == 0x8D and fixed[i+3] == 0x17 and fixed[i+4] == 0x40:
        print(f"  ${0xC000+i:04X}: LDA #$40 / STA $4017 - DISABLES frame IRQ")
