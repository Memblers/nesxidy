/**
 * opt_backup.c - Backup storage using CHR-RAM
 * 
 * PPU CHR-RAM: $0000-$1FFF (8KB)
 * Slot 0: $0000-$0FFF
 * Slot 1: $1000-$1FFF
 */

#include "opt_backup.h"
#include "../mapper30.h"

extern void bankswitch_prg(__reg("a") uint8_t bank);

// PPU registers
#define PPUCTRL   (*(volatile uint8_t*)0x2000)
#define PPUMASK   (*(volatile uint8_t*)0x2001)
#define PPUSTATUS (*(volatile uint8_t*)0x2002)
#define PPUADDR   (*(volatile uint8_t*)0x2006)
#define PPUDATA   (*(volatile uint8_t*)0x2007)

// Base addresses for slots in CHR-RAM
#define SLOT0_BASE 0x0000
#define SLOT1_BASE 0x1000

void backup_init(void) {
    // Rendering already disabled by opt_do_recompile()
    // Just reset the PPU latch
    (void)PPUSTATUS;
    
    // DEBUG: Write marker to CHR-RAM to verify backup system is running
    PPUADDR = 0x1F;
    PPUADDR = 0xF0;
    PPUDATA = 0xAA;
}

void backup_finish(void) {
    // DEBUG: Write completion marker to CHR-RAM
    PPUADDR = 0x1F;
    PPUADDR = 0xF1;
    PPUDATA = 0xBB;
    
    // Keep rendering disabled - optimizer runs with screen off
    // Game loop will re-enable rendering when appropriate
}

void backup_write(uint8_t slot, uint16_t offset, uint8_t data) {
    uint16_t addr = (slot ? SLOT1_BASE : SLOT0_BASE) + offset;
    
    // Dummy read to reset latch
    (void)PPUSTATUS;
    
    // Set PPU address
    PPUADDR = addr >> 8;
    PPUADDR = addr & 0xFF;
    
    // Write data
    PPUDATA = data;
}

uint8_t backup_read(uint8_t slot, uint16_t offset) {
    uint16_t addr = (slot ? SLOT1_BASE : SLOT0_BASE) + offset;
    
    // Dummy read to reset latch
    (void)PPUSTATUS;
    
    // Set PPU address
    PPUADDR = addr >> 8;
    PPUADDR = addr & 0xFF;
    
    // First read is buffered, discard it
    (void)PPUDATA;
    
    // Second read returns actual data
    return PPUDATA;
}

// Flash programming function (external)
extern void flash_byte_program(uint16_t addr, uint8_t bank, uint8_t data);

#pragma section bank1
void backup_copy_from_flash(uint8_t slot, uint16_t flash_addr, uint8_t flash_bank) {
    uint16_t chr_base = slot ? SLOT1_BASE : SLOT0_BASE;
    uint16_t i;
    
    // Switch to flash bank
    bankswitch_prg(flash_bank);
    
    // Reset PPU latch
    (void)PPUSTATUS;
    
    // Set starting CHR-RAM address
    PPUADDR = chr_base >> 8;
    PPUADDR = chr_base & 0xFF;
    
    // Copy 4KB from flash to CHR-RAM
    // Flash at $8000 + flash_addr (low 12 bits)
    for (i = 0; i < 4096; i++) {
        PPUDATA = *((uint8_t*)(0x8000 + (flash_addr & 0x0FFF) + i));
    }
}

#pragma section bank1
void backup_copy_to_flash(uint8_t slot, uint16_t flash_addr, uint8_t flash_bank) {
    uint16_t chr_base = slot ? SLOT1_BASE : SLOT0_BASE;
    uint16_t i;
    
    // Reset PPU latch
    (void)PPUSTATUS;
    
    // Copy 4KB from CHR-RAM to flash
    for (i = 0; i < 4096; i++) {
        // Set PPU address for each read (address auto-increments but we need exact control)
        (void)PPUSTATUS;
        PPUADDR = (chr_base + i) >> 8;
        PPUADDR = (chr_base + i) & 0xFF;
        
        // First read is buffered, discard
        (void)PPUDATA;
        
        // Second read gets actual data
        uint8_t data = PPUDATA;
        
        // Write to flash
        flash_byte_program(0x8000 + (flash_addr & 0x0FFF) + i, flash_bank, data);
    }
}
