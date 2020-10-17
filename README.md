# nesxidy V0.6 Alpha
Copyright ©2020 Joseph Parsell

additional credits:\
Fake6502 by Mike Chambers, interpreted emulator\
vbcc by Volker Barthelmann, C compiler used for development\
lazyNES by Matthias Bock, NES library for vbcc

## what it is
nesxidy is an experimental emulator that runs on the NES, which emulates a number of arcade games Exidy made on their 6502-based board.  In this Alpha version, the emulation is extremely slow and the games are not fully playable.  That could change in future versions.

#### partial support
* Side Track (1979)
* Targ (1980)
* Spectar (1980)

## how to use
1. Install vbcc for 6502 - http://www.compilers.de/vbcc.html
1. place (unzipped) ROM files into their respective ROM folders - https://www.mamedev.org/roms/
1. Modify *config.h* and *dynamos-asm.s* to select the game to include (a better selection system could be added later)
1. run w.bat to build
1. run in NES emulator

## why is this
This emulator is a demonstration of DynaMOS, dynamic recompiler targeting the MOS Technology 6502 CPU as the host system.  In this case, another 6502 is the guest system, as well.  The reason one can't simply run 6502 code from one machine on different 6502 machine is because the memory map may be completely different.  As it is a full-blown recompiler, much of the framework (with a significant amount of additional work) could be reused to host another CPU.

## how the hell
Every single instruction must be evaluated and replaced with equivalent sequences, every single memory access examined and remapped, and all of it reassembled, to suit the target system.  For any instructions that can't be recompiled, such as a branch or jump to a destination that hasn't been recompiled yet, or any unsupported instructions, control is handed over to an interpreter emulator (Fake6502).  Handling an interpreted instructions may take several hundred times longer than the native equivalent, but as more of the emulated program has been run, more branch destinations have become known, and can be assembled into a native 6502 branch.

## minimum system requirements
* NMOS 6502 or compatible CPU
* about 8kB of RAM, plus additional 4kB scratch RAM
* 512kB NOR Flash, SST39SF040
For NES: mapper 30 variant, 512kB Flash, 8kB PRG-RAM, 32kB CHR-RAM, 4-screen nametable.

## emulation features
Supports up to 64kB of emulated RAM/ROM\
6502 disassembler output to screen

## technical details
The 512kB of Flash memory is reserved as such:
* 32kB emulation program
* 32kB emulated ROM
* 64kB program counter flags and bank numbers
* 128kB program counter jump table
* 240kB cache blocks
* 16kB cache block map

This allows for 960 cache blocks, of 256 bytes each.  The program counter jump table allows for a virtual code space covering every possible program counter position for the emulated CPU.  The cache blocks may contain only 2 or 3 instructions, initially.  As the program counter jump table is populated, cache blocks will be more likely to contain longer sequences of instructions, and possibly entire subroutines.

To be added:
Once the 960 cache blocks are full, an optimization pass can occur that will consolidate the code into a smaller number of cache blocks, eliminating the many jump instructions between blocks, and freeing up more entries in the cache map.

## known issues
* coin input not accepted in Targ, Spectar
* sprites incorrect/disabled. Can't see the killer train in Side Track, can't see the player in Spectar and Targ
* IRQ emulation is currently disabled
* Spectar hangs

## to do / wish list
possible next steps
* finalize FlashROM cache support
* preserve cache across power cycles
* second optimization pass
* NES emulator
* debug breakpoints and single-step disassembly
* code/data flag for detection of self-modifying code
* additional CPU support.  Chip-8, 8080, Z80, LR35902..?
* partial TMS9918 emulation (CreatiVision (6502), Colecovision (Z80), MSX (Z80)

## version history
V0.6A - experimental support for new cache block format in FlashROM.  Much slower emulation, as the new system is unoptimized and the old system is still being used at the same time.  First public release.\
V0.5 - changed cartridge hardware to use mapper 30 with FlashROM and WRAM.\
V0.4 - added cache linking stage.\
V0.3 - DynaMOS system added. Speed approx 1.5 seconds per frame, hamstrung by limited RAM for cache.\
V0.2 - added Exidy I/O hardware emulation.  Speed approx 2 seconds per frame.\
V0.1 - interpreter-only emulator.






For personal use only.  Do not distribute any copyrighted ROM files with this emulator without specific permission.
