#!/usr/bin/python
from io import open
import sys
import re;

if len(sys.argv) != 3:
    print("Converts vbcc's vicelabels to Mesen mlb lables.")
    print("\tUsage: ./convertlabels.py vicelabels.map output.mlb")
    sys.exit()

expr = re.compile(r"al C:([0-9a-zA-Z]*) \.(.*)")
with open(sys.argv[1], "r") as file, open(sys.argv[2], "w") as outFile:
    for line in file:
        match = expr.match(line)
        addr = int(match.group(1), 16)
        if addr < 0x2000:
            outFile.write(f'R:{format(addr, "04X")}:{match.group(2)}\n')
        elif addr >= 0x6000 and addr < 0x8000:
            outFile.write(f'S:{format(addr - 0x6000, "04X")}:{match.group(2)}\n')
            outFile.write(f'W:{format(addr - 0x6000, "04X")}:{match.group(2)}\n')
        else:
            outFile.write(f'P:{format(addr - 0x8000, "04X")}:{match.group(2)}\n')