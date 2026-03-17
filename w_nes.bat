@echo off
setlocal enabledelayedexpansion

rem =====================================================================
rem w_nes.bat — Build DynaMoS for an NES (NROM-128) game.
rem
rem Usage:   w_nes.bat [game]
rem
rem   game = donkeykong (default), balloonfight, baseball, battlecity, binaryland,
rem          burgertime, defender, excitebike, exerion, galaxian, hypersports,
rem          loderunner, lunarpool, m82, zippyrace, door, golf, karateka,
rem          mariobros, millipede, pooyan, popeye, raidbung, roadfighter,
rem          skydestroyer, urbanchamp, warpman, yiear
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
if /i "%GAME%"=="balloonfight" set GAME_DEFINE=GAME_BALLOON_FIGHT& set ROM_BASE=balloonfight
if /i "%GAME%"=="baseball"    set GAME_DEFINE=GAME_BASEBALL&     set ROM_BASE=baseball
if /i "%GAME%"=="battlecity"  set GAME_DEFINE=GAME_BATTLE_CITY&  set ROM_BASE=battlecity
if /i "%GAME%"=="binaryland"  set GAME_DEFINE=GAME_BINARY_LAND&  set ROM_BASE=binaryland
if /i "%GAME%"=="burgertime"  set GAME_DEFINE=GAME_BURGER_TIME&  set ROM_BASE=burgertime
if /i "%GAME%"=="defender"    set GAME_DEFINE=GAME_DEFENDER&     set ROM_BASE=defender
if /i "%GAME%"=="excitebike"  set GAME_DEFINE=GAME_EXCITEBIKE&   set ROM_BASE=excitebike
if /i "%GAME%"=="exerion"     set GAME_DEFINE=GAME_EXERION&      set ROM_BASE=exerion
if /i "%GAME%"=="galaxian"    set GAME_DEFINE=GAME_GALAXIAN&     set ROM_BASE=galaxian
if /i "%GAME%"=="hypersports" set GAME_DEFINE=GAME_HYPER_SPORTS& set ROM_BASE=hypersports
if /i "%GAME%"=="loderunner"  set GAME_DEFINE=GAME_LODE_RUNNER&  set ROM_BASE=loderunner
if /i "%GAME%"=="lunarpool"   set GAME_DEFINE=GAME_LUNAR_POOL&   set ROM_BASE=lunarpool
if /i "%GAME%"=="m82"         set GAME_DEFINE=GAME_M82&          set ROM_BASE=m82
if /i "%GAME%"=="zippyrace"   set GAME_DEFINE=GAME_ZIPPY_RACE&   set ROM_BASE=zippyrace
if /i "%GAME%"=="door"        set GAME_DEFINE=GAME_DOOR&         set ROM_BASE=door
if /i "%GAME%"=="golf"        set GAME_DEFINE=GAME_GOLF&         set ROM_BASE=golf
if /i "%GAME%"=="karateka"    set GAME_DEFINE=GAME_KARATEKA&     set ROM_BASE=karateka
if /i "%GAME%"=="mariobros"   set GAME_DEFINE=GAME_MARIO_BROS&   set ROM_BASE=mariobros
if /i "%GAME%"=="millipede"   set GAME_DEFINE=GAME_MILLIPEDE&    set ROM_BASE=millipede
if /i "%GAME%"=="pooyan"      set GAME_DEFINE=GAME_POOYAN&       set ROM_BASE=pooyan
if /i "%GAME%"=="popeye"      set GAME_DEFINE=GAME_POPEYE&       set ROM_BASE=popeye
if /i "%GAME%"=="raidbung"    set GAME_DEFINE=GAME_RAID_BUNG&    set ROM_BASE=raidbung
if /i "%GAME%"=="roadfighter" set GAME_DEFINE=GAME_ROAD_FIGHTER& set ROM_BASE=roadfighter
if /i "%GAME%"=="skydestroyer" set GAME_DEFINE=GAME_SKY_DESTROYER& set ROM_BASE=skydestroyer
if /i "%GAME%"=="urbanchamp"  set GAME_DEFINE=GAME_URBAN_CHAMP&  set ROM_BASE=urbanchamp
if /i "%GAME%"=="warpman"     set GAME_DEFINE=GAME_WARPMAN&      set ROM_BASE=warpman
if /i "%GAME%"=="yiear"       set GAME_DEFINE=GAME_YIEAR&        set ROM_BASE=yiear

if "%GAME_DEFINE%"=="" (
    echo ERROR: Unknown game "%GAME%"
    echo Valid games: donkeykong balloonfight baseball battlecity binaryland
    echo              burgertime defender excitebike exerion galaxian hypersports
    echo              loderunner lunarpool m82 zippyrace door golf karateka
    echo              mariobros millipede pooyan popeye raidbung roadfighter
    echo              skydestroyer urbanchamp warpman yiear
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

rem --- Detect ENABLE_NATIVE_STACK from config.h (auto-sync C→asm) ---
set VASM_NS=
findstr /r /c:"^#define ENABLE_NATIVE_STACK" config.h >nul 2>&1
if !errorlevel! equ 0 (
    set VASM_NS=-DENABLE_NATIVE_STACK=1
    echo [Native Stack mode enabled]
)

rem --- Assemble dynamos-asm.s separately with NES defines ---
vasm6502_oldstyle -quiet -nowarn=62 -opt-branch -Fvobj -DGAME_NUMBER=10 -DPLATFORM_NES=1 !VASM_NS! dynamos-asm.s -o dynamos-asm-nes.o
if %errorlevel% neq 0 (
    echo ASM FAILED
    exit /b %errorlevel%
)

rem --- Compile C files and link with pre-assembled object ---
vc +mapper30 -+ -c99 -O2 -DPLATFORM_NES -D%GAME_DEFINE% mapper30.c nes.c fake6502.c dynamos.c frontend/cpu_6502.c platform/platform_nes.c backend/emit_6502.c backend/ir.c backend/ir_opt.c backend/ir_opt_ext.c backend/ir_lower.c core/optimizer_v2_simple.c core/static_analysis.c core/metrics.c dynamos-asm-nes.o -llazynes -o %OUTPUT%
if %errorlevel% neq 0 (
    echo BUILD FAILED
    exit /b %errorlevel%
)

echo BUILD OK: %OUTPUT%
python convertlabels.py vicemap.map nes_%ROM_BASE%.mlb
python patch_oam_dma.py %OUTPUT% vicemap.map
python patch_nmi_yield.py %OUTPUT% vicemap.map
