#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "lazynes.h"
#include "config.h"
#include "exidy.h"
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


// ******************************************************************************************

// Spectar
#ifdef GAME_SPECTAR
	//#include "spectar\spectar.h"
	extern const unsigned char rom_sidetrac[];
	const uint8_t palette[] = { 0x01,0x16,0x21,0x2A, 0x01,0x21,0x11,0x01, 0x01,0x28,0x18,0x08, 0x01,0x26,0x16,0x06, 0x01,0x24,0x14,0x04, 0x01,0x24,0x14,0x04, 0x01,0x24,0x14,0x04, 0x01,0x24,0x14,0x04 };
#endif

// Side Track
#ifdef GAME_SIDE_TRACK
#pragma section bank23
	extern const unsigned char rom_sidetrac[];
	extern const unsigned char chr_sidetrac[];
	extern const unsigned char spr_sidetrac[];
#pragma section default
	const uint8_t palette[] = { 0x0F,0x30,0x30,0x28, 0x0F,0x21,0x11,0x01, 0x0F,0x28,0x18,0x08, 0x0F,0x26,0x16,0x06, 0x0F,0x30,0x30,0x28, 0x0F,0x30,0x30,0x28, 0x0F,0x30,0x30,0x28, 0x0F,0x30,0x30,0x28 };	
#endif

// CPU 6502 Test
#ifdef GAME_CPU_6502_TEST
#pragma section bank23
extern const unsigned char rom_cpu6502test[];
#pragma section default
const uint8_t palette[] = { 0x0F,0x00,0x10,0x20, 0x0F,0x10,0x20,0x30, 0x0F,0x20,0x30,0x00, 0x0F,0x30,0x00,0x10, 0x0F,0x00,0x10,0x20, 0x0F,0x10,0x20,0x30, 0x0F,0x20,0x30,0x00, 0x0F,0x30,0x00,0x10 };
#define ROM_NAME rom_cpu6502test
#define ROM_OFFSET 0x2800
#endif

// Targ
#ifdef GAME_TARG
	const uint8_t palette[] = { 0x01,0x16,0x21,0x28, 0x01,0x21,0x11,0x01, 0x01,0x28,0x18,0x08, 0x01,0x26,0x16,0x06, 0x01,0x24,0x14,0x04, 0x01,0x24,0x14,0x04, 0x01,0x24,0x14,0x04, 0x01,0x24,0x14,0x04 };
#endif

// Targ Test
#ifdef GAME_TARG_TEST_ROM
	extern const uint8_t rom_targtest[];
	const uint8_t palette[] = { 0x01,0x16,0x21,0x28, 0x01,0x21,0x11,0x01, 0x01,0x28,0x18,0x08, 0x01,0x26,0x16,0x06, 0x01,0x24,0x14,0x04, 0x01,0x24,0x14,0x04, 0x01,0x24,0x14,0x04, 0x01,0x24,0x14,0x04 };
#endif

// ******************************************************************************************

extern uint8_t RAM_BASE[];
extern uint8_t SCREEN_RAM_BASE[];
extern uint8_t CHARACTER_RAM_BASE[];
extern uint16_t interpret_count;  // Debug: from fake6502.c
extern uint8_t write_50xx_count;  // Debug: writes to $50xx from interpreter
extern uint8_t last_interpreted_opcode;  // Debug: last opcode interpreted
extern uint8_t indy_hit_count;  // Debug: from dynamos.c
extern uint8_t sta_indy_interpret_count;  // Debug: from fake6502.c
extern uint16_t last_indy_ea;  // Debug: from fake6502.c
extern uint8_t sta_5000_count;  // Debug: STA to $5000 specifically
#ifdef ENABLE_DEBUG_STATS
extern void debug_stats_update(void);  // Debug: write stats to WRAM $7E00
#endif

__zpage uint8_t interrupt_condition;
__zpage uint8_t character_ram_updated = 0;
__zpage uint8_t screen_ram_updated = 0;

// 64-byte zero buffer for re-clearing the attribute table after
// full-screen nametable pushes (Exidy has no attribute concept).
#pragma section rodata21
static const uint8_t attr_zeros[64] = {0};
#pragma section default

// Shadow buffer and VRAM update list are in dynamos-asm.s (BSS/WRAM)
extern uint8_t screen_shadow[];
extern uint8_t vram_update_list[];
__zpage extern uint8_t vram_list_pos;

// WRAM-resident assembly diff helper (dynamos-asm.s)
// Returns: 0 = no changes, 1 = list built, $FF = overflow
extern uint8_t screen_diff_build_list(void);

// Empty update list (kept for potential future use)
static const uint8_t empty_update_list[] = {0xFF};

// Forward declarations
void render_video(void);

// Sprite 1 (player/motion object 1)
__zpage uint8_t sprite1_xpos = 0;
__zpage uint8_t sprite1_ypos = 0;
__zpage uint8_t sprite1_write_count = 0;  // Debug counter

// Sprite 2 (enemy/motion object 2) 
__zpage uint8_t sprite2_xpos = 0;
__zpage uint8_t sprite2_ypos = 0;
__zpage uint8_t sprite2_write_count = 0;  // Debug counter

// Sprite control registers
__zpage uint8_t spriteno = 0;        // $5100: bits 0-3 = sprite1 tile, bits 4-7 = sprite2 tile
__zpage uint8_t sprite_enable = 0;   // $5101: bit 7 = enable sprite1, bit 6 = enable sprite2
                                     //        bit 5 = sprite1 set, bit 4 = sprite2 set

__zpage uint8_t irq_count = 0;       // Debug: count IRQs triggered

__zpage uint32_t frame_time;
__zpage uint8_t last_nmi_frame = 0;  // Tracks lazyNES NMI frame counter (ZP $26)
__zpage uint8_t audio = 0;
__zpage uint8_t sprites_converted = 0;


// ******************************************************************************************

// ==========================================================================
// convert_sprite / convert_sprites — moved to BANK_INIT_CODE (bank25).
// Init-only code, saves ~481 bytes of fixed-bank space.
// Only touches PPU registers ($2006/$2007) and reads source data.
// ==========================================================================
#pragma section bank25

// Convert a single 16x16 Exidy sprite (32 bytes, 1bpp) to 4 NES 8x8 tiles
// Exidy format: 16 bytes left column (8 pixels wide, 16 rows), 16 bytes right column
// NES format: 4 tiles in order: TL, TR, BL, BR (each 16 bytes: 8 bytes plane0, 8 bytes plane1=0)
static void convert_sprite_b2(const uint8_t *src, uint16_t nes_chr_addr)
{
	// Top-left tile (rows 0-7, columns 0-7)
	IO8(0x2006) = nes_chr_addr >> 8;
	IO8(0x2006) = nes_chr_addr & 0xFF;
	for (uint8_t row = 0; row < 8; row++)
		IO8(0x2007) = src[row];  // Left column, rows 0-7
	for (uint8_t row = 0; row < 8; row++)
		IO8(0x2007) = 0;  // Plane 1 = 0 (1bpp)
	
	// Top-right tile (rows 0-7, columns 8-15)
	IO8(0x2006) = (nes_chr_addr + 16) >> 8;
	IO8(0x2006) = (nes_chr_addr + 16) & 0xFF;
	for (uint8_t row = 0; row < 8; row++)
		IO8(0x2007) = src[16 + row];  // Right column, rows 0-7
	for (uint8_t row = 0; row < 8; row++)
		IO8(0x2007) = 0;  // Plane 1 = 0
	
	// Bottom-left tile (rows 8-15, columns 0-7)
	IO8(0x2006) = (nes_chr_addr + 32) >> 8;
	IO8(0x2006) = (nes_chr_addr + 32) & 0xFF;
	for (uint8_t row = 0; row < 8; row++)
		IO8(0x2007) = src[8 + row];  // Left column, rows 8-15
	for (uint8_t row = 0; row < 8; row++)
		IO8(0x2007) = 0;  // Plane 1 = 0
	
	// Bottom-right tile (rows 8-15, columns 8-15)
	IO8(0x2006) = (nes_chr_addr + 48) >> 8;
	IO8(0x2006) = (nes_chr_addr + 48) & 0xFF;
	for (uint8_t row = 0; row < 8; row++)
		IO8(0x2007) = src[24 + row];  // Right column, rows 8-15
	for (uint8_t row = 0; row < 8; row++)
		IO8(0x2007) = 0;  // Plane 1 = 0
}

// Convert all Exidy sprites to NES CHR format
// Sidetrac has 16 sprites (512 bytes / 32 = 16 sprites)
// Each 16x16 sprite becomes 4 NES 8x8 tiles
// Sprites go into CHR starting at tile 0x80 (second half of pattern table 0)
static void convert_sprites_b2(const uint8_t *src)
{
	uint16_t nes_addr = 0x0800;  // Start of second half of pattern table 0 (tiles 0x80+)
	for (uint8_t sprite = 0; sprite < 16; sprite++)
	{
		convert_sprite_b2(src + (sprite * 32), nes_addr);
		nes_addr += 64;  // 4 tiles * 16 bytes each
	}
}

#pragma section default

// Fixed-bank trampolines
void convert_sprite(const uint8_t *src, uint16_t nes_chr_addr)
{
	uint8_t saved_bank = mapper_prg_bank;
	bankswitch_prg(BANK_INIT_CODE);
	convert_sprite_b2(src, nes_chr_addr);
	bankswitch_prg(saved_bank);
}

void convert_sprites(const uint8_t *src)
{
	uint8_t saved_bank = mapper_prg_bank;
	bankswitch_prg(BANK_INIT_CODE);
	convert_sprites_b2(src);
	bankswitch_prg(saved_bank);
}

// ******************************************************************************************

//#pragma section text0

int main(void)
{	
	lnSync(1);

	// lnSync's first call sets PPUCTRL = $88 (NMI enable + increment-
	// by-32).  We must clear the increment bit BEFORE lnPush, otherwise
	// the palette push scatters data every 32 PPU addresses — only
	// writing palette index 0 and spilling palette values into CHR-RAM
	// ($0000, $0020, ... $02E0), corrupting tile patterns.
	lnPPUCTRL &= ~0x08;
	IO8(0x2000) = lnPPUCTRL;

	lnPush(0x3F00, 32, palette);

	// Disable NMIs while doing direct $2006/$2007 writes.
	// The CHR-RAM clear takes ~98K cycles (3+ frames), so LazyNES's
	// NMI handler would fire multiple times and corrupt the PPU address
	// register, leaving random CHR-RAM bytes un-cleared.
	IO8(0x2000) = 0x00;

	// Clear all CHR-RAM ($0000-$1FFF) so unwritten bit-planes are zero.
	// convert_chr only writes one plane per tile group; without this,
	// stray non-zero bytes in the other plane cause pixel artifacts.
	IO8(0x2006) = 0x00;
	IO8(0x2006) = 0x00;
	for (uint16_t i = 0; i < 0x2000; i++) IO8(0x2007) = 0;

	// Clear nametable 0 attribute table ($23C0-$23FF, 64 bytes).
	// Ensures palette-0 everywhere.  Without this, a power-cycle in
	// the emulator can leave random attribute data that never gets
	// overwritten (the Exidy has no attribute-table concept).
	IO8(0x2006) = 0x23;
	IO8(0x2006) = 0xC0;
	for (uint8_t i = 0; i < 64; i++) IO8(0x2007) = 0;

#ifdef ENABLE_CHR_ROM
	// Copy ROM CHR/sprite data from BANK_PLATFORM_ROM to WRAM before converting,
	// because convert_chr/convert_sprites trampolines switch to BANK_RENDER
	// which unmaps the BANK_PLATFORM_ROM source data.
	bankswitch_prg(BANK_PLATFORM_ROM);
	memcpy(CHARACTER_RAM_BASE, (uint8_t*)chr_sidetrac, 1024);
	memcpy(CHARACTER_RAM_BASE + 1024, (uint8_t*)spr_sidetrac, 512);
	bankswitch_prg(0);
	convert_chr(CHARACTER_RAM_BASE);
	convert_sprites(CHARACTER_RAM_BASE + 1024);
#endif

	// Re-enable NMIs now that all direct PPU writes are done.
	IO8(0x2000) = lnPPUCTRL;
	
	lnSync(0);
	reset6502();		
	
	// Disable APU frame IRQ — lazynes doesn't do this and the frame
	// counter defaults to 4-step mode which generates IRQs.  The NES I
	// flag is set during JIT dispatch, but belt-and-suspenders.
	IO8(0x4017) = 0x40;
	
	lnPPUCTRL &= ~0x08;
	lnPPUMASK = 0x3A;

#ifdef ENABLE_CACHE_PERSIST
	flash_init_persist();
#else
	flash_format();
#endif
	flash_cache_init_sectors();
	
#ifdef ENABLE_OPTIMIZER
	// Initialize optimizer with debug thresholds
	// Requires OPT_MIN_BLOCKS_DEBUG unique blocks before optimizer can run
	opt_init(OPT_THRESHOLD_DEBUG, OPT_MIN_BLOCKS_DEBUG);
#endif

#ifdef ENABLE_STATIC_ANALYSIS
	// Run one-time static analysis pass: BFS walk of ROM + batch compile.
	// Must run after flash_format() and before the main loop.
	sa_run();
#endif
	
	interrupt_condition = 0;

	last_nmi_frame = *(volatile uint8_t*)0x26;  // sync with lazyNES NMI counter

#ifdef TRACK_TICKS
	frame_time = clockticks6502 + FRAME_LENGTH;
#else
	frame_time = 0;
#endif	
	
	while (1)
	{
		#ifdef INTERPRETER_ONLY
		step6502();
		#else
		run_6502();
		#endif

#ifdef ENABLE_OPTIMIZER
		// Check if optimization should run (periodically from main loop)
		// This catches dispatches even if flash blocks aren't being executed
		opt_check_trigger();
#endif
		
		
#ifndef TRACK_TICKS
		// NMI-driven frame timing: lazyNES NMI handler increments ZP $26
		// every vblank (60Hz). This decouples frame timing from instruction
		// count, so JIT blocks with tight native loops don't starve the IRQ.
		{
			uint8_t cur_nmi = *(volatile uint8_t*)0x26;
			if (cur_nmi != last_nmi_frame)
			{
				interrupt_condition |= FLAG_EXIDY_IRQ;
#ifdef ENABLE_DEBUG_STATS
				debug_stats_update();
#endif
#ifdef ENABLE_OPTIMIZER_V2
				opt2_frame_tick();
#endif
#ifdef ENABLE_METRICS
				{ uint8_t _mb = mapper_prg_bank; bankswitch_prg(BANK_METRICS); metrics_dump_runtime_b2(); bankswitch_prg(_mb); }
#endif
				nes_gamepad_refresh();
				render_video();
				// Re-read AFTER render_video: if an NMI fired during
				// render setup, absorb the $26 increment so we don't
				// immediately re-trigger a second render.
				last_nmi_frame = *(volatile uint8_t*)0x26;
			}
		}
#else
		if (clockticks6502 > frame_time)
		{
			frame_time += FRAME_LENGTH;
			interrupt_condition |= FLAG_EXIDY_IRQ;
#ifdef ENABLE_DEBUG_STATS
			debug_stats_update();
#endif
#ifdef ENABLE_OPTIMIZER_V2
			opt2_frame_tick();
#endif
#ifdef ENABLE_METRICS
			{ uint8_t _mb = mapper_prg_bank; bankswitch_prg(BANK_METRICS); metrics_dump_runtime_b2(); bankswitch_prg(_mb); }
#endif
			nes_gamepad_refresh();
			render_video();
			// Re-read AFTER render_video: if an NMI fired during
			// render setup, absorb the $26 increment so the backstop
			// doesn't immediately re-trigger a second render.
			last_nmi_frame = *(volatile uint8_t*)0x26;
		}
		// NMI backstop: even if the cycle counter hasn't reached frame_time
		// (e.g. during very tight delay loops where DISPATCH_OVERHEAD under-
		// estimates), the real NES vblank still fires.  Checking ZP $26
		// ensures we never fall more than one real vblank behind.
		// This also catches cases where SEI disables IRQs for thousands
		// of iterations (crash animation) — the frame counter still advances.
		{
			uint8_t cur_nmi = *(volatile uint8_t*)0x26;
			if (cur_nmi != last_nmi_frame)
			{
				last_nmi_frame = cur_nmi;
				// If IRQ handler is executing (I flag set), don't render —
				// the handler is mid-dispatch and will RTI soon. Rendering
				// here would burn a frame waiting for vblank, starving the
				// handler and causing 2 renders per 3 frames.
				if (!(interrupt_condition & FLAG_EXIDY_IRQ) && !(status & FLAG_INTERRUPT))
				{
					interrupt_condition |= FLAG_EXIDY_IRQ;
					// Sync cycle counter to avoid double-firing when
					// the next cycle-based frame would also trigger
					frame_time = clockticks6502 + FRAME_LENGTH;
					render_video();
					last_nmi_frame = *(volatile uint8_t*)0x26;
				}
			}
		}
#endif	
		// Fire emulated IRQ once per frame (if interrupts are enabled).
		// The Side Track IRQ handler at $2B0E is the game's frame tick —
		// it updates sprites, runs game logic, and reads $5103 to ack.
		if (!(status & FLAG_INTERRUPT) && (interrupt_condition & FLAG_EXIDY_IRQ))
		{
			interrupt_condition &= ~FLAG_EXIDY_IRQ;
			irq_count++;
			irq6502();
		}
	}
	
	return 0;
}

// ******************************************************************************************

uint8_t read6502(uint16_t address)
{
#ifdef DEBUG_CPU_READ
	printf("RD $%02X:$%02X\n", ROM_NAME[(address & 0x3FFF) - ROM_OFFSET], address);
#endif
#ifdef DEBUG_AUDIO
	audio ^= address >> 8;
	IO8(0x4011) = audio;
#endif
	if (address < 0x400)		
		return RAM_BASE[address];	
	if (address < 0x4000)
	{
		uint8_t saved_bank = mapper_prg_bank;
		bankswitch_prg(BANK_PLATFORM_ROM);
		uint8_t temp = ROM_NAME[address - ROM_OFFSET];
		bankswitch_prg(saved_bank);
		return temp;
	}
	if (address < 0x4800)
		return SCREEN_RAM_BASE[address - 0x4000];
#ifdef ENABLE_CHR_ROM
	if (address < 0x5000)
	{
		uint8_t saved_bank = mapper_prg_bank;
		bankswitch_prg(BANK_PLATFORM_ROM);
		uint8_t temp = CHR_NAME[address - CHR_OFFSET];
		bankswitch_prg(saved_bank);
		return temp;
	}
#else	
	if (address < 0x5000)
		return IO8(CHARACTER_RAM_BASE + address - 0x4800);	
#endif	
	
	if (address == 0x5100)	// $5100 DIP switch settings
		return 0xED;
	if (address == 0x5101)	// $5101 control inputs port
		return nes_gamepad();
	if (address == 0x5103)	// $5103 interrupt condition latch
	{
		uint8_t vblank = interrupt_condition;
		interrupt_condition &= ~FLAG_EXIDY_IRQ;
		if (nes_gamepad() & TARG_COIN1)
			return (0x40 | vblank);
		else
			return (0x00 | vblank);
	}
		//return 0xC0;

	if (address >= 0xFF00)
	{
		uint8_t saved_bank = mapper_prg_bank;
		bankswitch_prg(BANK_PLATFORM_ROM);
		uint8_t temp = ROM_NAME[(address & 0x3FFF) - ROM_OFFSET];
		bankswitch_prg(saved_bank);
		return temp;
	}
	return IO8(address); // OK, just read whatever then	
	
}

// ******************************************************************************************

void write6502(uint16_t address, uint8_t value)
{
#ifdef DEBUG_CPU_WRITE
	printf("WR $%02X:$%02X\n", value, address);
#endif
#ifdef DEBUG_AUDIO
	audio ^= value;
	IO8(0x4011) = audio;
#endif
	if (address < 0x400)
		RAM_BASE[address] = value;
	else if ((address & 0xFFC0) == 0x5000)  // $5000-$503F: Sprite 1 X position
	{
		sprite1_xpos = value;  // The VALUE written is the position
		sprite1_write_count++;
	}
	else if ((address & 0xFFC0) == 0x5040)  // $5040-$507F: Sprite 1 Y position
	{
		sprite1_ypos = value;
		sprite1_write_count++;
	}
	else if ((address & 0xFFC0) == 0x5080)  // $5080-$50BF: Sprite 2 X position
	{
		sprite2_xpos = value;
		sprite2_write_count++;
	}
	else if ((address & 0xFFC0) == 0x50C0)  // $50C0-$50FF: Sprite 2 Y position
	{
		sprite2_ypos = value;
		sprite2_write_count++;
	}
	else if ((address & 0xFFFC) == 0x5100)  // $5100: Sprite number / $5101: Sprite enable
	{
		if ((address & 0x03) == 0)
			spriteno = value;       // $5100: bits 0-3 = sprite1, bits 4-7 = sprite2
		else if ((address & 0x03) == 1)
			sprite_enable = value;  // $5101: bit 7 = enable spr1, bit 6 = enable spr2
	}
	else if ((address >= 0x4000) && (address < 0x4800))
	{		
		SCREEN_RAM_BASE[address - 0x4000] = value;
		screen_ram_updated = 1;
	}
	else if ((address >= 0x4800) && (address < 0x5000))
	{
		CHARACTER_RAM_BASE[address - 0x4800] = value;
		character_ram_updated = 1;
	}
	return;
}

// ******************************************************************************************

static uint8_t cached_gamepad = 0xFF;  // all buttons released

void nes_gamepad_refresh(void)
{	
	uint8_t targ = 0;		
	uint8_t joypad = lnGetPad(1);
	targ |= (joypad & lfR) ? TARG_RIGHT : 0x00;
	targ |= (joypad & lfL) ? TARG_LEFT : 0x00;
	targ |= (joypad & lfU) ? TARG_UP : 0x00;
	targ |= (joypad & lfD) ? TARG_DOWN : 0x00;
	targ |= (joypad & lfA) ? TARG_FIRE : 0x00;
	targ |= (joypad & lfSelect) ? TARG_COIN1 : 0x00;
	targ |= (joypad & lfStart) ? TARG_1START : 0x00;	
	cached_gamepad = (targ ^ 0xFF);
}

uint8_t nes_gamepad(void)
{
	return cached_gamepad;
}

// ******************************************************************************************

// ==========================================================================
// ln_fire_and_forget — non-blocking replacement for lnSync(0).
// Sets up lnState for the NMI handler to process OAM + update list,
// but returns immediately without waiting for vblank.
// This reclaims the ~16ms vblank wait for emulation work.
//
// Safe because:
// - The main loop only calls render_video when $26 changes (NMI fired).
// - The NMI handler processes OAM/lnList BEFORE incrementing $26.
// - So by the time render_video is called again, previous data is consumed.
// ==========================================================================
static void ln_fire_and_forget(void)
{
	// Enable bg+sprite rendering (same as lnSync(0))
	lnPPUMASK |= 0x18;

	// Clear split-mode bit
	*(volatile uint8_t*)0x25 &= ~0x02;

	// Fill remaining OAM with $FF (hide unused sprites)
	{
		uint8_t pos = *(volatile uint8_t*)0x27;  // nmiOamPos
		volatile uint8_t *oam = (volatile uint8_t*)0x0200;
		while (pos != 0) {
			oam[pos] = 0xFF;
			pos += 4;
		}
		*(volatile uint8_t*)0x27 = 0;  // reset nmiOamPos
	}

	// Set sync request: clear bit 5, set bit 6.
	// Bit 6 tells the NMI handler to do OAM DMA + process lnList.
	{
		uint8_t state = *(volatile uint8_t*)0x25;
		state = (state & ~0x20) | 0x40;
		*(volatile uint8_t*)0x25 = state;
	}

	// Clear scroll call counter (same as lnSync epilogue)
	*(volatile uint8_t*)0x2F = 0;
}

// ==========================================================================
// render_video — moved to BANK_RENDER (bank22 for Exidy) to free bank2 space.
// Called once per frame; only touches fixed-bank LazyNES calls and WRAM.
// ==========================================================================
#pragma section bank22

void render_video_b2(void)
{
	// Early out: if nothing changed in background/CHR, skip work.
	if (!screen_ram_updated && !character_ram_updated)
		return;

	// --- Character RAM: requires blank mode, must be handled first ---
	// If char RAM changed, go blank and push everything (screen too).
#ifndef ENABLE_CHR_ROM
	if (character_ram_updated)
	{
		lnSync(1);
		character_ram_updated = 0;
		// Disable NMIs while convert_chr writes $2006/$2007 directly.
		// convert_chr processes 256 tiles (thousands of cycles); without
		// this, the NMI handler fires mid-write and reads $2002, resetting
		// the $2006 hi/lo latch.  That sends tile data to random PPU
		// addresses, corrupting the attribute table.
		IO8(0x2000) = lnPPUCTRL & 0x7F;
		convert_chr(CHARACTER_RAM_BASE);
		IO8(0x2000) = lnPPUCTRL;
		// While blanked, push screen RAM too if dirty
		if (screen_ram_updated)
		{
			screen_ram_updated = 0;
			lnPush(0x2000, 0, SCREEN_RAM_BASE);
			lnPush(0x2100, 0, SCREEN_RAM_BASE+0x100);
			lnPush(0x2200, 0, SCREEN_RAM_BASE+0x200);
			lnPush(0x2300, 0xC0, SCREEN_RAM_BASE+0x300);
			// lnPush left PPU address at $23C0 (attribute table).
			// Re-clear attributes so any stray $2007 write between
			// here and the NMI address-reset can't corrupt them.
			lnPush(0x23C0, 64, attr_zeros);
			memcpy(screen_shadow, SCREEN_RAM_BASE, 0x400);
		}
	}
	else
#endif
	// --- Screen RAM: incremental update via shadow-buffer diff ---
	if (screen_ram_updated)
	{
		screen_ram_updated = 0;
		uint8_t result = screen_diff_build_list();
		if (result == 0xFF)
		{
			// Too many changes for one vblank (screen transition).
			// Blank for one frame and push everything.
			lnSync(1);
			lnPush(0x2000, 0, SCREEN_RAM_BASE);
			lnPush(0x2100, 0, SCREEN_RAM_BASE+0x100);
			lnPush(0x2200, 0, SCREEN_RAM_BASE+0x200);
			lnPush(0x2300, 0xC0, SCREEN_RAM_BASE+0x300);
			// Re-clear attribute table (Exidy has no attributes).
			lnPush(0x23C0, 64, attr_zeros);
			// Shadow already updated by asm helper
		}
		else if (result == 1)
		{
			// Incremental update: screen stays on, no flicker!
			lnList(vram_update_list);
		}
	}

	// --- Sprites (unchanged) ---
	uint8_t spr1_x = 236 - sprite1_xpos - 4;
	uint8_t spr1_y = 244 - sprite1_ypos - 4;
	uint8_t spr2_x = 236 - sprite2_xpos - 4;
	uint8_t spr2_y = 244 - sprite2_ypos - 4;

	uint8_t spr1_exidy_tile = (spriteno & 0x0F);
	uint8_t spr2_exidy_tile = ((spriteno >> 4) & 0x0F);

	uint8_t spr1_nes_base = 0x80 + (spr1_exidy_tile * 4);
	uint8_t spr2_nes_base = 0x80 + (spr2_exidy_tile * 4);

	uint8_t spr2_meta[17] = {
		0, 0, spr2_nes_base + 0, 1,
		8, 0, spr2_nes_base + 1, 1,
		0, 8, spr2_nes_base + 2, 1,
		8, 8, spr2_nes_base + 3, 1,
		128
	};
	lnAddSpr(spr2_meta, spr2_x, spr2_y);

	uint8_t spr1_meta[17] = {
		0, 0, spr1_nes_base + 0, 0,
		8, 0, spr1_nes_base + 1, 0,
		0, 8, spr1_nes_base + 2, 0,
		8, 8, spr1_nes_base + 3, 0,
		128
	};
	lnAddSpr(spr1_meta, spr1_x, spr1_y);

	// --- Final sync (screen on, non-blocking) ---
	// Queue OAM + lnList for NMI processing and return immediately.
	// Unlike lnSync(0), this doesn't wait for vblank, reclaiming
	// ~16ms per frame for emulation when running below 60fps.
	ln_fire_and_forget();

	return;
}

#pragma section default

// Fixed-bank trampoline: saves current bank, switches to BANK_RENDER, calls
// render_video_b2(), then restores the previous bank.
void render_video(void)
{
	uint8_t saved_bank = mapper_prg_bank;
	bankswitch_prg(BANK_RENDER);
	render_video_b2();
	bankswitch_prg(saved_bank);
}

// ******************************************************************************************

// ==========================================================================
// convert_chr — moved to BANK_INIT_CODE (bank25) to free bank22 space.
// Only called at init and when character RAM is updated.
// ==========================================================================
#pragma section bank25

static void convert_chr_b2(uint8_t *source)
{
	uint16_t tiles = 0;
	uint16_t nes_address = 0;
	uint16_t emu_address = 0;

	for (; tiles < 64; tiles++)
	{			
		IO8(0x2006) = nes_address >> 8;
		IO8(0x2006) = nes_address & 0xFF;
		for (uint8_t i = 0; i < 8; i++)
		{
			IO8(0x2007) = source[emu_address + i];
		}
		nes_address += 16;
		emu_address += 8;
	}
	
	nes_address += 0x8;
	
	for (; tiles < 128; tiles++)
	{			
		IO8(0x2006) = nes_address >> 8;
		IO8(0x2006) = nes_address & 0xFF;
		for (uint8_t i = 0; i < 8; i++)
		{
			IO8(0x2007) = source[emu_address + i];
		}
		nes_address += 16;
		emu_address += 8;
	}
	
	nes_address -= 0x8;
	
	for (; tiles < 192; tiles++)
	{			
		IO8(0x2006) = nes_address >> 8;
		IO8(0x2006) = nes_address & 0xFF;
		for (uint8_t i = 0; i < 8; i++)
		{
			IO8(0x2007) = source[emu_address + i];
		}
		for (uint8_t i = 0; i < 8; i++)
		{
			IO8(0x2007) = source[emu_address + i];
		}
		nes_address += 16;
		emu_address += 8;
	}
	
	nes_address += 0x8;
	
	for (; tiles < 256; tiles++)
	{			
		IO8(0x2006) = nes_address >> 8;
		IO8(0x2006) = nes_address & 0xFF;
		for (uint8_t i = 0; i < 8; i++)
		{
			IO8(0x2007) = source[emu_address + i];
		}
		nes_address += 16;
		emu_address += 8;
	}
}

#pragma section default

void convert_chr(uint8_t *source)
{
	uint8_t saved_bank = mapper_prg_bank;
	bankswitch_prg(BANK_INIT_CODE);
	convert_chr_b2(source);
	bankswitch_prg(saved_bank);
}


// ******************************************************************************************
// ==========================================================================
// flash_format — moved to BANK_INIT_CODE (bank25) to free bank2 space.
// Startup-only.  Calls flash_sector_erase() which lives in WRAM and
// manages its own bank switching internally.
// ==========================================================================
#pragma section bank25

#ifdef ENABLE_STATIC_ANALYSIS
// Extern refs for SA_SECTOR_FIRST/LAST macros (defined in static_analysis.c)
extern uint8_t sa_code_bitmap[];
extern uint8_t sa_indirect_list[];
extern uint8_t sa_subroutine_list[];
#endif

static void flash_format_b2(void)
{	
	for (uint8_t bank = 3; bank < 31; bank++)
	{
		// Skip banks that contain our code/data — erasing them
		// would destroy the running program and ROM assets.
		if (bank == BANK_COMPILE)      continue;  // compile-time banked code
		if (bank == BANK_RENDER)       continue;  // render_video_b2, metrics
		if (bank == BANK_PLATFORM_ROM) continue;  // ROM incbin data
		if (bank == BANK_SA_CODE)      continue;  // static analysis code
		if (bank == BANK_INIT_CODE)    continue;  // this function + convert_chr_b2

		for (uint16_t sector = 0x8000; sector < 0xC000; sector += 0x1000)
		{
#ifdef ENABLE_STATIC_ANALYSIS
			// Protect the SA persistence region in bank 3 from erasure.
			// Sectors from SA_SECTOR_FIRST to SA_SECTOR_LAST contain the
			// code bitmap, header, indirect-target list, and subroutine
			// table.  The BFS walk checks the header signature to decide
			// whether to erase and rebuild — so these sectors must survive
			// flash_format.
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

// ******************************************************************************************

#ifdef ENABLE_CACHE_PERSIST

// Compute a 4-byte hash from the source ROM to detect game changes.
// Reads the reset vector ($FFFC-$FFFD), IRQ vector ($FFFE-$FFFF),
// and first two ROM bytes to form a simple fingerprint.
static void cache_compute_rom_hash(uint8_t *hash)
{
	hash[0] = read6502(0xFFFC);  // reset vector lo
	hash[1] = read6502(0xFFFD);  // reset vector hi
	hash[2] = read6502(0xFFFE);  // IRQ vector lo
	hash[3] = read6502(0xFFFF);  // IRQ vector hi
}

// Write the cache signature + ROM hash to flash (bank 3, offset $3D0).
// Must be called AFTER flash_format() since erase sets all bytes to $FF.
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

// Check if a valid cache signature exists in flash.
// Returns 1 if signature matches (magic + ROM hash), 0 otherwise.
uint8_t cache_check_signature(void)
{
	uint8_t rom_hash[4];
	cache_compute_rom_hash(rom_hash);
	
	bankswitch_prg(BANK_FLASH_BLOCK_FLAGS);
	
	// Check magic bytes
	volatile uint8_t *sig = (volatile uint8_t *)CACHE_SIG_ADDRESS;
	if (sig[0] != CACHE_SIG_MAGIC_0) return 0;
	if (sig[1] != CACHE_SIG_MAGIC_1) return 0;
	if (sig[2] != CACHE_SIG_MAGIC_2) return 0;
	if (sig[3] != CACHE_SIG_MAGIC_3) return 0;
	
	// Check ROM hash
	if (sig[4] != rom_hash[0]) return 0;
	if (sig[5] != rom_hash[1]) return 0;
	if (sig[6] != rom_hash[2]) return 0;
	if (sig[7] != rom_hash[3]) return 0;
	
	return 1;  // Valid cache
}

// Initialize flash cache with persistence support.
// If a valid signature is found, skip the expensive full erase.
// Otherwise, format flash and write a new signature.
void flash_init_persist(void)
{
	if (cache_check_signature())
	{
		// Valid cache found — skip flash_format() entirely.
		// All block flags, PC tables, and compiled code are still intact.
		return;
	}
	
	// No valid cache (first boot, different ROM, or corrupted) — full erase
	flash_format();
	cache_write_signature();
}

#endif // ENABLE_CACHE_PERSIST

// ******************************************************************************************


