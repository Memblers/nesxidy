#pragma section default
#ifndef NES_H
#define NES_H

#include "config.h"

// ******************************************************************************************
// NES platform header — Donkey Kong (NROM-128: 16KB PRG, 8KB CHR)
// ******************************************************************************************

// Frame timing
#ifndef TRACK_TICKS
#define FRAME_LENGTH	(const uint16_t) ((1789772 / 60) / 1)	// estimate number of instructions
#else
#define FRAME_LENGTH	(const uint32_t) (1789772 / 60)
#endif

// Dispatch overhead — same concept as Exidy, tuned for NES clock
#define DISPATCH_OVERHEAD	80

// Guest ROM offset — NROM-128 PRG starts at $C000 (mirrored at $8000)
#define ROM_OFFSET 0xC000

// Gamepad button mapping (NES standard controller)
// These match the Exidy TARG_* names so shared code (nes_gamepad) compiles
#define TARG_1START	0x01
#define TARG_2START	0x02
#define TARG_RIGHT	0x04
#define TARG_LEFT	0x08
#define TARG_FIRE	0x10
#define TARG_UP		0x20
#define TARG_DOWN	0x40
#define TARG_COIN1	0x80

#define FLAG_INTERRUPT	0x04	// 6502 I flag
#define FLAG_DECIMAL	0x08

#define IO8(addr) (*(volatile uint8_t *)(addr))

// ******************************************************************************************

extern uint8_t ROM_NAME[];	// PRG-ROM data (assembly symbol)

extern void reset6502(void);
extern void step6502(void);
extern void run6502(void);
extern void irq6502(void);
extern void nmi6502(void);
__zpage extern uint32_t clockticks6502;
__zpage extern uint8_t status;

__zpage extern uint16_t decoded_address;
__zpage extern uint16_t encoded_address;

// NMI guard state (used by ENABLE_BATCH_DISPATCH in dynamos.c)
extern uint8_t nmi_active;
extern uint8_t nmi_sp_guard;
__zpage extern uint8_t last_nmi_frame;
__zpage extern uint8_t nmi_yield;

// Platform functions
uint8_t nes_gamepad(void);
void nes_gamepad_refresh(void);
void render_video(void);
uint8_t read6502(uint16_t address);
void write6502(uint16_t address, uint8_t value);
void flash_format(void);

#ifdef ENABLE_CACHE_PERSIST
void cache_write_signature(void);
uint8_t cache_check_signature(void);
void flash_init_persist(void);
#endif

#endif
#pragma section default
