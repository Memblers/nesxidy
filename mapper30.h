#ifndef MAPPER30_H
#define MAPPER30_H

#pragma section data
void flash_byte_program(uint16_t addr, uint8_t bank, uint8_t data);
void flash_sector_erase(uint16_t addr, uint8_t bank);

#pragma section default
void bankswitch_prg(__reg("a") uint8_t bank);
void bankswitch_chr(__reg("a") uint8_t bank);

#endif