/**
 * optimizer.c - Flash cache optimization system
 * 
 * The trampolines in opt_trampoline.s live in the FIXED bank.
 * They call flash functions (which switch banks) and then
 * switch back to bank1 before returning to the caller.
 * 
 * Optimization Process (sector-by-sector):
 * For each PC table sector (2048 addresses):
 *   1. Backup sector to CHR-RAM
 *   2. Erase sector
 *   3. Recompile addresses found in backup
 *   4. Write new flags, restore unchanged entries
 *   5. Handle corresponding flag sector
 */

#pragma section bank1

#include <stdint.h>
#include "optimizer.h"
#include "../config.h"
#include "../dynamos.h"
#include "../mapper30.h"
#include "../exidy.h"

extern void bankswitch_prg(__reg("a") uint8_t bank);
extern uint8_t flash_block_flags[];
extern const uint8_t flash_cache_pc_flags[];
extern uint8_t read6502(uint16_t address);

// Trampoline functions (in opt_trampoline.s, fixed bank)
extern void opt_tramp_erase(uint16_t addr, uint8_t bank);
extern void opt_tramp_program(uint16_t addr, uint8_t bank, uint8_t data);
extern uint8_t opt_tramp_readflag(uint16_t addr);
extern void opt_setup_trampolines(void);

// From optimizer_v2.c (sector evacuation with erase)
extern void opt_do_recompile(void);

//============================================================================================================
// State (defined in optimizer_fixed.c for proper BSS initialization)
//============================================================================================================

//============================================================================================================
// PPU Control
//============================================================================================================

void ppu_wait_vblank(void) {
    while (!(PPU_STATUS & 0x80)) { }
}

void ppu_disable_rendering(void) {
    ppu_wait_vblank();
    PPU_MASK = 0x00;
    PPU_CTRL = 0x00;
}

void ppu_enable_rendering(void) {
    ppu_wait_vblank();
    PPU_MASK = 0x1E;
    PPU_CTRL = 0x90;
}

//============================================================================================================
// Main Entry Point
//============================================================================================================

void opt_run_full_rebuild(void) {
    // No PPU manipulation - it corrupts the emulated game's PPU state
    opt_state.opt_in_progress = 1;
    
    opt_do_recompile();
    
    opt_state.opt_in_progress = 0;
}

