#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "lazynes.h"
#include "config.h"
#include "nes.h"
#include "dynamos.h"
#include "mapper30.h"


// ******************************************************************************************

// Spectar
#ifdef GAME_SPECTAR
	//#include "spectar\spectar.h"
	extern const unsigned char rom_sidetrac[];
	const uint8_t palette[] = { 0x01,0x16,0x21,0x2A, 0x01,0x21,0x11,0x01, 0x01,0x28,0x18,0x08, 0x01,0x26,0x16,0x06, 0x01,0x24,0x14,0x04, 0x01,0x24,0x14,0x04, 0x01,0x24,0x14,0x04, 0x01,0x24,0x14,0x04 };
#endif

// Side Track
#ifdef GAME_SIDE_TRACK
#pragma section bank1
	extern const unsigned char rom_sidetrac[];
	extern const unsigned char chr_sidetrac[];
#pragma section default
	const uint8_t palette[] = { 0x0F,0x30,0x30,0x28, 0x0F,0x21,0x11,0x01, 0x0F,0x28,0x18,0x08, 0x0F,0x26,0x16,0x06, 0x0F,0x24,0x14,0x04, 0x0F,0x24,0x14,0x04, 0x0F,0x24,0x14,0x04, 0x0F,0x24,0x14,0x04 };	
#endif

// Targ
#ifdef GAME_TARG
	const uint8_t palette[] = { 0x01,0x16,0x21,0x28, 0x01,0x21,0x11,0x01, 0x01,0x28,0x18,0x08, 0x01,0x26,0x16,0x06, 0x01,0x24,0x14,0x04, 0x01,0x24,0x14,0x04, 0x01,0x24,0x14,0x04, 0x01,0x24,0x14,0x04 };
#endif

// Targ Test
#ifdef GAME_TARG_TEST_ROM
	extern const uint8_t rom_targtest[];
	const uint8_t palette[] = { 0x01,0x16,0x21,0x28, 0x01,0x21,0x11,0x01, 0x01,0x28,0x18,0x08, 0x01,0x26,0x16,0x06, 0x01,0x24,0x14,0x04, 0x01,0x24,0x14,0x04, 0x01,0x24,0x14,0x04, 0x01,0x24,0x14,0x04 };
#endif

// ******************************************************************************************

extern uint8_t RAM_BASE[];
extern uint8_t SCREEN_RAM_BASE[];
extern uint8_t CHARACTER_RAM_BASE[];

__zpage uint8_t interrupt_condition;
__zpage uint8_t character_ram_updated = 0;
__zpage uint8_t screen_ram_updated = 0;
__zpage uint8_t wummel_x_position = 0;
__zpage uint8_t wummel_y_position = 0;

__zpage uint32_t frame_time;
__zpage uint8_t audio = 0;

uint8_t debug_out[0x80];
__zpage extern uint8_t sp;
__zpage extern uint16_t pc;
__zpage extern uint8_t a;
__zpage extern uint8_t x;
__zpage extern uint8_t y;
__zpage extern uint8_t opcode;

uint8_t ppu_queue_index = 0;
uint8_t ppu_queue[256];
uint8_t ppu_queue_ready = 0;

uint8_t PPUCTRL_soft = 0;
uint8_t PPUMASK_soft = 0;
uint8_t PPUADDR_soft[2]; // = {0};
uint8_t PPUADDR_latch = 0;
uint8_t PPUSCROLL_soft[2]; // = {0};


// ******************************************************************************************

//#pragma section text0

int main(void)
{	
	lnSync(1);
	lnPush(0x3F00, 32, palette);

	convert_chr((uint8_t*)chr_sidetrac);
	bankswitch_prg(0);
	IO8(0x201) = 0x01;
	
	lnSync(0);
	reset6502();		
	
	lnPPUCTRL &= ~0x08;
	lnPPUMASK = 0x3A;
	
	flash_format();
	
	interrupt_condition = 0;

#ifdef TRACK_TICKS
	frame_time = clockticks6502 + FRAME_LENGTH;
#else
	frame_time = 0;
#endif	
	
	while (1)
	{
				
		#ifdef INTERPRETER_ONLY
		interpret_6502(); //step6502();
		#else
		run_6502();
		#endif
		
		/*		
		if (opcode == 0x40)	// Donkey Kong, Popeye
		{
			if (PPUCTRL_soft & 0x80)
				nmi6502();
		}
		*/		


#ifndef TRACK_TICKS		
		if (frame_time++ > FRAME_LENGTH)
		{
			frame_time = 0;
			//interrupt_condition |= FLAG_EXIDY_IRQ;			
			render_video();
			if (PPUCTRL_soft & 0x80)
				nmi6502();
		}
#else
		if (clockticks6502 > frame_time)
		{
			frame_time += FRAME_LENGTH;
			//interrupt_condition |= FLAG_EXIDY_IRQ;
			render_video();
			if (PPUCTRL_soft & 0x80)
				nmi6502();
		}	
#endif	
/*
		if (!(status & FLAG_INTERRUPT) && (interrupt_condition & FLAG_EXIDY_IRQ))
		{
			irq6502();			
		}
*/
		
		#ifdef DEBUG_OUT
		debug_out[0] = (pc & 0xFF);
		debug_out[1] = (pc >> 8);
		debug_out[2] = sp;
		debug_out[3] = status;
		debug_out[4] = a;
		debug_out[5] = x;
		debug_out[6] = y;
		debug_out[7] = opcode;
		#endif
		
	}
	
	return 0;
}

// ******************************************************************************************

uint8_t read6502(uint16_t address)
{
#ifdef DEBUG_CPU_READ
	printf("RD $%02X:$%02X\n", ROM_NAME[(address & 0x3FFF) - ROM_OFFSET], address);
#endif
#ifdef DEBUG_AUDIO
	audio ^= address >> 8;
	IO8(0x4011) = audio;
#endif
/*
#ifdef DEBUG_OUT
	debug_out[8] = (address & 0xFF);
	debug_out[9] = (address >> 8);
#endif
*/
	if (address < 0x8000)
	{
		if (address < 0x2000)		
			return RAM_BASE[(address & 0x7FF)];	
		else if (address < 0x5000)
			return IO8(address);
	}
	else if (address >= 0xC000)
	{
		bankswitch_prg(1);	// 16kB Programs only for now
		uint8_t temp = ROM_NAME[address - 0xC000];
		bankswitch_prg(0);
		return temp;		
	}
	else	// $8000-$BFFF
	{
		bankswitch_prg(1);
		uint8_t temp = ROM_NAME[address - ROM_OFFSET];
		bankswitch_prg(0);
		return temp;		
	}

	return IO8(address); // OK, just read whatever then	
	
}

// ******************************************************************************************

void write6502(uint16_t address, uint8_t value)
{
#ifdef DEBUG_CPU_WRITE
	printf("WR $%02X:$%02X\n", value, address);
#endif
#ifdef DEBUG_AUDIO
	audio ^= value;
	IO8(0x4011) = audio;
#endif
/*
#ifdef DEBUG_OUT
	debug_out[10] = (address & 0xFF);
	debug_out[11] = (address >> 8);
#endif	
*/
	if (address < 0x2000)
		RAM_BASE[(address & 0x7FF)] = value;
	else if (address < 0x4000)
		nes_register_write(address, value); //IO8(address) = value;
	else if (address < 0x5000)
	{
		if (address == 0x4014)
			return;
		else
			IO8(address) = value;
	}
	else if (address < 0x8000)
		debug_out[0x10 + (address & 0x3f)] = value;

	return;
}

// ******************************************************************************************

void nes_register_write(uint16_t address, uint8_t value)
{
	switch (address)
	{
		case 0x2000:
		{
			lnPPUCTRL = (value | 0x80);
			PPUCTRL_soft = value;
			lnSync(0);
			break;
		}
		case 0x2001:
		{
			lnPPUMASK = value;
			PPUMASK_soft = value;
			lnSync(0);
			break;
		}				
		case 0x2002:
		case 0x2003:
		case 0x2004:
		{
			break;
		}
		case 0x2005:
		{
			PPUSCROLL_soft[PPUADDR_latch++ & 1] = value;
			lnScroll(PPUSCROLL_soft[0], PPUSCROLL_soft[1]);
			break;
		}
		case 0x2006:
		{
			PPUADDR_soft[PPUADDR_latch++ & 1] = value;
			break;
		}
		case 0x2007:
		{
			/*			
			static uint16_t addr_old = 0;
			static uint8_t ppu_data_index = 0;
			if (address == addr_old)
			{
				ppu_queue[ppu_queue_index - 2] += 1;
				ppu_queue[ppu_queue_index + ppu_data_index++] = value;
				if (ppu_data_index > 0x10)
				{
					ppu_queue_index += ppu_data_index;
					address = 0; // invalidate next match					
				}
			}
			else
			{
				ppu_queue[ppu_queue_index + 0] = PPUADDR_soft[0];
				ppu_queue[ppu_queue_index + 0] |= ((PPUCTRL_soft & 0x4) ? lfVer : lfHor);
				ppu_queue[ppu_queue_index + 1] = PPUADDR_soft[1];
				ppu_queue[ppu_queue_index + 2] = 1;
				ppu_queue[ppu_queue_index + 3] = value;				
				ppu_queue_index += 4;
				ppu_data_index = 0;
			}			
			addr_old = address;
			*/
			
			
			/*
			ppu_queue[ppu_queue_index + 0] = PPUADDR_soft[0];
			ppu_queue[ppu_queue_index + 0] |= ((PPUCTRL_soft & 0x4) ? lfVer : lfHor);
			ppu_queue[ppu_queue_index + 1] = PPUADDR_soft[1];
			ppu_queue[ppu_queue_index + 2] = 1;
			ppu_queue[ppu_queue_index + 3] = value;				
			ppu_queue_index += 4;
			*/
			
			ppu_queue[ppu_queue_index + 0] = PPUADDR_soft[0];			
			ppu_queue[ppu_queue_index + 1] = PPUADDR_soft[1];
			ppu_queue[ppu_queue_index + 2] = value;				
			ppu_queue_index += 3;			
			
			if (PPUCTRL_soft & 0x4)	// inc by 32
			{
				if ((PPUADDR_soft[1] & 0xE0) == 0xE0)
					PPUADDR_soft[0]++;
				PPUADDR_soft[1] += 32;
				
			}
			
			else	// inc by 1
			{				
				if (++PPUADDR_soft[1] == 0)
					PPUADDR_soft[0]++;				
			}			
			
			if (ppu_queue_index >= 96)
				//ppu_queue[++ppu_queue_index] = 0xFF;
				render_video();			
			break;
		}
	}
	
	return;
}


// ******************************************************************************************

uint8_t nes_gamepad(void)
{	
	uint8_t targ = 0;		
	uint8_t joypad = lnGetPad(1);
	targ |= (joypad & lfR) ? TARG_RIGHT : 0x00;
	targ |= (joypad & lfL) ? TARG_LEFT : 0x00;
	targ |= (joypad & lfU) ? TARG_UP : 0x00;
	targ |= (joypad & lfD) ? TARG_DOWN : 0x00;
	targ |= (joypad & lfA) ? TARG_FIRE : 0x00;
	targ |= (joypad & lfSelect) ? TARG_COIN1 : 0x00;
	targ |= (joypad & lfStart) ? TARG_1START : 0x00;	
	return (targ ^= 0xFF);
}

// ******************************************************************************************

void render_video(void)
{
	ppu_queue[ppu_queue_index] = lfEnd;
	lnList(ppu_queue);	
	ppu_queue_index = 0;
	lnSync(0);
	return;
}

// ******************************************************************************************

void convert_chr(uint8_t *source)
{	
	bankswitch_prg(2);
	IO8(0x2006) = 0;
	IO8(0x2006) = 0;
	for (uint16_t index = 0; index < 0x2000; index++)	
		IO8(0x2007) = chr_sidetrac[index];		
	return;
}


// ******************************************************************************************
/*
void backup_chr(void)
{
	uint8_t buffer[256];	
	uint16_t index;
	for (uint8_t page = 0; page < 4; page++)
	{
		bankswitch_chr(0x00);
		IO8(0X2006) = 0x20 + page;
		IO8(0X2006) = 0x00;
		for (index = 0; index < 0x100; index++)
		{
			buffer[index] = IO8(0x2007);
		}
		bankswitch_chr(0x20);
		IO8(0X2006) = 0x20 + page;
		IO8(0X2006) = 0x00;
		for (index = 0; index < 0x100; index++)
		{
			IO8(0x2007) = buffer[index];
		}
	}
	return;
}
*/		

// ******************************************************************************************
void flash_format(void)
{	
	for (uint8_t bank = 3; bank < 31; bank++)
	{		
		for (uint16_t sector = 0x8000; sector < 0xC000; sector += 0x1000)
		{
			flash_sector_erase(sector, bank);			
		}
	}
}

// ******************************************************************************************


