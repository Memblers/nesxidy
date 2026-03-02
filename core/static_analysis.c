/**
 * static_analysis.c - One-time power-on ROM analysis pass
 *
 * BFS walk of guest ROM from reset/NMI/IRQ vectors (+ persisted indirect
 * targets from previous runs).  Discovers all reachable code, records it in
 * a persistent bitmap in flash bank 3, and optionally batch-compiles
 * discovered entry points before starting execution.
 *
 * Banking:
 *   bank2  — sa_run_b2()       BFS walker + batch compiler
 *   default — sa_run()         trampoline
 *            sa_record_indirect_target()  runtime feedback (fixed bank)
 */

#include <stdint.h>
#include "../config.h"

#ifdef ENABLE_STATIC_ANALYSIS

#include "../dynamos.h"
#include "../exidy.h"
#include "../mapper30.h"
#include "static_analysis.h"
#include "metrics.h"

// -------------------------------------------------------------------------
// External references
// -------------------------------------------------------------------------

extern void bankswitch_prg(__reg("a") uint8_t bank);
extern void flash_byte_program(uint16_t addr, uint8_t bank, uint8_t data);
extern void flash_sector_erase(uint16_t addr, uint8_t bank);
extern uint8_t mapper_prg_bank;
extern uint8_t peek_bank_byte(uint8_t bank, uint16_t addr);
extern uint8_t read6502(uint16_t address);
extern void run_6502(void);

extern uint8_t addrmodes[];

#ifdef ENABLE_COMPILE_PPU_EFFECT
extern uint8_t compile_ppu_effect;
extern uint8_t compile_ppu_active;
extern uint8_t lnPPUMASK;  // lazynes shadow for $2001
#endif

// From dynamos.c — needed for batch compile
__zpage extern uint16_t pc;
__zpage extern uint8_t code_index;
__zpage extern uint8_t cache_index;
extern uint8_t cache_flag[];
extern uint8_t cache_code[BLOCK_COUNT][CACHE_CODE_BUF_SIZE];
extern uint8_t flash_sector_alloc(uint8_t total_size);
extern void setup_flash_address(uint16_t emulated_pc, uint16_t block_number);
extern void setup_flash_pc_tables(uint16_t emulated_pc);
extern void flash_cache_pc_update(uint8_t code_address, uint8_t flags);
extern uint8_t recompile_opcode(void);
__zpage extern uint16_t flash_cache_index;
extern uint16_t flash_code_address;
extern uint8_t flash_code_bank;
extern uint16_t sector_free_offset[];
extern uint8_t next_free_sector;

extern uint8_t flash_block_flags[];
// flash_cache_pc / flash_cache_pc_flags are macros in core/cache.h
__zpage extern uint8_t a;
extern void flash_dispatch_return(void);
extern void cross_bank_dispatch(void);
extern void xbank_trampoline(void);
extern uint8_t xbank_addr;

// Intra-block backward branch support (from dynamos.c)
extern uint8_t block_ci_map[];
__zpage extern uint8_t block_has_jsr;
__zpage extern uint8_t cache_entry_pc_lo[];
__zpage extern uint8_t cache_entry_pc_hi[];

// Peephole state (from dynamos.c)
#ifdef ENABLE_PEEPHOLE
extern volatile uint8_t block_flags_saved;
extern uint8_t peephole_skipped;
#endif
extern uint8_t recompile_instr_start;

#ifdef ENABLE_OPTIMIZER_V2
#include "optimizer_v2_simple.h"
#endif

// -------------------------------------------------------------------------
// Bank 3 storage - placed by the linker alongside flash_block_flags
// and cache_bit_array.  These arrays live in flash and are read/written
// via peek_bank_byte / flash_byte_program.
// -------------------------------------------------------------------------

#pragma section bank3

// 4KB bitmap: 1 bit per address, covers 32K addresses.
// Bit CLEAR = known code.  Erased ($FF) = unknown.
uint8_t sa_code_bitmap[SA_BITMAP_SIZE];

// 8-byte header: magic(4) + rom_hash(4)
uint8_t sa_header[SA_HEADER_SIZE];

// Indirect-target list: 3 bytes each (lo, hi, type)
uint8_t sa_indirect_list[SA_INDIRECT_MAX * 3];

// Subroutine table: 3 bytes each (addr_lo, addr_hi, flags)
// Populated during BFS walk, flags set during stack-safety analysis.
uint8_t sa_subroutine_list[SA_SUBROUTINE_MAX * 3];

#pragma section default

// Shorthand macros for the flash addresses of the bank3 variables.
// These resolve to the linker-assigned addresses at compile time.
#define SA_BITMAP_BASE      ((uint16_t)&sa_code_bitmap[0])
#define SA_HEADER_BASE      ((uint16_t)&sa_header[0])
#define SA_INDIRECT_BASE    ((uint16_t)&sa_indirect_list[0])
#define SA_SUBROUTINE_BASE  ((uint16_t)&sa_subroutine_list[0])

// -------------------------------------------------------------------------
// Bitmap helpers (fixed bank) — called from sa_record_indirect_target
// and from bank2 code (which can reach fixed bank at $C000+).
// Bit CLEAR = known code.  Flash can only clear bits, so marking is free.
// -------------------------------------------------------------------------

static uint8_t sa_bitmap_is_unknown(uint16_t addr)
{
    uint16_t byte_offset = addr >> 3;
    uint8_t bit_mask = 1 << (addr & 7);
    uint8_t val = peek_bank_byte(BANK_FLASH_BLOCK_FLAGS,
                                 SA_BITMAP_BASE + byte_offset);
    return val & bit_mask;
}

static void sa_bitmap_mark(uint16_t addr)
{
    uint16_t byte_offset = addr >> 3;
    uint8_t bit_mask = 1 << (addr & 7);
    uint8_t cur = peek_bank_byte(BANK_FLASH_BLOCK_FLAGS,
                                 SA_BITMAP_BASE + byte_offset);
    if (cur & bit_mask)
    {
        flash_byte_program(SA_BITMAP_BASE + byte_offset,
                           BANK_FLASH_BLOCK_FLAGS,
                           cur & ~bit_mask);
    }
}

// -------------------------------------------------------------------------
// Indirect-target list helpers (fixed bank) — called from sa_record_indirect_target
// -------------------------------------------------------------------------

static uint16_t sa_indirect_find_empty(void)
{
    for (uint16_t i = 0; i < SA_INDIRECT_MAX; i++)
    {
        uint16_t entry_addr = SA_INDIRECT_BASE + i * 3;
        uint8_t type = peek_bank_byte(BANK_FLASH_BLOCK_FLAGS, entry_addr + 2);
        if (type == SA_TYPE_EMPTY)
            return i;
    }
    return SA_INDIRECT_MAX;
}

static uint8_t sa_indirect_exists(uint16_t target_pc)
{
    for (uint16_t i = 0; i < SA_INDIRECT_MAX; i++)
    {
        uint16_t entry_addr = SA_INDIRECT_BASE + i * 3;
        uint8_t type = peek_bank_byte(BANK_FLASH_BLOCK_FLAGS, entry_addr + 2);
        if (type == SA_TYPE_EMPTY)
            return 0;
        uint8_t lo = peek_bank_byte(BANK_FLASH_BLOCK_FLAGS, entry_addr + 0);
        uint8_t hi = peek_bank_byte(BANK_FLASH_BLOCK_FLAGS, entry_addr + 1);
        if ((lo | (hi << 8)) == target_pc)
            return 1;
    }
    return 0;
}

// -------------------------------------------------------------------------
// BFS queue state (fixed-bank / BSS)
// CRITICAL: These MUST NOT be in bank2 — static variables in a bank2
// pragma section land in flash and are read-only at runtime.
// Placed here in the default section so vbcc puts them in BSS (WRAM).
// -------------------------------------------------------------------------
static uint8_t q_head;
static uint8_t q_tail;
static uint8_t q_count;

// =========================================================================
// Bank 2 section — BFS walker, header helpers, queue, opcode helpers
// =========================================================================

#ifdef PLATFORM_NES
#pragma section bank19
#else
#pragma section bank24
#endif

// Forward declaration: sa_record_subroutine is in the bank24 section below.
// Called from sa_walk_b2 (same bank, direct call).
// Must be declared AFTER the #pragma so vbcc binds it to the correct section.
void sa_record_subroutine(uint16_t target);

// -------------------------------------------------------------------------
// Opcode length helper (bank24) — only called from sa_walk_b2.
// -------------------------------------------------------------------------

static uint8_t opcode_length(uint8_t opcode)
{
    uint8_t mode = addrmodes[opcode];
    switch (mode)
    {
        case imp:
        case acc:
            return 1;
        case imm:
        case zp:
        case zpx:
        case zpy:
        case rel:
        case indx:
        case indy:
            return 2;
        case abso:
        case absx:
        case absy:
        case ind:
            return 3;
        default:
            return 1;
    }
}

// -------------------------------------------------------------------------
// SA header helpers (bank2)
// -------------------------------------------------------------------------

static uint8_t sa_check_header(void)
{
    uint8_t rom_hash[4];
    rom_hash[0] = read6502(0xFFFC);
    rom_hash[1] = read6502(0xFFFD);
    rom_hash[2] = read6502(0xFFFE);
    rom_hash[3] = read6502(0xFFFF);

    if (peek_bank_byte(BANK_FLASH_BLOCK_FLAGS, SA_HEADER_BASE + 0) != SA_SIG_MAGIC_0) return 0;
    if (peek_bank_byte(BANK_FLASH_BLOCK_FLAGS, SA_HEADER_BASE + 1) != SA_SIG_MAGIC_1) return 0;
    if (peek_bank_byte(BANK_FLASH_BLOCK_FLAGS, SA_HEADER_BASE + 2) != SA_SIG_MAGIC_2) return 0;
    if (peek_bank_byte(BANK_FLASH_BLOCK_FLAGS, SA_HEADER_BASE + 3) != SA_SIG_MAGIC_3) return 0;
    if (peek_bank_byte(BANK_FLASH_BLOCK_FLAGS, SA_HEADER_BASE + 4) != rom_hash[0]) return 0;
    if (peek_bank_byte(BANK_FLASH_BLOCK_FLAGS, SA_HEADER_BASE + 5) != rom_hash[1]) return 0;
    if (peek_bank_byte(BANK_FLASH_BLOCK_FLAGS, SA_HEADER_BASE + 6) != rom_hash[2]) return 0;
    if (peek_bank_byte(BANK_FLASH_BLOCK_FLAGS, SA_HEADER_BASE + 7) != rom_hash[3]) return 0;
    return 1;
}

static void sa_write_header(void)
{
    uint8_t rom_hash[4];
    rom_hash[0] = read6502(0xFFFC);
    rom_hash[1] = read6502(0xFFFD);
    rom_hash[2] = read6502(0xFFFE);
    rom_hash[3] = read6502(0xFFFF);

    flash_byte_program(SA_HEADER_BASE + 0, BANK_FLASH_BLOCK_FLAGS, SA_SIG_MAGIC_0);
    flash_byte_program(SA_HEADER_BASE + 1, BANK_FLASH_BLOCK_FLAGS, SA_SIG_MAGIC_1);
    flash_byte_program(SA_HEADER_BASE + 2, BANK_FLASH_BLOCK_FLAGS, SA_SIG_MAGIC_2);
    flash_byte_program(SA_HEADER_BASE + 3, BANK_FLASH_BLOCK_FLAGS, SA_SIG_MAGIC_3);
    flash_byte_program(SA_HEADER_BASE + 4, BANK_FLASH_BLOCK_FLAGS, rom_hash[0]);
    flash_byte_program(SA_HEADER_BASE + 5, BANK_FLASH_BLOCK_FLAGS, rom_hash[1]);
    flash_byte_program(SA_HEADER_BASE + 6, BANK_FLASH_BLOCK_FLAGS, rom_hash[2]);
    flash_byte_program(SA_HEADER_BASE + 7, BANK_FLASH_BLOCK_FLAGS, rom_hash[3]);
}

// -------------------------------------------------------------------------
// BFS queue (bank2) — uses cache_code[0] as backing store.
// Circular queue with head/tail indices.
// BFS queue uses cache_code[0] as backing store (WRAM).
// q_head, q_tail, q_count are in BSS (see above, before bank2 pragma).
// -------------------------------------------------------------------------

#define Q_BUF   ((uint8_t*)cache_code[0])

static void q_init(void)
{
    q_head = 0;
    q_tail = 0;
    q_count = 0;
}

static uint8_t q_full(void)
{
    return q_count >= SA_QUEUE_SLOTS;
}

static uint8_t q_empty(void)
{
    return q_count == 0;
}

static void q_push(uint16_t addr)
{
    if (q_full()) return;
    uint8_t idx = q_tail * 2;
    Q_BUF[idx] = (uint8_t)addr;
    Q_BUF[idx + 1] = (uint8_t)(addr >> 8);
    q_tail = (q_tail + 1) & (SA_QUEUE_SLOTS - 1);
    q_count++;
}

static uint16_t q_pop(void)
{
    uint8_t idx = q_head * 2;
    uint16_t addr = Q_BUF[idx] | ((uint16_t)Q_BUF[idx + 1] << 8);
    q_head = (q_head + 1) & (SA_QUEUE_SLOTS - 1);
    q_count--;
    return addr;
}

// -------------------------------------------------------------------------
// Enqueue helper (bank2): only enqueue if in ROM range and not yet visited.
// Calls sa_bitmap_is_unknown/sa_bitmap_mark in fixed bank — OK because
// bank2 ($8000) can always reach fixed bank ($C000+).
// -------------------------------------------------------------------------

static void sa_enqueue_if_valid(uint16_t addr)
{
    if (addr < ROM_ADDR_MIN || addr > ROM_ADDR_MAX)
        return;
    if (sa_bitmap_is_unknown(addr))
    {
        sa_bitmap_mark(addr);
        q_push(addr);
    }
}

// -------------------------------------------------------------------------
// Forward declarations for bank2 static helpers (defined later in file)
// -------------------------------------------------------------------------
static uint8_t is_invalid_opcode(uint8_t op);

// -------------------------------------------------------------------------
// Invalid opcode check (bank2)
// -------------------------------------------------------------------------

static uint8_t is_invalid_opcode(uint8_t op)
{
    uint8_t lo = op & 0x0F;
    uint8_t hi = op >> 4;

    if (lo == 0x02 && hi != 0x0A)
        return 1;
    if (lo == 0x03 || lo == 0x07 || lo == 0x0F)
        return 1;
    if (lo == 0x0B && hi != 0x0A && hi != 0x0C && hi != 0x0E)
        return 1;

    return 0;
}

// -------------------------------------------------------------------------
// BFS walker + header init (bank 2 implementation)
// Checks header, erases SA sectors if ROM changed, then does BFS walk.
// -------------------------------------------------------------------------

static void sa_walk_b2(void)
{
    // Check if SA header matches current ROM.  If the ROM changed,
    // erase the SA region and start fresh.
    if (!sa_check_header())
    {
        // Erase the SA sectors in bank 3.
        // Range covers bitmap + header + indirect list + subroutine table.
        {
            uint16_t base = SA_BITMAP_BASE & 0xF000;
            uint16_t end  = (SA_SUBROUTINE_BASE + SA_SUBROUTINE_MAX * 3 - 1) | 0x0FFF;
            for (uint16_t sector = base; sector <= end; sector += 0x1000)
                flash_sector_erase(sector, BANK_FLASH_BLOCK_FLAGS);
        }
        sa_write_header();
    }

    q_init();

    metrics_bfs_start();

    // Seed from reset, NMI, IRQ vectors
    uint16_t reset_pc = read6502(0xFFFC) | ((uint16_t)read6502(0xFFFD) << 8);
    uint16_t nmi_pc   = read6502(0xFFFA) | ((uint16_t)read6502(0xFFFB) << 8);
    uint16_t irq_pc   = read6502(0xFFFE) | ((uint16_t)read6502(0xFFFF) << 8);

    sa_enqueue_if_valid(reset_pc);
    sa_enqueue_if_valid(nmi_pc);
    sa_enqueue_if_valid(irq_pc);

    // Seed from persisted indirect-target list (previous runs)
    for (uint16_t i = 0; i < SA_INDIRECT_MAX; i++)
    {
        uint16_t entry_addr = SA_INDIRECT_BASE + i * 3;
        uint8_t type = peek_bank_byte(BANK_FLASH_BLOCK_FLAGS, entry_addr + 2);
        if (type == SA_TYPE_EMPTY)
            break;
        uint16_t tgt = peek_bank_byte(BANK_FLASH_BLOCK_FLAGS, entry_addr + 0)
                     | ((uint16_t)peek_bank_byte(BANK_FLASH_BLOCK_FLAGS, entry_addr + 1) << 8);
        sa_enqueue_if_valid(tgt);
    }

    // Enable monochrome during BFS walk
#ifdef ENABLE_COMPILE_PPU_EFFECT
    lnPPUMASK = 0x3B | compile_ppu_effect;
    *(volatile uint8_t*)0x2001 = lnPPUMASK;  // mid-frame
#endif

    // BFS loop
    while (!q_empty())
    {
        uint16_t cur_pc = q_pop();
        metrics_bfs_visit_address();

#ifdef ENABLE_COMPILE_PPU_EFFECT
        // Toggle blue emphasis (bit 7) per BFS node
        compile_ppu_effect ^= 0x80;
        lnPPUMASK = 0x3B | compile_ppu_effect;
        *(volatile uint8_t*)0x2001 = lnPPUMASK;  // mid-frame
#endif

        // Walk linear code from cur_pc
        while (cur_pc >= ROM_ADDR_MIN && cur_pc <= ROM_ADDR_MAX)
        {
            uint8_t op = read6502(cur_pc);

            // Check for invalid/undocumented opcodes — likely data
            if (is_invalid_opcode(op))
                break;

            uint8_t len = opcode_length(op);

            // Operand bytes are NOT marked in the bitmap.
            // Only instruction head bytes are marked (by sa_enqueue_if_valid
            // or by the continuation logic after JSR/branch/default).
            // This ensures sa_compile_b2 only sees entry points.

            // Classify control flow
            switch (op)
            {
                // ----- Unconditional jumps -----
                case opJMP:     // $4C
                {
                    uint16_t target = read6502(cur_pc + 1)
                                    | ((uint16_t)read6502(cur_pc + 2) << 8);
                    sa_enqueue_if_valid(target);
                    metrics_bfs_entry_point();
                    goto next_path;     // unconditional — stop linear walk
                }

                case opJMPi:    // $6C — indirect jump, can't resolve statically
                {
                    // Log the instruction PC itself (the indirect operand address)
                    // but we can't follow it.  Runtime will feed back the target.
                    goto next_path;
                }

                // ----- Subroutine call -----
                case opJSR:     // $20
                {
                    uint16_t target = read6502(cur_pc + 1)
                                    | ((uint16_t)read6502(cur_pc + 2) << 8);
                    sa_enqueue_if_valid(target);
                    sa_record_subroutine(target);
                    metrics_bfs_entry_point();
                    // Continue linear walk after JSR (return address = cur_pc+3)
                    cur_pc += 3;
                    if (cur_pc <= ROM_ADDR_MAX)
                        sa_bitmap_mark(cur_pc);
                    continue;
                }

                // ----- Returns -----
                case opRTS:     // $60
                case opRTI:     // $40
                    goto next_path;

                // ----- Conditional branches -----
                case opBPL: case opBMI: case opBVC: case opBVS:
                case opBCC: case opBCS: case opBNE: case opBEQ:
                {
                    int8_t offset = (int8_t)read6502(cur_pc + 1);
                    uint16_t target = cur_pc + 2 + offset;
                    sa_enqueue_if_valid(target);
                    metrics_bfs_entry_point();
                    // Fall through to the not-taken path
                    cur_pc += 2;
                    if (cur_pc <= ROM_ADDR_MAX)
                        sa_bitmap_mark(cur_pc);
                    continue;
                }

                case opBRK:     // $00 — software interrupt, stop walk
                    goto next_path;

                // ----- Everything else: continue linear walk -----
                default:
                    cur_pc += len;
                    if (cur_pc <= ROM_ADDR_MAX)
                        sa_bitmap_mark(cur_pc);
                    continue;
            }
        }
next_path:
        ;   // pop next BFS entry
    }

    metrics_bfs_end();
}

// -------------------------------------------------------------------------
// Batch compile pass (bank 2 implementation)
// Iterates the SA bitmap in address order and compiles every discovered
// code address.  Uses the existing recompile_opcode() infrastructure.
// -------------------------------------------------------------------------

#ifdef ENABLE_STATIC_COMPILE

static void sa_compile_b2(void)
{
    // Scan the bitmap starting from pc (set by the caller in sa_run).
    // Find the next address that is known code but not yet compiled,
    // set pc to it, and return.  If nothing found, set pc = 0xFFFF.
    //
    // The scan cursor is managed by sa_run (fixed bank, guaranteed WRAM)
    // and passed in/out through pc.  This avoids any static-local-in-bank2
    // issue where variables land in flash and can't be written at runtime.

    uint16_t addr = pc;

    // Note: use != instead of <= to handle ROM_ADDR_MAX == 0xFFFF
    while (addr != (uint16_t)(ROM_ADDR_MAX + 1u))
    {
        // Check if this address is known code
        if (sa_bitmap_is_unknown(addr))
        {
            addr++;
            continue;
        }

        // Check if already processed (compiled or interpreted).
        uint8_t flag = peek_bank_byte(
            (addr >> 14) + BANK_PC_FLAGS,
            (uint16_t)&flash_cache_pc_flags[addr & FLASH_BANK_MASK]);

        if (flag != 0xFF)
        {
            addr++;
            continue;
        }

        // Found an uncompiled known-code address.
        pc = addr;
        return;
    }

    // All addresses processed
    pc = 0xFFFF;
}

#endif // ENABLE_STATIC_COMPILE

#ifdef PLATFORM_NES
#pragma section bank19
#else
#pragma section bank24
#endif

// -------------------------------------------------------------------------
// Compile a single block — moved to BANK_SA_CODE (compile-time only, not perf).
// Called repeatedly by sa_run during batch compilation.
// Returns 1 if a block was compiled, 0 if nothing to do.
// -------------------------------------------------------------------------

#ifdef ENABLE_STATIC_COMPILE

uint16_t sa_blocks_total = 0;

// -------------------------------------------------------------------------
// sa_compile_one_block — compile a single block to flash.
//
// Pass 1 (sa_compile_pass == 0):
//   Compile to RAM buffer.  Allocate a max-size flash slot.  Write PC
//   table entries (needed for backward branch resolution within pass 1).
//   Do NOT write code to flash.  Forward branches use 21-byte patchable
//   template (pessimistic size).  After return, caller records the entry
//   in the entry list (entry_pc, exit_pc, native_addr, bank, code_len).
//
// Pass 2 (sa_compile_pass == 2):
//   Recompile with full knowledge of all block entry addresses (via
//   lookup_entry_list).  Allocates max-size (same as pass 1) to keep
//   sector-skip decisions identical.  Forward branches can now use
//   2-byte native branches or 5-byte direct JMP for same-bank targets.
//   Write code + header to flash.  Write PC tables fresh (they were
//   erased between passes).  Block termination is forced at
//   sa_block_exit_pc to match pass 1's block boundaries.
//
// Returns 1 if a block was processed, 0 if cache full.
// -------------------------------------------------------------------------
static uint8_t sa_compile_one_block(void)
{
#ifdef ENABLE_COMPILE_PPU_EFFECT
    // Toggle green emphasis (bit 6) per block compiled
    compile_ppu_effect ^= 0x40;
    lnPPUMASK = 0x3B | compile_ppu_effect;
    *(volatile uint8_t*)0x2001 = lnPPUMASK;  // mid-frame
#endif

    // Allocate space in a flash sector for max-size block.
    // BOTH passes use max-size so that flash_sector_alloc makes identical
    // sector-skip decisions.  This guarantees every block lands at the
    // same flash address in both passes, keeping entry list native_addr
    // values correct.  The wasted space per block is reclaimed after
    // pass 2 by a sector compaction sweep.
    //
    // Allocate space in a flash sector for max-size block.
    // BOTH passes use max-size so that flash_sector_alloc makes identical
    // sector-skip decisions, keeping entry list native_addr values correct.
    //
    // Save allocator state to undo if the block produces no code (rare:
    // first instruction is interpreted).  Empty blocks must not appear in
    // the entry list or consume flash, or pass 1/2 addresses will diverge.
    extern uint16_t sector_free_offset[];
    extern uint8_t next_free_sector;
    uint8_t pre_alloc_next_free = next_free_sector;

    if (!flash_sector_alloc(CODE_SIZE + EPILOGUE_SIZE + XBANK_EPILOGUE_SIZE))
        return 0;           // cache full

    // For undo: restore allocated sector's free pointer to header_start
    // (conservative — may waste a few alignment bytes, but correct).
    uint8_t alloc_sector = next_free_sector;
    uint16_t undo_free_offset = flash_code_address & FLASH_SECTOR_MASK;

    uint16_t entry_pc = pc;

    // Use cache index 0 as temporary compilation buffer (same as run_6502)
    cache_index = 0;
    code_index = 0;
    cache_flag[0] = 0;
    block_has_jsr = 0;  // reset for new block
#ifdef ENABLE_PEEPHOLE
    block_flags_saved = 0;
#endif

    // Set entry PC so try_intra_block_branch can find block start
    cache_entry_pc_lo[0] = (uint8_t)entry_pc;
    cache_entry_pc_hi[0] = (uint8_t)(entry_pc >> 8);

    // Clear intra-block code_index map
    for (uint8_t ci_i = 0; ci_i < 64; ci_i++)
        block_ci_map[ci_i] = 0;

    setup_flash_pc_tables(pc);

    // Compile loop — same as in run_6502 but without dispatch
    do
    {
        uint16_t pc_old = pc;
        uint8_t code_index_old = code_index;

#ifdef ENABLE_PEEPHOLE
        peephole_skipped = 0;
#endif
        recompile_opcode();

        uint8_t instr_len = code_index - code_index_old;

        // Use recompile_instr_start (set inside recompile_opcode_b2 after
        // any deferred PLP flush) so dispatch/branch targets skip the PLP.
#ifdef ENABLE_PEEPHOLE
        uint8_t native_offset = recompile_instr_start;
#else
        uint8_t native_offset = code_index_old;
#endif

        // Record this instruction's code_index for intra-block branch lookup.
        {
            uint8_t ci_slot = (uint8_t)(pc_old - entry_pc) & 0x3F;
            block_ci_map[ci_slot] = native_offset + 1;
        }

        setup_flash_pc_tables(pc_old);

        if (sa_compile_pass == 2)
        {
            // Pass 2: write code bytes to flash immediately
            for (uint8_t i = 0; i < instr_len; i++)
            {
                flash_byte_program(flash_code_address + BLOCK_HEADER_SIZE + code_index_old + i,
                                   flash_code_bank, cache_code[0][code_index_old + i]);
            }
        }

        if (code_index > (CODE_SIZE - 6))
        {
            cache_flag[0] |= OUT_OF_CACHE;
            cache_flag[0] &= ~READY_FOR_NEXT;
        }

        // Pass 2: force block termination at the same exit_pc as pass 1.
        // This prevents block boundary drift when shorter forward branches
        // leave more room in the code buffer.
        if (sa_compile_pass == 2 && pc >= sa_block_exit_pc)
        {
            cache_flag[0] &= ~READY_FOR_NEXT;
        }

        if (cache_flag[0] & INTERPRET_NEXT_INSTRUCTION)
        {
            flash_cache_pc_update(native_offset, INTERPRETED);
        }
#ifdef ENABLE_PEEPHOLE
        else if (peephole_skipped)
        {
            flash_cache_pc_update(native_offset, INTERPRETED);
        }
#endif
        else if (instr_len || pc != pc_old)
        {
            flash_cache_pc_update(native_offset, RECOMPILED);

#ifdef ENABLE_OPTIMIZER_V2
            if (sa_compile_pass == 2)
            {
                opt2_notify_block_compiled(pc_old,
                    flash_code_address + BLOCK_HEADER_SIZE + native_offset,
                    flash_code_bank);
            }
#endif
        }

    } while (cache_flag[0] & READY_FOR_NEXT);

    if (code_index == 0)
    {
        // Block produced no native code (first instruction was interpreted).
        // Undo the flash allocation so the empty slot doesn't consume space
        // or cause address divergence between passes.
        sector_free_offset[alloc_sector] = undo_free_offset;
        next_free_sector = pre_alloc_next_free;
        return 1;  // success (block handled as interpreted), but no entry
    }

    {
        uint16_t exit_pc = pc;

#ifdef ENABLE_PEEPHOLE
        // Flush deferred PLP from peephole before epilogue.
        // Must also write to flash — the compile loop only wrote bytes
        // 0..code_index-1, and the epilogue writer below starts at
        // epilogue_start, so this byte would otherwise stay $FF (erased).
        if (block_flags_saved) {
            *(volatile uint8_t *)&cache_code[0][code_index] = 0x28;  // PLP
            if (sa_compile_pass == 2) {
                flash_byte_program(flash_code_address + BLOCK_HEADER_SIZE + code_index,
                                   flash_code_bank, 0x28);
            }
            code_index++;
            block_flags_saved = 0;
        }
#endif
        uint8_t epilogue_start = code_index;

#ifdef ENABLE_PATCHABLE_EPILOGUE
        cache_code[0][code_index++] = 0x08;     // PHP
        cache_code[0][code_index++] = 0x18;     // CLC
        cache_code[0][code_index++] = 0x90;     // BCC
        cache_code[0][code_index++] = 4;        // offset -> regular at +8
        cache_code[0][code_index++] = 0x28;     // PLP (fast path)
        cache_code[0][code_index++] = 0x4C;     // JMP (fast path)
        cache_code[0][code_index++] = 0xFF;     // JMP lo (PATCHABLE)
        cache_code[0][code_index++] = 0xFF;     // JMP hi (PATCHABLE)
        cache_code[0][code_index++] = 0x85;     // STA _a (regular path)
        cache_code[0][code_index++] = (uint8_t)((uint16_t)&a);
        cache_code[0][code_index++] = 0xA9;     // LDA #<exit_pc
        cache_code[0][code_index++] = (uint8_t)exit_pc;
        cache_code[0][code_index++] = 0x85;     // STA _pc
        cache_code[0][code_index++] = (uint8_t)((uint16_t)&pc);
        cache_code[0][code_index++] = 0xA9;     // LDA #>exit_pc
        cache_code[0][code_index++] = (uint8_t)(exit_pc >> 8);
        cache_code[0][code_index++] = 0x85;     // STA _pc+1
        cache_code[0][code_index++] = (uint8_t)(((uint16_t)&pc) + 1);
        cache_code[0][code_index++] = 0x4C;     // JMP cross_bank_dispatch
        cache_code[0][code_index++] = (uint8_t)((uint16_t)&cross_bank_dispatch);
        cache_code[0][code_index++] = (uint8_t)(((uint16_t)&cross_bank_dispatch) >> 8);
        // +21: Cross-bank fast-path setup (18 bytes)
        cache_code[0][code_index++] = 0x08;     // PHP (re-save guest flags)
        cache_code[0][code_index++] = 0x85;     // STA _a
        cache_code[0][code_index++] = (uint8_t)((uint16_t)&a);
        cache_code[0][code_index++] = 0xA9;     // LDA #$FF (target addr lo, PATCHABLE)
        cache_code[0][code_index++] = 0xFF;
        cache_code[0][code_index++] = 0x8D;     // STA xbank_addr (abs)
        cache_code[0][code_index++] = (uint8_t)((uint16_t)&xbank_addr);
        cache_code[0][code_index++] = (uint8_t)(((uint16_t)&xbank_addr) >> 8);
        cache_code[0][code_index++] = 0xA9;     // LDA #$FF (target addr hi, PATCHABLE)
        cache_code[0][code_index++] = 0xFF;
        cache_code[0][code_index++] = 0x8D;     // STA xbank_addr+1 (abs)
        cache_code[0][code_index++] = (uint8_t)(((uint16_t)&xbank_addr) + 1);
        cache_code[0][code_index++] = (uint8_t)((((uint16_t)&xbank_addr) + 1) >> 8);
        cache_code[0][code_index++] = 0xA9;     // LDA #$FF (target bank, PATCHABLE)
        cache_code[0][code_index++] = 0xFF;
        cache_code[0][code_index++] = 0x4C;     // JMP xbank_trampoline
        cache_code[0][code_index++] = (uint8_t)((uint16_t)&xbank_trampoline);
        cache_code[0][code_index++] = (uint8_t)(((uint16_t)&xbank_trampoline) >> 8);
#else
        cache_code[0][code_index++] = 0x85;     // STA _a
        cache_code[0][code_index++] = (uint8_t)((uint16_t)&a);
        cache_code[0][code_index++] = 0x08;     // PHP
        cache_code[0][code_index++] = 0xA9;     // LDA #<exit_pc
        cache_code[0][code_index++] = (uint8_t)exit_pc;
        cache_code[0][code_index++] = 0x85;     // STA _pc
        cache_code[0][code_index++] = (uint8_t)((uint16_t)&pc);
        cache_code[0][code_index++] = 0xA9;     // LDA #>exit_pc
        cache_code[0][code_index++] = (uint8_t)(exit_pc >> 8);
        cache_code[0][code_index++] = 0x85;     // STA _pc+1
        cache_code[0][code_index++] = (uint8_t)(((uint16_t)&pc) + 1);
        cache_code[0][code_index++] = 0x4C;     // JMP cross_bank_dispatch
        cache_code[0][code_index++] = (uint8_t)((uint16_t)&cross_bank_dispatch);
        cache_code[0][code_index++] = (uint8_t)(((uint16_t)&cross_bank_dispatch) >> 8);
#endif

        if (sa_compile_pass == 2)
        {
            // Pass 2: write header + epilogue to flash
            flash_byte_program(flash_code_address + 0, flash_code_bank, (uint8_t)entry_pc);
            flash_byte_program(flash_code_address + 1, flash_code_bank, (uint8_t)(entry_pc >> 8));
            flash_byte_program(flash_code_address + 2, flash_code_bank, (uint8_t)exit_pc);
            flash_byte_program(flash_code_address + 3, flash_code_bank, (uint8_t)(exit_pc >> 8));
            flash_byte_program(flash_code_address + 4, flash_code_bank, code_index);
            flash_byte_program(flash_code_address + 5, flash_code_bank, epilogue_start);

            for (uint8_t i = epilogue_start; i < code_index; i++)
            {
                flash_byte_program(flash_code_address + BLOCK_HEADER_SIZE + i,
                                   flash_code_bank, cache_code[0][i]);
            }
        }

        // After pass 2, a sector compaction sweep reclaims the wasted
        // space by scanning block headers and adjusting sector_free_offset.
    }

    return 1;
}

#endif // ENABLE_STATIC_COMPILE

#ifdef PLATFORM_NES
#pragma section bank19
#else
#pragma section bank24
#endif

// -------------------------------------------------------------------------
// Subroutine table helpers — moved to BANK_SA_CODE (compile-time only).
// sa_record_subroutine is called from sa_walk_b2 (same bank, direct call).
// sa_subroutine_lookup is kept in bank2 (called from recompile_opcode_b2).
// sa_run has a fixed-bank trampoline below.
// sa_record_indirect_target has a fixed-bank trampoline below.
// -------------------------------------------------------------------------

// Opcode length helper (fixed bank copy for stack-safety analysis)
static uint8_t sa_opcode_length(uint8_t opcode)
{
    uint8_t mode = addrmodes[opcode];
    switch (mode)
    {
        case imp: case acc:
            return 1;
        case imm: case zp: case zpx: case zpy:
        case rel: case indx: case indy:
            return 2;
        case abso: case absx: case absy: case ind:
            return 3;
        default:
            return 1;
    }
}

// Invalid opcode check (fixed bank copy for stack-safety analysis)
static uint8_t sa_is_invalid_opcode(uint8_t op)
{
    uint8_t lo = op & 0x0F;
    uint8_t hi = op >> 4;
    if (lo == 0x02 && hi != 0x0A) return 1;
    if (lo == 0x03 || lo == 0x07 || lo == 0x0F) return 1;
    if (lo == 0x0B && hi != 0x0A && hi != 0x0C && hi != 0x0E) return 1;
    return 0;
}

// Record a JSR target in the subroutine table (if not already present).
// Called from the JSR case of sa_walk_b2 (bank2 can call fixed bank).
// Flags byte left as 0xFF — filled by stack-safety analysis after walk.
void sa_record_subroutine(uint16_t target)
{
    uint8_t lo = (uint8_t)target;
    uint8_t hi = (uint8_t)(target >> 8);

    for (uint16_t i = 0; i < SA_SUBROUTINE_MAX; i++)
    {
        uint16_t entry_addr = SA_SUBROUTINE_BASE + i * 3;
        uint8_t slot_lo = peek_bank_byte(BANK_FLASH_BLOCK_FLAGS, entry_addr + 0);

        // Empty slot (erased flash) — not found, insert here
        if (slot_lo == 0xFF)
        {
            uint8_t slot_hi = peek_bank_byte(BANK_FLASH_BLOCK_FLAGS, entry_addr + 1);
            if (slot_hi == 0xFF)
            {
                flash_byte_program(entry_addr + 0, BANK_FLASH_BLOCK_FLAGS, lo);
                flash_byte_program(entry_addr + 1, BANK_FLASH_BLOCK_FLAGS, hi);
                return;
            }
        }

        // Already recorded?
        if (slot_lo == lo)
        {
            uint8_t slot_hi = peek_bank_byte(BANK_FLASH_BLOCK_FLAGS, entry_addr + 1);
            if (slot_hi == hi)
                return;
        }
    }
    // Table full — silently drop
}

// -------------------------------------------------------------------------
// Stack-safety analysis — walks each subroutine body checking for
// TSX ($BA) or TXS ($9A).  A subroutine without these is "stack-clean"
// and safe for native JSR/RTS.
//
// Simple linear scan from entry to RTS.  Follows JMPs and branches
// within the ROM range.  Does NOT recurse into nested JSRs (those have
// their own entries and are analyzed separately).
// -------------------------------------------------------------------------

static uint8_t sa_analyze_subroutine_safety(uint16_t entry)
{
    // Linear scan with a small local stack for branch targets.
    // 16 pending branch targets should handle any reasonable subroutine.
    uint16_t pending[16];
    uint8_t pending_count = 0;
    uint16_t cur = entry;
    uint16_t steps = 0;

    for (;;)
    {
        while (cur >= ROM_ADDR_MIN && cur <= ROM_ADDR_MAX && steps < 256)
        {
            steps++;
            uint8_t op = read6502(cur);

            // Check for TSX ($BA) or TXS ($9A)
            if (op == 0xBA || op == 0x9A)
                return SA_SUB_DIRTY;

            if (sa_is_invalid_opcode(op))
                break;

            uint8_t len = sa_opcode_length(op);

            switch (op)
            {
                case 0x4C:  // JMP abs
                {
                    uint16_t t = read6502(cur + 1)
                               | ((uint16_t)read6502(cur + 2) << 8);
                    cur = t;
                    continue;
                }

                case 0x6C:  // JMP indirect — can't follow
                case 0x60:  // RTS
                case 0x40:  // RTI
                case 0x00:  // BRK
                    goto next_path;

                case 0x20:  // JSR — skip, continue after
                    cur += 3;
                    continue;

                // Conditional branches
                case 0x10: case 0x30: case 0x50: case 0x70:
                case 0x90: case 0xB0: case 0xD0: case 0xF0:
                {
                    int8_t offset = (int8_t)read6502(cur + 1);
                    uint16_t bt = cur + 2 + offset;
                    if (pending_count < 16)
                        pending[pending_count++] = bt;
                    cur += 2;
                    continue;
                }

                default:
                    cur += len;
                    continue;
            }
        }
next_path:
        if (pending_count == 0)
            break;
        cur = pending[--pending_count];
    }

    return SA_SUB_CLEAN;
}

// Run stack-safety analysis on all recorded subroutines.
static void sa_analyze_all_subroutines(void)
{
    for (uint16_t i = 0; i < SA_SUBROUTINE_MAX; i++)
    {
        uint16_t entry_addr = SA_SUBROUTINE_BASE + i * 3;
        uint8_t lo = peek_bank_byte(BANK_FLASH_BLOCK_FLAGS, entry_addr + 0);
        uint8_t hi = peek_bank_byte(BANK_FLASH_BLOCK_FLAGS, entry_addr + 1);

        if (lo == 0xFF && hi == 0xFF)
            break;  // end of list

        uint8_t flags = peek_bank_byte(BANK_FLASH_BLOCK_FLAGS, entry_addr + 2);
        if (flags != SA_SUB_EMPTY)
            continue;   // already analyzed

        uint16_t sub_addr = lo | ((uint16_t)hi << 8);
        uint8_t result = sa_analyze_subroutine_safety(sub_addr);
        flash_byte_program(entry_addr + 2, BANK_FLASH_BLOCK_FLAGS, result);
    }
}

// -------------------------------------------------------------------------
// Main entry point — fixed-bank trampoline
//
// Two-pass static compilation:
//   Pass 1 (allocate): Scan bitmap, allocate max-size flash slots for
//     every block entry, write PC table entries.  No code emitted.
//     This establishes every block's native address in the PC tables.
//   Between passes: Erase code sectors (banks 4-18).  Reset allocator.
//     PC tables (banks 19-30) survive — they're in different sectors.
//   Pass 2 (emit): Recompile every block.  Now all native addresses are
//     known, so forward branches can use 2-byte native branches (when
//     in ±127 range) or 5-byte direct JMP (same bank).  Write code +
//     headers to flash.  Shrink allocations.
//
// Dynamic recompiler (run_6502) is unaffected — sa_compile_pass==0
// during runtime, so forward branches still use 21-byte patchable
// templates that opt2 patches post-compile.
// -------------------------------------------------------------------------

static void sa_run_b2(void)
{
    // Save pc — reset6502() has already set it to the entry point.
    // The batch compile pass uses pc as scratch, so we must restore it.
    uint16_t saved_pc = pc;

#ifdef ENABLE_COMPILE_PPU_EFFECT
    // Enable PPU compile effect for boot-time visual feedback
    compile_ppu_active = 1;
#endif

    // Run BFS walker — sa_walk_b2 is in bank2 (same bank, direct call)
    sa_walk_b2();

    // Analyze stack safety for all discovered subroutines.
    // sa_analyze_all_subroutines is in bank2 now (direct call).
    // Uses peek_bank_byte/flash_byte_program (WRAM) and read6502 (fixed bank).
    sa_analyze_all_subroutines();

#ifdef ENABLE_STATIC_COMPILE
    // =====================================================================
    // Two-pass static compilation with entry list.
    //
    // Architecture overview:
    //   Bank 18 (BANK_ENTRY_LIST) holds a sequential table of 8-byte entries
    //   written during pass 1.  Each entry records: entry_pc, exit_pc,
    //   native_addr (code_index=0), code_bank, and code_len.  Pass 2
    //   iterates this list (not the bitmap) to find block entry points and
    //   uses lookup_entry_list() for forward branch resolution.
    //
    //   Between passes, ALL code sectors (4-17), PC address sectors (19-26,
    //   except BANK_RENDER and BANK_PLATFORM_ROM),
    //   and PC flag sectors (27-30) are erased.  Only the entry list (bank 18)
    //   and the SA metadata (bank 3) survive.  Pass 2 rewrites everything
    //   fresh, with correct code_index values for the final native layout.
    //
    // Why this works:
    //   - Both passes allocate max-size (258 bytes) in the same bitmap
    //     order, so flash_sector_alloc makes identical sector-skip
    //     decisions and returns identical addresses.
    //   - Pass 2 forces block termination at pass 1's exit_pc, so block
    //     boundaries match even when forward branches produce shorter code.
    //   - Entry list stores code_index=0 native addresses, which are stable
    //     between passes (flash_code_address + BLOCK_PREFIX_SIZE).
    //   - After pass 2, a sector compaction sweep reclaims wasted space
    //     by reading block headers and adjusting sector_free_offset.
    // =====================================================================

    // --- Erase entry list bank before pass 1 ---
    for (uint16_t sector = FLASH_BANK_BASE; sector < FLASH_BANK_BASE + FLASH_BANK_SIZE; sector += FLASH_ERASE_SECTOR_SIZE)
        flash_sector_erase(sector, BANK_ENTRY_LIST);
    entry_list_offset = 0;

    // =====================================================================
    // PASS 1: Compile to RAM, allocate flash slots, populate PC tables,
    //         and build the entry list.
    //
    // For each bitmap-marked code address that hasn't been processed yet,
    // compile the block to the RAM buffer (no flash writes).  This:
    //   1. Discovers block boundaries — the compile loop sets PC flags for
    //      EVERY instruction within the block, so subsequent bitmap entries
    //      that fall mid-block see flag != 0xFF and are skipped.
    //   2. Populates PC tables — needed for backward branch resolution
    //      within pass 1 itself (lookup_native_addr_safe).
    //   3. Records each block's entry_pc, exit_pc, native_addr, and bank
    //      in the entry list for pass 2 to iterate.
    // =====================================================================
    metrics_compile_start();
    sa_compile_pass = 0;
    sa_blocks_total = 0;

    // Note: use != instead of <= to handle ROM_ADDR_MAX == 0xFFFF
    // (uint16_t can never exceed 0xFFFF, so <= would loop forever).
    for (uint16_t scan_cursor = ROM_ADDR_MIN;
         scan_cursor != (uint16_t)(ROM_ADDR_MAX + 1u);
         scan_cursor++)
    {
        if (sa_bitmap_is_unknown(scan_cursor))
            continue;

        metrics_bitmap_entry();

        // Check PC flags: already handled?  The compile loop in
        // sa_compile_one_block sets flags for every instruction within
        // a block, so mid-block addresses are auto-skipped here.
        uint8_t flag = peek_bank_byte(
            (scan_cursor >> 14) + BANK_PC_FLAGS,
            (uint16_t)&flash_cache_pc_flags[scan_cursor & FLASH_BANK_MASK]);
        if (flag != 0xFF)
        {
            metrics_block_skipped();
            continue;
        }

        // Compile block to RAM (no flash writes when sa_compile_pass==0).
        // This allocates a max-size flash slot and updates PC tables for
        // all instructions within the block.
        pc = scan_cursor;
        if (!sa_compile_one_block())
            break;  // cache full

        // Record block in entry list: entry_pc, exit_pc, native_addr, bank, code_len.
        // After sa_compile_one_block returns:
        //   pc = exit_pc (guest PC after last compiled instruction)
        //   flash_code_address = allocated slot address (set by flash_sector_alloc)
        //   flash_code_bank = allocated bank
        //   code_index = total bytes (code + epilogue)
        //
        // Skip blocks with code_index==0 — these are "interpreted" entries
        // (first instruction hit enable_interpret()).  They have PC flags
        // set but no native code.  Recording them would create entry list
        // entries pointing to unwritten flash, causing JMP-to-BRK crashes.
        if (code_index > 0)
        {
            uint16_t entry_native = flash_code_address + BLOCK_PREFIX_SIZE;
            uint16_t addr = FLASH_BANK_BASE + entry_list_offset;
            flash_byte_program(addr + 0, BANK_ENTRY_LIST, (uint8_t)scan_cursor);
            flash_byte_program(addr + 1, BANK_ENTRY_LIST, (uint8_t)(scan_cursor >> 8));
            flash_byte_program(addr + 2, BANK_ENTRY_LIST, (uint8_t)pc);       // exit_pc lo
            flash_byte_program(addr + 3, BANK_ENTRY_LIST, (uint8_t)(pc >> 8)); // exit_pc hi
            flash_byte_program(addr + 4, BANK_ENTRY_LIST, (uint8_t)entry_native);
            flash_byte_program(addr + 5, BANK_ENTRY_LIST, (uint8_t)(entry_native >> 8));
            flash_byte_program(addr + 6, BANK_ENTRY_LIST, flash_code_bank);
            flash_byte_program(addr + 7, BANK_ENTRY_LIST, code_index);  // code_len
            entry_list_offset += 8;
            metrics_block_compiled(code_index);
        }
        else
        {
            metrics_block_failed();
        }

        sa_blocks_total++;
    }

    // =====================================================================
    // Between passes: erase code sectors + PC table sectors, reset allocator.
    //
    // Code banks 4-16 (13 banks, 52 sectors): erased so pass 2 can
    //   rewrite code with correct branch encodings.
    // PC address banks 19-26 (up to 8 banks): erased because
    //   mid-block code_index values shift when pass 2 uses shorter branches.
    //   flash_cache_pc_update's AND-corruption guard requires erased (0xFF)
    //   flag bytes before writing.
    //   SKIPPED: BANK_RENDER, BANK_PLATFORM_ROM, BANK_SA_CODE, BANK_INIT_CODE (and NES PRG banks).
    // PC flag banks 27-30 (4 banks, 16 sectors): erased for the same
    //   reason — pass 2 rewrites all PC flags fresh.
    //
    // Entry list (bank 18) survives.  SA metadata (bank 3) survives.
    // =====================================================================

    // Erase code sectors
    for (uint8_t bank = BANK_CODE; bank < BANK_CODE + FLASH_CACHE_BANKS; bank++)
    {
        for (uint16_t sector = FLASH_BANK_BASE; sector < FLASH_BANK_BASE + FLASH_BANK_SIZE; sector += FLASH_ERASE_SECTOR_SIZE)
            flash_sector_erase(sector, bank);
    }

    // Erase PC address sectors (banks 19-26), skip repurposed banks
    for (uint8_t bank = BANK_PC; bank < BANK_PC + 8; bank++)
    {
        if (bank == BANK_RENDER) continue;        // render/init code lives here
        if (bank == BANK_PLATFORM_ROM) continue;  // guest ROM data lives here
        if (bank == BANK_SA_CODE) continue;       // SA walker/compiler code
        if (bank == BANK_INIT_CODE) continue;     // init-only code + metrics
#ifdef PLATFORM_NES
        if (bank == BANK_NES_PRG_LO) continue;   // NES PRG-ROM low bank
#endif
        for (uint16_t sector = FLASH_BANK_BASE; sector < FLASH_BANK_BASE + FLASH_BANK_SIZE; sector += FLASH_ERASE_SECTOR_SIZE)
            flash_sector_erase(sector, bank);
    }

    // Erase PC flag sectors (banks 27-30)
    for (uint8_t bank = BANK_PC_FLAGS; bank < BANK_PC_FLAGS + 4; bank++)
    {
        for (uint16_t sector = FLASH_BANK_BASE; sector < FLASH_BANK_BASE + FLASH_BANK_SIZE; sector += FLASH_ERASE_SECTOR_SIZE)
            flash_sector_erase(sector, bank);
    }

    flash_cache_init_sectors();

    // =====================================================================
    // PASS 2: Recompile with full knowledge of all block entry addresses.
    //
    // Iterates the entry list (not the bitmap).  For each block:
    //   - Sets sa_block_exit_pc so the compile loop terminates at the
    //     same guest PC as pass 1 (prevents block boundary drift when
    //     shorter forward branches leave more room in the code buffer).
    //   - sa_compile_pass==2 tells recompile_opcode_b2() to use
    //     lookup_entry_list() for forward branches, emitting 2-byte
    //     native branches or 5-byte Bxx_inv+JMP where possible.
    //   - Writes code + headers to flash.  Writes PC tables fresh.
    //
    // Allocation uses max-size (same as pass 1) so blocks land at the
    // SAME flash addresses — the entry list's native_addr values remain
    // correct.
    // =====================================================================
    sa_compile_pass = 2;

    for (uint16_t i = 0; i < entry_list_offset; i += 8)
    {
        // Read entry from the list
        uint16_t addr = FLASH_BANK_BASE + i;
        uint8_t  ep_lo = peek_bank_byte(BANK_ENTRY_LIST, addr + 0);
        uint8_t  ep_hi = peek_bank_byte(BANK_ENTRY_LIST, addr + 1);
        uint8_t  ex_lo = peek_bank_byte(BANK_ENTRY_LIST, addr + 2);
        uint8_t  ex_hi = peek_bank_byte(BANK_ENTRY_LIST, addr + 3);

        if (ep_lo == 0xFF && ep_hi == 0xFF)
            break;  // end sentinel

        uint16_t entry_pc_val = ep_lo | ((uint16_t)ep_hi << 8);
        sa_block_exit_pc = ex_lo | ((uint16_t)ex_hi << 8);

        pc = entry_pc_val;
        if (!sa_compile_one_block())
            break;  // cache full

#ifdef ENABLE_OPTIMIZER_V2
        // Periodic sweep: drain pending-branch queue (for any cross-bank
        // forward branches that fell through to the 21-byte template).
        {
            static uint8_t sa_blocks_compiled = 0;
            if (++sa_blocks_compiled >= 4)
            {
                sa_blocks_compiled = 0;
                opt2_sweep_pending_patches();
            }
        }
#endif
    }

    // Restore dynamic mode
    sa_compile_pass = 0;
    sa_block_exit_pc = 0xFFFF;

    // Final drain after all blocks are compiled.
    // Aggressively resolves pending patches and epilogues before execution.
#ifdef ENABLE_OPTIMIZER_V2
    opt2_drain_static_patches();
#endif

    // =====================================================================
    // Interpreted-flag repair pass.
    //
    // Pass 0 flagged all known-code PCs (RECOMPILED or INTERPRETED).
    // Between passes, ALL flag banks were erased.  Pass 2 rewrote flags
    // for entry-list blocks, but blocks with code_index==0 (first
    // instruction is always-interpreted, e.g. CLI/SEI/PHP/PLP/SED or
    // I/O access) were NOT in the entry list.  Their flags are still $FF.
    //
    // Without this repair, each such PC triggers a wasted flash_sector_alloc
    // at runtime (~7,285 cycles + 258 bytes of flash per PC).
    //
    // Fix: scan the bitmap and mark any remaining $FF-flagged known-code
    // PCs as INTERPRETED ($80).  We don't need the native address — the
    // dispatcher will return 2 (interpret) for $80 flags.
    // =====================================================================
    {
        for (uint16_t scan_addr = ROM_ADDR_MIN;
             scan_addr != (uint16_t)(ROM_ADDR_MAX + 1u);
             scan_addr++)
        {
            if (sa_bitmap_is_unknown(scan_addr))
                continue;

            uint8_t flag_bank = (scan_addr >> 14) + BANK_PC_FLAGS;
            uint16_t flag_addr = (uint16_t)&flash_cache_pc_flags[scan_addr & FLASH_BANK_MASK];

            uint8_t flag = peek_bank_byte(flag_bank, flag_addr);
            if (flag != 0xFF)
                continue;   // already programmed by pass 2

            // This PC is known code but has no flag — mark it INTERPRETED.
            // Flag byte $80 = bit 7 set, bit 6 clear → dispatch returns 2.
            flash_byte_program(flag_addr, flag_bank, RECOMPILED);
        }
    }

    // =====================================================================
    // Sector compaction: reclaim wasted space from max-size allocations.
    //
    // Both passes used max-size allocations (250 bytes of code space) to
    // guarantee identical sector-skip decisions.  Now that pass 2 is
    // complete, we scan each sector's block headers to find the actual
    // end of the last block and adjust sector_free_offset accordingly.
    // This lets the dynamic path (run_6502) pack new blocks tightly in
    // the leftover space.
    //
    // CRITICAL: The walk must advance by the same max-size stride used
    // during allocation, NOT by actual code_len.  Consecutive blocks are
    // spaced (max-alloc + alignment) bytes apart; the gap between them
    // is erased ($FF).  Advancing by actual code_len would land in the
    // gap, read code_len=$FF, and stop — leaving sector_free_offset
    // pointing into the middle of valid statically compiled blocks.
    // The dynamic compiler would then write over them without erasing,
    // producing AND-corrupted garbage (flash can only clear bits).
    //
    // Block header format: byte +4 = code_len (code + epilogue total).
    // =====================================================================
    {
        extern uint16_t sector_free_offset[];
        // Max-size code allocation used by sa_compile_one_block:
        uint16_t sa_max_alloc = CODE_SIZE + EPILOGUE_SIZE + XBANK_EPILOGUE_SIZE;

        for (uint8_t s = 0; s < FLASH_CACHE_SECTORS; s++)
        {
            if (sector_free_offset[s] == 0)
                continue;  // empty sector

            uint8_t bank = (s >> 2) + BANK_CODE;
            uint16_t sector_base = FLASH_BANK_BASE + ((uint16_t)(s & 3) << 12);

            uint16_t offset = 0;
            uint16_t last_end = 0;
            while (offset < FLASH_ERASE_SECTOR_SIZE)
            {
                // Align to find next block's code_start (same formula as flash_sector_alloc)
                uint16_t code_start = (offset + BLOCK_HEADER_SIZE + BLOCK_ALIGNMENT_MASK)
                                    & ~BLOCK_ALIGNMENT_MASK;
                uint16_t header_start = code_start - BLOCK_HEADER_SIZE;
                if (code_start >= FLASH_ERASE_SECTOR_SIZE)
                    break;

                // Read code_len from header byte +4
                uint8_t code_len = peek_bank_byte(bank, sector_base + header_start + 4);
                if (code_len == 0xFF || code_len == 0)
                    break;  // no more blocks (erased or empty)

                last_end = code_start + code_len;
                // Advance by max-size allocation stride (matching flash_sector_alloc)
                // so the next iteration lands at the correct next block, not in
                // the erased gap between max-size slots.
                offset = code_start + sa_max_alloc;
            }

            if (last_end > 0 && last_end < sector_free_offset[s])
                sector_free_offset[s] = last_end;
        }
    }

#endif // ENABLE_STATIC_COMPILE

    // Restore normal PPU rendering after static analysis
#ifdef ENABLE_COMPILE_PPU_EFFECT
    compile_ppu_active = 0;
    compile_ppu_effect = 0;
    lnPPUMASK = 0x3A;
    *(volatile uint8_t*)0x2001 = lnPPUMASK;  // mid-frame restore
#endif

    // Dump SA metrics — metrics_compile_end() is a WRAM macro (safe here).
    // metrics_dump_sa_b2() lives in BANK_RENDER — called from the
    // fixed-bank sa_run() trampoline after we return.
    metrics_compile_end();

    // Restore pc — reset6502() set it to the game's entry point before
    // sa_run() was called.  The batch compile pass clobbered it.
    pc = saved_pc;
}

// -------------------------------------------------------------------------
// Subroutine lookup — check if a JSR target is stack-clean.
// Lives in bank2 so recompile_opcode_b2 (also bank2) can call it directly.
// Returns SA_SUB_CLEAN, SA_SUB_DIRTY, or SA_SUB_EMPTY (not found).
// -------------------------------------------------------------------------
#pragma section bank2

uint8_t sa_subroutine_lookup(uint16_t target_pc)
{
    uint8_t lo = (uint8_t)target_pc;
    uint8_t hi = (uint8_t)(target_pc >> 8);

    for (uint16_t i = 0; i < SA_SUBROUTINE_MAX; i++)
    {
        uint16_t entry_addr = SA_SUBROUTINE_BASE + i * 3;
        uint8_t slot_lo = peek_bank_byte(BANK_FLASH_BLOCK_FLAGS, entry_addr + 0);

        // Empty slot — end of populated entries
        if (slot_lo == 0xFF)
        {
            uint8_t slot_hi = peek_bank_byte(BANK_FLASH_BLOCK_FLAGS, entry_addr + 1);
            if (slot_hi == 0xFF)
                return SA_SUB_EMPTY;
        }

        if (slot_lo == lo)
        {
            uint8_t slot_hi = peek_bank_byte(BANK_FLASH_BLOCK_FLAGS, entry_addr + 1);
            if (slot_hi == hi)
                return peek_bank_byte(BANK_FLASH_BLOCK_FLAGS, entry_addr + 2);
        }
    }
    return SA_SUB_EMPTY;
}

// -------------------------------------------------------------------------
// Runtime feedback: record an indirect-jump target discovered during
// execution.  Called from the interpreter via fixed-bank trampoline.
// -------------------------------------------------------------------------
#ifdef PLATFORM_NES
#pragma section bank19
#else
#pragma section bank24
#endif

static void sa_record_indirect_target_b2(uint16_t target_pc, uint8_t type)
{
    // Quick check: is this address already in the bitmap?
    // If the walker already found it, no need to record.
    if (!sa_bitmap_is_unknown(target_pc))
        return;

    // Check if already in the indirect list
    if (sa_indirect_exists(target_pc))
        return;

    // Find empty slot
    uint16_t slot = sa_indirect_find_empty();
    if (slot >= SA_INDIRECT_MAX)
        return;         // list full

    uint16_t entry_addr = SA_INDIRECT_BASE + slot * 3;
    flash_byte_program(entry_addr + 0, BANK_FLASH_BLOCK_FLAGS,
                       (uint8_t)target_pc);
    flash_byte_program(entry_addr + 1, BANK_FLASH_BLOCK_FLAGS,
                       (uint8_t)(target_pc >> 8));
    flash_byte_program(entry_addr + 2, BANK_FLASH_BLOCK_FLAGS, type);

    // Also mark it in the bitmap for the walker
    sa_bitmap_mark(target_pc);
}

// -------------------------------------------------------------------------
// Fixed-bank trampolines for functions moved to bank24 (BANK_SA_CODE)
// -------------------------------------------------------------------------
#pragma section default

void sa_run(void)
{
    uint8_t saved_bank = mapper_prg_bank;
    bankswitch_prg(BANK_SA_CODE);
    sa_run_b2();
#ifdef ENABLE_METRICS
    bankswitch_prg(BANK_RENDER);
    metrics_dump_sa_b2();
#endif
    bankswitch_prg(saved_bank);
}

void sa_record_indirect_target(uint16_t target_pc, uint8_t type)
{
    uint8_t saved_bank = mapper_prg_bank;
    bankswitch_prg(BANK_SA_CODE);
    sa_record_indirect_target_b2(target_pc, type);
    bankswitch_prg(saved_bank);
}

#endif // ENABLE_STATIC_ANALYSIS
