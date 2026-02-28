@echo off
del nes_dk.nes 2>nul

rem Assemble dynamos-asm.s separately with NES defines
vasm6502_oldstyle -quiet -nowarn=62 -opt-branch -Fvobj -DGAME_NUMBER=10 -DPLATFORM_NES=1 dynamos-asm.s -o dynamos-asm-nes.o
if %errorlevel% neq 0 (
    echo ASM FAILED
    exit /b %errorlevel%
)

rem Compile C files and link with pre-assembled object
vc +mapper30 -+ -c99 -O2 -DPLATFORM_NES -DGAME_DONKEY_KONG mapper30.c nes.c fake6502.c dynamos.c frontend/cpu_6502.c platform/platform_nes.c backend/emit_6502.c core/optimizer_v2_simple.c core/static_analysis.c core/metrics.c dynamos-asm-nes.o -llazynes -o nes_dk.nes
if %errorlevel% neq 0 (
    echo BUILD FAILED
    exit /b %errorlevel%
)
echo BUILD OK: nes_dk.nes
python convertlabels.py vicemap.map nes_dk.mlb
python patch_oam_dma.py nes_dk.nes vicemap.map
