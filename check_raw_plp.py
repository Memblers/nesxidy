#!/usr/bin/env python3
"""Check raw bytes at expected PLP positions in compiled blocks."""

ROM_PATH = "exidy.nes"
INES_HEADER = 16
PRG_BANK_SIZE = 0x4000

def main():
    data = open(ROM_PATH, 'rb').read()
    
    # Block at entry=$2A6E: bank=4, flash=$9CD0
    # Bank 4 file offset = INES_HEADER + 4 * 0x4000
    bank4_off = INES_HEADER + 4 * PRG_BANK_SIZE  # 0x10010
    # flash=$9CD0 => bank-relative offset = $9CD0 - $8000 = $1CD0
    code_off = bank4_off + 0x1CD0
    
    # Header is 8 bytes before code
    hdr_off = code_off - 8
    entry_pc = data[hdr_off] | (data[hdr_off+1] << 8)
    exit_pc = data[hdr_off+2] | (data[hdr_off+3] << 8)
    code_len = data[hdr_off+4]
    epi_off = data[hdr_off+5]
    
    print(f"Block header: entry=${entry_pc:04X} exit=${exit_pc:04X} "
          f"code_len={code_len} epilogue_off={epi_off}")
    print()
    
    # Dump code bytes with annotations
    # PHA template body (without PHP/PLP): 11 bytes
    PHA_BODY = [0x86,0x62, 0xA6,0x60, 0x9D,0xBA,0x6E, 0xC6,0x60, 0xA6,0x62]
    PLA_BODY = [0x86,0x62, 0xE6,0x60, 0xA6,0x60, 0xBD,0xBA,0x6E, 0xA6,0x62]
    
    print("Code bytes (before epilogue):")
    for i in range(min(epi_off + 5, code_len)):
        b = data[code_off + i]
        marker = ""
        if i == epi_off:
            marker = " <--- EPILOGUE START"
        elif b == 0x28:
            marker = " (PLP)"
        elif b == 0x08:
            # Check if PHA or PLA template starts here
            remaining = [data[code_off + i + 1 + j] for j in range(11) if i+1+j < code_len]
            if remaining == PHA_BODY:
                marker = " (PHP = start of PHA template)"
            elif remaining == PLA_BODY:
                marker = " (PHP = start of PLA template)"
            else:
                marker = " (PHP)"
        print(f"  [{i:3d}] ${b:02X}{marker}")
    
    # Identify where PLP flush bytes should be
    print()
    print("=== PLP FLUSH ANALYSIS ===")
    # Find all template instances
    i = 0
    templates = []
    while i < epi_off:
        if data[code_off + i] == 0x08:  # PHP
            remaining = [data[code_off + i + 1 + j] for j in range(11) if i+1+j < code_len]
            if remaining == PHA_BODY:
                has_plp = (i + 12 < code_len) and data[code_off + i + 12] == 0x28
                templates.append(('PHA', i, 12 if not has_plp else 13, has_plp))
                i += 12 + (1 if has_plp else 0)
                continue
            elif remaining == PLA_BODY:
                has_plp = (i + 12 < code_len) and data[code_off + i + 12] == 0x28
                templates.append(('PLA', i, 12 if not has_plp else 13, has_plp))
                i += 12 + (1 if has_plp else 0)
                continue
        i += 1
    
    for tmpl_type, start, size, has_plp in templates:
        end = start + size
        if has_plp:
            print(f"  {tmpl_type} @{start}: intact (PLP at offset {end-1})")
        else:
            expected_plp_pos = end
            actual_byte = data[code_off + expected_plp_pos] if expected_plp_pos < code_len else None
            if actual_byte == 0x28:
                print(f"  {tmpl_type} @{start}: trimmed, PLP flush PRESENT at offset {expected_plp_pos}")
            else:
                byte_str = f"${actual_byte:02X}" if actual_byte is not None else "N/A"
                print(f"  {tmpl_type} @{start}: trimmed, PLP MISSING at offset {expected_plp_pos} (got {byte_str})")
    
    # Check ALL $2A blocks  
    print()
    print("=== ALL BLOCKS WITH PHA/PLA TEMPLATES ===")
    
    # Scan for blocks in bank 4
    for sector in range(4):
        sector_base = bank4_off + sector * 0x1000
        pos = 0
        while pos < 0x1000 - 8:
            cs = (pos + 8 + 15) & ~15  # align to 16
            hs = cs - 8
            if hs >= 0x1000:
                break
            ep = data[sector_base + hs] | (data[sector_base + hs + 1] << 8)
            xp = data[sector_base + hs + 2] | (data[sector_base + hs + 3] << 8)
            cl = data[sector_base + hs + 4]
            eo = data[sector_base + hs + 5]
            if (ep == 0xFFFF and xp == 0xFFFF) or cl == 0xFF or cl == 0:
                pos = cs + 16
                continue
            
            # Check for PHA/PLA templates in this block
            has_templates = False
            for i in range(min(eo, cl)):
                if data[sector_base + cs + i] == 0x08 and i + 12 <= cl:
                    remaining = [data[sector_base + cs + i + 1 + j] for j in range(11)]
                    if remaining == PHA_BODY or remaining == PLA_BODY:
                        has_templates = True
                        break
            
            if has_templates and 0x2800 <= ep <= 0x3FFF:
                flash_addr = 0x8000 + sector * 0x1000 + cs
                print(f"\n  Block entry=${ep:04X} exit=${xp:04X} flash=${flash_addr:04X} len={cl} epi@{eo}")
                # Find and report all templates
                ii = 0
                while ii < min(eo, cl):
                    if data[sector_base + cs + ii] == 0x08 and ii + 12 <= cl:
                        remaining = [data[sector_base + cs + ii + 1 + j] for j in range(11)]
                        if remaining == PHA_BODY:
                            has_plp = (ii + 12 < cl) and data[sector_base + cs + ii + 12] == 0x28
                            end_pos = ii + 12
                            next_byte = data[sector_base + cs + end_pos] if end_pos < cl else None
                            nb = f"${next_byte:02X}" if next_byte is not None else "N/A"
                            status = "intact" if has_plp else f"TRIMMED, next={nb}"
                            print(f"    PHA @{ii}: {status}")
                            ii += 12 + (1 if has_plp else 0)
                            continue
                        elif remaining == PLA_BODY:
                            has_plp = (ii + 12 < cl) and data[sector_base + cs + ii + 12] == 0x28
                            end_pos = ii + 12
                            next_byte = data[sector_base + cs + end_pos] if end_pos < cl else None
                            nb = f"${next_byte:02X}" if next_byte is not None else "N/A"
                            status = "intact" if has_plp else f"TRIMMED, next={nb}"
                            print(f"    PLA @{ii}: {status}")
                            ii += 12 + (1 if has_plp else 0)
                            continue
                    ii += 1
            
            pos = cs + cl
            pos = (pos + 15) & ~15

if __name__ == '__main__':
    main()
