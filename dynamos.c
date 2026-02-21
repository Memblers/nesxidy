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
#ifdef ENABLE_OPTIMIZER_V2
#include "core/optimizer_v2_simple.h"
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

// Address translation - must be in fixed bank (default section)
// Cannot call into bank1 during recompilation since flash banks are active
static uint16_t translate_address(uint16_t src_addr) {
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
}

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
#ifdef ENABLE_COMPILE_PPU_EFFECT
__zpage uint8_t compile_ppu_effect = 0;  // PPU emphasis bits toggled during compile
__zpage uint8_t compile_ppu_active = 0;  // 1 = PPU effect enabled (SA boot only)
#endif
__zpage uint16_t flash_cache_index;
uint8_t flash_enabled = 0;

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
uint8_t cache_code[BLOCK_COUNT][CODE_SIZE];
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
	
	uint8_t result = dispatch_on_pc();
	switch (result)
	{
		case 2:  // interpret
		{
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
			return;
		}
		case 0:  // executed from flash
		{
			cache_hits++;
#ifdef TRACK_TICKS
			// JIT blocks already have accurate cycle counts in the header,
			// but the dispatch round-trip overhead (~200 NES cycles) isn't
			// reflected.  Adding DISPATCH_OVERHEAD keeps frame timing
			// proportional to wall time even for short blocks (e.g.
			// DEX;BNE = 4 guest cycles + 80 overhead = 84 per dispatch).
			clockticks6502 += DISPATCH_OVERHEAD;
#endif
			return;
		}
		case 1:  // recompile needed
			break;
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
		
		// Record this instruction's code_index for intra-block branch lookup.
		// Index by (guest_pc - entry_pc) & 0x3F.  Store code_index + 1 (0 = empty).
		{
			uint8_t ci_slot = (uint8_t)(pc_old - entry_pc) & 0x3F;
			block_ci_map[ci_slot] = code_index_old + 1;
		}
		
		recompile_opcode();
		
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
		for (uint8_t i = 0; i < instr_len; i++)
		{
			flash_byte_program(flash_code_address + BLOCK_HEADER_SIZE + code_index_old + i, flash_code_bank, cache_code[0][code_index_old + i]);
		}
#endif
		
		if (code_index > (CODE_SIZE - 6))
		{
			cache_flag[0] |= OUT_OF_CACHE;
			cache_flag[0] &= ~READY_FOR_NEXT;
		}
		
		if (cache_flag[0] & INTERPRET_NEXT_INSTRUCTION)
			flash_cache_pc_update(code_index_old, INTERPRETED);
		else if (instr_len || pc != pc_old)
		{
			flash_cache_pc_update(code_index_old, RECOMPILED);
#ifdef ENABLE_OPTIMIZER_V2
			// V2: Check if any pending branches/epilogues target this PC
			opt2_notify_block_compiled(pc_old, flash_code_address + code_index_old + BLOCK_PREFIX_SIZE, flash_code_bank);
#endif
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
		// flash_code_address + 7 = reserved (leave 0xFF = erased)

		// --- Write code + epilogue to flash ---
		for (uint8_t i = 0; i < code_index; i++) {
			flash_byte_program(flash_code_address + BLOCK_HEADER_SIZE + i, flash_code_bank, cache_code[0][i]);
		}

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
// flash_cache_search — moved to bank 2 (appears to be dead code, but kept).
// Saves ~271 bytes of fixed-bank space.
// ==========================================================================
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
	(void)block_number;
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
// Pass-2 forward/backward branch helper (fixed bank — callable from bank2).
// For static pass 2 only: looks up target_pc in the entry list and emits
// a 2-byte native branch or 5-byte Bxx_inv+JMP if same-bank.
// Returns the emit length (2 or 5) on success, 0 if unable to resolve.
// The caller provides the branch opcode and code buffer pointer.
//============================================================================================================
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
// ==========================================================================
#pragma section bank2

// Shared helper: emit a fixed-size opcode template (PHA/PLA/PHP/PLP).
// Copies template bytes into code buffer, advances pc by 1, sets READY_FOR_NEXT.
// Returns updated cache_flag.
static uint8_t emit_template(uint8_t *tmpl, uint8_t sz)
{
	if ((code_index + sz + 3) < CODE_SIZE) {
		uint8_t *dst = &cache_code[cache_index][code_index];
		for (uint8_t i = 0; i < sz; i++)
			dst[i] = tmpl[i];
		pc += 1;
		code_index += sz;
		cache_flag[cache_index] |= READY_FOR_NEXT;
		return cache_flag[cache_index];
	}
	cache_flag[cache_index] |= (OUT_OF_CACHE | INTERPRET_NEXT_INSTRUCTION);
	cache_flag[cache_index] &= ~READY_FOR_NEXT;
	return cache_flag[cache_index];
}

static uint8_t recompile_opcode_b2()
{
	// WRAM helper for cross-bank reads (safe from bank2)
	extern uint8_t peek_bank_byte(uint8_t bank, uint16_t addr);

	// Dirty flags from exidy.c (ZP) — needed for INC emission on screen/char stores
	extern __zpage uint8_t screen_ram_updated;
	extern __zpage uint8_t character_ram_updated;

	uint8_t op_buffer_0;
	uint8_t op_buffer_1;
	uint8_t op_buffer_2;
	uint8_t *code_ptr = cache_code[cache_index];
	op_buffer_0 = read6502(pc);

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
				uint16_t branch_offset_addr = pattern_flash_address + BLOCK_PREFIX_SIZE + pattern_code_index + 3;
				uint16_t jmp_operand_addr = pattern_flash_address + BLOCK_PREFIX_SIZE + pattern_code_index + 5;
				opt2_record_pending_branch_safe(branch_offset_addr, jmp_operand_addr, pattern_flash_bank, target_pc, 0);

				setup_flash_address(pc, flash_cache_index);
				flash_cache_pc_update(pattern_code_index, RECOMPILED);

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
				uint16_t branch_offset_addr = pattern_flash_address + BLOCK_PREFIX_SIZE + pattern_code_index + 3;
				uint16_t jmp_operand_addr = pattern_flash_address + BLOCK_PREFIX_SIZE + pattern_code_index + 5;
				// Call via fixed-bank trampoline (safe from bank2)
				opt2_record_pending_branch_safe(branch_offset_addr, jmp_operand_addr, pattern_flash_bank, target_pc, 0);
				
				// Update PC table to point to this pattern
				// (required when OPT_BLOCK_METADATA is 0)
				setup_flash_address(pc, flash_cache_index);
				flash_cache_pc_update(pattern_code_index, RECOMPILED);
				
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
				if (try_intra_block_branch(target_pc, op_buffer_0))
					return cache_flag[cache_index];
				
				// INTER-BLOCK BACKWARD BRANCH: target is compiled in a
				// different block.  Same-bank: try 2-byte native branch
				// first, then 5-byte Bxx_inv+JMP.
				// Cross-bank: fall through to interpret.
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
				// Cross-bank or no space — interpret
				enable_interpret();
			}  // end else (target is compiled)
			}  // end else (backward branch)
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
				
				uint16_t bcc_operand_addr = flash_code_address + BLOCK_PREFIX_SIZE + code_index + 3;
				uint16_t jmp_operand_addr = flash_code_address + BLOCK_PREFIX_SIZE + code_index + 6;
				opt2_record_pending_branch_safe(bcc_operand_addr, jmp_operand_addr, flash_code_bank, target_pc, 0);
				
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
					// Without this, JIT absolute stores write directly to WRAM
					// but never set screen_ram_updated, so render_video skips
					// the shadow diff and changes are never pushed to the PPU.
					if (decoded_address)
					{
						uint8_t msb = encoded_address >> 8;
						// STA abs=$8D, STX abs=$8E, STY abs=$8C,
						// STA absx=$9D, STA absy=$99
						if ((op_buffer_0 == 0x8D || op_buffer_0 == 0x8E ||
						     op_buffer_0 == 0x8C || op_buffer_0 == 0x9D ||
						     op_buffer_0 == 0x99) &&
						    msb >= 0x40 && msb < 0x50 &&
						    (code_index + 2 + EPILOGUE_SIZE) < CODE_SIZE)
						{
							if (msb < 0x48) {
								// Screen RAM: INC screen_ram_updated (ZP)
								code_ptr[code_index++] = 0xE6;  // INC zp
								code_ptr[code_index++] = (uint8_t)((uint16_t)&screen_ram_updated);
							} else {
								// Character RAM: INC character_ram_updated (ZP)
								code_ptr[code_index++] = 0xE6;  // INC zp
								code_ptr[code_index++] = (uint8_t)((uint16_t)&character_ram_updated);
							}
						}
					}
					break;
				}						
				
				case zp:				
				{															
					code_ptr[code_index] |= 0x08; // change ZP to ABS (refer to 6502 opcode matrix) - note: except for STX ZP,Y and STY ZP,X !! (interpreted for now)
					uint16_t address = read6502(pc+1);
					address += (uint16_t) &RAM_BASE[0];
					code_ptr[code_index+1] = (uint8_t) address;
					code_ptr[code_index+2] = (uint8_t) (address >> 8);
					pc += 2;
					code_index += 3;
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
					uint8_t zp_addr = read6502(pc+1);

					if (op_buffer_0 == 0x91)
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
					code_ptr[code_index+1] = read6502(pc+1);								
					pc += 2;
					code_index += 2;
					break;
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
