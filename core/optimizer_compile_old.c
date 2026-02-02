/**
 * optimizer_compile.c - Phase 3: Two-pass recompilation with branch resolution
 * 
 * Pass 1: Compile all addresses contiguously to flash, record branches for fixup
 * Pass 2: Patch branch offsets now that all native addresses are known
 * 
 * Code is written sequentially (no fixed blocks) so branches are more likely in range.
 */

#pragma section bank1

#include <stdint.h>
#include "optimizer.h"
#include "../config.h"
#include "../dynamos.h"
#include "../mapper30.h"

// External state from dynamos.c
extern uint16_t flash_code_address;
extern uint8_t flash_code_bank;
extern uint16_t code_index;
extern uint8_t flash_block_flags[];
extern uint8_t cache_code[BLOCK_COUNT][CODE_SIZE];
extern uint8_t cache_flag[BLOCK_COUNT];
extern uint8_t cache_index;
extern uint16_t pc;
extern uint8_t a;  // 6502 A register
extern uint8_t mapper_prg_bank;

// Functions from dynamos.c
extern void bankswitch_prg(__reg("a") uint8_t bank);
extern uint8_t recompile_opcode(void);
extern void flash_byte_program(uint16_t addr, uint8_t bank, uint8_t data);
extern uint8_t read6502(uint16_t address);

// Debug port
#define DEBUG_PORT (*(volatile uint8_t*)0x4020)

// Contiguous flash write state
static uint16_t flash_write_addr;  // Current write position $8000-$BFFF
static uint8_t  flash_write_bank;  // Current bank (BANK_CODE to BANK_CODE+14)

// Branch fixup tracking - stored in upper CHR-RAM
// Each entry: 6 bytes [native_addr_lo, native_addr_hi, native_bank, target_6502_lo, target_6502_hi, opcode]
#define CHR_RAM_BRANCH_LIST  0x1800
#define MAX_BRANCH_ENTRIES   256

static uint16_t branch_count;

// Branch opcodes
#define IS_BRANCH_OP(op) ((op) == 0x10 || (op) == 0x30 || (op) == 0x50 || (op) == 0x70 || \
                          (op) == 0x90 || (op) == 0xB0 || (op) == 0xD0 || (op) == 0xF0)

//============================================================================================================
// CHR-RAM Access
//============================================================================================================

static uint16_t chr_ram_read16(uint16_t offset) {
    volatile uint8_t dummy = PPU_STATUS;
    PPU_ADDR = (uint8_t)(offset >> 8);
    PPU_ADDR = (uint8_t)(offset);
    dummy = PPU_DATA;  // Dummy read
    uint8_t lo = PPU_DATA;
    uint8_t hi = PPU_DATA;
    return lo | (hi << 8);
}

static void chr_ram_write16(uint16_t offset, uint16_t value) {
    volatile uint8_t dummy = PPU_STATUS;
    PPU_ADDR = (uint8_t)(offset >> 8);
    PPU_ADDR = (uint8_t)(offset);
    PPU_DATA = (uint8_t)(value);
    PPU_DATA = (uint8_t)(value >> 8);
}

//============================================================================================================
// Branch Fixup Recording
//============================================================================================================

// Record a branch that needs fixup in Pass 2
// native_offset_addr = flash address where the offset byte is
// target_6502 = the 6502 target PC of the branch
static void record_branch_fixup(uint16_t native_offset_addr, uint8_t native_bank, uint16_t target_6502) {
    if (branch_count >= MAX_BRANCH_ENTRIES) return;
    
    // Store: [native_addr_lo, native_addr_hi, native_bank, target_6502_lo, target_6502_hi]
    // Using 5 bytes per entry at CHR_RAM_BRANCH_LIST
    uint16_t offset = CHR_RAM_BRANCH_LIST + (branch_count * 6);
    
    volatile uint8_t dummy = PPU_STATUS;
    PPU_ADDR = (uint8_t)(offset >> 8);
    PPU_ADDR = (uint8_t)(offset);
    PPU_DATA = (uint8_t)(native_offset_addr);
    PPU_DATA = (uint8_t)(native_offset_addr >> 8);
    PPU_DATA = native_bank;
    PPU_DATA = (uint8_t)(target_6502);
    PPU_DATA = (uint8_t)(target_6502 >> 8);
    PPU_DATA = 0;  // Padding to 6 bytes
    
    branch_count++;
}

// Read a branch fixup entry
static void read_branch_fixup(uint16_t index, uint16_t *native_addr, uint8_t *native_bank, uint16_t *target_6502) {
    uint16_t offset = CHR_RAM_BRANCH_LIST + (index * 6);
    
    volatile uint8_t dummy = PPU_STATUS;
    PPU_ADDR = (uint8_t)(offset >> 8);
    PPU_ADDR = (uint8_t)(offset);
    dummy = PPU_DATA;  // Dummy read
    
    uint8_t lo = PPU_DATA;
    uint8_t hi = PPU_DATA;
    *native_addr = lo | (hi << 8);
    
    *native_bank = PPU_DATA;
    
    lo = PPU_DATA;
    hi = PPU_DATA;
    *target_6502 = lo | (hi << 8);
}

//============================================================================================================
// Address List Binary Search (for branch resolution)
//============================================================================================================

// Binary search for address in sorted CHR-RAM list
// Returns index if found, 0xFFFF if not found
static uint16_t find_address_index(uint16_t target, uint16_t count) {
    // Linear search for now - addresses may not be sorted
    for (uint16_t i = 0; i < count; i++) {
        uint16_t addr = chr_ram_read16(CHR_RAM_ADDR_LIST + (i * 2));
        if (addr == target) {
            return i;
        }
    }
    return 0xFFFF;
}

//============================================================================================================
// Pass 1: Compile all addresses with branch tracking
//============================================================================================================

// Custom branch handler for optimizer - writes opcode + placeholder, records for fixup
static uint8_t opt_handle_branch(uint16_t block_idx) {
    uint8_t opcode = read6502(pc);
    int8_t offset = (int8_t)read6502(pc + 1);
    uint16_t target_pc = pc + 2 + offset;
    
    // Write branch opcode to cache
    cache_code[0][code_index] = opcode;
    cache_code[0][code_index + 1] = 0x00;  // Placeholder - will leave as $FF in flash
    
    // Calculate where the offset byte will be in flash
    uint16_t offset_flash_addr = flash_code_address + code_index + 1;
    
    // Record for Pass 2 fixup
    record_branch_fixup(offset_flash_addr, flash_code_bank, target_pc);
    
    pc += 2;
    code_index += 2;
    cache_flag[0] |= READY_FOR_NEXT;
    return cache_flag[0];
}

static void compile_one_address(uint16_t src_pc, uint16_t block_idx) {
    // Set up for this block
    setup_flash_address(src_pc, block_idx);
    
    // Set PC for recompiler
    pc = src_pc;
    cache_index = 0;
    code_index = 0;
    cache_flag[0] = 0;
    
    // Compile instructions until block ends
    do {
        uint16_t pc_old = pc;
        uint8_t code_index_old = code_index;
        
        recompile_opcode();
        
        // Write instruction bytes to flash
        uint8_t instr_len = code_index - code_index_old;
        for (uint8_t i = 0; i < instr_len; i++) {
            flash_byte_program(flash_code_address + code_index_old + i, 
                             flash_code_bank, 
                             cache_code[0][code_index_old + i]);
        }
        
        // Update PC lookup table for this instruction
        if (instr_len) {
            setup_flash_address(pc_old, block_idx);  // Restore for PC update
            flash_cache_pc_update(code_index_old, RECOMPILED);
        }
        
        // Check for buffer overflow
        if (code_index > (CODE_SIZE - 6)) {
            cache_flag[0] |= OUT_OF_CACHE;
            cache_flag[0] &= ~READY_FOR_NEXT;
        }
        
    } while (cache_flag[0] & READY_FOR_NEXT);
    
    // Write epilogue
    if (code_index) {
        uint16_t exit_pc = pc;
        uint8_t epilogue_start = code_index;
        
        // STA a
        flash_byte_program(flash_code_address + epilogue_start + 0, flash_code_bank, 0x85);
        flash_byte_program(flash_code_address + epilogue_start + 1, flash_code_bank, (uint8_t)((uint16_t)&a));
        // PHP
        flash_byte_program(flash_code_address + epilogue_start + 2, flash_code_bank, 0x08);
        // LDA #lo(exit_pc)
        flash_byte_program(flash_code_address + epilogue_start + 3, flash_code_bank, 0xA9);
        flash_byte_program(flash_code_address + epilogue_start + 4, flash_code_bank, (uint8_t)exit_pc);
        // STA pc
        flash_byte_program(flash_code_address + epilogue_start + 5, flash_code_bank, 0x85);
        flash_byte_program(flash_code_address + epilogue_start + 6, flash_code_bank, (uint8_t)((uint16_t)&pc));
        // LDA #hi(exit_pc)
        flash_byte_program(flash_code_address + epilogue_start + 7, flash_code_bank, 0xA9);
        flash_byte_program(flash_code_address + epilogue_start + 8, flash_code_bank, (uint8_t)(exit_pc >> 8));
        // STA pc+1
        flash_byte_program(flash_code_address + epilogue_start + 9, flash_code_bank, 0x85);
        flash_byte_program(flash_code_address + epilogue_start + 10, flash_code_bank, (uint8_t)(((uint16_t)&pc) + 1));
        // JMP flash_dispatch_return
        extern void flash_dispatch_return(void);
        flash_byte_program(flash_code_address + epilogue_start + 11, flash_code_bank, 0x4C);
        flash_byte_program(flash_code_address + epilogue_start + 12, flash_code_bank, (uint8_t)((uint16_t)&flash_dispatch_return));
        flash_byte_program(flash_code_address + epilogue_start + 13, flash_code_bank, (uint8_t)(((uint16_t)&flash_dispatch_return) >> 8));
    }
    
    // Mark block as used
    bankswitch_prg(BANK_FLASH_BLOCK_FLAGS);
    flash_byte_program((uint16_t)&flash_block_flags[0] + block_idx, mapper_prg_bank, 
                       flash_block_flags[block_idx] & ~FLASH_AVAILABLE);
}

//============================================================================================================
// Main Two-Pass Recompilation
//============================================================================================================

void opt_do_recompile(uint16_t addr_count) {
    DEBUG_PORT = 0x30;  // Pass 1 start
    
    // Reset block allocation
    current_block_index = 0;
    
    //--- Pass 1: Compile all addresses, record native locations ---
    for (uint16_t i = 0; i < addr_count; i++) {
        // Read source PC from CHR-RAM address list
        uint16_t src_pc = chr_ram_read16(CHR_RAM_ADDR_LIST + (i * 2));
        
        // Allocate next block
        uint16_t block_idx = current_block_index++;
        
        // Record native address (block start) in CHR-RAM native list
        setup_flash_address(src_pc, block_idx);
        chr_ram_write16(CHR_RAM_NATIVE_LIST + (i * 2), flash_code_address);
        
        // Compile this address
        compile_one_address(src_pc, block_idx);
        
        // Progress indicator
        if ((i & 0x0F) == 0) {
            DEBUG_PORT = 0x31 + (i >> 4);
        }
    }
    
    DEBUG_PORT = 0x40;  // Pass 1 complete
    
    // Pass 2: Branch resolution would patch branches here
    // For now, branches that target compiled addresses will still work
    // because dispatch looks up the PC table
    
    DEBUG_PORT = 0x50;  // Pass 2 complete (no-op for now)
}
