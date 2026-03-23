"""Search for key function entry points in the trace to verify they execute."""
import re

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"

# Key function entry points (fixed bank trampolines)
targets = {
    'CB94': '_flash_format',
    'DFA8': '_flash_cache_init_sectors', 
    'E7B3': '_sa_run',
    'C51B': '_main',
    'E21F': '_recompile_opcode',
}

# Also look for the JSR instructions that call them
# JSR opcodes: 20 XX XX
# JSR $CB94 = 20 94 CB
# JSR $DFA8 = 20 A8 DF
# JSR $E7B3 = 20 B3 E7

print("=== SEARCHING FOR KEY FUNCTION ENTRIES IN FIRST 200K LINES ===")

hits = {k: [] for k in targets}
total = 0

with open(TRACE, 'r', buffering=4*1024*1024) as f:
    for i, line in enumerate(f):
        if i > 200000:
            break
        
        addr_m = re.match(r'([0-9A-Fa-f]{4})\s', line)
        if not addr_m:
            continue
        addr = addr_m.group(1).upper()
        
        if addr in targets:
            hits[addr].append((i, line.rstrip()[:150]))

for addr, name in targets.items():
    entries = hits[addr]
    print(f"\n{name} (${addr}): {len(entries)} entries")
    for ln, text in entries[:5]:
        print(f"  Line {ln}: {text}")

# Also search for JSR to these targets by checking "BC:" bytes
print("\n=== SEARCHING FOR JSR CALLS TO THESE FUNCTIONS ===")
jsr_targets = {
    '20 94 CB': 'JSR _flash_format',
    '20 A8 DF': 'JSR _flash_cache_init_sectors',
    '20 B3 E7': 'JSR _sa_run',
}

jsr_hits = {k: [] for k in jsr_targets}

with open(TRACE, 'r', buffering=4*1024*1024) as f:
    for i, line in enumerate(f):
        if i > 200000:
            break
        
        # Check BC: field for opcode bytes
        bc_m = re.search(r'BC:([0-9A-Fa-f ]+)', line)
        if not bc_m:
            continue
        bc = bc_m.group(1).strip().upper()
        
        for pattern, name in jsr_targets.items():
            if pattern.replace(' ', '') in bc.replace(' ', ''):
                jsr_hits[pattern].append((i, line.rstrip()[:150]))

for pattern, name in jsr_targets.items():
    entries = jsr_hits[pattern]
    print(f"\n{name} (BC:{pattern}): {len(entries)} calls")
    for ln, text in entries[:5]:
        print(f"  Line {ln}: {text}")
