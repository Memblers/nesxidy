#!/usr/bin/env python3
"""Decode the init section runner and constructor table."""

with open('exidy.nes', 'rb') as f:
    base = 16 + 31 * 16384  # bank 31 offset in iNES
    
    # Read init runner code at $C09E
    f.seek(base + 0x009E)
    code = f.read(64)
    
    print("=== Init runner at $C09E ===")
    print("Raw bytes:")
    for i in range(0, 48, 16):
        addr = 0xC09E + i
        hex_str = ' '.join(f'{code[i+j]:02X}' for j in range(min(16, 48-i)))
        print(f'  {addr:04X}: {hex_str}')
    
    print()
    print("Disassembly:")
    print("  C09E: A5 03       LDA $03        ; save ZP $00-$03")
    print("  C0A0: 48          PHA")
    print("  C0A1: A5 02       LDA $02")
    print("  C0A3: 48          PHA")
    print("  C0A4: A5 01       LDA $01")
    print("  C0A6: 48          PHA")
    print("  C0A7: A5 00       LDA $00")
    print("  C0A9: 48          PHA")
    print("  C0AA: A0 00       LDY #$00       ; Y = index into table")
    print("  C0AC: B9 5D EB    LDA $EB5D,Y    ; table[Y] -> ZP $00 (lo)")
    print("  C0AF: 85 00       STA $00")
    print("  C0B1: AA          TAX")
    print("  C0B2: B9 5E EB    LDA $EB5E,Y    ; table[Y+1] -> ZP $01 (hi)")
    print("  C0B5: 85 01       STA $01")
    print("  C0B7: D0 03       BNE $C0BC      ; if hi != 0, we have an entry")
    print("  C0B9: 8A          TXA")
    print("  C0BA: F0 0B       BEQ $C0C7      ; if both lo and hi are 0, done")
    print("  C0BC: 84 10       STY $10        ; save Y in ZP $10")
    print("  C0BE: 20 9B C0    JSR $C09B      ; -> JMP ($0000) = call function")
    print("  C0C1: A4 10       LDY $10        ; restore Y")
    print("  C0C3: C8          INY            ; Y += 2 (next entry)")
    print("  C0C4: C8          INY")
    print("  C0C5: D0 E5       BNE $C0AC      ; always branch (loop)")
    print("  C0C7: 68          PLA            ; restore ZP $00-$03")
    print("  C0C8: 85 00       STA $00")
    print("  C0CA: 68          PLA")
    print("  C0CB: 85 01       STA $01")
    print("  C0CD: 68          PLA")
    print("  C0CE: 85 02       STA $02")
    print("  C0D0: 68          PLA")
    print("  C0D1: 85 03       STA $03")
    print()
    
    # Read the constructor table at $EB5D
    print("=== Constructor/init table at $EB5D ===")
    f.seek(base + 0x2B5D)  # $EB5D in bank 31 = offset 0x2B5D
    table = f.read(32)
    
    print(f"Raw bytes: {' '.join(f'{b:02X}' for b in table[:32])}")
    print()
    print("Function pointers:")
    for i in range(0, 32, 2):
        lo, hi = table[i], table[i+1]
        if lo == 0 and hi == 0:
            print(f"  [{i//2}]: $0000 (END)")
            break
        addr = (hi << 8) | lo
        print(f"  [{i//2}]: ${addr:04X}")
    
    # Also check where main() is
    print()
    print("=== Looking for main() address ===")
    # Check the code after the init runner - does it JMP to main?
    # After JSR $C09E returns, we hit JMP $C098 (infinite loop)
    # So the init runner must NOT return - the last init function must be main()
    # OR: the init runner calls main(), and main() never returns
    
    # Let's check what's at the addresses in the table
    print()
    for i in range(0, 32, 2):
        lo, hi = table[i], table[i+1]
        if lo == 0 and hi == 0:
            break
        addr = (hi << 8) | lo
        if addr >= 0xC000:
            offset = addr - 0xC000
            f.seek(base + offset)
            first_bytes = f.read(8)
            print(f"  ${addr:04X}: {' '.join(f'{b:02X}' for b in first_bytes)}")
    
    # Check C0D2 explicitly (right after the init runner)
    print()
    print("=== Code after init runner (C0D2+) ===")
    f.seek(base + 0x00D2)
    after = f.read(32)
    print(f"  C0D2: {' '.join(f'{b:02X}' for b in after[:16])}")
    print(f"  C0E2: {' '.join(f'{b:02X}' for b in after[16:])}")
