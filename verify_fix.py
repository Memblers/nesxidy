"""Verify the fix: check that ir_node_size_table is in bank 1 ROM
and that CMP_IMM ($37) maps to size 2."""
with open("exidy.nes", "rb") as f:
    rom = f.read()

prg = rom[16:]

# Bank 1 PRG offset: 1 * 16384 = $4000
bank1 = prg[0x4000:0x8000]

# Search for the lookup table pattern in bank 1
# The table starts with: 0, 2, 2, 3, 2, 2, 3, 2, 2, 3, 2, 3, 2, 3, 2, 3
expected_start = bytes([0, 2, 2, 3, 2, 2, 3, 2, 2, 3, 2, 3, 2, 3, 2, 3])

pos = bank1.find(expected_start)
if pos == -1:
    print("Lookup table NOT found in bank 1!")
    # Try all banks
    for b in range(32):
        bank = prg[b*16384:(b+1)*16384]
        p = bank.find(expected_start)
        if p != -1:
            print(f"  Found in bank {b} at offset ${p:04X}")
            table = bank[p:p+116]
            print(f"  Table bytes: {table.hex(' ')}")
            print(f"  $37 (CMP_IMM): {table[0x37]} (should be 2)")
else:
    print(f"Lookup table found in bank 1 at offset ${pos:04X}")
    table = bank1[pos:pos+116]
    print(f"Table bytes ({len(table)} bytes):")
    for i in range(0, len(table), 16):
        hexs = ' '.join(f'{b:02X}' for b in table[i:i+16])
        print(f"  ${i:02X}: {hexs}")
    
    # Verify critical entries
    print(f"\nCritical checks:")
    print(f"  $37 CMP_IMM = {table[0x37]} (should be 2) {'OK' if table[0x37]==2 else 'FAIL!'}")
    print(f"  $32 ADC_IMM = {table[0x32]} (should be 2) {'OK' if table[0x32]==2 else 'FAIL!'}")
    print(f"  $39 CPY_IMM = {table[0x39]} (should be 2) {'OK' if table[0x39]==2 else 'FAIL!'}")
    print(f"  $47 CMP_ABS = {table[0x47]} (should be 3) {'OK' if table[0x47]==3 else 'FAIL!'}")
    print(f"  $4A INC_ZP  = {table[0x4A]} (should be 2) {'OK' if table[0x4A]==2 else 'FAIL!'}")
    print(f"  $56 ASL_A   = {table[0x56]} (should be 1) {'OK' if table[0x56]==1 else 'FAIL!'}")
    print(f"  $72 BIT_ZP  = {table[0x72]} (should be 2) {'OK' if table[0x72]==2 else 'FAIL!'}")
