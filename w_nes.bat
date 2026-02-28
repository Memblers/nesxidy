@echo off
setlocal enabledelayedexpansion

rem =====================================================================
rem w_nes.bat — Build DynaMoS for an NES (NROM-128) game.
rem
rem Usage:   w_nes.bat [game]
rem
rem   game = donkeykong (default), battlecity, burgertime, defender,
rem          galaxian, hypersports, loderunner, m82, zippyrace
rem
rem The script:
rem   1. Splits the .nes file to .prg/.chr if needed
rem   2. Copies .prg/.chr to roms\nes\_active.prg/chr
rem   3. Assembles + compiles + links
rem   4. Runs post-build patches (OAM DMA, label conversion)
rem =====================================================================

rem --- Default game ---
set GAME=donkeykong
if not "%~1"=="" set GAME=%~1

rem --- Map game name to compiler define and ROM base name ---
set GAME_DEFINE=
set ROM_BASE=

if /i "%GAME%"=="donkeykong"  set GAME_DEFINE=GAME_DONKEY_KONG&  set ROM_BASE=donkeykong
if /i "%GAME%"=="dk"          set GAME_DEFINE=GAME_DONKEY_KONG&  set ROM_BASE=donkeykong
if /i "%GAME%"=="battlecity"  set GAME_DEFINE=GAME_BATTLE_CITY&  set ROM_BASE=battlecity
if /i "%GAME%"=="burgertime"  set GAME_DEFINE=GAME_BURGER_TIME&  set ROM_BASE=burgertime
if /i "%GAME%"=="defender"    set GAME_DEFINE=GAME_DEFENDER&     set ROM_BASE=defender
if /i "%GAME%"=="galaxian"    set GAME_DEFINE=GAME_GALAXIAN&     set ROM_BASE=galaxian
if /i "%GAME%"=="hypersports" set GAME_DEFINE=GAME_HYPER_SPORTS& set ROM_BASE=hypersports
if /i "%GAME%"=="loderunner"  set GAME_DEFINE=GAME_LODE_RUNNER&  set ROM_BASE=loderunner
if /i "%GAME%"=="m82"         set GAME_DEFINE=GAME_M82&          set ROM_BASE=m82
if /i "%GAME%"=="zippyrace"   set GAME_DEFINE=GAME_ZIPPY_RACE&   set ROM_BASE=zippyrace

if "%GAME_DEFINE%"=="" (
    echo ERROR: Unknown game "%GAME%"
    echo Valid games: donkeykong battlecity burgertime defender
    echo              galaxian hypersports loderunner m82 zippyrace
    exit /b 1
)

set OUTPUT=nes_%ROM_BASE%.nes
echo Building %OUTPUT% (%GAME_DEFINE%)...

rem --- Split .nes to .prg/.chr if needed ---
if exist "roms\nes\%ROM_BASE%.nes" (
    if not exist "roms\nes\%ROM_BASE%.prg" (
        echo Splitting %ROM_BASE%.nes...
        python split_nes_rom.py "roms\nes\%ROM_BASE%.nes" "roms\nes"
        if %errorlevel% neq 0 (
            echo SPLIT FAILED
            exit /b %errorlevel%
        )
    )
)

rem --- Verify ROM files exist ---
if not exist "roms\nes\%ROM_BASE%.prg" (
    echo ERROR: roms\nes\%ROM_BASE%.prg not found
    exit /b 1
)
if not exist "roms\nes\%ROM_BASE%.chr" (
    echo ERROR: roms\nes\%ROM_BASE%.chr not found
    exit /b 1
)

rem --- Copy to _active.prg/chr for the assembler ---
copy /y "roms\nes\%ROM_BASE%.prg" "roms\nes\_active.prg" >nul
copy /y "roms\nes\%ROM_BASE%.chr" "roms\nes\_active.chr" >nul

rem --- Clean old output ---
del %OUTPUT% 2>nul

rem --- Assemble dynamos-asm.s separately with NES defines ---
vasm6502_oldstyle -quiet -nowarn=62 -opt-branch -Fvobj -DGAME_NUMBER=10 -DPLATFORM_NES=1 dynamos-asm.s -o dynamos-asm-nes.o
if %errorlevel% neq 0 (
    echo ASM FAILED
    exit /b %errorlevel%
)

rem --- Compile C files and link with pre-assembled object ---
vc +mapper30 -+ -c99 -O2 -DPLATFORM_NES -D%GAME_DEFINE% mapper30.c nes.c fake6502.c dynamos.c frontend/cpu_6502.c platform/platform_nes.c backend/emit_6502.c core/optimizer_v2_simple.c core/static_analysis.c core/metrics.c dynamos-asm-nes.o -llazynes -o %OUTPUT%
if %errorlevel% neq 0 (
    echo BUILD FAILED
    exit /b %errorlevel%
)

echo BUILD OK: %OUTPUT%
python convertlabels.py vicemap.map nes_%ROM_BASE%.mlb
python patch_oam_dma.py %OUTPUT% vicemap.map
