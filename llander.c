// ==========================================================================
// llander.c — Atari Lunar Lander arcade platform driver for DynaMoS
//
// Emulates the Lunar Lander arcade hardware (6502 @ 1.5 MHz) running on
// NES hardware via the DynaMoS dynamic binary translator.
//
// Display: DVG vector output → point-sampled dot sprites (3-frame mux)
// Audio:   discrete circuits (not emulated)
// Input:   NES gamepad mapped to rotate/thrust(analog)/select/abort
// ==========================================================================

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "lazynes.h"
#include "config.h"
#include "llander.h"
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
	extern const unsigned char rom_llander[];       // 8KB program ROM ($6000-$7FFF)
	extern const unsigned char rom_llander_vec[];    // 6KB vector ROM ($4800-$5FFF)
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
// Lunar Lander-specific state
// ==========================================================================
__zpage uint8_t interrupt_condition;
__zpage uint8_t screen_ram_updated = 0;
__zpage uint8_t character_ram_updated = 0;  // required by ASM symbol

// Vector RAM ($4000-$47FF) — writable display list
// Physical RAM is 1KB (two 2114 chips); $4400-$47FF mirrors $4000-$43FF
#define VECTOR_RAM_SIZE 1024
#define VECTOR_RAM_MASK 0x3FF
static uint8_t vector_ram[VECTOR_RAM_SIZE];

// DVG state
static dvg_state_t dvg;
__zpage uint8_t dvg_triggered = 0;  // set by write to $3000 (DVG Go)
__zpage uint8_t dvg_ever_started = 0;  // true after first VGGO (for DVG halt status)
__zpage uint8_t nmi_enabled = 0;       // set by $3E00 write — gates NMI (matches MAME periodic interrupt)

// Dot buffer — filled by DVG interpreter each frame
static dvg_dot_t dot_buffer[DVG_MAX_DOTS];
__zpage uint16_t dot_count = 0;

// Sprite multiplex frame counter
__zpage uint8_t mux_frame = 0;

// Cached gamepad state
static uint8_t cached_pad = 0;

// Analog thrust lever (0x00 = no thrust, 0xFF = max thrust)
// Ramped by D-pad Up/Down each frame, holds position when idle.
__zpage uint8_t thrust_level = 0;
#define THRUST_RAMP_RATE  4

// Frame timing
__zpage uint32_t frame_time;
__zpage uint8_t last_nmi_frame = 0;

// NMI pending latch for demand-driven interrupt delivery
__zpage uint8_t nmi_pending = 0;

// NMI pacing
#ifdef INTERPRETER_ONLY
#define NMI_MIN_GUEST_STEPS  500
#else
#define NMI_MIN_GUEST_STEPS  8
#endif
__zpage uint16_t guest_steps_since_nmi = 0;

// NMI nesting prevention
uint8_t nmi_active = 0;
uint8_t nmi_sp_guard;

// LED output latch ($3200 write)
__zpage uint8_t led_latch = 0;

// 3KHz clock toggle (IN0 bit 0) — toggles on each read of $2000
__zpage uint8_t clock_3khz = 0;

// Auto coin-insert: simulate coin + 1P start during boot attract cycle
__zpage uint16_t boot_frame_count = 0;
#define BOOT_COIN_FRAME     120   // simulate coin at frame ~120
#define BOOT_COIN_DURATION  10    // hold for 10 frames
#define BOOT_START_FRAME    150   // simulate 1P start at frame ~150
#define BOOT_START_DURATION 10    // hold for 10 frames

// ==========================================================================
// Trace markers
// ==========================================================================
#ifdef ENABLE_METRICS
#define TRACE_MARKER   (*(volatile uint8_t*)0x7EF0)
#define TRACE_MARK(v)  (TRACE_MARKER = (v))
#else
#define TRACE_MARK(v)  ((void)0)
#endif

// Score shadow (for incremental nametable updates)
static uint8_t score_shadow[6];
static uint8_t score_list[6 * 3 + 1];


// ==========================================================================
// DVG interpreter — parses display list, samples dots
// ==========================================================================
#pragma section bank20

// Read a 16-bit word from DVG address space (word-addressed).
// DVG is mapped starting at CPU $4000.
// Words 0x000-0x3FF (bytes $0000-$07FF) = vector RAM ($4000-$47FF)
// Words 0x400-0x7FF (bytes $0800-$0FFF) = vector ROM ($4800-$4FFF)
// Words 0x800-0xBFF (bytes $1000-$17FF) = vector ROM ($5000-$57FF)
// Words 0xC00-0xFFF (bytes $1800-$1FFF) = vector ROM ($5800-$5FFF)
static uint16_t dvg_read_word(uint16_t word_addr)
{
	uint16_t byte_addr = word_addr << 1;
	uint8_t lo, hi;

	if (byte_addr < 0x0800) {
		// Vector RAM — use full 2KB
		lo = vector_ram[byte_addr & VECTOR_RAM_MASK];
		hi = vector_ram[(byte_addr + 1) & VECTOR_RAM_MASK];
	} else if (byte_addr >= 0x0800 && byte_addr < 0x2000) {
		// Vector ROM at CPU $4800-$5FFF (byte offset $0800-$1FFF in DVG space)
		extern uint8_t peek_bank_byte(uint8_t bank, uint16_t addr);
		uint16_t rom_off = byte_addr - 0x0800;
		lo = peek_bank_byte(BANK_PLATFORM_ROM, (uint16_t)&rom_llander_vec[rom_off]);
		hi = peek_bank_byte(BANK_PLATFORM_ROM, (uint16_t)&rom_llander_vec[rom_off + 1]);
	} else {
		// Unmapped — return 0
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
static void dvg_sample_vector(int16_t dx, int16_t dy)
{
	int16_t adx = dx < 0 ? -dx : dx;
	int16_t ady = dy < 0 ? -dy : dy;
	int16_t length = adx > ady ? adx : ady;

	if (length == 0) {
		dvg_add_dot(dvg.beam_x, dvg.beam_y);
		return;
	}

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

	int16_t step_x, step_y;
	switch (num_dots) {
		case 1: step_x = dx; step_y = dy; break;
		case 2: step_x = dx >> 1; step_y = dy >> 1; break;
		case 4: step_x = dx >> 2; step_y = dy >> 2; break;
		default: step_x = dx / num_dots; step_y = dy / num_dots; break;
	}

	int16_t px = dvg.beam_x;
	int16_t py = dvg.beam_y;
	dvg_add_dot(px, py);
	for (int16_t i = 1; i <= num_dots; i++) {
		px += step_x;
		py += step_y;
		dvg_add_dot(px, py);
	}
}

// Execute the DVG display list.
static void dvg_execute(void)
{
	dvg.pc = 0;
	dvg.sp = 0;
	dvg.halted = 0;
	dvg.intensity = 0;
	dvg.global_scale = 0;
	dot_count = 0;

	uint16_t cmd_count = 0;

	while (!dvg.halted && cmd_count < DVG_MAX_COMMANDS) {
		uint16_t word = dvg_read_word(dvg.pc);
		cmd_count++;

		uint8_t opcode_nibble = (word >> 12) & 0x0F;

		if (opcode_nibble == 0x0F) {
			// SVEC — Short vector (single word)
			uint8_t scale_raw = ((word >> 2) & 0x02) | ((word >> 11) & 0x01);
			uint8_t shift = dvg.global_scale + scale_raw + 1;
			if (shift > 9) shift = 9;

			int16_t sx = (int16_t)(word & 0x03) << shift;
			if (word & 0x0004) sx = -sx;

			int16_t sy = (int16_t)((word >> 8) & 0x03) << shift;
			if (word & 0x0400) sy = -sy;

			uint8_t bright = (word >> 4) & 0x0F;

			if (bright > 0) {
				dvg.intensity = bright;
				dvg_sample_vector(sx, sy);
			}

			dvg.beam_x += sx;
			dvg.beam_y += sy;

		} else if (opcode_nibble == 0x0B) {
			// HALT
			dvg.halted = 1;

		} else if (opcode_nibble == 0x0D) {
			// RTSL
			if (dvg.sp > 0) {
				dvg.sp--;
				dvg.pc = dvg.stack[dvg.sp];
			} else {
				dvg.halted = 1;
			}
			continue;

		} else if (opcode_nibble == 0x0C) {
			// JSRL
			uint16_t target = word & 0x0FFF;
			if (dvg.sp < DVG_STACK_DEPTH) {
				dvg.stack[dvg.sp] = dvg.pc + 1;
				dvg.sp++;
				dvg.pc = target;
			} else {
				dvg.halted = 1;
			}
			continue;

		} else if (opcode_nibble == 0x0E) {
			// JMPL
			dvg.pc = word & 0x0FFF;
			continue;

		} else if (opcode_nibble == 0x0A) {
			// LABS
			uint16_t word2 = dvg_read_word(dvg.pc + 1);

			dvg.beam_y = word & 0x03FF;
			dvg.beam_x = word2 & 0x03FF;
			dvg.global_scale = (word2 >> 12) & 0x0F;
			dvg.pc += 2;
			continue;

		} else {
			// VEC
			uint16_t word2 = dvg_read_word(dvg.pc + 1);
			uint8_t local_scale = opcode_nibble;

			int16_t dy_mag = word & 0x03FF;
			int16_t dx_mag = word2 & 0x03FF;

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

			uint8_t bright = (word2 >> 12) & 0x0F;

			if (bright > 0) {
				dvg.intensity = bright;
				dvg_sample_vector(dx, dy);
			}

			dvg.beam_x += dx;
			dvg.beam_y += dy;
			dvg.pc += 2;
			continue;
		}

		dvg.pc++;
	}

	dvg.halted = 1;
}

#pragma section default

#pragma section bank19

static void generate_chr_tiles(void)
{
	IO8(0x2000) = 0x00;

	// Clear all CHR-RAM ($0000-$1FFF)
	IO8(0x2006) = 0x00;
	IO8(0x2006) = 0x00;
	for (uint16_t i = 0; i < 0x2000; i++) IO8(0x2007) = 0;

	// === Pattern Table 0 ($0000-$0FFF): Background tiles ===

	// Tile 1: 1x1 center dot
	IO8(0x2006) = 0x00;
	IO8(0x2006) = 0x10;
	for (uint8_t r = 0; r < 8; r++)
		IO8(0x2007) = (r == 3) ? 0x10 : 0x00;
	for (uint8_t r = 0; r < 8; r++)
		IO8(0x2007) = 0x00;

	// Tile 2: 2x2 dot
	IO8(0x2006) = 0x00;
	IO8(0x2006) = 0x20;
	for (uint8_t r = 0; r < 8; r++)
		IO8(0x2007) = (r == 3 || r == 4) ? 0x18 : 0x00;
	for (uint8_t r = 0; r < 8; r++)
		IO8(0x2007) = 0x00;

	// Tiles 16-25: digit font '0'-'9'
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
		uint16_t addr = (TILE_DIGIT_BASE + d) << 4;
		IO8(0x2006) = (addr >> 8);
		IO8(0x2006) = (addr & 0xFF);
		IO8(0x2007) = 0x00;
		for (uint8_t r = 0; r < 7; r++)
			IO8(0x2007) = digit_font[d][r];
		for (uint8_t r = 0; r < 8; r++)
			IO8(0x2007) = 0x00;
	}

	// === Pattern Table 1 ($1000-$1FFF): Sprite tiles ===

	// Tile 0 (sprite): 1x1 center dot
	IO8(0x2006) = 0x10;
	IO8(0x2006) = 0x00;
	for (uint8_t r = 0; r < 8; r++)
		IO8(0x2007) = (r == 3) ? 0x10 : 0x00;
	for (uint8_t r = 0; r < 8; r++)
		IO8(0x2007) = 0x00;

	// Tile 1 (sprite): 2x2 dot
	IO8(0x2006) = 0x10;
	IO8(0x2006) = 0x10;
	for (uint8_t r = 0; r < 8; r++)
		IO8(0x2007) = (r == 3 || r == 4) ? 0x18 : 0x00;
	for (uint8_t r = 0; r < 8; r++)
		IO8(0x2007) = 0x00;
}

#pragma section default


// ==========================================================================
// ln_fire_and_forget — non-blocking render sync
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
	if (dvg_triggered) {
		dvg_triggered = 0;
	}

	// --- Sprite pass: place dots for this mux frame ---
	uint8_t spr_count = 0;
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
		0, 0, 0, 0x00, 128
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

	mux_frame++;
	if (mux_frame >= DVG_MUX_FRAMES) mux_frame = 0;

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
// Input handling — map NES gamepad to Lunar Lander controls
// ==========================================================================
void nes_gamepad_refresh(void)
{
	cached_pad = lnGetPad(1);

	// Update analog thrust lever from D-pad Up/Down
	if (cached_pad & lfU) {
		// Increase thrust
		if (thrust_level <= 0xFF - THRUST_RAMP_RATE)
			thrust_level += THRUST_RAMP_RATE;
		else
			thrust_level = 0xFF;
	} else if (cached_pad & lfD) {
		// Decrease thrust
		if (thrust_level >= THRUST_RAMP_RATE)
			thrust_level -= THRUST_RAMP_RATE;
		else
			thrust_level = 0x00;
	}
	// If neither Up nor Down, thrust holds current position (analog lever)
}

uint8_t nes_gamepad(void)
{
	return cached_pad;
}


// ==========================================================================
// read6502 — Lunar Lander memory bus read handler
//
// Verified against MAME llander.cpp memory map.
//
// Address bus is 15 bits (global_mask 0x7FFF) — applied at entry.
//
// IN0  ($2000): Full byte read (NOT bit-addressed like Asteroids)
//   bit 0: unused (0)
//   bit 1: self-test (ACTIVE LOW: default 1 = not in test)
//   bit 2: tilt (0)
//   bit 3: DVG halt status (0=halted/done, 1=running)
//   bit 4: Select Game button (ACTIVE HIGH)
//   bit 5: Abort button (ACTIVE HIGH)
//   bits 6-7: unused (0)
//
// IN1  ($2400-$2407): bit-addressed, returns 0x80 or 0x7F
//   bit 0 ($2400): 1P Start (ACTIVE HIGH)
//   bit 1 ($2401): unused
//   bit 2 ($2402): Right Rotate (ACTIVE HIGH)
//   bit 3 ($2403): Left Rotate (ACTIVE HIGH)
//   bit 4 ($2404): unused
//   bit 5 ($2405): Coin 1 (ACTIVE HIGH)
//   bit 6 ($2406): Coin 2 (ACTIVE HIGH)
//   bit 7 ($2407): Coin 3 (ACTIVE HIGH)
//
// DSW1 ($2800-$2803): TTL153 dual 4:1 mux (same mechanism as Asteroids)
//
// THRUST ($2C00): Analog thrust lever (0x00-0xFF)
// ==========================================================================
uint8_t read6502(uint16_t address)
{
    #ifdef DEBUG_AUDIO
        audio ^= address >> 8;
        IO8(0x4011) = audio;
    #endif

	// 15-bit address bus
	address &= 0x7FFF;

	// RAM: $0000-$00FF (256 bytes), mirrored across $0000-$01FF
	// MAME: installed at 0x0000-0x00ff, 8-bit mirror mask 0x1f00
	// We use a simpler approach: anything < $2000 reads RAM[addr & 0xFF]
	if (address < 0x2000)
		return RAM_BASE[address & 0xFF];

	// IN0: $2000 — full byte read
	if (address == 0x2000) {
		uint8_t val = 0;
		// Bit 0: 3KHz clock (MAME: clock_r) — toggle on each read
		clock_3khz ^= 1;
		val |= clock_3khz;
		// Bit 1: self-test (ACTIVE LOW: 1 = normal operation)
		val |= 0x02;
		// Bit 2: tilt — not active
		// Bit 3: DVG halt status (1 = running, 0 = halted/done)
		if (!dvg.halted) val |= 0x08;
		// Bit 4: Select Game → NES Start
		if (cached_pad & lfStart) val |= 0x10;
		// Bit 5: Abort → NES Select
		if (cached_pad & lfSelect) val |= 0x20;
		return val;
	}

	// IN1: $2400-$2407 — bit-addressed player controls
	if (address >= 0x2400 && address <= 0x2407) {
		uint8_t bit_sel = address & 0x07;
		switch (bit_sel) {
		case 0: { // 1P Start
			uint8_t pressed = 0;
			// Auto-start during boot attract
			if (boot_frame_count >= BOOT_START_FRAME &&
			    boot_frame_count < BOOT_START_FRAME + BOOT_START_DURATION)
				pressed = 1;
			return pressed ? 0x80 : 0x7F;
		}
		case 1: // unused
			return 0x7F;
		case 2: // Right Rotate → NES D-pad Right
			return (cached_pad & lfR) ? 0x80 : 0x7F;
		case 3: // Left Rotate → NES D-pad Left
			return (cached_pad & lfL) ? 0x80 : 0x7F;
		case 4: // unused
			return 0x7F;
		case 5: { // Coin 1
			uint8_t pressed = 0;
			if (boot_frame_count >= BOOT_COIN_FRAME &&
			    boot_frame_count < BOOT_COIN_FRAME + BOOT_COIN_DURATION)
				pressed = 1;
			return pressed ? 0x80 : 0x7F;
		}
		case 6: // Coin 2 — not inserted
			return 0x7F;
		case 7: // Coin 3 — not inserted
			return 0x7F;
		}
	}

	// DSW1: $2800-$2803 — DIP switches via TTL153 dual 4:1 multiplexer
	// Lunar Lander DIP layout:
	//   bit 7,6: Right coin (00 = 1 credit)
	//   bit 5,4: Coinage (00 = free play)
	//   bit 3,2: Fuel per coin (00 = 450 units)
	//   bit 1,0: Language / unused (00 = English)
	if (address >= 0x2800 && address <= 0x2803) {
		uint8_t dsw = 0x00;  // free play, 450 fuel, English

		uint8_t offset = address & 0x03;
		uint8_t bit_a, bit_b;
		switch (offset) {
		case 0: bit_a = (dsw >> 7) & 1; bit_b = (dsw >> 6) & 1; break;
		case 1: bit_a = (dsw >> 5) & 1; bit_b = (dsw >> 4) & 1; break;
		case 2: bit_a = (dsw >> 3) & 1; bit_b = (dsw >> 2) & 1; break;
		default: bit_a = (dsw >> 1) & 1; bit_b = (dsw >> 0) & 1; break;
		}
		return (bit_a << 7) | (bit_b << 6);
	}

	// Thrust lever: $2C00 — return analog thrust level
	if (address == 0x2C00)
		return thrust_level;

	// Vector RAM: $4000-$47FF (2KB)
	if (address >= 0x4000 && address < 0x4800)
		return vector_ram[(address - 0x4000) & VECTOR_RAM_MASK];

	// Vector ROM: $4800-$5FFF (6KB, 3 × 2KB chips)
	if (address >= 0x4800 && address < 0x6000) {
		uint8_t saved_bank = mapper_prg_bank;
		bankswitch_prg(BANK_PLATFORM_ROM);
		uint8_t val = rom_llander_vec[address - 0x4800];
		bankswitch_prg(saved_bank);
		return val;
	}

	// Program ROM: $6000-$7FFF (8KB)
	if (address >= 0x6000) {
		uint8_t saved_bank = mapper_prg_bank;
		bankswitch_prg(BANK_PLATFORM_ROM);
		uint8_t val = rom_llander[address - 0x6000];
		bankswitch_prg(saved_bank);
		return val;
	}

	return 0x00;  // unmapped
}


// ==========================================================================
// write6502 — Lunar Lander memory bus write handler
// ==========================================================================
void write6502(uint16_t address, uint8_t value)
{
	// 15-bit address bus
	address &= 0x7FFF;

	// RAM: $0000-$00FF (mirrored across $0000-$1FFF)
	if (address < 0x2000) {
		RAM_BASE[address & 0xFF] = value;
		return;
	}

	// IN0/IN1/DSW1/Thrust ranges ($2000-$2FFF) are read-only — ignore writes
	if (address >= 0x2000 && address < 0x3000)
		return;

	// DVG Go: $3000 write — trigger DVG execution synchronously
	if (address == 0x3000) {
		uint8_t saved_bank = mapper_prg_bank;
		bankswitch_prg(BANK_RENDER);
		dvg_execute();
		bankswitch_prg(saved_bank);
		dvg_triggered = 1;
		dvg_ever_started = 1;
		return;
	}

	// LED output latch: $3200 write (no RAMSEL — just store)
	if (address == 0x3200) {
		led_latch = value;
		return;
	}

	// Watchdog: $3400
	if (address == 0x3400)
		return;

	// NMI ack/clear: $3C00 — acknowledge current NMI
	if (address == 0x3C00)
		return;

	// NMI enable: $3E00 — hardware latch that gates periodic NMI
	if (address == 0x3E00) {
		nmi_enabled = 1;
		return;
	}

	// Catch-all for $3000-$3FFF
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
// flash_format for Lunar Lander
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
// main() — Lunar Lander emulator entry point
// ==========================================================================
int main(void)
{
	lnSync(1);

	// Fix PPU increment mode before palette push
	lnPPUCTRL &= ~0x08;
	IO8(0x2000) = lnPPUCTRL;

	// Initial palette: black BG, white dots
	{
		static const uint8_t default_pal[] = {
			0x0F,0x20,0x10,0x00,  // BG palette 0: black, white, gray, dark
			0x0F,0x20,0x10,0x00,  // BG palette 1
			0x0F,0x20,0x10,0x00,  // BG palette 2
			0x0F,0x20,0x10,0x00,  // BG palette 3
			0x0F,0x20,0x10,0x00,  // Sprite palette 0: black + white dots
			0x0F,0x20,0x10,0x00,  // Sprite palette 1
			0x0F,0x20,0x10,0x00,  // Sprite palette 2
			0x0F,0x20,0x10,0x00,  // Sprite palette 3
		};
		lnPush(0x3F00, 32, default_pal);
	}

	// Generate CHR tiles (dots + font)
	{
		uint8_t saved_bank = mapper_prg_bank;
		bankswitch_prg(BANK_SA_CODE);
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

	// Zero guest RAM before reset (256 bytes)
	for (uint16_t i = 0; i < 0x0100; i++) RAM_BASE[i] = 0;
	// Pre-seed NMI handler sentinel so early NMIs don't trap
	RAM_BASE[0x00] = 0x85;

	// Zero vector RAM so DVG doesn't process garbage
	for (uint16_t i = 0; i < VECTOR_RAM_SIZE; i++) vector_ram[i] = 0;

	reset6502();

	// Disable APU frame IRQ
	IO8(0x4017) = 0x40;

	// PPUCTRL: BG from pattern table 0, sprites from pattern table 1
	lnPPUCTRL |= 0x08;
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
	dvg_ever_started = 0;
	nmi_enabled = 0;
	dot_count = 0;
	mux_frame = 0;
	nmi_pending = 0;
	thrust_level = 0;
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

		if (!nmi_active)
			guest_steps_since_nmi++;

		// Detect RTI — NMI handler has returned
		if (nmi_active && sp == nmi_sp_guard)
			nmi_active = 0;

		// Lunar Lander NMI delivery — same approach as Asteroids:
		// deliver 1 NMI per NES VBlank, gated by guest_steps_since_nmi
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

				// NMI fires periodically when enabled by $3E00 write
				// (MAME: llander_interrupt at CLOCK_3KHZ/12, gated on m_nmi_enabled)
				if (nmi_enabled)
					nmi_pending = 1;
			}

			if (nmi_pending > 0 && guest_steps_since_nmi >= NMI_MIN_GUEST_STEPS && !nmi_active)
			{
				TRACE_MARK(0x10 | nmi_pending);
				nmi_pending--;
				guest_steps_since_nmi = 0;

				// Prevent NMI overrun watchdog — same pattern as Asteroids.
				// Clear FrameCounter ($73) before NMI so the handler's
				// prescaled INC $73 can't accumulate past the threshold.
				RAM_BASE[0x73] = 0;

				nmi_sp_guard = sp;
				nmi_active = 1;
				nmi6502();

				// Force $73 = 1 to unblock the main loop drain poll
				// at $652D (LSR $73 / BCC $652D).
				RAM_BASE[0x73] = 1;

				TRACE_MARK(0x1F);
			}
		}
#else
		if (clockticks6502 > frame_time)
		{
			frame_time += FRAME_LENGTH;
			if (nmi_enabled)
				interrupt_condition |= FLAG_LLANDER_NMI;
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
		// Fire NMI
		if ((interrupt_condition & FLAG_LLANDER_NMI) && !nmi_active)
		{
			interrupt_condition &= ~FLAG_LLANDER_NMI;
			nmi_sp_guard = sp;
			nmi_active = 1;
			nmi6502();
		}
	}
}
