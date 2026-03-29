@echo off
setlocal enabledelayedexpansion

rem =====================================================================
rem w_asteroids.bat — Build DynaMoS for Atari Asteroids arcade
rem
rem Usage:   w_asteroids.bat
rem
rem Asteroids arcade hardware:
rem   6502 CPU @ 1.512 MHz
rem   1KB RAM ($0000-$03FF)
rem   6KB program ROM ($6800-$7FFF, 3x 2KB chips)
rem   2KB vector ROM ($4800-$4FFF)
rem   DVG (Digital Vector Generator) — emulated in software
rem   POKEY ($2000-$200F) for sound/random
rem =====================================================================

set OUTPUT=asteroids.nes
echo Building %OUTPUT% (PLATFORM_ASTEROIDS)...

rem --- Clean old output ---
del %OUTPUT% 2>nul

rem --- Detect ENABLE_NATIVE_STACK from config.h ---
set VASM_NS=
findstr /r /c:"^#define ENABLE_NATIVE_STACK" config.h >nul 2>&1
if !errorlevel! equ 0 (
    set VASM_NS=-DENABLE_NATIVE_STACK=1
    echo [Native Stack mode enabled]
)

rem --- Assemble dynamos-asm.s with Asteroids defines ---
rem GAME_NUMBER=6 selects Asteroids ROM incbin blocks
rem PLATFORM_ASTEROIDS=1 selects correct BSS layout and bank assignments
vasm6502_oldstyle -quiet -nowarn=62 -opt-branch -Fvobj -DGAME_NUMBER=6 -DPLATFORM_ASTEROIDS=1 !VASM_NS! dynamos-asm.s -o dynamos-asm-asteroids.o
if %errorlevel% neq 0 (
    echo ASM FAILED
    exit /b %errorlevel%
)

rem --- Compile C files and link ---
rem Uses asteroids.c as main driver
rem -DPLATFORM_ASTEROIDS selects Asteroids memory map, bank layout, etc.
rem -DGAME_ASTEROIDS selects ROM address range, idle PC, etc.
vc +mapper30 -+ -c99 -O2 -DPLATFORM_ASTEROIDS -DGAME_ASTEROIDS mapper30.c asteroids.c fake6502.c dynamos.c frontend/cpu_6502.c platform/platform_asteroids.c backend/emit_6502.c backend/ir.c backend/ir_opt.c backend/ir_opt_ext.c backend/ir_lower.c core/optimizer_v2_simple.c core/static_analysis.c core/metrics.c dynamos-asm-asteroids.o -llazynes -o %OUTPUT%
if %errorlevel% neq 0 (
    echo BUILD FAILED
    exit /b %errorlevel%
)

echo BUILD OK: %OUTPUT%

rem --- Post-build patches ---
rem Convert vbcc linker labels for Mesen debugger
python convertlabels.py vicemap.map asteroids.mlb

rem Patch NMI yield hook
python patch_nmi_yield.py %OUTPUT% vicemap.map
