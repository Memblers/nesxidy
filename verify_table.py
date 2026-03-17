# Verify the lookup table
entries = [
    0,  # $00
    2,2,3, 2,2,3, 2,2,3,  # $01-$09: LDA/LDX/LDY IMM/ZP/ABS
    2,3, 2,3, 2,3,  # $0A-$0F: STA/STX/STY ZP/ABS
    3,3,  # $10-$11: JMP/JSR
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,  # $12-$1F: implied
    2,2,2,2,2,2,2,2,  # $20-$27: branches
    1,1,1,1,1,1,1,1,1,1,  # $28-$31: transfers+INX/INY/DEX/DEY
    2,2,2,2,2,2,2,2,  # $32-$39: ALU IMM
    2,2,2,2,2,2,2,2,  # $3A-$41: ALU ZP
    3,3,3,3,3,3,3,3,  # $42-$49: ALU ABS + CPX/CPY ABS
    2,2,2,2,2,2,  # $4A-$4F: RMW ZP
    3,3,3,3,3,3,  # $50-$55: RMW ABS
    1,1,1,1,  # $56-$59: accumulator
    3,3,3,3, 3,3,3,3,3,3,  # $5A-$63: ABSX/ABSY
    3,3,3,3,3,3,  # $64-$69: ABSY
    3,3,  # $6A-$6B: LDX_ABSY, LDY_ABSX
    3,3,3,3,3,3,  # $6C-$71: RMW ABSX
    2,3,  # $72-$73: BIT ZP/ABS
]
print(f"Total entries: {len(entries)} (expected 116 for $00-$73)")
print(f"Hex range: $00-${len(entries)-1:02X}")
print(f"$37 CMP_IMM = {entries[0x37]} (should be 2)")
print(f"$32 ADC_IMM = {entries[0x32]} (should be 2)")
print(f"$39 CPY_IMM = {entries[0x39]} (should be 2)")
print(f"$47 CMP_ABS = {entries[0x47]} (should be 3)")
print(f"$48 CPX_ABS = {entries[0x48]} (should be 3)")
print(f"$49 CPY_ABS = {entries[0x49]} (should be 3)")
print(f"$4A INC_ZP = {entries[0x4A]} (should be 2)")
print(f"$4F ROR_ZP = {entries[0x4F]} (should be 2)")
print(f"$50 INC_ABS = {entries[0x50]} (should be 3)")
print(f"$56 ASL_A = {entries[0x56]} (should be 1)")
print(f"$72 BIT_ZP = {entries[0x72]} (should be 2)")
print(f"$73 BIT_ABS = {entries[0x73]} (should be 3)")
