MEMORY
{
  out:     org=0x7FF0, len=0xffffff
  zero:    org=0, len=0x0100
  
  b0:      org=0x8000, len=0x4000    
  b1:      org=0x8000, len=0x4000
  b2:      org=0x8000, len=0x4000  
  b3:      org=0x8000, len=0x4000  
  b4:      org=0x8000, len=0x4000    
  b5:      org=0x8000, len=0x4000
  b6:      org=0x8000, len=0x4000  
  b7:      org=0x8000, len=0x4000  
  b8:      org=0x8000, len=0x4000    
  b9:      org=0x8000, len=0x4000
  b10:      org=0x8000, len=0x4000  
  b11:      org=0x8000, len=0x4000  
  b12:      org=0x8000, len=0x4000    
  b13:      org=0x8000, len=0x4000
  b14:      org=0x8000, len=0x4000  
  b15:      org=0x8000, len=0x4000  
  b16:      org=0x8000, len=0x4000    
  b17:      org=0x8000, len=0x4000
  b18:      org=0x8000, len=0x4000  
  b19:      org=0x8000, len=0x4000  
  b20:      org=0x8000, len=0x4000    
  b21:      org=0x8000, len=0x4000
  b22:      org=0x8000, len=0x4000  
  b23:      org=0x8000, len=0x4000  
  b24:      org=0x8000, len=0x4000    
  b25:      org=0x8000, len=0x4000
  b26:      org=0x8000, len=0x4000  
  b27:      org=0x8000, len=0x4000  
  b28:      org=0x8000, len=0x4000    
  b29:      org=0x8000, len=0x4000
  b30:      org=0x8000, len=0x4000  
  b31:      org=0xC000, len=0x4000  
  
  ram:     org=0x0300, len=0x0500
  wram:	   org=0x6000, len=0x2000
}


INES_MAPPER = 30 ; /* The iNES mapper number */
INES_SUBMAPPER = 0 ; /* The submapper number, if needed */

INES_PRG_BANKS = 32 ; /* Number of 16K PRG banks, change to 2 for NROM256 */
INES_CHR_BANKS = 0 ; /* number of 8K CHR banks */
INES_MIRRORING = 1 ; /* 0 horizontal, 1 vertical, 8 four screen */

INES_V20 = 1 ; /* Use NES 2.0 header format?  Note: if this is 0, most of the following flags are ignored. */

INES_USE_4SCREEN = 1;
INES_HAS_TRAINER = 0;
INES_HAS_BATTERY_RAM = 1;	/* Flash ROM */
INES_CONSOLE_TYPE = 0;

/*
  RAM sizes are expressed according to the following:
   0 = 0 bytes
   1 = 128 bytes
   2 = 256 bytes
   3 = 512 bytes
   4 = 1,024 bytes
   5 = 2,048 bytes
   6 = 4,096 bytes
   7 = 8,192 bytes
   8 = 16,384 bytes
   9 = 32,768 bytes
  10 = 65,536 bytes
  11 = 131,072 bytes
  12 = 262,144 bytes
  13 = 524,288 bytes
  14 = 1,048,576 bytes
  15 = reserved (do not use)
*/

INES_PRG_NVRAM_SIZE = 0;
INES_PRG_RAM_SIZE = 7; /* 8KB PRG-RAM */

INES_CHR_NVRAM_SIZE = 0;
INES_CHR_RAM_SIZE = 9 ; /* 32KB CHR-RAM */

INES_IS_PAL = 0;
INES_BOTH_PAL_AND_NTSC = 0;

INES_VS_CPU_BITS = 0;
INES_VS_PPU_BITS = 0;

SECTIONS
{
  header:
  {
    /* iNES header - comments directly from https://wiki.nesdev.com/w/index.php/NES_2.0 */
    /* 0-3: Identification String. Must be "NES<EOF>". */
    BYTE(0x4e);BYTE(0x45);BYTE(0x53);BYTE(0x1a);

    /* 4: PRG-ROM size LSB */
    BYTE(INES_PRG_BANKS);

    /* 5: CHR-ROM size LSB */
    BYTE(INES_CHR_BANKS);

    /* 6: Flags 6
       D~7654 3210
         ---------
         NNNN FTBM
         |||| |||+-- Hard-wired nametable mirroring type
         |||| |||     0: Horizontal or mapper-controlled
         |||| |||     1: Vertical
         |||| ||+--- "Battery" and other non-volatile memory
         |||| ||      0: Not present
         |||| ||      1: Present
         |||| |+--- 512-byte Trainer
         |||| |      0: Not present
         |||| |      1: Present between Header and PRG-ROM data
         |||| +---- Hard-wired four-screen mode
         ||||        0: No
         ||||        1: Yes
         ++++------ Mapper Number D0..D3
    */
    BYTE(((INES_MAPPER & 0x0f) << 4) | ((INES_USE_4SCREEN & 0x01) << 3) | ((INES_HAS_TRAINER & 0x01) << 2) | ((INES_HAS_BATTERY_RAM & 0x01) << 1) | ((INES_MIRRORING & 0x01) << 0));

    /* 7: Flags 7
       D~7654 3210
         ---------
         NNNN 10TT
         |||| ||++- Console type
         |||| ||     0: Nintendo Entertainment System/Family Computer
         |||| ||     1: Nintendo Vs. System
         |||| ||     2: Nintendo Playchoice 10
         |||| ||     3: Extended Console Type
         |||| ++--- NES 2.0 identifier
         ++++------ Mapper Number D4..D7
    */
    BYTE((INES_MAPPER & 0xf0) | ((INES_V20 & 0x01) << 3) | (INES_CONSOLE_TYPE & 0x03));
 
    /* 8: Mapper MSB/Submapper
       D~7654 3210
         ---------
         SSSS NNNN
         |||| ++++- Mapper number D8..D11
         ++++------ Submapper number
    */
    BYTE((((INES_SUBMAPPER * (INES_V20 & 0x01)) & 0x0f) << 4) | (((INES_MAPPER * (INES_V20 & 0x01)) >> 8) & 0x0f));

    /* 9: PRG-ROM/CHR-ROM size MSB
       D~7654 3210
         ---------
         CCCC PPPP
         |||| ++++- PRG-ROM size MSB
         ++++------ CHR-ROM size MSB
    */
    BYTE((((INES_CHR_BANKS * (INES_V20 & 0x01)) >> 4) & 0xf0) | (((INES_PRG_BANKS * (INES_V20 & 0x01)) >> 8) & 0x0f));

    /* 10: PRG-RAM/EEPROM size
       D~7654 3210
         ---------
         pppp PPPP
         |||| ++++- PRG-RAM (volatile) shift count
         ++++------ PRG-NVRAM/EEPROM (non-volatile) shift count
       If the shift count is zero, there is no PRG-(NV)RAM.
       If the shift count is non-zero, the actual size is
       "64 << shift count" bytes, i.e. 8192 bytes for a shift count of 7.
    */
    BYTE((((INES_PRG_NVRAM_SIZE * (INES_V20 & 0x01)) & 0x0f) << 4) | (((INES_PRG_RAM_SIZE * (INES_V20 & 0x01)) & 0x0f) << 0));

    /* 11: CHR-RAM size
       D~7654 3210
         ---------
         cccc CCCC
         |||| ++++- CHR-RAM size (volatile) shift count
         ++++------ CHR-NVRAM size (non-volatile) shift count
       If the shift count is zero, there is no CHR-(NV)RAM.
       If the shift count is non-zero, the actual size is
       "64 << shift count" bytes, i.e. 8192 bytes for a shift count of 7.
    */
    BYTE((((INES_CHR_NVRAM_SIZE * (INES_V20 & 0x01)) & 0x0f) << 4) | (((INES_CHR_RAM_SIZE * (INES_V20 & 0x01)) & 0x0f) << 0));
    
    /* 12: CPU/PPU Timing
       D~7654 3210
         ---------
         .... ..VV
                ++- CPU/PPU timing mode
                     0: RP2C02 ("NTSC NES")
                     1: RP2C07 ("Licensed PAL NES")
                     2: Multiple-region
                     3: UMC 6527P ("Dendy")
    */
    BYTE((((INES_IS_PAL * (INES_V20 & 0x01)) & 0x01) << 0) | (((INES_BOTH_PAL_AND_NTSC * (INES_V20 & 0x01)) & 0x01) << 1));

    /* 13: When Byte 7 AND 3 =1: Vs. System Type
       D~7654 3210
         ---------
         MMMM PPPP
         |||| ++++- Vs. PPU Type
         ++++------ Vs. Hardware Type

       When Byte 7 AND 3 =3: Extended Console Type
       D~7654 3210
         ---------
         .... CCCC
              ++++- Extended Console Type
    */
    BYTE((((INES_VS_CPU_BITS * (INES_V20 & 0x01)) & 0x0f) << 4) | (((INES_VS_PPU_BITS * (INES_V20 & 0x01)) & 0x0f) << 0));

    /* 14: Miscellaneous ROMs
       D~7654 3210
         ---------
         .... ..RR
                ++- Number of miscellaneous ROMs present
    */
    BYTE(0);

    /* 15: Default Expansion Device
       D~7654 3210
         ---------
         ..DD DDDD
           ++-++++- Default Expansion Device
    */
    BYTE(0);
  } >out

	text0: { .=0x8000; *(text0) } >b0 AT>out
	rodata0: { *(rodata0) } >b0 AT>out
        bank0: { *(bank0) } >b0 AT>out
	fill0: { .=0xC000; } >b0 AT>out

	text1: { .=0x8000; *(text1) } >b1 AT>out
	rodata1: { *(rodata1) } >b1 AT>out
        bank1: { *(bank1) } >b1 AT>out
	fill1: { .=0xC000; } >b1 AT>out

	text2: { .=0x8000; *(text2) } >b2 AT>out
	rodata2: { *(rodata2) } >b2 AT>out
        bank2: { *(bank2) } >b2 AT>out
	fill2: { .=0xC000; } >b2 AT>out
	
	text3: { .=0x8000; *(text3) } >b3 AT>out		
	rodata3: { *(rodata3) } >b3 AT>out
        bank3: { *(bank3) } >b3 AT>out
	fill3: { .=0xC000; } >b3 AT>out

	text4: { .=0x8000; *(text4) } >b4 AT>out
	rodata4: { *(rodata4) } >b4 AT>out
        bank4: { *(bank4) } >b4 AT>out
	fill4: { .=0xC000; } >b4 AT>out

	text5: { .=0x8000; *(text5) } >b5 AT>out
	rodata5: { *(rodata5) } >b5 AT>out
        bank5: { *(bank5) } >b5 AT>out
	fill5: { .=0xC000; } >b5 AT>out
	
	text6: { .=0x8000; *(text6) } >b6 AT>out
	rodata6: { *(rodata6) } >b6 AT>out
        bank6: { *(bank6) } >b6 AT>out
	fill6: { .=0xC000; } >b6 AT>out
	
	text7: { .=0x8000; *(text7) } >b7 AT>out
	rodata7: { *(rodata7) } >b7 AT>out
        bank7: { *(bank7) } >b7 AT>out
	fill7: { .=0xC000; } >b7 AT>out
	
	text8: { .=0x8000; *(text8) } >b8 AT>out
	rodata8: { *(rodata8) } >b8 AT>out
        bank8: { *(bank8) } >b8 AT>out
	fill8: { .=0xC000; } >b8 AT>out

	text9: { .=0x8000; *(text9) } >b9 AT>out
	rodata9: { *(rodata9) } >b9 AT>out
        bank9: { *(bank9) } >b9 AT>out
	fill9: { .=0xC000; } >b9 AT>out
	
	text10: { .=0x8000; *(text10) } >b10 AT>out
	rodata10: { *(rodata10) } >b10 AT>out
        bank10: { *(bank10) } >b10 AT>out
	fill10: { .=0xC000; } >b10 AT>out
	
	text11: { .=0x8000; *(text11) } >b11 AT>out
	rodata11: { *(rodata11) } >b11 AT>out
        bank11: { *(bank11) } >b11 AT>out
	fill11: { .=0xC000; } >b11 AT>out
	
	text12: { .=0x8000; *(text12) } >b12 AT>out
	rodata12: { *(rodata12) } >b12 AT>out
        bank12: { *(bank12) } >b12 AT>out
	fill12: { .=0xC000; } >b12 AT>out
	
	text13: { .=0x8000; *(text13) } >b13 AT>out
	rodata13: { *(rodata13) } >b13 AT>out
        bank13: { *(bank13) } >b13 AT>out
	fill13: { .=0xC000; } >b13 AT>out
	
	text14: { .=0x8000; *(text11) } >b14 AT>out
	rodata14: { *(rodata14) } >b14 AT>out
        bank14: { *(bank14) } >b14 AT>out
	fill14: { .=0xC000; } >b14 AT>out
	
	text15: { .=0x8000; *(text15) } >b15 AT>out
	rodata15: { *(rodata15) } >b15 AT>out
        bank015: { *(bank15) } >b15 AT>out
	fill15: { .=0xC000; } >b15 AT>out

	text16: { .=0x8000; *(text16) } >b16 AT>out
	rodata16: { *(rodata16) } >b16 AT>out
        bank16: { *(bank16) } >b16 AT>out
	fill16: { .=0xC000; } >b16 AT>out

	text17: { .=0x8000; *(text17) } >b17 AT>out
	rodata17: { *(rodata17) } >b17 AT>out
        bank17: { *(bank17) } >b17 AT>out
	fill17: { .=0xC000; } >b17 AT>out
	
	text18: { .=0x8000; *(text18) } >b18 AT>out		
	rodata18: { *(rodata18) } >b18 AT>out
        bank18: { *(bank18) } >b18 AT>out
	fill18: { .=0xC000; } >b18 AT>out

	text19: { .=0x8000; *(text19) } >b19 AT>out
	rodata19: { *(rodata19) } >b19 AT>out
        bank19: { *(bank19) } >b19 AT>out
	fill19: { .=0xC000; } >b19 AT>out

	text20: { .=0x8000; *(text20) } >b20 AT>out
	rodata20: { *(rodata20) } >b20 AT>out
        bank20: { *(bank20) } >b20 AT>out
	fill20: { .=0xC000; } >b20 AT>out
	
	text21: { .=0x8000; *(text21) } >b21 AT>out
	rodata21: { *(rodata21) } >b21 AT>out
        bank21: { *(bank21) } >b21 AT>out
	fill21: { .=0xC000; } >b21 AT>out
	
	text22: { .=0x8000; *(text22) } >b22 AT>out
	rodata22: { *(rodata22) } >b22 AT>out
        bank22: { *(bank22) } >b22 AT>out
	fill22: { .=0xC000; } >b22 AT>out
	
	text23: { .=0x8000; *(text23) } >b23 AT>out
	rodata23: { *(rodata23) } >b23 AT>out
        bank23: { *(bank23) } >b23 AT>out
	fill23: { .=0xC000; } >b23 AT>out

	text24: { .=0x8000; *(text24) } >b24 AT>out
	rodata24: { *(rodata24) } >b24 AT>out
        bank24: { *(bank24) } >b24 AT>out
	fill24: { .=0xC000; } >b24 AT>out
	
	text25: { .=0x8000; *(text25) } >b25 AT>out
	rodata25: { *(rodata25) } >b25 AT>out
        bank25: { *(bank25) } >b25 AT>out
	fill25: { .=0xC000; } >b25 AT>out
	
	text26: { .=0x8000; *(text26) } >b26 AT>out
	rodata26: { *(rodata26) } >b26 AT>out
        bank26: { *(bank26) } >b26 AT>out
	fill26: { .=0xC000; } >b26 AT>out
	
	text27: { .=0x8000; *(text27) } >b27 AT>out
	rodata27: { *(rodata27) } >b27 AT>out
        bank27: { *(bank27) } >b27 AT>out
	fill27: { .=0xC000; } >b27 AT>out
	
	text28: { .=0x8000; *(text28) } >b28 AT>out
	rodata28: { *(rodata28) } >b28 AT>out
        bank28: { *(bank28) } >b28 AT>out
	fill28: { .=0xC000; } >b28 AT>out
	
	text29: { .=0x8000; *(text29) } >b29 AT>out
	rodata29: { *(rodata29) } >b29 AT>out
        bank29: { *(bank29) } >b29 AT>out
	fill29: { .=0xC000; } >b29 AT>out
	
	text30: { .=0x8000; *(text30) } >b30 AT>out
	rodata30: { *(rodata30) } >b30 AT>out
        bank30: { *(bank30) } >b30 AT>out
	fill30: { .=0xC000; } >b30 AT>out
  
  text:   {*(text)} >b31 AT>out
  .dtors: { *(.dtors) } >b31 AT>out
  .ctors: { *(.ctors) } >b31 AT>out
  rodata: {*(rodata)}  >b31 AT>out
  init:   {*(init)}  >b31 AT>out
  data:   {*(data)} >wram AT>out  

  /* fill program bank */
  fill: { .=.+0x10000-6-ADDR(init)-SIZEOF(init)-SIZEOF(data);} >b31 AT>out
  vectors:{ *(vectors)} >b31 AT>out


  zpage (NOLOAD) : {*(zpage) *(zp1) *(zp2)} >zero
  nesram (NOLOAD): {*(nesram)} >ram
  bss (NOLOAD): {*(bss)} >wram  

  __DS = ADDR(data);
  __DE = ADDR(data) + SIZEOF(data);
  __DC = LOADADDR(data)-0x88000;
/*
  __DS = ADDR(data);
  __DE = ADDR(data) + SIZEOF(data);
  __DC = LOADADDR(data);
*/

  __STACK = 0x8000;

  ___heap = ADDR(bss) + SIZEOF(bss);
  ___heapend = __STACK;
}
