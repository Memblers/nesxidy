"""
analyze_dk_loop13.py - Search large trace for sa_run return and compile start.
Sample at intervals instead of reading every line.
"""
import re

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"

# We want to find when:
# 1. flash_sector_erase appears (compile pass starting)
# 2. sa_run returns ($C60F)
# 3. dispatch_on_pc ($623A) - game starts
# Search through entire trace but skip ahead in chunks

# Strategy: read in chunks, looking for key addresses
CHUNK_SIZE = 200000
START = 660000  # Continue from where last script left off

print(f"Scanning trace from line {START} in chunks of {CHUNK_SIZE}...")

results = {
    'first_sector_erase': None,
    'first_dispatch': None,
    'sa_run_return': None,
    'sa_compile_completed': None,
    'flash_byte_programs': [],  # (line, frame) tuples, sampled
    'frames_seen': set(),
}

total_fbp = 0
total_fse = 0
total_nmi = 0
total_dispatch = 0

with open(TRACE, 'r', errors='replace') as f:
    for i, line in enumerate(f, 1):
        if i < START:
            continue
        if i > 30000000:  # Stop at 30M lines
            break
        
        text = line.rstrip()
        
        # Quick filter - only process lines starting with hex address
        if len(text) < 4:
            continue
        addr4 = text[:4].upper()
        
        # Track frame (every 10000 lines to save time)
        if i % 50000 == 0:
            fm = re.search(r'Fr:(\d+)', text)
            if fm:
                frame = int(fm.group(1))
                results['frames_seen'].add(frame)
                print(f"  Line {i:,}: Frame {frame}, FBP={total_fbp}, FSE={total_fse}")
        
        # Check for key addresses
        if addr4 == "6060":  # flash_sector_erase
            total_fse += 1
            if results['first_sector_erase'] is None:
                fm = re.search(r'Fr:(\d+)', text)
                frame = int(fm.group(1)) if fm else -1
                results['first_sector_erase'] = (i, frame)
                print(f"  *** FIRST flash_sector_erase at line {i}, frame {frame}")
        
        elif addr4 == "6000" and "LDX" in text:  # flash_byte_program
            total_fbp += 1
        
        elif addr4 == "623A":  # dispatch_on_pc
            total_dispatch += 1
            if results['first_dispatch'] is None:
                fm = re.search(r'Fr:(\d+)', text)
                frame = int(fm.group(1)) if fm else -1
                results['first_dispatch'] = (i, frame)
                print(f"  *** FIRST dispatch_on_pc at line {i}, frame {frame}")
        
        elif addr4 == "C60F":  # sa_run return
            if results['sa_run_return'] is None:
                fm = re.search(r'Fr:(\d+)', text)
                frame = int(fm.group(1)) if fm else -1
                results['sa_run_return'] = (i, frame)
                print(f"  *** sa_run RETURNS at line {i}, frame {frame}")
        
        elif "60E1" in text and "STA" in text:  # sa_compile_completed
            if results['sa_compile_completed'] is None:
                fm = re.search(r'Fr:(\d+)', text)
                frame = int(fm.group(1)) if fm else -1
                results['sa_compile_completed'] = (i, frame)
                print(f"  *** sa_compile_completed WRITTEN at line {i}, frame {frame}")
        
        elif addr4 == "C192":  # NMI handler
            total_nmi += 1

print(f"\n=== FINAL SUMMARY ===")
print(f"Scanned lines {START:,} to {min(i, 30000000):,}")
print(f"Total flash_byte_program: {total_fbp:,}")
print(f"Total flash_sector_erase: {total_fse:,}")
print(f"Total NMI handler entries: {total_nmi:,}")
print(f"Total dispatch_on_pc: {total_dispatch:,}")
print(f"First sector_erase: {results['first_sector_erase']}")
print(f"First dispatch: {results['first_dispatch']}")
print(f"sa_run return: {results['sa_run_return']}")
print(f"sa_compile_completed: {results['sa_compile_completed']}")
