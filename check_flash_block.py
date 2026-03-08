"""Check flash ROM at the JIT block address that caused the crash"""
import sys

f = open(r'c:\proj\c\NES\nesxidy-co\nesxidy\nes_donkeykong.nes', 'rb')
header = f.read(16)
prg_banks = header[4]
chr_banks = header[5]
print(f'PRG banks: {prg_banks}, CHR banks: {chr_banks}')
print(f'ROM size: {16 + prg_banks * 16384 + chr_banks * 8192} bytes')

# Read bank 4
bank4_offset = 16 + 4 * 16384
f.seek(bank4_offset)
bank4 = f.read(16384)

# Check $AA40 mapped: $AA40 - $8000 = $2A40
offset = 0x2A40
print(f'\nBank 4, $AA40 region (JIT block that crashed):')
for i in range(0, 128, 16):
    hexbytes = ' '.join(f'{bank4[offset+i+j]:02X}' for j in range(16))
    print(f'  ${0xAA40+i:04X}: {hexbytes}')

# Check if the block is all zeros
block_data = bank4[offset:offset+128]
zero_count = sum(1 for b in block_data if b == 0)
print(f'\nZero bytes in first 128: {zero_count}/128')

# Search entire bank 4 for non-zero regions
print(f'\nBank 4 non-zero regions:')
in_nonzero = False
for i in range(0, len(bank4)):
    if bank4[i] != 0:
        if not in_nonzero:
            in_nonzero = True
            start = i
    else:
        if in_nonzero:
            in_nonzero = False
            if i - start > 4:  # only show regions > 4 bytes
                print(f'  ${0x8000+start:04X}-${0x8000+i-1:04X} ({i-start} bytes)')
if in_nonzero:
    print(f'  ${0x8000+start:04X}-${0x8000+len(bank4)-1:04X} ({len(bank4)-start} bytes)')

# Count total non-zero bytes in bank 4
nz = sum(1 for b in bank4 if b != 0)
print(f'\nTotal non-zero bytes in bank 4: {nz}/{len(bank4)}')

# Also check address table at bank $1A, offset $B526
# Bank $1A = 26
bank1A_offset = 16 + 0x1A * 16384
f.seek(bank1A_offset)
bank1A = f.read(16384)
# $B526 - $8000 = $3526
at_offset = 0x3526
print(f'\nAddress table at bank $1A, $B526:')
for i in range(0, 16, 2):
    lo = bank1A[at_offset + i]
    hi = bank1A[at_offset + i + 1]
    print(f'  ${0xB526+i:04X}: ${hi:02X}{lo:02X} (addr=${hi:02X}{lo:02X})')

# Check page table at bank $1E, offset $BA93
# Bank $1E = 30
bank1E_offset = 16 + 0x1E * 16384
f.seek(bank1E_offset)
bank1E = f.read(16384)
# $BA93 - $8000 = $3A93
pt_offset = 0x3A93
print(f'\nPage table at bank $1E, $BA93:')
for i in range(0, 16):
    val = bank1E[pt_offset + i]
    print(f'  ${0xBA93+i:04X}: ${val:02X} (bank={val & 0x1F}, flags=${val & 0xE0:02X})')

f.close()
