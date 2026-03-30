#!/usr/bin/env python3
"""
gen_asteroids_tiles.py - Pre-generate NES CHR tiles from Asteroids DVG Vector ROM.

Reads the raw Vector ROM binary (035127-02.np3, 2KB), simulates the DVG interpreter
to extract line segments for each shape, rasterizes them at NES-appropriate sizes,
packs into deduplicated 8x8 NES CHR tiles with flip optimization, and outputs:
  1. C header (asteroids_tiles.h) with CHR data + metasprite definitions
  2. HTML visual preview of all shapes

Usage:
    python tools/gen_asteroids_tiles.py [--rom path/to/035127-02.np3] [--out-dir .]
"""

import argparse
import sys
import os
from pathlib import Path

# ---------------------------------------------------------------------------
# DVG Simulator
# ---------------------------------------------------------------------------

class DVGSimulator:
    """Minimal DVG interpreter for the 2KB Vector ROM ($1000-$17FF)."""

    STACK_DEPTH = 4
    MAX_COMMANDS = 4096

    def __init__(self, rom_data: bytes):
        assert len(rom_data) == 2048, f"Expected 2048-byte ROM, got {len(rom_data)}"
        self.rom = rom_data

    def read_word(self, word_addr: int) -> int:
        byte_addr = word_addr << 1
        if 0x1000 <= byte_addr < 0x1800:
            off = byte_addr - 0x1000
            return self.rom[off] | (self.rom[off + 1] << 8)
        return 0

    def execute_shape(self, entry_word_addr: int, global_scale: int = 0):
        """Execute a DVG subroutine, return list of (x0, y0, x1, y1, brightness).
        Beam starts at (0,0). Segments are in DVG coordinate units."""
        segments = []
        beam_x, beam_y = 0, 0
        gs = global_scale
        pc = entry_word_addr
        stack = []
        halted = False
        cmd_count = 0

        while not halted and cmd_count < self.MAX_COMMANDS:
            word = self.read_word(pc)
            cmd_count += 1
            opcode = (word >> 12) & 0x0F

            if opcode == 0x0F:
                # SVEC
                scale_raw = ((word >> 2) & 0x02) | ((word >> 11) & 0x01)
                shift = gs + scale_raw + 1
                if shift > 9:
                    shift = 9
                sx = (word & 0x03) << shift
                if word & 0x0004:
                    sx = -sx
                sy = ((word >> 8) & 0x03) << shift
                if word & 0x0400:
                    sy = -sy
                bright = (word >> 4) & 0x0F
                if bright > 0:
                    segments.append((beam_x, beam_y, beam_x + sx, beam_y + sy, bright))
                beam_x += sx
                beam_y += sy
                pc += 1

            elif opcode == 0x0B:
                halted = True

            elif opcode == 0x0D:
                if stack:
                    pc = stack.pop()
                else:
                    halted = True
                continue

            elif opcode == 0x0C:
                target = word & 0x0FFF
                stack.append(pc + 1)
                pc = target
                continue

            elif opcode == 0x0E:
                pc = word & 0x0FFF
                continue

            elif opcode == 0x0A:
                word2 = self.read_word(pc + 1)
                beam_y = word & 0x03FF
                beam_x = word2 & 0x03FF
                gs = (word2 >> 12) & 0x0F
                pc += 2
                continue

            else:
                # VEC (opcode 0-9)
                word2 = self.read_word(pc + 1)
                local_scale = opcode
                dy_mag = word & 0x03FF
                dx_mag = word2 & 0x03FF
                total_scale = gs + local_scale
                if total_scale <= 9:
                    shift = 9 - total_scale
                    dx = dx_mag >> shift
                    dy = dy_mag >> shift
                else:
                    shift = total_scale - 9
                    if shift > 6:
                        shift = 6
                    dx = dx_mag << shift
                    dy = dy_mag << shift
                if word & 0x0400:
                    dy = -dy
                if word2 & 0x0400:
                    dx = -dx
                bright = (word2 >> 12) & 0x0F
                if bright > 0:
                    segments.append((beam_x, beam_y, beam_x + dx, beam_y + dy, bright))
                beam_x += dx
                beam_y += dy
                pc += 2
                continue

            if halted:
                break

        return segments


# ---------------------------------------------------------------------------
# Shape Catalog
# ---------------------------------------------------------------------------

def dvg_byte_to_word_addr(dvg_byte_addr: int) -> int:
    return dvg_byte_addr // 2


# Rock patterns (4 variants) - DVG byte addresses
ROCK_ADDRS = [0x11E6, 0x11FE, 0x121A, 0x1234]

# UFO
UFO_ADDR = 0x1252

# Ship/Thrust shapes discovered by RTS scanning of the ROM
# Format: (ship_byte_addr, thrust_byte_addr) for each direction 0,4,8,...,64
SHIP_THRUST_ADDRS = [
    (0x1290, 0x12A2),  # dir 0
    (0x12A8, 0x12C2),  # dir 4
    (0x12CC, 0x12E6),  # dir 8
    (0x12F0, 0x130A),  # dir 12
    (0x1314, 0x132C),  # dir 16
    (0x1336, 0x1350),  # dir 20
    (0x135A, 0x1374),  # dir 24
    (0x137E, 0x1398),  # dir 28
    (0x13A2, 0x13BC),  # dir 32
    (0x13C6, 0x13E0),  # dir 36
    (0x13EA, 0x1404),  # dir 40
    (0x140E, 0x1428),  # dir 44
    (0x1432, 0x144C),  # dir 48
    (0x1456, 0x1470),  # dir 52
    (0x147A, 0x1494),  # dir 56
    (0x149E, 0x14B8),  # dir 60
    (0x14C2, 0x14D4),  # dir 64
]

# Ship explosion
SHIP_EXPLOSION_ADDR = 0x10E0

# Shrapnel (4 patterns)
SHRAPNEL_ADDRS = [0x1100, 0x112C, 0x116A, 0x11A0]

# Lives icon
LIVES_ADDR = 0x14DA

# Characters A-Z
CHAR_ADDRS = {
    'A': 0x14F0, 'B': 0x1500, 'C': 0x151A, 'D': 0x1526,
    'E': 0x1536, 'F': 0x1546, 'G': 0x1554, 'H': 0x1566,
    'I': 0x1574, 'J': 0x1582, 'K': 0x158E, 'L': 0x159A,
    'M': 0x15A4, 'N': 0x15B0, 'O': 0x15BA, 'P': 0x15C6,
    'Q': 0x15D4, 'R': 0x15E6, 'S': 0x15F6, 'T': 0x1604,
    'U': 0x1610, 'V': 0x161C, 'W': 0x1626, 'X': 0x1634,
    'Y': 0x163E, 'Z': 0x164C,
}

DIGIT_ADDRS = {
    ' ': 0x1658,
    '1': 0x165C, '2': 0x1664, '3': 0x1674, '4': 0x1682,
    '5': 0x1690, '6': 0x169E, '7': 0x16AC, '8': 0x16B6,
    '9': 0x16C6,
}


# ---------------------------------------------------------------------------
# Rasterizer with auto-scaling
# ---------------------------------------------------------------------------

class PixelGrid:
    """Sparse pixel grid with brightness tracking."""

    def __init__(self):
        self.pixels = {}

    def set_pixel(self, x: int, y: int, brightness: int):
        key = (x, y)
        self.pixels[key] = max(self.pixels.get(key, 0), brightness)

    def draw_line(self, x0: int, y0: int, x1: int, y1: int, brightness: int):
        dx = abs(x1 - x0)
        dy = abs(y1 - y0)
        sx = 1 if x0 < x1 else -1
        sy = 1 if y0 < y1 else -1
        err = dx - dy
        while True:
            self.set_pixel(x0, y0, brightness)
            if x0 == x1 and y0 == y1:
                break
            e2 = 2 * err
            if e2 > -dy:
                err -= dy
                x0 += sx
            if e2 < dx:
                err += dx
                y0 += sy

    def bounds(self):
        if not self.pixels:
            return None
        xs = [k[0] for k in self.pixels]
        ys = [k[1] for k in self.pixels]
        return min(xs), min(ys), max(xs), max(ys)


def rasterize_segments(segments, target_size=24):
    """Rasterize DVG segments scaled to fit within target_size pixels.

    Returns a PixelGrid with coordinates in [0, target_size) range.
    """
    grid = PixelGrid()
    if not segments:
        return grid

    # Compute DVG bounding box of visible segments
    all_x = [x for s in segments for x in [s[0], s[2]]]
    all_y = [y for s in segments for y in [s[1], s[3]]]
    min_x, max_x = min(all_x), max(all_x)
    min_y, max_y = min(all_y), max(all_y)
    dvg_w = max_x - min_x
    dvg_h = max_y - min_y
    dvg_extent = max(dvg_w, dvg_h, 1)

    # Scale to fit within target pixel box
    scale = (target_size - 1) / dvg_extent if dvg_extent > 0 else 1.0

    # Center the shape in the target box (Y negated for screen coords)
    nes_w = dvg_w * scale
    nes_h = dvg_h * scale
    ox = (target_size - nes_w) / 2 - min_x * scale
    oy = (target_size - nes_h) / 2 + max_y * scale  # +max_y because Y is negated

    for x0, y0, x1, y1, bri in segments:
        nx0 = round(x0 * scale + ox)
        ny0 = round(-y0 * scale + oy)  # DVG Y-up -> screen Y-down
        nx1 = round(x1 * scale + ox)
        ny1 = round(-y1 * scale + oy)  # DVG Y-up -> screen Y-down
        grid.draw_line(nx0, ny0, nx1, ny1, bri)

    return grid


# ---------------------------------------------------------------------------
# NES CHR Tile Packing
# ---------------------------------------------------------------------------

def brightness_to_palette(bri: int) -> int:
    if bri <= 4:
        return 0
    elif bri <= 8:
        return 1
    elif bri <= 12:
        return 2
    else:
        return 3


class TileData:
    def __init__(self, rows=None):
        self.rows = list(rows) if rows else [0] * 8

    def is_empty(self):
        return all(r == 0 for r in self.rows)

    def flip_h(self):
        def rev(b):
            b = ((b & 0xF0) >> 4) | ((b & 0x0F) << 4)
            b = ((b & 0xCC) >> 2) | ((b & 0x33) << 2)
            b = ((b & 0xAA) >> 1) | ((b & 0x55) << 1)
            return b & 0xFF
        return TileData([rev(r) for r in self.rows])

    def flip_v(self):
        return TileData(list(reversed(self.rows)))

    def flip_hv(self):
        return self.flip_h().flip_v()

    def to_bytes(self) -> bytes:
        return bytes(self.rows) + bytes(8)

    def key(self) -> tuple:
        return tuple(self.rows)

    def __eq__(self, other):
        return self.rows == other.rows

    def __hash__(self):
        return hash(self.key())


class TileBank:
    def __init__(self):
        self.tiles = []
        self.tile_map = {}  # key -> (tile_index, flip_attr)

    def add_tile(self, tile: TileData) -> tuple:
        key = tile.key()
        if key in self.tile_map:
            return self.tile_map[key]

        for flip_tile, attr in [
            (tile.flip_h(), 0x40),
            (tile.flip_v(), 0x80),
            (tile.flip_hv(), 0xC0),
        ]:
            fkey = flip_tile.key()
            if fkey in self.tile_map:
                existing_idx, existing_attr = self.tile_map[fkey]
                combined_attr = existing_attr ^ attr
                self.tile_map[key] = (existing_idx, combined_attr)
                return (existing_idx, combined_attr)

        idx = len(self.tiles)
        self.tiles.append(tile)
        self.tile_map[key] = (idx, 0x00)

        for flip_tile, attr in [
            (tile.flip_h(), 0x40),
            (tile.flip_v(), 0x80),
            (tile.flip_hv(), 0xC0),
        ]:
            fkey = flip_tile.key()
            if fkey not in self.tile_map:
                self.tile_map[fkey] = (idx, attr)

        return (idx, 0x00)

    def count(self):
        return len(self.tiles)


# ---------------------------------------------------------------------------
# Metasprite Builder
# ---------------------------------------------------------------------------

class MetaspriteEntry:
    __slots__ = ('tile_id', 'x_off', 'y_off', 'attr')

    def __init__(self, tile_id, x_off, y_off, attr):
        self.tile_id = tile_id
        self.x_off = x_off
        self.y_off = y_off
        self.attr = attr


class ShapeVariant:
    def __init__(self, name, dvg_addr, entries, width, height,
                 anchor_x=0, anchor_y=0, target_size=0):
        self.name = name
        self.dvg_addr = dvg_addr
        self.entries = entries
        self.width = width
        self.height = height
        self.anchor_x = anchor_x
        self.anchor_y = anchor_y
        self.target_size = target_size


def build_metasprite(grid: PixelGrid, tile_bank: TileBank, name: str,
                     dvg_addr: int, target_size: int) -> ShapeVariant:
    bounds = grid.bounds()
    if bounds is None:
        return ShapeVariant(name, dvg_addr, [], 0, 0, target_size=target_size)

    min_x, min_y, max_x, max_y = bounds

    # Align to 8x8 tile grid
    tile_x0 = (min_x // 8) * 8
    tile_y0 = (min_y // 8) * 8
    tile_x1 = ((max_x // 8) + 1) * 8
    tile_y1 = ((max_y // 8) + 1) * 8

    # Anchor = center of bounding box
    anchor_x = (min_x + max_x) // 2
    anchor_y = (min_y + max_y) // 2

    entries = []

    for ty in range(tile_y0, tile_y1, 8):
        for tx in range(tile_x0, tile_x1, 8):
            rows = []
            brightness_sum = 0
            brightness_count = 0
            for row in range(8):
                byte_val = 0
                for col in range(8):
                    px = tx + col
                    py = ty + row
                    bri = grid.pixels.get((px, py), 0)
                    if bri > 0:
                        byte_val |= (0x80 >> col)
                        brightness_sum += bri
                        brightness_count += 1
                rows.append(byte_val)

            tile = TileData(rows)
            if tile.is_empty():
                continue

            tile_idx, flip_attr = tile_bank.add_tile(tile)
            avg_bri = brightness_sum // brightness_count if brightness_count else 7
            palette = brightness_to_palette(avg_bri)
            attr = flip_attr | palette

            x_off = tx - anchor_x
            y_off = ty - anchor_y
            entries.append(MetaspriteEntry(tile_idx, x_off, y_off, attr))

    width = tile_x1 - tile_x0
    height = tile_y1 - tile_y0
    return ShapeVariant(name, dvg_addr, entries, width, height,
                        anchor_x, anchor_y, target_size=target_size)


# ---------------------------------------------------------------------------
# Shape Rendering Pipeline
# ---------------------------------------------------------------------------

def render_shape(dvg: DVGSimulator, name: str, dvg_byte_addr: int,
                 target_size: int, tile_bank: TileBank,
                 global_scale: int = 0) -> ShapeVariant:
    """Execute a DVG subroutine and build a NES metasprite.

    Uses global_scale=0 for clean shape extraction (no precision loss from
    shift clamping), then auto-scales to target_size NES pixels.
    """
    word_addr = dvg_byte_to_word_addr(dvg_byte_addr)
    segments = dvg.execute_shape(word_addr, global_scale=global_scale)
    grid = rasterize_segments(segments, target_size=target_size)
    return build_metasprite(grid, tile_bank, name, dvg_byte_addr, target_size)


# Target NES pixel sizes per shape type
TARGET_ROCK_LARGE = 26
TARGET_ROCK_MEDIUM = 16
TARGET_ROCK_SMALL = 10
TARGET_UFO_LARGE = 24
TARGET_UFO_SMALL = 14
TARGET_SHIP = 16
TARGET_THRUST = 12
TARGET_LIVES = 14
TARGET_EXPLOSION = 20
TARGET_SHRAPNEL = 10
TARGET_CHAR = 7


def render_all_shapes(dvg: DVGSimulator, tile_bank: TileBank):
    shapes = []

    # Rocks at 3 sizes (4 variants each)
    rock_sizes = [
        (TARGET_ROCK_LARGE, 'large'),
        (TARGET_ROCK_MEDIUM, 'medium'),
        (TARGET_ROCK_SMALL, 'small'),
    ]
    for i, addr in enumerate(ROCK_ADDRS, 1):
        for target, size_name in rock_sizes:
            name = f"rock{i}_{size_name}"
            shapes.append(render_shape(dvg, name, addr, target, tile_bank))

    # UFO at 2 sizes
    shapes.append(render_shape(dvg, "ufo_large", UFO_ADDR, TARGET_UFO_LARGE, tile_bank))
    shapes.append(render_shape(dvg, "ufo_small", UFO_ADDR, TARGET_UFO_SMALL, tile_bank))

    # Ships (17 rotations)
    for i, (ship_addr, _) in enumerate(SHIP_THRUST_ADDRS):
        dir_num = i * 4
        shapes.append(render_shape(dvg, f"ship_dir{dir_num}", ship_addr,
                                   TARGET_SHIP, tile_bank))

    # Thrust flames (17 rotations)
    for i, (_, thrust_addr) in enumerate(SHIP_THRUST_ADDRS):
        dir_num = i * 4
        shapes.append(render_shape(dvg, f"thrust_dir{dir_num}", thrust_addr,
                                   TARGET_THRUST, tile_bank))

    # Lives icon
    shapes.append(render_shape(dvg, "lives_icon", LIVES_ADDR, TARGET_LIVES, tile_bank))

    # Ship explosion
    shapes.append(render_shape(dvg, "ship_explosion", SHIP_EXPLOSION_ADDR,
                               TARGET_EXPLOSION, tile_bank))

    # Shrapnel (4 patterns)
    for i, addr in enumerate(SHRAPNEL_ADDRS, 1):
        shapes.append(render_shape(dvg, f"shrapnel{i}", addr, TARGET_SHRAPNEL, tile_bank))

    # Characters A-Z
    for ch, addr in sorted(CHAR_ADDRS.items()):
        shapes.append(render_shape(dvg, f"char_{ch}", addr, TARGET_CHAR, tile_bank))

    # Digits 0-9 + space (O is shared with digit 0)
    for ch, addr in sorted(DIGIT_ADDRS.items()):
        label = ch if ch != ' ' else 'space'
        shapes.append(render_shape(dvg, f"digit_{label}", addr, TARGET_CHAR, tile_bank))

    return shapes


# ---------------------------------------------------------------------------
# C Header Output
# ---------------------------------------------------------------------------

def generate_chr_header(tile_bank: TileBank, num_shapes: int, num_entries: int, out_path: str):
    """Generate asteroids_chr_data.h — CHR tile bytes only (for bank19 init)."""
    num_tiles = tile_bank.count()
    lines = []
    lines.append("// ========================================================================")
    lines.append("// asteroids_chr_data.h -- Auto-generated by gen_asteroids_tiles.py")
    lines.append("// DO NOT EDIT -- regenerate with: python tools/gen_asteroids_tiles.py")
    lines.append("// ========================================================================")
    lines.append(f"// Unique CHR tiles: {num_tiles}")
    lines.append("// ========================================================================")
    lines.append("")
    lines.append("#ifndef ASTEROIDS_CHR_DATA_H")
    lines.append("#define ASTEROIDS_CHR_DATA_H")
    lines.append("")
    lines.append("#include <stdint.h>")
    lines.append("")
    lines.append(f"#define AST_CHR_TILE_COUNT  {num_tiles}")
    lines.append("")
    lines.append(f"static const uint8_t ast_chr_data[{num_tiles * 8}] = {{")
    for i, tile in enumerate(tile_bank.tiles):
        row_strs = [f"0x{r:02X}" for r in tile.rows]
        lines.append(f"    {', '.join(row_strs)}, // tile {i}")
    lines.append("};")
    lines.append("")
    lines.append("#endif  // ASTEROIDS_CHR_DATA_H")
    lines.append("")

    with open(out_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))
    print(f"Generated {out_path}")
    print(f"  {num_tiles} unique CHR tiles ({num_tiles * 8} bytes plane0)")


def generate_meta_header(tile_bank: TileBank, shapes: list, out_path: str):
    """Generate asteroids_tiles.h — metasprites, lookups, anchors (for render bank)."""
    all_entries = []
    shape_offsets = []
    shape_counts = []

    for sv in shapes:
        shape_offsets.append(len(all_entries))
        shape_counts.append(len(sv.entries))
        all_entries.extend(sv.entries)

    num_shapes = len(shapes)
    num_tiles = tile_bank.count()
    num_entries = len(all_entries)

    lines = []
    lines.append("// ========================================================================")
    lines.append("// asteroids_tiles.h -- Auto-generated by gen_asteroids_tiles.py")
    lines.append("// DO NOT EDIT -- regenerate with: python tools/gen_asteroids_tiles.py")
    lines.append("// ========================================================================")
    lines.append(f"// Shape variants:   {num_shapes}")
    lines.append(f"// Metasprite entries: {num_entries}")
    lines.append("// ========================================================================")
    lines.append("")
    lines.append("#ifndef ASTEROIDS_TILES_H")
    lines.append("#define ASTEROIDS_TILES_H")
    lines.append("")
    lines.append("#include <stdint.h>")
    lines.append("")
    lines.append(f"#define AST_SHAPE_COUNT     {num_shapes}")
    lines.append(f"#define AST_META_TOTAL      {num_entries}")
    lines.append("")

    # Metasprite entry struct
    lines.append("typedef struct {")
    lines.append("    uint8_t tile_id;")
    lines.append("    int8_t  x_off;")
    lines.append("    int8_t  y_off;")
    lines.append("    uint8_t attr;  // bits 6-7: H/V flip, bits 0-1: palette")
    lines.append("} ast_metasprite_entry_t;")
    lines.append("")

    # Packed metasprite array
    lines.append(f"static const ast_metasprite_entry_t ast_metasprites[{num_entries}] = {{")
    for e in all_entries:
        lines.append(f"    {{ {e.tile_id}, {e.x_off}, {e.y_off}, 0x{e.attr:02X} }},")
    lines.append("};")
    lines.append("")

    # Per-shape offset + count tables
    lines.append(f"static const uint16_t ast_meta_offsets[{num_shapes}] = {{")
    for i in range(0, num_shapes, 8):
        chunk = shape_offsets[i:i+8]
        lines.append("    " + ", ".join(str(o) for o in chunk) + ",")
    lines.append("};")
    lines.append("")

    lines.append(f"static const uint8_t ast_meta_counts[{num_shapes}] = {{")
    for i in range(0, num_shapes, 8):
        chunk = shape_counts[i:i+8]
        lines.append("    " + ", ".join(str(c) for c in chunk) + ",")
    lines.append("};")
    lines.append("")

    # Shape name enum
    lines.append("enum ast_shape_id {")
    for i, sv in enumerate(shapes):
        lines.append(f"    AST_SHAPE_{sv.name.upper()} = {i},")
    lines.append("};")
    lines.append("")

    # DVG address lookup table
    lines.append("typedef struct {")
    lines.append("    uint16_t dvg_addr;")
    lines.append("    uint8_t  target_size;")
    lines.append("    uint8_t  shape_idx;")
    lines.append("} ast_shape_lookup_t;")
    lines.append("")

    lines.append(f"static const ast_shape_lookup_t ast_shape_lookup[{num_shapes}] = {{")
    for i, sv in enumerate(shapes):
        lines.append(f"    {{ 0x{sv.dvg_addr:04X}, {sv.target_size}, {i} }},  // {sv.name}")
    lines.append("};")
    lines.append("")

    # Shape anchor offsets
    lines.append(f"static const int8_t ast_shape_anchor_x[{num_shapes}] = {{")
    for i in range(0, num_shapes, 8):
        chunk = [shapes[j].anchor_x for j in range(i, min(i+8, num_shapes))]
        lines.append("    " + ", ".join(str(a) for a in chunk) + ",")
    lines.append("};")
    lines.append("")
    lines.append(f"static const int8_t ast_shape_anchor_y[{num_shapes}] = {{")
    for i in range(0, num_shapes, 8):
        chunk = [shapes[j].anchor_y for j in range(i, min(i+8, num_shapes))]
        lines.append("    " + ", ".join(str(a) for a in chunk) + ",")
    lines.append("};")
    lines.append("")

    lines.append("#endif  // ASTEROIDS_TILES_H")
    lines.append("")

    with open(out_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))

    print(f"Generated {out_path}")
    print(f"  {num_shapes} shape variants, {num_entries} total metasprite entries")


# ---------------------------------------------------------------------------
# HTML Preview Output
# ---------------------------------------------------------------------------

PALETTE_COLORS = {0: '#555555', 1: '#AAAAAA', 2: '#DDDDDD', 3: '#FFFFFF'}


def generate_html_preview(tile_bank: TileBank, shapes: list, out_path: str):
    html = []
    html.append("<!DOCTYPE html><html><head>")
    html.append("<title>Asteroids DVG Tileset Preview</title>")
    html.append("<style>")
    html.append("body{background:#000;color:#fff;font-family:monospace}")
    html.append(".shape{display:inline-block;margin:8px;vertical-align:top}")
    html.append(".shape-name{text-align:center;font-size:10px;margin-bottom:2px}")
    html.append("canvas{border:1px solid #333;image-rendering:pixelated}")
    html.append(".stats{margin:16px;padding:8px;background:#111}")
    html.append(".section{margin:16px 0} h2{color:#0f0}")
    html.append("</style></head><body>")
    html.append(f"<h1>Asteroids DVG Tileset: {tile_bank.count()} unique tiles, "
                f"{len(shapes)} shapes</h1>")

    # Stats
    total_entries = sum(len(sv.entries) for sv in shapes)
    nonempty = sum(1 for sv in shapes if sv.entries)
    html.append(f"<div class='stats'><b>Tiles:</b> {tile_bank.count()}/256 | "
                f"<b>Entries:</b> {total_entries} | "
                f"<b>Non-empty:</b> {nonempty}/{len(shapes)}</div>")

    # Tile atlas
    atlas_cols = 16
    atlas_rows = (tile_bank.count() + atlas_cols - 1) // atlas_cols
    aw, ah = atlas_cols * 8, max(atlas_rows * 8, 8)
    html.append(f"<div class='section'><h2>Tile Atlas ({tile_bank.count()} tiles)</h2>")
    html.append(f"<canvas id='atlas' width='{aw}' height='{ah}' "
                f"style='width:{aw*4}px;height:{ah*4}px'></canvas>")
    html.append("<script>(function(){")
    html.append(f"var c=document.getElementById('atlas'),x=c.getContext('2d'),id=x.createImageData({aw},{ah}),d=id.data;")
    for i, tile in enumerate(tile_bank.tiles):
        tx, ty = (i % atlas_cols) * 8, (i // atlas_cols) * 8
        for row in range(8):
            b = tile.rows[row]
            for col in range(8):
                if b & (0x80 >> col):
                    off = ((ty + row) * aw + tx + col) * 4
                    html.append(f"d[{off}]=d[{off+1}]=d[{off+2}]=255;d[{off+3}]=255;")
    html.append("x.putImageData(id,0,0);})();</script></div>")

    # Group shapes
    categories = [
        ('Rocks', [s for s in shapes if s.name.startswith('rock')]),
        ('UFO', [s for s in shapes if s.name.startswith('ufo')]),
        ('Ships', [s for s in shapes if s.name.startswith('ship_dir')]),
        ('Thrust', [s for s in shapes if s.name.startswith('thrust')]),
        ('Special', [s for s in shapes if s.name in ('lives_icon', 'ship_explosion')
                     or s.name.startswith('shrapnel')]),
        ('Characters', [s for s in shapes if s.name.startswith('char_')]),
        ('Digits', [s for s in shapes if s.name.startswith('digit_')]),
    ]

    cid = 0
    for cat_name, cat_shapes in categories:
        if not cat_shapes:
            continue
        html.append(f"<div class='section'><h2>{cat_name} ({len(cat_shapes)})</h2>")
        for sv in cat_shapes:
            if not sv.entries:
                html.append(f"<div class='shape'><div class='shape-name'>"
                            f"{sv.name} (empty)</div></div>")
                continue
            min_x = min(e.x_off for e in sv.entries)
            min_y = min(e.y_off for e in sv.entries)
            max_x = max(e.x_off + 8 for e in sv.entries)
            max_y = max(e.y_off + 8 for e in sv.entries)
            cw, ch = max_x - min_x, max_y - min_y
            scale = max(2, 64 // max(cw, ch, 1))
            cname = f"s{cid}"; cid += 1
            html.append(f"<div class='shape'><div class='shape-name'>{sv.name} "
                        f"({len(sv.entries)}t)</div>")
            html.append(f"<canvas id='{cname}' width='{cw}' height='{ch}' "
                        f"style='width:{cw*scale}px;height:{ch*scale}px'></canvas>")
            html.append("<script>(function(){")
            html.append(f"var c=document.getElementById('{cname}'),x=c.getContext('2d'),"
                        f"id=x.createImageData({cw},{ch}),d=id.data;")
            for e in sv.entries:
                tile = tile_bank.tiles[e.tile_id]
                pal = e.attr & 0x03
                cr, cg, cb = [int(PALETTE_COLORS[pal][j:j+2], 16) for j in (1, 3, 5)]
                hf = bool(e.attr & 0x40)
                vf = bool(e.attr & 0x80)
                for row in range(8):
                    sr = (7 - row) if vf else row
                    bv = tile.rows[sr]
                    for col in range(8):
                        sc = (7 - col) if hf else col
                        if bv & (0x80 >> sc):
                            px = (e.x_off - min_x) + col
                            py = (e.y_off - min_y) + row
                            if 0 <= px < cw and 0 <= py < ch:
                                off = (py * cw + px) * 4
                                html.append(f"d[{off}]={cr};d[{off+1}]={cg};"
                                            f"d[{off+2}]={cb};d[{off+3}]=255;")
            html.append(f"x.putImageData(id,0,0);}})()</script></div>")
        html.append("</div>")

    html.append("</body></html>")

    with open(out_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(html))
    print(f"Generated {out_path}")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Generate NES CHR tiles from Asteroids DVG Vector ROM")
    parser.add_argument('--rom', default=os.path.join('roms', 'asteroid', '035127-02.np3'))
    parser.add_argument('--out-dir', default='.')
    args = parser.parse_args()

    rom_path = Path(args.rom)
    if not rom_path.exists():
        print(f"ERROR: ROM file not found: {rom_path}", file=sys.stderr)
        sys.exit(1)

    rom_data = rom_path.read_bytes()
    print(f"Loaded vector ROM: {rom_path} ({len(rom_data)} bytes)")

    dvg = DVGSimulator(rom_data)
    tile_bank = TileBank()

    print("Rendering shapes...")
    shapes = render_all_shapes(dvg, tile_bank)

    # Summary
    print(f"\n{'Shape':<30} {'Addr':>6} {'Size':>4} {'Tiles':>5}")
    print("-" * 50)
    for sv in shapes:
        if sv.entries:
            print(f"{sv.name:<30} ${sv.dvg_addr:04X}  {sv.target_size:>3}  "
                  f"{len(sv.entries):>5}")

    nonempty = [sv for sv in shapes if sv.entries]
    print(f"\nTotal: {len(nonempty)} non-empty shapes, "
          f"{tile_bank.count()} unique tiles, "
          f"{sum(len(sv.entries) for sv in shapes)} metasprite entries")

    if tile_bank.count() > 256:
        print(f"WARNING: {tile_bank.count()} tiles exceeds NES PT1 limit of 256!",
              file=sys.stderr)

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    num_entries = sum(len(sv.entries) for sv in shapes)
    generate_chr_header(tile_bank, len(shapes), num_entries,
                        str(out_dir / "asteroids_chr_data.h"))
    generate_meta_header(tile_bank, shapes, str(out_dir / "asteroids_tiles.h"))
    generate_html_preview(tile_bank, shapes, str(out_dir / "asteroids_tiles_preview.html"))

    print(f"\nDone! Files in {out_dir}")


if __name__ == '__main__':
    main()
