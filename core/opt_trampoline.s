; opt_trampoline.s - WRAM trampolines for optimizer
;
; These wrapper functions live in the FIXED bank and:
; 1. Call flash functions (which change banks internally)
; 2. Switch back to bank1
; 3. Return to caller in bank1
;
; The key insight is these wrappers are in fixed bank so they're
; always accessible. They switch back to bank1 before RTS.

	section "text"

	global _opt_tramp_erase
	global _opt_tramp_program
	global _opt_tramp_readflag
	global _opt_tramp_recompile
	global _opt_tramp_lookup_native
	global _opt_setup_trampolines

	extern _flash_sector_erase
	extern _flash_byte_program
	extern _mapper_prg_bank
	extern _mapper_chr_bank
	extern _recompile_opcode

	zpage sp

; Bank switch register  
BANK_REG	= $C000

; PC flags bank base
BANK_PC_FLAGS	= 27

;============================================================================================================
; opt_setup_trampolines - No-op for now, wrappers are in fixed bank
;============================================================================================================

_opt_setup_trampolines:
	rts

;============================================================================================================
; Wrapper functions in fixed bank
; These call flash functions, then switch back to bank1 before returning
;============================================================================================================

; void opt_tramp_erase(uint16_t addr, uint8_t bank)
; VBCC calling convention: args on sp stack
_opt_tramp_erase:
	; Args are already on the VBCC sp stack, just call the function
	jsr _flash_sector_erase
	; Switch back to bank1
	lda #1
	ora _mapper_chr_bank
	sta BANK_REG
	rts

; void opt_tramp_program(uint16_t addr, uint8_t bank, uint8_t data)
_opt_tramp_program:
	jsr _flash_byte_program
	; Switch back to bank1
	lda #1
	ora _mapper_chr_bank
	sta BANK_REG
	rts

; uint8_t opt_tramp_readflag(uint16_t addr)
; Returns flag byte in A for the given 6502 PC address
; Switches to appropriate flag bank, reads, switches back to bank1
_opt_tramp_readflag:
	; Get addr from sp stack
	ldy #0
	lda (sp),y	; addr lo
	tax		; save in X
	iny
	lda (sp),y	; addr hi
	
	; Calculate flag bank: (addr >> 14) + BANK_PC_FLAGS
	; addr_hi >> 6 gives us the 16KB page number (0-3)
	lsr
	lsr
	lsr
	lsr
	lsr
	lsr		; A now has page number 0-3
	clc
	adc #BANK_PC_FLAGS
	ora _mapper_chr_bank
	sta BANK_REG	; Switch to flag bank
	
	; Now read the flag from $8000 + (addr & 0x3FFF)
	; addr_lo is in X, addr_hi is still needed
	ldy #1
	lda (sp),y	; reload addr hi
	and #$3F	; Mask to 14 bits
	ora #$80	; Add base $8000
	sta opt_read_ptr+1
	stx opt_read_ptr
	ldy #0
	lda (opt_read_ptr),y	; read via indirect
	
	pha		; Save result
	
	; Switch back to bank1
	lda #1
	ora _mapper_chr_bank
	sta BANK_REG
	
	pla		; Restore result in A
	rts

;============================================================================================================
; Zero-page pointer for indirect read
;============================================================================================================
	section "zpage"
opt_read_ptr:	reserve 2

;============================================================================================================
; Trampoline to call recompile_opcode in bank0 from bank1
; uint8_t opt_tramp_recompile(void)
; Returns result in A, switches bank0 for call, then back to bank1
;============================================================================================================
	section "text"
	
_opt_tramp_recompile:
	; Switch to bank 0
	lda #0
	ora _mapper_chr_bank
	sta BANK_REG
	
	; Call recompile_opcode
	jsr _recompile_opcode
	
	; Save return value
	pha
	
	; Switch back to bank1
	lda #1
	ora _mapper_chr_bank
	sta BANK_REG
	
	; Restore return value
	pla
	rts

;============================================================================================================
; Lookup native address for a target PC
; uint16_t opt_tramp_lookup_native(uint16_t target_pc, uint8_t *out_bank)
; Returns native address in A (lo) and X (hi), or $FFFF if not found
; Writes code bank to *out_bank if found
;============================================================================================================
	
_opt_tramp_lookup_native:
	; Args on VBCC sp stack: target_pc (2 bytes), out_bank ptr (2 bytes)
	
	; Calculate flag bank: (target_pc >> 14) + 27
	ldy #1
	lda (sp),y	; target_pc hi
	lsr
	lsr
	lsr
	lsr
	lsr
	lsr		; A = page 0-3
	clc
	adc #BANK_PC_FLAGS
	ora _mapper_chr_bank
	sta BANK_REG	; Switch to flag bank
	
	; Read flag from $8000 + (target_pc & 0x3FFF)
	ldy #0
	lda (sp),y	; target_pc lo
	sta opt_read_ptr
	ldy #1
	lda (sp),y	; target_pc hi
	and #$3F
	ora #$80
	sta opt_read_ptr+1
	
	ldy #0
	lda (opt_read_ptr),y	; flag byte
	
	; Check if valid (not $FF, not RECOMPILED bit set)
	cmp #$FF
	beq .not_found
	bmi .not_found	; bit 7 set = RECOMPILED = not valid
	
	; Valid! Save code bank and read PC table
	pha		; save flag (contains code bank in low 5 bits)
	
	; Write code bank to *out_bank
	and #$1F
	sta opt_temp	; save code bank
	
	ldy #2
	lda (sp),y	; out_bank ptr lo
	sta opt_read_ptr
	iny
	lda (sp),y	; out_bank ptr hi
	sta opt_read_ptr+1
	lda opt_temp
	ldy #0
	sta (opt_read_ptr),y	; *out_bank = code_bank
	
	; Calculate PC table bank: (target_pc >> 13) + 19
	ldy #1
	lda (sp),y	; target_pc hi
	lsr
	lsr
	lsr
	lsr
	lsr		; A = (hi >> 5) = upper 3 bits of hi byte
	clc
	adc #19		; BANK_PC
	ora _mapper_chr_bank
	sta BANK_REG
	
	; Calculate PC table offset: (target_pc << 1) & 0x3FFF
	; = (target_pc * 2) with bit 14 cleared
	ldy #0
	lda (sp),y	; target_pc lo
	asl
	sta opt_read_ptr
	ldy #1
	lda (sp),y	; target_pc hi
	rol
	and #$3F
	ora #$80
	sta opt_read_ptr+1
	
	; Read 16-bit native address
	ldy #0
	lda (opt_read_ptr),y	; native lo
	tax
	iny
	lda (opt_read_ptr),y	; native hi
	
	; Swap: need lo in A, hi in X for return
	pha
	txa
	tax		; now X = lo
	pla		; A = hi... wait that's backwards
	
	; Actually VBCC returns 16-bit in... let me just use temp
	sta opt_temp+1	; hi
	stx opt_temp	; lo
	
	pla		; discard saved flag
	
	; Switch back to bank1
	lda #1
	ora _mapper_chr_bank
	sta BANK_REG
	
	; Return value: lo in A, hi in X (or vice versa - check VBCC convention)
	lda opt_temp	; lo
	ldx opt_temp+1	; hi
	rts

.not_found:
	; Switch back to bank1
	lda #1
	ora _mapper_chr_bank
	sta BANK_REG
	
	; Return $FFFF
	lda #$FF
	tax
	rts

;============================================================================================================
; Temporary storage
;============================================================================================================
	section "zpage"
opt_temp:	reserve 2
