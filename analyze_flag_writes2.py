"""
Precise bank tracking for flash writes.
For each STA ($02),Y at $6041, find the immediately preceding STA $C000
within the same flash_byte_program call to determine the target bank.

The flash_byte_program code has 5 STA $C000:
  1. bank 1   (SST39SF040 cmd)
  2. bank 0   (SST39SF040 cmd)
  3. bank 1   (SST39SF040 cmd)
  4. TARGET   (actual destination bank)
  5. restore  (mapper_prg_bank)

We need #4. It's always the 4th STA $C000 in the sequence, or equivalently
the LAST STA $C000 before STA ($02),Y.

Approach: for each STA ($02),Y at $6041, use the preceding STA $C000 A value.
We track a small window of recent STA $C000 instructions.
"""

import sys

TRACE_FILE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_excitebike.txt"

# Addresses for $D364
FLAG_D364 = 0x9364
JUMP_D364_LO = 0xA6C8
JUMP_D364_HI = 0xA6C9

# Addresses for $D367
FLAG_D367 = 0x9367
JUMP_D367_LO = 0xA6CE
JUMP_D367_HI = 0xA6CF

# Addresses for $D3C4
FLAG_D3C4 = 0x93C4
JUMP_D3C4_LO = 0xA788
JUMP_D3C4_HI = 0xA789

WATCH = {
    FLAG_D364: "F:D364", JUMP_D364_LO: "J:D364lo", JUMP_D364_HI: "J:D364hi",
    FLAG_D367: "F:D367", JUMP_D367_LO: "J:D367lo", JUMP_D367_HI: "J:D367hi",
    FLAG_D3C4: "F:D3C4", JUMP_D3C4_LO: "J:D3C4lo", JUMP_D3C4_HI: "J:D3C4hi",
}

# Also categorize ALL flash writes by bank
BANK_PC_FLAGS = 27
BANK_PC_START = 19
BANK_CODE_START = 4

def analyze():
    current_frame = 0
    found = []
    all_writes = 0
    
    # Track 4 most recent STA $C000 instructions (to identify the 4th)
    recent_c000 = []  # list of (addr, A_value)
    c000_count = 0
    last_c000_a = 0
    
    # Strategy: Track STA $C000 within the $600x range (flash_byte_program)
    # The function at $6000 has STA $C000 at fixed offsets.
    # Between flash_byte_program calls, there may be other STA $C000 from
    # bankswitch_prg etc. But WITHIN flash_byte_program, the 4th STA $C000 
    # before the STA ($02),Y is the target bank.
    #
    # Simpler: just track the A register at the STA $C000 instruction 
    # immediately preceding ($604x range STA ($02),Y).
    # In flash_byte_program WRAM code, the instruction order is:
    #   STA $C000 (target bank) → very next instruction is STA ($02),Y
    
    # So: track the last STA $C000 line where addr is in $600x-$604x range
    
    print(f"Scanning for precisely banked flash writes...")
    
    with open(TRACE_FILE, "r") as f:
        prev_c000_a_in_fbp = 0  # last STA $C000 A value within flash_byte_program
        
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
            
            if not line.startswith("60"):
                continue
            parts = line.split()
            if not parts: continue
            try: addr = int(parts[0], 16)
            except: continue
            
            # Track STA $C000 within the flash_byte_program function ($6000-$605F)
            if 0x6000 <= addr <= 0x605F and "STA $C000" in line:
                a_idx = line.find("A:")
                if a_idx >= 0:
                    try:
                        prev_c000_a_in_fbp = int(line[a_idx+2:a_idx+4], 16)
                    except: pass
            
            # Track STA ($02),Y at $6041
            if addr == 0x6041:
                bracket_idx = line.find("[$")
                if bracket_idx < 0: continue
                try: target = int(line[bracket_idx+2:bracket_idx+6], 16)
                except: continue
                a_idx = line.find("A:")
                try: data = int(line[a_idx+2:a_idx+4], 16)
                except: continue
                
                # The bank used for this write is prev_c000_a_in_fbp
                # (strip CHR bank bits - only low 5 bits are PRG bank)
                target_bank = prev_c000_a_in_fbp & 0x1F
                
                all_writes += 1
                
                if target in WATCH:
                    found.append({
                        "frame": current_frame,
                        "target": target,
                        "data": data,
                        "bank": target_bank,
                        "raw_c000": prev_c000_a_in_fbp,
                        "label": WATCH[target],
                        "line": i+1,
                    })
            
            if i % 1000000 == 0:
                print(f"  Line {i:,}, frame {current_frame}, {len(found)} matches, {all_writes} total writes")
    
    print(f"\nTotal flash writes: {all_writes}")
    
    print(f"\n=== Watched Address Writes ({len(found)}) ===")
    print(f"{'Frame':>6} {'Target':>7} {'Data':>6} {'Bank':>5} {'C000':>5} {'Label'}")
    for w in found:
        is_correct_bank = ""
        if "F:" in w['label']:
            # Flag writes should be in bank 27-30
            if 27 <= w['bank'] <= 30: is_correct_bank = " ✓ FLAG BANK"
            else: is_correct_bank = f" ✗ code bank {w['bank']}"
        elif "J:" in w['label']:
            # Jump table writes should be in bank 19-26
            if 19 <= w['bank'] <= 26: is_correct_bank = " ✓ JMPTBL BANK"
            else: is_correct_bank = f" ✗ NOT jmptbl bank {w['bank']}"
        print(f"  {w['frame']:>5} ${w['target']:04X}  ${w['data']:02X}    {w['bank']:>3}   ${w['raw_c000']:02X}   {w['label']}{is_correct_bank}")
    
    # Check for writes to flag bank addresses that are NOT in our watch list
    # but might indicate flag writes for other PCs
    print(f"\nDone.")


if __name__ == "__main__":
    analyze()
