/**
 * metrics.c - Metrics variable storage and WRAM dump
 *
 * Collection is done by macros in metrics.h (bank-safe).
 * This file provides:
 *   - Global metric structs (WRAM BSS)
 *   - Bank2 dump helpers that copy metrics to WRAM $7E30+
 *   - Fixed-bank trampolines for calling from the main loop
 */

#include <stdint.h>
#include "../config.h"

#ifdef ENABLE_METRICS

#include "metrics.h"

/* ================================================================
 * Global storage (WRAM BSS - no section pragma, linker places in RAM)
 * ================================================================ */

sa_metrics_t      sa_metrics;
runtime_metrics_t runtime_metrics;

/* External references for flash-state query */
extern uint16_t sector_free_offset[];
extern uint8_t  mapper_prg_bank;
extern void     bankswitch_prg(uint8_t bank);

/* ================================================================
 * WRAM dump layout at $7E30
 *
 * SA metrics (written once after sa_run):
 *   +$00  bfs_addresses_visited  u16
 *   +$02  bfs_entry_points       u16
 *   +$04  blocks_compiled        u16
 *   +$06  blocks_failed          u16
 *   +$08  code_bytes_written     u16
 *   +$0A  flash_sectors_used     u16
 *   +$0C  bfs_time  (cycles)     u32
 *   +$10  compile_time (cycles)  u32
 *
 * Runtime metrics (updated every frame):
 *   +$14  cache_hits             u32
 *   +$18  cache_misses           u32
 *   +$1C  blocks_recompiled      u32
 *   +$20  optimizer_runs         u32
 *   +$24  opt_branches_patched   u32
 *   +$28  peephole_php_elided    u32
 *   +$2C  peephole_plp_elided    u32
 *   +$30  dynamic_compile_frames u32
 *   +$34  total_frames           u32
 *   +$38  flash_occupancy_pct    u8
 *   +$39  magic  'M' 'E'
 * ================================================================ */

#define METRICS_WRAM  ((volatile uint8_t *)0x7FA0)

// Must match dynamos.h:  13 banks × 4 sectors = 52 sectors, 4KB each
#define FLASH_CACHE_SECTORS 52
#define FLASH_SECTOR_SIZE   0x1000UL

/* ---- Banked implementations ---- */
/* Exidy: bank22 (BANK_RENDER, $6000-$7FFF dead on Exidy)   */
/* NES:   bank21 (BANK_RENDER, $4000-$5FFF dead on NES DK)  */
/* BANK_METRICS = bank21 on both NES and Exidy */
#pragma section bank21

void metrics_dump_sa_b2(void)
{
    volatile uint8_t *p = METRICS_WRAM;

    *(volatile uint16_t *)(p + 0x00) = sa_metrics.bfs_addresses_visited;
    *(volatile uint16_t *)(p + 0x02) = sa_metrics.bfs_entry_points;
    *(volatile uint16_t *)(p + 0x04) = sa_metrics.bitmap_marked;
    *(volatile uint16_t *)(p + 0x06) = sa_metrics.blocks_compiled;
    *(volatile uint16_t *)(p + 0x08) = sa_metrics.blocks_failed;
    *(volatile uint16_t *)(p + 0x0A) = sa_metrics.blocks_skipped;
    *(volatile uint16_t *)(p + 0x0C) = sa_metrics.code_bytes_written;
    *(volatile uint16_t *)(p + 0x0E) = sa_metrics.flash_sectors_used;
    *(volatile uint32_t *)(p + 0x10) = sa_metrics.bfs_end_cycle
                                     - sa_metrics.bfs_start_cycle;
}

void metrics_dump_runtime_b2(void)
{
    volatile uint8_t *p = METRICS_WRAM;

    /* Read existing dynamos.c globals (uint32_t, __zpage) */
    extern __zpage uint32_t cache_hits;
    extern __zpage uint32_t cache_misses;
    extern uint32_t cache_interpret;

    /* Cache / dispatch — starts at +$14 */
    *(volatile uint32_t *)(p + 0x14) = cache_hits;
    *(volatile uint32_t *)(p + 0x18) = cache_misses;
    *(volatile uint32_t *)(p + 0x1C) = cache_interpret;

    /* Optimizer — read the actual opt2 stats directly instead of the
       unused runtime_metrics fields.  opt2_stat_total counts every
       branch/epilogue that entered the optimizer; opt2_stat_direct
       counts how many were successfully patched in flash. */
    extern uint16_t opt2_stat_total;
    extern uint16_t opt2_stat_direct;
    *(volatile uint32_t *)(p + 0x20) = (uint32_t)opt2_stat_total;
    *(volatile uint32_t *)(p + 0x24) = (uint32_t)opt2_stat_direct;

    /* Peephole */
    *(volatile uint32_t *)(p + 0x28) = runtime_metrics.peephole_php_elided;
    *(volatile uint32_t *)(p + 0x2C) = runtime_metrics.peephole_plp_elided;

    /* Frame counters */
    *(volatile uint32_t *)(p + 0x30) = runtime_metrics.dynamic_compile_frames;
    *(volatile uint32_t *)(p + 0x34) = runtime_metrics.total_frames++;

    /* Flash occupancy (quick scan) */
    {
        uint32_t total_free = 0;
        uint8_t i;
        for (i = 0; i < FLASH_CACHE_SECTORS; i++)
            total_free += (FLASH_SECTOR_SIZE - sector_free_offset[i]);
        uint32_t used = ((uint32_t)FLASH_CACHE_SECTORS * FLASH_SECTOR_SIZE) - total_free;
        p[0x38] = (uint8_t)((used * 100UL) / ((uint32_t)FLASH_CACHE_SECTORS * FLASH_SECTOR_SIZE));
    }

    /* Magic signature */
    p[0x39] = 0x4D;  /* 'M' */
    p[0x3A] = 0x45;  /* 'E' */

    /* IR optimisation stats — starts at +$3C */
    *(volatile uint32_t *)(p + 0x3C) = runtime_metrics.ir_blocks_processed;
    *(volatile uint32_t *)(p + 0x40) = runtime_metrics.ir_bytes_before;
    *(volatile uint32_t *)(p + 0x44) = runtime_metrics.ir_bytes_after;
    *(volatile uint32_t *)(p + 0x48) = runtime_metrics.ir_nodes_killed;
    *(volatile uint16_t *)(p + 0x4C) = runtime_metrics.ir_pass_redundant_load;
    *(volatile uint16_t *)(p + 0x4E) = runtime_metrics.ir_pass_dead_store;
    *(volatile uint16_t *)(p + 0x50) = runtime_metrics.ir_pass_dead_load;
    *(volatile uint16_t *)(p + 0x52) = runtime_metrics.ir_pass_php_plp;
    *(volatile uint16_t *)(p + 0x54) = runtime_metrics.ir_pass_pair_rewrite;
    *(volatile uint16_t *)(p + 0x56) = runtime_metrics.ir_pass_rmw_fusion;
}

#pragma section default

#endif /* ENABLE_METRICS */
