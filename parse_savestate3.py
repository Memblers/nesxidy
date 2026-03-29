#!/usr/bin/env python3
"""Find NES internal RAM and CPU state in Mesen2 savestate."""
import struct
import zlib

def main():
    with open(r"C:\Users\membl\OneDrive\Documents\Mesen2\SaveStates\exidy_1.mss", "rb") as f:
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
    
    blk1 = zlib_blocks[1][1]
    wram_start = 0xA393  # From previous analysis
    
    # WRAM is in block 1 starting at offset 0xA393 (8KB = $6000-$7FFF)
    wram = blk1[wram_start:wram_start+8192]
    
    # NES internal RAM (2KB) should be somewhere else in the savestate
    # In Mesen2, the CPU state and internal RAM are in specific sections
    # Let me search block 1 more systematically
    
    # The Mesen2 MSS format stores data in sections
    # Let me look at the raw structure around known offsets
    
    # We know WRAM is at offset 0xA393 in block 1
    # Internal RAM (2KB) might be right before it or elsewhere
    
    # Let me search for NES ZP by looking for memory that has our emulated reg values
    # at offsets $6A-$70. Since the system crashed, _pc should be something recognizable.
    
    # Actually let me look at what's before WRAM in block 1
    print("=== Memory before WRAM in block 1 ===")
    # Check if 2KB before WRAM is NES internal RAM
    nes_ram_start = wram_start - 2048
    if nes_ram_start >= 0:
        nes_ram = blk1[nes_ram_start:nes_ram_start+2048]
        print(f"2KB block before WRAM (offset 0x{nes_ram_start:X}):")
        print(f"  ZP $60-$7F:")
        zp = nes_ram[0x60:0x80]
        hex_str = ' '.join(f'{b:02X}' for b in zp)
        print(f"    {hex_str}")
        pc_lo = nes_ram[0x6A]
        pc_hi = nes_ram[0x6B]
        sp = nes_ram[0x6C]
        a_val = nes_ram[0x6D]
        x_val = nes_ram[0x6E]
        y_val = nes_ram[0x6F]
        status = nes_ram[0x70]
        print(f"  _pc=${pc_hi:02X}{pc_lo:02X} _sp=${sp:02X} _a=${a_val:02X} _x=${x_val:02X} _y=${y_val:02X} _status=${status:02X}")
    
    # Also try other potential offsets
    # Maybe NES RAM is stored at the very start of block 1
    print(f"\n  Block 1 offset 0 ZP $60-$7F:")
    zp0 = blk1[0x60:0x80]
    hex_str = ' '.join(f'{b:02X}' for b in zp0)
    print(f"    {hex_str}")
    print(f"  _pc=${blk1[0x6B]:02X}{blk1[0x6A]:02X} _sp=${blk1[0x6C]:02X} _a=${blk1[0x6D]:02X} _x=${blk1[0x6E]:02X} _y=${blk1[0x6F]:02X} _status=${blk1[0x70]:02X}")
    
    # Let me search more broadly. Mesen2 stores CPU registers as a struct, not in RAM.
    # The NES CPU registers (A, X, Y, SP, PC, P) are stored separately.
    # Let me search for distinctive patterns
    
    # Actually, let me search for the reboot detection code.
    # The C code checks: if (pc == 0x2800 && run_count > 1000) -> infinite loop
    # If the crash is a reboot, _pc might be $2800
    # Or it might be stuck in the for(;;) nop loop
    
    # Let me find the infinite loop code. It's: for(;;) { __asm("nop"); }
    # That would be: EA 4C xx xx (NOP; JMP self)
    # Search for NOP; JMP pattern
    search = bytes([0xEA, 0x4C])
    idx = 0
    nop_loops = []
    while True:
        pos = blk1.find(search, idx)
        if pos == -1:
            break
        # Check if JMP target points to self (the NOP)
        if pos + 4 < len(blk1):
            jmp_lo = blk1[pos+2]
            jmp_hi = blk1[pos+3]
            nop_loops.append((pos, jmp_lo, jmp_hi))
        idx = pos + 1
    
    print(f"\n  NOP+JMP patterns in block 1: {len(nop_loops)}")
    for pos, lo, hi in nop_loops[:20]:
        print(f"    offset 0x{pos:X}: NOP; JMP ${hi:02X}{lo:02X}")
    
    # Let me try to understand the block 1 layout by looking at its structure
    print(f"\n=== Block 1 structure analysis ===")
    print(f"  Total size: {len(blk1)} bytes")
    
    # Dump some interesting areas
    # Let me scan for regions that look like 6502 code vs data
    # WRAM at 0xA393 is 8KB, so it goes to 0xC393
    # What's after WRAM?
    after_wram = wram_start + 8192
    print(f"  After WRAM (offset 0x{after_wram:X}):")
    after_data = blk1[after_wram:after_wram+64]
    hex_str = ' '.join(f'{b:02X}' for b in after_data)
    print(f"    {hex_str}")
    
    # What's immediately before what we identified as WRAM?
    print(f"\n  Before WRAM (offset 0x{wram_start-64:X} to 0x{wram_start:X}):")
    before_data = blk1[wram_start-64:wram_start]
    hex_str = ' '.join(f'{b:02X}' for b in before_data)
    print(f"    {hex_str}")
    
    # Actually, Mesen2 stores things as: 
    # - Mapper state  
    # - CPU state (A, X, Y, SP, PC, flags as individual values)
    # - NES RAM (2KB)
    # - WRAM (8KB)
    # - CHR RAM
    # - etc.
    
    # But we found WRAM by searching for the dispatch_on_pc code pattern.
    # Let me try to find NES internal RAM by searching for a known pattern.
    # The emulated stack at $0100+ in NES RAM contains the emulated register values.
    # But we also have debug values written at $0100-$0101 (frame counter).
    
    # Better: search for the _haltwait code which is JMP $6263 at the start of trampoline
    # It's at some WRAM address before dispatch_on_pc
    
    # Let me look at the dispatch_on_pc dump more carefully
    print("\n=== Detailed dispatch_on_pc analysis ===")
    dispatch_data = wram[0x01F4:0x01F4+160]
    
    # At offset $6260 in WRAM (offset $0260 in wram array):
    # .dispatch_addr_instruction: JMP $xxxx (self-modifying)
    disp_instr_offset = 0x0260
    jmp_byte = wram[disp_instr_offset]
    jmp_lo = wram[disp_instr_offset + 1]
    jmp_hi = wram[disp_instr_offset + 2]
    print(f"  .dispatch_addr_instruction ($6260): {jmp_byte:02X} {jmp_lo:02X} {jmp_hi:02X}")
    print(f"  Self-modifying JMP target: ${jmp_hi:02X}{jmp_lo:02X}")
    
    # Check what .dispatch_addr (+1) is
    dispatch_addr_val = wram[0x0261] | (wram[0x0262] << 8)
    print(f"  .dispatch_addr value (self-mod target): ${dispatch_addr_val:04X}")
    
    # The $6254 area should have the JSR to .dispatch_addr_instruction
    print(f"\n  Code around JSR .dispatch ($6254-$6270):")
    for i in range(0x0254, 0x0278, 16):
        hex_str = ' '.join(f'{b:02X}' for b in wram[i:i+16])
        addr = 0x6000 + i
        print(f"    ${addr:04X}: {hex_str}")
    
    # Now let me trace what happens step by step
    print("\n=== WRAM dispatch_on_pc byte-by-byte decode ===")
    pc = 0x01F4  # WRAM offset for $61F4
    end = 0x0278
    while pc < end:
        b = wram[pc]
        addr = 0x6000 + pc
        if b == 0xA5:  # LDA zp
            print(f"  ${addr:04X}: LDA ${wram[pc+1]:02X}")
            pc += 2
        elif b == 0x85:  # STA zp
            print(f"  ${addr:04X}: STA ${wram[pc+1]:02X}")
            pc += 2
        elif b == 0xA6:  # LDX zp
            print(f"  ${addr:04X}: LDX ${wram[pc+1]:02X}")
            pc += 2
        elif b == 0xA4:  # LDY zp
            print(f"  ${addr:04X}: LDY ${wram[pc+1]:02X}")
            pc += 2
        elif b == 0x86:  # STX zp
            print(f"  ${addr:04X}: STX ${wram[pc+1]:02X}")
            pc += 2
        elif b == 0x84:  # STY zp
            print(f"  ${addr:04X}: STY ${wram[pc+1]:02X}")
            pc += 2
        elif b == 0x0A:  # ASL A
            print(f"  ${addr:04X}: ASL A")
            pc += 1
        elif b == 0x06:  # ASL zp
            print(f"  ${addr:04X}: ASL ${wram[pc+1]:02X}")
            pc += 2
        elif b == 0x26:  # ROL zp
            print(f"  ${addr:04X}: ROL ${wram[pc+1]:02X}")
            pc += 2
        elif b == 0x2A:  # ROL A
            print(f"  ${addr:04X}: ROL A")
            pc += 1
        elif b == 0x4A:  # LSR A
            print(f"  ${addr:04X}: LSR A")
            pc += 1
        elif b == 0x38:  # SEC
            print(f"  ${addr:04X}: SEC")
            pc += 1
        elif b == 0x18:  # CLC
            print(f"  ${addr:04X}: CLC")
            pc += 1
        elif b == 0x6A:  # ROR A
            print(f"  ${addr:04X}: ROR A")
            pc += 1
        elif b == 0x29:  # AND imm
            print(f"  ${addr:04X}: AND #${wram[pc+1]:02X}")
            pc += 2
        elif b == 0x09:  # ORA imm
            print(f"  ${addr:04X}: ORA #${wram[pc+1]:02X}")
            pc += 2
        elif b == 0x69:  # ADC imm
            print(f"  ${addr:04X}: ADC #${wram[pc+1]:02X}")
            pc += 2
        elif b == 0x8D:  # STA abs
            print(f"  ${addr:04X}: STA ${wram[pc+2]:02X}{wram[pc+1]:02X}")
            pc += 3
        elif b == 0xAD:  # LDA abs
            print(f"  ${addr:04X}: LDA ${wram[pc+2]:02X}{wram[pc+1]:02X}")
            pc += 3
        elif b == 0xA9:  # LDA imm
            print(f"  ${addr:04X}: LDA #${wram[pc+1]:02X}")
            pc += 2
        elif b == 0xF0:  # BEQ
            off = wram[pc+1]
            if off > 127: off -= 256
            target = addr + 2 + off
            print(f"  ${addr:04X}: BEQ ${target:04X} (offset {off})")
            pc += 2
        elif b == 0x30:  # BMI
            off = wram[pc+1]
            if off > 127: off -= 256
            target = addr + 2 + off
            print(f"  ${addr:04X}: BMI ${target:04X} (offset {off})")
            pc += 2
        elif b == 0xD0:  # BNE
            off = wram[pc+1]
            if off > 127: off -= 256
            target = addr + 2 + off
            print(f"  ${addr:04X}: BNE ${target:04X} (offset {off})")
            pc += 2
        elif b == 0xB1:  # LDA (zp),Y
            print(f"  ${addr:04X}: LDA (${wram[pc+1]:02X}),Y")
            pc += 2
        elif b == 0xA0:  # LDY imm
            print(f"  ${addr:04X}: LDY #${wram[pc+1]:02X}")
            pc += 2
        elif b == 0xC9:  # CMP imm
            print(f"  ${addr:04X}: CMP #${wram[pc+1]:02X}")
            pc += 2
        elif b == 0x48:  # PHA
            print(f"  ${addr:04X}: PHA")
            pc += 1
        elif b == 0x68:  # PLA
            print(f"  ${addr:04X}: PLA")
            pc += 1
        elif b == 0x28:  # PLP
            print(f"  ${addr:04X}: PLP")
            pc += 1
        elif b == 0x08:  # PHP
            print(f"  ${addr:04X}: PHP")
            pc += 1
        elif b == 0x20:  # JSR abs
            print(f"  ${addr:04X}: JSR ${wram[pc+2]:02X}{wram[pc+1]:02X}")
            pc += 3
        elif b == 0x4C:  # JMP abs
            print(f"  ${addr:04X}: JMP ${wram[pc+2]:02X}{wram[pc+1]:02X}")
            pc += 3
        elif b == 0x60:  # RTS
            print(f"  ${addr:04X}: RTS")
            pc += 1
        elif b == 0xC8:  # INY
            print(f"  ${addr:04X}: INY")
            pc += 1
        elif b == 0xEA:  # NOP
            print(f"  ${addr:04X}: NOP")
            pc += 1
        else:
            print(f"  ${addr:04X}: ??? ${b:02X}")
            pc += 1

if __name__ == "__main__":
    main()
