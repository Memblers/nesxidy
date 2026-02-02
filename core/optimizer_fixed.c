/**
 * optimizer_fixed.c - Fixed bank entry points for optimizer
 * 
 * These functions MUST be in the fixed bank ($C000-$FFFF) since they
 * are called from the main loop which doesn't know what bank is active.
 * 
 * Keep this file small - it competes with other fixed bank code for space.
 */

#include <stdint.h>
#include "optimizer.h"
#include "../mapper30.h"
#include "opt_backup.h"

// Trampoline setup (in opt_trampoline.s, also fixed bank)
extern void opt_setup_trampolines(void);

// Bank1 function (declared but lives in optimizer.c)
extern void opt_run_full_rebuild(void);

// Critical CPU state that optimizer must not corrupt
extern uint16_t pc;
extern uint8_t cache_index;
extern uint16_t code_index;
extern uint8_t cache_flag[];

// Global state - in fixed bank so it's in normal BSS (gets zeroed)
opt_state_t opt_state;

// One-time init flag
static uint8_t opt_initialized = 0;

// Debug structure - located at $0100-$010F (CPU RAM page 1)
typedef struct {
    uint8_t trigger_fired;      // 0x00: 0xEE when opt_do_recompile called
    uint8_t tracking_init;      // 0x01: 0xFF when sector tracking initialized
    uint8_t current_sector;     // 0x02: sector being checked
    uint8_t sector_status;      // 0x03: in-use status for that sector
    uint8_t evacuation_marker;  // 0x04: 0xDD when evacuation starts
    uint8_t evac_sector;        // 0x05: which sector being evacuated
    uint8_t marked_bank;        // 0x06: last bank marked as in-use
    uint8_t erase_complete;     // 0x07: 0xAA when erase completes
    uint8_t block_count;        // 0x08: unique blocks compiled
    uint8_t marked_invalid;     // 0x09: 0x99 if invalid bank marked
    uint8_t trigger_marker;     // 0x0A: marker (0x55 trigger, 0x77 mark, 0x66 entry)
    uint8_t padding[5];         // 0x0B-0x0F: padding to next aligned location
} opt_debug_t;

static opt_debug_t opt_debug = {0};

// Sector tracking state - stored in WRAM at $6010-$602D for 30 sectors (Bank 0 WRAM, never cleared)
// Access via volatile pointer to $6010+sector
#define SECTOR_ARRAY_BASE 0x6010

//============================================================================================================
// Sector marking - called when code is compiled to a bank
//============================================================================================================

#define NUM_CODE_BANKS 15
#define SECTORS_PER_BANK 2
#define NUM_SECTORS (NUM_CODE_BANKS * SECTORS_PER_BANK)
#define BANK_CODE 4

void opt_mark_sector_in_use(uint8_t bank) {
    // Use raw volatile pointers to known RAM locations
    volatile uint8_t *debug = (volatile uint8_t*)0x0100;
    volatile uint8_t *sector_ram = (volatile uint8_t*)0x0110;
    
    // DEBUG: Mark function entry
    debug[10] = 0x66;
    
    if (bank < BANK_CODE || bank >= BANK_CODE + NUM_CODE_BANKS) {
        debug[9] = 0x99;  // Invalid bank marker
        return;
    }
    
    uint8_t bank_offset = bank - BANK_CODE;
    uint8_t sector = bank_offset * SECTORS_PER_BANK;
    
    // Mark both sectors as in-use
    if (sector < NUM_SECTORS) {
        sector_ram[sector] = 1;
        // DEBUG: Read back and show status
        uint8_t readback = sector_ram[sector];
        debug[3] = readback;
        debug[2] = sector;
    }
    if ((sector + 1) < NUM_SECTORS) {
        sector_ram[sector + 1] = 1;
    }
    
    // DEBUG: Show which bank was marked
    debug[6] = bank;
    debug[10] = 0x77;
}

//============================================================================================================
// Worklist access (data is in bank1 BSS, but RAM doesn't switch)
//============================================================================================================

#define MAX_WORKLIST 200

typedef struct {
    uint16_t src_pc;
    uint16_t new_native;
} worklist_entry_t;

extern worklist_entry_t worklist[MAX_WORKLIST];
extern uint16_t worklist_count;

// PC flags bank range
#define BANK_PC_FLAGS 27
#define BANK_CODE 4

// Flag bits
#define RECOMPILED 0x80

//============================================================================================================
// Scan for code bank - MUST be in fixed bank since it calls bankswitch_prg()
//============================================================================================================

uint16_t scan_for_code_bank(uint8_t target_code_bank) {
    worklist_count = 0;
    
    // Scan all 64K addresses (PC flags are 1 byte each, in banks 27-30)
    for (uint8_t flag_bank = BANK_PC_FLAGS; flag_bank <= BANK_PC_FLAGS + 3; flag_bank++) {
        bankswitch_prg(flag_bank);
        
        uint16_t base_pc = (uint16_t)(flag_bank - BANK_PC_FLAGS) << 14;
        
        for (uint16_t off = 0; off < 0x4000 && worklist_count < MAX_WORKLIST; off++) {
            uint8_t flag = *(volatile uint8_t*)(0x8000 + off);
            
            // Check if this entry points to code (not $FF, not INTERPRETED)
            if (flag != 0xFF && !(flag & RECOMPILED)) {
                uint8_t code_bank = flag & 0x1F;
                
                if (code_bank == target_code_bank) {
                    worklist[worklist_count].src_pc = base_pc + off;
                    worklist[worklist_count].new_native = 0;
                    worklist_count++;
                }
            }
        }
    }
    
    // Restore bank 1 for caller (optimizer code)
    bankswitch_prg(1);
    
    return worklist_count;
}

//============================================================================================================
// PC table and flag table updates - must be in fixed bank since they call bankswitch_prg
//============================================================================================================

// Flash programming functions (in mapper30.c)
extern void flash_byte_program(uint16_t addr, uint8_t bank, uint8_t data);
extern void flash_sector_erase(uint16_t addr, uint8_t bank);

// Update a single PC table entry
uint8_t update_pc_entry(uint16_t src_pc, uint16_t new_native) {
    // DEBUG: Mark that we entered update_pc_entry
    {
        volatile uint8_t *ppu_data = (volatile uint8_t*)0x2007;
        volatile uint8_t *ppu_addr = (volatile uint8_t*)0x2006;
        volatile uint8_t *ppu_status = (volatile uint8_t*)0x2002;
        
        (void)*ppu_status;  // Reset latch
        *ppu_addr = 0x1F;
        *ppu_addr = 0xE0;
        *ppu_data = 0xCC;  // Mark entry to update_pc_entry
    }
    
    uint8_t pc_bank = (src_pc >> 13) + 19;  // BANK_PC = 19
    uint16_t pc_off = (src_pc << 1) & 0x3FFF;
    
    bankswitch_prg(pc_bank);
    uint16_t old_native = *(volatile uint8_t*)(0x8000 + pc_off);
    old_native |= (uint16_t)(*(volatile uint8_t*)(0x8000 + pc_off + 1)) << 8;
    
    if (old_native != 0xFFFF) {
        bankswitch_prg(1);
        return 0;  // Already has value, skip
    }
    
    // Erase sector and update via backup
    backup_init();
    backup_copy_from_flash(0, 0x8000, pc_bank);
    flash_sector_erase(0x8000, pc_bank);
    backup_write(0, pc_off, new_native & 0xFF);
    backup_write(0, pc_off + 1, new_native >> 8);
    backup_copy_to_flash(0, 0x8000, pc_bank);
    backup_finish();
    bankswitch_prg(1);
    return 1;
}

// Update a single flag table entry
uint8_t update_flag_entry(uint16_t src_pc, uint8_t new_bank) {
    uint8_t flag_bank = (src_pc >> 14) + BANK_PC_FLAGS;
    uint16_t flag_off = src_pc & 0x3FFF;
    
    bankswitch_prg(flag_bank);
    uint8_t old_flag = *(volatile uint8_t*)(0x8000 + flag_off);
    
    if (old_flag == new_bank) {
        bankswitch_prg(1);
        return 1;  // Already correct
    }
    
    // Check if write is possible (can only clear bits)
    if (old_flag != 0xFF && (new_bank & old_flag) != new_bank) {
        bankswitch_prg(1);
        return 0;  // Can't set bits in flash
    }
    
    // Erase sector and update via backup
    backup_init();
    backup_copy_from_flash(0, 0x8000, flag_bank);
    flash_sector_erase(0x8000, flag_bank);
    backup_write(0, flag_off, new_bank);
    backup_copy_to_flash(0, 0x8000, flag_bank);
    backup_finish();
    bankswitch_prg(1);
    return 1;
}

//============================================================================================================
// Fixed Bank Entry Points
//============================================================================================================

void opt_init(uint16_t threshold, uint16_t min_blocks) {
    // Only initialize once at startup
    if (opt_initialized)
        return;
    opt_initialized = 1;
    
    // Explicitly zero everything first
    opt_state.opt_in_progress = 0;  // MUST be first!
    opt_state.dispatch_count = 0;
    opt_state.opt_threshold = threshold;
    opt_state.compiled_count = 0;
    opt_state.unique_blocks = 0;
    opt_state.min_blocks_required = min_blocks;
    opt_state.phase = 0;
    
    // Initialize sector tracking array at $0110-$012D
    // This clears out any existing data (only runs once due to guard)
    volatile uint8_t *sector_ptr = (volatile uint8_t*)0x0110;
    uint8_t i;
    for (i = 0; i < 30; i++) {
        sector_ptr[i] = 0;
    }
    
    opt_setup_trampolines();
}

void opt_notify_block_compiled(void) {
    // Called when a new unique block is compiled to flash
    if (opt_state.unique_blocks < 0xFFFF) {
        opt_state.unique_blocks++;
    }
    // TODO: Mark the appropriate sector as in-use
}

void opt_check_trigger(void) {
    volatile uint8_t *debug = (volatile uint8_t*)0x0100;
    
    // Don't run if already in progress
    if (opt_state.opt_in_progress)
        return;
    
    // Gate: don't run until minimum blocks compiled
    if (opt_state.unique_blocks < opt_state.min_blocks_required) {
        debug[8] = opt_state.unique_blocks;
        return;
    }
    
    debug[8] = opt_state.unique_blocks;
    debug[10] = 0x55;  // Trigger fired
    
    // Save critical state - optimizer corrupts these
    uint16_t saved_pc = pc;
    uint8_t saved_cache_index = cache_index;
    uint16_t saved_code_index = code_index;
    uint8_t saved_cache_flag = cache_flag[0];
    
    bankswitch_prg(1);
    opt_run_full_rebuild();
    
    // Restore bank 0 for interpreter
    bankswitch_prg(0);
    
    // Restore critical state
    pc = saved_pc;
    cache_index = saved_cache_index;
    code_index = saved_code_index;
    cache_flag[0] = saved_cache_flag;
}
