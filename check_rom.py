import os

rom_path = r'c:\proj\c\NES\nesxidy-co\nesxidy\millipede.nes'
with open(rom_path, 'rb') as f:
    rom = f.read()

header_size = 16
bank_size = 0x4000  # 16KB

# Bank 20 (0x14) - RENDER bank
bank20_offset = header_size + 20 * bank_size
print(f'Bank 20 offset in ROM: 0x{bank20_offset:X}')

# Address $810D is at offset $10D within the bank
offset_in_bank = 0x810D - 0x8000
print(f'Offset in bank: 0x{offset_in_bank:X}')

rom_offset = bank20_offset + offset_in_bank
print(f'ROM offset: 0x{rom_offset:X}')
bytes_at = rom[rom_offset:rom_offset+16]
print(f'Bytes at $810D: {" ".join(f"{b:02X}" for b in bytes_at)}')

# Also show context around it
print()
print(f'=== Bank 20, $8100-$8120 ===')
for addr in range(0x8100, 0x8130, 16):
    off = bank20_offset + (addr - 0x8000)
    bytes_str = ' '.join(f'{b:02X}' for b in rom[off:off+16])
    print(f'  ${addr:04X}: {bytes_str}')

# Also check the beginning of bank 20 
print()
print(f'=== Bank 20, $8000-$8040 ===')
for addr in range(0x8000, 0x8040, 16):
    off = bank20_offset + (addr - 0x8000)
    bytes_str = ' '.join(f'{b:02X}' for b in rom[off:off+16])
    print(f'  ${addr:04X}: {bytes_str}')

# Check bank 31 (fixed) to see the JSR $810D
bank31_offset = header_size + 31 * bank_size
off_cb2f = bank31_offset + (0xCB2F - 0xC000)
cb2f_bytes = rom[off_cb2f:off_cb2f+6]
print(f'\nBytes at $CB2F (bank 31): {" ".join(f"{b:02X}" for b in cb2f_bytes)}')
print(f'  (should be JSR $810D = 20 0D 81)')

# Check if bank 20 is all FF (unwritten flash)
bank20_data = rom[bank20_offset:bank20_offset + bank_size]
ff_count = sum(1 for b in bank20_data if b == 0xFF)
zero_count = sum(1 for b in bank20_data if b == 0x00)
print(f'\nBank 20 stats: {ff_count} FF bytes, {zero_count} 00 bytes, out of {bank_size}')

# Find the first non-FF byte in bank 20
first_nonff = None
for i, b in enumerate(bank20_data):
    if b != 0xFF:
        first_nonff = i
        break
if first_nonff is not None:
    print(f'First non-FF byte at offset 0x{first_nonff:04X} = 0x{bank20_data[first_nonff]:02X}')
else:
    print('Bank 20 is ALL 0xFF (empty/unwritten!)')
