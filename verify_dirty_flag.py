"""Verify the JSR-based dirty flag fix in the built ROM"""
import struct

f = open(r'c:\proj\c\NES\nesxidy-co\nesxidy\nes_donkeykong.nes', 'rb')
data = f.read()
f.close()

print(f"ROM size: {len(data)} bytes")
print(f"PRG banks: {data[4]}")

# Search for the dirty flag subroutines pattern in WRAM data section
# PHP(08) INC zp(E6 32) PLP(28) RTS(60)
screen_pattern = bytes([0x08, 0xE6, 0x32, 0x28, 0x60])
# PHP(08) INC zp(E6 33) PLP(28) RTS(60)  
char_pattern = bytes([0x08, 0xE6, 0x33, 0x28, 0x60])

print("\nSearching for dirty_flag_screen subroutine (08 E6 32 28 60)...")
for i in range(16, len(data) - len(screen_pattern)):
    if data[i:i+len(screen_pattern)] == screen_pattern:
        bank = (i - 16) // 16384
        offset_in_bank = (i - 16) % 16384
        mapped_addr = 0x8000 + offset_in_bank
        print(f"  Found at file offset {i:#x}, bank {bank}, mapped ${mapped_addr:04X}")

print("\nSearching for dirty_flag_char subroutine (08 E6 33 28 60)...")
for i in range(16, len(data) - len(char_pattern)):
    if data[i:i+len(char_pattern)] == char_pattern:
        bank = (i - 16) // 16384
        offset_in_bank = (i - 16) % 16384
        mapped_addr = 0x8000 + offset_in_bank
        print(f"  Found at file offset {i:#x}, bank {bank}, mapped ${mapped_addr:04X}")

# Search for JSR to dirty_flag routines (20 xx xx patterns near known addresses)
# First find the exact address by looking at the section "data" content
# The WRAM section starts at bank 0 mapped to $6000-$7FFF range
# Search for JSR patterns with consistent target addresses
print("\nSearching for JSR instructions that could be dirty flag calls...")
jsr_targets = {}
for i in range(16, len(data) - 2):
    if data[i] == 0x20:  # JSR opcode
        target = data[i+1] | (data[i+2] << 8)
        if 0x6000 <= target <= 0x7FFF:  # WRAM range
            if target not in jsr_targets:
                jsr_targets[target] = 0
            jsr_targets[target] += 1

# Show JSR targets in WRAM range with multiple hits
print("  JSR targets in WRAM range ($6000-$7FFF):")
for addr, count in sorted(jsr_targets.items()):
    if count >= 2:
        print(f"    JSR ${addr:04X}: {count} occurrences")

# Check for any remaining inline PHP/INC $32 without PLP (old pattern)
print("\nChecking for old-style inline dirty flag (08 E6 32 [not 28])...")
old_count = 0
for i in range(16, len(data) - 3):
    if data[i] == 0x08 and data[i+1] == 0xE6 and data[i+2] == 0x32:
        if i + 3 < len(data) and data[i+3] != 0x28:
            bank = (i - 16) // 16384
            offset_in_bank = (i - 16) % 16384
            mapped_addr = 0x8000 + offset_in_bank
            next_byte = data[i+3]
            old_count += 1
            print(f"  Bank {bank:2d} ${mapped_addr:04X}: 08 E6 32 {next_byte:02X}")
print(f"  Total old-style dirty flags without PLP: {old_count}")

# Total dirty flag sequences
total = 0
with_plp = 0
for i in range(16, len(data) - 3):
    if data[i] == 0x08 and data[i+1] == 0xE6 and data[i+2] == 0x32:
        total += 1
        if i + 3 < len(data) and data[i+3] == 0x28:
            with_plp += 1
print(f"  Total PHP/INC $32 sequences: {total}")
print(f"  With PLP: {with_plp}")
