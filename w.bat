@echo off
setlocal enabledelayedexpansion
del exidy.nes
del e:\drive-e\nes\sav\exidy.ips

rem --- Detect ENABLE_NATIVE_STACK from config.h → generate asm include ---
findstr /r /c:"^#define ENABLE_NATIVE_STACK" config.h >nul 2>&1
if !errorlevel! equ 0 (
    (echo ENABLE_NATIVE_STACK = 1) > asm_config.inc
    echo [Native Stack mode enabled]
) else (
    (echo ENABLE_NATIVE_STACK = 0) > asm_config.inc
)

vc +mapper30 -+ -c99 -O2 mapper30.c exidy.c fake6502.c dynamos.c frontend/cpu_6502.c platform/platform_exidy.c backend/emit_6502.c backend/ir.c backend/ir_opt.c backend/ir_opt_ext.c backend/ir_lower.c core/optimizer_v2_simple.c core/static_analysis.c core/metrics.c dynamos-asm.s -llazynes -o exidy.nes
rem if %errorlevel% neq 0 exit /b %errorlevel%
rem if %errorlevel% neq 0 pause
python convertlabels.py vicemap.map exidy.mlb

