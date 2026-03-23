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

#ifdef ENABLE_IR
#include "../backend/ir.h"
extern ir_ctx_t ir_ctx;
extern uint8_t ir_optimize(ir_ctx_t *ctx);
extern uint8_t ir_optimize_ext(ir_ctx_t *ctx);
extern uint8_t ir_lower(ir_ctx_t *ctx, uint8_t *buf, uint8_t buf_size);
extern uint8_t ir_opt_rmw_fusion(ir_ctx_t *ctx);
extern void ir_resolve_deferred_patches(void);
extern void ir_resolve_direct_branches(void);
extern void ir_rebuild_block_ci_map(void);
extern void ir_compute_instr_offsets(void);
// SA pass 2 mid-block PC tracking arrays (defined in dynamos.c)
extern uint8_t sa_ir_instr_pc_offset[];
extern uint8_t sa_ir_instr_first_node[];
extern uint8_t sa_ir_instr_native_off[];
extern uint8_t sa_ir_instr_count;
#endif

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
extern uint8_t sa_do_compile;
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

// Auto-detected idle loop table: 2 bytes each (addr_lo, addr_hi)
// Populated by sa_scan_idle_loops after the BFS walk.
#ifdef ENABLE_AUTO_IDLE_DETECT
uint8_t sa_idle_list[SA_IDLE_MAX * SA_IDLE_ENTRY_SIZE];
#endif

#pragma section default

// Shorthand macros for the flash addresses of the bank3 variables.
// These resolve to the linker-assigned addresses at compile time.
#define SA_BITMAP_BASE      ((uint16_t)&sa_code_bitmap[0])
#define SA_HEADER_BASE      ((uint16_t)&sa_header[0])
#define SA_INDIRECT_BASE    ((uint16_t)&sa_indirect_list[0])
#define SA_SUBROUTINE_BASE  ((uint16_t)&sa_subroutine_list[0])

#ifdef ENABLE_AUTO_IDLE_DETECT
#define SA_IDLE_BASE        ((uint16_t)&sa_idle_list[0])
#endif

// -------------------------------------------------------------------------
// Bitmap helpers (fixed bank) — called from sa_record_indirect_target
// and from bank2 code (which can reach fixed bank at $C000+).
// Bit CLEAR = known code.  Flash can only clear bits, so marking is free.
// -------------------------------------------------------------------------

static uint8_t sa_bitmap_is_unknown(uint16_t addr)
{
    uint16_t byte_offset = (addr & 0x7FFF) >> 3;
    uint8_t bit_mask = 1 << (addr & 7);
    uint8_t val = peek_bank_byte(BANK_FLASH_BLOCK_FLAGS,
                                 SA_BITMAP_BASE + byte_offset);
    return val & bit_mask;
}

void sa_bitmap_mark(uint16_t addr)
{
    uint16_t byte_offset = (addr & 0x7FFF) >> 3;
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
// Indirect-target list helpers — moved to BANK_SA_CODE (bank19/bank24)
// alongside their only caller sa_record_indirect_target_b2.
// See the section near sa_record_indirect_target_b2 below.
// -------------------------------------------------------------------------

// -------------------------------------------------------------------------
// BFS queue state (fixed-bank / BSS)
// CRITICAL: These MUST NOT be in bank2 — static variables in a bank2
// pragma section land in flash and are read-only at runtime.
// Placed here in the default section so vbcc puts them in BSS (WRAM).
// -------------------------------------------------------------------------
static uint8_t q_head;
static uint8_t q_tail;
static uint8_t q_count;

#ifdef ENABLE_AUTO_IDLE_DETECT
// Count of auto-detected idle PCs (lives in BSS/WRAM, set during SA walk).
uint8_t sa_idle_count = 0;
#endif

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
// Idle loop scanner (bank2) — detect tight polling loops.
//
// Called after the BFS walk has populated the code bitmap.  For every
// conditional branch in the ROM whose displacement is backward (negative),
// inspects the loop body from branch_target to branch_instruction.  If
// ALL opcodes in the body are side-effect-free (loads, compares, bit-tests,
// branches, NOP), the branch target is flagged as an idle PC.
//
// Typical NES idle patterns detected:
//   LDA $xx    / BNE *-3   (wait for RAM flag cleared by NMI)
//   LDA $xx    / BEQ *-3   (wait for RAM flag set by NMI)
//   BIT $2002  / BPL *-3   (wait for VBlank bit 7 of PPU status)
//   LDA zp     / CMP zp    / BNE *-5  (wait for counter match)
//   LDA $xxxx  / BNE *-5   (absolute RAM poll)
//
// Constraints:
//   - Loop body ≤ 12 bytes (prevents false positives on larger loops)
//   - No stores (STA/STX/STY), increments (INC/DEC), shifts, JSR, JMP
//   - No stack manipulation (PHA/PLA/PHP/PLP/TSX/TXS)
//   - Body must contain at least one load/compare/bit-test (reads memory)
//   - The backward branch must target a known-code address
// -------------------------------------------------------------------------
#ifdef ENABLE_AUTO_IDLE_DETECT

#define IDLE_LOOP_MAX_BYTES  12   // max loop body size in bytes

// Check if an opcode is side-effect-free for idle-loop purposes.
// Returns 1 if the opcode only reads memory and/or modifies registers/flags.
static uint8_t is_idle_safe_opcode(uint8_t op)
{
    switch (op)
    {
        // --- Loads (read-only, set A/X/Y + flags) ---
        case 0xA5: case 0xB5:              // LDA zp, zpx
        case 0xAD: case 0xBD: case 0xB9:  // LDA abs, absx, absy
        case 0xA1: case 0xB1:              // LDA indx, indy
        case 0xA9:                          // LDA imm
        case 0xA6: case 0xB6:              // LDX zp, zpy
        case 0xAE: case 0xBE:              // LDX abs, absy
        case 0xA2:                          // LDX imm
        case 0xA4: case 0xB4:              // LDY zp, zpx
        case 0xAC: case 0xBC:              // LDY abs, absx
        case 0xA0:                          // LDY imm

        // --- Compares (read-only, set flags) ---
        case 0xC5: case 0xD5:              // CMP zp, zpx
        case 0xCD: case 0xDD: case 0xD9:  // CMP abs, absx, absy
        case 0xC1: case 0xD1:              // CMP indx, indy
        case 0xC9:                          // CMP imm
        case 0xE4: case 0xEC:              // CPX zp, abs
        case 0xE0:                          // CPX imm
        case 0xC4: case 0xCC:              // CPY zp, abs
        case 0xC0:                          // CPY imm

        // --- Bit test (read-only, set flags) ---
        case 0x24: case 0x2C:              // BIT zp, abs

        // --- Branches (control flow only) ---
        case 0x10: case 0x30:              // BPL, BMI
        case 0x50: case 0x70:              // BVC, BVS
        case 0x90: case 0xB0:              // BCC, BCS
        case 0xD0: case 0xF0:              // BNE, BEQ

        // --- Register transfers (no memory access) ---
        case 0xAA: case 0xA8:              // TAX, TAY
        case 0x8A: case 0x98:              // TXA, TYA

        // --- No-op ---
        case 0xEA:                          // NOP

        // --- Flag manipulation (no memory side effects) ---
        case 0x18: case 0x38:              // CLC, SEC
        case 0xD8: case 0xF8:              // CLD, SED
        case 0x58: case 0x78:              // CLI, SEI
        case 0xB8:                          // CLV

        // --- AND/ORA/EOR (read-only, modify A + flags) ---
        case 0x25: case 0x35:              // AND zp, zpx
        case 0x2D: case 0x3D: case 0x39:  // AND abs, absx, absy
        case 0x29:                          // AND imm
        case 0x05: case 0x15:              // ORA zp, zpx
        case 0x0D: case 0x1D: case 0x19:  // ORA abs, absx, absy
        case 0x09:                          // ORA imm
        case 0x45: case 0x55:              // EOR zp, zpx
        case 0x4D: case 0x5D: case 0x59:  // EOR abs, absx, absy
        case 0x49:                          // EOR imm

            return 1;

        default:
            return 0;
    }
}

// Check if an opcode reads memory (as opposed to register-only ops).
// Used to require at least one memory read in the loop body — a loop
// of pure register ops (TAX/NOP/BNE) is a delay loop, not an idle wait.
static uint8_t is_memory_read_opcode(uint8_t op)
{
    switch (op)
    {
        // LDA from memory (not imm)
        case 0xA5: case 0xB5:
        case 0xAD: case 0xBD: case 0xB9:
        case 0xA1: case 0xB1:
        // LDX from memory (not imm)
        case 0xA6: case 0xB6:
        case 0xAE: case 0xBE:
        // LDY from memory (not imm)
        case 0xA4: case 0xB4:
        case 0xAC: case 0xBC:
        // CMP from memory (not imm)
        case 0xC5: case 0xD5:
        case 0xCD: case 0xDD: case 0xD9:
        case 0xC1: case 0xD1:
        // CPX from memory (not imm)
        case 0xE4: case 0xEC:
        // CPY from memory (not imm)
        case 0xC4: case 0xCC:
        // BIT (memory)
        case 0x24: case 0x2C:
        // AND/ORA/EOR from memory (not imm)
        case 0x25: case 0x35:
        case 0x2D: case 0x3D: case 0x39:
        case 0x05: case 0x15:
        case 0x0D: case 0x1D: case 0x19:
        case 0x45: case 0x55:
        case 0x4D: case 0x5D: case 0x59:
            return 1;
        default:
            return 0;
    }
}

// Check if an opcode uses absolute addressing (3-byte instruction with
// a 16-bit operand address).  Used to extract the target address and
// reject idle-loop reads that hit NES hardware registers ($2000-$5FFF).
static uint8_t is_absolute_read(uint8_t op)
{
    switch (op)
    {
        case 0xAD: case 0xBD: case 0xB9:  // LDA abs, absx, absy
        case 0xAE: case 0xBE:              // LDX abs, absy
        case 0xAC: case 0xBC:              // LDY abs, absx
        case 0xCD: case 0xDD: case 0xD9:  // CMP abs, absx, absy
        case 0xEC:                          // CPX abs
        case 0xCC:                          // CPY abs
        case 0x2C:                          // BIT abs
        case 0x2D: case 0x3D: case 0x39:  // AND abs, absx, absy
        case 0x0D: case 0x1D: case 0x19:  // ORA abs, absx, absy
        case 0x4D: case 0x5D: case 0x59:  // EOR abs, absx, absy
            return 1;
        default:
            return 0;
    }
}

// Record an idle PC in the flash table.  Returns 1 on success, 0 if full.
static uint8_t sa_record_idle_pc(uint16_t idle_pc)
{
    if (sa_idle_count >= SA_IDLE_MAX)
        return 0;

    // Check for duplicates
    for (uint8_t i = 0; i < sa_idle_count; i++)
    {
        uint16_t entry_addr = SA_IDLE_BASE + i * SA_IDLE_ENTRY_SIZE;
        uint8_t lo = peek_bank_byte(BANK_FLASH_BLOCK_FLAGS, entry_addr + 0);
        uint8_t hi = peek_bank_byte(BANK_FLASH_BLOCK_FLAGS, entry_addr + 1);
        if (lo == (uint8_t)idle_pc && hi == (uint8_t)(idle_pc >> 8))
            return 1;  // already recorded
    }

    uint16_t entry_addr = SA_IDLE_BASE + sa_idle_count * SA_IDLE_ENTRY_SIZE;
    flash_byte_program(entry_addr + 0, BANK_FLASH_BLOCK_FLAGS, (uint8_t)idle_pc);
    flash_byte_program(entry_addr + 1, BANK_FLASH_BLOCK_FLAGS, (uint8_t)(idle_pc >> 8));
    sa_idle_count++;
    return 1;
}

// Scan the entire ROM for idle polling loops.
// Called once after the BFS walk has populated the code bitmap.
static void sa_scan_idle_loops(void)
{
    sa_idle_count = 0;

    // Count any idle PCs already persisted from a previous run
    // (flash survives soft reset; sa_idle_list is in bank 3).
    for (uint8_t i = 0; i < SA_IDLE_MAX; i++)
    {
        uint16_t entry_addr = SA_IDLE_BASE + i * SA_IDLE_ENTRY_SIZE;
        uint8_t lo = peek_bank_byte(BANK_FLASH_BLOCK_FLAGS, entry_addr + 0);
        uint8_t hi = peek_bank_byte(BANK_FLASH_BLOCK_FLAGS, entry_addr + 1);
        if (lo == 0xFF && hi == 0xFF)
            break;  // end sentinel (erased flash)
        sa_idle_count++;
    }

    // If we already have a full table from a prior run, skip re-scan
    if (sa_idle_count >= SA_IDLE_MAX)
        return;

    // Scan the ROM address space for backward conditional branches.
    // We only check addresses marked as known code in the SA bitmap.
    for (uint16_t scan = ROM_ADDR_MIN; scan != (uint16_t)(ROM_ADDR_MAX + 1u); scan++)
    {
        // Skip unknown addresses (not in code bitmap)
        if (sa_bitmap_is_unknown(scan))
            continue;

        uint8_t op = read6502(scan);

        // Only look at conditional branch instructions
        if (op != 0x10 && op != 0x30 &&   // BPL, BMI
            op != 0x50 && op != 0x70 &&    // BVC, BVS
            op != 0x90 && op != 0xB0 &&    // BCC, BCS
            op != 0xD0 && op != 0xF0)      // BNE, BEQ
            continue;

        // Read displacement — must be backward (negative)
        int8_t disp = (int8_t)read6502(scan + 1);
        if (disp >= 0)
            continue;   // forward branch, not a loop

        uint16_t target = scan + 2 + (int16_t)disp;

        // Target must be in ROM range and known code
        if (target < ROM_ADDR_MIN || target > ROM_ADDR_MAX)
            continue;
        if (sa_bitmap_is_unknown(target))
            continue;

        // Loop body: [target .. scan+1] inclusive
        // (scan = branch opcode, scan+1 = displacement byte)
        uint16_t body_len = (scan + 2) - target;   // includes branch itself
        if (body_len > IDLE_LOOP_MAX_BYTES || body_len < 3)
            continue;   // too large or degenerate

        // Walk the loop body and check every opcode
        uint8_t all_safe = 1;
        uint8_t has_mem_read = 0;
        uint8_t has_hw_read = 0;
        uint16_t body_pc = target;

        while (body_pc < scan + 2)
        {
            uint8_t body_op = read6502(body_pc);
            if (!is_idle_safe_opcode(body_op))
            {
                all_safe = 0;
                break;
            }
            if (is_memory_read_opcode(body_op))
            {
                if (is_absolute_read(body_op))
                {
                    // Decode the 16-bit operand address
                    uint16_t addr = read6502(body_pc + 1)
                                  | ((uint16_t)read6502(body_pc + 2) << 8);
                    // Reject reads from NES hardware registers ($2000-$5FFF).
                    // These are PPU/APU/IO polls (e.g. LDA $2002 for VBlank),
                    // NOT idle waits for an interrupt handler to set a RAM flag.
                    if (addr >= 0x2000 && addr < 0x6000)
                        has_hw_read = 1;
                    else
                        has_mem_read = 1;
                }
                else
                {
                    // ZP or indirect — always targets RAM
                    has_mem_read = 1;
                }
            }

            uint8_t len = opcode_length(body_op);
            body_pc += len;
        }

        // Must end exactly at the byte after the branch displacement
        if (body_pc != scan + 2)
            all_safe = 0;

        if (all_safe && has_mem_read && !has_hw_read)
        {
            if (!sa_record_idle_pc(target))
                return;  // table full
        }
    }
}

#endif // ENABLE_AUTO_IDLE_DETECT

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
        // Range covers bitmap + header + indirect list + subroutine table
        // + idle list (if enabled).
        {
            uint16_t base = SA_BITMAP_BASE & 0xF000;
#ifdef ENABLE_AUTO_IDLE_DETECT
            uint16_t end  = (SA_IDLE_BASE + SA_IDLE_MAX * SA_IDLE_ENTRY_SIZE - 1) | 0x0FFF;
#else
            uint16_t end  = (SA_SUBROUTINE_BASE + SA_SUBROUTINE_MAX * 3 - 1) | 0x0FFF;
#endif
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

    // Darken screen during BFS walk (all 3 emphasis bits + greyscale)
#ifdef ENABLE_COMPILE_PPU_EFFECT
    lnPPUMASK = 0x1B | 0xE0;
    *(volatile uint8_t*)0x2001 = lnPPUMASK;
#endif

    // BFS loop
    while (!q_empty())
    {
        uint16_t cur_pc = q_pop();
        metrics_bfs_visit_address();

#ifdef ENABLE_COMPILE_PPU_EFFECT
        // Toggle greyscale per BFS node (screen stays darkened)
        compile_ppu_effect ^= 0x01;
        lnPPUMASK = 0x1A | 0xE0 | compile_ppu_effect;
        *(volatile uint8_t*)0x2001 = lnPPUMASK;
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
//   Recompile with tight allocation (sa_block_alloc_size = pass 1's
//   code_len).  Backward branches resolve via lookup_native_addr_safe
//   (target already compiled, same iteration order as pass 1).  Forward
//   branches use 21-byte patchable templates (same as pass 1, so
//   code_len matches tight allocation).  opt2_drain_static_patches
//   resolves them post-compile.
//   Write code + header to flash.  Write PC tables fresh (they were
//   erased between passes).  Block termination is forced at
//   sa_block_exit_pc to match pass 1's block boundaries.
//
// Returns 1 if a block was processed, 0 if cache full.
// -------------------------------------------------------------------------
static uint8_t sa_compile_one_block(void)
{
#ifdef ENABLE_COMPILE_PPU_EFFECT
    // Toggle greyscale per block compiled (screen stays darkened)
    compile_ppu_effect ^= 0x01;
    lnPPUMASK = 0x1A | 0xE0 | compile_ppu_effect;
    *(volatile uint8_t*)0x2001 = lnPPUMASK;
#endif

    // Allocate space in a flash sector.
    //
    // Pass 1 (sa_compile_pass==0): max-size allocation.  We don't know
    //   actual code_len yet; forward branches use 21-byte templates.
    //   code_len is recorded in the entry list after compilation.
    //
    // Pass 2 (sa_compile_pass==2): tight allocation using pass 1's
    //   code_len (set in sa_block_alloc_size by the caller).  Backward
    //   branches resolve via lookup_native_addr_safe (target already
    //   compiled in same iteration order).  Forward branches use 21-byte
    //   patchable templates (same as pass 1) — patched post-compile.
    //
    // Save allocator state to undo if the block produces no code (rare:
    // first instruction is interpreted).  Empty blocks must not appear in
    // the entry list or consume flash.
    extern uint16_t sector_free_offset[];
    extern uint8_t next_free_sector;
    uint8_t pre_alloc_next_free = next_free_sector;

    {
        uint8_t alloc_sz = (sa_compile_pass == 2 && sa_block_alloc_size > 0)
                         ? sa_block_alloc_size
                         : CODE_SIZE + EPILOGUE_SIZE + XBANK_EPILOGUE_SIZE;
        if (!flash_sector_alloc(alloc_sz))
            return 0;       // cache full
    }

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

#ifdef ENABLE_IR
    // Reset SA mid-block PC tracking for this block.
    // sa_prev_node_count tracks the IR node count before each instruction.
    // For the first instruction, IR_INIT (inside recompile_opcode_b2) resets
    // ir_ctx.node_count to 0, so initialising to 0 here is correct.
    sa_ir_instr_count = 0;
    uint8_t sa_prev_node_count = 0;
#endif

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

#ifdef ENABLE_IR
        // Track this instruction for post-lowering mid-block PC publishing.
        // Only record when IR is active in pass 2 — non-IR blocks write
        // per-instruction PC flags directly in the loop below.
        if (sa_compile_pass == 2 && ir_ctx.enabled
            && sa_ir_instr_count < SA_IR_MAX_INSTRS) {
            sa_ir_instr_pc_offset[sa_ir_instr_count] = (uint8_t)(pc_old - entry_pc);
            sa_ir_instr_first_node[sa_ir_instr_count] = sa_prev_node_count;
            // Seed raw (pre-IR) native offset.  ir_compute_instr_offsets()
            // overwrites with post-lowering offset when IR succeeds.  When
            // IR overflows mid-block (enabled→0), the raw offset remains
            // and the deferred publish section uses it as-is — correct
            // because IR lowering never ran for this block.
#ifdef ENABLE_PEEPHOLE
            sa_ir_instr_native_off[sa_ir_instr_count] = recompile_instr_start;
#else
            sa_ir_instr_native_off[sa_ir_instr_count] = code_index_old;
#endif
            sa_ir_instr_count++;
            sa_prev_node_count = ir_ctx.node_count;
        }
#endif

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

        if (sa_compile_pass == 2
#ifdef ENABLE_IR
            && !ir_ctx.enabled  /* IR active: skip per-instruction writes,
                                 * bulk write after ir_lower instead */
#endif
        )
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

        // Guard: stop block if pc advanced outside the ROM range or into
        // the vector table.  Mirrors the guard in run_6502's compile loop.
        //
        // IMPORTANT: volatile prevents VBCC -O2 from optimizing this away.
        { volatile uint16_t pc_v = pc;
        if (pc_v >= 0xFFFA || pc_v < ROM_ADDR_MIN || pc_v > ROM_ADDR_MAX)
            cache_flag[0] &= ~READY_FOR_NEXT; }

        // Pass 2: force block termination at the same exit_pc as pass 1.
        // This prevents block boundary drift when shorter forward branches
        // leave more room in the code buffer.
        if (sa_compile_pass == 2 && pc >= sa_block_exit_pc)
        {
            cache_flag[0] &= ~READY_FOR_NEXT;
        }

#ifdef ENABLE_IR
        /* When IR is active in pass 2, skip per-instruction PC flag writes.
         * The pre-optimization native offsets would be wrong after ir_lower.
         * All mid-block PCs are published in the deferred section after
         * lowering (see "Publish ALL mid-block instruction PCs").
         * When IR overflows mid-block (enabled→0), this guard falls
         * through for POST-overflow instructions; PRE-overflow PCs are
         * covered by the deferred section using raw native offsets seeded
         * in the tracking section above.
         * Pass 1 and non-IR blocks still write per-instruction flags. */
        if (!(sa_compile_pass == 2 && ir_ctx.enabled))
#endif
        {
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
        } /* end !ir_ctx.enabled guard */

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

#ifdef ENABLE_IR
        // --- IR optimisation + lowering for SA pass 2 ---
        // When IR recorded a block in pass 2, run the full optimise/lower
        // pipeline (same as the dynamic path in run_6502).  The lowered
        // bytes replace cache_code[0]; code_index is updated.  The bulk
        // code write happens here; PC publishing is deferred until AFTER
        // the header + epilogue are written (see sa_compile_pass==2 block
        // below) to ensure the complete block is in flash before any
        // dispatch can enter it.
        if (sa_compile_pass == 2) {
            if (ir_ctx.enabled) {
                // Track blocks that exceeded the SA instr tracking limit
                if (sa_ir_instr_count == SA_IR_MAX_INSTRS)
                    metrics_ir_instr_overflow();

                uint8_t ir_bytes_before = code_index;
                // IR pipeline runs through WRAM trampoline because this
                // function is in BANK_SA_CODE ($8000-$BFFF) and can't
                // bankswitch to the IR banks without losing its own code.
                // sa_ir_pipeline_full does: optimize(b0) → optimize_ext(b17)
                // → rmw_fusion+lower+resolve(b1) → restore BANK_SA_CODE.
                extern void sa_ir_pipeline_full(void);
                extern uint8_t sa_ir_lowered_size;
                sa_ir_pipeline_full();
                uint8_t lowered_size = sa_ir_lowered_size;

                metrics_ir_instrs_eliminated(sa_ir_instrs_eliminated);

                if (lowered_size) {
                    code_index = lowered_size;
                    /* NOP-pad gap to prevent template BEQ offset corruption
                     * (same guard as the dynamic path in run_6502). */
                    while (code_index < ir_bytes_before)
                        cache_code[0][code_index++] = 0xEA;  /* NOP */
                }
            }
            /* Bulk write all code bytes to flash (epilogue written later).
             * This runs whether IR lowering ran or IR was disabled mid-block
             * (node overflow).  When IR was disabled, the compile loop wrote
             * post-overflow instructions per-instruction but pre-overflow
             * instructions remain only in cache_code[0].  Re-writing
             * already-programmed bytes is harmless on SST39SF040.
             * Matches the dynamic path (dynamos.c) which bulk-writes
             * unconditionally. */
            for (uint8_t bw = 0; bw < code_index; bw++) {
                flash_byte_program(flash_code_address + BLOCK_HEADER_SIZE + bw,
                                   flash_code_bank, cache_code[0][bw]);
            }

            // NOTE: PC publishing (entry + fences) deferred to after
            // epilogue write — see the sa_compile_pass==2 block below.
        }
#endif

#ifdef ENABLE_PEEPHOLE
        // Flush deferred PLP from peephole before epilogue.
        // Must also write to flash — the compile loop only wrote bytes
        // 0..code_index-1, and the epilogue writer below starts at
        // epilogue_start, so this byte would otherwise stay $FF (erased).
        if (block_flags_saved) {
            *(volatile uint8_t *)&cache_code[0][code_index] = 0x28;  // PLP
            if (sa_compile_pass == 2) {
                /* Always write to flash — even when IR is enabled.
                 * If ir_lower already included the PLP, this is a harmless
                 * re-write ($28 over $28).  If ir_lower didn't include it,
                 * this fills the gap that would otherwise stay $FF (erased).
                 * The old !ir_ctx.enabled guard skipped this write but still
                 * incremented code_index, creating a $FF gap byte between
                 * block body and epilogue → illegal opcode crash. */
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
            // Reload entry_pc from global arrays — VBCC -O2 may have
            // corrupted the local via register reuse during compile.
            entry_pc = (uint16_t)cache_entry_pc_lo[0]
                     | ((uint16_t)cache_entry_pc_hi[0] << 8);
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

            // Block-complete sentinel
            flash_byte_program(flash_code_address + 7, flash_code_bank, 0xAA);

#ifdef ENABLE_IR
            // --- Deferred IR PC publishing ---
            // Now that header + epilogue + all code are in flash, it's safe
            // to publish PC table entries.  This matches the dynamic path's
            // order (write everything, then publish) and ensures dispatch
            // never enters a block with incomplete epilogue.
            // Always publish the entry PC — matches the dynamic path which
            // calls setup_flash_pc_tables + flash_cache_pc_update outside
            // any ir_ctx.enabled guard.  When IR was disabled mid-block,
            // per-instruction PC flags were skipped for pre-overflow
            // instructions, but the entry PC must still be published so
            // dispatch can find this block.
            {
                // Reload: flash writes above may have clobbered entry_pc.
                entry_pc = (uint16_t)cache_entry_pc_lo[0]
                         | ((uint16_t)cache_entry_pc_hi[0] << 8);
                setup_flash_pc_tables(entry_pc);
                flash_cache_pc_update(0, RECOMPILED);
#ifdef ENABLE_OPTIMIZER_V2
                opt2_notify_block_compiled(entry_pc,
                    flash_code_address + BLOCK_HEADER_SIZE,
                    flash_code_bank);
#endif
                // Publish mid-block fence PCs — only valid when IR lowering
                // actually ran (enabled=1).  When IR is disabled mid-block,
                // fence offsets were never populated and would be wrong.
                if (ir_ctx.enabled && ir_ctx.fence_count) {
                    uint8_t _f;
                    for (_f = 0; _f < ir_ctx.fence_count; _f++) {
                        uint16_t fpc = ir_ctx.fence_guest_pc[_f];
                        uint8_t  foff = ir_ctx.fence_native_offset[_f];
                        // Safety: don't publish fences past main code
                        if (foff >= epilogue_start) continue;
                        setup_and_update_pc(fpc, foff, RECOMPILED);
#ifdef ENABLE_OPTIMIZER_V2
                        opt2_notify_block_compiled(fpc,
                            flash_code_address + BLOCK_HEADER_SIZE + foff,
                            flash_code_bank);
#endif
                    }
                }

                // --- Publish ALL mid-block instruction PCs ---
                // sa_ir_instr_native_off[] holds either:
                //  (a) post-lowering offsets from ir_compute_instr_offsets()
                //      when IR succeeded (enabled=1), or
                //  (b) raw pre-IR native offsets seeded in the compile loop
                //      when IR overflowed mid-block (enabled=0).
                // Case (b) is correct because IR lowering never ran, so the
                // un-optimised native code is what's actually in flash.
                // Eliminated instructions are forwarded to the NEXT surviving
                // instruction's offset so dispatch entering at that guest PC
                // falls through correctly.  Only trailing eliminated instrs
                // with no successor keep 0xFF and are skipped here.
                // Reload: fence loop above may have clobbered entry_pc.
                entry_pc = (uint16_t)cache_entry_pc_lo[0]
                         | ((uint16_t)cache_entry_pc_hi[0] << 8);
                if (sa_ir_instr_count > 0) {
                    uint8_t _mi;
                    for (_mi = 0; _mi < sa_ir_instr_count; _mi++) {
                        uint8_t noff = sa_ir_instr_native_off[_mi];
                        // Skip trailing eliminated instructions (no successor)
                        if (noff == 0xFF) continue;
                        if (noff >= epilogue_start) continue;
                        // Skip entry PC (already published above at offset 0)
                        if (noff == 0 && sa_ir_instr_pc_offset[_mi] == 0) continue;
                        uint16_t mpc = entry_pc + sa_ir_instr_pc_offset[_mi];
                        // Skip PLP guard: if the native code at this offset
                        // starts with PLP ($28), dispatch entering here would
                        // corrupt the stack.  Advance past it.
                        if (noff < epilogue_start &&
                            cache_code[0][noff] == 0x28)
                            noff++;
                        if (noff >= epilogue_start) continue;
                        setup_and_update_pc(mpc, noff, RECOMPILED);
#ifdef ENABLE_OPTIMIZER_V2
                        opt2_notify_block_compiled(mpc,
                            flash_code_address + BLOCK_HEADER_SIZE + noff,
                            flash_code_bank);
#endif
                    }
                }
            }
#endif  /* ENABLE_IR */
        }
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
// sa_subroutine_lookup has a fixed-bank trampoline (callable from any bank).
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
// Stack-safety analysis — walks each subroutine body checking for:
//   1. TSX ($BA) or TXS ($9A) — direct SP manipulation
//   2. Unbalanced PLA/PLP — pulls below entry stack depth, indicating
//      a return-address discard trick (e.g. DK's PLA/PLA at $F33B)
// A subroutine without these is "stack-clean" and safe for native JSR/RTS.
//
// Simple linear scan from entry to RTS.  Follows JMPs and branches
// within the ROM range.  Does NOT recurse into nested JSRs (those have
// their own entries and are analyzed separately).  Stack depth is tracked
// per-path and propagated to branch targets.
// -------------------------------------------------------------------------

static uint8_t sa_analyze_subroutine_safety(uint16_t entry)
{
    // Linear scan with a small local stack for branch targets.
    // 16 pending branch targets should handle any reasonable subroutine.
    uint16_t pending[16];
    int8_t   pending_depth[16];  // stack depth at each pending branch target
    uint8_t pending_count = 0;
    uint16_t cur = entry;
    uint16_t steps = 0;
    int8_t stack_depth = 0;     // track push/pull balance (0 = entry level)

    for (;;)
    {
        while (cur >= ROM_ADDR_MIN && cur <= ROM_ADDR_MAX && steps < 256)
        {
            steps++;
            uint8_t op = read6502(cur);

            // Check for TSX ($BA) or TXS ($9A)
            if (op == 0xBA || op == 0x9A)
                return SA_SUB_DIRTY;

            // Track push/pull depth to detect unbalanced stack tricks.
            // A PLA/PLP that pulls below entry depth means the subroutine
            // is discarding its caller's return address (e.g. DK's PLA/PLA
            // trick at $F33B, $E107).  These are not safe for NJSR.
            switch (op)
            {
                case 0x48:  // PHA
                case 0x08:  // PHP
                    stack_depth++;
                    break;
                case 0x68:  // PLA
                case 0x28:  // PLP
                    stack_depth--;
                    if (stack_depth < 0)
                        return SA_SUB_DIRTY;  // unbalanced pull
                    break;
            }

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
                    if (pending_count < 16) {
                        pending[pending_count] = bt;
                        pending_depth[pending_count] = stack_depth;
                        pending_count++;
                    }
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
        pending_count--;
        cur = pending[pending_count];
        stack_depth = pending_depth[pending_count];
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

#ifdef ENABLE_AUTO_IDLE_DETECT
    // Scan for idle polling loops (backward branches with side-effect-free bodies).
    // Must run after BFS walk so the code bitmap is fully populated.
    sa_scan_idle_loops();
#endif

#ifdef ENABLE_STATIC_COMPILE
  if (sa_do_compile) {
    // =====================================================================
    // Two-pass static compilation with entry list and tight allocation.
    // Only runs on warm reboot (sa_do_compile set by cache-pressure
    // reboot or Select+Start hold).  Cold boot skips compile and lets
    // the dynamic JIT handle compilation on demand.
    // Warm boots benefit from richer coverage data discovered by the
    // dynamic JIT during the previous session.
    //
    // Architecture overview:
    //   Bank 18 (BANK_ENTRY_LIST) holds a sequential table of 16-byte entries
    //   written during pass 1.  Each entry records: entry_pc, exit_pc,
    //   native_addr, code_bank, code_len, and the block's exit register
    //   state (A/X/Y val+known) for boundary-state seeding.
    //
    //   Pass 1: compile to RAM with max-size allocation.  Records actual
    //     code_len per block in the entry list.
    //
    //   Between passes: erase code + PC sectors.  Pre-populate ALL block
    //     entry_pc addresses into the PC tables using TIGHT allocation
    //     (code_len from pass 1).  This enables lookup_native_addr_safe
    //     to resolve both forward and backward branches in pass 2.
    //     Reset the allocator after pre-population.
    //
    //   Pass 2: recompile with tight allocation (sa_block_alloc_size =
    //     code_len).  The deterministic allocator replays the same
    //     sequence as pre-population → identical addresses.  Forward
    //     branches resolve via PC tables (pre-populated) instead of
    //     the entry list, enabling 2-byte or 5-byte direct branches.
    //
    // Why this works:
    //   - Pre-populate and pass 2 allocate in the same order with the
    //     same sizes → deterministic allocator produces identical addresses.
    //   - Pass 2 forces block termination at pass 1's exit_pc, so block
    //     boundaries match even when forward branches produce shorter code.
    //   - Pass 2 code_len <= pass 1 code_len (shorter branches), so the
    //     tight allocation always has sufficient space.
    //   - Tight allocation eliminates ~70% per-block waste vs max-stride.
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

#ifdef ENABLE_IR
            // Capture exit register state via IR optimizer shadow tracking.
            // ir_opt_redundant_load walks forward and snapshots A/X/Y into
            // ir_ctx.exit_* before each barrier reset.  The last snapshot
            // is the state just before the terminal branch/JMP.
            // Side effects (node kills) are harmless — pass 1 bytes are
            // already in cache_code and are never used from flash.
            if (ir_ctx.enabled) {
                // IR optimize passes run through WRAM trampoline
                // (same reason as pass 2 — can't bankswitch from
                // BANK_SA_CODE without losing our own code).
                extern void sa_ir_capture_exit(void);
                sa_ir_capture_exit();
                flash_byte_program(addr + 8,  BANK_ENTRY_LIST, ir_ctx.exit_a_val);
                flash_byte_program(addr + 9,  BANK_ENTRY_LIST, ir_ctx.exit_a_known);
                flash_byte_program(addr + 10, BANK_ENTRY_LIST, ir_ctx.exit_x_val);
                flash_byte_program(addr + 11, BANK_ENTRY_LIST, ir_ctx.exit_x_known);
                flash_byte_program(addr + 12, BANK_ENTRY_LIST, ir_ctx.exit_y_val);
                flash_byte_program(addr + 13, BANK_ENTRY_LIST, ir_ctx.exit_y_known);
            }
#endif
            // Bytes 14-15: reserved (left as 0xFF)
            entry_list_offset += ENTRY_LIST_STRIDE;
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
        if (bank == BANK_IR_OPT) continue;        // IR optimizer code
        for (uint16_t sector = FLASH_BANK_BASE; sector < FLASH_BANK_BASE + FLASH_BANK_SIZE; sector += FLASH_ERASE_SECTOR_SIZE)
            flash_sector_erase(sector, bank);
    }

    // Erase PC flag sectors (banks 27-30)
    for (uint8_t bank = BANK_PC_FLAGS; bank < BANK_PC_FLAGS + 4; bank++)
    {
        if (bank == BANK_IR_OPT) continue;    // IR optimizer code
        for (uint16_t sector = FLASH_BANK_BASE; sector < FLASH_BANK_BASE + FLASH_BANK_SIZE; sector += FLASH_ERASE_SECTOR_SIZE)
            flash_sector_erase(sector, bank);
    }

    flash_cache_init_sectors();

    // PRE-POPULATE REMOVED.
    //
    // The pre-populate phase previously walked the entry list, replayed
    // the allocator with flash_sector_alloc(code_len), and wrote entry-PC
    // addresses into the PC tables before pass 2.  This enabled forward
    // branch resolution during pass 2 (shorter 2/5-byte branches instead
    // of 21-byte patchable templates).
    //
    // Problem: nes_rom_data_copy allocates 256 bytes in the flash sector
    // during pass 2 compilation for indexed ROM reads (abs,X / abs,Y).
    // Pre-populate only replayed flash_sector_alloc and could not predict
    // these extra allocations.  After the first ROM data copy in pass 2,
    // every subsequent block's actual address diverged from the pre-
    // populated PC table entry — stale entries pointed into other blocks'
    // unpatched epilogues, causing JMP $FFFF crashes.
    //
    // Without pre-populate, pass 2 writes PC table entries as each block
    // compiles (addresses are always correct).  Forward branches use
    // 21-byte patchable templates (same as pass 1, so tight allocation
    // sizes match).  opt2_drain_static_patches resolves them post-compile.
    // Backward branches still resolve directly via lookup_native_addr_safe
    // (same iteration order — target already compiled).

    // =====================================================================
    // PASS 2: Recompile with tight allocation.
    //
    // Iterates the entry list (not the bitmap).  For each block:
    //   - Sets sa_block_exit_pc so the compile loop terminates at the
    //     same guest PC as pass 1 (prevents block boundary drift when
    //     shorter forward branches leave more room in the code buffer).
    //   - sa_block_alloc_size = code_len from entry list (tight alloc).
    //   - sa_compile_pass==2 tells recompile_opcode_b2() to attempt
    //     direct branches via lookup_native_addr_safe.  Backward targets
    //     resolve (already compiled); forward targets fall through to the
    //     21-byte patchable template (same as pass 1).
    //   - Writes code + headers to flash.  Writes PC tables fresh.
    //
    // Tight allocation eliminates the ~70% per-block waste of max-stride.
    // Forward branches are patched post-compile by opt2_drain_static_patches.
    // =====================================================================
    sa_compile_pass = 2;

    for (uint16_t i = 0; i < entry_list_offset; i += ENTRY_LIST_STRIDE)
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

        // Tight allocation: use pass 1's actual code_len
        {
            uint8_t p2_codelen = peek_bank_byte(BANK_ENTRY_LIST, addr + 7);
            sa_block_alloc_size = (p2_codelen > 0 && p2_codelen != 0xFF)
                                ? p2_codelen : 0;
        }

#ifdef ENABLE_IR
        // Boundary-state seeding: look up predecessor block whose exit_pc
        // matches this block's entry_pc.  Check the immediately preceding
        // entry first (O(1) for sequential fall-through blocks), then scan
        // the rest of the list for branch predecessors.
        {
            uint8_t seed_found = 0;
            // Fast path: check previous entry in list
            if (i >= ENTRY_LIST_STRIDE) {
                uint16_t prev_addr = FLASH_BANK_BASE + i - ENTRY_LIST_STRIDE;
                uint8_t pex_lo = peek_bank_byte(BANK_ENTRY_LIST, prev_addr + 2);
                uint8_t pex_hi = peek_bank_byte(BANK_ENTRY_LIST, prev_addr + 3);
                if (pex_lo == ep_lo && pex_hi == ep_hi) {
                    ir_ctx.seed_a_val   = peek_bank_byte(BANK_ENTRY_LIST, prev_addr + 8);
                    ir_ctx.seed_a_known = peek_bank_byte(BANK_ENTRY_LIST, prev_addr + 9);
                    ir_ctx.seed_x_val   = peek_bank_byte(BANK_ENTRY_LIST, prev_addr + 10);
                    ir_ctx.seed_x_known = peek_bank_byte(BANK_ENTRY_LIST, prev_addr + 11);
                    ir_ctx.seed_y_val   = peek_bank_byte(BANK_ENTRY_LIST, prev_addr + 12);
                    ir_ctx.seed_y_known = peek_bank_byte(BANK_ENTRY_LIST, prev_addr + 13);
                    seed_found = 1;
                }
            }
            // Slow path: scan entire list for any predecessor
            if (!seed_found) {
                for (uint16_t j = 0; j < entry_list_offset; j += ENTRY_LIST_STRIDE) {
                    if (j == i) continue;
                    uint16_t p_addr = FLASH_BANK_BASE + j;
                    uint8_t jex_lo = peek_bank_byte(BANK_ENTRY_LIST, p_addr + 2);
                    uint8_t jex_hi = peek_bank_byte(BANK_ENTRY_LIST, p_addr + 3);
                    if (jex_lo == ep_lo && jex_hi == ep_hi) {
                        ir_ctx.seed_a_val   = peek_bank_byte(BANK_ENTRY_LIST, p_addr + 8);
                        ir_ctx.seed_a_known = peek_bank_byte(BANK_ENTRY_LIST, p_addr + 9);
                        ir_ctx.seed_x_val   = peek_bank_byte(BANK_ENTRY_LIST, p_addr + 10);
                        ir_ctx.seed_x_known = peek_bank_byte(BANK_ENTRY_LIST, p_addr + 11);
                        ir_ctx.seed_y_val   = peek_bank_byte(BANK_ENTRY_LIST, p_addr + 12);
                        ir_ctx.seed_y_known = peek_bank_byte(BANK_ENTRY_LIST, p_addr + 13);
                        seed_found = 1;
                        break;
                    }
                }
            }
            // If no predecessor found, IR_INIT will zero the seed fields
        }
#endif

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
    sa_block_alloc_size = 0;

    // Final drain after all blocks are compiled.
    // Aggressively resolves pending patches and epilogues before execution.
#ifdef ENABLE_OPTIMIZER_V2
    opt2_drain_static_patches();
#endif

    // PC TABLE VERIFICATION PASS REMOVED.
    //
    // The verification pass was a band-aid for pre-populate/pass-2
    // divergence.  It wrote $00 to flags for stale entries.  But
    // flash_cache_pc_update_b17's guard (if current_flag != 0xFF return)
    // prevented the dynamic JIT from ever updating a $00 flag — causing
    // infinite recompile loops that filled the entire flash cache with
    // copies of the same block.  The root cause (pre-populate divergence
    // from nes_rom_data_copy) is fixed by removing pre-populate above.

    // Interpreted-flag repair pass REMOVED.
    //
    // The old pass scanned the SA bitmap and wrote $80 (INTERPRETED) to
    // every known-code PC still at $FF after pass 2.  This was intended
    // to avoid a one-time flash_sector_alloc cost (~7K cycles) for PCs
    // whose first instruction is always-interpreted (CLI/SEI/I/O).
    //
    // Problem: in IR mode, pass 2 intentionally leaves mid-block PCs at
    // $FF so they can be published with correct post-lowering native
    // offsets.  The repair pass clobbered those $FF slots with $80,
    // permanently trapping compilable mid-block code in interpret mode.
    // For DK, this affected 43 PCs (~33% of all dispatches).
    //
    // The dynamic JIT handles both cases correctly at runtime:
    //   - Always-interpreted PCs: compile loop writes INTERPRETED flag,
    //     undoes flash allocation — one-time ~7K cycle cost per PC.
    //   - Normal code PCs: compiles and writes RECOMPILED flag.

    // Tight allocation: pass 2 allocates each block using its actual
    // code_len from pass 1 (set via sa_block_alloc_size).  All PC table
    // entries are pre-populated with tight addresses between passes, so
    // forward branches resolve correctly via lookup_native_addr_safe.
    // sector_free_offset values now reflect actual usage — no wasted gaps.
    //
    // Old max-stride approach (REMOVED) wasted ~70% of flash cache space
    // because every block got CODE_SIZE+EPILOGUE allocation regardless of
    // actual code size.  The compaction sweep attempted to reclaim the
    // tail gap but caused AND-corruption when dynamic JIT allocated into
    // the gaps.  Tight allocation eliminates the gaps entirely.

    // Signal that SA two-pass compile ran to completion.  The cache-
    // pressure auto-reset in check_recompile_triggers reads this to
    // avoid endlessly cycling: SA compile → dynamic fill → pressure
    // → soft reset → SA compile → ...
    {
        extern volatile uint8_t sa_compile_completed;
        sa_compile_completed = 1;
    }

  } // if (sa_do_compile)
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
// Moved to BANK_SA_CODE to free bank2 space.  Called from bank2 via
// fixed-bank trampoline (compilation speed is not critical).
// Returns SA_SUB_CLEAN, SA_SUB_DIRTY, or SA_SUB_EMPTY (not found).
// -------------------------------------------------------------------------
#ifdef PLATFORM_NES
#pragma section bank19
#else
#pragma section bank24
#endif

static uint8_t sa_subroutine_lookup_impl(uint16_t target_pc)
{
#ifdef FORCE_DIRTY_SUBS
    // Per-game blacklist: force-dirty specific addresses that the
    // heuristic might miss.  Checked first for fast bail-out.
    {
        static const uint16_t force_dirty[] = { FORCE_DIRTY_SUBS };
        for (uint8_t i = 0; force_dirty[i] != 0; i++) {
            if (target_pc == force_dirty[i])
                return SA_SUB_DIRTY;
        }
    }
#endif

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

#pragma section default

uint8_t sa_subroutine_lookup(uint16_t target_pc)
{
    uint8_t saved_bank = mapper_prg_bank;
    bankswitch_prg(BANK_SA_CODE);
    uint8_t result = sa_subroutine_lookup_impl(target_pc);
    bankswitch_prg(saved_bank);
    return result;
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
#ifdef ENABLE_AUTO_IDLE_DETECT
    // Load the flash idle list into a WRAM cache for fast runtime lookup.
    // Must happen after sa_run_b2 (which populates the flash table) and
    // before the main dispatch loop starts.  sa_load_idle_cache reads
    // flash via peek_bank_byte (WRAM) — no bank dependency.
    sa_load_idle_cache();
#endif
#ifdef ENABLE_METRICS
    bankswitch_prg(BANK_METRICS);
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

// -------------------------------------------------------------------------
// sa_is_idle_pc — fixed-bank idle PC lookup.
// Checks the WRAM-cached idle table and the compile-time GAME_IDLE_PC.
// Called from the dispatch loop and main loop — must be fast.
// The WRAM cache (sa_idle_cache) is populated by sa_load_idle_cache()
// which runs once at the end of sa_run().
// -------------------------------------------------------------------------
#ifdef ENABLE_AUTO_IDLE_DETECT

// WRAM cache of auto-detected idle PCs — loaded once from flash,
// checked every dispatch iteration.  Avoids per-dispatch flash reads.
// Non-static so metrics.c can read them for the WRAM metrics dump.
uint16_t sa_idle_cache[SA_IDLE_MAX];
uint8_t  sa_idle_cache_count = 0;

// Load the flash idle list into the WRAM cache.
// Called once at the end of sa_run().
void sa_load_idle_cache(void)
{
    sa_idle_cache_count = 0;

#ifdef GAME_IDLE_PC
    // Prepend the manual override so it's checked first
    sa_idle_cache[sa_idle_cache_count++] = GAME_IDLE_PC;
#endif

    // Load auto-detected entries from flash
    for (uint8_t i = 0; i < SA_IDLE_MAX && sa_idle_cache_count < SA_IDLE_MAX; i++)
    {
        uint16_t entry_addr = SA_IDLE_BASE + i * SA_IDLE_ENTRY_SIZE;
        uint8_t lo = peek_bank_byte(BANK_FLASH_BLOCK_FLAGS, entry_addr + 0);
        uint8_t hi = peek_bank_byte(BANK_FLASH_BLOCK_FLAGS, entry_addr + 1);
        if (lo == 0xFF && hi == 0xFF)
            break;   // end of list

        uint16_t idle_pc = lo | ((uint16_t)hi << 8);

        // Skip duplicates (e.g. if GAME_IDLE_PC was also auto-detected)
        uint8_t dup = 0;
        for (uint8_t j = 0; j < sa_idle_cache_count; j++) {
            if (sa_idle_cache[j] == idle_pc) { dup = 1; break; }
        }
        if (!dup)
            sa_idle_cache[sa_idle_cache_count++] = idle_pc;
    }
}

uint8_t sa_is_idle_pc(uint16_t addr)
{
    for (uint8_t i = 0; i < sa_idle_cache_count; i++)
    {
        if (sa_idle_cache[i] == addr)
            return 1;
    }
    return 0;
}
#endif // ENABLE_AUTO_IDLE_DETECT

// sa_record_subroutine_runtime — moved to WRAM assembly stub in
// dynamos-asm.s to save fixed-bank space.  The asm version does the
// same save-bank / bankswitch(BANK_SA_CODE) / call / restore pattern
// but avoids the vbcc __rsave12/__rload12 prologue overhead.

#endif // ENABLE_STATIC_ANALYSIS
