#!/usr/bin/env python3
"""Parse Mesen2 savestate - find NES RAM, WRAM, and CPU state."""
import struct
import zlib

def parse_mesen_savestate(path):
    with open(path, "rb") as f:
        data = f.read()
    
    # Extract zlib blocks
    pos = 0
    zlib_blocks = []
    while pos < len(data) - 2:
        if data[pos] == 0x78 and data[pos+1] in (0x01, 0x5E, 0x9C, 0xDA):
            try:
                decomp_obj = zlib.decompressobj()
                result = decomp_obj.decompress(data[pos:])
                consumed = len(data[pos:]) - len(decomp_obj.unused_data)
                zlib_blocks.append((pos, bytearray(result)))
                pos += consumed
                continue
            except:
                pass
        pos += 1
    
    print(f"Found {len(zlib_blocks)} zlib blocks")
    for i, (off, blk) in enumerate(zlib_blocks):
        print(f"  Block {i}: offset=0x{off:X}, size={len(blk)} bytes")
    
    # Block 0 is 122880 bytes - that's 120KB
    # NES has: 2KB internal RAM + 8KB WRAM + possibly PPU + mapper state
    # Plus 512KB flash ROM (32 banks * 16KB)
    # 32 * 16384 = 524288 for full ROM, but that's too big
    # Maybe it's just the changed banks?
    
    # Let me search for the dispatch_on_pc code pattern in all blocks
    # Pattern: A5 6B (LDA _pc+1) at the start of dispatch_on_pc
    # Actually, let's search for the full pattern:
    # A5 6B 0A 26 xx 0A 26 xx (LDA $6B; ASL; ROL zp; ASL; ROL zp)
    
    for blk_idx, (off, blk) in enumerate(zlib_blocks):
        # Search for LDA $6B (A5 6B)
        search = bytes([0xA5, 0x6B, 0x0A])  # LDA $6B; ASL
        idx = 0
        while True:
            pos = blk.find(search, idx)
            if pos == -1:
                break
            # Check if this looks like dispatch_on_pc
            context = blk[pos:pos+32]
            hex_str = ' '.join(f'{b:02X}' for b in context)
            print(f"\n  Block {blk_idx} offset 0x{pos:X}: {hex_str}")
            
            # Check if the subsequent bytes match dispatch_on_pc pattern
            if len(context) > 5 and context[3] == 0x26:  # ROL zp
                print(f"  ^^^ Matches dispatch_on_pc pattern! ROL ${context[4]:02X}")
                
                # This is dispatch_on_pc. Print more context.
                full = blk[pos:pos+160]
                print(f"\n  Full dispatch_on_pc dump:")
                for i in range(0, len(full), 16):
                    hex_str = ' '.join(f'{b:02X}' for b in full[i:i+16])
                    # Calculate NES address: dispatch_on_pc is at $61F4
                    nes_addr = 0x61F4 + i
                    print(f"    ${nes_addr:04X}: {hex_str}")
                
                # WRAM starts 0x1F4 bytes before this point
                wram_start = pos - 0x01F4
                if wram_start >= 0:
                    print(f"\n  WRAM starts at block offset 0x{wram_start:X}")
                    
                    # Read zero page emulated regs
                    # But wait - ZP is in NES internal RAM ($0000-$00FF), not WRAM ($6000-$7FFF)
                    # The emulated regs are VBCC zero-page variables mapped to NES ZP
                    # They live in the actual NES zero page
                    
                    # Let me find NES internal RAM instead
                    # It's 2048 bytes. In the savestate, it might be before WRAM or after
                    
                    # Actually, the ZP variables ($6A-$70) are in NES hardware ZP
                    # which is part of the 2KB internal RAM
                    # Internal RAM is NOT in WRAM
                    
                    # Let me check flash_dispatch_return_no_regs at $6267
                    # That's WRAM offset $0267
                    fdr_offset = wram_start + 0x0267
                    fdr_data = blk[fdr_offset:fdr_offset+16]
                    hex_str = ' '.join(f'{b:02X}' for b in fdr_data)
                    print(f"  flash_dispatch_return_no_regs ($6267): {hex_str}")
                    # Expected: 68 85 70 A9 00 60 (PLA; STA $70; LDA #0; RTS)
                    expected = bytes([0x68, 0x85, 0x70, 0xA9, 0x00, 0x60])
                    if fdr_data[:6] == expected:
                        print(f"  ^^^ MATCHES expected code ✓")
                    else:
                        print(f"  ^^^ DOES NOT MATCH expected! Expected: {' '.join(f'{b:02X}' for b in expected)}")
                    
                    # Also check flash_dispatch_return at $6263
                    fdr2_offset = wram_start + 0x0263
                    fdr2_data = blk[fdr2_offset:fdr2_offset+16]
                    hex_str = ' '.join(f'{b:02X}' for b in fdr2_data)
                    print(f"  flash_dispatch_return ($6263): {hex_str}")
                    # Expected: 86 6E 84 6F (STX $6E; STY $6F) then falls through to no_regs
                    
            idx = pos + 1
    
    # Now search for NES internal RAM
    # Look for the 2KB block containing zero-page variables
    # _pc at $6A-$6B, _sp at $6C, etc.
    # In Mesen2, the internal RAM might be a separate section
    
    # Let me look at block structure more carefully
    # Block 0: 122880 bytes = 120 * 1024 = 120KB
    # This could be: something + WRAM(8KB) + some flash banks
    # 120KB = 8 banks * 16KB - 8KB? Or 7.5 banks?
    # 122880 / 16384 = 7.5 banks
    # Or: 2KB (internal RAM) + 8KB (WRAM) + 7 * 16KB (some banks) = 2048 + 8192 + 114688 = 124928 - no
    
    # Let me try: maybe block 0 IS the whole PRG ROM (or a subset)
    # And block 1 contains CPU/PPU/mapper state and RAM
    
    print("\n\n=== Searching block 1 for CPU state ===")
    blk1 = zlib_blocks[1][1]
    
    # Mesen2 savestate format uses sections with identifiers
    # Let me search for text markers
    for i in range(len(blk1) - 4):
        chunk = blk1[i:i+4]
        if all(32 <= b < 127 for b in chunk):
            text = chunk.decode('ascii')
            if text.isalpha():
                # Check surrounding context
                context = blk1[max(0,i-4):i+20]
                hex_str = ' '.join(f'{b:02X}' for b in context)
                if len(text) >= 3:
                    pass  # Too many matches, skip
    
    # Try a different approach: search for the A5 6B pattern in block 1 too
    search = bytes([0xA5, 0x6B, 0x0A])
    idx = 0
    while True:
        pos = blk1.find(search, idx)
        if pos == -1:
            break
        context = blk1[pos:pos+16]
        hex_str = ' '.join(f'{b:02X}' for b in context)
        print(f"  Block 1 offset 0x{pos:X}: {hex_str}")
        idx = pos + 1
    
    # Search for known WRAM addresses pattern
    # haltwait at the start of trampoline section: 4C 63 62 (JMP $6263)
    search2 = bytes([0x4C, 0x63, 0x62])
    idx = 0
    while True:
        pos = blk1.find(search2, idx)
        if pos == -1:
            break
        print(f"  Block 1: JMP $6263 at offset 0x{pos:X}")
        idx = pos + 1
    
    # Let me also search block 0
    blk0 = zlib_blocks[0][1]
    idx = 0
    while True:
        pos = blk0.find(search2, idx)
        if pos == -1:
            break
        # Only print first few matches
        if idx < 20:
            context = blk0[max(0,pos-4):pos+16]
            hex_str = ' '.join(f'{b:02X}' for b in context)
            print(f"  Block 0: JMP $6263 at offset 0x{pos:X}: {hex_str}")
        idx = pos + 1
    
    # Block 0 is 122880 bytes. Let me see if it starts with NES internal RAM.
    # NES internal RAM is 2KB ($0000-$07FF)
    # Then WRAM is 8KB ($6000-$7FFF)  
    # They're not contiguous in NES memory space
    
    # In Mesen2, the savestate might store memory regions sequentially
    # Internal RAM (2KB) first, then potentially WRAM (8KB)
    
    print("\n=== Block 0 layout analysis ===")
    # Check if first 2KB is NES internal RAM
    # ZP vars: _pc at $6A-$6B
    print(f"Block 0 first 256 bytes (potential ZP):")
    for i in range(0, 256, 16):
        hex_str = ' '.join(f'{b:02X}' for b in blk0[i:i+16])
        print(f"  ${i:04X}: {hex_str}")
    
    # Check ZP values
    if len(blk0) > 0x70:
        pc_lo = blk0[0x6A]
        pc_hi = blk0[0x6B]
        sp = blk0[0x6C]
        a_val = blk0[0x6D]
        x_val = blk0[0x6E]
        y_val = blk0[0x6F]
        status = blk0[0x70]
        print(f"\n  Emulated regs (if this is ZP):")
        print(f"  _pc = ${pc_hi:02X}{pc_lo:02X}, _sp = ${sp:02X}")
        print(f"  _a = ${a_val:02X}, _x = ${x_val:02X}, _y = ${y_val:02X}, _status = ${status:02X}")
    
    # Check hardware stack ($0100-$01FF)
    print(f"\n  Hardware stack area ($0100-$01FF):")
    for i in range(0x100, 0x200, 16):
        hex_str = ' '.join(f'{b:02X}' for b in blk0[i:i+16])
        print(f"  ${i:04X}: {hex_str}")

if __name__ == "__main__":
    parse_mesen_savestate(r"C:\Users\membl\OneDrive\Documents\Mesen2\SaveStates\exidy_1.mss")
