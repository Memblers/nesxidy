"""
Scan trace for ALL flash_byte_program writes to bank 4 in the address range
$9B80-$9BC0 (around $9BA0).  This tells us who writes the xbank setup code
that eventually lives at $9BA0.

Also check for ANY STA $C000 with A=04 (bankswitch to bank 4) near the
crash moment to verify bank 4 is the active bank.

flash_byte_program = $6000 (WRAM).  Data write: STA (r2),Y at PC=$6041.
The bank is in X (loaded from r2 at $6000).
The effective address is in the [...] annotation.

We need writes where:
  - X = $04 (bank 4, but with chr bank OR'd in — may be $04, or $04|chr)
  - effective address = [$9B80]-[$9BC0]

Actually, the bank parameter includes chr bank bits. BANK_CODE = 4, and
the bank is computed as (sector >> 2) + BANK_CODE.  For bank 4, sector 0-3.
The mapper register = prg_bank | chr_bank.  For bank 4 with chr=0, 
X would be $04.  With chr bank bits, X = $04 | chr.

For flash_byte_program, the bank param is just the flash bank (0-30).
But the WRAM code does: TXA / ORA _mapper_chr_bank / STA $C000.
So the actual bank switch is bank | chr.  But the X register entering
flash_byte_program is the raw bank (without chr).

Looking at the flash_byte_program code at $602E:
  602E  TXA              ; X = bank parameter (raw)
  602F  ORA _mapper_chr_bank
  6032  STA $C000        ; bank switch = bank | chr

So X at $6041 is the BANKED value including chr bits, not the raw bank.

Wait no — let me re-read:
  6000  LDX r2 = $19     ; X = raw bank from r2 (set by caller as bank param)
  ...
  602E  TXA              ; A = raw bank
  602F  ORA _mapper_chr_bank ; A = bank | chr  
  6032  STA $C000        ; bankswitch
  6035  LDA ASM_BLOCK_COUNT ; A = hi byte of address
  6037  STA r3           ; store
  6039  LDA ENABLE_NATIVE_STACK ; A = lo byte
  603B  STA r2           ; store  
  603D  LDA r4           ; A = data byte
  603F  LDY #$00
  6041  STA (r2),Y       ; write data byte to (r2+Y) = (addr)

So at $6041, X still has the raw bank from entry. In the trace, X=$19 for
the PC table writes. For code bank 4 writes, X would be $04.

Let's scan for STA (r2),Y at PC=$6041 where X=$04 and effective address
is in the range $9B80-$9BC0.
"""
import sys

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"

target_range_lo = 0x9B80
target_range_hi = 0x9BC0
target_bank_x = 0x04

CHUNK = 200 * 1024 * 1024  # 200 MB
hits = []

print(f"Scanning for flash writes to bank 4, addresses ${target_range_lo:04X}-${target_range_hi:04X}")
print(f"Trace: {TRACE}")

with open(TRACE, 'rb') as f:
    f.seek(0, 2)
    file_size = f.tell()
    f.seek(0)
    
    leftover = b""
    offset = 0
    
    while offset < file_size:
        raw = f.read(CHUNK)
        if not raw:
            break
        data = leftover + raw
        
        # Find last newline
        last_nl = data.rfind(b'\n')
        if last_nl < 0:
            leftover = data
            offset += len(raw)
            continue
        leftover = data[last_nl+1:]
        chunk_text = data[:last_nl]
        
        for line in chunk_text.split(b'\n'):
            if b'6041' not in line:
                continue
            line_str = line.decode('utf-8', errors='replace')
            
            # Check: STA (r2),Y at PC=$6041
            if '6041' not in line_str[:8]:
                continue
            if 'STA (r2),Y' not in line_str:
                continue
            
            # Extract X register
            x_pos = line_str.find('X:')
            if x_pos < 0:
                continue
            try:
                x_val = int(line_str[x_pos+2:x_pos+4], 16)
            except:
                continue
            
            if x_val != target_bank_x:
                continue
            
            # Extract effective address from [$XXXX]
            bracket_pos = line_str.find('[$')
            if bracket_pos < 0:
                continue
            try:
                eff_addr = int(line_str[bracket_pos+2:bracket_pos+6], 16)
            except:
                continue
            
            if eff_addr < target_range_lo or eff_addr > target_range_hi:
                continue
            
            # Extract A register (data byte)
            a_pos = line_str.find('A:')
            if a_pos < 0:
                continue
            try:
                a_val = int(line_str[a_pos+2:a_pos+4], 16)
            except:
                continue
            
            # Extract frame
            fr_pos = line_str.find('Fr:')
            frame = "?"
            if fr_pos >= 0:
                fr_end = line_str.find(' ', fr_pos+3)
                if fr_end < 0:
                    fr_end = len(line_str)
                frame = line_str[fr_pos+3:fr_end]
            
            hits.append((frame, eff_addr, a_val, line_str.strip()))
        
        offset += len(raw)
        gb = offset / (1024*1024*1024)
        print(f"  Scanned {gb:.1f}GB... ({len(hits)} hits so far)")

print(f"\n=== Found {len(hits)} writes to bank 4, ${target_range_lo:04X}-${target_range_hi:04X} ===")
for frame, addr, data, line in hits:
    print(f"  Fr:{frame}  [{addr:04X}] = ${data:02X}  | {line[:120]}")

# Also summarize which addresses were written
if hits:
    addr_map = {}
    for frame, addr, data, line in hits:
        if addr not in addr_map:
            addr_map[addr] = []
        addr_map[addr].append((frame, data))
    
    print(f"\n=== Address summary ===")
    for addr in sorted(addr_map.keys()):
        writes = addr_map[addr]
        vals = ' '.join(f"Fr:{f}=${d:02X}" for f, d in writes)
        print(f"  ${addr:04X}: {len(writes)} writes: {vals}")

print("\nDone.")
