#!/usr/bin/env python3
"""Check the assembled screen_diff_build_list in the NES ROM binary."""

data = open('exidy.nes', 'rb').read()

# Search for: A9 00 85 AF A0 00 (LDA #0, STA $AF, LDY #0)
# Then B9 = LDA abs,Y or B1 = LDA (zp),Y
for i in range(len(data) - 10):
    if (data[i] == 0xA9 and data[i+1] == 0x00 and 
        data[i+2] == 0x85 and data[i+3] == 0xAF and
        data[i+4] == 0xA0 and data[i+5] == 0x00):
        chunk = data[i:i+50]
        hex_str = ' '.join(f'{b:02X}' for b in chunk)
        print(f'ROM 0x{i:05X}: {hex_str}')
        next_op = data[i+6]
        if next_op == 0xB9:
            addr = data[i+7] | (data[i+8] << 8)
            print(f'  -> LDA ${addr:04X},Y  (absolute,Y = 4 cycles)')
        elif next_op == 0xB1:
            zp = data[i+7]
            print(f'  -> LDA (${zp:02X}),Y  (indirect,Y = 5 cycles)')
        else:
            print(f'  -> Unknown opcode 0x{next_op:02X}')

        # Also look at CMP following the LDA
        if next_op == 0xB9:
            cmp_off = i + 9  # after 3-byte LDA abs,Y
        elif next_op == 0xB1:
            cmp_off = i + 8  # after 2-byte LDA (zp),Y
        else:
            continue
        cmp_op = data[cmp_off]
        if cmp_op == 0xD9:
            addr2 = data[cmp_off+1] | (data[cmp_off+2] << 8)
            print(f'  -> CMP ${addr2:04X},Y  (absolute,Y = 4 cycles)')
        elif cmp_op == 0xD1:
            zp2 = data[cmp_off+1]
            print(f'  -> CMP (${zp2:02X}),Y  (indirect,Y = 5 cycles)')
        else:
            print(f'  -> CMP opcode: 0x{cmp_op:02X}')
        break
else:
    print("Pattern not found! Trying broader search...")
    # Try without the STA ZP - maybe the assembler used absolute STA
    for i in range(len(data) - 10):
        if (data[i] == 0xA9 and data[i+1] == 0x00):
            # Look for LDY #0 within next few bytes
            for j in range(2, 8):
                if i+j+2 < len(data) and data[i+j] == 0xA0 and data[i+j+1] == 0x00:
                    next_op = data[i+j+2]
                    if next_op in (0xB9, 0xB1):
                        chunk = data[i:i+30]
                        hex_str = ' '.join(f'{b:02X}' for b in chunk)
                        mode = 'abs,Y' if next_op == 0xB9 else '(zp),Y'
                        print(f'ROM 0x{i:05X}: {hex_str}  ({mode})')
