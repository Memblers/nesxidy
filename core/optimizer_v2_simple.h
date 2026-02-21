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

// Static-compile drain: aggressively resolve as many pending patches
// and epilogues as possible before execution begins.
void opt2_drain_static_patches(void);

// Get statistics for debugging
void opt2_get_stats(uint16_t *total, uint16_t *direct, uint16_t *stub, uint16_t *pending);

// Reset state (for testing)
void opt2_reset(void);

#endif
