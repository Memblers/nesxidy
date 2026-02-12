#!/usr/bin/env python3
"""Analyze flash state from NES ROM file to debug dispatch issues."""

import sys

NES_HEADER = 16
BANK_SIZE = 0x4000  # 16KB
BANK_BASE = 0x8000  # $8000 in NES address space

# Bank layout
BANK_CODE = 3          # First code bank
BANK_PC = 19           # PC lookup starts
BANK_PC_FLAGS = 27     # PC flags starts
BANK_FLASH_BLOCK_FLAGS = 2
FLASH_BANK_MASK = 0x3FFF

BLOCK_SIZE = 256
BLOCKS_PER_BANK = 64   # 16384 / 256

def bank_offset(bank):
    """File offset for start of bank N."""
    return NES_HEADER + bank * BANK_SIZE

def read_bank(rom, bank):
    """Read a full 16KB bank from ROM."""
    off = bank_offset(bank)
    return rom[off:off+BANK_SIZE]

def get_pc_flag(rom, emulated_pc):
    """Get the flag byte for an emulated PC."""
    bank = (emulated_pc >> 14) + BANK_PC_FLAGS
    addr = emulated_pc & FLASH_BANK_MASK
    off = bank_offset(bank) + addr
    return rom[off]

def get_pc_native_addr(rom, emulated_pc):
    """Get the native address (2 bytes) for an emulated PC."""
    bank = (emulated_pc >> 13) + BANK_PC
    addr = (emulated_pc << 1) & FLASH_BANK_MASK
    off = bank_offset(bank) + addr
    lo = rom[off]
    hi = rom[off+1]
    return lo | (hi << 8)

def get_block_flags(rom):
    """Read block flags from bank 2."""
    return read_bank(rom, BANK_FLASH_BLOCK_FLAGS)

def main():
    with open(r"c:\proj\c\NES\nesxidy-co\nesxidy\exidy.nes", "rb") as f:
        rom = bytearray(f.read())
    
    print(f"ROM size: {len(rom)} bytes ({len(rom)//1024}KB)")
    print(f"Banks: {(len(rom) - NES_HEADER) // BANK_SIZE}")
    print()
    
    # === 1. Find compiled blocks ===
    block_flags = get_block_flags(rom)
    compiled_blocks = []
    for i in range(min(len(block_flags), 1792)):  # 28 banks * 64 blocks max
        # FLASH_AVAILABLE bit is set when block is free
        # Block is compiled when FLASH_AVAILABLE is cleared
        flag = block_flags[i]
        if flag != 0xFF:  # Any bit cleared = block used
            compiled_blocks.append((i, flag))
    
    print(f"=== Compiled blocks ({len(compiled_blocks)}) ===")
    for idx, flag in compiled_blocks:
        code_bank = (idx // 64) + BANK_CODE
        code_addr = ((idx % 64) << 8) + BANK_BASE
        print(f"  Block #{idx}: flag=0x{flag:02X}, code bank={code_bank}, addr=0x{code_addr:04X}")
        
        # Read the block data
        block_off = bank_offset(code_bank) + (code_addr - BANK_BASE)
        block_data = rom[block_off:block_off+BLOCK_SIZE]
        
        # Show first 48 bytes and last 32 bytes
        hex_line = ' '.join(f'{b:02X}' for b in block_data[:48])
        print(f"    First 48: {hex_line}")
        
        # Check byte 255 for epilogue offset
        epi_off = block_data[255]
        if epi_off != 0xFF:
            print(f"    Epilogue offset (byte 255): {epi_off}")
            # Show epilogue
            epi_data = block_data[epi_off:epi_off+21]
            hex_epi = ' '.join(f'{b:02X}' for b in epi_data)
            print(f"    Epilogue ({epi_off}-{epi_off+20}): {hex_epi}")
            # Parse exit_pc from epilogue (patchable: offset+11 = lo, offset+15 = hi)
            if len(epi_data) >= 16:
                exit_lo = epi_data[11]
                exit_hi = epi_data[15]
                print(f"    Exit PC: 0x{exit_hi:02X}{exit_lo:02X}")
        print()
    
    # === 2. Scan PC flags for all compiled PCs ===
    print("=== PC flags scan (non-$FF entries) ===")
    compiled_pcs = []
    for pc_val in range(0x2800, 0x4000):  # Sidetrac ROM range
        flag = get_pc_flag(rom, pc_val)
        if flag != 0xFF:
            native = get_pc_native_addr(rom, pc_val)
            compiled_pcs.append((pc_val, flag, native))
    
    print(f"  Found {len(compiled_pcs)} PC entries in $2800-$3FFF")
    for pc_val, flag, native in compiled_pcs:
        flag_desc = ""
        if flag == 0x00:
            flag_desc = "UNINITIALIZED?"
        elif flag & 0x80:
            if flag & 0x40:
                flag_desc = "ERASED ($FF shouldn't be here)"
            else:
                flag_desc = f"INTERPRETED (flag=0x{flag:02X})"
        else:
            bank = flag & 0x1F
            flag_desc = f"COMPILED bank={bank}"
        print(f"  PC=0x{pc_val:04X}: flag=0x{flag:02X} ({flag_desc}), native_addr=0x{native:04X}")
    
    # === 3. Check specific PCs that we know about from debugging ===
    print()
    print("=== Key PC checks ===")
    for test_pc in [0x2E0B, 0x2E0D, 0x2E0E, 0x2E58, 0x2E5C, 0x2800, 0x331D]:
        flag = get_pc_flag(rom, test_pc)
        native = get_pc_native_addr(rom, test_pc)
        print(f"  PC=0x{test_pc:04X}: flag=0x{flag:02X}, native=0x{native:04X}")
    
    # === 4. Check dispatch_on_pc address computation for a sample PC ===
    print()
    print("=== Address computation verification ===")
    for test_pc in [0x2E58, 0x2E0B]:
        # Flag lookup
        flag_bank = (test_pc >> 14) + BANK_PC_FLAGS
        flag_addr = test_pc & FLASH_BANK_MASK
        # PC lookup  
        pc_bank = (test_pc >> 13) + BANK_PC
        pc_addr = (test_pc << 1) & FLASH_BANK_MASK
        print(f"  PC=0x{test_pc:04X}:")
        print(f"    Flag: bank={flag_bank}, addr=0x{flag_addr:04X} (ROM offset=0x{bank_offset(flag_bank)+flag_addr:06X})")
        print(f"    PC:   bank={pc_bank}, addr=0x{pc_addr:04X} (ROM offset=0x{bank_offset(pc_bank)+pc_addr:06X})")

    # === 5. Look at the dispatch_on_pc ASM computation in detail ===
    # The ASM does bit manipulation differently, let's simulate it
    print()
    print("=== ASM dispatch_on_pc simulation ===")
    for test_pc in [0x2E58, 0x2E0B]:
        pc_lo = test_pc & 0xFF
        pc_hi = (test_pc >> 8) & 0xFF
        
        # Flag address computation
        temp = 0
        a = pc_hi
        a = (a << 1) & 0xFF
        temp = (temp << 1) | (1 if (pc_hi & 0x80) else 0)  # ROL temp
        a_after_first_asl = a
        a = (a << 1) & 0xFF
        carry_out = 1 if (a_after_first_asl & 0x80) else 0
        temp = (temp << 1) | carry_out  # ROL temp
        temp2 = a
        
        # LSR then SEC ROR for addr_hi
        a_lsr = a >> 1
        # SEC; ROR = set carry, rotate right through carry
        # Actually: SEC sets C=1, then ROR A: bit7=C=1, shift right, C=old_bit0
        addr_hi = (a_lsr >> 1) | 0x80
        
        addr_lo = pc_lo
        
        # Bank for flags
        flag_bank_asm = (temp & 0x03) + BANK_PC_FLAGS
        
        print(f"  PC=0x{test_pc:04X} (lo=0x{pc_lo:02X}, hi=0x{pc_hi:02X}):")
        print(f"    After ASL ASL: temp=0x{temp:02X}, temp2=0x{temp2:02X}, a=0x{a:02X}")
        print(f"    addr_hi=0x{addr_hi:02X}, addr_lo=0x{addr_lo:02X}")
        print(f"    Flag bank (ASM): {flag_bank_asm}")
        print(f"    Flag bank (C):   {(test_pc >> 14) + BANK_PC_FLAGS}")
        print(f"    Flag addr (ASM): 0x{addr_hi:02X}{addr_lo:02X}")
        print(f"    Flag addr (C):   0x{(test_pc & FLASH_BANK_MASK):04X}")
        
        # Now PC remap address
        # asl temp2
        temp2_asl = (temp2 << 1) & 0xFF
        carry_from_temp2 = 1 if (temp2 & 0x80) else 0
        # rol temp (but temp was modified... let me re-read the ASM more carefully)
        # Actually in ASM, temp still has its value from before
        a2 = temp
        a2 = ((a2 << 1) | carry_from_temp2) & 0xFF  # ROL
        a2 = a2 & 0x07
        pc_bank_asm = a2 + BANK_PC
        
        # addr_lo = asl addr_lo (which was pc_lo)
        pc_addr_lo = (pc_lo << 1) & 0xFF
        carry_from_lo = 1 if (pc_lo & 0x80) else 0
        # ROL pc_hi, AND #$3F, ORA #$80
        a3 = pc_hi
        a3 = ((a3 << 1) | carry_from_lo) & 0xFF  # ROL
        a3 = a3 & 0x3F
        a3 = a3 | 0x80
        pc_addr_hi = a3
        
        print(f"    PC remap bank (ASM): {pc_bank_asm}")
        print(f"    PC remap bank (C):   {(test_pc >> 13) + BANK_PC}")
        print(f"    PC remap addr (ASM): 0x{pc_addr_hi:02X}{pc_addr_lo:02X}")
        print(f"    PC remap addr (C):   0x{((test_pc << 1) & FLASH_BANK_MASK) | 0x8000:04X}")

if __name__ == "__main__":
    main()
