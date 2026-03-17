#!/usr/bin/env python3
"""
map_profiler_final.py - Complete profiler address mapping for NES recompiler.
Maps Mesen2 PRG addresses to function names using vicemap.map + ROM analysis.
"""

import re

# ============================================================================
# Parse vicemap.map for function labels
# ============================================================================
labels = {}
with open("vicemap.map", "r") as f:
    for line in f:
        m = re.match(r'al C:([0-9a-fA-F]+)\s+\.(\S+)', line)
        if m:
            addr = int(m.group(1), 16)
            name = m.group(2)
            labels[addr] = name

# ============================================================================
# BANK 28 (BANK_IR_OPT) - ir_opt.c static helpers + optimization passes
# Verified by ROM byte analysis (RTS boundaries match exactly)
# ============================================================================
bank28_functions = [
    # (PRG_addr, CPU_addr, function_name, source_file, description)
    (0x70000, 0x8000, "ir_kill",                 "ir_opt.c", "Kill/delete an IR node"),
    (0x7002E, 0x802E, "writes_a",               "ir_opt.c", "Check if opcode writes A"),
    (0x7007E, 0x807E, "reads_a",                "ir_opt.c", "Check if opcode reads A"),
    (0x700CE, 0x80CE, "writes_x",               "ir_opt.c", "Check if opcode writes X"),
    (0x700FB, 0x80FB, "reads_x",                "ir_opt.c", "Check if opcode reads X"),
    (0x70143, 0x8143, "writes_y",               "ir_opt.c", "Check if opcode writes Y"),
    (0x7016A, 0x816A, "reads_y",                "ir_opt.c", "Check if opcode reads Y"),
    (0x701A6, 0x81A6, "writes_flags",           "ir_opt.c", "Check if opcode writes flags"),
    (0x701DF, 0x81DF, "reads_flags",            "ir_opt.c", "Check if opcode reads flags"),
    (0x7023B, 0x823B, "template_writes_memory", "ir_opt.c", "Check if template writes mem"),
    (0x7025C, 0x825C, "is_barrier",             "ir_opt.c", "Check if node is opt barrier"),
    (0x702A2, 0x82A2, "writes_to_zp",           "ir_opt.c", "Check if opcode writes ZP"),
    (0x702C9, 0x82C9, "writes_to_abs",          "ir_opt.c", "Check if opcode writes ABS"),
    (0x702F0, 0x82F0, "flags_safe",             "ir_opt.c", "Check if flags unused ahead"),
    (0x7037E, 0x837E, "ir_opt_redundant_load",  "ir_opt.c", "Pass: redundant load elim"),
    (0x721F2, 0xA1F2, "ir_opt_dead_store",      "ir_opt.c", "Pass: dead store elim"),
    (0x724BA, 0xA4BA, "ir_opt_dead_load",       "ir_opt.c", "Pass: dead load elim"),
    (0x727D3, 0xA7D3, "ir_opt_php_plp_elision", "ir_opt.c", "Pass: PHP/PLP elision"),
    (0x7292B, 0xA92B, "ir_optimize",            "ir_opt.c", "Main: IR optimizer dispatch"),
]

# Also in bank 28 from optimizer_v2_simple.c:
bank28_opt2 = [
    (0x72A45, 0xAA45, "opt2_full_epilogue_scan_b1", "optimizer_v2_simple.c", "Epilogue scan (all blocks)"),
    (0x72EFA, 0xAEFA, "opt2_full_branch_scan_b1",   "optimizer_v2_simple.c", "Branch scan (all blocks)"),
]

# ============================================================================
# BANK 17 (BANK_COMPILE) - dynamos.c compile infra + ir_opt_ext.c
# dynamos.c bank17 functions come first (linker order), then ir_opt_ext.c
# ============================================================================
bank17_dynamos = [
    # dynamos.c bank17 compile infrastructure
    (0x44000, 0x8000, "flash_sector_alloc_b17",         "dynamos.c", "Flash sector allocation"),
    (0x44375, 0x8375, "recompile_block_b17 (est.)",     "dynamos.c", "Block recompilation logic"),
    (0x4443E, 0x843E, "recompile_block_b17 + body",     "dynamos.c", "Block recompilation (cont)"),
    (0x446CD, 0x86CD, "recompile_block_b17 + body",     "dynamos.c", "Block recompilation (cont)"),
]

bank17_iropt_ext = [
    # ir_opt_ext.c duplicate helpers (same as ir_opt.c but in bank17)
    (0x4499D, 0x899D, "ir_kill [dup]",                  "ir_opt_ext.c", "Duplicate helper"),
    (0x44C06, 0x8C06, "template_writes_memory [dup]",   "ir_opt_ext.c", "Duplicate helper (est)"),
    (0x44C27, 0x8C27, "is_barrier [dup]",               "ir_opt_ext.c", "Duplicate helper (est)"),
    (0x44C6D, 0x8C6D, "writes_to_zp [dup]",             "ir_opt_ext.c", "Duplicate helper (est)"),
    (0x44C94, 0x8C94, "dup_interferes",                  "ir_opt_ext.c", "Interference check for pairs"),
    # Confirmed from vicemap.map:
    (0x44D50, 0x8D50, "ir_opt_pair_rewrite",             "ir_opt_ext.c", "Pass: instruction pair rewrite"),
    (0x45E52, 0x9E52, "ir_opt_clc_sec_sink",             "ir_opt_ext.c", "Pass: CLC/SEC carry sinking"),
    (0x46113, 0xA113, "ir_optimize_ext",                  "ir_opt_ext.c", "Extended optimizer dispatch"),
]

# ============================================================================
# BANK 1 (BANK_EMIT) - ir_lower.c + emit_6502.c + optimizer_v2_simple.c
# ============================================================================
bank1_functions = [
    (0x0537F, 0x937F, "ir_emit_raw_block + $26A",            "ir_lower.c/ir.c", "Raw block emit (inner)"),
    (0x05469, 0x9469, "ir_emit_raw_block + $354",            "ir_lower.c/ir.c", "Raw block emit (inner)"),
    (0x05547, 0x9547, "ir_lower",                             "ir_lower.c",      "IR to native 6502 lowering"),
    (0x05A5F, 0x9A5F, "ir_resolve_deferred_patches",         "ir_lower.c",      "Deferred branch patches"),
    (0x05C43, 0x9C43, "ir_resolve_direct_branches",          "ir_lower.c",      "Direct branch resolution"),
]

# ============================================================================
# BANK 31 (FIXED) - run_6502, recompile_opcode, bankswitch, etc.
# ============================================================================
bank31_functions = [
    (0x7C3C4, 0xC3C4, "bankswitch_prg",                     "dynamos-asm.s", "PRG bankswitch"),
    (0x7C729, 0xC729, "read6502",                            "nes.c",         "NES memory read"),
    (0x7CB95, 0xCB95, "__rload6 (vbcc runtime)",             "vbcc",          "Register restore"),
    (0x7CBD6, 0xCBD6, "__rsave6 (vbcc runtime)",             "vbcc",          "Register save"),
    (0x7CC1E, 0xCC1E, "run_6502",                            "dynamos.c",     "Main recompiler loop"),
]

# ============================================================================
# OUTPUT
# ============================================================================
print("=" * 110)
print("  MESEN2 PROFILER -> FUNCTION MAP (NES Recompiler: DynaMoS)")
print("=" * 110)

def print_section(title, entries, is_ir_opt=False):
    print()
    print("-" * 110)
    hdr = "  " + title
    if is_ir_opt:
        hdr += "  [IR OPTIMIZATION]"
    print(hdr)
    print("-" * 110)
    hdr = "  PRG Addr    CPU    Function                          Source             Description"
    print(hdr)
    print("  " + "-" * 106)
    for prg, cpu, name, src, desc in entries:
        print("  $%05X   $%04X   %-34s %-18s %s" % (prg, cpu, name, src, desc))

print_section(
    "BANK 28 (BANK_IR_OPT)  PRG $70000-$73FFF  --  ir_opt.c core optimization",
    bank28_functions, is_ir_opt=True)

print_section(
    "BANK 17 (BANK_COMPILE) PRG $44000-$47FFF  --  dynamos.c compile infrastructure",
    bank17_dynamos, is_ir_opt=False)

print_section(
    "BANK 17 (BANK_COMPILE) PRG $44000-$47FFF  --  ir_opt_ext.c extended optimization",
    bank17_iropt_ext, is_ir_opt=True)

print_section(
    "BANK 1  (BANK_EMIT)    PRG $04000-$07FFF  --  ir_lower.c + emit helpers",
    bank1_functions, is_ir_opt=True)

print_section(
    "BANK 31 (FIXED)        PRG $7C000-$7FFFF  --  fixed bank dispatch/runtime",
    bank31_functions, is_ir_opt=False)

# ============================================================================
# Classification summary
# ============================================================================
print()
print("=" * 110)
print("  CLASSIFICATION SUMMARY")
print("=" * 110)
print()

ir_core = bank28_functions  # 16 entries
ir_ext_helpers = bank17_iropt_ext[:5]  # 5 helper entries
ir_ext_passes = bank17_iropt_ext[5:]   # 3 pass entries
ir_lower = bank1_functions             # 5 entries
compile_infra = bank17_dynamos         # 4 entries
fixed_runtime = bank31_functions       # 5 entries

n_ir_core = len(ir_core)
n_ir_ext = len(ir_ext_helpers) + len(ir_ext_passes)
n_ir_lower = len(ir_lower)
n_compile = len(compile_infra)
n_fixed = len(fixed_runtime)
n_total = n_ir_core + n_ir_ext + n_ir_lower + n_compile + n_fixed

print("  Category                              Entries  PRG Range")
print("  " + "-" * 75)
print("  IR optimization core (ir_opt.c)       %3d      $70000-$73FFF  (bank 28)" % n_ir_core)
print("  IR optimization ext  (ir_opt_ext.c)   %3d      $44000-$47FFF  (bank 17)" % n_ir_ext)
print("  IR lowering/emit     (ir_lower.c)     %3d      $04000-$07FFF  (bank 1)" % n_ir_lower)
print("  Compile infra        (dynamos.c b17)  %3d      $44000-$47FFF  (bank 17)" % n_compile)
print("  Fixed bank runtime   (bank 31)        %3d      $7C000-$7FFFF  (bank 31)" % n_fixed)
print("  " + "-" * 75)
print("  TOTAL                                 %3d" % n_total)

n_all_ir = n_ir_core + n_ir_ext + n_ir_lower
print()
print("  IR Optimization total:   %d / %d entries  (%.0f%%)" % (n_all_ir, n_total, 100.0 * n_all_ir / n_total))
print("  Compile infrastructure:  %d / %d entries  (%.0f%%)" % (n_compile, n_total, 100.0 * n_compile / n_total))
print("  Fixed bank runtime:      %d / %d entries  (%.0f%%)" % (n_fixed, n_total, 100.0 * n_fixed / n_total))

print()
print("=" * 110)
print("  HOW TO FILTER PROFILER FOR IR OPTIMIZATION ONLY")
print("=" * 110)
print()
print("  In Mesen2 Profiler, sum exclusive cycles for these PRG address ranges:")
print()
print("    1) Bank 28:  PRG $70000-$73FFF   ir_optimize() + all core passes")
print("       Contains: ir_optimize, ir_opt_redundant_load, ir_opt_dead_store,")
print("                 ir_opt_dead_load, ir_opt_php_plp_elision")
print("                 + 14 static helper switch-tables (writes_a, reads_a, etc.)")
print()
print("    2) Bank 17:  PRG $4499D-$47FFF   ir_optimize_ext() + extended passes")
print("       Contains: ir_optimize_ext, ir_opt_pair_rewrite, ir_opt_clc_sec_sink")
print("                 + duplicate helpers + dup_interferes")
print("       NOTE: PRG $44000-$4499C is compile INFRASTRUCTURE (dynamos.c),")
print("             NOT ir_opt. Exclude those addresses from IR opt totals.")
print()
print("    3) Bank 1:   PRG $04000-$07FFF   ir_lower() + RMW fusion + emit")
print("       Contains: ir_lower, ir_resolve_deferred_patches,")
print("                 ir_resolve_direct_branches, ir_emit_raw_block")
print("       NOTE: Bank 1 also has cpu_6502.c, platform_nes.c, emit_6502.c,")
print("             optimizer_v2_simple.c, etc. Only entries near ir_lower's")
print("             address ($05547+) are IR lowering cost.")
print()
print("  COMPILE PATH TOTAL = sum(bank 28) + sum(bank 17 IR entries)")
print("                     + sum(bank 1 IR entries) + sum(recompile_opcode in bank 31)")
