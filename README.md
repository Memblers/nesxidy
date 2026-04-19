# nesxidy V0.9
Copyright ©2020,2026 Joseph Parsell

additional credits:\
Fake6502 by Mike Chambers, interpreted emulator\
vbcc by Volker Barthelmann, C compiler used for development\
lazyNES by Matthias Bock, NES library for vbcc\

## what it is
nesxidy is an experimental multi-platform emulator that runs on the NES, which originally had emulated a number of arcade games Exidy made on their 6502-based board.  Though the emulation is slow and the games are virtually unplayable as a result, limited support for some other 6502-based systems have been added.

#### partial support
* Side Track (1979)
* Targ (1980)
* Spectar (1980)
* Asteroids (1979)
* Lunar Lander (1979)
* Millipede (1982)

#### platform support
* Famicom (1983) - NROM 16kB PRG + 8kB CHR

## how to use
1. Install vbcc for 6502 - http://www.compilers.de/vbcc.html
1. place (unzipped) ROM files into their respective ROM folders - https://www.mamedev.org/roms/
1. Modify *config.h* to set global and per-game settings
1. run w.bat to build Side Track, which is playable.  Other w*.bat files build other games or platforms.
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
Vector display simulation by sprite dots\
Pre-rendered downscaled tileset for Asteroids\
IR optimization\
idle loop detection\
address swizzling (must be set manually though, see Side Track)

## technical details
The 512kB of Flash memory is reserved as such:
* 48kB emulation program
* 32kB emulated ROM
* 64kB program counter flags and bank numbers
* 128kB program counter jump table
* 224kB cache blocks
* 16kB cache block map

The program counter jump table allows for a virtual code space covering every possible program counter position for the emulated CPU.  The emulator first does a static analysis pass, to identify code entry points.  During execution, if the target hasn't been compiled yet, it is compiled into a 6502-like IR (intermediate representation) and optimized there.  This only works on "basic blocks" which are pretty small, so it's impact is limited, as it is now.  To handle branches to unknown destinations, a template format is used in flash with self-modifying code, by changing individual 1 bits to 0 in the instruction operands.  For example, the return to interpreter path is reached by JMP $FFF0, and that instruction may be patched to any location on a 16-byte boundary.\

The emulator is has an optional static recompilation pass that may be used.  The performance is only slightly better than the dynamic mode.  It's set to automatically trigger when the cache is nearly full.\

Select + B triggers a static recompilation pass on supported platforms.\
Select + A triggers branch link search on supported platforms.\

## known issues
* coin input not accepted in Targ, Spectar
* Spectar hangs
* other Exidy games have be selected by modifying *dynamos-asm.s*
* Millipede controls not working, wrong colors, attribute clash
* Asteroids player ship and thrust not visible
* Lunar Lander, not playable


## to do / wish list
possible next steps
* additional CPU support.  Chip-8, 8080, Z80, 6809, LR35902..?
* partial TMS9918 emulation (CreatiVision)
* improved static recompilation quality
* move to newer mapper, such as Rainbow, allowing 8x8 attribute for display emulation, and more memory.


## version history
V0.9 - flash cache support, vector graphic dot render support, Famicom support, Atari arcade games, optimization passes.\
V0.6A - experimental support for new cache block format in FlashROM.  Much slower emulation, as the new system is unoptimized and the old system is still being used at the same time.  First public release.\
V0.5 - changed cartridge hardware to use mapper 30 with FlashROM and WRAM.\
V0.4 - added cache linking stage.\
V0.3 - DynaMOS system added. Speed approx 1.5 seconds per frame, hamstrung by limited RAM for cache.\
V0.2 - added Exidy I/O hardware emulation.  Speed approx 2 seconds per frame.\
V0.1 - interpreter-only emulator.

## ai use disclosure
Claude Opus 4.5, 4.6 were used to revive this project.  I was surprised with how well it worked for this compared to other models, as a debugging tool, outputting an implementation, and doing refactors of the code easily, this project has become a playground for me to try different things.  The code in /backend, /core, /frontend, /platform, /tools is generated output based on planning files.  The source codes in the root folder are the original ones authored by myself and others, though they have been modified heavily over the course of various implemenation sessions.



For personal use only.  Do not distribute any copyrighted ROM files with this emulator without specific permission.


Copyright ©2020,2026 Joseph Parsell

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
