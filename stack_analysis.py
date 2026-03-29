#!/usr/bin/env python3
"""Check actual Exidy ROM data and analyze JSR template behavior in detail."""
import zlib

# Load NES ROM
with open(r"C:\proj\c\NES\nesxidy-co\nesxidy\exidy.nes", "rb") as f:
    rom = f.read()

HEADER_SIZE = 16
BANK_SIZE = 0x4000

def rom_offset(bank, addr):
    return HEADER_SIZE + bank * BANK_SIZE + (addr & 0x3FFF)

# Load savestate WRAM
with open(r"C:\Users\membl\OneDrive\Documents\Mesen2\SaveStates\exidy_1.mss", "rb") as f:
    data = f.read()

pos = 0
zblocks = []
while pos < len(data) - 2:
    if data[pos] == 0x78 and data[pos+1] in (0x01, 0x5E, 0x9C, 0xDA):
        try:
            d = zlib.decompressobj()
            r = d.decompress(data[pos:])
            c = len(data[pos:]) - len(d.unused_data)
            zblocks.append((pos, bytearray(r)))
            pos += c
            continue
        except:
            pass
    pos += 1

blk1 = zblocks[1][1]
wram_marker = b'mapper.workRam'
wp = blk1.find(wram_marker)
wram_after = wp + len(wram_marker) + 1
wram_sz = int.from_bytes(blk1[wram_after:wram_after+4], 'little')
wram = blk1[wram_after+4:wram_after+4+wram_sz]

ram_marker = b'memoryManager.internalRam'
p = blk1.find(ram_marker)
after_null = p + len(ram_marker) + 1
sz = int.from_bytes(blk1[after_null:after_null+4], 'little')
ram = blk1[after_null+4:after_null+4+sz]

# First, let's decode the compiled block at $9E00 (for PC=$2E0B)
bank4_off = rom_offset(4, 0x8000)  # bank 4, $8000 range
block_9E00 = rom[bank4_off + 0x1E00:bank4_off + 0x1E00 + 64]
print("=== Compiled block at $9E00 (for emulated PC=$2E0B) ===")
print("Raw bytes:")
for i in range(0, 64, 16):
    hex_str = ' '.join(f'{b:02X}' for b in block_9E00[i:i+16])
    print(f"  +{i:02X}: {hex_str}")

# Decode the JSR template
print("\nDecoded JSR template:")
i = 0
def decode_byte(data, pos, label=""):
    b = data[pos]
    if label:
        print(f"  +{pos:02X}: {b:02X}    ; {label}")
    return b

# The template: PHP STA_a STX_x STY_y LDX_sp PUSH_HI PUSH_LO STX_sp SET_PC JMP
# PHP
print(f"  +00: {block_9E00[0]:02X}         PHP")
# STA $6D
print(f"  +01: {block_9E00[1]:02X} {block_9E00[2]:02X}      STA ${block_9E00[2]:02X}")
# STX $6E
print(f"  +03: {block_9E00[3]:02X} {block_9E00[4]:02X}      STX ${block_9E00[4]:02X}")
# STY $6F
print(f"  +05: {block_9E00[5]:02X} {block_9E00[6]:02X}      STY ${block_9E00[6]:02X}")
# LDX $6C
print(f"  +07: {block_9E00[7]:02X} {block_9E00[8]:02X}      LDX ${block_9E00[8]:02X} (load SP)")

# Push hi byte
# LDA #imm
print(f"  +09: {block_9E00[9]:02X} {block_9E00[10]:02X}     LDA #${block_9E00[10]:02X} (ret_hi)")
# STA ($6E00+x) - absolute indexed 
# Actually: STA abs,X -> $9D lo hi
print(f"  +0B: {block_9E00[11]:02X} {block_9E00[12]:02X} {block_9E00[13]:02X}  STA ${block_9E00[13]:02X}{block_9E00[12]:02X},X")
# DEX
print(f"  +0E: {block_9E00[14]:02X}         DEX")

# Push lo byte  
print(f"  +0F: {block_9E00[15]:02X} {block_9E00[16]:02X}     LDA #${block_9E00[16]:02X} (ret_lo)")
print(f"  +11: {block_9E00[17]:02X} {block_9E00[18]:02X} {block_9E00[19]:02X}  STA ${block_9E00[19]:02X}{block_9E00[18]:02X},X")
print(f"  +14: {block_9E00[20]:02X}         DEX")

# STX $6C (update SP)
print(f"  +15: {block_9E00[21]:02X} {block_9E00[22]:02X}      STX ${block_9E00[22]:02X} (store SP)")

# Set target PC
# LDA #tgt_lo
print(f"  +17: {block_9E00[23]:02X} {block_9E00[24]:02X}     LDA #${block_9E00[24]:02X} (target PC lo)")
# STA $6A
print(f"  +19: {block_9E00[25]:02X} {block_9E00[26]:02X}      STA ${block_9E00[26]:02X}")
# LDA #tgt_hi
print(f"  +1B: {block_9E00[27]:02X} {block_9E00[28]:02X}     LDA #${block_9E00[28]:02X} (target PC hi)")
# STA $6B
print(f"  +1D: {block_9E00[29]:02X} {block_9E00[30]:02X}      STA ${block_9E00[30]:02X}")
# JMP flash_dispatch_return_no_regs ($6267)
print(f"  +1F: {block_9E00[31]:02X} {block_9E00[32]:02X} {block_9E00[33]:02X}  JMP ${block_9E00[33]:02X}{block_9E00[32]:02X}")

print(f"\nJSR template summary for block $2E0B:")
print(f"  Return address pushed: ${block_9E00[10]:02X}{block_9E00[16]:02X}")
print(f"  Target PC: ${block_9E00[28]:02X}{block_9E00[24]:02X}")
print(f"  JMP target: ${block_9E00[33]:02X}{block_9E00[32]:02X}")

# Now decode block at $9F00 (for PC=$2E58)
block_9F00 = rom[bank4_off + 0x1F00:bank4_off + 0x1F00 + 80]
print(f"\n=== Compiled block at $9F00 (for emulated PC=$2E58) ===")
print("Raw bytes:")
for i in range(0, 80, 16):
    hex_str = ' '.join(f'{b:02X}' for b in block_9F00[i:i+16])
    print(f"  +{i:02X}: {hex_str}")

# This block has regular instructions first, then a JSR template
# From prior analysis: LDA #$00, STA $6D15 (5 bytes), then JSR template (34 bytes)
# Let's decode
print(f"\nDecoded block $2E58:")
# LDA #00
print(f"  +00: {block_9F00[0]:02X} {block_9F00[1]:02X}     LDA #${block_9F00[1]:02X}")
# STA abs
print(f"  +02: {block_9F00[2]:02X} {block_9F00[3]:02X} {block_9F00[4]:02X}  STA ${block_9F00[4]:02X}{block_9F00[3]:02X}")

# Then JSR template at +05
jsr_off = 5
print(f"\n  JSR template at +{jsr_off:02X}:")
print(f"  +{jsr_off+0x00:02X}: {block_9F00[jsr_off+0]:02X}         PHP")
print(f"  +{jsr_off+0x01:02X}: {block_9F00[jsr_off+1]:02X} {block_9F00[jsr_off+2]:02X}      STA ${block_9F00[jsr_off+2]:02X}")
print(f"  +{jsr_off+0x03:02X}: {block_9F00[jsr_off+3]:02X} {block_9F00[jsr_off+4]:02X}      STX ${block_9F00[jsr_off+4]:02X}")
print(f"  +{jsr_off+0x05:02X}: {block_9F00[jsr_off+5]:02X} {block_9F00[jsr_off+6]:02X}      STY ${block_9F00[jsr_off+6]:02X}")
print(f"  +{jsr_off+0x07:02X}: {block_9F00[jsr_off+7]:02X} {block_9F00[jsr_off+8]:02X}      LDX ${block_9F00[jsr_off+8]:02X}")

hi_val = block_9F00[jsr_off+10]
lo_val = block_9F00[jsr_off+16]
print(f"  +{jsr_off+0x09:02X}: {block_9F00[jsr_off+9]:02X} {hi_val:02X}     LDA #${hi_val:02X} (ret_hi)")
print(f"  Push hi to stack via STA abs,X + DEX")
print(f"  +{jsr_off+0x0F:02X}: {block_9F00[jsr_off+15]:02X} {lo_val:02X}     LDA #${lo_val:02X} (ret_lo)")
print(f"  Push lo to stack via STA abs,X + DEX")

tgt_lo = block_9F00[jsr_off+24]
tgt_hi = block_9F00[jsr_off+28]
print(f"  Target PC: ${tgt_hi:02X}{tgt_lo:02X}")
print(f"  Return addr: ${hi_val:02X}{lo_val:02X}")

jmp_lo = block_9F00[jsr_off+32]
jmp_hi = block_9F00[jsr_off+33]
print(f"  JMP target: ${jmp_hi:02X}{jmp_lo:02X}")

# Now decode the epilogue
ep_off = jsr_off + 34  # 5 + 34 = 39
print(f"\n  Epilogue at +{ep_off:02X}:")
ep = block_9F00[ep_off:]
print(f"  +{ep_off+0:02X}: {ep[0]:02X}         PHP")
print(f"  +{ep_off+1:02X}: {ep[1]:02X}         CLC")
print(f"  +{ep_off+2:02X}: {ep[2]:02X} {ep[3]:02X}      BCC +{ep[3]:02X}")
# skip 4 bytes (PLP + JMP $FFFF)
print(f"  +{ep_off+4:02X}: {ep[4]:02X}         PLP")
print(f"  +{ep_off+5:02X}: {ep[5]:02X} {ep[6]:02X} {ep[7]:02X}  JMP ${ep[7]:02X}{ep[6]:02X}")
# BCC lands here at +ep_off+8
print(f"  +{ep_off+8:02X}: {ep[8]:02X} {ep[9]:02X}      STA ${ep[9]:02X}")
exit_lo = ep[11]
exit_hi = ep[14]
print(f"  +{ep_off+0xA:02X}: {ep[10]:02X} {ep[11]:02X}     LDA #${ep[11]:02X} (exit_pc lo)")
print(f"  +{ep_off+0xC:02X}: {ep[12]:02X} {ep[13]:02X}      STA ${ep[13]:02X}")
print(f"  +{ep_off+0xE:02X}: {ep[14]:02X} {ep[15]:02X}     LDA #${ep[15]:02X} (exit_pc hi)")  
# Wait, the order might be different. Let me just show raw bytes
print(f"\n  Epilogue raw (21 bytes from +{ep_off:02X}):")
for i in range(0, 21):
    print(f"    +{ep_off+i:02X}: {ep[i]:02X}")

# Now the key question: what does the emulated stack ACTUALLY show?
# And does it make sense with the execution flow?
print(f"\n=== Stack analysis ===")
emu_stack_off = 0x0E00
print(f"Emulated SP = $FB")
print(f"Stack contents ($01F8-$01FF):")
for i in range(0xF8, 0x100):
    val = wram[emu_stack_off + i]
    print(f"  ${0x100+i:04X}: ${val:02X}")

print(f"\nAnalysis:")
print(f"  $01FF: $28 - this is from a JSR BEFORE our compiled code")
print(f"  $01FE: $2D - (return to $282E? No: $2D,$28 -> lo=$2D hi=$28 -> RTS to $282E)")
print(f"  Wait: 6502 JSR pushes hi first, then lo.")
print(f"  So JSR at some addr pushed $28@$01FF (PCH) then $2D@$01FE (PCL)")
print(f"  RTS would pop lo=$2D, hi=$28 -> return to $282E")

print(f"\n  $01FD: $2E - from our JSR template (block $2E0B)")
print(f"  $01FC: $0D - from our JSR template (block $2E0B)")
print(f"  -> This is the return address for JSR $2E0B: lo=$0D hi=$2E -> RTS to $2E0E")

print(f"\n  But SP=$FB means something is at $01FC and above.")
print(f"  $01FB: $2D")
print(f"  $01FA: $8B")
print(f"  $01F9: $29")
print(f"  $01F8: $FE")
print(f"  These look like they could be from subroutine calls within $2CB0")

# Key insight: the JSR template for $2E58 should have pushed $2E, $5E 
# (return addr for JSR $2CB0). But we see $0D at $01FC and $2E at $01FD.
# This means the template for $2E0B pushed these (return $2E0E).
# And the template for $2E58 should have pushed BELOW that.
# 
# Wait: the JSR template PUSHES using the EMULATED SP. 
# The initial SP was $FF. 
# Template at $2E0B: LDX $6C (=FF), push $2E at ($6E00+FF)=$6EFF->$01FF, DEX
#                    push $0D at ($6E00+FE)=$6EFE->$01FE, DEX, STX $6C (=$FD)
# Then _pc=$2E58, returns to C.
# C dispatches $2E58, runs block $9F00.
# Block $9F00: LDA#00 STA$6D15, then JSR template:
#   LDX $6C (=$FD), push ret_hi at ($6E00+FD)=$6EFD->$01FD, DEX
#   push ret_lo at ($6E00+FC)=$6EFC->$01FC, DEX, STX $6C (=$FB)
# 
# So after both JSR templates:
#   $01FF: $28 (pre-existing, NOT from our JSR!)
#   $01FE: $2D (pre-existing)
#   $01FD: $2E (from 2nd JSR at $2E58, pushing ret_hi for $2E5F-1=$2E5E)
#   $01FC: $0D (WRONG! should be $5E for return lo of $2E5F-1=$2E5E)
#
# Wait: what return address does the JSR template at $2E58 push?
# The JSR instruction at $2E58 is "JSR $2CB0". It's at addresses $2E58,$2E59,$2E5A.
# The return address for 6502 JSR: push PC+2 (pointing at next instruction), 
# but actually JSR pushes PC+2-1. So push $2E5A. hi=$2E, lo=$5A.
# But no: our template doesn't use the real JSR return convention.
# Our template patches ret_hi and ret_lo as immediates.
# Let me check what values were patched...

print(f"\n=== Re-checking patched return addresses ===")
# Block $9E00 (PC=$2E0B): 
# The JSR at $2E0B goes to $2E58
# In dynamos.c, the return address is computed as pc + opsize (=3)
# So return = $2E0B + 3 = $2E0E
# Template patches: ret_hi = ($2E0E >> 8) = $2E, ret_lo = ($2E0E & 0xFF) = $0E
# BUT: the emulated stack convention is different from 6502!
# 6502 pushes PC+2 (=PC of last byte of JSR), and RTS adds 1.
# Our template pushes the ACTUAL return address, and the interpreter's
# opRTS should just pop and use it directly.
# Wait, need to check what opRTS does.

# Actually, the conversation says ret_lo is at offset 16 in the template.
# Let me re-read block $9E00's template bytes:
print(f"\nBlock $9E00 ret_hi (offset 10): ${block_9E00[10]:02X}")
print(f"Block $9E00 ret_lo (offset 16): ${block_9E00[16]:02X}")
print(f"Block $9E00 -> return address: ${block_9E00[10]:02X}{block_9E00[16]:02X}")

print(f"\nBlock $9F00 ret_hi (offset 5+10=15): ${block_9F00[15]:02X}")
print(f"Block $9F00 ret_lo (offset 5+16=21): ${block_9F00[21]:02X}")
print(f"Block $9F00 -> return address: ${block_9F00[15]:02X}{block_9F00[21]:02X}")

# Hmm wait, the offsets within the JSR template are defined as:
# JSR_TEMPLATE_PATCH_RET_HI = 10 (the byte at template[10] is the immediate for LDA #ret_hi)
# JSR_TEMPLATE_PATCH_RET_LO = 16
# So for block $9E00 (pure JSR template, starts at offset 0):
#   template[10] = ret_hi
#   template[16] = ret_lo
# For block $9F00 (JSR template starts at offset 5):
#   (template base + 10) = block[15] = ret_hi
#   (template base + 16) = block[21] = ret_lo
