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

#pragma section bank0

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

/* Helper: does this instruction READ the carry flag specifically?
 * (ADC/SBC use carry-in, ROL/ROR rotate through carry,
 *  BCC/BCS branch on carry.) */
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
 * JSR, RTS, JMP, and raw bytes are all opaque — the optimizer cannot
 * reason about their register or ZP side-effects, so every pass must
 * treat them conservatively.
 *
 * Templates are NO LONGER opaque: they now have proper register flags
 * set by ir_get_template_flags(), allowing optimization passes to
 * understand and penetrate them just like normal instructions. */
static uint8_t is_opaque(uint8_t op)
{
    switch (op) {
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

/* Helper: does intermediate instruction 'mid' prevent a later
 * identical copy of 'orig' from being redundant?
 * Uses the IR node flags bitfield for register/flag checks.
 * Returns 1 if the forward scan must stop. */
static uint8_t dup_interferes(ir_node_t *orig, ir_node_t *mid)
{
    if (is_branch(mid->op) || is_opaque(mid->op)) return 1;

    /* Register + flag check via bitfield.
     * orig_touch has a set bit (in write-position 4-7) for every
     * register orig reads OR writes.  If mid writes any of those
     * registers, the duplicate is no longer guaranteed equivalent. */
    uint8_t ot = ((orig->flags & 0x0F) << 4) | (orig->flags & 0xF0);
    if (ot & mid->flags & 0xF0) return 1;

    /* ZP memory: if orig accesses a ZP addr and mid writes it */
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
        if (ctx->nodes[j].op == IR_DEAD) continue;
        if (reads_flags(ctx->nodes[j].op)) return 0;
        if (writes_flags(ctx->nodes[j].op)) return 1;
    }
    return 1;
}

uint8_t ir_opt_redundant_load(ir_ctx_t *ctx)
{
    uint8_t changes = 0;
    ir_reg_shadow_t *r = &ctx->regs;

    /* Reset ALL shadow state — ZP/ABS caches must be cleared because
     * ir_optimize() calls this pass in a loop; stale cache entries
     * from a previous iteration would poison the scan. */
    r->a_known = r->x_known = r->y_known = 0;
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
            uint8_t val = (n->op == IR_AND_IMM) ? 0x00 : 0xFF;
            n->op = IR_LDA_IMM; n->operand = val;
            n->flags = IR_F_WRITES_A | IR_F_WRITES_FLAGS;
            r->a_val = val; r->a_known = 1;
            changes++; continue;
        }

        /* --- Constant folding: AND/ORA/EOR #imm with known A --- */
        if (r->a_known && (n->op == IR_AND_IMM || n->op == IR_ORA_IMM || n->op == IR_EOR_IMM)) {
            uint8_t val = r->a_val, imm = (uint8_t)n->operand;
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
            uint8_t addr = (uint8_t)n->operand;
            uint8_t reg_known = 0, reg_val = 0;
            if (n->op == IR_STA_ZP) { reg_known = r->a_known; reg_val = r->a_val; }
            else if (n->op == IR_STX_ZP) { reg_known = r->x_known; reg_val = r->x_val; }
            else { reg_known = r->y_known; reg_val = r->y_val; }

            if (reg_known) {
                uint8_t s;
                for (s = 0; s < ZP_SHADOW_SIZE; s++) {
                    if (r->zp_known[s] && r->zp_addr[s] == addr &&
                        r->zp_val[s] == reg_val) {
                        ir_kill(ctx, i); changes++; goto next_node;
                    }
                }
            }
        }

        /* --- Redundant store elimination with ABS shadow --- */
        if (n->op == IR_STA_ABS || n->op == IR_STX_ABS || n->op == IR_STY_ABS) {
            uint16_t addr = n->operand;
            uint8_t reg_known = 0, reg_val = 0;
            if (n->op == IR_STA_ABS) { reg_known = r->a_known; reg_val = r->a_val; }
            else if (n->op == IR_STX_ABS) { reg_known = r->x_known; reg_val = r->x_val; }
            else { reg_known = r->y_known; reg_val = r->y_val; }

            if (reg_known) {
                uint8_t s;
                for (s = 0; s < ABS_SHADOW_SIZE; s++) {
                    if (r->abs_known[s] && r->abs_addr[s] == addr &&
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
            uint8_t addr = (uint8_t)n->operand;
            uint8_t reg_known = 0, reg_val = 0;
            if (n->op == IR_STA_ZP) { reg_known = r->a_known; reg_val = r->a_val; }
            else if (n->op == IR_STX_ZP) { reg_known = r->x_known; reg_val = r->x_val; }
            else { reg_known = r->y_known; reg_val = r->y_val; }

            /* Invalidate any existing entry for this addr */
            { uint8_t s; for (s = 0; s < ZP_SHADOW_SIZE; s++) {
                if (r->zp_known[s] && r->zp_addr[s] == addr) { r->zp_known[s] = 0; break; }
            } }
            /* Insert if register value is known */
            if (reg_known) {
                /* Try to find a free slot first, else evict LRU */
                uint8_t slot = r->zp_lru;
                { uint8_t s; for (s = 0; s < ZP_SHADOW_SIZE; s++) {
                    if (!r->zp_known[s]) { slot = s; break; }
                } }
                r->zp_addr[slot] = addr;
                r->zp_val[slot] = reg_val;
                r->zp_known[slot] = 1;
                r->zp_lru = (slot + 1) & (ZP_SHADOW_SIZE - 1);
            }
        }
        else if (n->op == IR_LDA_ZP || n->op == IR_LDX_ZP || n->op == IR_LDY_ZP) {
            uint8_t addr = (uint8_t)n->operand;
            /* If this ZP addr is in the cache, set register known */
            { uint8_t s; for (s = 0; s < ZP_SHADOW_SIZE; s++) {
                if (r->zp_known[s] && r->zp_addr[s] == addr) {
                    if (n->op == IR_LDA_ZP) { r->a_val = r->zp_val[s]; r->a_known = 1; }
                    else if (n->op == IR_LDX_ZP) { r->x_val = r->zp_val[s]; r->x_known = 1; }
                    else { r->y_val = r->zp_val[s]; r->y_known = 1; }
                    break;
                }
            } }
        }
        /* RMW ops on ZP invalidate that cache entry */
        if (writes_to_zp(n->op) &&
            n->op != IR_STA_ZP && n->op != IR_STX_ZP && n->op != IR_STY_ZP) {
            uint8_t addr = (uint8_t)n->operand;
            { uint8_t s; for (s = 0; s < ZP_SHADOW_SIZE; s++) {
                if (r->zp_known[s] && r->zp_addr[s] == addr) { r->zp_known[s] = 0; break; }
            } }
        }

        /* --- Update ABS shadow cache --- */
        if (n->op == IR_STA_ABS || n->op == IR_STX_ABS || n->op == IR_STY_ABS) {
            uint16_t addr = n->operand;
            uint8_t reg_known = 0, reg_val = 0;
            if (n->op == IR_STA_ABS) { reg_known = r->a_known; reg_val = r->a_val; }
            else if (n->op == IR_STX_ABS) { reg_known = r->x_known; reg_val = r->x_val; }
            else { reg_known = r->y_known; reg_val = r->y_val; }

            /* Invalidate existing entry for this addr */
            { uint8_t s; for (s = 0; s < ABS_SHADOW_SIZE; s++) {
                if (r->abs_known[s] && r->abs_addr[s] == addr) { r->abs_known[s] = 0; break; }
            } }
            if (reg_known) {
                uint8_t slot = r->abs_lru;
                { uint8_t s; for (s = 0; s < ABS_SHADOW_SIZE; s++) {
                    if (!r->abs_known[s]) { slot = s; break; }
                } }
                r->abs_addr[slot] = addr;
                r->abs_val[slot] = reg_val;
                r->abs_known[slot] = 1;
                r->abs_lru = (slot + 1) & (ABS_SHADOW_SIZE - 1);
            }
        }
        else if (n->op == IR_LDA_ABS || n->op == IR_LDX_ABS || n->op == IR_LDY_ABS) {
            uint16_t addr = n->operand;
            { uint8_t s; for (s = 0; s < ABS_SHADOW_SIZE; s++) {
                if (r->abs_known[s] && r->abs_addr[s] == addr) {
                    if (n->op == IR_LDA_ABS) { r->a_val = r->abs_val[s]; r->a_known = 1; }
                    else if (n->op == IR_LDX_ABS) { r->x_val = r->abs_val[s]; r->x_known = 1; }
                    else { r->y_val = r->abs_val[s]; r->y_known = 1; }
                    break;
                }
            } }
        }
        /* RMW ops on abs invalidate that cache entry */
        if (writes_to_abs(n->op) &&
            n->op != IR_STA_ABS && n->op != IR_STX_ABS && n->op != IR_STY_ABS) {
            uint16_t addr = n->operand;
            { uint8_t s; for (s = 0; s < ABS_SHADOW_SIZE; s++) {
                if (r->abs_known[s] && r->abs_addr[s] == addr) { r->abs_known[s] = 0; break; }
            } }
        }
        /* Indexed stores (ABSX/ABSY) — we can't track the effective
         * address, so conservatively flush the entire abs cache. */
        if (n->op == IR_STA_ABSX || n->op == IR_STA_ABSY) {
            r->abs_known[0] = r->abs_known[1] = r->abs_known[2] = r->abs_known[3] = 0;
        }

        /* Opaque nodes and branches invalidate all shadow state (conservative) */
        if (is_opaque(n->op) || is_branch(n->op)) {
            r->a_known = r->x_known = r->y_known = 0;
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
            uint8_t flags = ctx->nodes[j].flags;
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

                /* Check register writes: for templates, check flags */
                uint8_t writes_reg = 0;
                if (n->op == IR_LDA_ZP) {
                    writes_reg = writes_a(op) || (op == IR_TEMPLATE && (flags & 0x10));
                } else if (n->op == IR_LDX_ZP) {
                    writes_reg = writes_x(op) || (op == IR_TEMPLATE && (flags & 0x20));
                } else if (n->op == IR_LDY_ZP) {
                    writes_reg = writes_y(op) || (op == IR_TEMPLATE && (flags & 0x40));
                }
                if (writes_reg) reg_dead = 1;
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

        /* --- Non-adjacent duplicate instruction elimination ---
         * Scan forward from `a` for an identical instruction (same
         * op + operand).  Kill duplicates unless an intermediate
         * instruction interferes — modifies a register, flags, or
         * memory location that `a` reads or writes.  Stores benefit
         * most: they don't write flags, so the scan passes through
         * flag-writing ALU ops that don't touch the source register.
         * RMW/stack/inc-dec ops are excluded (each execution has
         * a unique side effect). */
        if (!is_opaque(a->op) && !is_branch(a->op)) {
            uint8_t dup_eligible = 1;
            /* Exclude non-idempotent ops: RMW memory, stack,
             * inc/dec, ADC/SBC/EOR (accumulate), shifts on A,
             * JSR/RTS, and all indexed modes (incomplete flags). */
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
            /* All ABSX/ABSY modes: flag table lacks R:X/R:Y */
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

        /* --- CMP #$00 / CPX #$00 / CPY #$00 elimination ---
         * CMP #$00 sets N/Z identically to any instruction that
         * writes A+flags, and sets C=1 unconditionally (A >= 0).
         * Safe to kill if:
         *   1. Preceding instruction writes the register AND flags,
         *   2. The carry flag is NOT read before being overwritten
         *      (since we'd lose the unconditional C=1 from CMP).
         * Same logic applies to CPX #$00 / CPY #$00. */
        if (b->op == IR_CMP_IMM && (uint8_t)b->operand == 0x00 &&
            writes_a(a->op) && writes_flags(a->op)) {
            uint8_t c_needed = 0;
            for (uint8_t k = j + 1; k < ctx->node_count; k++) {
                if (ctx->nodes[k].op == IR_DEAD) continue;
                if (reads_carry(ctx->nodes[k].op)) { c_needed = 1; break; }
                if (writes_flags(ctx->nodes[k].op)) break;
                if (is_opaque(ctx->nodes[k].op) || is_branch(ctx->nodes[k].op)) {
                    c_needed = ctx->carry_live_at_exit;
                    break;
                }
            }
            if (!c_needed) {
                ir_kill(ctx, j);
                changes++;
                continue;
            }
        }
        if (b->op == IR_CPX_IMM && (uint8_t)b->operand == 0x00 &&
            writes_x(a->op) && writes_flags(a->op)) {
            uint8_t c_needed = 0;
            for (uint8_t k = j + 1; k < ctx->node_count; k++) {
                if (ctx->nodes[k].op == IR_DEAD) continue;
                if (reads_carry(ctx->nodes[k].op)) { c_needed = 1; break; }
                if (writes_flags(ctx->nodes[k].op)) break;
                if (is_opaque(ctx->nodes[k].op) || is_branch(ctx->nodes[k].op)) {
                    c_needed = ctx->carry_live_at_exit;
                    break;
                }
            }
            if (!c_needed) {
                ir_kill(ctx, j);
                changes++;
                continue;
            }
        }
        if (b->op == IR_CPY_IMM && (uint8_t)b->operand == 0x00 &&
            writes_y(a->op) && writes_flags(a->op)) {
            uint8_t c_needed = 0;
            for (uint8_t k = j + 1; k < ctx->node_count; k++) {
                if (ctx->nodes[k].op == IR_DEAD) continue;
                if (reads_carry(ctx->nodes[k].op)) { c_needed = 1; break; }
                if (writes_flags(ctx->nodes[k].op)) break;
                if (is_opaque(ctx->nodes[k].op) || is_branch(ctx->nodes[k].op)) {
                    c_needed = ctx->carry_live_at_exit;
                    break;
                }
            }
            if (!c_needed) {
                ir_kill(ctx, j);
                changes++;
                continue;
            }
        }
    }

    return changes;
}

/* ===================================================================
 * Pass 4b: CLC/SEC sinking — move CLC/SEC closer to carry consumer
 * (from opt65.c opti2: carry-init sinking)
 *
 * The recompiler may emit CLC early, then STA _x / LDX … before
 * the ADC that actually needs carry.  Sinking CLC to be adjacent
 * to the ADC enables future pair-rewrite rules and improves code
 * locality.
 * =================================================================== */

/* Helper: does this instruction WRITE carry but NOT READ carry?
 * Used to detect carry clobbers that aren't consumers. */
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

        /* Scan forward for the first carry consumer (ADC, SBC, ROL,
         * ROR, BCC, BCS).  Stop if we hit a carry clobber (CMP, ASL,
         * LSR, another CLC/SEC, PLP), a branch, or an opaque node.
         * PHP also reads carry (pushes status) — treat as barrier. */
        uint8_t consumer = 0;
        uint8_t found = 0;
        uint8_t gap = 0;   /* 1 if any non-dead instruction between */

        for (k = i + 1; k < ctx->node_count; k++) {
            uint8_t kop = ctx->nodes[k].op;
            if (kop == IR_DEAD) continue;
            if (reads_carry(kop)) { consumer = k; found = 1; break; }
            if (writes_carry_no_read(kop) || is_branch(kop) ||
                is_opaque(kop) || kop == IR_PHP) break;
            gap = 1;
        }
        if (!found || !gap) continue;

        /* Safety: no labels may target nodes in [i, consumer).
         * Bubble-swapping past a label target would invalidate the
         * label index, breaking intra-block branches. */
        uint8_t safe = 1;
        for (k = 0; k < ctx->label_count; k++) {
            uint8_t t = ctx->label_targets[k];
            if (t >= i && t < consumer) { safe = 0; break; }
        }
        if (!safe) continue;

        /* Bubble CLC/SEC forward by swapping past each non-dead
         * intermediate instruction, preserving their relative order. */
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
 * ir_optimize — run all enabled passes, iterating until stable
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

        c = ir_opt_redundant_load(ctx);
        ctx->stat_redundant_load += c;
        changes += c;

        c = ir_opt_dead_store(ctx);
        ctx->stat_dead_store += c;
        changes += c;

        c = ir_opt_dead_load(ctx);
        ctx->stat_dead_load += c;
        changes += c;

        c = ir_opt_php_plp_elision(ctx);
        ctx->stat_php_plp += c;
        changes += c;

        c = ir_opt_clc_sec_sink(ctx);
        ctx->stat_pair_rewrite += c;
        changes += c;

        c = ir_opt_pair_rewrite(ctx);
        ctx->stat_pair_rewrite += c;
        changes += c;

        total_changes += changes;
        iterations++;
    } while (changes > 0 && iterations < 4);

    return total_changes;
}

#pragma section default
