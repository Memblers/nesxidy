/**
 * ir_lower.c - IR to native 6502 byte lowering
 *
 * Walks the optimized ir_nodes[] array, skips IR_DEAD nodes, and emits
 * concrete 6502 opcodes + operands into an output buffer (cache_code[]).
 * Resolves intra-block labels for branch fixups.
 *
 * Lives in bank 2 (compile-time only).
 */

#pragma section bank1

#include <stdint.h>
#include "ir.h"
#include "../dynamos.h"   /* template externs, ROM array sizes */

/* ===================================================================
 * IR opcode → native 6502 opcode mapping table
 *
 * For IR ops that map 1:1 to a real 6502 instruction, this table
 * gives the native opcode byte.  0 = needs special handling.
 * =================================================================== */
#pragma section rodata1
static const uint8_t ir_to_native[] = {
    /*  0x00 */ 0x00, /* unused */
    /*  IR_LDA_IMM  0x01 */ 0xA9,
    /*  IR_LDA_ZP   0x02 */ 0xA5,
    /*  IR_LDA_ABS  0x03 */ 0xAD,
    /*  IR_LDX_IMM  0x04 */ 0xA2,
    /*  IR_LDX_ZP   0x05 */ 0xA6,
    /*  IR_LDX_ABS  0x06 */ 0xAE,
    /*  IR_LDY_IMM  0x07 */ 0xA0,
    /*  IR_LDY_ZP   0x08 */ 0xA4,
    /*  IR_LDY_ABS  0x09 */ 0xAC,
    /*  IR_STA_ZP   0x0A */ 0x85,
    /*  IR_STA_ABS  0x0B */ 0x8D,
    /*  IR_STX_ZP   0x0C */ 0x86,
    /*  IR_STX_ABS  0x0D */ 0x8E,
    /*  IR_STY_ZP   0x0E */ 0x84,
    /*  IR_STY_ABS  0x0F */ 0x8C,

    /*  IR_JMP_ABS  0x10 */ 0x4C,
    /*  IR_JSR      0x11 */ 0x20,
    /*  IR_RTS      0x12 */ 0x60,
    /*  IR_PHP      0x13 */ 0x08,
    /*  IR_PLP      0x14 */ 0x28,
    /*  IR_PHA      0x15 */ 0x48,
    /*  IR_PLA      0x16 */ 0x68,
    /*  IR_NOP      0x17 */ 0xEA,
    /*  IR_CLC      0x18 */ 0x18,
    /*  IR_SEC      0x19 */ 0x38,
    /*  IR_CLD      0x1A */ 0xD8,
    /*  IR_SED      0x1B */ 0xF8,
    /*  IR_CLI      0x1C */ 0x58,
    /*  IR_SEI      0x1D */ 0x78,
    /*  IR_CLV      0x1E */ 0xB8,
    /*  IR_BRK      0x1F */ 0x00,

    /*  IR_BPL      0x20 */ 0x10,
    /*  IR_BMI      0x21 */ 0x30,
    /*  IR_BVC      0x22 */ 0x50,
    /*  IR_BVS      0x23 */ 0x70,
    /*  IR_BCC      0x24 */ 0x90,
    /*  IR_BCS      0x25 */ 0xB0,
    /*  IR_BNE      0x26 */ 0xD0,
    /*  IR_BEQ      0x27 */ 0xF0,

    /*  IR_TAX      0x28 */ 0xAA,
    /*  IR_TAY      0x29 */ 0xA8,
    /*  IR_TXA      0x2A */ 0x8A,
    /*  IR_TYA      0x2B */ 0x98,
    /*  IR_TSX      0x2C */ 0xBA,
    /*  IR_TXS      0x2D */ 0x9A,
    /*  IR_INX      0x2E */ 0xE8,
    /*  IR_INY      0x2F */ 0xC8,
    /*  IR_DEX      0x30 */ 0xCA,
    /*  IR_DEY      0x31 */ 0x88,

    /*  IR_ADC_IMM  0x32 */ 0x69,
    /*  IR_SBC_IMM  0x33 */ 0xE9,
    /*  IR_AND_IMM  0x34 */ 0x29,
    /*  IR_ORA_IMM  0x35 */ 0x09,
    /*  IR_EOR_IMM  0x36 */ 0x49,
    /*  IR_CMP_IMM  0x37 */ 0xC9,
    /*  IR_CPX_IMM  0x38 */ 0xE0,
    /*  IR_CPY_IMM  0x39 */ 0xC0,

    /*  IR_ADC_ZP   0x3A */ 0x65,
    /*  IR_SBC_ZP   0x3B */ 0xE5,
    /*  IR_AND_ZP   0x3C */ 0x25,
    /*  IR_ORA_ZP   0x3D */ 0x05,
    /*  IR_EOR_ZP   0x3E */ 0x45,
    /*  IR_CMP_ZP   0x3F */ 0xC5,
    /*  IR_CPX_ZP   0x40 */ 0xE4,
    /*  IR_CPY_ZP   0x41 */ 0xC4,

    /*  IR_ADC_ABS  0x42 */ 0x6D,
    /*  IR_SBC_ABS  0x43 */ 0xED,
    /*  IR_AND_ABS  0x44 */ 0x2D,
    /*  IR_ORA_ABS  0x45 */ 0x0D,
    /*  IR_EOR_ABS  0x46 */ 0x4D,
    /*  IR_CMP_ABS  0x47 */ 0xCD,
    /*  IR_CPX_ABS  0x48 */ 0xEC,
    /*  IR_CPY_ABS  0x49 */ 0xCC,

    /*  IR_INC_ZP   0x4A */ 0xE6,
    /*  IR_DEC_ZP   0x4B */ 0xC6,
    /*  IR_ASL_ZP   0x4C */ 0x06,
    /*  IR_LSR_ZP   0x4D */ 0x46,
    /*  IR_ROL_ZP   0x4E */ 0x26,
    /*  IR_ROR_ZP   0x4F */ 0x66,

    /*  IR_INC_ABS  0x50 */ 0xEE,
    /*  IR_DEC_ABS  0x51 */ 0xCE,
    /*  IR_ASL_ABS  0x52 */ 0x0E,
    /*  IR_LSR_ABS  0x53 */ 0x4E,
    /*  IR_ROL_ABS  0x54 */ 0x2E,
    /*  IR_ROR_ABS  0x55 */ 0x6E,

    /*  IR_ASL_A    0x56 */ 0x0A,
    /*  IR_LSR_A    0x57 */ 0x4A,
    /*  IR_ROL_A    0x58 */ 0x2A,
    /*  IR_ROR_A    0x59 */ 0x6A,

    /*  IR_LDA_ABSX 0x5A */ 0xBD,
    /*  IR_LDA_ABSY 0x5B */ 0xB9,
    /*  IR_STA_ABSX 0x5C */ 0x9D,
    /*  IR_STA_ABSY 0x5D */ 0x99,
    /*  IR_ADC_ABSX 0x5E */ 0x7D,
    /*  IR_SBC_ABSX 0x5F */ 0xFD,
    /*  IR_AND_ABSX 0x60 */ 0x3D,
    /*  IR_ORA_ABSX 0x61 */ 0x1D,
    /*  IR_EOR_ABSX 0x62 */ 0x5D,
    /*  IR_CMP_ABSX 0x63 */ 0xDD,
    /*  IR_ADC_ABSY 0x64 */ 0x79,
    /*  IR_SBC_ABSY 0x65 */ 0xF9,
    /*  IR_AND_ABSY 0x66 */ 0x39,
    /*  IR_ORA_ABSY 0x67 */ 0x19,
    /*  IR_EOR_ABSY 0x68 */ 0x59,
    /*  IR_CMP_ABSY 0x69 */ 0xD9,
    /*  IR_LDX_ABSY 0x6A */ 0xBE,
    /*  IR_LDY_ABSX 0x6B */ 0xBC,
    /*  IR_INC_ABSX 0x6C */ 0xFE,
    /*  IR_DEC_ABSX 0x6D */ 0xDE,
    /*  IR_ASL_ABSX 0x6E */ 0x1E,
    /*  IR_LSR_ABSX 0x6F */ 0x5E,
    /*  IR_ROL_ABSX 0x70 */ 0x3E,
    /*  IR_ROR_ABSX 0x71 */ 0x7E,
    /*  IR_BIT_ZP   0x72 */ 0x24,
    /*  IR_BIT_ABS  0x73 */ 0x2C,
};

#define IR_TO_NATIVE_COUNT (sizeof(ir_to_native) / sizeof(ir_to_native[0]))
#pragma section bank1

/* ===================================================================
 * Template pointer table — resolves IR_TMPL_* IDs to byte arrays
 * =================================================================== */
typedef struct {
    uint8_t *data;   /* pointer to template byte array in ROM/WRAM */
    uint8_t  size;   /* template size (read from extern const) */
} tmpl_entry_t;

/* Helper to get template entry — can't use static initializer with
 * address-of in vbcc, so we build on each call (cheap). */
static void get_template(uint8_t tmpl_id, uint8_t **out_data, uint8_t *out_size)
{
    switch (tmpl_id) {
        case IR_TMPL_PHA:
            *out_data = opcode_6502_pha;
            *out_size = opcode_6502_pha_size;
            return;
        case IR_TMPL_PLA:
            *out_data = opcode_6502_pla;
            *out_size = opcode_6502_pla_size;
            return;
        case IR_TMPL_PHP:
            *out_data = opcode_6502_php;
            *out_size = opcode_6502_php_size;
            return;
        case IR_TMPL_PLP:
            *out_data = opcode_6502_plp;
            *out_size = opcode_6502_plp_size;
            return;
        case IR_TMPL_JSR:
            *out_data = opcode_6502_jsr;
            *out_size = opcode_6502_jsr_size;
            return;
        case IR_TMPL_NJSR:
            *out_data = opcode_6502_njsr;
            *out_size = opcode_6502_njsr_size;
            return;
        case IR_TMPL_NRTS:
            *out_data = opcode_6502_nrts;
            *out_size = opcode_6502_nrts_size;
            return;
        case IR_TMPL_INDY_READ:
            *out_data = addr_6502_indy;
            *out_size = addr_6502_indy_size;
            return;
        case IR_TMPL_STA_INDY:
            *out_data = sta_indy_template;
            *out_size = sta_indy_template_size;
            return;
        case IR_TMPL_INDX:
            *out_data = addr_6502_indx;
            *out_size = addr_6502_indx_size;
            return;
        default:
            *out_data = 0;
            *out_size = 0;
            return;
    }
}

/* ===================================================================
 * Determine native byte size of an IR node (for offset calculation)
 * =================================================================== */
static uint8_t node_byte_size(const ir_ctx_t *ctx, uint8_t idx)
{
    const ir_node_t *n = &ctx->nodes[idx];
    uint8_t op = n->op;

    if (op == IR_DEAD) return 0;
    if (op == IR_PC_MARK) return 0;  /* fence — zero native bytes */
    if (op == IR_RAW_BYTE) return 1;
    if (op == IR_RAW_WORD) return 2;

    if (op == IR_TEMPLATE) {
        uint8_t *data;
        uint8_t size;
        get_template((uint8_t)n->operand, &data, &size);
        return size;
    }

    /* Standard instructions: 1-byte implied, 2-byte imm/zp/branch, 3-byte abs */
    if (op >= 0x01 && op < IR_TO_NATIVE_COUNT) {
        /* Implied (1-byte) */
        if ((op >= IR_RTS && op <= IR_BRK) ||     /* 0x12..0x1F */
            (op >= IR_TAX && op <= IR_DEY) ||      /* 0x28..0x31 */
            (op >= IR_ASL_A && op <= IR_ROR_A))    /* 0x56..0x59 */
            return 1;
        /* Branch (2-byte) */
        if (op >= IR_BPL && op <= IR_BEQ)
            return 2;
        /* Immediate (2-byte) */
        if (op == IR_LDA_IMM || op == IR_LDX_IMM || op == IR_LDY_IMM ||
            (op >= IR_ADC_IMM && op <= IR_CPY_IMM))
            return 2;
        /* ZP (2-byte) */
        if (op == IR_LDA_ZP || op == IR_LDX_ZP || op == IR_LDY_ZP ||
            op == IR_STA_ZP || op == IR_STX_ZP || op == IR_STY_ZP ||
            (op >= IR_ADC_ZP && op <= IR_CPY_ZP) ||
            (op >= IR_INC_ZP && op <= IR_ROR_ZP) ||
            op == IR_BIT_ZP)
            return 2;
        /* Everything else: ABS (3-byte) */
        return 3;
    }

    /* RAW_OP_ABS: 1 (opcode was emitted as RAW_BYTE) + 2 (addr as RAW_WORD) = handled separately */
    if (op == IR_RAW_OP_ABS) return 3;

    return 0;
}

/* ===================================================================
 * ir_lower — convert optimized IR to native bytes
 * =================================================================== */
uint8_t ir_lower(ir_ctx_t *ctx, uint8_t *output_buf, uint8_t max_size)
{
    uint8_t pos = 0;  /* output byte position */

    /* --- Pre-pass: compute byte offsets for each node (for branch fixups) --- */
    uint8_t node_offsets[IR_MAX_NODES];
    {
        uint8_t offset = 0;
        for (uint8_t i = 0; i < ctx->node_count; i++) {
            node_offsets[i] = offset;
            offset += node_byte_size(ctx, i);
        }
    }

    /* --- Main lowering pass --- */
    for (uint8_t i = 0; i < ctx->node_count; i++) {
        ir_node_t *n = &ctx->nodes[i];
        uint8_t op = n->op;

        if (op == IR_DEAD) continue;

        /* --- PC fence (zero bytes, record native offset) --- */
        if (op == IR_PC_MARK) {
            /* Find this node's fence slot and record the current pos */
            for (uint8_t f = 0; f < ctx->fence_count; f++) {
                if (ctx->fence_node_idx[f] == i) {
                    ctx->fence_native_offset[f] = pos;
                    break;
                }
            }
            continue;
        }

        /* --- Raw byte --- */
        if (op == IR_RAW_BYTE) {
            if (pos >= max_size) return 0;
            output_buf[pos++] = (uint8_t)n->operand;
            continue;
        }

        /* --- Raw word (2 bytes, little-endian) --- */
        if (op == IR_RAW_WORD) {
            if (pos + 1 >= max_size) return 0;
            output_buf[pos++] = (uint8_t)(n->operand & 0xFF);
            output_buf[pos++] = (uint8_t)(n->operand >> 8);
            continue;
        }

        /* --- Template blob --- */
        if (op == IR_TEMPLATE) {
            uint8_t *tmpl_data;
            uint8_t tmpl_size;
            get_template((uint8_t)n->operand, &tmpl_data, &tmpl_size);
            if (!tmpl_data || (pos + tmpl_size) > max_size) return 0;

            /* Copy template bytes */
            for (uint8_t t = 0; t < tmpl_size; t++)
                output_buf[pos + t] = tmpl_data[t];

            /* Apply template patches */
            for (uint8_t p = 0; p < ctx->tmpl_patch_count; p++) {
                if (ctx->tmpl_patches[p].tmpl_node_index == i) {
                    uint8_t off = ctx->tmpl_patches[p].byte_offset;
                    if (off < tmpl_size)
                        output_buf[pos + off] = ctx->tmpl_patches[p].value;
                }
            }

            pos += tmpl_size;
            continue;
        }

        /* --- Branch instructions (need offset fixup) --- */
        if (op >= IR_BPL && op <= IR_BEQ) {
            if (pos + 1 >= max_size) return 0;
            output_buf[pos] = ir_to_native[op];

            /* operand is a raw signed offset for now (pre-computed by caller).
             * In the future, if operand is a label index, resolve here:
             *   int16_t target = node_offsets[ctx->label_targets[n->operand]];
             *   int8_t rel = (int8_t)(target - (node_offsets[i] + 2));
             */
            output_buf[pos + 1] = (uint8_t)n->operand;
            pos += 2;
            continue;
        }

        /* --- Standard mapped instructions --- */
        if (op >= 0x01 && op < IR_TO_NATIVE_COUNT) {
            uint8_t native_op = ir_to_native[op];
            uint8_t sz = node_byte_size(ctx, i);

            if (pos + sz > max_size) return 0;

            output_buf[pos] = native_op;
            if (sz == 2) {
                output_buf[pos + 1] = (uint8_t)(n->operand & 0xFF);
            } else if (sz == 3) {
                output_buf[pos + 1] = (uint8_t)(n->operand & 0xFF);
                output_buf[pos + 2] = (uint8_t)(n->operand >> 8);
            }
            pos += sz;
            continue;
        }

        /* Unknown op — skip (shouldn't happen) */
    }

    return pos;
}

/* ===================================================================
 * ir_resolve_deferred_patches — Phase B post-lowering patch resolution
 *
 * After ir_lower() rewrites cache_code[0][], patchable templates
 * (21-byte branch, 9-byte JMP, $FFF0 variants) may have shifted
 * position.  This function scans the lowered buffer for patchable
 * JMP patterns, matches them 1:1 (in order) with the deferred_patches[]
 * table, and records the correct flash addresses via
 * opt2_record_pending_branch_safe.
 *
 * Pattern signatures:
 *   Old templates:  $4C $FF $FF  (JMP $FFFF)
 *   $FFF0 templates: $4C $F0 $FF  (JMP $FFF0)
 *
 * Must be called while bank 1 is mapped (lives in bank1 code).
 * Accesses WRAM globals directly to avoid argument-passing overhead
 * in the fixed bank (which has zero headroom).
 * =================================================================== */
void ir_resolve_deferred_patches(void)
{
    extern ir_ctx_t ir_ctx;
    extern uint8_t cache_code[][CACHE_CODE_BUF_SIZE];
    extern __zpage uint8_t code_index;
    extern uint16_t flash_code_address;
    extern uint8_t flash_code_bank;
    extern void opt2_record_pending_branch_safe(
        uint16_t, uint16_t, uint8_t, uint16_t, uint8_t);

    if (ir_ctx.deferred_patch_count == 0)
        return;

    uint8_t dp_idx = 0;
    uint8_t len = code_index;
    uint8_t *buf = cache_code[0];

    for (uint8_t p = 0; (uint8_t)(p + 2) < len && dp_idx < ir_ctx.deferred_patch_count; p++) {
        uint8_t type = ir_ctx.deferred_patches[dp_idx].is_branch;

#ifdef ENABLE_FFF0_TEMPLATES
        // $FFF0 templates: scan for $4C $F0 $FF (JMP $FFF0)
        if ((type == DEFERRED_PATCH_FFF0_BRANCH || type == DEFERRED_PATCH_FFF0_JMP)
            && buf[p] == 0x4C
            && buf[p + 1] == (FFF0_DISPATCH & 0xFF)
            && buf[p + 2] == ((FFF0_DISPATCH >> 8) & 0xFF))
        {
            uint16_t jmp_op_addr = flash_code_address + BLOCK_PREFIX_SIZE + p + 1;
            // $FFF0 templates: branch_addr = 0 (no branch byte to patch)
            opt2_record_pending_branch_safe(
                0, jmp_op_addr,
                flash_code_bank,
                ir_ctx.deferred_patches[dp_idx].target_pc, 0);
            dp_idx++;
            continue;
        }
#endif

        // Old templates: scan for $4C $FF $FF (JMP $FFFF)
        if (buf[p] == 0x4C && buf[p + 1] == 0xFF && buf[p + 2] == 0xFF) {
            uint16_t jmp_op_addr = flash_code_address + BLOCK_PREFIX_SIZE + p + 1;
            if (type == DEFERRED_PATCH_BRANCH_OLD) {
                /* 21-byte branch template: branch offset byte at JMP - 1 */
                uint16_t br_off_addr = flash_code_address + BLOCK_PREFIX_SIZE + p - 1;
                opt2_record_pending_branch_safe(
                    br_off_addr, jmp_op_addr,
                    flash_code_bank,
                    ir_ctx.deferred_patches[dp_idx].target_pc, 0);
            } else {
                /* 9-byte JMP template: BCC offset byte at JMP - 2 */
                uint16_t bcc_op_addr = flash_code_address + BLOCK_PREFIX_SIZE + p - 2;
                opt2_record_pending_branch_safe(
                    bcc_op_addr, jmp_op_addr,
                    flash_code_bank,
                    ir_ctx.deferred_patches[dp_idx].target_pc, 0);
            }
            dp_idx++;
        }
    }
}

/* ===================================================================
 * Pass 5: RMW fusion (post-pass, called from dynamos.c after
 * ir_optimize completes, before ir_lower)
 *
 * Patterns:
 *   LDA zp/abs, ASL/LSR/ROL/ROR A, STA zp/abs -> shift zp/abs
 *   LDX zp/abs, INX/DEX, STX zp/abs -> INC/DEC zp/abs
 *   LDY zp/abs, INY/DEY, STY zp/abs -> INC/DEC zp/abs
 * =================================================================== */

/* Inline barrier check: branch or opaque pseudo-op */
#define IS_BARRIER(op) \
    ((op) == IR_JMP_ABS || (op) == IR_JSR || (op) == IR_RTS || \
     ((op) >= IR_BPL && (op) <= IR_BEQ) || (op) >= IR_TEMPLATE)

/* ===================================================================
 * Pass 6: Non-adjacent register substitution
 * (from opt65.c opti2: STA tmp,...,LDA tmp → TAX,...,TXA)
 *
 * When a register is saved to a ZP temp and later reloaded, and a
 * surrogate register is free between store and load AND dead after
 * the load, replace the memory round-trip with a transfer pair.
 * Saves 2 bytes per hit (2×2B ZP ops → 2×1B transfers).
 *
 * Runs as a post-pass in bank1 (after iterative loop, before lower).
 * Uses node flags bitfield directly, no bank0 helpers needed.
 * =================================================================== */

/* ZP-mode opcode check — any instruction whose operand is a ZP address */
#define IS_ZP_ACCESS(op) \
    ((op) == IR_LDA_ZP || (op) == IR_LDX_ZP || (op) == IR_LDY_ZP || \
     (op) == IR_STA_ZP || (op) == IR_STX_ZP || (op) == IR_STY_ZP || \
     ((op) >= IR_ADC_ZP && (op) <= IR_CPY_ZP) || \
     ((op) >= IR_INC_ZP && (op) <= IR_ROR_ZP) || \
     (op) == IR_BIT_ZP)

/* Pure store (no read of old value) */
#define IS_ZP_PURE_STORE(op) \
    ((op) == IR_STA_ZP || (op) == IR_STX_ZP || (op) == IR_STY_ZP)

static uint8_t ir_opt_reg_subst(ir_ctx_t *ctx)
{
    uint8_t changes = 0;
    ir_node_t *nd = ctx->nodes;
    uint8_t nc = ctx->node_count;

    for (uint8_t i = 0; i < nc; i++) {
        uint8_t sop = nd[i].op;
        uint8_t lop;

        if (sop == IR_STA_ZP) lop = IR_LDA_ZP;
        else if (sop == IR_STX_ZP) lop = IR_LDX_ZP;
        else if (sop == IR_STY_ZP) lop = IR_LDY_ZP;
        else continue;

        uint8_t addr = (uint8_t)nd[i].operand;

        /* Forward scan: find matching load, track surrogate freedom.
         * Uses node flags bitfield for register checks. */
        uint8_t j, found = 0;
        uint8_t a_ok = 1, x_ok = 1, y_ok = 1;

        for (j = i + 1; j < nc; j++) {
            uint8_t op = nd[j].op;
            if (op == IR_DEAD) continue;
            if (IS_BARRIER(op)) break;
            if (op == lop && (uint8_t)nd[j].operand == addr)
                { found = 1; break; }
            if (IS_ZP_ACCESS(op) && (uint8_t)nd[j].operand == addr) break;
            if (nd[j].flags & 0x11) a_ok = 0;  /* touches A */
            if (nd[j].flags & 0x22) x_ok = 0;  /* touches X */
            if (nd[j].flags & 0x44) y_ok = 0;  /* touches Y */
        }
        if (!found) continue;

        /* Quick gate: any usable surrogate? */
        if (sop == IR_STA_ZP && !x_ok && !y_ok) continue;
        if (sop != IR_STA_ZP && !a_ok) continue;

        /* Flags safe: transfer sets N,Z which store doesn't.
         * Scan from i+1: if flags are read before written, bail. */
        { uint8_t fsafe = 0;
          for (uint8_t k = i + 1; k < nc; k++) {
              if (nd[k].op == IR_DEAD) continue;
              if (nd[k].flags & IR_F_READS_FLAGS) break;
              if (nd[k].flags & IR_F_WRITES_FLAGS) { fsafe = 1; break; }
          }
          if (!fsafe) continue;
        }

        /* No labels in (i, j] */
        { uint8_t ok = 1;
          for (uint8_t k = 0; k < ctx->label_count; k++)
              if (ctx->label_targets[k] > i && ctx->label_targets[k] <= j)
                  { ok = 0; break; }
          if (!ok) continue;
        }

        /* Post-load scan: ZP addr must be dead (not read before
         * overwritten) AND surrogate register must be dead after j. */
        { uint8_t alive = 0, adone = 0;
          uint8_t ad = 0, xd = 0, yd = 0; /* 0=unknown, 1=dead, 2=live */
          for (uint8_t k = j + 1; k < nc; k++) {
              uint8_t op = nd[k].op;
              uint8_t fl = nd[k].flags;
              if (op == IR_DEAD) continue;
              if (IS_BARRIER(op)) {
                  if (!adone) alive = 1;
                  if (!ad) ad = 2; if (!xd) xd = 2; if (!yd) yd = 2;
                  break;
              }
              if (!adone && IS_ZP_ACCESS(op) &&
                  (uint8_t)nd[k].operand == addr) {
                  adone = 1;
                  if (!IS_ZP_PURE_STORE(op)) alive = 1;
              }
              if (!ad) { if (fl & 0x01) ad = 2; else if (fl & 0x10) ad = 1; }
              if (!xd) { if (fl & 0x02) xd = 2; else if (fl & 0x20) xd = 1; }
              if (!yd) { if (fl & 0x04) yd = 2; else if (fl & 0x40) yd = 1; }
              if (alive) break;
          }
          /* End-of-block fallthrough: conservatively assume live.
           * Epilogue reads all regs; ZP may be read by later blocks. */
          if (!adone) alive = 1;
          if (!ad) ad = 2;
          if (!xd) xd = 2;
          if (!yd) yd = 2;
          if (alive) continue;

          /* Choose surrogate: STA uses X or Y; STX/STY uses A.
           * 1=written-first(dead), 2=read(live) */
          uint8_t sv, rv, sf, rf;
          if (sop == IR_STA_ZP) {
              if      (x_ok && xd != 2)
                  { sv = IR_TAX; sf = 0xA1; rv = IR_TXA; rf = 0x92; }
              else if (y_ok && yd != 2)
                  { sv = IR_TAY; sf = 0xC1; rv = IR_TYA; rf = 0x94; }
              else continue;
          } else if (sop == IR_STX_ZP) {
              if (ad == 2) continue;
              sv = IR_TXA; sf = 0x92; rv = IR_TAX; rf = 0xA1;
          } else {
              if (ad == 2) continue;
              sv = IR_TYA; sf = 0x94; rv = IR_TAY; rf = 0xC1;
          }

          nd[i].op = sv; nd[i].flags = sf; nd[i].operand = 0;
          nd[j].op = rv; nd[j].flags = rf; nd[j].operand = 0;
          changes++;
        }
    }

    return changes;
}

/* ===================================================================
 * Pass 5: RMW fusion (read-modify-write pattern collapsing)
 * =================================================================== */

uint8_t ir_opt_rmw_fusion(ir_ctx_t *ctx)
{
    /* Pass 6: register substitution — runs before RMW patterns */
    ctx->stat_pair_rewrite += ir_opt_reg_subst(ctx);

    uint8_t changes = 0;
    ir_node_t *nd = ctx->nodes;
    uint8_t nc = ctx->node_count;

    for (uint8_t i = 0; i < nc; i++) {
        uint8_t op1 = nd[i].op;
        if (op1 == IR_DEAD) continue;

        /* Find next two non-dead nodes */
        uint8_t j, k;
        for (j = i + 1; j < nc && nd[j].op == IR_DEAD; j++);
        if (j >= nc) break;
        for (k = j + 1; k < nc && nd[k].op == IR_DEAD; k++);
        if (k >= nc) break;

        uint8_t op2 = nd[j].op;
        uint8_t op3 = nd[k].op;
        uint16_t addr = nd[i].operand;

        /* --- Shift fusion: LDA zp/abs, shift_A, STA zp/abs ---
         * Condition: same address, A dead after store. */
        if ((op1 == IR_LDA_ZP || op1 == IR_LDA_ABS) &&
            op2 >= IR_ASL_A && op2 <= IR_ROR_A &&
            op3 == ((op1 == IR_LDA_ZP) ? IR_STA_ZP : IR_STA_ABS) &&
            nd[k].operand == addr) {
            /* Check A dead after via flags bitfield */
            uint8_t a_dead = 1;
            { uint8_t m; for (m = k + 1; m < nc; m++) {
                uint8_t fop = nd[m].op;
                if (fop == IR_DEAD) continue;
                if (nd[m].flags & IR_F_READS_A) { a_dead = 0; break; }
                if (nd[m].flags & IR_F_WRITES_A) break;
                if (IS_BARRIER(fop)) { a_dead = 0; break; }
            } }
            if (a_dead) {
                uint8_t base = (op1 == IR_LDA_ZP) ? IR_ASL_ZP : IR_ASL_ABS;
                nd[i].op = base + (op2 - IR_ASL_A);
                nd[i].flags = IR_F_WRITES_FLAGS;
                if (op2 == IR_ROL_A || op2 == IR_ROR_A)
                    nd[i].flags |= IR_F_READS_FLAGS;
                nd[j].op = IR_DEAD; nd[j].flags = 0; nd[j].operand = 0;
                nd[k].op = IR_DEAD; nd[k].flags = 0; nd[k].operand = 0;
                changes += 2;
                continue;
            }
        }

        /* --- Inc/Dec fusion via X or Y ---
         * Merged X and Y patterns to share the rewrite logic. */
        {
            uint8_t match = 0, is_zp = 0, inc = 0;
            uint8_t r_flag = 0, w_flag = 0;
            uint8_t ld_zp = 0, ld_abs = 0;

            if ((op1 == IR_LDX_ZP || op1 == IR_LDX_ABS) &&
                (op2 == IR_INX || op2 == IR_DEX)) {
                is_zp = (op1 == IR_LDX_ZP);
                if (op3 == (is_zp ? IR_STX_ZP : IR_STX_ABS) &&
                    nd[k].operand == addr) {
                    match = 1; inc = (op2 == IR_INX);
                    r_flag = IR_F_READS_X; w_flag = IR_F_WRITES_X;
                    ld_zp = IR_LDX_ZP; ld_abs = IR_LDX_ABS;
                }
            }
            if (!match &&
                (op1 == IR_LDY_ZP || op1 == IR_LDY_ABS) &&
                (op2 == IR_INY || op2 == IR_DEY)) {
                is_zp = (op1 == IR_LDY_ZP);
                if (op3 == (is_zp ? IR_STY_ZP : IR_STY_ABS) &&
                    nd[k].operand == addr) {
                    match = 1; inc = (op2 == IR_INY);
                    r_flag = IR_F_READS_Y; w_flag = IR_F_WRITES_Y;
                    ld_zp = IR_LDY_ZP; ld_abs = IR_LDY_ABS;
                }
            }

            if (match) {
                uint8_t rmw = inc ? (is_zp ? IR_INC_ZP : IR_INC_ABS)
                                  : (is_zp ? IR_DEC_ZP : IR_DEC_ABS);
                /* Check register liveness via flags bitfield */
                uint8_t reg_live = 0;
                { uint8_t m; for (m = k + 1; m < nc; m++) {
                    uint8_t fop = nd[m].op;
                    if (fop == IR_DEAD) continue;
                    if (nd[m].flags & r_flag) { reg_live = 1; break; }
                    if (nd[m].flags & w_flag) break;
                    if (IS_BARRIER(fop)) { reg_live = 1; break; }
                } }
                nd[i].op = rmw;
                nd[i].flags = IR_F_WRITES_FLAGS;
                nd[j].op = IR_DEAD; nd[j].flags = 0; nd[j].operand = 0;
                if (reg_live) {
                    nd[k].op = is_zp ? ld_zp : ld_abs;
                    nd[k].flags = w_flag | IR_F_WRITES_FLAGS;
                    changes++;
                } else {
                    nd[k].op = IR_DEAD; nd[k].flags = 0; nd[k].operand = 0;
                    changes += 2;
                }
                continue;
            }
        }
    }

    return changes;
}

#pragma section default
