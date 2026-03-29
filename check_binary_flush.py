#!/usr/bin/env python3
"""Verify that the PLP flush code actually exists in the compiled ROM binary."""

ROM_PATH = "exidy.nes"
INES_HEADER = 16
PRG_BANK_SIZE = 0x4000

# From linker map:
# _run_6502 = $CB26
# _recompile_opcode = $DFF2
# _setup_flash_pc_tables = $D7DF
# _block_flags_saved (l28) = in WRAM (data section)

# Fixed bank = bank 31, mapped at $C000-$FFFF
FIXED_BANK = 31

def rom_offset(addr):
    """Convert NES address in fixed bank ($C000-$FFFF) to ROM file offset."""
    return INES_HEADER + FIXED_BANK * PRG_BANK_SIZE + (addr - 0xC000)

def main():
    data = open(ROM_PATH, 'rb').read()
    
    # Find the compile loop.
    # From assembly, the pattern after setup_flash_pc_tables call is:
    #   l98:  lda <block_flags_saved_addr>  (AD xx xx or A5 xx)
    #         beq <skip>
    #         ... PLP flush code ...
    #   l52:  ...
    
    # Let me search for the pattern around _recompile_opcode ($DFF2)
    # The compile loop calls _setup_flash_pc_tables then enters the loop.
    # The loop body calls _recompile_opcode.
    
    # Search for JSR _recompile_opcode ($20 F2 DF) in the fixed bank
    target_jsr = bytes([0x20, 0xF2, 0xDF])
    
    fixed_bank_start = INES_HEADER + FIXED_BANK * PRG_BANK_SIZE
    fixed_bank_data = data[fixed_bank_start:fixed_bank_start + PRG_BANK_SIZE]
    
    print("Searching for JSR _recompile_opcode ($20 F2 DF) in fixed bank...")
    matches = []
    for i in range(len(fixed_bank_data) - 2):
        if fixed_bank_data[i:i+3] == target_jsr:
            addr = 0xC000 + i
            matches.append((i, addr))
            print(f"  Found at offset {i:04X} = ${addr:04X}")
    
    if not matches:
        print("  NOT FOUND! The compile loop may be structured differently.")
        return
    
    # For each match, dump the surrounding bytes to find the PLP flush pattern
    for offset, addr in matches:
        print(f"\n--- Context around JSR _recompile_opcode at ${addr:04X} ---")
        # Dump 60 bytes before the JSR
        start = max(0, offset - 80)
        print(f"  Bytes before (from ${0xC000+start:04X}):")
        for i in range(start, offset + 3):
            b = fixed_bank_data[i]
            a = 0xC000 + i
            marker = ""
            if i == offset:
                marker = " <== JSR _recompile_opcode"
            elif b == 0x28 and i > start + 2:
                # Check if this is LDA #$28 (A9 28) or STA pattern
                if fixed_bank_data[i-1] == 0xA9:
                    marker = " (part of LDA #$28)"
                else:
                    marker = " ($28 = PLP opcode or #40 decimal)"
            print(f"  ${a:04X}: {b:02X}{marker}")
    
    # Now specifically look for the l28 (block_flags_saved) address.
    # In the assembly, it's accessed as "lda l28" / "sta l28".
    # l28 is a static in the data section -> WRAM.
    # Let me search for all "LDA abs" instructions that load from WRAM ($6xxx-$7xxx)
    # near the JSR _recompile_opcode.
    
    print("\n\n=== Searching for block_flags_saved access pattern ===")
    # Pattern: LDA abs (AD xx xx) where addr is in $6000-$7FFF
    # followed by BEQ (F0 xx)
    for offset, addr in matches:
        search_start = max(0, offset - 100)
        search_end = offset
        print(f"\nSearching before JSR at ${addr:04X}:")
        for i in range(search_start, search_end):
            if fixed_bank_data[i] == 0xAD:  # LDA abs
                lo = fixed_bank_data[i+1] if i+1 < len(fixed_bank_data) else 0
                hi = fixed_bank_data[i+2] if i+2 < len(fixed_bank_data) else 0
                wram_addr = lo | (hi << 8)
                if 0x6000 <= wram_addr <= 0x7FFF:
                    a = 0xC000 + i
                    # Check if followed by BEQ
                    next_op = fixed_bank_data[i+3] if i+3 < len(fixed_bank_data) else 0
                    is_beq = "→ BEQ" if next_op == 0xF0 else ""
                    print(f"  ${a:04X}: LDA ${wram_addr:04X} {is_beq}")
                    
                    if next_op == 0xF0:
                        beq_off = fixed_bank_data[i+4]
                        if beq_off >= 0x80:
                            beq_off -= 0x100
                        beq_target = a + 5 + beq_off
                        print(f"         BEQ ${beq_target:04X}")
                        
                        # This could be the block_flags_saved check!
                        # Dump what's between here and the BEQ target
                        flush_start = i + 5
                        flush_end = i + 5 + beq_off if beq_off > 0 else flush_start
                        if 0 < beq_off < 60:
                            print(f"         PLP flush body ({beq_off} bytes):")
                            j = flush_start
                            while j < flush_end and j < len(fixed_bank_data):
                                b = fixed_bank_data[j]
                                ba = 0xC000 + j
                                if b == 0xA9 and j+1 < flush_end:
                                    operand = fixed_bank_data[j+1]
                                    mnem = f"LDA #${operand:02X}"
                                    if operand == 0x28:
                                        mnem += " (= PLP opcode!)"
                                    print(f"           ${ba:04X}: {b:02X} {operand:02X}  {mnem}")
                                    j += 2
                                elif b == 0x91 and j+1 < flush_end:
                                    zp = fixed_bank_data[j+1]
                                    print(f"           ${ba:04X}: {b:02X} {zp:02X}  STA (${zp:02X}),Y")
                                    j += 2
                                elif b == 0x8D and j+2 < flush_end:
                                    lo2 = fixed_bank_data[j+1]
                                    hi2 = fixed_bank_data[j+2]
                                    w = lo2 | (hi2 << 8)
                                    print(f"           ${ba:04X}: {b:02X} {lo2:02X} {hi2:02X}  STA ${w:04X}")
                                    j += 3
                                else:
                                    print(f"           ${ba:04X}: {b:02X}")
                                    j += 1
    
    # Also check: is LDA #40 ($A9 $28) present near the JSR?
    print("\n\n=== Looking for LDA #$28 (PLP value = #40) near compile loop ===")
    for offset, addr in matches:
        search_start = max(0, offset - 120)
        search_end = min(len(fixed_bank_data), offset + 100)
        for i in range(search_start, search_end):
            if fixed_bank_data[i] == 0xA9 and fixed_bank_data[i+1] == 0x28:
                a = 0xC000 + i
                print(f"  ${a:04X}: A9 28  LDA #$28  (PLP opcode value)")

if __name__ == '__main__':
    main()
