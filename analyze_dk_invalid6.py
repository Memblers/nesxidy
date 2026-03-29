"""
Parse all flash_byte_program calls, extract arguments (addr, bank, data),
and find the write that targeted $84B0 in bank 8.
VBCC calling convention for flash_byte_program(uint16_t addr, uint8_t bank, uint8_t data):
  - addr lo: stored in ENABLE_NATIVE_STACK ($00) or r0
  - addr hi: stored in ASM_BLOCK_COUNT ($01) or r1
  - bank: stored in r2 ($02)
  - data: stored in r4 ($04)
"""
import os, re

trace_file = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"
size = os.path.getsize(trace_file)

tail_bytes = 5_000_000
with open(trace_file, 'r', encoding='utf-8', errors='replace') as f:
    f.seek(max(0, size - tail_bytes))
    if f.tell() > 0:
        f.readline()
    tail_lines = f.readlines()

crash_idx = len(tail_lines) - 1
print(f"Read {len(tail_lines)} tail lines")

# Find all flash_byte_program entries
fbp_entries = []
for i in range(len(tail_lines)):
    line = tail_lines[i].lstrip()
    if line.startswith('6000'):
        fbp_entries.append(i)

print(f"Found {len(fbp_entries)} flash_byte_program entries")

# For each entry, extract the register values at the JSR instruction
# The JSR is at i-1 (the line before entry to $6000)
# Arguments are set up in the lines before the JSR:
# r2 = bank, r4 = data, $00/$01 = addr lo/hi
for entry_num, idx in enumerate(fbp_entries):
    # Look at the JSR line (should be idx-1)
    # And trace back to find the argument stores
    addr_lo = None
    addr_hi = None
    bank = None
    data = None
    
    for j in range(idx - 1, max(0, idx - 15), -1):
        line = tail_lines[j]
        # Look for STA instructions that set up arguments
        # r2 = bank
        if 'STA r2' in line and bank is None:
            m = re.search(r'A:([0-9A-Fa-f]{2})', line)
            if m: bank = int(m.group(1), 16)
        # r4 = data
        if 'STA r4' in line and data is None:
            m = re.search(r'A:([0-9A-Fa-f]{2})', line)
            if m: data = int(m.group(1), 16)
        # addr lo - stored in $00 (ENABLE_NATIVE_STACK) 
        if 'STA ENABLE_NATIVE_STACK' in line and addr_lo is None:
            m = re.search(r'A:([0-9A-Fa-f]{2})', line)
            if m: addr_lo = int(m.group(1), 16)
        # addr hi - stored in $01 (ASM_BLOCK_COUNT)
        if 'STA ASM_BLOCK_COUNT' in line and addr_hi is None:
            m = re.search(r'A:([0-9A-Fa-f]{2})', line)
            if m: addr_hi = int(m.group(1), 16)
    
    addr = (addr_hi << 8 | addr_lo) if addr_lo is not None and addr_hi is not None else None
    
    if bank == 0x08 or addr == 0x84B0:
        addr_s = f"${addr:04X}" if addr is not None else "$????"
        bank_s = f"${bank:02X}" if bank is not None else "$??"
        data_s = f"${data:02X}" if data is not None else "$??"
        print(f"  Entry #{entry_num} at T{idx}: addr={addr_s}, bank={bank_s}, data={data_s}")
        # Show context
        for j in range(max(0, idx - 12), min(idx + 2, len(tail_lines))):
            print(f"    {'>>>' if j == idx else '   '} T{j}: {tail_lines[j].rstrip()[:180]}")

# Also let's look at the $DAC5 code loop more carefully
# Extract the code bytes being written to bank 8
print(f"\n=== CODE BYTES WRITTEN TO BANK 8 ===")
code_bytes_bank8 = []
for entry_num, idx in enumerate(fbp_entries):
    addr_lo = None
    addr_hi = None  
    bank = None
    data = None
    
    for j in range(idx - 1, max(0, idx - 15), -1):
        line = tail_lines[j]
        if 'STA r2' in line and bank is None:
            m = re.search(r'A:([0-9A-Fa-f]{2})', line)
            if m: bank = int(m.group(1), 16)
        if 'STA r4' in line and data is None:
            m = re.search(r'A:([0-9A-Fa-f]{2})', line)
            if m: data = int(m.group(1), 16)
        if 'STA ENABLE_NATIVE_STACK' in line and addr_lo is None:
            m = re.search(r'A:([0-9A-Fa-f]{2})', line)
            if m: addr_lo = int(m.group(1), 16)
        if 'STA ASM_BLOCK_COUNT' in line and addr_hi is None:
            m = re.search(r'A:([0-9A-Fa-f]{2})', line)
            if m: addr_hi = int(m.group(1), 16)
    
    addr = (addr_hi << 8 | addr_lo) if addr_lo is not None and addr_hi is not None else None
    
    if bank is not None:
        frame_match = re.search(r'Fr:(\d+)', tail_lines[idx])
        frame = frame_match.group(1) if frame_match else '?'
        code_bytes_bank8.append((entry_num, idx, addr, bank, data, frame))
        
print(f"Total writes with parsed bank: {len(code_bytes_bank8)}")
for entry_num, idx, addr, bank, data, frame in code_bytes_bank8:
    if bank == 0x08:
        addr_s = f"${addr:04X}" if addr is not None else "$????"
        data_s = f"${data:02X}" if data is not None else "$??"
        print(f"  #{entry_num}: addr={addr_s} bank=${bank:02X} data={data_s} fr={frame}")
    elif bank in [0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F]:
        addr_s = f"${addr:04X}" if addr is not None else "$????"
        data_s = f"${data:02X}" if data is not None else "$??"
        print(f"  #{entry_num}: addr={addr_s} bank=${bank:02X} data={data_s} fr={frame} [PC TABLE]")

# Summary: show all unique banks written to
banks = set()
for _, _, _, bank, _, _ in code_bytes_bank8:
    if bank is not None:
        banks.add(bank)
print(f"\nUnique banks written to: {sorted([f'${b:02X}' for b in banks])}")
