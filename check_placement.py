# Let's verify: do functions land in their intended banks?
# By cross-referencing map addresses with ROM content

rom = open('exidy.nes', 'rb').read()
header = 16
bank_size = 0x4000

# The map addresses are virtual addresses. 
# For bank2 section: virtual $8000-$BFFF = physical ROM offset = header + 2 * bank_size + (addr - $8000)
# For text section (default): virtual $C000-$FFFF = physical ROM offset = header + 31 * bank_size + (addr - $C000)

# Let's check where the build actually placed key functions by looking at
# the vbcc-generated assembly output. But we don't have that.

# Alternative: look at what's in bank 2 at the expected offset of find_zp_mirror ($81DA)
# and compare with what we'd expect (the function code)

# Actually let's do something more reliable: compile with -asm flag to see section placement
# Or: check the .o intermediate files

# For now let's use a different approach: trace what the trampoline calls
# The crash is at convert_chr_b2. The trampoline switches to bank 25 and JSRs $81E1.
# If convert_chr_b2 is ACTUALLY in bank 31 (default), then its map address would be $C000+.
# But the trampoline was generated with the address from the linker, which would be whatever
# the linker assigned.

# Wait, let me re-read the map addresses for the functions that have warning 371:
funcs_371 = {
    'find_zp_mirror': 0x81DA,        # warning: bank2 ignored
    'find_zp_mirror_lo': 0x821C,     # warning: bank2 ignored
    'mirrored_ptrs': 0xA514,         # warning: bank2 ignored (DATA)
    'sa_record_subroutine': 0x91A0,  # warning: bank24 ignored
    'sa_subroutine_lookup': 0xAD22,  # warning: bank2 ignored
    'metrics_dump_sa_b2': 0x8278,    # warning: bank22 ignored
    'metrics_dump_runtime_b2': 0x830E, # warning: bank22 ignored
}

# Functions without warning 371 (should be in correct bank):
funcs_ok = {
    'render_video_b2': None,  # need to look up
}

# All addresses are in $8000-$BFFF range, which means they ARE in a swappable bank section.
# If they were in 'text' (default), they'd be at $C000+.
# So maybe vbcc DOES place them in the bankN section despite the warning?

# But then why are banks 22/24/25 empty? Let me check more carefully...

# Oh wait - maybe the functions are all being placed in bank 0 or bank 2 (default swappable bank)
# instead of their intended bank. The "default" section for C code might not be "text" but "text0"?

# Let's check: in vbcc, what section name does '#pragma section default' use?
# Answer: it uses "text" for code, "data" for initialized data, "bss" for uninitialized
# In the linker script, "text" goes to b31.
# But "#pragma section bank2" would put code in section "bank2" which goes to b2.
# "#pragma section default" would revert to "text" which goes to b31.

# So if the warning means the bank2/bank22/bank24 attribute is TRULY ignored,
# the code stays in "text" section → b31 → $C000-$FFFF

# BUT the map shows these functions at $8000-$BFFF... which contradicts that.
# UNLESS: the symbol addresses in the map are from BEFORE linking, just local offsets?

# No, 'al C:xxxx' in a VICE map file means absolute address xxxx.
# Let's check: if these functions are at $81DA in the FIXED bank ($C000-$FFFF),
# that's impossible because $81DA < $C000.

# Maybe vbcc generates different section names than I think. Let me check the
# compile output to see what sections vbcc actually emits.

# Actually, the most reliable test: build with -asm flag to see the generated assembly.
# Or: check what's in bank 2 vs bank 31 at specific offsets.

# Let me do a definitive test: find a known function in bank 31 (e.g. bankswitch_prg)
# and see its address pattern, then compare with the warning-371 functions.

# From vicemap.map, bankswitch_prg should be in the fixed bank:
# Let me search for it
print("=== Searching map for known fixed-bank functions ===")
with open('vicemap.map', 'r') as f:
    for line in f:
        if 'bankswitch' in line or 'nmi' in line.lower() or 'irq' in line.lower():
            print(line.strip())

print("\n=== Checking: do the warning-371 functions exist in bank 31? ===")
# In bank 31, addresses start at $C000. 
# If find_zp_mirror is at $81DA, it CANNOT be in bank 31 (which starts at $C000)
# So it must be in some bankN section that maps to $8000-$BFFF.
# The question is: WHICH bank?

# Each bankN section all map to $8000-$BFFF but go to different physical ROM locations.
# A function at virtual $81DA could be in ANY of the 31 swappable banks.
# The linker resolves the virtual address, but the physical location in ROM depends on the section.

# So the real test: check physical ROM byte at bank2's $81DA vs other banks' $81DA
# to see if find_zp_mirror's code is there.

# We need to know what find_zp_mirror's machine code looks like.
# It's a small function that loops through mirrored_ptrs[].
# Let me check all banks at offset $81DA and see which one has valid 6502 code.

print("\n=== Bytes at offset $01DA in each non-empty bank ===")
for bank in [0, 1, 2, 3, 25, 31]:
    if bank == 31:
        # bank 31 starts at $C000 in virtual space, $81DA doesn't exist there
        # $81DA in bank 31 would be at virtual $81DA? No - bank 31 is $C000-$FFFF
        # Actually in ROM, bank 31 physical offset = header + 31*bank_size
        # The $81DA function address is relative to $8000, so offset = $01DA
        # In bank 31's ROM space, offset $01DA would be virtual $C1DA
        rom_offset = header + 31 * bank_size + 0x01DA
    else:
        rom_offset = header + bank * bank_size + 0x01DA
    data = rom[rom_offset:rom_offset+24]
    print(f"  Bank {bank}: {' '.join(f'{b:02X}' for b in data)}")

# The same check but for sa_record_subroutine at $91A0 (offset $11A0 from $8000)
print("\n=== Bytes at offset $11A0 (sa_record_subroutine) in each non-empty bank ===")
for bank in [0, 1, 2, 3, 24, 25, 31]:
    rom_offset = header + bank * bank_size + 0x11A0
    data = rom[rom_offset:rom_offset+24]
    non_ff = sum(1 for b in data if b != 0xFF)
    if non_ff > 0:
        print(f"  Bank {bank}: {' '.join(f'{b:02X}' for b in data)}")
    else:
        print(f"  Bank {bank}: (all FF)")

# Check metrics functions too
print("\n=== Bytes at offset $0278 (metrics_dump_sa_b2) in each non-empty bank ===")
for bank in [0, 1, 2, 22, 31]:
    rom_offset = header + bank * bank_size + 0x0278
    data = rom[rom_offset:rom_offset+24]
    non_ff = sum(1 for b in data if b != 0xFF)
    if non_ff > 0:
        print(f"  Bank {bank}: {' '.join(f'{b:02X}' for b in data)}")
    else:
        print(f"  Bank {bank}: (all FF)")
