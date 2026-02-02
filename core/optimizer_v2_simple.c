/**
 * optimizer_v2_simple.c - In-place branch patching optimizer
 * 
 * Theory of operation:
 * 
 * 1. Unresolved branches are compiled as:
 *        BNE +5          ; Inverted condition, skip JMP if NOT taken
 *        JMP $FFFF       ; Placeholder (all bits set for patching)
 *    +5: ...             ; Fall-through continues
 * 
 *    IMPORTANT: The JMP $FFFF is NEVER executed initially because we only
 *    generate this pattern for branches where the condition is typically taken.
 *    The inverted branch skips the JMP. When target is resolved, we patch it.
 * 
 * 2. When target block is compiled, we can patch JMP $FFFF -> JMP $XXYY:
 *    - Flash can only clear bits (1->0), so $FFFF can become any address
 *    - Just write the two operand bytes directly
 * 
 * 3. No sector evacuation needed - all patches are additive (bit-clearing)
 */

#include <stdint.h>
#include "../config.h"
#include "../mapper30.h"

//============================================================================
// External references (from fixed bank)
//============================================================================

extern void bankswitch_prg(__reg("a") uint8_t bank);
extern void flash_byte_program(uint16_t addr, uint8_t bank, uint8_t data);
extern uint8_t mapper_prg_bank;

// This module goes in bank1 (switchable) to save fixed bank space
#pragma section bank1

// Alignment for direct patching (must be power of 2)
#define PATCH_ALIGNMENT 16

// Maximum pending patches to track
#define MAX_PENDING_PATCHES 64

//============================================================================
// Pending patch tracking
//============================================================================

typedef struct {
    uint16_t branch_offset_addr;  // Address of branch offset byte (patch $03 -> $00)
    uint16_t jmp_operand_addr;    // Address of JMP operand in flash (low byte)
    uint8_t  patch_bank;          // Bank containing the code
    uint16_t target_pc;           // 6502 PC this branch wants to reach
} pending_patch_t;

static pending_patch_t pending_patches[MAX_PENDING_PATCHES];
static uint8_t pending_count = 0;

//============================================================================
// Statistics (for later optimization)
//============================================================================

typedef struct {
    uint16_t total_branches;      // Total branches compiled
    uint16_t direct_patches;      // Patched directly (aligned target)
    uint16_t stub_patches;        // Patched via stub (unaligned)
    uint16_t pending_patches;     // Still waiting for target
} opt_stats_t;

static opt_stats_t stats = {0};

//============================================================================
// External references
//============================================================================

extern void bankswitch_prg(__reg("a") uint8_t bank);
extern void flash_byte_program(uint16_t addr, uint8_t bank, uint8_t data);

//============================================================================
// API: Record a pending branch patch
//============================================================================

void opt2_record_pending_branch(uint16_t branch_offset_addr, uint16_t jmp_operand_addr, uint8_t code_bank, uint16_t target_pc) {
    stats.total_branches++;
    
    if (pending_count >= MAX_PENDING_PATCHES) {
        // Table full - just drop it, branch will stay slow
        return;
    }
    
    pending_patches[pending_count].branch_offset_addr = branch_offset_addr;
    pending_patches[pending_count].jmp_operand_addr = jmp_operand_addr;
    pending_patches[pending_count].patch_bank = code_bank;
    pending_patches[pending_count].target_pc = target_pc;
    pending_count++;
    stats.pending_patches++;
}

//============================================================================
// API: Notify that a block was compiled - check if any pending patches can resolve
//============================================================================

void opt2_notify_block_compiled(uint16_t block_pc, uint16_t native_addr, uint8_t native_bank) {
    uint8_t i = 0;
    
    while (i < pending_count) {
        if (pending_patches[i].target_pc == block_pc) {
            // Found a patch that targets this block!
            
            uint16_t target = native_addr;
            
            // Validate: Read the bytes we're about to patch and verify they're correct
            bankswitch_prg(pending_patches[i].patch_bank);
            uint8_t offset_byte = *(volatile uint8_t*)pending_patches[i].branch_offset_addr;
            uint8_t jmp_lo = *(volatile uint8_t*)pending_patches[i].jmp_operand_addr;
            uint8_t jmp_hi = *(volatile uint8_t*)(pending_patches[i].jmp_operand_addr + 1);
            
            if (offset_byte != 0x03 || jmp_lo != 0xFF || jmp_hi != 0xFF) {
                // ERROR: Bytes don't match expected pattern - don't patch
                bankswitch_prg(mapper_prg_bank);
                i++;
                continue;
            }
            
            // Check alignment
            if ((target & (PATCH_ALIGNMENT - 1)) == 0) {
                // Aligned - patch directly
                stats.direct_patches++;
            } else {
                // Not aligned - would need stub
                // For now, just patch directly anyway (we'll add stubs later)
                stats.stub_patches++;
            }
            
            // Two patches needed:
            // 1. Branch offset: $03 -> $00 (clears bits 0,1 to jump to fast path)
            // 2. JMP operand: $FFFF -> native_addr
            
            // Patch 1: Branch offset $03 -> $00
            flash_byte_program(pending_patches[i].branch_offset_addr, pending_patches[i].patch_bank, 0x00);
            
            // Patch 2: JMP operand (2 bytes: low, high)
            // $FFFF -> $target requires clearing bits (always possible)
            uint8_t lo = target & 0xFF;
            uint8_t hi = (target >> 8) & 0xFF;
            
            flash_byte_program(pending_patches[i].jmp_operand_addr, pending_patches[i].patch_bank, lo);
            flash_byte_program(pending_patches[i].jmp_operand_addr + 1, pending_patches[i].patch_bank, hi);
            
            // Remove from pending list (swap with last)
            pending_count--;
            stats.pending_patches--;
            if (i < pending_count) {
                pending_patches[i] = pending_patches[pending_count];
            }
            // Don't increment i - check swapped entry
        } else {
            i++;
        }
    }
}

//============================================================================
// API: Sweep pending patches - re-check if any can now be resolved
//============================================================================

void opt2_sweep_pending_patches(void) {
    // This is called periodically (every 8 blocks) to re-scan pending patches
    // In case targets have been compiled since they were recorded.
    // We don't re-check here because targets are notified immediately as they compile.
    // But this function exists for future expansion (e.g., triggering compilation
    // of pending targets, or batch processing).
    
    // For now, just a no-op that could be expanded later.
    // All patch resolution happens in opt2_notify_block_compiled().
}

//============================================================================
// API: Get statistics (for debugging)
//============================================================================

void opt2_get_stats(uint16_t *total, uint16_t *direct, uint16_t *stub, uint16_t *pending) {
    *total = stats.total_branches;
    *direct = stats.direct_patches;
    *stub = stats.stub_patches;
    *pending = stats.pending_patches;
}

//============================================================================
// API: Reset (for testing)
//============================================================================

void opt2_reset(void) {
    pending_count = 0;
    stats.total_branches = 0;
    stats.direct_patches = 0;
    stats.stub_patches = 0;
    stats.pending_patches = 0;
}
