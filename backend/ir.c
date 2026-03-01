/**
 * ir.c - IR recording functions
 *
 * Implements the IR buffer API.  All functions are small and designed
 * to live in bank 2 alongside recompile_opcode_b2 (compile-time only).
 */

#pragma section bank1

#include <stdint.h>
#include "ir.h"

/* -------------------------------------------------------------------
 * Lookup table: IR opcode -> native instruction size in bytes.
 * Used by ir_estimate_size() and the lowering pass.
 * Indexed by (ir_op).  0 = special handling required.
 * ------------------------------------------------------------------- */
#pragma section rodata1
static const uint8_t ir_native_size[] = {
    /*  0x00 */ 0,  /* unused */
    /*  IR_LDA_IMM  0x01 */ 2,
    /*  IR_LDA_ZP   0x02 */ 2,
    /*  IR_LDA_ABS  0x03 */ 3,
    /*  IR_LDX_IMM  0x04 */ 2,
    /*  IR_LDX_ZP   0x05 */ 2,
    /*  IR_LDX_ABS  0x06 */ 3,
    /*  IR_LDY_IMM  0x07 */ 2,
    /*  IR_LDY_ZP   0x08 */ 2,
    /*  IR_LDY_ABS  0x09 */ 3,
    /*  IR_STA_ZP   0x0A */ 2,
    /*  IR_STA_ABS  0x0B */ 3,
    /*  IR_STX_ZP   0x0C */ 2,
    /*  IR_STX_ABS  0x0D */ 3,
    /*  IR_STY_ZP   0x0E */ 2,
    /*  IR_STY_ABS  0x0F */ 3,

    /*  IR_JMP_ABS  0x10 */ 3,
    /*  IR_JSR      0x11 */ 3,
    /*  IR_RTS      0x12 */ 1,
    /*  IR_PHP      0x13 */ 1,
    /*  IR_PLP      0x14 */ 1,
    /*  IR_PHA      0x15 */ 1,
    /*  IR_PLA      0x16 */ 1,
    /*  IR_NOP      0x17 */ 1,
    /*  IR_CLC      0x18 */ 1,
    /*  IR_SEC      0x19 */ 1,
    /*  IR_CLD      0x1A */ 1,
    /*  IR_SED      0x1B */ 1,
    /*  IR_CLI      0x1C */ 1,
    /*  IR_SEI      0x1D */ 1,
    /*  IR_CLV      0x1E */ 1,
    /*  IR_BRK      0x1F */ 1,

    /*  IR_BPL      0x20 */ 2,
    /*  IR_BMI      0x21 */ 2,
    /*  IR_BVC      0x22 */ 2,
    /*  IR_BVS      0x23 */ 2,
    /*  IR_BCC      0x24 */ 2,
    /*  IR_BCS      0x25 */ 2,
    /*  IR_BNE      0x26 */ 2,
    /*  IR_BEQ      0x27 */ 2,

    /*  IR_TAX      0x28 */ 1,
    /*  IR_TAY      0x29 */ 1,
    /*  IR_TXA      0x2A */ 1,
    /*  IR_TYA      0x2B */ 1,
    /*  IR_TSX      0x2C */ 1,
    /*  IR_TXS      0x2D */ 1,
    /*  IR_INX      0x2E */ 1,
    /*  IR_INY      0x2F */ 1,
    /*  IR_DEX      0x30 */ 1,
    /*  IR_DEY      0x31 */ 1,

    /*  IR_ADC_IMM  0x32 */ 2,
    /*  IR_SBC_IMM  0x33 */ 2,
    /*  IR_AND_IMM  0x34 */ 2,
    /*  IR_ORA_IMM  0x35 */ 2,
    /*  IR_EOR_IMM  0x36 */ 2,
    /*  IR_CMP_IMM  0x37 */ 2,
    /*  IR_CPX_IMM  0x38 */ 2,
    /*  IR_CPY_IMM  0x39 */ 2,

    /*  IR_ADC_ZP   0x3A */ 2,
    /*  IR_SBC_ZP   0x3B */ 2,
    /*  IR_AND_ZP   0x3C */ 2,
    /*  IR_ORA_ZP   0x3D */ 2,
    /*  IR_EOR_ZP   0x3E */ 2,
    /*  IR_CMP_ZP   0x3F */ 2,
    /*  IR_CPX_ZP   0x40 */ 2,
    /*  IR_CPY_ZP   0x41 */ 2,

    /*  IR_ADC_ABS  0x42 */ 3,
    /*  IR_SBC_ABS  0x43 */ 3,
    /*  IR_AND_ABS  0x44 */ 3,
    /*  IR_ORA_ABS  0x45 */ 3,
    /*  IR_EOR_ABS  0x46 */ 3,
    /*  IR_CMP_ABS  0x47 */ 3,
    /*  IR_CPX_ABS  0x48 */ 3,
    /*  IR_CPY_ABS  0x49 */ 3,

    /*  IR_INC_ZP   0x4A */ 2,
    /*  IR_DEC_ZP   0x4B */ 2,
    /*  IR_ASL_ZP   0x4C */ 2,
    /*  IR_LSR_ZP   0x4D */ 2,
    /*  IR_ROL_ZP   0x4E */ 2,
    /*  IR_ROR_ZP   0x4F */ 2,

    /*  IR_INC_ABS  0x50 */ 3,
    /*  IR_DEC_ABS  0x51 */ 3,
    /*  IR_ASL_ABS  0x52 */ 3,
    /*  IR_LSR_ABS  0x53 */ 3,
    /*  IR_ROL_ABS  0x54 */ 3,
    /*  IR_ROR_ABS  0x55 */ 3,

    /*  IR_ASL_A    0x56 */ 1,
    /*  IR_LSR_A    0x57 */ 1,
    /*  IR_ROL_A    0x58 */ 1,
    /*  IR_ROR_A    0x59 */ 1,

    /*  IR_LDA_ABSX 0x5A */ 3,
    /*  IR_LDA_ABSY 0x5B */ 3,
    /*  IR_STA_ABSX 0x5C */ 3,
    /*  IR_STA_ABSY 0x5D */ 3,
    /*  IR_ADC_ABSX 0x5E */ 3,
    /*  IR_SBC_ABSX 0x5F */ 3,
    /*  IR_AND_ABSX 0x60 */ 3,
    /*  IR_ORA_ABSX 0x61 */ 3,
    /*  IR_EOR_ABSX 0x62 */ 3,
    /*  IR_CMP_ABSX 0x63 */ 3,
    /*  IR_ADC_ABSY 0x64 */ 3,
    /*  IR_SBC_ABSY 0x65 */ 3,
    /*  IR_AND_ABSY 0x66 */ 3,
    /*  IR_ORA_ABSY 0x67 */ 3,
    /*  IR_EOR_ABSY 0x68 */ 3,
    /*  IR_CMP_ABSY 0x69 */ 3,
    /*  IR_LDX_ABSY 0x6A */ 3,
    /*  IR_LDY_ABSX 0x6B */ 3,
    /*  IR_INC_ABSX 0x6C */ 3,
    /*  IR_DEC_ABSX 0x6D */ 3,
    /*  IR_ASL_ABSX 0x6E */ 3,
    /*  IR_LSR_ABSX 0x6F */ 3,
    /*  IR_ROL_ABSX 0x70 */ 3,
    /*  IR_ROR_ABSX 0x71 */ 3,
    /*  IR_BIT_ZP   0x72 */ 2,
    /*  IR_BIT_ABS  0x73 */ 3,
};

#define IR_NATIVE_SIZE_COUNT (sizeof(ir_native_size) / sizeof(ir_native_size[0]))
#pragma section bank1

/* -------------------------------------------------------------------
 * ir_init — reset context for a new block
 * ------------------------------------------------------------------- */
void ir_init(ir_ctx_t *ctx)
{
    ctx->node_count = 0;
    ctx->label_count = 0;
    ctx->ext_used = 0;
    ctx->tmpl_patch_count = 0;
    ctx->enabled = 1;
    ctx->block_has_jsr = 0;
    ctx->estimated_size = 0;

    /* Reset register shadow */
    ctx->regs.a_known = 0;
    ctx->regs.x_known = 0;
    ctx->regs.y_known = 0;
    ctx->regs.flags_saved = 0;
}

/* -------------------------------------------------------------------
 * ir_emit — append a single IR node
 * ------------------------------------------------------------------- */
uint8_t ir_emit(ir_ctx_t *ctx, uint8_t op, uint8_t flags, uint16_t operand)
{
    if (ctx->node_count >= IR_MAX_NODES)
        return 0;

    ir_node_t *n = &ctx->nodes[ctx->node_count];
    n->op = op;
    n->flags = flags;
    n->operand = operand;
    ctx->node_count++;

    /* Update size estimate */
    if (op < IR_NATIVE_SIZE_COUNT)
        ctx->estimated_size += ir_native_size[op];

    return 1;
}

/* -------------------------------------------------------------------
 * Convenience: emit implied (1-byte) native opcode via IR_RAW_BYTE
 * ------------------------------------------------------------------- */
uint8_t ir_emit_byte(ir_ctx_t *ctx, uint8_t native_opcode)
{
    return ir_emit(ctx, IR_RAW_BYTE, 0, (uint16_t)native_opcode);
}

/* -------------------------------------------------------------------
 * Convenience: emit an IR node with 8-bit immediate operand
 * ------------------------------------------------------------------- */
uint8_t ir_emit_imm(ir_ctx_t *ctx, uint8_t ir_op, uint8_t value)
{
    uint8_t flags = 0;
    /* Auto-set flags for common immediates */
    switch (ir_op) {
        case IR_LDA_IMM: flags = IR_F_WRITES_A | IR_F_WRITES_FLAGS; break;
        case IR_LDX_IMM: flags = IR_F_WRITES_X | IR_F_WRITES_FLAGS; break;
        case IR_LDY_IMM: flags = IR_F_WRITES_Y | IR_F_WRITES_FLAGS; break;
        case IR_ADC_IMM: case IR_SBC_IMM:
            flags = IR_F_READS_A | IR_F_READS_FLAGS | IR_F_WRITES_A | IR_F_WRITES_FLAGS; break;
        case IR_AND_IMM: case IR_ORA_IMM: case IR_EOR_IMM:
            flags = IR_F_READS_A | IR_F_WRITES_A | IR_F_WRITES_FLAGS; break;
        case IR_CMP_IMM: flags = IR_F_READS_A | IR_F_WRITES_FLAGS; break;
        case IR_CPX_IMM: flags = IR_F_READS_X | IR_F_WRITES_FLAGS; break;
        case IR_CPY_IMM: flags = IR_F_READS_Y | IR_F_WRITES_FLAGS; break;
    }
    return ir_emit(ctx, ir_op, flags, (uint16_t)value);
}

/* -------------------------------------------------------------------
 * Convenience: emit an IR node with 8-bit ZP address operand
 * ------------------------------------------------------------------- */
uint8_t ir_emit_zp(ir_ctx_t *ctx, uint8_t ir_op, uint8_t addr)
{
    uint8_t flags = 0;
    switch (ir_op) {
        case IR_LDA_ZP: flags = IR_F_WRITES_A | IR_F_WRITES_FLAGS; break;
        case IR_LDX_ZP: flags = IR_F_WRITES_X | IR_F_WRITES_FLAGS; break;
        case IR_LDY_ZP: flags = IR_F_WRITES_Y | IR_F_WRITES_FLAGS; break;
        case IR_STA_ZP: flags = IR_F_READS_A; break;
        case IR_STX_ZP: flags = IR_F_READS_X; break;
        case IR_STY_ZP: flags = IR_F_READS_Y; break;
        case IR_INC_ZP: case IR_DEC_ZP:
        case IR_ASL_ZP: case IR_LSR_ZP:
        case IR_ROL_ZP: case IR_ROR_ZP:
            flags = IR_F_WRITES_FLAGS; break;
    }
    return ir_emit(ctx, ir_op, flags, (uint16_t)addr);
}

/* -------------------------------------------------------------------
 * Convenience: emit an IR node with 16-bit ABS address operand
 * ------------------------------------------------------------------- */
uint8_t ir_emit_abs(ir_ctx_t *ctx, uint8_t ir_op, uint16_t addr)
{
    uint8_t flags = 0;
    switch (ir_op) {
        case IR_LDA_ABS: case IR_LDA_ABSX: case IR_LDA_ABSY:
            flags = IR_F_WRITES_A | IR_F_WRITES_FLAGS; break;
        case IR_LDX_ABS: case IR_LDX_ABSY:
            flags = IR_F_WRITES_X | IR_F_WRITES_FLAGS; break;
        case IR_LDY_ABS: case IR_LDY_ABSX:
            flags = IR_F_WRITES_Y | IR_F_WRITES_FLAGS; break;
        case IR_STA_ABS: case IR_STA_ABSX: case IR_STA_ABSY:
            flags = IR_F_READS_A; break;
        case IR_STX_ABS: flags = IR_F_READS_X; break;
        case IR_STY_ABS: flags = IR_F_READS_Y; break;
        case IR_JMP_ABS: break;
        case IR_JSR:     break;
        case IR_ADC_ABS: case IR_SBC_ABS: case IR_ADC_ABSX: case IR_SBC_ABSX:
        case IR_ADC_ABSY: case IR_SBC_ABSY:
            flags = IR_F_READS_A | IR_F_READS_FLAGS | IR_F_WRITES_A | IR_F_WRITES_FLAGS; break;
        case IR_AND_ABS: case IR_ORA_ABS: case IR_EOR_ABS:
        case IR_AND_ABSX: case IR_ORA_ABSX: case IR_EOR_ABSX:
        case IR_AND_ABSY: case IR_ORA_ABSY: case IR_EOR_ABSY:
            flags = IR_F_READS_A | IR_F_WRITES_A | IR_F_WRITES_FLAGS; break;
        case IR_CMP_ABS: case IR_CMP_ABSX: case IR_CMP_ABSY:
            flags = IR_F_READS_A | IR_F_WRITES_FLAGS; break;
        case IR_CPX_ABS: flags = IR_F_READS_X | IR_F_WRITES_FLAGS; break;
        case IR_CPY_ABS: flags = IR_F_READS_Y | IR_F_WRITES_FLAGS; break;
        case IR_INC_ABS: case IR_DEC_ABS:
        case IR_ASL_ABS: case IR_LSR_ABS:
        case IR_ROL_ABS: case IR_ROR_ABS:
        case IR_INC_ABSX: case IR_DEC_ABSX:
        case IR_ASL_ABSX: case IR_LSR_ABSX:
        case IR_ROL_ABSX: case IR_ROR_ABSX:
            flags = IR_F_WRITES_FLAGS; break;
        case IR_BIT_ABS: flags = IR_F_READS_A | IR_F_WRITES_FLAGS; break;
    }
    return ir_emit(ctx, ir_op, flags, addr);
}

/* -------------------------------------------------------------------
 * ir_emit_raw_op_abs — raw 6502 opcode + 16-bit operand
 * Pack native opcode into operand[15:8], low addr byte into operand[7:0]
 * Actually we just use IR_RAW_BYTE for the opcode, then IR_RAW_WORD for addr.
 * Simpler: three raw bytes.
 * ------------------------------------------------------------------- */
uint8_t ir_emit_raw_op_abs(ir_ctx_t *ctx, uint8_t native_opcode, uint16_t addr)
{
    if (!ir_emit(ctx, IR_RAW_BYTE, 0, (uint16_t)native_opcode))
        return 0;
    return ir_emit(ctx, IR_RAW_WORD, 0, addr);
}

/* -------------------------------------------------------------------
 * ir_emit_template — record a template blob reference
 * ------------------------------------------------------------------- */
uint8_t ir_emit_template(ir_ctx_t *ctx, uint8_t tmpl_id)
{
    return ir_emit(ctx, IR_TEMPLATE, 0, (uint16_t)tmpl_id);
}

/* -------------------------------------------------------------------
 * ir_add_tmpl_patch — record a patch for a template node
 * ------------------------------------------------------------------- */
uint8_t ir_add_tmpl_patch(ir_ctx_t *ctx, uint8_t tmpl_node_idx,
                          uint8_t byte_offset, uint8_t value)
{
    if (ctx->tmpl_patch_count >= IR_MAX_TMPL_PATCHES)
        return 0;

    ir_tmpl_patch_t *p = &ctx->tmpl_patches[ctx->tmpl_patch_count];
    p->tmpl_node_index = tmpl_node_idx;
    p->byte_offset = byte_offset;
    p->value = value;
    ctx->tmpl_patch_count++;
    return 1;
}

/* -------------------------------------------------------------------
 * ir_define_label — record an intra-block label at the current position
 * ------------------------------------------------------------------- */
uint8_t ir_define_label(ir_ctx_t *ctx, uint8_t *out_label_idx)
{
    if (ctx->label_count >= IR_MAX_LABELS)
        return 0;

    *out_label_idx = ctx->label_count;
    ctx->label_targets[ctx->label_count] = ctx->node_count;
    ctx->label_count++;
    return 1;
}

/* -------------------------------------------------------------------
 * ir_emit_raw_block — emit N raw bytes as individual IR_RAW_BYTE nodes
 * ------------------------------------------------------------------- */
uint8_t ir_emit_raw_block(ir_ctx_t *ctx, const uint8_t *data, uint8_t len)
{
    for (uint8_t i = 0; i < len; i++) {
        if (!ir_emit(ctx, IR_RAW_BYTE, 0, (uint16_t)data[i]))
            return 0;
    }
    return 1;
}

/* -------------------------------------------------------------------
 * ir_estimate_size — quick size estimate without lowering
 * ------------------------------------------------------------------- */
uint8_t ir_estimate_size(const ir_ctx_t *ctx)
{
    /* For now, return the running estimate maintained during recording.
     * This is approximate — templates and raw blocks may differ. */
    return ctx->estimated_size;
}

/* -------------------------------------------------------------------
 * Native 6502 opcode → IR opcode reverse mapping.
 * Maps the 256 possible 6502 opcodes to their IR equivalents.
 * 0 = no mapping (emit as IR_RAW_BYTE passthrough).
 *
 * This table also encodes the instruction size (1, 2, or 3 bytes)
 * so we know how many bytes to consume from the input buffer.
 * Packed as: high nibble = byte count (1/2/3), low byte = IR opcode.
 * We use a separate size lookup to keep it simple.
 * ------------------------------------------------------------------- */

/* Instruction size by native 6502 opcode (1/2/3 bytes, 0=unknown) */
#pragma section rodata1
static const uint8_t native_instr_size[256] = {
/*        0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F */
/* 0 */   1, 2, 0, 0, 0, 2, 2, 0, 1, 2, 1, 0, 0, 3, 3, 0,
/* 1 */   2, 2, 0, 0, 0, 2, 2, 0, 1, 3, 0, 0, 0, 3, 3, 0,
/* 2 */   3, 2, 0, 0, 2, 2, 2, 0, 1, 2, 1, 0, 3, 3, 3, 0,
/* 3 */   2, 2, 0, 0, 0, 2, 2, 0, 1, 3, 0, 0, 0, 3, 3, 0,
/* 4 */   1, 2, 0, 0, 0, 2, 2, 0, 1, 2, 1, 0, 3, 3, 3, 0,
/* 5 */   2, 2, 0, 0, 0, 2, 2, 0, 1, 3, 0, 0, 0, 3, 3, 0,
/* 6 */   1, 2, 0, 0, 0, 2, 2, 0, 1, 2, 1, 0, 3, 3, 3, 0,
/* 7 */   2, 2, 0, 0, 0, 2, 2, 0, 1, 3, 0, 0, 0, 3, 3, 0,
/* 8 */   0, 2, 0, 0, 2, 2, 2, 0, 1, 0, 1, 0, 3, 3, 3, 0,
/* 9 */   2, 2, 0, 0, 2, 2, 2, 0, 1, 3, 1, 0, 0, 3, 0, 0,
/* A */   2, 2, 2, 0, 2, 2, 2, 0, 1, 2, 1, 0, 3, 3, 3, 0,
/* B */   2, 2, 0, 0, 2, 2, 2, 0, 1, 3, 1, 0, 3, 3, 3, 0,
/* C */   2, 2, 0, 0, 2, 2, 2, 0, 1, 2, 1, 0, 3, 3, 3, 0,
/* D */   2, 2, 0, 0, 0, 2, 2, 0, 1, 3, 0, 0, 0, 3, 3, 0,
/* E */   2, 2, 0, 0, 2, 2, 2, 0, 1, 2, 1, 0, 3, 3, 3, 0,
/* F */   2, 2, 0, 0, 0, 2, 2, 0, 1, 3, 0, 0, 0, 3, 3, 0,
};

/* Native opcode → IR opcode mapping (0 = no mapping, use raw bytes) */
static const uint8_t native_to_ir[256] = {
/*        0          1          2          3          4          5          6          7  */
/* 0 */   IR_BRK,    0,         0,         0,         0,         IR_ORA_ZP, IR_ASL_ZP, 0,
/* 0 */   IR_PHP,    IR_ORA_IMM,IR_ASL_A,  0,         0,         IR_ORA_ABS,IR_ASL_ABS,0,
/* 1 */   IR_BPL,    0,         0,         0,         0,         0,         0,         0,
/* 1 */   IR_CLC,    IR_ORA_ABSY,0,        0,         0,         IR_ORA_ABSX,IR_ASL_ABSX,0,
/* 2 */   IR_JSR,    0,         0,         0,         IR_BIT_ZP, IR_AND_ZP, IR_ROL_ZP, 0,
/* 2 */   IR_PLP,    IR_AND_IMM,IR_ROL_A,  0,         IR_BIT_ABS,IR_AND_ABS,IR_ROL_ABS,0,
/* 3 */   IR_BMI,    0,         0,         0,         0,         0,         0,         0,
/* 3 */   IR_SEC,    IR_AND_ABSY,0,        0,         0,         IR_AND_ABSX,IR_ROL_ABSX,0,
/* 4 */   0,         0,         0,         0,         0,         IR_EOR_ZP, IR_LSR_ZP, 0,
/* 4 */   IR_PHA,    IR_EOR_IMM,IR_LSR_A,  0,         IR_JMP_ABS,IR_EOR_ABS,IR_LSR_ABS,0,
/* 5 */   IR_BVC,    0,         0,         0,         0,         0,         0,         0,
/* 5 */   IR_CLI,    IR_EOR_ABSY,0,        0,         0,         IR_EOR_ABSX,IR_LSR_ABSX,0,
/* 6 */   IR_RTS,    0,         0,         0,         0,         IR_ADC_ZP, IR_ROR_ZP, 0,
/* 6 */   IR_PLA,    IR_ADC_IMM,IR_ROR_A,  0,         0,         IR_ADC_ABS,IR_ROR_ABS,0,
/* 7 */   IR_BVS,    0,         0,         0,         0,         0,         0,         0,
/* 7 */   IR_SEI,    IR_ADC_ABSY,0,        0,         0,         IR_ADC_ABSX,IR_ROR_ABSX,0,
/* 8 */   0,         0,         0,         0,         IR_STY_ZP, IR_STA_ZP, IR_STX_ZP, 0,
/* 8 */   IR_DEY,    0,         IR_TXA,    0,         IR_STY_ABS,IR_STA_ABS,IR_STX_ABS,0,
/* 9 */   IR_BCC,    0,         0,         0,         0,         0,         0,         0,
/* 9 */   IR_TYA,    IR_STA_ABSY,IR_TXS,   0,         0,         IR_STA_ABSX,0,        0,
/* A */   IR_LDY_IMM,0,         IR_LDX_IMM,0,         IR_LDY_ZP, IR_LDA_ZP, IR_LDX_ZP, 0,
/* A */   IR_TAY,    IR_LDA_IMM,IR_TAX,    0,         IR_LDY_ABS,IR_LDA_ABS,IR_LDX_ABS,0,
/* B */   IR_BCS,    0,         0,         0,         IR_LDY_ABSX,0,         0,         0,
/* B */   IR_CLV,    IR_LDA_ABSY,IR_TSX,   0,         IR_LDY_ABSX,IR_LDA_ABSX,IR_LDX_ABSY,0,
/* C */   IR_CPY_IMM,0,         0,         0,         IR_CPY_ZP, IR_CMP_ZP, IR_DEC_ZP, 0,
/* C */   IR_INY,    IR_CMP_IMM,IR_DEX,    0,         IR_CPY_ABS,IR_CMP_ABS,IR_DEC_ABS,0,
/* D */   IR_BNE,    0,         0,         0,         0,         0,         0,         0,
/* D */   IR_CLD,    IR_CMP_ABSY,0,        0,         0,         IR_CMP_ABSX,IR_DEC_ABSX,0,
/* E */   IR_CPX_IMM,0,         0,         0,         IR_CPX_ZP, IR_SBC_ZP, IR_INC_ZP, 0,
/* E */   IR_INX,    IR_SBC_IMM,IR_NOP,    0,         IR_CPX_ABS,IR_SBC_ABS,IR_INC_ABS,0,
/* F */   IR_BEQ,    0,         0,         0,         0,         0,         0,         0,
/* F */   IR_SED,    IR_SBC_ABSY,0,        0,         0,         IR_SBC_ABSX,IR_INC_ABSX,0,
};
#pragma section bank1

/* -------------------------------------------------------------------
 * ir_record_from_buffer — scan raw 6502 bytes, populate IR nodes.
 *
 * Walks the byte buffer left-to-right, decoding each native 6502
 * instruction and emitting the corresponding IR node.  Unrecognised
 * opcodes or sequences are passed through as IR_RAW_BYTE nodes so
 * the output is always a faithful representation of the input.
 *
 * Returns the number of IR nodes recorded.
 * ------------------------------------------------------------------- */
uint8_t ir_record_from_buffer(ir_ctx_t *ctx, const uint8_t *buf, uint8_t len)
{
    uint8_t pos = 0;
    uint8_t start_count = ctx->node_count;

    while (pos < len) {
        uint8_t opcode = buf[pos];
        uint8_t sz = native_instr_size[opcode];
        uint8_t ir_op = native_to_ir[opcode];

        /* Check we have enough bytes for the full instruction */
        if (sz == 0 || (pos + sz) > len) {
            /* Unknown opcode or truncated — emit as raw byte(s) */
            ir_emit(ctx, IR_RAW_BYTE, 0, (uint16_t)opcode);
            pos++;
            continue;
        }

        if (ir_op == 0) {
            /* No IR mapping — emit as raw bytes */
            for (uint8_t i = 0; i < sz; i++)
                ir_emit(ctx, IR_RAW_BYTE, 0, (uint16_t)buf[pos + i]);
            pos += sz;
            continue;
        }

        /* Construct the operand from instruction bytes */
        uint16_t operand = 0;
        if (sz == 2) {
            operand = (uint16_t)buf[pos + 1];
        } else if (sz == 3) {
            operand = (uint16_t)buf[pos + 1] | ((uint16_t)buf[pos + 2] << 8);
        }

        /* Use the convenience emitter for flag annotation */
        if (sz == 1) {
            /* Implied: emit with appropriate flags */
            uint8_t flags = 0;
            switch (ir_op) {
                case IR_TAX: flags = IR_F_READS_A | IR_F_WRITES_X | IR_F_WRITES_FLAGS; break;
                case IR_TAY: flags = IR_F_READS_A | IR_F_WRITES_Y | IR_F_WRITES_FLAGS; break;
                case IR_TXA: flags = IR_F_READS_X | IR_F_WRITES_A | IR_F_WRITES_FLAGS; break;
                case IR_TYA: flags = IR_F_READS_Y | IR_F_WRITES_A | IR_F_WRITES_FLAGS; break;
                case IR_TSX: flags = IR_F_WRITES_X | IR_F_WRITES_FLAGS; break;
                case IR_TXS: flags = IR_F_READS_X; break;
                case IR_INX: case IR_DEX: flags = IR_F_READS_X | IR_F_WRITES_X | IR_F_WRITES_FLAGS; break;
                case IR_INY: case IR_DEY: flags = IR_F_READS_Y | IR_F_WRITES_Y | IR_F_WRITES_FLAGS; break;
                case IR_PHA: flags = IR_F_READS_A; break;
                case IR_PLA: flags = IR_F_WRITES_A | IR_F_WRITES_FLAGS; break;
                case IR_PHP: flags = IR_F_READS_FLAGS; break;
                case IR_PLP: flags = IR_F_WRITES_FLAGS; break;
                case IR_CLC: case IR_SEC: case IR_CLD: case IR_SED:
                case IR_CLI: case IR_SEI: case IR_CLV:
                    flags = IR_F_WRITES_FLAGS; break;
                case IR_ASL_A: case IR_LSR_A: case IR_ROL_A: case IR_ROR_A:
                    flags = IR_F_READS_A | IR_F_WRITES_A | IR_F_WRITES_FLAGS; break;
                case IR_RTS: case IR_BRK: case IR_NOP: break;
            }
            ir_emit(ctx, ir_op, flags, 0);
        } else if (sz == 2 && ir_op >= IR_BPL && ir_op <= IR_BEQ) {
            /* Branch: operand is the raw signed offset */
            ir_emit(ctx, ir_op, IR_F_READS_FLAGS, operand);
        } else if (sz == 2) {
            /* IMM or ZP: use convenience emitters for flag annotation */
            /* Check if it's an immediate or ZP based on IR_op range */
            if ((ir_op >= IR_LDA_IMM && ir_op <= IR_LDY_IMM) ||
                (ir_op >= IR_ADC_IMM && ir_op <= IR_CPY_IMM)) {
                ir_emit_imm(ctx, ir_op, (uint8_t)operand);
            } else {
                ir_emit_zp(ctx, ir_op, (uint8_t)operand);
            }
        } else {
            /* ABS (3-byte) */
            ir_emit_abs(ctx, ir_op, operand);
        }

        pos += sz;
    }

    return ctx->node_count - start_count;
}

#pragma section default
