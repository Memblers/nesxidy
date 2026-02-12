#!/usr/bin/env python3
"""Deep crash trace analysis - figure out what happened."""
import zlib

# Load savestate
with open(r"C:\Users\membl\OneDrive\Documents\Mesen2\SaveStates\exidy_1.mss", "rb") as f:
    data = f.read()

pos = 0
zblocks = []
while pos < len(data) - 2:
    if data[pos] == 0x78 and data[pos+1] in (0x01, 0x5E, 0x9C, 0xDA):
        try:
            d = zlib.decompressobj()
            r = d.decompress(data[pos:])
            c = len(data[pos:]) - len(d.unused_data)
            zblocks.append((pos, bytearray(r)))
            pos += c
            continue
        except:
            pass
    pos += 1

blk1 = zblocks[1][1]

# Get internal RAM
marker = b'memoryManager.internalRam'
p = blk1.find(marker)
after_null = p + len(marker) + 1
sz = int.from_bytes(blk1[after_null:after_null+4], 'little')
ram_start = after_null + 4
ram = blk1[ram_start:ram_start+sz]

# Get WRAM
wram_marker = b'mapper.workRam'
wp = blk1.find(wram_marker)
wram_after = wp + len(wram_marker) + 1
wram_sz = int.from_bytes(blk1[wram_after:wram_after+4], 'little')
wram = blk1[wram_after+4:wram_after+4+wram_sz]

print("=== NES CPU State at crash ===")
print(f"  PC = $C140")
print(f"  SP = $F5")
print(f"  A  = $EA")
print(f"  X  = $2E")
print(f"  Y  = $00")
print(f"  PS = $85 (NV--DIZC = 10000101 -> N=1, V=0, D=0, I=1, Z=0, C=1)")

print(f"\nEmulated regs:")
print(f"  _pc    = $2E5F")
print(f"  _sp    = $FB")
print(f"  _a     = $FF")
print(f"  _x     = $00")
print(f"  _y     = $00")
print(f"  _status= $B5")

# Now let's think about the stack more carefully.
# SP=$F5 means next push goes to $01F5.
# Top of stack (last pushed) is at $01F6.
# 
# BUT WAIT: this is the hardware stack at the time of the savestate.
# The system seems to be running C code at $C140 (RTI opcode? no, $40=RTI)
# $C140 = RTI instruction!
# 
# Actually wait: $C140 in the ROM is 40 A0 00 B1 30...
# $40 = RTI. But the NMI handler or IRQ handler would end with RTI.
# The PC is AT the RTI, which means it hasn't executed yet.
#
# This is a VBLANK/NMI handler! The code from $C141 onward reads 
# from ($30),Y and writes to PPU registers ($2006, $2007).
# 
# So the crash isn't IN our JSR code - the savestate was taken
# during normal NMI processing. Let me re-think.

# Actually, the system is still running. Let's check if there are signs
# of the JSR code having executed by looking at the emulated state.

# _pc = $2E5F means the emulated PC advanced past the JSR at $2E0B.
# The JSR at $2E0B targeted $2E58, and the block at $2E58 had an 
# exit_pc of $2E5F. So it seems the JSR WORKED and the block at $2E58
# executed successfully!

# Wait, let me re-check. From the conversation summary:
# - Block at $9E00 for PC=$2E0B: JSR template to $2E58, exit_pc=$2E0E
# - Block at $9F00 for PC=$2E58: some code + JSR to $2CB0, exit_pc=$2E5F
#
# So _pc=$2E5F means block $9F00 (PC=$2E58) completed. It hit its epilogue
# and returned with exit_pc=$2E5F. Then the system would try to compile/dispatch
# $2E5F next.

# Let's check: is $2E5F compiled?
rom_data = open(r"C:\proj\c\NES\nesxidy-co\nesxidy\exidy.nes", "rb").read()
HEADER_SIZE = 16
BANK_SIZE = 0x4000

# PC=$2E5F: flag bank = ($2E5F >> 14) + 27 = 27
# flag offset = $2E5F & $3FFF = $2E5F
flag_off = HEADER_SIZE + 27 * BANK_SIZE + 0x2E5F
flag = rom_data[flag_off]
print(f"\nPC=$2E5F flash flag: ${flag:02X}")

# PC lookup
pc_bank = (0x2E5F >> 13) + 19  # = 20
pc_addr = (0x2E5F << 1) & 0x3FFF  # = $1CBE
pc_off = HEADER_SIZE + pc_bank * BANK_SIZE + pc_addr
native_lo = rom_data[pc_off]
native_hi = rom_data[pc_off + 1]
native = (native_hi << 8) | native_lo
print(f"PC=$2E5F native addr: ${native:04X}")

if flag == 0xFF:
    print("-> $2E5F is NOT compiled (flag=$FF)")
    print("-> dispatch_on_pc would return 1 (recompile) or 2 (interpret)")
elif flag != 0xFF:
    print(f"-> $2E5F IS compiled (flag=${flag:02X}, native=${native:04X})")

# Check the emulated stack
print(f"\nEmulated stack (SP=$FB, stack at WRAM $6E00):")
emu_stack_off = 0x0E00
# SP=$FB means top of stack at $01FC
# Stack contents around $01FC
for i in range(0xF8, 0x100):
    val = wram[emu_stack_off + i]
    print(f"  ${0x0100+i:04X}: ${val:02X}", end="")
    if i == 0xFB:
        print(" <- SP", end="")
    if i == 0xFC:
        print(" <- top of stack (last pushed)", end="")
    print()

# The emulated stack should show the JSR return addresses
# JSR at $2E0B to $2E58: pushes return address $2E0D (hi=$2E, lo=$0D)
# Actually wait: 6502 JSR pushes PC+2-1, so for JSR $2E58 at PC=$2E0B:
# $2E0B + 2 = $2E0D, - 1 = technically no: 
# JSR pushes the address of the LAST byte of the JSR instruction
# JSR is 3 bytes: opcode + lo + hi. At $2E0B, bytes are $2E0B, $2E0C, $2E0D
# JSR pushes $2E0D (hi first=$2E, then lo=$0D)
# BUT in our JSR template, WE push the return address, not the hardware 6502!
# The template pushes to emulated stack at WRAM.

# Let me check what the emulated stack shows
print(f"\nFull emulated stack top:")
for i in range(0xF0, 0x100):
    val = wram[emu_stack_off + i]
    if val != 0:
        print(f"  ${0x0100+i:04X}: ${val:02X}")

# Now check: in the JSR template for $2E0B, it pushes $2E (hi) then $0D (lo)
# In the JSR template for $2E58 (JSR to $2CB0), it pushes $2E (hi) then $5E (lo)
# SP starts at $FF typically, each push decrements:
# After 1st JSR (to $2E58): pushed $2E at $01FF, $0D at $01FE -> SP=$FD
# Then block $2E58 has another JSR (to $2CB0): pushed $2E at $01FD, $5E at $01FC -> SP=$FB
# And _sp=$FB matches!
# After RTS from $2CB0, the block epilogue would restore: 
# Wait, the JSR template ALSO stores the return via _sp

# Actually: after the JSR template at $2E58's block finishes:
# The block's epilogue sets _pc=$2E5F and returns to C
# But the emulated stack still has the pushed values from both JSRs!
# This means the system hasn't done RTS for either JSR yet.

print(f"\nExpected emulated stack state:")
print(f"  1st JSR ($2E0B -> $2E58): push $2E@$01FF, $0D@$01FE -> SP=$FD")
print(f"  2nd JSR ($2E58 -> $2CB0): push $2E@$01FD, $5E@$01FC -> SP=$FB")
print(f"  Current SP = $FB -> both JSR returns still on stack")
print(f"  When $2CB0 subroutine does RTS: pops $5E,$2E -> returns to $2E5F")  
print(f"  When code reaches RTS matching 1st JSR: pops $0D,$2E -> returns to $2E0E")

# Wait, but _pc is already $2E5F and the block ended!
# This means the block at $2E58 set exit_pc=$2E5F (its EPILOGUE exit pc)
# but the subroutine at $2CB0 hasn't returned yet via emulated RTS.
# The emulated code thinks we returned from the JSR to $2CB0, but we didn't!

# Actually no: the JSR template doesn't CALL the subroutine via emulated JSR.
# It sets _pc to the target ($2E58, or $2CB0) and jumps to dispatch.
# The subroutine code at $2CB0 runs, and when IT reaches RTS, it pops from 
# emulated stack... but wait, if the block ending JSR is in "test mode" 
# (READY_FOR_NEXT cleared), then the block at $2E58 contains a JSR 
# to $2CB0 that ALSO ends the block. So:
# 
# Block $2E58 runs LDA#0, STA$6D15 (5 bytes), then the JSR template:
#   - push $2E and $5E to emulated stack (SP goes FD->FB)
#   - set _pc=$2CB0
#   - JMP to flash_dispatch_return_no_regs
# Then flash_dispatch_return_no_regs does PLA STA $70 LDA#0 RTS
# This returns 0 to run_6502
# run_6502 returns to main loop
# main loop calls run_6502 again
# run_6502 calls compile()/dispatch for _pc=$2CB0
# ... and so on until $2CB0's code does an emulated RTS

# So the emulated stack should show the pushed values.
# Let's verify:
print(f"\nActual emulated stack values:")
for addr in [0xFF, 0xFE, 0xFD, 0xFC, 0xFB]:
    val = wram[emu_stack_off + addr]
    print(f"  ${0x0100+addr:04X}: ${val:02X}")

# Check if we have compiled blocks around $2CB0
pc_test = 0x2CB0
flag_off2 = HEADER_SIZE + 27 * BANK_SIZE + (pc_test & 0x3FFF)
flag2 = rom_data[flag_off2]
pc_bank2 = (pc_test >> 13) + 19
pc_addr2 = (pc_test << 1) & 0x3FFF
pc_off2 = HEADER_SIZE + pc_bank2 * BANK_SIZE + pc_addr2
native2 = int.from_bytes(rom_data[pc_off2:pc_off2+2], 'little')
print(f"\nPC=$2CB0: flag=${flag2:02X}, native=${native2:04X}")

# Check what the original Exidy ROM code at $2E5F looks like
# The original game code is in roms/ directory, but also in flash
# bank 1 is ROM data bank. Let's check it.
rom_bank1_off = HEADER_SIZE + 1 * BANK_SIZE
print(f"\nOriginal ROM at $2E58-$2E70 (bank 1):")
for i in range(0, 32, 16):
    off = rom_bank1_off + 0x2E58 + i
    hex_str = ' '.join(f'{b:02X}' for b in rom_data[off:off+16])
    print(f"  ${0x2E58+i:04X}: {hex_str}")

# Let's also look at $2CB0
print(f"\nOriginal ROM at $2CB0-$2CD0 (bank 1):")
for i in range(0, 32, 16):
    off = rom_bank1_off + 0x2CB0 + i
    hex_str = ' '.join(f'{b:02X}' for b in rom_data[off:off+16])
    print(f"  ${0x2CB0+i:04X}: {hex_str}")

# Check a few more PCs around the area that should have been compiled
for test_pc in [0x2E0B, 0x2E0E, 0x2E58, 0x2E5F, 0x2CB0, 0x2CB5]:
    fb = HEADER_SIZE + 27 * BANK_SIZE + (test_pc & 0x3FFF)
    fl = rom_data[fb]
    pb = (test_pc >> 13) + 19
    pa = (test_pc << 1) & 0x3FFF
    po = HEADER_SIZE + pb * BANK_SIZE + pa
    na = int.from_bytes(rom_data[po:po+2], 'little')
    compiled = "compiled" if fl != 0xFF else "NOT compiled"
    print(f"  PC=${test_pc:04X}: flag=${fl:02X}, native=${na:04X} [{compiled}]")
