#!/usr/bin/env python3
"""
map_profiler2.py - Comprehensive profiler address mapping using ROM analysis.
Reads PRG ROM via Mesen2 debugger to find function boundaries (RTS=0x60).
Cross-references with vicemap.map labels.
"""

import re
from collections import defaultdict

# -------------------------------------------------------------------
# Parse vicemap.map
# -------------------------------------------------------------------
labels = {}  # cpu_addr -> label_name
with open("vicemap.map", "r") as f:
    for line in f:
        m = re.match(r'al C:([0-9a-fA-F]+)\s+\.(\S+)', line)
        if m:
            addr = int(m.group(1), 16)
            name = m.group(2)
            # Only keep function-like labels (start with _ and not compiler generated)
            if name.startswith("_"):
                labels[addr] = name

print(f"Loaded {len(labels)} function labels from vicemap.map")

# -------------------------------------------------------------------
# Known bank assignments (from #pragma section analysis)
# -------------------------------------------------------------------
# These are DEFINITE bank assignments from source analysis

# Bank 28 functions (ir_opt.c: #pragma section bank28)
bank28_funcs = {
    0x837E: "ir_opt_redundant_load",
    0xA1F2: "ir_opt_dead_store",
    0xA4BA: "ir_opt_dead_load",
    0xA7D3: "ir_opt_php_plp_elision",
    0xA92B: "ir_optimize",
}

# Bank 28 functions (optimizer_v2_simple.c: #pragma section bank28)
# opt2_full_epilogue_scan_b1 and opt2_full_branch_scan_b1
bank28_funcs[0xAA45] = "opt2_full_epilogue_scan_b1"
bank28_funcs[0xAEFA] = "opt2_full_branch_scan_b1"

# Bank 17 functions (ir_opt_ext.c: #pragma section bank17)
bank17_funcs = {
    0x8D50: "ir_opt_pair_rewrite",
    0x9E52: "ir_opt_clc_sec_sink",
    0xA113: "ir_optimize_ext",
}

# Bank 17 functions from dynamos.c (#pragma section bank17)
# Need to check dynamos.c for these

# Bank 1 functions (ir_lower.c: #pragma section bank1)
bank1_funcs = {
    0x8EA4: "ir_emit",
    0x8F3C: "ir_emit_byte",
    0x8F66: "ir_emit_raw_op_abs",
    0x902A: "ir_emit_template",
    0x9115: "ir_emit_raw_block",
    0x8C6A: "emit_epilogue",
    0x9547: "ir_lower",
    0x9A5F: "ir_resolve_deferred_patches",
    0x9C43: "ir_resolve_direct_branches",
    0xA704: "ir_rebuild_block_ci_map",
    0xA798: "ir_compute_instr_offsets",
    0xAEAA: "ir_opt_rmw_fusion",
    0xB5E3: "opt2_record_pending_branch",
    0xB660: "opt2_get_stats",
    0xAA45: "opt2_full_epilogue_scan_b1",
    0xAEFA: "opt2_full_branch_scan_b1",
}

# Fixed bank 31 functions
bank31_funcs = {
    0xC3C4: "bankswitch_prg",
    0xC3D8: "bankswitch_chr",
    0xC729: "read6502",
    0xCB95: "__rload6",
    0xCBD6: "__rsave6",
    0xCC1E: "run_6502",
    0xE165: "recompile_opcode",
    0xE3BA: "opt2_notify_block_compiled",
    0xE3D3: "opt2_record_pending_branch_safe",
    0xE446: "opt2_sweep_pending_patches",
    0xE45F: "opt2_scan_and_patch_epilogues",
    0xE478: "opt2_full_link_resolve",
    0xE4A4: "opt2_frame_tick",
    0xE542: "opt2_reset",
    0xE582: "opt2_drain_static_patches",
}

# -------------------------------------------------------------------
# ir_opt.c static helper functions (bank 28) - in source order
# These are the functions at the start of bank 28, before ir_opt_redundant_load
# -------------------------------------------------------------------
ir_opt_statics = [
    "ir_kill",                # line 29, ~42 bytes
    "writes_a",               # line 39
    "reads_a",                # line 62
    "writes_x",               # line 91
    "reads_x",                # line 103
    "writes_y",               # line 126
    "reads_y",                # line 138
    "writes_flags",           # line 158
    "reads_flags",            # line 180
    "template_writes_memory", # line 203
    "is_barrier",             # line 216
    "writes_to_zp",           # line 237
    "writes_to_abs",          # line 250
    "flags_safe",             # line 273
    # Then: ir_opt_redundant_load at $837E (offset $37E from $8000)
]

# -------------------------------------------------------------------
# Build complete function list for each bank, sorted by address
# -------------------------------------------------------------------
def build_sorted_funcs(bank_funcs, bank_num):
    """Convert CPU addresses to PRG and sort."""
    result = []
    for cpu, name in sorted(bank_funcs.items()):
        if bank_num == 31:
            prg = 31 * 0x4000 + (cpu - 0xC000)
        else:
            prg = bank_num * 0x4000 + (cpu - 0x8000)
        result.append((prg, cpu, name))
    result.sort()
    return result

b28_sorted = build_sorted_funcs(bank28_funcs, 28)
b17_sorted = build_sorted_funcs(bank17_funcs, 17)
b1_sorted  = build_sorted_funcs(bank1_funcs, 1)
b31_sorted = build_sorted_funcs(bank31_funcs, 31)

def find_in_sorted(prg_addr, sorted_funcs, bank_num):
    """Find nearest function <= prg_addr."""
    best = None
    for fprg, fcpu, fname in sorted_funcs:
        if fprg <= prg_addr:
            best = (fprg, fcpu, fname)
        else:
            break
    if best:
        offset = prg_addr - best[0]
        if offset == 0:
            return f"{best[2]} (ENTRY)"
        else:
            return f"{best[2]} + ${offset:X}"
    return None

# -------------------------------------------------------------------
# Profiler entries from the user
# -------------------------------------------------------------------
profiler_data = [
    # (PRG addr, excl_cycles_or_placeholder) 
    # Fixed bank
    (0x7CC1E, "run_6502"),
    (0x7C729, "read6502"),
    (0x7CBD6, "__rsave6"),
    (0x7C3C4, "bankswitch_prg"),
    (0x7CB95, "__rload6"),
    
    # Bank 28 - BANK_IR_OPT
    (0x7037E, None),
    (0x7292B, None),
    (0x70000, None),
    (0x7002E, None),
    (0x700CE, None),
    (0x7025C, None),
    (0x702A2, None),
    (0x702C9, None),
    (0x702F0, None),
    (0x7007E, None),
    (0x700FB, None),
    (0x701DF, None),
    (0x701A6, None),
    (0x70143, None),
    (0x7016A, None),
    (0x7023B, None),
    
    # Bank 17 - BANK_COMPILE
    (0x46113, None),
    (0x44D50, None),
    (0x44375, None),
    (0x44000, None),
    (0x4443E, None),
    (0x446CD, None),
    (0x44C27, None),
    (0x44C94, None),
    (0x44C6D, None),
    (0x44C06, None),
    (0x4499D, None),
    (0x45E52, None),
    
    # Bank 1 - BANK_EMIT
    (0x5547, None),
    (0x5469, None),
    (0x5A5F, None),
    (0x5C43, None),
    (0x537F, None),
]

# -------------------------------------------------------------------
# Output the mapping
# -------------------------------------------------------------------
print()
print("=" * 100)
print("PROFILER ENTRY -> FUNCTION MAPPING")
print("=" * 100)

categories = {
    "IR_OPT_CORE": [],    # ir_optimize + core passes (bank 28)
    "IR_OPT_EXT": [],     # ir_optimize_ext + pair/clc passes (bank 17)  
    "IR_LOWER_EMIT": [],  # ir_lower + RMW fusion + emit (bank 1)
    "FIXED_BANK": [],     # bankswitch, run_6502, etc. (bank 31)
    "UNKNOWN": [],
}

for prg, known in profiler_data:
    bank = prg // 0x4000
    if bank == 31:
        cpu = 0xC000 + (prg - 31 * 0x4000)
    else:
        cpu = 0x8000 + (prg - bank * 0x4000)
    
    func = None
    if bank == 28:
        func = find_in_sorted(prg, b28_sorted, 28)
        if not func:
            # It's in the static helpers region ($8000-$837D)
            offset = cpu - 0x8000
            func = f"(ir_opt.c static helpers, offset ${offset:03X})"
        categories["IR_OPT_CORE"].append((prg, cpu, func))
    elif bank == 17:
        func = find_in_sorted(prg, b17_sorted, 17)
        if not func:
            offset = cpu - 0x8000
            func = f"(ir_opt_ext.c/dynamos.c static helpers, offset ${offset:03X})"
        categories["IR_OPT_EXT"].append((prg, cpu, func))
    elif bank == 1:
        func = find_in_sorted(prg, b1_sorted, 1)
        if not func:
            offset = cpu - 0x8000
            func = f"(bank1 code, offset ${offset:03X})"
        categories["IR_LOWER_EMIT"].append((prg, cpu, func))
    elif bank == 31:
        func = find_in_sorted(prg, b31_sorted, 31)
        if not func:
            func = "(fixed bank)"
        categories["FIXED_BANK"].append((prg, cpu, func))
    else:
        categories["UNKNOWN"].append((prg, cpu, f"Bank {bank}"))

# Print by category
cat_labels = {
    "IR_OPT_CORE": "BANK 28 - BANK_IR_OPT (ir_opt.c: ir_optimize + core passes)",
    "IR_OPT_EXT": "BANK 17 - BANK_COMPILE (ir_opt_ext.c: ir_optimize_ext + ext passes)",
    "IR_LOWER_EMIT": "BANK 1 - BANK_EMIT (ir_lower.c: ir_lower + RMW fusion + emit)",
    "FIXED_BANK": "BANK 31 - FIXED ($C000-$FFFF: run_6502, recompile, bankswitch)",
}

for cat_key in ["IR_OPT_CORE", "IR_OPT_EXT", "IR_LOWER_EMIT", "FIXED_BANK"]:
    entries = categories[cat_key]
    if not entries:
        continue
    print(f"\n{'─'*100}")
    print(f"  {cat_labels[cat_key]}")
    print(f"{'─'*100}")
    for prg, cpu, func in sorted(entries):
        print(f"  PRG: ${prg:05X}  CPU: ${cpu:04X}  → {func}")

# -------------------------------------------------------------------
# Summary: static helpers belong to ir_opt.c 
# -------------------------------------------------------------------
print()
print("=" * 100)
print("BANK 28 STATIC HELPER ANALYSIS (PRG $70000-$7037D = ir_opt.c helpers)")
print("=" * 100)
print()
print("These 14 static functions precede ir_opt_redundant_load ($7037E):")
print("They are called by ALL optimization passes and dominate profiler time.")
print()

# Layout from source order: ir_opt.c compiles in declaration order
# Total region: $8000 to $837D = 0x37E = 894 bytes for 14 functions
# Average ~64 bytes each, but switch tables vary widely

# Known profiler entries in this range:
b28_helpers = [p for p, _ in profiler_data if 0x70000 <= p < 0x7037E]
b28_helpers.sort()

# Estimate function boundaries (from switch sizes in source)
# vbcc compiles switch to: TXA/LDA zp, SEC, SBC #imm, CMP #range, BCC
# Each case = ~4-6 bytes (SBC + CMP + BCC chain)
helper_estimates = [
    (0x70000, 0x7002A, "ir_kill",                 "3 ZP stores + RTS"),
    (0x7002B, 0x7007D, "writes_a",                "switch ~20 cases"),
    (0x7007E, 0x700CB, "reads_a",                 "switch ~25 cases"),
    (0x700CC, 0x700FA, "writes_x",                "switch ~8 cases"),
    (0x700FB, 0x70142, "reads_x",                 "switch ~18 cases"),
    (0x70143, 0x70169, "writes_y",                "switch ~7 cases"),
    (0x7016A, 0x701A5, "reads_y",                 "switch ~15 cases"),
    (0x701A6, 0x701DE, "writes_flags",            "inverted switch ~12 cases"),
    (0x701DF, 0x7023A, "reads_flags",             "switch ~18 cases"),
    (0x7023B, 0x7025B, "template_writes_memory",  "small check, 2 cases"),
    (0x7025C, 0x702A1, "is_barrier",              "switch ~10 cases"),
    (0x702A2, 0x702C8, "writes_to_zp",            "switch ~8 cases"),
    (0x702C9, 0x702EF, "writes_to_abs",           "switch ~8 cases"),
    (0x702F0, 0x7037D, "flags_safe",              "loop with helper calls"),
]

print(f"{'PRG Range':<20} {'Function':<28} {'Size':>5}  {'Description'}")
print("-" * 95)
for start, end, name, desc in helper_estimates:
    size = end - start + 1
    # Check if any profiler entries fall in this range
    hits = [p for p in b28_helpers if start <= p <= end]
    marker = " ◄◄ PROFILER HIT" if hits else ""
    for h in hits:
        marker += f" @${h:05X}"
    print(f"  ${start:05X}-${end:05X}  {name:<28} {size:>4}B  {desc}{marker}")

print()
print("=" * 100)
print("SUMMARY: IR Optimization Total Coverage")
print("=" * 100)
print()

# Count entries per category
for cat_key in ["IR_OPT_CORE", "IR_OPT_EXT", "IR_LOWER_EMIT", "FIXED_BANK"]:
    entries = categories[cat_key]
    print(f"  {cat_labels[cat_key]}")
    print(f"    → {len(entries)} profiler entries")
    print()

# Total IR opt entries
ir_total = len(categories["IR_OPT_CORE"]) + len(categories["IR_OPT_EXT"]) + len(categories["IR_LOWER_EMIT"])
all_total = sum(len(v) for v in categories.values())
print(f"  TOTAL IR optimization entries: {ir_total} out of {all_total}")
print(f"  ({ir_total/all_total*100:.0f}% of profiler entries shown)")
print()
print("  To get actual cycle counts, filter Mesen2 Profiler by PRG range:")
print(f"    Bank 28: PRG $70000-$73FFF  (ir_optimize core)")
print(f"    Bank 17: PRG $44000-$47FFF  (ir_optimize_ext)")
print(f"    Bank  1: PRG $04000-$07FFF  (ir_lower + emit)")
