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

// From dynamos.c — needed for batch compile
__zpage extern uint16_t pc;
__zpage extern uint8_t code_index;
__zpage extern uint8_t cache_index;
extern uint8_t cache_flag[];
extern uint8_t cache_code[BLOCK_COUNT][CODE_SIZE];
extern uint16_t flash_cache_select(void);
extern void setup_flash_address(uint16_t emulated_pc, uint16_t block_number);
extern void flash_cache_pc_update(uint8_t code_address, uint8_t flags);
extern uint8_t recompile_opcode(void);
__zpage extern uint16_t flash_cache_index;
extern uint16_t flash_code_address;
extern uint8_t flash_code_bank;

extern uint8_t flash_block_flags[];
extern uint8_t flash_cache_pc[];
extern const uint8_t flash_cache_pc_flags[];
__zpage extern uint8_t a;
extern void flash_dispatch_return(void);

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

#pragma section default

// Shorthand macros for the flash addresses of the bank3 variables.
// These resolve to the linker-assigned addresses at compile time.
#define SA_BITMAP_BASE   ((uint16_t)&sa_code_bitmap[0])
#define SA_HEADER_BASE   ((uint16_t)&sa_header[0])
#define SA_INDIRECT_BASE ((uint16_t)&sa_indirect_list[0])

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
        {
            uint16_t base = SA_BITMAP_BASE & 0xF000;
            uint16_t end  = (SA_INDIRECT_BASE + SA_INDIRECT_MAX * 3 - 1) | 0x0FFF;
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

    // BFS loop
    while (!q_empty())
    {
        uint16_t cur_pc = q_pop();

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
    flash_cache_index = flash_cache_select();
    if (!flash_cache_index)
        return 0;           // cache full
    flash_cache_index--;

    // Use cache index 0 as temporary compilation buffer (same as run_6502)
    cache_index = 0;
    code_index = 0;
    cache_flag[0] = 0;

    setup_flash_address(pc, flash_cache_index);

    // Compile loop — same as in run_6502 but without dispatch
    do
    {
        uint16_t pc_old = pc;
        uint8_t code_index_old = code_index;

        recompile_opcode();

        uint8_t instr_len = code_index - code_index_old;

        setup_flash_address(pc_old, flash_cache_index);
        for (uint8_t i = 0; i < instr_len; i++)
        {
            flash_byte_program(flash_code_address + code_index_old + i,
                               flash_code_bank, cache_code[0][code_index_old + i]);
        }

        if (code_index > (CODE_SIZE - 6))
        {
            cache_flag[0] |= OUT_OF_CACHE;
            cache_flag[0] &= ~READY_FOR_NEXT;
        }

        if (cache_flag[0] & INTERPRET_NEXT_INSTRUCTION)
            flash_cache_pc_update(code_index_old, INTERPRETED);
        else if (instr_len)
        {
            flash_cache_pc_update(code_index_old, RECOMPILED);
#ifdef ENABLE_OPTIMIZER_V2
            opt2_notify_block_compiled(pc_old,
                flash_code_address + code_index_old + BLOCK_PREFIX_SIZE,
                flash_code_bank);
#endif
        }

    } while (cache_flag[0] & READY_FOR_NEXT);

    if (code_index)
    {
        uint16_t exit_pc = pc;
        setup_flash_address(pc, flash_cache_index);
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
        cache_code[0][code_index++] = 0x4C;     // JMP dispatch_return
        cache_code[0][code_index++] = (uint8_t)((uint16_t)&flash_dispatch_return);
        cache_code[0][code_index++] = (uint8_t)(((uint16_t)&flash_dispatch_return) >> 8);
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
        cache_code[0][code_index++] = 0x4C;     // JMP dispatch_return
        cache_code[0][code_index++] = (uint8_t)((uint16_t)&flash_dispatch_return);
        cache_code[0][code_index++] = (uint8_t)(((uint16_t)&flash_dispatch_return) >> 8);
#endif

        for (uint8_t i = epilogue_start; i < code_index; i++)
        {
            flash_byte_program(flash_code_address + i, flash_code_bank,
                               cache_code[0][i]);
        }
#ifdef ENABLE_PATCHABLE_EPILOGUE
        flash_byte_program(flash_code_address + 255, flash_code_bank,
                           epilogue_start);
#endif

        // Mark block as used
        bankswitch_prg(BANK_FLASH_BLOCK_FLAGS);
        flash_byte_program(
            (uint16_t)&flash_block_flags[0] + flash_cache_index,
            mapper_prg_bank,
            flash_block_flags[flash_cache_index] & ~FLASH_AVAILABLE);
    }

    return 1;
}

#endif // ENABLE_STATIC_COMPILE

#pragma section default

// -------------------------------------------------------------------------
// Main entry point — fixed-bank trampoline
// -------------------------------------------------------------------------

void sa_run(void)
{
    // Save pc — reset6502() has already set it to the entry point.
    // The batch compile pass uses pc as scratch, so we must restore it.
    uint16_t saved_pc = pc;

    // Run BFS walker (bank 2) — includes header check and sector erase
    uint8_t saved_bank = mapper_prg_bank;
    bankswitch_prg(2);
    sa_walk_b2();
    bankswitch_prg(saved_bank);

#ifdef ENABLE_STATIC_COMPILE
    // Batch compilation: scan bitmap from fixed bank, compile each hit.
    // All bitmap/flag reads use peek_bank_byte (WRAM helper), so this
    // runs entirely from the fixed bank — no bank2 trampoline needed.
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
        if (++sa_blocks_compiled >= 8)
        {
            sa_blocks_compiled = 0;
            opt2_sweep_pending_patches();
        }
#endif
    }

    // Final sweep after all blocks are compiled
#ifdef ENABLE_OPTIMIZER_V2
    opt2_sweep_pending_patches();
#ifdef ENABLE_PATCHABLE_EPILOGUE
    // Full epilogue scan — cover ALL compiled blocks, not just one
    // EPILOGUE_SCAN_BATCH.  FLASH_CACHE_BLOCKS/32 calls to scan them all,
    // rounded up.
    for (uint16_t s = 0; s < (FLASH_CACHE_BLOCKS + 31) / 32; s++)
        opt2_scan_and_patch_epilogues();
#endif
#endif

#endif // ENABLE_STATIC_COMPILE

    // Restore pc — reset6502() set it to the game's entry point before
    // sa_run() was called.  The batch compile pass clobbered it.
    pc = saved_pc;
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
