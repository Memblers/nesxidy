/**
 * translator.h - Instruction translator interface
 * 
 * Connects CPU frontend (decoder) to backend (emitter).
 * Handles register mapping between source and target CPUs.
 */

#ifndef TRANSLATOR_H
#define TRANSLATOR_H

#include <stdint.h>
#include "../frontend/cpu_frontend.h"
#include "../platform/platform.h"
#include "../backend/emit_6502.h"

// Translation result codes
#define TRANSLATE_OK            0   // Instruction translated successfully
#define TRANSLATE_INTERPRET     1   // Must interpret this instruction
#define TRANSLATE_END_BLOCK     2   // Block should end after this
#define TRANSLATE_OVERFLOW      3   // Output buffer full

// Register mapping (source CPU register ID -> NES zero-page address)
#define MAX_REGS 16

typedef struct {
    const cpu_frontend_t *frontend;
    const platform_t     *platform;
    emit_ctx_t           *emitter;
    
    // Register mapping (source reg ID -> zero-page address on NES)
    uint8_t reg_map[MAX_REGS];
    
    // Current PC being translated
    uint16_t current_pc;
    
    // Flags for current block
    uint8_t block_flags;
    
} translator_t;

// Initialize translator with CPU frontend and platform
void translator_init(translator_t *t, 
                     const cpu_frontend_t *cpu,
                     const platform_t *platform,
                     emit_ctx_t *emitter);

// Translate a single decoded instruction
// Returns TRANSLATE_* result code
uint8_t translate_instruction(translator_t *t, 
                              const decoded_instr_t *instr);

// Set up register mapping for source CPU
// Called once during initialization
void translator_setup_registers(translator_t *t);

// 6502-specific register IDs (for reg_map)
#define REG_6502_A      0
#define REG_6502_X      1
#define REG_6502_Y      2
#define REG_6502_SP     3
#define REG_6502_STATUS 4

// SM83-specific register IDs (future)
#define REG_SM83_A      0
#define REG_SM83_F      1
#define REG_SM83_B      2
#define REG_SM83_C      3
#define REG_SM83_D      4
#define REG_SM83_E      5
#define REG_SM83_H      6
#define REG_SM83_L      7
#define REG_SM83_SP_LO  8
#define REG_SM83_SP_HI  9

#endif // TRANSLATOR_H
