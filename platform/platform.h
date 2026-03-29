/**
 * platform.h - Source platform interface
 * 
 * Defines the interface for source system emulation.
 * Each platform (Exidy, Game Boy, etc.) implements this interface.
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>
#include <stdbool.h>

// Platform interface - each source system implements these
typedef struct platform {
    const char* name;           // Platform name ("Exidy", "Game Boy", etc.)
    
    // Memory interface (used by interpreter and recompiler)
    uint8_t (*read)(uint16_t addr);
    void (*write)(uint16_t addr, uint8_t val);
    
    // Address translation for recompiler
    // Translates source system address to NES address space
    // Returns 0 if address cannot be statically translated (must interpret)
    uint16_t (*translate_addr)(uint16_t src_addr);
    
    // Memory region queries
    bool (*is_rom)(uint16_t addr);      // Read-only memory
    bool (*is_ram)(uint16_t addr);      // Read-write memory  
    bool (*is_io)(uint16_t addr);       // I/O registers (must interpret)
    
    // Frame timing and interrupts
    void (*run_frame)(void);            // Called each frame
    void (*handle_interrupt)(void);     // Handle pending interrupts
    uint8_t (*get_interrupt_status)(void);
    
    // Reset and initialization
    void (*reset)(void);
    uint16_t (*get_reset_vector)(void);
    
    // Platform-specific rendering (called by main loop)
    void (*render_video)(void);
    
} platform_t;

// Available platforms
extern const platform_t platform_exidy;
// extern const platform_t platform_gameboy;  // Future

// Currently active platform
extern const platform_t *current_platform;

// Helper to set active platform
void platform_init(const platform_t *platform);

// Memory access macros (use current platform)
#define read_src(addr)       (current_platform->read(addr))
#define write_src(addr, val) (current_platform->write(addr, val))
#define translate_addr(addr) (current_platform->translate_addr(addr))

#endif // PLATFORM_H
