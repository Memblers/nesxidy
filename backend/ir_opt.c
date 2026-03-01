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
 * Lives in bank 2 (compile-time only, alongside recompile_opcode_b2).
 */

#pragma section bank1

#include <stdint.h>
#include "ir.h"

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

/* Helper: is this a branch node? */
static uint8_t is_branch(uint8_t op)
{
    return (op >= IR_BPL && op <= IR_BEQ);
}

/* Helper: is this an opaque / barrier node?
 * Templates, JSR, RTS, JMP, and raw bytes are all opaque — the
 * optimizer cannot reason about their register or ZP side-effects,
 * so every pass must treat them conservatively. */
static uint8_t is_opaque(uint8_t op)
{
    switch (op) {
        case IR_TEMPLATE:
        case IR_JSR:
        case IR_RTS:
        case IR_JMP_ABS:
        case IR_RAW_BYTE:
        case IR_RAW_WORD:
        case IR_RAW_OP_ABS:
            return 1;
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

/* ===================================================================
 * Pass 1: Redundant load elimination
 * (from opt65.c opti1 / peephole_patterns.txt Phase 1)
 *
 * Forward scan tracking last-known register values.
 * Kill LDA/LDX/LDY #imm if register already holds that value
 * and the instruction's flag side-effects are unused.
 * =================================================================== */
uint8_t ir_opt_redundant_load(ir_ctx_t *ctx)
{
    uint8_t changes = 0;
    ir_reg_shadow_t *r = &ctx->regs;

    /* Reset shadow */
    r->a_known = r->x_known = r->y_known = 0;

    for (uint8_t i = 0; i < ctx->node_count; i++) {
        ir_node_t *n = &ctx->nodes[i];
        if (n->op == IR_DEAD) continue;

        /* --- Check for redundant immediate loads --- */
        if (n->op == IR_LDA_IMM && r->a_known && r->a_val == (uint8_t)n->operand) {
            /* A already holds this value.  Safe to kill if the next
             * non-dead node doesn't read flags set by this load.
             * Conservative: check if any following node reads flags
             * before the next flag-writing node. */
            uint8_t flags_needed = 0;
            for (uint8_t j = i + 1; j < ctx->node_count; j++) {
                if (ctx->nodes[j].op == IR_DEAD) continue;
                if (reads_flags(ctx->nodes[j].op)) { flags_needed = 1; break; }
                if (writes_flags(ctx->nodes[j].op)) break;
            }
            if (!flags_needed) {
                ir_kill(ctx, i);
                changes++;
                continue;
            }
        }
        if (n->op == IR_LDX_IMM && r->x_known && r->x_val == (uint8_t)n->operand) {
            uint8_t flags_needed = 0;
            for (uint8_t j = i + 1; j < ctx->node_count; j++) {
                if (ctx->nodes[j].op == IR_DEAD) continue;
                if (reads_flags(ctx->nodes[j].op)) { flags_needed = 1; break; }
                if (writes_flags(ctx->nodes[j].op)) break;
            }
            if (!flags_needed) {
                ir_kill(ctx, i);
                changes++;
                continue;
            }
        }
        if (n->op == IR_LDY_IMM && r->y_known && r->y_val == (uint8_t)n->operand) {
            uint8_t flags_needed = 0;
            for (uint8_t j = i + 1; j < ctx->node_count; j++) {
                if (ctx->nodes[j].op == IR_DEAD) continue;
                if (reads_flags(ctx->nodes[j].op)) { flags_needed = 1; break; }
                if (writes_flags(ctx->nodes[j].op)) break;
            }
            if (!flags_needed) {
                ir_kill(ctx, i);
                changes++;
                continue;
            }
        }

        /* --- Constant folding: AND/ORA/EOR #imm with known A --- */
        if (r->a_known) {
            if (n->op == IR_AND_IMM) {
                uint8_t result = r->a_val & (uint8_t)n->operand;
                n->op = IR_LDA_IMM;
                n->operand = result;
                n->flags = IR_F_WRITES_A | IR_F_WRITES_FLAGS;
                r->a_val = result;
                changes++;
                continue;
            }
            if (n->op == IR_ORA_IMM) {
                uint8_t result = r->a_val | (uint8_t)n->operand;
                n->op = IR_LDA_IMM;
                n->operand = result;
                n->flags = IR_F_WRITES_A | IR_F_WRITES_FLAGS;
                r->a_val = result;
                changes++;
                continue;
            }
            if (n->op == IR_EOR_IMM) {
                uint8_t result = r->a_val ^ (uint8_t)n->operand;
                n->op = IR_LDA_IMM;
                n->operand = result;
                n->flags = IR_F_WRITES_A | IR_F_WRITES_FLAGS;
                r->a_val = result;
                changes++;
                continue;
            }
        }

        /* --- Update register shadow --- */
        if (n->op == IR_LDA_IMM) { r->a_val = (uint8_t)n->operand; r->a_known = 1; }
        else if (writes_a(n->op))  { r->a_known = 0; }

        if (n->op == IR_LDX_IMM) { r->x_val = (uint8_t)n->operand; r->x_known = 1; }
        else if (writes_x(n->op))  { r->x_known = 0; }

        if (n->op == IR_LDY_IMM) { r->y_val = (uint8_t)n->operand; r->y_known = 1; }
        else if (writes_y(n->op))  { r->y_known = 0; }

        /* Opaque nodes and branches invalidate register shadow (conservative) */
        if (is_opaque(n->op) || is_branch(n->op)) {
            r->a_known = r->x_known = r->y_known = 0;
        }
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
                if (store_op == IR_STA_ZP && writes_a(op)) break;
                if (store_op == IR_STX_ZP && writes_x(op)) break;
                if (store_op == IR_STY_ZP && writes_y(op)) break;
                /* ZP address was modified by another instruction */
                if (writes_to_zp(op) && (uint8_t)ctx->nodes[j].operand == target_addr) break;
                /* Opaque / branch barrier */
                if (is_opaque(op) || is_branch(op)) break;
            }
            if (killed) continue;
        }

        /* Scan forward: look for another store to same address,
         * or a read from same address (which makes this store live) */
        for (uint8_t j = i + 1; j < ctx->node_count; j++) {
            ir_node_t *m = &ctx->nodes[j];
            if (m->op == IR_DEAD) continue;

            /* Opaque or branch — bail (conservative) */
            if (is_branch(m->op) || is_opaque(m->op))
                break;

            /* Another store to same ZP address? First store is dead. */
            if (m->op == store_op && (uint8_t)m->operand == target_addr) {
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

        if (n->op != IR_LDA_ZP && n->op != IR_LDX_ZP && n->op != IR_LDY_ZP)
            continue;

        uint8_t reg_dead = 0, flags_dead = 0;

        for (uint8_t j = i + 1; j < ctx->node_count; j++) {
            uint8_t op = ctx->nodes[j].op;
            if (op == IR_DEAD) continue;

            /* Barrier — conservatively assume register/flags live */
            if (is_opaque(op) || is_branch(op)) break;

            /* --- Register liveness --- */
            if (!reg_dead) {
                uint8_t rused = 0;
                if (n->op == IR_LDA_ZP && reads_a(op)) rused = 1;
                if (n->op == IR_LDX_ZP && reads_x(op)) rused = 1;
                if (n->op == IR_LDY_ZP && reads_y(op)) rused = 1;
                if (rused) break;  /* register is live */

                if (n->op == IR_LDA_ZP && writes_a(op)) reg_dead = 1;
                if (n->op == IR_LDX_ZP && writes_x(op)) reg_dead = 1;
                if (n->op == IR_LDY_ZP && writes_y(op)) reg_dead = 1;
            }

            /* --- Flags liveness ---
             * PLP replaces all flags from the stack; it does NOT
             * read the current N/Z values, so treat it as a flag
             * writer only. */
            if (!flags_dead) {
                if (op != IR_PLP && reads_flags(op)) break;
                if (writes_flags(op)) flags_dead = 1;
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
                 * the PLP is needed — stop looking */
                if (reads_flags(ctx->nodes[j].op) || writes_flags(ctx->nodes[j].op))
                    break;

                /* Opaque or branch — stop */
                if (is_branch(ctx->nodes[j].op) || is_opaque(ctx->nodes[j].op))
                    break;
            }
        }
    }

    return changes;
}

/* ===================================================================
 * Pass 4: Adjacent-pair rewrites
 * (from opt65.c opti3 / peephole_patterns.txt Phase 3)
 *
 * 27 concrete pair rules from the peephole pattern list.
 * =================================================================== */
uint8_t ir_opt_pair_rewrite(ir_ctx_t *ctx)
{
    uint8_t changes = 0;

    for (uint8_t i = 0; i + 1 < ctx->node_count; i++) {
        ir_node_t *a = &ctx->nodes[i];
        if (a->op == IR_DEAD) continue;

        /* Find next non-dead node */
        uint8_t j;
        for (j = i + 1; j < ctx->node_count; j++) {
            if (ctx->nodes[j].op != IR_DEAD) break;
        }
        if (j >= ctx->node_count) break;
        ir_node_t *b = &ctx->nodes[j];

        /* --- TAX, TXA → TAX --- */
        if (a->op == IR_TAX && b->op == IR_TXA) {
            ir_kill(ctx, j);
            changes++;
            continue;
        }
        /* --- TXA, TAX → TXA --- */
        if (a->op == IR_TXA && b->op == IR_TAX) {
            ir_kill(ctx, j);
            changes++;
            continue;
        }
        /* --- TAY, TYA → TAY --- */
        if (a->op == IR_TAY && b->op == IR_TYA) {
            ir_kill(ctx, j);
            changes++;
            continue;
        }
        /* --- TYA, TAY → TYA --- */
        if (a->op == IR_TYA && b->op == IR_TAY) {
            ir_kill(ctx, j);
            changes++;
            continue;
        }
        /* --- TXS, TSX → TXS --- */
        if (a->op == IR_TXS && b->op == IR_TSX) {
            ir_kill(ctx, j);
            changes++;
            continue;
        }
        /* --- TSX, TXS → TSX --- */
        if (a->op == IR_TSX && b->op == IR_TXS) {
            ir_kill(ctx, j);
            changes++;
            continue;
        }

        /* --- LDA #imm, TAX → LDX #imm (if A not needed after) --- */
        if (a->op == IR_LDA_IMM && b->op == IR_TAX) {
            /* Check if A is used after this pair */
            uint8_t a_used = 0;
            for (uint8_t k = j + 1; k < ctx->node_count; k++) {
                if (ctx->nodes[k].op == IR_DEAD) continue;
                if (reads_a(ctx->nodes[k].op)) { a_used = 1; break; }
                if (writes_a(ctx->nodes[k].op)) break;
                if (is_branch(ctx->nodes[k].op) || is_opaque(ctx->nodes[k].op))
                    { a_used = 1; break; }
            }
            if (!a_used) {
                a->op = IR_LDX_IMM;
                a->flags = IR_F_WRITES_X | IR_F_WRITES_FLAGS;
                ir_kill(ctx, j);
                changes++;
                continue;
            }
        }

        /* --- LDA #imm, TAY → LDY #imm (if A not needed after) --- */
        if (a->op == IR_LDA_IMM && b->op == IR_TAY) {
            uint8_t a_used = 0;
            for (uint8_t k = j + 1; k < ctx->node_count; k++) {
                if (ctx->nodes[k].op == IR_DEAD) continue;
                if (reads_a(ctx->nodes[k].op)) { a_used = 1; break; }
                if (writes_a(ctx->nodes[k].op)) break;
                if (is_branch(ctx->nodes[k].op) || is_opaque(ctx->nodes[k].op))
                    { a_used = 1; break; }
            }
            if (!a_used) {
                a->op = IR_LDY_IMM;
                a->flags = IR_F_WRITES_Y | IR_F_WRITES_FLAGS;
                ir_kill(ctx, j);
                changes++;
                continue;
            }
        }

        /* --- STA zp, LDA zp (same addr) → kill LDA --- */
        if (a->op == IR_STA_ZP && b->op == IR_LDA_ZP &&
            (uint8_t)a->operand == (uint8_t)b->operand) {
            /* LDA is redundant — A still holds the value.
             * But we must check that flags from LDA aren't needed. */
            uint8_t flags_needed = 0;
            for (uint8_t k = j + 1; k < ctx->node_count; k++) {
                if (ctx->nodes[k].op == IR_DEAD) continue;
                if (reads_flags(ctx->nodes[k].op)) { flags_needed = 1; break; }
                if (writes_flags(ctx->nodes[k].op)) break;
            }
            if (!flags_needed) {
                ir_kill(ctx, j);
                changes++;
                continue;
            }
        }

        /* --- LDA zp, STA zp (same addr) → kill STA --- */
        if (a->op == IR_LDA_ZP && b->op == IR_STA_ZP &&
            (uint8_t)a->operand == (uint8_t)b->operand) {
            ir_kill(ctx, j);
            changes++;
            continue;
        }

        /* --- STX zp, LDX zp (same addr) → kill LDX --- */
        if (a->op == IR_STX_ZP && b->op == IR_LDX_ZP &&
            (uint8_t)a->operand == (uint8_t)b->operand) {
            uint8_t flags_needed = 0;
            for (uint8_t k = j + 1; k < ctx->node_count; k++) {
                if (ctx->nodes[k].op == IR_DEAD) continue;
                if (reads_flags(ctx->nodes[k].op)) { flags_needed = 1; break; }
                if (writes_flags(ctx->nodes[k].op)) break;
            }
            if (!flags_needed) {
                ir_kill(ctx, j);
                changes++;
                continue;
            }
        }

        /* --- STY zp, LDY zp (same addr) → kill LDY --- */
        if (a->op == IR_STY_ZP && b->op == IR_LDY_ZP &&
            (uint8_t)a->operand == (uint8_t)b->operand) {
            uint8_t flags_needed = 0;
            for (uint8_t k = j + 1; k < ctx->node_count; k++) {
                if (ctx->nodes[k].op == IR_DEAD) continue;
                if (reads_flags(ctx->nodes[k].op)) { flags_needed = 1; break; }
                if (writes_flags(ctx->nodes[k].op)) break;
            }
            if (!flags_needed) {
                ir_kill(ctx, j);
                changes++;
                continue;
            }
        }

        /* --- TAX, STX zp → STA zp (if X not needed after) --- */
        if (a->op == IR_TAX && b->op == IR_STX_ZP) {
            uint8_t x_used = 0;
            for (uint8_t k = j + 1; k < ctx->node_count; k++) {
                if (ctx->nodes[k].op == IR_DEAD) continue;
                if (reads_x(ctx->nodes[k].op)) { x_used = 1; break; }
                if (writes_x(ctx->nodes[k].op)) break;
                if (is_branch(ctx->nodes[k].op) || is_opaque(ctx->nodes[k].op))
                    { x_used = 1; break; }
            }
            if (!x_used) {
                b->op = IR_STA_ZP;
                b->flags = IR_F_READS_A;
                ir_kill(ctx, i);
                changes++;
                continue;
            }
        }

        /* --- TAY, STY zp → STA zp (if Y not needed after) --- */
        if (a->op == IR_TAY && b->op == IR_STY_ZP) {
            uint8_t y_used = 0;
            for (uint8_t k = j + 1; k < ctx->node_count; k++) {
                if (ctx->nodes[k].op == IR_DEAD) continue;
                if (reads_y(ctx->nodes[k].op)) { y_used = 1; break; }
                if (writes_y(ctx->nodes[k].op)) break;
                if (is_branch(ctx->nodes[k].op) || is_opaque(ctx->nodes[k].op))
                    { y_used = 1; break; }
            }
            if (!y_used) {
                b->op = IR_STA_ZP;
                b->flags = IR_F_READS_A;
                ir_kill(ctx, i);
                changes++;
                continue;
            }
        }

        /* --- STA zp, LDX same → STA zp, TAX (if saving bytes when paired) --- */
        if (a->op == IR_STA_ZP && b->op == IR_LDX_ZP &&
            (uint8_t)a->operand == (uint8_t)b->operand) {
            b->op = IR_TAX;
            b->flags = IR_F_READS_A | IR_F_WRITES_X | IR_F_WRITES_FLAGS;
            b->operand = 0;
            changes++;
            continue;
        }

        /* --- STA zp, LDY same → STA zp, TAY --- */
        if (a->op == IR_STA_ZP && b->op == IR_LDY_ZP &&
            (uint8_t)a->operand == (uint8_t)b->operand) {
            b->op = IR_TAY;
            b->flags = IR_F_READS_A | IR_F_WRITES_Y | IR_F_WRITES_FLAGS;
            b->operand = 0;
            changes++;
            continue;
        }

        /* --- STX zp, LDA same → STX zp, TXA --- */
        if (a->op == IR_STX_ZP && b->op == IR_LDA_ZP &&
            (uint8_t)a->operand == (uint8_t)b->operand) {
            b->op = IR_TXA;
            b->flags = IR_F_READS_X | IR_F_WRITES_A | IR_F_WRITES_FLAGS;
            b->operand = 0;
            changes++;
            continue;
        }

        /* --- STY zp, LDA same → STY zp, TYA --- */
        if (a->op == IR_STY_ZP && b->op == IR_LDA_ZP &&
            (uint8_t)a->operand == (uint8_t)b->operand) {
            b->op = IR_TYA;
            b->flags = IR_F_READS_Y | IR_F_WRITES_A | IR_F_WRITES_FLAGS;
            b->operand = 0;
            changes++;
            continue;
        }

        /* --- TXA, STA zp → STX zp (if A not needed after) --- */
        if (a->op == IR_TXA && b->op == IR_STA_ZP) {
            uint8_t a_used = 0;
            for (uint8_t k = j + 1; k < ctx->node_count; k++) {
                if (ctx->nodes[k].op == IR_DEAD) continue;
                if (reads_a(ctx->nodes[k].op)) { a_used = 1; break; }
                if (writes_a(ctx->nodes[k].op)) break;
                if (is_branch(ctx->nodes[k].op) || is_opaque(ctx->nodes[k].op))
                    { a_used = 1; break; }
            }
            if (!a_used) {
                b->op = IR_STX_ZP;
                b->flags = IR_F_READS_X;
                ir_kill(ctx, i);
                changes++;
                continue;
            }
        }

        /* --- TYA, STA zp → STY zp (if A not needed after) --- */
        if (a->op == IR_TYA && b->op == IR_STA_ZP) {
            uint8_t a_used = 0;
            for (uint8_t k = j + 1; k < ctx->node_count; k++) {
                if (ctx->nodes[k].op == IR_DEAD) continue;
                if (reads_a(ctx->nodes[k].op)) { a_used = 1; break; }
                if (writes_a(ctx->nodes[k].op)) break;
                if (is_branch(ctx->nodes[k].op) || is_opaque(ctx->nodes[k].op))
                    { a_used = 1; break; }
            }
            if (!a_used) {
                b->op = IR_STY_ZP;
                b->flags = IR_F_READS_Y;
                ir_kill(ctx, i);
                changes++;
                continue;
            }
        }

        /* --- LDX zp, TXA → LDA zp (if X not needed after) --- */
        if (a->op == IR_LDX_ZP && b->op == IR_TXA) {
            uint8_t x_used = 0;
            for (uint8_t k = j + 1; k < ctx->node_count; k++) {
                if (ctx->nodes[k].op == IR_DEAD) continue;
                if (reads_x(ctx->nodes[k].op)) { x_used = 1; break; }
                if (writes_x(ctx->nodes[k].op)) break;
                if (is_branch(ctx->nodes[k].op) || is_opaque(ctx->nodes[k].op))
                    { x_used = 1; break; }
            }
            if (!x_used) {
                a->op = IR_LDA_ZP;
                a->flags = IR_F_WRITES_A | IR_F_WRITES_FLAGS;
                ir_kill(ctx, j);
                changes++;
                continue;
            }
        }

        /* --- LDY zp, TYA → LDA zp (if Y not needed after) --- */
        if (a->op == IR_LDY_ZP && b->op == IR_TYA) {
            uint8_t y_used = 0;
            for (uint8_t k = j + 1; k < ctx->node_count; k++) {
                if (ctx->nodes[k].op == IR_DEAD) continue;
                if (reads_y(ctx->nodes[k].op)) { y_used = 1; break; }
                if (writes_y(ctx->nodes[k].op)) break;
                if (is_branch(ctx->nodes[k].op) || is_opaque(ctx->nodes[k].op))
                    { y_used = 1; break; }
            }
            if (!y_used) {
                a->op = IR_LDA_ZP;
                a->flags = IR_F_WRITES_A | IR_F_WRITES_FLAGS;
                ir_kill(ctx, j);
                changes++;
                continue;
            }
        }
    }

    return changes;
}

/* ===================================================================
 * ir_optimize — run all enabled passes, iterating until stable
 * =================================================================== */
uint8_t ir_optimize(ir_ctx_t *ctx)
{
    uint8_t total_changes = 0;
    uint8_t iterations = 0;
    uint8_t changes;

    ctx->stat_redundant_load = 0;
    ctx->stat_dead_store     = 0;
    ctx->stat_php_plp        = 0;
    ctx->stat_pair_rewrite   = 0;

    do {
        uint8_t c;
        changes = 0;

        c = ir_opt_redundant_load(ctx);
        ctx->stat_redundant_load += c; changes += c;

        c = ir_opt_dead_store(ctx);
        ctx->stat_dead_store += c; changes += c;

        c = ir_opt_dead_load(ctx);
        ctx->stat_redundant_load += c; changes += c;

        c = ir_opt_php_plp_elision(ctx);
        ctx->stat_php_plp += c; changes += c;

        c = ir_opt_pair_rewrite(ctx);
        ctx->stat_pair_rewrite += c; changes += c;

        total_changes += changes;
        iterations++;
    } while (changes > 0 && iterations < 4);

    return total_changes;
}

#pragma section default
