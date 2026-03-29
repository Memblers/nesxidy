import re
with open(r'c:\proj\c\NES\nesxidy-co\nesxidy\roms\milliped\Millipede.asm') as f:
    text = f.read()
named = re.findall(r'^([a-z]\w+):', text, re.MULTILINE)
generic_L = re.findall(r'^(L[0-9A-F]+):', text, re.MULTILINE)
generic_sub = re.findall(r'^(sub_[0-9A-F]+):', text, re.MULTILINE)
subs_named = [n for n in named if not n.startswith('sub_') and not n.startswith('cold')]
blocks = len(re.findall(r'^; ={10,}', text, re.MULTILINE)) // 2
print(f"Named subroutines: {len(subs_named)}")
print(f"Generic sub_XXXX: {len(generic_sub)}")
print(f"Branch labels (LXXXX): {len(generic_L)}")
print(f"Block comment sections: {blocks}")
print()
print("Named subroutines:")
for n in sorted(subs_named):
    print(f"  {n}")
