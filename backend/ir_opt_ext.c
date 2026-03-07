/**
 * ir_opt_ext.c - Extended IR optimization passes (bank17)
 *
 * Contains ir_opt_pair_rewrite and ir_opt_clc_sec_sink, moved out of
 * ir_opt.c (bank0) to relieve bank0 space pressure.
 *
 * These passes run after the core optimization loop in ir_optimize().
 * Callers switch to BANK_COMPILE (bank17) and call ir_optimize_ext().
 */

#pragma section bank17

#include <stdint.h>
#include "ir.h"

/* ===================================================================
 * Duplicated helpers (must be local — bank0 functions not callable
 * from bank17).  These are small switch statements that generate
 * compact 6502 code as lookup tables.
 * =================================================================== */

static void ir_kill(ir_ctx_t *ctx, uint8_t idx)
{
    ctx->nodes[idx].op = IR_DEAD;
    ctx->nodes[idx].flags = 0;
    ctx->nodes[idx].operand = 0;
}

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
        case IR_TEMPLATE:
            return 1;
    }
    return 0;
}

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

static uint8_t reads_x(uint8_t op)
{
    switch (op) {
        case IR_STX_ZP: case IR_STX_ABS:
        case IR_TXA: case IR_TXS:
        case IR_INX: case IR_DEX:
        case IR_CPX_IMM: case IR_CPX_ZP: case IR_CPX_ABS:
        case IR_LDA_ABSX: case IR_STA_ABSX:
        case IR_ADC_ABSX: case IR_SBC_ABSX:
        case IR_AND_ABSX: case IR_ORA_ABSX: case IR_EOR_ABSX:
        case IR_CMP_ABSX:
        case IR_LDY_ABSX:
        case IR_INC_ABSX: case IR_DEC_ABSX:
        case IR_ASL_ABSX: case IR_LSR_ABSX:
        case IR_ROL_ABSX: case IR_ROR_ABSX:
        case IR_TEMPLATE:
            return 1;
    }
    return 0;
}

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

static uint8_t reads_y(uint8_t op)
{
    switch (op) {
        case IR_STY_ZP: case IR_STY_ABS:
        case IR_TYA:
        case IR_INY: case IR_DEY:
        case IR_CPY_IMM: case IR_CPY_ZP: case IR_CPY_ABS:
        case IR_LDA_ABSY: case IR_STA_ABSY:
        case IR_ADC_ABSY: case IR_SBC_ABSY:
        case IR_AND_ABSY: case IR_ORA_ABSY: case IR_EOR_ABSY:
        case IR_CMP_ABSY:
        case IR_LDX_ABSY:
        case IR_TEMPLATE:
            return 1;
    }
    return 0;
}

static uint8_t writes_flags(uint8_t op)
{
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
            return 1;
    }
}

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

static uint8_t reads_carry(uint8_t op)
{
    switch (op) {
        case IR_ADC_IMM: case IR_SBC_IMM:
        case IR_ADC_ZP:  case IR_SBC_ZP:
        case IR_ADC_ABS: case IR_SBC_ABS:
        case IR_ADC_ABSX: case IR_SBC_ABSX:
        case IR_ADC_ABSY: case IR_SBC_ABSY:
        case IR_ROL_A: case IR_ROR_A:
        case IR_ROL_ZP: case IR_ROR_ZP:
        case IR_ROL_ABS: case IR_ROR_ABS:
        case IR_ROL_ABSX: case IR_ROR_ABSX:
        case IR_BCC: case IR_BCS:
        case IR_TEMPLATE:
            return 1;
    }
    return 0;
}

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

static uint8_t is_barrier(ir_node_t *n)
{
    uint8_t op = n->op;
    switch (op) {
        case IR_JSR: case IR_RTS: case IR_JMP_ABS:
        case IR_RAW_BYTE: case IR_RAW_WORD: case IR_RAW_OP_ABS:
        case IR_BPL: case IR_BMI: case IR_BVC: case IR_BVS:
        case IR_BCC: case IR_BCS: case IR_BNE: case IR_BEQ:
        case IR_PC_MARK:
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

static uint8_t dup_interferes(ir_node_t *orig, ir_node_t *mid)
{
    if (is_barrier(mid)) return 1;
    if (template_writes_memory(mid)) return 1;
    uint8_t ot = ((orig->flags & 0x0F) << 4) | (orig->flags & 0xF0);
    if (ot & mid->flags & 0xF0) return 1;
    if (writes_to_zp(mid->op)) {
        switch (orig->op) {
            case IR_STA_ZP: case IR_STX_ZP: case IR_STY_ZP:
            case IR_LDA_ZP: case IR_LDX_ZP: case IR_LDY_ZP:
            case IR_AND_ZP: case IR_ORA_ZP:
            case IR_CMP_ZP: case IR_CPX_ZP: case IR_CPY_ZP:
            case IR_BIT_ZP:
                if ((uint8_t)orig->operand == (uint8_t)mid->operand)
                    return 1;
                break;
        }
    }
    return 0;
}

/* ===================================================================
 * Pass 4a: Pair rewrite — adjacent-instruction pattern matching
 * (moved from ir_opt.c bank0 to relieve space pressure)
 * =================================================================== */
uint8_t ir_opt_pair_rewrite(ir_ctx_t *ctx)
{
    uint8_t changes = 0;

    for (uint8_t i = 0; i + 1 < ctx->node_count; i++) {
        ir_node_t *a = &ctx->nodes[i];
        if (a->op == IR_DEAD) continue;

        /* --- Non-adjacent duplicate instruction elimination --- */
        if (!is_barrier(a)) {
            uint8_t dup_eligible = 1;
            switch (a->op) {
                case IR_INC_ZP: case IR_DEC_ZP:
                case IR_ASL_ZP: case IR_LSR_ZP:
                case IR_ROL_ZP: case IR_ROR_ZP:
                case IR_INC_ABS: case IR_DEC_ABS:
                case IR_ASL_ABS: case IR_LSR_ABS:
                case IR_ROL_ABS: case IR_ROR_ABS:
                case IR_PHP: case IR_PLP:
                case IR_PHA: case IR_PLA:
                case IR_INX: case IR_DEX:
                case IR_INY: case IR_DEY:
                case IR_JSR: case IR_RTS:
                case IR_ADC_IMM: case IR_SBC_IMM: case IR_EOR_IMM:
                case IR_ADC_ZP:  case IR_SBC_ZP:  case IR_EOR_ZP:
                case IR_ADC_ABS: case IR_SBC_ABS: case IR_EOR_ABS:
                case IR_ASL_A: case IR_LSR_A:
                case IR_ROL_A: case IR_ROR_A:
                    dup_eligible = 0;
                    break;
            }
            if (a->op >= IR_LDA_ABSX && a->op <= IR_ROR_ABSX)
                dup_eligible = 0;
            if (dup_eligible) {
                for (uint8_t k = i + 1; k < ctx->node_count; k++) {
                    ir_node_t *m = &ctx->nodes[k];
                    if (m->op == IR_DEAD) continue;
                    if (m->op == a->op && m->operand == a->operand) {
                        ir_kill(ctx, k);
                        changes++;
                        continue;
                    }
                    if (dup_interferes(a, m)) break;
                }
            }
        }

        /* Find next non-dead node */
        uint8_t j;
        for (j = i + 1; j < ctx->node_count; j++) {
            if (ctx->nodes[j].op != IR_DEAD) break;
        }
        if (j >= ctx->node_count) break;
        ir_node_t *b = &ctx->nodes[j];

        /* --- TAX, TXA → TAX --- */
        if (a->op == IR_TAX && b->op == IR_TXA) {
            ir_kill(ctx, j); changes++; continue;
        }
        if (a->op == IR_TXA && b->op == IR_TAX) {
            ir_kill(ctx, j); changes++; continue;
        }
        if (a->op == IR_TAY && b->op == IR_TYA) {
            ir_kill(ctx, j); changes++; continue;
        }
        if (a->op == IR_TYA && b->op == IR_TAY) {
            ir_kill(ctx, j); changes++; continue;
        }
        if (a->op == IR_TXS && b->op == IR_TSX) {
            ir_kill(ctx, j); changes++; continue;
        }
        if (a->op == IR_TSX && b->op == IR_TXS) {
            ir_kill(ctx, j); changes++; continue;
        }

        /* --- LDA #imm, TAX → LDX #imm (if A not needed after) --- */
        if (a->op == IR_LDA_IMM && b->op == IR_TAX) {
            uint8_t a_used = 0;
            for (uint8_t k = j + 1; k < ctx->node_count; k++) {
                if (ctx->nodes[k].op == IR_DEAD) continue;
                if (reads_a(ctx->nodes[k].op)) { a_used = 1; break; }
                if (writes_a(ctx->nodes[k].op)) break;
                if (is_barrier(&ctx->nodes[k])) { a_used = 1; break; }
            }
            if (!a_used) {
                a->op = IR_LDX_IMM;
                a->flags = IR_F_WRITES_X | IR_F_WRITES_FLAGS;
                ir_kill(ctx, j); changes++; continue;
            }
        }

        /* --- LDA #imm, TAY → LDY #imm (if A not needed after) --- */
        if (a->op == IR_LDA_IMM && b->op == IR_TAY) {
            uint8_t a_used = 0;
            for (uint8_t k = j + 1; k < ctx->node_count; k++) {
                if (ctx->nodes[k].op == IR_DEAD) continue;
                if (reads_a(ctx->nodes[k].op)) { a_used = 1; break; }
                if (writes_a(ctx->nodes[k].op)) break;
                if (is_barrier(&ctx->nodes[k])) { a_used = 1; break; }
            }
            if (!a_used) {
                a->op = IR_LDY_IMM;
                a->flags = IR_F_WRITES_Y | IR_F_WRITES_FLAGS;
                ir_kill(ctx, j); changes++; continue;
            }
        }

        /* --- STA zp, LDA zp (same addr) → kill LDA --- */
        if (a->op == IR_STA_ZP && b->op == IR_LDA_ZP &&
            (uint8_t)a->operand == (uint8_t)b->operand) {
            uint8_t flags_needed = 0;
            for (uint8_t k = j + 1; k < ctx->node_count; k++) {
                if (ctx->nodes[k].op == IR_DEAD) continue;
                if (reads_flags(ctx->nodes[k].op)) { flags_needed = 1; break; }
                if (writes_flags(ctx->nodes[k].op)) break;
            }
            if (!flags_needed) {
                ir_kill(ctx, j); changes++; continue;
            }
        }

        /* --- LDA zp, STA zp (same addr) → kill STA --- */
        if (a->op == IR_LDA_ZP && b->op == IR_STA_ZP &&
            (uint8_t)a->operand == (uint8_t)b->operand) {
            ir_kill(ctx, j); changes++; continue;
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
                ir_kill(ctx, j); changes++; continue;
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
                ir_kill(ctx, j); changes++; continue;
            }
        }

        /* --- TAX, STX zp → STA zp (if X not needed after) --- */
        if (a->op == IR_TAX && b->op == IR_STX_ZP) {
            uint8_t x_used = 0;
            for (uint8_t k = j + 1; k < ctx->node_count; k++) {
                if (ctx->nodes[k].op == IR_DEAD) continue;
                if (reads_x(ctx->nodes[k].op)) { x_used = 1; break; }
                if (writes_x(ctx->nodes[k].op)) break;
                if (is_barrier(&ctx->nodes[k])) { x_used = 1; break; }
            }
            if (!x_used) {
                b->op = IR_STA_ZP;
                b->flags = IR_F_READS_A;
                ir_kill(ctx, i); changes++; continue;
            }
        }

        /* --- TAY, STY zp → STA zp (if Y not needed after) --- */
        if (a->op == IR_TAY && b->op == IR_STY_ZP) {
            uint8_t y_used = 0;
            for (uint8_t k = j + 1; k < ctx->node_count; k++) {
                if (ctx->nodes[k].op == IR_DEAD) continue;
                if (reads_y(ctx->nodes[k].op)) { y_used = 1; break; }
                if (writes_y(ctx->nodes[k].op)) break;
                if (is_barrier(&ctx->nodes[k])) { y_used = 1; break; }
            }
            if (!y_used) {
                b->op = IR_STA_ZP;
                b->flags = IR_F_READS_A;
                ir_kill(ctx, i); changes++; continue;
            }
        }

        /* --- STA zp, LDX same → STA zp, TAX --- */
        if (a->op == IR_STA_ZP && b->op == IR_LDX_ZP &&
            (uint8_t)a->operand == (uint8_t)b->operand) {
            b->op = IR_TAX;
            b->flags = IR_F_READS_A | IR_F_WRITES_X | IR_F_WRITES_FLAGS;
            b->operand = 0;
            changes++; continue;
        }

        /* --- STA zp, LDY same → STA zp, TAY --- */
        if (a->op == IR_STA_ZP && b->op == IR_LDY_ZP &&
            (uint8_t)a->operand == (uint8_t)b->operand) {
            b->op = IR_TAY;
            b->flags = IR_F_READS_A | IR_F_WRITES_Y | IR_F_WRITES_FLAGS;
            b->operand = 0;
            changes++; continue;
        }

        /* --- STX zp, LDA same → STX zp, TXA --- */
        if (a->op == IR_STX_ZP && b->op == IR_LDA_ZP &&
            (uint8_t)a->operand == (uint8_t)b->operand) {
            b->op = IR_TXA;
            b->flags = IR_F_READS_X | IR_F_WRITES_A | IR_F_WRITES_FLAGS;
            b->operand = 0;
            changes++; continue;
        }

        /* --- STY zp, LDA same → STY zp, TYA --- */
        if (a->op == IR_STY_ZP && b->op == IR_LDA_ZP &&
            (uint8_t)a->operand == (uint8_t)b->operand) {
            b->op = IR_TYA;
            b->flags = IR_F_READS_Y | IR_F_WRITES_A | IR_F_WRITES_FLAGS;
            b->operand = 0;
            changes++; continue;
        }

        /* --- TXA, STA zp → STX zp (if A not needed after) --- */
        if (a->op == IR_TXA && b->op == IR_STA_ZP) {
            uint8_t a_used = 0;
            for (uint8_t k = j + 1; k < ctx->node_count; k++) {
                if (ctx->nodes[k].op == IR_DEAD) continue;
                if (reads_a(ctx->nodes[k].op)) { a_used = 1; break; }
                if (writes_a(ctx->nodes[k].op)) break;
                if (is_barrier(&ctx->nodes[k])) { a_used = 1; break; }
            }
            if (!a_used) {
                b->op = IR_STX_ZP;
                b->flags = IR_F_READS_X;
                ir_kill(ctx, i); changes++; continue;
            }
        }

        /* --- TYA, STA zp → STY zp (if A not needed after) --- */
        if (a->op == IR_TYA && b->op == IR_STA_ZP) {
            uint8_t a_used = 0;
            for (uint8_t k = j + 1; k < ctx->node_count; k++) {
                if (ctx->nodes[k].op == IR_DEAD) continue;
                if (reads_a(ctx->nodes[k].op)) { a_used = 1; break; }
                if (writes_a(ctx->nodes[k].op)) break;
                if (is_barrier(&ctx->nodes[k])) { a_used = 1; break; }
            }
            if (!a_used) {
                b->op = IR_STY_ZP;
                b->flags = IR_F_READS_Y;
                ir_kill(ctx, i); changes++; continue;
            }
        }

        /* --- LDX zp, TXA → LDA zp (if X not needed after) --- */
        if (a->op == IR_LDX_ZP && b->op == IR_TXA) {
            uint8_t x_used = 0;
            for (uint8_t k = j + 1; k < ctx->node_count; k++) {
                if (ctx->nodes[k].op == IR_DEAD) continue;
                if (reads_x(ctx->nodes[k].op)) { x_used = 1; break; }
                if (writes_x(ctx->nodes[k].op)) break;
                if (is_barrier(&ctx->nodes[k])) { x_used = 1; break; }
            }
            if (!x_used) {
                a->op = IR_LDA_ZP;
                a->flags = IR_F_WRITES_A | IR_F_WRITES_FLAGS;
                ir_kill(ctx, j); changes++; continue;
            }
        }

        /* --- LDY zp, TYA → LDA zp (if Y not needed after) --- */
        if (a->op == IR_LDY_ZP && b->op == IR_TYA) {
            uint8_t y_used = 0;
            for (uint8_t k = j + 1; k < ctx->node_count; k++) {
                if (ctx->nodes[k].op == IR_DEAD) continue;
                if (reads_y(ctx->nodes[k].op)) { y_used = 1; break; }
                if (writes_y(ctx->nodes[k].op)) break;
                if (is_barrier(&ctx->nodes[k])) { y_used = 1; break; }
            }
            if (!y_used) {
                a->op = IR_LDA_ZP;
                a->flags = IR_F_WRITES_A | IR_F_WRITES_FLAGS;
                ir_kill(ctx, j); changes++; continue;
            }
        }

        /* --- CMP #$00 / CPX #$00 / CPY #$00 elimination --- */
        if (b->op == IR_CMP_IMM && (uint8_t)b->operand == 0x00 &&
            writes_a(a->op) && writes_flags(a->op)) {
            uint8_t c_needed = 0;
            for (uint8_t k = j + 1; k < ctx->node_count; k++) {
                if (ctx->nodes[k].op == IR_DEAD) continue;
                if (reads_carry(ctx->nodes[k].op)) { c_needed = 1; break; }
                if (writes_flags(ctx->nodes[k].op)) break;
                if (is_barrier(&ctx->nodes[k])) {
                    c_needed = ctx->carry_live_at_exit;
                    break;
                }
            }
            if (!c_needed) {
                ir_kill(ctx, j); changes++; continue;
            }
        }
        if (b->op == IR_CPX_IMM && (uint8_t)b->operand == 0x00 &&
            writes_x(a->op) && writes_flags(a->op)) {
            uint8_t c_needed = 0;
            for (uint8_t k = j + 1; k < ctx->node_count; k++) {
                if (ctx->nodes[k].op == IR_DEAD) continue;
                if (reads_carry(ctx->nodes[k].op)) { c_needed = 1; break; }
                if (writes_flags(ctx->nodes[k].op)) break;
                if (is_barrier(&ctx->nodes[k])) {
                    c_needed = ctx->carry_live_at_exit;
                    break;
                }
            }
            if (!c_needed) {
                ir_kill(ctx, j); changes++; continue;
            }
        }
        if (b->op == IR_CPY_IMM && (uint8_t)b->operand == 0x00 &&
            writes_y(a->op) && writes_flags(a->op)) {
            uint8_t c_needed = 0;
            for (uint8_t k = j + 1; k < ctx->node_count; k++) {
                if (ctx->nodes[k].op == IR_DEAD) continue;
                if (reads_carry(ctx->nodes[k].op)) { c_needed = 1; break; }
                if (writes_flags(ctx->nodes[k].op)) break;
                if (is_barrier(&ctx->nodes[k])) {
                    c_needed = ctx->carry_live_at_exit;
                    break;
                }
            }
            if (!c_needed) {
                ir_kill(ctx, j); changes++; continue;
            }
        }
    }

    return changes;
}

/* ===================================================================
 * Pass 4b: CLC/SEC sinking — move CLC/SEC closer to carry consumer
 * =================================================================== */

static uint8_t writes_carry_no_read(uint8_t op)
{
    switch (op) {
        case IR_CLC: case IR_SEC: case IR_PLP:
        case IR_CMP_IMM: case IR_CMP_ZP: case IR_CMP_ABS:
        case IR_CMP_ABSX: case IR_CMP_ABSY:
        case IR_CPX_IMM: case IR_CPX_ZP: case IR_CPX_ABS:
        case IR_CPY_IMM: case IR_CPY_ZP: case IR_CPY_ABS:
        case IR_ASL_A: case IR_LSR_A:
        case IR_ASL_ZP: case IR_LSR_ZP:
        case IR_ASL_ABS: case IR_LSR_ABS:
        case IR_ASL_ABSX: case IR_LSR_ABSX:
            return 1;
    }
    return 0;
}

uint8_t ir_opt_clc_sec_sink(ir_ctx_t *ctx)
{
    uint8_t changes = 0;
    uint8_t i, k;

    for (i = 0; i + 1 < ctx->node_count; i++) {
        uint8_t src_op = ctx->nodes[i].op;
        if (src_op != IR_CLC && src_op != IR_SEC) continue;

        uint8_t consumer = 0;
        uint8_t found = 0;
        uint8_t gap = 0;

        for (k = i + 1; k < ctx->node_count; k++) {
            uint8_t kop = ctx->nodes[k].op;
            if (kop == IR_DEAD) continue;
            if (reads_carry(kop)) { consumer = k; found = 1; break; }
            if (writes_carry_no_read(kop) || is_barrier(&ctx->nodes[k]) ||
                kop == IR_PHP) break;
            gap = 1;
        }
        if (!found || !gap) continue;

        uint8_t safe = 1;
        for (k = 0; k < ctx->label_count; k++) {
            uint8_t t = ctx->label_targets[k];
            if (t >= i && t < consumer) { safe = 0; break; }
        }
        if (!safe) continue;

        uint8_t pos = i;
        for (k = i + 1; k < consumer; k++) {
            if (ctx->nodes[k].op == IR_DEAD) continue;
            ir_node_t tmp = ctx->nodes[pos];
            ctx->nodes[pos] = ctx->nodes[k];
            ctx->nodes[k] = tmp;
            pos = k;
        }
        changes++;
    }

    return changes;
}

/* ===================================================================
 * ir_optimize_ext — run extended passes (pair rewrite + CLC/SEC sink)
 *
 * Called after ir_optimize() completes its core loop.
 * These passes are independent of the core loop — they do pattern
 * matching that rarely creates opportunities for redundant_load etc.
 * =================================================================== */
uint8_t ir_optimize_ext(ir_ctx_t *ctx)
{
    uint8_t total = 0;
    uint8_t c;

    c = ir_opt_pair_rewrite(ctx);
    ctx->stat_pair_rewrite += c;
    total += c;

    c = ir_opt_clc_sec_sink(ctx);
    ctx->stat_pair_rewrite += c;
    total += c;

    return total;
}

#pragma section default
