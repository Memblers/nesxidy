#!/usr/bin/env python3
"""Trace block compilation for the PLA/PHA joystick routine at $35A6."""

# The block entry is likely $35A6 (after LDA $5101 which is interpreted)
# Guest code sequence:
#   $35A6: EOR #$FF     (2 bytes, imm)
#   $35A8: PHA          (1 byte)
#   $35A9: AND #$08     (2 bytes, imm)
#   $35AB: BNE $35C6    (2 bytes, forward branch)
#   $35AD: NOP          (1 byte)
#   $35AE: PLA          (1 byte)
#   $35AF: PHA          (1 byte)
#   $35B0: AND #$04     (2 bytes, imm)
#   $35B2: BNE $35C8    (2 bytes, forward branch)
#   $35B4: NOP          (1 byte)
#   $35B5: PLA          (1 byte)
#   $35B6: PHA          (1 byte)
#   $35B7: AND #$20     (2 bytes, imm)
#   $35B9: BNE $35C7    (2 bytes, forward branch)
#   $35BB: NOP          (1 byte)
#   $35BC: PLA          (1 byte)
#   $35BD: PHA          (1 byte)
#   $35BE: AND #$40     (2 bytes, imm)
#   $35C0: BNE $35C5    (2 bytes, forward branch)
#   $35C2: NOP          (1 byte)
#   $35C3: PLA          (1 byte)
#   $35C4: RTS          (1 byte)
#   $35C5: INX          (1 byte)
#   $35C6: INX          (1 byte)
#   $35C7: INX          (1 byte)
#   $35C8: PLA          (1 byte)  <- note: PLA after the branches land

# Trace compilation with peephole:
bfs = 0  # block_flags_saved
code_index = 0
print("Tracing block compilation from $35A6:")
print(f"{'PC':>6} {'Opcode':>10} {'bfs_in':>6} {'skip':>4} {'trim':>4} {'flush':>5} {'code_sz':>7} {'ci_out':>6} {'bfs_out':>7} {'INTERPRETED?':>12}")
print("-" * 95)

opcodes = [
    (0x35A6, "EOR #FF", "imm"),
    (0x35A8, "PHA", "pha"),
    (0x35A9, "AND #08", "imm"),
    (0x35AB, "BNE $35C6", "branch_fwd"),
    (0x35AD, "NOP", "nop"),
    (0x35AE, "PLA", "pla"),
    (0x35AF, "PHA", "pha"),
    (0x35B0, "AND #04", "imm"),
    (0x35B2, "BNE $35C8", "branch_fwd"),
    (0x35B4, "NOP", "nop"),
    (0x35B5, "PLA", "pla"),
    (0x35B6, "PHA", "pha"),
    (0x35B7, "AND #20", "imm"),
    (0x35B9, "BNE $35C7", "branch_fwd"),
    (0x35BB, "NOP", "nop"),
    (0x35BC, "PLA", "pla"),
    (0x35BD, "PHA", "pha"),
    (0x35BE, "AND #40", "imm"),
    (0x35C0, "BNE $35C5", "branch_fwd"),
    (0x35C2, "NOP", "nop"),
    (0x35C3, "PLA", "pla"),
    (0x35C4, "RTS", "rts"),
    # Branch targets (if block continues to them):
    (0x35C5, "INX", "inx"),
    (0x35C6, "INX", "inx"),
    (0x35C7, "INX", "inx"),
    (0x35C8, "PLA", "pla"),
]

CODE_SIZE = 211  # 256 - 21 (EPILOGUE_SIZE) - 18 (XBANK_EPILOGUE_SIZE) - 6

for pc, name, typ in opcodes:
    if code_index >= CODE_SIZE - 6:
        print(f"  === OUT_OF_CACHE at ${pc:04X}, code_index={code_index} ===")
        break
    
    bfs_in = bfs
    skip = 0
    trim = 0
    flush = 0
    interp = False
    sz = 0
    
    if typ in ("pha", "pla"):
        # emit_template path
        # PHA template: PHP...PLP (13 bytes), starts with PHP (0x08), ends with PLP (0x28)
        # PLA template: PHP...PLP (13 bytes), starts with PHP (0x08), ends with PLP (0x28)
        tmpl_starts_php = True
        tmpl_ends_plp = True
        tmpl_sz = 13
        
        if tmpl_starts_php:
            if bfs:
                skip = 1
            if tmpl_ends_plp:
                trim = 1
        
        sz = tmpl_sz - skip - trim
        bfs = trim  # 1 if trim, 0 if not
    elif typ == "imm":
        # Flush check first
        if bfs:
            flush = 1
            code_index += 1  # PLP byte
            bfs = 0
        sz = 3  # opcode rewritten to abs (zp->abs or imm stays 2... wait, AND #imm is 2 bytes native)
        # Actually AND # immediate is 2 bytes in 6502, stays 2 bytes in compiled code
        sz = 2
    elif typ == "branch_fwd":
        # Flush check
        if bfs:
            flush = 1
            code_index += 1
            bfs = 0
        sz = 21  # patchable template
    elif typ == "nop":
        # Flush check (NOP is a regular opcode going through recompile_opcode_b2)
        if bfs:
            flush = 1
            code_index += 1
            bfs = 0
        sz = 0  # NOP emits 0 bytes
    elif typ == "rts":
        # Flush check
        if bfs:
            flush = 1
            code_index += 1
            bfs = 0
        # RTS causes enable_interpret() -> block ends
        interp = True
        sz = 0
    elif typ == "inx":
        if bfs:
            flush = 1
            code_index += 1
            bfs = 0
        sz = 1  # INX is 1 byte
    
    ci_before = code_index
    code_index += sz
    
    interp_mark = ""
    if interp:
        interp_mark = "INTERPRET"
    elif skip:
        interp_mark = "SKIP->INTERP"
    
    print(f"${pc:04X}  {name:>10}  {bfs_in:>5}  {skip:>4}  {trim:>4}  {flush:>5}  {sz:>6}  {code_index:>6}  {bfs:>6}  {interp_mark:>12}")
    
    if interp:
        print(f"  === Block ends (interpret) at ${pc:04X} ===")
        break

print(f"\nFinal code_index: {code_index}")
print(f"Final block_flags_saved: {bfs}")
