#!/usr/bin/env python3
"""Trace exactly what happens after the init runner returns."""

with open('exidy.nes', 'rb') as f:
    base = 16 + 31 * 16384
    
    # Read C0C7 onwards (where init runner exits when table is empty)
    f.seek(base + 0x00C7)
    code = f.read(32)
    
    print("=== After init runner loop exits at $C0C7 ===")
    print("Raw bytes:")
    for i in range(0, 32, 16):
        addr = 0xC0C7 + i
        hex_str = ' '.join(f'{code[i+j]:02X}' for j in range(min(16, 32-i)))
        print(f'  {addr:04X}: {hex_str}')
    
    print()
    print("Disassembly from $C0C7:")
    print("  C0C7: 68          PLA            ; pop saved $00")
    print("  C0C8: 85 00       STA $00")
    print("  C0CA: 68          PLA")
    print("  C0CB: 85 01       STA $01")
    print("  C0CD: 68          PLA")
    print("  C0CE: 85 02       STA $02")
    print("  C0D0: 68          PLA")
    print("  C0D1: 85 03       STA $03")
    # Now what's at C0D3?
    print(f"  C0D3: {code[0x0C]:02X} {code[0x0D]:02X} {code[0x0E]:02X}    JSR ${code[0x0E]:02X}{code[0x0D]:02X}")
    
    # Wait, C0D3 = offset 0x0C from C0C7
    pc = 0x0C
    print()
    print(f"  At offset C0D3: byte = {code[pc]:02X}")
    if code[pc] == 0x20:
        print(f"  C0D3: 20 {code[pc+1]:02X} {code[pc+2]:02X}    JSR ${code[pc+2]:02X}{code[pc+1]:02X}")
    
    # Actually wait - the init runner was JSR'd from C095. So after the PLA/STA sequence,
    # the NEXT instruction after STA $03 at C0D1 is at C0D3, but there's no RTS!
    # Unless... hmm. Let me look at the raw bytes again more carefully.
    
    print()
    print("Detailed byte-by-byte from C0C7:")
    for i in range(20):
        addr = 0xC0C7 + i
        print(f"  {addr:04X}: {code[i]:02X}")
    
    # The function was called via JSR $C09E from $C095
    # When JSR $C09E was done, the return address on stack is $C097
    # (JSR pushes PC+2-1 = $C095+3-1 = $C097)
    # So when the init runner does RTS, it returns to $C098
    # But there's no RTS in the code I see...
    
    # Unless the init runner is SUPPOSED to fall through to code that
    # eventually does RTS. Let me look more carefully.
    
    print()
    print("=== Does the init runner have RTS? ===")
    f.seek(base + 0x009E)
    runner = f.read(64)
    for i in range(64):
        if runner[i] == 0x60:  # RTS
            print(f"  Found RTS at ${0xC09E+i:04X}")
    
    # Let me check - C0D2 is the byte after STA $03 (C0D1 is 85 03)
    # C0D2 should be at base + 0x00D2
    f.seek(base + 0x00D2)
    byte = f.read(1)[0]
    print(f"\n  Byte at C0D2: {byte:02X}")
    if byte == 0x60:
        print("  -> RTS! Init runner returns here")
    elif byte == 0x20:
        f.seek(base + 0x00D3)
        args = f.read(2)
        target = (args[1] << 8) | args[0]
        print(f"  -> JSR ${target:04X}")
