rom = open('exidy.nes', 'rb').read()
header = 16
bank_size = 0x4000

# The key question: are the banked functions in the CORRECT bank?
# Expected placements:
#   find_zp_mirror, find_zp_mirror_lo -> bank 2  (bank2 section in dynamos.c)
#   mirrored_ptrs -> bank 2  (data, bank2 section)
#   metrics_dump_sa_b2, metrics_dump_runtime_b2 -> bank 22 (Exidy) or bank 19 (NES)
#   sa_subroutine_lookup -> bank 2  (bank2 section)
#   sa_record_subroutine -> bank 24  (bank24 section)
#   convert_chr_b2 -> bank 25  (bank25 section, init code)
#   render_video_b2 -> bank 22  (bank22 section, render code)

# But ACTUALLY: bank 22 is EMPTY, bank 24 is EMPTY, bank 25 has some data
# Only banks 0,1,2,3,25,31 have content.

# Let's verify the vicemap.map - what section does each function go to?
# By reading the map file for section assignments

print("=== Reading linker map for section assignments ===")
with open('vicemap.map', 'r') as f:
    map_lines = f.readlines()

# Find section boundaries
for line in map_lines:
    line = line.strip()
    if 'text' in line.lower() and ('section' in line.lower() or 'segment' in line.lower()):
        print(line)

print("\n=== Looking for section info ===")
for line in map_lines:
    line = line.strip()
    # Look for lines defining sections/segments
    if line.startswith('section') or line.startswith('SECTION') or 'bank' in line.lower():
        if 'al ' not in line:  # skip address labels
            print(line)

# Actually let me just dump the first 100 non-label lines
print("\n=== Map file structure (first 100 non-al lines) ===")
count = 0
for line in map_lines:
    if not line.strip().startswith('al '):
        print(line.rstrip())
        count += 1
        if count > 100:
            break
