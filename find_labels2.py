import json

with open(r'C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.json', 'r', encoding='utf-8-sig') as f:
    data = json.load(f)

labels = data['WorkspaceByCpu']['Nes']['Labels']

# Build sorted NesPrgRom label list
prg_labels = sorted(
    [lbl for lbl in labels if lbl['MemoryType'] == 'NesPrgRom'],
    key=lambda x: x['Address']
)

# Verify mapping: _fff0_dispatch at $7FF0 -> CPU $FFF0
# This means PRG $4000-$7FFF maps to CPU $C000-$FFFF
# So CPU $DB4D = PRG $4000 + ($DB4D - $C000) = PRG $5B4D
print("=== Mapping verification ===")
print("_fff0_dispatch at PRG $7FF0 -> CPU $C000 + ($7FF0 - $4000) = $FFF0 ✓")
print("CPU $DB4D -> PRG $4000 + $1B4D = PRG $5B4D")
print("CPU $DB00 -> PRG $5B00")
print("CPU $DD70 -> PRG $5D70")

print()
print("=== ALL NesPrgRom labels from $5A00 to $5E00 (CPU $DA00-$DE00) ===")
# Find named (non-l###) labels first
for lbl in prg_labels:
    if 0x5A00 <= lbl['Address'] <= 0x5E00:
        name = lbl['Label']
        is_auto = name.startswith('l') and name[1:].isdigit()
        marker = "" if is_auto else " <<<NAMED>>>"
        cpu_addr = lbl['Address'] - 0x4000 + 0xC000
        print(f"  PRG ${lbl['Address']:04X} (CPU ${cpu_addr:04X}): {name}{marker}")

print()
print("=== Named functions near $5B4D (PRG), i.e. CPU $DB4D ===")
# Find the closest named (non-auto) labels
named_before = None
named_after = None
for lbl in prg_labels:
    name = lbl['Label']
    is_auto = name.startswith('l') and name[1:].isdigit()
    if is_auto:
        continue
    if lbl['Address'] <= 0x5B4D:
        named_before = lbl
    if lbl['Address'] >= 0x5B4D and named_after is None:
        named_after = lbl

if named_before:
    cpu = named_before['Address'] - 0x4000 + 0xC000
    dist = 0x5B4D - named_before['Address']
    print(f"  Before: PRG ${named_before['Address']:04X} (CPU ${cpu:04X}): {named_before['Label']}  (${dist:X} bytes before)")
if named_after:
    cpu = named_after['Address'] - 0x4000 + 0xC000
    dist = named_after['Address'] - 0x5B4D
    print(f"  After:  PRG ${named_after['Address']:04X} (CPU ${cpu:04X}): {named_after['Label']}  (${dist:X} bytes after)")

# Also find the immediate labels (including auto) around $5B4D
print()
print("=== Immediate labels around PRG $5B4D (CPU $DB4D) ===")
prev_lbl = None
for lbl in prg_labels:
    if lbl['Address'] > 0x5B4D:
        if prev_lbl:
            cpu = prev_lbl['Address'] - 0x4000 + 0xC000
            print(f"  Just before: PRG ${prev_lbl['Address']:04X} (CPU ${cpu:04X}): {prev_lbl['Label']}  ({0x5B4D - prev_lbl['Address']} bytes before $DB4D)")
        cpu = lbl['Address'] - 0x4000 + 0xC000
        print(f"  Just after:  PRG ${lbl['Address']:04X} (CPU ${cpu:04X}): {lbl['Label']}  ({lbl['Address'] - 0x5B4D} bytes after $DB4D)")
        break
    prev_lbl = lbl

# Show all labels in $5B00-$5D70 range (CPU $DB00-$DD70)
print()
print("=== ALL labels PRG $5B00-$5D70 (CPU $DB00-$DD70) ===")
for lbl in prg_labels:
    if 0x5B00 <= lbl['Address'] <= 0x5D70:
        name = lbl['Label']
        is_auto = name.startswith('l') and name[1:].isdigit()
        cpu_addr = lbl['Address'] - 0x4000 + 0xC000
        marker = "" if is_auto else " <<<NAMED>>>"
        print(f"  PRG ${lbl['Address']:04X} (CPU ${cpu_addr:04X}): {name}{marker}")

# Show all NAMED labels in the fixed bank area ($4000-$7FFF PRG)
print()
print("=== ALL NAMED labels in fixed bank (PRG $4000-$7FFF = CPU $C000-$FFFF) ===")
for lbl in prg_labels:
    if 0x4000 <= lbl['Address'] <= 0x7FFF:
        name = lbl['Label']
        is_auto = name.startswith('l') and name[1:].isdigit()
        if not is_auto:
            cpu_addr = lbl['Address'] - 0x4000 + 0xC000
            print(f"  PRG ${lbl['Address']:04X} (CPU ${cpu_addr:04X}): {name}")

# Show key recompiler symbols with CPU addresses
print()
print("=== Key recompiler symbols (NesPrgRom with CPU addresses) ===")
key_names = ['_run_6502', '_recompile_opcode', '_fff0_dispatch', '_flash_sector_alloc',
             '_nes_rom_data_copy', '_opt2_notify_block_compiled', '_ir_resolve',
             '_sa_record_subroutine', '_ir_resolve_direct_branches']
for lbl in prg_labels:
    if any(lbl['Label'].startswith(k) for k in key_names):
        if lbl['Address'] >= 0x4000:
            cpu_addr = lbl['Address'] - 0x4000 + 0xC000
        else:
            cpu_addr = lbl['Address'] + 0x8000  # switchable bank
        print(f"  PRG ${lbl['Address']:04X} (CPU ${cpu_addr:04X}): {lbl['Label']}")

# Also show WRAM labels for dispatch/recompile
print()
print("=== Key WRAM labels (dispatch, recompile, flash) ===")
for lbl in labels:
    if lbl['MemoryType'] == 'NesWorkRam':
        name = lbl['Label']
        if any(kw in name.lower() for kw in ['dispatch', 'recompile', 'flash', 'compile', 'run_']):
            # WRAM $0000 maps to CPU $6000
            cpu_addr = lbl['Address'] + 0x6000
            print(f"  WRAM ${lbl['Address']:04X} (CPU ${cpu_addr:04X}): {name}")
