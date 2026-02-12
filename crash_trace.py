#!/usr/bin/env python3
"""Full crash analysis: trace what happened at NES CPU PC=$C140."""
import zlib

# Load NES ROM
with open(r"C:\proj\c\NES\nesxidy-co\nesxidy\exidy.nes", "rb") as f:
    rom = f.read()

HEADER_SIZE = 16
BANK_SIZE = 0x4000

def rom_offset(bank, addr):
    return HEADER_SIZE + bank * BANK_SIZE + (addr & 0x3FFF)

# PC=$C140 is in the $C000-$FFFF range = bank 31 (fixed bank)
bank31_start = rom_offset(31, 0xC000)
# $C140 in bank 31 = offset $0140 within bank
off = rom_offset(31, 0xC140)

print("=== Code at NES $C140 (bank 31, the fixed bank) ===")
for i in range(0, 64, 16):
    hex_str = ' '.join(f'{b:02X}' for b in rom[off+i:off+i+16])
    print(f"  ${0xC140+i:04X}: {hex_str}")

# Let's also check what's at the map file addresses
# dispatch_on_pc is in WRAM ($61F4) - already verified
# But $C140 is in the fixed bank. Let's see what function is near $C140

# Search the map file or linker output for $C140
# Let's just look at the code around $C140 to identify the function

print("\n=== Wider context around $C140 ===")
off_start = rom_offset(31, 0xC100)
for i in range(0, 128, 16):
    hex_str = ' '.join(f'{b:02X}' for b in rom[off_start+i:off_start+i+16])
    ascii_str = ''.join(chr(b) if 32 <= b < 127 else '.' for b in rom[off_start+i:off_start+i+16])
    print(f"  ${0xC100+i:04X}: {hex_str}  {ascii_str}")

# Also check $C180-$C200 region
off2 = rom_offset(31, 0xC180)
for i in range(0, 128, 16):
    hex_str = ' '.join(f'{b:02X}' for b in rom[off2+i:off2+i+16])
    print(f"  ${0xC180+i:04X}: {hex_str}")

# The hardware stack has interesting data. SP=$F5, so top of stack starts at $01F5+1=$01F6
# Stack contents at $01F5-$01FF from the savestate (already extracted):
# $01DF: DB
# $01E0: C1 00 01 DB C1 1C C9 8B 63 21 C7 28 2D 00 DB C1
# $01F0: 19 DB C1 00 2E EA A1 45 58 C6 00 00 D5 C0 97 C0

# SP=$F5 means next push at $01F5, current top at $01F6
# Stack reading (bottom up, return addresses are lo-1, hi format):
# $01F6-$01F7: $C1 $19 -> return to $C11A (this was the last JSR)
# Wait, 6502 stack: SP=$F5 means stack pointer points at $01F5
# Data pushed: $01F6, $01F7, $01F8... upward
# For JSR, pushes PC+2-1 (hi then lo), so popped as lo then hi
# Let's decode the return chain from $01F6 upward

hw_stack = bytes.fromhex(
    "8C 0C 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
    "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
    "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
    "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
    "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
    "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
    "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
    "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
    "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
    "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
    "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
    "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
    "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
    "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 DB"
    "C1 00 01 DB C1 1C C9 8B 63 21 C7 28 2D 00 DB C1"
    "19 DB C1 00 2E EA A1 45 58 C6 00 00 D5 C0 97 C0"
)

print("\n=== Hardware stack return chain (SP=$F5) ===")
print(f"Stack data $01F0-$01FF: {' '.join(f'{b:02X}' for b in hw_stack[0xF0:0x100])}")
print(f"Stack data $01E0-$01EF: {' '.join(f'{b:02X}' for b in hw_stack[0xE0:0xF0])}")
print(f"Stack data $01D0-$01DF: {' '.join(f'{b:02X}' for b in hw_stack[0xD0:0xE0])}")

# SP=$F5: data above SP is return chain
# $01F6 = DB, $01F7 = C1 -> return addr = $C1DC (JSR at $C1D9?)
# Wait, 6502 pushes hi first then lo. So RTS pops lo ($01F6) then hi ($01F7)
# RTS adds 1, so return = ($C1 << 8 | $DB) + 1 = $C1DC
# But wait: $01F6=$19, $01F7=$DB? No let me re-read

# SP=$F5 means the LAST byte pushed is at $01F6
# And the bytes go upward. Let me re-decode properly
sp = 0xF5
print(f"\nSP = $F5, top of stack at ${0x100 + sp + 1:04X}")
print(f"Bytes above SP: ", end="")
for i in range(sp+1, 0x100):
    print(f"${hw_stack[i]:02X} ", end="")
print()

# Decode return addresses (each JSR pushes 2 bytes: hi then lo of PC+2-1)
# So they appear in stack as: lo @ sp+1, hi @ sp+2
# RTS: pops lo, pops hi, increments to get return address
idx = sp + 1
print(f"\nReturn chain decode:")
count = 0
while idx < 0xFF and count < 10:
    lo = hw_stack[idx]
    hi = hw_stack[idx+1]
    ret_addr = ((hi << 8) | lo) + 1
    print(f"  [{idx:02X}]: lo=${lo:02X} hi=${hi:02X} -> RTS to ${ret_addr:04X}")
    idx += 2
    count += 1

# Also check what's in the WRAM around the dispatch code
# dispatch_on_pc is at $61F4, flash_dispatch_return_no_regs at $6267
# C main calls run_6502 -> dispatch_on_pc

# Let's look at what JSR could have led to PC=$C140
# If we look at $C140 - 3 (a JSR instruction is 3 bytes), that'd be $C13D
# But the return chain might tell us more

print("\n=== Code at $C0D5-$C0E0 (from return chain $C0D6) ===")
off3 = rom_offset(31, 0xC0D0)
for i in range(0, 32, 16):
    hex_str = ' '.join(f'{b:02X}' for b in rom[off3+i:off3+i+16])
    print(f"  ${0xC0D0+i:04X}: {hex_str}")

print("\n=== Code at $C095-$C0A0 (from return chain $C098) ===")
off4 = rom_offset(31, 0xC090)
for i in range(0, 32, 16):
    hex_str = ' '.join(f'{b:02X}' for b in rom[off4+i:off4+i+16])
    print(f"  ${0xC090+i:04X}: {hex_str}")

# Let's look at what's at $C1DC, $C1DA, etc
print("\n=== Code at $C1D0-$C200 ===")
off5 = rom_offset(31, 0xC1D0)
for i in range(0, 48, 16):
    hex_str = ' '.join(f'{b:02X}' for b in rom[off5+i:off5+i+16])
    print(f"  ${0xC1D0+i:04X}: {hex_str}")

# Check $C6XX area (from stack $C659?)
print("\n=== Code at $C650-$C670 ===")
off6 = rom_offset(31, 0xC650)
for i in range(0, 32, 16):
    hex_str = ' '.join(f'{b:02X}' for b in rom[off6+i:off6+i+16])
    print(f"  ${0xC650+i:04X}: {hex_str}")
