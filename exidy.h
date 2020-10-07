#pragma section default
#ifndef EXIDY_H
#define EXIDY_H

// ******************************************************************************************

#ifndef TRACK_TICKS
#define FRAME_LENGTH	(const uint16_t) ((705562 / 59.996811) / 4)	// estimate number of instructions
#else
#define FRAME_LENGTH	(const uint16_t) (705562 / 59.996811)
#endif

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
void render_video(void);
void convert_chr(uint8_t *source);
uint8_t read6502(uint16_t address);
void write6502(uint16_t address, uint8_t value);
void flash_format(void);

#endif
#pragma section default