"""
Trace the actual flash byte programming loop to see what bytes were written.
Focus on the $DAC5 loop and extract the data bytes.
"""
import os

trace_file = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"
size = os.path.getsize(trace_file)

# Read 5MB tail
tail_bytes = 5_000_000
with open(trace_file, 'r', encoding='utf-8', errors='replace') as f:
    f.seek(max(0, size - tail_bytes))
    if f.tell() > 0:
        f.readline()
    tail_lines = f.readlines()

crash_idx = len(tail_lines) - 1
print(f"Read {len(tail_lines)} tail lines, crash at {crash_idx}")

# Find all flash_byte_program calls (the function at $6000)
# Let's find where execution enters $6000 (LDX r2 at $6000)
fbp_entries = []
for i in range(max(0, crash_idx - 43000), crash_idx):
    line = tail_lines[i].lstrip()
    if line.startswith('6000'):
        fbp_entries.append(i)

print(f"Found {len(fbp_entries)} entries to flash_byte_program ($6000)")

# For each entry, show the context (arguments are in registers/ZP before the call)
# flash_byte_program prototype: void flash_byte_program(uint16_t addr, uint8_t bank, uint8_t data);
# In VBCC 6502 calling convention, args go in zero page temp regs
# Let's show the lines around each entry to see what arguments were passed
print(f"\n=== FLASH BYTE PROGRAM ENTRIES WITH CONTEXT ===")
for entry_num, idx in enumerate(fbp_entries):
    # Show the JSR that called $6000 and a few lines before
    # The JSR should be right before
    context_start = max(0, idx - 12)
    print(f"\n--- Entry #{entry_num} at T{idx} ---")
    for j in range(context_start, min(idx + 5, len(tail_lines))):
        line = tail_lines[j].rstrip()[:200]
        marker = ">>>" if j == idx else "   "
        print(f"{marker} T{j}: {line}")
    
    if entry_num > 75:  # limit output
        print(f"... ({len(fbp_entries) - entry_num - 1} more entries)")
        break
