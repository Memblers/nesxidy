GAME_NUMBER = 0

;=======================================================	
; Kludgeville city limits

	global _ROM_NAME, _ROM_OFFSET

; - dynamos.h settings MUST match these
ASM_BLOCK_COUNT = 8
ASM_CODE_SIZE = 250

FLASH_CACHE_BLOCKS 		= 960
FLASH_CACHE_BLOCK_SIZE	= $100

; - config.h settings MUST match these
	if (GAME_NUMBER == 0)
_ROM_OFFSET = $2800
_ROM_NAME = _rom_sidetrac
	endif

	if (GAME_NUMBER == 1)
_ROM_OFFSET = $1800
_ROM_NAME = _rom_targ
	endif

	if (GAME_NUMBER == 2)
_ROM_OFFSET = $1800
_ROM_NAME = _rom_targtest
	endif

	if (GAME_NUMBER == 3)
_ROM_OFFSET = $1000
_ROM_NAME = _rom_spectar
	endif

; end of that

;=======================================================	
;-------------------------------------------------------

RECOMPILED	=	$80
INTERPRETED	=	$40

BANK_PC			=	19
BANK_PC_FLAGS	=	27

BLOCK_CONFIG_BASE	=	250

;=======================================================	
	section "zpage"
;-------------------------------------------------------
addr_lo:	reserve 1
addr_hi:	reserve 1
temp:		reserve 1
temp2:		reserve 1
target_bank:	reserve 1

;=======================================================	
	section "bss"
	global _RAM_BASE, _CHARACTER_RAM_BASE, _SCREEN_RAM_BASE
	align 8
;-------------------------------------------------------	
_RAM_BASE:	reserve $400
_CHARACTER_RAM_BASE:	reserve $800
	section "nesram"
	align 8
_SCREEN_RAM_BASE: reserve $400
	

;=======================================================	
	section "bank1"
	global _rom_sidetrac, _chr_sidetrac
;-------------------------------------------------------
	
	if (GAME_NUMBER == 0)
	align 8
_rom_sidetrac:	
	;incbin "cpu_6502_test.bin"
	incbin "roms\sidetrac\stl8a-1"
	incbin "roms\sidetrac\stl7a-2"
	incbin "roms\sidetrac\stl6a-2"	
	align 8
_chr_sidetrac:
	incbin "roms\sidetrac\stl9c-1"
	align 8
	global _spr_sidetrac
_spr_sidetrac:
	incbin "roms\sidetrac\stl11d"
	endif
	
;=======================================================	
	section "bank1"
	global _rom_targ
;-------------------------------------------------------

	if (GAME_NUMBER == 1)
	align 8
_rom_targ:
	incbin "roms\targ\hrl10a-1"
	incbin "roms\targ\hrl9a-1"
	incbin "roms\targ\hrl8a-1"
	incbin "roms\targ\hrl7a-1"
	incbin "roms\targ\hrl6a-1"
	endif
	
;=======================================================	
	section "bank1"
	global _rom_targtest
;-------------------------------------------------------
	if (GAME_NUMBER == 2)
	align 8
_rom_targtest:
	incbin "roms\targ\hrl10a-1"
	incbin "roms\targ\hrl9a-1"
	incbin "roms\targ\hrl8a-1"
	incbin "roms\targtest\hrl7a-1"
	incbin "roms\targ\hrl6a-1"
	endif

;=======================================================	
	section "bank1"
	global _rom_spectar
;-------------------------------------------------------
	if (GAME_NUMBER == 3)
	align 8
_rom_spectar:
	incbin "roms\spectar\spl11a-3"
	incbin "roms\spectar\spl10a-2"
	incbin "roms\spectar\spl9a-3"
	incbin "roms\spectar\spl8a-2"
	incbin "roms\spectar\spl7a-2"
	incbin "roms\spectar\spl6a-2"
	endif


;=======================================================
	section "bank1"
	global _rom_cpu6502test
;-------------------------------------------------------
	if (GAME_NUMBER == 4)
	align 8
_rom_cpu6502test:
	incbin "cpu_6502_test.bin"
	endif

endif


;=======================================================	
	zpage	_status, _a, _x, _y, _pc
	
;=======================================================	
	

;=======================================================	
; REMOVED: _dispatch_cache_asm, _dispatch_return, _dispatch_table
; These were part of the old RAM cache execution system.
; Flash cache execution uses _dispatch_on_pc and _flash_dispatch_return instead.
	section "data"
	global _cache_code, _cache_index
	zpage _cache_index
;-------------------------------------------------------	

;=======================================================	
	section "trampoline"
;-------------------------------------------------------	
_haltwait:
	jmp _flash_dispatch_return

;=======================================================	
	section "data"
	global _dispatch_on_pc, _flash_cache_index, _flash_dispatch_return
	zpage addr_lo, addr_hi, target_bank, temp, temp2
;-------------------------------------------------------
_dispatch_on_pc:	; D0-D13 - address in bank   pc_flags
	lda _pc+1		; D14-D15 - bank number
	asl
	rol temp
	asl
	rol temp
	sta temp2
	lsr
	sec		; set upper bit
	ror	
	sta addr_hi
	lda temp	
	and #%00000011
	;clc
	adc #BANK_PC_FLAGS
	sta $C000
	lda _pc
	sta addr_lo
	ldy #0
	lda (addr_lo),y	; get pc_flag
	beq not_recompiled ; $00 = uninitialized flash, treat as not compiled
	bmi not_recompiled ; bit 7 SET = not recompiled
	and #$1F	; bank select
	sta target_bank
	;jsr _bankswitch_prg
	
	; get PC remap address	
	asl temp2
	lda temp
	rol
	and #%00000111
	clc
	adc #BANK_PC
	sta $C000
		
	asl addr_lo
	lda _pc+1
	rol
	and #%00111111
	ora #%10000000
	sta addr_hi
	
	lda (addr_lo),y
	sta .dispatch_addr
	iny
	lda (addr_lo),y
	sta .dispatch_addr + 1
	
	; Check for invalid code address ($8000 = uninitialized)
	; This can happen if flag was written but code address wasn't
	cmp #$80
	bne .addr_valid
	lda .dispatch_addr
	bne .addr_valid
	; Address is $8000 - treat as invalid
	jmp not_recompiled
.addr_valid:
	
	lda target_bank
	sta $C000
	
	lda _status
	ora #$04	; hide IRQ/BRK flag during JIT execution
	pha	

	;lda #$26	
	;sta $4020
	
	lda _a
	ldx _x	
	ldy _y
	plp
	
	jsr .dispatch_addr_instruction	
	rts

.dispatch_addr_instruction:	
.dispatch_addr = * + 1
	jmp $FFFF	; self-modifying	
	
_flash_dispatch_return:	
	; Note: _a, _pc, and status (on stack) are already set by the code block epilogue
	stx _x
	sty _y	
	
	pla
	sta _status

	lda #0	; return 0 = executed from flash
	rts	
	
not_recompiled:
	and #INTERPRETED
	beq needs_interpret   ; If INTERPRETED bit CLEAR, interpret (was marked for interpretation)
	lda #1 ; INTERPRETED bit SET = not yet processed, needs recompile
	rts
	
needs_interpret:
	lda #2	; interpret this PC
	rts


;=======================================================	
	section "text"
	global _cache_search
	zpage _matched, _cache_index, _cache_entry_pc_lo, _cache_entry_pc_hi
;-------------------------------------------------------
	
_cache_search:
	lda _pc
	ldx #ASM_BLOCK_COUNT-1
search_loop:
	cmp _cache_entry_pc_lo-1,x
	beq found
continue:
	dex
	bne search_loop
	rts
found:
	lda _pc+1
	cmp _cache_entry_pc_hi-1,x
	bne continue
	lda #1
	sta _matched
	dex
	stx _cache_index
	rts
	
;=======================================================	
	global _decode_address_asm, _decode_address_asm2
	zpage _decoded_address, _encoded_address, _character_ram_updated, _screen_ram_updated
;-------------------------------------------------------

	align 8
address_decoding_table:
	;_RAM_BASE ; 0x04 @ 0000-03FF						
	db >(_RAM_BASE + $0000), >(_RAM_BASE + $0100), >(_RAM_BASE + $0200), >(_RAM_BASE + $0300) 
	db >(_RAM_BASE + $0000), >(_RAM_BASE + $0100), >(_RAM_BASE + $0200), >(_RAM_BASE + $0300) 
	db >(_RAM_BASE + $0000), >(_RAM_BASE + $0100), >(_RAM_BASE + $0200), >(_RAM_BASE + $0300) 
	db >(_RAM_BASE + $0000), >(_RAM_BASE + $0100), >(_RAM_BASE + $0200), >(_RAM_BASE + $0300) 

	;<_ROM_NAME + i	; 0x30 @ 1000-3FFF;
	
	db >(_ROM_NAME + $1000 - _ROM_OFFSET), >(_ROM_NAME + $1100 - _ROM_OFFSET), >(_ROM_NAME + $1200 - _ROM_OFFSET), >(_ROM_NAME + $1300 - _ROM_OFFSET)
	db >(_ROM_NAME + $1400 - _ROM_OFFSET), >(_ROM_NAME + $1500 - _ROM_OFFSET), >(_ROM_NAME + $1600 - _ROM_OFFSET), >(_ROM_NAME + $1700 - _ROM_OFFSET)
	db >(_ROM_NAME + $1800 - _ROM_OFFSET), >(_ROM_NAME + $1900 - _ROM_OFFSET), >(_ROM_NAME + $1A00 - _ROM_OFFSET), >(_ROM_NAME + $1B00 - _ROM_OFFSET)
	db >(_ROM_NAME + $1C00 - _ROM_OFFSET), >(_ROM_NAME + $1D00 - _ROM_OFFSET), >(_ROM_NAME + $1E00 - _ROM_OFFSET), >(_ROM_NAME + $1F00 - _ROM_OFFSET)
	
	db >(_ROM_NAME + $2000 - _ROM_OFFSET), >(_ROM_NAME + $2100 - _ROM_OFFSET), >(_ROM_NAME + $2200 - _ROM_OFFSET), >(_ROM_NAME + $2300 - _ROM_OFFSET)
	db >(_ROM_NAME + $2400 - _ROM_OFFSET), >(_ROM_NAME + $2500 - _ROM_OFFSET), >(_ROM_NAME + $2600 - _ROM_OFFSET), >(_ROM_NAME + $2700 - _ROM_OFFSET)
	db >(_ROM_NAME + $2800 - _ROM_OFFSET), >(_ROM_NAME + $2900 - _ROM_OFFSET), >(_ROM_NAME + $2A00 - _ROM_OFFSET), >(_ROM_NAME + $2B00 - _ROM_OFFSET)
	db >(_ROM_NAME + $2C00 - _ROM_OFFSET), >(_ROM_NAME + $2D00 - _ROM_OFFSET), >(_ROM_NAME + $2E00 - _ROM_OFFSET), >(_ROM_NAME + $2F00 - _ROM_OFFSET)

	db >(_ROM_NAME + $3000 - _ROM_OFFSET), >(_ROM_NAME + $3100 - _ROM_OFFSET), >(_ROM_NAME + $3200 - _ROM_OFFSET), >(_ROM_NAME + $3300 - _ROM_OFFSET)
	db >(_ROM_NAME + $3400 - _ROM_OFFSET), >(_ROM_NAME + $3500 - _ROM_OFFSET), >(_ROM_NAME + $3600 - _ROM_OFFSET), >(_ROM_NAME + $3700 - _ROM_OFFSET)
	db >(_ROM_NAME + $3800 - _ROM_OFFSET), >(_ROM_NAME + $3900 - _ROM_OFFSET), >(_ROM_NAME + $3A00 - _ROM_OFFSET), >(_ROM_NAME + $3B00 - _ROM_OFFSET)
	db >(_ROM_NAME + $3C00 - _ROM_OFFSET), >(_ROM_NAME + $3D00 - _ROM_OFFSET), >(_ROM_NAME + $3E00 - _ROM_OFFSET), >(_ROM_NAME + $3F00 - _ROM_OFFSET)	
	
	;<_SCREEN_RAM_BASE + i	; 0x04 @ 4000-43FF ??	
	db >(_SCREEN_RAM_BASE + $0000), >(_SCREEN_RAM_BASE + $0100), >(_SCREEN_RAM_BASE + $0200), >(_SCREEN_RAM_BASE + $0300)
	db >(_SCREEN_RAM_BASE + $0000), >(_SCREEN_RAM_BASE + $0100), >(_SCREEN_RAM_BASE + $0200), >(_SCREEN_RAM_BASE + $0300)
	
	;<_CHARACTER_RAM_BASE + i	; 0x08 @ 4800-4FFF	
	db >(_CHARACTER_RAM_BASE + $0000), >(_CHARACTER_RAM_BASE + $0100), >(_CHARACTER_RAM_BASE + $0200), >(_CHARACTER_RAM_BASE + $0300)
	db >(_CHARACTER_RAM_BASE + $0400), >(_CHARACTER_RAM_BASE + $0500), >(_CHARACTER_RAM_BASE + $0600), >(_CHARACTER_RAM_BASE + $0700)
	
	align 8	; pad with zeros
address_action_table:
	;_RAM_BASE
	db $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
	;_ROM_NAME
	db $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
	db $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
	db $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
	;_SCREEN_RAM_BASE
	db $80,$81,$82,$83
	db $80,$81,$82,$83
	;_CHARACTER_RAM_BASE
	db $C0,$C1,$C2,$C3,$C4,$C5,$C6,$C7

	
_decode_address_asm:	
	;lda _encoded_address+1
	stx _x
	tax
	lda address_decoding_table,x	
	sta _decoded_address+1
	lda address_action_table,x
	bmi take_action
	ldx _x
	rts
take_action:
	lda address_action_table,x
	cmp #$C0
	bcs write_character
	inc _screen_ram_updated
	ldx _x
	rts
	
write_character:
	inc _character_ram_updated
	ldx _x
	rts	

_decode_address_asm2:
	;sta _encoded_address
	;stx _encoded_address+1
	lda _encoded_address+1
	cmp #$04
	bcc addr_ram
	cmp #$40
	bcc addr_rom
	cmp #$48
	bcc addr_screen_ram
	cmp #$50
	bcc addr_character_ram
	lda #0
	sta _decoded_address
	sta _decoded_address+1	
	rts
	
addr_ram:
	lda _encoded_address+1
	clc
	adc #>_RAM_BASE
	sta _decoded_address+1
	rts
	
addr_rom:
	sec
	sbc #>_ROM_OFFSET
	;sta _decoded_address+1
	;lda _decoded_address+1
	clc
	adc #>_ROM_NAME
	sta _decoded_address+1
	rts
	
addr_screen_ram:
	sec
	sbc #$40
	;sta _decoded_address+1
	;lda _decoded_address+1
	clc
	adc #>_SCREEN_RAM_BASE
	sta _decoded_address+1
	inc _screen_ram_updated
	rts
	
addr_character_ram:
	sec
	sbc #$48
	;sta _decoded_address+1
	;lda _decoded_address+1
	clc
	adc #>_CHARACTER_RAM_BASE
	sta _decoded_address+1
	inc _character_ram_updated
	rts


;=======================================================
	section "data"
	global _addr_6502_indy, _addr_6502_indy_size, _indy_address_lo, _indy_address_hi, _indy_opcode_location
;-------------------------------------------------------	
; - RELOCATABLE CODE - INTERNAL ACCESS ONLY -
_addr_6502_indy:
	php
	pha
_indy_opcode_location = * + 1
	lda #$FF
	sta _indy_opcode
_indy_address_lo = * + 1
	lda $FFFF
	sta _decoded_address	; low byte aligned
_indy_address_hi = * + 1
	lda $FFFF
	;sta _encoded_address+1
	jsr _decode_address_asm
	pla
	plp
	jsr handle_io_indy


_addr_6502_indy_end:

_addr_6502_indy_size:	db ( _addr_6502_indy_end - _addr_6502_indy)

;=======================================================
	section "data"
;-------------------------------------------------------	
handle_io_indy:
_indy_opcode:	; insert opcode
	nop
_indy_operand:
	db _decoded_address	; (decoded_address),y
	rts
	
;=======================================================
	section "data"
	global _addr_6502_indx, _addr_6502_indx_size, _indx_address_lo, _indx_address_hi, _indx_opcode_location
;-------------------------------------------------------	
; - RELOCATABLE CODE - INTERNAL ACCESS ONLY -
_addr_6502_indx:
	php
	pha	
_indx_opcode_location = * + 1
	lda #$FF
	sta _indx_opcode
	clc
	txa
_indx_address_lo = * + 1	
	adc $FFFF
	sta _decoded_address	; low byte aligned
_indx_address_hi = * + 1
	lda $FFFF
	;sta _encoded_address+1
	jsr _decode_address_asm
	pla
	plp
	jsr handle_io_indx
	

_addr_6502_indx_end:

_addr_6502_indx_size:	db ( _addr_6502_indx_end - _addr_6502_indx)

;=======================================================
	section "data"
;-------------------------------------------------------	
handle_io_indx:
	stx _x
	ldx #0
_indx_opcode:	; insert opcode
	nop
_indx_operand:
	db _decoded_address	; (ind,x)
	ldx _x
	rts
	
;=======================================================
	section "text"
	global _opcode_6502_pha, _opcode_6502_pha_size
;-------------------------------------------------------
_opcode_6502_pha:
	php
	stx _x
	ldx _sp
	sta _RAM_BASE + $100, x
	dec _sp
	ldx _x
	plp
	
_opcode_6502_pha_end:
_opcode_6502_pha_size:	db (_opcode_6502_pha_end - _opcode_6502_pha)

;=======================================================
	section "text"
	global _opcode_6502_pla, _opcode_6502_pla_size
;-------------------------------------------------------
_opcode_6502_pla:	; PLA: increment SP first, then read
	php
	stx _x
	inc _sp		; increment SP FIRST
	ldx _sp
	lda _RAM_BASE + $100, x  ; then read from new SP location
	ldx _x
	plp
	
_opcode_6502_pla_end:
_opcode_6502_pla_size:	db (_opcode_6502_pla_end - _opcode_6502_pla)

;=======================================================
	section "data"
	global _opcode_stx_zpy, _opcode_stx_zpy_size
;-------------------------------------------------------	; to add

_opcode_stx_zpy:
	php
	sta _a
	txa
_opcode_stx_zpy_address = * + 1
	sta $FFFF,y
	lda _a
	plp
	
_opcode_stx_zpy_end:
_opcode_stx_zpy_size:	db (_opcode_stx_zpy_end - _opcode_stx_zpy)

;=======================================================
	section "zpage"
	global _indy_ea, _indy_value, _indy_zp, _indy_y, _indy_ptr
;-------------------------------------------------------
_indy_ea:		reserve 2
_indy_value:	reserve 1
_indy_zp:		reserve 1
_indy_y:		reserve 1
_indy_ptr:		reserve 2

;=======================================================
	section "data"
	global _sta_indy_handler
;-------------------------------------------------------
; STA (zp),Y handler - computes effective address and calls write6502
; Called via: PHA / LDX #zp / JSR handler / PLA
; On entry: X = ZP pointer address, Y = Y index (live), A on stack
_sta_indy_handler:
	; Save ALL state first - write6502 may corrupt things
	sta _indy_value		; save A (will get real value from stack later)
	stx _indy_zp		; save ZP address
	sty _indy_y			; save Y for address calc
	
	; Set up pointer to _RAM_BASE + zp_addr
	clc
	txa
	adc #<_RAM_BASE
	sta _indy_ptr
	lda #>_RAM_BASE
	adc #0
	sta _indy_ptr + 1
	
	; Read pointer from emulated zero page using indirect addressing
	ldy #0
	lda (_indy_ptr),y	; low byte of pointer
	sta _indy_ea
	iny
	lda (_indy_ptr),y	; high byte of pointer
	sta _indy_ea + 1
	
	; Add emulated Y to get effective address
	lda _indy_ea
	clc
	adc _indy_y
	sta _indy_ea
	bcc .no_carry
	inc _indy_ea + 1
.no_carry:
	
	; Get value from stack (A was pushed before LDX/JSR)
	; Stack layout after JSR: [ret_lo @ SP+1] [ret_hi @ SP+2] [saved_A @ SP+3]
	tsx
	lda $103,x			; saved A is at SP+3
	sta _indy_value
	
	; Save emulated CPU registers before C call (write6502 may trash them)
	lda _x
	pha
	lda _y
	pha
	
	; Call write6502(_indy_ea, _indy_value) using vbcc calling convention
	lda _indy_ea
	sta r0
	lda _indy_ea + 1
	sta r1
	lda _indy_value
	sta r2
	jsr _write6502
	
	; Restore emulated CPU registers
	pla
	sta _y
	pla
	sta _x
	
	; Restore Y for the emulated code
	ldy _indy_y
	
	rts

;=======================================================	
	section "bank3"
	global _flash_cache_data, _flash_block_flags
;-------------------------------------------------------	
_flash_block_flags:
	reserve	FLASH_CACHE_BLOCKS

;=======================================================	
	section "bank4"	; occupies banks 4-18, 15 banks
	global _flash_cache_code
;-------------------------------------------------------	
	align 14
_flash_cache_code:	reserve 16384

;=======================================================	
	section "bank19"	; banks 19-26, 8 banks
	global _flash_cache_pc, _flash_cache_pc_flags
;-------------------------------------------------------	
	align 14
_flash_cache_pc:	
	section "bank20"
	align 14
	section "bank21"
	align 14
	section "bank22"
	align 14	
	section "bank23"
	align 14
	section "bank24"
	align 14
	section "bank25"
	align 14
	section "bank26"
	align 14
	section "bank27"
	align 14
_flash_cache_pc_flags:
	section "bank28"
	align 14
	section "bank29"
	align 14
	section "bank30"
	align 14