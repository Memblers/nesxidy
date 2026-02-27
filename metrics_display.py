#!/usr/bin/env python3
"""
metrics_display.py - Parse and display emulation metrics from running emulator
"""

import sys
import struct
import time

def print_sa_metrics(metrics):
    """Print static analysis metrics"""
    print("\n=== STATIC ANALYSIS METRICS ===")
    print(f"BFS Discovery:")
    print(f"  Addresses visited: {metrics['bfs_addresses_visited']}")
    print(f"  Entry points found: {metrics['bfs_entry_points']}")
    print(f"  Max queue depth: {metrics['bfs_queue_max_size']}")
    print(f"  ROM code bytes analyzed: {metrics['rom_code_bytes_analyzed']}")
    
    bfs_cycles = metrics['bfs_end_cycle'] - metrics['bfs_start_cycle']
    print(f"  BFS time: {bfs_cycles} NES cycles (~{bfs_cycles/1789773*1000:.1f}ms)")
    
    print(f"\nBatch Compilation:")
    print(f"  Blocks compiled: {metrics['blocks_compiled']}")
    print(f"  Blocks failed: {metrics['blocks_failed']}")
    print(f"  Total code bytes: {metrics['code_bytes_written']}")
    print(f"  Flash sectors used: {metrics['flash_sectors_used']}")
    
    compile_cycles = metrics['compile_end_cycle'] - metrics['compile_start_cycle']
    print(f"  Compile time: {compile_cycles} NES cycles (~{compile_cycles/1789773*1000:.1f}ms)")
    
    print(f"\nROM Coverage:")
    print(f"  Estimated coverage: {metrics['rom_coverage_percent']}%")

def print_runtime_metrics(metrics):
    """Print runtime cache and optimizer metrics"""
    print("\n=== RUNTIME METRICS ===")
    
    total_dispatches = metrics['cache_hits'] + metrics['cache_misses']
    if total_dispatches > 0:
        hit_ratio = (metrics['cache_hits'] / total_dispatches) * 100
    else:
        hit_ratio = 0
    
    print(f"Cache Behavior:")
    print(f"  Cache hits: {metrics['cache_hits']}")
    print(f"  Cache misses: {metrics['cache_misses']}")
    print(f"  Hit ratio: {hit_ratio:.1f}%")
    print(f"  Blocks recompiled: {metrics['blocks_recompiled']}")
    
    print(f"\nOptimizer:")
    print(f"  Optimizer runs: {metrics['optimizer_runs']}")
    print(f"  Blocks moved: {metrics['opt_blocks_moved']}")
    print(f"  Branches patched: {metrics['opt_branches_patched']}")
    
    print(f"\nPeephole Optimization:")
    print(f"  PHP elided: {metrics['peephole_php_elided']}")
    print(f"  PLP elided: {metrics['peephole_plp_elided']}")
    
    print(f"\nFlash State:")
    print(f"  Free bytes: {metrics['flash_free_bytes']}")
    print(f"  Occupancy: {metrics['flash_occupancy_percent']}%")
    
    if metrics['total_frames'] > 0:
        recompile_frame_ratio = (metrics['dynamic_compile_frames'] / metrics['total_frames']) * 100
        print(f"\nPerformance Impact:")
        print(f"  Frames with recompilation: {metrics['dynamic_compile_frames']}/{metrics['total_frames']}")
        print(f"  Recompile frame ratio: {recompile_frame_ratio:.1f}%")

def extract_metrics_from_save(save_file):
    """
    Extract metrics from a Mesen save state (if metrics are dumped there)
    This is a placeholder for actual implementation.
    """
    print("Save state metrics extraction not yet implemented")
    print(f"Place holder for: {save_file}")

if __name__ == '__main__':
    if len(sys.argv) > 1:
        extract_metrics_from_save(sys.argv[1])
    else:
        print("Emulation Metrics Analysis Tool")
        print("Usage: python metrics_display.py [save_state_file]")
        print("\nEnable ENABLE_METRICS in config.h to collect metrics")
        print("Access metrics via: metrics_get_sa_snapshot() and metrics_get_runtime_snapshot()")
