/**
 * ir_opt.c - IR optimization passes
 *
 * All passes operate on the linear ir_nodes[] array in place.
 * Dead nodes get op = IR_DEAD (skipped during lowering).
 * Passes are individually gatable via config.h defines.
 *
 * Port of patterns from opt65.c (opti1/opti2/opti3) and
 * peephole_patterns.txt, adapted for the IR node representation.
 *
 * Lives in BANK_IR_OPT:
 *   NES:   bank 28 (dead flag-table for $4000-$7FFF)
 *   Exidy: bank 29 (dead flag-table for $8000-$BFFF)
 */

#ifdef PLATFORM_NES
#pragma section bank28
#else
#pragma section bank29
#endif

#include <stdint.h>
#include "ir.h"
#include "../config.h"

/* ===================================================================
 * Helper: kill (delete) a node
 * =================================================================== */
static void ir_kill(ir_ctx_t *ctx, uint8_t idx)
{
    ctx->nodes[idx].op = IR_DEAD;
    ctx->nodes[idx].flags = 0;
    ctx->nodes[idx].operand = 0;
}

/* ===================================================================
 * Helper: check if an IR opcode writes to A
 * =================================================================== */
static uint8_t writes_a(uint8_t op)
{
    switch (op) {
        case IR_LDA_IMM: case IR_LDA_ZP: case IR_LDA_ABS:
        case IR_LDA_ABSX: case IR_LDA_ABSY:
        case IR_PLA: case IR_TXA: case IR_TYA:
        case IR_ADC_IMM: case IR_SBC_IMM:
        case IR_AND_IMM: case IR_ORA_IMM: case IR_EOR_IMM:
        case IR_ADC_ZP: case IR_SBC_ZP:
        case IR_AND_ZP: case IR_ORA_ZP: case IR_EOR_ZP:
        case IR_ADC_ABS: case IR_SBC_ABS:
        case IR_AND_ABS: case IR_ORA_ABS: case IR_EOR_ABS:
        case IR_ADC_ABSX: case IR_SBC_ABSX:
        case IR_AND_ABSX: case IR_ORA_ABSX: case IR_EOR_ABSX:
        case IR_ADC_ABSY: case IR_SBC_ABSY:
        case IR_AND_ABSY: case IR_ORA_ABSY: case IR_EOR_ABSY:
        case IR_ASL_A: case IR_LSR_A: case IR_ROL_A: case IR_ROR_A:
            return 1;
    }
    return 0;
}

/* Helper: check if an IR opcode reads A */
static uint8_t reads_a(uint8_t op)
{
    switch (op) {
        case IR_STA_ZP: case IR_STA_ABS: case IR_STA_ABSX: case IR_STA_ABSY:
        case IR_PHA: case IR_TAX: case IR_TAY:
        case IR_ADC_IMM: case IR_SBC_IMM:
        case IR_AND_IMM: case IR_ORA_IMM: case IR_EOR_IMM:
        case IR_CMP_IMM:
        case IR_ADC_ZP: case IR_SBC_ZP:
        case IR_AND_ZP: case IR_ORA_ZP: case IR_EOR_ZP:
        case IR_CMP_ZP:
        case IR_ADC_ABS: case IR_SBC_ABS:
        case IR_AND_ABS: case IR_ORA_ABS: case IR_EOR_ABS:
        case IR_CMP_ABS:
        case IR_ADC_ABSX: case IR_SBC_ABSX:
        case IR_AND_ABSX: case IR_ORA_ABSX: case IR_EOR_ABSX:
        case IR_CMP_ABSX:
        case IR_ADC_ABSY: case IR_SBC_ABSY:
        case IR_AND_ABSY: case IR_ORA_ABSY: case IR_EOR_ABSY:
        case IR_CMP_ABSY:
        case IR_ASL_A: case IR_LSR_A: case IR_ROL_A: case IR_ROR_A:
        case IR_BIT_ZP: case IR_BIT_ABS:
        case IR_TEMPLATE:  /* conservative: treat templates as touching A */
            return 1;
    }
    return 0;
}

/* Helper: check if an IR opcode writes X */
static uint8_t writes_x(uint8_t op)
{
    switch (op) {
        case IR_LDX_IMM: case IR_LDX_ZP: case IR_LDX_ABS: case IR_LDX_ABSY:
        case IR_TAX: case IR_TSX:
        case IR_INX: case IR_DEX:
            return 1;
    }
    return 0;
}

/* Helper: check if an IR opcode reads X */
static uint8_t reads_x(uint8_t op)
{
    switch (op) {
        case IR_STX_ZP: case IR_STX_ABS:
        case IR_TXA: case IR_TXS:
        case IR_INX: case IR_DEX:
        case IR_CPX_IMM: case IR_CPX_ZP: case IR_CPX_ABS:
        /* indexed-X addressing modes use X implicitly */
        case IR_LDA_ABSX: case IR_STA_ABSX:
        case IR_ADC_ABSX: case IR_SBC_ABSX:
        case IR_AND_ABSX: case IR_ORA_ABSX: case IR_EOR_ABSX:
        case IR_CMP_ABSX:
        case IR_LDY_ABSX:
        case IR_INC_ABSX: case IR_DEC_ABSX:
        case IR_ASL_ABSX: case IR_LSR_ABSX:
        case IR_ROL_ABSX: case IR_ROR_ABSX:
        case IR_TEMPLATE:  /* conservative */
            return 1;
    }
    return 0;
}

/* Helper: check if an IR opcode writes Y */
static uint8_t writes_y(uint8_t op)
{
    switch (op) {
        case IR_LDY_IMM: case IR_LDY_ZP: case IR_LDY_ABS: case IR_LDY_ABSX:
        case IR_TAY:
        case IR_INY: case IR_DEY:
            return 1;
    }
    return 0;
}

/* Helper: check if an IR opcode reads Y */
static uint8_t reads_y(uint8_t op)
{
    switch (op) {
        case IR_STY_ZP: case IR_STY_ABS:
        case IR_TYA:
        case IR_INY: case IR_DEY:
        case IR_CPY_IMM: case IR_CPY_ZP: case IR_CPY_ABS:
        /* indexed-Y addressing modes use Y implicitly */
        case IR_LDA_ABSY: case IR_STA_ABSY:
        case IR_ADC_ABSY: case IR_SBC_ABSY:
        case IR_AND_ABSY: case IR_ORA_ABSY: case IR_EOR_ABSY:
        case IR_CMP_ABSY:
        case IR_LDX_ABSY:
        case IR_TEMPLATE:  /* conservative */
            return 1;
    }
    return 0;
}

/* Helper: check if an IR opcode writes flags (P register) */
static uint8_t writes_flags(uint8_t op)
{
    /* Most ALU/load/compare ops set flags.  Stores, transfers to
     * memory (STA/STX/STY), and PHP/PLP have special handling. */
    switch (op) {
        case IR_STA_ZP: case IR_STA_ABS: case IR_STA_ABSX: case IR_STA_ABSY:
        case IR_STX_ZP: case IR_STX_ABS:
        case IR_STY_ZP: case IR_STY_ABS:
        case IR_PHA:
        case IR_TXS:
        case IR_JMP_ABS: case IR_JSR: case IR_RTS:
        case IR_NOP:
        case IR_RAW_BYTE: case IR_RAW_WORD: case IR_RAW_OP_ABS:
        case IR_TEMPLATE:
        case IR_DEAD:
            return 0;
        default:
            return 1;  /* Most instructions do set flags */
    }
}

/* Helper: check if an IR opcode reads flags */
static uint8_t reads_flags(uint8_t op)
{
    switch (op) {
        case IR_ADC_IMM: case IR_SBC_IMM:
        case IR_ADC_ZP: case IR_SBC_ZP:
        case IR_ADC_ABS: case IR_SBC_ABS:
        case IR_ADC_ABSX: case IR_SBC_ABSX:
        case IR_ADC_ABSY: case IR_SBC_ABSY:
        case IR_ROL_A: case IR_ROR_A:
        case IR_ROL_ZP: case IR_ROR_ZP:
        case IR_ROL_ABS: case IR_ROR_ABS:
        case IR_ROL_ABSX: case IR_ROR_ABSX:
        case IR_PLP:
        case IR_BPL: case IR_BMI: case IR_BVC: case IR_BVS:
        case IR_BCC: case IR_BCS: case IR_BNE: case IR_BEQ:
            return 1;
    }
    return 0;
}

/* Helper: does this template write to arbitrary memory?
 * STA (ZP),Y can write to any address — ZP/ABS shadow caches
 * must be flushed but register shadow can use the flags. */
static uint8_t template_writes_memory(ir_node_t *n)
{
    if (n->op != IR_TEMPLATE) return 0;
    switch ((uint8_t)n->operand) {
        case IR_TMPL_STA_INDY:
        case IR_TMPL_NATIVE_STA_INDY:
            return 1;
    }
    return 0;
}

/* Helper: is this node a full optimization barrier?
 * Combines is_opaque, is_branch, and control-flow templates. */
static uint8_t is_barrier(ir_node_t *n)
{
    uint8_t op = n->op;
    switch (op) {
        case IR_JSR: case IR_RTS: case IR_JMP_ABS:
        case IR_RAW_BYTE: case IR_RAW_WORD: case IR_RAW_OP_ABS:
        case IR_BPL: case IR_BMI: case IR_BVC: case IR_BVS:
        case IR_BCC: case IR_BCS: case IR_BNE: case IR_BEQ:
        case IR_PC_MARK:  /* mid-block fence — resets optimizer state */
            return 1;
        case IR_TEMPLATE:
            switch ((uint8_t)n->operand) {
                case IR_TMPL_JSR: case IR_TMPL_NJSR: case IR_TMPL_NRTS:
                case IR_TMPL_BRANCH_PATCHABLE: case IR_TMPL_JMP_PATCHABLE:
                    return 1;
            }
    }
    return 0;
}

/* Helper: does this instruction write to a ZP memory location? */
static uint8_t writes_to_zp(uint8_t op)
{
    switch (op) {
        case IR_STA_ZP: case IR_STX_ZP: case IR_STY_ZP:
        case IR_INC_ZP: case IR_DEC_ZP:
        case IR_ASL_ZP: case IR_LSR_ZP:
        case IR_ROL_ZP: case IR_ROR_ZP:
            return 1;
    }
    return 0;
}

/* Helper: does this instruction write to an absolute memory location? */
static uint8_t writes_to_abs(uint8_t op)
{
    switch (op) {
        case IR_STA_ABS: case IR_STX_ABS: case IR_STY_ABS:
        case IR_INC_ABS: case IR_DEC_ABS:
        case IR_ASL_ABS: case IR_LSR_ABS:
        case IR_ROL_ABS: case IR_ROR_ABS:
            return 1;
    }
    return 0;
}

/* ===================================================================
 * Pass 1: Redundant load elimination
 * (from opt65.c opti1 / peephole_patterns.txt Phase 1)
 *
 * Forward scan tracking last-known register values.
 * Kill LDA/LDX/LDY #imm if register already holds that value
 * and the instruction's flag side-effects are unused.
 * =================================================================== */

/* Helper: are flags consumed before the next flag-writing node?
 * Returns 1 if flags are NOT needed (safe to kill). */
static uint8_t flags_safe(ir_ctx_t *ctx, uint8_t start)
{
    uint8_t j;
    for (j = start; j < ctx->node_count; j++) {
        ir_node_t *n = &ctx->nodes[j];
        if (n->op == IR_DEAD) continue;
        if (reads_flags(n->op) || (n->op == IR_TEMPLATE && (n->flags & 0x08))) return 0;
        if (writes_flags(n->op) || (n->op == IR_TEMPLATE && (n->flags & 0x80))) return 1;
    }
    return 1;
}

/* Scratch variables for ir_opt_redundant_load — placed in BSS (WRAM)
 * via #pragma section default, instead of the software stack.
 * Under a banked section pragma, static locals go to ROM (flash),
 * where writes trigger flash programming — completely wrong.
 * These MUST be in a RAM-backed section.
 *
 * The function is only called during JIT compilation (single-threaded,
 * never re-entrant), so shared scratch is safe. */
#pragma section default
static uint8_t addr8;
static uint16_t addr16;
static uint8_t reg_known, reg_val, s, slot, val, imm;
#ifdef PLATFORM_NES
#pragma section bank28
#else
#pragma section bank29
#endif

uint8_t ir_opt_redundant_load(ir_ctx_t *ctx)
{
    uint8_t changes = 0;
    ir_reg_shadow_t *r = &ctx->regs;

    /* Reset ALL shadow state — ZP/ABS caches must be cleared because
     * ir_optimize() calls this pass in a loop; stale cache entries
     * from a previous iteration would poison the scan.
     * A/X/Y are seeded from predecessor block's exit state (boundary-
     * state seeding); defaults to 0 (unknown) when no predecessor. */
    r->a_val = ctx->seed_a_val;   r->a_known = ctx->seed_a_known;
    r->x_val = ctx->seed_x_val;   r->x_known = ctx->seed_x_known;
    r->y_val = ctx->seed_y_val;   r->y_known = ctx->seed_y_known;
    r->zp_known[0] = r->zp_known[1] = r->zp_known[2] = r->zp_known[3] = 0;
    r->abs_known[0] = r->abs_known[1] = r->abs_known[2] = r->abs_known[3] = 0;

    for (uint8_t i = 0; i < ctx->node_count; i++) {
        ir_node_t *n = &ctx->nodes[i];
        if (n->op == IR_DEAD) continue;

        /* --- Redundant immediate loads (A/X/Y) --- */
        if ((n->op == IR_LDA_IMM && r->a_known && r->a_val == (uint8_t)n->operand) ||
            (n->op == IR_LDX_IMM && r->x_known && r->x_val == (uint8_t)n->operand) ||
            (n->op == IR_LDY_IMM && r->y_known && r->y_val == (uint8_t)n->operand)) {
            if (flags_safe(ctx, i + 1)) { ir_kill(ctx, i); changes++; continue; }
        }

        /* --- Identity-op elimination: AND #$FF, ORA #$00, EOR #$00 --- */
        if ((n->op == IR_AND_IMM && (uint8_t)n->operand == 0xFF) ||
            (n->op == IR_ORA_IMM && (uint8_t)n->operand == 0x00) ||
            (n->op == IR_EOR_IMM && (uint8_t)n->operand == 0x00)) {
            if (flags_safe(ctx, i + 1)) { ir_kill(ctx, i); changes++; continue; }
        }

        /* --- Zero/full-mask folding: AND #$00→LDA #$00, ORA #$FF→LDA #$FF --- */
        if ((n->op == IR_AND_IMM && (uint8_t)n->operand == 0x00) ||
            (n->op == IR_ORA_IMM && (uint8_t)n->operand == 0xFF)) {
            val = (n->op == IR_AND_IMM) ? 0x00 : 0xFF;
            n->op = IR_LDA_IMM; n->operand = val;
            n->flags = IR_F_WRITES_A | IR_F_WRITES_FLAGS;
            r->a_val = val; r->a_known = 1;
            changes++; continue;
        }

        /* --- Constant folding: AND/ORA/EOR #imm with known A --- */
        if (r->a_known && (n->op == IR_AND_IMM || n->op == IR_ORA_IMM || n->op == IR_EOR_IMM)) {
            val = r->a_val; imm = (uint8_t)n->operand;
            if (n->op == IR_AND_IMM) val &= imm;
            else if (n->op == IR_ORA_IMM) val |= imm;
            else val ^= imm;
            n->op = IR_LDA_IMM; n->operand = val;
            n->flags = IR_F_WRITES_A | IR_F_WRITES_FLAGS;
            r->a_val = val;
            changes++; continue;
        }

        /* --- Redundant store elimination with ZP shadow --- */
        if (n->op == IR_STA_ZP || n->op == IR_STX_ZP || n->op == IR_STY_ZP) {
            addr8 = (uint8_t)n->operand;
            reg_known = 0; reg_val = 0;
            if (n->op == IR_STA_ZP) { reg_known = r->a_known; reg_val = r->a_val; }
            else if (n->op == IR_STX_ZP) { reg_known = r->x_known; reg_val = r->x_val; }
            else { reg_known = r->y_known; reg_val = r->y_val; }

            if (reg_known) {
                for (s = 0; s < ZP_SHADOW_SIZE; s++) {
                    if (r->zp_known[s] && r->zp_addr[s] == addr8 &&
                        r->zp_val[s] == reg_val) {
                        ir_kill(ctx, i); changes++; goto next_node;
                    }
                }
            }
        }

        /* --- Redundant store elimination with ABS shadow --- */
        if (n->op == IR_STA_ABS || n->op == IR_STX_ABS || n->op == IR_STY_ABS) {
            addr16 = n->operand;
            reg_known = 0; reg_val = 0;
            if (n->op == IR_STA_ABS) { reg_known = r->a_known; reg_val = r->a_val; }
            else if (n->op == IR_STX_ABS) { reg_known = r->x_known; reg_val = r->x_val; }
            else { reg_known = r->y_known; reg_val = r->y_val; }

            if (reg_known) {
                for (s = 0; s < ABS_SHADOW_SIZE; s++) {
                    if (r->abs_known[s] && r->abs_addr[s] == addr16 &&
                        r->abs_val[s] == reg_val) {
                        ir_kill(ctx, i); changes++; goto next_node;
                    }
                }
            }
        }

        /* --- Update register shadow --- */
        if (n->op == IR_LDA_IMM) { r->a_val = (uint8_t)n->operand; r->a_known = 1; }
        else if (writes_a(n->op) || (n->op == IR_TEMPLATE && (n->flags & 0x10))) { r->a_known = 0; }

        if (n->op == IR_LDX_IMM) { r->x_val = (uint8_t)n->operand; r->x_known = 1; }
        else if (n->op == IR_INX && r->x_known) { r->x_val = (uint8_t)(r->x_val + 1); }
        else if (n->op == IR_DEX && r->x_known) { r->x_val = (uint8_t)(r->x_val - 1); }
        else if (writes_x(n->op) || (n->op == IR_TEMPLATE && (n->flags & 0x20))) { r->x_known = 0; }

        if (n->op == IR_LDY_IMM) { r->y_val = (uint8_t)n->operand; r->y_known = 1; }
        else if (n->op == IR_INY && r->y_known) { r->y_val = (uint8_t)(r->y_val + 1); }
        else if (n->op == IR_DEY && r->y_known) { r->y_val = (uint8_t)(r->y_val - 1); }
        else if (writes_y(n->op) || (n->op == IR_TEMPLATE && (n->flags & 0x40))) { r->y_known = 0; }

        /* --- Update ZP shadow cache --- */
        /* On STA/STX/STY zp with known register: record in cache.
         * On LDA/LDX/LDY zp: if cached, set register shadow from it. */
        if (n->op == IR_STA_ZP || n->op == IR_STX_ZP || n->op == IR_STY_ZP) {
            addr8 = (uint8_t)n->operand;
            reg_known = 0; reg_val = 0;
            if (n->op == IR_STA_ZP) { reg_known = r->a_known; reg_val = r->a_val; }
            else if (n->op == IR_STX_ZP) { reg_known = r->x_known; reg_val = r->x_val; }
            else { reg_known = r->y_known; reg_val = r->y_val; }

            /* Invalidate any existing entry for this addr */
            for (s = 0; s < ZP_SHADOW_SIZE; s++) {
                if (r->zp_known[s] && r->zp_addr[s] == addr8) { r->zp_known[s] = 0; break; }
            }
            /* Insert if register value is known */
            if (reg_known) {
                /* Try to find a free slot first, else evict LRU */
                slot = r->zp_lru;
                for (s = 0; s < ZP_SHADOW_SIZE; s++) {
                    if (!r->zp_known[s]) { slot = s; break; }
                }
                r->zp_addr[slot] = addr8;
                r->zp_val[slot] = reg_val;
                r->zp_known[slot] = 1;
                r->zp_lru = (slot + 1) & (ZP_SHADOW_SIZE - 1);
            }
        }
        else if (n->op == IR_LDA_ZP || n->op == IR_LDX_ZP || n->op == IR_LDY_ZP) {
            addr8 = (uint8_t)n->operand;
            /* If this ZP addr is in the cache, set register known */
            for (s = 0; s < ZP_SHADOW_SIZE; s++) {
                if (r->zp_known[s] && r->zp_addr[s] == addr8) {
                    if (n->op == IR_LDA_ZP) { r->a_val = r->zp_val[s]; r->a_known = 1; }
                    else if (n->op == IR_LDX_ZP) { r->x_val = r->zp_val[s]; r->x_known = 1; }
                    else { r->y_val = r->zp_val[s]; r->y_known = 1; }
                    break;
                }
            }
        }
        /* RMW ops on ZP invalidate that cache entry */
        if (writes_to_zp(n->op) &&
            n->op != IR_STA_ZP && n->op != IR_STX_ZP && n->op != IR_STY_ZP) {
            addr8 = (uint8_t)n->operand;
            for (s = 0; s < ZP_SHADOW_SIZE; s++) {
                if (r->zp_known[s] && r->zp_addr[s] == addr8) { r->zp_known[s] = 0; break; }
            }
        }

        /* --- Update ABS shadow cache --- */
        if (n->op == IR_STA_ABS || n->op == IR_STX_ABS || n->op == IR_STY_ABS) {
            addr16 = n->operand;
            reg_known = 0; reg_val = 0;
            if (n->op == IR_STA_ABS) { reg_known = r->a_known; reg_val = r->a_val; }
            else if (n->op == IR_STX_ABS) { reg_known = r->x_known; reg_val = r->x_val; }
            else { reg_known = r->y_known; reg_val = r->y_val; }

            /* Invalidate existing entry for this addr */
            for (s = 0; s < ABS_SHADOW_SIZE; s++) {
                if (r->abs_known[s] && r->abs_addr[s] == addr16) { r->abs_known[s] = 0; break; }
            }
            if (reg_known) {
                slot = r->abs_lru;
                for (s = 0; s < ABS_SHADOW_SIZE; s++) {
                    if (!r->abs_known[s]) { slot = s; break; }
                }
                r->abs_addr[slot] = addr16;
                r->abs_val[slot] = reg_val;
                r->abs_known[slot] = 1;
                r->abs_lru = (slot + 1) & (ABS_SHADOW_SIZE - 1);
            }
        }
        else if (n->op == IR_LDA_ABS || n->op == IR_LDX_ABS || n->op == IR_LDY_ABS) {
            addr16 = n->operand;
            for (s = 0; s < ABS_SHADOW_SIZE; s++) {
                if (r->abs_known[s] && r->abs_addr[s] == addr16) {
                    if (n->op == IR_LDA_ABS) { r->a_val = r->abs_val[s]; r->a_known = 1; }
                    else if (n->op == IR_LDX_ABS) { r->x_val = r->abs_val[s]; r->x_known = 1; }
                    else { r->y_val = r->abs_val[s]; r->y_known = 1; }
                    break;
                }
            }
        }
        /* RMW ops on abs invalidate that cache entry */
        if (writes_to_abs(n->op) &&
            n->op != IR_STA_ABS && n->op != IR_STX_ABS && n->op != IR_STY_ABS) {
            addr16 = n->operand;
            for (s = 0; s < ABS_SHADOW_SIZE; s++) {
                if (r->abs_known[s] && r->abs_addr[s] == addr16) { r->abs_known[s] = 0; break; }
            }
        }
        /* Indexed stores (ABSX/ABSY) — we can't track the effective
         * address, so conservatively flush the entire abs cache. */
        if (n->op == IR_STA_ABSX || n->op == IR_STA_ABSY) {
            r->abs_known[0] = r->abs_known[1] = r->abs_known[2] = r->abs_known[3] = 0;
        }

        /* Opaque nodes and branches invalidate all shadow state (conservative).
         * Before resetting, snapshot the current A/X/Y state as the block's
         * exit state.  The last barrier's snapshot is the final exit state. */
        if (is_barrier(n)) {
            ctx->exit_a_val = r->a_val; ctx->exit_a_known = r->a_known;
            ctx->exit_x_val = r->x_val; ctx->exit_x_known = r->x_known;
            ctx->exit_y_val = r->y_val; ctx->exit_y_known = r->y_known;
            r->a_known = r->x_known = r->y_known = 0;
            r->zp_known[0] = r->zp_known[1] = r->zp_known[2] = r->zp_known[3] = 0;
            r->abs_known[0] = r->abs_known[1] = r->abs_known[2] = r->abs_known[3] = 0;
        }
        /* Memory-writing templates: flush ZP/ABS caches (could alias) */
        else if (template_writes_memory(n)) {
            r->zp_known[0] = r->zp_known[1] = r->zp_known[2] = r->zp_known[3] = 0;
            r->abs_known[0] = r->abs_known[1] = r->abs_known[2] = r->abs_known[3] = 0;
        }
        next_node: ;
    }

    return changes;
}

/* ===================================================================
 * Pass 2: Dead store elimination
 *
 * Backward scan: if a store (STA/STX/STY to a ZP address) is
 * overwritten by a subsequent store to the same address before
 * any read of that address, the first store is dead.
 * =================================================================== */
uint8_t ir_opt_dead_store(ir_ctx_t *ctx)
{
    uint8_t changes = 0;

    for (uint8_t i = 0; i < ctx->node_count; i++) {
        ir_node_t *n = &ctx->nodes[i];
        if (n->op == IR_DEAD) continue;

        /* Only consider STA/STX/STY to ZP targets */
        uint8_t is_store_zp = 0;
        if (n->op == IR_STA_ZP || n->op == IR_STX_ZP || n->op == IR_STY_ZP)
            is_store_zp = 1;

        if (!is_store_zp) continue;

        uint8_t target_addr = (uint8_t)n->operand;
        uint8_t store_op = n->op;

        /* --- Store-back elimination (backward scan) ---
         * If the register was loaded from this same ZP address and
         * neither the register nor the address changed since, the
         * store writes back an identical value — kill it. */
        {
            uint8_t load_op;
            if (store_op == IR_STA_ZP) load_op = IR_LDA_ZP;
            else if (store_op == IR_STX_ZP) load_op = IR_LDX_ZP;
            else load_op = IR_LDY_ZP;

            uint8_t killed = 0;
            uint8_t j = i;
            while (j > 0) {
                j--;
                uint8_t op = ctx->nodes[j].op;
                if (op == IR_DEAD) continue;

                /* Found matching load from same address → store-back */
                if (op == load_op && (uint8_t)ctx->nodes[j].operand == target_addr) {
                    ir_kill(ctx, i);
                    changes++;
                    killed = 1;
                    break;
                }
                /* Register was clobbered */
                if (store_op == IR_STA_ZP && (writes_a(op) || (op == IR_TEMPLATE && (ctx->nodes[j].flags & 0x10)))) break;
                if (store_op == IR_STX_ZP && (writes_x(op) || (op == IR_TEMPLATE && (ctx->nodes[j].flags & 0x20)))) break;
                if (store_op == IR_STY_ZP && (writes_y(op) || (op == IR_TEMPLATE && (ctx->nodes[j].flags & 0x40)))) break;
                /* ZP address was modified by another instruction */
                if (writes_to_zp(op) && (uint8_t)ctx->nodes[j].operand == target_addr) break;
                /* Memory-writing templates could alias this ZP address */
                if (template_writes_memory(&ctx->nodes[j])) break;
                /* Opaque / branch / control-flow template barrier */
                if (is_barrier(&ctx->nodes[j])) break;
            }
            if (killed) continue;
        }

        /* Scan forward: look for another store to same address,
         * or a read from same address (which makes this store live) */
        for (uint8_t j = i + 1; j < ctx->node_count; j++) {
            ir_node_t *m = &ctx->nodes[j];
            if (m->op == IR_DEAD) continue;

            /* Opaque, branch, or control-flow template — bail (conservative) */
            if (is_barrier(m))
                break;

            /* Memory-writing templates could alias this ZP address */
            if (template_writes_memory(m))
                break;

            /* Another store to same ZP address? First store is dead.
             * Match any STA/STX/STY, not just the same opcode — e.g.
             * STA _pc followed by STX _pc is still a dead first store. */
            if ((m->op == IR_STA_ZP || m->op == IR_STX_ZP || m->op == IR_STY_ZP) &&
                (uint8_t)m->operand == target_addr) {
                ir_kill(ctx, i);
                changes++;
                break;
            }

            /* Read from same ZP address? First store is live. */
            if ((m->op == IR_LDA_ZP || m->op == IR_LDX_ZP || m->op == IR_LDY_ZP ||
                 m->op == IR_ADC_ZP || m->op == IR_SBC_ZP ||
                 m->op == IR_AND_ZP || m->op == IR_ORA_ZP || m->op == IR_EOR_ZP ||
                 m->op == IR_CMP_ZP || m->op == IR_CPX_ZP || m->op == IR_CPY_ZP ||
                 m->op == IR_INC_ZP || m->op == IR_DEC_ZP ||
                 m->op == IR_ASL_ZP || m->op == IR_LSR_ZP ||
                 m->op == IR_ROL_ZP || m->op == IR_ROR_ZP ||
                 m->op == IR_BIT_ZP) &&
                (uint8_t)m->operand == target_addr)
                break;  /* store is live */
        }
    }

    return changes;
}

/* ===================================================================
 * Pass 2b: Dead load elimination
 *
 * Forward scan: if a ZP load writes a register that is overwritten
 * before being read, AND the flags the load sets (N, Z) are also
 * overwritten before being read, the load is dead.
 * =================================================================== */
uint8_t ir_opt_dead_load(ir_ctx_t *ctx)
{
    uint8_t changes = 0;

    for (uint8_t i = 0; i < ctx->node_count; i++) {
        ir_node_t *n = &ctx->nodes[i];
        if (n->op == IR_DEAD) continue;

        /* Determine which register this load targets (0=none/skip) */
        uint8_t load_reg;  /* 'A', 'X', 'Y', or 0 */
        switch (n->op) {
            case IR_LDA_ZP: case IR_LDA_IMM: load_reg = 'A'; break;
            case IR_LDX_ZP: case IR_LDX_IMM: load_reg = 'X'; break;
            case IR_LDY_ZP: case IR_LDY_IMM: load_reg = 'Y'; break;
            default: continue;
        }

        uint8_t reg_dead = 0, flags_dead = 0;

        for (uint8_t j = i + 1; j < ctx->node_count; j++) {
            uint8_t op = ctx->nodes[j].op;
            if (op == IR_DEAD) continue;

            /* Barrier — conservatively assume register/flags live */
            if (is_barrier(&ctx->nodes[j])) break;

            /* --- Register liveness (template-aware via flags) --- */
            if (!reg_dead) {
                uint8_t rused = 0;
                uint8_t fl = ctx->nodes[j].flags;
                if (load_reg == 'A' && (reads_a(op) || (op == IR_TEMPLATE && (fl & 0x01)))) rused = 1;
                if (load_reg == 'X' && (reads_x(op) || (op == IR_TEMPLATE && (fl & 0x02)))) rused = 1;
                if (load_reg == 'Y' && (reads_y(op) || (op == IR_TEMPLATE && (fl & 0x04)))) rused = 1;
                if (rused) break;  /* register is live */

                uint8_t writes_reg = 0;
                if (load_reg == 'A') {
                    writes_reg = writes_a(op) || (op == IR_TEMPLATE && (fl & 0x10));
                } else if (load_reg == 'X') {
                    writes_reg = writes_x(op) || (op == IR_TEMPLATE && (fl & 0x20));
                } else {
                    writes_reg = writes_y(op) || (op == IR_TEMPLATE && (fl & 0x40));
                }
                if (writes_reg) reg_dead = 1;
            }

            /* --- Flags liveness (template-aware) ---
             * PLP replaces all flags from the stack; it does NOT
             * read the current N/Z values, so treat it as a flag
             * writer only. */
            if (!flags_dead) {
                uint8_t fl = ctx->nodes[j].flags;
                if (op != IR_PLP && (reads_flags(op) || (op == IR_TEMPLATE && (fl & 0x08)))) break;
                if (writes_flags(op) || (op == IR_TEMPLATE && (fl & 0x80))) flags_dead = 1;
            }

            if (reg_dead && flags_dead) {
                ir_kill(ctx, i);
                changes++;
                break;
            }
        }
    }

    return changes;
}

/* ===================================================================
 * Pass 3: PHP/PLP pair elision
 * (generalized version of ENABLE_PEEPHOLE)
 *
 * Adjacent PLP → PHP pairs across instruction boundaries can be
 * removed: the PLP restores flags, PHP immediately saves them again.
 * Also handles PLP → (flag-neutral ops) → PHP.
 * =================================================================== */
uint8_t ir_opt_php_plp_elision(ir_ctx_t *ctx)
{
    uint8_t changes = 0;

    for (uint8_t i = 0; i + 1 < ctx->node_count; i++) {
        if (ctx->nodes[i].op == IR_DEAD) continue;

        if (ctx->nodes[i].op == IR_PLP) {
            /* Look for a following PHP, skipping dead nodes */
            for (uint8_t j = i + 1; j < ctx->node_count; j++) {
                if (ctx->nodes[j].op == IR_DEAD) continue;

                if (ctx->nodes[j].op == IR_PHP) {
                    /* PLP/PHP pair — remove both */
                    ir_kill(ctx, i);
                    ir_kill(ctx, j);
                    changes += 2;
                    break;
                }

                /* If the intervening instruction reads or writes flags,
                 * the PLP is needed — stop looking.
                 * Templates: check flags for R:F or W:F. */
                {
                    uint8_t fl = ctx->nodes[j].flags;
                    if (reads_flags(ctx->nodes[j].op) || writes_flags(ctx->nodes[j].op) ||
                        (ctx->nodes[j].op == IR_TEMPLATE && (fl & 0x88)))
                        break;
                }

                /* Opaque, branch, or control-flow template — stop */
                if (is_barrier(&ctx->nodes[j]))
                    break;
            }
        }
    }

    return changes;
}

/* Passes 4a (pair_rewrite) and 4b (clc_sec_sink) have been moved to
 * backend/ir_opt_ext.c (bank17) to relieve bank0 space pressure.
 * They are called via ir_optimize_ext() after the core loop.
 * =================================================================== */
uint8_t ir_optimize(ir_ctx_t *ctx)
{
    uint8_t total_changes = 0;
    uint8_t iterations = 0;
    uint8_t changes;

    ctx->stat_redundant_load = 0;
    ctx->stat_dead_store     = 0;
    ctx->stat_dead_load      = 0;
    ctx->stat_php_plp        = 0;
    ctx->stat_pair_rewrite   = 0;
    ctx->stat_rmw_fusion     = 0;

    do {
        uint8_t c;
        changes = 0;

#ifdef ENABLE_IR_OPT_REDUNDANT_LOAD
        c = ir_opt_redundant_load(ctx);
        ctx->stat_redundant_load += c;
        changes += c;
#endif

#ifdef ENABLE_IR_OPT_DEAD_STORE
        c = ir_opt_dead_store(ctx);
        ctx->stat_dead_store += c;
        changes += c;
#endif

#ifdef ENABLE_IR_OPT_DEAD_LOAD
        c = ir_opt_dead_load(ctx);
        ctx->stat_dead_load += c;
        changes += c;
#endif

#ifdef ENABLE_IR_OPT_PHP_PLP
        c = ir_opt_php_plp_elision(ctx);
        ctx->stat_php_plp += c;
        changes += c;
#endif

        total_changes += changes;
        iterations++;
    } while (changes > 0 && iterations < 4);

    return total_changes;
}

#pragma section default
