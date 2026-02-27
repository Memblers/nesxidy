# Verify ROM data is still in bank 23
rom = open('exidy.nes', 'rb').read()
header = 16
bank_size = 0x4000

# rom_sidetrac should be at the start of bank 23
bank23_start = header + 23 * bank_size
data = rom[bank23_start:bank23_start+32]
print(f"Bank 23 start: {' '.join(f'{b:02X}' for b in data)}")

# From map file, _rom_sidetrac is at $8000 (start of bank)
# Check if this matches the side trac ROM signature
print(f"First bytes look like Exidy ROM data: {'yes' if data[0] != 0xFF and data[0] != 0x00 else 'no'}")

# Check bank 0 content 
bank0_start = header + 0 * bank_size
bank0_data = rom[bank0_start:bank0_start + bank_size]
# Count actual code (non-00, non-FF)
code_bytes = sum(1 for b in bank0_data if b != 0xFF and b != 0x00)
print(f"\nBank 0: {code_bytes} code bytes (non-00, non-FF)")

# Check what's at the start of bank 0
data = rom[bank0_start:bank0_start+32]
print(f"Bank 0 start: {' '.join(f'{b:02X}' for b in data)}")

# Check the interp functions are still in bank 0
# From vicemap.map: _irq6502 at $90BD, _nmi6502 at $906C
# These are in $8000-$BFFF range = swappable bank
# Wait, but these are at $9xxx which is in the $8000-$BFFF range
# Let me check vicemap.map for bank 0 functions
print("\nKey bank 0 function addresses from map:")
with open('vicemap.map', 'r') as f:
    for line in f:
        for kw in ['exec6502', 'step6502', 'interpret_6502', 'run_6502', 'irq6502', 'nmi6502']:
            if kw in line:
                print(f"  {line.strip()}")
                break
