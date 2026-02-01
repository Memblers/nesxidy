/**
 * emit_6502.c - NES 6502 code emitter implementation
 * 
 * Generates native 6502 code for the NES target.
 */

#pragma section bank1

#include <stdint.h>
#include "emit_6502.h"

// Initialize emitter context
void emit_init(emit_ctx_t *ctx, uint8_t *buffer, uint8_t max_size) {
    ctx->buffer = buffer;
    ctx->offset = 0;
    ctx->max_size = max_size;
}

// Low-level byte emission
void emit_byte(emit_ctx_t *ctx, uint8_t b) {
    if (ctx->offset < ctx->max_size) {
        ctx->buffer[ctx->offset++] = b;
    }
}

void emit_word(emit_ctx_t *ctx, uint16_t w) {
    emit_byte(ctx, (uint8_t)(w & 0xFF));
    emit_byte(ctx, (uint8_t)(w >> 8));
}

// LDA instructions
void emit_lda_imm(emit_ctx_t *ctx, uint8_t val) {
    emit_byte(ctx, OP_LDA_IMM);
    emit_byte(ctx, val);
}

void emit_lda_zp(emit_ctx_t *ctx, uint8_t addr) {
    emit_byte(ctx, OP_LDA_ZP);
    emit_byte(ctx, addr);
}

void emit_lda_abs(emit_ctx_t *ctx, uint16_t addr) {
    emit_byte(ctx, OP_LDA_ABS);
    emit_word(ctx, addr);
}

// LDX instructions
void emit_ldx_imm(emit_ctx_t *ctx, uint8_t val) {
    emit_byte(ctx, OP_LDX_IMM);
    emit_byte(ctx, val);
}

void emit_ldx_zp(emit_ctx_t *ctx, uint8_t addr) {
    emit_byte(ctx, OP_LDX_ZP);
    emit_byte(ctx, addr);
}

void emit_ldx_abs(emit_ctx_t *ctx, uint16_t addr) {
    emit_byte(ctx, OP_LDX_ABS);
    emit_word(ctx, addr);
}

// LDY instructions
void emit_ldy_imm(emit_ctx_t *ctx, uint8_t val) {
    emit_byte(ctx, OP_LDY_IMM);
    emit_byte(ctx, val);
}

void emit_ldy_zp(emit_ctx_t *ctx, uint8_t addr) {
    emit_byte(ctx, OP_LDY_ZP);
    emit_byte(ctx, addr);
}

void emit_ldy_abs(emit_ctx_t *ctx, uint16_t addr) {
    emit_byte(ctx, OP_LDY_ABS);
    emit_word(ctx, addr);
}

// STA instructions
void emit_sta_zp(emit_ctx_t *ctx, uint8_t addr) {
    emit_byte(ctx, OP_STA_ZP);
    emit_byte(ctx, addr);
}

void emit_sta_abs(emit_ctx_t *ctx, uint16_t addr) {
    emit_byte(ctx, OP_STA_ABS);
    emit_word(ctx, addr);
}

// STX instructions
void emit_stx_zp(emit_ctx_t *ctx, uint8_t addr) {
    emit_byte(ctx, OP_STX_ZP);
    emit_byte(ctx, addr);
}

void emit_stx_abs(emit_ctx_t *ctx, uint16_t addr) {
    emit_byte(ctx, OP_STX_ABS);
    emit_word(ctx, addr);
}

// STY instructions
void emit_sty_zp(emit_ctx_t *ctx, uint8_t addr) {
    emit_byte(ctx, OP_STY_ZP);
    emit_byte(ctx, addr);
}

void emit_sty_abs(emit_ctx_t *ctx, uint16_t addr) {
    emit_byte(ctx, OP_STY_ABS);
    emit_word(ctx, addr);
}

// Jump/Call instructions
void emit_jmp_abs(emit_ctx_t *ctx, uint16_t addr) {
    emit_byte(ctx, OP_JMP_ABS);
    emit_word(ctx, addr);
}

void emit_jsr(emit_ctx_t *ctx, uint16_t addr) {
    emit_byte(ctx, OP_JSR);
    emit_word(ctx, addr);
}

void emit_rts(emit_ctx_t *ctx) {
    emit_byte(ctx, OP_RTS);
}

// Branch instruction
void emit_branch(emit_ctx_t *ctx, uint8_t opcode, int8_t offset) {
    emit_byte(ctx, opcode);
    emit_byte(ctx, (uint8_t)offset);
}

// Single-byte instructions
void emit_nop(emit_ctx_t *ctx) {
    emit_byte(ctx, OP_NOP);
}

void emit_php(emit_ctx_t *ctx) {
    emit_byte(ctx, OP_PHP);
}

void emit_plp(emit_ctx_t *ctx) {
    emit_byte(ctx, OP_PLP);
}

void emit_pha(emit_ctx_t *ctx) {
    emit_byte(ctx, OP_PHA);
}

void emit_pla(emit_ctx_t *ctx) {
    emit_byte(ctx, OP_PLA);
}

/**
 * Emit block epilogue - saves CPU state and returns to dispatcher
 * 
 * Generated code:
 *   STA a_addr          ; Save A register
 *   PHP                 ; Push status
 *   LDA #<exit_pc       ; Load exit PC low byte
 *   STA pc_lo_addr      ; Store to PC low
 *   LDA #>exit_pc       ; Load exit PC high byte  
 *   STA pc_hi_addr      ; Store to PC high
 *   JMP return_func     ; Jump to dispatcher return
 */
void emit_epilogue(emit_ctx_t *ctx, uint16_t exit_pc,
                   uint8_t a_addr, uint8_t pc_lo_addr, uint8_t pc_hi_addr,
                   void (*return_func)(void)) {
    // STA a_addr
    emit_byte(ctx, OP_STA_ZP);
    emit_byte(ctx, a_addr);
    
    // PHP
    emit_byte(ctx, OP_PHP);
    
    // LDA #<exit_pc
    emit_byte(ctx, OP_LDA_IMM);
    emit_byte(ctx, (uint8_t)exit_pc);
    
    // STA pc_lo_addr
    emit_byte(ctx, OP_STA_ZP);
    emit_byte(ctx, pc_lo_addr);
    
    // LDA #>exit_pc
    emit_byte(ctx, OP_LDA_IMM);
    emit_byte(ctx, (uint8_t)(exit_pc >> 8));
    
    // STA pc_hi_addr
    emit_byte(ctx, OP_STA_ZP);
    emit_byte(ctx, pc_hi_addr);
    
    // JMP return_func
    emit_byte(ctx, OP_JMP_ABS);
    emit_word(ctx, (uint16_t)return_func);
}

// Copy raw bytes to output
void emit_copy(emit_ctx_t *ctx, const uint8_t *src, uint8_t len) {
    for (uint8_t i = 0; i < len && ctx->offset < ctx->max_size; i++) {
        ctx->buffer[ctx->offset++] = src[i];
    }
}
