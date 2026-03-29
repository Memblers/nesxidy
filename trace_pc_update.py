"""
Find the flash_cache_pc_update call after the first compile of $D364.

Strategy: After the sentinel write ($AA at $92FF for block 3), the next
flash_byte_program calls should be the PC table update. Look for:
1. bankswitch_prg to the flag bank (bank 30)
2. The LDA (r2),Y that reads the flag
3. CMP #$FF / BNE (AND-corruption guard)
4. If not taken: the flash_byte_program calls for jump table + flag

The sentinel for block 3 is at $92FF (block at $92F8, offset +7).
After the sentinel, there should be code running in the fixed bank
that calls flash_cache_pc_update.

Let me search for the bankswitch to bank 30 (flag bank for $D364)
in the trace after the block 3 writes.
"""

import sys

TRACE_FILE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_excitebike.txt"

def analyze():
    # Block 3 ($D364) sentinel should be around the writes at $92FF
    # Let me find the sentinel write and then capture the next 500 lines
    
    current_frame = 0
    found_sentinel = False
    sentinel_line = 0
    capture_count = 0
    
    # First, find the sentinel for block 3 (first $D364 compile)
    # The sentinel is STA ($02),Y [$92FF] with data $AA (block at $92F8)
    
    print("Phase 1: Finding sentinel for block 3 ($D364 first compile)...")
    
    with open(TRACE_FILE, "r") as f:
        for i, line in enumerate(f):
            fr_idx = line.find("Fr:")
            if fr_idx >= 0:
                try:
                    fr_str = ""
                    for c in line[fr_idx+3:fr_idx+12]:
                        if c.isdigit(): fr_str += c
                        else: break
                    if fr_str: current_frame = int(fr_str)
                except: pass
            
            if not found_sentinel:
                # Look for the sentinel write: STA ($02),Y [$92FF] with A=$AA
                if "6041" in line[:6] and "$92FF" in line:
                    a_idx = line.find("A:")
                    if a_idx >= 0:
                        try:
                            a_val = int(line[a_idx+2:a_idx+4], 16)
                            if a_val == 0xAA:
                                found_sentinel = True
                                sentinel_line = i + 1
                                print(f"  Found sentinel at line {sentinel_line}, frame {current_frame}")
                                # Now capture next 1000 lines to find PC update
                                capture_count = 1000
                        except: pass
            
            if found_sentinel and capture_count > 0:
                capture_count -= 1
                
                # Track key events
                addr = 0
                if line[:2].isalnum():
                    parts = line.split()
                    if parts:
                        try: addr = int(parts[0], 16)
                        except: pass
                
                # Look for bankswitch to bank 30 ($1E) — flag bank for $D364
                if "STA $C000" in line:
                    a_idx = line.find("A:")
                    if a_idx >= 0:
                        try:
                            a_val = int(line[a_idx+2:a_idx+4], 16)
                            bank = a_val & 0x1F
                            if bank == 30 or bank == 25:
                                print(f"  L{i+1}: BANKSWITCH to {bank} (A=${a_val:02X}) — {line.rstrip()[:120]}")
                        except: pass
                
                # Look for CMP #$FF (AND-corruption guard)
                if "CMP #$FF" in line:
                    print(f"  L{i+1}: AND-CORRUPTION GUARD — {line.rstrip()[:120]}")
                
                # Look for BNE after CMP #$FF
                if "BNE" in line and i > 0:
                    # Check if previous line was CMP #$FF  
                    pass  # too complex to track
                
                # Look for flash_byte_program calls from fixed bank
                if addr >= 0xC000 and "JSR _flash_byte_program" in line:
                    print(f"  L{i+1}: JSR flash_byte_program — {line.rstrip()[:120]}")
                
                # Look for STA ($02),Y at $6041 (actual flash writes)
                if addr == 0x6041:
                    bracket_idx = line.find("[$")
                    if bracket_idx >= 0:
                        target = line[bracket_idx+1:bracket_idx+6]
                        a_idx = line.find("A:")
                        a_val = line[a_idx+2:a_idx+4] if a_idx >= 0 else "??"
                        print(f"  L{i+1}: FLASH WRITE → {target} data=${a_val} — {line.rstrip()[:120]}")
                
                # Look for LDA (r2),Y that reads the flag (within flash_cache_pc_update)
                if addr >= 0xC000 and "LDA (r2)" in line:
                    print(f"  L{i+1}: FLAG READ — {line.rstrip()[:120]}")
                
                if capture_count == 0:
                    break
            
            if i > 400000 and not found_sentinel:
                print("  (sentinel not found in first 400K lines, continuing...)")
                break
    
    if not found_sentinel:
        print("Sentinel not found!")


if __name__ == "__main__":
    analyze()
