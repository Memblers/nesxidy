// ==========================================================================
// millipede.c — Atari Millipede arcade platform driver for DynaMoS
//
// Emulates the Millipede arcade hardware (6502 @ 1.512 MHz) running on
// NES hardware via the DynaMoS dynamic binary translator.
//
// Video:  240×256 rotated monitor → displayed sideways on NES (30×32 tiles)
// Audio:  POKEY stubs (no sound emulation yet)
// Input:  NES gamepad mapped to trackball substitutes + buttons
// ==========================================================================

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "lazynes.h"
#include "config.h"
#include "millipede.h"
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
// ROM data externs — defined via incbin in dynamos-asm.s (bank 23 + bank 24)
// Section attributes for extern references tell vbcc which bank to bankswitch to.
// ==========================================================================
#pragma section bank23
	extern const unsigned char rom_millipede[];     // 16KB program ROM
#pragma section default
// CHR/color PROM in bank24 — accessed via bankswitch_prg(BANK_MILLIPEDE_CHR)
	extern const unsigned char chr_millipede[];     // 4KB character/sprite ROM
	extern const unsigned char color_prom_millipede[];  // 256-byte color PROM


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
// Millipede-specific state
// ==========================================================================
__zpage uint8_t interrupt_condition;
__zpage uint8_t screen_ram_updated = 0;
__zpage uint8_t character_ram_updated = 0; // unused — required by dirty_flag_char ASM symbol

// 64-byte attribute table for NES nametable palette assignment.
// Built from VRAM byte bits 7-6 (Millipede color per tile).
static uint8_t attr_table[64];

// Shadow buffer and VRAM update list (from dynamos-asm.s)
extern uint8_t screen_shadow[];
extern uint8_t vram_update_list[];
__zpage extern uint8_t vram_list_pos;
extern uint8_t screen_diff_build_list(void);

// POKEY random number LFSR (17-bit polynomial counter)
// Polynomial: x^17 + x^3 + 1, Galois LFSR implementation.
// Period: 131071 (2^17 - 1), all non-zero states.
static uint32_t pokey1_lfsr = 0x1FFFF;  // seed with all-ones
static uint32_t pokey2_lfsr = 0x12345;  // seed differently

// Palette RAM shadow (32 entries, Millipede writes at $2480-$249F)
static uint8_t palette_ram[32];
__zpage uint8_t palette_dirty = 1;  // force initial palette push

// Sprite RAM shadow ($13C0-$13FF, 64 bytes)
// Layout: [0x00-0x0F] tile/flip, [0x10-0x1F] Y pos,
//         [0x20-0x2F] X pos,     [0x30-0x3F] color
static uint8_t sprite_ram[64];

// Shadow buffers for incremental VRAM diff (avoids full push every frame)
static uint8_t attr_shadow[64];   // previous frame's NES attribute table
static uint8_t pal_shadow[32];    // previous frame's NES palette
static uint8_t nes_pal[32];       // current NES palette (built from palette_ram)

// IRQ acknowledge flag
__zpage uint8_t irq_acked = 0;

// Trackball simulation counters
__zpage uint8_t trackball_x = 0;
__zpage uint8_t trackball_y = 0;
__zpage uint8_t trackball_select = 0;
__zpage uint8_t tben = 0;  // trackball enable (output latch $2505)

// Frame timing
__zpage uint32_t frame_time;
__zpage uint8_t last_nmi_frame = 0;

// Forward declarations
void render_video(void);

// Note: lnPush (declared in lazynes.h with __reg attributes) is called
// directly.  Its ASL $20 may occasionally shift sp_lo when the NMI
// fires, but this matches the working Exidy approach.


// ==========================================================================
// Millipede palette byte → NES color index lookup table
// Generated from Millipede RGB decode + NES 2C02 palette distance matching.
// Millipede palette format (inverted bits):
//   Bits 1-0: Red   (2b) weights 0xA8, 0x4F
//   Bits 4-2: Green (3b) weights 0x97, 0x47, 0x21
//   Bits 7-5: Blue  (3b) weights 0x97, 0x47, 0x21
// ==========================================================================
static const uint8_t milliped_to_nes_color[256] = {
    0x20, 0x3C, 0x2C, 0x2C, 0x20, 0x31, 0x2C, 0x2C, 0x34, 0x31, 0x21, 0x2C, 0x34, 0x32, 0x21, 0x21,
    0x24, 0x23, 0x22, 0x11, 0x24, 0x23, 0x12, 0x12, 0x24, 0x23, 0x13, 0x12, 0x24, 0x13, 0x13, 0x12,
    0x20, 0x3C, 0x2C, 0x2C, 0x20, 0x3C, 0x2C, 0x2C, 0x35, 0x31, 0x2C, 0x2C, 0x35, 0x32, 0x21, 0x2C,
    0x24, 0x23, 0x22, 0x11, 0x24, 0x23, 0x12, 0x11, 0x24, 0x14, 0x13, 0x11, 0x24, 0x14, 0x13, 0x12,
    0x20, 0x3B, 0x3B, 0x2C, 0x36, 0x3B, 0x2C, 0x2C, 0x36, 0x3D, 0x2C, 0x2C, 0x36, 0x3D, 0x2C, 0x2C,
    0x25, 0x23, 0x22, 0x11, 0x25, 0x23, 0x13, 0x11, 0x25, 0x14, 0x13, 0x02, 0x25, 0x14, 0x13, 0x02,
    0x37, 0x3A, 0x2B, 0x2B, 0x37, 0x3A, 0x2B, 0x2B, 0x37, 0x3D, 0x2B, 0x2B, 0x36, 0x10, 0x2C, 0x1C,
    0x25, 0x10, 0x00, 0x1C, 0x25, 0x14, 0x00, 0x1C, 0x25, 0x14, 0x03, 0x02, 0x25, 0x14, 0x03, 0x02,
    0x38, 0x39, 0x2B, 0x2B, 0x38, 0x39, 0x2B, 0x2B, 0x37, 0x39, 0x2B, 0x2B, 0x26, 0x10, 0x2B, 0x1C,
    0x26, 0x26, 0x00, 0x1C, 0x26, 0x15, 0x00, 0x1C, 0x26, 0x15, 0x04, 0x01, 0x15, 0x15, 0x04, 0x01,
    0x38, 0x39, 0x2A, 0x2B, 0x38, 0x39, 0x2A, 0x2B, 0x38, 0x39, 0x2A, 0x2B, 0x27, 0x27, 0x00, 0x1B,
    0x26, 0x27, 0x00, 0x1B, 0x26, 0x16, 0x00, 0x0C, 0x26, 0x15, 0x2D, 0x0C, 0x15, 0x15, 0x05, 0x0C,
    0x38, 0x29, 0x2A, 0x2A, 0x27, 0x28, 0x2A, 0x2A, 0x27, 0x28, 0x2A, 0x1B, 0x27, 0x28, 0x2A, 0x1B,
    0x27, 0x27, 0x18, 0x1B, 0x27, 0x16, 0x2D, 0x0A, 0x16, 0x16, 0x05, 0x0C, 0x16, 0x16, 0x05, 0x1D,
    0x28, 0x29, 0x2A, 0x2A, 0x27, 0x28, 0x2A, 0x2A, 0x27, 0x28, 0x29, 0x1A, 0x27, 0x28, 0x19, 0x1A,
    0x27, 0x27, 0x18, 0x1A, 0x27, 0x17, 0x18, 0x0A, 0x16, 0x16, 0x07, 0x09, 0x16, 0x16, 0x06, 0x1D,
};


// ==========================================================================
// POKEY LFSR — 17-bit polynomial counter (x^17 + x^3 + 1)
// ==========================================================================
static uint8_t pokey_random(uint32_t *lfsr)
{
	// Advance the LFSR by 1 step
	uint8_t bit = ((*lfsr) ^ ((*lfsr) >> 14)) & 1;
	*lfsr = ((*lfsr) >> 1) | ((uint32_t)bit << 16);
	return (uint8_t)(*lfsr & 0xFF);
}


// ==========================================================================
// CHR conversion — will be done at boot with bankswitch approach
// ==========================================================================

// flash_format in BANK_INIT_CODE
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
		if (bank == BANK_PLATFORM_ROM) continue;  // program ROM
		if (bank == BANK_MILLIPEDE_CHR) continue;  // CHR + color PROM
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

// Bit-reverse a byte: Millipede char ROM is LSB-first (pixel 0 = bit 0),
// NES CHR-RAM is MSB-first (pixel 0 = bit 7).  Nibble LUT approach.
static const uint8_t nibble_rev[16] = {
	0x0, 0x8, 0x4, 0xC, 0x2, 0xA, 0x6, 0xE,
	0x1, 0x9, 0x5, 0xD, 0x3, 0xB, 0x7, 0xF
};
static uint8_t reverse_bits(uint8_t b)
{
	return (nibble_rev[b & 0x0F] << 4) | nibble_rev[b >> 4];
}

void convert_chr(uint8_t *source)
{
	(void)source;
	// MAME milliped_get_tile_info:
	//   tile = (data & 0x3F) + 0x40 + (bank * 0x80)
	//   bank = (data >> 6) & 1  (with gfx_bank=0)
	//   bit 6=0: ROM tiles 64-127,  bit 6=1: ROM tiles 192-255
	//   No per-tile flip in upright mode.
	// We load CHR-RAM so NES tile index = raw VRAM byte:
	//   NES 0-63:    ROM 64-127   (VRAM bits 7:6 = 00)
	//   NES 64-127:  ROM 192-255  (VRAM bits 7:6 = 01)
	//   NES 128-191: ROM 64-127   (VRAM bits 7:6 = 10, mirror)
	//   NES 192-255: ROM 192-255  (VRAM bits 7:6 = 11, mirror)
	IO8(0x2006) = 0x00;
	IO8(0x2006) = 0x00;
	bankswitch_prg(BANK_MILLIPEDE_CHR);

	// --- Pattern table 0: Background tiles ---
	// NES tiles 0-63 ← ROM tiles 64-127
	for (uint16_t t = 0; t < 64; t++)
	{
		uint16_t base = (64 + t) << 3;
		for (uint8_t j = 0; j < 8; j++)
			IO8(0x2007) = chr_millipede[base + j];
		for (uint8_t j = 0; j < 8; j++)
			IO8(0x2007) = chr_millipede[2048 + base + j];
	}
	// NES tiles 64-127 ← ROM tiles 192-255
	for (uint16_t t = 0; t < 64; t++)
	{
		uint16_t base = (192 + t) << 3;
		for (uint8_t j = 0; j < 8; j++)
			IO8(0x2007) = chr_millipede[base + j];
		for (uint8_t j = 0; j < 8; j++)
			IO8(0x2007) = chr_millipede[2048 + base + j];
	}
	// NES tiles 128-191 ← ROM tiles 64-127 (mirror for bit 7 set)
	for (uint16_t t = 0; t < 64; t++)
	{
		uint16_t base = (64 + t) << 3;
		for (uint8_t j = 0; j < 8; j++)
			IO8(0x2007) = chr_millipede[base + j];
		for (uint8_t j = 0; j < 8; j++)
			IO8(0x2007) = chr_millipede[2048 + base + j];
	}
	// NES tiles 192-255 ← ROM tiles 192-255 (mirror for bit 7 set)
	for (uint16_t t = 0; t < 64; t++)
	{
		uint16_t base = (192 + t) << 3;
		for (uint8_t j = 0; j < 8; j++)
			IO8(0x2007) = chr_millipede[base + j];
		for (uint8_t j = 0; j < 8; j++)
			IO8(0x2007) = chr_millipede[2048 + base + j];
	}
	// --- Pattern table 1: Sprite tiles (all 256 in order) ---
	for (uint16_t tile = 0; tile < 256; tile++)
	{
		uint16_t base = tile << 3;
		for (uint8_t j = 0; j < 8; j++)
			IO8(0x2007) = chr_millipede[base + j];
		for (uint8_t j = 0; j < 8; j++)
			IO8(0x2007) = chr_millipede[2048 + base + j];
	}
	bankswitch_prg(0);
}

void flash_format(void)
{
	uint8_t saved_bank = mapper_prg_bank;
	bankswitch_prg(BANK_INIT_CODE);
	flash_format_b2();
	bankswitch_prg(saved_bank);
}


// ==========================================================================
// Input handling — map NES gamepad to Millipede controls
// ==========================================================================
static uint8_t cached_in0 = 0;     // IN0: fire, joystick/trackball
static uint8_t cached_in2 = 0;     // IN2: coin, start

void nes_gamepad_refresh(void)
{
	uint8_t joypad = lnGetPad(1);
	uint8_t in0 = 0;
	uint8_t in2 = 0;

	// Trackball simulation: D-pad moves the trackball counters
	// Millipede reads trackball as signed delta per frame
	if (joypad & lfL) trackball_x -= 4;
	if (joypad & lfR) trackball_x += 4;
	if (joypad & lfU) trackball_y -= 4;
	if (joypad & lfD) trackball_y += 4;

	// Fire button — handled in IN0 ($2000) bit 4 (active low)
	if (joypad & lfA) in0 |= 0x10;

	// Joystick for IN2 ($2010) bits 0-3 (active low)
	if (joypad & lfR) in2 |= 0x01;
	if (joypad & lfL) in2 |= 0x02;
	if (joypad & lfD) in2 |= 0x04;
	if (joypad & lfU) in2 |= 0x08;

	// Coin on Select
	if (joypad & lfSelect) in2 |= 0x10;

	// Start — handled in IN0 ($2000) bit 5 (active low)
	if (joypad & lfStart) in0 |= 0x20;

	cached_in0 = in0;
	cached_in2 = in2;
}

uint8_t nes_gamepad(void)
{
	return cached_in0;
}


// ==========================================================================
// Palette update — convert Millipede palette RAM to NES PPU palette
// ==========================================================================
#pragma section bank20

// Build NES attribute table from Millipede VRAM palette bits.
// Each VRAM byte bits 7-6 = 2-bit palette. NES attribute table
// packs 4 palettes per byte for each 4x4 tile group.
static void build_attr_table(void)
{
	uint16_t row_base = 0;
	uint8_t idx = 0;
	for (uint8_t ay = 0; ay < 8; ay++)
	{
		uint16_t col_base = row_base;
		for (uint8_t ax = 0; ax < 8; ax++)
		{
			uint8_t tl = 0, tr = 0, bl = 0, br = 0;
			if (col_base < 960)
				tl = (SCREEN_RAM_BASE[col_base] >> 6) & 3;
			if (col_base + 2 < 960)
				tr = (SCREEN_RAM_BASE[col_base + 2] >> 6) & 3;
			if (col_base + 64 < 960)
				bl = (SCREEN_RAM_BASE[col_base + 64] >> 6) & 3;
			if (col_base + 66 < 960)
				br = (SCREEN_RAM_BASE[col_base + 66] >> 6) & 3;
			attr_table[idx] = tl | (tr << 2) | (bl << 4) | (br << 6);
			idx++;
			col_base += 4;
		}
		row_base += 128;
	}
}

// Build the NES palette from Millipede palette RAM into the file-scope
// nes_pal[] buffer.  Does NOT push to PPU — caller decides how to deliver
// the bytes (lnPush in blank mode, or appended to an lnList).
static void build_nes_palette(void)
{
	for (uint8_t i = 0; i < 32; i++)
		nes_pal[i] = milliped_to_nes_color[palette_ram[i]];
	// Force black background (first entry of each 4-color sub-palette)
	nes_pal[0] = 0x0F;
	nes_pal[4] = 0x0F;
	nes_pal[8] = 0x0F;
	nes_pal[12] = 0x0F;
	nes_pal[16] = 0x0F;
	nes_pal[20] = 0x0F;
	nes_pal[24] = 0x0F;
	nes_pal[28] = 0x0F;
}

#pragma section default


// ==========================================================================
// read6502 — Millipede memory bus read handler
// ==========================================================================
uint8_t read6502(uint16_t address)
{
    #ifdef DEBUG_AUDIO
        audio ^= address >> 8;
        IO8(0x4011) = audio;
    #endif
	// RAM: $0000-$03FF
	if (address < 0x0400)
		return RAM_BASE[address];

	// POKEY 1: $0400-$040F
	if (address >= 0x0400 && address <= 0x040F)
	{
		uint8_t reg = address & 0x0F;
		if (reg == 0x08)  // ALLPOT — DIP switch bank 1
			return 0x14;  // DSW1: easy, 3 lives, 15000 bonus, easy spider, select on
		if (reg == 0x0A)  // RANDOM
			return pokey_random(&pokey1_lfsr);
		if (reg == 0x0F)  // SKSTAT
			return 0xFF;
		return 0x00;
	}

	// POKEY 2: $0800-$080F
	if (address >= 0x0800 && address <= 0x080F)
	{
		uint8_t reg = address & 0x0F;
		if (reg == 0x08)  // ALLPOT — DIP switch bank 2
			return 0x02;  // DSW2: 1 coin/1 credit, right*1, left*1, no bonus
		if (reg == 0x0A)  // RANDOM
			return pokey_random(&pokey2_lfsr);
		if (reg == 0x0F)  // SKSTAT
			return 0xFF;
		return 0x00;
	}

	// Video RAM: $1000-$13BF
	if (address >= 0x1000 && address <= 0x13BF)
		return SCREEN_RAM_BASE[address - 0x1000];

	// Sprite RAM: $13C0-$13FF
	if (address >= 0x13C0 && address <= 0x13FF)
		return sprite_ram[address - 0x13C0];

	// IN0: $2000 — player 1 inputs + VBLANK
	// Bit 7: trackball horiz direction sign
	// Bit 6: VBLANK (active high, 1 = in vblank)
	// Bit 5: Start 1 (active low, 1 = not pressed)
	// Bit 4: Fire 1 (active low, 1 = not pressed)
	// Bits 3-0: trackball horiz count / DIP switches (P8 bottom)
	if (address == 0x2000)
	{
		uint8_t val = 0x30;  // bits 5,4 high = start/fire not pressed
		// Clear bit 4 (fire) or bit 5 (start) when pressed (active low)
		if (cached_in0 & 0x10) val &= ~0x10;  // fire pressed
		if (cached_in0 & 0x20) val &= ~0x20;  // start pressed
		// VBLANK flag at bit 6
		if (interrupt_condition & FLAG_MILLIPEDE_IRQ)
			val |= 0x40;
		// Bits 0-3: trackball X (when TBEN) or P8 DIP (when !TBEN)
		if (tben)
			val |= (trackball_x & 0x0F);
		else
			val |= 0x04;  // P8 DIP: English, bonus "0 1x"
		return val;
	}

	// IN1: $2001 — player 2 inputs + trackball Y
	// Bit 7: trackball vert direction sign
	// Bit 5: Start 2 (active low)
	// Bit 4: Fire 2 (active low)
	// Bits 3-0: trackball vert count / DIP switches (P8 top)
	if (address == 0x2001)
	{
		uint8_t val = 0x30;  // P2 fire/start not pressed
		if (tben)
			val |= (trackball_y & 0x0F);
		// else: P8 DIP bits 4-7 = 0x00 (credit min=1, 1 counter)
		return val;
	}

	// IN2: $2010 — joystick + coins
	// Bits 3-0: P1 joystick (active low: right, left, down, up)
	// Bit 4: Tilt (active low)
	// Bit 5: Coin 1 (active low)
	// Bit 6: Coin 2 (active low)
	// Bit 7: Service coin (active low)
	if (address == 0x2010)
	{
		uint8_t val = 0xFF;  // all inactive (active low)
		// Map NES gamepad to joystick (active low)
		if (cached_in2 & 0x08) val &= ~0x08;  // up
		if (cached_in2 & 0x04) val &= ~0x04;  // down
		if (cached_in2 & 0x02) val &= ~0x02;  // left
		if (cached_in2 & 0x01) val &= ~0x01;  // right
		// Coin on select
		if (cached_in2 & 0x10) val &= ~0x20;  // coin 1 at bit 5
		return val;
	}

	// IN3: $2011 — self-test + cabinet
	// Bit 7: Self-test (active low, 0 = On)
	// Bit 5: Cabinet (1 = Upright, 0 = Cocktail)
	// Bits 3-0: P2 joystick
	if (address == 0x2011)
		return 0xFF;  // bit 7 low = self-test ON, bit 5 high = upright

	// EAROM read: $2030
	if (address == 0x2030)
		return 0x00;  // EAROM read data (not implemented)

	// Program ROM: $4000-$7FFF (and mirrors at $8000-$FFFF)
	if (address >= 0x4000)
	{
		uint8_t saved_bank = mapper_prg_bank;
		bankswitch_prg(BANK_PLATFORM_ROM);
		uint8_t temp = ROM_NAME[(address & 0x3FFF)];
		bankswitch_prg(saved_bank);
		return temp;
	}

	return 0x00;  // unmapped
}


// ==========================================================================
// write6502 — Millipede memory bus write handler
// ==========================================================================
void write6502(uint16_t address, uint8_t value)
{
	// RAM: $0000-$03FF
	if (address < 0x0400)
	{
		RAM_BASE[address] = value;
		return;
	}

	// POKEY 1: $0400-$040F (sound registers — stub)
	if (address >= 0x0400 && address <= 0x040F)
		return;  // ignore sound writes for now

	// POKEY 2: $0800-$080F (sound registers — stub)
	if (address >= 0x0800 && address <= 0x080F)
		return;  // ignore sound writes for now

	// Video RAM: $1000-$13BF
	if (address >= 0x1000 && address <= 0x13BF)
	{
		SCREEN_RAM_BASE[address - 0x1000] = value;
		screen_ram_updated = 1;
		return;
	}

	// Sprite RAM: $13C0-$13FF
	if (address >= 0x13C0 && address <= 0x13FF)
	{
		sprite_ram[address - 0x13C0] = value;
		return;
	}

	// $2000 write — In MAME this is not a write register; ignore
	// (reads go to IN0 in read6502)
	if (address == 0x2000)
		return;

	// Trackball select: $2001 write
	if (address == 0x2001)
	{
		trackball_select = value;
		return;
	}

	// Palette RAM: $2480-$249F (32 entries)
	if (address >= 0x2480 && address <= 0x249F)
	{
		palette_ram[address - 0x2480] = value;
		palette_dirty = 1;
		return;
	}

	// Output latches: $2500-$2507
	// $2505 = TBEN (trackball enable), $2506 = flip screen, etc.
	// MAME: ls259_device::write_d7 — uses bit 7 of data byte
	if (address >= 0x2500 && address <= 0x2507)
	{
		if ((address & 0x07) == 5)
			tben = (value >> 7) & 1;  // TBEN: D7=1→trackball, D7=0→DIP
		return;
	}

	// IRQ acknowledge: $2600 write
	if (address == 0x2600)
	{
		irq_acked = 1;
		interrupt_condition &= ~FLAG_MILLIPEDE_IRQ;
		return;
	}

	// Watchdog: $2680
	if (address == 0x2680)
		return;  // ignore watchdog

	// ROM area writes are ignored
}


// ==========================================================================
// ln_fire_and_forget — non-blocking render sync (same as exidy.c)
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
// render_video — per-frame video update
// ==========================================================================
#pragma section bank20

void render_video_b2(void)
{
	// ---- Decide what needs updating this frame ----
	uint8_t do_palette = palette_dirty;
	if (do_palette) {
		palette_dirty = 0;
		build_nes_palette();           // fills file-scope nes_pal[32]
	}

	uint8_t result = 0;
	if (screen_ram_updated) {
		screen_ram_updated = 0;
		build_attr_table();
		result = screen_diff_build_list();
	}

	// ---- Route to full-push or incremental lnList ----
	// CRITICAL: lnPush requires blank mode (lnSync(1) first).
	// lnList is safe with the screen visible.  Never mix them.
	uint8_t need_full = (result == 0xFF);
	uint8_t pos = 0;

	if (!need_full && (result == 1 || do_palette))
	{
		// --- Incremental path: everything goes through lnList ---
		pos = (result == 1) ? vram_list_pos : 0;

		// Append changed attribute bytes (single-tile lnList entries)
		if (result == 1) {
			for (uint8_t i = 0; i < 64; i++) {
				if (attr_table[i] != attr_shadow[i]) {
					attr_shadow[i] = attr_table[i];
					if (pos >= 240) { need_full = 1; break; }
					vram_update_list[pos]     = 0x23;
					vram_update_list[pos + 1] = (uint8_t)(0xC0 + i);
					vram_update_list[pos + 2] = attr_table[i];
					pos += 3;
				}
			}
		}

		// Append changed palette bytes (single-tile lnList entries)
		if (!need_full && do_palette) {
			for (uint8_t i = 0; i < 32; i++) {
				if (nes_pal[i] != pal_shadow[i]) {
					pal_shadow[i] = nes_pal[i];
					if (pos >= 240) { need_full = 1; break; }
					vram_update_list[pos]     = 0x3F;
					vram_update_list[pos + 1] = i;
					vram_update_list[pos + 2] = nes_pal[i];
					pos += 3;
				}
			}
		}
	}

	if (need_full)
	{
		// Too many changes — blank screen and push everything via lnPush
		lnSync(1);
		lnPush(0x2000, 0,    SCREEN_RAM_BASE);
		lnPush(0x2100, 0,    SCREEN_RAM_BASE + 0x100);
		lnPush(0x2200, 0,    SCREEN_RAM_BASE + 0x200);
		lnPush(0x2300, 0xC0, SCREEN_RAM_BASE + 0x300);
		lnPush(0x23C0, 64,   attr_table);
		if (do_palette)
			lnPush(0x3F00, 32, nes_pal);
		// Sync shadow buffers with what we just pushed
		for (uint8_t i = 0; i < 64; i++) attr_shadow[i] = attr_table[i];
		if (do_palette)
			for (uint8_t i = 0; i < 32; i++) pal_shadow[i] = nes_pal[i];
	}
	else if (pos > 0)
	{
		// Incremental update — deliver via lnList (screen stays visible)
		vram_update_list[pos] = 0xFF;   // lfEnd terminator
		lnList(vram_update_list);
	}

	// --- Sprites ---
	// Millipede has 16 motion objects, 8×16 pixels each.
	// Sprite RAM layout at $13C0-$13FF:
	//   $13C0+i ($00-$0F): tile/flip
	//     Bits 1-5: tile number (low 5 bits)
	//     Bit 0:    tile bit 6 (bank select for upper 64 codes)
	//     Bit 6:    unused (not flip — Millipede uses VIDROT for screen flip)
	//     Bit 7:    flip Y
	//   $13D0+i ($10-$1F): Y position
	//   $13E0+i ($20-$2F): X position
	//   $13F0+i ($30-$3F): color
	//
	// MAME code: ((byte & 0x3E) >> 1) | ((byte & 0x01) << 6)
	// This gives a sprite code 0-127 for 8×16 sprites.
	// Our NES PT1 has 8×8 tiles loaded sequentially, so each MAME
	// 8×16 code maps to two NES tiles: code*2 (top) and code*2+1 (bottom).
	for (uint8_t i = 0; i < 16; i++)
	{
		uint8_t tile_byte = sprite_ram[i];
		uint8_t y_pos = sprite_ram[0x10 + i];
		uint8_t x_pos = sprite_ram[0x20 + i];
		uint8_t color  = sprite_ram[0x30 + i];

		// MAME sprite code (8×16 index)
		uint8_t code = ((tile_byte & 0x3E) >> 1) | ((tile_byte & 0x01) << 6);

		// Convert to NES 8×8 tile pair
		uint8_t tile_top = code << 1;       // code * 2
		uint8_t tile_bot = tile_top + 1;    // code * 2 + 1

		// Flip flags — Millipede: no per-sprite flipX (VIDROT only)
		uint8_t flip_y = (tile_byte & 0x80) ? 1 : 0;

		// If flipY, swap top/bottom halves
		if (flip_y)
		{
			uint8_t tmp = tile_top;
			tile_top = tile_bot;
			tile_bot = tmp;
		}

		// Y is inverted on Millipede (0 = bottom of screen)
		uint8_t nes_y = 240 - y_pos;
		uint8_t nes_x = x_pos;

		// NES OAM attribute byte:
		// Bit 6: flip horizontal, Bit 7: flip vertical
		// Bits 0-1: palette (use sprite color lower bits)
		uint8_t attr = (color & 0x03);
		if (flip_y) attr |= 0x80;

		// Top half
		uint8_t spr_meta_top[5] = {
			0, 0, tile_top, attr, 128
		};
		lnAddSpr(spr_meta_top, nes_x, nes_y);

		// Bottom half (8 pixels down)
		if (nes_y < 232)  // don't overflow off screen
		{
			uint8_t spr_meta_bot[5] = {
				0, 0, tile_bot, attr, 128
			};
			lnAddSpr(spr_meta_bot, nes_x, nes_y + 8);
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
// main() — Millipede emulator entry point
// ==========================================================================
int main(void)
{
	lnSync(1);

	// Fix PPU increment mode before palette push
	lnPPUCTRL &= ~0x08;
	IO8(0x2000) = lnPPUCTRL;

	// Initial default palette (will be overwritten by game's palette writes)
	{
		static const uint8_t default_pal[] = {
			0x0F,0x20,0x10,0x00,  // BG palette 0: black, white, gray, dark
			0x0F,0x16,0x26,0x36,  // BG palette 1: reds
			0x0F,0x19,0x29,0x39,  // BG palette 2: greens
			0x0F,0x12,0x22,0x32,  // BG palette 3: blues
			0x0F,0x20,0x10,0x00,  // Sprite palette 0
			0x0F,0x16,0x26,0x36,  // Sprite palette 1
			0x0F,0x19,0x29,0x39,  // Sprite palette 2
			0x0F,0x12,0x22,0x32,  // Sprite palette 3
		};
		lnPush(0x3F00, 32, default_pal);
	}

	// Disable NMIs for CHR-RAM writes
	IO8(0x2000) = 0x00;

	// Clear all CHR-RAM ($0000-$1FFF)
	IO8(0x2006) = 0x00;
	IO8(0x2006) = 0x00;
	for (uint16_t i = 0; i < 0x2000; i++) IO8(0x2007) = 0;

	// Clear nametable 0 ($2000-$23FF = 1024 bytes: 960 tiles + 64 attributes)
	IO8(0x2006) = 0x20;
	IO8(0x2006) = 0x00;
	for (uint16_t i = 0; i < 1024; i++) IO8(0x2007) = 0;

	// Convert character ROM from flash directly to NES CHR-RAM format
	// (conversion function is in bank24, same bank as CHR data)
	convert_chr(0);

	// Re-enable NMIs
	IO8(0x2000) = lnPPUCTRL;

	lnSync(0);
	reset6502();

	// Disable APU frame IRQ
	IO8(0x4017) = 0x40;

	lnPPUCTRL &= ~0x08;
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

	interrupt_condition = 0;
	last_nmi_frame = *(volatile uint8_t*)0x26;

#ifdef TRACK_TICKS
	frame_time = clockticks6502 + FRAME_LENGTH;
#else
	frame_time = 0;
#endif

	// ---- Main loop ----
	while (1)
	{
#ifdef INTERPRETER_ONLY
		step6502();
#else
		run_6502();
#endif

#if defined(ENABLE_OPTIMIZER) && !defined(PLATFORM_MILLIPEDE)
		opt_check_trigger();
#endif

#ifndef TRACK_TICKS
		{
			uint8_t cur_nmi = *(volatile uint8_t*)0x26;
			if (cur_nmi != last_nmi_frame)
			{
				interrupt_condition |= FLAG_MILLIPEDE_IRQ;
#if defined(ENABLE_OPTIMIZER_V2) && !defined(PLATFORM_MILLIPEDE)
				opt2_frame_tick();
#endif
#ifdef ENABLE_METRICS
				{ uint8_t _mb = mapper_prg_bank; bankswitch_prg(BANK_METRICS); metrics_dump_runtime_b2(); bankswitch_prg(_mb); }
#endif
				nes_gamepad_refresh();
				render_video();
				last_nmi_frame = *(volatile uint8_t*)0x26;
			}
		}
#else
		// Tick-based IRQ: fire guest VBLANK IRQ after enough guest
		// cycles have elapsed (FRAME_LENGTH ≈ 24787 @ 1.512 MHz).
		if (clockticks6502 > frame_time)
		{
			frame_time += FRAME_LENGTH;
			interrupt_condition |= FLAG_MILLIPEDE_IRQ;
		}
		// Render + input at NES refresh rate (every NMI, ~60 Hz).
		// This is decoupled from IRQ generation so the guest isn't
		// bombarded with more IRQs than it can process.
		{
			uint8_t cur_nmi = *(volatile uint8_t*)0x26;
			if (cur_nmi != last_nmi_frame)
			{
				last_nmi_frame = cur_nmi;
#ifdef ENABLE_METRICS
				{ uint8_t _mb = mapper_prg_bank; bankswitch_prg(BANK_METRICS); metrics_dump_runtime_b2(); bankswitch_prg(_mb); }
#endif
				nes_gamepad_refresh();
				render_video();
			}
		}
#endif
		// Fire emulated IRQ — Millipede uses VBLANK IRQ for game tick.
		// The handler acknowledges by writing $2600, which clears the flag.
		// We must NOT clear interrupt_condition here — the handler needs
		// to see VBLANK=1 (bit 6 of IN0 at $2000) during the ISR.
		if (!(status & FLAG_INTERRUPT) && (interrupt_condition & FLAG_MILLIPEDE_IRQ))
		{
			irq6502();
		}
	}

	return 0;
}


// ==========================================================================
// Cache persistence (same pattern as exidy.c)
// ==========================================================================
#pragma section bank19

static void cache_compute_rom_hash(uint8_t *hash)
{
	hash[0] = read6502(0xFFFC);
	hash[1] = read6502(0xFFFD);
	hash[2] = read6502(0xFFFE);
	hash[3] = read6502(0xFFFF);
}

#pragma section default

#ifdef ENABLE_CACHE_PERSIST
void cache_write_signature(void)
{
	uint8_t rom_hash[4];
	cache_compute_rom_hash(rom_hash);
	bankswitch_prg(BANK_FLASH_BLOCK_FLAGS);
	flash_byte_program(CACHE_SIG_ADDRESS + 0, BANK_FLASH_BLOCK_FLAGS, CACHE_SIG_MAGIC_0);
	flash_byte_program(CACHE_SIG_ADDRESS + 1, BANK_FLASH_BLOCK_FLAGS, CACHE_SIG_MAGIC_1);
	flash_byte_program(CACHE_SIG_ADDRESS + 2, BANK_FLASH_BLOCK_FLAGS, CACHE_SIG_MAGIC_2);
	flash_byte_program(CACHE_SIG_ADDRESS + 3, BANK_FLASH_BLOCK_FLAGS, CACHE_SIG_MAGIC_3);
	flash_byte_program(CACHE_SIG_ADDRESS + 4, BANK_FLASH_BLOCK_FLAGS, rom_hash[0]);
	flash_byte_program(CACHE_SIG_ADDRESS + 5, BANK_FLASH_BLOCK_FLAGS, rom_hash[1]);
	flash_byte_program(CACHE_SIG_ADDRESS + 6, BANK_FLASH_BLOCK_FLAGS, rom_hash[2]);
	flash_byte_program(CACHE_SIG_ADDRESS + 7, BANK_FLASH_BLOCK_FLAGS, rom_hash[3]);
}

uint8_t cache_check_signature(void)
{
	uint8_t rom_hash[4];
	cache_compute_rom_hash(rom_hash);
	bankswitch_prg(BANK_FLASH_BLOCK_FLAGS);
	volatile uint8_t *sig = (volatile uint8_t *)CACHE_SIG_ADDRESS;
	if (sig[0] != CACHE_SIG_MAGIC_0) return 0;
	if (sig[1] != CACHE_SIG_MAGIC_1) return 0;
	if (sig[2] != CACHE_SIG_MAGIC_2) return 0;
	if (sig[3] != CACHE_SIG_MAGIC_3) return 0;
	if (sig[4] != rom_hash[0]) return 0;
	if (sig[5] != rom_hash[1]) return 0;
	if (sig[6] != rom_hash[2]) return 0;
	if (sig[7] != rom_hash[3]) return 0;
	return 1;
}

void flash_init_persist(void)
{
	if (cache_check_signature()) return;
	flash_format();
	cache_write_signature();
}
#endif
