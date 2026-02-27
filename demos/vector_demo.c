/*
 * vector_demo.c
 * Simple monochrome vector rasteriser for NES demo purpose.
 * - Draws line lists into a WRAM framebuffer (1 bit per pixel)
 * - Packs framebuffer into 8x8 tiles (plane0 only) into a contiguous tile buffer
 * - Provides a vblank_upload_tiles() stub showing where to copy tiles into CHR
 *
 * This file is intentionally standalone and does not modify existing project
 * files. It assumes you will hook its functions into your NMI/vblank and main
 * loop. It is C (portable) so you can adapt or translate to 6502 asm later.
 */

#include <stdint.h>
#include <string.h>

// Framebuffer size (pixels)
#define FB_W 256
#define FB_H 240

// Tile layout
#define TILE_W 8
#define TILE_H 8
#define TILES_X (FB_W / TILE_W)   // 32
#define TILES_Y (FB_H / TILE_H)   // 30
#define TILE_COUNT (TILES_X * TILES_Y) // 960

// For demo we pack a monochrome framebuffer in WRAM: 1 bit per pixel.
// Size = FB_W * FB_H / 8 = 256*240/8 = 7680 bytes (~7.5KB) - fits in mapper RAM.
static uint8_t fb_bits[(FB_W * FB_H) / 8];

// Tile bitmap buffer: each tile is 16 bytes (two bitplanes). We'll only fill
// plane0 (low plane) for monochrome; high plane zeros. For portability we
// allocate space for a contiguous block of TILE_COUNT tiles (960*16 = 15360 bytes)
// but typical NES CHR area may be smaller; you will copy a subset into CHR.
static uint8_t tile_bank[TILE_COUNT * 16];

// Helpers for framebuffer bit ops
static inline void fb_clear(void) {
    memset(fb_bits, 0, sizeof(fb_bits));
}

static inline void fb_setpixel(int x, int y) {
    if ((unsigned)x >= FB_W || (unsigned)y >= FB_H) return;
    unsigned idx = y * FB_W + x;
    fb_bits[idx >> 3] |= (1 << (idx & 7));
}

static inline void fb_clrpixel(int x, int y) {
    if ((unsigned)x >= FB_W || (unsigned)y >= FB_H) return;
    unsigned idx = y * FB_W + x;
    fb_bits[idx >> 3] &= ~(1 << (idx & 7));
}

// Bresenham line draw into FB (monochrome set)
void draw_line(int x0, int y0, int x1, int y1) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    int sx = dx >= 0 ? 1 : -1;
    int sy = dy >= 0 ? 1 : -1;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;

    int err = (dx > dy ? dx : -dy) / 2;
    while (1) {
        fb_setpixel(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        int e2 = err;
        if (e2 > -dx) { err -= dy; x0 += sx; }
        if (e2 <  dy) { err += dx; y0 += sy; }
    }
}

// Convert framebuffer into tile bitmap bank. We'll generate low plane only.
// tile_bank layout: for each tile (0..TILE_COUNT-1), 16 bytes: 8 bytes plane0, 8 bytes plane1.
// We write plane0 from the FB and zero plane1.
void fb_to_tiles(void) {
    // For each tile row
    for (int ty = 0; ty < TILES_Y; ++ty) {
        for (int tx = 0; tx < TILES_X; ++tx) {
            int tile_index = ty * TILES_X + tx;
            uint8_t *tileptr = &tile_bank[tile_index * 16];
            // build 8 rows
            for (int row = 0; row < 8; ++row) {
                uint8_t plane0 = 0;
                // assemble bits for 8 pixels in this tile row
                for (int bit = 0; bit < 8; ++bit) {
                    int x = tx * 8 + (7 - bit); // NES bit ordering left->right uses bit7..bit0
                    int y = ty * 8 + row;
                    unsigned idx = y * FB_W + x;
                    uint8_t v = (fb_bits[idx >> 3] >> (idx & 7)) & 1;
                    plane0 |= (v << bit);
                }
                // plane0 row goes into tileptr[row], plane1 row into tileptr[row+8]
                tileptr[row] = plane0;    // low plane
                tileptr[row + 8] = 0x00;  // high plane zero for monochrome
            }
        }
    }
}

// Stub for uploading tiles to CHR pattern memory during vblank.
// You must replace the body of this function with the project's platform CHR
// write helper (e.g. functions that poke $2006/$2007 or WRAM->CHR copy). The
// demo leaves the implementation as a placeholder to avoid modifying project
// internals.
void vblank_upload_tiles(void) {
    // Example pseudocode for what you would do in vblank:
    // - Choose which tile range in CHR to update (banked or fixed)
    // - Write 16 bytes per tile via PPUDATA ($2007) or mapper helper
    // For safety we do nothing here; integrate manually into your platform code:
    // e.g. platform_write_chr(tile_offset, tile_bank, num_bytes);
}

// Simple demo: draw a starfield of N random lines
#include <stdlib.h>
void demo_random_vectors(int count) {
    fb_clear();
    for (int i = 0; i < count; ++i) {
        int x0 = rand() & 255;
        int y0 = rand() % 240;
        int x1 = rand() & 255;
        int y1 = rand() % 240;
        draw_line(x0, y0, x1, y1);
    }
    fb_to_tiles();
    // call vblank_upload_tiles() from NMI/vblank context
}

// Exported accessors for external harness (optional)
uint8_t *vector_demo_get_tile_bank(void) { return tile_bank; }
uint8_t *vector_demo_get_fb(void) { return fb_bits; }

/* End of vector_demo.c */
