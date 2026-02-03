/**
 * optimizer_v2_simple.h - In-place branch patching optimizer
 */

#ifndef OPTIMIZER_V2_SIMPLE_H
#define OPTIMIZER_V2_SIMPLE_H

#include <stdint.h>

// Record a pending branch that needs patching when target is compiled
// branch_offset_addr: flash address of branch offset byte
// jmp_operand_addr: flash address of JMP operand (low byte)
// code_bank: bank containing the code
// target_pc: 6502 PC the branch wants to reach
// branch_patch_value: value to patch the branch offset to
void opt2_record_pending_branch(uint16_t branch_offset_addr, uint16_t jmp_operand_addr, uint8_t code_bank, uint16_t target_pc, uint8_t branch_patch_value);

// Called when a block is compiled - resolves any pending patches to this block
// block_pc: 6502 PC of the block that was just compiled
// native_addr: flash address where block starts
// native_bank: bank containing the compiled block
void opt2_notify_block_compiled(uint16_t block_pc, uint16_t native_addr, uint8_t native_bank);

// Periodic sweep to check pending patches (called every 8 blocks)
// Currently just a placeholder for future expansion
void opt2_sweep_pending_patches(void);

// Get statistics for debugging
void opt2_get_stats(uint16_t *total, uint16_t *direct, uint16_t *stub, uint16_t *pending);

// Reset state (for testing)
void opt2_reset(void);

#endif
