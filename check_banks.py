import struct

rom = open('exidy.nes', 'rb').read()
header = 16
bank_size = 0x4000

# Function addresses (all relative to $8000 within their bank)
funcs = {
    'find_zp_mirror': 0x81DA,
    'find_zp_mirror_lo': 0x821C,
    'metrics_dump_sa_b2': 0x8278,
    'metrics_dump_runtime_b2': 0x830E,
    'sa_subroutine_lookup': 0xAD22,
    'sa_record_subroutine': 0x91A0,
    'render_video_b2': 0x8000,  # check bank 22 start
    'convert_chr_b2': 0x81E1,   # from the crash disasm
}

# For each function, check which banks have non-FF at that offset
for name, addr in funcs.items():
    offset_in_bank = addr - 0x8000
    print(f'\n{name} (${addr:04X}, offset ${offset_in_bank:04X}):')
    for bank in range(32):
        rom_offset = header + bank * bank_size + offset_in_bank
        # Read a few bytes
        data = rom[rom_offset:rom_offset+8]
        # Check if it's all FF or 00
        if not all(b == 0xFF for b in data) and not all(b == 0x00 for b in data):
            print(f'  Bank {bank}: {" ".join(f"{b:02X}" for b in data)}')

# Also check: what section did default-section code go to?
# The "text" section (default) goes to bank 31 (fixed bank)
# Check if these functions appear in bank 31 ($C000-$FFFF range)
print("\n\n--- Checking fixed bank (31) for misplaced functions ---")
for name, addr in funcs.items():
    # In fixed bank, addr would be at $C000 + offset if misplaced there
    fixed_offset = addr - 0x8000 + 0x4000  # offset in fixed bank = addr-$8000+$4000
    rom_offset = header + 31 * bank_size + fixed_offset
    if rom_offset + 8 <= len(rom):
        data = rom[rom_offset:rom_offset+8]
        if not all(b == 0xFF for b in data) and not all(b == 0x00 for b in data):
            fixed_addr = 0xC000 + fixed_offset
            print(f'  {name}: found at fixed bank offset ${fixed_addr:04X}: {" ".join(f"{b:02X}" for b in data)}')

# Let's also check: where does vbcc put the "text" (default) section?
# Read the linker script to verify
print("\n\n--- Bank content summary (non-empty banks) ---")
for bank in range(32):
    start = header + bank * bank_size
    end = start + bank_size
    data = rom[start:end]
    non_ff = sum(1 for b in data if b != 0xFF)
    non_00 = sum(1 for b in data if b != 0x00)
    if non_ff > 16 and non_00 > 16:  # meaningful content
        print(f'  Bank {bank}: {non_ff} non-FF bytes, {non_00} non-00 bytes')
