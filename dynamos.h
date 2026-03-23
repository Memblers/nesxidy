#ifndef DYNAMOS_H
#define DYNAMOS_H

#include "config.h"

#define	BLOCK_COUNT 1

// Free-form flash code cache layout:
//
// Each compiled block is stored contiguously in a 4KB flash sector:
//   [header (8 bytes)] [native code (variable)] [epilogue (21+18 bytes)]
//
// Block header (block_header_t, 8 bytes):
//   +0: entry_pc   (2B) - guest PC at block start
//   +2: exit_pc    (2B) - guest PC at block end
//   +4: code_len   (1B) - total bytes of native code + epilogue
//   +5: epilogue_offset (1B) - offset from code start to patchable epilogue
//   +6: flags      (1B) - cycle count when ENABLE_BLOCK_CYCLES, else $FF
//   +7: sentinel   (1B) - $AA = block complete; $FF = incomplete/erased
//
// PC table entries point past the header to native code start.
// Code entry points are aligned to 16-byte boundaries.
// Header starts at (aligned_addr - BLOCK_HEADER_SIZE).
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

#ifdef ENABLE_PATCHABLE_EPILOGUE
#define EPILOGUE_SIZE 21
#else
#define EPILOGUE_SIZE 14
#endif

// XBANK epilogue stub size (cross-bank fast-path setup)
#define XBANK_EPILOGUE_SIZE 18

// Block header size (must match block_header_t in core/cache.h)
#define BLOCK_HEADER_SIZE 8

// Code entry point alignment (16 bytes)
#define BLOCK_ALIGNMENT      16
#define BLOCK_ALIGNMENT_MASK (BLOCK_ALIGNMENT - 1)

// Block prefix = header size (PC table points past header to code)
#define BLOCK_PREFIX_SIZE BLOCK_HEADER_SIZE

// Block-complete sentinel value written to header offset 7 after all code
// bytes are in flash.  Dispatch checks this before jumping to the block.
#define BLOCK_SENTINEL 0xAA

// Max native code per block (keep staging buffer <= 256 for now)
#define CODE_SIZE (256 - EPILOGUE_SIZE - XBANK_EPILOGUE_SIZE - 6)

// Buffer size for cache_code[] — must hold CODE_SIZE + epilogue bytes
#define CACHE_CODE_BUF_SIZE (CODE_SIZE + EPILOGUE_SIZE + XBANK_EPILOGUE_SIZE)

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

#define FLASH_CACHE_MEMORY_SIZE	0x34000
#define	FLASH_ERASE_SECTOR_SIZE	0x1000
#define FLASH_BANK_BASE			0x8000
#define FLASH_BANK_SIZE			0x4000
#define FLASH_BANK_MASK			0x3FFF
#define FLASH_CACHE_BANKS		(FLASH_CACHE_MEMORY_SIZE / FLASH_BANK_SIZE)

// Sector-based allocation (from cache.h)
#define FLASH_SECTORS_PER_BANK    (FLASH_BANK_SIZE / FLASH_ERASE_SECTOR_SIZE)
#define FLASH_CACHE_SECTORS       (FLASH_CACHE_BANKS * FLASH_SECTORS_PER_BANK)
#define FLASH_SECTOR_MASK         0x0FFF

#define FLASH_AVAILABLE		0x01

// Program remap status bits
#define RECOMPILED		0x80	// 0 = has been recompiled.  bits D0-D5 contain PRG bank number
#define INTERPRETED		0x40	// 0 = interpret this instruction
#define	CODE_DATA		0x20	// 0 = code, 1 = data

#include "bank_map.h"

// Legacy aliases removed — now defined in bank_map.h:
//   BANK_FLASH_BLOCK_FLAGS, BANK_CODE, BANK_ENTRY_LIST,
//   BANK_PC, BANK_PC_FLAGS, BANK_RENDER, BANK_PLATFORM_ROM

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

// Recompile-requested signature
// Stored at offset $3C8 in bank 3.  WARNING: this address falls inside
// cache_bit_array ($8000-$9FFF), which is actively written by the JIT.
// The write routine (write_recompile_signature_b21) must sector-erase
// before programming to avoid NOR-flash AND-corruption from stale bits.
// Written by reset triggers (cache pressure / Select+Start hold) before
// soft-reset; checked by boot code to detect a deliberate recompile cycle.
// Value $55 = "recompile requested".  Cleared by writing $00 (burn bits).
#define RECOMPILE_SIG_OFFSET     0x3C8
#define RECOMPILE_SIG_ADDRESS    (FLASH_BANK_BASE + RECOMPILE_SIG_OFFSET)
#define RECOMPILE_SIG_VALUE      0x55

// Cache pressure threshold: ~80% of available sectors triggers auto-reset.
// FLASH_CACHE_SECTORS = 52 (13 banks × 4 sectors), threshold ≈ 42.
#define FLASH_CACHE_PRESSURE_THRESHOLD  ((FLASH_CACHE_SECTORS * 4) / 5)

#pragma section bank23
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
__regsused("a/x/y") extern void flash_dispatch_return_status_saved();
__regsused("a/x/y") extern void cross_bank_dispatch();
__regsused("a/x/y") extern void xbank_trampoline();
extern uint8_t xbank_addr;

#ifdef PLATFORM_NES
// Flash data copy for indexed ROM reads — see nes_rom_data_copy() in dynamos.c.
// ROM data is copied into the flash cache sector at compile time.
// No runtime bank-switching needed.
extern uint8_t nes_rom_copy_bank;   // set by nes_rom_data_copy()
#endif


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
extern const uint8_t opcode_6502_php_size;
extern const uint8_t opcode_6502_plp_size;
extern uint8_t opcode_6502_pha[];
extern uint8_t opcode_6502_pla[];
extern uint8_t opcode_6502_php[];
extern uint8_t opcode_6502_plp[];

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

// Native Stack JSR template (ENABLE_NATIVE_STACK: uses PHA to push return addr)
extern const uint8_t opcode_6502_ns_jsr_size;
extern uint8_t opcode_6502_ns_jsr[];
extern const uint8_t opcode_6502_ns_jsr_ret_hi;
extern const uint8_t opcode_6502_ns_jsr_ret_lo;
extern const uint8_t opcode_6502_ns_jsr_tgt_lo;
extern const uint8_t opcode_6502_ns_jsr_tgt_hi;

// Native Stack RTS template (ENABLE_NATIVE_STACK: uses PLA to pop return addr)
extern const uint8_t opcode_6502_ns_rts_size;
extern uint8_t opcode_6502_ns_rts[];

// STA (zp),Y native handler
extern void sta_indy_handler();
extern uint8_t sta_indy_template[];
extern const uint8_t sta_indy_template_size;
extern uint8_t sta_indy_zp_patch;

// Flash cache PC lookup tables (in flash memory, bankswitched to $8000-$BFFF).
// The assembly labels _flash_cache_pc / _flash_cache_pc_flags may NOT sit at
// $8000 on NES (SA code shares the first PC-table bank).  All accesses go
// through the bankswitched window, so the base is always FLASH_BANK_BASE.
#define flash_cache_pc       ((uint8_t *)FLASH_BANK_BASE)
#define flash_cache_pc_flags ((const uint8_t *)FLASH_BANK_BASE)


// Removed: ready(), check_cache_links(), verify_link_type0(), verify_link_type1(), combine_caches()
// These were all part of the old RAM cache execution system.
// Removed: decode_address_c() - now in platform/platform_exidy.c
uint8_t recompile_opcode();
void cache_bit_enable(uint16_t addr);
uint8_t cache_bit_check(uint16_t addr);
uint8_t flash_sector_alloc(uint8_t total_size);  // allocate space in a sector; sets flash_code_bank/address
void flash_cache_pc_update(uint8_t code_address, uint8_t flags);
void setup_flash_address(uint16_t emulated_pc, uint16_t block_number);  // legacy, kept for PC-table side
void setup_flash_pc_tables(uint16_t emulated_pc);  // set up PC table pointers only
void setup_and_update_pc(uint16_t emulated_pc, uint8_t code_address, uint8_t flags);  // combined setup+update (1 bankswitch)
void flash_cache_init_sectors(void);  // erase code cache sectors, zero free-pointer table
uint8_t flash_cache_search(uint16_t emulated_pc);

#ifdef ENABLE_IR
// SA pass 2 mid-block PC tracking (see dynamos.c for details)
#define SA_IR_MAX_INSTRS 96
extern uint8_t sa_ir_instr_pc_offset[SA_IR_MAX_INSTRS];
extern uint8_t sa_ir_instr_first_node[SA_IR_MAX_INSTRS];
extern uint8_t sa_ir_instr_native_off[SA_IR_MAX_INSTRS];
extern uint8_t sa_ir_instr_count;
extern uint8_t sa_ir_instrs_eliminated;
#endif

// Static compilation pass flag (0=dynamic/pass1, 2=pass2-emit-with-knowledge)
extern uint8_t sa_compile_pass;

// Set to 1 after SA two-pass compile completes.  Suppresses the cache-
// pressure auto-reset so the system doesn't endlessly cycle between
// SA compile → dynamic fill → cache pressure → soft reset → SA compile.
// Lives in WRAM so it resets to 0 on every soft reset (C startup re-inits).
// volatile: VBCC -O2 must not optimise away the cross-unit write/read.
extern volatile uint8_t sa_compile_completed;

// Pass 2: forced exit PC for the current block being compiled.
// When sa_compile_pass==2 and pc >= sa_block_exit_pc, the compile loop
// forces block termination to match pass 1's block boundaries.
extern uint16_t sa_block_exit_pc;

// Entry list write cursor (offset within BANK_ENTRY_LIST, in bytes).
// 16 bytes per entry: entry_pc(2), exit_pc(2), native_addr(2), bank(1),
// code_len(1), exit_a_val(1), exit_a_known(1), exit_x_val(1),
// exit_x_known(1), exit_y_val(1), exit_y_known(1), reserved(2).
#define ENTRY_LIST_STRIDE  16
extern uint16_t entry_list_offset;

// Pass 2: allocation size for the current block (set from entry list's code_len).
// sa_compile_one_block uses this instead of max-size in pass 2.
extern uint8_t sa_block_alloc_size;

// Native address lookup result globals (set by lookup_native_addr_safe / lookup_entry_list)
extern uint16_t reserve_result_addr;
extern uint8_t  reserve_result_bank;

// Look up the native address for a compiled PC.
// On success, sets reserve_result_addr and reserve_result_bank, returns 1.
// Returns 0 if target not compiled.  Safe to call from bank2.
uint8_t lookup_native_addr_safe(uint16_t target_pc);

// Look up a block entry point in the two-pass entry list (BANK_ENTRY_LIST).
// Only finds block ENTRY PCs (code_index=0), not mid-block instructions.
// On success, sets reserve_result_addr and reserve_result_bank, returns 1.
// Safe to call from bank2.  Used in pass 2 for forward branch resolution.
uint8_t lookup_entry_list(uint16_t target_pc);

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

// --- Native ZP pointer mirroring ---
#ifdef ENABLE_POINTER_SWIZZLE

typedef struct {
	uint8_t guest_lo;    // guest ZP address of pointer low byte
	uint8_t guest_hi;    // guest ZP address of pointer high byte
	uint8_t nes_zp;      // NES ZP slot for native mirror (lo; hi = lo+1)
	uint8_t side_effect; // 0=none, 1=screen_ram_updated, 2=character_ram_updated
} mirrored_ptr_t;

// mirrored_ptrs array lives in bank2 section (BSS — vbcc puts uninitialized
// data in BSS regardless of section, but the declaration must match the
// definition's section to avoid warning 371).
#pragma section bank2
extern mirrored_ptr_t mirrored_ptrs[ZP_MIRROR_COUNT];

// Mirror helpers live in bank2, called from recompile_opcode_b2.
// Declarations must carry the bank2 section attribute to match the
// definitions in dynamos.c and avoid vbcc warning 371.
#pragma section bank2

// Look up a guest ZP address in the mirror table (lo or hi byte match).
const mirrored_ptr_t *find_zp_mirror(uint8_t guest_zp);

// Look up by guest_lo only (for STA (zp),Y operand).
const mirrored_ptr_t *find_zp_mirror_lo(uint8_t guest_zp);

#pragma section default

#endif /* ENABLE_POINTER_SWIZZLE */



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