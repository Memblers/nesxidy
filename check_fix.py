with open('millipede.nes', 'rb') as f:
    rom = f.read()

header = 16
bank31_off = header + 31 * 0x4000

# Check the JSR target at CB2F
off_cb2f = bank31_off + (0xCB2F - 0xC000)
b = rom[off_cb2f:off_cb2f+6]
print(f'CB2F bytes: {" ".join(f"{x:02X}" for x in b)}')
target = b[1] | (b[2] << 8) if b[0] == 0x20 else 0
print(f'JSR target: ${target:04X}')
print(f'Expected: $8403 (_metrics_dump_runtime_b2)')

# Verify $8403 in bank 20 
bank20_off = header + 20 * 0x4000
off_8403 = bank20_off + (0x8403 - 0x8000)
b2 = rom[off_8403:off_8403+8]
print()
print(f'Bank 20 at $8403: {" ".join(f"{x:02X}" for x in b2)}')
if b2[0] == 0x38:
    print('  -> SEC = function prologue, CORRECT!')
else:
    print(f'  -> 0x{b2[0]:02X} is NOT SEC')

# Also verify render_video_b2 at $8086
off_8086 = bank20_off + (0x8086 - 0x8000)
b3 = rom[off_8086:off_8086+8]
print()
print(f'Bank 20 at $8086 (render_video_b2): {" ".join(f"{x:02X}" for x in b3)}')

# Check the JSR for render_video_b2 call in render_video
# render_video is at $CA07. Look for JSR $8086 in fixed bank
found = False
for i in range(0x4000):
    if rom[bank31_off + i] == 0x20:
        lo = rom[bank31_off + i + 1]
        hi = rom[bank31_off + i + 2]
        addr = lo | (hi << 8)
        if addr == 0x8086:
            npc = 0xC000 + i
            print(f'  Found JSR $8086 at ${npc:04X}')
            found = True
if not found:
    print('  No JSR $8086 found in fixed bank')
