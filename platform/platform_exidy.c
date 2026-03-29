/**
 * platform_exidy.c - Exidy arcade platform implementation
 * 
 * Implements the platform interface for Exidy arcade games
 * (Side Trac, Targ, Spectar, etc.)
 */

#pragma section bank1

#include <stdint.h>
#include "platform.h"
#include "../config.h"
#include "../exidy.h"

// Forward declarations for existing exidy.c functions
extern uint8_t read6502(uint16_t address);
extern void write6502(uint16_t address, uint8_t value);
extern void render_video(void);
extern void reset6502(void);

// External memory bases defined in exidy.c
extern uint8_t RAM_BASE[];
extern uint8_t SCREEN_RAM_BASE[];
extern uint8_t CHARACTER_RAM_BASE[];
extern uint8_t ROM_NAME[];

// Interrupt condition from exidy.c
extern uint8_t interrupt_condition;

// Platform implementation functions

static uint8_t exidy_read(uint16_t addr) {
    return read6502(addr);
}

static void exidy_write(uint16_t addr, uint8_t val) {
    write6502(addr, val);
}

/**
 * Translate Exidy address to NES address space.
 * This is the decode_address_c() logic from dynamos.c
 */
static uint16_t exidy_translate_addr(uint16_t src_addr) {
    uint8_t msb = src_addr >> 8;
    
    if (msb < 0x04) {
        // RAM: $0000-$03FF -> RAM_BASE
        return src_addr + (uint16_t)RAM_BASE;
    }
    else if (msb < 0x40) {
        // ROM: $0400-$3FFF -> ROM_NAME (with offset)
        uint16_t nes_addr = (src_addr - ROM_OFFSET) + (uint16_t)ROM_NAME;
        
        // Check for conflict with flash cache bank ($8000-$BFFF)
        // If ROM address falls in switchable bank, must interpret
        if ((nes_addr >= 0x8000) && (nes_addr < 0xC000))
            return 0;  // Can't translate, must interpret
            
        return nes_addr;
    }
    else if (msb < 0x48) {
        // Screen RAM: $4000-$47FF -> SCREEN_RAM_BASE
        return (src_addr - 0x4000) + (uint16_t)SCREEN_RAM_BASE;
    }
    else if (msb < 0x50) {
        // Character RAM: $4800-$4FFF -> CHARACTER_RAM_BASE
        return (src_addr - 0x4800) + (uint16_t)CHARACTER_RAM_BASE;
    }
    
    // I/O or unmapped - must interpret
    return 0;
}

static uint8_t exidy_is_rom(uint16_t addr) {
    uint8_t msb = addr >> 8;
    return (msb >= 0x04) && (msb < 0x40);
}

static uint8_t exidy_is_ram(uint16_t addr) {
    uint8_t msb = addr >> 8;
    if (msb < 0x04) return 1;           // Work RAM
    if (msb >= 0x40 && msb < 0x50) return 1;  // Screen/char RAM
    return 0;
}

static uint8_t exidy_is_io(uint16_t addr) {
    // $5000-$51FF: Sprite registers
    // $5100-$5103: DIP switches, inputs, interrupts
    uint8_t msb = addr >> 8;
    return (msb >= 0x50 && msb < 0x52);
}

static void exidy_run_frame(void) {
    render_video();
}

static void exidy_handle_interrupt(void) {
    // Exidy IRQ handling - currently disabled in main loop
}

static uint8_t exidy_get_interrupt_status(void) {
    return interrupt_condition;
}

static void exidy_reset(void) {
    reset6502();
}

static uint16_t exidy_get_reset_vector(void) {
    // Read reset vector from ROM
    return read6502(0xFFFC) | (read6502(0xFFFD) << 8);
}

static void exidy_render_video(void) {
    render_video();
}

// Public wrapper function for dynamos.c to call directly
// (Avoids function pointer through struct which VBCC may not handle well)
uint16_t exidy_translate_addr_wrapper(uint16_t src_addr) {
    return exidy_translate_addr(src_addr);
}

// Platform structure
const platform_t platform_exidy = {
    .name = "Exidy",
    .read = exidy_read,
    .write = exidy_write,
    .translate_addr = exidy_translate_addr,
    .is_rom = exidy_is_rom,
    .is_ram = exidy_is_ram,
    .is_io = exidy_is_io,
    .run_frame = exidy_run_frame,
    .handle_interrupt = exidy_handle_interrupt,
    .get_interrupt_status = exidy_get_interrupt_status,
    .reset = exidy_reset,
    .get_reset_vector = exidy_get_reset_vector,
    .render_video = exidy_render_video
};

// Current platform pointer
const platform_t *current_platform = &platform_exidy;

void platform_init(const platform_t *platform) {
    current_platform = platform;
}
