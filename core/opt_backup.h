/**
 * opt_backup.h - Abstracted backup storage for optimizer
 * 
 * Currently uses CHR-RAM but can be swapped to WRAM later.
 * Two 4KB slots available:
 *   Slot 0: $0000-$0FFF (sector backup)
 *   Slot 1: $1000-$1FFF (new flag data accumulator)
 */

#ifndef OPT_BACKUP_H
#define OPT_BACKUP_H

#include <stdint.h>

// Initialize backup system (disable PPU rendering, etc)
void backup_init(void);

// Finish backup system (re-enable rendering)
void backup_finish(void);

// Write byte to backup slot
// slot: 0 or 1
// offset: 0-4095 within slot
void backup_write(uint8_t slot, uint16_t offset, uint8_t data);

// Read byte from backup slot
uint8_t backup_read(uint8_t slot, uint16_t offset);

// Bulk copy from flash sector to backup slot
void backup_copy_from_flash(uint8_t slot, uint16_t flash_addr, uint8_t flash_bank);

// Bulk copy from backup slot to flash sector (after erase)
void backup_copy_to_flash(uint8_t slot, uint16_t flash_addr, uint8_t flash_bank);

// Slot assignments
#define BACKUP_SLOT_SECTOR   0  // 4KB sector backup
#define BACKUP_SLOT_FLAGS    1  // 4KB new flag accumulator

#endif
