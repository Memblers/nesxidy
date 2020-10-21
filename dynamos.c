#pragma section default

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "config.h"
#include "dynamos.h"
#include "exidy.h"
#include "mapper30.h"

__zpage extern uint8_t sp;
__zpage extern uint16_t pc;
__zpage uint16_t pc_end;
__zpage extern uint8_t opcode;

static uint16_t rom_remap_index = 0;
static uint8_t debug_out[0x80];
__zpage uint32_t cache_hits = 0;
__zpage uint32_t cache_misses = 0;
uint32_t cache_links = 0;
uint32_t cache_links_found = 0;
uint32_t cache_links_dropped = 0;
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
uint8_t cache_link[BLOCK_COUNT];
uint8_t cache_branch_link[BLOCK_COUNT];
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

void run_6502(void)
{		
	//cache_test();
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
			interpret_6502(); //enable_interpret();	
			return;
		}
			
		else
		{
			decimal_mode = 0;
		}
	}


	flash_cache_search(pc);
	//dispatch_on_pc();
	
	matched = 0;
	cache_search();
	if (matched == 1)
	{
		cache_hits++;
		ready();
		return;
	}

	
	// create new cache for this pc

	cache_misses++;
	
	flash_cache_index = flash_cache_select();
	if (flash_cache_index)
	{
		flash_cache_index--;
		flash_enabled = 1;
	}
	else
	{
		flash_enabled = 0;
	}
	
	
/*
	// find least-used cache		
	//cache_hit_count[cache_index]++;
	for (uint8_t i = 0, lowest = 0; i < BLOCK_COUNT; i++)
	{
		if (cache_hit_count[i] <= lowest)
		{
			next_new_cache = i;
			lowest = cache_hit_count[i];				
		}
	}
	cache_index = next_new_cache;		
*/		

	cache_index = next_new_cache;		
	// when cache is full, overwrite the oldest ones
	if (cache_index-- == 0)
		cache_index = BLOCK_COUNT-1;		
			
	code_index = 0;		

	cache_entry_pc_lo[cache_index] = (uint8_t) pc;
	cache_entry_pc_hi[cache_index] = (uint8_t) (pc >> 8);
	/*
	if (flash_enabled)
	{
		setup_flash_address(pc, flash_cache_index);
		flash_cache_pc_update(code_index);		
	}
	*/
	
	cache_flag[cache_index] = 0;
	cache_cycles[cache_index] = 0;	
	cache_hit_count[cache_index] = 0;
	
	//cache_hit_count[cache_index]++;	

	do
	{
		#ifdef TRACK_TICKS
		cache_cycles[cache_index] += ticktable[read6502(pc)];
		#else
		cache_cycles[cache_index]++;
		#endif
		
		uint16_t pc_old = pc;
		uint8_t code_index_old = code_index;
		
		recompile_opcode();
		
		if (code_index > (CODE_SIZE - 6))
		{
			cache_flag[cache_index] |= OUT_OF_CACHE;
			cache_flag[cache_index] &= ~READY_FOR_NEXT;
		}
		if (flash_enabled)
		{
			setup_flash_address(pc_old, flash_cache_index);
			if (cache_flag[cache_index] & INTERPRET_NEXT_INSTRUCTION)
				flash_cache_pc_update(code_index_old, INTERPRETED);
			else
				flash_cache_pc_update(code_index_old, RECOMPILED);
		}
		
	} while (cache_flag[cache_index] & READY_FOR_NEXT);	// need to re-add OUT_OF_CACHE_LOGIC		
	
	cache_vpc[cache_index] = code_index;
	
	if (code_index)
	{
		cache_code[cache_index][code_index++] = 0x4C; //opJMP
		cache_code[cache_index][code_index++] = (uint8_t) ((uint16_t) &dispatch_return); //opJMP
		cache_code[cache_index][code_index++] = (uint8_t) (((uint16_t) &dispatch_return) >> 8); //opJMP
		
		//flash_cache_pc_flag_clear((cache_entry_pc_lo[cache_index] | (cache_entry_pc_hi[cache_index] << 8)), (uint8_t) ~RECOMPILED);	// testing - may be interpreted
		if (flash_enabled)
			flash_cache_copy(cache_index, flash_cache_index);
	}
		
	else // code buffer empty
	{
		//flash_cache_pc_flag_clear((cache_entry_pc_lo[cache_index] | (cache_entry_pc_hi[cache_index] << 8)), ~INTERPRETED);	// testing - may be interpreted		
		bankswitch_prg(0);
		interpret_6502();
		cache_entry_pc_hi[cache_index] = 0; // cancel cache
		cache_entry_pc_lo[cache_index] = 0;
		return; // get out
	}
	
	if (cache_flag[cache_index] & INTERPRET_NEXT_INSTRUCTION)
	{
		uint16_t pc_temp = pc;
		switch (read6502(pc))
		{
			case abso:
			case absy:
			case absx:
			case ind:
			{
				pc_temp += 3;
				break;
			}	
			
			case zp:
			case zpx:
			case zpy:
			case indx:
			case indy:
			case imm:
			case rel:
			{
				pc_temp += 2;
				break;
			}
			
			case imp:
			case acc:
			{
				pc_temp += 1;
				break;
			}
			default:
		}
		cache_exit_pc_lo[cache_index] = (uint8_t) pc_temp;
		cache_exit_pc_hi[cache_index] = (uint8_t) (pc_temp >> 8);		
	}
	else
	{
		cache_exit_pc_lo[cache_index] = (uint8_t) pc;
		cache_exit_pc_hi[cache_index] = (uint8_t) (pc >> 8 );		
	}
	
	if (flash_enabled)
	{
		flash_byte_program((flash_code_address + BLOCK_CONFIG_BASE + 0), flash_code_bank, cache_exit_pc_lo[cache_index]);
		flash_byte_program((flash_code_address + BLOCK_CONFIG_BASE + 1), flash_code_bank, cache_exit_pc_hi[cache_index]);
	}		

	check_cache_links();	
/*	
	while (code_index < CODE_SIZE)	// for easier viewing in memory
		cache_code[cache_index][code_index++] = 0;		
*/			

	next_new_cache = cache_index;
	
	//cache_bit_enable(pc);
	

	ready();
	return;
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
	IO8(0x4020) = 0x27;	
	uint8_t test = (flash_cache_pc_flags[pc_jump_flag_address] && RECOMPILED);
	if (test) //(flash_cache_pc_flags[pc_jump_flag_address] && RECOMPILED);	// D7 clear if RECOMPILED
		return 0;
		
	// run!
	uint32_t full_address = (emulated_pc << 1);
	pc_jump_bank = ((emulated_pc >> 13) + BANK_PC);
	pc_jump_address = (full_address & FLASH_BANK_MASK);
	bankswitch_prg(pc_jump_bank);
	
	//const uint16_t *run_label = (uint16_t *) &run_again[0];
	IO8(0x4020) = 0x25;
	void (*code_ptr)(void) = (void*) flash_cache_pc[pc_jump_address];
	(*code_ptr)();
	return 1;
}

//============================================================================================================

uint16_t flash_cache_select()
{
	bankswitch_prg(BANK_FLASH_BLOCK_FLAGS);
	for (uint16_t i = 0; i < FLASH_CACHE_BLOCKS; i++)
	{
		if (flash_block_flags[i] & FLASH_AVAILABLE)
		{
			//flash_byte_program((uint16_t) &flash_block_flags[0] + i, mapper_prg_bank, flash_block_flags[i] & ~FLASH_AVAILABLE);
			return (i + 1);
		}
	}
	return 0;
}	

//============================================================================================================
void flash_cache_copy(uint8_t src_idx, uint16_t dest_idx)
{
	//IO8(0x4020) = 0x91;
	uint8_t i;
	for (i = 0; (i < FLASH_CACHE_BLOCK_SIZE - 6) && (i < cache_vpc[src_idx]); i++)
	{
		uint8_t data = cache_code[src_idx][i];
		flash_byte_program((flash_code_address + i), flash_code_bank, data);
	}
	flash_byte_program((flash_code_address + i + 0), flash_code_bank, 0x4C); //opJMP
	flash_byte_program((flash_code_address + i + 1), flash_code_bank, (uint8_t) ((uint16_t) &flash_dispatch_return)); //opJMP
	flash_byte_program((flash_code_address + i + 2), flash_code_bank, (uint8_t) (((uint16_t) &flash_dispatch_return) >> 8)); //opJMP
		
	bankswitch_prg(BANK_FLASH_BLOCK_FLAGS);
	flash_byte_program((uint16_t) &flash_block_flags[0] + dest_idx, mapper_prg_bank, flash_block_flags[dest_idx] & ~FLASH_AVAILABLE);
}

//============================================================================================================

void flash_cache_pc_update(uint8_t code_address, uint8_t flags)
{		
	flash_byte_program((uint16_t) &flash_cache_pc[0] + pc_jump_address + 0, pc_jump_bank, (uint8_t) flash_code_address + code_address);
	flash_byte_program((uint16_t) &flash_cache_pc[0] + pc_jump_address + 1, pc_jump_bank, (uint8_t) ((flash_code_address + code_address) >> 8));			
	
	flash_byte_program((uint16_t) &flash_cache_pc_flags[0] + pc_jump_flag_address, pc_jump_flag_bank, flash_code_bank | ((~flags) & 0xE0));
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
/*
uint16_t flash_cache_search(uint16_t emulated_pc)
{
	if (emulated_pc < 0x4000)
		bankswitch_prg(BANK_PC_FLAGS + 0);
	if (emulated_pc < 0x8000)
		bankswitch_prg(BANK_PC_FLAGS + 1);
	if (emulated_pc < 0xC000)
		bankswitch_prg(BANK_PC_FLAGS + 2);
	else
		bankswitch_prg(BANK_PC_FLAGS + 3);
	uint16_t bank_index = (emulated_pc & FLASH_BANK_MASK);
	if (flash_cache_pc_flags[bank_index])
		

}
*/
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
				if (((branch_loc + pc) >> 8) >= (cache_entry_pc_hi[cache_index]))	// only within the current cache
				{
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
			pc +=1;
			cache_flag[cache_index] |= READY_FOR_NEXT;
			return cache_flag[cache_index]; //continue; // do while
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
				}
				case indy:
				{
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

//============================================================================================================

void check_cache_links()
{
		#ifdef ENABLE_LINKING
		// see if caches can be linked together
		/*
		// if it loops into itself - sounds dangerous
		for (uint16_t i = 0; i < BLOCK_COUNT; i++)
		{
			if ((cache_entry_pc_lo[i] | (cache_entry_pc_hi[i] << 8)) == (cache_exit_pc_lo[i] | (cache_exit_pc_hi[i] << 8)))
			{
				cache_links_found++;
				cache_link[cache_index] = i;
				cache_flag[cache_index] |= LINKED;
			}
		}
		*/
		
		// if exit of this cache leads to the entrance of another
		for (uint16_t i = 0; i < BLOCK_COUNT; i++)
		{
			if ((cache_entry_pc_lo[i] | (cache_entry_pc_hi[i] << 8)) == (cache_exit_pc_lo[cache_index] | (cache_exit_pc_hi[cache_index] << 8)))
			{				
				//cache_links_found++;
				cache_link[cache_index] = i;
				cache_flag[cache_index] |= LINKED;
				//combine_caches(cache_index);
				break;
			}
		}
		
		// if another cache leads into this one
		for (uint16_t i = 0; i < BLOCK_COUNT; i++)
		{
			if ((cache_entry_pc_lo[cache_index] | (cache_entry_pc_hi[cache_index] << 8)) == (cache_exit_pc_lo[i] | (cache_exit_pc_hi[i] << 8)))
			{			
				//cache_links_found++;
				cache_link[i] = cache_index;
				cache_flag[i] |= LINKED;
			}
		}

		// if branch is linked to another cache
		for (uint16_t i = 0; i < BLOCK_COUNT; i++)
		{
			if ((cache_entry_pc_lo[i] | (cache_entry_pc_hi[i] << 8)) == (cache_branch_pc_lo[cache_index] | (cache_branch_pc_hi[cache_index] << 8)))
			{				
				//cache_links_found++;
				cache_branch_link[cache_index] = i;
				cache_flag[cache_index] |= BRANCH_LINKED;
				break;
			}
		}

		#endif // ENABLE_LINKING
}

//============================================================================================================

void ready()
{	
	//const uint16_t *run_label = (uint16_t *) &run_again[0];
	//void (*code_ptr)(void) = (void*) &cache_code[cache_index][0];
	//(*code_ptr)();		
	
	bankswitch_prg(1);
	run_again:	
	
	//if (cache_hit_count[cache_index] < 0xF0)		
//		cache_hit_count[cache_index]++;
	
	//run_loc = (uint16_t *) &cache_code[cache_index][0];		
	//run_asm();
	dispatch_cache_asm();
	
	#ifdef TRACK_TICKS
	clockticks6502 += cache_cycles[cache_index];
	#else
	frame_time += cache_cycles[cache_index];
	#endif
	
	//pc = (cache_exit_pc_lo[cache_index]) | (cache_exit_pc_hi[cache_index] << 8);
		
	if (cache_flag[cache_index] & INTERPRET_NEXT_INSTRUCTION)
	{
		bankswitch_prg(0);
		interpret_6502();					
	}
	
	#ifdef ENABLE_LINKING

	if (verify_link_type1(cache_index))
	{
		cache_index = verify_link_type1(cache_index) - 1;
		goto run_again;
	}	
	

	if (verify_link_type0(cache_index))
	{
		cache_index = verify_link_type0(cache_index) - 1;
		goto run_again;
	}	
	
	#endif // ENABLE_LINKING	
	bankswitch_prg(0);
}


//============================================================================================================
// return 0 - bad link, otherwise return link+1
uint8_t verify_link_type0(uint8_t ix)
{
	if (cache_flag[ix] & LINKED)
	{		
		if (((uint8_t) pc == cache_entry_pc_lo[cache_link[ix]])  && (((uint8_t) pc >> 8) == cache_entry_pc_hi[cache_link[ix]]))
		{			
			cache_links++;
			return (cache_link[ix]) + 1;
		}
		else
		{
			cache_link[ix] = 0;	// delete link
			cache_flag[ix] &= ~LINKED;
			cache_links_dropped++;
			return 0;
		}
	}
	return 0;
}
//============================================================================================================
uint8_t verify_link_type1(uint8_t ix)
{
	if (cache_flag[ix] & BRANCH_LINKED)
	{		
		if (((uint8_t) pc == cache_entry_pc_lo[cache_branch_link[ix]]) && (((uint8_t) pc >> 8) == cache_entry_pc_hi[cache_branch_link[ix]]))
		{			
			cache_links++;
			return (cache_branch_link[ix]) + 1;
		}
		else
		{
			cache_branch_link[ix] = 0;	// delete link
			cache_flag[ix] &= ~BRANCH_LINKED;
			cache_links_dropped++;
			return 0;
		}
	}
	return 0;
}
//============================================================================================================

// rewrite
void combine_caches(uint8_t start_with)
{
	int i = start_with;	
	uint16_t vpc_count = 0;
	
	uint8_t ix2 = 0;
	if ((verify_link_type0(i)) && (cache_flag[i] & OUT_OF_CACHE))
	{			
		uint8_t ix = 0;
		vpc_count += cache_vpc[i];
		do
		{					
			do
			{					
				l1_cache_code[ix2++] = cache_code[i][ix++];				
			} while (ix < cache_vpc[i]);
						
			uint8_t temp_i = i;
			i = cache_link[temp_i];
			
			vpc_count += cache_vpc[i]; // check size including next cache
		} while (vpc_count < CACHE_L1_CODE_SIZE);		
	}
	else
	{
		l1_cache_code[0] = 0x60;		
	}
	
}

//============================================================================================================
void decode_address_c(void)
{
	uint8_t msb_compare = encoded_address >> 8;
	if (msb_compare < 4)
		decoded_address = (uint16_t) encoded_address + (uint16_t) RAM_BASE;			
	else if (msb_compare < 0x40)	
		decoded_address = (uint16_t) (encoded_address - ROM_OFFSET) + (uint16_t) ROM_NAME;
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
	
/*
	IO8(0x4020) = 0x43;		
	uint8_t bit_mask = (1 << (addr & 3));
	addr = addr >> 3;
	bankswitch_prg(3);
	uint8_t value = bit_mask & cache_bit_array[addr];
	bankswitch_prg(0);
	return value;
*/
}

//============================================================================================================

void cache_test(void)
{
	/*
	uint8_t count;
	uint16_t ix = 128;
	for (count = 0; count < 240; count++)
		{
			cache_code[0][count] = ix++;
		}
		cache_vpc[0] = count;		
	
	uint16_t number = flash_cache_select();
	number--;
	setup_flash_address(0, 256);
	flash_cache_copy(0, 256);	
	*/
	
	for (uint16_t i = 0; i < FLASH_CACHE_BLOCKS; i++)
	{
		uint16_t ix = i;
		uint8_t count;
		uint16_t test_block;
		for (count = 0; count < 240; count++)
		{
			cache_code[0][count] = ix++;
		}
		cache_vpc[0] = count;		
		test_block = (flash_cache_select()) - 1;
		setup_flash_address(i, test_block);
		flash_cache_copy(0, test_block);			
	}
	
	for (uint16_t i = 0; i < 0xFFFF; i++)
	{				
		setup_flash_address(i, 0);
		flash_cache_pc_update(i, 0x55);
		//flash_cache_pc_flag_clear(ix, (uint8_t) ~RECOMPILED);	// testing - may be interpreted
	}	
	
	IO8(0x4020) = 0x86;	
}
