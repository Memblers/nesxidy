import sys

data = open('exidy.nes','rb').read()
offset = 0x0110F8
lo, hi = data[offset], data[offset+1]
entry = lo | (hi << 8)
elo, ehi = data[offset+2], data[offset+3]
exit_pc = elo | (ehi << 8)
clen = data[offset+4]
epi = data[offset+5]
code = data[offset+8:offset+8+epi]

print(f"Block entry=${entry:04X} exit=${exit_pc:04X} clen={clen} epi={epi}")
print(f"  (was clen=114 epi=75 with 16-byte exit patterns)")
print()

# Disassemble
i = 0
while i < len(code):
    b = code[i]
    if b == 0xA9:
        print(f"  ci={i:02d}: LDA #${code[i+1]:02X}")
        i += 2
    elif b == 0x8D:
        addr = code[i+1] | (code[i+2] << 8)
        print(f"  ci={i:02d}: STA ${addr:04X}")
        i += 3
    elif b == 0xCE:
        addr = code[i+1] | (code[i+2] << 8)
        print(f"  ci={i:02d}: DEC ${addr:04X}")
        i += 3
    elif b == 0xD0:
        rel = code[i+1]
        if rel > 127: rel -= 256
        target = i + 2 + rel
        print(f"  ci={i:02d}: BNE ci={target:02d}  (offset {rel:+d})  *** NATIVE BRANCH ***")
        i += 2
    elif b == 0xF0:
        rel = code[i+1]
        if rel > 127: rel -= 256
        target = i + 2 + rel
        print(f"  ci={i:02d}: BEQ ci={target:02d}  (offset {rel:+d})")
        i += 2
    elif b == 0x08:
        print(f"  ci={i:02d}: PHP")
        i += 1
    elif b == 0x85:
        print(f"  ci={i:02d}: STA ${code[i+1]:02X}")
        i += 2
    elif b == 0x86:
        print(f"  ci={i:02d}: STX ${code[i+1]:02X}")
        i += 2
    elif b == 0x84:
        print(f"  ci={i:02d}: STY ${code[i+1]:02X}")
        i += 2
    elif b == 0xA6:
        print(f"  ci={i:02d}: LDX ${code[i+1]:02X}")
        i += 2
    elif b == 0xE8:
        print(f"  ci={i:02d}: INX")
        i += 1
    elif b == 0xBD:
        addr = code[i+1] | (code[i+2] << 8)
        print(f"  ci={i:02d}: LDA ${addr:04X},X")
        i += 3
    elif b == 0xE6:
        print(f"  ci={i:02d}: INC ${code[i+1]:02X}")
        i += 2
    elif b == 0x4C:
        addr = code[i+1] | (code[i+2] << 8)
        print(f"  ci={i:02d}: JMP ${addr:04X}")
        i += 3
    else:
        print(f"  ci={i:02d}: ${b:02X}")
        i += 1

print()
# The original guest code for reference:
# $29CC: LDA #$00
# $29CE: STA $15     (-> $6D15 in WRAM)  
# $29D0: DEC $16     (-> $6D16 in WRAM)
# $29D2: BNE $29D0   (inner loop)
# $29D4: DEC $15     (-> $6D15 in WRAM)
# $29D6: BNE $29D0   (outer loop)
# $29D8: RTS
print("Expected: BNE at ci=08 -> ci=05 (inner), BNE at ci=13 -> ci=05 (outer)")
print("Both should be NATIVE BRANCH, not 16-byte exit-to-dispatcher!")
