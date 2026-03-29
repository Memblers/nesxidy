# More definitive test: check if the expected banks are truly empty or have code

rom = open('exidy.nes', 'rb').read()
header = 16
bank_size = 0x4000

print("=== Detailed bank content analysis ===")
for bank in range(32):
    start = header + bank * bank_size
    end = start + bank_size
    data = rom[start:end]
    
    # Count non-FF and non-00 bytes
    non_ff = sum(1 for b in data if b != 0xFF)
    non_00 = sum(1 for b in data if b != 0x00)
    total = len(data)
    
    # Find first and last non-FF byte
    first_non_ff = -1
    last_non_ff = -1
    for i, b in enumerate(data):
        if b != 0xFF:
            if first_non_ff == -1:
                first_non_ff = i
            last_non_ff = i
    
    if non_ff > 0:
        first_addr = 0x8000 + first_non_ff if bank < 31 else 0xC000 + first_non_ff
        last_addr = 0x8000 + last_non_ff if bank < 31 else 0xC000 + last_non_ff
        print(f"  Bank {bank:2d}: {non_ff:5d} non-FF bytes, {non_00:5d} non-00 bytes, "
              f"range ${first_addr:04X}-${last_addr:04X}")

# Now specifically check convert_chr_b2 location
# From the crash: trampoline switches to bank 25 and JSRs $81E1
# Let's see what convert_chr is and where it lives

print("\n=== Searching for convert_chr in map ===")
with open('vicemap.map', 'r') as f:
    for line in f:
        if 'convert' in line.lower():
            print(line.strip())

# Also check render_video_b2
print("\n=== Searching for render_video in map ===")
with open('vicemap.map', 'r') as f:
    for line in f:
        if 'render_video' in line.lower():
            print(line.strip())

# Check all exidy.c-related functions
print("\n=== Searching for key exidy.c functions in map ===")
with open('vicemap.map', 'r') as f:
    for line in f:
        for kw in ['flash_format', 'convert_sprite', 'convert_chr', 'render_video', 'exidy_main', 'init_ppu']:
            if kw in line.lower():
                print(line.strip())
                break
