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
#ifdef ENABLE_OPTIMIZER_V2
#include "core/optimizer_v2_simple.h"
#endif
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

// Software VBlank flag: the lazynes NMI handler reads real $2002 on every
// VBlank, clearing the hardware VBlank flag before the guest can see it.
// We set this flag after each render_video() (VBlank sync) and return it
// to the guest on the first $2002 read, then clear it — matching real NES
// behaviour where reading $2002 clears the VBlank bit.
uint8_t guest_vblank_pending = 0;

// Software $2007 (PPUDATA) read buffer.  On real NES hardware, reads from
// $2007 for addresses $0000-$3EFF return the PREVIOUS contents of an
// internal buffer, then load the addressed byte into the buffer.  Palette
// reads ($3F00-$3F1F) return immediately.  We emulate this for CHR-ROM
// ($0000-$1FFF) by reading from the CHR flash bank, and track the read
// buffer + auto-increment in software — the real PPU's address register
// is managed by lazynes and can't be shared with the guest.
uint8_t ppudata_read_buffer = 0;

extern void bankswitch_prg(__reg("a") uint8_t bank);
extern uint8_t mapper_prg_bank;
extern void flash_sector_erase(uint16_t addr, uint8_t bank);
extern void flash_byte_program(uint16_t addr, uint8_t bank, uint8_t data);
extern uint8_t peek_bank_byte(uint8_t bank, uint16_t addr);
extern void flash_cache_init_sectors(void);

// Forward declarations (defined later, in BANK_RENDER segment)
void render_video(void);
void render_video_noblock(void);
void render_video_finish(void);

__zpage uint8_t last_nmi_frame;  // __zpage: ASM dispatch references it directly

// Nested NMI prevention: guest NMI handler runs through the main-loop
// dispatches and may span several real VBlanks.  Without a guard, each
// VBlank would fire nmi6502() again, corrupting the guest stack.
// We detect RTI completion by monitoring the guest stack pointer.
//
// NES_NMI_VBLANK_FLAG games (e.g. Lunar Pool) call their main game
// loop FROM the NMI handler, then spin-wait for the next NMI.  We fire
// selective reentrant NMI: only when the guest has cleared the VBlank
// flag and entered a spin loop (safe to re-enter NMI handler), not
// while the NMI handler's own PPU work is still in progress.
uint8_t nmi_active = 0;
uint8_t nmi_sp_guard;  // sp value just before outermost nmi6502()

// OAM DMA deferred execution: JIT-compiled STA $4014 writes the source
// page here instead of hitting the hardware register directly.  The main
// loop executes the actual DMA during VBlank when timing is correct.
uint8_t oam_dma_request = 0;

// Raw gamepad state (lazynes convention: bit SET = pressed).
// Stored by nes_gamepad_refresh() for use by recompile trigger checks.
static uint8_t cached_raw_pad = 0;

// Recompile trigger state
static uint8_t cache_pressure_frames = 0;

// Assembly routine: JMP ($FFFC) — never returns.
extern void trigger_soft_reset(void);


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
			// Shadow applied by NMI handler at next VBlank.
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
			uint8_t addr_hi = PPUADDR_soft[0] & 0x3F;
			ppu_queue[ppu_queue_index + 0] = addr_hi;
			ppu_queue[ppu_queue_index + 1] = PPUADDR_soft[1];
			ppu_queue[ppu_queue_index + 2] = value;
			ppu_queue_index += 3;

#ifdef NES_MIRROR_VERTICAL
			// Vertical mirroring emulation for four-screen VRAM hardware.
			// The guest expects NT0/NT2 and NT1/NT3 to be mirrors (XOR $0800).
			// Duplicate nametable writes ($2000-$2FFF) to the mirror address.
			if (addr_hi >= 0x20 && addr_hi < 0x30) {
				ppu_queue[ppu_queue_index + 0] = addr_hi ^ 0x08;
				ppu_queue[ppu_queue_index + 1] = PPUADDR_soft[1];
				ppu_queue[ppu_queue_index + 2] = value;
				ppu_queue_index += 3;
			}
#endif
#ifdef NES_MIRROR_HORIZONTAL
			// Horizontal mirroring emulation for four-screen VRAM hardware.
			// The guest expects NT0/NT1 and NT2/NT3 to be mirrors (XOR $0400).
			// Duplicate nametable writes ($2000-$2FFF) to the mirror address.
			if (addr_hi >= 0x20 && addr_hi < 0x30) {
				ppu_queue[ppu_queue_index + 0] = addr_hi ^ 0x04;
				ppu_queue[ppu_queue_index + 1] = PPUADDR_soft[1];
				ppu_queue[ppu_queue_index + 2] = value;
				ppu_queue_index += 3;
			}
#endif

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

			// Flush threshold: keep <=96 to avoid overrunning VBlank.
			// The lazynes NMI handler must process the entire VRU list
			// within ~2270 PPU cycles.  With NES_MIRROR_VERTICAL doubling
			// nametable writes (6 bytes each), 96 bytes ≈ 16 writes —
			// safe.  Higher thresholds (e.g. 240) cause PPU corruption
			// because the NMI handler bleeds into active rendering.
			if (ppu_queue_index >= 96)
				render_video();
			break;
		}
		default:
			break;
	}
}


// ******************************************************************************************
// Recompile signature helpers — warm boot detection and reset triggers
//
// Banked into BANK_RENDER (bank21) to save fixed-bank space.  The
// flash_byte_program / peek_bank_byte routines live in WRAM so they
// are callable from any bank.  trigger_soft_reset is also in WRAM.
// ******************************************************************************************

#pragma section bank21

static void write_recompile_signature_b21(void)
{
	// Erase the 4KB sector containing the signature before programming.
	// RECOMPILE_SIG_ADDRESS ($83C8) falls inside cache_bit_array, which
	// is actively written by the dynamic JIT (clearing bits).  NOR flash
	// byte-program can only clear bits (AND), so writing $55 over a byte
	// that already has bits cleared by cache_bit_array produces garbage
	// instead of $55.  Erasing the sector restores all bits to $FF first.
	// This destroys 4KB of cache_bit_array, but we're about to soft-reset
	// and flash_format will erase the entire sector anyway.
	flash_sector_erase(RECOMPILE_SIG_ADDRESS & 0xF000, BANK_FLASH_BLOCK_FLAGS);
	flash_byte_program(RECOMPILE_SIG_ADDRESS, BANK_FLASH_BLOCK_FLAGS, RECOMPILE_SIG_VALUE);
}

uint8_t check_recompile_signature_b21(void)
{
	uint8_t val = peek_bank_byte(BANK_FLASH_BLOCK_FLAGS, RECOMPILE_SIG_ADDRESS);
	return (val == RECOMPILE_SIG_VALUE);
}

void clear_recompile_signature_b21(void)
{
	flash_byte_program(RECOMPILE_SIG_ADDRESS, BANK_FLASH_BLOCK_FLAGS, 0x00);
}

static void check_recompile_triggers_b21(void)
{
	extern uint16_t sector_free_offset[];
	extern volatile uint8_t sa_compile_completed;

	// --- Cache pressure auto-reset ---
	// Count how many sectors are actually occupied (sector_free_offset > 0).
	// next_free_sector is a wrapping cursor and cannot be used as a
	// high-water mark — it wraps to 0 after reaching FLASH_CACHE_SECTORS.
	//
	// SKIP if SA two-pass compile already ran this boot.  The SA-compiled
	// code covers the bulk of the game; any remaining blocks are compiled
	// dynamically and, once flash is full, interpreted.  Without this
	// guard the system endlessly cycles: SA compile → dynamic fill →
	// cache pressure → soft reset → SA compile → ...
	if (!sa_compile_completed)
	{
		uint8_t occupied = 0;
		for (uint8_t i = 0; i < FLASH_CACHE_SECTORS; i++) {
			if (sector_free_offset[i] > 0)
				occupied++;
		}
		if (occupied >= FLASH_CACHE_PRESSURE_THRESHOLD) {
			cache_pressure_frames++;
			if (cache_pressure_frames >= 8) {  // ~0.13s confirmation
				write_recompile_signature_b21();
				trigger_soft_reset();
				// Never reached
			}
		} else {
			cache_pressure_frames = 0;
		}
	}

	// --- Manual B+Select press (instantaneous) ---
	if ((cached_raw_pad & (lfB | lfSelect)) == (lfB | lfSelect)) {
		write_recompile_signature_b21();
		trigger_soft_reset();
		// Never reached
	}
}

#pragma section default

// Fixed-bank trampoline — called once per frame from the main loop.
static void check_recompile_triggers(void)
{
	uint8_t saved = mapper_prg_bank;
	bankswitch_prg(BANK_RENDER);
	check_recompile_triggers_b21();
	bankswitch_prg(saved);

	// --- Manual B+Start press: exhaustive link resolve ---
	// Runs all 4 phases of opt2_full_link_resolve() to patch every
	// resolvable branch/epilogue/JMP in the flash cache.  Unlike
	// B+Select (full recompile via soft reset), this only resolves
	// links without erasing anything.  Safe to call from the fixed
	// bank — opt2_full_link_resolve handles its own bank switching.
#ifdef ENABLE_PATCHABLE_EPILOGUE
	{
		static uint8_t b_start_held = 0;
		if ((cached_raw_pad & (lfB | lfStart)) == (lfB | lfStart)) {
			if (!b_start_held) {
				b_start_held = 1;
				opt2_full_link_resolve();
			}
		} else {
			b_start_held = 0;
		}
	}
#endif
}


// ******************************************************************************************
// Main
// ******************************************************************************************

int main(void)
{
	lnSync(1);

	// --- Suppress NMI for bulk PPU uploads ---
	// lnPush/lnSync may leave VRU data queued for the next NMI.
	// If NMI fires during the $2007 upload loop below, the handler
	// processes the VRU command (changes $2006 for palette writes),
	// then RTI returns mid-loop with the PPU address register pointing
	// at the wrong address.  This corrupts CHR-RAM, leaving pattern
	// tables blank (all-zero tiles).
	IO8(0x2000) = lnPPUCTRL & 0x7F;  // disable NMI

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

	// Re-enable NMI and push initial palette
	IO8(0x2000) = lnPPUCTRL;
	lnPush(0x3F00, 32, palette);

	lnSync(0);
	reset6502();

	// Disable APU frame IRQ
	IO8(0x4017) = 0x40;

	lnPPUCTRL &= ~0x08;
	lnPPUMASK = 0x3A;

	// --- Cold boot vs warm boot ---
	// Warm boot: recompile signature was written by a reset trigger
	// (cache pressure or Select+Start hold).  SA data from the prior
	// dynamic run is already in flash.  Clear the signature and set
	// sa_do_compile so the SA bitmap is enriched with coverage data.
	//
	// Cold boot: no signature present.  SA walks code from reset/NMI
	// vectors (populating the bitmap and idle detection).
	//
	// In both cases, ENABLE_STATIC_COMPILE causes sa_run() to perform
	// the two-pass static compile after the walk.  Warm boots benefit
	// from richer bitmap data discovered by the dynamic JIT.
	{
		extern uint8_t sa_do_compile;
		uint8_t saved_bank = mapper_prg_bank;
		bankswitch_prg(BANK_RENDER);
		if (check_recompile_signature_b21()) {
			clear_recompile_signature_b21();
			// Warm boot — SA data intact, static compile will run
			sa_do_compile = 1;
		}
		bankswitch_prg(saved_bank);
	}

#ifdef ENABLE_CACHE_PERSIST
	flash_init_persist();
#else
	flash_format();
#endif
	flash_cache_init_sectors();

#ifdef ENABLE_STATIC_ANALYSIS
	sa_run();
#endif

	// Belt-and-suspenders: ensure sa_compile_completed is set even if the
	// write inside sa_run_b2 was optimised away or lost across bank switches.
	// Must happen in the same compilation unit as check_recompile_triggers_b21
	// to guarantee the same symbol address is used for write and read.
#ifdef ENABLE_STATIC_COMPILE
	{
		extern volatile uint8_t sa_compile_completed;
		sa_compile_completed = 1;
	}
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
		// WRAM integrity sentinel: check first byte of dispatch_on_pc.
		// If WRAM code area gets corrupted, halt immediately with diagnostics
		// at $7EF0-$7EF5 so we can identify WHEN it happens.
		{
			extern void dispatch_on_pc(void);
			volatile uint8_t *sentinel = (volatile uint8_t *)&dispatch_on_pc;
			if (*sentinel != 0xA9) {  // LDA #imm opcode
				*(volatile uint8_t *)(0x7EF0) = 0xDE;  // corruption marker
				*(volatile uint8_t *)(0x7EF1) = *sentinel;  // corrupted value
				*(volatile uint8_t *)(0x7EF2) = (uint8_t)(pc);
				*(volatile uint8_t *)(0x7EF3) = (uint8_t)(pc >> 8);
				*(volatile uint8_t *)(0x7EF4) = mapper_prg_bank;
				*(volatile uint8_t *)(0x7EF5) = mapper_chr_bank;
				for(;;);  // halt — check $7EF0-$7EF5 for corruption diagnostics
			}
		}

#ifdef ENABLE_AUTO_IDLE_DETECT
#ifdef NES_NMI_VBLANK_FLAG
		// When the guest VBlank flag is set, the guest MUST run even at
		// the idle PC ($FF5B) so it can read the flag and exit its spin
		// loop.  Without this, setting $01C8=1 has no effect because the
		// idle detection prevents the guest from ever executing.
		if (!sa_is_idle_pc(pc) || RAM_BASE[NES_NMI_VBLANK_FLAG])
#else
		if (!sa_is_idle_pc(pc))
#endif
#elif defined(GAME_IDLE_PC)
		if (pc != GAME_IDLE_PC)
#endif
		{
#ifdef INTERPRETER_ONLY
			step6502();
#else
			run_6502();
#endif
		}

		// Detect guest NMI handler completion: RTI restores sp to pre-NMI value.
		// NES_NMI_VBLANK_FLAG: use a small unsigned window (<8) to catch RTI +
		// subsequent RTS within a single run_6502() batch, without the signed-
		// overflow false-positive that (int8_t)>= has on deep stacks.
#ifdef NES_NMI_VBLANK_FLAG
		if (nmi_active && (uint8_t)(sp - nmi_sp_guard) < 8)
			nmi_active = 0;
#else
		if (nmi_active && sp == nmi_sp_guard)
			nmi_active = 0;
#endif

		// Execute deferred OAM DMA: the guest wrote a source page number
		// via STA $4014, but both the JIT-compiled code and the interpreter
		// read from the PATCHED guest ROM (patch_oam_dma.py already
		// translates the page to account for RAM_BASE relocation).
		// So oam_dma_request already contains the correct real page —
		// just forward it directly to the hardware.
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
#ifdef NES_NMI_VBLANK_FLAG
				// Selective reentrant NMI: always refresh gamepad
				// (game reads it inside NMI handler's JSR $C003).
				cached_raw_pad = lnGetPad(1);
				nes_gamepad_refresh();
				check_recompile_triggers();
#ifdef ENABLE_METRICS
				{ uint8_t _mb = mapper_prg_bank; bankswitch_prg(BANK_METRICS); metrics_dump_runtime_b2(); bankswitch_prg(_mb); }
#endif
				if (ppu_queue_index > 0)
					render_video();
				else
					guest_vblank_pending = 1;
				if (PPUCTRL_soft & 0x80)
				{
					if (!nmi_active)
					{
						// First NMI — fire normally
						nmi_sp_guard = sp;
						nmi6502();
						nmi_active = 1;
					}
					else
					{
						// Guest NMI handler calls the main game loop
						// recursively (JSR $C003).  DO NOT fire reentrant
						// nmi6502() — that creates infinite recursive NMI
						// frames that corrupt the guest stack and variables.
						// Instead, just set the guest's software VBlank
						// flag so the spin loop inside JSR $C003 releases.
						// The existing NMI handler will finish naturally:
						// $C003 returns → NMI restores regs → RTI →
						// sp returns to nmi_sp_guard → nmi_active clears.
						RAM_BASE[NES_NMI_VBLANK_FLAG] = 1;
					}
				}
#else
				if (!nmi_active)
				{
					cached_raw_pad = lnGetPad(1);
					nes_gamepad_refresh();
					check_recompile_triggers();
#ifdef ENABLE_METRICS
					{ uint8_t _mb = mapper_prg_bank; bankswitch_prg(BANK_METRICS); metrics_dump_runtime_b2(); bankswitch_prg(_mb); }
#endif
					// Always flush the PPU queue.  Many games temporarily
					// disable NMI at the start of their NMI handler (STA $2000
					// with bit 7 clear), so checking PPUCTRL_soft here would
					// race with the guest and skip the flush, corrupting
					// nametable/palette data.  When the queue is empty,
					// skip the blocking render_video() — nmiCounter already
					// advanced (that’s how we entered this path), so no NMI
					// sync is needed.  This keeps init fast (no blocking on
					// empty queues) while ensuring gameplay PPU data is
					// always flushed.
					if (ppu_queue_index > 0)
						render_video();
					else
						guest_vblank_pending = 1;
					if (PPUCTRL_soft & 0x80)
					{
						nmi_sp_guard = sp;
						nmi6502();
						nmi_active = 1;
					}
				}
#ifdef ENABLE_AUTO_IDLE_DETECT
				else if (sa_is_idle_pc(pc))
				{
					// NMI handler never returns (e.g. Millipede) but guest
					// is at its idle loop, not mid-$4016 read — safe to
					// read controller for B+Select detection.
					cached_raw_pad = lnGetPad(1);
					check_recompile_triggers();
				}
#elif defined(GAME_IDLE_PC)
				else if (pc == GAME_IDLE_PC)
				{
					cached_raw_pad = lnGetPad(1);
					check_recompile_triggers();
				}
#endif
#endif
				last_nmi_frame = *(volatile uint8_t*)0x26;
				nmi_yield = 0;  // VBlank processed — allow backward branches to loop
			}
			else if (++nmi_stuck_count >= 3)
			{
				// Stuck-frame watchdog: lazynes only increments nmiCounter
				// when lnSync is pending.  If the guest is in a hardware-
				// polling loop (BIT $2002 / BVC for sprite-0-hit, etc.),
				// the batch dispatch loop exhausts without calling lnSync,
				// leaving nmiCounter frozen.  Force a render_video() call
				// — its manual NMI setup re-arms lazynes so nmiCounter
				// advances on the next VBlank, breaking the deadlock.
				nmi_stuck_count = 0;
				render_video();
				render_video_finish();  // watchdog must block — needs nmiCounter to advance
				cur_nmi = *(volatile uint8_t*)0x26;
				if (cur_nmi != last_nmi_frame)
				{
#ifdef NES_NMI_VBLANK_FLAG
					cached_raw_pad = lnGetPad(1);
					nes_gamepad_refresh();
					if (PPUCTRL_soft & 0x80)
					{
						if (!nmi_active)
						{
							nmi_sp_guard = sp;
							nmi6502();
							nmi_active = 1;
						}
						else
						{
							// Same as normal VBlank path: release the
							// guest's spin loop instead of recursive NMI.
							RAM_BASE[NES_NMI_VBLANK_FLAG] = 1;
						}
					}
#else
					if (!nmi_active)
					{
						cached_raw_pad = lnGetPad(1);
						nes_gamepad_refresh();
						if (PPUCTRL_soft & 0x80)
						{
							nmi_sp_guard = sp;
							nmi6502();
							nmi_active = 1;
						}
					}
#ifdef ENABLE_AUTO_IDLE_DETECT
					else if (sa_is_idle_pc(pc))
					{
						cached_raw_pad = lnGetPad(1);
						check_recompile_triggers();
					}
#elif defined(GAME_IDLE_PC)
					else if (pc == GAME_IDLE_PC)
					{
						cached_raw_pad = lnGetPad(1);
						check_recompile_triggers();
					}
#endif
#endif
					last_nmi_frame = cur_nmi;
					nmi_yield = 0;  // VBlank processed (watchdog path)
				}
			}
		}
#else
		if (clockticks6502 > frame_time)
		{
			frame_time += FRAME_LENGTH;
			cached_raw_pad = lnGetPad(1);
			nes_gamepad_refresh();
			check_recompile_triggers();
#ifdef ENABLE_METRICS
			{ uint8_t _mb = mapper_prg_bank; bankswitch_prg(BANK_METRICS); metrics_dump_runtime_b2(); bankswitch_prg(_mb); }
#endif
			render_video();
			render_video_finish();  // TRACK_TICKS needs blocking sync
			if (PPUCTRL_soft & 0x80)
				nmi6502();
			last_nmi_frame = *(volatile uint8_t*)0x26;
			nmi_yield = 0;
		}
		// NMI backstop
		{
			uint8_t cur_nmi = *(volatile uint8_t*)0x26;
			if (cur_nmi != last_nmi_frame)
			{
				last_nmi_frame = cur_nmi;
				nmi_yield = 0;
				if (!(status & FLAG_INTERRUPT))
				{
					frame_time = clockticks6502 + FRAME_LENGTH;
					render_video();
					render_video_finish();  // TRACK_TICKS NMI backstop needs blocking sync
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
	{
		if ((address & 7) == 2)
		{
			PPUADDR_latch = 0;	// Reading $2002 resets the w register
			// Read real hardware for bits 0-6 (sprite overflow, sprite-0-hit, etc.)
			// but substitute the software-tracked VBlank flag for bit 7.
			// The lazynes NMI handler reads $2002 every VBlank, clearing the
			// hardware flag before the guest can see it.  guest_vblank_pending
			// is set by render_video() after each VBlank sync.
			uint8_t val = IO8(0x2002);
			if (guest_vblank_pending)
			{
				val |= 0x80;
				guest_vblank_pending = 0;
			}
			return val;
		}
		if ((address & 7) == 7)
		{
			// $2007 PPUDATA read — emulate in software.
			// The real PPU's address register is managed by lazynes and
			// isn't synced with the guest's $2006 writes.  For CHR-ROM
			// reads ($0000-$1FFF), return data from the flash CHR bank.
			// NES read buffer: non-palette reads return the previous buffer
			// contents, then load the new byte into the buffer.
			uint16_t ppu_addr = ((uint16_t)PPUADDR_soft[0] << 8) | PPUADDR_soft[1];
			uint8_t result;

			if ((ppu_addr & 0x3FFF) < 0x2000)
			{
				// CHR-ROM: read from the CHR flash bank
				result = ppudata_read_buffer;
				uint8_t saved_bank = mapper_prg_bank;
				bankswitch_prg(BANK_NES_CHR);
				ppudata_read_buffer = chr_nes[ppu_addr & 0x1FFF];
				bankswitch_prg(saved_bank);
			}
			else
			{
				// Nametable/palette: return buffered value but do NOT
				// read real $2007 — that auto-increments the PPU's
				// internal address register, which lazynes manages for
				// VRU list processing.  Reading here desyncs the PPU
				// address and corrupts subsequent nametable/palette writes.
				result = ppudata_read_buffer;
				// ppudata_read_buffer = IO8(0x2007);  // DISABLED — causes PPU desync
			}

			// Auto-increment PPUADDR_soft (same logic as write path)
			if (PPUCTRL_soft & 0x4)
			{
				if ((PPUADDR_soft[1] & 0xE0) == 0xE0)
					PPUADDR_soft[0] = (PPUADDR_soft[0] + 1) & 0x3F;
				PPUADDR_soft[1] += 32;
			}
			else
			{
				if (++PPUADDR_soft[1] == 0)
					PPUADDR_soft[0] = (PPUADDR_soft[0] + 1) & 0x3F;
			}

			return result;
		}
		return IO8(0x2000 + (address & 7));	// PPU registers mirrored
	}

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
	// cached_raw_pad must be set by the caller (via lnGetPad(1))
	// before calling this function.  Calling lnGetPad twice per
	// frame returns corrupt data on the second read, causing
	// phantom button presses (e.g. repeated Start in DK).
	uint8_t joypad = cached_raw_pad;
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

// Flush the PPU queue and request VBlank sync — NON-BLOCKING.
//
// Submits the PPU list via lnList() and sets up the NMI handler to
// process it on the next VBlank.  Returns immediately so guest code
// can continue executing while waiting for the NMI.
//
// The previous sync (if any) is completed first with a brief spin.
// Because a full frame of guest code ran since the last render_video(),
// the NMI has almost certainly already fired, so the spin exits in
// 0-1 iterations (vs. the old design that blocked ~29,780 cycles
// every frame).
//
// IMPORTANT: We avoid lnSync(0) entirely.  lnSync's blocking poll loop
// at $C2AA waits for the NMI handler to clear nmiFlags bit 6.  On 2nd+
// calls, lnSync skips the PPUCTRL write to real $2000 — so if bit 7
// (NMI enable) was lost on the hardware, NMI never fires and lnSync
// deadlocks.  Even writing lnPPUCTRL to $2000 beforehand doesn't help
// if lnPPUCTRL itself was corrupted (bit 7 cleared).

// Track pending sync state: the nmiCounter value we're waiting to change.
static uint8_t rv_sync_nmi;
static uint8_t rv_sync_active = 0;

// Complete a previously submitted render_video sync.
// Called at the top of render_video() and from render_video_finish().
static void rv_complete_pending(void)
{
	if (!rv_sync_active)
		return;

	// Brief spin — NMI should have already fired since guest code
	// ran for a full frame between calls.  Timeout is a safety net.
	uint16_t timeout = 0;
	while (*(volatile uint8_t*)0x26 == rv_sync_nmi) {
		if (++timeout > 50000) {
			// Force NMI re-arm and break deadlock
			*(volatile uint8_t*)0x25 &= ~0x44;
			lnPPUCTRL |= 0x80;
			IO8(0x2000) = lnPPUCTRL;
			break;
		}
	}
	rv_sync_active = 0;
}

// Callable from the main loop to ensure previous render completed
// before accessing PPU state.  Lightweight — returns immediately
// if no sync is pending.
void render_video_finish(void)
{
	rv_complete_pending();
}

void render_video(void)
{
	// Finish any previous pending sync first.
	rv_complete_pending();

	ppu_queue[ppu_queue_index] = lfEnd;
	lnList(ppu_queue);
	ppu_queue_index = 0;

	// --- Manual lnSync(0) replacement ---

	// Enable bg+sprite rendering in PPUMASK shadow
	lnPPUMASK |= 0x18;

	// Clear split-mode flag (bit 1) in nmiFlags
	*(volatile uint8_t*)0x25 &= ~0x02;

	// Fill remaining OAM entries with $FF (hide unused sprites)
	{
		uint8_t pos = *(volatile uint8_t*)0x27;  // nmiOamPos
		volatile uint8_t *oam = (volatile uint8_t*)0x0200;
		while (pos != 0) {
			oam[pos] = 0xFF;
			pos += 4;
		}
		*(volatile uint8_t*)0x27 = 0;  // reset nmiOamPos
	}

	// Set sync request: clear bit 5, set bit 6
	{
		uint8_t flags = *(volatile uint8_t*)0x25;
		flags = (flags & ~0x20) | 0x40;
		*(volatile uint8_t*)0x25 = flags;
	}

	// Clear scroll counter
	*(volatile uint8_t*)0x2F = 0;

	// Force NMI enabled on real hardware.
	lnPPUCTRL |= 0x80;
	IO8(0x2000) = lnPPUCTRL;

	// Record the nmiCounter we're waiting to change and return
	// immediately.  The NMI handler will process the list during
	// the next VBlank while guest code continues executing.
	rv_sync_nmi = *(volatile uint8_t*)0x26;
	rv_sync_active = 1;

	// Signal software VBlank immediately — the guest can read $2002
	// and see bit 7 set.  The PPU list will be consumed by the NMI
	// handler before the next render_video() call.
	guest_vblank_pending = 1;
}


// ******************************************************************************************
// flash_format — erase all flash cache banks
//
// Banked into BANK_INIT_CODE (bank19) — only called once at boot.
// flash_sector_erase lives in WRAM so it is callable from any bank.
// ******************************************************************************************

#pragma section bank19

static void flash_format_b19(void)
{
#ifdef ENABLE_STATIC_ANALYSIS
	// Extern refs for SA_SECTOR_FIRST/LAST macros (defined in static_analysis.c)
	extern uint8_t sa_code_bitmap[];
	extern uint8_t sa_subroutine_list[];
#ifdef ENABLE_AUTO_IDLE_DETECT
	extern uint8_t sa_idle_list[];
#endif
#endif

	for (uint8_t bank = 3; bank < 31; bank++)
	{
		// Skip repurposed banks
		if (bank == BANK_COMPILE) continue;
		if (bank == BANK_NES_PRG_LO) continue;
		if (bank == BANK_RENDER) continue;
		if (bank == BANK_METRICS) continue;
		if (bank == BANK_NES_CHR) continue;
		if (bank == BANK_SA_CODE) continue;
		if (bank == BANK_INIT_CODE) continue;
		if (bank == BANK_IR_OPT) continue;   // IR optimizer lives here now

		for (uint16_t sector = 0x8000; sector < 0xC000; sector += 0x1000)
		{
#ifdef ENABLE_STATIC_ANALYSIS
			// Protect the SA persistence region in bank 3 from erasure.
			if (bank == 3 && sector >= SA_SECTOR_FIRST && sector <= SA_SECTOR_LAST)
				continue;
#endif
			flash_sector_erase(sector, bank);
		}
	}
}

#pragma section default

// Fixed-bank trampoline — called once at boot from main().
void flash_format(void)
{
	uint8_t saved = mapper_prg_bank;
	bankswitch_prg(BANK_INIT_CODE);
	flash_format_b19();
	bankswitch_prg(saved);
}
