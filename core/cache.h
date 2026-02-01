/**
 * cache.h - Recompiler cache management interface
 * 
 * Platform-agnostic flash cache and dispatch infrastructure.
 * Handles block allocation, PC lookup tables, and dispatch mechanism.
 */

#ifndef CACHE_H
#define CACHE_H

#include <stdint.h>

// Cache configuration
#define BLOCK_COUNT 8
#define CODE_SIZE 250
#define CACHE_L1_CODE_SIZE 256

// Flash configuration
#define FLASH_CACHE_MEMORY_SIZE   0x3C000
#define FLASH_CACHE_BLOCK_SIZE    0x100
#define FLASH_ERASE_SECTOR_SIZE   0x1000
#define FLASH_BANK_BASE           0x8000
#define FLASH_BANK_SIZE           0x4000
#define FLASH_BANK_MASK           0x3FFF
#define FLASH_CACHE_BLOCKS        (FLASH_CACHE_MEMORY_SIZE / FLASH_CACHE_BLOCK_SIZE)
#define FLASH_CACHE_BANKS         (FLASH_CACHE_MEMORY_SIZE / FLASH_BANK_SIZE)

#define BLOCK_CONFIG_BASE 250

// Block status flags
#define FLASH_AVAILABLE     0x01

// PC flags
#define RECOMPILED          0x80    // bit clear = has been recompiled
#define INTERPRETED         0x40    // bit clear = interpret this instruction
#define CODE_DATA           0x20    // 0 = code, 1 = data

// Cache result flags
#define PC_CHANGE                   1
#define INTERPRET_NEXT_INSTRUCTION  2
#define OUT_OF_CACHE                4
#define LINKED                      8
#define BRANCH_LINKED               16
#define READY_FOR_NEXT              32

// Bank assignments
#define BANK_FLASH_BLOCK_FLAGS  3
#define BANK_CODE               4
#define BANK_PC                 19
#define BANK_PC_FLAGS           27

// Statistics (defined in cache.c)
extern uint32_t cache_hits;
extern uint32_t cache_misses;
extern uint32_t cache_branches;
extern uint32_t branch_not_compiled;
extern uint32_t branch_wrong_bank;
extern uint32_t branch_out_of_range;
extern uint32_t branch_forward;

// Cache buffers
extern uint8_t l1_cache_code[CACHE_L1_CODE_SIZE];
extern uint8_t cache_code[BLOCK_COUNT][CODE_SIZE];
extern uint8_t cache_flag[BLOCK_COUNT];
extern uint8_t cache_entry_pc_lo[BLOCK_COUNT];
extern uint8_t cache_entry_pc_hi[BLOCK_COUNT];
extern uint8_t cache_exit_pc_lo[BLOCK_COUNT];
extern uint8_t cache_exit_pc_hi[BLOCK_COUNT];
extern uint16_t cache_cycles[BLOCK_COUNT];
extern uint8_t cache_hit_count[BLOCK_COUNT];
extern uint8_t cache_branch_pc_lo[BLOCK_COUNT];
extern uint8_t cache_branch_pc_hi[BLOCK_COUNT];
extern uint8_t cache_vpc[BLOCK_COUNT];

// Current cache state
extern uint8_t cache_index;
extern uint8_t code_index;
extern uint16_t flash_cache_index;
extern uint8_t flash_enabled;

// Flash address state
extern uint16_t pc_jump_address;
extern uint8_t pc_jump_bank;
extern uint16_t pc_jump_flag_address;
extern uint8_t pc_jump_flag_bank;
extern uint16_t flash_code_address;
extern uint8_t flash_code_bank;

// Flash cache lookup tables (in flash memory)
extern uint8_t flash_cache_pc[];
extern const uint8_t flash_cache_pc_flags[];
extern uint8_t flash_block_flags[];

// Cache management functions
uint16_t cache_allocate_block(void);
void cache_setup_flash_address(uint16_t emulated_pc, uint16_t block_number);
void cache_pc_update(uint8_t code_address, uint8_t flags);
void cache_pc_flag_clear(uint16_t emulated_pc, uint8_t flag);
uint8_t cache_search(uint16_t emulated_pc);
void cache_copy_to_flash(uint8_t src_idx, uint16_t dest_idx);

// Bit array for tracking compiled addresses
void cache_bit_enable(uint16_t addr);
uint8_t cache_bit_check(uint16_t addr);

// ASM dispatch functions
__regsused("a/x/y") extern uint8_t dispatch_on_pc(void);
__regsused("a/x/y") extern void flash_dispatch_return(void);

#endif // CACHE_H
