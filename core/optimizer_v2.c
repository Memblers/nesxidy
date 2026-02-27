/**
 * optimizer_v2.c - Sector evacuation with branch resolution
 * 
 * Algorithm:
 * 1. Pick a code sector to evacuate (e.g., sector 0 = bank 4, lower 4KB)
 * 2. Scan PC flags table to find all PCs whose code is in that sector
 * 3. Build worklist in RAM (limited size, ~200 entries)
 * 4. Allocate a fresh code sector for output
 * 5. Recompile each PC to the new sector:
 *    - Check branch targets, resolve if possible
 *    - Write to new location
 *    - Queue PC table update
 * 6. Update PC table entries (requires backup/erase/rewrite per PC sector)
 * 7. Mark old code sector as free, erase it
 * 
 * Memory usage:
 * - Worklist: 4 bytes per entry (2=src_pc, 2=new_native) × 200 = 800 bytes
 * - CHR-RAM: 4KB for PC table sector backup during updates
 */

#include <stdint.h>
#include "optimizer.h"
#include "opt_backup.h"
#include "../config.h"
#include "../dynamos.h"
#include "../mapper30.h"

// External references
extern void bankswitch_prg(__reg("a") uint8_t bank);
extern uint8_t recompile_opcode(void);
extern void flash_byte_program(uint16_t addr, uint8_t bank, uint8_t data);
extern uint8_t read6502(uint16_t address);
extern void flash_dispatch_return(void);
extern void opt_tramp_erase(uint8_t sector);
extern void backup_copy_from_flash(uint8_t slot, uint16_t flash_addr, uint8_t flash_bank);
extern void backup_copy_to_flash(uint8_t slot, uint16_t flash_addr, uint8_t flash_bank);

#pragma section bank1

// External data
extern uint8_t cache_code[BLOCK_COUNT][CACHE_CODE_BUF_SIZE];
extern uint8_t cache_flag[BLOCK_COUNT];
extern uint8_t cache_index;
extern uint16_t code_index;
extern uint16_t pc;

// Sector tracking state stored at $0110-$011D (30 bytes for 30 sectors)
#define SECTOR_ARRAY_BASE 0x0110

// Note: sector array data is accessed via direct RAM, not a pointer
// No need for extern - just use the fixed address

//============================================================================
// Worklist - entries to recompile (shared with optimizer_fixed.c)
//============================================================================

#define MAX_WORKLIST 200

typedef struct {
    uint16_t src_pc;      // Emulated PC
    uint16_t new_native;  // New native address after recompile
} worklist_entry_t;

// These are in bank1 BSS but accessible from fixed bank (RAM doesn't switch)
worklist_entry_t worklist[MAX_WORKLIST];
uint16_t worklist_count;

//============================================================================
// Sector tracking - PLACED IN CPU RAM AT $0110 FOR ACCESSIBILITY
// Code banks 4-18 = 15 banks = 30 sectors (2 sectors per bank, 4KB each)
// We track which sectors are "in use" vs "free for output"
//============================================================================

// Sector tracking state stored at $0110-$011D (30 bytes for 30 sectors)
#define SECTOR_ARRAY_BASE 0x0110

// Debug structure
typedef struct {
    uint8_t trigger_fired;      // 0x00
    uint8_t tracking_init;      // 0x01
    uint8_t current_sector;     // 0x02
    uint8_t sector_status;      // 0x03
    uint8_t evacuation_marker;  // 0x04
    uint8_t evac_sector;        // 0x05
    uint8_t marked_bank;        // 0x06
    uint8_t erase_complete;     // 0x07
    uint8_t block_count;        // 0x08
    uint8_t marked_invalid;     // 0x09
    uint8_t trigger_marker;     // 0x0A
    uint8_t padding[5];         // 0x0B-0x0F
} opt_debug_t;

// Access debug structure from fixed bank BSS via volatile pointer at $0100
static volatile opt_debug_t *opt_debug_ptr = (volatile opt_debug_t *)0x0100;
//============================================================================

#define NUM_CODE_BANKS 15       // Banks 4-18
#define SECTORS_PER_BANK 2
#define NUM_SECTORS (NUM_CODE_BANKS * SECTORS_PER_BANK)  // 30 sectors

// Sector state array at fixed RAM location $0700 (unused by game)
// Note: sector_in_use array is now allocated in fixed bank (sector_in_use_array)
// No need for pointer here - just use the extern directly

// Next sector to evacuate (round-robin)
static uint8_t next_evac_sector;

// Next free sector to use for output
static uint8_t next_output_sector;

// Initialize sector tracking - mark all as FREE initially
// (they will be marked IN_USE as code is compiled to them)
static void init_sector_tracking(void) {
    // Initialize all sectors as FREE (0)
    // They will be marked IN_USE (1) as code is compiled to them
    volatile uint8_t *sector_array = (volatile uint8_t*)SECTOR_ARRAY_BASE;
    uint8_t i;
    for (i = 0; i < NUM_SECTORS; i++) {
        sector_array[i] = 0;
    }
    
    // Sectors 26-29 are reserved for optimizer output, start as free
    // (they'll be marked in-use when code is output to them)
    next_evac_sector = 0;
    next_output_sector = 26;
}

// Find next free sector for output, returns 0xFF if none
static uint8_t find_free_sector(void) {
    volatile uint8_t *sector_array = (volatile uint8_t*)SECTOR_ARRAY_BASE;
    for (uint8_t i = 0; i < NUM_SECTORS; i++) {
        uint8_t idx = (next_output_sector + i) % NUM_SECTORS;
        if (!sector_array[idx]) {
            next_output_sector = (idx + 1) % NUM_SECTORS;
            return idx;
        }
    }
    return 0xFF;  // No free sector
}

// Convert sector number to bank and address
static void sector_to_bank_addr(uint8_t sector, uint8_t *bank, uint16_t *addr) {
    *bank = BANK_CODE + (sector / SECTORS_PER_BANK);
    *addr = (sector & 1) ? 0xA000 : 0x8000;  // Lower or upper 4KB
}

// Convert bank to sector number
static uint8_t bank_to_sector(uint8_t bank) {
    if (bank < BANK_CODE || bank >= BANK_CODE + NUM_CODE_BANKS) {
        return 0xFF;  // Invalid bank
    }
    uint8_t bank_offset = bank - BANK_CODE;
    return bank_offset * SECTORS_PER_BANK;  // Upper sector of the bank pair
}

//============================================================================
// Flash write state
//============================================================================

static uint16_t fw_addr;   // Current write address ($8000-$BFFF)
static uint8_t fw_bank;    // Current write bank

static void fw_init(uint8_t bank) {
    fw_addr = 0x8000;
    fw_bank = bank;
}

static void fw_byte(uint8_t data) {
    flash_byte_program(fw_addr, fw_bank, data);
    if (++fw_addr >= 0xC000) {
        fw_addr = 0x8000;
        fw_bank++;
    }
}

static uint16_t fw_pos(void) {
    return fw_addr;
}

static uint8_t fw_bank_get(void) {
    return fw_bank;
}

//============================================================================
// Scan PC flags to find entries pointing to target code bank
// NOTE: This function is now in optimizer_fixed.c (fixed bank) because it
// must call bankswitch_prg() without switching away from its own code.
//============================================================================

extern uint16_t scan_for_code_bank(uint8_t target_code_bank);

//============================================================================
// Lookup native address for a PC (for branch resolution)
//============================================================================

static uint16_t lookup_native(uint16_t target_pc, uint8_t *out_bank) {
    // Check PC flags first
    uint8_t flag_bank = (target_pc >> 14) + BANK_PC_FLAGS;
    uint16_t flag_off = target_pc & 0x3FFF;
    
    bankswitch_prg(flag_bank);
    uint8_t flag = *(volatile uint8_t*)(0x8000 + flag_off);
    
    if (flag == 0xFF || (flag & RECOMPILED)) {
        return 0xFFFF; // Not compiled
    }
    
    *out_bank = flag & 0x1F;
    
    // Read PC table
    uint8_t pc_bank = (target_pc >> 13) + BANK_PC;
    uint16_t pc_off = (target_pc << 1) & 0x3FFF;
    
    bankswitch_prg(pc_bank);
    uint16_t native = *(volatile uint8_t*)(0x8000 + pc_off);
    native |= (uint16_t)(*(volatile uint8_t*)(0x8000 + pc_off + 1)) << 8;
    
    return native;
}

//============================================================================
// Recompile one PC to current flash position, with branch resolution
//============================================================================

static uint16_t recompile_with_branches(uint16_t src_pc) {
    uint16_t start = fw_pos();
    uint8_t start_bank = fw_bank_get();
    
    // Use local variables to avoid corrupting global state
    uint16_t local_pc = src_pc;
    uint8_t local_code_index = 0;
    uint8_t local_flag = READY_FOR_NEXT;
    
    // Set up global state for recompile_opcode()
    pc = local_pc;
    cache_index = 0;
    code_index = 0;
    cache_flag[0] = READY_FOR_NEXT;
    
    do {
        uint8_t idx_old = code_index;
        uint8_t op = read6502(pc);
        
        // Check for conditional branches
        if (op==0x10||op==0x30||op==0x50||op==0x70||op==0x90||op==0xB0||op==0xD0||op==0xF0) {
            int8_t offset = (int8_t)read6502(pc + 1);
            uint16_t target_pc = pc + 2 + offset;
            
            // Try to resolve branch
            uint8_t target_bank;
            uint16_t target_native = lookup_native(target_pc, &target_bank);
            
            if (target_native != 0xFFFF) {
                if (target_bank == start_bank) {
                    // Target is compiled and in same bank
                    int16_t branch_offset = (int16_t)target_native - (int16_t)(fw_pos() + 2);
                    
                    if (branch_offset >= -128 && branch_offset <= 127) {
                        // Can use direct branch!
                        fw_byte(op);
                        fw_byte((uint8_t)branch_offset);
                        pc += 2;
                        cache_flag[0] &= ~READY_FOR_NEXT;
                        continue;
                    }
                    
                    // Same bank but out of range - use opposite branch over JMP
                    // Bxx +3 / JMP target (5 bytes total)
                    uint8_t opposite_op = op ^ 0x20;  // Toggle bit 5 to get opposite condition
                    fw_byte(opposite_op);
                    fw_byte(0x03);  // Skip over JMP
                    fw_byte(0x4C); // JMP
                    fw_byte(target_native & 0xFF);
                    fw_byte(target_native >> 8);
                    pc += 2;
                    cache_flag[0] &= ~READY_FOR_NEXT;
                    continue;
                }
                // TODO: Different bank - would need trampoline
            }
            
            // Can't resolve - end block here, will dispatch
            cache_flag[0] &= ~READY_FOR_NEXT;
            
        } else {
            // Regular opcode - use existing recompiler
            recompile_opcode();
            
            // Copy generated code to flash
            for (uint8_t i = idx_old; i < code_index; i++) {
                fw_byte(cache_code[0][i]);
            }
        }
        
        if (code_index > CODE_SIZE - 20) {
            cache_flag[0] &= ~READY_FOR_NEXT;
        }
        
    } while (cache_flag[0] & READY_FOR_NEXT);
    
    // Emit epilogue
    uint16_t exit_pc = pc;
    extern uint8_t a;
    
    fw_byte(0x85); fw_byte((uint8_t)(uint16_t)&a);       // STA a
    fw_byte(0x08);                                        // PHP
    fw_byte(0xA9); fw_byte(exit_pc & 0xFF);              // LDA #<exit_pc
    fw_byte(0x85); fw_byte((uint8_t)(uint16_t)&pc);      // STA pc
    fw_byte(0xA9); fw_byte(exit_pc >> 8);                // LDA #>exit_pc
    fw_byte(0x85); fw_byte(((uint16_t)&pc) + 1);         // STA pc+1
    fw_byte(0x4C);                                        // JMP
    fw_byte((uint16_t)&flash_dispatch_return & 0xFF);
    fw_byte(((uint16_t)&flash_dispatch_return) >> 8);
    
    return start;
}

//============================================================================
// Simple PC table and flag table updates 
// These call bankswitch_prg, so they're in optimizer_fixed.c (fixed bank)
//============================================================================

extern uint8_t update_pc_entry(uint16_t src_pc, uint16_t new_native);
extern uint8_t update_flag_entry(uint16_t src_pc, uint8_t new_bank);

// Output bank for recompiled code (tracked during evacuation)
static uint8_t output_code_bank;

// Update all entries in worklist
static void update_all_entries(void) {
    uint16_t pc_updated = 0;
    uint16_t flag_updated = 0;
    
    for (uint16_t i = 0; i < worklist_count; i++) {
        if (update_pc_entry(worklist[i].src_pc, worklist[i].new_native)) {
            pc_updated++;
        }
        
        if (update_flag_entry(worklist[i].src_pc, output_code_bank)) {
            flag_updated++;
        }
    }
}

// Trampoline to call recompile_opcode in bank0 from bank1
extern uint8_t opt_tramp_recompile(void);

// Cache buffers from dynamos.c
extern uint8_t cache_code[BLOCK_COUNT][CACHE_CODE_BUF_SIZE];

// Register variables
extern uint8_t a;

// Lookup native address (in fixed bank, assembly version)
extern uint16_t opt_tramp_lookup_native(uint16_t target_pc, uint8_t *out_bank);

//============================================================================
// Branch resolution helpers
//============================================================================

// Check if a target PC is in our current worklist (being recompiled together)
// If found, return the new native address, otherwise 0xFFFF
static uint16_t find_in_worklist(uint16_t target_pc) {
    for (uint16_t i = 0; i < worklist_count; i++) {
        if (worklist[i].src_pc == target_pc && worklist[i].new_native != 0) {
            return worklist[i].new_native;
        }
    }
    return 0xFFFF;
}

//============================================================================
// Recompile one block to output flash with branch resolution
// Returns number of bytes written, or 0 on failure
//============================================================================

// Branch opcodes
#define IS_BRANCH(op) ((op)==0x10||(op)==0x30||(op)==0x50||(op)==0x70||(op)==0x90||(op)==0xB0||(op)==0xD0||(op)==0xF0)

static uint16_t recompile_block_to_flash(uint16_t src_pc) {
    uint16_t start_addr = fw_pos();
    
    // Set up global state for recompile_opcode()
    pc = src_pc;
    cache_index = 0;
    code_index = 0;
    cache_flag[0] = READY_FOR_NEXT;
    
    // Recompile the block using standard recompiler (via trampoline to bank0)
    do {
        opt_tramp_recompile();
    } while (cache_flag[0] & READY_FOR_NEXT);
    
    if (code_index == 0) {
        return 0;  // Nothing compiled
    }
    
    // Note the exit PC (where this block ends)
    uint16_t exit_pc = pc;
    
    // Write the compiled code to our output flash
    for (uint8_t i = 0; i < code_index; i++) {
        fw_byte(cache_code[0][i]);
    }
    
    // Check if exit_pc is in our worklist (block continues to another we're compiling)
    uint16_t target_native = find_in_worklist(exit_pc);
    
    // If not in worklist, try looking up in PC table
    uint8_t target_bank = 0;
    if (target_native == 0xFFFF || target_native == 0) {
        target_native = opt_tramp_lookup_native(exit_pc, &target_bank);
        // Only use if in same bank as our output
        if (target_native != 0xFFFF && target_bank != output_code_bank) {
            target_native = 0xFFFF;  // Different bank, can't direct jump
        }
    }

    if (target_native != 0xFFFF && target_native != 0) {
        // Exit target is already compiled and in same bank!
        // We can chain directly to it instead of going through dispatch
        fw_byte(0x85); fw_byte((uint8_t)(uint16_t)&a);   // STA a (save A)
        fw_byte(0x08);                                    // PHP (save flags)
        fw_byte(0x4C);                                    // JMP target
        fw_byte(target_native & 0xFF);
        fw_byte(target_native >> 8);
    } else {
        // Standard epilogue - go through dispatch
        fw_byte(0x85); fw_byte((uint8_t)(uint16_t)&a);       // STA a
        fw_byte(0x08);                                        // PHP
        fw_byte(0xA9); fw_byte(exit_pc & 0xFF);              // LDA #<exit_pc
        fw_byte(0x85); fw_byte((uint8_t)(uint16_t)&pc);      // STA pc
        fw_byte(0xA9); fw_byte(exit_pc >> 8);                // LDA #>exit_pc
        fw_byte(0x85); fw_byte(((uint16_t)&pc) + 1);         // STA pc+1
        fw_byte(0x4C);                                        // JMP
        fw_byte((uint16_t)&flash_dispatch_return & 0xFF);
        fw_byte(((uint16_t)&flash_dispatch_return) >> 8);
    }
    
    return fw_pos() - start_addr;
}

//============================================================================
// Main optimization pass - evacuate one sector
// Returns 1 if successful, 0 if nothing to do or failed
//============================================================================

static uint8_t output_sector;  // Track which sector we're writing to

uint8_t opt_evacuate_sector(uint8_t evac_sector) {
    // DEBUG: Mark that evacuation started
    opt_debug_ptr->evacuation_marker = 0xDD;  // Evacuation started marker
    opt_debug_ptr->evac_sector = evac_sector;  // Which sector
    
    uint8_t code_bank = BANK_CODE + (evac_sector / SECTORS_PER_BANK);
    
    // Step 1: Scan for entries pointing to this code bank
    // scan_for_code_bank() is in fixed bank, so it can safely switch banks
    uint16_t count = scan_for_code_bank(code_bank);
    
    // DEBUG: Worklist count would go here if needed
    
    if (count == 0) {
        return 0;
    }
    
    // Step 2: Find a free sector for output
    output_sector = find_free_sector();
    if (output_sector == 0xFF) {
        return 0;
    }
    
    // Convert sector to bank/address
    uint8_t out_bank;
    uint16_t out_addr;
    sector_to_bank_addr(output_sector, &out_bank, &out_addr);
    
    output_code_bank = out_bank;
    fw_addr = out_addr;
    fw_bank = out_bank;
    
    // Step 3: Two-pass recompilation for branch chaining
    // Pass 1: Estimate code sizes and assign new_native addresses
    // We need to know where blocks will be placed before compiling them,
    // so that branch targets within the same evacuation batch can be resolved
    
    uint16_t sector_limit = (output_sector & 1) ? 0xC000 : 0xA000;  // 4KB boundary
    uint16_t estimated_addr = fw_pos();
    
    for (uint16_t i = 0; i < worklist_count; i++) {
        worklist[i].new_native = estimated_addr;
        
        // Estimate: assume each block is ~40 bytes (rough average)
        // This gives find_in_worklist() reasonable values during Pass 2
        estimated_addr += 40;
        
        if (estimated_addr > sector_limit - 0x100) {
            break;  // Won't fit in this sector
        }
    }
    
    uint16_t blocks_to_compile = 0;
    for (uint16_t i = 0; i < worklist_count; i++) {
        if (worklist[i].new_native > 0 && worklist[i].new_native < sector_limit) {
            blocks_to_compile++;
        } else {
            break;
        }
    }
    
    // Pass 2: Actual recompilation with branch addresses now available
    uint16_t recompiled = 0;
    uint16_t recompile_limit = blocks_to_compile;  // Only recompile blocks we estimated
    
    for (uint16_t i = 0; i < recompile_limit; i++) {
        uint16_t bytes = recompile_block_to_flash(worklist[i].src_pc);
        if (bytes > 0) {
            recompiled++;
        }
        
        // Stop if running low on space in this sector
        if (fw_pos() > sector_limit - 0x100) {
            break;
        }
    }
    
    // Step 4: Update PC table and flag entries
    update_all_entries();
    
    // Step 5: Mark sectors as used/free
    volatile uint8_t *sector_array = (volatile uint8_t*)SECTOR_ARRAY_BASE;
    sector_array[output_sector] = 1;  // Output sector now in use
    sector_array[evac_sector] = 0;    // Evacuated sector now free
    
    // Step 6: Erase the old sector (via fixed bank trampoline)
    opt_tramp_erase(evac_sector);
    
    opt_debug_ptr->erase_complete = 0xAA;  // Erase completed marker
    
    return 1;
}

//============================================================================
// Entry point called from optimizer.c
//============================================================================

static uint8_t tracking_initialized = 0;

void opt_do_recompile(void) {
    // DEBUG: Use opt_debug structure at 0x7A30 (safe from game)
    opt_debug_ptr->trigger_fired = 0xEE;  // Mark that opt_do_recompile was called
    
    // One-time initialization of sector tracking
    if (!tracking_initialized) {
        init_sector_tracking();
        tracking_initialized = 1;
        opt_debug_ptr->tracking_init = 0xFF;  // Mark sector tracking initialized
    }
    
    // Simple single-step: check ONE sector per call (no loop!)
    // This avoids the overhead of iterating through all sectors
    uint8_t sector = next_evac_sector;
    next_evac_sector++;
    if (next_evac_sector >= NUM_SECTORS) {
        next_evac_sector = 0;
    }
    
    // DEBUG: Show sector number and in_use status
    opt_debug_ptr->current_sector = sector;  // Current sector being checked
    
    // Read from sector array at $0110
    volatile uint8_t *sector_array = (volatile uint8_t*)SECTOR_ARRAY_BASE;
    uint8_t sec_status = sector_array[sector];
    opt_debug_ptr->sector_status = sec_status;
    
    if (sec_status) {
        // Only disable rendering when actually evacuating
        volatile uint8_t *ppu_mask = (volatile uint8_t*)0x2001;
        volatile uint8_t *ppu_status = (volatile uint8_t*)0x2002;
        
        *ppu_mask = 0x00;  // Turn off rendering
        
        opt_evacuate_sector(sector);
        
        // Wait for vblank before re-enabling rendering to avoid glitches
        while (!(*ppu_status & 0x80)) {}  // Wait for vblank
        *ppu_mask = 0x1E;  // Re-enable sprites and background
    }
}
