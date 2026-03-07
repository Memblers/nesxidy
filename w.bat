del exidy.nes
del e:\drive-e\nes\sav\exidy.ips
vc +mapper30 -+ -c99 -O2 mapper30.c exidy.c fake6502.c dynamos.c dynamos-asm.s frontend/cpu_6502.c platform/platform_exidy.c backend/emit_6502.c backend/ir.c backend/ir_opt.c backend/ir_opt_ext.c backend/ir_lower.c core/optimizer_v2_simple.c core/static_analysis.c core/metrics.c -llazynes -o exidy.nes
rem if %errorlevel% neq 0 exit /b %errorlevel%
rem if %errorlevel% neq 0 pause
python convertlabels.py vicemap.map exidy.mlb

