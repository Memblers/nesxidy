#include <stdint.h>
#include "mapper30.h"
#include "exidy.h"
#include "lazynes.h"

#pragma section default

uint8_t mapper_prg_bank = 0;
uint8_t mapper_chr_bank = 0;
uint8_t mapper_register = 0;

// ******************************************************************************************
#pragma section data
void flash_byte_program(uint16_t addr, uint8_t bank, uint8_t data)
{
	IO8(0x2000) = lnPPUCTRL & 0x7F;
	IO8(0xC000) = 0x01 | mapper_chr_bank;
	IO8(0x9555) = 0xAA;
	IO8(0xC000) = 0x00 | mapper_chr_bank;
	IO8(0xAAAA) = 0x55;
	IO8(0xC000) = 0x01 | mapper_chr_bank;
	IO8(0x9555) = 0xA0;
	IO8(0xC000) = bank | mapper_chr_bank;
	IO8(addr) = data;
	while (IO8(addr) != IO8(addr));	// wait for program
	IO8(0xC000) = mapper_prg_bank | mapper_chr_bank;
	IO8(0x2000) = lnPPUCTRL;
}

void flash_sector_erase(uint16_t addr, uint8_t bank)
{
	IO8(0x2000) = lnPPUCTRL & 0x7F;
	IO8(0xC000) = 0x01 | mapper_chr_bank;
	IO8(0x9555) = 0xAA;
	IO8(0xC000) = 0x00 | mapper_chr_bank;
	IO8(0xAAAA) = 0x55;
	IO8(0xC000) = 0x01 | mapper_chr_bank;
	IO8(0x9555) = 0x80;
	IO8(0x9555) = 0xAA;
	IO8(0xC000) = 0x00 | mapper_chr_bank;
	IO8(0xAAAA) = 0x55;
	IO8(0xC000) = bank | mapper_chr_bank;
	IO8(addr) = 0x30;
	while (IO8(addr) != IO8(addr));	// wait for erase	
	IO8(0xC000) = mapper_prg_bank | mapper_chr_bank;
	IO8(0x2000) = lnPPUCTRL;
}

// ******************************************************************************************


// ******************************************************************************************
#pragma section default
void bankswitch_prg(uint8_t bank)
{
	mapper_prg_bank = bank;
	mapper_register = bank | mapper_chr_bank;
	IO8(0xC000) = mapper_register;
}	

// ******************************************************************************************

void bankswitch_chr(uint8_t bank)
{
	mapper_chr_bank = bank;
	mapper_register = bank | mapper_prg_bank;
	IO8(0xC000) = mapper_register;
}	

// ******************************************************************************************
