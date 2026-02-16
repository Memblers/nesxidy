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
__zpage uint16_t flash_cache_index;
__zpage uint8_t pc_2b27_count = 0;  // Debug: count when PC = $2B27 (STA $5000)
uint8_t flash_enabled = 0;

// Monotonic cursor for flash_cache_select: remembers where the last
// allocation was, so subsequent calls skip already-used blocks.
// Blocks are never freed (flash can only clear bits), so previously
// scanned entries are guaranteed to still be occupied.
uint16_t next_free_block = 0;

// Block reservation system: pre-allocate flash blocks for branch/JMP targets
// so the compiler can emit direct native jumps instead of patchable templates.
// Used during batch compile (sa_run) where all targets are compiled in-order.
#define MAX_RESERVATIONS 32
uint16_t reserved_pc[MAX_RESERVATIONS];
uint16_t reserved_block[MAX_RESERVATIONS];
uint8_t reservation_count = 0;
uint8_t reservations_enabled = 0;  // gate: only allow reservations during batch compile

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

	// Blocks in use: next_free_block is a monotonic cursor — all entries
	// below it are used (blocks are never freed on flash).
	extern uint16_t next_free_block;
	*(volatile uint16_t*)(p + 0x26) = next_free_block;

	// Frame counter (global in BSS/WRAM — static in bank2 flash would be read-only)
	extern uint16_t stats_frame;
	*(volatile uint16_t*)(p + 0x28) = stats_frame++;

	// Magic signature
	p[0x2A] = 0xDB;
	p[0x2B] = 0x57;

	// SA compile counter
	extern uint16_t sa_blocks_total;
	*(volatile uint16_t*)(p + 0x2C) = sa_blocks_total;
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
	
	// Debug: track when we enter IRQ handler
	if (pc == 0x2B0E) pc_2b27_count++;
	
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
			return;
		}
		case 0:  // executed from flash
		{
			cache_hits++;
			return;
		}
		case 1:  // recompile needed
		{			
		}
	}
	
	// Compile directly to flash
	cache_misses++;
	
	flash_cache_index = flash_cache_select();
	if (flash_cache_index)
	{
		flash_cache_index--;
	}
	else
	{
		// No flash available, fall back to interpreter
		bankswitch_prg(0);
		interpret_6502();
		return;
	}
	
	// Save the entry PC before compilation
	uint16_t entry_pc = pc;
	
	// Use cache index 0 as temporary compilation buffer
	cache_index = 0;
	code_index = 0;		

	cache_flag[0] = 0;
	
	// Set up flash address BEFORE compilation loop so flash_code_bank is valid
	setup_flash_address(pc, flash_cache_index);

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
		
		recompile_opcode();
		
		uint8_t instr_len = code_index - code_index_old;

#if OPT_BLOCK_METADATA
		// Track branches for metadata
		uint8_t op = read6502(pc_old);
		if ((op==0x10||op==0x30||op==0x50||op==0x70||op==0x90||op==0xB0||op==0xD0||op==0xF0) 
		    && branch_count < 16) {
			int8_t off = read6502(pc_old + 1);
			uint16_t target = pc_old + 2 + off;
			// The branch operand is at code_index_old + 1 (opcode at +0, operand at +1)
			branch_offsets[branch_count] = code_index_old + 1;
			branch_targets[branch_count] = target;
			branch_count++;
		}
		// Must call setup_flash_address to update pc_jump_address for flash_cache_pc_update
		setup_flash_address(pc_old, flash_cache_index);
#else
		// Write this instruction's bytes to flash immediately (old behavior)
		setup_flash_address(pc_old, flash_cache_index);
		for (uint8_t i = 0; i < instr_len; i++)
		{
			flash_byte_program(flash_code_address + code_index_old + i, flash_code_bank, cache_code[0][code_index_old + i]);
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
	
	if (code_index)
	{
		// Exit PC is the current pc value (instruction to interpret or continue from)
		uint16_t exit_pc = pc;
		
#if OPT_BLOCK_METADATA
		setup_flash_address(entry_pc, flash_cache_index);  // Get flash address base
		
		// --- Write everything to flash at once with prefix and metadata ---
		uint8_t flash_offset = 0;
		
		// 1. Write code length prefix
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, code_index);
		
		// 2. Write compiled code
		for (uint8_t i = 0; i < code_index; i++) {
			flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, cache_code[0][i]);
		}
		
		// 3. Write epilogue
#ifdef ENABLE_PATCHABLE_EPILOGUE
		uint8_t epilogue_flash_offset = flash_offset;  // save for byte 255 marker
		// Patchable epilogue (21 bytes)
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, 0x08); // PHP
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, 0x18); // CLC
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, 0x90); // BCC
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, 4);    // offset → regular at +8
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, 0x28); // PLP (fast)
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, 0x4C); // JMP (fast)
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, 0xFF); // JMP lo (PATCHABLE)
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, 0xFF); // JMP hi (PATCHABLE)
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, 0x85); // STA _a (regular)
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, (uint8_t)((uint16_t)&a));
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, 0xA9); // LDA #<exit_pc
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, (uint8_t)exit_pc);
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, 0x85); // STA _pc
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, (uint8_t)((uint16_t)&pc));
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, 0xA9); // LDA #>exit_pc
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, (uint8_t)(exit_pc >> 8));
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, 0x85); // STA _pc+1
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, (uint8_t)(((uint16_t)&pc) + 1));
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, 0x4C); // JMP cross_bank_dispatch
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, (uint8_t)((uint16_t)&cross_bank_dispatch));
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, (uint8_t)(((uint16_t)&cross_bank_dispatch) >> 8));
		// Epilogue chaining is handled by opt2_scan_and_patch_epilogues() — no queue needed
		// Write epilogue start offset to byte 255 so scanner can find it directly
		flash_byte_program(flash_code_address + 255, flash_code_bank, epilogue_flash_offset);
#else
		// Standard epilogue (14 bytes)
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, 0x85); // STA zp
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, (uint8_t)((uint16_t)&a));
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, 0x08); // PHP
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, 0xA9); // LDA #
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, (uint8_t)exit_pc);
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, 0x85); // STA zp
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, (uint8_t)((uint16_t)&pc));
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, 0xA9); // LDA #
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, (uint8_t)(exit_pc >> 8));
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, 0x85); // STA zp
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, (uint8_t)(((uint16_t)&pc) + 1));
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, 0x4C); // JMP
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, (uint8_t)((uint16_t)&cross_bank_dispatch));
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, (uint8_t)(((uint16_t)&cross_bank_dispatch) >> 8));
#endif  // ENABLE_PATCHABLE_EPILOGUE
		
		// 4. Write metadata: exit_pc
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, (uint8_t)exit_pc);
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, (uint8_t)(exit_pc >> 8));
		
#if OPT_TRACK_CYCLES
		// 5. Write cycle count (TODO: implement actual cycle tracking)
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, 0);
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, 0);
#endif
		
		// 6. Write branch info
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, branch_count);
		for (uint8_t i = 0; i < branch_count; i++) {
			flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, branch_offsets[i]);
			flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, (uint8_t)branch_targets[i]);
			flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, (uint8_t)(branch_targets[i] >> 8));
		}
		
#else
		// --- Old behavior: epilogue only, no prefix ---
		// Build epilogue into cache_code buffer, then write everything to flash at once
		setup_flash_address(pc, flash_cache_index);  // Get flash address base
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

		// Write epilogue to flash
		for (uint8_t i = epilogue_start; i < code_index; i++) {
			flash_byte_program(flash_code_address + i, flash_code_bank, cache_code[0][i]);
		}
#ifdef ENABLE_PATCHABLE_EPILOGUE
		// Write epilogue start offset to byte 255 so scanner can find it directly
		flash_byte_program(flash_code_address + 255, flash_code_bank, epilogue_start);
#endif
		// Epilogue chaining is handled by opt2_scan_and_patch_epilogues() — no queue needed
#endif  // OPT_BLOCK_METADATA
		
		// Mark block as used
		bankswitch_prg(BANK_FLASH_BLOCK_FLAGS);
		flash_byte_program((uint16_t) &flash_block_flags[0] + flash_cache_index, mapper_prg_bank, flash_block_flags[flash_cache_index] & ~FLASH_AVAILABLE);
		
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
	else // code buffer empty
	{
		bankswitch_prg(0);
		interpret_6502();
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

uint16_t flash_cache_select()
{
	bankswitch_prg(BANK_FLASH_BLOCK_FLAGS);
	for (uint16_t i = next_free_block; i < FLASH_CACHE_BLOCKS; i++)
	{
		if (flash_block_flags[i] & FLASH_AVAILABLE)
		{
			next_free_block = i + 1;
			return (i + 1);
		}
	}
	return 0;
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
// in the bitmap.
// Must be called from fixed bank context (default section).
uint16_t reserve_block_for_pc(uint16_t target_pc)
{
	// Check for duplicate: if target_pc is already reserved, return the
	// existing block.  Multiple blocks can branch/JMP to the same target;
	// all should use the same reserved block.
	for (uint8_t i = 0; i < reservation_count; i++)
	{
		if (reserved_pc[i] == target_pc)
			return reserved_block[i] + 1;  // return 1-based
	}

	// Verify target is in the bitmap (BFS identified it as code).
	// This guarantees the forward scan will reach it and compile it.
	{
		uint16_t byte_offset = target_pc >> 3;
		uint8_t bit_mask = 1 << (target_pc & 7);
		extern uint8_t sa_code_bitmap[];
		uint8_t val = peek_bank_byte(BANK_FLASH_BLOCK_FLAGS,
		                             (uint16_t)&sa_code_bitmap[0] + byte_offset);
		if (val & bit_mask)
			return 0;  // target not in bitmap — can't guarantee compilation
	}

	if (reservation_count >= MAX_RESERVATIONS)
		return 0;

	uint16_t block_1based = flash_cache_select();
	if (!block_1based)
		return 0;
	uint16_t block = block_1based - 1;

	// Mark block as used in flash_block_flags
	bankswitch_prg(BANK_FLASH_BLOCK_FLAGS);
	flash_byte_program((uint16_t)&flash_block_flags[0] + block,
	                   mapper_prg_bank,
	                   flash_block_flags[block] & ~FLASH_AVAILABLE);

	// Record the reservation (no PC flags — written when block is compiled)
	reserved_pc[reservation_count] = target_pc;
	reserved_block[reservation_count] = block;
	reservation_count++;

	return block_1based;
}

// Initialize reservation system. Called once at boot before sa_run.
// Clears all reservation state and disables new reservations.
void init_reservations(void)
{
	uint8_t i;
	reservation_count = 0;
	reservations_enabled = 0;
	
	// Zero out reservation arrays to ensure clean state
	for (i = 0; i < 32; i++)
	{
		reserved_pc[i] = 0;
		reserved_block[i] = 0;
	}
}

// Check if target_pc has a reservation.  If so, remove it from the
// reservation list and return the 0-based block number.  Returns -1 if
// no reservation exists.
int16_t consume_reservation(uint16_t target_pc)
{
	for (uint8_t i = 0; i < reservation_count; i++)
	{
		if (reserved_pc[i] == target_pc)
		{
			int16_t block = (int16_t)reserved_block[i];
			// Swap-remove
			reservation_count--;
			reserved_pc[i] = reserved_pc[reservation_count];
			reserved_block[i] = reserved_block[reservation_count];
			return block;
		}
	}
	return -1;
}

// Fixed-bank trampoline for reserve_block_for_pc — callable from bank 2.
// Reserves a block, sets reserve_result_* globals with the native address
// info so the caller can emit direct branches/jumps.
// Returns non-zero on success, 0 on failure.
uint16_t reserve_result_addr;    // native entry address of reserved block
uint8_t  reserve_result_bank;    // flash bank of reserved block

uint8_t reserve_block_for_pc_safe(uint16_t target_pc)
{
	// Reservation system disabled — to be re-evaluated later.
	// The patchable epilogue handles forward branches fine;
	// reservations saved 16 bytes/block but caused boot crashes.
	(void)target_pc;
	return 0;

	uint8_t saved_prg_bank = mapper_prg_bank;

	// reserve_block_for_pc no longer calls setup_flash_address, so it
	// doesn't clobber flash_code_address / flash_code_bank.
	uint16_t block_1based = reserve_block_for_pc(target_pc);

	if (!block_1based)
	{
		bankswitch_prg(saved_prg_bank);
		return 0;
	}
	uint16_t block = block_1based - 1;
	reserve_result_bank = (block / 0x40) + BANK_CODE;
	reserve_result_addr = ((block % 0x40) << 8) + FLASH_BANK_BASE + BLOCK_PREFIX_SIZE;
	bankswitch_prg(saved_prg_bank);
	return 1;
}

//============================================================================================================

void flash_cache_pc_update(uint8_t code_address, uint8_t flags)
{
	// With metadata, code starts at offset 1 (after length prefix)
	uint16_t native_addr = flash_code_address + code_address + BLOCK_PREFIX_SIZE;
	
	// Guard against flash AND corruption: flash can only clear bits (1→0).
	// If this PC was already programmed as RECOMPILED (bit 7 clear, non-zero),
	// reprogramming would AND the old and new values, corrupting both the
	// native address and the bank/flag byte.  This happens when the same
	// emulated PC appears in two different compiled blocks (e.g. a conditional
	// branch merges into a shared instruction).  Skip the update — the
	// existing entry is still valid.
	uint8_t saved_bank = mapper_prg_bank;
	bankswitch_prg(pc_jump_flag_bank);
	uint8_t current_flag = flash_cache_pc_flags[pc_jump_flag_address];
	bankswitch_prg(saved_bank);
	if (current_flag != 0xFF && !(current_flag & RECOMPILED))
		return;  // already a valid RECOMPILED entry — don't corrupt it
	
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

void setup_flash_address(uint16_t emulated_pc, uint16_t block_number)
{
	uint32_t full_address;
	//full_address = (block_number * FLASH_CACHE_BLOCK_SIZE);
	
	flash_code_bank = (block_number / 0x40) + BANK_CODE ; // 64 blocks per 16KB bank (256 bytes each)
	flash_code_address = ((block_number % 0x40) << 8) + FLASH_BANK_BASE; // block * 256
	
	full_address = (emulated_pc << 1);
	pc_jump_bank = ((emulated_pc >> 13) + BANK_PC);
	pc_jump_address = (full_address & FLASH_BANK_MASK);	

	lookup_pc_jump_flag(emulated_pc);	
	//bankswitch_prg((jump_target / FLASH_BANK_SIZE) + BANK_PC);	
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

static uint8_t recompile_opcode_b2()
{
	// WRAM helper for cross-bank reads (safe from bank2)
	extern uint8_t peek_bank_byte(uint8_t bank, uint16_t addr);

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
				// Forward branch — target not yet compiled.
				// Try to reserve a flash block for the target so we can emit
				// a direct native branch/JMP instead of a 21-byte template.
				uint16_t target_pc = pc + 2 + branch_offset;

#ifdef ENABLE_OPTIMIZER_V2
				branch_forward++;

				// --- Try reservation: emit 5-byte direct pattern ---
				// reserve_block_for_pc_safe is a fixed-bank trampoline (safe from bank2)
				if ((code_index + 5 + EPILOGUE_SIZE + 6) < CODE_SIZE &&
				    reserve_block_for_pc_safe(target_pc) &&
				    reserve_result_bank == flash_code_bank)
				{
					// Same bank — emit inverted branch + direct JMP (5 bytes)
					code_ptr[code_index+0] = invert_branch(op_buffer_0);
					code_ptr[code_index+1] = 3;  // skip over JMP

					code_ptr[code_index+2] = 0x4C;  // JMP
					code_ptr[code_index+3] = reserve_result_addr & 0xFF;
					code_ptr[code_index+4] = (reserve_result_addr >> 8) & 0xFF;

					pc += 2;
					code_index += 5;
					cache_branches++;
					cache_flag[cache_index] |= READY_FOR_NEXT;
					return cache_flag[cache_index];
				}

				// --- Fallback: 21-byte patchable template ---
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
				// Target IS compiled - check if same flash code bank
				uint8_t target_code_bank = target_flag & 0x1F;
				if (target_code_bank != flash_code_bank)
				{
					// Different bank - cannot direct jump, must interpret
					branch_wrong_bank++;
					enable_interpret();
					// Fall through to emit interpreted branch - don't try to optimize!
				}
				else
				{
				
				// Get target's native address via peek_bank_byte (safe from bank2)
				uint8_t target_pc_bank = ((target_pc >> 13) + BANK_PC);
				uint16_t target_pc_addr = ((target_pc << 1) & FLASH_BANK_MASK);
			uint16_t target_native = peek_bank_byte(target_pc_bank, (uint16_t)&flash_cache_pc[target_pc_addr])
			                         | (peek_bank_byte(target_pc_bank, (uint16_t)&flash_cache_pc[target_pc_addr + 1]) << 8);
			
			// Calculate native branch offset
			uint16_t current_native = flash_code_address + code_index + 2;
			int16_t native_offset = (int16_t)(target_native - current_native);
			
			// Check if offset fits in signed byte (-128 to +127)
			if (native_offset >= -128 && native_offset <= 127)
			{
				// Emit native branch!
				code_ptr[code_index] = op_buffer_0;
				code_ptr[code_index+1] = (uint8_t) native_offset;
				pc += 2;
				code_index += 2;
				cache_branches++;
				cache_flag[cache_index] |= READY_FOR_NEXT;
				return cache_flag[cache_index];
			}
			
			// Out of range - just interpret (both V1 and V2)
			branch_out_of_range++;
			enable_interpret();

#if 1  // 5-byte pattern - re-enabled with space check fix
			// V2: Out of range - use inverted branch + JMP pattern
			// Check space: need 5 bytes + epilogue
			if ((code_index + 5 + 14) < CODE_SIZE)
			{
				// Emit: inverted_branch +3 (skip over JMP)
				code_ptr[code_index] = invert_branch(op_buffer_0);
				code_ptr[code_index+1] = 3;
				
				// Emit: JMP target_native (target is compiled, we know address)
				code_ptr[code_index+2] = 0x4C;  // JMP
				code_ptr[code_index+3] = target_native & 0xFF;
				code_ptr[code_index+4] = (target_native >> 8) & 0xFF;
				
				pc += 2;
				code_index += 5;
				cache_branches++;
				cache_flag[cache_index] |= READY_FOR_NEXT;
				return cache_flag[cache_index];
			}
			// else: not enough space, fall through to interpreted epilogue
#endif
				}  // end else (same bank)
			}  // end else (target is compiled)
			}  // end else (backward branch)
		}
		
		case opJMP:
		{
			uint16_t target_pc = (uint16_t)read6502(pc+1) | ((uint16_t)read6502(pc+2) << 8);

			// --- Try reservation: emit direct JMP (3 bytes) ---
			// Only for forward targets — backward targets that aren't
			// already compiled are not in the bitmap.
			if (0 && target_pc > pc &&
			    code_index + 3 < CODE_SIZE &&
			    reserve_block_for_pc_safe(target_pc) &&
			    reserve_result_bank == flash_code_bank)
			{
				// Same bank — direct JMP to reserved block (3 bytes)
				code_ptr[code_index+0] = 0x4C;  // JMP
				code_ptr[code_index+1] = reserve_result_addr & 0xFF;
				code_ptr[code_index+2] = (reserve_result_addr >> 8) & 0xFF;

				pc = target_pc;
				code_index += 3;
				cache_flag[cache_index] &= ~READY_FOR_NEXT;  // JMP ends the block
				return cache_flag[cache_index];
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
		{
			if ((code_index + opcode_6502_pha_size + 3) < CODE_SIZE)
			{
				for (uint8_t i = 0; i < opcode_6502_pha_size; i++)
					{
						code_ptr[code_index+i] = opcode_6502_pha[i];
					}					
					pc += 1;  // PHA is a 1-byte instruction
					code_index += opcode_6502_pha_size;
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
		case opPLA:				
		{
			if ((code_index + opcode_6502_pla_size + 3) < CODE_SIZE)
			{
				for (uint8_t i = 0; i < opcode_6502_pla_size; i++)
					{
						code_ptr[code_index+i] = opcode_6502_pla[i];
					}					
					pc += 1;  // PLA is a 1-byte instruction
					cache_flag[cache_index] |= READY_FOR_NEXT;
					code_index += opcode_6502_pla_size;
					return cache_flag[cache_index];
			}
			else
			{
				cache_flag[cache_index] |= (OUT_OF_CACHE | INTERPRET_NEXT_INSTRUCTION);
				cache_flag[cache_index] &= ~READY_FOR_NEXT;
				return cache_flag[cache_index];
			}																				
		}
		case opCLI:
		case opSEI:
		case opPHP:
		case opPLP:
		{
			enable_interpret();
		}		
		case opSTX_ZPY:
		case opSTY_ZPX:
		{			
			enable_interpret();
		}
		
		case opSED:
		{
			decimal_mode = 1;
			enable_interpret();
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
