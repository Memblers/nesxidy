"""
Track ALL flash writes to the PC flag address for $D364.
Flag address: $9364 (bank 30, offset $1364)
Also track writes to jump table: $A6C8-$A6C9 (bank 25)
Also track the bank switch STA $C000 that precedes each write.
"""

import sys

TRACE_FILE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_excitebike.txt"

# Target addresses for PC $D364
FLAG_ADDR = 0x9364      # PC flag for $D364 (bank 30, offset $1364)
JUMP_LO_ADDR = 0xA6C8   # Jump table lo for $D364 (bank 25)
JUMP_HI_ADDR = 0xA6C9   # Jump table hi for $D364 (bank 25)

# Also check $D367
FLAG_D367 = 0x9367
JUMP_D367_LO = 0xA6CE   # ($D367 << 1) & $3FFF = $26CE
JUMP_D367_HI = 0xA6CF

WATCH_ADDRS = {
    FLAG_ADDR: "FLAG $D364",
    JUMP_LO_ADDR: "JMPTBL_LO $D364",
    JUMP_HI_ADDR: "JMPTBL_HI $D364",
    FLAG_D367: "FLAG $D367",
    JUMP_D367_LO: "JMPTBL_LO $D367",
    JUMP_D367_HI: "JMPTBL_HI $D367",
}

def analyze():
    current_frame = 0
    last_bank_switch_value = None  # A register value at last STA $C000
    
    # Also track the bank switch context
    last_c000_a = 0
    write_count = 0
    all_writes_count = 0
    
    # Track ALL flash writes (at $6041) and their bank context
    # The bank is set by `STA $C000` at some point before the data write
    
    # We'll track the A register value whenever we see STA $C000 near $6035
    # (within the flash_byte_program function)
    
    # Actually, let me track the bank by looking at the A register at the 
    # STA $C000 that's the 4th one in the sequence (right before the data write)
    # In flash_byte_program:
    #   STA $C000 (bank 1)
    #   STA $9555 ($AA)
    #   STA $C000 (bank 0)
    #   STA $AAAA ($55)
    #   STA $C000 (bank 1)
    #   STA $9555 ($A0)
    #   STA $C000 (TARGET BANK) <-- this one!
    #   STA ($02),Y (DATA WRITE)

    # Strategy: for each STA ($02),Y at $6041, look at the most recent
    # STA $C000 to determine the bank
    
    found_writes = []
    
    print(f"Scanning trace for writes to watched addresses...")
    print(f"Watched: {WATCH_ADDRS}")
    
    with open(TRACE_FILE, "r") as f:
        prev_c000_a = 0  # A register at last STA $C000
        
        for i, line in enumerate(f):
            # Track frame
            fr_idx = line.find("Fr:")
            if fr_idx >= 0:
                try:
                    fr_str = ""
                    for c in line[fr_idx+3:fr_idx+12]:
                        if c.isdigit(): fr_str += c
                        else: break
                    if fr_str: current_frame = int(fr_str)
                except: pass
            
            if not line.startswith("60"):
                continue
            
            parts = line.split()
            if not parts: continue
            try: addr = int(parts[0], 16)
            except: continue
            
            # Track STA $C000 (bankswitch) - appears as "STA $C000"
            if "STA $C000" in line:
                a_idx = line.find("A:")
                if a_idx >= 0:
                    try:
                        prev_c000_a = int(line[a_idx+2:a_idx+4], 16)
                    except: pass
            
            # Track STA ($02),Y at $6041 (flash data write)
            if addr == 0x6041:
                bracket_idx = line.find("[$")
                if bracket_idx < 0: continue
                try: target = int(line[bracket_idx+2:bracket_idx+6], 16)
                except: continue
                a_idx = line.find("A:")
                try: data = int(line[a_idx+2:a_idx+4], 16)
                except: continue
                
                all_writes_count += 1
                
                if target in WATCH_ADDRS:
                    found_writes.append({
                        "frame": current_frame,
                        "target": target,
                        "data": data,
                        "bank": prev_c000_a,
                        "label": WATCH_ADDRS[target],
                        "line": i+1,
                    })
            
            if i % 1000000 == 0:
                print(f"  Line {i:,}, frame {current_frame}, found {len(found_writes)} watched writes")
    
    print(f"\nTotal flash writes: {all_writes_count}")
    print(f"\nWatched address writes ({len(found_writes)}):")
    print(f"{'Frame':>7} {'Target':>7} {'Data':>6} {'Bank':>6} {'Label'}")
    for w in found_writes:
        print(f"  {w['frame']:>5}  ${w['target']:04X}  ${w['data']:02X}    ${w['bank']:02X}    {w['label']}  (line {w['line']})")
    
    # Summary
    print(f"\nSummary by address:")
    from collections import Counter
    addr_counts = Counter(w['target'] for w in found_writes)
    for addr, count in sorted(addr_counts.items()):
        label = WATCH_ADDRS.get(addr, "?")
        print(f"  ${addr:04X} ({label}): {count} writes")
    
    if not found_writes:
        print("\n*** NO WRITES FOUND to watched addresses! ***")
        print("This means the PC flag for $D364 is NEVER programmed via flash_byte_program.")
        print("Possible cause: AND-corruption guard blocking all writes (flag already != $FF)")


if __name__ == "__main__":
    analyze()
