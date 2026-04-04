#pragma section default
#ifndef MILLIPEDE_H
#define MILLIPEDE_H

#include "config.h"

// ==========================================================================
// Millipede arcade hardware emulation header
//
// Atari Millipede (1982) — 6502 @ 1.512 MHz
// Memory map:
//   $0000-$03FF  RAM (1KB)
//   $0400-$040F  POKEY 1 (sound + I/O)
//   $0800-$080F  POKEY 2 (sound + I/O)
//   $1000-$13BF  Video RAM (960 bytes, 30×32 tiles)
//   $13C0-$13FF  Sprite RAM (64 bytes, 16 sprites × 4 bytes)
//   $2000        IN0 — player 1 trackball/buttons
//   $2001        IN1 — player 2 trackball/buttons
//   $2010        IN2 — coin/start
//   $2011        IN3 — (unused?)
//   $2030        DSW1 — DIP switches bank 1
//   $2031        DSW2 — DIP switches bank 2
//   $2480-$249F  Palette RAM (32 bytes, writable color palette)
//   $2500-$2507  Output latches (LEDs, coin counters)
//   $2600        Watchdog reset
//   $4000-$7FFF  Program ROM (16KB, 4 × 4KB EPROMs)
//   $8000-$FFFF  ROM mirror (A14-A15 not fully decoded)
//
// Video: 240×256 rotated monitor, 30×32 tile grid, 2bpp characters
// Sprites: 16 motion objects, 8×16 pixels, 2bpp
// ==========================================================================

// Frame timing — Millipede runs at ~61 Hz (derived from crystal)
#ifndef TRACK_TICKS
#define FRAME_LENGTH	(const uint16_t) ((1512000 / 61.0) / 24)
#else
#define FRAME_LENGTH	(const uint16_t) (1512000 / 61.0)
#endif

// Dispatch overhead compensation (same concept as Exidy)
#define DISPATCH_OVERHEAD	80

// ROM address range for static walker
#define ROM_OFFSET  0x4000
#define ROM_ADDR_MIN  0x4000
#define ROM_ADDR_MAX  0x7FFF

// Input button bit definitions (mapped from NES gamepad)
#define MILLI_FIRE      0x80    // fire button
#define MILLI_START1    0x40    // 1P start
#define MILLI_START2    0x20    // 2P start
#define MILLI_COIN1     0x10    // coin 1
#define MILLI_COIN2     0x08    // coin 2
#define MILLI_LEFT      0x04    // trackball substitute
#define MILLI_RIGHT     0x02
#define MILLI_UP        0x01
#define MILLI_DOWN      0x80    // on IN1

#define FLAG_INTERRUPT 	0x04	// 6502 I flag
#define FLAG_DECIMAL    0x08    // 6502 D flag
#define FLAG_MILLIPEDE_IRQ	0x80

#define IO8(addr) (*(volatile uint8_t *)(addr))

// ==========================================================================
// POKEY random number generator (17-bit LFSR)
// The Atari POKEY chip includes a 17-bit polynomial counter used for
// random number generation and noise audio.  The polynomial is
// x^17 + x^3 + 1, implemented as a Galois LFSR.
// Games read the RANDOM register ($0408/$0808) frequently.
// ==========================================================================
#define POKEY_POLY17_PERIOD  131071  // 2^17 - 1

// ==========================================================================
// Millipede palette format (writable palette RAM at $2480-$249F):
// Each byte encodes RGB with INVERTED bits:
//   Bits 1-0: Red   (2 bits) — weights 0xA8, 0x4F
//   Bits 4-2: Green (3 bits) — weights 0x97, 0x47, 0x21
//   Bits 7-5: Blue  (3 bits) — weights 0x97, 0x47, 0x21
// ==========================================================================

// ==========================================================================
// Extern declarations matching the Exidy interface
// Shared code (dynamos.c, fake6502.c) references these symbols.
// ==========================================================================

extern uint8_t ROM_NAME[];      // ROM data (aliased in dynamos-asm.s)

extern void reset6502(void);
extern void step6502(void);
extern void run6502(void);
extern void irq6502(void);
extern void hookexternal(void *funcptr);
__zpage extern uint32_t clockticks6502;
__zpage extern uint8_t status;
__zpage extern uint16_t pc;

__zpage extern uint16_t decoded_address;
__zpage extern uint16_t encoded_address;

uint8_t nes_gamepad(void);
void nes_gamepad_refresh(void);
void render_video(void);
void convert_chr(uint8_t *source);
uint8_t read6502(uint16_t address);      // WRAM asm fast path (dynamos-asm.s)
uint8_t read6502_io(uint16_t address);   // C slow path for I/O registers
void write6502(uint16_t address, uint8_t value);
void flash_format(void);

#ifdef ENABLE_CACHE_PERSIST
void cache_write_signature(void);
uint8_t cache_check_signature(void);
void flash_init_persist(void);
#endif

#endif  // MILLIPEDE_H
#pragma section default
