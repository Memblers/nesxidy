#pragma once
#include <stdint.h>

void fb_clear(void);
void draw_line(int x0, int y0, int x1, int y1);
void fb_to_tiles(void);
void vblank_upload_tiles(void);
void demo_random_vectors(int count);

uint8_t *vector_demo_get_tile_bank(void);
uint8_t *vector_demo_get_fb(void);
