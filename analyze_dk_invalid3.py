"""
Search backward from crash for flash_byte_program calls that wrote to address $84B0 in bank 8.
Also look for the compile start (entry_pc setup) and IR lowering results.
Read a larger trace window to find the actual code writes.
"""
import os

trace_file = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"
size = os.path.getsize(trace_file)

# Read 10MB tail to find the compilation
tail_bytes = 10_000_000

with open(trace_file, 'r', encoding='utf-8', errors='replace') as f:
    f.seek(max(0, size - tail_bytes))
    if f.tell() > 0:
        f.readline()  # skip partial
    tail_lines = f.readlines()

print(f"Read {len(tail_lines)} tail lines")

# Find the crash line
crash_idx = None
for i in range(len(tail_lines) - 1, max(0, len(tail_lines) - 100), -1):
    if 'BF CB 70' in tail_lines[i].upper() or 'LAX' in tail_lines[i]:
        crash_idx = i
        break

print(f"Crash at tail line {crash_idx}")

# Search for flash_byte_program calls ($6000)
# The function at $6000 takes (addr, bank, data) as arguments
# Before each call, the arguments are set up
print(f"\n=== SEARCHING FOR FLASH BYTE PROGRAM CALLS NEAR $84B0 ===")

# flash_byte_program is at $6000, called via JSR $6000
# Look for calls to $6000 in the last 50000 lines before crash
search_start = max(0, crash_idx - 50000)
fbp_calls = []
for i in range(search_start, crash_idx):
    line = tail_lines[i]
    if 'JSR' in line and '$6000' in line:
        fbp_calls.append(i)

print(f"Found {len(fbp_calls)} flash_byte_program calls")
if fbp_calls:
    print(f"First call at T{fbp_calls[0]}, last at T{fbp_calls[-1]}")
    
# Look at the code around each flash write to find the one that writes to $84B0
# The parameters are typically loaded before the JSR:
# - addr is passed as the first argument
# - We need to find LDA/STA patterns that set up $84B0
# Search for $84B0 or just show the writes near the end of the compile
    
# Show the last 30 flash writes before crash (these are likely the code bytes)
print(f"\n=== LAST 30 FLASH WRITES BEFORE CRASH ===")
for idx in fbp_calls[-30:]:
    # Show 5 lines before the JSR to see setup
    for j in range(max(0, idx-8), idx+2):
        line = tail_lines[j].rstrip()[:180]
        marker = ">>>" if j == idx else "   "
        print(f"{marker} T{j}: {line}")
    print()

# Also check if there's a compile entry (entry_pc setup) nearby
print(f"\n=== ENTRY PC SETUP NEAR COMPILE ===")
for i in range(search_start, crash_idx):
    line = tail_lines[i]
    if 'cache_entry_pc' in line.lower() and 'STA' in line:
        if i > crash_idx - 20000:  # only show recent ones
            print(f"  T{i}: {line.rstrip()[:180]}")
