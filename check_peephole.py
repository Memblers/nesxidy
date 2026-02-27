#!/usr/bin/env python3
"""
check_peephole.py — Focused analysis of peephole trim in compiled blocks.
Checks whether deferred PLP bytes are actually present in flash.
"""

ROM_PATH = "exidy.nes"
INES_HEADER = 16
PRG_BANK_SIZE = 0x4000

BANK_CODE = 4
BLOCK_HEADER_SIZE = 8
BLOCK_ALIGNMENT = 16
FLASH_ERASE_SECTOR_SIZE = 0x1000
FLASH_SECTORS_PER_BANK = 4

TEST_ROM_START = 0x2800
TEST_ROM_END = 0x3FFF

# PHA template body (without leading PHP and trailing PLP):
# stx _x(86 62)  ldx _sp(A6 60)  sta _RAM+100,x(9D BA 6E)  dec _sp(C6 60)  ldx _x(A6 62)
PHA_BODY = bytes([0x86, 0x62, 0xA6, 0x60, 0x9D, 0xBA, 0x6E, 0xC6, 0x60, 0xA6, 0x62])
# PLA template body (without leading PHP and trailing PLP):
# stx _x(86 62)  inc _sp(E6 60)  ldx _sp(A6 60)  lda _RAM+100,x(BD BA 6E)  ldx _x(A6 62)
PLA_BODY = bytes([0x86, 0x62, 0xE6, 0x60, 0xA6, 0x60, 0xBD, 0xBA, 0x6E, 0xA6, 0x62])
# PHP template body (without leading PHP, trailing PLP): 15 bytes total
# php pha php pla ora#30 stx_x ldx_sp sta_ram dec_sp ldx_x pla plp
# Full: 08 48 08 68 09 30 86 62 A6 60 9D BA 6E C6 60 A6 62 68 28
PHP_INNER = bytes([0x48, 0x08, 0x68, 0x09, 0x30, 0x86, 0x62, 0xA6, 0x60, 0x9D, 0xBA, 0x6E, 0xC6, 0x60, 0xA6, 0x62, 0x68, 0x28])

def bank_offset(bank):
    return INES_HEADER + bank * PRG_BANK_SIZE

def find_blocks_in_sector(data, bank, sector):
    base = bank_offset(bank) + sector * FLASH_ERASE_SECTOR_SIZE
    sd = data[base:base + FLASH_ERASE_SECTOR_SIZE]
    blocks = []
    pos = 0
    while pos < FLASH_ERASE_SECTOR_SIZE - BLOCK_HEADER_SIZE:
        cs = (pos + BLOCK_HEADER_SIZE + BLOCK_ALIGNMENT - 1) & ~(BLOCK_ALIGNMENT - 1)
        hs = cs - BLOCK_HEADER_SIZE
        if hs >= FLASH_ERASE_SECTOR_SIZE:
            break
        ep = sd[hs] | (sd[hs+1] << 8)
        xp = sd[hs+2] | (sd[hs+3] << 8)
        cl = sd[hs+4]
        eo = sd[hs+5]
        if (ep == 0xFFFF and xp == 0xFFFF) or cl == 0xFF or cl == 0:
            pos = cs + BLOCK_ALIGNMENT
            continue
        flash_addr = 0x8000 + sector * FLASH_ERASE_SECTOR_SIZE + cs
        blocks.append({
            'bank': bank, 'flash_addr': flash_addr,
            'entry_pc': ep, 'exit_pc': xp,
            'code_len': cl, 'epilogue_off': eo,
            'code': sd[cs:cs+cl],
        })
        pos = cs + cl
        pos = (pos + BLOCK_ALIGNMENT - 1) & ~(BLOCK_ALIGNMENT - 1)
    return blocks

def find_template_instances(code, epilogue_off):
    """Find PHA/PLA/PHP template instances in compiled code (before epilogue)."""
    main = code[:epilogue_off] if epilogue_off < len(code) else code
    instances = []
    i = 0
    while i < len(main):
        if main[i] == 0x08:  # PHP opcode - could be start of template
            # Check for PHA body
            if i + 1 + len(PHA_BODY) <= len(main) and main[i+1:i+1+len(PHA_BODY)] == PHA_BODY:
                end = i + 1 + len(PHA_BODY)
                has_plp = end < len(main) and main[end] == 0x28
                instances.append(('PHA', i, end + (1 if has_plp else 0), has_plp))
                i = end + (1 if has_plp else 0)
                continue
            # Check for PLA body
            if i + 1 + len(PLA_BODY) <= len(main) and main[i+1:i+1+len(PLA_BODY)] == PLA_BODY:
                end = i + 1 + len(PLA_BODY)
                has_plp = end < len(main) and main[end] == 0x28
                instances.append(('PLA', i, end + (1 if has_plp else 0), has_plp))
                i = end + (1 if has_plp else 0)
                continue
            # Check for PHP template body (starts with PHP then PHA(48) PHP(08)...)
            if i + 1 + len(PHP_INNER) <= len(main) and main[i+1:i+1+len(PHP_INNER)] == PHP_INNER:
                sz = 1 + len(PHP_INNER)  # 19 bytes: 08 + body
                instances.append(('PHP', i, i + sz, True))  # PHP template always has its PLP inside
                i += sz
                continue
        i += 1
    return instances

def check_plp_flush(code, instances):
    """Check if PLP flush bytes are present between trimmed templates and next instruction."""
    results = []
    for idx, (tmpl_type, start, end, has_plp) in enumerate(instances):
        if not has_plp:
            # Template was trimmed - check if PLP flush byte exists after it
            # The PLP should be at position 'end' (the byte right after the trimmed template)
            if end < len(code):
                byte_after = code[end]
                results.append({
                    'template': tmpl_type,
                    'offset': start,
                    'end': end,
                    'trimmed': True,
                    'flush_byte': byte_after,
                    'flush_is_plp': byte_after == 0x28,
                })
            else:
                results.append({
                    'template': tmpl_type,
                    'offset': start,
                    'end': end,
                    'trimmed': True,
                    'flush_byte': None,
                    'flush_is_plp': False,
                })
        else:
            results.append({
                'template': tmpl_type,
                'offset': start,
                'end': end,
                'trimmed': False,
                'flush_byte': None,
                'flush_is_plp': None,
            })
    return results

# Simple 6502 disassembler (just for context bytes)
OPCODES = {
    0x08:"PHP",0x28:"PLP",0x48:"PHA",0x68:"PLA",0x18:"CLC",0x38:"SEC",0xB8:"CLV",
    0xD8:"CLD",0x78:"SEI",0xEA:"NOP",0xAA:"TAX",0x8A:"TXA",0xA8:"TAY",0x98:"TYA",
    0xCA:"DEX",0xE8:"INX",0x88:"DEY",0xC8:"INY",0x60:"RTS",0x0A:"ASL A",0x4A:"LSR A",
    0x2A:"ROL A",0x6A:"ROR A",
}
IMM = {0xA9:"LDA",0xA2:"LDX",0xA0:"LDY",0xC9:"CMP",0xE0:"CPX",0xC0:"CPY",
       0x69:"ADC",0xE9:"SBC",0x29:"AND",0x49:"EOR",0x09:"ORA"}
ZP = {0x85:"STA",0x86:"STX",0x84:"STY",0xA5:"LDA",0xA6:"LDX",0xA4:"LDY",
      0xC5:"CMP",0xE6:"INC",0xC6:"DEC",0xE4:"CPX",0xC4:"CPY",0x65:"ADC",
      0xE5:"SBC",0x25:"AND",0x45:"EOR",0x05:"ORA",0x24:"BIT",
      0x06:"ASL",0x46:"LSR",0x26:"ROL",0x66:"ROR"}
ABS3 = {0x8D:"STA",0x8E:"STX",0x8C:"STY",0xAD:"LDA",0xAE:"LDX",0xAC:"LDY",
        0xCD:"CMP",0xEE:"INC",0xCE:"DEC",0xEC:"CPX",0xCC:"CPY",
        0x6D:"ADC",0xED:"SBC",0x2D:"AND",0x4D:"EOR",0x0D:"ORA",0x2C:"BIT",
        0x4C:"JMP",0x20:"JSR",0x0E:"ASL",0x4E:"LSR",0x2E:"ROL",0x6E:"ROR",
        0x9D:"STA,X",0xBD:"LDA,X",0x7D:"ADC,X",0xFD:"SBC,X",
        0x99:"STA,Y",0xB9:"LDA,Y",0x79:"ADC,Y",0xF9:"SBC,Y",
        0xBE:"LDX,Y",0xFE:"INC,X",0xDE:"DEC,X"}
BR = {0x10:"BPL",0x30:"BMI",0x50:"BVC",0x70:"BVS",0x90:"BCC",0xB0:"BCS",0xD0:"BNE",0xF0:"BEQ"}

def disasm_byte(code, i, base):
    b = code[i]
    if b in OPCODES: return f"${base+i:04X}: {b:02X}        {OPCODES[b]}", 1
    if b in IMM and i+1<len(code): return f"${base+i:04X}: {b:02X} {code[i+1]:02X}     {IMM[b]} #${code[i+1]:02X}", 2
    if b in ZP and i+1<len(code): return f"${base+i:04X}: {b:02X} {code[i+1]:02X}     {ZP[b]} ${code[i+1]:02X}", 2
    if b in ABS3 and i+2<len(code):
        w = code[i+1]|(code[i+2]<<8)
        return f"${base+i:04X}: {b:02X} {code[i+1]:02X} {code[i+2]:02X}  {ABS3[b]} ${w:04X}", 3
    if b in BR and i+1<len(code):
        off = code[i+1]; 
        if off>=0x80: off-=0x100
        t = base+i+2+off
        return f"${base+i:04X}: {b:02X} {code[i+1]:02X}     {BR[b]} ${t:04X}", 2
    return f"${base+i:04X}: {b:02X}        .db ${b:02X}", 1

def main():
    with open(ROM_PATH, 'rb') as f:
        data = f.read()
    
    all_blocks = []
    for bank in range(BANK_CODE, BANK_CODE + 14):
        for sector in range(FLASH_SECTORS_PER_BANK):
            all_blocks.extend(find_blocks_in_sector(data, bank, sector))
    
    test_blocks = [b for b in all_blocks if TEST_ROM_START <= b['entry_pc'] <= TEST_ROM_END]
    test_blocks.sort(key=lambda b: b['entry_pc'])
    
    print(f"Total compiled blocks: {len(all_blocks)}")
    print(f"Blocks in test ROM range: {len(test_blocks)}")
    print()
    
    # Find blocks with PHA/PLA templates
    peephole_blocks = []
    for blk in test_blocks:
        instances = find_template_instances(blk['code'], blk['epilogue_off'])
        if instances:
            peephole_blocks.append((blk, instances))
    
    print(f"Blocks containing PHA/PLA/PHP templates: {len(peephole_blocks)}")
    print("=" * 72)
    
    total_trimmed = 0
    total_trimmed_with_flush = 0
    total_trimmed_without_flush = 0
    
    for blk, instances in peephole_blocks:
        checks = check_plp_flush(blk['code'], instances)
        
        has_interesting = any(c['trimmed'] for c in checks)
        if not has_interesting:
            continue
        
        print(f"\n--- Block: entry=${blk['entry_pc']:04X}  exit=${blk['exit_pc']:04X}  "
              f"bank={blk['bank']}  len={blk['code_len']}  "
              f"epilogue@{blk['epilogue_off']}  flash=${blk['flash_addr']:04X} ---")
        
        for c in checks:
            total_trimmed += 1 if c['trimmed'] else 0
            
            if c['trimmed']:
                if c['flush_is_plp']:
                    total_trimmed_with_flush += 1
                    symbol = "✓ PLP PRESENT"
                else:
                    total_trimmed_without_flush += 1
                    fb = f"${c['flush_byte']:02X}" if c['flush_byte'] is not None else "N/A"
                    symbol = f"✗ PLP MISSING (got {fb})"
                
                print(f"  {c['template']} @offset {c['offset']:3d}  TRIMMED  → {symbol}")
                
                # Show context: 3 bytes before end, the expected PLP position, 3 bytes after
                end = c['end']
                base = blk['flash_addr']
                code = blk['code']
                
                ctx_start = max(0, end - 3)
                ctx_end = min(len(code), end + 4)
                
                print(f"    Context around flush point (offset {end}):")
                i = ctx_start
                while i < ctx_end:
                    line, sz = disasm_byte(code, i, base)
                    marker = " ◄── PLP expected here" if i == end else ""
                    if i == end and code[i] == 0x28:
                        marker = " ◄── PLP flush ✓"
                    print(f"    {line}{marker}")
                    i += sz
            else:
                print(f"  {c['template']} @offset {c['offset']:3d}  intact (not trimmed)")
    
    print()
    print("=" * 72)
    print("SUMMARY")
    print("=" * 72)
    print(f"  Total trimmed templates:     {total_trimmed}")
    print(f"  PLP flush present in flash:  {total_trimmed_with_flush}")
    print(f"  PLP flush MISSING in flash:  {total_trimmed_without_flush}")
    if total_trimmed_without_flush > 0:
        print()
        print("  ⚠ CRITICAL: Trimmed templates have NO PLP in flash!")
        print("  The deferred PLP is written to cache_code buffer but")
        print("  never reaches flash.  At runtime, host flags stay")
        print("  corrupted after the trimmed template executes.")

if __name__ == '__main__':
    main()
