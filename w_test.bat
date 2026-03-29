@echo off
REM Build script for CPU 6502 Test ROM
REM Requires: config.h has GAME_CPU_6502_TEST defined
REM Assembles cpu_6502_test.asm first, then builds NES ROM

echo Assembling cpu_6502_test.asm...
e:\drive-e\nes\dev\asm6.exe cpu_6502_test.asm cpu_6502_test.bin
if errorlevel 1 (
    echo ASM failed
    exit /b 1
)

echo Assembling dynamos-asm.s with GAME_NUMBER=4...
vasm6502_oldstyle -quiet -nowarn=62 -opt-branch -Fvobj -DGAME_NUMBER=4 dynamos-asm.s -o dynamos-asm-test.o
if errorlevel 1 (
    echo VASM failed
    exit /b 1
)

echo Building test ROM...
del exidy.nes 2>nul
vc +mapper30 -+ -c99 -O2 mapper30.c exidy.c fake6502.c dynamos.c frontend/cpu_6502.c platform/platform_exidy.c backend/emit_6502.c backend/ir.c backend/ir_opt.c backend/ir_lower.c core/optimizer_v2_simple.c core/static_analysis.c core/metrics.c dynamos-asm-test.o -llazynes -o exidy.nes
if errorlevel 1 (
    echo Build FAILED
    exit /b 1
)

python convertlabels.py vicemap.map exidy.mlb
echo Build successful: exidy.nes (CPU test)
