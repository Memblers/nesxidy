del exidy.nes
del e:\drive-e\nes\sav\exidy.ips
vc +mapper30 -+ -c99 -O2 mapper30.c exidy.c fake6502.c dynamos.c dynamos-asm.s disasm.s frontend/cpu_6502.c platform/platform_exidy.c backend/emit_6502.c core/optimizer.c core/optimizer_fixed.c core/optimizer_v2.c core/opt_backup.c core/opt_trampoline.s -llazynes -o exidy.nes
rem if %errorlevel% neq 0 exit /b %errorlevel%
rem if %errorlevel% neq 0 pause
python convertlabels.py vicemap.map exidy.mlb
