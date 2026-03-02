/**
 * nes.c - NES platform implementation (Donkey Kong / NROM)
 *
 * Equivalent of exidy.c for NES games.  Contains main(), read6502(),
 * write6502(), render_video(), and NES-specific PPU handling.
 *
 * Bank map:
 *   BANK_NES_PRG_LO (20) — 16KB PRG-ROM ($C000-$FFFF, mirrored at $8000)
 *   BANK_NES_CHR    (23) — 8KB CHR-ROM (uploaded to PPU at boot)
 *   BANK_RENDER     (21) — render/metrics code (NES build)
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "lazynes.h"
#include "config.h"
#include "nes.h"
#include "dynamos.h"
#include "bank_map.h"
#include "mapper30.h"
#include "core/optimizer.h"
#ifdef ENABLE_STATIC_ANALYSIS
#include "core/static_analysis.h"
#endif
#include "core/metrics.h"


// ******************************************************************************************
// ROM extern declarations — symbols live in bank20 (PRG) and bank23 (CHR)
// ******************************************************************************************

#pragma section bank20
extern const unsigned char rom_nes_prg[];
#pragma section bank23
extern const unsigned char chr_nes[];
#pragma section default

// ******************************************************************************************
// NES default palette (loaded at boot, games override via $2006/$2007 writes)
// ******************************************************************************************
const uint8_t palette[] = {
	0x0F,0x06,0x16,0x30, 0x0F,0x0A,0x2A,0x30, 0x0F,0x02,0x12,0x30, 0x0F,0x08,0x18,0x30,
	0x0F,0x06,0x16,0x30, 0x0F,0x0A,0x2A,0x30, 0x0F,0x02,0x12,0x30, 0x0F,0x08,0x18,0x30
};

// ROM_NAME is an assembly symbol (_ROM_NAME) set by dynamos-asm.s for GAME_NUMBER=10.


// ******************************************************************************************
// State
// ******************************************************************************************

extern uint8_t RAM_BASE[];
extern uint8_t SCREEN_RAM_BASE[];
extern uint8_t CHARACTER_RAM_BASE[];

__zpage uint8_t interrupt_condition;

// Dummy Exidy-specific vars referenced by shared dynamos.c code.
// NES translate_address returns 0 for $4000-$4FFF so these are never touched.
__zpage uint8_t screen_ram_updated = 0;
__zpage uint8_t character_ram_updated = 0;

__zpage uint32_t frame_time;
__zpage uint8_t audio = 0;

__zpage extern uint8_t sp;
__zpage extern uint16_t pc;
__zpage extern uint8_t a;
__zpage extern uint8_t x;
__zpage extern uint8_t y;
__zpage extern uint8_t opcode;

uint8_t ppu_queue_index = 0;
uint8_t ppu_queue[256];

uint8_t PPUCTRL_soft = 0;
uint8_t PPUMASK_soft = 0;
uint8_t PPUADDR_soft[2];
uint8_t PPUADDR_latch = 0;
uint8_t PPUSCROLL_soft[2];

extern void bankswitch_prg(__reg("a") uint8_t bank);
extern uint8_t mapper_prg_bank;
extern void flash_sector_erase(uint16_t addr, uint8_t bank);
extern void flash_cache_init_sectors(void);

__zpage uint8_t last_nmi_frame;  // __zpage: ASM dispatch references it directly
static uint8_t fps_counter;

// Nested NMI prevention: guest NMI handler runs through the main-loop
// dispatches and may span several real VBlanks.  Without a guard, each
// VBlank would fire nmi6502() again, corrupting the guest stack.
// We detect RTI completion by monitoring the guest stack pointer.
uint8_t nmi_active = 0;
uint8_t nmi_sp_guard;  // sp value just before nmi6502()

// OAM DMA deferred execution: JIT-compiled STA $4014 writes the source
// page here instead of hitting the hardware register directly.  The main
// loop executes the actual DMA during VBlank when timing is correct.
uint8_t oam_dma_request = 0;


// ******************************************************************************************
// PPU register write handler (NES $2000-$2007)
// ******************************************************************************************

void nes_register_write(uint16_t address, uint8_t value)
{
	switch (address & 7)
	{
		case 0: // $2000 PPUCTRL
			lnPPUCTRL = (value | 0x80);
			PPUCTRL_soft = value;
			// Don't lnSync here — the NMI handler applies lnPPUCTRL
			// to $2000 every VBlank.  Blocking here causes multi-frame
			// stalls because DK writes $2000 several times per game frame.
			break;
		case 1: // $2001 PPUMASK
			lnPPUMASK = value;
			PPUMASK_soft = value;
			// Same — shadow applied by NMI handler at next VBlank.
			break;
		case 5: // $2005 PPUSCROLL
			PPUSCROLL_soft[PPUADDR_latch++ & 1] = value;
			lnScroll(PPUSCROLL_soft[0], PPUSCROLL_soft[1]);
			break;
		case 6: // $2006 PPUADDR
			PPUADDR_soft[PPUADDR_latch++ & 1] = value;
			break;
		case 7: // $2007 PPUDATA
		{
			// Mask PPU address hi to 6 bits: lnList format uses bit 6 (lfHor)
			// and bit 7 (lfVer) as command flags.  Raw PPU addresses >= $4000
			// (mirrors, or auto-incremented past $3FFF) would be misinterpreted
			// as bulk-write commands, desynchronising the VRU stream and
			// causing the NMI handler to loop forever.
			ppu_queue[ppu_queue_index + 0] = PPUADDR_soft[0] & 0x3F;
			ppu_queue[ppu_queue_index + 1] = PPUADDR_soft[1];
			ppu_queue[ppu_queue_index + 2] = value;
			ppu_queue_index += 3;

			if (PPUCTRL_soft & 0x4)	// inc by 32
			{
				if ((PPUADDR_soft[1] & 0xE0) == 0xE0)
					PPUADDR_soft[0] = (PPUADDR_soft[0] + 1) & 0x3F;
				PPUADDR_soft[1] += 32;
			}
			else	// inc by 1
			{
				if (++PPUADDR_soft[1] == 0)
					PPUADDR_soft[0] = (PPUADDR_soft[0] + 1) & 0x3F;
			}

			if (ppu_queue_index >= 96)
				render_video();
			break;
		}
		default:
			break;
	}
}


// ******************************************************************************************
// Main
// ******************************************************************************************

int main(void)
{
	lnSync(1);
	lnPush(0x3F00, 32, palette);

	// Upload 8KB CHR-ROM to PPU pattern tables ($0000-$1FFF)
	IO8(0x2006) = 0x00;
	IO8(0x2006) = 0x00;
	{
		uint8_t saved_bank = mapper_prg_bank;
		bankswitch_prg(BANK_NES_CHR);
		for (uint16_t i = 0; i < 0x2000; i++)
			IO8(0x2007) = chr_nes[i];
		bankswitch_prg(saved_bank);
	}

	// Clear nametable 0 ($2000-$23FF)
	IO8(0x2006) = 0x20;
	IO8(0x2006) = 0x00;
	for (uint16_t i = 0; i < 0x400; i++) IO8(0x2007) = 0;

	// Clear attribute table
	IO8(0x2006) = 0x23;
	IO8(0x2006) = 0xC0;
	for (uint8_t i = 0; i < 64; i++) IO8(0x2007) = 0;

	// Enable NMI + sprites + background
	IO8(0x2000) = lnPPUCTRL;

	lnSync(0);
	reset6502();

	// Disable APU frame IRQ
	IO8(0x4017) = 0x40;

	lnPPUCTRL &= ~0x08;
	lnPPUMASK = 0x3A;

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

	while (1)
	{
#ifdef GAME_IDLE_PC
		if (pc != GAME_IDLE_PC)
#endif
		{
#ifdef INTERPRETER_ONLY
			step6502();
#else
			run_6502();
#endif
		}

		// Detect guest NMI handler completion: RTI restores sp to pre-NMI value
		if (nmi_active && sp == nmi_sp_guard)
			nmi_active = 0;

		// Execute deferred OAM DMA: JIT-compiled STA $4014 set the flag,
		// now write to the real hardware register during the main loop
		// where VBlank timing is appropriate.
		if (oam_dma_request) {
			IO8(0x4014) = oam_dma_request;
			oam_dma_request = 0;
		}

		// NMI-driven frame timing (same approach as exidy.c)
#ifndef TRACK_TICKS
		{
			static uint8_t nmi_stuck_count = 0;
			uint8_t cur_nmi = *(volatile uint8_t*)0x26;
			if (cur_nmi != last_nmi_frame)
			{
				nmi_stuck_count = 0;
				if (!nmi_active)
				{
					// Guest main loop — safe to render and fire NMI
					nes_gamepad_refresh();
#ifdef ENABLE_METRICS
					{ uint8_t _mb = mapper_prg_bank; bankswitch_prg(BANK_RENDER); metrics_dump_runtime_b2(); bankswitch_prg(_mb); }
#endif
					render_video();
					if (PPUCTRL_soft & 0x80)
					{
						nmi_sp_guard = sp;
						nmi6502();
						nmi_active = 1;
					}
				}
				// Always absorb counter changes to prevent re-triggering
				last_nmi_frame = *(volatile uint8_t*)0x26;
			}
			else if (++nmi_stuck_count >= 3)
			{
				// Stuck-frame watchdog: lazynes only increments nmiCounter
				// when lnSync is pending.  If the guest is in a hardware-
				// polling loop (BIT $2002 / BVC for sprite-0-hit, etc.),
				// the batch dispatch loop exhausts without calling lnSync,
				// leaving nmiCounter frozen.  Force a render_video() call
				// — its lnSync re-arms lazynes so nmiCounter advances on
				// the next VBlank, breaking the deadlock.
				nmi_stuck_count = 0;
				render_video();
				cur_nmi = *(volatile uint8_t*)0x26;
				if (cur_nmi != last_nmi_frame)
				{
					if (!nmi_active)
					{
						nes_gamepad_refresh();
						if (PPUCTRL_soft & 0x80)
						{
							nmi_sp_guard = sp;
							nmi6502();
							nmi_active = 1;
						}
					}
					last_nmi_frame = cur_nmi;
				}
			}
		}
#else
		if (clockticks6502 > frame_time)
		{
			frame_time += FRAME_LENGTH;
			nes_gamepad_refresh();
#ifdef ENABLE_METRICS
			{ uint8_t _mb = mapper_prg_bank; bankswitch_prg(BANK_RENDER); metrics_dump_runtime_b2(); bankswitch_prg(_mb); }
#endif
			render_video();
			if (PPUCTRL_soft & 0x80)
				nmi6502();
			last_nmi_frame = *(volatile uint8_t*)0x26;
		}
		// NMI backstop
		{
			uint8_t cur_nmi = *(volatile uint8_t*)0x26;
			if (cur_nmi != last_nmi_frame)
			{
				last_nmi_frame = cur_nmi;
				if (!(status & FLAG_INTERRUPT))
				{
					frame_time = clockticks6502 + FRAME_LENGTH;
					render_video();
					if (PPUCTRL_soft & 0x80)
						nmi6502();
					last_nmi_frame = *(volatile uint8_t*)0x26;
				}
			}
		}
#endif
	}

	return 0;
}


// ******************************************************************************************
// read6502 — NES memory map
// ******************************************************************************************

uint8_t read6502(uint16_t address)
{
#ifdef DEBUG_AUDIO
	audio ^= address >> 8;
	IO8(0x4011) = audio;
#endif

	if (address < 0x2000)
		return RAM_BASE[address & 0x7FF];	// 2KB RAM mirrored

	if (address < 0x4000)
		return IO8(0x2000 + (address & 7));	// PPU registers mirrored

	if (address < 0x4020)
		return IO8(address);	// APU/IO

	if (address >= 0x8000)
	{
		// PRG-ROM: $8000-$FFFF (NROM-128 mirrors $8000-$BFFF = $C000-$FFFF)
		uint8_t saved_bank = mapper_prg_bank;
		bankswitch_prg(BANK_NES_PRG_LO);
		uint8_t temp = ROM_NAME[address & 0x3FFF];
		bankswitch_prg(saved_bank);
		return temp;
	}

	return 0;	// open bus
}


// ******************************************************************************************
// write6502 — NES memory map
// ******************************************************************************************

void write6502(uint16_t address, uint8_t value)
{
	if (address < 0x2000)
	{
		RAM_BASE[address & 0x7FF] = value;
		return;
	}

	if (address < 0x4000)
	{
		nes_register_write(address, value);
		return;
	}

	if (address < 0x4020)
	{
		if (address == 0x4014) {
			oam_dma_request = value;
			return;
		}
		IO8(address) = value;
		return;
	}
}



// ******************************************************************************************
// Gamepad — NES standard controller via lazynes
// Cached once per frame to avoid expensive lnGetPad calls on every $5101 poll.
// ******************************************************************************************

static uint8_t cached_gamepad = 0xFF;  // all buttons released

void nes_gamepad_refresh(void)
{
	uint8_t targ = 0;
	uint8_t joypad = lnGetPad(1);
	targ |= (joypad & lfR) ? TARG_RIGHT : 0;
	targ |= (joypad & lfL) ? TARG_LEFT  : 0;
	targ |= (joypad & lfU) ? TARG_UP    : 0;
	targ |= (joypad & lfD) ? TARG_DOWN  : 0;
	targ |= (joypad & lfA) ? TARG_FIRE  : 0;
	targ |= (joypad & lfSelect) ? TARG_COIN1  : 0;
	targ |= (joypad & lfStart)  ? TARG_1START : 0;
	cached_gamepad = (targ ^ 0xFF);
}

uint8_t nes_gamepad(void)
{
	return cached_gamepad;
}


// ******************************************************************************************
// render_video — flush PPU queue and sync to VBlank
// ******************************************************************************************

// Flush the PPU queue without blocking.  Used when the emulator
// has more guest work to do and doesn't want to waste a full VBlank.
void render_video_noblock(void)
{
	if (ppu_queue_index == 0)
		return;  // nothing to flush
	ppu_queue[ppu_queue_index] = lfEnd;
	lnList(ppu_queue);
	ppu_queue_index = 0;
}

// Flush the PPU queue AND block until VBlank completes.
// Use sparingly — each call costs one real frame (~16.7ms).
void render_video(void)
{
	ppu_queue[ppu_queue_index] = lfEnd;
	lnList(ppu_queue);
	ppu_queue_index = 0;
	lnSync(0);
}


// ******************************************************************************************
// flash_format — erase all flash cache banks
// ******************************************************************************************

void flash_format(void)
{
	for (uint8_t bank = 3; bank < 31; bank++)
	{
		// Skip repurposed banks
		if (bank == BANK_NES_PRG_LO) continue;
		if (bank == BANK_RENDER) continue;
		if (bank == BANK_NES_CHR) continue;
		if (bank == BANK_SA_CODE) continue;
		if (bank == BANK_INIT_CODE) continue;

		for (uint16_t sector = 0x8000; sector < 0xC000; sector += 0x1000)
			flash_sector_erase(sector, bank);
	}
}
