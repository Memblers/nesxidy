Vector demo (Asteroids‑style) for nesxidy

Overview

This demo implements a simple monochrome vector rasteriser suitable for an
Asteroids‑like display. It rasterises line lists into a 1‑bit WRAM framebuffer
and converts the framebuffer into 8x8 CHR tile bitmaps (low plane only).

Files

- `demos/vector_demo.c` : C implementation (standalone).
- `demos/vector_demo.h` : Public header.

Integration notes

This code is intentionally standalone. To use it inside the existing project:

1. Include `demos/vector_demo.c` in the build (add to your build list or
   compile and link separately). Do not modify existing source files.

2. Call `demo_random_vectors(n)` (or your own vector list renderer) from
   the main loop to rasterise into the in‑memory framebuffer.

3. During NMI/vblank, call `vblank_upload_tiles()` to copy `tile_bank` data
   into CHR pattern memory (via your platform's CHR write helper). The demo
   leaves `vblank_upload_tiles()` as a stub so you can implement the right
   platform calls (e.g. mapper write helpers, $2006/$2007 bursts or banked
   CHR copying), because modifying platform internals is disallowed by the
   request.

4. Use palette/attribute tricks for monochrome display (plane0 only). If you
   implement mid‑frame CHR bank switching, you can place the tile definitions
   into the appropriate bank and switch in HBlank or via your mapper IRQ
   trick.

Performance notes

- `draw_line()` uses Bresenham and writes single pixels; on a 1.79MHz 6502
  a compact implementation costs on the order of ~15–25 cycles per step.
- Translating the framebuffer into tiles (`fb_to_tiles`) is ~7680 bits →
  960 tiles × 8 rows = deterministic CPU cost during pre‑vblank.
- Uploading tiles during vblank must be implemented carefully to stay within
  the vblank time budget; consider updating only the changed tiles (dirty
  rects) to reduce CHR writes.

License

Drop this into your repo under the same license as the project.
