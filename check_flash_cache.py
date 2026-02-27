#!/usr/bin/env python3
"""
check_flash_cache.py — Examine the JIT flash cache in exidy.nes
 
NES ROM layout (mapper 30, UNROM-512):
  - 16-byte iNES header
  - 32 x 16KB PRG banks ($0000-$3FFF each)
  
Flash cache layout:
  BANK_CODE = 4     (banks 4..17 = 14 banks of code cache, 4 sectors/bank)
  BANK_PC = 19      (banks 19..26 = 8 banks, 2 bytes per guest PC → native addr)
  BANK_PC_FLAGS = 27 (bank 27 = 1 byte per guest PC → flag byte)
  BANK_FLASH_BLOCK_FLAGS = 3 (block flags + cache signature)

Block format in flash:
  [header 8B] [native code variable] [epilogue 21B] [xbank 18B]
  Header: entry_pc(2) exit_pc(2) code_len(1) epilogue_off(1) flags(1) pad(1)
  
PC table (banks 19-26): 2 bytes per emulated PC → native address in flash
PC flags (bank 27):     1 byte per emulated PC → flag/bank byte
  $FF = not compiled
  bit7 set, bit6 clear = INTERPRETED (dispatch returns 2)
  bit7 clear = bank number of compiled code

PHA template (13 bytes): php stx ldx sta dec ldx plp
PLA template (13 bytes): php stx inc ldx lda ldx plp
  Both start with $08 (PHP) and end with $28 (PLP) — trim candidates
PHP template (15 bytes): php pha php pla ora stx ldx sta dec ldx pla plp
  Starts with $08 but size != 13 — NOT a trim candidate
"""

import sys

ROM_PATH = "exidy.nes"
INES_HEADER = 16
PRG_BANK_SIZE = 0x4000  # 16KB

BANK_FLASH_BLOCK_FLAGS = 3
BANK_CODE = 4
BANK_PC = 19
BANK_PC_FLAGS = 27

BLOCK_HEADER_SIZE = 8
BLOCK_ALIGNMENT = 16
FLASH_ERASE_SECTOR_SIZE = 0x1000
FLASH_SECTORS_PER_BANK = PRG_BANK_SIZE // FLASH_ERASE_SECTOR_SIZE  # 4

# Test ROM PC range
TEST_ROM_START = 0x2800
TEST_ROM_END = 0x3FFF

def bank_offset(bank):
    """File offset of start of PRG bank"""
    return INES_HEADER + bank * PRG_BANK_SIZE

def read_bank(data, bank):
    """Read entire 16KB bank from ROM data"""
    off = bank_offset(bank)
    return data[off:off + PRG_BANK_SIZE]

def read_byte(data, bank, addr):
    """Read a byte from bank at address (addr is 0-based within bank)"""
    return data[bank_offset(bank) + addr]

def read_word(data, bank, addr):
    return read_byte(data, bank, addr) | (read_byte(data, bank, addr + 1) << 8)

def disasm_6502(code, base_addr, max_bytes=None):
    """Simple 6502 disassembler for common opcodes"""
    mnemonics = {
        0x08: ("PHP", 1, "impl"), 0x28: ("PLP", 1, "impl"),
        0x48: ("PHA", 1, "impl"), 0x68: ("PLA", 1, "impl"),
        0x18: ("CLC", 1, "impl"), 0x38: ("SEC", 1, "impl"),
        0xB8: ("CLV", 1, "impl"), 0xD8: ("CLD", 1, "impl"),
        0x78: ("SEI", 1, "impl"), 0x58: ("CLI", 1, "impl"),
        0xEA: ("NOP", 1, "impl"),
        0xAA: ("TAX", 1, "impl"), 0x8A: ("TXA", 1, "impl"),
        0xA8: ("TAY", 1, "impl"), 0x98: ("TYA", 1, "impl"),
        0xBA: ("TSX", 1, "impl"), 0x9A: ("TXS", 1, "impl"),
        0xCA: ("DEX", 1, "impl"), 0xE8: ("INX", 1, "impl"),
        0x88: ("DEY", 1, "impl"), 0xC8: ("INY", 1, "impl"),
        0x60: ("RTS", 1, "impl"), 0x40: ("RTI", 1, "impl"),
        0x00: ("BRK", 1, "impl"),
        # Immediate
        0xA9: ("LDA", 2, "imm"), 0xA2: ("LDX", 2, "imm"), 0xA0: ("LDY", 2, "imm"),
        0xC9: ("CMP", 2, "imm"), 0xE0: ("CPX", 2, "imm"), 0xC0: ("CPY", 2, "imm"),
        0x69: ("ADC", 2, "imm"), 0xE9: ("SBC", 2, "imm"),
        0x29: ("AND", 2, "imm"), 0x49: ("EOR", 2, "imm"), 0x09: ("ORA", 2, "imm"),
        # Zero page
        0x85: ("STA", 2, "zp"), 0x86: ("STX", 2, "zp"), 0x84: ("STY", 2, "zp"),
        0xA5: ("LDA", 2, "zp"), 0xA6: ("LDX", 2, "zp"), 0xA4: ("LDY", 2, "zp"),
        0xC5: ("CMP", 2, "zp"), 0xE4: ("CPX", 2, "zp"), 0xC4: ("CPY", 2, "zp"),
        0x65: ("ADC", 2, "zp"), 0xE5: ("SBC", 2, "zp"),
        0x25: ("AND", 2, "zp"), 0x45: ("EOR", 2, "zp"), 0x05: ("ORA", 2, "zp"),
        0x24: ("BIT", 2, "zp"),
        0xE6: ("INC", 2, "zp"), 0xC6: ("DEC", 2, "zp"),
        0x06: ("ASL", 2, "zp"), 0x46: ("LSR", 2, "zp"),
        0x26: ("ROL", 2, "zp"), 0x66: ("ROR", 2, "zp"),
        # Zero page,X
        0x95: ("STA", 2, "zp,x"), 0xB5: ("LDA", 2, "zp,x"),
        0x75: ("ADC", 2, "zp,x"), 0xF5: ("SBC", 2, "zp,x"),
        0xF6: ("INC", 2, "zp,x"), 0xD6: ("DEC", 2, "zp,x"),
        # Absolute
        0x8D: ("STA", 3, "abs"), 0x8E: ("STX", 3, "abs"), 0x8C: ("STY", 3, "abs"),
        0xAD: ("LDA", 3, "abs"), 0xAE: ("LDX", 3, "abs"), 0xAC: ("LDY", 3, "abs"),
        0xCD: ("CMP", 3, "abs"), 0xEC: ("CPX", 3, "abs"), 0xCC: ("CPY", 3, "abs"),
        0x6D: ("ADC", 3, "abs"), 0xED: ("SBC", 3, "abs"),
        0x2D: ("AND", 3, "abs"), 0x4D: ("EOR", 3, "abs"), 0x0D: ("ORA", 3, "abs"),
        0x2C: ("BIT", 3, "abs"),
        0xEE: ("INC", 3, "abs"), 0xCE: ("DEC", 3, "abs"),
        0x0E: ("ASL", 3, "abs"), 0x4E: ("LSR", 3, "abs"),
        0x2E: ("ROL", 3, "abs"), 0x6E: ("ROR", 3, "abs"),
        # Absolute,X / Absolute,Y
        0x9D: ("STA", 3, "abs,x"), 0xBD: ("LDA", 3, "abs,x"),
        0x7D: ("ADC", 3, "abs,x"), 0xFD: ("SBC", 3, "abs,x"),
        0x99: ("STA", 3, "abs,y"), 0xB9: ("LDA", 3, "abs,y"),
        0x79: ("ADC", 3, "abs,y"), 0xF9: ("SBC", 3, "abs,y"),
        0xBE: ("LDX", 3, "abs,y"),
        0xFE: ("INC", 3, "abs,x"), 0xDE: ("DEC", 3, "abs,x"),
        # Branches
        0x10: ("BPL", 2, "rel"), 0x30: ("BMI", 2, "rel"),
        0x50: ("BVC", 2, "rel"), 0x70: ("BVS", 2, "rel"),
        0x90: ("BCC", 2, "rel"), 0xB0: ("BCS", 2, "rel"),
        0xD0: ("BNE", 2, "rel"), 0xF0: ("BEQ", 2, "rel"),
        # JMP
        0x4C: ("JMP", 3, "abs"), 0x6C: ("JMP", 3, "(abs)"),
        # JSR
        0x20: ("JSR", 3, "abs"),
        # Indirect
        0xA1: ("LDA", 2, "(zp,x)"), 0xB1: ("LDA", 2, "(zp),y"),
        0x81: ("STA", 2, "(zp,x)"), 0x91: ("STA", 2, "(zp),y"),
        # ASL A / LSR A / ROL A / ROR A
        0x0A: ("ASL", 1, "a"), 0x4A: ("LSR", 1, "a"),
        0x2A: ("ROL", 1, "a"), 0x6A: ("ROR", 1, "a"),
    }
    
    lines = []
    i = 0
    limit = max_bytes if max_bytes else len(code)
    while i < limit and i < len(code):
        b = code[i]
        addr = base_addr + i
        if b in mnemonics:
            mnem, sz, mode = mnemonics[b]
            if i + sz > len(code):
                lines.append(f"  ${addr:04X}: {b:02X}         ???")
                break
            raw = ' '.join(f'{code[i+j]:02X}' for j in range(sz))
            if mode == "impl" or mode == "a":
                operand = "" if mode == "impl" else "A"
                lines.append(f"  ${addr:04X}: {raw:<9s} {mnem} {operand}".rstrip())
            elif mode == "imm":
                lines.append(f"  ${addr:04X}: {raw:<9s} {mnem} #${code[i+1]:02X}")
            elif mode == "zp":
                lines.append(f"  ${addr:04X}: {raw:<9s} {mnem} ${code[i+1]:02X}")
            elif mode == "zp,x":
                lines.append(f"  ${addr:04X}: {raw:<9s} {mnem} ${code[i+1]:02X},X")
            elif mode == "abs" or mode == "(abs)":
                w = code[i+1] | (code[i+2] << 8)
                paren = "(" if mode == "(abs)" else ""
                cparen = ")" if mode == "(abs)" else ""
                lines.append(f"  ${addr:04X}: {raw:<9s} {mnem} {paren}${w:04X}{cparen}")
            elif mode == "abs,x":
                w = code[i+1] | (code[i+2] << 8)
                lines.append(f"  ${addr:04X}: {raw:<9s} {mnem} ${w:04X},X")
            elif mode == "abs,y":
                w = code[i+1] | (code[i+2] << 8)
                lines.append(f"  ${addr:04X}: {raw:<9s} {mnem} ${w:04X},Y")
            elif mode == "rel":
                off = code[i+1]
                if off >= 0x80: off -= 0x100
                target = addr + 2 + off
                lines.append(f"  ${addr:04X}: {raw:<9s} {mnem} ${target:04X}")
            elif mode == "(zp,x)":
                lines.append(f"  ${addr:04X}: {raw:<9s} {mnem} (${code[i+1]:02X},X)")
            elif mode == "(zp),y":
                lines.append(f"  ${addr:04X}: {raw:<9s} {mnem} (${code[i+1]:02X}),Y")
            else:
                lines.append(f"  ${addr:04X}: {raw:<9s} {mnem}")
            i += sz
        else:
            lines.append(f"  ${addr:04X}: {b:02X}         .db ${b:02X}")
            i += 1
    return lines

def find_blocks_in_sector(data, bank, sector_within_bank):
    """Scan a 4KB sector for compiled blocks (look for aligned headers)"""
    sector_base = sector_within_bank * FLASH_ERASE_SECTOR_SIZE
    sector_data = data[bank_offset(bank) + sector_base : bank_offset(bank) + sector_base + FLASH_ERASE_SECTOR_SIZE]
    
    blocks = []
    # Scan for block headers at aligned positions
    # Header is at (aligned - BLOCK_HEADER_SIZE), code at aligned boundary
    pos = 0
    while pos < FLASH_ERASE_SECTOR_SIZE - BLOCK_HEADER_SIZE:
        # Check if there's a valid header here:
        # aligned code start would be at pos + BLOCK_HEADER_SIZE,
        # but headers can start at any offset that makes code_start aligned
        code_start = (pos + BLOCK_HEADER_SIZE + BLOCK_ALIGNMENT - 1) & ~(BLOCK_ALIGNMENT - 1)
        header_start = code_start - BLOCK_HEADER_SIZE
        
        if header_start >= FLASH_ERASE_SECTOR_SIZE:
            break
            
        # Read header
        entry_pc = sector_data[header_start] | (sector_data[header_start+1] << 8)
        exit_pc = sector_data[header_start+2] | (sector_data[header_start+3] << 8)
        code_len = sector_data[header_start+4]
        epilogue_off = sector_data[header_start+5]
        flags = sector_data[header_start+6]
        
        # Check if this looks like a valid block
        # entry_pc should be in reasonable range, code_len > 0, all bytes should not be $FF
        if (entry_pc == 0xFFFF and exit_pc == 0xFFFF) or code_len == 0xFF or code_len == 0:
            pos = code_start + BLOCK_ALIGNMENT
            continue
        
        # Looks valid
        flash_addr = 0x8000 + sector_base + code_start
        native_code = sector_data[code_start : code_start + code_len]
        
        blocks.append({
            'bank': bank,
            'sector': sector_within_bank,
            'header_offset': header_start,
            'code_offset': code_start,
            'flash_addr': flash_addr,
            'entry_pc': entry_pc,
            'exit_pc': exit_pc,
            'code_len': code_len,
            'epilogue_off': epilogue_off,
            'flags': flags,
            'code': native_code,
        })
        
        pos = code_start + code_len
        # Align to next possible header
        pos = (pos + BLOCK_ALIGNMENT - 1) & ~(BLOCK_ALIGNMENT - 1)
    
    return blocks

def check_peephole_trim(code):
    """Analyze a block's native code for peephole trim patterns.
    
    With trim ON: PHA/PLA templates (13 bytes) should have their trailing PLP ($28) removed,
    and a PLP should be flushed (re-inserted) before the next non-PHA/PLA instruction.
    
    Without trim: every PHA/PLA template starts with PHP ($08) and ends with PLP ($28).
    
    Returns analysis notes.
    """
    notes = []
    
    # Count PHP/PLP pairs
    php_count = sum(1 for b in code if b == 0x08)
    plp_count = sum(1 for b in code if b == 0x28)
    
    if php_count != plp_count:
        notes.append(f"  ⚠ PHP/PLP MISMATCH: {php_count} PHP vs {plp_count} PLP")
    
    # Look for consecutive PHP...PLP PHP...PLP patterns (no trim)
    # vs PHP...PHP (trimmed — missing PLP between them)
    i = 0
    template_starts = []
    while i < len(code):
        if code[i] == 0x08:  # PHP
            template_starts.append(i)
        i += 1
    
    if len(template_starts) >= 2:
        for j in range(len(template_starts) - 1):
            gap_start = template_starts[j]
            gap_end = template_starts[j+1]
            # Check if there's a PLP ($28) between these two PHPs
            gap = code[gap_start+1:gap_end]
            has_plp = 0x28 in gap
            if not has_plp:
                notes.append(f"  ✂ TRIM detected: no PLP between PHP@{gap_start} and PHP@{gap_end} (saved 2 bytes)")
            else:
                plp_pos = gap_start + 1 + list(gap).index(0x28)
                notes.append(f"  ○ PLP@{plp_pos} between PHP@{gap_start} and PHP@{gap_end} (no trim / flush)")
    
    return notes

def main():
    with open(ROM_PATH, 'rb') as f:
        data = f.read()
    
    print(f"ROM size: {len(data)} bytes ({len(data)//1024} KB)")
    print(f"PRG banks: {(len(data) - INES_HEADER) // PRG_BANK_SIZE}")
    print()
    
    # === Scan PC flags table (bank 27) for test ROM PCs ===
    print("=" * 70)
    print("PC FLAGS TABLE (bank 27) — Test ROM range $2800–$3FFF")
    print("=" * 70)
    
    # PC flags: 1 byte per guest PC
    # PC table: 2 bytes per guest PC (banks 19-26)
    # Address mapping: pc_jump_bank = (emulated_pc >> 13) + BANK_PC
    # pc_jump_address = (emulated_pc << 1) & 0x3FFF
    # flag bank = different calculation...
    
    # Let me check: for the test ROM ($2800-$3FFF), the PC range is 0x2800..0x3FFF
    # flag_bank = BANK_PC_FLAGS = 27 (single bank for all flags)
    # Actually let me re-read... pc_jump_flag_bank is separate
    
    # From flash_cache_pc_update / setup_flash_pc_tables:
    # pc_jump_bank = (emulated_pc >> 13) + BANK_PC  (for the 2-byte native addr)
    # pc_jump_address = (emulated_pc << 1) & 0x3FFF
    # The flag bank is separate: lookup_pc_jump_flag
    
    # For PCs $2800-$3FFF: emulated_pc >> 13 = 1, so pc_jump_bank = 20
    # pc_jump_address = (pc << 1) & 0x3FFF
    
    compiled_pcs = []
    
    # Check flag byte for each test PC
    # Flag byte location: we need to find lookup_pc_jump_flag
    # From the code pattern, flag is 1 byte per PC, stored differently
    # Let me just scan for non-$FF entries in the PC table range
    
    pc_bank = BANK_PC + (TEST_ROM_START >> 13)  # bank 20 for PCs $2000-$3FFF
    print(f"PC table bank: {pc_bank}")
    
    found_entries = 0
    for pc in range(TEST_ROM_START, TEST_ROM_END + 1):
        pc_addr = (pc << 1) & 0x3FFF
        lo = read_byte(data, pc_bank, pc_addr)
        hi = read_byte(data, pc_bank, pc_addr + 1)
        native = lo | (hi << 8)
        
        if native != 0xFFFF:  # not empty
            compiled_pcs.append((pc, native))
            found_entries += 1
    
    print(f"Found {found_entries} compiled PC entries in test ROM range")
    print()
    
    # Show the compiled PCs
    if compiled_pcs:
        print("Guest PC → Native Address:")
        for pc, native in compiled_pcs[:60]:  # first 60
            print(f"  ${pc:04X} → ${native:04X}")
        if len(compiled_pcs) > 60:
            print(f"  ... and {len(compiled_pcs)-60} more")
    print()
    
    # === Scan code cache banks for compiled blocks ===
    print("=" * 70)
    print(f"COMPILED BLOCKS (banks {BANK_CODE}–{BANK_CODE + 13})")
    print("=" * 70)
    
    all_blocks = []
    for bank in range(BANK_CODE, BANK_CODE + 14):  # 14 code cache banks
        for sector in range(FLASH_SECTORS_PER_BANK):
            blocks = find_blocks_in_sector(data, bank, sector)
            all_blocks.extend(blocks)
    
    print(f"Found {len(all_blocks)} compiled blocks total")
    print()
    
    # Filter to blocks whose entry_pc is in test ROM range
    test_blocks = [b for b in all_blocks if TEST_ROM_START <= b['entry_pc'] <= TEST_ROM_END]
    
    print(f"Blocks with entry_pc in test ROM range ($2800-$3FFF): {len(test_blocks)}")
    print()
    
    for blk in sorted(test_blocks, key=lambda b: b['entry_pc']):
        entry = blk['entry_pc']
        exit_pc = blk['exit_pc']
        code = blk['code']
        bank = blk['bank']
        
        print(f"--- Block: entry=${entry:04X}  exit=${exit_pc:04X}  bank={bank}  "
              f"len={blk['code_len']}  epilogue_off={blk['epilogue_off']}  "
              f"flash=${blk['flash_addr']:04X} ---")
        
        # Disassemble the native code
        lines = disasm_6502(code, blk['flash_addr'], max_bytes=blk['code_len'])
        for line in lines:
            print(line)
        
        # Check for peephole patterns
        # Look at code up to epilogue
        main_code = code[:blk['epilogue_off']] if blk['epilogue_off'] < len(code) else code
        notes = check_peephole_trim(main_code)
        if notes:
            print("  [Peephole analysis]")
            for n in notes:
                print(n)
        print()
    
    # === Check for specific test labels (from the test asm) ===
    # Test T66-T88 are the peephole tests.  Let's look for PHA/PLA patterns
    # in the compiled blocks.
    print("=" * 70)
    print("PEEPHOLE TRIM SUMMARY")
    print("=" * 70)
    
    trim_blocks = 0
    notrim_blocks = 0
    mismatch_blocks = 0
    
    for blk in sorted(test_blocks, key=lambda b: b['entry_pc']):
        main_code = blk['code'][:blk['epilogue_off']] if blk['epilogue_off'] < len(blk['code']) else blk['code']
        
        php_count = sum(1 for b in main_code if b == 0x08)
        plp_count = sum(1 for b in main_code if b == 0x28)
        
        if php_count > 0 or plp_count > 0:
            status = "✓" if php_count == plp_count else "⚠ MISMATCH"
            if php_count != plp_count:
                mismatch_blocks += 1
                # This likely means trim is active
                if php_count > plp_count:
                    trim_blocks += 1
                    status = "✂ TRIMMED"
                    
            print(f"  ${blk['entry_pc']:04X}-${blk['exit_pc']:04X}: "
                  f"PHP={php_count} PLP={plp_count}  {status}  "
                  f"(code={blk['code_len']}B, epilogue@{blk['epilogue_off']})")
    
    print(f"\nBlocks with PHP/PLP: trimmed={trim_blocks}, balanced={notrim_blocks}, mismatch={mismatch_blocks}")

if __name__ == '__main__':
    main()
