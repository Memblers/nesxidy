/**
 * optimizer_compile.c - One-pass sector-based optimization with branch resolution
 * 
 * When OPT_COPY_BLOCKS is enabled:
 *   - Reads metadata from existing blocks (code_len, exit_pc, branches)
 *   - Copies native code directly instead of recompiling
 *   - Much faster than recompilation
 * 
 * Process by PC table sector (2048 addresses, 4KB):
 *   1. Backup PC sector to SLOT_SECTOR
 *   2. Erase PC sector  
 *   3. Copy/recompile addresses, write PC entries directly
 *   4. Accumulate new flags to SLOT_FLAGS
 *   5. After PC sector done, backup flag half-sector to SLOT_SECTOR
 *   6. Erase flag sector (at 4KB boundaries)
 *   7. Write accumulated flags from SLOT_FLAGS
 *   8. Restore flag entries still $FF from backup
 */

#include <stdint.h>
#include "optimizer.h"
#include "opt_backup.h"
#include "../config.h"
#include "../dynamos.h"
#include "../mapper30.h"

// External references (before section pragma to avoid redeclaration warnings)
extern void bankswitch_prg(__reg("a") uint8_t bank);
extern uint8_t recompile_opcode(void);
extern void flash_byte_program(uint16_t addr, uint8_t bank, uint8_t data);
extern uint8_t read6502(uint16_t address);
extern void flash_dispatch_return(void);
extern void opt_tramp_erase(uint8_t sector);

#pragma section bank1

// External data references
extern uint8_t cache_code[BLOCK_COUNT][CACHE_CODE_BUF_SIZE];
extern uint8_t cache_flag[BLOCK_COUNT];
extern uint8_t cache_index;
extern uint16_t code_index;
extern uint16_t pc;
extern uint8_t a;

// Current flash write position
static uint16_t fw_addr;
static uint8_t fw_bank;

// Branch fixup tracking (in second half of SLOT_FLAGS)
static uint16_t br_count;
#define MAX_BRANCHES 400  // 5 bytes each = 2000 bytes, fits in 2KB

// Flash write with bank crossing
static void fw_byte(uint8_t data) {
    flash_byte_program(fw_addr, fw_bank, data);
    if (++fw_addr >= 0xC000) {
        fw_addr = 0x8000;
        fw_bank++;
    }
}

// Record branch for later fixup
// Stored in upper 2KB of SLOT_FLAGS (offset 0x800-0xFFF)
static void rec_branch(uint16_t br_flash_addr, uint8_t br_flash_bank, uint16_t target_pc) {
    if (br_count >= MAX_BRANCHES) return;
    
    uint16_t off = 0x800 + br_count * 5;  // offset within SLOT_FLAGS
    backup_write(BACKUP_SLOT_FLAGS, off, br_flash_addr & 0xFF);
    backup_write(BACKUP_SLOT_FLAGS, off + 1, br_flash_addr >> 8);
    backup_write(BACKUP_SLOT_FLAGS, off + 2, br_flash_bank);
    backup_write(BACKUP_SLOT_FLAGS, off + 3, target_pc & 0xFF);
    backup_write(BACKUP_SLOT_FLAGS, off + 4, target_pc >> 8);
    br_count++;
}

// Get branch record
static void get_branch(uint16_t idx, uint16_t *addr, uint8_t *bank, uint16_t *target) {
    uint16_t off = 0x800 + idx * 5;
    *addr = backup_read(BACKUP_SLOT_FLAGS, off) | (backup_read(BACKUP_SLOT_FLAGS, off + 1) << 8);
    *bank = backup_read(BACKUP_SLOT_FLAGS, off + 2);
    *target = backup_read(BACKUP_SLOT_FLAGS, off + 3) | (backup_read(BACKUP_SLOT_FLAGS, off + 4) << 8);
}

#if OPT_COPY_BLOCKS && OPT_BLOCK_METADATA
// Copy block from old location using metadata
// Returns new native address, or 0 on failure
static uint16_t copy_block(uint16_t old_native, uint8_t old_bank) {
    uint16_t start = fw_addr;
    
    // Read from old flash location
    // native_addr points to code (after prefix), so prefix is at native_addr - 1
    bankswitch_prg(old_bank);
    uint16_t block_base = (old_native & 0x3FFF) - BLOCK_PREFIX_SIZE;
    uint8_t *src = (uint8_t*)(0x8000 + block_base);
    
    // Read metadata from old block
    uint8_t code_len = src[0];  // Length prefix
    
    if (code_len == 0 || code_len > CODE_SIZE) {
        return 0;  // Invalid block
    }
    
    // Calculate where epilogue and metadata are
    // Layout: [len:1][code:code_len][epilogue:14][exit_pc:2][cycles:2?][br_count:1][branches...]
    uint16_t epilogue_off = 1 + code_len;
    uint16_t meta_off = epilogue_off + 14;  // After epilogue
    
    uint16_t exit_pc = src[meta_off] | (src[meta_off + 1] << 8);
    uint16_t meta_ptr = meta_off + 2;
    
#if OPT_TRACK_CYCLES
    // Skip cycle count for now
    meta_ptr += 2;
#endif
    
    uint8_t old_branch_count = src[meta_ptr++];
    
    // Copy code bytes (skip length prefix, copy code only)
    for (uint8_t i = 0; i < code_len; i++) {
        uint8_t byte = src[1 + i];  // +1 to skip length prefix
        
        fw_byte(byte);
    }
    
    // Record branches from metadata for later fixup
    for (uint8_t i = 0; i < old_branch_count && i < 16; i++) {
        uint8_t br_off = src[meta_ptr++];
        uint16_t br_target = src[meta_ptr] | (src[meta_ptr + 1] << 8);
        meta_ptr += 2;
        
        // Record this branch for fixup
        // br_off is relative to code start, need to convert to absolute flash addr
        rec_branch(start + br_off, fw_bank, br_target);
    }
    
    // Write epilogue (regenerate with same exit_pc)
    fw_byte(0x85); fw_byte((uint8_t)(uint16_t)&a);
    fw_byte(0x08);
    fw_byte(0xA9); fw_byte(exit_pc & 0xFF);
    fw_byte(0x85); fw_byte((uint8_t)(uint16_t)&pc);
    fw_byte(0xA9); fw_byte(exit_pc >> 8);
    fw_byte(0x85); fw_byte(((uint16_t)&pc) + 1);
    fw_byte(0x4C);
    fw_byte((uint16_t)&flash_dispatch_return & 0xFF);
    fw_byte(((uint16_t)&flash_dispatch_return) >> 8);
    
    return start;
}

#else
// Recompile block from source (fallback when metadata not available)
static uint16_t compile_one(uint16_t src) {
    uint16_t start = fw_addr;
    
    pc = src;
    cache_index = 0;
    code_index = 0;
    cache_flag[0] = 0;
    
    do {
        uint8_t idx_old = code_index;
        uint8_t op = read6502(pc);
        
        // Check for conditional branches
        if (op==0x10||op==0x30||op==0x50||op==0x70||op==0x90||op==0xB0||op==0xD0||op==0xF0) {
            int8_t off = read6502(pc + 1);
            uint16_t tgt = pc + 2 + off;
            
            fw_byte(op);
            rec_branch(fw_addr, fw_bank, tgt);
            fw_byte(0x00);  // Placeholder
            
            pc += 2;
            cache_flag[0] &= ~READY_FOR_NEXT;
        } else {
            recompile_opcode();
            for (uint8_t i = idx_old; i < code_index; i++) {
                fw_byte(cache_code[0][i]);
            }
        }
        
        if (code_index > CODE_SIZE - 20) {
            cache_flag[0] &= ~READY_FOR_NEXT;
        }
    } while (cache_flag[0] & READY_FOR_NEXT);
    
    // Epilogue
    uint16_t exit_pc = pc;
    fw_byte(0x85); fw_byte((uint8_t)(uint16_t)&a);
    fw_byte(0x08);
    fw_byte(0xA9); fw_byte(exit_pc & 0xFF);
    fw_byte(0x85); fw_byte((uint8_t)(uint16_t)&pc);
    fw_byte(0xA9); fw_byte(exit_pc >> 8);
    fw_byte(0x85); fw_byte(((uint16_t)&pc) + 1);
    fw_byte(0x4C);
    fw_byte((uint16_t)&flash_dispatch_return & 0xFF);
    fw_byte(((uint16_t)&flash_dispatch_return) >> 8);
    
    return start;
}
#endif

// Resolve branches after compiling a batch
static void resolve_branches(void) {
    for (uint16_t i = 0; i < br_count; i++) {
        uint16_t br_addr, target_pc;
        uint8_t br_bank;
        get_branch(i, &br_addr, &br_bank, &target_pc);
        
        // Look up target's native address from PC table
        uint8_t pc_bank = (target_pc >> 13) + BANK_PC;
        uint16_t pc_off = (target_pc << 1) & 0x3FFF;
        
        bankswitch_prg(pc_bank);
        uint16_t target_native = *(uint8_t*)(0x8000 + pc_off);
        target_native |= (uint16_t)(*(uint8_t*)(0x8000 + pc_off + 1)) << 8;
        
        // Check if target was recompiled (not $FFFF)
        if (target_native != 0xFFFF) {
            // Calculate branch offset
            int16_t offset = target_native - (br_addr + 1);
            
            // Only patch if in range
            if (offset >= -128 && offset <= 127) {
                flash_byte_program(br_addr, br_bank, (uint8_t)offset);
            }
        }
    }
}

// Process one PC table sector (2048 addresses, 4KB)
// Returns number of addresses processed
static uint16_t process_pc_sector(uint16_t base_addr, uint8_t *flag_backup) {
    uint8_t pc_bank = (base_addr >> 13) + BANK_PC;
    uint8_t sector = pc_bank * 2 + ((base_addr & 0x1000) ? 1 : 0);
    uint16_t count = 0;
    
    // Step 1: Backup PC sector to SLOT_SECTOR
    backup_copy_from_flash(BACKUP_SLOT_SECTOR, 0, pc_bank);
    
    // Step 2: Erase PC sector
    opt_tramp_erase(sector);
    
    // Step 3: Scan backup for valid entries and copy/recompile
    // Also accumulate new flag values to SLOT_FLAGS (lower 2KB)
    for (uint16_t i = 0; i < 2048; i++) {
        uint16_t off = i * 2;
        uint8_t lo = backup_read(BACKUP_SLOT_SECTOR, off);
        uint8_t hi = backup_read(BACKUP_SLOT_SECTOR, off + 1);
        uint16_t old_native = lo | (hi << 8);
        
        if (old_native != 0xFFFF) {
            // This address was compiled - get old bank from flag_backup
            uint8_t old_bank = flag_backup[i] & 0x1F;
            uint16_t new_native;
            
#if OPT_COPY_BLOCKS && OPT_BLOCK_METADATA
            // Copy block using metadata
            new_native = copy_block(old_native, old_bank);
            if (new_native == 0) {
                // Copy failed, skip this block
                backup_write(BACKUP_SLOT_FLAGS, i, 0xFF);
                continue;
            }
#else
            // Recompile from source
            uint16_t src = base_addr + i;
            new_native = compile_one(src);
#endif
            
            // Write new PC entry directly to flash
            flash_byte_program(0x8000 + off, pc_bank, new_native & 0xFF);
            flash_byte_program(0x8000 + off + 1, pc_bank, new_native >> 8);
            
            // Store new flag in SLOT_FLAGS (lower 2KB)
            backup_write(BACKUP_SLOT_FLAGS, i, fw_bank & 0x1F);
            
            count++;
        } else {
            // Not compiled - mark flag as $FF so we skip it later
            backup_write(BACKUP_SLOT_FLAGS, i, 0xFF);
        }
    }
    
    return count;
}

// Process flag sector corresponding to a PC sector
static void process_flag_sector(uint16_t base_addr) {
    uint8_t flag_bank = (base_addr >> 14) + BANK_PC_FLAGS;
    uint8_t sector = flag_bank * 2 + ((base_addr & 0x2000) ? 1 : 0);
    
    // Calculate offset within flag bank
    uint16_t flag_off_base = base_addr & 0x3FFF;
    
    // Step 6: Backup flag half-sector (2KB) to SLOT_SECTOR
    // Note: flag sectors are 4KB but we process 2KB at a time
    // For simplicity, do a 4KB backup but only use 2KB
    backup_copy_from_flash(BACKUP_SLOT_SECTOR, flag_off_base & 0x1000, flag_bank);
    
    // Step 7: Erase flag sector only at 4KB boundaries
    if ((base_addr & 0x0FFF) == 0) {
        opt_tramp_erase(sector);
    }
    
    // Step 8: Write accumulated flags from SLOT_FLAGS
    // Step 9: Restore flags still $FF from backup
    for (uint16_t i = 0; i < 2048; i++) {
        uint8_t new_flag = backup_read(BACKUP_SLOT_FLAGS, i);
        uint16_t flag_flash_off = flag_off_base + i;
        
        if (new_flag != 0xFF) {
            // Write new flag
            flash_byte_program(0x8000 + (flag_flash_off & 0x0FFF), flag_bank, new_flag);
        } else {
            // Restore from backup
            uint8_t old_flag = backup_read(BACKUP_SLOT_SECTOR, i);
            if (old_flag != 0xFF) {
                flash_byte_program(0x8000 + (flag_flash_off & 0x0FFF), flag_bank, old_flag);
            }
        }
    }
}

// Main entry point - one-pass optimization
void opt_do_recompile(void) {
    
    // Initialize flash write position
    fw_addr = 0x8000;
    fw_bank = BANK_CODE;
    br_count = 0;
    
    // Initialize backup system
    backup_init();
    
    // Temporary buffer for flag table portion (in RAM)
    // We need to read old flags before erasing them
    // This uses 2KB of stack/RAM but avoids extra CHR-RAM complexity
    static uint8_t flag_cache[2048];
    
    // Process each 2KB address range (one PC sector)
    // Full 6502 address space: $0000-$FFFF = 32 sectors
    for (uint16_t sector_num = 0; sector_num < 32; sector_num++) {
        uint16_t base_addr = sector_num * 2048;
        
        // First, read the current flag table entries for this range
        // These tell us which flash bank contains each block
        uint8_t flag_bank = (base_addr >> 14) + BANK_PC_FLAGS;
        uint16_t flag_off_base = base_addr & 0x3FFF;
        
        bankswitch_prg(flag_bank);
        for (uint16_t i = 0; i < 2048; i++) {
            flag_cache[i] = *(uint8_t*)(0x8000 + flag_off_base + i);
        }
        
        // Process PC sector (copy/recompile blocks)
        uint16_t count = process_pc_sector(base_addr, flag_cache);
        
        if (count > 0) {
            // Process corresponding flag sector
            process_flag_sector(base_addr);
            
            // Resolve branches for this batch
            resolve_branches();
            
            // Reset branch counter for next sector
            br_count = 0;
        }
    }
    
    backup_finish();
}
