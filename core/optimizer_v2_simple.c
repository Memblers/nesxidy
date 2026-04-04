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
// flash_cache_pc / flash_cache_pc_flags are macros in core/cache.h
extern uint16_t sector_free_offset[];

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

// First header offset in each sector: code aligned to 16 means header at
// ((BLOCK_HEADER_SIZE + BLOCK_ALIGNMENT - 1) & ~(BLOCK_ALIGNMENT - 1)) - BLOCK_HEADER_SIZE = 8
#define SECTOR_FIRST_HEADER  (((BLOCK_HEADER_SIZE + BLOCK_ALIGNMENT - 1) & ~(BLOCK_ALIGNMENT - 1)) - BLOCK_HEADER_SIZE)

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
uint16_t opt2_blocks_compiled = 0;  // incremented by opt2_notify_block_compiled

// Diagnostic counters: breakdown of why patches are skipped.
// Reset by opt2_full_link_resolve() before each exhaustive scan pass.
uint16_t opt2_diag_xbank = 0;      // cross-bank target (can't direct-patch)
uint16_t opt2_diag_align = 0;      // $FFF0 alignment (low nibble != 0)
uint16_t opt2_diag_selfloop = 0;   // self-loop guard
uint16_t opt2_diag_nocompile = 0;  // target not compiled or stale

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
    if (block_pc | native_addr | native_bank) {}  // suppress unused-parameter warning
    if (opt2_blocks_compiled < 0xFFFF) opt2_blocks_compiled++;
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
// Sector-based cursor: which sector (0..59) and byte offset within sector.
static uint8_t opt2_epilogue_sector;
static uint16_t opt2_epilogue_offset;

// Per-sector trampoline pool: 256 bytes at end of each sector ($F00-$FFF)
// = 16 slots on 16-byte boundaries.  Each slot holds JMP na (3 bytes).
// No WRAM tracking — slots are scanned in flash (read $4C = used, $FF = free).
#define SECTOR_TRAMP_POOL_BASE  (FLASH_ERASE_SECTOR_SIZE - 256)
#define SECTOR_TRAMP_SLOTS      16

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
        
#ifdef ENABLE_FFF0_TEMPLATES
        // $FFF0 templates use pp_branch_addr == 0 as a sentinel (no branch
        // byte to patch).  Detect this to use the right validation & patching.
        uint8_t is_fff0 = (pp_branch_addr[i] == 0);
#endif

        // Don't patch unconditional self-loops (opJMP and epilogue patterns).
        // These use CLC+BCC (always taken), so patching them to jump back into
        // the same block creates a tight infinite native loop that never returns
        // to the dispatcher, blocking NMI/interrupt handling.
        // Conditional branches (21-byte Bxx pattern) are fine — they'll exit
        // the loop when the condition changes.
#ifdef ENABLE_FFF0_TEMPLATES
        // $FFF0 JMP templates (is_fff0 && pp_patch_val==0) are unconditional.
        // $FFF0 branch templates (is_fff0 && pp_patch_val!=0) are conditional
        // and safe from self-loops (the branch must be taken for the JMP).
        if (is_fff0) {
            if (pp_patch_val[i] == 0) {
                // Unconditional $FFF0 JMP — check for self-loop
                uint16_t jmp_addr = pp_jmp_addr[i];
                if (na >= jmp_addr - 250 && na <= jmp_addr + 8) {
                    i++;
                    continue;
                }
            }
        } else
#endif
        {
            // Old-style templates: discriminate by gap between branch and JMP
            // Discriminator: opJMP has jmp_addr - branch_addr == 3 (PLP between BCC and JMP),
            //                Bxx has jmp_addr - branch_addr == 2 (JMP right after branch byte).
            uint16_t gap = pp_jmp_addr[i] - pp_branch_addr[i];
            if (gap == 3) {  // unconditional (opJMP / epilogue pattern)
                // Check if target is in the same block as the pattern.
                // The JMP operand is within the block; a block's max total
                // size is BLOCK_HEADER_SIZE + CODE_SIZE + EPILOGUE_SIZE +
                // XBANK_EPILOGUE_SIZE ≈ 258 bytes.  If na is close to the
                // pattern address, assume it's the same block.
                uint16_t jmp_addr = pp_jmp_addr[i];
                if (na >= jmp_addr - 250 && na <= jmp_addr + 8) {
                    i++;
                    continue;
                }
            }
        }
        
        // Validate patch site (peek from code bank)
        uint8_t jmp_lo = peek_bank_byte(pp_bank[i], pp_jmp_addr[i]);
        uint8_t jmp_hi = peek_bank_byte(pp_bank[i], pp_jmp_addr[i]+1);
#ifdef ENABLE_FFF0_TEMPLATES
        // $FFF0 templates have JMP $FFF0 (lo=$F0, hi=$FF) as placeholder.
        // Old templates have JMP $FFFF (lo=$FF, hi=$FF).
        uint8_t expected_lo = is_fff0 ? (FFF0_DISPATCH & 0xFF) : 0xFF;
        if (jmp_lo != expected_lo || jmp_hi != 0xFF) {
#else
        if (jmp_lo != 0xFF || jmp_hi != 0xFF) {
#endif
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
#ifdef ENABLE_FFF0_TEMPLATES
        if (is_fff0) {
            // $FFF0 template: only 2 flash writes (JMP lo + JMP hi).
            // No branch byte to patch — the branch is pre-wired in the template.
            //
            // Non-aligned target handling: if na is not 16-byte aligned, we
            // cannot JMP directly to it from a 16-aligned block (the JMP
            // operand can only be patched to values reachable by clearing bits
            // from $FFF0, which means the low nibble can only go to $x0).
            // Actually — flash can clear ANY bits in $F0 to reach any address,
            // so alignment is not a constraint for the JMP operand itself.
            // The constraint is that $FF can become any value (all bits set).
            // $F0 can only become $x0 where x <= F — i.e., low nibble stuck at 0!
            //
            // So if (na & 0x0F) != 0, we need a local trampoline:
            // Find/use a 16-byte-aligned JMP $FFFF slot in the same block,
            // patch that to JMP na, and patch our JMP $FFF0 to point to the
            // trampoline instead.
            //
            // For now: if target is not reachable (low nibble != 0), skip
            // and leave for later.  TODO: implement local trampoline.
            if ((na & 0x0F) != 0) {
                // Target not aligned — can't patch JMP $FFF0 directly.
                // The low byte $F0 can only clear bits, giving $x0 values.
                // Skip for now; the slow path ($FFF0 dispatch) still works.
                i++;
                continue;
            }
            flash_byte_program(pp_jmp_addr[i], pp_bank[i], na & 0xFF);
            flash_byte_program(pp_jmp_addr[i]+1, pp_bank[i], (na >> 8) & 0xFF);
        } else
#endif
        {
            // Guard: na must be in flash code range.  Writing $FF to flash
            // is a no-op, so if na=$FFFF the branch activates but JMP stays
            // $FFFF → crash.
            if (na < 0x8001 || na >= 0xC000) { i++; continue; }
            // Old-style template: 3 flash writes (branch byte + JMP lo + JMP hi)
            flash_byte_program(pp_branch_addr[i], pp_bank[i], pp_patch_val[i]);
            flash_byte_program(pp_jmp_addr[i], pp_bank[i], na & 0xFF);
            flash_byte_program(pp_jmp_addr[i]+1, pp_bank[i], (na >> 8) & 0xFF);
        }
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
#define EPILOGUE_SCAN_BATCH 64

static void opt2_scan_and_patch_epilogues_b2(void) {
    uint8_t remaining = EPILOGUE_SCAN_BATCH;
    
    while (remaining--) {
        // Wrap around sectors
        if (opt2_epilogue_sector >= FLASH_CACHE_SECTORS) {
            opt2_epilogue_sector = 0;
            opt2_epilogue_offset = SECTOR_FIRST_HEADER;
        }
        
        uint8_t sector = opt2_epilogue_sector;
        uint16_t free_off = sector_free_offset[sector];
        
        // If this sector is empty or cursor is past allocated data, advance
        if (free_off == 0 || opt2_epilogue_offset >= free_off) {
            opt2_epilogue_sector++;
            opt2_epilogue_offset = SECTOR_FIRST_HEADER;
            continue;
        }
        
        // Compute flash bank and base address for this sector
        // Use >>2 and &3 instead of /4 and %4 to avoid 32-bit division
        uint8_t code_bank = (sector >> 2) + BANK_CODE;
        uint16_t sector_base = FLASH_BANK_BASE + ((uint16_t)(sector & 3) << 12);
        uint16_t hdr_addr = sector_base + opt2_epilogue_offset;
        
        // Read block header from flash via peek_bank_byte
        uint8_t code_len = peek_bank_byte(code_bank, hdr_addr + 4);
        uint8_t epi_off = peek_bank_byte(code_bank, hdr_addr + 5);
        
        // Validate header: code_len must be nonzero and reasonable
        if (code_len == 0 || code_len == 0xFF) {
            // Corrupt or unwritten — skip to next sector
            opt2_epilogue_sector++;
            opt2_epilogue_offset = 0;
            continue;
        }
        
        // Advance cursor past this block to next header.
        // Must mirror flash_sector_alloc's layout: next code_start is
        // 16-byte aligned after (cur + BLOCK_HEADER_SIZE), then header
        // sits at code_start - BLOCK_HEADER_SIZE.
        // cur = opt2_epilogue_offset + BLOCK_HEADER_SIZE + code_len
        uint16_t cur_end = opt2_epilogue_offset + BLOCK_HEADER_SIZE + code_len;
        uint16_t next_code = (cur_end + BLOCK_HEADER_SIZE + BLOCK_ALIGNMENT - 1) & ~(uint16_t)(BLOCK_ALIGNMENT - 1);
        opt2_epilogue_offset = next_code - BLOCK_HEADER_SIZE;
        
        // Skip if no valid epilogue offset
        if (epi_off == 0xFF || epi_off == 0) continue;
        
        // Code starts at hdr_addr + BLOCK_HEADER_SIZE
        uint16_t code_base = hdr_addr + BLOCK_HEADER_SIZE;
        
        // Check if fast-path JMP operand is still unpatched ($FFFF)
        uint8_t jmp_lo = peek_bank_byte(code_bank, code_base + epi_off + 6);
        uint8_t jmp_hi = peek_bank_byte(code_bank, code_base + epi_off + 7);
        if (jmp_lo != 0xFF || jmp_hi != 0xFF) continue;  // already patched
        
        // Read exit_pc from header (+2, +3)
        uint16_t exit_pc = peek_bank_byte(code_bank, hdr_addr + 2)
                | ((uint16_t)peek_bank_byte(code_bank, hdr_addr + 3) << 8);
        
        // Check if exit_pc is compiled (peek from PC_FLAGS bank)
        uint8_t flag = peek_bank_byte(
            (exit_pc >> 14) + BANK_PC_FLAGS,
            (uint16_t)&flash_cache_pc_flags[exit_pc & FLASH_BANK_MASK]);
        if (flag & RECOMPILED) continue;
        // Guard: flag==0 is "uninitialized" — dispatch treats as not-compiled
        // (BEQ not_recompiled) but the RECOMPILED bit-7 check misses it.
        if (flag == 0) continue;
        
        // Read native address (peek from PC bank)
        uint16_t pc_addr = (exit_pc << 1) & FLASH_BANK_MASK;
        uint8_t pc_bank = (exit_pc >> 13) + BANK_PC;
        uint16_t na = peek_bank_byte(pc_bank, (uint16_t)&flash_cache_pc[pc_addr])
                    | (peek_bank_byte(pc_bank, (uint16_t)&flash_cache_pc[pc_addr+1]) << 8);
        
        // Guard: native address must be in flash code range ($8001-$BFFF).
        // $FFFF = erased/unwritten table entry; $8000 = partially-written
        // (flag set but address not).  Writing $FF to SST39SF040 is a no-op,
        // so xbank operands stay unpatched → JMP $FFFF crash.
        if (na < 0x8001 || na >= 0xC000) continue;
        
        // Don't patch self-loops: the patchable epilogue's fast path is
        // unconditional (CLC+BCC always taken), so patching it to jump back
        // into the same block creates a tight infinite loop that never returns
        // to the dispatcher (preventing interrupt handling and timing updates).
        if (na >= code_base && na < code_base + code_len) continue;
        
        uint8_t target_bank = flag & 0x1F;
        
        if (target_bank == code_bank) {
            // Same-bank: patch fast-path JMP directly to native address
            // BCC offset 4→0 activates fast path: PLP / JMP native_addr
            flash_byte_program(code_base + epi_off + 3, code_bank, 0);
            flash_byte_program(code_base + epi_off + 6, code_bank, na & 0xFF);
            flash_byte_program(code_base + epi_off + 7, code_bank, (na >> 8) & 0xFF);
        } else {
            // Cross-bank: patch fast-path JMP to +21 (xbank setup code),
            // then patch the three LDA immediates in the setup code:
            //   +25 = target addr lo, +30 = target addr hi, +35 = target bank
            uint16_t setup_addr = code_base + epi_off + 21;
            flash_byte_program(code_base + epi_off + 3, code_bank, 0);
            flash_byte_program(code_base + epi_off + 6, code_bank, setup_addr & 0xFF);
            flash_byte_program(code_base + epi_off + 7, code_bank, (setup_addr >> 8) & 0xFF);
            flash_byte_program(code_base + epi_off + 25, code_bank, na & 0xFF);
            flash_byte_program(code_base + epi_off + 30, code_bank, (na >> 8) & 0xFF);
            flash_byte_program(code_base + epi_off + 35, code_bank, target_bank);
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

// Forward declarations: live in BANK_IR_OPT (bank28 NES / bank29 Exidy).
// Note: vbcc warns about section mismatch — the definition's pragma
// takes precedence for actual placement, warning is harmless.
#ifdef PLATFORM_NES
#pragma section bank28
#else
#pragma section bank29
#endif
void opt2_full_epilogue_scan_b1(void);
void opt2_full_branch_scan_b1(void);
#pragma section default

void opt2_full_link_resolve(void) {
    uint8_t saved_bank = mapper_prg_bank;
    // Reset diagnostic counters for this pass
    opt2_diag_xbank = 0;
    opt2_diag_align = 0;
    opt2_diag_selfloop = 0;
    opt2_diag_nocompile = 0;
    // Phase 1: drain ALL pending branch patches (queue-based)
    bankswitch_prg(2);
    opt2_sweep_pending_patches_b2();
    // Phase 2: exhaustive epilogue scan (all sectors, all blocks)
    // Lives in BANK_IR_OPT (bank28 NES / bank29 Exidy) — compile-time only.
    bankswitch_prg(BANK_IR_OPT);
    opt2_full_epilogue_scan_b1();
    // Phase 3: exhaustive inline branch scan (JMP $FFFF in code bodies)
    opt2_full_branch_scan_b1();
    // Phase 4: sweep again in case branch scan freed queue slots
    bankswitch_prg(2);
    opt2_sweep_pending_patches_b2();
    bankswitch_prg(saved_bank);
}
#endif  // ENABLE_PATCHABLE_EPILOGUE

//============================================================================
// Settling detector: tracks unique_blocks across frames.
// When no new blocks are compiled for SETTLE_FRAMES consecutive frames,
// fires one exhaustive opt2_full_link_resolve() pass.
//============================================================================

#define SETTLE_FRAMES 8     // frames with no new compiles before full sweep
#define RESETTLE_COOLDOWN 120 // ~2 sec cooldown after full resolve before re-settle

static uint16_t opt2_prev_unique_blocks = 0;
static uint8_t opt2_settle_counter = 0;
static uint8_t opt2_settled = 0;  // 1 = full resolve already fired

void opt2_frame_tick(void) {
#ifdef ENABLE_PATCHABLE_EPILOGUE
    
    if (opt2_settled) {
        // Cooldown after full resolve prevents thrashing when the cache
        // is full and blocks are constantly evicted/recompiled.
        if (opt2_settle_counter < 255) opt2_settle_counter++;
        
        // Light periodic sweep every 8 frames
        if ((opt2_settle_counter & 0x07) == 0) {
            opt2_sweep_pending_patches();
            opt2_scan_and_patch_epilogues();
        }
        
        // During cooldown (~2 sec), track blocks but don't un-settle
        if (opt2_settle_counter < RESETTLE_COOLDOWN) {
            opt2_prev_unique_blocks = opt2_blocks_compiled;
            return;
        }
        
        // Past cooldown: un-settle if new blocks appeared
        if (opt2_blocks_compiled != opt2_prev_unique_blocks) {
            opt2_prev_unique_blocks = opt2_blocks_compiled;
            opt2_settled = 0;
            opt2_settle_counter = 0;
        }
        return;
    }
    
    if (opt2_blocks_compiled == opt2_prev_unique_blocks) {
        // No new blocks this frame
        if (opt2_settle_counter < SETTLE_FRAMES) {
            opt2_settle_counter++;
        }
        if (opt2_settle_counter >= SETTLE_FRAMES && opt2_blocks_compiled > 0) {
            // Settled! Fire exhaustive link resolve.
            opt2_full_link_resolve();
            opt2_settled = 1;
            opt2_settle_counter = 0;
        }
    } else {
        // New blocks compiled — reset settling counter
        opt2_prev_unique_blocks = opt2_blocks_compiled;
        opt2_settle_counter = 0;
    }
#endif
}

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

#ifdef ENABLE_PATCHABLE_EPILOGUE
// Full scan of ALL patchable epilogues across all sectors and blocks.
// Lives in BANK_IR_OPT (bank28 NES / bank29 Exidy) — uses only
// peek_bank_byte (WRAM) and flash_byte_program (WRAM), neither of
// which calls bankswitch_prg, so this is safe from any switchable bank.
#ifdef PLATFORM_NES
#pragma section bank28
#else
#pragma section bank29
#endif
#define EPILOGUE_FULL_SCAN_MAX 255

void opt2_full_epilogue_scan_b1(void) {
    uint8_t patches_applied = 0;
    
    for (uint8_t sector = 0; sector < FLASH_CACHE_SECTORS; sector++) {
        uint16_t free_off = sector_free_offset[sector];
        if (free_off == 0) continue;  // empty sector
        
        uint8_t code_bank = (sector >> 2) + BANK_CODE;
        uint16_t sector_base = FLASH_BANK_BASE + ((uint16_t)(sector & 3) << 12);
        uint16_t offset = SECTOR_FIRST_HEADER;
        
        while (offset < free_off) {
            uint16_t hdr_addr = sector_base + offset;
            
            uint8_t code_len = peek_bank_byte(code_bank, hdr_addr + 4);
            uint8_t epi_off = peek_bank_byte(code_bank, hdr_addr + 5);
            
            if (code_len == 0 || code_len == 0xFF) break;  // end of valid blocks
            
            // Advance cursor past this block
            uint16_t cur_end = offset + BLOCK_HEADER_SIZE + code_len;
            uint16_t next_code = (cur_end + BLOCK_HEADER_SIZE + BLOCK_ALIGNMENT - 1) & ~(uint16_t)(BLOCK_ALIGNMENT - 1);
            offset = next_code - BLOCK_HEADER_SIZE;
            
            if (epi_off == 0xFF || epi_off == 0) continue;
            
            uint16_t code_base = hdr_addr + BLOCK_HEADER_SIZE;
            
            // Check if fast-path JMP operand is still unpatched ($FFFF)
            uint8_t jmp_lo = peek_bank_byte(code_bank, code_base + epi_off + 6);
            uint8_t jmp_hi = peek_bank_byte(code_bank, code_base + epi_off + 7);
            if (jmp_lo != 0xFF || jmp_hi != 0xFF) continue;  // already patched
            
            // Read exit_pc from header
            uint16_t exit_pc = peek_bank_byte(code_bank, hdr_addr + 2)
                    | ((uint16_t)peek_bank_byte(code_bank, hdr_addr + 3) << 8);
            
            // Check if exit_pc is compiled
            uint8_t flag = peek_bank_byte(
                (exit_pc >> 14) + BANK_PC_FLAGS,
                (uint16_t)&flash_cache_pc_flags[exit_pc & FLASH_BANK_MASK]);
            if (flag & RECOMPILED) continue;
            if (flag == 0) continue;  // uninitialized (dispatch BEQ)
            
            // Read native address
            uint16_t pc_addr = (exit_pc << 1) & FLASH_BANK_MASK;
            uint8_t pc_bank = (exit_pc >> 13) + BANK_PC;
            uint16_t na = peek_bank_byte(pc_bank, (uint16_t)&flash_cache_pc[pc_addr])
                        | (peek_bank_byte(pc_bank, (uint16_t)&flash_cache_pc[pc_addr+1]) << 8);
            
            // Guard: na must be in flash code range ($8001-$BFFF)
            if (na < 0x8001 || na >= 0xC000) continue;
            
            // Don't patch self-loops
            if (na >= code_base && na < code_base + code_len) continue;
            
            uint8_t target_bank = flag & 0x1F;
            
            if (target_bank == code_bank) {
                // Same-bank: patch fast-path JMP directly
                flash_byte_program(code_base + epi_off + 3, code_bank, 0);
                flash_byte_program(code_base + epi_off + 6, code_bank, na & 0xFF);
                flash_byte_program(code_base + epi_off + 7, code_bank, (na >> 8) & 0xFF);
            } else {
                // Cross-bank: patch to xbank setup code
                uint16_t setup_addr = code_base + epi_off + 21;
                flash_byte_program(code_base + epi_off + 3, code_bank, 0);
                flash_byte_program(code_base + epi_off + 6, code_bank, setup_addr & 0xFF);
                flash_byte_program(code_base + epi_off + 7, code_bank, (setup_addr >> 8) & 0xFF);
                flash_byte_program(code_base + epi_off + 25, code_bank, na & 0xFF);
                flash_byte_program(code_base + epi_off + 30, code_bank, (na >> 8) & 0xFF);
                flash_byte_program(code_base + epi_off + 35, code_bank, target_bank);
            }
            patches_applied++;
            if (patches_applied >= EPILOGUE_FULL_SCAN_MAX) return;
        }
    }
    opt2_stat_direct += patches_applied;
}

// Full scan of ALL 21-byte branch patterns across all sectors.
// Looks for JMP $FFFF ($4C $FF $FF) inside blocks and resolves them
// against the current PC table — catches branches that were never
// queued (IR path, queue overflow) or whose queue entry was evicted.
//
// Lives in BANK_IR_OPT alongside epilogue scan.  Uses only
// peek_bank_byte (WRAM) and flash_byte_program (WRAM), neither of
// which calls bankswitch_prg, so this is safe from any switchable bank.
void opt2_full_branch_scan_b1(void) {
    uint8_t patches_applied = 0;
    
    for (uint8_t sector = 0; sector < FLASH_CACHE_SECTORS; sector++) {
        uint16_t free_off = sector_free_offset[sector];
        if (free_off == 0) continue;
        
        uint8_t code_bank = (sector >> 2) + BANK_CODE;
        uint16_t sector_base = FLASH_BANK_BASE + ((uint16_t)(sector & 3) << 12);
        uint16_t offset = SECTOR_FIRST_HEADER;
        
        while (offset < free_off) {
            uint16_t hdr_addr = sector_base + offset;
            
            uint8_t code_len = peek_bank_byte(code_bank, hdr_addr + 4);
            if (code_len == 0 || code_len == 0xFF) break;
            
            uint16_t cur_end = offset + BLOCK_HEADER_SIZE + code_len;
            uint16_t next_code = (cur_end + BLOCK_HEADER_SIZE + BLOCK_ALIGNMENT - 1) & ~(uint16_t)(BLOCK_ALIGNMENT - 1);
            offset = next_code - BLOCK_HEADER_SIZE;
            
            uint16_t code_base = hdr_addr + BLOCK_HEADER_SIZE;
            
            // Per-block trailing trampoline for $FFF0 alignment workaround.
            // The first 16-aligned address at or after code_len sits in the
            // padding between this block and the next header ($FF flash).
            // We can write a 3-byte JMP there to bounce misaligned targets.
            // Even when code_len is already 16-aligned (tramp_off == code_len),
            // there are 8 bytes of header padding available (next code must
            // be 16-aligned, so next header is 8 bytes before that).
#ifdef ENABLE_FFF0_TEMPLATES
            uint16_t tramp_off = (uint16_t)((code_len + 15) & ~15);
            uint16_t tramp_addr = code_base + tramp_off;
            // Next header is at sector_base + offset (already advanced).
            // Trampoline needs 3 bytes: verify it fits before next header.
            uint8_t tramp_avail = (tramp_addr + 3 <= sector_base + offset);
            uint16_t tramp_na = 0;  // 0 = slot unused; nonzero = target written
#endif

            // Scan code bytes for patchable JMP patterns.
            // Old templates: $4C $FF $FF (JMP $FFFF)
            // $FFF0 templates: $4C $F0 $FF (JMP $FFF0)
            //
            // Old template types:
            //  (A) 21-byte branch: [invBxx $13] [Bxx $03] [JMP $FFFF] [STA _a] [PHP]
            //  (B) 9-byte opJMP:   [PHP] [CLC] [BCC $04] [PLP] [JMP $FFFF] [PLP]
            //
            // $FFF0 template types:
            //  (C) 19-byte branch: [STA _a] [PHP] [LDA# lo] [STA _pc] [LDA# hi] [STA _pc+1] [LDA _a] [PLP] [Bxx_inv +3] [JMP $FFF0]
            //  (D) 17-byte JMP:    [STA _a] [PHP] [LDA# lo] [STA _pc] [LDA# hi] [STA _pc+1] [LDA _a] [PLP] [JMP $FFF0]
            //
            for (uint8_t p = 0; (uint8_t)(p + 6) < code_len; p++) {
                uint8_t b0 = peek_bank_byte(code_bank, code_base + p);
                if (b0 != 0x4C) continue;
                uint8_t b2 = peek_bank_byte(code_bank, code_base + p + 2);
                if (b2 != 0xFF) continue;
                uint8_t b1 = peek_bank_byte(code_bank, code_base + p + 1);

#ifdef ENABLE_FFF0_TEMPLATES
                // --- Pattern C/D: $FFF0 templates (JMP $FFF0) ---
                if (b1 == (FFF0_DISPATCH & 0xFF)) {
                    // Determine branch vs JMP by checking for Bxx_inv at p-2
                    uint16_t target_pc;
                    uint8_t is_branch_tmpl = 0;
                    if (p >= 2) {
                        uint8_t maybe_bxx = peek_bank_byte(code_bank, code_base + p - 2);
                        if ((maybe_bxx & 0x1F) == 0x10) is_branch_tmpl = 1;
                    }
                    if (is_branch_tmpl) {
                        // 19-byte branch: target lo at p-12, hi at p-8
                        uint8_t tpc_lo = peek_bank_byte(code_bank, code_base + p - 12);
                        uint8_t tpc_hi = peek_bank_byte(code_bank, code_base + p - 8);
                        target_pc = tpc_lo | ((uint16_t)tpc_hi << 8);
                    } else {
                        // 17-byte JMP: target lo at p-10, hi at p-6
                        uint8_t tpc_lo = peek_bank_byte(code_bank, code_base + p - 10);
                        uint8_t tpc_hi = peek_bank_byte(code_bank, code_base + p - 6);
                        target_pc = tpc_lo | ((uint16_t)tpc_hi << 8);
                    }

                    uint8_t flag = peek_bank_byte(
                        (target_pc >> 14) + BANK_PC_FLAGS,
                        (uint16_t)&flash_cache_pc_flags[target_pc & FLASH_BANK_MASK]);

                    if ((flag & RECOMPILED) || flag == 0) {
                        opt2_diag_nocompile++;
                        p += 2;
                        continue;
                    }
                    if ((flag & 0x1F) != code_bank) {
                        opt2_diag_xbank++;
                        p += 2;
                        continue;
                    }
                    {
                        uint16_t pa = (target_pc << 1) & FLASH_BANK_MASK;
                        uint8_t pc_bank2 = (target_pc >> 13) + BANK_PC;
                        uint16_t na = peek_bank_byte(pc_bank2, (uint16_t)&flash_cache_pc[pa])
                                    | (peek_bank_byte(pc_bank2, (uint16_t)&flash_cache_pc[pa+1]) << 8);

                        // Self-loop guard for unconditional JMP (not branch)
                        if (!is_branch_tmpl && na >= code_base && na < code_base + code_len) {
                            opt2_diag_selfloop++;
                            p += 2;
                            continue;
                        }

                        // Alignment guard: JMP $FFF0 lo byte is $F0, can only
                        // clear bits → target lo nibble must be 0.
                        if ((na & 0x0F) != 0) {
                            // Target not aligned — try block-trailing trampoline.
                            // Write JMP na at tramp_addr (16-aligned padding slot),
                            // then patch JMP $FFF0 → JMP tramp_addr instead.
                            if (tramp_avail && tramp_na == 0) {
                                // Verify slot is $FF (unwritten flash)
                                if (peek_bank_byte(code_bank, tramp_addr) == 0xFF &&
                                    peek_bank_byte(code_bank, tramp_addr + 1) == 0xFF &&
                                    peek_bank_byte(code_bank, tramp_addr + 2) == 0xFF) {
                                    flash_byte_program(tramp_addr, code_bank, 0x4C);
                                    flash_byte_program(tramp_addr + 1, code_bank, na & 0xFF);
                                    flash_byte_program(tramp_addr + 2, code_bank, (na >> 8) & 0xFF);
                                    tramp_na = na;
                                }
                            }
                            if (tramp_na == na) {
                                // Redirect JMP $FFF0 → JMP tramp_addr
                                flash_byte_program(code_base + p + 1, code_bank, tramp_addr & 0xFF);
                                flash_byte_program(code_base + p + 2, code_bank, (tramp_addr >> 8) & 0xFF);
                                patches_applied++;
                                p += 2;
                                if (patches_applied >= EPILOGUE_FULL_SCAN_MAX) goto done;
                                continue;
                            }
                            // Fallback: per-sector trampoline pool (16 slots)
                            {
                                uint16_t pool = sector_base + SECTOR_TRAMP_POOL_BASE;
                                uint16_t found_addr = 0;
                                for (uint8_t sl = 0; sl < SECTOR_TRAMP_SLOTS; sl++) {
                                    uint16_t sa = pool + ((uint16_t)sl << 4);
                                    uint8_t b = peek_bank_byte(code_bank, sa);
                                    if (b == 0xFF) {
                                        // Free slot — write JMP na
                                        flash_byte_program(sa, code_bank, 0x4C);
                                        flash_byte_program(sa + 1, code_bank, na & 0xFF);
                                        flash_byte_program(sa + 2, code_bank, (na >> 8) & 0xFF);
                                        found_addr = sa;
                                        break;
                                    }
                                    if (b == 0x4C) {
                                        // Used slot — check if target matches
                                        uint8_t slo = peek_bank_byte(code_bank, sa + 1);
                                        uint8_t shi = peek_bank_byte(code_bank, sa + 2);
                                        if (slo == (na & 0xFF) && shi == ((na >> 8) & 0xFF)) {
                                            found_addr = sa;
                                            break;
                                        }
                                    }
                                }
                                if (found_addr) {
                                    flash_byte_program(code_base + p + 1, code_bank, found_addr & 0xFF);
                                    flash_byte_program(code_base + p + 2, code_bank, (found_addr >> 8) & 0xFF);
                                    patches_applied++;
                                    p += 2;
                                    if (patches_applied >= EPILOGUE_FULL_SCAN_MAX) goto done;
                                    continue;
                                }
                            }
                            opt2_diag_align++;
                            p += 2;
                            continue;
                        }

                        // Patch: only 2 writes (JMP lo + JMP hi)
                        flash_byte_program(code_base + p + 1, code_bank, na & 0xFF);
                        flash_byte_program(code_base + p + 2, code_bank, (na >> 8) & 0xFF);
                        patches_applied++;
                    }
                    p += 2;
                    if (patches_applied >= EPILOGUE_FULL_SCAN_MAX) goto done;
                    continue;
                }
#endif  /* ENABLE_FFF0_TEMPLATES */

                if (b1 != 0xFF) continue;  // not $4C $FF $FF either
                
                // --- Pattern A: 21-byte branch template ---
                if (p >= 4 && (uint8_t)(p + 12) < code_len) {
                    uint8_t sta = peek_bank_byte(code_bank, code_base + p + 3);
                    uint8_t php = peek_bank_byte(code_bank, code_base + p + 5);
                    if (sta == 0x85 && php == 0x08) {
                        uint8_t tpc_lo = peek_bank_byte(code_bank, code_base + p + 7);
                        uint8_t tpc_hi = peek_bank_byte(code_bank, code_base + p + 11);
                        uint16_t target_pc = tpc_lo | ((uint16_t)tpc_hi << 8);
                        
                        uint8_t flag = peek_bank_byte(
                            (target_pc >> 14) + BANK_PC_FLAGS,
                            (uint16_t)&flash_cache_pc_flags[target_pc & FLASH_BANK_MASK]);
                        
                        if ((flag & RECOMPILED) || flag == 0) {
                            opt2_diag_nocompile++;
                        } else if ((flag & 0x1F) != code_bank) {
                            opt2_diag_xbank++;
                        } else {
                            uint16_t pa = (target_pc << 1) & FLASH_BANK_MASK;
                            uint8_t pc_bank2 = (target_pc >> 13) + BANK_PC;
                            uint16_t na = peek_bank_byte(pc_bank2, (uint16_t)&flash_cache_pc[pa])
                                        | (peek_bank_byte(pc_bank2, (uint16_t)&flash_cache_pc[pa+1]) << 8);
                            
                            if (na >= 0x8001 && na < 0xC000) {
                                flash_byte_program(code_base + p - 1, code_bank, 0);
                                flash_byte_program(code_base + p + 1, code_bank, na & 0xFF);
                                flash_byte_program(code_base + p + 2, code_bank, (na >> 8) & 0xFF);
                                patches_applied++;
                            }
                        }
                        p += 20;
                        if (patches_applied >= EPILOGUE_FULL_SCAN_MAX) goto done;
                        continue;
                    }
                }
                
                // --- Pattern B: 9-byte opJMP template ---
                if (p >= 5) {
                    uint8_t plp = peek_bank_byte(code_bank, code_base + p - 1);
                    uint8_t bcc = peek_bank_byte(code_bank, code_base + p - 3);
                    if (plp == 0x28 && bcc == 0x90) {
                        uint16_t exit_pc = peek_bank_byte(code_bank, hdr_addr + 2)
                                | ((uint16_t)peek_bank_byte(code_bank, hdr_addr + 3) << 8);
                        
                        uint8_t flag = peek_bank_byte(
                            (exit_pc >> 14) + BANK_PC_FLAGS,
                            (uint16_t)&flash_cache_pc_flags[exit_pc & FLASH_BANK_MASK]);
                        
                        if ((flag & RECOMPILED) || flag == 0) {
                            opt2_diag_nocompile++;
                        } else if ((flag & 0x1F) != code_bank) {
                            opt2_diag_xbank++;
                        } else {
                            uint16_t pa = (exit_pc << 1) & FLASH_BANK_MASK;
                            uint8_t pc_bank2 = (exit_pc >> 13) + BANK_PC;
                            uint16_t na = peek_bank_byte(pc_bank2, (uint16_t)&flash_cache_pc[pa])
                                        | (peek_bank_byte(pc_bank2, (uint16_t)&flash_cache_pc[pa+1]) << 8);
                            
                            if (na >= 0x8001 && na < 0xC000 &&
                                !(na >= code_base && na < code_base + code_len)) {
                                flash_byte_program(code_base + p - 2, code_bank, 0);
                                flash_byte_program(code_base + p + 1, code_bank, na & 0xFF);
                                flash_byte_program(code_base + p + 2, code_bank, (na >> 8) & 0xFF);
                                patches_applied++;
                            } else if (na >= code_base && na < code_base + code_len) {
                                opt2_diag_selfloop++;
                            }
                        }
                        p += 2;
                        if (patches_applied >= EPILOGUE_FULL_SCAN_MAX) goto done;
                        continue;
                    }
                }
            }
        }
    }
done:
    opt2_stat_direct += patches_applied;
}
#endif  /* ENABLE_PATCHABLE_EPILOGUE */

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
    opt2_blocks_compiled = 0;
    opt2_diag_xbank = 0;
    opt2_diag_align = 0;
    opt2_diag_selfloop = 0;
    opt2_diag_nocompile = 0;
    opt2_epilogue_sector = 0;
    opt2_epilogue_offset = SECTOR_FIRST_HEADER;
    opt2_prev_unique_blocks = 0;
    opt2_settle_counter = 0;
    opt2_settled = 0;
}

//============================================================================
// Static-compilation drain: resolve as many patches as possible before runtime
//============================================================================

#pragma section default

void opt2_drain_static_patches(void) {
    // Iterate until the pending queue is drained (or we hit a safety cap).
    for (uint8_t pass = 0; pass < 16 && pending_count > 0; pass++) {
        opt2_sweep_pending_patches();
#ifdef ENABLE_PATCHABLE_EPILOGUE
        // One full scan of all sectors per pass.
        for (uint8_t i = 0; i < (FLASH_CACHE_SECTORS + EPILOGUE_SCAN_BATCH - 1) / EPILOGUE_SCAN_BATCH; i++) {
            opt2_scan_and_patch_epilogues();
        }
#endif
        opt2_sweep_pending_patches();
    }
}
