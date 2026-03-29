/**
 * emit_6502.h - NES 6502 code emitter interface
 * 
 * Generates native 6502 code for the NES target.
 */

#ifndef EMIT_6502_H
#define EMIT_6502_H

#include <stdint.h>

// Emit context - tracks current output position
typedef struct {
    uint8_t *buffer;        // Output buffer (cache_code[])
    uint8_t  offset;        // Current position in buffer
    uint8_t  max_size;      // Buffer limit
} emit_ctx_t;

// Initialize emitter context
void emit_init(emit_ctx_t *ctx, uint8_t *buffer, uint8_t max_size);

// Low-level byte emission
void emit_byte(emit_ctx_t *ctx, uint8_t b);
void emit_word(emit_ctx_t *ctx, uint16_t w);

// 6502 opcode constants
#define OP_BRK      0x00
#define OP_NOP      0xEA
#define OP_LDA_IMM  0xA9
#define OP_LDA_ZP   0xA5
#define OP_LDA_ABS  0xAD
#define OP_LDX_IMM  0xA2
#define OP_LDX_ZP   0xA6
#define OP_LDX_ABS  0xAE
#define OP_LDY_IMM  0xA0
#define OP_LDY_ZP   0xA4
#define OP_LDY_ABS  0xAC
#define OP_STA_ZP   0x85
#define OP_STA_ABS  0x8D
#define OP_STX_ZP   0x86
#define OP_STX_ABS  0x8E
#define OP_STY_ZP   0x84
#define OP_STY_ABS  0x8C
#define OP_JMP_ABS  0x4C
#define OP_JMP_IND  0x6C
#define OP_JSR      0x20
#define OP_RTS      0x60
#define OP_RTI      0x40
#define OP_PHP      0x08
#define OP_PLP      0x28
#define OP_PHA      0x48
#define OP_PLA      0x68
#define OP_BPL      0x10
#define OP_BMI      0x30
#define OP_BVC      0x50
#define OP_BVS      0x70
#define OP_BCC      0x90
#define OP_BCS      0xB0
#define OP_BNE      0xD0
#define OP_BEQ      0xF0
#define OP_CLC      0x18
#define OP_SEC      0x38
#define OP_CLI      0x58
#define OP_SEI      0x78
#define OP_CLV      0xB8
#define OP_CLD      0xD8
#define OP_SED      0xF8

// Instruction emitters
void emit_lda_imm(emit_ctx_t *ctx, uint8_t val);
void emit_lda_zp(emit_ctx_t *ctx, uint8_t addr);
void emit_lda_abs(emit_ctx_t *ctx, uint16_t addr);
void emit_ldx_imm(emit_ctx_t *ctx, uint8_t val);
void emit_ldx_zp(emit_ctx_t *ctx, uint8_t addr);
void emit_ldx_abs(emit_ctx_t *ctx, uint16_t addr);
void emit_ldy_imm(emit_ctx_t *ctx, uint8_t val);
void emit_ldy_zp(emit_ctx_t *ctx, uint8_t addr);
void emit_ldy_abs(emit_ctx_t *ctx, uint16_t addr);

void emit_sta_zp(emit_ctx_t *ctx, uint8_t addr);
void emit_sta_abs(emit_ctx_t *ctx, uint16_t addr);
void emit_stx_zp(emit_ctx_t *ctx, uint8_t addr);
void emit_stx_abs(emit_ctx_t *ctx, uint16_t addr);
void emit_sty_zp(emit_ctx_t *ctx, uint8_t addr);
void emit_sty_abs(emit_ctx_t *ctx, uint16_t addr);

void emit_jmp_abs(emit_ctx_t *ctx, uint16_t addr);
void emit_jsr(emit_ctx_t *ctx, uint16_t addr);
void emit_rts(emit_ctx_t *ctx);

void emit_branch(emit_ctx_t *ctx, uint8_t opcode, int8_t offset);

void emit_nop(emit_ctx_t *ctx);
void emit_php(emit_ctx_t *ctx);
void emit_plp(emit_ctx_t *ctx);
void emit_pha(emit_ctx_t *ctx);
void emit_pla(emit_ctx_t *ctx);

// Block epilogue - saves state and returns to dispatcher
void emit_epilogue(emit_ctx_t *ctx, uint16_t exit_pc, 
                   uint8_t a_addr, uint8_t pc_lo_addr, uint8_t pc_hi_addr,
                   void (*return_func)(void));

// Copy raw bytes
void emit_copy(emit_ctx_t *ctx, const uint8_t *src, uint8_t len);

// Code snippets (templates for complex operations)
extern const uint8_t snippet_pha[];
extern const uint8_t snippet_pha_size;
extern const uint8_t snippet_pla[];
extern const uint8_t snippet_pla_size;
extern const uint8_t snippet_indy_template[];
extern const uint8_t snippet_indy_size;
extern const uint8_t snippet_indx_template[];
extern const uint8_t snippet_indx_size;

#endif // EMIT_6502_H
