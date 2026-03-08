"""Check exidy.nes ROM at crash point: bank 21 ($15), address $8040"""
import sys

f = open('exidy.nes', 'rb')
data = f.read()
f.close()

header = data[:16]
prg_banks = header[4]
chr_banks = header[5]
flags6 = header[6]
print(f'PRG banks: {prg_banks} ({prg_banks*16}KB)')
print(f'CHR banks: {chr_banks} ({chr_banks*8}KB)')
print(f'Flags6: {flags6:#04x} (mapper={(flags6>>4)&0xF})')
print(f'ROM size: {len(data)} bytes')

# Check bank 21 ($15) - the crash bank
bank = 21
bank_offset = 16 + bank * 16384
if bank_offset + 16384 > len(data):
    print(f'Bank {bank} beyond ROM end!')
    sys.exit(1)
bank_data = data[bank_offset:bank_offset+16384]

print(f'\nBank {bank} ($15) at file offset {bank_offset:#x}:')
print('  First 128 bytes:')
for i in range(0, 128, 16):
    hexbytes = ' '.join(f'{bank_data[i+j]:02X}' for j in range(16))
    print(f'  ${0x8000+i:04X}: {hexbytes}')

# Check if this bank is erased/empty
ff_count = sum(1 for b in bank_data if b == 0xFF)
zero_count = sum(1 for b in bank_data if b == 0x00)
other = len(bank_data) - ff_count - zero_count
print(f'\n  FF bytes: {ff_count}/{len(bank_data)}')
print(f'  00 bytes: {zero_count}/{len(bank_data)}')
print(f'  Other:    {other}/{len(bank_data)}')

# Show what each bank contains (first pass)
print('\n\nBank summary (first 16 bytes + erased/empty status):')
total_prg = prg_banks * 16384
for b in range(prg_banks):
    off = 16 + b * 16384
    bd = data[off:off+16384]
    ff = sum(1 for x in bd if x == 0xFF)
    nz = sum(1 for x in bd if x != 0x00 and x != 0xFF)
    first16 = ' '.join(f'{bd[j]:02X}' for j in range(16))
    status = 'EMPTY(FF)' if ff == 16384 else ('EMPTY(00)' if nz == 0 else f'DATA({nz} non-trivial)')
    print(f'  Bank {b:2d} (${ b:02X}): {first16}  [{status}]')

# Check what's at $FB0A in the fixed bank (bank 31)
fixed_bank = prg_banks - 1
fixed_offset = 16 + fixed_bank * 16384
fixed_data = data[fixed_offset:fixed_offset+16384]
# $FB0A in fixed bank = $FB0A - $C000 = $3B0A
fb0a_off = 0x3B0A
print(f'\nFixed bank ({fixed_bank}), code at $FB0A:')
for i in range(0, 32, 16):
    hexbytes = ' '.join(f'{fixed_data[fb0a_off+i+j]:02X}' for j in range(16))
    print(f'  ${0xC000+fb0a_off+i:04X}: {hexbytes}')

# Also check what the mapper30 linker puts in each bank
# Search for the _metrics_dump_sa_b2 function (at $8040 in some bank)
print('\nSearching for _metrics_dump_sa_b2 content at offset $40 across banks:')
for b in range(prg_banks):
    off = 16 + b * 16384 + 0x40
    chunk = data[off:off+8]
    if any(x != 0xFF and x != 0x00 for x in chunk):
        hexbytes = ' '.join(f'{x:02X}' for x in chunk)
        print(f'  Bank {b:2d} ($8040): {hexbytes}')
