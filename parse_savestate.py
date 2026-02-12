#!/usr/bin/env python3
"""Parse Mesen2 savestate to find CPU registers and WRAM at crash time."""
import struct
import zlib

def parse_mesen_savestate(path):
    with open(path, "rb") as f:
        data = f.read()
    
    print(f"File size: {len(data)} bytes")
    print(f"Header: {data[:4]}")
    
    # Mesen2 .mss format:
    # 4 bytes: "MSS\x01" magic
    # Then sections, each potentially zlib compressed
    
    # Try to find zlib streams
    pos = 0
    sections = []
    while pos < len(data):
        # Look for zlib header (78 01, 78 9C, 78 DA, etc.)
        if pos + 2 < len(data) and data[pos] == 0x78:
            try:
                decompressed = zlib.decompress(data[pos:])
                print(f"  Found zlib stream at offset {pos} (0x{pos:X}), decompressed to {len(decompressed)} bytes")
                sections.append((pos, decompressed))
                # Can't easily know compressed size, skip to next
                # Try decompressing with different endpoints
                break
            except:
                pass
        pos += 1
    
    if not sections:
        # Try treating entire file (after header) as one zlib stream
        for start in range(4, min(256, len(data))):
            try:
                decompressed = zlib.decompress(data[start:])
                print(f"  Found zlib at offset {start}, decompressed to {len(decompressed)} bytes")
                sections.append((start, decompressed))
                break
            except:
                pass
    
    # The MSS format has multiple compressed blocks
    # Let me try a different approach - find ALL zlib blocks
    pos = 0
    all_decompressed = bytearray()
    zlib_blocks = []
    
    while pos < len(data) - 2:
        if data[pos] == 0x78 and data[pos+1] in (0x01, 0x5E, 0x9C, 0xDA):
            try:
                decomp_obj = zlib.decompressobj()
                result = decomp_obj.decompress(data[pos:])
                consumed = len(data[pos:]) - len(decomp_obj.unused_data)
                print(f"  Zlib block at 0x{pos:X}: {consumed} compressed -> {len(result)} decompressed")
                zlib_blocks.append((pos, result))
                all_decompressed.extend(result)
                pos += consumed
                continue
            except Exception as e:
                pass
        pos += 1
    
    print(f"\nTotal decompressed data: {len(all_decompressed)} bytes")
    
    # Now search the decompressed data for CPU state
    # NES CPU registers are typically: A, X, Y, SP, PC(lo), PC(hi), P(status)
    # WRAM is 8KB at $6000-$7FFF
    
    # Look for WRAM - it should be 8192 bytes
    # dispatch_on_pc starts at $61F4 in NES address space
    # In WRAM that's offset $01F4
    
    for block_idx, (offset, block_data) in enumerate(zlib_blocks):
        print(f"\n--- Block {block_idx} (from 0x{offset:X}, {len(block_data)} bytes) ---")
        
        # Check if this block could be WRAM (8KB)
        if len(block_data) >= 8192:
            # Check for dispatch_on_pc code at offset $01F4
            # The first instruction is LDA _pc+1
            # In machine code: LDA zp -> A5 6B
            wram_offset = 0x01F4
            if wram_offset + 2 < len(block_data):
                byte0 = block_data[wram_offset]
                byte1 = block_data[wram_offset + 1]
                print(f"  Checking WRAM offset $01F4: {byte0:02X} {byte1:02X} (expect A5 6B for LDA _pc+1)")
                if byte0 == 0xA5 and byte1 == 0x6B:
                    print("  *** FOUND WRAM with dispatch_on_pc! ***")
                    # Dump dispatch_on_pc
                    disp_start = wram_offset
                    disp_data = block_data[disp_start:disp_start+128]
                    for i in range(0, len(disp_data), 16):
                        addr = 0x6000 + disp_start + i
                        hex_str = ' '.join(f'{b:02X}' for b in disp_data[i:i+16])
                        print(f"  ${addr:04X}: {hex_str}")
                    
                    # Also check flash_dispatch_return at $6263
                    ret_offset = 0x0263
                    ret_data = block_data[ret_offset:ret_offset+16]
                    hex_str = ' '.join(f'{b:02X}' for b in ret_data)
                    print(f"\n  flash_dispatch_return at $6263: {hex_str}")
                    
                    ret_nr_offset = 0x0267
                    ret_nr_data = block_data[ret_nr_offset:ret_nr_offset+8]
                    hex_str = ' '.join(f'{b:02X}' for b in ret_nr_data)
                    print(f"  flash_dispatch_return_no_regs at $6267: {hex_str}")
        
        # Check for zero-page data (256 bytes)
        # _pc is at $6A-$6B, _sp at $6C, _a at $6D, _x at $6E, _y at $6F, _status at $70
        if len(block_data) >= 256 and len(block_data) < 512:
            print(f"  Small block ({len(block_data)} bytes) - could be zero page or CPU regs")
            hex_str = ' '.join(f'{b:02X}' for b in block_data[:min(128, len(block_data))])
            print(f"  First 128: {hex_str}")
        
        # Try to identify NES internal RAM (2KB at $0000-$07FF)
        if len(block_data) == 2048:
            print(f"  Exactly 2KB - could be NES internal RAM ($0000-$07FF)")
            # Zero page is at offset 0
            zp = block_data[:256]
            pc_lo = zp[0x6A] if len(zp) > 0x6B else 0
            pc_hi = zp[0x6B] if len(zp) > 0x6B else 0
            sp = zp[0x6C] if len(zp) > 0x6C else 0
            a_val = zp[0x6D] if len(zp) > 0x6D else 0
            x_val = zp[0x6E] if len(zp) > 0x6E else 0
            y_val = zp[0x6F] if len(zp) > 0x6F else 0
            status = zp[0x70] if len(zp) > 0x70 else 0
            print(f"  ZP $6A-$70 (emulated regs): PC=${pc_hi:02X}{pc_lo:02X} SP=${sp:02X} A=${a_val:02X} X=${x_val:02X} Y=${y_val:02X} P=${status:02X}")
            
            # Also check the hardware stack area ($0100-$01FF)
            hw_sp_area = block_data[0x100:0x200]
            print(f"  Hardware stack ($0100-$01FF):")
            for i in range(0, 256, 16):
                hex_str = ' '.join(f'{b:02X}' for b in hw_sp_area[i:i+16])
                print(f"    $01{i:02X}: {hex_str}")

    print("\n\nDone.")

if __name__ == "__main__":
    # Use the more recent savestate
    parse_mesen_savestate(r"C:\Users\membl\OneDrive\Documents\Mesen2\SaveStates\exidy_1.mss")
