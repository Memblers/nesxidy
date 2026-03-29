#!/usr/bin/env python3
"""Parse Mesen2 savestate - extract CPU + ZP + stack."""
import zlib

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

# Find NES CPU registers
print("=== NES CPU Registers ===")
for name in ['cpu.a', 'cpu.x', 'cpu.y', 'cpu.sp', 'cpu.pc', 'cpu.ps']:
    marker = name.encode('ascii')
    p = blk1.find(marker)
    if p > 0 and blk1[p-1] == 0:
        after_null = p + len(marker) + 1
        sz = int.from_bytes(blk1[after_null:after_null+4], 'little')
        val = blk1[after_null+4:after_null+4+sz]
        val_int = int.from_bytes(val, 'little')
        if sz == 2:
            print(f"  {name}: ${val_int:04X}")
        else:
            print(f"  {name}: ${val_int:02X}")

# Find internal RAM
marker = b'memoryManager.internalRam'
p = blk1.find(marker)
after_null = p + len(marker) + 1
sz = int.from_bytes(blk1[after_null:after_null+4], 'little')
ram_start = after_null + 4
print(f"\n=== Internal RAM (size=${sz:X}) at offset 0x{ram_start:X} ===")

ram = blk1[ram_start:ram_start+sz]

print("\nZero page ($0060-$00B0):")
for i in range(0x60, 0xB0, 16):
    hex_str = ' '.join(f'{b:02X}' for b in ram[i:i+16])
    print(f"  ${i:04X}: {hex_str}")

pc_lo = ram[0x6A]
pc_hi = ram[0x6B]
sp_val = ram[0x6C]
a_val = ram[0x6D]
x_val = ram[0x6E]
y_val = ram[0x6F]
st_val = ram[0x70]

print(f"\nEmulated regs (ZP):")
print(f"  _pc    = ${pc_hi:02X}{pc_lo:02X}")
print(f"  _sp    = ${sp_val:02X}")
print(f"  _a     = ${a_val:02X}")
print(f"  _x     = ${x_val:02X}")
print(f"  _y     = ${y_val:02X}")
print(f"  _status= ${st_val:02X}")

# dispatch_on_pc ZP vars
print(f"\ndispatch_on_pc ZP vars:")
print(f"  addr_lo ($AA) = ${ram[0xAA]:02X}")
print(f"  addr_hi ($AB) = ${ram[0xAB]:02X}")
print(f"  temp    ($AC) = ${ram[0xAC]:02X}")
print(f"  temp2   ($AD) = ${ram[0xAD]:02X}")
print(f"  target_bank ($AE) = ${ram[0xAE]:02X}")

# Additional ZP (VBCC variables)
print(f"\nOther ZP vars:")
for off in range(0x80, 0xC0):
    if ram[off] != 0:
        print(f"  ${off:02X} = ${ram[off]:02X}")

print(f"\nHardware stack ($0100-$01FF):")
for i in range(0x100, 0x200, 16):
    hex_str = ' '.join(f'{b:02X}' for b in ram[i:i+16])
    # Mark non-zero entries
    has_data = any(b != 0 for b in ram[i:i+16])
    mark = " <--" if has_data else ""
    print(f"  ${i:04X}: {hex_str}{mark}")

# Emulated stack in WRAM (at NES $6E00 = RAM_BASE + $100)
# WRAM is at mapper.workRam
wram_marker = b'mapper.workRam'
wp = blk1.find(wram_marker)
wram_after = wp + len(wram_marker) + 1
wram_sz = int.from_bytes(blk1[wram_after:wram_after+4], 'little')
wram_data_start = wram_after + 4
wram = blk1[wram_data_start:wram_data_start+wram_sz]
print(f"\n=== WRAM (size=${wram_sz:X}) ===")

# Emulated stack at WRAM offset $0E00 ($6E00 - $6000)
print(f"\nEmulated stack (WRAM $6E00, emulated $0100):")
emu_stack_off = 0x0E00
for i in range(0, 0x100, 16):
    hex_str = ' '.join(f'{b:02X}' for b in wram[emu_stack_off+i:emu_stack_off+i+16])
    has_data = any(b != 0 for b in wram[emu_stack_off+i:emu_stack_off+i+16])
    mark = " <--" if has_data else ""
    sp_mark = ""
    if sp_val >= i and sp_val < i + 16:
        sp_mark = f" [SP=${sp_val:02X} here]"
    print(f"  ${0x0100+i:04X}: {hex_str}{mark}{sp_mark}")

# Mapper state
print(f"\n=== Mapper state ===")
mapper_prg_marker = b'mapper.prgBank'
mp = blk1.find(mapper_prg_marker)
if mp > 0 and blk1[mp-1] == 0:
    ma = mp + len(mapper_prg_marker) + 1
    msz = int.from_bytes(blk1[ma:ma+4], 'little')
    mval = int.from_bytes(blk1[ma+4:ma+4+msz], 'little')
    print(f"  mapper.prgBank = ${mval:02X}")

# Debug variables in WRAM
# reboot_detected is a volatile uint8_t
# Let's check WRAM for the debug area
# The debug frame counter at $0100-$0101 
print(f"\n=== Debug area (NES $0100-$0101) ===")
print(f"  Frame counter: ${ram[0x100]:02X}{ram[0x101]:02X}")
