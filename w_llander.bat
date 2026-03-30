@echo off
setlocal enabledelayedexpansion

rem =====================================================================
rem w_llander.bat — Build DynaMoS for Atari Lunar Lander arcade
rem
rem Usage:   w_llander.bat
rem
rem Lunar Lander arcade hardware:
rem   6502 CPU @ 1.512 MHz
rem   256B RAM ($0000-$00FF, mirrored to $1FFF)
rem   8KB program ROM ($6000-$7FFF, 4x 2KB chips)
rem   6KB vector ROM ($4800-$5FFF, 3x 2KB chips)
rem   DVG (Digital Vector Generator) — emulated in software
rem   Analog thrust lever via ADC ($2C00)
rem =====================================================================

set OUTPUT=llander.nes
echo Building %OUTPUT% (PLATFORM_LLANDER)...

rem --- Clean old output ---
del %OUTPUT% 2>nul

rem --- Detect ENABLE_NATIVE_STACK from config.h ---
set VASM_NS=
findstr /r /c:"^#define ENABLE_NATIVE_STACK" config.h >nul 2>&1
if !errorlevel! equ 0 (
    set VASM_NS=-DENABLE_NATIVE_STACK=1
    echo [Native Stack mode enabled]
)

rem --- Assemble dynamos-asm.s with Lunar Lander defines ---
rem GAME_NUMBER=7 selects Lunar Lander ROM incbin blocks
rem PLATFORM_LLANDER=1 selects correct BSS layout and bank assignments
vasm6502_oldstyle -quiet -nowarn=62 -opt-branch -Fvobj -DGAME_NUMBER=7 -DPLATFORM_LLANDER=1 !VASM_NS! dynamos-asm.s -o dynamos-asm-llander.o
if %errorlevel% neq 0 (
    echo ASM FAILED
    exit /b %errorlevel%
)

rem --- Compile C files and link ---
rem Uses llander.c as main driver
rem -DPLATFORM_LLANDER selects Lunar Lander memory map, bank layout, etc.
rem -DGAME_LLANDER selects ROM address range, etc.
vc +mapper30 -+ -c99 -O2 -DPLATFORM_LLANDER -DGAME_LLANDER mapper30.c llander.c fake6502.c dynamos.c frontend/cpu_6502.c platform/platform_llander.c backend/emit_6502.c backend/ir.c backend/ir_opt.c backend/ir_opt_ext.c backend/ir_lower.c core/optimizer_v2_simple.c core/static_analysis.c core/metrics.c dynamos-asm-llander.o -llazynes -o %OUTPUT%
if %errorlevel% neq 0 (
    echo BUILD FAILED
    exit /b %errorlevel%
)

echo BUILD OK: %OUTPUT%

rem --- Post-build patches ---
rem Convert vbcc linker labels for Mesen debugger
python convertlabels.py vicemap.map llander.mlb

rem Patch NMI yield hook
python patch_nmi_yield.py %OUTPUT% vicemap.map
