"""Find the block header for code at $9780 in bank 5"""
f = open(r'c:\proj\c\NES\nesxidy-co\nesxidy\nes_donkeykong.nes', 'rb')
data = f.read()
f.close()

bank = 5
bank_offset = 16 + bank * 16384
bank_data = data[bank_offset:bank_offset+16384]

target_offset = 0x1780  # $9780 - $8000

# Search ALL headers in the bank for one whose code points near $9780
print("All block headers in bank 5:")
i = 0
while i < len(bank_data) - 8:
    if bank_data[i+7] == 0xAA:
        entry_lo, entry_hi = bank_data[i], bank_data[i+1]
        exit_lo, exit_hi = bank_data[i+2], bank_data[i+3]
        code_len = bank_data[i+4]
        epil_off = bank_data[i+5]
        byte6 = bank_data[i+6]
        code_start = i + 8
        mapped_code = 0x8000 + code_start
        entry_pc = (entry_hi << 8) | entry_lo
        exit_pc = (exit_hi << 8) | exit_lo
        
        # Check block prefix: the dispatch adds BLOCK_PREFIX_SIZE (1 byte)
        # So the actual JMP target = code_start + BLOCK_PREFIX_SIZE
        marker = ''
        if code_start == target_offset or code_start + 1 == target_offset:
            marker = '  *** TARGET BLOCK ***'
        
        print(f'  ${0x8000+i:04X}: entry=${entry_pc:04X} exit=${exit_pc:04X} len={code_len} epil={epil_off} code@${mapped_code:04X}{marker}')
        if marker:
            # Print full code
            code = bank_data[code_start:code_start+code_len]
            print(f'    Full code ({code_len} bytes):')
            for j in range(0, len(code), 16):
                chunk = code[j:j+16]
                hexbytes = ' '.join(f'{b:02X}' for b in chunk)
                print(f'      ${mapped_code+j:04X}: {hexbytes}')
            print(f'    PHP count: {sum(1 for b in code if b==0x08)}')
            print(f'    PLP count: {sum(1 for b in code if b==0x28)}')
            # Check first few bytes
            print(f'    First 8 bytes: {" ".join(f"{b:02X}" for b in code[:8])}')
        i = code_start + max(code_len, 1)
    else:
        i += 1

# Also check: what's the BLOCK_PREFIX_SIZE?
print("\nSearching dynamos.h for BLOCK_PREFIX_SIZE...")
import os
for fn in ['dynamos.h', 'dynamos.c', 'config.h']:
    path = os.path.join(r'c:\proj\c\NES\nesxidy-co\nesxidy', fn)
    if os.path.exists(path):
        with open(path, 'r') as f2:
            for lineno, line in enumerate(f2, 1):
                if 'BLOCK_PREFIX' in line or 'BLOCK_HEADER' in line:
                    print(f'  {fn}:{lineno}: {line.rstrip()}')
