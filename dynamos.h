#ifndef DYNAMOS_H
#define DYNAMOS_H

#include "config.h"

#define	BLOCK_COUNT 8

// Block layout:
// +0: code_len (1 byte) - if OPT_BLOCK_METADATA
// +1: native code (variable)
// +N: epilogue (14 or 21 bytes)
// +N+14/21: metadata - if OPT_BLOCK_METADATA
//   exit_pc (2 bytes)
//   cycle_count (2 bytes) - if OPT_TRACK_CYCLES  
//   branch_count (1 byte)
//   branches[] (3 bytes each: offset, target_lo, target_hi)
//
// Patchable epilogue layout (21 bytes, ENABLE_PATCHABLE_EPILOGUE):
//   +0: PHP           ; 1B  save flags
//   +1: CLC           ; 1B
//   +2: BCC +4        ; 2B  always-taken → regular at +8. PATCH to +0 → fast at +4
//   +4: PLP           ; 1B  fast path: restore flags
//   +5: JMP $FFFF     ; 3B  fast path: PATCH operand → next block native addr
//   +8: STA _a        ; 2B  regular path
//  +10: LDA #<exit_pc ; 2B
//  +12: STA _pc       ; 2B
//  +14: LDA #>exit_pc ; 2B
//  +16: STA _pc+1     ; 2B
//  +18: JMP dispatch  ; 3B
//
// Patching: BCC offset 4→0 (clear bits ✓), JMP $FFFF→$XXYY (clear bits ✓)
// Fast path cost: PHP+CLC+BCC+PLP+JMP = 15 cycles (vs ~130 for full dispatch)
//
// Byte 255 of each block stores the epilogue start offset (0..~229).
// $FF = no patchable epilogue.  Used by opt2_scan_and_patch_epilogues().

#ifdef ENABLE_PATCHABLE_EPILOGUE
#define EPILOGUE_SIZE 21
#else
#define EPILOGUE_SIZE 14
#endif

#if OPT_BLOCK_METADATA
#define BLOCK_PREFIX_SIZE 1 // 1 byte for code length
#define CODE_SIZE (256 - BLOCK_PREFIX_SIZE - EPILOGUE_SIZE - 16) // room for prefix + epilogue + metadata
#else
#define BLOCK_PREFIX_SIZE 0
#define CODE_SIZE (256 - EPILOGUE_SIZE - 6) // room for epilogue + safety margin
#endif

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

#define BLOCK_CONFIG_BASE 250

#define FLASH_AVAILABLE		0x01

// Program remap status bits
#define RECOMPILED		0x80	// 0 = has been recompiled.  bits D0-D5 contain PRG bank number
#define INTERPRETED		0x40	// 0 = interpret this instruction
#define	CODE_DATA		0x20	// 0 = code, 1 = data

#define BANK_FLASH_BLOCK_FLAGS	3
#define BANK_CODE		4
#define	BANK_PC			19
#define BANK_PC_FLAGS	27

// Cache persistence signature
// Stored in bank 3 after the block flags array (960 bytes = $3C0).
// Signature sits at offset $3D0 within bank 3 (address $83D0).
// Layout: 4-byte magic + 4-byte ROM hash = 8 bytes total.
#define CACHE_SIG_OFFSET        0x3D0
#define CACHE_SIG_ADDRESS       (FLASH_BANK_BASE + CACHE_SIG_OFFSET)
#define CACHE_SIG_MAGIC_0       0x44   // 'D'
#define CACHE_SIG_MAGIC_1       0x4D   // 'M'
#define CACHE_SIG_MAGIC_2       0x53   // 'S'
#define CACHE_SIG_MAGIC_3       0x01   // version 1
#define CACHE_SIG_SIZE          8      // 4 magic + 4 ROM hash

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
__regsused("a/x/y") extern uint16_t *run_loc;
__regsused("a/x") extern void cache_search();
extern void decode_address_asm();
extern void decode_address_asm2();
//__regsused("a/x") extern void decode_address_asm2();
//__regsused("a/x") extern void decode_address_asm(__reg("a/x") uint16_t encoded_address);
// Removed: dispatch_cache_asm(), dispatch_return() - RAM cache execution (dead code)
__regsused("a/x/y") extern uint8_t dispatch_on_pc();
__regsused("a/x/y") extern void flash_dispatch_return();
__regsused("a/x/y") extern void flash_dispatch_return_no_regs();
__regsused("a/x/y") extern void cross_bank_dispatch();


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

// Native JSR template and patch offsets
extern const uint8_t opcode_6502_jsr_size;
extern uint8_t opcode_6502_jsr[];
extern const uint8_t opcode_6502_jsr_ret_hi;
extern const uint8_t opcode_6502_jsr_ret_lo;
extern const uint8_t opcode_6502_jsr_tgt_lo;
extern const uint8_t opcode_6502_jsr_tgt_hi;

// Native JSR template (trampoline mode) and patch offsets
extern const uint8_t opcode_6502_njsr_size;
extern uint8_t opcode_6502_njsr[];
extern const uint8_t opcode_6502_njsr_ret_hi;
extern const uint8_t opcode_6502_njsr_ret_lo;
extern const uint8_t opcode_6502_njsr_tgt_lo;
extern const uint8_t opcode_6502_njsr_tgt_hi;
extern void native_jsr_trampoline();
extern uint8_t native_jsr_saved_sp;

// Native RTS template
extern const uint8_t opcode_6502_nrts_size;
extern uint8_t opcode_6502_nrts[];

// STA (zp),Y native handler
extern void sta_indy_handler();
extern uint8_t sta_indy_template[];
extern const uint8_t sta_indy_template_size;
extern uint8_t sta_indy_zp_patch;

extern uint8_t flash_cache_pc[];
extern const uint8_t flash_cache_pc_flags[];


// Removed: ready(), check_cache_links(), verify_link_type0(), verify_link_type1(), combine_caches()
// These were all part of the old RAM cache execution system.
// Removed: decode_address_c() - now in platform/platform_exidy.c
uint8_t recompile_opcode();
void cache_bit_enable(uint16_t addr);
uint8_t cache_bit_check(uint16_t addr);
uint16_t flash_cache_select();
void flash_cache_pc_update(uint8_t code_address, uint8_t flags);
void setup_flash_address(uint16_t emulated_pc, uint16_t block_number);
uint8_t flash_cache_search(uint16_t emulated_pc);

// Block reservation API for eager allocation
#define MAX_RESERVATIONS 32
extern uint16_t reserved_pc[];
extern uint16_t reserved_block[];
extern uint8_t reservation_count;
extern uint8_t reservations_enabled;
uint16_t reserve_block_for_pc(uint16_t target_pc);
int16_t consume_reservation(uint16_t target_pc);
uint8_t reserve_block_for_pc_safe(uint16_t target_pc);
extern uint16_t reserve_result_addr;
extern uint8_t  reserve_result_bank;

enum 6502op
{
	opBRK = 0x00,
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
	
	opNOP = 0xEA,
	opRTI = 0x40
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