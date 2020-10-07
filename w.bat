del exidy.nes
del e:\drive-e\nes\sav\exidy.ips
vc +nrom256w -+ -c99 -O2 mapper30.c exidy.c fake6502.c dynamos.c dynamos-asm.s disasm.s -llazynes -o exidy.nes
rem if %errorlevel% neq 0 exit /b %errorlevel%
if %errorlevel% neq 0 pause
convertlabels.py vicemap.map exidy.mlb
exidy.nes