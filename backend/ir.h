/**
 * ir.h - Block-level intermediate representation for pre-flash optimization
 *
 * Sits between guest-opcode decoding (recompile_opcode_b2) and flash writes.
 * Records semantic IR nodes instead of raw bytes; optimization passes run
 * over the IR before lowering to native 6502 code.
 *
 * Design constraints:
 *   - ~512 bytes new WRAM for the IR buffer (fits in ~2.4 KB free)
 *   - 96 nodes max (covers worst-case 250-byte output at ~2.6 B avg/instr)
 *   - 4 bytes per node, fixed-size for simplicity
 *   - Guest-CPU agnostic: any frontend produces IR nodes via ir_emit_*()
 *   - Optimizer passes are target-centric (NES 6502 native ops)
 */

#ifndef IR_H
#define IR_H

#include <stdint.h>

/* ===================================================================
 * IR opcodes — target-centric (NES 6502 native)
 * =================================================================== */

/* Native 6502 instructions (1:1 mapping) */
#define IR_LDA_IMM      0x01
#define IR_LDA_ZP       0x02
#define IR_LDA_ABS      0x03
#define IR_LDX_IMM      0x04
#define IR_LDX_ZP       0x05
#define IR_LDX_ABS      0x06
#define IR_LDY_IMM      0x07
#define IR_LDY_ZP       0x08
#define IR_LDY_ABS      0x09
#define IR_STA_ZP       0x0A
#define IR_STA_ABS      0x0B
#define IR_STX_ZP       0x0C
#define IR_STX_ABS      0x0D
#define IR_STY_ZP       0x0E
#define IR_STY_ABS      0x0F

#define IR_JMP_ABS      0x10
#define IR_JSR           0x11
#define IR_RTS           0x12
#define IR_PHP           0x13
#define IR_PLP           0x14
#define IR_PHA           0x15
#define IR_PLA           0x16
#define IR_NOP           0x17
#define IR_CLC           0x18
#define IR_SEC           0x19
#define IR_CLD           0x1A
#define IR_SED           0x1B
#define IR_CLI           0x1C
#define IR_SEI           0x1D
#define IR_CLV           0x1E
#define IR_BRK           0x1F

/* Branch instructions (operand = target label index) */
#define IR_BPL           0x20
#define IR_BMI           0x21
#define IR_BVC           0x22
#define IR_BVS           0x23
#define IR_BCC           0x24
#define IR_BCS           0x25
#define IR_BNE           0x26
#define IR_BEQ           0x27

/* Transfer instructions */
#define IR_TAX           0x28
#define IR_TAY           0x29
#define IR_TXA           0x2A
#define IR_TYA           0x2B
#define IR_TSX           0x2C
#define IR_TXS           0x2D
#define IR_INX           0x2E
#define IR_INY           0x2F
#define IR_DEX           0x30
#define IR_DEY           0x31

/* ALU with immediate (operand = immediate value) */
#define IR_ADC_IMM      0x32
#define IR_SBC_IMM      0x33
#define IR_AND_IMM      0x34
#define IR_ORA_IMM      0x35
#define IR_EOR_IMM      0x36
#define IR_CMP_IMM      0x37
#define IR_CPX_IMM      0x38
#define IR_CPY_IMM      0x39

/* ALU with ZP (operand = zp address) */
#define IR_ADC_ZP       0x3A
#define IR_SBC_ZP       0x3B
#define IR_AND_ZP       0x3C
#define IR_ORA_ZP       0x3D
#define IR_EOR_ZP       0x3E
#define IR_CMP_ZP       0x3F
#define IR_CPX_ZP       0x40
#define IR_CPY_ZP       0x41

/* ALU with ABS (operand = abs address) */
#define IR_ADC_ABS      0x42
#define IR_SBC_ABS      0x43
#define IR_AND_ABS      0x44
#define IR_ORA_ABS      0x45
#define IR_EOR_ABS      0x46
#define IR_CMP_ABS      0x47
#define IR_CPX_ABS      0x48
#define IR_CPY_ABS      0x49

/* Read-modify-write ZP */
#define IR_INC_ZP       0x4A
#define IR_DEC_ZP       0x4B
#define IR_ASL_ZP       0x4C
#define IR_LSR_ZP       0x4D
#define IR_ROL_ZP       0x4E
#define IR_ROR_ZP       0x4F

/* Read-modify-write ABS */
#define IR_INC_ABS      0x50
#define IR_DEC_ABS      0x51
#define IR_ASL_ABS      0x52
#define IR_LSR_ABS      0x53
#define IR_ROL_ABS      0x54
#define IR_ROR_ABS      0x55

/* Accumulator-mode RMW */
#define IR_ASL_A        0x56
#define IR_LSR_A        0x57
#define IR_ROL_A        0x58
#define IR_ROR_A        0x59

/* Indexed modes (operand = translated abs address; index implied by opcode) */
#define IR_LDA_ABSX     0x5A
#define IR_LDA_ABSY     0x5B
#define IR_STA_ABSX     0x5C
#define IR_STA_ABSY     0x5D
#define IR_ADC_ABSX     0x5E
#define IR_SBC_ABSX     0x5F
#define IR_AND_ABSX     0x60
#define IR_ORA_ABSX     0x61
#define IR_EOR_ABSX     0x62
#define IR_CMP_ABSX     0x63
#define IR_ADC_ABSY     0x64
#define IR_SBC_ABSY     0x65
#define IR_AND_ABSY     0x66
#define IR_ORA_ABSY     0x67
#define IR_EOR_ABSY     0x68
#define IR_CMP_ABSY     0x69
#define IR_LDX_ABSY     0x6A
#define IR_LDY_ABSX     0x6B
#define IR_INC_ABSX     0x6C
#define IR_DEC_ABSX     0x6D
#define IR_ASL_ABSX     0x6E
#define IR_LSR_ABSX     0x6F
#define IR_ROL_ABSX     0x70
#define IR_ROR_ABSX     0x71
#define IR_BIT_ZP       0x72
#define IR_BIT_ABS      0x73

/* ---------------------------------------------------------------
 * Semantic pseudo-ops (higher-level intent, not 1:1 with opcodes)
 * --------------------------------------------------------------- */

/* Opaque template blob — operand = template ID (see IR_TMPL_* below).
 * The template bytes are stored in ir_operand_ext[] sidecar.
 * Optimizer treats these as black boxes in phase 1. */
#define IR_TEMPLATE      0xE0

/* Raw byte emission — operand[7:0] = the byte.
 * Used for 1:1 passthrough of any opcode the IR doesn't model yet. */
#define IR_RAW_BYTE      0xE1

/* Emit a raw 6502 opcode whose operand is the full 16-bit in node.operand.
 * op field contains the real 6502 opcode byte in operand[15:8].
 * Used for opcode + abs_addr pairs that don't have a dedicated IR op. */
#define IR_RAW_OP_ABS    0xE2

/* Two raw bytes — operand[7:0] = byte1, operand[15:8] = byte2 */
#define IR_RAW_WORD      0xE3

/* Dead node — removed by optimizer, skipped during lowering */
#define IR_DEAD          0xFF

/* ===================================================================
 * IR node flags (bitfield in ir_node_t.flags)
 * =================================================================== */
#define IR_F_READS_A     0x01
#define IR_F_READS_X     0x02
#define IR_F_READS_Y     0x04
#define IR_F_READS_FLAGS 0x08
#define IR_F_WRITES_A    0x10
#define IR_F_WRITES_X    0x20
#define IR_F_WRITES_Y    0x40
#define IR_F_WRITES_FLAGS 0x80

/* ===================================================================
 * Template IDs for IR_TEMPLATE nodes
 * =================================================================== */
#define IR_TMPL_PHA          0   /* opcode_6502_pha (~12-13 bytes) */
#define IR_TMPL_PLA          1   /* opcode_6502_pla (~12-13 bytes) */
#define IR_TMPL_PHP          2   /* opcode_6502_php (~15-19 bytes) */
#define IR_TMPL_PLP          3   /* opcode_6502_plp (~12-18 bytes) */
#define IR_TMPL_JSR          4   /* opcode_6502_jsr (34 bytes) */
#define IR_TMPL_NJSR         5   /* opcode_6502_njsr (36 bytes) */
#define IR_TMPL_NRTS         6   /* opcode_6502_nrts (32 bytes) */
#define IR_TMPL_INDY_READ    7   /* addr_6502_indy template */
#define IR_TMPL_STA_INDY     8   /* sta_indy_template */
#define IR_TMPL_NATIVE_STA_INDY 9 /* native_sta_indy_tmpl */
#define IR_TMPL_INDX         10  /* addr_6502_indx template */
#define IR_TMPL_BRANCH_PATCHABLE 11 /* 21-byte patchable branch pattern */
#define IR_TMPL_JMP_PATCHABLE    12 /* 9-byte patchable JMP pattern */
#define IR_TMPL_COUNT        13

/* ===================================================================
 * IR node structure — 4 bytes, fixed-size
 * =================================================================== */
typedef struct {
    uint8_t  op;       /* IR opcode (IR_LDA_IMM, IR_TEMPLATE, etc.) */
    uint8_t  flags;    /* IR_F_* bitfield */
    uint16_t operand;  /* immediate, address, template ID, label index, etc. */
} ir_node_t;

/* ===================================================================
 * IR context — holds the node buffer and state for one block
 * =================================================================== */

#define IR_MAX_NODES     96    /* 96 × 4 = 384 bytes */
#define IR_MAX_LABELS    16    /* intra-block branch target labels */
#define IR_MAX_EXT       64    /* operand extension sidecar bytes */
#define IR_MAX_TMPL_PATCHES 8  /* template patch entries */

/* Template patch: records a (offset, value) pair for template byte patching */
typedef struct {
    uint8_t tmpl_node_index;  /* which IR_TEMPLATE node this patch applies to */
    uint8_t byte_offset;      /* offset within the template blob */
    uint8_t value;            /* byte value to write at that offset */
    uint8_t pad;              /* alignment */
} ir_tmpl_patch_t;

/* Register shadow state for optimizer */
typedef struct {
    uint8_t a_val, x_val, y_val;     /* last-known register values */
    uint8_t a_known, x_known, y_known; /* 1 = value is valid */
    uint8_t flags_saved;              /* 1 = PHP was emitted, flags on stack */
    uint8_t pad;
} ir_reg_shadow_t;

typedef struct {
    /* Node buffer */
    ir_node_t nodes[IR_MAX_NODES];
    uint8_t   node_count;

    /* Label table: label_targets[i] = node index of label i */
    uint8_t   label_targets[IR_MAX_LABELS];
    uint8_t   label_count;

    /* Operand extension sidecar — variable-length data for templates */
    uint8_t   ext[IR_MAX_EXT];
    uint8_t   ext_used;

    /* Template patches */
    ir_tmpl_patch_t tmpl_patches[IR_MAX_TMPL_PATCHES];
    uint8_t   tmpl_patch_count;

    /* Bookkeeping */
    uint8_t   enabled;        /* 1 = recording IR, 0 = bypass (old path) */
    uint8_t   block_has_jsr;  /* JSR/NJSR seen in this block */
    uint8_t   estimated_size; /* running estimate of lowered byte count */

    /* Optimizer state */
    ir_reg_shadow_t regs;

    /* Per-sub-optimization change counters (incremented in-pass) */
    uint8_t stat_redundant_load;  /* Pass 1: redundant loads + identity + fold */
    uint8_t stat_dead_store;      /* Pass 2: dead store + store-back          */
    uint8_t stat_dead_load;       /* Pass 2b: dead load elimination           */
    uint8_t stat_php_plp;         /* Pass 3: PLP/PHP pairs removed            */
    uint8_t stat_pair_rewrite;    /* Pass 4: pair rewrites + CMP #0           */
} ir_ctx_t;

/* ===================================================================
 * IR API — recording
 * =================================================================== */

/* Initialize/reset the IR context for a new block */
void ir_init(ir_ctx_t *ctx);

/* Emit a single IR node (returns 0 on overflow) */
uint8_t ir_emit(ir_ctx_t *ctx, uint8_t op, uint8_t flags, uint16_t operand);

/* Convenience emitters for common patterns */
uint8_t ir_emit_byte(ir_ctx_t *ctx, uint8_t native_opcode);
uint8_t ir_emit_imm(ir_ctx_t *ctx, uint8_t ir_op, uint8_t value);
uint8_t ir_emit_zp(ir_ctx_t *ctx, uint8_t ir_op, uint8_t addr);
uint8_t ir_emit_abs(ir_ctx_t *ctx, uint8_t ir_op, uint16_t addr);

/* Emit a raw native opcode + abs operand (for opcodes not yet in the IR enum) */
uint8_t ir_emit_raw_op_abs(ir_ctx_t *ctx, uint8_t native_opcode, uint16_t addr);

/* Emit a template blob reference.
 * tmpl_id = IR_TMPL_* constant.
 * The actual template bytes are resolved at lowering time from ROM arrays. */
uint8_t ir_emit_template(ir_ctx_t *ctx, uint8_t tmpl_id);

/* Record a template patch (to be applied during lowering) */
uint8_t ir_add_tmpl_patch(ir_ctx_t *ctx, uint8_t tmpl_node_idx,
                          uint8_t byte_offset, uint8_t value);

/* Define a label at the current node position (for intra-block branches) */
uint8_t ir_define_label(ir_ctx_t *ctx, uint8_t *out_label_idx);

/* Emit a raw block of bytes (for patchable patterns etc.) */
uint8_t ir_emit_raw_block(ir_ctx_t *ctx, const uint8_t *data, uint8_t len);

/* ===================================================================
 * IR API — optimization (see ir_opt.c)
 * =================================================================== */

/* Run all enabled optimization passes on the IR. Returns bytes saved. */
uint8_t ir_optimize(ir_ctx_t *ctx);

/* Individual passes (called by ir_optimize, or directly for testing) */
uint8_t ir_opt_redundant_load(ir_ctx_t *ctx);
uint8_t ir_opt_dead_store(ir_ctx_t *ctx);
uint8_t ir_opt_dead_load(ir_ctx_t *ctx);
uint8_t ir_opt_php_plp_elision(ir_ctx_t *ctx);
uint8_t ir_opt_pair_rewrite(ir_ctx_t *ctx);

/* ===================================================================
 * IR API — lowering (see ir_lower.c)
 * =================================================================== */

/* Lower IR to native bytes in output buffer.
 * Returns the number of bytes written (0 on error).
 * output_buf must be at least 256 bytes. */
uint8_t ir_lower(ir_ctx_t *ctx, uint8_t *output_buf, uint8_t max_size);

/* Estimate the lowered size without actually emitting (for budget checks).
 * Faster than ir_lower but approximate. */
uint8_t ir_estimate_size(const ir_ctx_t *ctx);

/* ===================================================================
 * IR API — buffer recording (see ir.c)
 * =================================================================== */

/* Scan a raw 6502 byte buffer and populate IR nodes.
 * This is the post-hoc recording path: recompile_opcode_b2 emits raw bytes
 * to cache_code[], then this function replays them into the IR for
 * optimization.  Each recognized instruction becomes an IR node; unknown
 * byte sequences are recorded as IR_RAW_BYTE nodes (passthrough).
 * Returns the number of IR nodes recorded. */
uint8_t ir_record_from_buffer(ir_ctx_t *ctx, const uint8_t *buf, uint8_t len);

#endif /* IR_H */
