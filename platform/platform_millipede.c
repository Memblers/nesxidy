/**
 * platform_millipede.c - Atari Millipede arcade platform implementation
 *
 * Implements the platform interface for the Millipede arcade game.
 * Follows the same pattern as platform_exidy.c.
 */

#pragma section bank1

#include <stdint.h>
#include "platform.h"
#include "../config.h"
#include "../millipede.h"

// Forward declarations for millipede.c functions
extern uint8_t read6502(uint16_t address);
extern void write6502(uint16_t address, uint8_t value);
extern void render_video(void);
extern void reset6502(void);

// External memory bases
extern uint8_t RAM_BASE[];
extern uint8_t SCREEN_RAM_BASE[];
extern uint8_t ROM_NAME[];

// Interrupt condition from millipede.c
extern uint8_t interrupt_condition;

// Platform implementation functions

static uint8_t millipede_read(uint16_t addr) {
    return read6502(addr);
}

static void millipede_write(uint16_t addr, uint8_t val) {
    write6502(addr, val);
}

/**
 * Translate Millipede address to NES address space.
 * Mirrors the translate_address() logic in dynamos.c for PLATFORM_MILLIPEDE.
 */
static uint16_t millipede_translate_addr(uint16_t src_addr) {
    if (src_addr < 0x0400) {
        // RAM: $0000-$03FF → RAM_BASE
        return src_addr + (uint16_t)RAM_BASE;
    }
    else if (src_addr >= 0x1000 && src_addr < 0x13C0) {
        // Video RAM: $1000-$13BF → SCREEN_RAM_BASE
        return (src_addr - 0x1000) + (uint16_t)SCREEN_RAM_BASE;
    }
    else if (src_addr >= 0x4000 && src_addr < 0x8000) {
        // ROM: $4000-$7FFF → ROM_NAME
        uint16_t nes_addr = (src_addr - 0x4000) + (uint16_t)ROM_NAME;
        if ((nes_addr >= 0x8000) && (nes_addr < 0xC000))
            return 0;  // conflicts with flash cache
        return nes_addr;
    }
    // I/O or unmapped
    return 0;
}

static uint8_t millipede_is_rom(uint16_t addr) {
    return (addr >= 0x4000 && addr < 0x8000);
}

static uint8_t millipede_is_ram(uint16_t addr) {
    if (addr < 0x0400) return 1;           // Work RAM
    if (addr >= 0x1000 && addr < 0x1400) return 1;  // Video + sprite RAM
    return 0;
}

static uint8_t millipede_is_io(uint16_t addr) {
    // POKEY 1: $0400-$040F
    // POKEY 2: $0800-$080F
    // Inputs: $2000-$2031
    // Palette: $2480-$249F
    // Outputs: $2500-$2507
    // Watchdog: $2600
    if (addr >= 0x0400 && addr <= 0x040F) return 1;
    if (addr >= 0x0800 && addr <= 0x080F) return 1;
    if (addr >= 0x2000 && addr <= 0x2031) return 1;
    if (addr >= 0x2480 && addr <= 0x2600) return 1;
    return 0;
}

static void millipede_run_frame(void) {
    render_video();
}

static void millipede_handle_interrupt(void) {
    // IRQ handling done in main loop
}

static uint8_t millipede_get_interrupt_status(void) {
    return interrupt_condition;
}

static void millipede_reset(void) {
    reset6502();
}

static uint16_t millipede_get_reset_vector(void) {
    // Read reset vector from ROM (through mirror at $FFFC)
    return read6502(0xFFFC) | (read6502(0xFFFD) << 8);
}

static void millipede_render_video(void) {
    render_video();
}

// Public wrapper for dynamos.c direct calls
uint16_t millipede_translate_addr_wrapper(uint16_t src_addr) {
    return millipede_translate_addr(src_addr);
}

// Platform structure
const platform_t platform_millipede = {
    .name = "Millipede",
    .read = millipede_read,
    .write = millipede_write,
    .translate_addr = millipede_translate_addr,
    .is_rom = millipede_is_rom,
    .is_ram = millipede_is_ram,
    .is_io = millipede_is_io,
    .run_frame = millipede_run_frame,
    .handle_interrupt = millipede_handle_interrupt,
    .get_interrupt_status = millipede_get_interrupt_status,
    .reset = millipede_reset,
    .get_reset_vector = millipede_get_reset_vector,
    .render_video = millipede_render_video
};

// Current platform pointer
const platform_t *current_platform = &platform_millipede;

void platform_init(const platform_t *platform) {
    current_platform = platform;
}
