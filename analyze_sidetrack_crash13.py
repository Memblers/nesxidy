"""Compare ROM on disk vs ROM in Mesen to verify which version is loaded."""
import struct

# Read the first 256 bytes at offset $DE39-$C000 = $1E39 in the fixed bank
# The fixed bank is the last 16KB of PRG ROM.
# For a 512KB (32 banks) mapper30 ROM, the fixed bank is at the end.

# Actually, let's just check if the file on disk has our changes.
# Our function ir_emit_direct_branch_placeholder should be at PRG offset
# corresponding to label $5E39.
# 
# In the label file: P:5E39:_ir_emit_direct_branch_placeholder
# P: prefix means PRG ROM offset $5E39.
# NES header is 16 bytes, so file offset = 16 + $5E39 = 16 + 24121 = 24137

with open("exidy.nes", "rb") as f:
    # Read at the function location
    f.seek(16 + 0x5E39)
    data = f.read(64)
    print("=== ROM bytes at _ir_emit_direct_branch_placeholder ($5E39) ===")
    print("Hex:", data[:32].hex(' '))
    
    # Decode as 6502: first bytes should be our capacity check
    # LDA sp / BNE... or LDA _code_index / LDX #$00 / CLC / ADC...
    # Actually our function starts with the capacity check:
    # if ((code_index + 5 + EPILOGUE_SIZE + 6) >= CODE_SIZE ...
    # which is: LDA _code_index, then add and compare
    
    # Also check what's at the OLD trampoline location.
    # In the old build, the trampoline was around $DCF8 area.
    # $DCF8 in CPU space = $5CF8 in PRG.
    f.seek(16 + 0x5CF8)
    data2 = f.read(64)
    print("\n=== ROM bytes at $5CF8 (was old trampoline, now _setup_flash_pc_tables) ===")
    print("Hex:", data2[:32].hex(' '))
    
    # Let's also check what USED to be the trampoline.
    # The previous build had ir_emit_direct_branch_placeholder at a different offset.
    # Let's search for the bankswitch sequence: A9 11 20 C4 C3 (LDA #$11; JSR $C3C4)
    # followed by 20 3E 84 (JSR $843E)
    f.seek(16)
    prg = f.read()
    
    # Search for the pattern in fixed bank (last 16KB)
    fixed_bank_start = len(prg) - 16384
    fixed = prg[fixed_bank_start:]
    
    pattern_843e = bytes([0x20, 0x3E, 0x84])  # JSR $843E
    print(f"\n=== Searching for JSR $843E in fixed bank ===")
    pos = 0
    while True:
        idx = fixed.find(pattern_843e, pos)
        if idx == -1:
            break
        cpu_addr = 0xC000 + idx
        print(f"  Found at fixed bank offset ${idx:04X} (CPU ${cpu_addr:04X})")
        # Show context
        ctx_start = max(0, idx - 8)
        ctx = fixed[ctx_start:idx+16]
        print(f"  Context: {ctx.hex(' ')}")
        pos = idx + 1
    
    # Search for JSR $DCF8 in the entire PRG
    pattern_dcf8 = bytes([0x20, 0xF8, 0xDC])  # JSR $DCF8
    print(f"\n=== Searching for JSR $DCF8 in PRG ROM ===")
    pos = 0
    count = 0
    while True:
        idx = prg.find(pattern_dcf8, pos)
        if idx == -1:
            break
        # Determine which bank this is in
        bank = idx // 16384
        offset_in_bank = idx % 16384
        cpu_addr = 0x8000 + offset_in_bank
        print(f"  Found at PRG ${idx:04X} (bank {bank}, CPU ${cpu_addr:04X})")
        count += 1
        pos = idx + 1
    print(f"  Total: {count}")
    
    # Also search for JSR $DE39 
    pattern_de39 = bytes([0x20, 0x39, 0xDE])
    print(f"\n=== Searching for JSR $DE39 in PRG ROM ===")
    pos = 0
    count = 0
    while True:
        idx = prg.find(pattern_de39, pos)
        if idx == -1:
            break
        bank = idx // 16384
        offset_in_bank = idx % 16384
        cpu_addr = 0x8000 + offset_in_bank
        print(f"  Found at PRG ${idx:04X} (bank {bank}, CPU ${cpu_addr:04X})")
        count += 1
        pos = idx + 1
    print(f"  Total: {count}")
