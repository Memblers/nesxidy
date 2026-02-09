/**
 * optimizer_v2_simple.c - In-place branch patching optimizer
 * 
 * Theory of operation:
 * 
 * BRANCH PATCHING (queue-based):
 * 1. Unresolved branches are compiled as:
 *        BNE +5          ; Inverted condition, skip JMP if NOT taken
 *        JMP $FFFF       ; Placeholder (all bits set for patching)
 *    +5: ...             ; Fall-through continues
 * 
 *    When target block is compiled, we patch JMP $FFFF -> JMP $XXYY.
 *    Flash can only clear bits (1->0), so $FFFF can become any address.
 *    Pending branches are tracked in a small RAM queue.
 * 
 * EPILOGUE CHAINING (scan-based, no queue):
 * 2. Patchable epilogues have the signature:
 *        PHP / CLC / BCC +4 / PLP / JMP $FFFF / STA _a / ...
 *    When opt2_scan_and_patch_epilogues() is called, it scans all used
 *    flash blocks, finds patchable epilogues by their byte signature,
 *    reads the embedded exit_pc, checks if that PC is now compiled,
 *    and patches the BCC offset + JMP target if they're in the same bank.
 *    This requires NO queue and works retroactively on all blocks.
 */

#include <stdint.h>
#include "../config.h"
#include "../mapper30.h"
#include "../dynamos.h"

//============================================================================
// External references (from fixed bank)
//============================================================================

extern void bankswitch_prg(__reg("a") uint8_t bank);
extern void flash_byte_program(uint16_t addr, uint8_t bank, uint8_t data);
extern uint8_t mapper_prg_bank;
extern uint8_t flash_block_flags[];
extern uint8_t flash_cache_pc[];
extern const uint8_t flash_cache_pc_flags[];

// Functions that call bankswitch_prg() MUST be in the fixed bank ($C000-$FFFF)
// or WRAM — NOT bank1.  bankswitch changes $8000-$BFFF, so code there would
// be swapped out from under the CPU.
//
// Layout:
//   bank1  — opt2_record_pending_branch, opt2_notify_block_compiled,
//            opt2_get_stats, opt2_reset  (none of these bankswitch)
//   text   — opt2_sweep_pending_patches, opt2_scan_and_patch_epilogues
//            (these DO bankswitch, so they live in the always-mapped fixed bank)

// Alignment for direct patching (must be power of 2)
#define PATCH_ALIGNMENT 16

// Maximum pending patches to track (branches only — epilogues use scan)
#define MAX_PENDING_PATCHES 32

//============================================================================
// Pending patch tracking — flat arrays for compact 6502 code generation.
// Struct-of-arrays avoids expensive base+i*stride+offset indexing.
//============================================================================

static uint16_t pp_branch_addr[MAX_PENDING_PATCHES];
static uint16_t pp_jmp_addr[MAX_PENDING_PATCHES];
static uint8_t  pp_bank[MAX_PENDING_PATCHES];
static uint16_t pp_target_pc[MAX_PENDING_PATCHES];
static uint8_t  pp_patch_val[MAX_PENDING_PATCHES];
static uint8_t pending_count = 0;

//============================================================================
// Statistics
//============================================================================

static uint16_t stat_total = 0;
static uint16_t stat_direct = 0;
static uint16_t stat_pending = 0;

//============================================================================
// The following function does NOT bankswitch, so it can live in bank1.
// Callers must bankswitch_prg(1) before calling.
//============================================================================
#pragma section bank1

void opt2_record_pending_branch(uint16_t branch_offset_addr, uint16_t jmp_operand_addr, uint8_t code_bank, uint16_t target_pc, uint8_t branch_patch_value) {
    stat_total++;
    if (pending_count >= MAX_PENDING_PATCHES) return;
    pp_branch_addr[pending_count] = branch_offset_addr;
    pp_jmp_addr[pending_count] = jmp_operand_addr;
    pp_bank[pending_count] = code_bank;
    pp_target_pc[pending_count] = target_pc;
    pp_patch_val[pending_count] = branch_patch_value;
    pending_count++;
    stat_pending++;
}

//============================================================================
// All functions below are in the fixed bank ($C000-$FFFF, section "text")
// which is always mapped regardless of which bank is in $8000-$BFFF.
// opt2_notify is a no-op stub but must be callable from any bank context.
// opt2_sweep and opt2_scan call bankswitch_prg() and MUST be here.
//============================================================================
#pragma section default

//============================================================================
// API: Notify that a block was compiled
// Lightweight stub — actual patch resolution deferred to sweep.
//============================================================================

void opt2_notify_block_compiled(uint16_t block_pc, uint16_t native_addr, uint8_t native_bank) {
    (void)block_pc; (void)native_addr; (void)native_bank;
}


//============================================================================
// API: Sweep pending patches - re-check if any can now be resolved
//============================================================================

void opt2_sweep_pending_patches(void) {
    uint8_t i = 0;
    while (i < pending_count) {
        uint16_t tpc = pp_target_pc[i];
        
        // Check if target is compiled
        bankswitch_prg((tpc >> 14) + BANK_PC_FLAGS);
        uint8_t flag = flash_cache_pc_flags[tpc & FLASH_BANK_MASK];
        
        if ((flag & RECOMPILED) || (flag & 0x1F) != pp_bank[i]) {
            i++;
            continue;
        }
        
        // Read native address
        bankswitch_prg((tpc >> 13) + BANK_PC);
        uint16_t pa = (tpc << 1) & FLASH_BANK_MASK;
        uint16_t na = flash_cache_pc[pa] | (flash_cache_pc[pa+1] << 8);
        
        // Validate patch site
        bankswitch_prg(pp_bank[i]);
        if (*(volatile uint8_t*)pp_jmp_addr[i] != 0xFF ||
            *(volatile uint8_t*)(pp_jmp_addr[i]+1) != 0xFF) {
            --pending_count;
            pp_branch_addr[i] = pp_branch_addr[pending_count];
            pp_jmp_addr[i] = pp_jmp_addr[pending_count];
            pp_bank[i] = pp_bank[pending_count];
            pp_target_pc[i] = pp_target_pc[pending_count];
            pp_patch_val[i] = pp_patch_val[pending_count];
            stat_pending--;
            continue;
        }
        
        // Patch
        flash_byte_program(pp_branch_addr[i], pp_bank[i], pp_patch_val[i]);
        flash_byte_program(pp_jmp_addr[i], pp_bank[i], na & 0xFF);
        flash_byte_program(pp_jmp_addr[i]+1, pp_bank[i], (na >> 8) & 0xFF);
        stat_direct++;
        --pending_count;
        pp_branch_addr[i] = pp_branch_addr[pending_count];
        pp_jmp_addr[i] = pp_jmp_addr[pending_count];
        pp_bank[i] = pp_bank[pending_count];
        pp_target_pc[i] = pp_target_pc[pending_count];
        pp_patch_val[i] = pp_patch_val[pending_count];
        stat_pending--;
    }
}

//============================================================================
// API: Scan flash blocks and patch any unresolved patchable epilogues
//
// Each block stores its epilogue start offset at byte 255.  We read that
// byte directly (no pattern scanning), verify the JMP operand is still
// $FFFF, read the embedded exit_pc, and patch if the target is compiled
// in the same bank.
//
// Byte 255 == $FF means no epilogue recorded (block may be empty or
// non-patchable).  Valid offsets are 0..234 (CODE_SIZE max).
//============================================================================

#ifdef ENABLE_PATCHABLE_EPILOGUE
// Batch size per call — keeps function compact and spreads work over frames
#define EPILOGUE_SCAN_BATCH 32

void opt2_scan_and_patch_epilogues(void) {
    static uint16_t cursor = 0;
    uint8_t remaining = EPILOGUE_SCAN_BATCH;
    
    while (remaining--) {
        if (cursor >= FLASH_CACHE_BLOCKS)
            cursor = 0;
        
        uint16_t block = cursor++;
        
        // Check if block is in use
        bankswitch_prg(BANK_FLASH_BLOCK_FLAGS);
        if (flash_block_flags[block] & FLASH_AVAILABLE)
            continue;
        
        // Compute block's flash address and bank
        uint8_t code_bank = (block >> 6) + BANK_CODE;
        uint16_t code_base = ((block & 0x3F) << 8) + FLASH_BANK_BASE;
        
        // Read epilogue offset from byte 255
        bankswitch_prg(code_bank);
        volatile uint8_t *base = (volatile uint8_t *)code_base;
        
        uint8_t off = base[255];
        if (off == 0xFF) continue;
        if (base[off+6] != 0xFF || base[off+7] != 0xFF) continue;
        
        // Read exit_pc from embedded LDA immediates
        uint16_t exit_pc = base[off+11] | ((uint16_t)base[off+15] << 8);
        
        // Check if exit_pc is compiled
        bankswitch_prg((exit_pc >> 14) + BANK_PC_FLAGS);
        uint8_t flag = flash_cache_pc_flags[exit_pc & FLASH_BANK_MASK];
        if (flag & RECOMPILED) continue;
        if ((flag & 0x1F) != code_bank) continue;
        
        // Read native address
        bankswitch_prg((exit_pc >> 13) + BANK_PC);
        uint16_t pc_addr = (exit_pc << 1) & FLASH_BANK_MASK;
        uint16_t na = flash_cache_pc[pc_addr] | (flash_cache_pc[pc_addr+1] << 8);
        
        // Patch
        flash_byte_program(code_base + off + 3, code_bank, 0);
        flash_byte_program(code_base + off + 6, code_bank, na & 0xFF);
        flash_byte_program(code_base + off + 7, code_bank, (na >> 8) & 0xFF);
        stat_direct++;
    }
}
#endif  // ENABLE_PATCHABLE_EPILOGUE

//============================================================================
// API: Get statistics (for debugging)
//============================================================================

//============================================================================
// Small utility functions — no bankswitching, safe for bank1
//============================================================================
#pragma section bank1

void opt2_get_stats(uint16_t *total, uint16_t *direct, uint16_t *stub, uint16_t *pending) {
    *total = stat_total;
    *direct = stat_direct;
    *stub = 0;
    *pending = stat_pending;
}

//============================================================================
// API: Reset (for testing)
//============================================================================

void opt2_reset(void) {
    pending_count = 0;
    stat_total = 0;
    stat_direct = 0;
    stat_pending = 0;
}
