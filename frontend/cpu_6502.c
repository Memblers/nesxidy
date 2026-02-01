/**
 * cpu_6502.c - 6502 CPU frontend implementation
 * 
 * Decodes 6502 instructions for the recompiler.
 * Provides the addrmodes table and decode functions.
 */

#pragma section bank1

#include <stdint.h>
#include <stdbool.h>
#include "cpu_frontend.h"
#include "../platform/platform.h"

// Use the same enum values as dynamos.h:
// enum address_modes { imp=0, acc=1, imm=2, zp=3, zpx=4, zpy=5, rel=6, abso=7, absx=8, absy=9, ind=10, indx=11, indy=12 };
#define imp   0
#define acc   1
#define imm   2
#define zp    3
#define zpx   4
#define zpy   5
#define rel   6
#define abso  7
#define absx  8
#define absy  9
#define ind  10
#define indx 11
#define indy 12

// Addressing mode table - exported for dynamos.c to use
// This is THE canonical table, dynamos.c should use this one
const uint8_t cpu_6502_addrmodes[256] = {
/*        |  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |  A  |  B  |  C  |  D  |  E  |  F  |     */
/* 0 */     imp, indx,  imp, indx,   zp,   zp,   zp,   zp,  imp,  imm,  acc,  imm, abso, abso, abso, abso, /* 0 */
/* 1 */     rel, indy,  imp, indy,  zpx,  zpx,  zpx,  zpx,  imp, absy,  imp, absy, absx, absx, absx, absx, /* 1 */
/* 2 */    abso, indx,  imp, indx,   zp,   zp,   zp,   zp,  imp,  imm,  acc,  imm, abso, abso, abso, abso, /* 2 */
/* 3 */     rel, indy,  imp, indy,  zpx,  zpx,  zpx,  zpx,  imp, absy,  imp, absy, absx, absx, absx, absx, /* 3 */
/* 4 */     imp, indx,  imp, indx,   zp,   zp,   zp,   zp,  imp,  imm,  acc,  imm, abso, abso, abso, abso, /* 4 */
/* 5 */     rel, indy,  imp, indy,  zpx,  zpx,  zpx,  zpx,  imp, absy,  imp, absy, absx, absx, absx, absx, /* 5 */
/* 6 */     imp, indx,  imp, indx,   zp,   zp,   zp,   zp,  imp,  imm,  acc,  imm,  ind, abso, abso, abso, /* 6 */
/* 7 */     rel, indy,  imp, indy,  zpx,  zpx,  zpx,  zpx,  imp, absy,  imp, absy, absx, absx, absx, absx, /* 7 */
/* 8 */     imm, indx,  imm, indx,   zp,   zp,   zp,   zp,  imp,  imm,  imp,  imm, abso, abso, abso, abso, /* 8 */
/* 9 */     rel, indy,  imp, indy,  zpx,  zpx,  zpy,  zpy,  imp, absy,  imp, absy, absx, absx, absy, absy, /* 9 */
/* A */     imm, indx,  imm, indx,   zp,   zp,   zp,   zp,  imp,  imm,  imp,  imm, abso, abso, abso, abso, /* A */
/* B */     rel, indy,  imp, indy,  zpx,  zpx,  zpy,  zpy,  imp, absy,  imp, absy, absx, absx, absy, absy, /* B */
/* C */     imm, indx,  imm, indx,   zp,   zp,   zp,   zp,  imp,  imm,  imp,  imm, abso, abso, abso, abso, /* C */
/* D */     rel, indy,  imp, indy,  zpx,  zpx,  zpx,  zpx,  imp, absy,  imp, absy, absx, absx, absx, absx, /* D */
/* E */     imm, indx,  imm, indx,   zp,   zp,   zp,   zp,  imp,  imm,  imp,  imm, abso, abso, abso, abso, /* E */
/* F */     rel, indy,  imp, indy,  zpx,  zpx,  zpx,  zpx,  imp, absy,  imp, absy, absx, absx, absx, absx  /* F */
};

#undef imp
#undef acc
#undef imm
#undef zp
#undef zpx
#undef zpy
#undef rel
#undef abso
#undef absx
#undef absy
#undef ind
#undef indx
#undef indy

// Instruction length by addressing mode value
static uint8_t get_length_from_mode(uint8_t mode) {
    switch (mode) {
        case 0:   // imp
        case 1:   // acc
            return 1;
        case 2:   // imm
        case 3:   // zp
        case 4:   // zpx
        case 5:   // zpy
        case 6:   // rel
        case 11:  // indx
        case 12:  // indy
            return 2;
        case 7:   // abso
        case 8:   // absx
        case 9:   // absy
        case 10:  // ind
            return 3;
        default:
            return 1;
    }
}

// 6502 opcode constants for control flow detection
#define OP_BRK 0x00
#define OP_JSR 0x20
#define OP_RTI 0x40
#define OP_RTS 0x60
#define OP_JMP_ABS 0x4C
#define OP_JMP_IND 0x6C
#define OP_BPL 0x10
#define OP_BMI 0x30
#define OP_BVC 0x50
#define OP_BVS 0x70
#define OP_BCC 0x90
#define OP_BCS 0xB0
#define OP_BNE 0xD0
#define OP_BEQ 0xF0
#define OP_SED 0xF8
#define OP_CLD 0xD8

// Get addressing mode for opcode
static addr_mode_t cpu_6502_get_addr_mode(uint8_t opcode) {
    return (addr_mode_t)cpu_6502_addrmodes[opcode];
}

// Get instruction length from opcode
static uint8_t cpu_6502_get_length(uint8_t opcode) {
    return get_length_from_mode(cpu_6502_addrmodes[opcode]);
}

// Decode instruction at PC
static uint8_t cpu_6502_decode(uint16_t pc_addr, decoded_instr_t *out) {
    out->opcode = read_src(pc_addr);
    out->prefix = 0;  // 6502 has no prefix bytes
    out->addr_mode = cpu_6502_get_addr_mode(out->opcode);
    out->length = get_length_from_mode(cpu_6502_addrmodes[out->opcode]);
    out->flags = 0;
    
    // Read operand based on length
    if (out->length == 2) {
        out->operand = read_src(pc_addr + 1);
    } else if (out->length == 3) {
        out->operand = read_src(pc_addr + 1) | (read_src(pc_addr + 2) << 8);
    } else {
        out->operand = 0;
    }
    
    // Set operation type based on opcode
    switch (out->opcode) {
        case OP_JMP_ABS:
        case OP_JMP_IND:
            out->op_type = OP_JUMP;
            out->flags = INSTR_ENDS_BLOCK | INSTR_MODIFIES_PC;
            break;
        case OP_JSR:
            out->op_type = OP_CALL;
            out->flags = INSTR_ENDS_BLOCK | INSTR_MODIFIES_PC;
            break;
        case OP_RTS:
            out->op_type = OP_RET;
            out->flags = INSTR_ENDS_BLOCK | INSTR_MODIFIES_PC;
            break;
        case OP_RTI:
            out->op_type = OP_RTI;
            out->flags = INSTR_ENDS_BLOCK | INSTR_MODIFIES_PC;
            break;
        case OP_BRK:
            out->op_type = OP_BREAK;
            out->flags = INSTR_ENDS_BLOCK | INSTR_MODIFIES_PC;
            break;
        case OP_BPL:
        case OP_BMI:
        case OP_BVC:
        case OP_BVS:
        case OP_BCC:
        case OP_BCS:
        case OP_BNE:
        case OP_BEQ:
            out->op_type = OP_BRANCH;
            out->flags = INSTR_CONDITIONAL | INSTR_MODIFIES_PC;
            break;
        case OP_SED:
            out->op_type = OP_SET_FLAG;
            out->flags = INSTR_DECIMAL_MODE;
            break;
        default:
            out->op_type = OP_UNKNOWN;
            break;
    }
    
    return out->length;
}

// Check if instruction requires interpretation
static bool cpu_6502_is_compilable(const decoded_instr_t *instr) {
    // These always need interpretation
    switch (instr->opcode) {
        case OP_JSR:
        case OP_RTS:
        case OP_RTI:
        case OP_JMP_ABS:
        case OP_JMP_IND:
        case OP_BRK:
            return false;
    }
    
    // Decimal mode flag
    if (instr->flags & INSTR_DECIMAL_MODE)
        return false;
        
    // Indirect addressing modes need interpretation due to bank conflicts
    // ADDR_INDIRECT_X = 11, ADDR_INDIRECT_Y = 12
    if (instr->addr_mode == 11 || instr->addr_mode == 12)
        return false;
        
    return true;
}

// Calculate branch target
static uint16_t cpu_6502_get_branch_target(uint16_t pc_addr, const decoded_instr_t *instr) {
    if (instr->op_type == OP_BRANCH) {
        // Relative branch: PC + 2 + signed offset
        int8_t offset = (int8_t)instr->operand;
        return pc_addr + 2 + offset;
    }
    else if (instr->op_type == OP_JUMP || instr->op_type == OP_CALL) {
        // Absolute address
        return instr->operand;
    }
    return 0;
}

// CPU frontend structure
const cpu_frontend_t cpu_6502 = {
    .name = "6502",
    .decode = cpu_6502_decode,
    .get_length = cpu_6502_get_length,
    .is_compilable = cpu_6502_is_compilable,
    .get_branch_target = cpu_6502_get_branch_target,
    .get_addr_mode = cpu_6502_get_addr_mode
};
