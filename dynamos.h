#ifndef DYNAMOS_H
#define DYNAMOS_H

#define	BLOCK_COUNT 14
#define CODE_SIZE 250

#define CACHE_L1_CODE_SIZE 256

#define ZP_CACHE_SIZE	4
#define ZP_CACHE_PTRS	(ZP_CACHE_SIZE / 2)

#define FLAG_CONSTANT  0x20

#define PC_CHANGE	1
#define INTERPRET_NEXT_INSTRUCTION	2
#define OUT_OF_CACHE	4
#define LINKED	8
#define BRANCH_LINKED	16
#define READY_FOR_NEXT	32

#define FLASH_CACHE_MEMORY_SIZE	0x3C000
#define FLASH_CACHE_BLOCK_SIZE	0x100
#define	FLASH_ERASE_SECTOR_SIZE	0x1000
#define FLASH_BANK_BASE			0x8000
#define FLASH_BANK_SIZE			0x4000
#define FLASH_BANK_MASK			0x3FFF
#define FLASH_CACHE_BLOCKS		(FLASH_CACHE_MEMORY_SIZE / FLASH_CACHE_BLOCK_SIZE)
#define FLASH_CACHE_BANKS		(FLASH_CACHE_MEMORY_SIZE / FLASH_BANK_SIZE)

#define FLASH_AVAILABLE		0x01

// Program remap status bits
#define RECOMPILED		0x80	// 0 = has been recompiled.  bits D0-D5 contain PRG bank number
#define INTERPRETED		0x40	// 0 = interpret this instruction
#define	CODE_DATA		0x20	// 0 = code, 1 = data

#define BANK_FLASH_BLOCK_FLAGS	3
#define BANK_CODE		4
#define	BANK_PC			19
#define BANK_PC_FLAGS	27

#pragma section bank1
extern const unsigned char rom_sidetrac[];
extern const unsigned char rom_targ[];
extern const unsigned char rom_targtest[];
extern const unsigned char rom_spectar[];
#pragma section default
//extern const unsigned char ROM_NAME[];
extern void interpret_6502();
extern void run_6502();
extern const uint8_t ticktable[];
__zpage extern uint32_t frame_time;

extern uint8_t RAM_BASE[];
extern uint8_t SCREEN_RAM_BASE[];
extern uint8_t CHARACTER_RAM_BASE[];

extern void disassemble();
extern uint8_t flash_cache_code[];

extern uint8_t mapper_prg_bank;
extern uint8_t mapper_chr_bank;
extern uint8_t mapper_register;

//#pragma section text0

// asm functions
__regsused("a/x/y") extern void run_asm();
__regsused("a/x/y") extern uint16_t *run_loc;
__regsused("a/x") extern void cache_search();
extern void decode_address_asm();
extern void decode_address_asm2();
//__regsused("a/x") extern void decode_address_asm2();
//__regsused("a/x") extern void decode_address_asm(__reg("a/x") uint16_t encoded_address);
__regsused("a/x/y") extern void dispatch_cache_asm();
__regsused("a/x/y") extern void dispatch_return();


extern uint8_t addr_6502_indy[];
extern const uint8_t addr_6502_indy_size;
extern uint8_t indy_opcode, indy_opcode_location;
extern uint16_t indy_address_lo, indy_address_hi;

extern uint8_t addr_6502_indx[];
extern const uint8_t addr_6502_indx_size;
extern uint8_t indx_opcode, indx_opcode_location;
extern uint16_t indx_address_lo, indx_address_hi;

extern const uint8_t opcode_6502_pha_size;
extern const uint8_t opcode_6502_pla_size;
extern uint8_t opcode_6502_pha[];
extern uint8_t opcode_6502_pla[];


extern uint8_t flash_cache_pc[];
extern uint8_t flash_cache_pc_flags[];


void ready();
uint8_t recompile_opcode();
void check_cache_links();
uint8_t verify_link_type0(uint8_t ix);
uint8_t verify_link_type1(uint8_t ix);
void combine_caches(uint8_t start_with);
void decode_address_c(void);
void cache_bit_enable(uint16_t addr);
uint8_t cache_bit_check(uint16_t addr);
uint16_t flash_cache_select();
void flash_cache_pc_update(uint8_t code_address);
void flash_cache_copy(uint8_t src_idx, uint16_t dest_idx);
void setup_flash_address(uint16_t emulated_pc, uint16_t block_number);
uint8_t flash_cache_search(uint16_t emulated_pc);
void flash_cache_pc_flag_clear(uint16_t emulated_pc, uint8_t flag);
void cache_test(void);

enum 6502op
{
	opJMP = 0x4C,
	opJMPi = 0x6C,
	opJSR = 0x20,
	opRTS = 0x60,
	opBPL = 0x10,
	opBMI = 0x30,
	opBVC = 0x50,
	opBVS = 0x70,
	opBCC = 0x90,
	opBCS = 0xB0,
	opBNE = 0xD0,
	opBEQ = 0xF0,	
	
	opTXS = 0x9A,
	opTSX = 0xBA,
	
	opSEI = 0x78,
	opCLI = 0x58,
	opSED = 0xF8,
	opCLD = 0xD8,
	
	opSTA_ZP = 0x85,
	opSTA_ABS = 0x8D,
	opSTA_INDX = 0x81,
	opSTA_INDY = 0x91,
	opSTX_ABS = 0x8E,
	opSTX_ZP = 0x86,
	
	opLDA_ABS = 0xAD,
	opLDA_ZP = 0xA5,
	opLDX_ABS = 0xAE,
	opLDX_ZP = 0xA6,
	
	opPHA = 0x48,
	opPLA = 0x68,
	opPHP = 0x08,
	opPLP = 0x28,
	
	opSTX_ZPY = 0x96,
	opSTY_ZPX = 0x94,
	
	opNOP = 0xEA
};

enum address_modes { imp, acc, imm, zp, zpx, zpy, rel, abso, absx, absy, ind, indx, indy };



#define enable_interpret()\
{\
	cache_flag[cache_index] |= INTERPRET_NEXT_INSTRUCTION;\
	cache_flag[cache_index] &= ~READY_FOR_NEXT;\
	return cache_flag[cache_index];\
}

/*
#define enable_interpret()\
{\
	cache_flag[cache_index] |= INTERPRET_NEXT_INSTRUCTION;\	
	goto block_complete;\
}
*/


#endif
#pragma section default