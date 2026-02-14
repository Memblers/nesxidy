/**
 * static_analysis.h - One-time power-on ROM analysis pass
 *
 * BFS walk of guest ROM starting from reset/NMI/IRQ vectors, discovering
 * all reachable code.  Results stored in a persistent bitmap + typed list
 * in flash bank 3.  Optional batch compilation of discovered entry points.
 *
 * Flash bank 3 layout (after existing data):
 *   $8000-$83BF  flash_block_flags[960]        (existing)
 *   $83D0-$83D7  cache signature               (existing)
 *   $83D8-$93D7  cache_bit_array[0x2000]       (existing, 8KB)
 *   $93D8-$A3D7  sa_code_bitmap[0x1000]        (NEW: 4KB, 1 bit/addr = 32K addrs)
 *   $A3D8-$A3DF  sa_header                     (NEW: 8 bytes)
 *   $A3E0-$A7DF  sa_indirect_targets[~340]     (NEW: 1KB, 3 bytes each)
 *
 * The code bitmap and indirect-target list survive flash_format() because
 * flash_format_b2() is patched to skip the sectors containing them.
 *
 * On each reset the walker re-discovers code from vectors (cheap BFS) and
 * seeds additionally from the persisted indirect-jump target list, which
 * grows at runtime as the interpreter resolves JMP ($xxxx) instructions.
 */

#ifndef STATIC_ANALYSIS_H
#define STATIC_ANALYSIS_H

#include <stdint.h>
#include "../config.h"

// -------------------------------------------------------------------------
// Flash layout for the static-analysis region in bank 3
// -------------------------------------------------------------------------
// All SA data is placed in bank 3 via #pragma section bank3, alongside
// flash_block_flags and cache_bit_array.  The linker assigns addresses.

// Code bitmap: 4KB = 32768 bits, one bit per guest address.
// Bit CLEAR = address is known code.  Erased state ($FF) = unknown.
// This matches the flash-friendly convention (clearing bits is free).
#define SA_BITMAP_SIZE      0x1000          // 4KB

// Header: 8 bytes — magic(4) + rom_hash(4)
// Stored inline in the sa_header[] array.
#define SA_HEADER_SIZE      8

// Indirect-target list: 3 bytes each (addr_lo, addr_hi, type_flags)
#define SA_INDIRECT_MAX     170             // ~512B / 3 bytes

// Target-type flags (stored in byte 2 of each indirect-target entry)
#define SA_TYPE_JSR         0x01
#define SA_TYPE_BRANCH      0x02
#define SA_TYPE_JMP_IND     0x03    // resolved at runtime
#define SA_TYPE_JMP_ABS     0x04
#define SA_TYPE_EMPTY       0xFF    // erased / unused slot

// Signature magic for the SA header (distinct from cache signature)
#define SA_SIG_MAGIC_0      0x53    // 'S'
#define SA_SIG_MAGIC_1      0x41    // 'A'
#define SA_SIG_MAGIC_2      0x56    // 'V'
#define SA_SIG_MAGIC_3      0x01    // version 1

// -------------------------------------------------------------------------
// BFS walker queue — lives in WRAM during the analysis pass.
// Reuses flash_compile_buffer (cache_code[0], 256 bytes) as a circular
// queue of 2-byte PC entries => 128 slots.
// -------------------------------------------------------------------------

#define SA_QUEUE_SLOTS      64

// -------------------------------------------------------------------------
// Bank 3 flash storage (defined in static_analysis.c with #pragma section bank3)
// Callers that need the flash addresses (e.g. flash_format sector skip)
// must declare their own extern and use the SA_SECTOR macros below.
// -------------------------------------------------------------------------

// Start/end sector addresses for protecting SA data during flash_format.
// Sectors are 4KB aligned ($x000).  sa_code_bitmap is the first SA variable
// placed in bank3; sa_indirect_list is the last.
// Usage: declare extern for sa_code_bitmap and sa_indirect_list first.
#define SA_SECTOR_FIRST  ((uint16_t)&sa_code_bitmap[0] & 0xF000)
#define SA_SECTOR_LAST   (((uint16_t)&sa_indirect_list[0] + SA_INDIRECT_MAX * 3 - 1) & 0xF000)

// -------------------------------------------------------------------------
// Public API  (all in bank 2)
// -------------------------------------------------------------------------

// Run the full static analysis pass:
//   1. Check / write SA header signature
//   2. BFS walk from vectors + persisted indirect targets
//   3. (if ENABLE_STATIC_COMPILE) batch-compile discovered entry points
void sa_run(void);

// Record a runtime-discovered indirect-jump target into the persistent
// flash list.  Called from the interpreter when JMP ($xxxx) is executed.
// Safe to call from any bank (lives in the fixed bank).
void sa_record_indirect_target(uint16_t target_pc, uint8_t type);

#endif // STATIC_ANALYSIS_H
