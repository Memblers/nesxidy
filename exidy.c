#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "lazynes.h"
#include "config.h"
#include "exidy.h"
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


// ******************************************************************************************

//#pragma section text0

int main(void)
{	
	lnSync(1);
	lnPush(0x3F00, 32, palette);
#ifdef ENABLE_CHR_ROM
	bankswitch_prg(1);
	convert_chr((uint8_t*)chr_sidetrac);
	bankswitch_prg(0);
#endif
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
		step6502();
		#else
		run_6502();
		#endif
	
#ifndef TRACK_TICKS		
		if (frame_time++ > FRAME_LENGTH)
		{
			frame_time = 0;
			interrupt_condition |= FLAG_EXIDY_IRQ;			
			render_video();
			//irq6502();
		}
#else
		if (clockticks6502 > frame_time)
		{
			frame_time += FRAME_LENGTH;
			interrupt_condition |= FLAG_EXIDY_IRQ;
			render_video();
			//irq6502();
		}	
#endif	
		if (!(status & FLAG_INTERRUPT) && (interrupt_condition & FLAG_EXIDY_IRQ))
		{
			//irq6502();			
		}
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
	if (address < 0x400)		
		return RAM_BASE[address];	
	if (address < 0x4000)
	{
		bankswitch_prg(1);
		uint8_t temp = ROM_NAME[address - ROM_OFFSET];
		bankswitch_prg(0);
		return temp;
		return ROM_NAME[address - ROM_OFFSET];
	}
	if (address < 0x4800)
		return SCREEN_RAM_BASE[address - 0x4000];
#ifdef ENABLE_CHR_ROM
	if (address < 0x5000)
	{
		bankswitch_prg(1);
		uint8_t temp = CHR_NAME[address - CHR_OFFSET];
		bankswitch_prg(0);
		return temp;		
		return CHR_NAME[address - CHR_OFFSET];
	}
#else	
	if (address < 0x5000)
		return IO8(CHARACTER_RAM_BASE + address - 0x4800);	
#endif	
	
	if (address == 0x5100)	// $5100 DIP switch settings
		return 0xED;
	if (address == 0x5101)	// $5101 control inputs port
		return nes_gamepad();
	if (address == 0x5103)	// $5103 interrupt condition latch
	{
		uint8_t vblank = interrupt_condition;
		interrupt_condition &= ~FLAG_EXIDY_IRQ;
		if (nes_gamepad() && TARG_COIN1)
			return (0x40 | vblank);
		else
			return (0x00 | vblank);
	}
		//return 0xC0;

	if (address >= 0xFF00)
	{		
		bankswitch_prg(1);
		uint8_t temp = ROM_NAME[(address & 0x3FFF) - ROM_OFFSET];
		bankswitch_prg(0);
		return temp;
		return ROM_NAME[(address & 0x3FFF) - ROM_OFFSET];
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
	if (address < 0x400)
		RAM_BASE[address] = value;
	else if (address == 0x5000)
		wummel_x_position = (value + 16) ^ 0xFF;
	else if (address == 0x5040)
		wummel_y_position = (value + 16) ^ 0xFF;
	else if (address == 0x5080)
		IO8(0x207) = value;
	else if (address == 0x50C0)
		IO8(0x204) = value;
	else if ((address >= 0x4000) && (address < 0x4800))
	{		
		SCREEN_RAM_BASE[address - 0x4000] = value;
		screen_ram_updated = 1;
	}
	else if ((address >= 0x4800) && (address < 0x5000))
	{
		CHARACTER_RAM_BASE[address - 0x4800] = value;
		character_ram_updated = 1;
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
	if (screen_ram_updated)
	{		
		lnSync(1);
		screen_ram_updated = 0;
		lnPush(0x2000, 0, SCREEN_RAM_BASE);
		lnPush(0x2100, 0, SCREEN_RAM_BASE+0x100);
		lnPush(0x2200, 0, SCREEN_RAM_BASE+0x200);
		lnPush(0x2300, 0xC0, SCREEN_RAM_BASE+0x300);
		lnPush(0x2800, 0x40, SCREEN_RAM_BASE+0x3C0);
	}	

#ifndef ENABLE_CHR_ROM
	if (character_ram_updated)
	{
		lnSync(1);
		character_ram_updated = 0;
		convert_chr(CHARACTER_RAM_BASE);		
	}
#endif	

	static const unsigned char object[5] = { 0, 0, 10, 3, 128 };
	lnAddSpr(object, wummel_x_position, wummel_y_position);

	lnSync(0);
	return;
}

// ******************************************************************************************

void convert_chr(uint8_t *source)
{
	uint16_t tiles = 0;
	uint16_t nes_address = 0;
	uint16_t emu_address = 0;

	for (; tiles < 64; tiles++)
	{			
		IO8(0x2006) = nes_address >> 8;
		IO8(0x2006) = nes_address & 0xFF;
		for (uint8_t i = 0; i < 8; i++)
		{
			IO8(0x2007) = source[emu_address + i];
		}
		nes_address += 16;
		emu_address += 8;
	}
	
	nes_address += 0x8;
	
	for (; tiles < 128; tiles++)
	{			
		IO8(0x2006) = nes_address >> 8;
		IO8(0x2006) = nes_address & 0xFF;
		for (uint8_t i = 0; i < 8; i++)
		{
			IO8(0x2007) = source[emu_address + i];
		}
		nes_address += 16;
		emu_address += 8;
	}
	
	nes_address -= 0x8;
	
	for (; tiles < 192; tiles++)
	{			
		IO8(0x2006) = nes_address >> 8;
		IO8(0x2006) = nes_address & 0xFF;
		for (uint8_t i = 0; i < 8; i++)
		{
			IO8(0x2007) = source[emu_address + i];
		}
		for (uint8_t i = 0; i < 8; i++)
		{
			IO8(0x2007) = source[emu_address + i];
		}
		nes_address += 16;
		emu_address += 8;
	}
	
	nes_address += 0x8;
	
	for (; tiles < 256; tiles++)
	{			
		IO8(0x2006) = nes_address >> 8;
		IO8(0x2006) = nes_address & 0xFF;
		for (uint8_t i = 0; i < 8; i++)
		{
			IO8(0x2007) = source[emu_address + i];
		}
		nes_address += 16;
		emu_address += 8;
	}
}


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


