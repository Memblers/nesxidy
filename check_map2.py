with open('vicemap.map', 'r') as f:
    lines = f.readlines()

symbols = []
for line in lines:
    line = line.strip()
    if line.startswith('al C:'):
        parts = line.split()
        addr_str = parts[1][2:]
        addr = int(addr_str, 16)
        name = parts[2] if len(parts) > 2 else '?'
        symbols.append((addr, name))

symbols.sort()

# Find symbols in render code area
print('=== Symbols $8000-$8130 (bank 20 render code area) ===')
for addr, name in symbols:
    if 0x8000 <= addr <= 0x8130:
        print(f'  ${addr:04X}: {name}')

# What's at $810D?
print()
print('=== Closest symbols to $810D ===')
below = [(a,n) for a,n in symbols if a <= 0x810D]
above = [(a,n) for a,n in symbols if a >= 0x810D]
if below:
    for a,n in below[-5:]:
        print(f'  ${a:04X}: {n}')
print(f'  --- $810D target ---')
if above:
    for a,n in above[:5]:
        print(f'  ${a:04X}: {n}')

# Find render_video and nearby
print()
print('=== render_video function area ($CA00-$CB50) ===')
for addr, name in symbols:
    if 0xCA00 <= addr <= 0xCB50:
        print(f'  ${addr:04X}: {name}')

# Check: how many symbols are at $8000?
print()
print('=== All symbols at exactly $8000 ===')
for addr, name in symbols:
    if addr == 0x8000:
        print(f'  ${addr:04X}: {name}')

# Check: how many symbols at $8086?
print()
print('=== All symbols at exactly $8086 ===')
for addr, name in symbols:
    if addr == 0x8086:
        print(f'  ${addr:04X}: {name}')
