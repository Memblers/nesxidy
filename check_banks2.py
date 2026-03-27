import os

rom_path = r'c:\proj\c\NES\nesxidy-co\nesxidy\millipede.nes'
with open(rom_path, 'rb') as f:
    rom = f.read()

header = 16
bsize = 0x4000

for bank_num in [19, 20, 21, 22]:
    offset = header + bank_num * bsize
    data = rom[offset:offset + bsize]
    
    # Count non-zero, non-FF bytes
    code_bytes = sum(1 for b in data if b != 0x00 and b != 0xFF)
    zero_bytes = sum(1 for b in data if b == 0x00)
    ff_bytes = sum(1 for b in data if b == 0xFF)
    
    # Find function prologues (SEC; LDA sp; SBC)
    prologues = []
    for i in range(len(data) - 4):
        if data[i] == 0x38 and data[i+1] == 0xA5 and data[i+2] == 0x20 and data[i+3] == 0xE9:
            prologues.append((0x8000 + i, data[i+4]))
    
    print(f'Bank {bank_num}: code={code_bytes} zero={zero_bytes} ff={ff_bytes}')
    print(f'  First 16 bytes: {" ".join(f"{b:02X}" for b in data[:16])}')
    print(f'  Function prologues: {[(f"${a:04X}", f"frame=${sz:02X}") for a,sz in prologues]}')
    
    # Show $810D content
    off_810d = 0x10D
    print(f'  $810D: {" ".join(f"{b:02X}" for b in data[off_810d:off_810d+8])}')
    print()
