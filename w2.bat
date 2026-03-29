@echo off
REM Build script for V2 testing - STANDALONE (no exidy.c dependencies)
REM Uses dynamos_v2.c with standalone v2 headers and symbols

del exidy_v2.nes
vc +mapper30 -+ -c99 -O2 mapper30.c fake6502.c dynamos_v2.c dynamos_v2_stubs.c disasm.s -llazynes -o exidy_v2.nes
python convertlabels.py vicemap.map exidy.mlb

if errorlevel 1 (
    echo Compilation failed
    exit /b 1
) else (
    echo Build successful: exidy_v2.nes
)
