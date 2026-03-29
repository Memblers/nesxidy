#!/usr/bin/env python3
"""
map_profiler.py - Map Mesen2 profiler PRG addresses to function names
using vicemap.map label data.

PRG address = bank * 0x4000 + (cpu_addr - 0x8000)  for banked code ($8000-$BFFF)
PRG address = 31 * 0x4000 + (cpu_addr - 0xC000)    for fixed bank ($C000-$FFFF)
"""

import re
from collections import defaultdict

# -------------------------------------------------------------------
# Step 1: Parse vicemap.map for ALL function labels
# -------------------------------------------------------------------
labels = {}  # cpu_addr -> label_name
with open("vicemap.map", "r") as f:
    for line in f:
        m = re.match(r'al C:([0-9a-fA-F]+)\s+\.(\S+)', line)
        if m:
            addr = int(m.group(1), 16)
            name = m.group(2)
            labels[addr] = name

print(f"Loaded {len(labels)} labels from vicemap.map")

# -------------------------------------------------------------------
# Step 2: Determine bank membership from #pragma section in source
# -------------------------------------------------------------------
# Key: function label -> bank number
# We know from source analysis:
#   ir_opt.c:          #pragma section bank28 (NES)
#   ir_opt_ext.c:      #pragma section bank17
#   ir_lower.c:        #pragma section bank1
#   optimizer_v2_simple.c: has both bank1 and bank28 sections
#   emit_6502.c:       likely bank1 or bank2
#   dynamos.c:         has bank17 sections, but most is bank2 or fixed
#   Fixed bank code:   $C000-$FFFF (bank 31)

# Build bank assignments from pragma sections
# For addresses $C000-$FFFF: always bank 31
# For addresses $8000-$BFFF: need to determine from source

# Known bank assignments (from #pragma section analysis):
bank_assignments = {}

# ir_opt.c functions -> bank 28
for name in ['_ir_opt_redundant_load', '_ir_opt_dead_store', '_ir_opt_dead_load',
             '_ir_opt_php_plp_elision', '_ir_optimize']:
    bank_assignments[name] = 28

# ir_opt_ext.c functions -> bank 17
for name in ['_ir_opt_pair_rewrite', '_ir_opt_clc_sec_sink', '_ir_optimize_ext']:
    bank_assignments[name] = 17

# ir_lower.c functions -> bank 1
for name in ['_ir_lower', '_ir_opt_rmw_fusion', '_ir_compute_instr_offsets',
             '_ir_rebuild_block_ci_map', '_ir_resolve_deferred_patches',
             '_ir_resolve_direct_branches']:
    bank_assignments[name] = 1

# emit_6502.c / ir.c functions -> bank 1 (they use #pragma section bank1)
for name in ['_ir_emit', '_ir_emit_byte', '_ir_emit_raw_op_abs',
             '_ir_emit_template', '_ir_emit_raw_block', '_emit_epilogue']:
    bank_assignments[name] = 1

# optimizer_v2_simple.c bank1 section
for name in ['_opt2_record_pending_branch', '_opt2_get_stats',
             '_opt2_full_epilogue_scan_b1', '_opt2_full_branch_scan_b1']:
    bank_assignments[name] = 1

# optimizer_v2_simple.c bank28 section (NES)
# opt2_full_link_resolve, opt2_sweep etc in fixed bank ($C000+)
# But opt2_sweep_pending_patches_b2, opt2_scan_and_patch_epilogues_b2 -> bank 28
# Let's check their addresses

# All labels in $C000-$FFFF range are fixed bank 31
# All labels in $6000-$7FFF range are WRAM (bank 0 data or asm routines)

# -------------------------------------------------------------------
# Step 3: Convert labels to PRG addresses
# -------------------------------------------------------------------
prg_functions = {}  # prg_addr -> (label, bank)

for cpu_addr, name in labels.items():
    if cpu_addr >= 0xC000:
        bank = 31
        prg = 31 * 0x4000 + (cpu_addr - 0xC000)
        prg_functions[prg] = (name, bank)
    elif cpu_addr >= 0x8000 and cpu_addr < 0xC000:
        # Need bank info
        if name in bank_assignments:
            bank = bank_assignments[name]
        else:
            # Try to infer from context - skip for now, mark unknown
            bank = None
        if bank is not None:
            prg = bank * 0x4000 + (cpu_addr - 0x8000)
            prg_functions[prg] = (name, bank)
    elif cpu_addr >= 0x6000 and cpu_addr < 0x8000:
        # WRAM - not PRG
        pass

# -------------------------------------------------------------------
# Step 4: Build sorted function lists per bank for range lookups
# -------------------------------------------------------------------
bank_sorted = defaultdict(list)
for prg, (name, bank) in prg_functions.items():
    bank_sorted[bank].append((prg, name))
for bank in bank_sorted:
    bank_sorted[bank].sort()

def find_function_at(prg_addr):
    """Find which function a PRG address most likely belongs to."""
    # Exact match?
    if prg_addr in prg_functions:
        return prg_functions[prg_addr][0] + " (ENTRY)", prg_functions[prg_addr][1]
    
    bank = prg_addr // 0x4000
    if bank in bank_sorted:
        funcs = bank_sorted[bank]
        best = None
        for fprg, fname in funcs:
            if fprg <= prg_addr:
                best = (fprg, fname)
            else:
                break
        if best:
            offset = prg_addr - best[0]
            return f"{best[1]} + ${offset:X}", bank
    return "(unknown)", prg_addr // 0x4000

# -------------------------------------------------------------------
# Step 5: User's profiler entries
# -------------------------------------------------------------------
profiler_entries = {
    # Fixed bank (PRG $7C000-$7FFFF)
    0x7CC1E: None,
    0x7C729: None,
    0x7CBD6: None,
    0x7C3C4: None,
    0x7CB95: None,
    
    # Bank 28 (PRG $70000-$73FFF) - BANK_IR_OPT
    0x7037E: None,
    0x7292B: None,
    0x70000: None,
    0x7002E: None,
    0x700CE: None,
    0x7025C: None,
    0x702A2: None,
    0x702C9: None,
    0x702F0: None,
    0x7007E: None,
    0x700FB: None,
    0x701DF: None,
    0x701A6: None,
    0x70143: None,
    0x7016A: None,
    0x7023B: None,
    
    # Bank 17 (PRG $44000-$47FFF) - BANK_COMPILE
    0x46113: None,
    0x44D50: None,
    0x44375: None,
    0x44000: None,
    0x4443E: None,
    0x446CD: None,
    0x44C27: None,
    0x44C94: None,
    0x44C6D: None,
    0x44C06: None,
    0x4499D: None,
    0x45E52: None,
    
    # Bank 1 (PRG $04000-$07FFF) - BANK_EMIT
    0x5547: None,
    0x5469: None,
    0x5A5F: None,
    0x5C43: None,
    0x537F: None,
}

print()
print("=" * 95)
print(f"{'PRG Address':>12}  {'Bank':>4}  {'CPU Addr':>8}  {'Function':<55}")
print("=" * 95)

bank_names = {
    28: "BANK_IR_OPT (ir_opt.c)",
    17: "BANK_COMPILE (ir_opt_ext.c + dynamos.c)",
    1:  "BANK_EMIT (ir_lower.c + emit_6502.c + opt_v2)",
    31: "FIXED (dynamos.c fixed, run_6502, etc.)",
}

# Group by bank
from itertools import groupby

entries_by_bank = defaultdict(list)
for prg in profiler_entries:
    bank = prg // 0x4000
    entries_by_bank[bank].append(prg)

for bank in sorted(entries_by_bank.keys()):
    bname = bank_names.get(bank, f"Bank {bank}")
    print(f"\n--- {bname} ---")
    for prg in sorted(entries_by_bank[bank]):
        func, fbank = find_function_at(prg)
        # Reconstruct CPU address
        if bank == 31:
            cpu = 0xC000 + (prg - 31 * 0x4000)
        else:
            cpu = 0x8000 + (prg - bank * 0x4000)
        print(f"  ${prg:05X}    {bank:>3}   ${cpu:04X}    {func}")

# -------------------------------------------------------------------
# Step 6: Summary of IR optimization functions
# -------------------------------------------------------------------
print()
print("=" * 95)
print("SUMMARY: IR Optimization Pass Functions → PRG Address Mapping")
print("=" * 95)

ir_functions = [
    # (function_name, source_file, bank, description)
    ("ir_optimize",           "ir_opt.c",     28, "Main IR optimizer dispatch loop"),
    ("ir_opt_redundant_load", "ir_opt.c",     28, "Redundant load elimination"),
    ("ir_opt_dead_store",     "ir_opt.c",     28, "Dead store elimination"),
    ("ir_opt_dead_load",      "ir_opt.c",     28, "Dead load elimination"),
    ("ir_opt_php_plp_elision","ir_opt.c",     28, "PHP/PLP flag save elision"),
    ("ir_optimize_ext",       "ir_opt_ext.c", 17, "Extended optimizer dispatch"),
    ("ir_opt_pair_rewrite",   "ir_opt_ext.c", 17, "Instruction pair rewriting"),
    ("ir_opt_clc_sec_sink",   "ir_opt_ext.c", 17, "CLC/SEC carry sinking"),
    ("ir_opt_rmw_fusion",     "ir_lower.c",   1,  "RMW instruction fusion"),
    ("ir_lower",              "ir_lower.c",   1,  "IR to native 6502 lowering"),
]

print(f"\n{'Function':<30} {'Source':<18} {'Bank':>4}  {'CPU':>7}  {'PRG':>8}  {'Description'}")
print("-" * 110)

for fname, src, bank, desc in ir_functions:
    label = "_" + fname
    if label in labels or label in [n for _, n in labels.items()]:
        for cpu_addr, name in labels.items():
            if name == label:
                if bank == 31:
                    prg = 31 * 0x4000 + (cpu_addr - 0xC000)
                else:
                    prg = bank * 0x4000 + (cpu_addr - 0x8000)
                print(f"{fname:<30} {src:<18} {bank:>4}  ${cpu_addr:04X}  ${prg:05X}   {desc}")
                break

print()
print("KEY BANK RANGES for profiler filtering:")
print(f"  Bank 28 (BANK_IR_OPT):  PRG ${28*0x4000:05X}-${28*0x4000+0x3FFF:05X}  <- ir_optimize + core passes")
print(f"  Bank 17 (BANK_COMPILE): PRG ${17*0x4000:05X}-${17*0x4000+0x3FFF:05X}  <- ir_optimize_ext + pair rewrite + CLC/SEC sink")
print(f"  Bank  1 (BANK_EMIT):    PRG ${1*0x4000:05X}-${1*0x4000+0x3FFF:05X}  <- ir_lower + RMW fusion + emit helpers")
print(f"  Bank 31 (FIXED):        PRG ${31*0x4000:05X}-${31*0x4000+0x3FFF:05X}  <- run_6502 + recompile_opcode + bankswitch")
