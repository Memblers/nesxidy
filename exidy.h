#pragma section default
#ifndef EXIDY_H
#define EXIDY_H

#include "config.h"

// Redirect to NES header when building for NES platform.
// All shared code includes "exidy.h" — this avoids changing those includes.
#ifdef PLATFORM_NES
#include "nes.h"
#elif defined(PLATFORM_MILLIPEDE)
#include "millipede.h"
#else
// ******************************************************************************************

#ifndef TRACK_TICKS
#define FRAME_LENGTH	(const uint16_t) ((705562 / 59.996811) / 24)	// estimate number of instructions
//#define FRAME_LENGTH	(const uint16_t) ((705562 / 59.996811) / 4)	// estimate number of instructions
#else
#define FRAME_LENGTH	(const uint16_t) (705562 / 59.996811)
#endif

// Dispatch overhead compensation: each main-loop dispatch (run_6502 call)
// costs ~200+ real NES CPU cycles in C overhead, dispatch_on_pc assembly,
// bank switching, etc.  But the guest cycle counter only advances by the
// emulated instruction count (~3-7 cycles for a tight loop iteration).
// This constant adds extra guest-equivalent cycles per dispatch so that
// busy-wait delay loops (DEX;BNE, DEC zp;BNE) consume their correct
// proportion of frame time instead of running 40-100x slower than real
// hardware.  Tuned so a 65536-iteration delay loop takes ~4 seconds
// (matching the ~4.3s on real Exidy hardware at 705 kHz).
#define DISPATCH_OVERHEAD	80

#define TARG_1START 1
#define TARG_2START 2
#define TARG_RIGHT	4
#define TARG_LEFT	8
#define TARG_FIRE	16
#define TARG_UP		32
#define TARG_DOWN	64
#define TARG_COIN1	128

#define FLAG_INTERRUPT 	0x04	// 6502 status flag
#define FLAG_DECIMAL   0x08

#define FLAG_EXIDY_IRQ	0x80

#define IO8(addr) (*(volatile uint8_t *)(addr))

// ******************************************************************************************

// Spectar
#ifdef GAME_SPECTAR
	#define ROM_OFFSET 0x1000	// Spectar
	//#define ROM_NAME rom_spectar
#endif

// Side Track
#ifdef GAME_SIDE_TRACK
	#define ROM_OFFSET 0x2800	                            
	#define ENABLE_CHR_ROM	1
	#define CHR_OFFSET 0x4800
	#define CHR_NAME chr_sidetrac
#endif

// Targ
#ifdef GAME_TARG
	#define ROM_OFFSET 0x1800	// Targ
	//#define ROM_NAME rom_targ	
#endif


// Targ Test
#ifdef GAME_TARG_TEST_ROM
	#define ROM_OFFSET 0x1800
	//#define ROM_NAME rom_targtest
#endif

// CPU 6502 Test
#ifdef GAME_CPU_6502_TEST
#define ROM_OFFSET 0x2800
#define ROM_NAME rom_cpu6502test
#endif

// ******************************************************************************************

extern uint8_t ROM_NAME[];	//ROM data

extern void reset6502();
extern void step6502();
extern void run6502();
extern void irq6502();
extern void hookexternal(void *funcptr);
__zpage extern uint32_t clockticks6502;
__zpage extern uint8_t status;

__zpage extern uint16_t decoded_address;
__zpage extern uint16_t encoded_address;

//#pragma section text0

uint8_t nes_gamepad(void);
void nes_gamepad_refresh(void);
void render_video(void);
void convert_chr(uint8_t *source);
uint8_t read6502(uint16_t address);
void write6502(uint16_t address, uint8_t value);
void flash_format(void);

#ifdef ENABLE_CACHE_PERSIST
void cache_write_signature(void);
uint8_t cache_check_signature(void);
void flash_init_persist(void);
#endif

#endif  // PLATFORM_NES else
#endif  // EXIDY_H
#pragma section default