/**
 * cpu_frontend.h - Source CPU decoder interface
 * 
 * Defines the interface for decoding source CPU instructions.
 * Each source CPU (6502, SM83, Z80, etc.) implements this interface.
 */

#ifndef CPU_FRONTEND_H
#define CPU_FRONTEND_H

#include <stdint.h>
#include <stdbool.h>

// Use address mode enum from dynamos.h for compatibility
// enum address_modes { imp, acc, imm, zp, zpx, zpy, rel, abso, absx, absy, ind, indx, indy };
typedef enum {
    ADDR_IMPLIED = 0,       // imp
    ADDR_ACCUMULATOR = 1,   // acc
    ADDR_IMMEDIATE = 2,     // imm
    ADDR_ZERO_PAGE = 3,     // zp
    ADDR_ZERO_PAGE_X = 4,   // zpx
    ADDR_ZERO_PAGE_Y = 5,   // zpy
    ADDR_RELATIVE = 6,      // rel
    ADDR_ABSOLUTE = 7,      // abso
    ADDR_ABSOLUTE_X = 8,    // absx
    ADDR_ABSOLUTE_Y = 9,    // absy
    ADDR_INDIRECT = 10,     // ind
    ADDR_INDIRECT_X = 11,   // indx
    ADDR_INDIRECT_Y = 12    // indy
} addr_mode_t;

// Operation types (semantic meaning)
typedef enum {
    OP_NOP,
    OP_LOAD,
    OP_STORE,
    OP_TRANSFER,        // Register to register
    OP_ADD,
    OP_SUB,
    OP_AND,
    OP_OR,
    OP_XOR,
    OP_COMPARE,
    OP_INC,
    OP_DEC,
    OP_SHIFT_LEFT,
    OP_SHIFT_RIGHT,
    OP_ROTATE_LEFT,
    OP_ROTATE_RIGHT,
    OP_BRANCH,          // Conditional branch
    OP_JUMP,            // Unconditional jump
    OP_CALL,            // Subroutine call
    OP_RET,             // Return from subroutine
    OP_RTI,             // Return from interrupt
    OP_PUSH,
    OP_POP,
    OP_SET_FLAG,
    OP_CLEAR_FLAG,
    OP_BIT_TEST,
    OP_HALT,
    OP_BREAK,
    OP_UNKNOWN
} op_type_t;

// Instruction flags
#define INSTR_ENDS_BLOCK        0x01    // JMP, RET, RTI, etc.
#define INSTR_CONDITIONAL       0x02    // BNE, JR NZ, etc.
#define INSTR_NEEDS_INTERPRET   0x04    // Too complex to compile
#define INSTR_MODIFIES_PC       0x08    // Any control flow
#define INSTR_DECIMAL_MODE      0x10    // Uses decimal mode (6502)
#define INSTR_IO_ACCESS         0x20    // Accesses I/O ports

// Decoded instruction structure
typedef struct {
    uint8_t     opcode;         // Raw opcode byte
    uint8_t     prefix;         // Prefix byte (CB for GB, etc.) or 0
    uint8_t     length;         // Instruction length in bytes (1-4)
    addr_mode_t addr_mode;      // Addressing mode
    op_type_t   op_type;        // Semantic operation type
    uint8_t     src_reg;        // Source register ID (CPU-specific)
    uint8_t     dst_reg;        // Destination register ID
    uint16_t    operand;        // Immediate value or address
    uint8_t     flags;          // INSTR_* flags
} decoded_instr_t;

// CPU frontend interface - each source CPU implements these
typedef struct cpu_frontend {
    const char* name;           // CPU name ("6502", "SM83", etc.)
    
    // Decode instruction at PC, return length
    uint8_t (*decode)(uint16_t pc, decoded_instr_t *out);
    
    // Get instruction length from opcode (quick lookup)
    uint8_t (*get_length)(uint8_t opcode);
    
    // Check if instruction can be compiled
    bool (*is_compilable)(const decoded_instr_t *instr);
    
    // Calculate branch/jump target
    uint16_t (*get_branch_target)(uint16_t pc, const decoded_instr_t *instr);
    
    // Get addressing mode from opcode
    addr_mode_t (*get_addr_mode)(uint8_t opcode);
    
} cpu_frontend_t;

// Available frontends
extern const cpu_frontend_t cpu_6502;
// extern const cpu_frontend_t cpu_sm83;  // Future: Game Boy
// extern const cpu_frontend_t cpu_z80;   // Future: Z80

// Currently active frontend
extern const cpu_frontend_t *current_cpu;

// Helper to set active CPU
void cpu_frontend_init(const cpu_frontend_t *cpu);

#endif // CPU_FRONTEND_H
