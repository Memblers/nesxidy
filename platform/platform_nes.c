/**
 * platform_nes.c - NES platform implementation
 *
 * Implements the platform interface for NES NROM games
 * (Donkey Kong, etc.)
 */

#pragma section bank1

#include <stdint.h>
#include "platform.h"
#include "../config.h"
#include "../nes.h"

// Forward declarations for nes.c functions
extern uint8_t read6502(uint16_t address);
extern void write6502(uint16_t address, uint8_t value);
extern void render_video(void);
extern void reset6502(void);

// External memory bases
extern uint8_t RAM_BASE[];
extern uint8_t ROM_NAME[];

// Platform implementation functions

static uint8_t nes_read(uint16_t addr) {
    return read6502(addr);
}

static void nes_write(uint16_t addr, uint8_t val) {
    write6502(addr, val);
}

/**
 * Translate NES address to host (NES mapper30) address space.
 * For NROM: RAM is direct, ROM must be bankswitched.
 */
static uint16_t nes_translate_addr(uint16_t src_addr) {
    if (src_addr < 0x0800) {
        // Work RAM: $0000-$07FF -> RAM_BASE
        return src_addr + (uint16_t)RAM_BASE;
    }
    else if (src_addr < 0x2000) {
        // RAM mirror
        return (src_addr & 0x7FF) + (uint16_t)RAM_BASE;
    }
    else if (src_addr >= 0x8000) {
        // PRG-ROM — lives in a switched bank, can't translate directly
        // to a fixed NES address.  Return 0 to force interpret.
        uint16_t nes_addr = (src_addr & 0x3FFF) + (uint16_t)ROM_NAME;
        // If it lands in the switchable bank window, can't translate
        if ((nes_addr >= 0x8000) && (nes_addr < 0xC000))
            return 0;
        return nes_addr;
    }

    // I/O or PPU — must interpret
    return 0;
}

static uint8_t nes_is_rom(uint16_t addr) {
    return (addr >= 0x8000);
}

static uint8_t nes_is_ram(uint16_t addr) {
    return (addr < 0x2000);
}

static uint8_t nes_is_io(uint16_t addr) {
    return (addr >= 0x2000 && addr < 0x4020);
}

static void nes_run_frame(void) {
    render_video();
}

static void nes_handle_interrupt(void) {
    // NES uses NMI, handled in main loop
}

static uint8_t nes_get_interrupt_status(void) {
    return 0;
}

static void nes_reset(void) {
    reset6502();
}

static uint16_t nes_get_reset_vector(void) {
    return read6502(0xFFFC) | (read6502(0xFFFD) << 8);
}

static void nes_render_video(void) {
    render_video();
}

// Public wrapper for dynamos.c direct call
uint16_t nes_translate_addr_wrapper(uint16_t src_addr) {
    return nes_translate_addr(src_addr);
}

// Platform structure
const platform_t platform_nes = {
    .name = "NES",
    .read = nes_read,
    .write = nes_write,
    .translate_addr = nes_translate_addr,
    .is_rom = nes_is_rom,
    .is_ram = nes_is_ram,
    .is_io = nes_is_io,
    .run_frame = nes_run_frame,
    .handle_interrupt = nes_handle_interrupt,
    .get_interrupt_status = nes_get_interrupt_status,
    .reset = nes_reset,
    .get_reset_vector = nes_get_reset_vector,
    .render_video = nes_render_video
};

// Current platform pointer
const platform_t *current_platform = &platform_nes;

void platform_init(const platform_t *platform) {
    current_platform = platform;
}
