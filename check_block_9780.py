"""Examine the flash block at $9780 in bank 5"""
f = open(r'c:\proj\c\NES\nesxidy-co\nesxidy\nes_donkeykong.nes', 'rb')
data = f.read()
f.close()

# Block at $9780 - bank 5 (from dispatch trace)
bank = 5
offset = 16 + bank * 16384
bank_data = data[offset:offset+16384]

# $9780 - $8000 = $1780
block_off = 0x1780
print(f'Bank {bank}, raw bytes at $9780:')
for i in range(0, 64, 16):
    hexbytes = ' '.join(f'{bank_data[block_off+i+j]:02X}' for j in range(16))
    print(f'  ${0x8000+block_off+i:04X}: {hexbytes}')

# Search backward for sentinel $AA to find block header
print('\nSearching for block header (sentinel $AA)...')
for check_off in range(block_off - 1, max(0, block_off - 64), -1):
    if bank_data[check_off] == 0xAA:
        hdr_start = check_off - 7
        if hdr_start < 0:
            continue
        header = bank_data[hdr_start:hdr_start+8]
        entry_lo, entry_hi = header[0], header[1]
        exit_lo, exit_hi = header[2], header[3]
        code_len = header[4]
        epil_off = header[5]
        byte6 = header[6]
        sentinel = header[7]
        code_start_off = hdr_start + 8
        mapped = 0x8000 + code_start_off
        print(f'  Header at ${0x8000+hdr_start:04X}:')
        print(f'    entry_pc = ${entry_hi:02X}{entry_lo:02X}')
        print(f'    exit_pc  = ${exit_hi:02X}{exit_lo:02X}')
        print(f'    code_len = {code_len}')
        print(f'    epil_off = {epil_off}')
        print(f'    byte6    = ${byte6:02X}')
        print(f'    sentinel = ${sentinel:02X}')
        print(f'    code at  = ${mapped:04X}')
        if mapped == 0x9780:
            print('    *** MATCH: this is the dispatch target!')
        code = bank_data[code_start_off:code_start_off+min(code_len, 80)]
        print(f'    Code ({code_len} bytes): {" ".join(f"{b:02X}" for b in code)}')
        
        # Check stack balance
        php_count = sum(1 for b in code if b == 0x08)
        plp_count = sum(1 for b in code if b == 0x28)
        print(f'    PHP count: {php_count}, PLP count: {plp_count}, balance: {php_count - plp_count}')
        break

# Also scan the block_prefix area
BLOCK_PREFIX_SIZE = 1  # Usually 1 byte (NOP or similar)
print(f'\nLet me check various prefix sizes:')
for prefix in range(0, 5):
    candidate_hdr = block_off - prefix - 8
    if candidate_hdr < 0:
        continue
    if bank_data[candidate_hdr + 7] == 0xAA:
        hdr = bank_data[candidate_hdr:candidate_hdr+8]
        entry_lo, entry_hi = hdr[0], hdr[1]
        code_start_off = candidate_hdr + 8 + prefix
        mapped_code = 0x8000 + code_start_off
        print(f'  prefix={prefix}: hdr at ${0x8000+candidate_hdr:04X}, entry=${entry_hi:02X}{entry_lo:02X}, code at ${mapped_code:04X}')
        if mapped_code == 0x9780:
            print('    *** MATCH!')
            code_len = hdr[4]
            code = bank_data[code_start_off:code_start_off+min(code_len, 80)]
            print(f'    Code: {" ".join(f"{b:02X}" for b in code)}')
