"""
Analyze Millipede character ROM tile ordering.
Compare tile $77 vs $FB/$FF to find mapping relationship.
"""
import os

ROM_DIR = r"roms\milliped"
PLANE0_FILE = os.path.join(ROM_DIR, "136013-107.r5")  # 2KB, loaded at offset 0
PLANE1_FILE = os.path.join(ROM_DIR, "136013-106.p5")  # 2KB, loaded at offset 2048

def reverse_bits(b):
    """Reverse all 8 bits of a byte (our convert_chr does this)."""
    return int(f'{b:08b}'[::-1], 2)

def get_tile_data(plane0_data, plane1_data, tile_idx):
    """Get the raw 8-byte pairs for a tile (before bit reversal)."""
    base = tile_idx * 8
    p0 = plane0_data[base:base+8]
    p1 = plane1_data[base:base+8]
    return p0, p1

def print_tile_visual(p0_bytes, p1_bytes, label="", reverse=False):
    """Print a tile as ASCII art. p0=NES plane0, p1=NES plane1."""
    print(f"  {label}")
    for row in range(8):
        b0 = p0_bytes[row]
        b1 = p1_bytes[row]
        if reverse:
            b0 = reverse_bits(b0)
            b1 = reverse_bits(b1)
        line = ""
        for bit in range(7, -1, -1):  # MSB first for NES
            px = ((b0 >> bit) & 1) | (((b1 >> bit) & 1) << 1)
            line += ".123"[px]
        print(f"    {line}  (p0={b0:02X} p1={b1:02X})")
    print()

def main():
    plane0 = open(PLANE0_FILE, "rb").read()  # 107.r5
    plane1 = open(PLANE1_FILE, "rb").read()  # 106.p5
    print(f"Plane 0 (107.r5): {len(plane0)} bytes")
    print(f"Plane 1 (106.p5): {len(plane1)} bytes")
    print()

    # Show key tiles
    for idx in [0x77, 0xFB, 0xFF, 0xEE, 0x88, 0x37, 0xB7]:
        p0, p1 = get_tile_data(plane0, plane1, idx)
        print(f"=== Tile ${idx:02X} (decimal {idx}) ===")
        print(f"  Raw from ROM (107.r5 as NES plane0, 106.p5 as NES plane1):")
        print_tile_visual(p0, p1, "Without bit-reverse (raw):")
        print_tile_visual(p0, p1, "With bit-reverse:", reverse=True)

        # Also show with planes swapped (106.p5 as NES plane0, 107.r5 as NES plane1)
        p1_data, p0_data = get_tile_data(plane1, plane0, idx)
        print_tile_visual(p0_data, p1_data, "Planes swapped + bit-reverse:", reverse=True)

    # Check if there's a relationship between tile $77 and $FB/$FF
    print("\n=== Mapping analysis ===")
    p0_77, p1_77 = get_tile_data(plane0, plane1, 0x77)

    # Look for which tile index, when loaded as we do it, produces
    # the same visual as tile $77 would in MAME
    # In MAME: tile N → plane0 from 106.p5[N*8..], plane1 from 107.r5[N*8..]
    # In our code: tile N → NES plane0 from 107.r5[N*8..], NES plane1 from 106.p5[N*8..]
    # So our NES tile N looks like MAME tile N but with planes swapped.
    # The question is whether $77's graphics appear at a different index.

    # What does tile $77 look like in the correct MAME decode?
    print("\nMAME-correct tile $77 (plane0=106.p5, plane1=107.r5):")
    mame_p0_77 = plane1[0x77*8:0x77*8+8]  # 106.p5 = MAME plane 0
    mame_p1_77 = plane0[0x77*8:0x77*8+8]  # 107.r5 = MAME plane 1
    print_tile_visual(mame_p0_77, mame_p1_77, "With bit-reverse:", reverse=True)

    # Now search: which NES CHR index shows the same pattern as MAME tile $77?
    # Our NES tile K: plane0 = 107.r5[K*8..], plane1 = 106.p5[K*8..]
    # reversed = (reverse(107.r5[K*8+r]), reverse(106.p5[K*8+r]))
    # MAME tile $77: plane0 = 106.p5[$77*8..], plane1 = 107.r5[$77*8..]
    # reversed = (reverse(106.p5[$77*8+r]), reverse(107.r5[$77*8+r]))
    # These match only if K == $77 AND planes are swapped.
    # Since planes are swapped in our loading, the pixel shape is the same
    # but colors 1 and 2 are exchanged.

    # Actually maybe the issue is screen rotation or address scrambling.
    # Let me check if there's an XOR or bit-swap pattern.
    print("\nBit manipulation relationships:")
    print(f"  $77 = {0x77:08b}")
    print(f"  $FB = {0xFB:08b}")
    print(f"  $FF = {0xFF:08b}")
    print(f"  $77 XOR $FF = ${0x77^0xFF:02X} = {0x77^0xFF:08b}")
    print(f"  $77 XOR $FB = ${0x77^0xFB:02X} = {0x77^0xFB:08b}")
    print(f"  ~$77 = ${(~0x77)&0xFF:02X}")
    print(f"  reverse($77) = ${reverse_bits(0x77):02X}")
    print(f"  $77 | $80 = ${0x77|0x80:02X}")
    print(f"  $77 << 1 & $FF = ${(0x77<<1)&0xFF:02X}")
    print(f"  $77 >> 1 = ${0x77>>1:02X}")

    # Check the color PROM while we're at it
    COLOR_PROM_FILE = os.path.join(ROM_DIR, "136001-213.e7")
    if os.path.exists(COLOR_PROM_FILE):
        cprom = open(COLOR_PROM_FILE, "rb").read()
        print(f"\nColor PROM: {len(cprom)} bytes")
        print("First 32 entries:")
        for i in range(32):
            print(f"  [{i:02X}] = ${cprom[i]:02X} ({cprom[i]:08b})", end="")
            # Decode Millipede color: ~bits, R=bits[1:0], G=bits[4:2], B=bits[7:5]
            inv = (~cprom[i]) & 0xFF
            r = inv & 0x03
            g = (inv >> 2) & 0x07
            b = (inv >> 5) & 0x07
            print(f"  → R={r} G={g} B={b}")

if __name__ == "__main__":
    main()
