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

// WRAM helper: reads one byte from an arbitrary bank and restores the
// caller's bank mapping before returning.  Lives in WRAM ($6000-$7FFF)
// so it is always reachable.  Code in switchable banks ($8000-$BFFF)
// MUST use this instead of bankswitch_prg() — calling bankswitch_prg()
// from a switchable bank causes the RTS to return into the wrong bank.
extern uint8_t peek_bank_byte(uint8_t bank, uint16_t addr);

// Layout:
//   bank1  — opt2_record_pending_branch, opt2_get_stats, opt2_reset
//            (none of these bankswitch)
//   bank2  — opt2_sweep_pending_patches_b2, opt2_scan_and_patch_epilogues_b2
//            (use peek_bank_byte for cross-bank reads; NEVER call bankswitch_prg)
//   default — trampolines: opt2_sweep_pending_patches, opt2_scan_and_patch_epilogues
//            (switch to bank 2, call _b2 impl, restore caller's bank)

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

uint16_t opt2_stat_total = 0;
uint16_t opt2_stat_direct = 0;
uint16_t opt2_stat_pending = 0;

//============================================================================
// The following function does NOT bankswitch, so it can live in bank1.
// Callers must bankswitch_prg(1) before calling.
//============================================================================
#pragma section bank1

void opt2_record_pending_branch(uint16_t branch_offset_addr, uint16_t jmp_operand_addr, uint8_t code_bank, uint16_t target_pc, uint8_t branch_patch_value) {
    opt2_stat_total++;
    if (pending_count >= MAX_PENDING_PATCHES) return;
    pp_branch_addr[pending_count] = branch_offset_addr;
    pp_jmp_addr[pending_count] = jmp_operand_addr;
    pp_bank[pending_count] = code_bank;
    pp_target_pc[pending_count] = target_pc;
    pp_patch_val[pending_count] = branch_patch_value;
    pending_count++;
    opt2_stat_pending++;
}

//============================================================================
// Functions below need cross-bank reads and MUST live in a safe location.
// The _b2 implementations run from bank 2 and use peek_bank_byte() (WRAM)
// for all cross-bank reads.  They NEVER call bankswitch_prg() directly.
// Fixed-bank trampolines handle the bank2 entry/exit.
//============================================================================

//============================================================================
// opt2_notify_block_compiled — no-op stub, must be callable from any context.
//============================================================================
#pragma section default

void opt2_notify_block_compiled(uint16_t block_pc, uint16_t native_addr, uint8_t native_bank) {
    (void)block_pc; (void)native_addr; (void)native_bank;
}

// Fixed-bank trampoline: bank2 code can safely call this to reach
// opt2_record_pending_branch (bank1).  Must be in the fixed bank so
// it's always reachable regardless of which swappable bank is mapped.
void opt2_record_pending_branch_safe(uint16_t branch_offset_addr,
                                     uint16_t jmp_operand_addr,
                                     uint8_t code_bank,
                                     uint16_t target_pc,
                                     uint8_t branch_patch_value)
{
    uint8_t saved_bank = mapper_prg_bank;
    bankswitch_prg(1);
    opt2_record_pending_branch(branch_offset_addr, jmp_operand_addr,
                               code_bank, target_pc, branch_patch_value);
    bankswitch_prg(saved_bank);
}

// -------------------------------------------------------------------------
// Epilogue scan cursor (fixed-bank / BSS)
// CRITICAL: Must NOT be inside bank2 pragma — static variables there
// land in flash and are read-only at runtime (writes silently ignored).
// -------------------------------------------------------------------------
static uint16_t opt2_epilogue_cursor;

//============================================================================
// Bank 2 implementations
// CRITICAL: NEVER call bankswitch_prg() from these functions!
// bankswitch_prg() is in the fixed bank — its RTS returns to $8xxx which
// is now mapped to the WRONG bank.  Use peek_bank_byte() (WRAM helper)
// for all cross-bank reads.  flash_byte_program() is safe because it
// lives in WRAM and restores mapper_prg_bank (which stays == 2).
//============================================================================
#pragma section bank2

static void opt2_sweep_pending_patches_b2(void) {
    uint8_t i = 0;
    while (i < pending_count) {
        uint16_t tpc = pp_target_pc[i];
        
        // Check if target is compiled (peek from PC_FLAGS bank)
        uint8_t flag = peek_bank_byte(
            (tpc >> 14) + BANK_PC_FLAGS,
            (uint16_t)&flash_cache_pc_flags[tpc & FLASH_BANK_MASK]);
        
        if ((flag & RECOMPILED) || (flag & 0x1F) != pp_bank[i]) {
            i++;
            continue;
        }
        
        // Read native address (peek from PC bank)
        uint16_t pa = (tpc << 1) & FLASH_BANK_MASK;
        uint8_t pc_bank = (tpc >> 13) + BANK_PC;
        uint16_t na = peek_bank_byte(pc_bank, (uint16_t)&flash_cache_pc[pa])
                    | (peek_bank_byte(pc_bank, (uint16_t)&flash_cache_pc[pa+1]) << 8);
        
        // Don't patch unconditional self-loops (opJMP and epilogue patterns).
        // These use CLC+BCC (always taken), so patching them to jump back into
        // the same block creates a tight infinite native loop that never returns
        // to the dispatcher, blocking NMI/interrupt handling.
        // Conditional branches (21-byte Bxx pattern) are fine — they'll exit
        // the loop when the condition changes.
        // Discriminator: opJMP has jmp_addr - branch_addr == 3 (PLP between BCC and JMP),
        //                Bxx has jmp_addr - branch_addr == 2 (JMP right after branch byte).
        {
            uint16_t gap = pp_jmp_addr[i] - pp_branch_addr[i];
            if (gap == 3) {  // unconditional (opJMP / epilogue pattern)
                uint16_t blk_base = pp_jmp_addr[i] & 0xFF00;
                if (na >= blk_base && na < blk_base + 256) {
                    i++;
                    continue;
                }
            }
        }
        
        // Validate patch site (peek from code bank)
        uint8_t jmp_lo = peek_bank_byte(pp_bank[i], pp_jmp_addr[i]);
        uint8_t jmp_hi = peek_bank_byte(pp_bank[i], pp_jmp_addr[i]+1);
        if (jmp_lo != 0xFF || jmp_hi != 0xFF) {
            --pending_count;
            pp_branch_addr[i] = pp_branch_addr[pending_count];
            pp_jmp_addr[i] = pp_jmp_addr[pending_count];
            pp_bank[i] = pp_bank[pending_count];
            pp_target_pc[i] = pp_target_pc[pending_count];
            pp_patch_val[i] = pp_patch_val[pending_count];
            opt2_stat_pending--;
            continue;
        }
        
        // Patch — flash_byte_program lives in WRAM, handles its own bankswitching.
        // mapper_prg_bank is still 2 (set by trampoline), so flash_byte_program
        // restores to bank 2 when done.
        flash_byte_program(pp_branch_addr[i], pp_bank[i], pp_patch_val[i]);
        flash_byte_program(pp_jmp_addr[i], pp_bank[i], na & 0xFF);
        flash_byte_program(pp_jmp_addr[i]+1, pp_bank[i], (na >> 8) & 0xFF);
        opt2_stat_direct++;
        --pending_count;
        pp_branch_addr[i] = pp_branch_addr[pending_count];
        pp_jmp_addr[i] = pp_jmp_addr[pending_count];
        pp_bank[i] = pp_bank[pending_count];
        pp_target_pc[i] = pp_target_pc[pending_count];
        pp_patch_val[i] = pp_patch_val[pending_count];
        opt2_stat_pending--;
    }
}

#ifdef ENABLE_PATCHABLE_EPILOGUE
#define EPILOGUE_SCAN_BATCH 32

static void opt2_scan_and_patch_epilogues_b2(void) {
    uint8_t remaining = EPILOGUE_SCAN_BATCH;
    
    while (remaining--) {
        if (opt2_epilogue_cursor >= FLASH_CACHE_BLOCKS)
            opt2_epilogue_cursor = 0;
        
        uint16_t block = opt2_epilogue_cursor++;
        
        // Check if block is in use (peek from block-flags bank)
        uint8_t bflag = peek_bank_byte(
            BANK_FLASH_BLOCK_FLAGS,
            (uint16_t)&flash_block_flags[block]);
        if (bflag & FLASH_AVAILABLE)
            continue;
        
        // Compute block's flash address and bank
        uint8_t code_bank = (block >> 6) + BANK_CODE;
        uint16_t code_base = ((block & 0x3F) << 8) + FLASH_BANK_BASE;
        
        // Read epilogue offset and signature from code bank (via peek)
        uint8_t off = peek_bank_byte(code_bank, code_base + 255);
        if (off == 0xFF) continue;
        
        // Check if fast-path JMP operand is still unpatched ($FFFF)
        uint8_t jmp_lo = peek_bank_byte(code_bank, code_base + off + 6);
        uint8_t jmp_hi = peek_bank_byte(code_bank, code_base + off + 7);
        if (jmp_lo != 0xFF || jmp_hi != 0xFF) continue;  // already patched
        
        // Read exit_pc from embedded LDA immediates at +11 and +15
        uint16_t exit_pc = peek_bank_byte(code_bank, code_base + off + 11)
                | ((uint16_t)peek_bank_byte(code_bank, code_base + off + 15) << 8);
        
        // Check if exit_pc is compiled (peek from PC_FLAGS bank)
        uint8_t flag = peek_bank_byte(
            (exit_pc >> 14) + BANK_PC_FLAGS,
            (uint16_t)&flash_cache_pc_flags[exit_pc & FLASH_BANK_MASK]);
        if (flag & RECOMPILED) continue;
        
        // Read native address (peek from PC bank)
        uint16_t pc_addr = (exit_pc << 1) & FLASH_BANK_MASK;
        uint8_t pc_bank = (exit_pc >> 13) + BANK_PC;
        uint16_t na = peek_bank_byte(pc_bank, (uint16_t)&flash_cache_pc[pc_addr])
                    | (peek_bank_byte(pc_bank, (uint16_t)&flash_cache_pc[pc_addr+1]) << 8);
        
        // Don't patch self-loops: the patchable epilogue's fast path is
        // unconditional (CLC+BCC always taken), so patching it to jump back
        // into the same block creates a tight infinite loop that never returns
        // to the dispatcher (preventing interrupt handling and timing updates).
        // Let self-referencing blocks go through the regular path instead.
        if (na >= code_base && na < code_base + 256) continue;
        
        uint8_t target_bank = flag & 0x1F;
        
        if (target_bank == code_bank) {
            // Same-bank: patch fast-path JMP directly to native address
            // BCC offset 4→0 activates fast path: PLP / JMP native_addr
            flash_byte_program(code_base + off + 3, code_bank, 0);
            flash_byte_program(code_base + off + 6, code_bank, na & 0xFF);
            flash_byte_program(code_base + off + 7, code_bank, (na >> 8) & 0xFF);
        } else {
            // Cross-bank: patch fast-path JMP to +21 (xbank setup code),
            // then patch the three LDA immediates in the setup code:
            //   +25 = target addr lo, +30 = target addr hi, +35 = target bank
            uint16_t setup_addr = code_base + off + 21;
            flash_byte_program(code_base + off + 3, code_bank, 0);
            flash_byte_program(code_base + off + 6, code_bank, setup_addr & 0xFF);
            flash_byte_program(code_base + off + 7, code_bank, (setup_addr >> 8) & 0xFF);
            flash_byte_program(code_base + off + 25, code_bank, na & 0xFF);
            flash_byte_program(code_base + off + 30, code_bank, (na >> 8) & 0xFF);
            flash_byte_program(code_base + off + 35, code_bank, target_bank);
        }
        opt2_stat_direct++;
    }
}
#endif  // ENABLE_PATCHABLE_EPILOGUE

//============================================================================
// Fixed-bank trampolines: switch to bank 2, call implementation, restore bank
//============================================================================
#pragma section default

void opt2_sweep_pending_patches(void) {
    uint8_t saved_bank = mapper_prg_bank;
    bankswitch_prg(2);
    opt2_sweep_pending_patches_b2();
    bankswitch_prg(saved_bank);
}

#ifdef ENABLE_PATCHABLE_EPILOGUE
void opt2_scan_and_patch_epilogues(void) {
    uint8_t saved_bank = mapper_prg_bank;
    bankswitch_prg(2);
    opt2_scan_and_patch_epilogues_b2();
    bankswitch_prg(saved_bank);
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
    *total = opt2_stat_total;
    *direct = opt2_stat_direct;
    *stub = 0;
    *pending = opt2_stat_pending;
}

//============================================================================
// API: Reset — must be in the fixed bank so sa_run() can call it
// directly without a trampoline.  Only touches WRAM variables.
//============================================================================
#pragma section default

void opt2_reset(void) {
    pending_count = 0;
    opt2_stat_total = 0;
    opt2_stat_direct = 0;
    opt2_stat_pending = 0;
}
