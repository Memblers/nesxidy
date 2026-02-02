/**
 * optimizer.h - Flash cache optimization system
 * 
 * Performs full rebuild of flash cache to:
 * - Resolve forward branches to native code
 * - Consolidate code for longer execution runs
 * - Eliminate interpretation where possible
 */

#ifndef OPTIMIZER_H
#define OPTIMIZER_H

#include <stdint.h>

//============================================================================================================
// Optimization State
//============================================================================================================

typedef struct {
    uint16_t dispatch_count;      // Count dispatch returns since last optimization
    uint16_t opt_threshold;       // Trigger optimization at this count
    uint16_t compiled_count;      // Number of compiled addresses (debug info)
    uint16_t unique_blocks;       // Count of unique blocks compiled (gate for optimizer)
    uint16_t min_blocks_required; // Minimum blocks before optimizer can run
    uint8_t  opt_in_progress;     // Flag: optimization currently running
    uint8_t  phase;               // Current phase (for multi-frame if needed)
} opt_state_t;

extern opt_state_t opt_state;

//============================================================================================================
// Optimization Thresholds
//============================================================================================================

// For testing - run optimization very frequently
#define OPT_THRESHOLD_DEBUG     5

// For normal use
#define OPT_THRESHOLD_NORMAL    1000
#define OPT_THRESHOLD_LAZY      10000

// Minimum unique blocks compiled before optimizer can run
// This ensures flash has enough content to make optimization worthwhile
#define OPT_MIN_BLOCKS_DEBUG    10
#define OPT_MIN_BLOCKS_NORMAL   200

//============================================================================================================
// ROM Address Range for Scanning
// These should match the game's address space
//============================================================================================================

// Exidy games use address space from ROM_OFFSET to 0x4000
// We scan the full range that might contain code
#define ROM_START   0x2800  // Lowest possible code address (Side Track)
#define ROM_END     0x4000  // End of ROM space

//============================================================================================================
// CHR-RAM Layout (8KB at PPU $0000-$1FFF)
//============================================================================================================

// CHR-RAM is used as temporary storage during optimization
// Total: 8KB = 8192 bytes
//
// Layout:
//   $0000-$0FFF: compiled_addrs[2048] - list of source PC addresses to recompile
//   $1000-$1FFF: new_native[2048]     - corresponding native addresses after rebuild
//
// Each entry is 16-bit, so 2048 entries = 4KB per array

#define CHR_RAM_ADDR_LIST       0x0000
#define CHR_RAM_NATIVE_LIST     0x1000
#define CHR_RAM_MAX_ENTRIES     2048

//============================================================================================================
// PPU Registers
//============================================================================================================

#define PPU_CTRL    (*(volatile uint8_t*)0x2000)
#define PPU_MASK    (*(volatile uint8_t*)0x2001)
#define PPU_STATUS  (*(volatile uint8_t*)0x2002)
#define PPU_SCROLL  (*(volatile uint8_t*)0x2005)
#define PPU_ADDR    (*(volatile uint8_t*)0x2006)
#define PPU_DATA    (*(volatile uint8_t*)0x2007)

//============================================================================================================
// Function Declarations
//============================================================================================================

// Main entry points
void opt_init(uint16_t threshold, uint16_t min_blocks);
void opt_check_trigger(void);
void opt_notify_block_compiled(void);  // Call when a new unique block is compiled
void opt_mark_sector_in_use(uint8_t bank);  // Mark sector as containing code
void opt_run_full_rebuild(void);

// PPU control (used internally)
void ppu_wait_vblank(void);
void ppu_disable_rendering(void);
void ppu_enable_rendering(void);

#endif // OPTIMIZER_H
