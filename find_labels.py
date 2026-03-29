import json

with open(r'C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.json', 'r', encoding='utf-8-sig') as f:
    data = json.load(f)

labels = data['WorkspaceByCpu']['Nes']['Labels']
print(f"Total labels: {len(labels)}")

# Collect all memory types seen
mem_types = set()
for lbl in labels:
    mem_types.add(lbl['MemoryType'])
print(f"Memory types: {mem_types}")

print()
print("=== Labels in $DA00-$DE00 range (all memory types) ===")
for lbl in labels:
    addr = lbl['Address']
    mem = lbl['MemoryType']
    if 0xDA00 <= addr <= 0xDE00:
        comment = lbl.get('Comment', '')
        if comment:
            comment = comment[:80].replace('\r\n', ' | ')
        print(f"  ${addr:04X} ({mem}): {lbl['Label']}  [len={lbl['Length']}]  {comment}")

print()
print("=== Labels in $D800-$E000 range (all memory types) ===")
for lbl in labels:
    addr = lbl['Address']
    mem = lbl['MemoryType']
    if 0xD800 <= addr <= 0xE000:
        print(f"  ${addr:04X} ({mem}): {lbl['Label']}  [len={lbl['Length']}]")

print()
print("=== Labels containing recompile, run_6502, dispatch, opcode, compile ===")
for lbl in labels:
    name = lbl['Label'].lower()
    if any(kw in name for kw in ['recompile', 'run_6502', 'dispatch', 'opcode', 'compile', 'dynamo', 'jit']):
        print(f"  ${lbl['Address']:04X} ({lbl['MemoryType']}): {lbl['Label']}  [len={lbl['Length']}]")

print()
print("=== Finding label closest to $DB4D ===")
# Find the label with the highest address <= $DB4D
best = None
best_addr = 0
for lbl in labels:
    addr = lbl['Address']
    if addr <= 0xDB4D and addr > best_addr:
        # Check if it's in NesMemory or NesPrgRom ranges that would map to $DB4D
        if lbl['MemoryType'] in ('NesMemory', 'NesPrgRom'):
            best = lbl
            best_addr = addr

if best:
    print(f"  Closest label at or before $DB4D: ${best['Address']:04X} ({best['MemoryType']}): {best['Label']}")
    print(f"  Offset from label: ${0xDB4D - best['Address']:04X} ({0xDB4D - best['Address']} bytes)")
else:
    print("  No label found")

# Also check NesPrgRom type specifically - PRG ROM addresses are offsets into the ROM
# For a fixed bank at $C000-$FFFF, the PRG ROM offset would be different
print()
print("=== All labels in $C000-$FFFF range (NesMemory) ===")
c000_labels = []
for lbl in labels:
    addr = lbl['Address']
    mem = lbl['MemoryType']
    if 0xC000 <= addr <= 0xFFFF and mem == 'NesMemory':
        c000_labels.append(lbl)

c000_labels.sort(key=lambda x: x['Address'])
for lbl in c000_labels:
    print(f"  ${lbl['Address']:04X}: {lbl['Label']}  [len={lbl['Length']}]")

print()
print("=== All labels in $C000-$FFFF range (NesPrgRom) ===")
prg_labels = []
for lbl in labels:
    addr = lbl['Address']
    mem = lbl['MemoryType']
    if 0xC000 <= addr <= 0xFFFF and mem == 'NesPrgRom':
        prg_labels.append(lbl)

prg_labels.sort(key=lambda x: x['Address'])
for lbl in prg_labels:
    print(f"  ${lbl['Address']:04X}: {lbl['Label']}  [len={lbl['Length']}]")

# Check all NesPrgRom labels to understand the address scheme
print()
print("=== NesPrgRom address range overview ===")
prg_all = [lbl for lbl in labels if lbl['MemoryType'] == 'NesPrgRom']
if prg_all:
    prg_all.sort(key=lambda x: x['Address'])
    print(f"  Min: ${prg_all[0]['Address']:04X}, Max: ${prg_all[-1]['Address']:04X}, Count: {len(prg_all)}")
    # Show those near $DB4D considering PRG ROM offset
    # For 16KB PRG + fixed bank, the fixed bank starts at PRG offset $4000
    # $DB4D in CPU space with $C000 base = offset $1B4D from bank start
    # If fixed bank is at PRG offset $4000, then PRG ROM address = $4000 + $1B4D = $5B4D
    print()
    print("  Labels near PRG ROM offset $5B4D (if fixed bank at PRG $4000):")
    for lbl in prg_all:
        if 0x5A00 <= lbl['Address'] <= 0x5E00:
            print(f"    ${lbl['Address']:04X}: {lbl['Label']}  [len={lbl['Length']}]")
    
    print()
    print("  Labels near PRG ROM offset $1B4D (if single 16KB bank):")
    for lbl in prg_all:
        if 0x1A00 <= lbl['Address'] <= 0x1E00:
            print(f"    ${lbl['Address']:04X}: {lbl['Label']}  [len={lbl['Length']}]")
