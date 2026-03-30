/**
 * platform_llander.c - Atari Lunar Lander arcade platform implementation
 *
 * Implements the platform interface for the Lunar Lander arcade game.
 * Follows the same pattern as platform_asteroids.c.
 */

#pragma section bank1

#include <stdint.h>
#include "platform.h"
#include "../config.h"
#include "../llander.h"

// Forward declarations for llander.c functions
extern uint8_t read6502(uint16_t address);
extern void write6502(uint16_t address, uint8_t value);
extern void render_video(void);
extern void reset6502(void);

// External memory bases
extern uint8_t RAM_BASE[];
extern uint8_t ROM_NAME[];

// Interrupt condition from llander.c
extern uint8_t interrupt_condition;

// Platform implementation functions

static uint8_t llander_read(uint16_t addr) {
    return read6502(addr);
}

static void llander_write(uint16_t addr, uint8_t val) {
    write6502(addr, val);
}

/**
 * Translate Lunar Lander address to NES address space.
 */
static uint16_t llander_translate_addr(uint16_t src_addr) {
    if (src_addr < 0x0100) {
        // RAM: $0000-$00FF → RAM_BASE
        return src_addr + (uint16_t)RAM_BASE;
    }
    else if (src_addr >= 0x6000 && src_addr < 0x8000) {
        // ROM: $6000-$7FFF → ROM_NAME
        uint16_t nes_addr = (src_addr - ROM_OFFSET) + (uint16_t)ROM_NAME;
        if ((nes_addr >= 0x8000) && (nes_addr < 0xC000))
            return 0;  // conflicts with flash cache
        return nes_addr;
    }
    // I/O, vector RAM/ROM, or unmapped — must interpret
    return 0;
}

static uint8_t llander_is_rom(uint16_t addr) {
    return (addr >= 0x6000 && addr < 0x8000);
}

static uint8_t llander_is_ram(uint16_t addr) {
    if (addr < 0x0100) return 1;               // Work RAM (256 bytes)
    if (addr >= 0x4000 && addr < 0x4800) return 1;  // Vector RAM (writable)
    return 0;
}

static uint8_t llander_is_io(uint16_t addr) {
    // IN0: $2000 (full byte read)
    if (addr == 0x2000) return 1;
    // IN1: $2400-$2407 (bit-addressed: start, rotate, coins)
    if (addr >= 0x2400 && addr <= 0x2407) return 1;
    // DSW1: $2800-$2803 (DIP switches via TTL153 mux)
    if (addr >= 0x2800 && addr <= 0x2803) return 1;
    // Thrust: $2C00 (analog lever ADC)
    if (addr == 0x2C00) return 1;
    // DVG/output/sound: $3000-$3FFF
    if (addr >= 0x3000 && addr <= 0x3FFF) return 1;
    return 0;
}

static void llander_run_frame(void) {
    render_video();
}

static void llander_handle_interrupt(void) {
    // NMI handling done in main loop
}

static uint8_t llander_get_interrupt_status(void) {
    return interrupt_condition;
}

static void llander_reset(void) {
    reset6502();
}

static uint16_t llander_get_reset_vector(void) {
    return read6502(0xFFFC) | (read6502(0xFFFD) << 8);
}

static void llander_render_video(void) {
    render_video();
}

// Public wrapper for dynamos.c direct calls
uint16_t llander_translate_addr_wrapper(uint16_t src_addr) {
    return llander_translate_addr(src_addr);
}

// Platform structure
const platform_t platform_llander = {
    .name = "Lunar Lander",
    .read = llander_read,
    .write = llander_write,
    .translate_addr = llander_translate_addr,
    .is_rom = llander_is_rom,
    .is_ram = llander_is_ram,
    .is_io = llander_is_io,
    .run_frame = llander_run_frame,
    .handle_interrupt = llander_handle_interrupt,
    .get_interrupt_status = llander_get_interrupt_status,
    .reset = llander_reset,
    .get_reset_vector = llander_get_reset_vector,
    .render_video = llander_render_video
};

// Current platform pointer
const platform_t *current_platform = &platform_llander;

void platform_init(const platform_t *platform) {
    current_platform = platform;
}
