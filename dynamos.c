#pragma section default

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "config.h"
#include "dynamos.h"
#include "exidy.h"
#include "mapper30.h"
#include "core/optimizer.h"
#ifdef ENABLE_OPTIMIZER_V2
#include "core/optimizer_v2_simple.h"
#endif

// Address mode table - must be in fixed bank (default section at $C000-$FFFF)
// Cannot use table from bank1 during recompilation since flash banks are active
const uint8_t addrmodes[256] = {
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
static uint8_t debug_out[0x80];
__zpage uint32_t cache_hits = 0;
__zpage uint32_t cache_misses = 0;
// Removed: cache_links, cache_links_found, cache_links_dropped (RAM cache linking)
uint32_t cache_branches = 0;
uint32_t branch_not_compiled = 0;   // target not yet compiled
uint32_t branch_wrong_bank = 0;     // target in different flash bank
uint32_t branch_out_of_range = 0;   // native offset > 127 bytes
uint32_t branch_forward = 0;        // forward branch (can't optimize)
//static uint16_t cache_index = 0;
__zpage uint8_t cache_index = BLOCK_COUNT-1;
//static uint8_t cache_active = 0;

__zpage uint8_t next_new_cache = 0;
__zpage uint8_t matched = 0;
__zpage uint8_t decimal_mode = 0;
__zpage uint16_t flash_cache_index;
__zpage uint8_t pc_2b27_count = 0;  // Debug: count when PC = $2B27 (STA $5000)
uint8_t flash_enabled = 0;

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

// Debug: track if we've passed initial startup
__zpage uint8_t startup_complete = 0;
__zpage uint16_t last_pc_before_reset = 0;

uint32_t cache_branch_long = 0;
__zpage uint8_t indy_hit_count = 0;  // Debug: count indy case hits

extern	uint8_t flash_block_flags[];

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

// Debug: reboot detection
volatile uint8_t reboot_detected = 0;
volatile uint16_t reboot_from_pc = 0;
volatile uint16_t run_count = 0;

void run_6502(void)
{		
	//cache_test();
	
	run_count++;
	
	// Debug: Detect reboot to $2800 after startup (after 1000 calls)
	if (pc == 0x2800 && run_count > 1000)
	{
		// We've rebooted! Store where we came from
		reboot_detected = 1;
		reboot_from_pc = last_pc_before_reset;
		// Infinite loop
		for(;;) { __asm("nop"); }
	}
	last_pc_before_reset = pc;  // Track PC before each step
	
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
			bankswitch_prg(0);
			interpret_6502();
			return;
		}
		case 0:  // executed from flash
		{
			// Optimization now triggered after recompilation, not execution
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
		else if (instr_len)
		{
			flash_cache_pc_update(code_index_old, RECOMPILED);
#ifdef ENABLE_OPTIMIZER_V2
			// V2: Check if any pending branches target this PC
			uint8_t saved_bank = mapper_prg_bank;
			bankswitch_prg(1);
			opt2_notify_block_compiled(pc_old, flash_code_address + code_index_old, flash_code_bank);
			bankswitch_prg(saved_bank);
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
		
		// 3. Write epilogue (14 bytes)
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
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, (uint8_t)((uint16_t)&flash_dispatch_return));
		flash_byte_program(flash_code_address + flash_offset++, flash_code_bank, (uint8_t)(((uint16_t)&flash_dispatch_return) >> 8));
		
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
		// Write epilogue directly to flash (code_index is current offset)
		setup_flash_address(pc, flash_cache_index);  // Get flash address base
		uint8_t epilogue_start = code_index;
		flash_byte_program(flash_code_address + epilogue_start + 0, flash_code_bank, 0x85); // STA zero page
		flash_byte_program(flash_code_address + epilogue_start + 1, flash_code_bank, (uint8_t) ((uint16_t) &a));
		flash_byte_program(flash_code_address + epilogue_start + 2, flash_code_bank, 0x08); // PHP
		flash_byte_program(flash_code_address + epilogue_start + 3, flash_code_bank, 0xA9); // LDA immediate
		flash_byte_program(flash_code_address + epilogue_start + 4, flash_code_bank, (uint8_t) exit_pc);
		flash_byte_program(flash_code_address + epilogue_start + 5, flash_code_bank, 0x85); // STA zero page
		flash_byte_program(flash_code_address + epilogue_start + 6, flash_code_bank, (uint8_t) ((uint16_t) &pc));
		flash_byte_program(flash_code_address + epilogue_start + 7, flash_code_bank, 0xA9); // LDA immediate
		flash_byte_program(flash_code_address + epilogue_start + 8, flash_code_bank, (uint8_t) (exit_pc >> 8));
		flash_byte_program(flash_code_address + epilogue_start + 9, flash_code_bank, 0x85); // STA zero page
		flash_byte_program(flash_code_address + epilogue_start + 10, flash_code_bank, (uint8_t) (((uint16_t) &pc) + 1));
		flash_byte_program(flash_code_address + epilogue_start + 11, flash_code_bank, 0x4C); // JMP
		flash_byte_program(flash_code_address + epilogue_start + 12, flash_code_bank, (uint8_t) ((uint16_t) &flash_dispatch_return));
		flash_byte_program(flash_code_address + epilogue_start + 13, flash_code_bank, (uint8_t) (((uint16_t) &flash_dispatch_return) >> 8));
#endif
		
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

void flash_cache_pc_flag_clear(uint16_t emulated_pc, uint8_t flag)
{
	lookup_pc_jump_flag(emulated_pc);
	bankswitch_prg(pc_jump_flag_bank);	
	uint8_t data = flash_cache_pc_flags[pc_jump_flag_address];
	data &= flag;
	flash_byte_program((uint16_t) &flash_cache_pc_flags[0] + pc_jump_flag_address, pc_jump_flag_bank, data);	
}

//============================================================================================================

uint8_t flash_cache_search(uint16_t emulated_pc)
{	
	lookup_pc_jump_flag(emulated_pc);
	bankswitch_prg(pc_jump_flag_bank);
	uint8_t test = (flash_cache_pc_flags[pc_jump_flag_address] & RECOMPILED);
	if (test) //(flash_cache_pc_flags[pc_jump_flag_address] & RECOMPILED);	// D7 clear if RECOMPILED
		return 0; // not found
		
	// run native code, return through flash_dispatch_return
	uint32_t full_address = (emulated_pc << 1);
	pc_jump_bank = ((emulated_pc >> 13) + BANK_PC);
	pc_jump_address = (full_address & FLASH_BANK_MASK);
	bankswitch_prg(pc_jump_bank);
	
	//const uint16_t *run_label = (uint16_t *) &run_again[0];
	IO8(0x4020) = 0x25;
	uint16_t code_addr = flash_cache_pc[pc_jump_address] | 
	                     (flash_cache_pc[pc_jump_address + 1] << 8);
	void (*code_ptr)(void) = (void*) code_addr;
	(*code_ptr)();
	//unreachable, returns through flash_dispatch_return
}

//============================================================================================================

uint16_t flash_cache_select()
{
	bankswitch_prg(BANK_FLASH_BLOCK_FLAGS);
	for (uint16_t i = 0; i < FLASH_CACHE_BLOCKS; i++)
	{
		if (flash_block_flags[i] & FLASH_AVAILABLE)					
			return (i + 1);		
	}
	return 0;
}	

//============================================================================================================
void flash_cache_copy(uint8_t src_idx, uint16_t dest_idx)
{
	uint8_t i;
	for (i = 0; (i < FLASH_CACHE_BLOCK_SIZE) && (i < cache_vpc[src_idx]); i++)
	{
		uint8_t data = cache_code[src_idx][i];
		flash_byte_program((flash_code_address + i), flash_code_bank, data);
	}
	// JMP to flash_dispatch_return is now embedded in the block epilogue
		
	bankswitch_prg(BANK_FLASH_BLOCK_FLAGS);
	flash_byte_program((uint16_t) &flash_block_flags[0] + dest_idx, mapper_prg_bank, flash_block_flags[dest_idx] & ~FLASH_AVAILABLE);
}

//============================================================================================================

void flash_cache_pc_update(uint8_t code_address, uint8_t flags)
{
	// With metadata, code starts at offset 1 (after length prefix)
	uint16_t native_addr = flash_code_address + code_address + BLOCK_PREFIX_SIZE;
	
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

uint8_t recompile_opcode()
{
	uint8_t op_buffer_0;
	uint8_t op_buffer_1;
	uint8_t op_buffer_2;
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
			
			// Only try to optimize backward branches
			int8_t branch_offset = (int8_t) op_buffer_1;
			if (branch_offset >= 0)
			{
				// Forward branch - interpret
				enable_interpret();
			}
			
			// Calculate target PC for backward branch
			uint16_t target_pc = pc + 2 + branch_offset;
			
			// Save current bank state
			uint8_t saved_bank = mapper_prg_bank;
			
			// Look up target's flag
			uint8_t target_flag_bank = ((target_pc >> 14) + BANK_PC_FLAGS);
			bankswitch_prg(target_flag_bank);
			uint8_t target_flag = flash_cache_pc_flags[target_pc & FLASH_BANK_MASK];
			
			// Check if target is compiled (bit 7 clear = RECOMPILED)
			if (target_flag & RECOMPILED)
			{
#ifdef ENABLE_OPTIMIZER_V2
				// V2: Three-path branch pattern (21 bytes)
				// Save flash address NOW before any pc_old updates
				uint16_t pattern_flash_address = flash_code_address;
				uint8_t pattern_flash_bank = flash_code_bank;
				uint8_t pattern_code_index = code_index;
				
				bankswitch_prg(saved_bank);
				branch_not_compiled++;
				
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
				cache_code[cache_index][code_index+0] = invert_branch(op_buffer_0);
				cache_code[cache_index][code_index+1] = 19;  // Skip to +21
				
				// +2: Original branch $03 (to slow path at +7) - PATCHABLE to $00
				cache_code[cache_index][code_index+2] = op_buffer_0;  // original opcode
				cache_code[cache_index][code_index+3] = 3;  // offset to slow path
				
				// +4: JMP $FFFF - fast path (operand PATCHABLE)
				cache_code[cache_index][code_index+4] = 0x4C;  // JMP
				cache_code[cache_index][code_index+5] = 0xFF;  // Low byte
				cache_code[cache_index][code_index+6] = 0xFF;  // High byte
				
				// +7: STA _a
				cache_code[cache_index][code_index+7] = 0x85;  // STA zp
				cache_code[cache_index][code_index+8] = (uint8_t)((uint16_t)&a);
				
				// +9: PHP
				cache_code[cache_index][code_index+9] = 0x08;  // PHP
				
				// +10: LDA #<target_pc
				cache_code[cache_index][code_index+10] = 0xA9;  // LDA immediate
				cache_code[cache_index][code_index+11] = target_pc & 0xFF;
				
				// +12: STA _pc
				cache_code[cache_index][code_index+12] = 0x85;  // STA zp
				cache_code[cache_index][code_index+13] = (uint8_t)((uint16_t)&pc);
				
				// +14: LDA #>target_pc
				cache_code[cache_index][code_index+14] = 0xA9;  // LDA immediate
				cache_code[cache_index][code_index+15] = (target_pc >> 8) & 0xFF;
				
				// +16: STA _pc+1
				cache_code[cache_index][code_index+16] = 0x85;  // STA zp
				cache_code[cache_index][code_index+17] = (uint8_t)(((uint16_t)&pc) + 1);
				
				// +18: JMP flash_dispatch_return
				cache_code[cache_index][code_index+18] = 0x4C;  // JMP
				cache_code[cache_index][code_index+19] = (uint8_t)((uint16_t)&flash_dispatch_return);
				cache_code[cache_index][code_index+20] = (uint8_t)(((uint16_t)&flash_dispatch_return) >> 8);
				
				// Record pending patch:
				// - Branch offset at +3 (patch $03 -> $00)
				// - JMP operand at +5 (patch $FFFF -> native)
				// Use saved addresses from when pattern was emitted
				uint16_t branch_offset_addr = pattern_flash_address + BLOCK_PREFIX_SIZE + pattern_code_index + 3;
				uint16_t jmp_operand_addr = pattern_flash_address + BLOCK_PREFIX_SIZE + pattern_code_index + 5;
				// Function is in bank1, bankswitch before JSR
				bankswitch_prg(1);
				opt2_record_pending_branch(branch_offset_addr, jmp_operand_addr, pattern_flash_bank, target_pc);
				bankswitch_prg(saved_bank);  // Restore current bank
				
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
#else
				// Target not compiled - restore and interpret
				bankswitch_prg(saved_bank);
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
					bankswitch_prg(saved_bank);
					branch_wrong_bank++;
					enable_interpret();
					// Fall through to emit interpreted branch - don't try to optimize!
				}
				else
				{
				
				// Get target's native address
				uint8_t target_pc_bank = ((target_pc >> 13) + BANK_PC);
				bankswitch_prg(target_pc_bank);
				uint16_t target_pc_addr = ((target_pc << 1) & FLASH_BANK_MASK);
			uint16_t target_native = flash_cache_pc[target_pc_addr] | 
			                         (flash_cache_pc[target_pc_addr + 1] << 8);
			
			// Restore bank state
			bankswitch_prg(saved_bank);
			
			// Calculate native branch offset
			uint16_t current_native = flash_code_address + code_index + 2;
			int16_t native_offset = (int16_t)(target_native - current_native);
			
			// Check if offset fits in signed byte (-128 to +127)
			if (native_offset >= -128 && native_offset <= 127)
			{
				// Emit native branch!
				cache_code[cache_index][code_index] = op_buffer_0;
				cache_code[cache_index][code_index+1] = (uint8_t) native_offset;
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
				cache_code[cache_index][code_index] = invert_branch(op_buffer_0);
				cache_code[cache_index][code_index+1] = 3;
				
				// Emit: JMP target_native (target is compiled, we know address)
				cache_code[cache_index][code_index+2] = 0x4C;  // JMP
				cache_code[cache_index][code_index+3] = target_native & 0xFF;
				cache_code[cache_index][code_index+4] = (target_native >> 8) & 0xFF;
				
				pc += 2;
				code_index += 5;
				cache_branches++;
				cache_flag[cache_index] |= READY_FOR_NEXT;
				return cache_flag[cache_index];
			}
			// else: not enough space, fall through to interpreted epilogue
#endif
				}  // end else (same code bank)
			}  // end else (target IS compiled)
		}  // end case branches
		
		case opJMP:
		case opJSR:
		{
			cache_branch_pc_lo[cache_index] = read6502(pc+1);
			cache_branch_pc_hi[cache_index] = read6502(pc+2);
			enable_interpret();
		}
		
		case opJMPi:		
		case opRTS:
		case opRTI:
		{
			enable_interpret();
		}
		
		case opTSX:
		{
			if ((code_index + 3 + 3) < CODE_SIZE)
			{				
				cache_code[cache_index][code_index] = opLDX_ZP;
				cache_code[cache_index][code_index+1] = (uint16_t) &sp;				
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
				cache_code[cache_index][code_index] = opSTX_ZP;
				cache_code[cache_index][code_index+1] = (uint16_t) &sp;				
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
						cache_code[cache_index][code_index+i] = opcode_6502_pha[i];
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
						cache_code[cache_index][code_index+i] = opcode_6502_pla[i];
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
			cache_code[cache_index][code_index] = opNOP;
			pc += 1;
			code_index += 1;
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
			cache_code[cache_index][code_index] = op_buffer_0;
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
						cache_code[cache_index][code_index+1] = (uint8_t) decoded_address;
						cache_code[cache_index][code_index+2] = (uint8_t) (decoded_address >> 8);
					}
					else
						enable_interpret();							
		
					pc += 3;
					code_index += 3;
					break;
				}						
				
				case zp:
				case zpx:
				case zpy:												
				{															
					cache_code[cache_index][code_index] |= 0x08; // change ZP to ABS (refer to 6502 opcode matrix) - note: except for STX ZP,Y and STY ZP,X !! (interpreted for now)
					uint16_t address = read6502(pc+1);
					address += (uint16_t) &RAM_BASE[0];
					cache_code[cache_index][code_index+1] = (uint8_t) address;
					cache_code[cache_index][code_index+2] = (uint8_t) (address >> 8);
					pc += 2;
					code_index += 3;
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
							cache_code[cache_index][code_index+i] = addr_6502_indx[i];
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
					break;
				}
				
				case imm:
				case rel:
					cache_code[cache_index][code_index+1] = read6502(pc+1);								
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

// Removed: check_cache_links(), ready(), verify_link_type0(), verify_link_type1(), combine_caches()
// These were all part of the old RAM cache execution system that is no longer used.
// Flash cache execution uses dispatch_on_pc() and flash_dispatch_return instead.

// Removed: decode_address_c() - now using platform_exidy.translate_addr() from platform/platform_exidy.c

//============================================================================================================
void cache_bit_enable(uint16_t addr)
{	
	uint8_t bit_mask = ~(1 << (addr & 3));
	addr = addr >> 3;
	bankswitch_prg(3);
	uint8_t value = bit_mask & cache_bit_array[addr];
	flash_byte_program((uint16_t) &cache_bit_array[0] + addr, 3, value);
	bankswitch_prg(0);
}

//============================================================================================================
uint8_t cache_bit_check(uint16_t addr)
{
	uint8_t bit_number = addr & 3;
	uint8_t value;
	addr = addr >> 3;
	bankswitch_prg(3);
	switch (bit_number)
	{
		case 0:
			value = 0x01 & cache_bit_array[addr];
		case 1:
			value = 0x02 & cache_bit_array[addr];
		case 2:
			value = 0x04 & cache_bit_array[addr];
		case 3:
			value = 0x08 & cache_bit_array[addr];
		case 4:
			value = 0x10 & cache_bit_array[addr];
		case 5:
			value = 0x20 & cache_bit_array[addr];
		case 6:
			value = 0x40 & cache_bit_array[addr];
		case 7:
			value = 0x80 & cache_bit_array[addr];
		default:
	}
	bankswitch_prg(0);
	return value;
}

//============================================================================================================
