@echo off
setlocal enabledelayedexpansion

rem =====================================================================
rem w_millipede.bat — Build DynaMoS for Atari Millipede arcade
rem
rem Usage:   w_millipede.bat
rem
rem Millipede arcade hardware:
rem   6502 CPU @ 1.512 MHz
rem   16KB program ROM ($4000-$7FFF)
rem   4KB character/sprite ROM (2 × 2KB)
rem   Rotated 240×256 display (displayed sideways on NES)
rem   2× POKEY sound chips (stubbed)
rem =====================================================================

set OUTPUT=millipede.nes
echo Building %OUTPUT% (PLATFORM_MILLIPEDE)...

rem --- Clean old output ---
del %OUTPUT% 2>nul

rem --- Detect ENABLE_NATIVE_STACK from config.h ---
set VASM_NS=
findstr /r /c:"^#define ENABLE_NATIVE_STACK" config.h >nul 2>&1
if !errorlevel! equ 0 (
    set VASM_NS=-DENABLE_NATIVE_STACK=1
    echo [Native Stack mode enabled]
)

rem --- Assemble dynamos-asm.s with Millipede defines ---
rem GAME_NUMBER=5 selects Millipede ROM incbin blocks
rem PLATFORM_MILLIPEDE=1 selects correct BSS layout and bank assignments
vasm6502_oldstyle -quiet -nowarn=62 -opt-branch -Fvobj -DGAME_NUMBER=5 -DPLATFORM_MILLIPEDE=1 !VASM_NS! dynamos-asm.s -o dynamos-asm-millipede.o
if %errorlevel% neq 0 (
    echo ASM FAILED
    exit /b %errorlevel%
)

rem --- Compile C files and link ---
rem Uses millipede.c as main driver (replaces exidy.c / nes.c)
rem -DPLATFORM_MILLIPEDE selects Millipede memory map, bank layout, etc.
rem -DGAME_MILLIPEDE_ARCADE selects ROM address range, idle PC, etc.
vc +mapper30 -+ -c99 -O2 -DPLATFORM_MILLIPEDE -DGAME_MILLIPEDE_ARCADE mapper30.c millipede.c fake6502.c dynamos.c frontend/cpu_6502.c platform/platform_millipede.c backend/emit_6502.c backend/ir.c backend/ir_opt.c backend/ir_opt_ext.c backend/ir_lower.c core/optimizer_v2_simple.c core/static_analysis.c core/metrics.c dynamos-asm-millipede.o -llazynes -o %OUTPUT%
if %errorlevel% neq 0 (
    echo BUILD FAILED
    exit /b %errorlevel%
)

echo BUILD OK: %OUTPUT%

rem --- Post-build patches ---
rem Convert vbcc linker labels for Mesen debugger
python convertlabels.py vicemap.map millipede.mlb

rem OAM DMA patch not needed for Millipede — we use $0200 directly.
rem python patch_oam_dma.py %OUTPUT% vicemap.map

rem Patch NMI yield hook
python patch_nmi_yield.py %OUTPUT% vicemap.map

rem Patch lnList for fast nametable updates
python patch_lnlist.py %OUTPUT%

rem NMI sp-guard: disabled for now — the __reg calling convention mismatch
rem in the lnPush_safe wrapper was the real cause of the black screen.
rem Exidy works fine without NMI patching; re-enable only if sp corruption
rem is confirmed as a real problem in later testing.
rem python patch_nmi_sp.py %OUTPUT% vicemap.map
