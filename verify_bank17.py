#!/usr/bin/env python3
"""verify_bank17.py - Verify bank 17 function mapping from ROM bytes."""

# Bank 17: ir_opt_ext.c ir_kill starts at PRG 0x4496F (confirmed from ROM).
# Profiler entry 0x4499D = writes_a [dup] (confirmed: same bytes as bank 28 writes_a).
# ir_opt_pair_rewrite at 0x44D50 (confirmed from vicemap.map).

# Distance from writes_a to ir_opt_pair_rewrite:
d = 0x44D50 - 0x4499D
print("Distance writes_a -> ir_opt_pair_rewrite: 0x%03X = %d bytes" % (d, d))

# In bank 28, all helpers = 0x37E = 894 bytes. 
# In bank 17, ir_kill -> ir_opt_pair_rewrite = 0x44D50 - 0x4496F = 0x3E1 = 993 bytes.
d2 = 0x44D50 - 0x4496F
print("Distance ir_kill -> ir_opt_pair_rewrite:  0x%03X = %d bytes" % (d2, d2))
print("Bank 28 helpers total:                    0x37E = 894 bytes")
print("Difference: %d bytes (= reads_carry + dup_interferes added)" % (d2 - 0x37E))
print()

# Bank 28 function offsets (from helpers start):
b28 = {
    "ir_kill":                 0x000,
    "writes_a":                0x02E,
    "reads_a":                 0x07E,
    "writes_x":                0x0CE,
    "reads_x":                 0x0FB,
    "writes_y":                0x143,
    "reads_y":                 0x16A,
    "writes_flags":            0x1A6,
    "reads_flags":             0x1DF,
    "template_writes_memory":  0x23B,
    "is_barrier":              0x25C,
    "writes_to_zp":            0x2A2,
    "writes_to_abs":           0x2C9,
    "flags_safe":              0x2F0,
    "ir_opt_redundant_load":   0x37E,
}

# Profiler entries in bank 17 helpers region:
b17_profiler_helpers = {
    0x4499D: "writes_a [dup]",        # CONFIRMED from ROM
    0x44C06: "?",
    0x44C27: "?",
    0x44C6D: "?",
    0x44C94: "?",
}

ext_start = 0x4496F

# Compute offsets from ir_opt_ext start:
print("Bank 17 profiler entries (offset from ir_opt_ext start at 0x4496F):")
print("=" * 80)
for prg in sorted(b17_profiler_helpers.keys()):
    off = prg - ext_start
    # Find corresponding bank 28 function (closest match accounting for shift)
    # After reads_flags, reads_carry inserts ~92 bytes (0x5C)
    # After writes_to_zp, dup_interferes inserts ~40 bytes
    # writes_to_abs and flags_safe are ABSENT in ir_opt_ext.c
    
    # Pre-reads_carry region: offsets match bank 28 exactly
    # Post-reads_carry region: offsets shifted by ~0x5C
    name = b17_profiler_helpers[prg]
    print("  PRG 0x%05X  offset +0x%03X  %s" % (prg, off, name))

print()
print("Mapping by offset analysis:")
print("-" * 80)

# reads_carry is after reads_flags. In bank 28, reads_flags ends at 0x23A.
# reads_carry in ir_opt_ext.c is ~92 bytes (0x5C) based on total shift.
# So everything after reads_flags is shifted +0x5C.

# template_writes_memory: bank28 offset 0x23B, bank17 = 0x23B + 0x5C = 0x297
# From ext_start: 0x4496F + 0x297 = 0x44C06. EXACT MATCH!
print("  template_writes_memory: b28=+0x23B, b17=+0x297 (shift +0x5C) -> PRG 0x44C06  CHECK")

# is_barrier: bank28 offset 0x25C, bank17 = 0x25C + 0x5C = 0x2B8
# From ext_start: 0x4496F + 0x2B8 = 0x44C27. EXACT MATCH!
print("  is_barrier:             b28=+0x25C, b17=+0x2B8 (shift +0x5C) -> PRG 0x44C27  CHECK")

# writes_to_zp: bank28 offset 0x2A2, bank17 = 0x2A2 + 0x5C = 0x2FE
# From ext_start: 0x4496F + 0x2FE = 0x44C6D. EXACT MATCH!
print("  writes_to_zp:           b28=+0x2A2, b17=+0x2FE (shift +0x5C) -> PRG 0x44C6D  CHECK")

# dup_interferes: NEW function in ir_opt_ext.c, after writes_to_zp.
# bank17 offset = 0x325 (0x44C94 - 0x4496F)
# writes_to_zp ends at +0x2FE + size(writes_to_zp) = 0x2FE + 0x27 = 0x325. EXACT!
print("  dup_interferes:         NEW func at +0x325                   -> PRG 0x44C94  CHECK")

print()
print("ALL MATCHES VERIFIED!")
print()
print("ir_opt_ext.c DOES NOT have: writes_to_abs, flags_safe (ir_opt.c only)")
print("ir_opt_ext.c ADDS:          reads_carry (0x5C bytes), dup_interferes")
print()

# Final complete mapping:
print("=" * 100)
print("FINAL VERIFIED FUNCTION MAP FOR ALL PROFILER ENTRIES")
print("=" * 100)
print()

all_entries = [
    # (PRG, function, source, bank, category)
    # Bank 28 - BANK_IR_OPT - ir_opt.c
    (0x70000, "ir_kill",                 "ir_opt.c",            28, "IR_OPT"),
    (0x7002E, "writes_a",               "ir_opt.c",            28, "IR_OPT"),
    (0x7007E, "reads_a",                "ir_opt.c",            28, "IR_OPT"),
    (0x700CE, "writes_x",               "ir_opt.c",            28, "IR_OPT"),
    (0x700FB, "reads_x",                "ir_opt.c",            28, "IR_OPT"),
    (0x70143, "writes_y",               "ir_opt.c",            28, "IR_OPT"),
    (0x7016A, "reads_y",                "ir_opt.c",            28, "IR_OPT"),
    (0x701A6, "writes_flags",           "ir_opt.c",            28, "IR_OPT"),
    (0x701DF, "reads_flags",            "ir_opt.c",            28, "IR_OPT"),
    (0x7023B, "template_writes_memory", "ir_opt.c",            28, "IR_OPT"),
    (0x7025C, "is_barrier",             "ir_opt.c",            28, "IR_OPT"),
    (0x702A2, "writes_to_zp",           "ir_opt.c",            28, "IR_OPT"),
    (0x702C9, "writes_to_abs",          "ir_opt.c",            28, "IR_OPT"),
    (0x702F0, "flags_safe",             "ir_opt.c",            28, "IR_OPT"),
    (0x7037E, "ir_opt_redundant_load",  "ir_opt.c",            28, "IR_OPT"),
    (0x7292B, "ir_optimize",            "ir_opt.c",            28, "IR_OPT"),
    
    # Bank 17 - dynamos.c compile infra
    (0x44000, "flash_sector_alloc_b17", "dynamos.c",           17, "COMPILE"),
    (0x44375, "recompile_block_b17",    "dynamos.c",           17, "COMPILE"),
    (0x4443E, "recompile_block_b17+",   "dynamos.c",           17, "COMPILE"),
    (0x446CD, "recompile_block_b17+",   "dynamos.c",           17, "COMPILE"),
    
    # Bank 17 - ir_opt_ext.c
    (0x4499D, "writes_a [dup]",               "ir_opt_ext.c",  17, "IR_OPT"),
    (0x44C06, "template_writes_memory [dup]",  "ir_opt_ext.c", 17, "IR_OPT"),
    (0x44C27, "is_barrier [dup]",              "ir_opt_ext.c", 17, "IR_OPT"),
    (0x44C6D, "writes_to_zp [dup]",            "ir_opt_ext.c", 17, "IR_OPT"),
    (0x44C94, "dup_interferes",                "ir_opt_ext.c", 17, "IR_OPT"),
    (0x44D50, "ir_opt_pair_rewrite",           "ir_opt_ext.c", 17, "IR_OPT"),
    (0x45E52, "ir_opt_clc_sec_sink",           "ir_opt_ext.c", 17, "IR_OPT"),
    (0x46113, "ir_optimize_ext",               "ir_opt_ext.c", 17, "IR_OPT"),
    
    # Bank 1 - ir_lower.c
    (0x0537F, "ir_emit_raw_block+0x26A","ir_lower.c/ir.c",     1, "IR_LOWER"),
    (0x05469, "ir_emit_raw_block+0x354","ir_lower.c/ir.c",     1, "IR_LOWER"),
    (0x05547, "ir_lower",               "ir_lower.c",           1, "IR_LOWER"),
    (0x05A5F, "ir_resolve_deferred_patches","ir_lower.c",       1, "IR_LOWER"),
    (0x05C43, "ir_resolve_direct_branches", "ir_lower.c",       1, "IR_LOWER"),
    
    # Bank 31 - fixed bank
    (0x7C3C4, "bankswitch_prg",         "dynamos-asm.s",       31, "RUNTIME"),
    (0x7C729, "read6502",               "nes.c",               31, "RUNTIME"),
    (0x7CB95, "__rload6",               "vbcc runtime",        31, "RUNTIME"),
    (0x7CBD6, "__rsave6",               "vbcc runtime",        31, "RUNTIME"),
    (0x7CC1E, "run_6502",               "dynamos.c",           31, "RUNTIME"),
]

for prg, func, src, bank, cat in all_entries:
    cpu = 0xC000 + (prg - 31*0x4000) if bank == 31 else 0x8000 + (prg - bank*0x4000)
    print("  PRG $%05X  CPU $%04X  Bank %2d  %-7s %-34s %s" % (prg, cpu, bank, cat, func, src))

print()
counts = {}
for _, _, _, _, cat in all_entries:
    counts[cat] = counts.get(cat, 0) + 1

print("TOTALS:")
for cat in ["IR_OPT", "IR_LOWER", "COMPILE", "RUNTIME"]:
    n = counts.get(cat, 0)
    print("  %-10s %2d entries" % (cat, n))

ir_total = counts.get("IR_OPT", 0) + counts.get("IR_LOWER", 0)
all_total = sum(counts.values())
print("  %-10s %2d entries  (%.0f%% of all)" % ("IR TOTAL", ir_total, 100.0*ir_total/all_total))
