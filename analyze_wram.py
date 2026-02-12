#!/usr/bin/env python3
"""Analyze WRAM and savestate to find crash state."""
import struct

NES_HEADER = 16
BANK_SIZE = 0x4000

def main():
    with open(r"c:\proj\c\NES\nesxidy-co\nesxidy\exidy.nes", "rb") as f:
        rom = bytearray(f.read())
    
    # The code in the PC flags looks correct in flash.
    # PC=0x2E0B: flag=0x04, native=0x9E00
    # PC=0x2E58: flag=0x04, native=0x9F00
    # Both blocks are in bank 4, and the code bytes match expected JSR templates.
    
    # Now let's look at what the WRAM code looks like.
    # WRAM is at NES $6000-$7FFF, which is a separate 8KB RAM chip.
    # The "data" section goes to WRAM.
    # dispatch_on_pc is in section "data" so it's in WRAM.
    
    # From vicemap.map, dispatch_on_pc is at the address shown there.
    # Let's check the .map file
    with open(r"c:\proj\c\NES\nesxidy-co\nesxidy\vicemap.map", "r") as f:
        for line in f:
            if 'dispatch_on_pc' in line or 'flash_dispatch_return' in line:
                print(line.strip())
    
    print()
    
    # Now let's look at the Mesen savestate to find the CPU registers at crash
    # Mesen savestates are .mss files (compressed)
    import gzip
    
    for ss_name in ["exidy_1.mss", "exidy_11.mss"]:
        ss_path = rf"C:\Users\membl\OneDrive\Documents\Mesen2\SaveStates\{ss_name}"
        try:
            with open(ss_path, "rb") as f:
                raw = f.read()
            print(f"=== Savestate: {ss_name} ({len(raw)} bytes) ===")
            
            # Try to decompress (Mesen uses different formats)
            try:
                data = gzip.decompress(raw)
                print(f"  Decompressed: {len(data)} bytes")
            except:
                data = raw
                print(f"  Not gzip compressed, using raw")
            
            # Search for CPU state markers
            # Mesen2 savestates use a binary format with section headers
            # Look for known patterns
            
            # Dump first 256 bytes to understand format
            for i in range(0, min(256, len(data)), 16):
                hex_str = ' '.join(f'{b:02X}' for b in data[i:i+16])
                ascii_str = ''.join(chr(b) if 32<=b<127 else '.' for b in data[i:i+16])
                print(f"  {i:04X}: {hex_str}  {ascii_str}")
            
            # Search for "NES" or "CPU" markers
            for pattern in [b"NES", b"CPU", b"PPU", b"WRAM", b"PRG", b"Cpu"]:
                idx = 0
                while True:
                    pos = data.find(pattern, idx)
                    if pos == -1:
                        break
                    context = data[pos:pos+32]
                    hex_ctx = ' '.join(f'{b:02X}' for b in context)
                    print(f"  Found '{pattern.decode()}' at offset {pos} (0x{pos:X}): {hex_ctx}")
                    idx = pos + 1
                    if idx - pos > 100:  # limit matches
                        break
            
            print()
        except Exception as e:
            print(f"  Error reading {ss_name}: {e}")
            print()

    # Let's also check: what does the block at bank 4 offset $9E00 look like
    # when accessed through the flash?
    # The dispatch_on_pc code for PC=$2E58 does:
    # 1. Read flag from bank 27, address $2E58 -> gets 0x04 (bank 4) ✓
    # 2. Read native addr from bank 20, address $1CB0 -> should get $9F00
    # Let's verify the native addr in bank 20
    off = NES_HEADER + 20 * BANK_SIZE + 0x1CB0
    lo = rom[off]
    hi = rom[off+1]
    native = lo | (hi << 8)
    print(f"Bank 20, offset $1CB0: native_addr = ${native:04X} (lo=${lo:02X} hi=${hi:02X})")
    print(f"  Expected: $9F00")
    print(f"  Match: {native == 0x9F00}")
    
    print()
    
    # Now the critical question: after dispatch_on_pc returns 0 from the first
    # JSR block, pc has been set to $2E58 by the JSR template.
    # run_6502 returns. Main loop calls run_6502 again.
    # dispatch_on_pc is called with _pc=$2E58.
    # It should find flag=0x04, native=$9F00, switch to bank 4, jump to $9F00.
    #
    # BUT WAIT - there's a timing issue. The JSR template is the FIRST block
    # compiled. After it's compiled and dispatched, it executes and sets _pc=$2E58.
    # Then run_6502 returns. Main loop calls run_6502 again.
    # dispatch_on_pc reads flag for $2E58 -> this was JUST written during
    # compilation. Was it written correctly?
    #
    # Let me check: during compilation of the first block (entry PC=$2E0B),
    # the compile loop processes the JSR at $2E0B. The JSR handler does:
    #   cache_flag &= ~READY_FOR_NEXT (end block)
    #   return cache_flag
    # Then the compile loop falls out (READY_FOR_NEXT not set).
    # exit_pc = pc = $2E0E (JSR advances pc by 3)
    # 
    # The compile loop wrote flash_cache_pc_update for $2E0B with the JSR template
    # native code offset. But $2E0E (which is the exit_pc in the epilogue) was 
    # NEVER compiled - it's just the epilogue exit.
    #
    # After the JSR template executes, _pc = $2E58 (set by the template).
    # Next call to dispatch_on_pc: reads flag for $2E58 -> $FF (never compiled!)
    # Returns 1 -> recompile.
    # Compiles $2E58 block. Writes flag=0x04 for $2E58, native=$9F00.
    # Then pc = entry_pc = $2E58, dispatch_on_pc() runs the new block.
    #
    # THIS SHOULD WORK. The flash data shows it was compiled correctly.
    #
    # Unless... the problem is in the second dispatch after compilation.
    # After compilation, bankswitch_prg was called to BANK_FLASH_BLOCK_FLAGS (2).
    # Then dispatch_on_pc is called. It writes directly to $C000 for bank switching.
    # But does mapper_prg_bank track this? No - dispatch_on_pc uses raw $C000 writes.
    # After dispatch returns, mapper_prg_bank is stale. But that's OK because
    # the C code always calls bankswitch_prg() before accessing banks.
    
    # What if the problem is in the WRAM code itself being corrupted?
    # The WRAM is initialized from the ROM's "data" section at boot.
    # Let me check what section "data" maps to in the ROM.
    
    # From the linker map, we can find where the data section is.
    # But we can also look at WRAM in the savestate if we can parse it.
    
    # Actually, let me look at what read6502 does during compilation.
    # read6502(pc+1) and read6502(pc+2) read the JSR operand.
    # read6502 calls bankswitch_prg(1) to read ROM data, then bankswitch_prg(0).
    # But during compilation, the flash banks need to be active for flash_byte_program!
    # 
    # Wait - look at flash_byte_program. It takes the bank as a parameter.
    # It does bankswitch_prg(bank) internally.
    # And flash_cache_pc_update calls flash_byte_program with pc_jump_bank.
    # 
    # The setup_flash_address() sets: flash_code_bank, flash_code_address,
    # pc_jump_bank, pc_jump_address, pc_jump_flag_bank, pc_jump_flag_address
    #
    # The compile loop calls setup_flash_address(pc_old, flash_cache_index) 
    # for EACH instruction. This updates ALL the address variables.
    #
    # But for JSR, the handler reads from the ROM (read6502), which does
    # bankswitch_prg(1) then bankswitch_prg(0). This changes mapper_prg_bank.
    # Then when we return from recompile_opcode(), the compile loop calls
    # setup_flash_address(pc_old, flash_cache_index) which resets the addresses.
    # Then it calls flash_cache_pc_update() which writes to the correct addresses.
    # So this should be fine.
    
    print("=== HYPOTHESIS CHECK ===")
    print()
    print("All flash data looks correct:")
    print("  PC=$2E0B: flag=0x04, native=$9E00 (JSR template, 34 bytes)")
    print("  PC=$2E58: flag=0x04, native=$9F00 (LDA+STA+JSR template, 39 bytes)")
    print("  Block epilogues present with correct exit PCs")
    print()
    print("Possible issues:")
    print("1. WRAM code (dispatch_on_pc) may differ from ROM if not reloaded")
    print("2. NMI could fire during dispatch and corrupt bank register")
    print("3. Hardware stack imbalance during JSR dispatch chain")
    print()
    
    # Let me check the hardware stack balance for the JSR template path:
    # 
    # dispatch_on_pc is called via C: JSR _dispatch_on_pc
    #   - This pushes return addr onto NES hardware stack (2 bytes)
    # Inside dispatch_on_pc:
    #   - pha (pushes status) -> +1
    #   - jsr .dispatch_addr_instruction -> +2 (pushes return addr)
    #   - .dispatch_addr_instruction: jmp $9E00 (no stack change)
    #   - At $9E00 (JSR template):
    #     - PHP -> +1
    #     - ... template code (no RTS/JMP that returns) ...
    #     - JMP $6267 (flash_dispatch_return_no_regs)
    #   - At $6267:
    #     - PLA -> -1 (pulls the status pushed by PHP in template? NO!)
    #
    # WAIT. Let me trace this more carefully.
    #
    # Stack state entering dispatch_on_pc:
    #   [return to C caller]  (2 bytes from JSR _dispatch_on_pc)
    #
    # dispatch_on_pc:
    #   lda _status
    #   ora #$04
    #   pha          <- push status (+1, total: 3 bytes on stack)
    #   ...
    #   lda _a; ldx _x; ldy _y; plp  <- PLP pops the status (-1, total: 2 bytes)
    #   jsr .dispatch_addr_instruction  <- pushes ret addr (+2, total: 4 bytes)
    #     jmp $9E00  <- no stack change (total: 4 bytes)
    #
    # At $9E00 (JSR template):
    #   PHP          <- +1 (total: 5 bytes)
    #   STA _a       <- no change
    #   STX _x       <- no change
    #   STY _y       <- no change
    #   LDX _sp      <- no change
    #   ... push ret addr to emulated stack, set _pc ...
    #   JMP $6267    <- no change (total: 5 bytes)
    #
    # At $6267 (flash_dispatch_return_no_regs):
    #   PLA          <- -1 (total: 4 bytes)
    #   STA _status
    #   LDA #0
    #   RTS          <- -2 (total: 2 bytes) - returns to after JSR in dispatch_on_pc
    #
    # Back in dispatch_on_pc after the jsr .dispatch_addr_instruction:
    #   rts          <- -2 (total: 0 bytes) - returns to C caller
    #
    # Stack is BALANCED! ✓
    
    print("Hardware stack balance through JSR template: BALANCED ✓")
    print()
    
    # But wait - there's also the PHP in the JSR template and PLA in
    # flash_dispatch_return_no_regs. The PHP saves the CPU flags (including
    # the emulated status), and PLA restores it to _status.
    # But the plp in dispatch_on_pc already restored the flags from _status.
    # So PHP saves the current CPU flags (which include the emulated status).
    # Then flash_dispatch_return_no_regs does PLA to get that saved byte back
    # into _status. This is correct - it preserves the P register through
    # the JSR template execution.
    
    # Actually, let me re-check. In dispatch_on_pc:
    #   pha  <- pushes _status | 0x04
    #   ...
    #   plp  <- pops that value into P register
    #   jsr .dispatch  <- pushes ret addr
    #     jmp code     <- runs JSR template
    #   
    # JSR template:
    #   PHP  <- pushes current P (which was restored from _status) ← THIS IS ON TOP
    #   ... rest of template ...
    #   JMP flash_dispatch_return_no_regs
    #
    # flash_dispatch_return_no_regs:
    #   PLA  <- pulls the PHP value (the saved P register)
    #   STA _status  <- saves it back
    #   LDA #0
    #   RTS  <- returns to dispatch_on_pc after the JSR

    # Wait, but there's a JSR return address between the PHP and PLA!
    # Stack after PLP: [C return addr]
    # After JSR .dispatch: [C return addr] [dispatch ret addr]
    # Inside JSR template after PHP: [C return addr] [dispatch ret addr] [P flags]
    # JMP to flash_dispatch_return_no_regs
    # PLA: pulls P flags (-1): [C return addr] [dispatch ret addr]  ← CORRECT
    # RTS: pulls dispatch ret addr (-2): [C return addr]  ← CORRECT
    # Back in dispatch_on_pc, RTS: pulls C return addr (-2): [] ← CORRECT
    
    print("Stack trace detailed:")
    print("  [C ret]                    - from JSR _dispatch_on_pc")
    print("  [C ret][status]            - after PHA in dispatch_on_pc")
    print("  [C ret]                    - after PLP (pops status to P)")
    print("  [C ret][disp_ret]          - after JSR .dispatch_addr_instruction")
    print("  [C ret][disp_ret][P_flags] - after PHP in JSR template")
    print("  ... template runs, JMP flash_dispatch_return_no_regs ...")
    print("  [C ret][disp_ret]          - after PLA in return handler")
    print("  [C ret]                    - after RTS (returns from JSR)")
    print("  []                         - after RTS in dispatch_on_pc")
    print("  BALANCED ✓")

if __name__ == "__main__":
    main()
