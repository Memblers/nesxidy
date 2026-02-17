import sys

data = open('exidy.nes','rb').read()

def disasm_block(offset):
    lo, hi = data[offset], data[offset+1]
    entry = lo | (hi << 8)
    elo, ehi = data[offset+2], data[offset+3]
    exit_pc = elo | (ehi << 8)
    clen = data[offset+4]
    epi = data[offset+5]
    code = data[offset+8:offset+8+clen]
    print(f"Block entry=${entry:04X} exit=${exit_pc:04X} clen={clen} epi={epi}")
    
    i = 0
    while i < len(code):
        b = code[i]
        prefix = "  " if i < epi else "E "  # E = epilogue
        if b in (0x10,0x30,0x50,0x70,0x90,0xB0,0xD0,0xF0):
            nm = {0x10:'BPL',0x30:'BMI',0x50:'BVC',0x70:'BVS',0x90:'BCC',0xB0:'BCS',0xD0:'BNE',0xF0:'BEQ'}[b]
            r = code[i+1] if i+1<len(code) else 0
            if r > 127: r -= 256
            print(f'{prefix}ci={i:02d}: {nm} ci={i+2+r} (off={r:+d})')
            i += 2
        elif b == 0xA9: print(f'{prefix}ci={i:02d}: LDA #${code[i+1]:02X}'); i+=2
        elif b == 0x85: print(f'{prefix}ci={i:02d}: STA ${code[i+1]:02X}'); i+=2
        elif b == 0x86: print(f'{prefix}ci={i:02d}: STX ${code[i+1]:02X}'); i+=2
        elif b == 0x84: print(f'{prefix}ci={i:02d}: STY ${code[i+1]:02X}'); i+=2
        elif b == 0xA6: print(f'{prefix}ci={i:02d}: LDX ${code[i+1]:02X}'); i+=2
        elif b == 0xE6: print(f'{prefix}ci={i:02d}: INC ${code[i+1]:02X}'); i+=2
        elif b == 0xC6: print(f'{prefix}ci={i:02d}: DEC ${code[i+1]:02X}'); i+=2
        elif b == 0x08: print(f'{prefix}ci={i:02d}: PHP'); i+=1
        elif b == 0x28: print(f'{prefix}ci={i:02d}: PLP'); i+=1
        elif b == 0xEA: print(f'{prefix}ci={i:02d}: NOP'); i+=1
        elif b == 0xE8: print(f'{prefix}ci={i:02d}: INX'); i+=1
        elif b == 0xCA: print(f'{prefix}ci={i:02d}: DEX'); i+=1
        elif b == 0x18: print(f'{prefix}ci={i:02d}: CLC'); i+=1
        elif b == 0x60: print(f'{prefix}ci={i:02d}: RTS'); i+=1
        elif b == 0x48: print(f'{prefix}ci={i:02d}: PHA'); i+=1
        elif b == 0x68: print(f'{prefix}ci={i:02d}: PLA'); i+=1
        elif b == 0x4C:
            a = code[i+1]|(code[i+2]<<8)
            print(f'{prefix}ci={i:02d}: JMP ${a:04X}')
            i += 3
        elif b == 0x8D:
            a = code[i+1]|(code[i+2]<<8)
            print(f'{prefix}ci={i:02d}: STA ${a:04X}')
            i += 3
        elif b == 0xAD:
            a = code[i+1]|(code[i+2]<<8)
            print(f'{prefix}ci={i:02d}: LDA ${a:04X}')
            i += 3
        elif b == 0xBD:
            a = code[i+1]|(code[i+2]<<8)
            print(f'{prefix}ci={i:02d}: LDA ${a:04X},X')
            i += 3
        elif b == 0x8E:
            a = code[i+1]|(code[i+2]<<8)
            print(f'{prefix}ci={i:02d}: STX ${a:04X}')
            i += 3
        elif b == 0x9D:
            a = code[i+1]|(code[i+2]<<8)
            print(f'{prefix}ci={i:02d}: STA ${a:04X},X')
            i += 3
        elif b == 0xCE:
            a = code[i+1]|(code[i+2]<<8)
            print(f'{prefix}ci={i:02d}: DEC ${a:04X}')
            i += 3
        else:
            print(f'{prefix}ci={i:02d}: [${b:02X}]')
            i += 1
    print()

# Coin check block at $2CB0
print("=== COIN CHECK ($2CB0) ===")
disasm_block(0x012B58)

# Also check $2CB3 (the block that might handle the BPL branch target)
# Find block at $2CB3
print("=== Attract delay block $2E58 ===")
disasm_block(0x013AC8)

print("=== Inner loop block $2E65 ===")
disasm_block(0x013B68)

print("=== Bare delay $29CC ===")
disasm_block(0x0110F8)
