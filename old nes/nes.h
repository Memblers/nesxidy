#pragma section default
#ifndef NES_H
#define NES_H

// ******************************************************************************************

#ifndef TRACK_TICKS
#define FRAME_LENGTH	(const uint16_t) ((1789772 / 59.996811) / 4)	// estimate number of instructions // 4?
#else
#define FRAME_LENGTH	(const uint16_t) (1789772 / 59.996811)
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

#define ROM_OFFSET 0xC000


// ******************************************************************************************

extern uint8_t ROM_NAME[];	//ROM data

extern void reset6502();
extern void step6502();
extern void run6502();
extern void irq6502();
extern void nmi6502();
extern void hookexternal(void *funcptr);
__zpage extern uint32_t clockticks6502;
__zpage extern uint8_t status;

__zpage extern uint16_t decoded_address;
__zpage extern uint16_t encoded_address;

//#pragma section text0

uint8_t nes_gamepad(void);
void nes_register_write(uint16_t address, uint8_t value);
void render_video(void);
void convert_chr(uint8_t *source);
uint8_t read6502(uint16_t address);
void write6502(uint16_t address, uint8_t value);
void flash_format(void);

#endif
#pragma section default