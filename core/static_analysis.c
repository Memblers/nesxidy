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
extern uint8_t cache_code[BLOCK_COUNT][CODE_SIZE];
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
extern uint8_t flash_cache_pc[];
extern const uint8_t flash_cache_pc_flags[];
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

// Forward declaration: sa_record_subroutine is in the fixed bank section
// but called from sa_walk_b2 (bank2).  Bank2 at $8000 can call fixed at $C000+.
void sa_record_subroutine(uint16_t target);

#pragma section bank2

// -------------------------------------------------------------------------
// Opcode length helper (bank2) — only called from sa_walk_b2.
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

    while (addr <= ROM_ADDR_MAX)
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

#pragma section default

// -------------------------------------------------------------------------
// Compile a single block from the fixed bank.
// Called repeatedly by the sa_run trampoline during batch compilation.
// Returns 1 if a block was compiled, 0 if nothing to do.
// -------------------------------------------------------------------------

#ifdef ENABLE_STATIC_COMPILE

uint16_t sa_blocks_total = 0;

static uint8_t sa_compile_one_block(void)
{
#ifdef ENABLE_COMPILE_PPU_EFFECT
    // Toggle green emphasis (bit 6) per block compiled
    compile_ppu_effect ^= 0x40;
    lnPPUMASK = 0x3B | compile_ppu_effect;
    *(volatile uint8_t*)0x2001 = lnPPUMASK;  // mid-frame
#endif

    // Allocate space in a flash sector for max-size block.
    // flash_sector_alloc sets flash_code_bank and flash_code_address (header start).
    if (!flash_sector_alloc(CODE_SIZE + EPILOGUE_SIZE + XBANK_EPILOGUE_SIZE))
        return 0;           // cache full

    uint16_t entry_pc = pc;

    // Use cache index 0 as temporary compilation buffer (same as run_6502)
    cache_index = 0;
    code_index = 0;
    cache_flag[0] = 0;
    block_has_jsr = 0;  // reset for new block

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

        // Record this instruction's code_index for intra-block branch lookup.
        {
            uint8_t ci_slot = (uint8_t)(pc_old - entry_pc) & 0x3F;
            block_ci_map[ci_slot] = code_index_old + 1;
        }

        recompile_opcode();

        uint8_t instr_len = code_index - code_index_old;

        setup_flash_pc_tables(pc_old);
        for (uint8_t i = 0; i < instr_len; i++)
        {
            flash_byte_program(flash_code_address + BLOCK_HEADER_SIZE + code_index_old + i,
                               flash_code_bank, cache_code[0][code_index_old + i]);
        }

        if (code_index > (CODE_SIZE - 6))
        {
            cache_flag[0] |= OUT_OF_CACHE;
            cache_flag[0] &= ~READY_FOR_NEXT;
        }

        if (cache_flag[0] & INTERPRET_NEXT_INSTRUCTION)
            flash_cache_pc_update(code_index_old, INTERPRETED);
        else if (instr_len || pc != pc_old)
        {
            flash_cache_pc_update(code_index_old, RECOMPILED);
#ifdef ENABLE_OPTIMIZER_V2
            opt2_notify_block_compiled(pc_old,
                flash_code_address + BLOCK_HEADER_SIZE + code_index_old,
                flash_code_bank);
#endif
        }

    } while (cache_flag[0] & READY_FOR_NEXT);

    if (code_index)
    {
        uint16_t exit_pc = pc;
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

        // --- Write block header to flash ---
        flash_byte_program(flash_code_address + 0, flash_code_bank, (uint8_t)entry_pc);
        flash_byte_program(flash_code_address + 1, flash_code_bank, (uint8_t)(entry_pc >> 8));
        flash_byte_program(flash_code_address + 2, flash_code_bank, (uint8_t)exit_pc);
        flash_byte_program(flash_code_address + 3, flash_code_bank, (uint8_t)(exit_pc >> 8));
        flash_byte_program(flash_code_address + 4, flash_code_bank, code_index);  // code_len
        flash_byte_program(flash_code_address + 5, flash_code_bank, epilogue_start);  // epilogue_offset
        // +6 flags, +7 reserved: leave as 0xFF (erased)

        // --- Write code + epilogue to flash ---
        for (uint8_t i = epilogue_start; i < code_index; i++)
        {
            flash_byte_program(flash_code_address + BLOCK_HEADER_SIZE + i,
                               flash_code_bank, cache_code[0][i]);
        }

        // Shrink sector allocation to actual size used (avoid wasting space)
        sector_free_offset[next_free_sector] = (flash_code_address & FLASH_SECTOR_MASK) + BLOCK_HEADER_SIZE + code_index;
    }

    return 1;
}

#endif // ENABLE_STATIC_COMPILE

#pragma section default

// -------------------------------------------------------------------------
// Subroutine table helpers (fixed bank) — record JSR targets and analyze
// stack safety.  These live in the fixed bank ($C000+) and are called
// from bank2 code (BFS walker) and from sa_run (fixed bank).
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
// -------------------------------------------------------------------------

void sa_run(void)
{
    // Save pc — reset6502() has already set it to the entry point.
    // The batch compile pass uses pc as scratch, so we must restore it.
    uint16_t saved_pc = pc;

#ifdef ENABLE_COMPILE_PPU_EFFECT
    // Enable PPU compile effect for boot-time visual feedback
    compile_ppu_active = 1;
#endif

    // Run BFS walker (bank 2) — includes header check and sector erase
    uint8_t saved_bank = mapper_prg_bank;
    bankswitch_prg(2);
    sa_walk_b2();
    bankswitch_prg(saved_bank);

    // Analyze stack safety for all discovered subroutines.
    // Runs from fixed bank; uses peek_bank_byte/flash_byte_program
    // for flash access and read6502 for ROM reads.
    sa_analyze_all_subroutines();

#ifdef ENABLE_STATIC_COMPILE
    // Batch compilation: scan bitmap from fixed bank, compile each hit.
    // All bitmap/flag reads use peek_bank_byte (WRAM helper), so this
    // runs entirely from the fixed bank — no bank2 trampoline needed.
    // Reservation system disabled — no reservation list to check.
    for (uint16_t scan_cursor = ROM_ADDR_MIN; scan_cursor <= ROM_ADDR_MAX; scan_cursor++)
    {
        // Check bitmap: is this address known code?
        if (sa_bitmap_is_unknown(scan_cursor))
            continue;

        // Check PC flags: already compiled or interpreted?
        uint8_t flag = peek_bank_byte(
            (scan_cursor >> 14) + BANK_PC_FLAGS,
            (uint16_t)&flash_cache_pc_flags[scan_cursor & FLASH_BANK_MASK]);
        if (flag != 0xFF)
            continue;   // already handled

        // Compile this address
        pc = scan_cursor;
        if (!sa_compile_one_block())
            break;              // cache full
        sa_blocks_total++;

#ifdef ENABLE_OPTIMIZER_V2
        // Periodic sweep: drain pending-branch queue only.
        // This is safe — each block is fully compiled (code + epilogue +
        // flags) before we get here.  We MUST drain periodically because
        // the queue is only 32 entries and batch compile generates many
        // branches.
        //
        // Epilogue scanning is deliberately NOT done here.  The epilogue
        // scanner patches the BCC offset (4→0) to enable the fast-JMP
        // path.  During batch compile this caused corrupt graphics —
        // likely because the scanner's static cursor and batch-size
        // interact poorly with partially-filled flash banks.  The final
        // full epilogue sweep after the loop is sufficient.
        static uint8_t sa_blocks_compiled = 0;
        if (++sa_blocks_compiled >= 4)
        {
            sa_blocks_compiled = 0;
            opt2_sweep_pending_patches();
        }
#endif
    }

    // Disable new reservations before draining — compiling a drained
    // block's branches/JMPs would otherwise create more reservations,
    // cascading until all flash blocks are consumed.
    reservations_enabled = 0;

    // Drain any unfulfilled reservations.  Forward targets the scan
    // didn't reach (cache full) or edge cases leave reservations
    // unconsumed — the direct JMP already points at the reserved
    // flash block, so we MUST compile code there.
    while (reservation_count > 0)
    {
        pc = reserved_pc[0];   // consume_reservation will remove [0]
        if (!sa_compile_one_block())
            break;             // cache full
        sa_blocks_total++;
    }

    // Final sweep after all blocks are compiled.
    // Sequence: pending → full epilogue scan → pending again.
    // The second pending sweep catches any branches whose targets were
    // in different banks during compile but got resolved by epilogue
    // chaining.  Two epilogue passes ensure ordering doesn't matter.
#ifdef ENABLE_OPTIMIZER_V2
    opt2_sweep_pending_patches();
#ifdef ENABLE_PATCHABLE_EPILOGUE
    for (uint8_t pass = 0; pass < 2; pass++)
    {
        // Scan all sectors — each call processes EPILOGUE_SCAN_BATCH blocks,
        // so iterate enough times to cover all 60 sectors generously.
        for (uint16_t s = 0; s < (FLASH_CACHE_SECTORS + 31) / 32; s++)
            opt2_scan_and_patch_epilogues();
        opt2_sweep_pending_patches();
    }
#endif
#endif

#endif // ENABLE_STATIC_COMPILE

    // Restore normal PPU rendering after static analysis
#ifdef ENABLE_COMPILE_PPU_EFFECT
    compile_ppu_active = 0;
    compile_ppu_effect = 0;
    lnPPUMASK = 0x3A;
    *(volatile uint8_t*)0x2001 = lnPPUMASK;  // mid-frame restore
#endif

    // Restore pc — reset6502() set it to the game's entry point before
    // sa_run() was called.  The batch compile pass clobbered it.
    pc = saved_pc;
}

// -------------------------------------------------------------------------
// Subroutine lookup — check if a JSR target is stack-clean.
// Lives in the fixed bank so it can be called from recompile_opcode
// in any bank context.  Returns SA_SUB_CLEAN, SA_SUB_DIRTY, or
// SA_SUB_EMPTY (not found).
// -------------------------------------------------------------------------

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
// execution.  Called from the interpreter.  Lives in the fixed bank so
// it can be called from any context.
// -------------------------------------------------------------------------

void sa_record_indirect_target(uint16_t target_pc, uint8_t type)
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

#endif // ENABLE_STATIC_ANALYSIS
