/**
 * optimizer_v2_simple.h - In-place branch patching optimizer
 */

#ifndef OPTIMIZER_V2_SIMPLE_H
#define OPTIMIZER_V2_SIMPLE_H

#include <stdint.h>
#include "../config.h"

// Record a pending branch that needs patching when target is compiled
// branch_offset_addr: flash address of branch offset byte
// jmp_operand_addr: flash address of JMP operand (low byte)
// code_bank: bank containing the code
// target_pc: 6502 PC the branch wants to reach
// branch_patch_value: value to patch the branch offset to
void opt2_record_pending_branch(uint16_t branch_offset_addr, uint16_t jmp_operand_addr, uint8_t code_bank, uint16_t target_pc, uint8_t branch_patch_value);

// Fixed-bank trampoline: safely calls opt2_record_pending_branch from bank2
void opt2_record_pending_branch_safe(uint16_t branch_offset_addr, uint16_t jmp_operand_addr, uint8_t code_bank, uint16_t target_pc, uint8_t branch_patch_value);

// Called when a block is compiled - resolves any pending patches to this block
// block_pc: 6502 PC of the block that was just compiled
// native_addr: flash address where block starts
// native_bank: bank containing the compiled block
void opt2_notify_block_compiled(uint16_t block_pc, uint16_t native_addr, uint8_t native_bank);

// Periodic sweep to re-check pending branch patches against PC flags table
void opt2_sweep_pending_patches(void);

// Scan all used flash blocks for patchable epilogues that can now be chained.
// Finds epilogues by byte signature, reads embedded exit_pc, checks if compiled,
// and patches in-place. No queue needed — works retroactively on all blocks.
#ifdef ENABLE_PATCHABLE_EPILOGUE
void opt2_scan_and_patch_epilogues(void);
#endif

// Exhaustive link resolve: scans ALL sectors, ALL blocks in one pass.
// Resolves pending branch patches, patchable epilogues, and inline JMP $FFFF
// patterns.  Expensive (~50-200ms) but eliminates all unnecessary dispatch_on_pc
// round-trips.  Called automatically when compile phase settles.
#ifdef ENABLE_PATCHABLE_EPILOGUE
void opt2_full_link_resolve(void);
#endif

// Per-frame settling detector tick — call once per vblank from main loop.
// Tracks unique_blocks; when no new blocks compile for SETTLE_FRAMES frames,
// automatically fires opt2_full_link_resolve() once.  After settling, runs
// lighter periodic sweeps to catch any late-compiled blocks.
void opt2_frame_tick(void);

// Static-compile drain: aggressively resolve as many pending patches
// and epilogues as possible before execution begins.
void opt2_drain_static_patches(void);

// Get statistics for debugging
void opt2_get_stats(uint16_t *total, uint16_t *direct, uint16_t *stub, uint16_t *pending);

// Diagnostic counters: breakdown of why scan-based patches are skipped.
// Reset each time opt2_full_link_resolve() runs.
extern uint16_t opt2_diag_xbank;      // cross-bank target
extern uint16_t opt2_diag_align;      // $FFF0 alignment (low nibble != 0)
extern uint16_t opt2_diag_selfloop;   // self-loop guard
extern uint16_t opt2_diag_nocompile;  // target not compiled or stale

// Reset state (for testing)
void opt2_reset(void);

#endif
