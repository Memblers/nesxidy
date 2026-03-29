#pragma section default
#ifndef ASTEROIDS_H
#define ASTEROIDS_H

#include "config.h"

// ==========================================================================
// Asteroids arcade hardware emulation header
//
// Atari Asteroids (1979) — 6502 @ 1.5 MHz
// Memory map (15-bit address bus, $0000-$7FFF):
//   $0000-$03FF  RAM (1KB, zero-page + stack; $200-$3FF bankable via RAMSEL)
//   $2000-$2007  IN0 (bit-addressed: 3KHz clock, DVG halt, buttons — active HIGH)
//   $2400-$2407  IN1 (bit-addressed: coins, start, thrust, rotate — active HIGH)
//   $2800-$2803  DSW1 (DIP switches via TTL153 mux, returns 2 bits per read)
//   $3000        DVG GO (write = start DVG execution)
//   $3200        MultiPurp (write: bit 2=RAMSEL for $200/$300 bank swap)
//   $3400        Watchdog reset (write = pet watchdog)
//   $3600        Explosion sound trigger (write)
//   $3C00-$3C05  Output latches (coin counters, player LEDs)
//   $4000-$47FF  Vector RAM (2KB, DVG display list writable)
//   $4800-$4FFF  Unmapped (DVG address gap)
//   $5000-$57FF  Vector ROM (2KB, character shapes + vector data)
//   $6800-$7FFF  Program ROM (6KB, 3 x 2KB EPROMs)
//
// Display: monochrome XY vector monitor, 0-1023 coordinate space
// Sound:   POKEY + discrete explosion circuit
// ==========================================================================

// Frame timing — Asteroids runs at ~60 Hz (derived from crystal)
#ifndef TRACK_TICKS
#define FRAME_LENGTH    (const uint16_t) ((1500000 / 60) / 24)
#else
#define FRAME_LENGTH    (const uint16_t) (1500000 / 60)
#endif

// Dispatch overhead compensation (same concept as Exidy)
#define DISPATCH_OVERHEAD   80

// ROM address range for static walker
#define ROM_OFFSET  0x6800
#define ROM_ADDR_MIN  0x6800
#define ROM_ADDR_MAX  0x7FFF

// Input button bit definitions (active low unless noted)
#define AST_CLOCK       0x80    // 3 KHz clock input (active high)
#define AST_HALT        0x04    // DVG halt status flag (bit 2)

#define FLAG_INTERRUPT  0x04    // 6502 I flag
#define FLAG_DECIMAL    0x08    // 6502 D flag
#define FLAG_ASTEROIDS_NMI  0x80

#define IO8(addr) (*(volatile uint8_t *)(addr))

// ==========================================================================
// DVG (Digital Vector Generator) interpreter
// ==========================================================================

// Maximum dots rendered per DVG frame
#define DVG_MAX_DOTS        390 //384

// Sprite multiplex: cycle over N frames
#define DVG_MUX_FRAMES      6 //12 //3

// DVG subroutine call stack depth
#define DVG_STACK_DEPTH     4

// DVG command processing safety limit (prevent infinite loops)
#define DVG_MAX_COMMANDS    4096

// DVG coordinate space (0-1023 in each axis)
#define DVG_COORD_MAX       1023

// Point sampling interval (DVG coordinate units between dots)
#define DVG_SAMPLE_INTERVAL 12

// Maximum dots placed along a single vector segment
#define DVG_MAX_DOTS_PER_VEC 2

// Dot buffer entry
typedef struct {
    uint8_t x;  // NES screen X (0-255)
    uint8_t y;  // NES screen Y (0-239)
} dvg_dot_t;

// DVG interpreter state
typedef struct {
    int16_t beam_x;     // current X (0-1023)
    int16_t beam_y;     // current Y (0-1023)
    uint8_t intensity;  // current brightness (0 = invisible)
    uint16_t pc;        // DVG program counter (word address)
    uint16_t stack[DVG_STACK_DEPTH];
    uint8_t sp;         // stack pointer (0-3)
    uint8_t halted;     // 1 = DVG halted
    uint8_t global_scale; // global scale set by LABS (0-15)
} dvg_state_t;

// ==========================================================================
// NES CHR tile IDs (loaded into CHR-RAM at init)
// ==========================================================================
#define TILE_EMPTY      0
#define TILE_DOT_1x1    1   // single center pixel
#define TILE_DOT_2x2    2   // 2x2 pixel cluster
#define TILE_DIGIT_BASE 16  // tiles 16-25 = digits '0'-'9'
#define TILE_ALPHA_BASE 26  // tiles 26-51 = letters 'A'-'Z'
#define TILE_SHIP_ICON  240 // life indicator
#define TILE_COPYRIGHT  252 // (c) symbol

// ==========================================================================
// Extern declarations — shared code references these symbols
// ==========================================================================

extern uint8_t ROM_NAME[];      // Program ROM data (aliased in dynamos-asm.s)

extern void reset6502(void);
extern void step6502(void);
extern void run6502(void);
extern void irq6502(void);
extern void nmi6502(void);
extern void hookexternal(void *funcptr);
__zpage extern uint32_t clockticks6502;
__zpage extern uint8_t status;
__zpage extern uint16_t pc;
__zpage extern uint8_t sp;

// NMI nesting prevention (shared with dynamos.c batch dispatch)
extern uint8_t nmi_active;
extern uint8_t nmi_sp_guard;

__zpage extern uint16_t decoded_address;
__zpage extern uint16_t encoded_address;

uint8_t nes_gamepad(void);
void nes_gamepad_refresh(void);
void render_video(void);
uint8_t read6502(uint16_t address);
void write6502(uint16_t address, uint8_t value);
void flash_format(void);

#ifdef ENABLE_CACHE_PERSIST
void cache_write_signature(void);
uint8_t cache_check_signature(void);
void flash_init_persist(void);
#endif

#endif  // ASTEROIDS_H
#pragma section default
