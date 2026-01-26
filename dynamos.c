#pragma section default

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "config.h"
#include "dynamos.h"
#include "exidy.h"
#include "mapper30.h"

__zpage extern uint8_t sp;
__zpage extern uint8_t a;
__zpage extern uint16_t pc;
__zpage uint16_t pc_end;
__zpage extern uint8_t opcode;

static uint16_t rom_remap_index = 0;
static uint8_t debug_out[0x80];
__zpage uint32_t cache_hits = 0;
__zpage uint32_t cache_misses = 0;
// Removed: cache_links, cache_links_found, cache_links_dropped (RAM cache linking)
uint32_t cache_branches = 0;
//static uint16_t cache_index = 0;
__zpage uint8_t cache_index = BLOCK_COUNT-1;
//static uint8_t cache_active = 0;

__zpage uint8_t next_new_cache = 0;
__zpage uint8_t matched = 0;
__zpage uint8_t decimal_mode = 0;
__zpage uint16_t flash_cache_index;
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

extern	uint8_t flash_block_flags[];

uint16_t pc_jump_address;
uint8_t pc_jump_bank;
uint16_t pc_jump_flag_address;
uint8_t pc_jump_flag_bank;
uint16_t flash_code_address;
uint8_t flash_code_bank;


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
	
	do
	{
		uint16_t pc_old = pc;
		uint8_t code_index_old = code_index;
		
		recompile_opcode();
		
		if (code_index > (CODE_SIZE - 6))
		{
			cache_flag[0] |= OUT_OF_CACHE;
			cache_flag[0] &= ~READY_FOR_NEXT;
		}
		
		setup_flash_address(pc_old, flash_cache_index);
		if (cache_flag[0] & INTERPRET_NEXT_INSTRUCTION)
			flash_cache_pc_update(code_index_old, INTERPRETED);
		else if (code_index)
			flash_cache_pc_update(code_index_old, RECOMPILED);
		
	} while (cache_flag[0] & READY_FOR_NEXT);
	
	if (code_index)
	{
		// Exit PC is the current pc value (instruction to interpret or continue from)
		uint16_t exit_pc = pc;
		
		// Embed exit PC and JMP to flash_dispatch_return at end of code:
		// STA _a (zp), PHP, LDA #<exit_pc, STA _pc (zp), LDA #>exit_pc, STA _pc+1 (zp), JMP flash_dispatch_return
		cache_code[0][code_index++] = 0x85; // STA zero page
		cache_code[0][code_index++] = (uint8_t) ((uint16_t) &a);
		cache_code[0][code_index++] = 0x08; // PHP - save status before LDA corrupts flags
		cache_code[0][code_index++] = 0xA9; // LDA immediate
		cache_code[0][code_index++] = (uint8_t) exit_pc;
		cache_code[0][code_index++] = 0x85; // STA zero page
		cache_code[0][code_index++] = (uint8_t) ((uint16_t) &pc);
		cache_code[0][code_index++] = 0xA9; // LDA immediate
		cache_code[0][code_index++] = (uint8_t) (exit_pc >> 8);
		cache_code[0][code_index++] = 0x85; // STA zero page
		cache_code[0][code_index++] = (uint8_t) (((uint16_t) &pc) + 1);
		cache_code[0][code_index++] = 0x4C; // JMP
		cache_code[0][code_index++] = (uint8_t) ((uint16_t) &flash_dispatch_return);
		cache_code[0][code_index++] = (uint8_t) (((uint16_t) &flash_dispatch_return) >> 8);
		
		// Update cache_vpc after adding epilogue
		cache_vpc[0] = code_index;
		
		// Copy compiled code to flash
		flash_cache_copy(0, flash_cache_index);
		
		// Restore PC to entry point and execute from flash
		pc = entry_pc;
		
		result = dispatch_on_pc();
		// dispatch_on_pc should return 0 (executed from flash)
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
	flash_byte_program((uint16_t) &flash_cache_pc[0] + pc_jump_address + 0, pc_jump_bank, (uint8_t) (flash_code_address + code_address));
	flash_byte_program((uint16_t) &flash_cache_pc[0] + pc_jump_address + 1, pc_jump_bank, (uint8_t) ((flash_code_address + code_address) >> 8));			
	
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
	
	flash_code_bank = (block_number / 0x40) + BANK_CODE ; //(uint32_t) (full_address >> 13) + BANK_CODE; //((full_address / FLASH_BANK_SIZE) + BANK_CODE);
	flash_code_address = ((block_number % 0x40) << 8) + FLASH_BANK_BASE; //((full_address & FLASH_BANK_MASK) + FLASH_BANK_BASE);
	
	full_address = (emulated_pc << 1);
	pc_jump_bank = ((emulated_pc >> 13) + BANK_PC);
	pc_jump_address = (full_address & FLASH_BANK_MASK);	

	lookup_pc_jump_flag(emulated_pc);	
	//bankswitch_prg((jump_target / FLASH_BANK_SIZE) + BANK_PC);	
}

//============================================================================================================

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
			#ifdef ENABLE_LINKING
			op_buffer_1 = read6502(pc+1);
			uint16_t branch_loc = op_buffer_1;			
			if (branch_loc & 0x80)
				branch_loc |= 0xFF00;
			branch_loc += pc+2;
			cache_branch_pc_lo[cache_index] = (uint8_t) branch_loc;
			cache_branch_pc_hi[cache_index] = (uint8_t) (branch_loc >> 8);
			
			// allow backwards branch within the cache block
			
			branch_loc = op_buffer_1;
			
			/*
			if (branch_loc & 0x80)	// only branch back	// UNSTABLE removed for now
			{								
				branch_loc |= 0xFF00;
				IO8(0x4020) = ((branch_loc + pc) >> 8);
				if (((branch_loc + pc) >> 8) >= (cache_entry_pc_hi[cache_index]))	// only within the current cache
				{					
					IO8(0x4020) = ((branch_loc + pc) & 0xFF);
					if (((branch_loc + pc) & 0xFF) >= (cache_entry_pc_lo[cache_index]))
					{						
						uint8_t temp = op_buffer_1;
						uint16_t start_pc = ((cache_entry_pc_hi[cache_index] << 8) | cache_entry_pc_lo[cache_index]);
						temp -= (code_index - ((pc + code_index) - start_pc));
						cache_branches++;
						if (temp & 0x80)	// only if branch still reaches
						{
							cache_code[cache_index][code_index] = op_buffer_0;
							cache_code[cache_index][code_index+1] = temp;
							pc += 2;
							code_index += 2;									
							break;
						}								
						else
						{							
							encoded_address = (branch_loc + pc);
							if (encoded_address >= ROM_OFFSET)
							{
								decoded_address = ((uint16_t) &cache_code[cache_index][code_index+3]) + temp;
							}
							else							
								//decode_address_asm();				
								decode_address_c();				
							
							cache_code[cache_index][code_index] = op_buffer_0 ^ 0x20;	// reverse branch condition (refer to 6502 opcode matrix)
							cache_code[cache_index][code_index+1] = 3;	// skip next JMP
							cache_code[cache_index][code_index+2] = 0x4C;							
							cache_code[cache_index][code_index+3] = (uint8_t) decoded_address;
							cache_code[cache_index][code_index+4] = (uint8_t) (decoded_address >> 8);
							pc += 2;
							code_index += 5;
							cache_branch_long++;
							break;							
						}
					}							
				}
				enable_interpret();						
			}
			else
			*/
				enable_interpret();	
			#else
				enable_interpret();
			#endif // ENABLE_LINKING
		}
		
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
					pc += 2;
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
					pc += 2;
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
					
					//decode_address_asm();
					decode_address_c();
					
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
					// Indy pointers can point to ROM in the switchable bank ($8000-$BFFF).
					// At runtime, the flash cache bank is at $8000, so ROM reads would fail.
					// Always interpret indy instructions to ensure correct bank switching.
					enable_interpret();
					break;
					
					/* Original compiled indy - disabled due to bank conflict:
					uint8_t address_8 = read6502(pc+1);
					uint16_t address = address_8;
					address += (uint16_t) &RAM_BASE[0];							
					
					if ((code_index + addr_6502_indy_size + 3) < CODE_SIZE)
					{	
						indy_opcode_location = op_buffer_0;
						indy_address_lo = address;
						indy_address_hi = address+1;
						for (uint8_t i = 0; i < addr_6502_indy_size; i++)
						{
							cache_code[cache_index][code_index+i] = addr_6502_indy[i];
						}						
						pc += 2;
						code_index += addr_6502_indy_size;
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

//============================================================================================================
void decode_address_c(void)
{
	uint8_t msb_compare = encoded_address >> 8;
	if (msb_compare < 4)
		decoded_address = (uint16_t) encoded_address + (uint16_t) RAM_BASE;			
	else if (msb_compare < 0x40)
	{
		// ROM access - calculate NES address
		decoded_address = (uint16_t) (encoded_address - ROM_OFFSET) + (uint16_t) ROM_NAME;
		// If decoded address is in the switchable bank ($8000-$BFFF), it conflicts with flash cache execution.
		// Return 0 to force interpretation so the interpreter can switch banks correctly.
		if ((decoded_address >= 0x8000) && (decoded_address < 0xC000))
			decoded_address = 0;
	}
	else if (msb_compare < 0x48)
		decoded_address = (encoded_address - 0x4000) + (uint16_t) SCREEN_RAM_BASE;
	else if (msb_compare < 0x50)
		decoded_address = (encoded_address - 0x4800) + (uint16_t) CHARACTER_RAM_BASE;
	else
		decoded_address = 0;	
}

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
