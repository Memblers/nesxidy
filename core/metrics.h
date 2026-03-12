/**
 * metrics.h - Performance and compilation metrics tracking
 *
 * Collection functions are macros (safe to call from ANY bank).
 * Dump-to-WRAM functions live in bank2 and are called via trampolines.
 *
 * Enable ENABLE_METRICS in config.h to activate.
 * When disabled, every macro expands to nothing (zero overhead).
 */

#ifndef METRICS_H
#define METRICS_H

#include <stdint.h>
#include "../config.h"

#ifdef ENABLE_METRICS

/* ================================================================
 * Structs - stored in WRAM BSS, always accessible
 * ================================================================ */

typedef struct {
    uint16_t bfs_addresses_visited;
    uint16_t bfs_entry_points;
    uint16_t bitmap_marked;           /* total addresses in SA bitmap */
    uint16_t blocks_compiled;
    uint16_t blocks_failed;           /* code_index==0 (interpreted) */
    uint16_t blocks_skipped;          /* already flagged, skipped in pass 1 */
    uint16_t code_bytes_written;
    uint16_t flash_sectors_used;
    uint32_t bfs_start_cycle;
    uint32_t bfs_end_cycle;
    uint32_t compile_start_cycle;
    uint32_t compile_end_cycle;
} sa_metrics_t;

typedef struct {
    uint32_t optimizer_runs;
    uint32_t opt_branches_patched;
    uint32_t peephole_php_elided;
    uint32_t peephole_plp_elided;
    uint32_t dynamic_compile_frames;
    uint32_t total_frames;
    uint16_t flash_free_bytes;
    uint8_t  flash_occupancy_percent;
    /* IR optimisation stats (cumulative across all blocks) */
    uint32_t ir_blocks_processed;     /* blocks that went through IR pipeline */
    uint32_t ir_nodes_killed;         /* total IR_DEAD nodes across all passes */
    uint32_t ir_bytes_before;         /* sum of pre-optimisation code sizes */
    uint32_t ir_bytes_after;          /* sum of post-optimisation code sizes */
    uint16_t ir_pass_redundant_load;  /* Pass 1: redundant load+identity+fold */
    uint16_t ir_pass_dead_store;      /* Pass 2: dead store + store-back      */
    uint16_t ir_pass_dead_load;       /* Pass 2b: dead load elimination       */
    uint16_t ir_pass_php_plp;         /* Pass 3: PLP/PHP pairs removed        */
    uint16_t ir_pass_pair_rewrite;    /* Pass 4: pair rewrites + CMP #0 elim  */
    uint16_t ir_pass_rmw_fusion;     /* Pass 5+6: RMW fusion (shift+inc/dec) */
    uint16_t ir_instrs_eliminated;   /* guest instrs fully removed by IR     */
    uint16_t ir_instr_overflow;      /* blocks where instr count > MAX_INSTRS */
} runtime_metrics_t;

/* ================================================================
 * Globals (WRAM BSS - always accessible from any bank)
 * ================================================================ */

extern sa_metrics_t      sa_metrics;
extern runtime_metrics_t runtime_metrics;
extern __zpage uint32_t  clockticks6502;

/* ================================================================
 * Collection macros - safe to call from ANY bank / section
 *
 * These expand inline at the call site and only touch WRAM
 * variables, so no bank-switching is needed.
 * ================================================================ */

/* BFS phase (called from bank2 in static_analysis.c) */
#define metrics_bfs_start() do { \
    sa_metrics.bfs_start_cycle = clockticks6502; \
    sa_metrics.bfs_addresses_visited = 0; \
    sa_metrics.bfs_entry_points = 0; \
} while(0)

#define metrics_bfs_visit_address()  do { sa_metrics.bfs_addresses_visited++; } while(0)
#define metrics_bfs_entry_point()    do { sa_metrics.bfs_entry_points++; } while(0)
#define metrics_bfs_end()            do { sa_metrics.bfs_end_cycle = clockticks6502; } while(0)

/* Compile phase (called from fixed bank in sa_run) */
#define metrics_compile_start() do { \
    sa_metrics.compile_start_cycle = clockticks6502; \
    sa_metrics.blocks_compiled = 0; \
    sa_metrics.blocks_failed = 0; \
    sa_metrics.blocks_skipped = 0; \
    sa_metrics.bitmap_marked = 0; \
    sa_metrics.code_bytes_written = 0; \
} while(0)

#define metrics_block_compiled(len) do { \
    sa_metrics.blocks_compiled++; \
    sa_metrics.code_bytes_written += (len); \
} while(0)

#define metrics_block_failed()       do { sa_metrics.blocks_failed++; } while(0)
#define metrics_block_skipped()      do { sa_metrics.blocks_skipped++; } while(0)
#define metrics_bitmap_entry()       do { sa_metrics.bitmap_marked++; } while(0)
#define metrics_compile_end()        do { sa_metrics.compile_end_cycle = clockticks6502; } while(0)

/* Runtime cache - tracked by existing cache_hits/cache_misses/cache_interpret
   globals in dynamos.c (uint32_t, zero-page).  No separate metrics needed. */

/* Optimizer */
#define metrics_optimizer_run(moved, patched) do { \
    runtime_metrics.optimizer_runs++; \
    runtime_metrics.opt_branches_patched += (patched); \
} while(0)

#define metrics_peephole_remove(php, plp) do { \
    runtime_metrics.peephole_php_elided += (php); \
    runtime_metrics.peephole_plp_elided += (plp); \
} while(0)

/* IR optimisation — called from the IR pipeline in dynamos.c */
#define metrics_ir_block(before, after) do { \
    runtime_metrics.ir_blocks_processed++; \
    runtime_metrics.ir_bytes_before += (before); \
    runtime_metrics.ir_bytes_after  += (after); \
} while(0)

#define metrics_ir_pass_results(rl, ds, dl, pp, pr, rmw) do { \
    runtime_metrics.ir_pass_redundant_load += (rl); \
    runtime_metrics.ir_pass_dead_store     += (ds); \
    runtime_metrics.ir_pass_dead_load      += (dl); \
    runtime_metrics.ir_pass_php_plp        += (pp); \
    runtime_metrics.ir_pass_pair_rewrite   += (pr); \
    runtime_metrics.ir_pass_rmw_fusion     += (rmw); \
} while(0)

#define metrics_ir_nodes_killed(n) do { \
    runtime_metrics.ir_nodes_killed += (n); \
} while(0)

#define metrics_ir_instrs_eliminated(n) do { \
    runtime_metrics.ir_instrs_eliminated += (n); \
} while(0)

#define metrics_ir_instr_overflow() do { \
    runtime_metrics.ir_instr_overflow++; \
} while(0)

/* ================================================================
 * WRAM dump functions (banked - call with BANK_METRICS mapped).
 * metrics_dump_sa_b2()      - call once after sa_run.
 * metrics_dump_runtime_b2() - call every frame.
 *
 * Declarations must carry the same section attribute as the
 * definitions in metrics.c, otherwise vbcc binds the symbol to
 * the section of the *first* declaration it sees (warning 371).
 * ================================================================ */
/* BANK_METRICS = bank21 on both platforms */
#pragma section bank21
void metrics_dump_sa_b2(void);
void metrics_dump_runtime_b2(void);
#pragma section default

#else  /* !ENABLE_METRICS */

#define metrics_bfs_start()               ((void)0)
#define metrics_bfs_visit_address()       ((void)0)
#define metrics_bfs_entry_point()         ((void)0)
#define metrics_bfs_end()                 ((void)0)
#define metrics_compile_start()           ((void)0)
#define metrics_block_compiled(len)       ((void)0)
#define metrics_block_failed()            ((void)0)
#define metrics_block_skipped()           ((void)0)
#define metrics_bitmap_entry()            ((void)0)
#define metrics_compile_end()             ((void)0)
#define metrics_optimizer_run(m,p)        ((void)0)
#define metrics_peephole_remove(p,l)      ((void)0)
#define metrics_ir_block(b,a)              ((void)0)
#define metrics_ir_pass_results(rl,ds,dl,pp,pr,rmw)  ((void)0)
#define metrics_ir_nodes_killed(n)         ((void)0)
#define metrics_ir_instrs_eliminated(n)    ((void)0)
#define metrics_ir_instr_overflow()        ((void)0)
static inline void metrics_dump_sa_b2(void) {}
static inline void metrics_dump_runtime_b2(void) {}

#endif /* ENABLE_METRICS */

#endif /* METRICS_H */
