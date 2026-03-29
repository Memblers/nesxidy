"""
Verify the built millipede.nes ROM:
1. Extract bank24 data, verify it matches the char ROM files
2. Simulate convert_chr to show what NES CHR-RAM tile $77 looks like
3. Check if something in the pipeline could remap tile indices
"""
import os

ROM_PATH = r"millipede.nes"
INES_HEADER = 16
BANK_SIZE = 16384
CHR_BANK = 24

# Original ROM files
P0_FILE = r"roms\milliped\136013-107.r5"  # plane at offset 0
P1_FILE = r"roms\milliped\136013-106.p5"  # plane at offset 2048

def reverse_bits(b):
    nibble_rev = [0x0,0x8,0x4,0xC,0x2,0xA,0x6,0xE,0x1,0x9,0x5,0xD,0x3,0xB,0x7,0xF]
    return (nibble_rev[b & 0x0F] << 4) | nibble_rev[b >> 4]

def print_nes_tile(plane0_bytes, plane1_bytes, label=""):
    """Print NES CHR-RAM tile (MSB-first, plane0=lo, plane1=hi)."""
    print(f"  {label}")
    for row in range(8):
        b0 = plane0_bytes[row]
        b1 = plane1_bytes[row]
        line = ""
        for bit in range(7, -1, -1):
            px = ((b0 >> bit) & 1) | (((b1 >> bit) & 1) << 1)
            line += ".123"[px]
        print(f"    {line}  (lo={b0:02X} hi={b1:02X})")
    print()

def main():
    # Read built ROM
    rom = open(ROM_PATH, "rb").read()
    print(f"ROM size: {len(rom)} bytes")
    
    # Extract bank24 ($8000-$BFFF region)
    bank24_offset = INES_HEADER + CHR_BANK * BANK_SIZE
    bank24_data = rom[bank24_offset:bank24_offset + BANK_SIZE]
    print(f"Bank24 file offset: 0x{bank24_offset:06X}")
    
    # _chr_millipede is at $8000 in bank24, so offset 0 in the bank
    chr_data = bank24_data[0:4096]  # 4KB: 2KB plane + 2KB plane
    
    # Compare with original ROM files
    p0_orig = open(P0_FILE, "rb").read()
    p1_orig = open(P1_FILE, "rb").read()
    
    if chr_data[:2048] == p0_orig:
        print("✓ Bank24[0..2047] matches 136013-107.r5 (plane at offset 0)")
    else:
        diffs = sum(1 for a,b in zip(chr_data[:2048], p0_orig) if a != b)
        print(f"✗ Bank24[0..2047] DIFFERS from 107.r5 ({diffs} byte diffs)")
    
    if chr_data[2048:4096] == p1_orig:
        print("✓ Bank24[2048..4095] matches 136013-106.p5 (plane at offset 2048)")
    else:
        diffs = sum(1 for a,b in zip(chr_data[2048:4096], p1_orig) if a != b)
        print(f"✗ Bank24[2048..4095] DIFFERS from 106.p5 ({diffs} byte diffs)")
    
    print()
    
    # Simulate convert_chr: what ends up in NES CHR-RAM
    # Our code: for each tile, write 8 bytes reverse(chr_data[tile*8+j]) then 8 bytes reverse(chr_data[2048+tile*8+j])
    nes_chr = bytearray(4096)  # pattern table 0
    for tile in range(256):
        base = tile * 8
        for j in range(8):
            nes_chr[tile*16 + j] = reverse_bits(chr_data[base + j])        # NES plane 0 from 107.r5
            nes_chr[tile*16 + 8 + j] = reverse_bits(chr_data[2048 + base + j])  # NES plane 1 from 106.p5
    
    # Show specific tiles
    for idx in [0x77, 0xFB, 0xFF, 0x37, 0xB7]:
        offset = idx * 16
        p0 = nes_chr[offset:offset+8]
        p1 = nes_chr[offset+8:offset+16]
        print(f"=== NES CHR tile ${idx:02X} ===")
        print_nes_tile(p0, p1, f"As it would appear on NES (tile ${idx:02X}):")
    
    # Now check: MAME uses plane0=106.p5, plane1=107.r5 (reversed from us)
    # What if we swap the planes during conversion?
    print("\n=== MAME-correct loading (swap planes) ===")
    for idx in [0x77, 0xFB]:
        base = idx * 8
        p0_mame = [reverse_bits(chr_data[2048 + base + j]) for j in range(8)]  # 106.p5 as NES plane 0
        p1_mame = [reverse_bits(chr_data[base + j]) for j in range(8)]          # 107.r5 as NES plane 1
        print(f"=== MAME-correct tile ${idx:02X} ===")
        print_nes_tile(p0_mame, p1_mame, f"Planes swapped:")
    
    # What does the Millipede ROM actually write for a mushroom?
    # Let's check the program ROM to find what tile values the game uses
    rom_bank = 23
    rom_offset_base = INES_HEADER + rom_bank * BANK_SIZE
    prog_rom = rom[rom_offset_base:rom_offset_base + BANK_SIZE]
    
    # Search for stores to video RAM with value $77
    print("\n=== Searching program ROM for tile $77 references ===")
    count = 0
    for i in range(len(prog_rom) - 2):
        # LDA #$77 = A9 77
        if prog_rom[i] == 0xA9 and prog_rom[i+1] == 0x77:
            addr = 0x8000 + i  # bank23 mapped to $8000
            # Convert to Millipede address space ($4000-$7FFF)
            mill_addr = 0x4000 + (i % 0x4000)
            print(f"  LDA #$77 at ${mill_addr:04X} (bank offset ${i:04X})")
            # Show context
            ctx = prog_rom[max(0,i-2):min(len(prog_rom),i+6)]
            hex_str = ' '.join(f'{b:02X}' for b in ctx)
            print(f"    Context: {hex_str}")
            count += 1
            if count >= 10: break
    print(f"  Total LDA #$77: {count}")
    
    # Also check if the game uses a tile lookup table
    print("\n=== Looking for tile lookup tables in ROM ===")
    # A common pattern: ROM has a table of tile IDs
    # Let's find sequences that include $77 followed by other common tiles
    
    # Check the self-test routine area
    # Self-test in Millipede is usually triggered by the service switch
    # It fills the screen with incrementing tile IDs
    # Let's find any loop that writes to $10xx
    count = 0
    for i in range(len(prog_rom) - 2):
        # STA $10xx = 8D xx 10
        if prog_rom[i] == 0x8D and prog_rom[i+2] == 0x10:
            addr = 0x4000 + (i % 0x4000)
            count += 1
            if count <= 5:
                ctx = prog_rom[max(0,i-4):min(len(prog_rom),i+4)]
                hex_str = ' '.join(f'{b:02X}' for b in ctx)
                print(f"  STA $10{prog_rom[i+1]:02X} at ${addr:04X}: {hex_str}")
    print(f"  Total STA $10xx: {count}")

if __name__ == "__main__":
    main()
