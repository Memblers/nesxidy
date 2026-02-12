#!/usr/bin/env python3
"""Decode the native JSR trampoline from the built ROM."""

with open(r"C:\proj\c\NES\nesxidy-co\nesxidy\exidy.nes", "rb") as f:
    rom = f.read()

# Trampoline found at ROM offset 0x7F9CD
off = 0x7F9CD
code = rom[off:off+32]

print("Native JSR trampoline at WRAM $626D:")
print(f"Raw: {' '.join(f'{b:02X}' for b in code[:20])}")
print()

# Manual decode
# $626D: 20 F4 61  JSR $61F4 (dispatch_on_pc)
# $6270: D0 0A     BNE +10 ($627C) -> bail
# $6272: A5 6C     LDA $6C (_sp)
# $6274: CD B6 00  CMP $00B6 (_native_jsr_saved_sp) [absolute mode for ZP var]
# $6277: 90 F4     BCC -12 ($626D) -> loop back
# $6279: A9 00     LDA #$00
# $627B: 60        RTS
# $627C: 60        RTS (bail path)

addr = 0x626D
i = 0
opcodes = {
    0x20: ("JSR", 3, "abs"),
    0xD0: ("BNE", 2, "rel"),
    0xA5: ("LDA", 2, "zp"),
    0xCD: ("CMP", 3, "abs"),
    0x90: ("BCC", 2, "rel"),
    0xA9: ("LDA", 2, "imm"),
    0x60: ("RTS", 1, "imp"),
}

while i < 16:
    b = code[i]
    if b in opcodes:
        name, sz, mode = opcodes[b]
        if sz == 1:
            print(f"  ${addr+i:04X}: {b:02X}         {name}")
        elif sz == 2:
            op = code[i+1]
            if mode == "rel":
                target = addr + i + 2 + (op if op < 128 else op - 256)
                print(f"  ${addr+i:04X}: {b:02X} {op:02X}      {name} ${target:04X}")
            elif mode == "imm":
                print(f"  ${addr+i:04X}: {b:02X} {op:02X}      {name} #${op:02X}")
            else:
                print(f"  ${addr+i:04X}: {b:02X} {op:02X}      {name} ${op:02X}")
        elif sz == 3:
            lo, hi = code[i+1], code[i+2]
            print(f"  ${addr+i:04X}: {b:02X} {lo:02X} {hi:02X}   {name} ${hi:02X}{lo:02X}")
        i += sz
    else:
        print(f"  ${addr+i:04X}: {b:02X}         ???")
        i += 1

print()
print("Flow analysis:")
print("  $626D: JSR dispatch_on_pc")
print("  $6270: BNE $627C (if A!=0 -> bail)")
print("  $6272: LDA _sp ($6C)")
print("  $6274: CMP _native_jsr_saved_sp ($00B6)")
print("  $6277: BCC $626D (if SP < saved_sp -> loop)")
print("  $6279: LDA #0 (success)")
print("  $627B: RTS (return to template)")
print("  $627C: RTS (bail - A already has non-zero code)")
