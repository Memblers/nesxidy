	ifnd GAME_NUMBER
GAME_NUMBER = 0
	endif

;=======================================================	
; Kludgeville city limits

	global _ROM_NAME, _ROM_OFFSET

; - dynamos.h settings MUST match these
ASM_BLOCK_COUNT = 1
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

	if (GAME_NUMBER == 10)
_ROM_OFFSET = $C000
_ROM_NAME = _rom_nes_prg
	endif

; end of that

;=======================================================	
;-------------------------------------------------------

RECOMPILED	=	$80
INTERPRETED	=	$40

; --- Bank assignments: MUST match bank_map.h ---
BANK_PC			=	19
BANK_PC_FLAGS	=	27
	ifnd PLATFORM_NES
BANK_RENDER		=	22
	else
BANK_RENDER		=	21
	endif

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
	align 1
;-------------------------------------------------------	
	align 8
_RAM_BASE:	reserve $400
_CHARACTER_RAM_BASE:	reserve $800
	section "nesram"
	align 8
_SCREEN_RAM_BASE: reserve $400
	

;=======================================================	
	section "bank23"
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
	section "bank23"
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
	section "bank23"
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
	section "bank23"
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
	section "bank23"
	global _rom_cpu6502test
;-------------------------------------------------------
	if (GAME_NUMBER == 4)
	align 8
_rom_cpu6502test:
	incbin "cpu_6502_test.bin"
	endif

;=======================================================
; NES ROM data — bank20 = PRG, bank23 = CHR
;=======================================================

	if (GAME_NUMBER == 10)
	section "bank20"
	global _rom_nes_prg
	align 8
_rom_nes_prg:
	incbin "roms\nes\donkeykong.prg"

	section "bank23"
	global _chr_nes
	align 8
_chr_nes:
	incbin "roms\nes\donkeykong.chr"
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
	zpage _clockticks6502
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

	; --- Block cycle counting ---
	; Header byte +6 = pre-computed 6502 cycle count.
	; Code starts at header + 8, so cycles = *(dispatch_addr - 2).
	; Add to 32-bit clockticks6502 for frame timing.
	; Cost: ~25 NES cycles per dispatch (~0.2% overhead).
	lda .dispatch_addr
	sec
	sbc #2
	sta addr_lo
	lda .dispatch_addr + 1
	sbc #0
	sta addr_hi
	ldy #0
	lda (addr_lo),y
	cmp #$FF			; $FF = erased flash (no cycle data)
	beq .no_cycles
	clc
	adc _clockticks6502
	sta _clockticks6502
	bcc .no_cycles
	inc _clockticks6502+1
	bne .no_cycles
	inc _clockticks6502+2
.no_cycles:

	lda _status
	;ora #$04	; REMOVED: was hiding IRQ flag during JIT, but this
	;           ; corrupted _status on block exit — the epilogue's PHP
	;           ; captured the forced I=1 and stored it back, clobbering
	;           ; the guest's CLI.  NES IRQ handler ($C140) is RTI, so
	;           ; no protection needed.
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

	; Restore PRG bank — native code ran in a flash bank (4-18),
	; but our caller (run_6502/main loop) expects the fixed bank (0).
	; Without this, any subsequent call to banked code (irq6502 at $90B3
	; in bank 2, etc.) would execute garbage from the flash bank.
	lda _mapper_prg_bank
	ora _mapper_chr_bank
	sta $C000

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
; VBLANK HANDLING: Every iteration, compare lazyNES NMI counter (ZP $26)
; with _last_nmi_frame.  If different, a real vblank has occurred.  Instead
; of bailing to C (which destroys NJSR context and makes subsequent loop
; iterations run at the slow C-dispatch rate), we simply absorb the vblank
; by updating _last_nmi_frame and continue looping.  This means no rendering
; or IRQ dispatch during the subroutine, but delay loops have static screens
; and the IRQ handler just mutes audio during delays — both acceptable.
; After the subroutine returns, the main loop catches up normally.
;
; On entry:  _pc = target, _sp = post-push SP, _native_jsr_saved_sp = pre-push SP
; On exit:   A = 0 if subroutine completed, non-zero if needs C
;-------------------------------------------------------
	zpage _last_nmi_frame, _native_jsr_saved_sp
	global _native_jsr_trampoline
_native_jsr_trampoline:
	; Save outer _native_jsr_saved_sp on NES stack for nesting safety.
	; If an inner NJSR fires, it overwrites _native_jsr_saved_sp;
	; we restore the outer value on exit so the caller's trampoline
	; still checks against the correct SP threshold.
	lda _native_jsr_saved_sp
	pha
.njsr_loop:
	jsr _dispatch_on_pc		; dispatch current _pc block
	; A = 0: block ran from flash
	; A = 1: needs recompile
	; A = 2: needs interpret
	beq .njsr_check_vblank	; A=0 → block executed, continue
	bne .njsr_exit			; A!=0 → bail to C for compile or interpret
	; NOTE: inline interpret was removed — it caused hangs because the
	; trampoline absorbs vblanks, preventing IRQ dispatch.  The C-level
	; guard in run_6502 case 1 handles interpret-only PCs cheaply.
	
.njsr_check_vblank:
	; Check for vblank: if NMI counter ($26) != last_nmi_frame, absorb it
	; and continue looping.
	lda $26					; lazyNES NMI counter (incremented every vblank)
	cmp _last_nmi_frame		; has a new vblank occurred?
	beq .njsr_no_vblank		; no → skip absorb
	sta _last_nmi_frame		; absorb vblank: update last_nmi_frame
.njsr_no_vblank:
	
	; Block executed. Check if subroutine returned.
	; If emulated SP >= saved_sp, the RTS has popped back to (or past) entry level.
	lda _sp
	cmp _native_jsr_saved_sp
	bcc .njsr_loop			; SP < saved_sp → still in subroutine, dispatch next block
	
	; Subroutine completed (SP restored). _pc and _status are already
	; set correctly by the nrts template's inner dispatch.
	; Restore outer saved_sp, pop njsr's PHP, return 0.
	pla						; restore outer saved_sp
	sta _native_jsr_saved_sp
	pla						; discard njsr's PHP (don't overwrite _status!)
	lda #0					; return 0 = executed from flash
	rts						; return to outer dispatch_on_pc's JSR .dispatch_addr_instruction

.njsr_exit:
	; Bail to C — subroutine hit something that needs recompile/interpret.
	; _status was already saved by the last block's epilogue.
	; Preserve dispatch result (A=1 compile, A=2 interpret) across stack cleanup.
	tax						; save dispatch result in X
	pla						; restore outer saved_sp
	sta _native_jsr_saved_sp
	pla						; discard njsr's PHP
	txa						; return actual dispatch result (1 or 2)
	rts
	
not_recompiled:
	; A still holds the flag byte ($00 for uninitialized, or a value with bit 7 set)
	; $00 = never seen before → compile (return 1)
	; INTERPRETED bit ($40) CLEAR but non-zero → was explicitly marked interpret-only (return 2)
	; INTERPRETED bit ($40) SET → not yet processed, needs recompile (return 1)
	beq .needs_compile    ; $00 = uninitialized → compile
	and #INTERPRETED
	bne .needs_compile    ; INTERPRETED bit SET → compile
	lda #2	; interpret this PC
	rts

.needs_compile:
	lda #1 ; needs recompile
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

	; Pages $50-$FF: unmapped on Exidy — route to safe WRAM page.
	; Without these, LDA address_decoding_table,X with X>=$50 reads
	; padding zeros → writes to NES zero page → corrupts emulator state.
	db >(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE)
	db >(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE)
	db >(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE)
	db >(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE)
	db >(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE)
	db >(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE)
	db >(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE)
	db >(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE)
	db >(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE)
	db >(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE)
	db >(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE),>(_RAM_BASE)

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
; STA ($zp),Y template — native handler, no write6502() for common cases.
; Compile-time: patch byte at _sta_indy_zp_patch with the ZP pointer address.
; Runtime: saves state, calls _sta_indy_handler (native address-table handler),
;          restores X/A/flags.
; STX _x before LDX ensures the emulated X is saved even if modified mid-block.
; - RELOCATABLE CODE - INTERNAL ACCESS ONLY -
_sta_indy_template:
	php				; save flags
	pha				; save A (value to store) — handler reads from stack
	stx _x				; save current emulated X (may differ from block entry)
_sta_indy_zp_patch = * + 1
	ldx #$FF			; patched: ZP pointer address
	jsr _sta_indy_handler		; native handler (uses address tables, not write6502)
	ldx _x				; restore emulated X
	pla				; restore A
	plp				; restore flags
_sta_indy_template_end:

_sta_indy_template_size: db (_sta_indy_template_end - _sta_indy_template)

;=======================================================
	section "data"
	global _native_sta_indy_tmpl, _native_sta_indy_tmpl_size
	global _native_sta_indy_emu_lo, _native_sta_indy_emu_hi
	zpage _indy_ea
;-------------------------------------------------------
; Native STA ($zp),Y template — reads pointer from emulated RAM,
; translates hi byte via address_decoding_table, stores through _indy_ea.
; Compile-time: patch 2 values before copying:
;   _native_sta_indy_emu_lo  = emulated lo address (16-bit)
;   _native_sta_indy_emu_hi  = emulated hi address (16-bit)
; Runtime: 21 bytes, ~37 cycles.
; Works for ANY Exidy address (screen, RAM, ROM) — no fixed hi_offset.
; - RELOCATABLE CODE - INTERNAL ACCESS ONLY -
_native_sta_indy_tmpl:
	pha				; save store value
	stx _x				; save emulated X
_native_sta_indy_emu_lo = * + 1
	lda $FFFF			; patched: emulated RAM lo
	sta _indy_ea			; temp ptr lo
_native_sta_indy_emu_hi = * + 1
	ldx $FFFF			; patched: emulated RAM hi → X
	lda address_decoding_table,x	; look up NES page
	sta _indy_ea+1			; temp ptr hi
	ldx _x				; restore emulated X
	pla				; restore store value
	sta (_indy_ea),y		; native indirect write
_native_sta_indy_tmpl_end:

_native_sta_indy_tmpl_size: db (_native_sta_indy_tmpl_end - _native_sta_indy_tmpl)

;=======================================================
	section "data"
	global _screen_diff_build_list
	global _screen_shadow, _vram_update_list, _vram_list_pos
;-------------------------------------------------------
; screen_diff_build_list — WRAM-resident screen diff + lnList builder.
; Compares SCREEN_RAM_BASE against screen_shadow, builds lnList entries
; for changed tiles, and updates shadow.  Lives in WRAM so it costs
; zero bytes of bank space.
;
; Returns: A = result code
;   0 = no changes found
;   1 = incremental list built (list_pos in _vram_list_pos)
;   $FF = overflow (too many changes for one vblank)
;
; PPU address mapping:
;   offset $000-$3BF → nametable 0 ($2000 + offset)
;   offset $3C0-$3FF → nametable 2 ($2800 + (offset - $3C0))

VRAM_UPDATE_MAX = 96	; 32 tiles × 3 bytes (must fit in vblank: 32×40≈1280 cyc)

	section "zpage"
_vram_list_pos:	reserve 1	; current write position in update list
	section "data"

; Optimized screen diff: uses absolute,Y (4 cyc) instead of
; (indirect),Y (5 cyc) per access, and unrolls by page to
; eliminate per-byte page-counter overhead.
; Hot path: 15 cycles/byte (pages 0-2), 17 cycles/byte (page 3).

_screen_diff_build_list:
	lda #0
	sta _vram_list_pos

	;--- Page 0: SCREEN_RAM_BASE+$000 vs screen_shadow+$000, PPU $20xx ---
	ldy #0
.p0_loop:
	lda _SCREEN_RAM_BASE,y
	cmp _screen_shadow,y
	bne .p0_changed
.p0_next:
	iny
	bne .p0_loop

	;--- Page 1: +$100, PPU $21xx ---
	ldy #0
.p1_loop:
	lda _SCREEN_RAM_BASE+$100,y
	cmp _screen_shadow+$100,y
	bne .p1_changed
.p1_next:
	iny
	bne .p1_loop

	;--- Page 2: +$200, PPU $22xx ---
	ldy #0
.p2_loop:
	lda _SCREEN_RAM_BASE+$200,y
	cmp _screen_shadow+$200,y
	bne .p2_changed
.p2_next:
	iny
	bne .p2_loop

	;--- Page 3: +$300, PPU $23xx, only $C0 bytes (skip $3C0+) ---
	ldy #0
.p3_loop:
	lda _SCREEN_RAM_BASE+$300,y
	cmp _screen_shadow+$300,y
	bne .p3_changed
.p3_next:
	iny
	cpy #$C0
	bne .p3_loop

	;--- Return result ---
	lda _vram_list_pos
	beq .diff_no_changes
	cmp #VRAM_UPDATE_MAX
	bcs .diff_overflow
	; Terminate the list with $FF (lfEnd)
	tax
	lda #$FF
	sta _vram_update_list,x
	lda #1			; result = incremental list built
	rts
.diff_overflow:
	lda #$FF		; result = overflow
	rts
.diff_no_changes:
	lda #0			; result = no changes
	rts

	;--- Per-page changed handlers ---
	; A = new tile value, Y = offset in page.
	; Shadow is updated unconditionally (even on overflow, keeping it in sync).
	; No PHA/PLA needed: store A (tile) first, then clobber for PPU bytes.
.p0_changed:
	sta _screen_shadow,y
	ldx _vram_list_pos
	cpx #VRAM_UPDATE_MAX
	bcs .p0_next		; overflow: shadow updated, skip list entry
	sta _vram_update_list+2,x	; tile value (A still has it)
	tya
	sta _vram_update_list+1,x	; PPU lo = Y
	lda #$20
	sta _vram_update_list,x		; PPU hi
	inx
	inx
	inx
	stx _vram_list_pos
	jmp .p0_next

.p1_changed:
	sta _screen_shadow+$100,y
	ldx _vram_list_pos
	cpx #VRAM_UPDATE_MAX
	bcs .p1_next
	sta _vram_update_list+2,x
	tya
	sta _vram_update_list+1,x
	lda #$21
	sta _vram_update_list,x
	inx
	inx
	inx
	stx _vram_list_pos
	jmp .p1_next

.p2_changed:
	sta _screen_shadow+$200,y
	ldx _vram_list_pos
	cpx #VRAM_UPDATE_MAX
	bcs .p2_next
	sta _vram_update_list+2,x
	tya
	sta _vram_update_list+1,x
	lda #$22
	sta _vram_update_list,x
	inx
	inx
	inx
	stx _vram_list_pos
	jmp .p2_next

.p3_changed:
	sta _screen_shadow+$300,y
	ldx _vram_list_pos
	cpx #VRAM_UPDATE_MAX
	bcs .p3_next
	sta _vram_update_list+2,x
	tya
	sta _vram_update_list+1,x
	lda #$23
	sta _vram_update_list,x
	inx
	inx
	inx
	stx _vram_list_pos
	jmp .p3_next

;-------------------------------------------------------
; BSS for shadow buffer and update list
	section "bss"
_screen_shadow:		reserve $400	; 1024 bytes
_vram_update_list:	reserve 97	; VRAM_UPDATE_MAX + 1 (terminator)
	section "data"

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
	global _opcode_6502_php, _opcode_6502_php_size
;-------------------------------------------------------
; PHP: push (P | BREAK | CONSTANT) to emulated stack.
; Preserves A, X, Y, NES P (guest flags unchanged by PHP).
_opcode_6502_php:
	php					; [1] save guest P for restoration
	pha					; [2] save guest A
	php					; [3] push guest P again (extract byte)
	pla					; A = guest P byte (from [3])
	ora #$30			; set B + CONSTANT (PHP always sets these)
	stx _x				; save X
	ldx _sp				; X = emulated SP
	sta _RAM_BASE + $100, x	; push status to emulated stack
	dec _sp				; decrement emulated SP
	ldx _x				; restore X
	pla					; restore guest A (from [2])
	plp					; restore guest P (from [1])

_opcode_6502_php_end:
_opcode_6502_php_size:	db (_opcode_6502_php_end - _opcode_6502_php)

;=======================================================
	section "text"
	global _opcode_6502_plp, _opcode_6502_plp_size
;-------------------------------------------------------
; PLP: pull from emulated stack, set as new NES P (guest flags).
; Preserves A, X, Y.  PLP is the last instruction so it correctly
; sets ALL flags (N, V, -, B, D, I, Z, C) from the pulled byte.
_opcode_6502_plp:
	sta _a				; save guest A (PLP doesn't change A)
	stx _x				; save X
	inc _sp				; increment emulated SP first
	ldx _sp				; X = new SP
	lda _RAM_BASE + $100, x	; A = pulled status byte
	ldx _x				; restore X
	ora #$20			; set CONSTANT bit
	pha					; push new status to NES stack
	lda _a				; restore guest A (Z/N corrupted here...)
	plp					; NES P = new status (overwrites ALL flags) ✓

_opcode_6502_plp_end:
_opcode_6502_plp_size:	db (_opcode_6502_plp_end - _opcode_6502_plp)

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
; Native JSR template (36 bytes)
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
	zpage _native_jsr_saved_sp
_opcode_6502_njsr:
	php							; +0  save flags (popped by flash_dispatch_return_no_regs)
	sta _a						; +1  save A
	stx _x						; +3  save X
	sty _y						; +5  save Y
	ldx _sp						; +7  load emulated SP
	stx _native_jsr_saved_sp	; +9  save pre-push SP for trampoline
_opcode_6502_njsr_ret_hi_loc:
	lda #$FF					; +11 LDA #>(return_addr) - PATCH
	sta _RAM_BASE + $100, x		; +13 push high byte
	dex							; +16
_opcode_6502_njsr_ret_lo_loc:
	lda #$FF					; +17 LDA #<(return_addr) - PATCH
	sta _RAM_BASE + $100, x		; +19 push low byte
	dex							; +22
	stx _sp						; +23 save updated SP
_opcode_6502_njsr_tgt_lo_loc:
	lda #$FF					; +25 LDA #<target - PATCH
	sta _pc						; +27
_opcode_6502_njsr_tgt_hi_loc:
	lda #$FF					; +29 LDA #>target - PATCH
	sta _pc+1					; +31
	jmp _native_jsr_trampoline	; +33 trampoline loops then exits to C via WRAM
	
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
; NES ZP slots for native pointer mirroring.
; These are real NES zero-page addresses used by native
; STA (zp),Y instructions.  The linker assigns actual
; ZP addresses; C code reads them via &zp_mirror_0 etc.
	section "zpage"
	global _zp_mirror_0, _zp_mirror_1, _zp_mirror_2
;-------------------------------------------------------
_zp_mirror_0:	reserve 2	; mirror for pointer pair 0
_zp_mirror_1:	reserve 2	; mirror for pointer pair 1
_zp_mirror_2:	reserve 2	; mirror for pointer pair 2

;=======================================================
	section "data"
	global _sta_indy_handler
;-------------------------------------------------------
; STA (zp),Y native handler — uses address decoding tables for direct writes.
; Only falls back to write6502() for I/O registers ($5000+).
;
; Called via template: PHP / PHA / STX _x / LDX #zp / JSR handler / LDX _x / PLA / PLP
; On entry: X = ZP pointer address, Y = emulated Y index (live)
; Stack:  [JSR_ret_lo @ SP+1] [JSR_ret_hi @ SP+2] [saved_A @ SP+3] [saved_P @ SP+4]
;
; Uses address_decoding_table[exidy_hi] for high-byte translation,
; address_action_table[exidy_hi] for side-effect detection (screen/char dirty).
_sta_indy_handler:
	sty _indy_y			; save emulated Y for address calc + restore
	
	; Build pointer to RAM_BASE + X (X = ZP address from template patch)
	clc
	txa
	adc #<_RAM_BASE
	sta _indy_ptr
	lda #>_RAM_BASE
	adc #0
	sta _indy_ptr + 1
	
	; Read 16-bit pointer from emulated zero page
	ldy #0
	lda (_indy_ptr),y	; low byte of Exidy pointer
	sta _indy_ea
	iny
	lda (_indy_ptr),y	; high byte of Exidy pointer
	sta _indy_ea + 1
	
	; Add emulated Y to get effective Exidy address
	lda _indy_ea
	clc
	adc _indy_y
	sta _indy_ea
	bcc .nc
	inc _indy_ea + 1
.nc:
	
	; Range check: I/O registers ($5000+) need full write6502
	lda _indy_ea + 1
	cmp #$50
	bcs .io_fallback
	
	; Translate high byte via address decoding table
	tax
	lda address_decoding_table,x
	sta _indy_ea + 1	; NES high byte
	
	; Check action table for side effects (screen/char RAM dirty flags)
	lda address_action_table,x	; X still = Exidy high byte
	bmi .has_side_effect
	
	; === Fast path: simple write (RAM, or ROM which is harmless) ===
	tsx
	lda $103,x			; value from stack (SP+3 = pushed A)
	ldy #0
	sta (_indy_ea),y	; direct write to translated NES address
	ldy _indy_y			; restore emulated Y
	rts

.has_side_effect:
	cmp #$C0
	bcs .char_write
	; === Screen RAM write ===
	inc _screen_ram_updated
	tsx
	lda $103,x
	ldy #0
	sta (_indy_ea),y
	ldy _indy_y
	rts

.char_write:
	; === Character RAM write ===
	inc _character_ram_updated
	tsx
	lda $103,x
	ldy #0
	sta (_indy_ea),y
	ldy _indy_y
	rts

.io_fallback:
	; === Rare: I/O register write ($5000+) — use write6502 for sprite regs etc. ===
	; _indy_ea = effective Exidy address (untranslated)
	; Save _x and _y ZP vars (write6502 is C, may trash ZP temporaries)
	lda _x
	pha
	lda _y
	pha
	
	; Call write6502(_indy_ea, value) using vbcc calling convention
	lda _indy_ea
	sta r0
	lda _indy_ea + 1
	sta r1
	; Get value from stack — 2 pushes deeper now
	; Stack: [_y] [_x] [JSR_ret_lo] [JSR_ret_hi] [saved_A] [saved_P]
	;        SP+1 SP+2  SP+3         SP+4         SP+5      SP+6
	tsx
	lda $105,x
	sta r2
	jsr _write6502
	
	; Restore ZP vars
	pla
	sta _y
	pla
	sta _x
	
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
	ifnd PLATFORM_NES
	align 14		; PC table: $0000-$1FFF (Exidy has no code here)
	endif
	; bank19 on NES = SA/init code, no align — section used by C code
_flash_cache_pc:	
	section "bank20"
	ifnd PLATFORM_NES
	align 14		; PC table: Exidy ROM $2000-$3FFF
	endif
	; bank20 on NES = NES PRG-ROM data, no align — section used by asm incbin
	ifdef PLATFORM_NES
	; NES: skip bank21 entirely — used for BANK_RENDER C code.
	; If we enter section "bank21" here, the linker overlaps this
	; reserve with the C-emitted render_video_b2 / metrics code.
	; PC table for $4000-$5FFF is dead on NES anyway (I/O space).
	else
	section "bank21"
	align 14		; PC table: $4000-$5FFF (dead on Exidy but allocated)
	endif
	section "bank22"
	ifdef PLATFORM_NES
	align 14		; PC table: NES PRG-RAM $6000-$7FFF
	endif
	; bank22 on Exidy = BANK_RENDER code, no align
	section "bank23"
	ifdef PLATFORM_NES
	; NES: bank23 repurposed for CHR data, no align
	else
	; Exidy: bank23 repurposed for platform ROM data, no align
	endif
	section "bank24"
	ifdef PLATFORM_NES
	align 14		; NES: PC table for $A000-$BFFF (PRG-ROM mirror)
	else
	; Exidy: bank24 repurposed for SA code, no align
	endif
	section "bank25"
	ifdef PLATFORM_NES
	align 14		; NES: PC table for $C000-$DFFF (PRG-ROM)
	else
	; Exidy: bank25 repurposed for init code, no align
	endif
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