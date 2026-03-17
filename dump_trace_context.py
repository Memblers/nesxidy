"""
Dump raw trace lines around the first flash write to $9364
to verify bank tracking and understand the instruction sequence.
"""

TRACE_FILE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_excitebike.txt"

def analyze():
    # Find the first write to $9364 and dump context
    target_line = 278584  # from earlier analysis
    
    # Read lines around this target
    window = 40
    
    print(f"Reading lines {target_line - window} to {target_line + 10}...")
    with open(TRACE_FILE, "r") as f:
        for i, line in enumerate(f):
            if i+1 >= target_line - window and i+1 <= target_line + 10:
                print(f"L{i+1:>8}: {line.rstrip()}")
            if i+1 > target_line + 10:
                break
    
    # Also find the first time $D364 triggers a compile
    # by looking for the flash_byte_program call at $6000 entry
    # around frame 6365 (first compile of $D364)
    print(f"\n\n=== Looking for $D364 compile context (frame 6365) ===")
    print(f"Searching for flash writes near $92F8 (block 3 flash address)...")
    
    with open(TRACE_FILE, "r") as f:
        frame = 0
        found_92f8 = False
        capture_after = 0
        for i, line in enumerate(f):
            fr_idx = line.find("Fr:")
            if fr_idx >= 0:
                try:
                    fr_str = ""
                    for c in line[fr_idx+3:fr_idx+12]:
                        if c.isdigit(): fr_str += c
                        else: break
                    if fr_str: frame = int(fr_str)
                except: pass
            
            if frame == 6365 and not found_92f8:
                if "$92F8" in line or "92F8" in line.upper():
                    found_92f8 = True
                    capture_start = max(0, i - 5)
                    capture_after = 200  # capture 200 lines after finding it
                    
            if found_92f8 and capture_after > 0:
                print(f"L{i+1:>8}: {line.rstrip()}")
                capture_after -= 1
                if capture_after == 0:
                    break
            
            if i > 500000 and not found_92f8:
                print("Not found in first 500K lines, searching wider...")
    
    if not found_92f8:
        # Search more broadly
        print("Searching for $92F8 in whole file...")
        with open(TRACE_FILE, "r") as f:
            for i, line in enumerate(f):
                if "92F8" in line.upper() and "60" in line[:4]:
                    print(f"L{i+1}: {line.rstrip()}")
                    break


if __name__ == "__main__":
    analyze()
