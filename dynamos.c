#pragma section default

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "config.h"
#include "dynamos.h"
#include "exidy.h"
#include "mapper30.h"
#include "core/optimizer.h"
#include "core/static_analysis.h"
#include "core/metrics.h"
#ifdef ENABLE_OPTIMIZER_V2
#include "core/optimizer_v2_simple.h"
#endif
#ifdef ENABLE_IR
#include "backend/ir.h"
#endif

#ifdef ENABLE_COMPILE_PPU_EFFECT
extern uint8_t lnPPUMASK;  // lazynes shadow for $2001
#endif

// Address mode table - must be in accessible memory during recompilation.
// Stored in WRAM (data section) which is always accessible regardless of bank switching.
#pragma section data
uint8_t addrmodes[256] = {
/*        |  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |  A  |  B  |  C  |  D  |  E  |  F  |     */
/* 0 */     imp, indx,  imp, indx,   zp,   zp,   zp,   zp,  imp,  imm,  acc,  imm, abso, abso, abso, abso, /* 0 */
/* 1 */     rel, indy,  imp, indy,  zpx,  zpx,  zpx,  zpx,  imp, absy,  imp, absy, absx, absx, absx, absx, /* 1 */
/* 2 */    abso, indx,  imp, indx,   zp,   zp,   zp,   zp,  imp,  imm,  acc,  imm, abso, abso, abso, abso, /* 2 */
/* 3 */     rel, indy,  imp, indy,  zpx,  zpx,  zpx,  zpx,  imp, absy,  imp, absy, absx, absx, absx, absx, /* 3 */
/* 4 */     imp, indx,  imp, indx,   zp,   zp,   zp,   zp,  imp,  imm,  acc,  imm, abso, abso, abso, abso, /* 4 */
/* 5 */     rel, indy,  imp, indy,  zpx,  zpx,  zpx,  zpx,  imp, absy,  imp, absy, absx, absx, absx, absx, /* 5 */
/* 6 */     imp, indx,  imp, indx,   zp,   zp,   zp,   zp,  imp,  imm,  acc,  imm,  ind, abso, abso, abso, /* 6 */
/* 7 */     rel, indy,  imp, indy,  zpx,  zpx,  zpx,  zpx,  imp, absy,  imp, absy, absx, absx, absx, absx, /* 7 */
/* 8 */     imm, indx,  imm, indx,   zp,   zp,   zp,   zp,  imp,  imm,  imp,  imm, abso, abso, abso, abso, /* 8 */
/* 9 */     rel, indy,  imp, indy,  zpx,  zpx,  zpy,  zpy,  imp, absy,  imp, absy, absx, absx, absy, absy, /* 9 */
/* A */     imm, indx,  imm, indx,   zp,   zp,   zp,   zp,  imp,  imm,  imp,  imm, abso, abso, abso, abso, /* A */
/* B */     rel, indy,  imp, indy,  zpx,  zpx,  zpy,  zpy,  imp, absy,  imp, absy, absx, absx, absy, absy, /* B */
/* C */     imm, indx,  imm, indx,   zp,   zp,   zp,   zp,  imp,  imm,  imp,  imm, abso, abso, abso, abso, /* C */
/* D */     rel, indy,  imp, indy,  zpx,  zpx,  zpx,  zpx,  imp, absy,  imp, absy, absx, absx, absx, absx, /* D */
/* E */     imm, indx,  imm, indx,   zp,   zp,   zp,   zp,  imp,  imm,  imp,  imm, abso, abso, abso, abso, /* E */
/* F */     rel, indy,  imp, indy,  zpx,  zpx,  zpx,  zpx,  imp, absy,  imp, absy, absx, absx, absx, absx  /* F */
};
#pragma section default

// Address translation - lives in fixed bank ($C000+), callable from bank2.
// Only references BSS globals (RAM_BASE, ROM_NAME, etc.), no bank switching.
uint16_t translate_address(uint16_t src_addr) {
#ifdef PLATFORM_NES
    // NES memory map: $0000-$07FF=RAM, $8000-$FFFF=ROM, rest=I/O
    if (src_addr < 0x0800) {
        return src_addr + (uint16_t)RAM_BASE;
    }
    else if (src_addr < 0x2000) {
        // RAM mirror
        return (src_addr & 0x7FF) + (uint16_t)RAM_BASE;
    }
    else if (src_addr >= 0x4000 && src_addr <= 0x4017) {
        // APU / I/O registers — compile as native hardware access.
        // $4000-$4013 = APU sound regs, $4015 = APU status,
        // $4016-$4017 = controller ports / frame counter.
        // These are real NES hardware addresses — JIT code can
        // STA/LDA them directly with zero interpretation overhead.
        if (src_addr == 0x4014) {
            // OAM DMA trigger — redirect to a RAM flag so the
            // main loop can execute it at the correct VBlank timing.
            extern uint8_t oam_dma_request;
            return (uint16_t)&oam_dma_request;
        }
        return src_addr;  // native hardware address
    }
    else if (src_addr >= 0x8000) {
        // PRG-ROM — offset into ROM_NAME using NROM mirror mask
        uint16_t nes_addr = (src_addr & 0x3FFF) + (uint16_t)ROM_NAME;
        if ((nes_addr >= 0x8000) && (nes_addr < 0xC000))
            return 0;  // conflicts with flash cache — interpret
        return nes_addr;
    }
    return 0;  // PPU ($2000-$3FFF) or unmapped — must interpret
#else
    // Exidy memory map
    uint8_t msb = src_addr >> 8;
    
    if (msb < 0x04) {
        // RAM: $0000-$03FF -> RAM_BASE
        return src_addr + (uint16_t)RAM_BASE;
    }
    else if (msb < 0x40) {
        // ROM: $0400-$3FFF -> ROM_NAME (with offset)
        uint16_t nes_addr = (src_addr - ROM_OFFSET) + (uint16_t)ROM_NAME;
        // If decoded address is in the switchable bank ($8000-$BFFF), it conflicts with flash cache execution.
        // Return 0 to force interpretation so the interpreter can switch banks correctly.
        if ((nes_addr >= 0x8000) && (nes_addr < 0xC000))
            return 0;
        return nes_addr;
    }
    else if (msb < 0x48) {
        // Screen RAM: $4000-$47FF -> SCREEN_RAM_BASE
        return (src_addr - 0x4000) + (uint16_t)SCREEN_RAM_BASE;
    }
    else if (msb < 0x50) {
        // Character RAM: $4800-$4FFF -> CHARACTER_RAM_BASE
        return (src_addr - 0x4800) + (uint16_t)CHARACTER_RAM_BASE;
    }
    
    // I/O or unmapped - must interpret
    return 0;
#endif  // PLATFORM_NES
}
#pragma section default

__zpage extern uint8_t sp;
__zpage extern uint8_t a;
__zpage extern uint8_t x;
__zpage extern uint8_t y;
__zpage extern uint16_t pc;
__zpage uint16_t pc_end;
__zpage extern uint8_t opcode;

static uint16_t rom_remap_index = 0;
__zpage uint32_t cache_hits = 0;
__zpage uint32_t cache_misses = 0;
uint32_t cache_interpret = 0;       // dispatch returned "interpret"
uint32_t cache_branches = 0;
uint32_t branch_not_compiled = 0;   // target not yet compiled
uint32_t branch_wrong_bank = 0;     // target in different flash bank
uint32_t branch_out_of_range = 0;   // native offset > 127 bytes
uint32_t branch_forward = 0;        // forward branch (can't optimize)
uint16_t stats_frame = 0;            // debug stats frame counter
//static uint16_t cache_index = 0;
__zpage uint8_t cache_index = BLOCK_COUNT-1;
//static uint8_t cache_active = 0;

__zpage uint8_t next_new_cache = 0;
__zpage uint8_t matched = 0;
__zpage uint8_t decimal_mode = 0;
__zpage uint8_t block_has_jsr = 0;  // set when JSR/NJSR compiled in current block
static uint8_t block_dirty_screen = 0;  // set after first INC screen_ram_updated in block
static uint8_t block_dirty_char = 0;    // set after first INC character_ram_updated in block
#ifdef ENABLE_PEEPHOLE
volatile uint8_t block_flags_saved = 0;   // peephole: trailing PLP deferred from previous template (volatile: prevent vbcc dead-code elimination)
uint8_t block_has_skip = 0;     // peephole: set when any template in block used skip=1
uint8_t peephole_skipped = 0;   // peephole: set per-instruction when skip=1 was used
#endif
uint8_t recompile_instr_start;           // code_index at instruction start (after any deferred PLP flush)

#ifdef ENABLE_IR
ir_ctx_t ir_ctx;                         // IR compilation context (~480B WRAM)
#endif
#ifdef ENABLE_COMPILE_PPU_EFFECT
__zpage uint8_t compile_ppu_effect = 0;  // PPU emphasis bits toggled during compile
__zpage uint8_t compile_ppu_active = 0;  // 1 = PPU effect enabled (SA boot only)
#endif
__zpage uint16_t flash_cache_index;
uint8_t flash_enabled = 0;

#ifdef ENABLE_IDLE_DETECT
// Idle loop detection state (see config.h ENABLE_IDLE_DETECT).
// idle_anchor = PC of the detected backward-branch target.
// idle_count  = consecutive hits before fast-path activates.
// idle_prev_pc = previous guest PC (for backward-branch detection).
__zpage uint16_t idle_anchor  = 0;
__zpage uint16_t idle_prev_pc = 0;
__zpage uint8_t  idle_count   = 0;
#endif

// Static compilation pass indicator:
//   0 = dynamic (runtime) or pass-1 measure — forward branches use 21-byte patchable template
//   2 = pass-2 emit — forward branches use lookup_entry_list() for direct branches
uint8_t sa_compile_pass = 0;

// Pass 2: forced exit PC for the current block being compiled.
// Set by sa_run before calling sa_compile_one_block in pass 2.
// The compile loop checks pc >= sa_block_exit_pc to force block termination,
// ensuring pass 2's block boundaries match pass 1's even when forward
// branches produce shorter native code.
uint16_t sa_block_exit_pc = 0xFFFF;

// Entry list write cursor (offset within BANK_ENTRY_LIST, in bytes).
// Each entry is 8 bytes: entry_pc(2), exit_pc(2), native_addr(2), bank(1), code_len(1).
// Reset to 0 before pass 1.
uint16_t entry_list_offset = 0;

// Pass 2: allocation size for current block (from entry list code_len).
// sa_compile_one_block calls flash_sector_alloc(sa_block_alloc_size) in pass 2.
uint8_t sa_block_alloc_size = 0;

// Intra-block backward branch map: maps (guest_pc - entry_pc) → code_index+1
// during compilation.  Index by offset & 0x3F (64-entry table).
// Value 0 = no entry at that offset.  Otherwise code_index = value - 1.
uint8_t block_ci_map[64];

// Sector-based free-form allocator:
// Each 4KB flash sector has a bump pointer tracking the next free offset.
// Stored in WRAM (mutable).  Zeroed at boot by flash_cache_init_sectors().
uint16_t sector_free_offset[FLASH_CACHE_SECTORS];
uint8_t next_free_sector = 0;  // monotonic sector cursor

uint8_t l1_cache_code[CACHE_L1_CODE_SIZE];
uint8_t cache_code[BLOCK_COUNT][CACHE_CODE_BUF_SIZE];
uint8_t cache_flag[BLOCK_COUNT];
__zpage uint8_t cache_entry_pc_lo[BLOCK_COUNT];
__zpage uint8_t cache_entry_pc_hi[BLOCK_COUNT];
uint8_t cache_exit_pc_lo[BLOCK_COUNT];
uint8_t cache_exit_pc_hi[BLOCK_COUNT];
// Removed: cache_link, cache_branch_link (RAM cache linking)
uint16_t cache_cycles[BLOCK_COUNT];
uint8_t cache_hit_count[BLOCK_COUNT];
uint8_t cache_branch_pc_lo[BLOCK_COUNT];
uint8_t cache_branch_pc_hi[BLOCK_COUNT];
uint8_t cache_vpc[BLOCK_COUNT];

__zpage uint8_t code_index;
__zpage uint16_t decoded_address;
__zpage uint16_t encoded_address;
__zpage uint8_t address_8;

uint32_t cache_branch_long = 0;
__zpage uint8_t indy_hit_count = 0;  // Debug: count indy case hits

extern	uint8_t flash_block_flags[];

// -------------------------------------------------------------------------
// Debug stats dump — writes key counters to a fixed WRAM region ($7F00)
// so they can be inspected in Mesen's hex editor (WRAM tab, offset $1F00).
//
// Layout at $7F00 (all little-endian):
//   +$00: 4B  cache_hits       (dispatch → ran compiled code)
//   +$04: 4B  cache_misses     (dispatch → needed recompile)
//   +$08: 4B  cache_interpret   (dispatch → interpreted)
//   +$0C: 4B  cache_branches   (branch optimization attempts)
//   +$10: 4B  branch_not_compiled
//   +$14: 4B  branch_wrong_bank
//   +$18: 4B  branch_out_of_range
//   +$1C: 4B  branch_forward
//   +$20: 2B  opt2_total       (V2 branch opportunities)
//   +$22: 2B  opt2_direct      (V2 patches applied)
//   +$24: 2B  opt2_pending     (V2 patches waiting)
//   +$26: 2B  blocks_used      (flash cache blocks in use / 960)
//   +$28: 2B  frame_counter    (incremented each dump)
//   +$2A: 2B  magic ($DB, $57) (signature to confirm stats are live)
//   +$2C: 2B  sa_blocks_total  (blocks compiled by static analysis)
// -------------------------------------------------------------------------
// Address $7E00 chosen: above BSS end (~$7A20) and below C stack ($8000-$200)
#define DEBUG_STATS_ADDR  ((volatile uint8_t*)0x7E00)

// Bank 2 implementation — no bankswitch_prg calls allowed.
// Uses peek_bank_byte (WRAM helper) for cross-bank reads.
#ifdef ENABLE_DEBUG_STATS
#pragma section bank2
static void debug_stats_update_b2(void)
{
	volatile uint8_t *p = DEBUG_STATS_ADDR;

	// 32-bit counters (little-endian) — all in WRAM/ZP, safe from bank 2
	*(volatile uint32_t*)(p + 0x00) = cache_hits;
	*(volatile uint32_t*)(p + 0x04) = cache_misses;
	*(volatile uint32_t*)(p + 0x08) = cache_interpret;
	*(volatile uint32_t*)(p + 0x0C) = cache_branches;
	*(volatile uint32_t*)(p + 0x10) = branch_not_compiled;
	*(volatile uint32_t*)(p + 0x14) = branch_wrong_bank;
	*(volatile uint32_t*)(p + 0x18) = branch_out_of_range;
	*(volatile uint32_t*)(p + 0x1C) = branch_forward;

	// 16-bit opt2 stats — read via peek_bank_byte from bank 1
#ifdef ENABLE_OPTIMIZER_V2
	extern uint8_t peek_bank_byte(uint8_t bank, uint16_t addr);
	extern uint16_t opt2_stat_total;
	extern uint16_t opt2_stat_direct;
	extern uint16_t opt2_stat_pending;
	*(volatile uint16_t*)(p + 0x20) = peek_bank_byte(1, (uint16_t)&opt2_stat_total)
	                                | (peek_bank_byte(1, (uint16_t)&opt2_stat_total + 1) << 8);
	*(volatile uint16_t*)(p + 0x22) = peek_bank_byte(1, (uint16_t)&opt2_stat_direct)
	                                | (peek_bank_byte(1, (uint16_t)&opt2_stat_direct + 1) << 8);
	*(volatile uint16_t*)(p + 0x24) = peek_bank_byte(1, (uint16_t)&opt2_stat_pending)
	                                | (peek_bank_byte(1, (uint16_t)&opt2_stat_pending + 1) << 8);
#endif

	// Blocks in use: report next_free_sector as a proxy for cache utilization
	extern uint8_t next_free_sector;
	*(volatile uint16_t*)(p + 0x26) = next_free_sector;

	// Frame counter (global in BSS/WRAM — static in bank2 flash would be read-only)
	extern uint16_t stats_frame;
	*(volatile uint16_t*)(p + 0x28) = stats_frame++;

	// Magic signature
	p[0x2A] = 0xDB;
	p[0x2B] = 0x57;

	// SA compile counter
#ifdef ENABLE_STATIC_ANALYSIS
	extern uint16_t sa_blocks_total;
	*(volatile uint16_t*)(p + 0x2C) = sa_blocks_total;
#else
	*(volatile uint16_t*)(p + 0x2C) = 0;
#endif
}
#pragma section default

// Fixed-bank trampoline — switches to bank 2, calls b2 impl, restores bank
void debug_stats_update(void)
{
	uint8_t saved_bank = mapper_prg_bank;
	bankswitch_prg(2);
	debug_stats_update_b2();
	bankswitch_prg(saved_bank);
}
#endif // ENABLE_DEBUG_STATS

uint16_t pc_jump_address;
uint8_t pc_jump_bank;
uint16_t pc_jump_flag_address;
uint8_t pc_jump_flag_bank;
uint16_t flash_code_address;
uint8_t flash_code_bank;


// Removed: local addrmodes[] array - now using cpu_6502_addrmodes from frontend/cpu_6502.c

#pragma section bank3
uint8_t cache_bit_array[0x2000];
#pragma section default


//============================================================================================================

/*
run_6502

search cache for entrance matching the current program counter
	if match found, call ready() and return	
	create new cache
		step through each instruction, translating to equivalent 6502 instruction sequence
			decode all data addresses used
				for a specified address, remap it
				for indirect access, insert run-time decoder
		loop until cache block full, unsupported instruction, or branch/jump outside of cache (backwards only)
		try to link new cache with any existing cache's known entries and exits
	

*/

void run_6502(void)
{		
	//cache_test();

#ifdef ENABLE_BLOCK_CYCLES
	// interpret_6502() already adds to clockticks6502 via ticktable.
	// JIT blocks add their pre-computed cycle count in dispatch_on_pc (asm).
#endif
	
#ifdef DEBUG_OUT
	static uint8_t print_delay = 0;
	print_delay++;
	if (print_delay == 0)
	{		
		disassemble();
		//snprintf(debug_out, sizeof(debug_out), "cache miss:%08lX hit:%08lX links:%05lX luse:%05lX drop:%05lX bra:%05lX", cache_misses, cache_hits, cache_links_found, cache_links, cache_links_dropped, cache_branches);
	}
#endif

	if (decimal_mode)
	{
		if (status & FLAG_DECIMAL)			
		{
			bankswitch_prg(0);
			interpret_6502(); //enable_interpret();	
			return;
		}
			
		else
		{
			decimal_mode = 0;
		}
	}
	
#ifdef ENABLE_BATCH_DISPATCH
	// Batch dispatch: loop here until an exit condition is met.
	// Saves one __rsave12/__rload12 pair (~260 NES cycles) per dispatch
	// by amortising the vbcc callee-save cost across all dispatches
	// within a single VBlank period.
	uint8_t batch_nmi = *(volatile uint8_t*)0x26;
	uint8_t result;
	uint8_t batch_count = 0;
	do {
	result = dispatch_on_pc();
#else
	uint8_t result = dispatch_on_pc();
#endif
	switch (result)
	{
		case 2:  // interpret
		{
#ifdef ENABLE_IDLE_DETECT
			// Idle loop detection — ONLY in the interpret path.
			// When a non-JIT'd PC keeps looping back to the same address,
			// batch-interpret a full iteration instead of paying the
			// dispatch round-trip each time.  JIT'd loops (case 0) are
			// already fast and must NOT be caught here.
			if (pc == idle_anchor) {
				if (++idle_count >= IDLE_DETECT_THRESHOLD) {
					cache_interpret++;
					bankswitch_prg(0);
					uint16_t anchor = pc;
					uint8_t steps = 8;
					do {
						interpret_6502();
					} while (pc != anchor && --steps);
					if (pc != anchor) {
						idle_count = 0;
						idle_anchor = 0;
					}
#ifdef TRACK_TICKS
					clockticks6502 += DISPATCH_OVERHEAD;
#endif
					idle_prev_pc = pc;
					return;
				}
			} else if (pc < idle_prev_pc) {
				idle_anchor = pc;
				idle_count = 1;
			}
			idle_prev_pc = pc;
#endif
			cache_interpret++;
			bankswitch_prg(0);
			interpret_6502();
#ifdef TRACK_TICKS
			// Dispatch overhead compensation: each dispatch round-trip costs
			// ~200+ real NES cycles in C/asm overhead (function calls, bank
			// switching, dispatch_on_pc lookup, etc.), but interpret_6502()
			// only executes one guest instruction (~3-7 guest cycles).
			// Adding DISPATCH_OVERHEAD compensates so that busy-wait delay
			// loops (DEX;BNE, DEC zp;BNE, LDA $5101 polling) advance the
			// cycle counter proportionally to wall-clock time elapsed.
			// Without this, tight delay loops run 40-100x slower than on
			// real hardware because each iteration pays the full dispatch
			// round-trip cost but only counts a few guest cycles.
			clockticks6502 += DISPATCH_OVERHEAD;
#endif
#ifdef ENABLE_BATCH_DISPATCH
			break;  // continue batch loop
#else
			return;
#endif
		}
		case 0:  // executed from flash
		{
			cache_hits++;
#ifdef ENABLE_IDLE_DETECT
			// JIT block ran successfully — this PC is compiled.
			// Reset idle state so we never intercept compiled loops.
			idle_count = 0;
#endif
#ifdef TRACK_TICKS
			// JIT blocks already have accurate cycle counts in the header,
			// but the dispatch round-trip overhead (~200 NES cycles) isn't
			// reflected.  Adding DISPATCH_OVERHEAD keeps frame timing
			// proportional to wall time even for short blocks (e.g.
			// DEX;BNE = 4 guest cycles + 80 overhead = 84 per dispatch).
			clockticks6502 += DISPATCH_OVERHEAD;
#endif
#ifdef ENABLE_BATCH_DISPATCH
			break;  // continue batch loop
#else
			return;
#endif
		}
		case 1:  // recompile needed
#ifdef ENABLE_BATCH_DISPATCH
			goto batch_exit;  // must return to compile
#else
			break;
#endif
	}
#ifdef ENABLE_BATCH_DISPATCH
	// Batch exit conditions — check after each dispatch:
	// 1. VBlank occurred — must return for NMI processing
	if (*(volatile uint8_t*)0x26 != batch_nmi) break;
	// 2. Iteration limit — prevents infinite batch when lazynes nmiCounter
	//    is stuck (no pending lnSync).  Guarantees the main loop runs
	//    periodically so its stuck-frame watchdog can re-arm lazynes.
	if (++batch_count >= 64) break;
	// 3. Known idle loop — stop dispatching, let main loop poll VBlank
#ifdef GAME_IDLE_PC
	if (pc == GAME_IDLE_PC) break;
#endif
	// 4. RTI completed — NMI handler finished, return for state update
#ifdef PLATFORM_NES
	if (nmi_active && sp == nmi_sp_guard) break;
#endif
	} while (1);
	return;  // VBlank / idle / RTI exit — return to main loop
batch_exit:  // case 1 (compile needed) jumps here
#endif
	
	// Guard: don't compile addresses outside the ROM range.
	// Transient stack corruption (or IO-space PCs) can produce
	// out-of-range addresses.  Compiling them would read garbage
	// from character/screen RAM and program it into flash, causing
	// hard crashes on subsequent dispatches.  Interpret instead.
	if (pc < ROM_ADDR_MIN || pc > ROM_ADDR_MAX)
	{
		cache_interpret++;
		bankswitch_prg(0);
		interpret_6502();
		return;
	}
	
	// Compile directly to flash
	cache_misses++;

	// Save the entry PC before compilation
	uint16_t entry_pc = pc;
	
	// Store entry PC in global arrays so recompile_opcode_b2 can detect
	// backward branches that target the same block's entry (self-loops).
	cache_entry_pc_lo[0] = (uint8_t)entry_pc;
	cache_entry_pc_hi[0] = (uint8_t)(entry_pc >> 8);
	
	// Use cache index 0 as temporary compilation buffer
	cache_index = 0;
	code_index = 0;		

	cache_flag[0] = 0;
	block_has_jsr = 0;  // reset for new block
	block_dirty_screen = 0;
	block_dirty_char = 0;
#ifdef ENABLE_PEEPHOLE
	block_flags_saved = 0;
	block_has_skip = 0;
#endif

	// Clear intra-block code_index map
	for (uint8_t ci_i = 0; ci_i < 64; ci_i++)
		block_ci_map[ci_i] = 0;

#ifdef ENABLE_BLOCK_CYCLES
	uint16_t block_cycles_acc = 0;  // accumulated 6502 cycles for this block
#endif
	
	// Tentatively allocate space for max-size block.
	// We'll write the actual size to the header after compilation.
	// flash_sector_alloc sets flash_code_bank and flash_code_address (to header start).
	//
	// Save allocator state so we can undo if the block produces no code
	// (first instruction is always-interpreted).  Without this, each
	// interpreted PC wastes 258 bytes of flash on an empty allocation.
	extern uint16_t sector_free_offset[];
	extern uint8_t next_free_sector;
	uint8_t pre_alloc_next_free = next_free_sector;
	
	if (!flash_sector_alloc(CODE_SIZE + EPILOGUE_SIZE + XBANK_EPILOGUE_SIZE))
	{
		// No flash available, fall back to interpreter
		bankswitch_prg(0);
		interpret_6502();
		return;
	}
	
	uint8_t alloc_sector = next_free_sector;
	uint16_t undo_free_offset = flash_code_address & FLASH_SECTOR_MASK;
	
	// flash_code_address now points to header start.
	// Native code starts at flash_code_address + BLOCK_HEADER_SIZE.
	// Set up PC table pointers for the entry PC.
	setup_flash_pc_tables(pc);

#if OPT_BLOCK_METADATA
	// Track branches for metadata
	uint8_t branch_count = 0;
	uint8_t branch_offsets[16];    // Offset within code where branch operand is
	uint16_t branch_targets[16];   // Target PC for each branch
#endif
	
	do
	{
		uint16_t pc_old = pc;
		uint8_t code_index_old = code_index;
		
#ifdef ENABLE_PEEPHOLE
		peephole_skipped = 0;  // reset per-instruction skip flag
#endif
		recompile_opcode();
		
		// Record this instruction's code_index for intra-block branch lookup.
		// recompile_instr_start (set inside recompile_opcode_b2 after any
		// deferred PLP flush) points to instruction code, not PLP byte.
#ifdef ENABLE_PEEPHOLE
		{
			uint8_t ci_slot = (uint8_t)(pc_old - entry_pc) & 0x3F;
			block_ci_map[ci_slot] = recompile_instr_start + 1;
		}
#else
		{
			uint8_t ci_slot = (uint8_t)(pc_old - entry_pc) & 0x3F;
			block_ci_map[ci_slot] = code_index_old + 1;
		}
#endif
		
		uint8_t instr_len = code_index - code_index_old;

#ifdef ENABLE_BLOCK_CYCLES
		// Add this opcode's base cycle count to the block total.
		// Uses the same ticktable as the interpreter.
		// Note: doesn't account for page-crossing penalties (acceptable approx).
		if (instr_len > 0)
			block_cycles_acc += ticktable[read6502(pc_old)];
#endif

#if OPT_BLOCK_METADATA
		// Track branches for metadata
		uint8_t op = read6502(pc_old);
		if ((op==0x10||op==0x30||op==0x50||op==0x70||op==0x90||op==0xB0||op==0xD0||op==0xF0) 
		    && branch_count < 16) {
			int8_t off = read6502(pc_old + 1);
			uint16_t target = pc_old + 2 + off;
			branch_offsets[branch_count] = code_index_old + 1;
			branch_targets[branch_count] = target;
			branch_count++;
		}
		setup_flash_pc_tables(pc_old);
#else
		// Write this instruction's bytes to flash immediately
		setup_flash_pc_tables(pc_old);
#ifdef ENABLE_IR
		// IR mode: defer flash writes — the optimized buffer will be
		// bulk-written after the compile loop.  Per-instruction writes
		// are redundant because the final loop at the bottom writes
		// the entire cache_code[0][] to flash anyway.
		(void)instr_len;  // suppress unused warning in IR mode
#else
		for (uint8_t i = 0; i < instr_len; i++)
		{
			flash_byte_program(flash_code_address + BLOCK_HEADER_SIZE + code_index_old + i, flash_code_bank, cache_code[0][code_index_old + i]);
		}
#endif
#endif
		
#ifdef ENABLE_PEEPHOLE
		// Account for deferred PLP byte (block_flags_saved) that will be
		// flushed before the next instruction or in the epilogue.
		if ((code_index + block_flags_saved) > (CODE_SIZE - 6))
#else
		if (code_index > (CODE_SIZE - 6))
#endif
		{
			cache_flag[0] |= OUT_OF_CACHE;
			cache_flag[0] &= ~READY_FOR_NEXT;
		}
		
		if (cache_flag[0] & INTERPRET_NEXT_INSTRUCTION)
			flash_cache_pc_update(recompile_instr_start, INTERPRETED);
#ifdef ENABLE_PEEPHOLE
		else if (peephole_skipped)
		{
			// Peephole safety: this instruction's leading PHP was skipped
			// (skip=1), so its native code is only valid when entered from
			// the preceding instruction in the block.  Mark it INTERPRETED
			// so that dispatch won't jump directly to this unsafe code.
			flash_cache_pc_update(recompile_instr_start, INTERPRETED);
		}
#endif
		else if (instr_len || pc != pc_old)
		{
#ifdef ENABLE_IR
			// IR mode: only the block entry PC (offset 0) has a valid
			// native address after optimisation.  Mid-block PCs would
			// carry stale pre-IR offsets, so leave them unprogrammed
			// ($FF).  Dispatch will treat them as cache-miss → compile
			// a new block starting at that PC when it is hit.
			//
			// DEFERRED: The PC table update for the entry PC is deferred
			// to AFTER the code bytes are written to flash.  Writing the
			// table entry here (before IR optimisation and the bulk code
			// write) creates a window where the table says "dispatch to
			// $XXXX" but the code at $XXXX isn't in flash yet.  If IR
			// optimisation eliminates all nodes (code_index == 0), the
			// early-return path undoes the flash allocation — but this
			// premature table entry persists, becoming a stale pointer
			// to flash space that will be reused by a future block.
			// See "Deferred IR entry-PC table update" below the code
			// write loop.
			(void)0;  // entry PC table update deferred
#else
			flash_cache_pc_update(recompile_instr_start, RECOMPILED);
#ifdef ENABLE_OPTIMIZER_V2
			// V2: Check if any pending branches/epilogues target this PC
			opt2_notify_block_compiled(pc_old, flash_code_address + recompile_instr_start + BLOCK_PREFIX_SIZE, flash_code_bank);
#endif
#endif  /* ENABLE_IR */
		}
		
	} while (cache_flag[0] & READY_FOR_NEXT);

	if (!code_index)
	{
		// Block produced no native code (first instruction was interpreted).
		// Undo the flash allocation — the empty slot would waste space and
		// the PC flag was already set by flash_cache_pc_update(INTERPRETED)
		// in the compile loop above.
		sector_free_offset[alloc_sector] = undo_free_offset;
		next_free_sector = pre_alloc_next_free;
		// Fall through to interpret_6502 below
		bankswitch_prg(0);
		interpret_6502();
		return;
	}

	{
		// Exit PC is the current pc value (instruction to interpret or continue from)
		uint16_t exit_pc = pc;

#ifdef ENABLE_PEEPHOLE
		// Flush deferred PLP from peephole before epilogue
		// NOTE: volatile pointer cast on cache_code write prevents vbcc DCE.
		// code_index uses plain ++ so compiler tracks the new value.
		if (block_flags_saved) {
#ifdef ENABLE_IR
			// Under IR, add the deferred PLP as an IR node so it's
			// included in the lowered output.  Emitting it directly to
			// cache_code creates a byte the IR doesn't know about,
			// causing lowered_size < code_index and epilogue overlap.
			IR_EMIT(&ir_ctx, IR_PLP, 0x80, 0);
#else
			*(volatile uint8_t *)&cache_code[0][code_index] = 0x28;  // PLP
			code_index++;
#endif
			block_flags_saved = 0;
		}
#endif

#ifdef ENABLE_IR
		// --- IR optimisation pass ---
		// IR nodes were already recorded per-instruction by the bank2
		// wrapper (ir_record_native_b2).  We only need to run the
		// optimiser and lower back to native bytes here.
		//
		// IR optimise lives in bank 0, lower/patches/RMW fusion in bank 1.
		{
			uint8_t ir_saved_bank = mapper_prg_bank;
			uint8_t ir_bytes_before = code_index;
			uint8_t ir_bytes_after_actual = code_index; /* default: no savings */
			uint8_t lowered_size;
			if (!ir_ctx.enabled) {
				/* IR was disabled mid-block (node overflow) — skip
				 * lowering.  cache_code[0] already has correct bytes. */
				lowered_size = 0;
				bankswitch_prg(BANK_EMIT);  /* bank1 needed for patches below */
			} else {
			bankswitch_prg(BANK_IR_OPT);
			ir_optimize(&ir_ctx);
			bankswitch_prg(BANK_EMIT);
			/* Pass 5: RMW fusion — runs as post-pass in bank1 */
			ir_ctx.stat_rmw_fusion = ir_opt_rmw_fusion(&ir_ctx);
			lowered_size = ir_lower(&ir_ctx, cache_code[0], CACHE_CODE_BUF_SIZE);
			}
			/* Update code_index to lowered size before resolving patches,
			 * so ir_resolve_deferred_patches scans the correct range. */
			if (lowered_size) {
				code_index = lowered_size;
				ir_bytes_after_actual = lowered_size;
				/* Safety: pad gap between lowered output and pre-IR end
				 * with NOP bytes.  21-byte patchable templates have an
				 * internal BEQ at +0 with offset 19 targeting +21.
				 * If the optimizer removed bytes, the epilogue (appended
				 * at code_index) could overlap a template's +21 position,
				 * skipping the epilogue's PHP and corrupting the stack.
				 * NOP padding keeps +21 within valid code. */
				while (code_index < ir_bytes_before) {
					cache_code[0][code_index++] = 0xEA;  /* NOP */
				}
			}
			/* Phase B: resolve deferred branch/JMP pending patches with
			 * correct post-lowering flash addresses (while bank1 mapped). */
			ir_resolve_deferred_patches();
			/* Capture per-pass counters while bank 1 is mapped */
			uint8_t ir_pass_rl = ir_ctx.stat_redundant_load;
			uint8_t ir_pass_ds = ir_ctx.stat_dead_store;
			uint8_t ir_pass_dl = ir_ctx.stat_dead_load;
			uint8_t ir_pass_pp = ir_ctx.stat_php_plp;
			uint8_t ir_pass_pr = ir_ctx.stat_pair_rewrite + ir_ctx.stat_rmw_fusion;
			/* Write RMW fusion count to WRAM for metrics_viewer */
			*(volatile uint8_t *)(0x7E86) += ir_ctx.stat_rmw_fusion;
			/* Count dead (killed) nodes */
			uint8_t ir_dead_cnt = 0;
			{ uint8_t k; for (k = 0; k < ir_ctx.node_count; k++) {
				if (ir_ctx.nodes[k].op == 0xFF) ir_dead_cnt++;
			} }
			bankswitch_prg(ir_saved_bank);
			metrics_ir_block(ir_bytes_before, ir_bytes_after_actual);
			metrics_ir_nodes_killed(ir_dead_cnt);
			metrics_ir_pass_results(ir_pass_rl, ir_pass_ds, ir_pass_dl,
			                        ir_pass_pp, ir_pass_pr);
			// If ir_lower returns 0 (error), keep original buffer unchanged
		}

#endif

		// --- Build epilogue into cache_code buffer, then write header + all code to flash ---
		uint8_t epilogue_start = code_index;

#ifdef ENABLE_PATCHABLE_EPILOGUE
		// Patchable epilogue (21 bytes) — built in buffer, written by loop below
		cache_code[0][code_index++] = 0x08;  // PHP
		cache_code[0][code_index++] = 0x18;  // CLC
		cache_code[0][code_index++] = 0x90;  // BCC
		cache_code[0][code_index++] = 4;     // offset → regular path at +8
		cache_code[0][code_index++] = 0x28;  // PLP (fast path)
		cache_code[0][code_index++] = 0x4C;  // JMP (fast path)
		cache_code[0][code_index++] = 0xFF;  // JMP lo (PATCHABLE)
		cache_code[0][code_index++] = 0xFF;  // JMP hi (PATCHABLE)
		cache_code[0][code_index++] = 0x85;  // STA _a (regular path)
		cache_code[0][code_index++] = (uint8_t)((uint16_t)&a);
		cache_code[0][code_index++] = 0xA9;  // LDA #<exit_pc
		cache_code[0][code_index++] = (uint8_t)exit_pc;
		cache_code[0][code_index++] = 0x85;  // STA _pc
		cache_code[0][code_index++] = (uint8_t)((uint16_t)&pc);
		cache_code[0][code_index++] = 0xA9;  // LDA #>exit_pc
		cache_code[0][code_index++] = (uint8_t)(exit_pc >> 8);
		cache_code[0][code_index++] = 0x85;  // STA _pc+1
		cache_code[0][code_index++] = (uint8_t)(((uint16_t)&pc) + 1);
		cache_code[0][code_index++] = 0x4C;  // JMP cross_bank_dispatch
		cache_code[0][code_index++] = (uint8_t)((uint16_t)&cross_bank_dispatch);
		cache_code[0][code_index++] = (uint8_t)(((uint16_t)&cross_bank_dispatch) >> 8);
		// +21: Cross-bank fast-path setup (18 bytes)
		cache_code[0][code_index++] = 0x08;  // PHP (re-save guest flags)
		cache_code[0][code_index++] = 0x85;  // STA _a
		cache_code[0][code_index++] = (uint8_t)((uint16_t)&a);
		cache_code[0][code_index++] = 0xA9;  // LDA #$FF (target addr lo, PATCHABLE)
		cache_code[0][code_index++] = 0xFF;
		cache_code[0][code_index++] = 0x8D;  // STA xbank_addr (abs)
		cache_code[0][code_index++] = (uint8_t)((uint16_t)&xbank_addr);
		cache_code[0][code_index++] = (uint8_t)(((uint16_t)&xbank_addr) >> 8);
		cache_code[0][code_index++] = 0xA9;  // LDA #$FF (target addr hi, PATCHABLE)
		cache_code[0][code_index++] = 0xFF;
		cache_code[0][code_index++] = 0x8D;  // STA xbank_addr+1 (abs)
		cache_code[0][code_index++] = (uint8_t)(((uint16_t)&xbank_addr) + 1);
		cache_code[0][code_index++] = (uint8_t)((((uint16_t)&xbank_addr) + 1) >> 8);
		cache_code[0][code_index++] = 0xA9;  // LDA #$FF (target bank, PATCHABLE)
		cache_code[0][code_index++] = 0xFF;
		cache_code[0][code_index++] = 0x4C;  // JMP xbank_trampoline
		cache_code[0][code_index++] = (uint8_t)((uint16_t)&xbank_trampoline);
		cache_code[0][code_index++] = (uint8_t)(((uint16_t)&xbank_trampoline) >> 8);
#else
		// Standard epilogue (14 bytes) — built in buffer, written by loop below
		cache_code[0][code_index++] = 0x85;  // STA _a
		cache_code[0][code_index++] = (uint8_t)((uint16_t)&a);
		cache_code[0][code_index++] = 0x08;  // PHP
		cache_code[0][code_index++] = 0xA9;  // LDA #<exit_pc
		cache_code[0][code_index++] = (uint8_t)exit_pc;
		cache_code[0][code_index++] = 0x85;  // STA _pc
		cache_code[0][code_index++] = (uint8_t)((uint16_t)&pc);
		cache_code[0][code_index++] = 0xA9;  // LDA #>exit_pc
		cache_code[0][code_index++] = (uint8_t)(exit_pc >> 8);
		cache_code[0][code_index++] = 0x85;  // STA _pc+1
		cache_code[0][code_index++] = (uint8_t)(((uint16_t)&pc) + 1);
		cache_code[0][code_index++] = 0x4C;  // JMP cross_bank_dispatch
		cache_code[0][code_index++] = (uint8_t)((uint16_t)&cross_bank_dispatch);
		cache_code[0][code_index++] = (uint8_t)(((uint16_t)&cross_bank_dispatch) >> 8);
#endif  // ENABLE_PATCHABLE_EPILOGUE

		// --- Write block header to flash ---
		// flash_code_address points to header start; code starts at +BLOCK_HEADER_SIZE
		flash_byte_program(flash_code_address + 0, flash_code_bank, (uint8_t)entry_pc);        // entry_pc lo
		flash_byte_program(flash_code_address + 1, flash_code_bank, (uint8_t)(entry_pc >> 8));  // entry_pc hi
		flash_byte_program(flash_code_address + 2, flash_code_bank, (uint8_t)exit_pc);          // exit_pc lo
		flash_byte_program(flash_code_address + 3, flash_code_bank, (uint8_t)(exit_pc >> 8));   // exit_pc hi
		flash_byte_program(flash_code_address + 4, flash_code_bank, code_index);                // code_len (code+epilogue)
		flash_byte_program(flash_code_address + 5, flash_code_bank, epilogue_start);            // epilogue_offset
#ifdef ENABLE_BLOCK_CYCLES
		// Store pre-computed 6502 cycle count (capped at 255).
		// Read by dispatch_on_pc asm before executing the block.
		flash_byte_program(flash_code_address + 6, flash_code_bank,
			(uint8_t)(block_cycles_acc > 255 ? 255 : block_cycles_acc));  // cycles
#else
		// flash_code_address + 6 = flags (leave 0xFF = erased)
#endif
		// flash_code_address + 7 = reserved — used as block-complete sentinel

		// --- Write code + epilogue to flash ---
		for (uint8_t i = 0; i < code_index; i++) {
			flash_byte_program(flash_code_address + BLOCK_HEADER_SIZE + i, flash_code_bank, cache_code[0][i]);
		}

		// Block-complete sentinel: write $AA to the reserved header byte
		// (offset 7 = dispatch_addr - 1) AFTER all code bytes are in flash.
		// The dispatch asm guard checks this byte before jumping to the
		// block — if it's still $FF (erased), the block is incomplete and
		// dispatch treats it as a cache miss.  This catches:
		//  - Stale PC table entries pointing to erased/reused flash
		//  - Partial flash writes (power loss, programming failure)
		//  - Any future bugs that create premature table entries
		flash_byte_program(flash_code_address + 7, flash_code_bank, BLOCK_SENTINEL);

#ifdef ENABLE_IR
		// --- Deferred IR entry-PC table update ---
		// Now that both the header and code are in flash, it's safe to
		// publish the PC table entry.  Re-set up the table pointers
		// (the compile loop's setup_flash_pc_tables calls for later
		// instructions may have overwritten them) and commit.
		setup_flash_pc_tables(entry_pc);
		flash_cache_pc_update(0, RECOMPILED);
#ifdef ENABLE_OPTIMIZER_V2
		opt2_notify_block_compiled(entry_pc, flash_code_address + BLOCK_PREFIX_SIZE, flash_code_bank);
#endif
#endif  /* ENABLE_IR */

		// Shrink sector allocation to actual size used (avoid wasting space)
		sector_free_offset[next_free_sector] = (flash_code_address & FLASH_SECTOR_MASK) + BLOCK_HEADER_SIZE + code_index;

#ifdef ENABLE_OPTIMIZER
		// Notify optimizer that a new unique block was compiled
		opt_notify_block_compiled();
		
		// Mark the sector as in-use since we just compiled code to it
		opt_mark_sector_in_use(flash_code_bank);
#endif

		// V2 notify is now called per-instruction in the compile loop above
		
#ifdef ENABLE_OPTIMIZER_V2
		// Periodic sweep: after every 8 blocks, resolve pending branch patches
		// and scan flash for patchable epilogues that can now be chained
		static uint8_t blocks_compiled = 0;
		if (++blocks_compiled >= 8) {
			blocks_compiled = 0;
			// Sweep pending branch patches
			opt2_sweep_pending_patches();
#ifdef ENABLE_PATCHABLE_EPILOGUE
			// Scan all flash blocks for epilogues that can now be chained
			opt2_scan_and_patch_epilogues();
#endif
		}
#endif
		
		// Restore PC to entry point and execute from flash
		pc = entry_pc;
		
		result = dispatch_on_pc();
		// dispatch_on_pc should return 0 (executed from flash)
		
		// Optimizer trigger check moved to main loop in exidy.c
		return;
	}
}

//============================================================================================================

#define lookup_pc_jump_flag(address)\
pc_jump_flag_bank = ((address >> 14) + BANK_PC_FLAGS);\
pc_jump_flag_address = (address & FLASH_BANK_MASK);

//============================================================================================================
// ==========================================================================
// flash_cache_search — dead code, removed to save bank2 space.
// ==========================================================================
#if 0
#pragma section bank2

static uint8_t flash_cache_search_b2(uint16_t emulated_pc)
{	
	lookup_pc_jump_flag(emulated_pc);
	uint8_t test = peek_bank_byte(pc_jump_flag_bank, (uint16_t)&flash_cache_pc_flags[pc_jump_flag_address]) & RECOMPILED;
	if (test)
		return 0; // not found
		
	// run native code, return through flash_dispatch_return
	uint32_t full_address = (emulated_pc << 1);
	pc_jump_bank = ((emulated_pc >> 13) + BANK_PC);
	pc_jump_address = (full_address & FLASH_BANK_MASK);
	
	IO8(0x4020) = 0x25;
	uint16_t code_addr = peek_bank_byte(pc_jump_bank, (uint16_t)&flash_cache_pc[pc_jump_address])
	                   | (peek_bank_byte(pc_jump_bank, (uint16_t)&flash_cache_pc[pc_jump_address + 1]) << 8);
	void (*code_ptr)(void) = (void*) code_addr;
	(*code_ptr)();
	//unreachable, returns through flash_dispatch_return
}

#pragma section default

uint8_t flash_cache_search(uint16_t emulated_pc)
{
	uint8_t saved_bank = mapper_prg_bank;
	bankswitch_prg(2);
	uint8_t result = flash_cache_search_b2(emulated_pc);
	bankswitch_prg(saved_bank);
	return result;
}
#endif // dead code

//============================================================================================================

uint8_t flash_sector_alloc(uint8_t total_size)
{
	// total_size = code + epilogue bytes (header added internally)
	uint16_t need = (uint16_t)total_size + BLOCK_HEADER_SIZE;
	
	uint8_t s = next_free_sector;
	uint8_t start_sector = s;
	uint8_t checked_any = 0;
	
	// Loop through all sectors looking for one with enough space.
	// Use start_sector wrap-around detection instead of a separate counter,
	// because vbcc optimizes away unused loop counters causing infinite loops.
	for (;;)
	{
		if (s >= FLASH_CACHE_SECTORS) s = 0;  // wrap without modulo
		if (checked_any && s == start_sector)
			break;  // wrapped all the way around — no space
		checked_any = 1;
		
		uint16_t cur = sector_free_offset[s];
		
		// Align code entry to 16-byte boundary:
		// header goes at (aligned - BLOCK_HEADER_SIZE), code at aligned
		uint16_t code_start = (cur + BLOCK_HEADER_SIZE + BLOCK_ALIGNMENT_MASK) & ~BLOCK_ALIGNMENT_MASK;
		uint16_t header_start = code_start - BLOCK_HEADER_SIZE;
		uint16_t end = code_start + total_size;
		
		if (end > FLASH_ERASE_SECTOR_SIZE)
		{
			s++;
			continue;  // doesn't fit in this sector
		}
		
		// Compute bank and address from sector index
		// Use >>2 and &3 instead of /4 and %4 to avoid 32-bit division
		uint8_t bank = (s >> 2) + BANK_CODE;
		uint16_t sector_base = FLASH_BANK_BASE + ((uint16_t)(s & 3) << 12);
		
		flash_code_bank = bank;
		flash_code_address = sector_base + header_start;  // points to header start
		
		// Advance the free pointer past this allocation
		sector_free_offset[s] = end;
		next_free_sector = s;  // start next search here
		
		return 1;  // success
	}
	return 0;  // all sectors full
}	

//============================================================================================================

// Reserve a flash block for a target PC.  Allocates the block, marks it
// as used in flash_block_flags, and records the mapping so the batch
// compile loop can fulfill it later.
//
// IMPORTANT: Does NOT write PC flags.  The scan loop checks the
// reservation list directly (is_reserved), so PC flags are not needed
// until sa_compile_one_block actually compiles the target.
// This avoids the bug where early PC flags caused the scan to skip
// reserved targets before they were compiled.
//
// Returns the 1-based block number (same as flash_cache_select), or 0
// if the cache is full, reservation table is full, or target is not
// Reservation system removed — two-pass static compilation (sa_compile_pass)
// handles forward branches in the static path, and the dynamic path uses
// 21-byte patchable templates with opt2 post-patching.

// Result globals for lookup_native_addr_safe (and formerly reserve_block_for_pc)
uint16_t reserve_result_addr;    // native entry address of looked-up block
uint8_t  reserve_result_bank;    // flash bank of looked-up block

// Fixed-bank helper: look up native address for a compiled PC.
// Caller (bank2) passes target_pc; on success sets reserve_result_addr
// and reserve_result_bank, returns 1.  Returns 0 if target not compiled
// or on failure.  Safe to call from bank2.
uint8_t lookup_native_addr_safe(uint16_t target_pc)
{
	uint8_t saved_bank = mapper_prg_bank;

	// Read flag
	uint8_t flag_bank = (target_pc >> 14) + BANK_PC_FLAGS;
	bankswitch_prg(flag_bank);
	uint8_t flag = flash_cache_pc_flags[target_pc & FLASH_BANK_MASK];
	if (flag & RECOMPILED) {
		bankswitch_prg(saved_bank);
		return 0;  // not compiled
	}
	reserve_result_bank = flag & 0x1F;

	// Read native address
	uint16_t pa = (target_pc << 1) & FLASH_BANK_MASK;
	uint8_t pc_bank = (target_pc >> 13) + BANK_PC;
	bankswitch_prg(pc_bank);
	uint16_t na = flash_cache_pc[pa] | ((uint16_t)flash_cache_pc[pa + 1] << 8);
	reserve_result_addr = na;

	bankswitch_prg(saved_bank);
	return 1;
}

// Fixed-bank helper: look up a block entry point in the two-pass entry list.
// The entry list (in BANK_ENTRY_LIST) is a sequential table written during
// pass 1 with 8-byte entries:
//   byte 0-1: entry_pc (little-endian)
//   byte 2-3: exit_pc  (little-endian)
//   byte 4-5: native_addr (= flash_code_address + BLOCK_PREFIX_SIZE)
//   byte 6:   code_bank
//   byte 7:   code_len (total bytes: code + epilogue)
// Only finds block ENTRY PCs (code_index=0).  Returns 1 on hit (sets
// reserve_result_addr and reserve_result_bank), 0 on miss.
// Safe to call from bank2.
uint8_t lookup_entry_list(uint16_t target_pc)
{
	uint8_t saved_bank = mapper_prg_bank;
	bankswitch_prg(BANK_ENTRY_LIST);

	uint8_t target_lo = (uint8_t)target_pc;
	uint8_t target_hi = (uint8_t)(target_pc >> 8);
	uint8_t *base = (uint8_t *)FLASH_BANK_BASE;

	for (uint16_t i = 0; i < entry_list_offset; i += 8)
	{
		if (base[i] == 0xFF && base[i + 1] == 0xFF)
			break;  // end sentinel
		if (base[i] == target_lo && base[i + 1] == target_hi)
		{
			reserve_result_addr = base[i + 4] | ((uint16_t)base[i + 5] << 8);
			reserve_result_bank = base[i + 6];
			bankswitch_prg(saved_bank);
			return 1;
		}
	}

	bankswitch_prg(saved_bank);
	return 0;
}

//============================================================================================================

void flash_cache_pc_update(uint8_t code_address, uint8_t flags)
{
	// Code starts at flash_code_address + BLOCK_PREFIX_SIZE (after header)
	uint16_t native_addr = flash_code_address + code_address + BLOCK_PREFIX_SIZE;
	
	// Guard against flash AND corruption: flash can only clear bits (1→0).
	// If this PC slot was already programmed (flag != $FF), reprogramming
	// would AND the old and new values, corrupting both the native address
	// and the bank/flag byte.  This happens when the same emulated PC
	// appears in two different compiled blocks (e.g. a conditional branch
	// merges into a shared instruction, or the same PC is first INTERPRETED
	// then RECOMPILED in a later block).  Skip the update — the existing
	// entry is still valid.
	uint8_t saved_bank = mapper_prg_bank;
	bankswitch_prg(pc_jump_flag_bank);
	uint8_t current_flag = flash_cache_pc_flags[pc_jump_flag_address];
	bankswitch_prg(saved_bank);
	if (current_flag != 0xFF)
		return;  // slot already programmed — don't AND-corrupt it
	
	flash_byte_program((uint16_t) &flash_cache_pc[0] + pc_jump_address + 0, pc_jump_bank, (uint8_t)native_addr);
	flash_byte_program((uint16_t) &flash_cache_pc[0] + pc_jump_address + 1, pc_jump_bank, (uint8_t)(native_addr >> 8));			
	
	uint8_t flag_byte;
	if (flags == RECOMPILED)
		flag_byte = flash_code_bank;  // Bit 7 clear = execute compiled code from flash
	else  // INTERPRETED - this instruction needs to be interpreted
		flag_byte = RECOMPILED;   // Bit 7 set, bit 6 clear = dispatch returns 2 (interpret)
	
	flash_byte_program((uint16_t) &flash_cache_pc_flags[0] + pc_jump_flag_address, pc_jump_flag_bank, flag_byte);
}

//============================================================================================================

void setup_flash_pc_tables(uint16_t emulated_pc)
{
	uint32_t full_address;
	full_address = (emulated_pc << 1);
	pc_jump_bank = ((emulated_pc >> 13) + BANK_PC);
	pc_jump_address = (full_address & FLASH_BANK_MASK);

	lookup_pc_jump_flag(emulated_pc);
}

void setup_flash_address(uint16_t emulated_pc, uint16_t block_number)
{
	// Legacy: block_number is no longer used for flash address computation.
	// flash_code_bank and flash_code_address are set by flash_sector_alloc().
	// This function now only sets up PC table pointers.
	if (block_number) {}  // suppress unused-parameter warning
	setup_flash_pc_tables(emulated_pc);
}

//============================================================================================================

void flash_cache_init_sectors(void)
{
	// Zero the free-pointer table
	for (uint8_t i = 0; i < FLASH_CACHE_SECTORS; i++)
		sector_free_offset[i] = 0;
	next_free_sector = 0;
}

//============================================================================================================

#ifdef ENABLE_OPTIMIZER_V2
// Invert branch conditions for the v2 optimizer pattern
// BPL(10)->BMI(30), BMI(30)->BPL(10), BVC(50)->BVS(70), BVS(70)->BVC(50)
// BCC(90)->BCS(B0), BCS(B0)->BCC(90), BNE(D0)->BEQ(F0), BEQ(F0)->BNE(D0)
static uint8_t invert_branch(uint8_t opcode) {
    return opcode ^ 0x20;  // XOR with 0x20 inverts branch condition
}
#endif

//============================================================================================================
// Intra-block backward branch helper (fixed bank — callable from bank2).
// If target_pc is within the current block being compiled and conditions
// are met, emits a native 2-byte backward branch and updates state.
// Returns 1 if native branch was emitted, 0 if caller should fall through.
//============================================================================================================
uint8_t try_intra_block_branch(uint16_t target_pc, uint8_t branch_opcode)
{
	if (block_has_jsr)
		return 0;
	
#ifdef ENABLE_PEEPHOLE
	// Peephole safety: if any instruction in this block used skip=1
	// (leading PHP elided), a backward branch to that code would enter
	// without the expected PHP on the NES HW stack, corrupting returns.
	if (block_has_skip)
		return 0;
#endif
	
	uint16_t block_entry = (uint16_t)cache_entry_pc_lo[cache_index]
	                     | ((uint16_t)cache_entry_pc_hi[cache_index] << 8);
	
	if (target_pc < block_entry || target_pc >= pc)
		return 0;
	
	// Look up the target's code_index from the in-block map.
	// The map is populated during compilation (indexed by guest-PC offset & 0x3F).
	// Value 0 = no entry (hash collision or out-of-range), otherwise code_index+1.
	uint8_t ci_slot = (uint8_t)(target_pc - block_entry) & 0x3F;
	uint8_t ci_val = block_ci_map[ci_slot];
	if (ci_val == 0)
		return 0;  // no entry — can't resolve target's code_index
	uint8_t target_code_index = ci_val - 1;
	
	// Compute native branch offset
	int16_t offset16 = (int16_t)target_code_index - (int16_t)(code_index + 2);
	
	if (offset16 < -128 || offset16 > 127)
		return 0;
	
	// Emit native backward branch
	cache_code[cache_index][code_index+0] = branch_opcode;
	cache_code[cache_index][code_index+1] = (uint8_t)(int8_t)offset16;
	
	setup_flash_address(pc, flash_cache_index);
	flash_cache_pc_update(code_index, RECOMPILED);
	
	pc += 2;
	code_index += 2;
	cache_branches++;
	cache_flag[cache_index] |= READY_FOR_NEXT;
	return 1;
}

//============================================================================================================
// Pass-2 forward/backward branch helper — moved to bank2 (compile-time only).
// For static pass 2 only: looks up target_pc in the entry list and emits
// a 2-byte native branch or 5-byte Bxx_inv+JMP if same-bank.
// Returns the emit length (2 or 5) on success, 0 if unable to resolve.
// The caller provides the branch opcode and code buffer pointer.
//============================================================================================================
#pragma section bank2
uint8_t try_direct_branch(uint16_t target_pc, uint8_t branch_opcode, uint8_t *code_ptr)
{
	if (!lookup_entry_list(target_pc))
		return 0;
	if (reserve_result_bank != flash_code_bank)
		return 0;

	int16_t native_offset = (int16_t)reserve_result_addr
	    - (int16_t)(flash_code_address + BLOCK_PREFIX_SIZE + code_index + 2);

	if (native_offset >= -128 && native_offset <= 127 &&
	    (code_index + 2 + EPILOGUE_SIZE + 6) < CODE_SIZE)
	{
		code_ptr[code_index+0] = branch_opcode;
		code_ptr[code_index+1] = (uint8_t)(int8_t)native_offset;
		return 2;
	}
	if ((code_index + 5 + EPILOGUE_SIZE + 6) < CODE_SIZE)
	{
		code_ptr[code_index+0] = branch_opcode ^ 0x20;  // invert
		code_ptr[code_index+1] = 3;
		code_ptr[code_index+2] = 0x4C;
		code_ptr[code_index+3] = reserve_result_addr & 0xFF;
		code_ptr[code_index+4] = (reserve_result_addr >> 8) & 0xFF;
		return 5;
	}
	return 0;
}

// ==========================================================================
// recompile_opcode — moved to bank 2 to save ~6KB of fixed-bank space.
// This is the single largest consumer (37% of fixed bank).
// Performance of the recompiler itself doesn't matter — only the output.
//
// BANK 2 SAFETY RULES:
//   - NEVER call bankswitch_prg() directly (would unmap self)
//   - Use peek_bank_byte() for cross-bank reads (WRAM helper)
//   - flash_byte_program() is safe (WRAM, restores mapper_prg_bank)
//   - Fixed-bank functions ($C000+) are always reachable
//   - opt2_record_pending_branch_safe() is a fixed-bank trampoline

#ifdef ENABLE_POINTER_SWIZZLE
// Mirror helpers — now in bank2 (compile-time only, called from recompile_opcode_b2).

const mirrored_ptr_t *find_zp_mirror(uint8_t guest_zp)
{
	for (uint8_t i = 0; i < ZP_MIRROR_COUNT; i++) {
		if (mirrored_ptrs[i].guest_lo == guest_zp ||
		    mirrored_ptrs[i].guest_hi == guest_zp)
			return &mirrored_ptrs[i];
	}
	return 0;
}

const mirrored_ptr_t *find_zp_mirror_lo(uint8_t guest_zp)
{
	for (uint8_t i = 0; i < ZP_MIRROR_COUNT; i++) {
		if (mirrored_ptrs[i].guest_lo == guest_zp)
			return &mirrored_ptrs[i];
	}
	return 0;
}

// emit_zp_mirror_write — bank2 helper called from recompile_opcode_b2.
// If zp_addr is a tracked pointer byte, emits a 2-byte ZP opcode
// (same opcode, mirror slot) to keep the NES mirror in sync.
// The mirror always stays in Exidy address space; translation to
// NES-space is deferred to the STA (zp),Y emit path.
// Returns bytes emitted (0 or 2).
uint8_t emit_zp_mirror_write(uint8_t *code_ptr, uint8_t ci, uint8_t opcode, uint8_t zp_addr)
{
	const mirrored_ptr_t *mp = find_zp_mirror(zp_addr);
	if (!mp) return 0;
	uint8_t mirror_zp = (zp_addr == mp->guest_lo) ? mp->nes_zp : (mp->nes_zp + 1);
	code_ptr[ci]   = opcode;
	code_ptr[ci+1] = mirror_zp;
	return 2;
}

// emit_native_sta_indy — bank2 helper called from recompile_opcode_b2.
// Copies the native_sta_indy_tmpl template, patching the emulated
// RAM addresses.  The template reads the pointer from emulated RAM
// (the ground truth) and translates the hi byte via the
// address_decoding_table at runtime — correct for ANY Exidy address
// (screen pointers, RAM offsets, ROM addresses).
// Returns bytes emitted, or 0 if zp_addr is not a mirrored pointer.
uint8_t emit_native_sta_indy(uint8_t *code_ptr, uint8_t ci,
                             uint8_t zp_addr, uint8_t scrn_zp, uint8_t char_zp)
{
	extern uint8_t native_sta_indy_tmpl[];
	extern uint8_t native_sta_indy_tmpl_size;
	extern uint16_t native_sta_indy_emu_lo;
	extern uint16_t native_sta_indy_emu_hi;

	const mirrored_ptr_t *mp = find_zp_mirror_lo(zp_addr);
	if (!mp) return 0;

	// Patch template with emulated RAM addresses for this pointer
	native_sta_indy_emu_lo = (uint16_t)mp->guest_lo + (uint16_t)&RAM_BASE[0];
	native_sta_indy_emu_hi = (uint16_t)mp->guest_hi + (uint16_t)&RAM_BASE[0];
	uint8_t n = native_sta_indy_tmpl_size;
	for (uint8_t i = 0; i < n; i++)
		code_ptr[ci + i] = native_sta_indy_tmpl[i];

	if (mp->side_effect) {
		code_ptr[ci + n++] = 0x08;       // PHP
		code_ptr[ci + n++] = 0xE6;       // INC zp
		code_ptr[ci + n++] = (mp->side_effect == 1) ? scrn_zp : char_zp;
		code_ptr[ci + n++] = 0x28;       // PLP
	}
	return n;
}

// ZP mirror slots defined in dynamos-asm.s (linker assigns ZP addresses)
__zpage extern uint8_t zp_mirror_0[2];
__zpage extern uint8_t zp_mirror_1[2];
__zpage extern uint8_t zp_mirror_2[2];

// Table initializer uses linker-assigned ZP addresses for nes_zp field.
// Can't use static initializer with address-of in vbcc, so we initialize
// at first use.  The mirrored_ptrs array is writable (not const).
mirrored_ptr_t mirrored_ptrs[ZP_MIRROR_COUNT];

static uint8_t zp_mirror_initialized = 0;
void init_zp_mirror_table(void)
{
	if (zp_mirror_initialized) return;
	mirrored_ptr_t init_table[ZP_MIRROR_COUNT] = { ZP_MIRROR_TABLE };
	uint8_t zp_addrs[ZP_MIRROR_COUNT];
	zp_addrs[0] = (uint8_t)((uint16_t)&zp_mirror_0);
	zp_addrs[1] = (uint8_t)((uint16_t)&zp_mirror_1);
	zp_addrs[2] = (uint8_t)((uint16_t)&zp_mirror_2);

	for (uint8_t i = 0; i < ZP_MIRROR_COUNT; i++) {
		mirrored_ptrs[i] = init_table[i];
		mirrored_ptrs[i].nes_zp = zp_addrs[i];
	}

	zp_mirror_initialized = 1;
}

// (bank2 continues — no section switch needed)
#endif

// emit_dirty_flag — bank2 helper (compile-time only).
// Emits PHP / INC screen_ram_updated or character_ram_updated / PLP.
// Returns bytes emitted (4) or 0 if not a screen/char store.
uint8_t emit_dirty_flag(uint8_t *code_ptr, uint8_t ci,
                        uint8_t opcode, uint8_t msb,
                        uint8_t scrn_zp, uint8_t char_zp)
{
	// Only for store opcodes to $40xx-$4Fxx
	if ((opcode == 0x8D || opcode == 0x8E || opcode == 0x8C ||
	     opcode == 0x9D || opcode == 0x99) &&
	    msb >= 0x40 && msb < 0x50)
	{
		code_ptr[ci]   = 0x08;  // PHP
		code_ptr[ci+1] = 0xE6;  // INC zp
		code_ptr[ci+2] = (msb < 0x48) ? scrn_zp : char_zp;
		code_ptr[ci+3] = 0x28;  // PLP
		return 4;
	}
	return 0;
}

// ==========================================================================
// (already in bank2 — section continues from try_direct_branch/emit_dirty_flag)

// Shared helper: emit a fixed-size opcode template (PHA/PLA/PHP/PLP).
// Peephole: if previous template deferred its trailing PLP and this
// template starts with PHP, skip the redundant PLP/PHP pair.
// If this template ends with PLP (and started with PHP), defer the
// trailing PLP for the next instruction to potentially elide.
uint8_t emit_template(uint8_t *tmpl, uint8_t sz)
{
#ifdef ENABLE_PEEPHOLE
	// Peephole: determine skip/trim
	uint8_t skip = 0;  // bytes to skip at start (leading PHP)
	uint8_t trim = 0;  // bytes to trim at end (trailing PLP)

#ifdef ENABLE_PEEPHOLE_SKIP
	// Skip: if previous template deferred its PLP (block_flags_saved)
	// and this template starts with PHP, elide the redundant PLP/PHP pair.
	// Limit to PHA/PLA-sized templates for safety (PHP template has inner
	// PHP/PLA pairs whose stack balance depends on the outer PHP).
	if (block_flags_saved && tmpl[0] == 0x08 && sz == opcode_6502_pha_size) {
		skip = 1;
		block_flags_saved = 0;  // cancel deferred PLP (elided)
		peephole_skipped = 1;   // mark: leading PHP was skipped
		block_has_skip = 1;     // block contains skip instructions
		metrics_peephole_remove(1, 1);
	} else if (block_flags_saved) {
		// Deferred PLP but template can't elide — flush now.
		*(volatile uint8_t *)&cache_code[cache_index][code_index] = 0x28;  // PLP
		code_index++;
		block_flags_saved = 0;
	}
#endif

	// Only trim PHA (sz=13) and PLA (sz=13) templates — NOT PHP (sz=15).
	// PHP's trailing PLP restores guest P from an inner PHA/PHP pair;
	// deferring it would leave guest flags corrupted.
	if (tmpl[0] == 0x08 && tmpl[sz - 1] == 0x28 && sz == opcode_6502_pha_size) {
		// Template starts with PHP and ends with PLP (PHA/PLA):
		// defer the trailing PLP for the compile loop to flush.
#ifdef ENABLE_PEEPHOLE_TRIM
		trim = 1;
#endif
	}
	uint8_t emit_sz = sz - skip - trim;
#else
	uint8_t skip = 0;
	uint8_t emit_sz = sz;
#endif
	// Size check: use sz-skip (not emit_sz) so the deferred PLP byte
	// from trim is still counted against the space budget.
	if ((code_index + sz - skip + 3) < CODE_SIZE) {
		uint8_t *dst = &cache_code[cache_index][code_index];
		for (uint8_t i = 0; i < emit_sz; i++)
			dst[i] = tmpl[skip + i];
		pc += 1;
		code_index += emit_sz;
#ifdef ENABLE_PEEPHOLE
		block_flags_saved = trim;  // 1 = PLP deferred, 0 = not
#endif
		cache_flag[cache_index] |= READY_FOR_NEXT;
		return cache_flag[cache_index];
	}
#ifdef ENABLE_PEEPHOLE
	// Out of space — flush deferred PLP if any before bailing
	// NOTE: volatile pointer cast on cache_code write prevents vbcc DCE.
	// code_index uses plain ++ so compiler tracks the new value.
	if (block_flags_saved) {
		*(volatile uint8_t *)&cache_code[cache_index][code_index] = 0x28;  // PLP
		code_index++;
		block_flags_saved = 0;
	}
#endif
	cache_flag[cache_index] |= (OUT_OF_CACHE | INTERPRET_NEXT_INSTRUCTION);
	cache_flag[cache_index] &= ~READY_FOR_NEXT;
	return cache_flag[cache_index];
}

/* ==========================================================================
 * IR Phase A: lookup tables and per-instruction IR recorder (bank 2).
 *
 * These replicate the native_to_ir / native_instr_size tables that
 * previously lived in bank 1 (rodata1) so we can emit IR nodes from
 * bank 2 without cross-bank calls.  ir_record_native_b2() is a lean
 * version of the old ir_record_from_buffer(): it walks a short byte
 * buffer (one instruction) and emits the corresponding IR node(s).
 * ========================================================================== */
#ifdef ENABLE_IR

#pragma section rodata2

/* Native 6502 opcode → IR opcode (0 = no mapping, emit as raw bytes) */
static const uint8_t native_to_ir_b2[256] = {
/*        0          1          2          3          4          5          6          7  */
/* 0 */   IR_BRK,    0,         0,         0,         0,         IR_ORA_ZP, IR_ASL_ZP, 0,
/* 0 */   IR_PHP,    IR_ORA_IMM,IR_ASL_A,  0,         0,         IR_ORA_ABS,IR_ASL_ABS,0,
/* 1 */   IR_BPL,    0,         0,         0,         0,         0,         0,         0,
/* 1 */   IR_CLC,    IR_ORA_ABSY,0,        0,         0,         IR_ORA_ABSX,IR_ASL_ABSX,0,
/* 2 */   IR_JSR,    0,         0,         0,         IR_BIT_ZP, IR_AND_ZP, IR_ROL_ZP, 0,
/* 2 */   IR_PLP,    IR_AND_IMM,IR_ROL_A,  0,         IR_BIT_ABS,IR_AND_ABS,IR_ROL_ABS,0,
/* 3 */   IR_BMI,    0,         0,         0,         0,         0,         0,         0,
/* 3 */   IR_SEC,    IR_AND_ABSY,0,        0,         0,         IR_AND_ABSX,IR_ROL_ABSX,0,
/* 4 */   0,         0,         0,         0,         0,         IR_EOR_ZP, IR_LSR_ZP, 0,
/* 4 */   IR_PHA,    IR_EOR_IMM,IR_LSR_A,  0,         IR_JMP_ABS,IR_EOR_ABS,IR_LSR_ABS,0,
/* 5 */   IR_BVC,    0,         0,         0,         0,         0,         0,         0,
/* 5 */   IR_CLI,    IR_EOR_ABSY,0,        0,         0,         IR_EOR_ABSX,IR_LSR_ABSX,0,
/* 6 */   IR_RTS,    0,         0,         0,         0,         IR_ADC_ZP, IR_ROR_ZP, 0,
/* 6 */   IR_PLA,    IR_ADC_IMM,IR_ROR_A,  0,         0,         IR_ADC_ABS,IR_ROR_ABS,0,
/* 7 */   IR_BVS,    0,         0,         0,         0,         0,         0,         0,
/* 7 */   IR_SEI,    IR_ADC_ABSY,0,        0,         0,         IR_ADC_ABSX,IR_ROR_ABSX,0,
/* 8 */   0,         0,         0,         0,         IR_STY_ZP, IR_STA_ZP, IR_STX_ZP, 0,
/* 8 */   IR_DEY,    0,         IR_TXA,    0,         IR_STY_ABS,IR_STA_ABS,IR_STX_ABS,0,
/* 9 */   IR_BCC,    0,         0,         0,         0,         0,         0,         0,
/* 9 */   IR_TYA,    IR_STA_ABSY,IR_TXS,   0,         0,         IR_STA_ABSX,0,        0,
/* A */   IR_LDY_IMM,0,         IR_LDX_IMM,0,         IR_LDY_ZP, IR_LDA_ZP, IR_LDX_ZP, 0,
/* A */   IR_TAY,    IR_LDA_IMM,IR_TAX,    0,         IR_LDY_ABS,IR_LDA_ABS,IR_LDX_ABS,0,
/* B */   IR_BCS,    0,         0,         0,         IR_LDY_ABSX,0,         0,         0,
/* B */   IR_CLV,    IR_LDA_ABSY,IR_TSX,   0,         IR_LDY_ABSX,IR_LDA_ABSX,IR_LDX_ABSY,0,
/* C */   IR_CPY_IMM,0,         0,         0,         IR_CPY_ZP, IR_CMP_ZP, IR_DEC_ZP, 0,
/* C */   IR_INY,    IR_CMP_IMM,IR_DEX,    0,         IR_CPY_ABS,IR_CMP_ABS,IR_DEC_ABS,0,
/* D */   IR_BNE,    0,         0,         0,         0,         0,         0,         0,
/* D */   IR_CLD,    IR_CMP_ABSY,0,        0,         0,         IR_CMP_ABSX,IR_DEC_ABSX,0,
/* E */   IR_CPX_IMM,0,         0,         0,         IR_CPX_ZP, IR_SBC_ZP, IR_INC_ZP, 0,
/* E */   IR_INX,    IR_SBC_IMM,IR_NOP,    0,         IR_CPX_ABS,IR_SBC_ABS,IR_INC_ABS,0,
/* F */   IR_BEQ,    0,         0,         0,         0,         0,         0,         0,
/* F */   IR_SED,    IR_SBC_ABSY,0,        0,         0,         IR_SBC_ABSX,IR_INC_ABSX,0,
};

/* Instruction size by native 6502 opcode (1/2/3 bytes, 0 = unknown) */
static const uint8_t native_instr_size_b2[256] = {
/*        0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F */
/* 0 */   1, 2, 0, 0, 0, 2, 2, 0, 1, 2, 1, 0, 0, 3, 3, 0,
/* 1 */   2, 2, 0, 0, 0, 2, 2, 0, 1, 3, 0, 0, 0, 3, 3, 0,
/* 2 */   3, 2, 0, 0, 2, 2, 2, 0, 1, 2, 1, 0, 3, 3, 3, 0,
/* 3 */   2, 2, 0, 0, 0, 2, 2, 0, 1, 3, 0, 0, 0, 3, 3, 0,
/* 4 */   1, 2, 0, 0, 0, 2, 2, 0, 1, 2, 1, 0, 3, 3, 3, 0,
/* 5 */   2, 2, 0, 0, 0, 2, 2, 0, 1, 3, 0, 0, 0, 3, 3, 0,
/* 6 */   1, 2, 0, 0, 0, 2, 2, 0, 1, 2, 1, 0, 3, 3, 3, 0,
/* 7 */   2, 2, 0, 0, 0, 2, 2, 0, 1, 3, 0, 0, 0, 3, 3, 0,
/* 8 */   0, 2, 0, 0, 2, 2, 2, 0, 1, 0, 1, 0, 3, 3, 3, 0,
/* 9 */   2, 2, 0, 0, 2, 2, 2, 0, 1, 3, 1, 0, 0, 3, 0, 0,
/* A */   2, 2, 2, 0, 2, 2, 2, 0, 1, 2, 1, 0, 3, 3, 3, 0,
/* B */   2, 2, 0, 0, 2, 2, 2, 0, 1, 3, 1, 0, 3, 3, 3, 0,
/* C */   2, 2, 0, 0, 2, 2, 2, 0, 1, 2, 1, 0, 3, 3, 3, 0,
/* D */   2, 2, 0, 0, 0, 2, 2, 0, 1, 3, 0, 0, 0, 3, 3, 0,
/* E */   2, 2, 0, 0, 2, 2, 2, 0, 1, 2, 1, 0, 3, 3, 3, 0,
/* F */   2, 2, 0, 0, 0, 2, 2, 0, 1, 3, 0, 0, 0, 3, 3, 0,
};

/* IR opcode → register-side-effect flags (indexed 0x00..0x73).
 * Matches the annotation logic in ir_emit_imm/ir_emit_zp/ir_emit_abs
 * but as a flat table — callable from any bank without code. */
static const uint8_t ir_op_flags_b2[116] = {
/*  0x00 unused          */ 0x00,
/*  0x01 IR_LDA_IMM      */ 0x90,  /*  W:A,F           */
/*  0x02 IR_LDA_ZP       */ 0x90,
/*  0x03 IR_LDA_ABS      */ 0x90,
/*  0x04 IR_LDX_IMM      */ 0xA0,  /*  W:X,F           */
/*  0x05 IR_LDX_ZP       */ 0xA0,
/*  0x06 IR_LDX_ABS      */ 0xA0,
/*  0x07 IR_LDY_IMM      */ 0xC0,  /*  W:Y,F           */
/*  0x08 IR_LDY_ZP       */ 0xC0,
/*  0x09 IR_LDY_ABS      */ 0xC0,
/*  0x0A IR_STA_ZP       */ 0x01,  /*  R:A             */
/*  0x0B IR_STA_ABS      */ 0x01,
/*  0x0C IR_STX_ZP       */ 0x02,  /*  R:X             */
/*  0x0D IR_STX_ABS      */ 0x02,
/*  0x0E IR_STY_ZP       */ 0x04,  /*  R:Y             */
/*  0x0F IR_STY_ABS      */ 0x04,
/*  0x10 IR_JMP_ABS      */ 0x00,
/*  0x11 IR_JSR          */ 0x00,
/*  0x12 IR_RTS          */ 0x00,
/*  0x13 IR_PHP          */ 0x08,  /*  R:F             */
/*  0x14 IR_PLP          */ 0x80,  /*  W:F             */
/*  0x15 IR_PHA          */ 0x01,  /*  R:A             */
/*  0x16 IR_PLA          */ 0x90,  /*  W:A,F           */
/*  0x17 IR_NOP          */ 0x00,
/*  0x18 IR_CLC          */ 0x80,  /*  W:F             */
/*  0x19 IR_SEC          */ 0x80,
/*  0x1A IR_CLD          */ 0x80,
/*  0x1B IR_SED          */ 0x80,
/*  0x1C IR_CLI          */ 0x80,
/*  0x1D IR_SEI          */ 0x80,
/*  0x1E IR_CLV          */ 0x80,
/*  0x1F IR_BRK          */ 0x00,
/*  0x20 IR_BPL          */ 0x08,  /*  R:F             */
/*  0x21 IR_BMI          */ 0x08,
/*  0x22 IR_BVC          */ 0x08,
/*  0x23 IR_BVS          */ 0x08,
/*  0x24 IR_BCC          */ 0x08,
/*  0x25 IR_BCS          */ 0x08,
/*  0x26 IR_BNE          */ 0x08,
/*  0x27 IR_BEQ          */ 0x08,
/*  0x28 IR_TAX          */ 0xA1,  /*  R:A W:X,F       */
/*  0x29 IR_TAY          */ 0xC1,  /*  R:A W:Y,F       */
/*  0x2A IR_TXA          */ 0x92,  /*  R:X W:A,F       */
/*  0x2B IR_TYA          */ 0x94,  /*  R:Y W:A,F       */
/*  0x2C IR_TSX          */ 0xA0,  /*  W:X,F           */
/*  0x2D IR_TXS          */ 0x02,  /*  R:X             */
/*  0x2E IR_INX          */ 0xA2,  /*  R:X W:X,F       */
/*  0x2F IR_DEX          */ 0xA2,
/*  0x30 IR_INY          */ 0xC4,  /*  R:Y W:Y,F       */
/*  0x31 IR_DEY          */ 0xC4,
/*  0x32 IR_ADC_IMM      */ 0x99,  /*  R:A,F W:A,F     */
/*  0x33 IR_SBC_IMM      */ 0x99,
/*  0x34 IR_AND_IMM      */ 0x91,  /*  R:A W:A,F       */
/*  0x35 IR_ORA_IMM      */ 0x91,
/*  0x36 IR_EOR_IMM      */ 0x91,
/*  0x37 IR_CMP_IMM      */ 0x81,  /*  R:A W:F         */
/*  0x38 IR_CPX_IMM      */ 0x82,  /*  R:X W:F         */
/*  0x39 IR_CPY_IMM      */ 0x84,  /*  R:Y W:F         */
/*  0x3A IR_ADC_ZP       */ 0x99,
/*  0x3B IR_SBC_ZP       */ 0x99,
/*  0x3C IR_AND_ZP       */ 0x91,
/*  0x3D IR_ORA_ZP       */ 0x91,
/*  0x3E IR_EOR_ZP       */ 0x91,
/*  0x3F IR_CMP_ZP       */ 0x81,
/*  0x40 IR_CPX_ZP       */ 0x82,
/*  0x41 IR_CPY_ZP       */ 0x84,
/*  0x42 IR_ADC_ABS      */ 0x99,
/*  0x43 IR_SBC_ABS      */ 0x99,
/*  0x44 IR_AND_ABS      */ 0x91,
/*  0x45 IR_ORA_ABS      */ 0x91,
/*  0x46 IR_EOR_ABS      */ 0x91,
/*  0x47 IR_CMP_ABS      */ 0x81,
/*  0x48 IR_CPX_ABS      */ 0x82,
/*  0x49 IR_CPY_ABS      */ 0x84,
/*  0x4A IR_INC_ZP       */ 0x80,  /*  W:F             */
/*  0x4B IR_DEC_ZP       */ 0x80,
/*  0x4C IR_ASL_ZP       */ 0x80,
/*  0x4D IR_LSR_ZP       */ 0x80,
/*  0x4E IR_ROL_ZP       */ 0x80,
/*  0x4F IR_ROR_ZP       */ 0x80,
/*  0x50 IR_INC_ABS      */ 0x80,
/*  0x51 IR_DEC_ABS      */ 0x80,
/*  0x52 IR_ASL_ABS      */ 0x80,
/*  0x53 IR_LSR_ABS      */ 0x80,
/*  0x54 IR_ROL_ABS      */ 0x80,
/*  0x55 IR_ROR_ABS      */ 0x80,
/*  0x56 IR_ASL_A        */ 0x91,  /*  R:A W:A,F       */
/*  0x57 IR_LSR_A        */ 0x91,
/*  0x58 IR_ROL_A        */ 0x91,
/*  0x59 IR_ROR_A        */ 0x91,
/*  0x5A IR_LDA_ABSX     */ 0x90,
/*  0x5B IR_LDA_ABSY     */ 0x90,
/*  0x5C IR_STA_ABSX     */ 0x01,
/*  0x5D IR_STA_ABSY     */ 0x01,
/*  0x5E IR_ADC_ABSX     */ 0x99,
/*  0x5F IR_SBC_ABSX     */ 0x99,
/*  0x60 IR_AND_ABSX     */ 0x91,
/*  0x61 IR_ORA_ABSX     */ 0x91,
/*  0x62 IR_EOR_ABSX     */ 0x91,
/*  0x63 IR_CMP_ABSX     */ 0x81,
/*  0x64 IR_ADC_ABSY     */ 0x99,
/*  0x65 IR_SBC_ABSY     */ 0x99,
/*  0x66 IR_AND_ABSY     */ 0x91,
/*  0x67 IR_ORA_ABSY     */ 0x91,
/*  0x68 IR_EOR_ABSY     */ 0x91,
/*  0x69 IR_CMP_ABSY     */ 0x81,
/*  0x6A IR_LDX_ABSY     */ 0xA0,
/*  0x6B IR_LDY_ABSX     */ 0xC0,
/*  0x6C IR_INC_ABSX     */ 0x80,
/*  0x6D IR_DEC_ABSX     */ 0x80,
/*  0x6E IR_ASL_ABSX     */ 0x80,
/*  0x6F IR_LSR_ABSX     */ 0x80,
/*  0x70 IR_ROL_ABSX     */ 0x80,
/*  0x71 IR_ROR_ABSX     */ 0x80,
/*  0x72 IR_BIT_ZP       */ 0x81,  /*  R:A W:F  (fix)  */
/*  0x73 IR_BIT_ABS      */ 0x81,
};

#pragma section bank2

/* -------------------------------------------------------------------
 * ir_record_native_b2 — scan a short byte buffer produced by one
 * instruction handler and emit the corresponding IR nodes.
 * Lean version of the old bank-1 ir_record_from_buffer(), using
 * rodata2 tables instead of bank-1 tables.
 * ------------------------------------------------------------------- */
static void ir_record_native_b2(const uint8_t *buf, uint8_t len)
{
	uint8_t pos = 0;
	while (pos < len) {
		/* Guard: if node buffer is nearly full, disable IR for this
		 * block rather than silently dropping nodes (which causes
		 * ir_lower to produce fewer bytes → NOP padding corruption). */
		if (ir_ctx.node_count + 3 > IR_MAX_NODES) {
			ir_ctx.enabled = 0;
			return;
		}
		uint8_t opcode = buf[pos];
		uint8_t sz     = native_instr_size_b2[opcode];
		uint8_t ir_op  = native_to_ir_b2[opcode];

		/* Unknown opcode or truncated instruction → raw byte(s) */
		if (sz == 0 || (uint8_t)(pos + sz) > len) {
			IR_EMIT(&ir_ctx, IR_RAW_BYTE, 0, (uint16_t)opcode);
			pos++;
			continue;
		}
		if (ir_op == 0) {
			/* No IR mapping → emit as raw bytes */
			uint8_t i;
			for (i = 0; i < sz; i++)
				IR_EMIT(&ir_ctx, IR_RAW_BYTE, 0, (uint16_t)buf[pos + i]);
			pos += sz;
			continue;
		}

		/* Construct operand from instruction bytes */
		uint16_t operand = 0;
		if (sz == 2)
			operand = (uint16_t)buf[pos + 1];
		else if (sz == 3)
			operand = (uint16_t)buf[pos + 1] | ((uint16_t)buf[pos + 2] << 8);

		IR_EMIT(&ir_ctx, ir_op, ir_op_flags_b2[ir_op], operand);
		pos += sz;
	}
}

#endif /* ENABLE_IR */

#pragma section bank2

static uint8_t recompile_opcode_b2_inner()
{
	// WRAM helper for cross-bank reads (safe from bank2)
	extern uint8_t peek_bank_byte(uint8_t bank, uint16_t addr);

	// Dirty flags from exidy.c (ZP) — needed for INC emission on screen/char stores
	extern __zpage uint8_t screen_ram_updated;
	extern __zpage uint8_t character_ram_updated;

#ifdef ENABLE_POINTER_SWIZZLE
	init_zp_mirror_table();
#endif

	uint8_t op_buffer_0;
	uint8_t op_buffer_1;
	uint8_t op_buffer_2;
	uint8_t *code_ptr = cache_code[cache_index];

	op_buffer_0 = read6502(pc);

#ifdef ENABLE_PEEPHOLE
	// Flush deferred PLP from previous instruction's trim.
	// Done here (bank2) so ALL compile loops get it for free.
	// NOTE: volatile pointer casts required — vbcc -O2 eliminates
	// plain stores inside if(block_flags_saved) blocks.
#ifdef ENABLE_PEEPHOLE_SKIP
	// For template opcodes (PHA/PLA/PHP/PLP) defer the flush —
	// emit_template() will elide the PLP/PHP pair if possible.
	if (block_flags_saved &&
	    op_buffer_0 != 0x48 && op_buffer_0 != 0x68 &&
	    op_buffer_0 != 0x08 && op_buffer_0 != 0x28) {
		*(volatile uint8_t *)&code_ptr[code_index] = 0x28;  // PLP
		code_index++;
		block_flags_saved = 0;
	}
#else
	if (block_flags_saved) {
		*(volatile uint8_t *)&code_ptr[code_index] = 0x28;  // PLP
		code_index++;  // plain increment so compiler tracks new value
		block_flags_saved = 0;
	}
#endif
#endif
	recompile_instr_start = code_index;

	switch (op_buffer_0)
	{
		case opBNE:
		case opBPL:
		case opBMI:
		case opBEQ:
		case opBCS:				
		case opBCC:				
		case opBVC:
		case opBVS:				
		{
			op_buffer_1 = read6502(pc+1);
			
			int8_t branch_offset = (int8_t) op_buffer_1;

			if (branch_offset >= 0)
			{
				// Forward branch
				uint16_t target_pc = pc + 2 + branch_offset;

#ifdef ENABLE_OPTIMIZER_V2
				branch_forward++;

				// --- Static pass 2: all native addresses known, emit direct ---
				// Under IR, skip direct branches — their offsets are
				// computed from pre-IR code_index and become stale after
				// optimization removes bytes.  Fall through to the
				// 21-byte patchable template (IR_RAW_BYTE, immune).
#ifndef ENABLE_IR
				if (sa_compile_pass == 2)
				{
					uint8_t emit_len = try_direct_branch(target_pc, op_buffer_0, code_ptr);
					if (emit_len)
					{
						pc += 2;
						code_index += emit_len;
						cache_branches++;
						cache_flag[cache_index] |= READY_FOR_NEXT;
						return cache_flag[cache_index];
					}
					// Not resolved — fall through to patchable template
				}
#endif

				// --- Dynamic / pass-1: 21-byte patchable template ---
				{
				uint16_t pattern_flash_address = flash_code_address;
				uint8_t pattern_flash_bank = flash_code_bank;
				uint8_t pattern_code_index = code_index;

				if ((code_index + 21 + 14) >= CODE_SIZE)
				{
					enable_interpret();
				}
				else
				{
				// +0: Inverted branch $13 (skip 19 bytes to +21)
				code_ptr[code_index+0] = invert_branch(op_buffer_0);
				code_ptr[code_index+1] = 19;

				// +2: Original branch $03 (to slow path at +7) - PATCHABLE to $00
				code_ptr[code_index+2] = op_buffer_0;
				code_ptr[code_index+3] = 3;

				// +4: JMP $FFFF - fast path (operand PATCHABLE)
				code_ptr[code_index+4] = 0x4C;
				code_ptr[code_index+5] = 0xFF;
				code_ptr[code_index+6] = 0xFF;

				// +7: STA _a (slow path epilogue)
				code_ptr[code_index+7] = 0x85;
				code_ptr[code_index+8] = (uint8_t)((uint16_t)&a);

				// +9: PHP
				code_ptr[code_index+9] = 0x08;

				// +10: LDA #<target_pc
				code_ptr[code_index+10] = 0xA9;
				code_ptr[code_index+11] = target_pc & 0xFF;

				// +12: STA _pc
				code_ptr[code_index+12] = 0x85;
				code_ptr[code_index+13] = (uint8_t)((uint16_t)&pc);

				// +14: LDA #>target_pc
				code_ptr[code_index+14] = 0xA9;
				code_ptr[code_index+15] = (target_pc >> 8) & 0xFF;

				// +16: STA _pc+1
				code_ptr[code_index+16] = 0x85;
				code_ptr[code_index+17] = (uint8_t)(((uint16_t)&pc) + 1);

				// +18: JMP cross_bank_dispatch
				code_ptr[code_index+18] = 0x4C;
				code_ptr[code_index+19] = (uint8_t)((uint16_t)&cross_bank_dispatch);
				code_ptr[code_index+20] = (uint8_t)(((uint16_t)&cross_bank_dispatch) >> 8);

				// Record pending patch
#ifndef ENABLE_IR
				uint16_t branch_offset_addr = pattern_flash_address + BLOCK_PREFIX_SIZE + pattern_code_index + 3;
				uint16_t jmp_operand_addr = pattern_flash_address + BLOCK_PREFIX_SIZE + pattern_code_index + 5;
				opt2_record_pending_branch_safe(branch_offset_addr, jmp_operand_addr, pattern_flash_bank, target_pc, 0);

				setup_flash_address(pc, flash_cache_index);
				flash_cache_pc_update(pattern_code_index, RECOMPILED);
#endif
				// (Under IR, deferred patches are recorded by the bank2 wrapper
				// and resolved post-lowering by ir_resolve_deferred_patches.)

				pc += 2;
				code_index += 21;
				cache_branches++;
				cache_flag[cache_index] |= READY_FOR_NEXT;
				return cache_flag[cache_index];
				}
				}  // end fallback block
#else
				branch_forward++;
				enable_interpret();
#endif
			}
			else
			{
			// Backward branch - can optimize if target compiled
			uint16_t target_pc = pc + 2 + branch_offset;
			
			// Look up target's flag via peek_bank_byte (safe from bank2)
			uint8_t target_flag_bank = ((target_pc >> 14) + BANK_PC_FLAGS);
			uint8_t target_flag = peek_bank_byte(target_flag_bank,
				(uint16_t)&flash_cache_pc_flags[target_pc & FLASH_BANK_MASK]);
			
			// Check if target is compiled (bit 7 clear = RECOMPILED)
			if (target_flag & RECOMPILED)
			{
#ifdef ENABLE_OPTIMIZER_V2
				branch_not_compiled++;

				// No reservation for backward-uncompiled targets.
				// During batch compile, the forward scan already passed
				// backward addresses; if the target wasn't compiled, it's
				// not in the bitmap.  Reserving would compile garbage.
				// Use 21-byte patchable template (runtime patching).
				{
				// V2: Three-path branch pattern (21 bytes)
				uint16_t pattern_flash_address = flash_code_address;
				uint8_t pattern_flash_bank = flash_code_bank;
				uint8_t pattern_code_index = code_index;
				
				// Check space: need 21 bytes + epilogue room
				if ((code_index + 21 + 14) >= CODE_SIZE)
				{
					enable_interpret();
					// Fall through - not enough space for 21-byte pattern
				}
				else
				{
				// Pattern:
				//   +0:  Bxx_inv $13   ; 2B - skip 19 bytes to +21 if NOT taken
				//   +2:  Bxx $03       ; 2B - original opcode (unconditional!) -> slow at +7
				//                      ;      PATCH to $00 -> fast at +4
				//   +4:  JMP $FFFF     ; 3B - fast path (PATCH operand to native addr)
				//   +7:  STA _a        ; 2B - slow path epilogue
				//   +9:  PHP           ; 1B
				//   +10: LDA #<target  ; 2B
				//   +12: STA _pc       ; 2B
				//   +14: LDA #>target  ; 2B
				//   +16: STA _pc+1     ; 2B
				//   +18: JMP dispatch  ; 3B - to dispatcher
				//   +21: [continues]   ; path 1 - branch not taken
				//
				// Path 1: Branch NOT taken - inverted branch jumps over everything
				// Path 2: Branch taken, target unknown - second branch to slow path
				// Path 3: Branch taken, target known - patched to fast JMP
				
				// +0: Inverted branch $13 (skip 19 bytes to +21)
				code_ptr[code_index+0] = invert_branch(op_buffer_0);
				code_ptr[code_index+1] = 19;  // Skip to +21
				
				// +2: Original branch $03 (to slow path at +7) - PATCHABLE to $00
				code_ptr[code_index+2] = op_buffer_0;  // original opcode
				code_ptr[code_index+3] = 3;  // offset to slow path
				
				// +4: JMP $FFFF - fast path (operand PATCHABLE)
				code_ptr[code_index+4] = 0x4C;  // JMP
				code_ptr[code_index+5] = 0xFF;  // Low byte
				code_ptr[code_index+6] = 0xFF;  // High byte
				
				// +7: STA _a
				code_ptr[code_index+7] = 0x85;  // STA zp
				code_ptr[code_index+8] = (uint8_t)((uint16_t)&a);
				
				// +9: PHP
				code_ptr[code_index+9] = 0x08;  // PHP
				
				// +10: LDA #<target_pc
				code_ptr[code_index+10] = 0xA9;  // LDA immediate
				code_ptr[code_index+11] = target_pc & 0xFF;
				
				// +12: STA _pc
				code_ptr[code_index+12] = 0x85;  // STA zp
				code_ptr[code_index+13] = (uint8_t)((uint16_t)&pc);
				
				// +14: LDA #>target_pc
				code_ptr[code_index+14] = 0xA9;  // LDA immediate
				code_ptr[code_index+15] = (target_pc >> 8) & 0xFF;
				
				// +16: STA _pc+1
				code_ptr[code_index+16] = 0x85;  // STA zp
				code_ptr[code_index+17] = (uint8_t)(((uint16_t)&pc) + 1);
				
				// +18: JMP cross_bank_dispatch
				code_ptr[code_index+18] = 0x4C;  // JMP
				code_ptr[code_index+19] = (uint8_t)((uint16_t)&cross_bank_dispatch);
				code_ptr[code_index+20] = (uint8_t)(((uint16_t)&cross_bank_dispatch) >> 8);
				
				// Record pending patch:
				// - Branch offset at +3 (patch $03 -> $00)
				// - JMP operand at +5 (patch $FFFF -> native)
				// Use saved addresses from when pattern was emitted
#ifndef ENABLE_IR
				uint16_t branch_offset_addr = pattern_flash_address + BLOCK_PREFIX_SIZE + pattern_code_index + 3;
				uint16_t jmp_operand_addr = pattern_flash_address + BLOCK_PREFIX_SIZE + pattern_code_index + 5;
				// Call via fixed-bank trampoline (safe from bank2)
				opt2_record_pending_branch_safe(branch_offset_addr, jmp_operand_addr, pattern_flash_bank, target_pc, 0);
				
				// Update PC table to point to this pattern
				// (required when OPT_BLOCK_METADATA is 0)
				setup_flash_address(pc, flash_cache_index);
				flash_cache_pc_update(pattern_code_index, RECOMPILED);
#endif
				
				pc += 2;
				code_index += 21;
				cache_branches++;
				cache_flag[cache_index] |= READY_FOR_NEXT;
				return cache_flag[cache_index];
				}  // end else (enough space for 21-byte pattern)
				}  // end fallback block
#else
				// Target not compiled - interpret
				branch_not_compiled++;
				enable_interpret();
				// Fall through to emit interpreted branch
#endif
			}
			else
			{
				// Target IS compiled.
				//
				// INTRA-BLOCK BACKWARD BRANCH: if the target is within
				// this block, emit a native 2-byte branch (fixed-bank helper).
				// Under IR, skip — offset becomes stale after optimisation.
#ifndef ENABLE_IR
				if (try_intra_block_branch(target_pc, op_buffer_0))
					return cache_flag[cache_index];
#endif
				
				// INTER-BLOCK BACKWARD BRANCH: target is compiled in a
				// different block.  Same-bank: try 2-byte native branch
				// first, then 5-byte Bxx_inv+JMP.
				// Cross-bank: fall through to interpret.
				// Under IR, skip all direct-offset paths — offsets are
				// pre-IR and become stale.  Fall through to interpret,
				// then fall through to the patchable template path.
#ifndef ENABLE_IR
				if ((target_flag & 0x1F) == flash_code_bank &&
				    (code_index + 2 + EPILOGUE_SIZE + 6) < CODE_SIZE &&
				    lookup_native_addr_safe(target_pc))
				{
					// Same bank — try native 2-byte branch, else 5-byte
					int16_t native_offset = (int16_t)reserve_result_addr
					    - (int16_t)(flash_code_address + BLOCK_PREFIX_SIZE + code_index + 2);
					uint8_t emit_len = 0;
					if (native_offset >= -128 && native_offset <= 127)
					{
						code_ptr[code_index+0] = op_buffer_0;
						code_ptr[code_index+1] = (uint8_t)(int8_t)native_offset;
						emit_len = 2;
					}
					else if ((code_index + 5 + EPILOGUE_SIZE + 6) < CODE_SIZE)
					{
						code_ptr[code_index+0] = invert_branch(op_buffer_0);
						code_ptr[code_index+1] = 3;
						code_ptr[code_index+2] = 0x4C;
						code_ptr[code_index+3] = reserve_result_addr & 0xFF;
						code_ptr[code_index+4] = (reserve_result_addr >> 8) & 0xFF;
						emit_len = 5;
					}
					if (emit_len)
					{
						setup_flash_address(pc, flash_cache_index);
						flash_cache_pc_update(code_index, RECOMPILED);
						pc += 2;
						code_index += emit_len;
						cache_branches++;
						cache_flag[cache_index] |= READY_FOR_NEXT;
						return cache_flag[cache_index];
					}
				}
#endif
				// Cross-bank or no space — interpret
				enable_interpret();
			}  // end else (target is compiled)
			}  // end else (backward branch)
			break;  // Phase B: prevent fallthrough to case opJMP
		}
		
		case opJMP:
		{
			uint16_t target_pc = (uint16_t)read6502(pc+1) | ((uint16_t)read6502(pc+2) << 8);

			// --- Static pass 2: target address known, emit direct JMP ---
			if (sa_compile_pass == 2 &&
			    code_index + 3 < CODE_SIZE &&
			    lookup_entry_list(target_pc))
			{
				if (reserve_result_bank == flash_code_bank)
				{
					// Same bank — direct JMP (3 bytes)
					code_ptr[code_index+0] = 0x4C;
					code_ptr[code_index+1] = reserve_result_addr & 0xFF;
					code_ptr[code_index+2] = (reserve_result_addr >> 8) & 0xFF;

					pc = target_pc;
					code_index += 3;
					cache_flag[cache_index] &= ~READY_FOR_NEXT;
					return cache_flag[cache_index];
				}
				// Cross-bank — fall through to patchable
			}

			// --- Fallback: 9-byte patchable JMP pattern ---
			if (code_index + 9 < CODE_SIZE) {
				// +0: PHP
				code_ptr[code_index] = 0x08;
				// +1: CLC
				code_ptr[code_index+1] = 0x18;
				// +2: BCC +4 (always taken → slow PLP at +8)
				code_ptr[code_index+2] = 0x90;
				code_ptr[code_index+3] = 4;
				// +4: PLP (fast path: restore flags)
				code_ptr[code_index+4] = 0x28;
				// +5: JMP $FFFF (fast path, patchable)
				code_ptr[code_index+5] = 0x4C;
				code_ptr[code_index+6] = 0xFF;
				code_ptr[code_index+7] = 0xFF;
				// +8: PLP (slow path: restore flags before epilogue)
				code_ptr[code_index+8] = 0x28;
				
#ifndef ENABLE_IR
				uint16_t bcc_operand_addr = flash_code_address + BLOCK_PREFIX_SIZE + code_index + 3;
				uint16_t jmp_operand_addr = flash_code_address + BLOCK_PREFIX_SIZE + code_index + 6;
				opt2_record_pending_branch_safe(bcc_operand_addr, jmp_operand_addr, flash_code_bank, target_pc, 0);
#endif
				// (Under IR, deferred patches are recorded by the bank2 wrapper
				// and resolved post-lowering by ir_resolve_deferred_patches.)
				
				pc = target_pc;
				code_index += 9;
				cache_flag[cache_index] &= ~READY_FOR_NEXT;  // JMP ends the block
				return cache_flag[cache_index];
			} else {
				enable_interpret();
			}
			break;
		}
		
		case opJSR:
		{			
			// JSR: push return address onto emulated stack, set _pc to target.
			//
			// Stack-clean subroutines (no TSX/TXS) use the native JSR template
			// which loops through subroutine blocks via a WRAM trampoline
			// without returning to C between each block dispatch.
			//
			// Stack-dirty or unknown subroutines use the emulated JSR template
			// which exits to the C dispatcher after pushing the return address.
			//
			// 6502 JSR convention: pushes (pc+2) hi then lo (address of last byte of JSR).
			// RTS pops lo then hi, adds 1, jumps there -> lands on pc+3.
			
			uint16_t target = (uint16_t)read6502(pc+1) | ((uint16_t)read6502(pc+2) << 8);
			uint16_t return_addr = pc + 2;  // 6502 convention: push PC+2 (addr of last byte)

#ifdef ENABLE_NATIVE_JSR
			// Check if subroutine is stack-clean — use native trampoline template
			if (sa_subroutine_lookup(target) == SA_SUB_CLEAN &&
			    (code_index + opcode_6502_njsr_size + EPILOGUE_SIZE + 6) < CODE_SIZE)
			{
				for (uint8_t i = 0; i < opcode_6502_njsr_size; i++)
					code_ptr[code_index+i] = opcode_6502_njsr[i];
				
				code_ptr[code_index + opcode_6502_njsr_ret_hi] = (uint8_t)(return_addr >> 8);
				code_ptr[code_index + opcode_6502_njsr_ret_lo] = (uint8_t)(return_addr);
				code_ptr[code_index + opcode_6502_njsr_tgt_lo] = (uint8_t)(target);
				code_ptr[code_index + opcode_6502_njsr_tgt_hi] = (uint8_t)(target >> 8);
				
				pc += 3;
				code_index += opcode_6502_njsr_size;
				block_has_jsr = 1;
				cache_flag[cache_index] |= READY_FOR_NEXT;
				return cache_flag[cache_index];
			}
			else
#endif
			// Emulated JSR: standard template (stack-dirty, unknown, or no SA)
			if ((code_index + opcode_6502_jsr_size + EPILOGUE_SIZE + 6) < CODE_SIZE)
			{
				for (uint8_t i = 0; i < opcode_6502_jsr_size; i++)
					code_ptr[code_index+i] = opcode_6502_jsr[i];
				
				code_ptr[code_index + opcode_6502_jsr_ret_hi] = (uint8_t)(return_addr >> 8);
				code_ptr[code_index + opcode_6502_jsr_ret_lo] = (uint8_t)(return_addr);
				code_ptr[code_index + opcode_6502_jsr_tgt_lo] = (uint8_t)(target);
				code_ptr[code_index + opcode_6502_jsr_tgt_hi] = (uint8_t)(target >> 8);
				
				pc += 3;
				code_index += opcode_6502_jsr_size;
				block_has_jsr = 1;
				cache_flag[cache_index] |= READY_FOR_NEXT;
				return cache_flag[cache_index];
			}
			else
			{
				// Not enough space -- fall back to interpreter
				cache_branch_pc_lo[cache_index] = read6502(pc+1);
				cache_branch_pc_hi[cache_index] = read6502(pc+2);
				enable_interpret();
			}
			break;
		}
		
		case opJMPi:
		case opRTI:
		{
			enable_interpret();
			break;
		}
		
		case opRTS:
		{
			// Compiled RTS: pop return addr from emulated stack, add 1, set _pc,
			// exit to dispatcher.  The native JSR trampoline detects the SP
			// change and knows the subroutine returned.
			if ((code_index + opcode_6502_nrts_size + EPILOGUE_SIZE + 6) < CODE_SIZE)
			{
				for (uint8_t i = 0; i < opcode_6502_nrts_size; i++)
					code_ptr[code_index+i] = opcode_6502_nrts[i];
				
				pc += 1;
				code_index += opcode_6502_nrts_size;
				cache_flag[cache_index] &= ~READY_FOR_NEXT;
				return cache_flag[cache_index];
			}
			enable_interpret();
			break;
		}
		
		case opTSX:
		{
			if ((code_index + 3 + 3) < CODE_SIZE)
			{				
				code_ptr[code_index] = opLDX_ZP;
				code_ptr[code_index+1] = (uint16_t) &sp;				
				pc += 1;
				code_index += 2;
				cache_flag[cache_index] |= READY_FOR_NEXT;
				return cache_flag[cache_index];
			}			
			else
			{
				cache_flag[cache_index] |= (OUT_OF_CACHE | INTERPRET_NEXT_INSTRUCTION);
				cache_flag[cache_index] &= ~READY_FOR_NEXT;
				return cache_flag[cache_index];
			}																				
		}
		
		case opTXS:
		{
			if ((code_index + 3 + 3) < CODE_SIZE)
			{				
				code_ptr[code_index] = opSTX_ZP;
				code_ptr[code_index+1] = (uint16_t) &sp;				
				pc += 1;
				code_index += 2;
				cache_flag[cache_index] |= READY_FOR_NEXT;
				return cache_flag[cache_index];
			}			
			else
			{
				cache_flag[cache_index] |= (OUT_OF_CACHE | INTERPRET_NEXT_INSTRUCTION);
				cache_flag[cache_index] &= ~READY_FOR_NEXT;
				return cache_flag[cache_index];
			}																				
		}		
		
		case opPHA:
			return emit_template(opcode_6502_pha, opcode_6502_pha_size);
		case opPLA:
			return emit_template(opcode_6502_pla, opcode_6502_pla_size);
		case opPHP:
			return emit_template(opcode_6502_php, opcode_6502_php_size);
		case opPLP:
			return emit_template(opcode_6502_plp, opcode_6502_plp_size);
		case opCLI:
		case opSEI:
		{
			enable_interpret();
			break;
		}		
		case opSTX_ZPY:
		case opSTY_ZPX:
		{			
			enable_interpret();
			break;
		}
		
		case opSED:
		{
			decimal_mode = 1;
			enable_interpret();
			break;
		}		

		case opNOP:
		{
			// Skip NOP — no code emitted, just advance pc
			pc += 1;
			cache_flag[cache_index] |= READY_FOR_NEXT;
			return cache_flag[cache_index];
		}
		
		case opBRK:
		{
			// BRK triggers interrupt - must be interpreted
			IO8(0x4021) = 0;	// debug marker?
			enable_interpret();
		}

		default:
		{			
			code_ptr[code_index] = op_buffer_0;
			switch (addrmodes[op_buffer_0])	// use address mode type to determine instruction size
			{					
					case abso:
				case absx:
				case absy:
				case ind:
				{					
					encoded_address = (uint16_t) ((read6502(pc+1)) | (read6502(pc+2) << 8));					
					
					// Use modular platform for address translation
					decoded_address = translate_address(encoded_address);
					
					if (decoded_address)
					{
						code_ptr[code_index+1] = (uint8_t) decoded_address;
						code_ptr[code_index+2] = (uint8_t) (decoded_address >> 8);
					}
					else
						enable_interpret();							
		
					pc += 3;
					code_index += 3;

					// Emit dirty flag for stores to screen/char RAM.
					// Skip if we already INC'd that region in this block.
					if (decoded_address)
					{
						uint8_t msb = encoded_address >> 8;
						if (msb >= 0x40 && msb < 0x50 &&
						    !((msb < 0x48) ? block_dirty_screen : block_dirty_char) &&
						    (code_index + 4 + EPILOGUE_SIZE) < CODE_SIZE)
						{
							uint8_t n = emit_dirty_flag(code_ptr, code_index,
							    op_buffer_0, msb,
							    (uint8_t)((uint16_t)&screen_ram_updated),
							    (uint8_t)((uint16_t)&character_ram_updated));
							if (n) {
								code_index += n;
								if (msb < 0x48) block_dirty_screen = 1;
								else            block_dirty_char = 1;
							}
						}
					}
					break;
				}						
				
				case zp:				
				{															
					code_ptr[code_index] |= 0x08; // change ZP to ABS (refer to 6502 opcode matrix) - note: except for STX ZP,Y and STY ZP,X !! (interpreted for now)
					uint8_t zp_addr = read6502(pc+1);
					uint16_t address = (uint16_t)zp_addr + (uint16_t) &RAM_BASE[0];
					code_ptr[code_index+1] = (uint8_t) address;
					code_ptr[code_index+2] = (uint8_t) (address >> 8);
					pc += 2;
					code_index += 3;

#ifdef ENABLE_POINTER_SWIZZLE
					// Mirror write: if this ZP is a tracked pointer byte,
					// also write/modify the NES ZP mirror slot.
					// Mirror stays in Exidy-space — always a 2-byte opcode.
					if ((op_buffer_0 == 0x85 || op_buffer_0 == 0x86 || op_buffer_0 == 0x84 ||
					     op_buffer_0 == 0xE6 || op_buffer_0 == 0xC6) &&
					    (code_index + 2 + EPILOGUE_SIZE) < CODE_SIZE)
					{
						code_index += emit_zp_mirror_write(code_ptr, code_index, op_buffer_0, zp_addr);
					}
#endif
					break;
				}
				case zpx:
				case zpy:
				{
#ifdef ENABLE_ZP_INDEX_WRAP
					// Interpret to preserve correct 6502 ZP index wrapping:
					// (zp_addr + X/Y) & 0xFF stays within zero page
					enable_interpret();
#else
					// Remap zpx→absx / zpy→absy (opcode |= 0x08, same as zp→abs)
					// Assumes program doesn't rely on ZP index wrap-around.
					// Note: STX zpy (0x96) and STY zpx (0x94) have no abs,X/Y
					// equivalent and are handled by explicit cases above.
					code_ptr[code_index] |= 0x08;
					uint16_t address = read6502(pc+1);
					address += (uint16_t) &RAM_BASE[0];
					code_ptr[code_index+1] = (uint8_t) address;
					code_ptr[code_index+2] = (uint8_t) (address >> 8);
					pc += 2;
					code_index += 3;
#endif
					break;
				}

				case indx:
				{
					// Indx pointers can point to ROM in the switchable bank ($8000-$BFFF).
					// At runtime, the flash cache bank is at $8000, so ROM reads would fail.
					// Always interpret indx instructions to ensure correct bank switching.
					enable_interpret();
					break;
					
					/* Original compiled indx - disabled due to bank conflict:
					uint8_t address_8 = read6502(pc+1);
					uint16_t address = address_8;
					address += (uint16_t) &RAM_BASE[0];							
					
					if ((code_index + addr_6502_indx_size + 3) < CODE_SIZE)
					{	
						indx_opcode_location = op_buffer_0;
						indx_address_lo = address;
						indx_address_hi = address+1;
						for (uint8_t i = 0; i < addr_6502_indx_size; i++)
						{
							code_ptr[code_index+i] = addr_6502_indx[i];
						}						
						pc += 2;
						code_index += addr_6502_indx_size;
					}
					else
					{
						cache_flag[cache_index] |= (OUT_OF_CACHE | INTERPRET_NEXT_INSTRUCTION);
						cache_flag[cache_index] &= ~READY_FOR_NEXT;
						return cache_flag[cache_index];
					}																				
					break;
					*/
				}
				case indy:
				{
					enable_interpret();
					uint8_t zp_addr = read6502(pc+1);

					if (op_buffer_0 == 0x91)
					{
#ifdef ENABLE_POINTER_SWIZZLE
						// Try native STA via address_decoding_table
						{
							uint8_t n = 0;
							if ((code_index + 25 + EPILOGUE_SIZE) < CODE_SIZE) {
								n = emit_native_sta_indy(code_ptr, code_index,
								    zp_addr,
								    (uint8_t)((uint16_t)&screen_ram_updated),
								    (uint8_t)((uint16_t)&character_ram_updated));
							}
							if (n) {
								code_index += n;
								pc += 2;
							} else
#endif
						{
						// STA ($zp),Y: route through write6502() for side effects
						if ((code_index + sta_indy_template_size + 3) < CODE_SIZE)
						{
							sta_indy_zp_patch = zp_addr;
							for (uint8_t i = 0; i < sta_indy_template_size; i++)
								code_ptr[code_index+i] = sta_indy_template[i];
							pc += 2;
							code_index += sta_indy_template_size;
						}
						else
						{
							cache_flag[cache_index] |= (OUT_OF_CACHE | INTERPRET_NEXT_INSTRUCTION);
							cache_flag[cache_index] &= ~READY_FOR_NEXT;
							return cache_flag[cache_index];
						}
						}
#ifdef ENABLE_POINTER_SWIZZLE
						}
#endif
					}
					else
					{
						// Read-type indy (LDA, AND, ORA, EOR, ADC, SBC, CMP):
						// use generic indy template with bank-switch trampoline.
						// handle_io_indy detects ROM addresses at runtime and
						// temporarily switches to bank1 for the read.
						uint16_t address = (uint16_t)zp_addr + (uint16_t)&RAM_BASE[0];
						if ((code_index + addr_6502_indy_size + 3) < CODE_SIZE)
						{
							indy_opcode_location = op_buffer_0;
							indy_address_lo = address;
							indy_address_hi = address + 1;
							for (uint8_t i = 0; i < addr_6502_indy_size; i++)
								code_ptr[code_index+i] = addr_6502_indy[i];
							pc += 2;
							code_index += addr_6502_indy_size;
						}
						else
						{
							cache_flag[cache_index] |= (OUT_OF_CACHE | INTERPRET_NEXT_INSTRUCTION);
							cache_flag[cache_index] &= ~READY_FOR_NEXT;
							return cache_flag[cache_index];
						}
					}
					break;
				}
				
				case imm:
				case rel:
				{
					// Immediate values are kept in original Exidy-space.
					// The NES ZP mirror applies hi_offset at the STA point.
					code_ptr[code_index+1] = read6502(pc+1);
					pc += 2;
					code_index += 2;
					break;
				}
				case imp:
				case acc:						
				{
					pc += 1;
					code_index += 1;
					break;
				}
				default:
				{
					enable_interpret();
					pc += 1;
					code_index += 1;
					break;
				}
			}			
		}
	}
	cache_flag[cache_index] |= READY_FOR_NEXT;
	return cache_flag[cache_index];
}

/* -------------------------------------------------------------------
 * recompile_opcode_b2 — bank2 wrapper around _inner().
 * Handles per-instruction IR recording so the post-loop IR block
 * only needs to run optimise + lower (no replay from buffer).
 *
 * Phase B: patchable templates (21-byte branch, 9-byte JMP) contain
 * internal relative offsets and JMP $FFFF placeholders.  These are
 * recorded as IR_RAW_BYTE nodes so the optimizer cannot touch them.
 * Deferred-patch info is extracted from the emitted bytes and stored
 * in ir_ctx.deferred_patches[] for post-lowering resolution.
 * ------------------------------------------------------------------- */
static uint8_t recompile_opcode_b2()
{
#ifdef ENABLE_IR
	uint8_t ci_before = code_index;
	if (ci_before == 0) {
		IR_INIT(&ir_ctx);
	}
#endif
	uint8_t result = recompile_opcode_b2_inner();
#ifdef ENABLE_IR
	if (code_index > ci_before) {
		uint8_t *buf = cache_code[cache_index] + ci_before;
		uint8_t delta = (uint8_t)(code_index - ci_before);

		/* Scan emitted bytes for patchable JMP $FFFF */
		uint8_t patchable_jmp_pos = 0xFF; /* 0xFF = not found */
		{
			uint8_t j;
			for (j = 0; (uint8_t)(j + 2) < delta; j++) {
				if (buf[j] == 0x4C && buf[j+1] == 0xFF && buf[j+2] == 0xFF) {
					patchable_jmp_pos = j;
					break;
				}
			}
		}

		if (patchable_jmp_pos != 0xFF) {
			/* Patchable template — record all bytes as IR_RAW_BYTE
			 * to prevent optimizer from touching internal offsets.
			 * Guard: if the node buffer can't hold all bytes, disable
			 * IR for this block.  Silent drops would produce fewer
			 * lowered bytes and NOP padding would overwrite template
			 * data ($EA over dispatch addresses → crash). */
			if (ir_ctx.node_count + delta > IR_MAX_NODES) {
				ir_ctx.enabled = 0;  /* skip ir_lower, use raw bytes */
			} else {
			uint8_t j;
			for (j = 0; j < delta; j++)
				IR_EMIT(&ir_ctx, IR_RAW_BYTE, 0, (uint16_t)buf[j]);

			/* Extract and store deferred patch info */
			if (ir_ctx.deferred_patch_count < IR_MAX_DEFERRED_PATCHES) {
				ir_deferred_patch_t *dp = &ir_ctx.deferred_patches[ir_ctx.deferred_patch_count];
				uint8_t p = patchable_jmp_pos;
				/* 21-byte branch template: Bxx $03 immediately before JMP */
				if (p >= 2 && buf[p - 1] == 0x03 && (buf[p - 2] & 0x1F) == 0x10) {
					/* target_pc is LDA# at +7 (lo) and +11 (hi) from JMP */
					dp->target_pc = (uint16_t)buf[p + 7]
					              | ((uint16_t)buf[p + 11] << 8);
					dp->is_branch = 1;
					/* Tell optimizer whether this branch reads carry.
					 * BCC=$90 BCS=$B0 read carry; all others don't. */
					{ uint8_t bop = buf[p - 2];
					  ir_ctx.carry_live_at_exit = (bop == 0x90 || bop == 0xB0) ? 1 : 0;
					}
				} else {
					/* 9-byte JMP template — target is where pc was set */
					dp->target_pc = pc;
					dp->is_branch = 0;
				}
				ir_ctx.deferred_patch_count++;
			}
			}  /* end else (node buffer had room) */
		} else {
			ir_record_native_b2(buf, delta);
		}
	}
#endif
	return result;
}

#pragma section default

// Fixed-bank trampoline for recompile_opcode
uint8_t recompile_opcode(void)
{
	uint8_t saved_bank = mapper_prg_bank;
	bankswitch_prg(2);
	uint8_t result = recompile_opcode_b2();
	bankswitch_prg(saved_bank);

#ifdef ENABLE_COMPILE_PPU_EFFECT
	// Toggle red emphasis (bit 5) per instruction compiled (SA boot only)
	if (compile_ppu_active) {
		compile_ppu_effect ^= 0x20;
		lnPPUMASK = 0x3B | compile_ppu_effect;
		*(volatile uint8_t*)0x2001 = lnPPUMASK;  // mid-frame
	}
#endif

	return result;
}

// Removed: check_cache_links(), ready(), verify_link_type0(), verify_link_type1(), combine_caches()
// These were all part of the old RAM cache execution system that is no longer used.
// Flash cache execution uses dispatch_on_pc() and flash_dispatch_return instead.

// Removed: decode_address_c() - now using platform_exidy.translate_addr() from platform/platform_exidy.c

//============================================================================================================
// ==========================================================================
// cache_bit_enable / cache_bit_check — moved to bank 2.
// Not performance-critical. Saves ~506 bytes of fixed-bank space.
// ==========================================================================
#pragma section bank2

static void cache_bit_enable_b2(uint16_t addr)
{	
	uint8_t bit_mask = ~(1 << (addr & 3));
	addr = addr >> 3;
	uint8_t value = bit_mask & peek_bank_byte(3, (uint16_t) &cache_bit_array[0] + addr);
	flash_byte_program((uint16_t) &cache_bit_array[0] + addr, 3, value);
}

//============================================================================================================
static uint8_t cache_bit_check_b2(uint16_t addr)
{
	uint8_t bit_number = addr & 3;
	uint8_t value;
	addr = addr >> 3;
	uint8_t byte_val = peek_bank_byte(3, (uint16_t) &cache_bit_array[0] + addr);
	switch (bit_number)
	{
		case 0:
			value = 0x01 & byte_val;
		case 1:
			value = 0x02 & byte_val;
		case 2:
			value = 0x04 & byte_val;
		case 3:
			value = 0x08 & byte_val;
		case 4:
			value = 0x10 & byte_val;
		case 5:
			value = 0x20 & byte_val;
		case 6:
			value = 0x40 & byte_val;
		case 7:
			value = 0x80 & byte_val;
		default:
	}
	return value;
}

#pragma section default

void cache_bit_enable(uint16_t addr)
{
	uint8_t saved_bank = mapper_prg_bank;
	bankswitch_prg(2);
	cache_bit_enable_b2(addr);
	bankswitch_prg(saved_bank);
}

//============================================================================================================
uint8_t cache_bit_check(uint16_t addr)
{
	uint8_t saved_bank = mapper_prg_bank;
	bankswitch_prg(2);
	uint8_t result = cache_bit_check_b2(addr);
	bankswitch_prg(saved_bank);
	return result;
}

//============================================================================================================
