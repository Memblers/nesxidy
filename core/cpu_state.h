/**
 * cpu_state.h - CPU state shared between interpreter and recompiler
 * 
 * Defines the emulated CPU registers that are stored in zero-page
 * for fast access by both the interpreter and recompiled code.
 */

#ifndef CPU_STATE_H
#define CPU_STATE_H

#include <stdint.h>

// 6502 CPU registers (in zero-page for speed)
__zpage extern uint16_t pc;
__zpage extern uint8_t sp, a, x, y, status;
__zpage extern uint8_t opcode;

// Clock cycle counter (optional)
__zpage extern uint32_t clockticks6502;

// 6502 Status flags
#define FLAG_CARRY     0x01
#define FLAG_ZERO      0x02
#define FLAG_INTERRUPT 0x04
#define FLAG_DECIMAL   0x08
#define FLAG_BREAK     0x10
#define FLAG_CONSTANT  0x20
#define FLAG_OVERFLOW  0x40
#define FLAG_SIGN      0x80

#endif // CPU_STATE_H
