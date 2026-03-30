// ==========================================================================
// asteroids.c — Atari Asteroids arcade platform driver for DynaMoS
//
// Emulates the Asteroids arcade hardware (6502 @ 1.5 MHz) running on
// NES hardware via the DynaMoS dynamic binary translator.
//
// Display: DVG vector output → point-sampled dot sprites (3-frame mux)
// Audio:   POKEY stubs (no sound emulation yet)
// Input:   NES gamepad mapped to rotate/thrust/fire/hyperspace
// ==========================================================================

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "lazynes.h"
#include "config.h"
#include "asteroids.h"
#include "dynamos.h"
#include "bank_map.h"
#include "mapper30.h"
#include "core/optimizer.h"
#ifdef ENABLE_OPTIMIZER_V2
#include "core/optimizer_v2_simple.h"
#endif
#ifdef ENABLE_STATIC_ANALYSIS
#include "core/static_analysis.h"
#endif
#include "core/metrics.h"


// ==========================================================================
// ROM data externs — defined via incbin in dynamos-asm.s (bank 23)
// ==========================================================================
#pragma section bank23
	extern const unsigned char rom_asteroids[];     // 6KB program ROM ($6800-$7FFF)
	extern const unsigned char rom_asteroids_vec[];  // 2KB vector ROM ($4800-$4FFF)
#pragma section default


// ==========================================================================
// Host memory externs (defined in dynamos-asm.s BSS/WRAM)
// ==========================================================================
extern uint8_t RAM_BASE[];
extern uint8_t SCREEN_RAM_BASE[];
extern uint8_t CHARACTER_RAM_BASE[];
extern uint16_t interpret_count;
extern uint8_t write_50xx_count;
extern uint8_t last_interpreted_opcode;
extern uint8_t indy_hit_count;


__zpage uint8_t audio;  // debug: audio test value written to $4011 on each read

// ==========================================================================
// Asteroids-specific state
// ==========================================================================
__zpage uint8_t interrupt_condition;
__zpage uint8_t screen_ram_updated = 0;
__zpage uint8_t character_ram_updated = 0;  // required by ASM symbol

// Vector RAM ($4000-$47FF) — writable display list
// Asteroids has 1KB of vector RAM; $4400-$47FF mirrors $4000-$43FF.
#define VECTOR_RAM_SIZE 1024
#define VECTOR_RAM_MASK 0x3FF
static uint8_t vector_ram[VECTOR_RAM_SIZE];

// DVG state
static dvg_state_t dvg;
__zpage uint8_t dvg_triggered = 0;  // set by write to $3000 (DVG Go)

// Dot buffer — filled by DVG interpreter each frame
static dvg_dot_t dot_buffer[DVG_MAX_DOTS];
__zpage uint16_t dot_count = 0;

// Sprite multiplex frame counter (0, 1, 2)
__zpage uint8_t mux_frame = 0;

// Shape instance buffer — filled by dvg_execute, rendered as metasprites
typedef struct {
	uint8_t shape_idx;
	uint8_t screen_x;
	uint8_t screen_y;
} shape_instance_t;

#define MAX_SHAPE_INSTANCES 32
static shape_instance_t shape_instances[MAX_SHAPE_INSTANCES];
__zpage uint8_t shape_instance_count = 0;



// POKEY random number LFSR (17-bit, same as Millipede)
static uint32_t pokey_lfsr = 0x1FFFF;

// Cached gamepad state
static uint8_t cached_pad = 0;

// Frame timing
__zpage uint32_t frame_time;
__zpage uint8_t last_nmi_frame = 0;

// NMI pending latch for demand-driven interrupt delivery
__zpage uint8_t nmi_pending = 0;

// NMI pacing — count main-loop iterations since the last NMI delivery.
// In interpreter mode, step6502() executes 1 instruction per iteration
// (500 iterations ≈ 500 guest instructions).
// In recompiler mode, run_6502() executes up to 64 batches of compiled
// blocks per call, so each iteration is thousands of guest instructions.
// The nmi_active/nmi_sp_guard mechanism already prevents re-entrant NMIs,
// so the step gate only needs to let the NMI handler finish one dispatch
// round-trip — a small threshold suffices.
#ifdef INTERPRETER_ONLY
#define NMI_MIN_GUEST_STEPS  500
#else
// Each run_6502() executes up to 64 batch dispatches.  Only non-NMI
// steps count (gated by !nmi_active in the main loop), so the guest
// is guaranteed this many run_6502() calls of main-code execution
// between NMI deliveries.
#define NMI_MIN_GUEST_STEPS  8
#endif
__zpage uint16_t guest_steps_since_nmi = 0;

// NMI nesting prevention — on real Asteroids hardware the NMI line is
// edge-triggered; only one NMI is active at a time.  If we deliver a
// second NMI before the handler RTIs, the stack grows into the
// interlock bytes ($01D0/$01FF) and the handler spins forever.
// Uses nmi_active + nmi_sp_guard (SP-based RTI detection) so it works
// in both interpreter and recompiler mode.  The dynamos.c batch
// dispatch also checks these to break out when RTI completes.
uint8_t nmi_active = 0;
uint8_t nmi_sp_guard;

// Vector RAM page select ($3200 write): 0 or 1
// Page 0 = CPU writes $4000-$43FF, DVG reads $4400-$47FF
// Page 1 = CPU writes $4400-$47FF, DVG reads $4000-$43FF
__zpage uint8_t vector_page = 0;

// Auto coin-insert: simulate coin + 1P start during boot attract cycle
__zpage uint16_t boot_frame_count = 0;
#define BOOT_COIN_FRAME     120   // simulate coin at frame ~120
#define BOOT_COIN_DURATION  10    // hold for 10 frames
#define BOOT_START_FRAME    150   // simulate 1P start at frame ~150
#define BOOT_START_DURATION 10    // hold for 10 frames

// ==========================================================================
// Trace markers — write sentinel bytes to WRAM for host-side trace analysis.
// Mesen's CPU trace log shows STA $7EF0 writes, letting you correlate
// host NES execution with guest emulator events.
// Gated by ENABLE_METRICS so production builds have zero overhead.
// ==========================================================================
#ifdef ENABLE_METRICS
#define TRACE_MARKER   (*(volatile uint8_t*)0x7EF0)
#define TRACE_MARK(v)  (TRACE_MARKER = (v))
// Marker values:
//   0x01 = before step6502
//   0x02 = after step6502
//   0x10 = NMI delivery (low nibble = nmi_pending before decrement)
//   0x1F = after nmi6502 returns
//   0xF0 = NES VBlank detected (new frame)
//   0xFE = NMI delivery skipped (gate)
#else
#define TRACE_MARK(v)  ((void)0)
#endif

// Score shadow (for incremental nametable updates)
static uint8_t score_shadow[6];  // 6 BCD digits for P1 score

// VRU list for score nametable updates
static uint8_t score_list[6 * 3 + 1];


// ==========================================================================
// POKEY LFSR — 17-bit polynomial counter (x^17 + x^3 + 1)
// Shared implementation with Millipede.
// ==========================================================================
static uint8_t pokey_random(void)
{
	uint8_t bit = ((pokey_lfsr) ^ ((pokey_lfsr) >> 14)) & 1;
	pokey_lfsr = ((pokey_lfsr) >> 1) | ((uint32_t)bit << 16);
	return (uint8_t)(pokey_lfsr & 0xFF);
}


// ==========================================================================
// DVG interpreter — parses display list, samples dots
// ==========================================================================
#pragma section bank20

#include "tools/asteroids_tiles.h"

// Read a 16-bit word from DVG address space (word-addressed).
// DVG is mapped starting at CPU $4000 (MAME: set_memory(0x4000)).
// Word 0 = $4000-$4001, word 1 = $4002-$4003, etc.
// Words 0x000-0x3FF (bytes $0000-$07FF) = vector RAM ($4000-$47FF)
// Words 0x400-0x7FF (bytes $0800-$0FFF) = unmapped ($4800-$4FFF)
// Words 0x800-0xBFF (bytes $1000-$17FF) = vector ROM ($5000-$57FF)
static uint16_t dvg_read_word(uint16_t word_addr)
{
	uint16_t byte_addr = word_addr << 1;
	uint8_t lo, hi;

	if (byte_addr < 0x0800) {
		// Vector RAM — mask to allocated size
		lo = vector_ram[byte_addr & VECTOR_RAM_MASK];
		hi = vector_ram[(byte_addr + 1) & VECTOR_RAM_MASK];
	} else if (byte_addr >= 0x1000 && byte_addr < 0x1800) {
		// Vector ROM at CPU $5000-$57FF (byte offset $1000-$17FF in DVG space)
		// Use peek_bank_byte to read from bank 23 without unmapping our own code (bank 22)
		extern uint8_t peek_bank_byte(uint8_t bank, uint16_t addr);
		uint16_t rom_off = byte_addr - 0x1000;
		lo = peek_bank_byte(BANK_PLATFORM_ROM, (uint16_t)&rom_asteroids_vec[rom_off]);
		hi = peek_bank_byte(BANK_PLATFORM_ROM, (uint16_t)&rom_asteroids_vec[rom_off + 1]);
	} else {
		// Unmapped region ($4800-$4FFF) — return 0
		lo = 0;
		hi = 0;
	}
	return (uint16_t)lo | ((uint16_t)hi << 8);
}

// Add a dot to the buffer, scaling from DVG coords (0-1023) to NES screen.
// DVG Y=0 is bottom of screen; NES Y=0 is top.
static void dvg_add_dot(int16_t dvg_x, int16_t dvg_y)
{
	if (dot_count >= DVG_MAX_DOTS) return;

	// Clamp to DVG coordinate range
	if (dvg_x < 0 || dvg_x > DVG_COORD_MAX) return;
	if (dvg_y < 0 || dvg_y > DVG_COORD_MAX) return;

	// Scale: DVG 0-1023 → NES 0-255 (X) and 0-239 (Y)
	uint8_t nx = (uint8_t)(dvg_x >> 2);        // /4 → 0-255
	uint8_t ny = (uint8_t)(239 - (dvg_y >> 2)); // /4, flip Y → 0-239

	if (ny >= 240) return;  // off-screen

	dot_buffer[dot_count].x = nx;
	dot_buffer[dot_count].y = ny;
	dot_count++;
}

// Sample points along a vector from current beam position.
// dx, dy are signed displacements in DVG coordinate space.
static void dvg_sample_vector(int16_t dx, int16_t dy)
{
	// Calculate absolute length (Manhattan distance approximation)
	int16_t adx = dx < 0 ? -dx : dx;
	int16_t ady = dy < 0 ? -dy : dy;
	int16_t length = adx > ady ? adx : ady;

	if (length == 0) {
		// Zero-length vector: just place a single dot at beam position
		dvg_add_dot(dvg.beam_x, dvg.beam_y);
		return;
	}

	// Number of dots to place along this vector.
	// Divide by sample interval, clamp to [1, DVG_MAX_DOTS_PER_VEC].
	// Use comparison chain to avoid expensive 16-bit division.
	int16_t num_dots;
#if DVG_MAX_DOTS_PER_VEC == 1
	num_dots = 1;
#else
	num_dots = 1;
	for (int16_t n = 2; n <= DVG_MAX_DOTS_PER_VEC; n++) {
		if (length >= (int16_t)(n * DVG_SAMPLE_INTERVAL))
			num_dots = n;
	}
#endif

	// Precompute step size to avoid per-iteration divides.
	// Use shifts for power-of-2 divisors; only odd divisors need real division.
	int16_t step_x, step_y;
	switch (num_dots) {
		case 1: step_x = dx; step_y = dy; break;
		case 2: step_x = dx >> 1; step_y = dy >> 1; break;
		case 4: step_x = dx >> 2; step_y = dy >> 2; break;
		default: step_x = dx / num_dots; step_y = dy / num_dots; break;
	}

	// Place dots along the vector using iterative stepping
	int16_t px = dvg.beam_x;
	int16_t py = dvg.beam_y;
	dvg_add_dot(px, py);
	for (int16_t i = 1; i <= num_dots; i++) {
		px += step_x;
		py += step_y;
		dvg_add_dot(px, py);
	}
}

// Linear search ast_shape_lookup for a DVG byte address.
// Returns shape_idx (0-89) or 0xFF if not found.
// Multi-size shapes (rocks, UFO) select variant by global_scale.
static uint8_t find_shape(uint16_t dvg_byte_addr, uint8_t gs)
{
	uint8_t first = 0xFF;
	uint8_t count = 0;
	for (uint8_t i = 0; i < AST_SHAPE_COUNT; i++) {
		if (ast_shape_lookup[i].dvg_addr == dvg_byte_addr) {
			if (first == 0xFF) first = i;
			count++;
		} else if (first != 0xFF) {
			break;  // entries are grouped — stop after last match
		}
	}
	if (first == 0xFF) return 0xFF;

	// Select size variant based on global_scale.
	// Asteroids rock status encoding: $04=large, $02=medium, $01=small.
	// At $7018 the game sets gs from status bits 0-1:
	//   bit 0 set → gs=14 (small), bit 1 set → gs=15 (medium), neither → gs=0 (large).
	// Lookup table order: offset 0=large, 1=medium, 2=small.
	uint8_t offset = 0;
	if (count == 3) {
		// Rocks: gs=0 large, gs=15 medium, gs=14 small
		if (gs < 7) offset = 0;        // large (gs=0)
		else if (gs >= 15) offset = 1; // medium (gs=15)
		else offset = 2;               // small (gs=14)
	} else if (count == 2) {
		// Ship (0x1252): gs=0 large variant, gs>=14 small variant
		offset = (gs < 7) ? 0 : 1;
	}

	return ast_shape_lookup[first + offset].shape_idx;
}

// Execute the DVG display list.
static void dvg_execute(void)
{
	dvg.pc = 0;  // start at word address 0 (= $4000 in byte space)
	dvg.sp = 0;
	dvg.halted = 0;
	dvg.intensity = 0;
	dvg.global_scale = 0;
	dot_count = 0;
	shape_instance_count = 0;

	uint8_t shape_suppress_sp = 0xFF;  // 0xFF = not inside a matched shape
	uint16_t cmd_count = 0;

	while (!dvg.halted && cmd_count < DVG_MAX_COMMANDS) {
		uint16_t word = dvg_read_word(dvg.pc);
		cmd_count++;

		// DVG instruction format (per computerarcheology.com/Arcade/Asteroids/DVG.html):
		// The opcode is in bits 15-12 of the first word.
		//   0-9: VEC, A: LABS, B: HALT, C: JSR, D: RTS, E: JMP, F: SVEC
		//
		// DVG opcodes (bits 15-12 of opcode word):
		//  0x0-0x9: VEC — draw vector at scale N
		//  0xA:     LABS — load absolute position
		//  0xB:     HALT — stop
		//  0xC:     JSRL — subroutine call
		//  0xD:     RTSL — return from subroutine
		//  0xE:     JMPL — unconditional jump
		//  0xF:     SVEC — short vector (single word)

		uint8_t opcode_nibble = (word >> 12) & 0x0F;

		if (opcode_nibble == 0x0F) {
			// SVEC — Short vector (single word)
			// Format: 1111 smYY BBBB SmXX
			//   Bit 11: s (scale low bit)
			//   Bit 10: m (Y sign, 1=negative)
			//   Bits 9-8: YY (Y magnitude, 2 bits)
			//   Bits 7-4: BBBB (brightness, 4 bits)
			//   Bit 3: S (scale high bit)
			//   Bit 2: m (X sign, 1=negative)
			//   Bits 1-0: XX (X magnitude, 2 bits)
			//   Scale Ss: 0=*2, 1=*4, 2=*8, 3=*16
			uint8_t scale_raw = ((word >> 2) & 0x02) | ((word >> 11) & 0x01); // {S,s}
			uint8_t shift = dvg.global_scale + scale_raw + 1;
			if (shift > 9) shift = 9;

			int16_t sx = (int16_t)(word & 0x03) << shift;
			if (word & 0x0004) sx = -sx;

			int16_t sy = (int16_t)((word >> 8) & 0x03) << shift;
			if (word & 0x0400) sy = -sy;

			uint8_t bright = (word >> 4) & 0x0F;

			if (bright > 0) {
				dvg.intensity = bright;
				if (shape_suppress_sp == 0xFF)
					dvg_sample_vector(sx, sy);
			}

			dvg.beam_x += sx;
			dvg.beam_y += sy;
			// fall through to dvg.pc++ (single-word instruction)

		} else if (opcode_nibble == 0x0B) {
			// HALT
			dvg.halted = 1;

		} else if (opcode_nibble == 0x0D) {
			// RTSL — return from subroutine
			if (dvg.sp > 0) {
				dvg.sp--;
				dvg.pc = dvg.stack[dvg.sp];
				// Clear shape suppression when exiting matched subroutine
				if (shape_suppress_sp != 0xFF && dvg.sp < shape_suppress_sp)
					shape_suppress_sp = 0xFF;
			} else {
				dvg.halted = 1;  // stack underflow → halt
			}
			continue;  // PC already set — don't increment

		} else if (opcode_nibble == 0x0C) {
			// JSRL — subroutine call
			uint16_t target = word & 0x0FFF;
			if (dvg.sp < DVG_STACK_DEPTH) {
				dvg.stack[dvg.sp] = dvg.pc + 1;
				dvg.sp++;

				// Shape detection: suppress dots for known shapes
				if (shape_suppress_sp == 0xFF) {
					uint16_t byte_addr = target << 1;
					uint8_t idx = find_shape(byte_addr, dvg.global_scale);
					if (idx != 0xFF) {
						if (shape_instance_count < MAX_SHAPE_INSTANCES &&
						    dvg.beam_x >= 0 && dvg.beam_x <= 1023 &&
						    dvg.beam_y >= 0 && dvg.beam_y <= 1023) {
							uint8_t sx = (uint8_t)(dvg.beam_x >> 2);
							uint8_t sy = (uint8_t)(239 - (dvg.beam_y >> 2));
							if (sy < 240) {
								shape_instances[shape_instance_count].shape_idx = idx;
								shape_instances[shape_instance_count].screen_x = sx;
								shape_instances[shape_instance_count].screen_y = sy;
								shape_instance_count++;
							}
						}
						shape_suppress_sp = dvg.sp;
					}
				}

				dvg.pc = target;
			} else {
				dvg.halted = 1;  // stack overflow → halt
			}
			continue;  // don't increment pc

		} else if (opcode_nibble == 0x0E) {
			// JMPL — unconditional jump
			dvg.pc = word & 0x0FFF;
			continue;  // don't increment pc

		} else if (opcode_nibble == 0x0A) {
			// LABS — load absolute position
			// First word: bits 15-12 = 0xA (opcode), bits 9-0 = Y position (10 bits)
			// Second word: bits 15-12 = global scale, bits 9-0 = X position (10 bits)
			uint16_t word2 = dvg_read_word(dvg.pc + 1);

			dvg.beam_y = word & 0x03FF;
			dvg.beam_x = word2 & 0x03FF;
			dvg.global_scale = (word2 >> 12) & 0x0F;
			dvg.pc += 2;
			continue;

		} else {
			// VEC — draw vector (opcode nibble 0x0-0x9 = local scale)
			// Two-word instruction:
			//   Word 1: SSSS -mYY YYYY YYYY  (S=local scale, m=Y sign)
			//   Word 2: BBBB -mXX XXXX XXXX  (B=brightness, m=X sign)
			// Effective displacement = magnitude >> (9 - total_scale)
			// where total_scale = global_scale + local_scale
			uint16_t word2 = dvg_read_word(dvg.pc + 1);
			uint8_t local_scale = opcode_nibble;

			int16_t dy_mag = word & 0x03FF;
			int16_t dx_mag = word2 & 0x03FF;

			// Total scale = global (from LABS) + local (from this VEC)
			uint8_t total_scale = dvg.global_scale + local_scale;
			int16_t dx, dy;
			if (total_scale <= 9) {
				uint8_t shift = 9 - total_scale;
				dx = dx_mag >> shift;
				dy = dy_mag >> shift;
			} else {
				uint8_t shift = total_scale - 9;
				if (shift > 6) shift = 6;
				dx = dx_mag << shift;
				dy = dy_mag << shift;
			}

			if (word & 0x0400) dy = -dy;
			if (word2 & 0x0400) dx = -dx;

			// Intensity from word2 bits 15-12
			uint8_t bright = (word2 >> 12) & 0x0F;

			if (bright > 0) {
				dvg.intensity = bright;
				if (shape_suppress_sp == 0xFF)
					dvg_sample_vector(dx, dy);
			}

			dvg.beam_x += dx;
			dvg.beam_y += dy;
			dvg.pc += 2;
			continue;
		}

		dvg.pc++;
	}

	// Always mark halted when execution ends — even if we hit
	// DVG_MAX_COMMANDS without a HALT opcode, the DVG is done.
	dvg.halted = 1;

}


#pragma section default

#pragma section bank19

#include "tools/asteroids_chr_data.h"

static void generate_chr_tiles(void)
{
	// Disable NMIs during CHR-RAM writes
	IO8(0x2000) = 0x00;

	// Clear all CHR-RAM ($0000-$1FFF)
	IO8(0x2006) = 0x00;
	IO8(0x2006) = 0x00;
	for (uint16_t i = 0; i < 0x2000; i++) IO8(0x2007) = 0;

	// === Pattern Table 0 ($0000-$0FFF): Background tiles ===

	// Tile 0: empty (already cleared)

	// Tile 1: 1x1 center dot (pixel at 3,3 in 8x8 tile)
	IO8(0x2006) = 0x00;
	IO8(0x2006) = 0x10;  // tile 1, plane 0
	for (uint8_t r = 0; r < 8; r++)
		IO8(0x2007) = (r == 3) ? 0x10 : 0x00;  // bit 4 = column 3
	for (uint8_t r = 0; r < 8; r++)
		IO8(0x2007) = 0x00;  // plane 1 = 0

	// Tile 2: 2x2 dot (pixels at 3,3 / 3,4 / 4,3 / 4,4)
	IO8(0x2006) = 0x00;
	IO8(0x2006) = 0x20;  // tile 2, plane 0
	for (uint8_t r = 0; r < 8; r++)
		IO8(0x2007) = (r == 3 || r == 4) ? 0x18 : 0x00;  // bits 4,3
	for (uint8_t r = 0; r < 8; r++)
		IO8(0x2007) = 0x00;

	// Tiles 16-25: digit font '0'-'9' for nametable score display
	// Simple 5x7 pixel font in 8x8 tiles
	static const uint8_t digit_font[10][7] = {
		{0x70,0x88,0x98,0xA8,0xC8,0x88,0x70}, // 0
		{0x20,0x60,0x20,0x20,0x20,0x20,0x70}, // 1
		{0x70,0x88,0x08,0x10,0x20,0x40,0xF8}, // 2
		{0x70,0x88,0x08,0x30,0x08,0x88,0x70}, // 3
		{0x10,0x30,0x50,0x90,0xF8,0x10,0x10}, // 4
		{0xF8,0x80,0xF0,0x08,0x08,0x88,0x70}, // 5
		{0x30,0x40,0x80,0xF0,0x88,0x88,0x70}, // 6
		{0xF8,0x08,0x10,0x20,0x40,0x40,0x40}, // 7
		{0x70,0x88,0x88,0x70,0x88,0x88,0x70}, // 8
		{0x70,0x88,0x88,0x78,0x08,0x10,0x60}, // 9
	};

	for (uint8_t d = 0; d < 10; d++) {
		uint16_t addr = (TILE_DIGIT_BASE + d) << 4;  // tile * 16 bytes
		IO8(0x2006) = (addr >> 8);
		IO8(0x2006) = (addr & 0xFF);
		IO8(0x2007) = 0x00;  // row 0 blank
		for (uint8_t r = 0; r < 7; r++)
			IO8(0x2007) = digit_font[d][r];
		// Plane 1 = 0 (monochrome)
		for (uint8_t r = 0; r < 8; r++)
			IO8(0x2007) = 0x00;
	}

	// === Pattern Table 1 ($1000-$1FFF): Pre-generated sprite tiles ===
	// Upload 254 tiles from ast_chr_data (plane 0 only, plane 1 = 0)
	IO8(0x2006) = 0x10;
	IO8(0x2006) = 0x00;  // PPU address = $1000
	{
		const uint8_t *src = ast_chr_data;
		for (uint16_t t = 0; t < AST_CHR_TILE_COUNT; t++) {
			for (uint8_t r = 0; r < 8; r++)
				IO8(0x2007) = *src++;  // plane 0
			for (uint8_t r = 0; r < 8; r++)
				IO8(0x2007) = 0x00;    // plane 1 = 0
		}
	}

	// Tile 254 (sprite PT1): 1x1 center dot for fallback dot rendering
	// PPU address is already at $1FE0 after uploading 254 tiles
	for (uint8_t r = 0; r < 8; r++)
		IO8(0x2007) = (r == 3) ? 0x10 : 0x00;  // single pixel at (3,3)
	for (uint8_t r = 0; r < 8; r++)
		IO8(0x2007) = 0x00;  // plane 1 = 0
}

#pragma section default


// ==========================================================================
// ln_fire_and_forget — non-blocking render sync (same as millipede.c)
// ==========================================================================
static void ln_fire_and_forget(void)
{
	lnPPUMASK |= 0x18;
	*(volatile uint8_t*)0x25 &= ~0x02;
	{
		uint8_t pos = *(volatile uint8_t*)0x27;
		volatile uint8_t *oam = (volatile uint8_t*)0x0200;
		while (pos != 0) {
			oam[pos] = 0xFF;
			pos += 4;
		}
		*(volatile uint8_t*)0x27 = 0;
	}
	{
		uint8_t state = *(volatile uint8_t*)0x25;
		state = (state & ~0x20) | 0x40;
		*(volatile uint8_t*)0x25 = state;
	}
	*(volatile uint8_t*)0x2F = 0;
}


// ==========================================================================
// render_video — per-frame: run DVG, place dot sprites, update score
// ==========================================================================
#pragma section bank20

void render_video_b2(void)
{
	// --- DVG pass: dot buffer + shape instances populated by dvg_execute()
	// called synchronously from write6502($3000).  Just clear the flag. ---
	if (dvg_triggered) {
		dvg_triggered = 0;
	}

	uint8_t spr_count = 0;

	// --- Shape pass: place pre-rendered metasprites ---
	// For shapes with >4 tiles, flicker alternate tile subsets each frame
	// to create authentic vector-display phosphor effect.
	{
		uint8_t spr_one[5] = {0, 0, 0, 0x00, 128};
		uint8_t phase = mux_frame & 1;
		for (uint8_t i = 0; i < shape_instance_count && spr_count < 64; i++) {
			uint8_t si = shape_instances[i].shape_idx;
			uint8_t sx = shape_instances[i].screen_x;
			uint8_t sy = shape_instances[i].screen_y;
			uint16_t offset = ast_meta_offsets[si];
			uint8_t count = ast_meta_counts[si];

			for (uint8_t j = 0; j < count && spr_count < 64; j++) {
				// Flicker: large shapes show even/odd tiles on alternate frames
				if (count > 4 && (j & 1) != phase) continue;
				const ast_metasprite_entry_t *e = &ast_metasprites[offset + j];
				spr_one[2] = e->tile_id;
				spr_one[3] = e->attr;
				uint8_t px = (uint8_t)((int16_t)sx + (int8_t)e->x_off);
				uint8_t py = (uint8_t)((int16_t)sy + (int8_t)e->y_off);
				lnAddSpr(spr_one, px, py);
				spr_count++;
			}
		}
	}

	// --- Dot pass: fill remaining OAM with multiplexed dots ---
	if (spr_count < 64) {
		uint16_t mux_dot_count = 0;
		for (uint16_t i = mux_frame; i < dot_count; i += DVG_MUX_FRAMES)
			mux_dot_count++;

		uint16_t oam_offset = 0;
		if (mux_dot_count > 0) {
			oam_offset = mux_frame * 17;
			while (oam_offset >= mux_dot_count) oam_offset -= mux_dot_count;
		}

		uint16_t dot_idx = mux_frame;
		for (uint16_t k = 0; k < oam_offset; k++) dot_idx += DVG_MUX_FRAMES;
		uint16_t wrap_span = dot_idx - mux_frame;
		for (uint16_t k = oam_offset; k < mux_dot_count; k++) wrap_span += DVG_MUX_FRAMES;
		uint8_t spr_meta[5] = {
			0, 0, 254, 0x02, 128  // tile 254 in PT1 (1x1 dot), palette 2 (bright)
		};
		for (uint16_t j = 0; j < mux_dot_count && spr_count < 64; j++) {
			if (dot_idx < dot_count) {
				lnAddSpr(spr_meta, dot_buffer[dot_idx].x, dot_buffer[dot_idx].y);
				spr_count++;
			}
			dot_idx += DVG_MUX_FRAMES;
			if (dot_idx >= dot_count) {
				dot_idx -= wrap_span;
			}
		}
	}

	// Advance mux frame
	mux_frame++;
	if (mux_frame >= DVG_MUX_FRAMES) mux_frame = 0;

	// --- Score background update ---
	// Asteroids stores Player 1 score at RAM $52-$53 (2 bytes BCD):
	//   $52 = Plr1ScoreTens (tens and hundreds digits)
	//   $53 = Plr1ScoreThous (thousands and ten-thousands digits)
	// Score is displayed as 5 digits + trailing zero (e.g., "00050" for 50 pts)
	// We display 4 BCD digits from $52-$53 plus a trailing "0"
	{
		uint8_t s0 = RAM_BASE[0x53];  // thousands & ten-thousands
		uint8_t s1 = RAM_BASE[0x52];  // tens & hundreds

		uint8_t digits[6];
		// Display as: [10K] [1K] [100] [10] [0] [0]
		digits[0] = (s0 >> 4) & 0x0F;
		digits[1] = s0 & 0x0F;
		digits[2] = (s1 >> 4) & 0x0F;
		digits[3] = s1 & 0x0F;
		digits[4] = 0;  // trailing zero (score always ends in 0)
		digits[5] = 0;  // second trailing zero for 6-digit display

		// Check if any digit changed
		uint8_t changed = 0;
		for (uint8_t i = 0; i < 6; i++) {
			if (digits[i] != score_shadow[i]) {
				changed = 1;
				score_shadow[i] = digits[i];
			}
		}

		if (changed) {
			// Build lnList to update score tiles at nametable row 1,
			// columns 2-7 (6 digits)
			uint8_t pos = 0;
			for (uint8_t i = 0; i < 6; i++) {
				uint16_t nt_addr = 0x2000 + 32 + 2 + i;  // row 1, col 2+i
				score_list[pos]     = (nt_addr >> 8);
				score_list[pos + 1] = (nt_addr & 0xFF);
				score_list[pos + 2] = TILE_DIGIT_BASE + digits[i];
				pos += 3;
			}
			score_list[pos] = 0xFF;  // lfEnd terminator
			lnList(score_list);
		}
	}

	// Non-blocking sync
	ln_fire_and_forget();
}

#pragma section default

void render_video(void)
{
	uint8_t saved_bank = mapper_prg_bank;
	bankswitch_prg(BANK_RENDER);
	render_video_b2();
	bankswitch_prg(saved_bank);
}


// ==========================================================================
// Input handling — map NES gamepad to Asteroids controls
// ==========================================================================
void nes_gamepad_refresh(void)
{
	cached_pad = lnGetPad(1);
}

uint8_t nes_gamepad(void)
{
	return cached_pad;
}


// ==========================================================================
// read6502 — Asteroids memory bus read handler
//
// Verified against MAME asteroid.cpp/asteroid_m.cpp and the Asteroids
// disassembly (asteroids_defines.asm + asteroids_program_rom.asm).
//
// Address bus is 15 bits (global_mask 0x7FFF) — applied at entry.
//
// IN0  ($2000-$2007): bit-addressed, each returns 0x80 or ~0x80 (0x7F)
//   bit 0 ($2000): unused (always 0x7F)
//   bit 1 ($2001): 3KHz clock (totalcycles & 0x100)
//   bit 2 ($2002): DVG done (ACTIVE LOW: 0x7F=done/idle, 0x80=running)
//   bit 3 ($2003): Hyperspace button (ACTIVE HIGH: 0x80=pressed)
//   bit 4 ($2004): Fire button (ACTIVE HIGH: 0x80=pressed)
//   bit 5 ($2005): Diagnostic step (ACTIVE HIGH: 0x80=pressed)
//   bit 6 ($2006): Slam/Tilt switch (ACTIVE HIGH: 0x80=tilt)
//   bit 7 ($2007): Self-test switch (ACTIVE HIGH: 0x80=test active)
//
// IN1  ($2400-$2407): bit-addressed, returns 0x80 or ~0x80
//   bit 0 ($2400): Left coin    (ACTIVE HIGH: 0x80=inserted)
//   bit 1 ($2401): Center coin  (ACTIVE HIGH)
//   bit 2 ($2402): Right coin   (ACTIVE HIGH)
//   bit 3 ($2403): 1P Start     (ACTIVE HIGH: 0x80=pressed)
//   bit 4 ($2404): 2P Start     (ACTIVE HIGH)
//   bit 5 ($2405): Thrust       (ACTIVE HIGH: 0x80=pressed)
//   bit 6 ($2406): Rotate Right (ACTIVE HIGH)
//   bit 7 ($2407): Rotate Left  (ACTIVE HIGH)
//
// DSW1 ($2800-$2803): TTL153 dual 4:1 mux, returns 2 bits in bits 7:6
//   offset 0: bits 7,6 = DIP sw 8,7 (coinage)
//   offset 1: bits 7,6 = DIP sw 6,5 (right coin mech)
//   offset 2: bits 7,6 = DIP sw 4,3 (center coin / ships per play)
//   offset 3: bits 7,6 = DIP sw 2,1 (language)
// ==========================================================================
uint8_t read6502(uint16_t address)
{
    #ifdef DEBUG_AUDIO
        audio ^= address >> 8;
        IO8(0x4011) = audio;
    #endif

	// 15-bit address bus
	address &= 0x7FFF;

	// RAM: $0000-$03FF (1KB)
	if (address < 0x0400)
		return RAM_BASE[address];

	// IN0: $2000-$2007 — bit-addressed switch reads
	// MAME asteroid_IN0_r: reads full port, extracts 1 bit via offset,
	// returns 0x80 if bit set, ~0x80 (0x7F) if not.
	if (address >= 0x2000 && address <= 0x2007) {
		uint8_t bit_sel = address & 0x07;
		switch (bit_sel) {
		case 0: // Unknown / unused — always return 0x7F (bit clear)
			return 0x7F;
		case 1: // 3KHz clock — MAME: (total_cycles & 0x100) ? 1 : 0
			return (clockticks6502 & 0x100) ? 0x80 : 0x7F;
		case 2: // DVG done — ACTIVE LOW via IP_ACTIVE_LOW in MAME
			// Port bit 2 = 0 when done (idle), 1 when running (busy)
			// So: halted → bit clear → return 0x7F
			//     running → bit set → return 0x80
			return dvg.halted ? 0x7F : 0x80;
		case 3: // Hyperspace — ACTIVE HIGH
			return (cached_pad & lfSelect) ? 0x80 : 0x7F;
		case 4: // Fire — ACTIVE HIGH
			return (cached_pad & lfA) ? 0x80 : 0x7F;
		case 5: // Diagnostic step — not pressed
			return 0x7F;
		case 6: // Slam/Tilt — not active
			return 0x7F;
		case 7: // Self-test — not active (must be 0x7F for NMIs to fire)
			return 0x7F;
		}
	}

	// IN1: $2400-$2407 — bit-addressed player controls
	// MAME asteroid_IN1_r: same bit-extraction as IN0.
	// All buttons are ACTIVE HIGH: 0x80=pressed, 0x7F=not pressed.
	if (address >= 0x2400 && address <= 0x2407) {
		uint8_t bit_sel = address & 0x07;
		switch (bit_sel) {
		case 0: { // Left coin
			uint8_t pressed = 0;
			// Auto coin-insert during boot attract
			if (boot_frame_count >= BOOT_COIN_FRAME &&
			    boot_frame_count < BOOT_COIN_FRAME + BOOT_COIN_DURATION)
				pressed = 1;
			return pressed ? 0x80 : 0x7F;
		}
		case 1: // Center coin — not inserted
			return 0x7F;
		case 2: // Right coin — not inserted
			return 0x7F;
		case 3: { // 1P Start — ACTIVE HIGH
			uint8_t pressed = (cached_pad & lfStart) ? 1 : 0;
			// Auto-start during boot attract
			if (boot_frame_count >= BOOT_START_FRAME &&
			    boot_frame_count < BOOT_START_FRAME + BOOT_START_DURATION)
				pressed = 1;
			return pressed ? 0x80 : 0x7F;
		}
		case 4: // 2P Start — not pressed
			return 0x7F;
		case 5: // Thrust — ACTIVE HIGH
			return (cached_pad & lfB) ? 0x80 : 0x7F;
		case 6: // Rotate Right — ACTIVE HIGH
			return (cached_pad & lfR) ? 0x80 : 0x7F;
		case 7: // Rotate Left — ACTIVE HIGH
			return (cached_pad & lfL) ? 0x80 : 0x7F;
		}
	}

	// DSW1: $2800-$2803 — DIP switches via TTL153 dual 4:1 multiplexer
	// MAME asteroid_DSW1_r: reads all 8 DIP switch bits, selects 2 at a time
	// based on offset. Returns selected pair in bits 7 and 6, other bits = 0xFC mask.
	//
	// DIP switch byte layout (bit 7..0):
	//   bit 7,6: Coinage (00=free play, 01=1coin/2play, 10=1coin/1play, 11=2coin/1play)
	//   bit 5,4: Right coin mech multiplier
	//   bit 3:   Center coin mech
	//   bit 2:   Ships per game (0=4ships, 1=3ships)
	//   bit 1,0: Language (00=English, 01=German, 10=French, 11=Spanish)
	//
	// Defaults: Free play ON (bits 7:6=00), standard coins, 4 ships (bit2=0), English
	if (address >= 0x2800 && address <= 0x2803) {
		// DIP switch byte — all 8 bits
		// Free play: bits 7:6 = 00
		// Right coin: bits 5:4 = 00 (1x multiplier)
		// Center coin: bit 3 = 0
		// Ships: bit 2 = 0 (4 ships per game)
		// Language: bits 1:0 = 00 (English)
		uint8_t dsw = 0x00;  // free play, 4 ships, English

		uint8_t offset = address & 0x03;
		// TTL153 mux: offset selects which 2-bit pair
		// Mux A output → data bus bit 7
		// Mux B output → data bus bit 6
		// Mux A selects (offset → DIP): 0→DIP8(bit7), 1→DIP6(bit5), 2→DIP4(bit3), 3→DIP2(bit1)
		// Mux B selects (offset → DIP): 0→DIP7(bit6), 1→DIP5(bit4), 2→DIP3(bit2), 3→DIP1(bit0)
		uint8_t bit_a, bit_b;
		switch (offset) {
		case 0: bit_a = (dsw >> 7) & 1; bit_b = (dsw >> 6) & 1; break;
		case 1: bit_a = (dsw >> 5) & 1; bit_b = (dsw >> 4) & 1; break;
		case 2: bit_a = (dsw >> 3) & 1; bit_b = (dsw >> 2) & 1; break;
		default: bit_a = (dsw >> 1) & 1; bit_b = (dsw >> 0) & 1; break;
		}
		return (bit_a << 7) | (bit_b << 6);
	}

	// Vector RAM: $4000-$47FF (2KB)
	if (address >= 0x4000 && address < 0x4800)
		return vector_ram[(address - 0x4000) & VECTOR_RAM_MASK];

	// Vector ROM: $5000-$57FF (primary, per MAME and disassembly)
	if (address >= 0x5000 && address < 0x5800) {
		uint8_t saved_bank = mapper_prg_bank;
		bankswitch_prg(BANK_PLATFORM_ROM);
		uint8_t val = rom_asteroids_vec[address - 0x5000];
		bankswitch_prg(saved_bank);
		return val;
	}

	// Program ROM: $6800-$7FFF
	if (address >= 0x6800) {
		uint8_t saved_bank = mapper_prg_bank;
		bankswitch_prg(BANK_PLATFORM_ROM);
		uint8_t val = rom_asteroids[address - 0x6800];
		bankswitch_prg(saved_bank);
		return val;
	}

	// No ROM mirror needed — 15-bit address bus means $8000+ is already
	// masked to $0000+ by the & 0x7FFF at entry.

	return 0x00;  // unmapped
}


// ==========================================================================
// write6502 — Asteroids memory bus write handler
// ==========================================================================
void write6502(uint16_t address, uint8_t value)
{
	// 15-bit address bus
	address &= 0x7FFF;

	// RAM: $0000-$03FF
	if (address < 0x0400) {
		RAM_BASE[address] = value;
		return;
	}

	// IN0/IN1/DSW1 address ranges ($2000-$2803) are read-only — ignore writes
	if (address >= 0x2000 && address < 0x2C00)
		return;

	// DVG Go: $3000 write — trigger DVG execution.
	// Execute synchronously: the guest polls $2002 for DVG-idle after
	// writing here.  On real hardware the DVG finishes in microseconds;
	// deferring to the next VBlank would make the guest spin forever.
	if (address == 0x3000) {
		uint8_t saved_bank = mapper_prg_bank;
		bankswitch_prg(BANK_RENDER);
		dvg_execute();
		bankswitch_prg(saved_bank);
		dvg_triggered = 1;  // tell render_video_b2 to show the new dots/shapes
		return;
	}

	// MultiPurp register: $3200 write — output latch
	// Bit 0: Player 2 button lamp
	// Bit 1: Player 1 button lamp
	// Bit 2: RAMSEL — swap RAM banks $0200-$02FF <-> $0300-$03FF
	// Bit 3: Left coin counter enable
	// Bit 4: Center coin counter enable
	// Bit 5: Right coin counter enable
	if (address == 0x3200) {
		uint8_t old_ramsel = vector_page & 0x04;
		uint8_t new_ramsel = value & 0x04;

		// If RAMSEL bit changed, swap the two 256-byte RAM pages
		if (old_ramsel != new_ramsel) {
			for (uint16_t i = 0; i < 256; i++) {
				uint8_t tmp = RAM_BASE[0x200 + i];
				RAM_BASE[0x200 + i] = RAM_BASE[0x300 + i];
				RAM_BASE[0x300 + i] = tmp;
			}
		}

		vector_page = value;  // store full latch value
		return;
	}

	// Watchdog: $3400
	if (address == 0x3400)
		return;

	// Explosion sound: $3600
	if (address == 0x3600)
		return;

	// Thump controls: $3800-$3802
	if (address >= 0x3800 && address <= 0x3802)
		return;

	// Thump/sound trigger: $3A00
	if (address == 0x3A00)
		return;

	// Sound enables and output latches: $3C00-$3C05
	if (address >= 0x3C00 && address <= 0x3C05)
		return;

	// LED / coin counter: $3E00
	if (address == 0x3E00)
		return;

	// Catch-all for $3000-$3FFF (other output latches we didn't handle)
	if (address >= 0x3000 && address < 0x4000)
		return;

	// Vector RAM: $4000-$47FF (2KB)
	if (address >= 0x4000 && address < 0x4800) {
		vector_ram[(address - 0x4000) & VECTOR_RAM_MASK] = value;
		return;
	}

	// Writes to ROM or unmapped areas are ignored
}


// ==========================================================================
// flash_format for Asteroids
// ==========================================================================
#pragma section bank19

#ifdef ENABLE_STATIC_ANALYSIS
extern uint8_t sa_code_bitmap[];
extern uint8_t sa_indirect_list[];
extern uint8_t sa_subroutine_list[];
#ifdef ENABLE_AUTO_IDLE_DETECT
extern uint8_t sa_idle_list[];
#endif
#endif

static void flash_format_b2(void)
{
	for (uint8_t bank = 3; bank < 31; bank++)
	{
		if (bank == BANK_COMPILE)      continue;
		if (bank == BANK_RENDER)       continue;
		if (bank == BANK_METRICS)      continue;
		if (bank == BANK_PLATFORM_ROM) continue;
		if (bank == BANK_SA_CODE)      continue;
		if (bank == BANK_INIT_CODE)    continue;
		if (bank == BANK_IR_OPT)       continue;

		for (uint16_t sector = 0x8000; sector < 0xC000; sector += 0x1000)
		{
#ifdef ENABLE_STATIC_ANALYSIS
			if (bank == 3 && sector >= SA_SECTOR_FIRST && sector <= SA_SECTOR_LAST)
				continue;
#endif
			flash_sector_erase(sector, bank);
		}
	}
}

#pragma section default

void flash_format(void)
{
	uint8_t saved_bank = mapper_prg_bank;
	bankswitch_prg(BANK_INIT_CODE);
	flash_format_b2();
	bankswitch_prg(saved_bank);
}


// ==========================================================================
// main() — Asteroids emulator entry point
// ==========================================================================
int main(void)
{
	lnSync(1);

	// Fix PPU increment mode before palette push
	lnPPUCTRL &= ~0x08;
	IO8(0x2000) = lnPPUCTRL;

	// Initial palette: black BG, brightness-graded sprite palettes
	{
		static const uint8_t default_pal[] = {
			0x0F,0x20,0x10,0x00,  // BG palette 0: black, white, gray, dark
			0x0F,0x20,0x10,0x00,  // BG palette 1
			0x0F,0x20,0x10,0x00,  // BG palette 2
			0x0F,0x20,0x10,0x00,  // BG palette 3
			0x0F,0x00,0x00,0x00,  // Sprite palette 0: dim (DVG brightness 1-3)
			0x0F,0x10,0x10,0x10,  // Sprite palette 1: medium (brightness 4-7)
			0x0F,0x20,0x20,0x20,  // Sprite palette 2: bright (brightness 8-11)
			0x0F,0x30,0x30,0x30,  // Sprite palette 3: full (brightness 12-15)
		};
		lnPush(0x3F00, 32, default_pal);
	}

	// Generate CHR tiles (dots + font)
	{
		uint8_t saved_bank = mapper_prg_bank;
		bankswitch_prg(BANK_SA_CODE);  // bank 19 for init code
		generate_chr_tiles();
		bankswitch_prg(saved_bank);
	}

	// Re-enable NMIs
	IO8(0x2000) = lnPPUCTRL;

	// Clear nametable 0
	IO8(0x2006) = 0x20;
	IO8(0x2006) = 0x00;
	for (uint16_t i = 0; i < 1024; i++) IO8(0x2007) = 0;

	lnSync(0);

	// Zero guest RAM before reset.
	// Asteroids' reset routine clears $0000-$03FF itself, but the NMI
	// handler ($7B65) checks interlock bytes $01FF/$01D0 immediately.
	// With INTERPRETER_ONLY the emulator is too slow — the first NES
	// VBlank fires before the clear loop reaches $01D0, and NES WRAM
	// contains random data, so the NMI handler spins forever at $7B71.
	for (uint16_t i = 0; i < 0x0400; i++) RAM_BASE[i] = 0;

	// Also zero vector RAM ($4000-$47FF) so DVG doesn't process garbage
	for (uint16_t i = 0; i < VECTOR_RAM_SIZE; i++) vector_ram[i] = 0;

	reset6502();

	// Disable APU frame IRQ
	IO8(0x4017) = 0x40;

	// PPUCTRL: BG from pattern table 0, sprites from pattern table 1
	lnPPUCTRL |= 0x08;  // sprite pattern table = 1
	lnPPUMASK = 0x3E;   // show BG + sprites + left-8px

#ifdef ENABLE_CACHE_PERSIST
	flash_init_persist();
#else
	flash_format();
#endif
	flash_cache_init_sectors();

#ifdef ENABLE_STATIC_ANALYSIS
	sa_run();
#endif

	// Initialize DVG state
	dvg.halted = 1;
	dvg.pc = 0;
	dvg.sp = 0;
	dvg.beam_x = 0;
	dvg.beam_y = 0;
	dvg.intensity = 0;
	dvg.global_scale = 0;
	dvg_triggered = 0;
	dot_count = 0;
	mux_frame = 0;
	nmi_pending = 0;
	guest_steps_since_nmi = NMI_MIN_GUEST_STEPS; // allow first NMI immediately

	// Clear score shadow
	for (uint8_t i = 0; i < 6; i++) score_shadow[i] = 0xFF;

	last_nmi_frame = *(volatile uint8_t*)0x26;

#ifdef TRACK_TICKS
	frame_time = clockticks6502 + FRAME_LENGTH;
#else
	frame_time = 0;
#endif

	// ---- Main loop ----
	while (1)
	{
		TRACE_MARK(0x01);
#ifdef INTERPRETER_ONLY
		step6502();
#else
		run_6502();
#endif
		TRACE_MARK(0x02);
		// Only count non-NMI guest steps — the counter gates NMI delivery
		// so the guest gets breathing room *between* handlers.  Without
		// this guard the counter climbs during handler execution and the
		// next NMI fires the instant RTI completes, starving init/main.
		if (!nmi_active)
			guest_steps_since_nmi++;

		// Detect RTI — NMI handler has returned (SP restored to pre-NMI level).
		// Works for both interpreter and recompiler mode (last_interpreted_opcode
		// is not set by compiled blocks, so we use the SP guard instead).
		if (nmi_active && sp == nmi_sp_guard)
			nmi_active = 0;

		// Asteroids NMI runs at CLOCK_3KHZ/12 ≈ 250 Hz.
		// The game increments NmiCounter every NMI, and every 4th NMI
		// increments FrameCounter (62.5 game frames/sec).
		// The NES emulator is far too slow for 4 NMIs per VBlank —
		// deliver 1 NMI per VBlank.  The game still runs, just at
		// ~60 Hz NMI rate instead of ~250 Hz.  FrameCounter will
		// advance every frame instead of every 4th NMI.
		// NMI delivery is gated on guest_steps_since_nmi to prevent
		// nesting into an in-progress handler.
#ifndef TRACK_TICKS
		{
			uint8_t cur_nmi = *(volatile uint8_t*)0x26;
			if (cur_nmi != last_nmi_frame)
			{
				TRACE_MARK(0xF0);
				if (boot_frame_count < 0xFFFF) boot_frame_count++;
#ifdef ENABLE_METRICS
				{ uint8_t _mb = mapper_prg_bank; bankswitch_prg(BANK_METRICS); metrics_dump_runtime_b2(); bankswitch_prg(_mb); }
#endif
				nes_gamepad_refresh();
				render_video();
				last_nmi_frame = *(volatile uint8_t*)0x26;

				// Queue 1 NMI per NES VBlank
				nmi_pending = 1;
			}

			// Deliver pending NMI only after guest has had enough
			// steps for the previous handler to complete
			if (nmi_pending > 0 && guest_steps_since_nmi >= NMI_MIN_GUEST_STEPS && !nmi_active)
			{
				TRACE_MARK(0x10 | nmi_pending);
				nmi_pending--;
				guest_steps_since_nmi = 0;

				// Prevent the NMI overrun watchdog ($5B >= 4 → spin at
				// $7B81).  On real hardware 4 NMIs fire per game frame;
				// we deliver only 1 per NES VBlank.  Clear FrameCounter
				// before the NMI so the handler's INC $5B can't reach 4.
				RAM_BASE[0x5B] = 0;

				nmi_sp_guard = sp;
				nmi_active = 1;
				nmi6502();

				// Ensure FrameCounter ($5B) is non-zero after the NMI
				// handler returns.  The main loop polls LSR $5B / BCC
				// $680C — it stalls until bit 0 is set.  On real
				// hardware the 4th NMI does INC $5B every game frame,
				// but we only deliver 1 NMI per VBlank, so 3 out of 4
				// deliveries skip the INC.  Force $5B = 1 to unblock
				// the main loop every frame.
				RAM_BASE[0x5B] = 1;

				TRACE_MARK(0x1F);
			}
		}
#else
		if (clockticks6502 > frame_time)
		{
			frame_time += FRAME_LENGTH;
			interrupt_condition |= FLAG_ASTEROIDS_NMI;
		}
		{
			uint8_t cur_nmi = *(volatile uint8_t*)0x26;
			if (cur_nmi != last_nmi_frame)
			{
				last_nmi_frame = cur_nmi;
				if (boot_frame_count < 0xFFFF) boot_frame_count++;
#ifdef ENABLE_METRICS
				{ uint8_t _mb = mapper_prg_bank; bankswitch_prg(BANK_METRICS); metrics_dump_runtime_b2(); bankswitch_prg(_mb); }
#endif
				nes_gamepad_refresh();
				render_video();
			}
		}
#endif
		// Fire NMI — Asteroids uses NMI for its ~250 Hz game tick
		if ((interrupt_condition & FLAG_ASTEROIDS_NMI) && !nmi_active)
		{
			interrupt_condition &= ~FLAG_ASTEROIDS_NMI;
			nmi_sp_guard = sp;
			nmi_active = 1;
			nmi6502();
		}
	}
}
