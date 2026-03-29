#!/usr/bin/env python3
"""Check DK ROM header and verify PRG layout."""

ROM_PATH = r"c:\proj\c\NES\nesxidy-co\nesxidy\nes_donkeykong.nes"

with open(ROM_PATH, 'rb') as f:
    data = f.read()

# iNES header
magic = data[0:4]
prg_16k = data[4]  # PRG ROM size in 16KB units
chr_8k = data[5]   # CHR ROM size in 8KB units
flags6 = data[6]
flags7 = data[7]

mapper = (flags6 >> 4) | (flags7 & 0xF0)
mirroring = "vertical" if (flags6 & 1) else "horizontal"
battery = bool(flags6 & 2)
trainer = bool(flags6 & 4)

prg_size = prg_16k * 16384
chr_size = chr_8k * 8192

print(f"=== iNES Header ===")
print(f"Magic: {magic}")
print(f"PRG ROM: {prg_16k} x 16KB = {prg_size} bytes")
print(f"CHR ROM: {chr_8k} x 8KB = {chr_size} bytes")
print(f"Mapper: {mapper}")
print(f"Mirroring: {mirroring}")
print(f"Battery: {battery}")
print(f"Trainer: {trainer}")

prg_offset = 16 + (512 if trainer else 0)
prg_data = data[prg_offset:prg_offset + prg_size]

if prg_16k == 1:
    # NROM-128: 16KB at $C000-$FFFF (mirrored at $8000-$BFFF)
    base_addr = 0xC000
    print(f"\nNROM-128: PRG mapped at $C000-$FFFF (mirrored $8000-$BFFF)")
elif prg_16k == 2:
    # NROM-256: 32KB at $8000-$FFFF
    base_addr = 0x8000
    print(f"\nNROM-256: PRG mapped at $8000-$FFFF")
else:
    base_addr = 0x8000
    print(f"\nUnexpected PRG size: {prg_16k} x 16KB")

# Check the interpreted PCs
INTERPRETED_PCS = [
    0xF4BE, 0xF52A, 0xF52D, 0xF52F, 0xF530, 0xFBF6, 0xF228, 0xFDDF,
    0xFE31, 0xFE34, 0xFE36, 0xFE39, 0xFE3B, 0xF519, 0xF51B, 0xF51E,
    0xF520, 0xF22B, 0xF22D, 0xF22F, 0xF231, 0xF233, 0xF236, 0xF23F,
    0xC1C3, 0xC1C6, 0xC1C9, 0xC1CC, 0xC1CE, 0xFBF9, 0xFBFB, 0xFBFD,
    0xFBFF, 0xFC01, 0xFC04, 0xFC06, 0xC47B, 0xC47D, 0xF4C1, 0xF4C3,
    0xF4C5, 0xFE2E, 0xF527,
]

print(f"\n=== Interpreted PCs (correct offset for base=${base_addr:04X}) ===")
print(f"{'PC':>6}  {'Offset':>6}  {'Opcode':>4}  {'Hex dump (8 bytes)'}")
print("-" * 60)

for pc in sorted(INTERPRETED_PCS):
    offset = pc - base_addr
    if 0 <= offset < len(prg_data):
        opcode = prg_data[offset]
        hexdump = " ".join(f"{prg_data[offset+j]:02X}" for j in range(min(8, len(prg_data)-offset)))
        print(f"${pc:04X}  ${offset:04X}    ${opcode:02X}    {hexdump}")
    else:
        print(f"${pc:04X}  OUT OF RANGE (offset ${offset:04X})")

# Also check vectors
print(f"\n=== Vectors ===")
vec_off = 0xFFFA - base_addr
if vec_off + 5 < len(prg_data):
    nmi = prg_data[vec_off] | (prg_data[vec_off+1] << 8)
    reset = prg_data[vec_off+2] | (prg_data[vec_off+3] << 8)
    irq = prg_data[vec_off+4] | (prg_data[vec_off+5] << 8)
    print(f"NMI:   ${nmi:04X}")
    print(f"RESET: ${reset:04X}")
    print(f"IRQ:   ${irq:04X}")

# Check what's around $F4BE area for context
print(f"\n=== Context around $F4BE ===")
for addr in [0xF4B0, 0xF520, 0xFBF0, 0xFDDA, 0xFE28]:
    offset = addr - base_addr
    if 0 <= offset < len(prg_data) - 16:
        hexdump = " ".join(f"{prg_data[offset+j]:02X}" for j in range(16))
        print(f"${addr:04X}: {hexdump}")
