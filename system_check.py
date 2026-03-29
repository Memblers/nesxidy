#!/usr/bin/env python3
"""Check if the system is functioning correctly or stuck. Analyze the execution flow."""
import zlib

# Load NES ROM
with open(r"C:\proj\c\NES\nesxidy-co\nesxidy\exidy.nes", "rb") as f:
    rom = f.read()

HEADER_SIZE = 16
BANK_SIZE = 0x4000

def rom_offset(bank, addr):
    return HEADER_SIZE + bank * BANK_SIZE + (addr & 0x3FFF)

# Load savestate WRAM
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
wram_marker = b'mapper.workRam'
wp = blk1.find(wram_marker)
wram_after = wp + len(wram_marker) + 1
wram_sz = int.from_bytes(blk1[wram_after:wram_after+4], 'little')
wram = blk1[wram_after+4:wram_after+4+wram_sz]

ram_marker = b'memoryManager.internalRam'
p = blk1.find(ram_marker)
after_null = p + len(ram_marker) + 1
sz = int.from_bytes(blk1[after_null:after_null+4], 'little')
ram = blk1[after_null+4:after_null+4+sz]

print("=== System State Summary ===")
print(f"NES CPU PC = $C140 (___irq - NMI handler entry)")
print(f"NES CPU SP = $F5")
print(f"NES CPU A=$EA X=$2E Y=$00 PS=$85")
print(f"mapper.prgBank = $02 (BANK_FLASH_BLOCK_FLAGS)")
print()

# Check the hardware stack return chain
print("=== NES Hardware Stack Return Chain ===")
# The NES is in an NMI interrupt. The NMI entry pushes P, PC_hi, PC_lo
# So at the bottom of the current frame:
# $01F8-$01FA: pushed by NMI entry (P, PCH, PCL)
# Wait, 6502 interrupt: pushes PCH, PCL, P in that order
# So: $01F8 = P (status before NMI)
#     $01F9 = PC_lo (return address)
#     $01FA = PC_hi
# But that doesn't make sense with SP=$F5
# Let me trace from SP:

# Before NMI was taken, SP was at some higher value
# NMI pushes: PCH to SP, PCL to SP-1, P to SP-2, SP-=3
# After NMI push: SP = original_SP - 3

# Currently in NMI at $C140 (RTI = first instruction), SP=$F5
# So NMI pushed at original SP+0,SP-1,SP-2
# original_SP = $F5 + 3 = $F8
# NMI saved: PCH at $01F9, PCL at $01F8, P at $01F7? 
# Wait: pushes go to SP address then SP--, so:
# Push PCH: write to $01F8, SP becomes $F7
# Push PCL: write to $01F7, SP becomes $F6
# Push P: write to $01F6, SP becomes $F5

hw_stack_data = bytearray(256)
for i in range(0x100):
    hw_stack_data[i] = ram[0x100 + i]

original_sp_before_nmi = 0xF5 + 3  # $F8
print(f"Original SP before NMI: ${original_sp_before_nmi:02X}")
nmi_pch = hw_stack_data[0xF8]
nmi_pcl = hw_stack_data[0xF7]
nmi_p = hw_stack_data[0xF6]
nmi_return_pc = (nmi_pch << 8) | nmi_pcl
print(f"NMI pushed: PCH=${nmi_pch:02X} PCL=${nmi_pcl:02X} P=${nmi_p:02X}")
print(f"NMI return address: ${nmi_return_pc:04X}")

# Now trace below original SP ($F8) to see the C/dispatch call chain
print(f"\nStack below original SP=${original_sp_before_nmi:02X}:")
for i in range(0xF9, 0x100):
    print(f"  ${0x100+i:04X}: ${hw_stack_data[i]:02X}")

# decode the return chain below NMI
print(f"\nC/dispatch return chain (below NMI):")
idx = 0xF9
count = 0
while idx < 0xFF and count < 5:
    lo = hw_stack_data[idx]
    hi = hw_stack_data[idx+1]
    ret = ((hi << 8) | lo) + 1
    print(f"  ${0x100+idx:04X}: lo=${lo:02X} hi=${hi:02X} -> RTS to ${ret:04X}")
    idx += 2
    count += 1

# Now the key question: what was the NES doing before the NMI?
# NMI return address tells us
print(f"\n=== Code at NMI return address ${nmi_return_pc:04X} ===")

if nmi_return_pc >= 0xC000:
    bank = 31
elif nmi_return_pc >= 0x8000:
    # Need to know which bank was loaded - mapper.prgBank=$02
    bank = 2
elif nmi_return_pc >= 0x6000:
    # WRAM
    print(f"  [In WRAM - need to read from savestate WRAM]")
    wram_off = nmi_return_pc - 0x6000
    code = wram[wram_off:wram_off+16]
    print(f"  {' '.join(f'{b:02X}' for b in code)}")
    bank = None
else:
    print(f"  [In RAM or unmapped area]")
    bank = None

if bank is not None:
    off = rom_offset(bank, nmi_return_pc)
    code = rom[off:off+32]
    print(f"  Bank {bank}, offset ${nmi_return_pc & 0x3FFF:04X}")
    for i in range(0, 32, 16):
        hex_str = ' '.join(f'{b:02X}' for b in code[i:i+16])
        print(f"  ${nmi_return_pc+i:04X}: {hex_str}")

# Let's also check ___exit at $C098
print(f"\n=== Code at $C098 (___exit) ===")
off = rom_offset(31, 0xC098)
code = rom[off:off+32]
for i in range(0, 32, 16):
    hex_str = ' '.join(f'{b:02X}' for b in code[i:i+16])
    print(f"  ${0xC098+i:04X}: {hex_str}")

# Check what's at the ___exit return chain $C0D6
print(f"\n=== Code at $C0D6 (from return chain) ===")
off = rom_offset(31, 0xC0C0)
code = rom[off:off+48]
for i in range(0, 48, 16):
    hex_str = ' '.join(f'{b:02X}' for b in code[i:i+16])
    print(f"  ${0xC0C0+i:04X}: {hex_str}")

# Check $C659
print(f"\n=== Code at $C650-$C660 ===")
off = rom_offset(31, 0xC650)
code = rom[off:off+32]
for i in range(0, 32, 16):
    hex_str = ' '.join(f'{b:02X}' for b in code[i:i+16])
    print(f"  ${0xC650+i:04X}: {hex_str}")

# The actual key question: is the emulated program making progress?
# _pc=$2E5F is where we left off. The block at $2E58 finished and 
# set _pc=$2E5F. If the system is working, it will compile/interpret $2E5F next.
# 
# The savestate was taken during an NMI. The NMI return address tells us
# where the CPU was executing before the interrupt.

# Let's also check: what does $C098 (___exit loop) do?
# It should be the main loop: call run_6502, check result, repeat
print(f"\n=== Map file entries around $C0xx ===")
# Read a few lines from the map file
with open(r"C:\proj\c\NES\nesxidy-co\nesxidy\vicemap.map", "r") as f:
    for line in f:
        line = line.strip()
        if ':c0' in line.lower() or ':c1' in line.lower():
            parts = line.split()
            if len(parts) >= 3:
                addr_part = parts[1].split(':')[1] if ':' in parts[1] else parts[1]
                try:
                    addr = int(addr_part, 16)
                    if 0xC000 <= addr <= 0xC200:
                        print(f"  {line}")
                except:
                    pass

# Check all symbols in the map to understand the memory layout
print(f"\n=== Key symbols ===")
with open(r"C:\proj\c\NES\nesxidy-co\nesxidy\vicemap.map", "r") as f:
    for line in f:
        line = line.strip()
        for name in ['run_6502', 'dispatch_on_pc', 'compile', 'flash_dispatch', 'exit', 'main_loop', 'flash_cache']:
            if name in line.lower():
                print(f"  {line}")
                break

# Also check: the NMI vector should point to $C140
print(f"\n=== Interrupt vectors ===")
# Vectors at $FFFA/$FFFB (NMI), $FFFC/$FFFD (RESET), $FFFE/$FFFF (IRQ)
vec_off = rom_offset(31, 0xFFFA)
nmi_vec = rom[vec_off] | (rom[vec_off+1] << 8)
rst_vec = rom[vec_off+2] | (rom[vec_off+3] << 8)
irq_vec = rom[vec_off+4] | (rom[vec_off+5] << 8)
print(f"  NMI: ${nmi_vec:04X}")
print(f"  RST: ${rst_vec:04X}")
print(f"  IRQ: ${irq_vec:04X}")
