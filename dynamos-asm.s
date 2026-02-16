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
	zpage	_status, _a, _x, _y, _pc, _sp
	
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
	global _cross_bank_dispatch
;-------------------------------------------------------
; Cross-bank dispatch trampoline (fixed bank)
; Called from patchable epilogue slow path when exit_pc is in a different
; flash bank.  Instead of returning to C and re-entering dispatch_on_pc,
; saves regs/status and re-dispatches directly — eliminating the C round-trip.
;
; On entry (from epilogue slow path):
;   _a, _pc already saved by epilogue
;   X, Y still live from guest code
;   Stack: [epilogue's PHP] [.dispatch_addr JSR return] [run_6502 return]
;
; Pops PHP and the stale .dispatch_addr return, then JMPs to dispatch_on_pc
; which creates a fresh JSR .dispatch_addr for the next block.
_cross_bank_dispatch:
	stx _x				; save X (not saved by epilogue)
	sty _y				; save Y (not saved by epilogue)
	pla					; pop epilogue's PHP
	sta _status
	pla					; pop stale .dispatch_addr return lo
	pla					; pop stale .dispatch_addr return hi
	jmp _dispatch_on_pc	; re-dispatch _pc without C round-trip

;=======================================================	
	section "data"
	global _xbank_trampoline, _xbank_addr
;-------------------------------------------------------
; Cross-bank fast trampoline (WRAM, single instance, self-modifying)
;
; Called from the cross-bank setup code appended to patchable epilogues.
; The setup code (in flash) writes xbank_addr before jumping here,
; and passes the target bank in A.
;
; On entry:
;   A = target flash bank number
;   _a = saved guest A (saved by setup code)
;   Stack: [guest PHP from setup code] [.dispatch_addr JSR return] [run_6502 return]
;
; Performs the bankswitch from WRAM (always reachable), restores guest
; A and flags, then jumps directly to the target native code.
_xbank_trampoline:
	sta $C000			; bankswitch (A = target bank)
	lda _a				; restore guest A
	plp					; restore guest flags
_xbank_addr = * + 1
	jmp $FFFF			; self-mod: target native address

;=======================================================	
	section "data"
	global _dispatch_on_pc, _flash_cache_index, _flash_dispatch_return, _flash_dispatch_return_no_regs
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
	
_flash_dispatch_return_no_regs:
	; Entry point when _a, _x, _y are already saved (used by native JSR)
	pla
	sta _status

	lda #0	; return 0 = executed from flash
	rts	

;-------------------------------------------------------
; Native JSR trampoline (WRAM)
; Called via JMP from the native JSR template (inside a dispatched block).
; Loops: dispatch_on_pc → run compiled block → check if subroutine returned.
; Returns via RTS when emulated SP >= saved SP (subroutine did RTS).
; The RTS returns to the outer dispatch_on_pc's JSR .dispatch_addr_instruction.
; If a block needs recompile/interpret, bails with A=non-zero.
;
; On entry:  _pc = target, _sp = post-push SP, _native_jsr_saved_sp = pre-push SP
; On exit:   A = 0 if subroutine completed, non-zero if needs C
;-------------------------------------------------------
	global _native_jsr_trampoline
_native_jsr_trampoline:
.njsr_loop:
	jsr _dispatch_on_pc		; dispatch current _pc block
	; A = 0: block ran from flash
	; A = 1: needs recompile
	; A = 2: needs interpret
	bne .njsr_exit			; non-zero → can't handle natively, bail to C
	
	; Block executed. Check if subroutine returned.
	; If emulated SP >= saved_sp, the RTS has popped back to (or past) entry level.
	lda _sp
	cmp _native_jsr_saved_sp
	bcc .njsr_loop			; SP < saved_sp → still in subroutine, dispatch next block
	
	; Subroutine completed (SP restored). _pc and _status are already
	; set correctly by the nrts template's inner dispatch. Pop the
	; njsr template's PHP without clobbering _status, then return 0.
	pla						; discard njsr's PHP (don't overwrite _status!)
	lda #0					; return 0 = executed from flash
	rts						; return to outer dispatch_on_pc's JSR .dispatch_addr_instruction
	
.njsr_exit:
	; Bail to C — subroutine hit something that needs recompile/interpret.
	; _status was already saved by the last block's epilogue.
	; Pop njsr's PHP without clobbering _status, return non-zero.
	pla						; discard njsr's PHP
	; A still has non-zero result from dispatch_on_pc (1=recompile, 2=interpret)
	; But dispatch_on_pc already returned here with non-zero A, and we did
	; lda _sp / cmp which changed A. We need to return non-zero to the caller.
	lda #1					; return non-zero = needs C handling
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
; handle_io_indy — WRAM trampoline for read-type indy opcodes.
; On entry: A = emulated A, flags = emulated flags, Y = emulated Y.
; Checks if _decoded_address points into the switchable ROM bank
; window ($8000-$BFFF).  If so, temporarily maps bank1 (ROM data)
; before executing the opcode, then restores the flash cache bank.
; On exit: A/flags reflect the opcode result, Y preserved.
handle_io_indy:
	pha				; save emulated A
	php				; save emulated flags
	lda _decoded_address+1
	cmp #$80
	bcc .io_fast			; < $80: RAM/WRAM, no switch needed
	cmp #$C0
	bcs .io_fast			; >= $C0: fixed bank, no switch needed
	; --- slow path: ROM is in the switchable bank window ---
	lda #1
	sta $C000			; map bank1 (ROM data)
	plp				; restore emulated flags
	pla				; restore emulated A
	jsr .io_trampoline		; execute opcode (result in A/flags)
	php				; save result flags
	pha				; save result A
	lda target_bank
	sta $C000			; restore flash cache bank
	pla				; restore result A
	plp				; restore result flags
	rts
.io_fast:
	; --- fast path: no bank switch needed ---
	plp				; restore emulated flags
	pla				; restore emulated A
	; fall through
.io_trampoline:
_indy_opcode:	; patched with actual opcode
	nop
_indy_operand:
	db _decoded_address		; (decoded_address),y
	rts

;=======================================================
	section "data"
	global _sta_indy_template, _sta_indy_template_size, _sta_indy_zp_patch
;-------------------------------------------------------
; STA ($zp),Y template — routes through write6502() for full correctness.
; Compile-time: patch byte at _sta_indy_zp_patch with the ZP pointer address.
; Runtime: saves state, calls _sta_indy_handler (which calls write6502),
;          restores X from _x (handler saves it), restores A & flags.
; - RELOCATABLE CODE - INTERNAL ACCESS ONLY -
_sta_indy_template:
	php				; save flags
	pha				; save A (value to store) — handler reads from stack
_sta_indy_zp_patch = * + 1
	ldx #$FF			; patched: ZP pointer address
	jsr _sta_indy_handler		; compute effective addr, call write6502
	ldx _x				; restore real X (handler saved _x but trashed real X)
	pla				; restore A
	plp				; restore flags
_sta_indy_template_end:

_sta_indy_template_size: db (_sta_indy_template_end - _sta_indy_template)

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
	section "text"
	global _opcode_6502_jsr, _opcode_6502_jsr_size
	global _opcode_6502_jsr_ret_hi, _opcode_6502_jsr_ret_lo
	global _opcode_6502_jsr_tgt_lo, _opcode_6502_jsr_tgt_hi
;-------------------------------------------------------
; Native JSR template (34 bytes)
; Pushes return address (pc+2) onto emulated stack, sets _pc to target,
; exits via flash_dispatch_return_no_regs.
; Caller must patch 4 bytes after copy:
;   _opcode_6502_jsr_ret_hi  = offset of return addr high byte
;   _opcode_6502_jsr_ret_lo  = offset of return addr low byte
;   _opcode_6502_jsr_tgt_lo  = offset of target addr low byte
;   _opcode_6502_jsr_tgt_hi  = offset of target addr high byte
_opcode_6502_jsr:
	php							; +0  save flags
	sta _a						; +1  save A
	stx _x						; +3  save X
	sty _y						; +5  save Y
	ldx _sp						; +7  load emulated SP
_opcode_6502_jsr_ret_hi_loc:
	lda #$FF					; +9  LDA #>(return_addr) - PATCH
	sta _RAM_BASE + $100, x		; +11 push high byte
	dex							; +14
_opcode_6502_jsr_ret_lo_loc:
	lda #$FF					; +15 LDA #<(return_addr) - PATCH
	sta _RAM_BASE + $100, x		; +17 push low byte
	dex							; +20
	stx _sp						; +21 save updated SP
_opcode_6502_jsr_tgt_lo_loc:
	lda #$FF					; +23 LDA #<target - PATCH
	sta _pc						; +25
_opcode_6502_jsr_tgt_hi_loc:
	lda #$FF					; +27 LDA #>target - PATCH
	sta _pc+1					; +29
	jmp _flash_dispatch_return_no_regs	; +31

_opcode_6502_jsr_end:
_opcode_6502_jsr_size:	db (_opcode_6502_jsr_end - _opcode_6502_jsr)
; Patch offsets (byte index within template of the immediate operand)
_opcode_6502_jsr_ret_hi: db (_opcode_6502_jsr_ret_hi_loc - _opcode_6502_jsr + 1)
_opcode_6502_jsr_ret_lo: db (_opcode_6502_jsr_ret_lo_loc - _opcode_6502_jsr + 1)
_opcode_6502_jsr_tgt_lo: db (_opcode_6502_jsr_tgt_lo_loc - _opcode_6502_jsr + 1)
_opcode_6502_jsr_tgt_hi: db (_opcode_6502_jsr_tgt_hi_loc - _opcode_6502_jsr + 1)

;=======================================================
	section "text"
	global _opcode_6502_njsr, _opcode_6502_njsr_size
	global _opcode_6502_njsr_ret_hi, _opcode_6502_njsr_ret_lo
	global _opcode_6502_njsr_tgt_lo, _opcode_6502_njsr_tgt_hi
;-------------------------------------------------------
; Native JSR template (37 bytes)
; Same stack-push convention as emulated JSR, but instead of exiting to C,
; JMPs to native_jsr_trampoline which loops through the subroutine's
; compiled blocks entirely in WRAM. Trampoline exits via JMP to
; flash_dispatch_return_no_regs (never returns to flash — avoids bank issues).
;
; The trampoline returns A=0 (subroutine completed) or A=non-zero (needs C).
; Either way, _pc is already set to the correct continuation address.
;
; Caller must patch 4 bytes after copy (same patch offsets as emulated):
;   _opcode_6502_njsr_ret_hi  = offset of return addr high byte
;   _opcode_6502_njsr_ret_lo  = offset of return addr low byte
;   _opcode_6502_njsr_tgt_lo  = offset of target addr low byte
;   _opcode_6502_njsr_tgt_hi  = offset of target addr high byte
_opcode_6502_njsr:
	php							; +0  save flags (popped by flash_dispatch_return_no_regs)
	sta _a						; +1  save A
	stx _x						; +3  save X
	sty _y						; +5  save Y
	ldx _sp						; +7  load emulated SP
	stx _native_jsr_saved_sp	; +9  save pre-push SP for trampoline (3 bytes - abs addr)
_opcode_6502_njsr_ret_hi_loc:
	lda #$FF					; +12 LDA #>(return_addr) - PATCH
	sta _RAM_BASE + $100, x		; +14 push high byte
	dex							; +17
_opcode_6502_njsr_ret_lo_loc:
	lda #$FF					; +18 LDA #<(return_addr) - PATCH
	sta _RAM_BASE + $100, x		; +20 push low byte
	dex							; +23
	stx _sp						; +24 save updated SP
_opcode_6502_njsr_tgt_lo_loc:
	lda #$FF					; +26 LDA #<target - PATCH
	sta _pc						; +28
_opcode_6502_njsr_tgt_hi_loc:
	lda #$FF					; +30 LDA #>target - PATCH
	sta _pc+1					; +32
	jmp _native_jsr_trampoline	; +34 trampoline loops then exits to C via WRAM
	
_opcode_6502_njsr_end:
_opcode_6502_njsr_size:	db (_opcode_6502_njsr_end - _opcode_6502_njsr)
; Patch offsets (byte index within template of the immediate operand)
_opcode_6502_njsr_ret_hi: db (_opcode_6502_njsr_ret_hi_loc - _opcode_6502_njsr + 1)
_opcode_6502_njsr_ret_lo: db (_opcode_6502_njsr_ret_lo_loc - _opcode_6502_njsr + 1)
_opcode_6502_njsr_tgt_lo: db (_opcode_6502_njsr_tgt_lo_loc - _opcode_6502_njsr + 1)
_opcode_6502_njsr_tgt_hi: db (_opcode_6502_njsr_tgt_hi_loc - _opcode_6502_njsr + 1)

;=======================================================
	section "text"
	global _opcode_6502_nrts, _opcode_6502_nrts_size
;-------------------------------------------------------
; Native RTS template (32 bytes)
; Pops return address from emulated stack, adds 1 (6502 convention),
; stores to _pc, exits via flash_dispatch_return_no_regs.
; No patching needed — this is pure fixed code.
;
; 6502 RTS: pull lo, pull hi, PC = (hi:lo) + 1.
; pull16 reads (sp+1) as lo, (sp+2) as hi, then sp += 2.
_opcode_6502_nrts:
	php							; +0  save flags
	sta _a						; +1  save A (clobbered by stack read)
	stx _x						; +3  save X (clobbered by SP indexing)
	sty _y						; +5  save Y
	ldx _sp						; +7  load emulated SP
	inx							; +9  sp+1
	lda _RAM_BASE + $100, x		; +10 read lo byte
	sta _pc						; +13
	inx							; +15 sp+2
	lda _RAM_BASE + $100, x		; +16 read hi byte
	sta _pc+1					; +19
	stx _sp						; +21 save updated SP
	inc _pc						; +23 add 1 (RTS convention)
	bne .nrts_no_carry			; +25
	inc _pc+1					; +27
.nrts_no_carry:
	jmp _flash_dispatch_return_no_regs	; +29 exit (pops PHP, returns 0)

_opcode_6502_nrts_end:
_opcode_6502_nrts_size:	db (_opcode_6502_nrts_end - _opcode_6502_nrts)

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
	section "zpage"
	global _native_jsr_saved_sp
;-------------------------------------------------------
_native_jsr_saved_sp:	reserve 1

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
	section "data"
	global _peek_bank_byte
;-------------------------------------------------------
; peek_bank_byte(uint8_t bank, uint16_t addr) -> uint8_t
;
; Reads a single byte from an arbitrary bank without corrupting the
; caller's bank mapping.  Lives in WRAM so it is always reachable,
; even when the switchable bank ($8000-$BFFF) has been changed.
;
; VBCC 6502 calling convention (2-byte register slots):
;   r0 = bank       (8-bit, first arg — uses slot 0: r0=$00, r1=$01 unused)
;   r2 = addr lo    (16-bit second arg, slot 1: r2=$02 low, r3=$03 high)
;   r3 = addr hi
;   return r0       (8-bit)
;
; Side effect: temporarily maps the target bank at $8000, then
;              restores the previous mapper_prg_bank before returning.
;
_peek_bank_byte:
	lda r0				; target bank
	ora _mapper_chr_bank
	sta $C000			; switch to target bank
	lda r2
	sta .peek_addr		; self-mod: set address low
	lda r3
	sta .peek_addr+1	; self-mod: set address high
.peek_addr = * + 1
	lda $FFFF			; read byte from target bank (self-mod address)
	pha					; save the byte
	lda _mapper_prg_bank
	ora _mapper_chr_bank
	sta $C000			; restore caller's bank
	pla
	sta r0				; return value
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