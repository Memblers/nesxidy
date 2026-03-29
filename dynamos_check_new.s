;vcprmin=10000
	section	text
	global	_translate_address
_translate_address:
	lda	sp
	bne	l21
	dec	sp+1
l21:
	dec	sp
	lda	r1
	ldx	#0
	sta	r2
	cmp	#4
	bcs	l4
	ldx	#>(_RAM_BASE)
	lda	#<(_RAM_BASE)
	clc
	adc	r0
	pha
	txa
	adc	r1
	tax
	pla
	jmp	l1
l4:
	lda	r2
	cmp	#64
	bcs	l7
	lda	r0
	sec
	sbc	#0
	sta	r2
	lda	r1
	sbc	#40
	sta	r3
	ldx	#>(_ROM_NAME)
	lda	#<(_ROM_NAME)
	clc
	adc	r2
	sta	r4
	txa
	adc	r3
	sta	r5
	cmp	#128
	bcc	l9
	bne	l22
	lda	r4
	cmp	#0
	bcc	l9
l22:
	lda	r5
	cmp	#192
	bcc	l23
	bne	l9
	lda	r4
	cmp	#0
	bcs	l9
l23:
	ldx	#0
	txa
	jmp	l1
l9:
	ldx	r5
	lda	r4
	jmp	l1
l7:
	lda	r2
	ldy	#0
	sta	(sp),y
	lda	r2
	cmp	#72
	bcs	l13
	lda	r0
	sec
	sbc	#0
	sta	r2
	lda	r1
	sbc	#64
	sta	r3
	ldx	#>(_SCREEN_RAM_BASE)
	lda	#<(_SCREEN_RAM_BASE)
	clc
	adc	r2
	pha
	txa
	adc	r3
	tax
	pla
	jmp	l1
l13:
	lda	r2
	cmp	#80
	bcs	l16
	lda	r0
	sec
	sbc	#0
	sta	r2
	lda	r1
	sbc	#72
	sta	r3
	ldx	#>(_CHARACTER_RAM_BASE)
	lda	#<(_CHARACTER_RAM_BASE)
	clc
	adc	r2
	pha
	txa
	adc	r3
	tax
	pla
	jmp	l1
l16:
	ldx	#0
	txa
l1:
	sta	r31
	inc	sp
	bne	l24
	inc	sp+1
l24:
	lda	r31
	rts
; stacksize=0+??
;vcprmin=10000
	section	text
	global	_run_6502
_run_6502:
	sec
	lda	sp
	sbc	#21
	sta	sp
	bcs	l114
	dec	sp+1
l114:
	ldy	#20
	jsr	___rsave12
	lda	_decimal_mode
	beq	l37
	lda	_status
	and	#8
	beq	l36
	lda	#0
	jsr	_bankswitch_prg
	jsr	_interpret_6502
	jmp	l31
l36:
	lda	#0
	sta	_decimal_mode
l37:
	jsr	_dispatch_on_pc
	tax
	beq	l40
	cpx	#1
	beq	l41
	cpx	#2
	bne	l41
	inc	_cache_interpret
	bne	l115
	inc	1+_cache_interpret
	bne	l115
	inc	2+_cache_interpret
	bne	l115
	inc	3+_cache_interpret
l115:
	lda	#0
	jsr	_bankswitch_prg
	jsr	_interpret_6502
	jmp	l31
l40:
	inc	_cache_hits
	bne	l116
	inc	1+_cache_hits
	bne	l116
	inc	2+_cache_hits
	bne	l116
	inc	3+_cache_hits
l116:
	jmp	l31
l41:
	inc	_cache_misses
	bne	l117
	inc	1+_cache_misses
	bne	l117
	inc	2+_cache_misses
	bne	l117
	inc	3+_cache_misses
l117:
	lda	1+_pc
	sta	r27
	lda	_pc
	sta	r26
	lda	_pc
	sta	_cache_entry_pc_lo
	lda	1+_pc
	ldx	#0
	sta	_cache_entry_pc_hi
	txa
	sta	_cache_index
	txa
	sta	_code_index
	txa
	sta	_cache_flag
	txa
	sta	_block_has_jsr
	txa
	sta	l26
	txa
	sta	l27
; volatile barrier
	lda	#0
	sta	l28
	sta	l29
	sta	r0
	ldy	#0
	sta	(sp),y
	tax
l93:
	lda	#0
	sta	0+_block_ci_map,x ;am(x)
	inx
	cpx	#64
	bcc	l93
	lda	_next_free_sector
	ldy	#2
	sta	(sp),y
	lda	#250
	sta	r0
	jsr	_flash_sector_alloc
	cmp	#0
	bne	l47
	lda	#0
	jsr	_bankswitch_prg
	jsr	_interpret_6502
	jmp	l31
l47:
	lda	_next_free_sector
	ldy	#3
	sta	(sp),y
	lda	_flash_code_address
	ldy	#0
	sta	(sp),y
	lda	1+_flash_code_address
	and	#15
	iny
	sta	(sp),y
	lda	1+_pc
	sta	r1
	lda	_pc
	sta	r0
	jsr	_setup_flash_pc_tables
	lda	r26
	sta	r21
	ldy	#4
	sta	(sp),y
l98:
; volatile barrier
	lda	l28
	beq	l52
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r4
	lda	#>(_cache_code)
	adc	#0
	sta	r5
	inc	_code_index
	lda	#40
	ldy	#0
	sta	(r4),y
; volatile barrier
	lda	#0
	sta	l28
l52:
	lda	1+_pc
	sta	r21
	lda	_pc
	sta	r20
	lda	_code_index
	sta	r16
	lda	_pc
	sec
	ldy	#4
	sbc	(sp),y
	and	#63
	tax
	lda	_code_index
	clc
	adc	#1
	sta	0+_block_ci_map,x ;am(x)
	lda	#0
	sta	l30
	jsr	_recompile_opcode
	lda	_code_index
	sec
	sbc	r16
	sta	r18
	lda	r21
	sta	r1
	lda	r20
	sta	r0
	jsr	_setup_flash_pc_tables
	lda	#0
	sta	r17
	lda	r18
	beq	l103
	lda	r16
	sta	r22
	lda	#0
	sta	r23
	lda	r16
	sta	r19
l99:
	ldy	r19
	lda	0+_cache_code,y ;am(r19)
	sta	r31
	lda	_flash_code_address
	clc
	adc	#8
	sta	r6
	lda	1+_flash_code_address
	adc	#0
	sta	r7
	lda	r6
	clc
	adc	r22
	sta	r6
	lda	r7
	adc	r23
	sta	r7
	lda	r17
	sta	r4
	lda	#0
	sta	r5
	lda	r4
	clc
	adc	r6
	sta	r0
	lda	r5
	adc	r7
	sta	r1
	lda	_flash_code_bank
	sta	r2
	lda	r31
	sta	r4
	jsr	_flash_byte_program
	inc	r17
	inc	r19
	lda	r17
	cmp	r18
	bcc	l99
l103:
; volatile barrier
	lda	l28
	beq	l58
	lda	_flash_code_address
	clc
	adc	#8
	sta	r4
	lda	1+_flash_code_address
	adc	#0
	sta	r5
	lda	_code_index
	ldx	#0
	clc
	adc	r4
	sta	r0
	txa
	adc	r5
	sta	r1
	lda	_flash_code_bank
	sta	r2
	lda	#40
	sta	r4
	jsr	_flash_byte_program
l58:
; volatile barrier
	lda	_code_index
	ldy	#5
	sta	(sp),y
	lda	#0
	iny
	sta	(sp),y
; volatile barrier
	lda	l28
	iny
	sta	(sp),y
	lda	#0
	iny
	sta	(sp),y
; volatile barrier
	dey
	lda	(sp),y
	clc
	ldy	#5
	adc	(sp),y
	ldy	#7
	sta	(sp),y
	iny
	lda	(sp),y
	ldy	#6
	adc	(sp),y
	ldy	#8
	sta	(sp),y
; volatile barrier
	lda	#205
	dey
	cmp	(sp),y
	lda	#0
	iny
	sbc	(sp),y
	bvc	l118
	eor	#128
l118:
	bpl	l60
	lda	_cache_flag
	ora	#4
	sta	_cache_flag
	and	#223
	sta	_cache_flag
l60:
	lda	_cache_flag
	and	#2
	beq	l62
	lda	r16
	sta	r0
	lda	#64
	sta	r2
	jsr	_flash_cache_pc_update
	jmp	l68
l62:
	lda	l30
	beq	l65
	lda	r16
	sta	r0
	lda	#64
	sta	r2
	jsr	_flash_cache_pc_update
	jmp	l68
l65:
	lda	r18
	bne	l67
	lda	1+_pc
	cmp	r21
	bne	l119
	lda	_pc
	cmp	r20
	beq	l68
l119:
l67:
	lda	r16
	sta	r0
	lda	#128
	sta	r2
	jsr	_flash_cache_pc_update
	lda	r16
	ldx	#0
	clc
	adc	_flash_code_address
	pha
	txa
	adc	1+_flash_code_address
	tax
	pla
	clc
	adc	#8
	bcc	l120
	inx
l120:
	sta	r31
	lda	r21
	sta	r1
	lda	r20
	sta	r0
	lda	r31
	sta	r2
	stx	r3
	lda	_flash_code_bank
	sta	r4
	jsr	_opt2_notify_block_compiled
l68:
	lda	_cache_flag
	and	#32
	bne	l98
	ldy	#4
	lda	(sp),y
	sta	r21
	lda	_code_index
	bne	l71
	ldy	#3
	lda	(sp),y
	ldx	#0
	stx	r31
	asl
	rol	r31
	ldx	r31
	clc
	adc	#<(_sector_free_offset)
	sta	r4
	txa
	adc	#>(_sector_free_offset)
	sta	r5
	ldy	#1
	lda	(sp),y
	sta	(r4),y
	dey
	lda	(sp),y
	sta	(r4),y
	ldy	#2
	lda	(sp),y
	sta	_next_free_sector
	lda	#0
	jsr	_bankswitch_prg
	jsr	_interpret_6502
	jmp	l31
l71:
	lda	1+_pc
	sta	r9
	lda	_pc
	sta	r8
; volatile barrier
	lda	l28
	beq	l73
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r4
	lda	#>(_cache_code)
	adc	#0
	sta	r5
	inc	_code_index
	lda	#40
	ldy	#0
	sta	(r4),y
; volatile barrier
	lda	#0
	sta	l28
l73:
	lda	_code_index
	sta	r16
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r0
	lda	#>(_cache_code)
	adc	#0
	sta	r1
	inc	_code_index
	lda	#8
	ldy	#0
	sta	(r0),y
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r0
	lda	#>(_cache_code)
	adc	#0
	sta	r1
	inc	_code_index
	lda	#24
	sta	(r0),y
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r0
	lda	#>(_cache_code)
	adc	#0
	sta	r1
	inc	_code_index
	lda	#144
	sta	(r0),y
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r0
	lda	#>(_cache_code)
	adc	#0
	sta	r1
	inc	_code_index
	lda	#4
	sta	(r0),y
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r0
	lda	#>(_cache_code)
	adc	#0
	sta	r1
	inc	_code_index
	lda	#40
	sta	(r0),y
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r0
	lda	#>(_cache_code)
	adc	#0
	sta	r1
	inc	_code_index
	lda	#76
	sta	(r0),y
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r0
	lda	#>(_cache_code)
	adc	#0
	sta	r1
	inc	_code_index
	lda	#255
	sta	(r0),y
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r0
	lda	#>(_cache_code)
	adc	#0
	sta	r1
	inc	_code_index
	lda	#255
	sta	(r0),y
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r0
	lda	#>(_cache_code)
	adc	#0
	sta	r1
	inc	_code_index
	lda	#133
	sta	(r0),y
	lda	#>(_a)
	sta	r1
	lda	#<(_a)
	sta	r0
	sta	r31
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r0
	lda	#>(_cache_code)
	adc	#0
	sta	r1
	lda	r31
	inc	_code_index
	sta	(r0),y
	sta	r31
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r0
	lda	#>(_cache_code)
	adc	#0
	sta	r1
	lda	r31
	inc	_code_index
	sta	r31
	lda	#169
	sta	(r0),y
	lda	r8
	sta	r18
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r0
	lda	#>(_cache_code)
	adc	#0
	sta	r1
	lda	r31
	inc	_code_index
	sta	r31
	lda	r18
	sta	(r0),y
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r0
	lda	#>(_cache_code)
	adc	#0
	sta	r1
	lda	r31
	inc	_code_index
	sta	r31
	lda	#133
	sta	(r0),y
	lda	#>(_pc)
	sta	r5
	lda	#<(_pc)
	sta	r4
	sta	r1
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r4
	lda	#>(_cache_code)
	adc	#0
	sta	r5
	lda	r31
	inc	_code_index
	sta	r31
	lda	r1
	sta	(r4),y
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r4
	lda	#>(_cache_code)
	adc	#0
	sta	r5
	lda	r31
	inc	_code_index
	sta	r31
	lda	#169
	sta	(r4),y
	lda	r31
	sta	r30
	lda	r9
	ldx	#0
	sta	r4
	stx	r5
	lda	r30
	sta	r31
	lda	r4
	sta	r17
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r4
	lda	#>(_cache_code)
	adc	#0
	sta	r5
	lda	r31
	inc	_code_index
	sta	r31
	lda	r17
	sta	(r4),y
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r4
	lda	#>(_cache_code)
	adc	#0
	sta	r5
	lda	r31
	inc	_code_index
	sta	r31
	lda	#133
	sta	(r4),y
	lda	r31
	inc	r1
	sta	r31
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r4
	lda	#>(_cache_code)
	adc	#0
	sta	r5
	lda	r31
	inc	_code_index
	sta	r31
	lda	r1
	sta	(r4),y
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r0
	lda	#>(_cache_code)
	adc	#0
	sta	r1
	lda	r31
	inc	_code_index
	sta	r31
	lda	#76
	sta	(r0),y
	lda	#>(_cross_bank_dispatch)
	sta	r5
	lda	#<(_cross_bank_dispatch)
	sta	r4
	sta	r1
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r6
	lda	#>(_cache_code)
	adc	#0
	sta	r7
	lda	r31
	inc	_code_index
	sta	r31
	lda	r1
	sta	(r6),y
	lda	r31
	sta	r30
	lda	r5
	sta	r4
	stx	r5
	lda	r30
	sta	r31
	lda	r4
	sta	r1
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r4
	lda	#>(_cache_code)
	adc	#0
	sta	r5
	lda	r31
	inc	_code_index
	sta	r31
	lda	r1
	sta	(r4),y
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r0
	lda	#>(_cache_code)
	adc	#0
	sta	r1
	lda	r31
	inc	_code_index
	sta	r31
	lda	#8
	sta	(r0),y
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r0
	lda	#>(_cache_code)
	adc	#0
	sta	r1
	lda	r31
	inc	_code_index
	sta	r31
	lda	#133
	sta	(r0),y
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r0
	lda	#>(_cache_code)
	adc	#0
	sta	r1
	lda	r31
	inc	_code_index
	sta	(r0),y
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r0
	lda	#>(_cache_code)
	adc	#0
	sta	r1
	inc	_code_index
	lda	#169
	sta	(r0),y
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r0
	lda	#>(_cache_code)
	adc	#0
	sta	r1
	inc	_code_index
	lda	#255
	sta	(r0),y
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r0
	lda	#>(_cache_code)
	adc	#0
	sta	r1
	inc	_code_index
	lda	#141
	sta	(r0),y
	ldx	#>(_xbank_addr)
	lda	#<(_xbank_addr)
	sta	r1
	sta	r31
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r4
	lda	#>(_cache_code)
	adc	#0
	sta	r5
	lda	r31
	inc	_code_index
	sta	r31
	lda	r1
	sta	(r4),y
	lda	r31
	sta	r30
	stx	r29
	txa
	ldx	#0
	sta	r4
	stx	r5
	ldx	r29
	lda	r30
	sta	r31
	lda	r4
	sta	r2
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r4
	lda	#>(_cache_code)
	adc	#0
	sta	r5
	lda	r31
	inc	_code_index
	sta	r31
	lda	r2
	sta	(r4),y
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r4
	lda	#>(_cache_code)
	adc	#0
	sta	r5
	lda	r31
	inc	_code_index
	sta	r31
	lda	#169
	sta	(r4),y
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r4
	lda	#>(_cache_code)
	adc	#0
	sta	r5
	lda	r31
	inc	_code_index
	sta	r31
	lda	#255
	sta	(r4),y
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r4
	lda	#>(_cache_code)
	adc	#0
	sta	r5
	lda	r31
	inc	_code_index
	sta	r31
	lda	#141
	sta	(r4),y
	lda	r31
	inc	r1
	sta	r31
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r4
	lda	#>(_cache_code)
	adc	#0
	sta	r5
	lda	r31
	inc	_code_index
	sta	r31
	lda	r1
	sta	(r4),y
	lda	r31
	clc
	adc	#1
	bcc	l121
	inx
l121:
	sta	r30
	stx	r29
	txa
	ldx	#0
	sta	r0
	stx	r1
	ldx	r29
	lda	r0
	sta	r31
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r0
	lda	#>(_cache_code)
	adc	#0
	sta	r1
	lda	r31
	inc	_code_index
	sta	(r0),y
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r0
	lda	#>(_cache_code)
	adc	#0
	sta	r1
	inc	_code_index
	lda	#169
	sta	(r0),y
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r0
	lda	#>(_cache_code)
	adc	#0
	sta	r1
	inc	_code_index
	lda	#255
	sta	(r0),y
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r0
	lda	#>(_cache_code)
	adc	#0
	sta	r1
	inc	_code_index
	lda	#76
	sta	(r0),y
	ldx	#>(_xbank_trampoline)
	lda	#<(_xbank_trampoline)
	sta	r1
	sta	r31
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r4
	lda	#>(_cache_code)
	adc	#0
	sta	r5
	lda	r31
	inc	_code_index
	sta	r31
	lda	r1
	sta	(r4),y
	lda	r31
	sta	r30
	stx	r29
	txa
	ldx	#0
	sta	r0
	stx	r1
	ldx	r29
	lda	r0
	sta	r31
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r0
	lda	#>(_cache_code)
	adc	#0
	sta	r1
	lda	r31
	inc	_code_index
	sta	(r0),y
	lda	1+_flash_code_address
	sta	r1
	lda	_flash_code_address
	sta	r0
	lda	_flash_code_bank
	sta	r2
	lda	r21
	sta	r4
	jsr	_flash_byte_program
	lda	r27
	ldx	#0
	sta	r0
	stx	r1
	lda	r0
	sta	r31
	lda	_flash_code_address
	clc
	adc	#1
	sta	r0
	lda	1+_flash_code_address
	adc	#0
	sta	r1
	lda	_flash_code_bank
	sta	r2
	lda	r31
	sta	r4
	jsr	_flash_byte_program
	lda	_flash_code_address
	clc
	adc	#2
	sta	r0
	lda	1+_flash_code_address
	adc	#0
	sta	r1
	lda	_flash_code_bank
	sta	r2
	lda	r18
	sta	r4
	jsr	_flash_byte_program
	lda	_flash_code_address
	clc
	adc	#3
	sta	r0
	lda	1+_flash_code_address
	adc	#0
	sta	r1
	lda	_flash_code_bank
	sta	r2
	lda	r17
	sta	r4
	jsr	_flash_byte_program
	lda	_flash_code_address
	clc
	adc	#4
	sta	r0
	lda	1+_flash_code_address
	adc	#0
	sta	r1
	lda	_flash_code_bank
	sta	r2
	lda	_code_index
	sta	r4
	jsr	_flash_byte_program
	lda	_flash_code_address
	clc
	adc	#5
	sta	r0
	lda	1+_flash_code_address
	adc	#0
	sta	r1
	lda	_flash_code_bank
	sta	r2
	lda	r16
	sta	r4
	jsr	_flash_byte_program
	lda	#0
	sta	r19
	lda	_code_index
	beq	l104
l100:
	ldy	r19
	lda	0+_cache_code,y ;am(r19)
	sta	r31
	lda	_flash_code_address
	clc
	adc	#8
	sta	r6
	lda	1+_flash_code_address
	adc	#0
	sta	r7
	lda	r19
	sta	r4
	lda	#0
	sta	r5
	lda	r4
	clc
	adc	r6
	sta	r0
	lda	r5
	adc	r7
	sta	r1
	lda	_flash_code_bank
	sta	r2
	lda	r31
	sta	r4
	jsr	_flash_byte_program
	inc	r19
	lda	r19
	cmp	_code_index
	bcc	l100
l104:
	lda	_flash_code_address
	pha
	lda	1+_flash_code_address
	and	#15
	tax
	pla
	clc
	adc	#8
	sta	r4
	txa
	adc	#0
	sta	r5
	lda	_code_index
	ldx	#0
	clc
	adc	r4
	pha
	txa
	adc	r5
	tax
	pla
	sta	r31
	lda	_next_free_sector
	sta	r4
	lda	#0
	sta	r5
	lda	r31
	asl	r4
	rol	r5
	sta	r31
	lda	r4
	clc
	adc	#<(_sector_free_offset)
	sta	r4
	lda	r5
	adc	#>(_sector_free_offset)
	sta	r5
	lda	r31
	ldy	#0
	sta	(r4),y
	txa
	iny
	sta	(r4),y
	inc	l78
	lda	l78
	cmp	#8
	bcc	l80
	lda	#0
	sta	l78
	jsr	_opt2_sweep_pending_patches
	jsr	_opt2_scan_and_patch_epilogues
l80:
	lda	r27
	sta	1+_pc
	lda	r26
	sta	_pc
	jsr	_dispatch_on_pc
l31:
	ldy	#20
	jsr	___rload12
	clc
	lda	sp
	adc	#21
	sta	sp
	bcc	l122
	inc	sp+1
l122:
	rts
; stacksize=0+??
	section	data
l78:
	byte	0
;vcprmin=10000
	section	text
	global	_flash_sector_alloc
_flash_sector_alloc:
	lda	r0
	sta	r10
	lda	#0
	sta	r11
	lda	_next_free_sector
	sta	r2
	lda	#0
	sta	r1
l139:
	lda	r2
	cmp	#56
	bcc	l130
	lda	#0
	sta	r2
l130:
	lda	r1
	beq	l132
	lda	r2
	cmp	_next_free_sector
	beq	l127
l132:
	lda	#1
	sta	r1
	lda	r2
	ldx	#0
	stx	r31
	asl
	rol	r31
	ldx	r31
	clc
	adc	#<(_sector_free_offset)
	sta	r4
	txa
	adc	#>(_sector_free_offset)
	sta	r5
	ldy	#1
	lda	(r4),y
	tax
	dey
	lda	(r4),y
	clc
	adc	#23
	bcc	l142
	inx
l142:
	and	#240
	sta	r31
	sec
	sbc	#8
	sta	r8
	txa
	sbc	#0
	sta	r9
	lda	r31
	clc
	adc	r10
	sta	r6
	txa
	adc	r11
	sta	r7
	cmp	#16
	bcc	l135
	bne	l143
	lda	r6
	cmp	#0
	bcc	l135
	beq	l135
l143:
	inc	r2
	jmp	l139
l135:
	lda	r2
	lsr
	lsr
	clc
	adc	#4
	sta	r1
	lda	r2
	ldx	#0
	and	#3
	tax
	lda	#0
	stx	r31
	asl
	rol	r31
	asl
	rol	r31
	asl
	rol	r31
	asl
	rol	r31
	ldx	r31
	clc
	adc	#0
	pha
	txa
	adc	#128
	tax
	pla
	sta	r31
	lda	r1
	sta	_flash_code_bank
	lda	r31
	clc
	adc	r8
	sta	_flash_code_address
	txa
	adc	r9
	sta	1+_flash_code_address
	lda	r7
	ldy	#1
	sta	(r4),y
	lda	r6
	dey
	sta	(r4),y
	lda	r2
	sta	_next_free_sector
	lda	#1
	rts
l127:
	lda	#0
l123:
	rts
; stacksize=0+??
;vcprmin=10000
	section	text
	global	_lookup_native_addr_safe
_lookup_native_addr_safe:
	lda	r16
	pha
	lda	r17
	pha
	lda	r18
	pha
	lda	r19
	pha
	lda	r20
	pha
	lda	r1
	sta	r19
	lda	r0
	sta	r18
	lda	_mapper_prg_bank
	sta	r20
	lda	r19
	ldx	#0
	stx	r31
	lsr	r31
	ror
	lsr	r31
	ror
	lsr	r31
	ror
	lsr	r31
	ror
	lsr	r31
	ror
	lsr	r31
	ror
	ldx	r31
	sta	r0
	clc
	adc	#27
	jsr	_bankswitch_prg
	lda	r18
	pha
	lda	r19
	and	#63
	tax
	pla
	clc
	adc	#<(_flash_cache_pc_flags)
	sta	r2
	txa
	adc	#>(_flash_cache_pc_flags)
	sta	r3
	ldy	#0
	lda	(r2),y
	sta	r1
	and	#128
	beq	l147
	lda	r20
	jsr	_bankswitch_prg
	lda	#0
	jmp	l144
l147:
	lda	r1
	and	#31
	sta	_reserve_result_bank
	ldx	r19
	lda	r18
	stx	r31
	asl
	rol	r31
	ldx	r31
	sta	r16
	txa
	and	#63
	sta	r17
	lda	r19
	ldx	#0
	stx	r31
	lsr	r31
	ror
	lsr	r31
	ror
	lsr	r31
	ror
	lsr	r31
	ror
	lsr	r31
	ror
	ldx	r31
	sta	r0
	clc
	adc	#19
	jsr	_bankswitch_prg
	lda	#<(_flash_cache_pc)
	clc
	adc	r16
	sta	r4
	lda	#>(_flash_cache_pc)
	adc	r17
	sta	r5
	lda	#<(1+_flash_cache_pc)
	clc
	adc	r16
	sta	r2
	lda	#>(1+_flash_cache_pc)
	adc	r17
	sta	r3
	ldy	#0
	lda	(r2),y
	ldx	#0
	sta	r30
	stx	r29
	tax
	tya
	sta	r2
	stx	r3
	ldx	r29
	lda	(r4),y
	ldx	#0
	ora	r2
	sta	_reserve_result_addr
	txa
	ora	r3
	sta	1+_reserve_result_addr
	lda	r20
	jsr	_bankswitch_prg
	lda	#1
l144:
	sta	r31
	pla
	sta	r20
	pla
	sta	r19
	pla
	sta	r18
	pla
	sta	r17
	pla
	sta	r16
	lda	r31
	rts
; stacksize=0+??
;vcprmin=10000
	section	text
	global	_lookup_entry_list
_lookup_entry_list:
	sec
	lda	sp
	sbc	#4
	sta	sp
	bcs	l172
	dec	sp+1
l172:
	lda	r16
	pha
	lda	r18
	pha
	lda	r19
	pha
	lda	r1
	sta	r19
	lda	r0
	sta	r18
	lda	_mapper_prg_bank
	sta	r16
	lda	#18
	jsr	_bankswitch_prg
	lda	r18
	sta	r9
	lda	r19
	ldx	#0
	sta	r10
	txa
	sta	r5
	txa
	sta	r4
	lda	_entry_list_offset
	ora	1+_entry_list_offset
	beq	l165
	lda	#128
	sta	r1
	lda	#1
	sta	r0
	lda	#128
	sta	r3
	lda	#0
	sta	r2
l164:
	ldy	#0
	lda	(r2),y
	sta	r8
	cmp	#255
	bne	l155
	ldy	#0
	lda	(r0),y
	cmp	#255
	beq	l165
l155:
	lda	r3
	ldy	#1
	sta	(sp),y
	lda	r2
	dey
	sta	(sp),y
	lda	r5
	ldy	#3
	sta	(sp),y
	lda	r4
	dey
	sta	(sp),y
	lda	r8
	cmp	r9
	bne	l158
	lda	r5
	sta	r7
	lda	r4
	sta	r6
	lda	r3
	ldy	#1
	sta	(sp),y
	lda	r2
	dey
	sta	(sp),y
	lda	r5
	ldy	#3
	sta	(sp),y
	lda	r4
	dey
	sta	(sp),y
	ldy	#0
	lda	(r0),y
	cmp	r10
	bne	l158
	lda	#4
	clc
	adc	r6
	sta	r4
	lda	#128
	adc	r7
	sta	r5
	lda	#5
	clc
	adc	r6
	sta	r2
	lda	#128
	adc	r7
	sta	r3
	ldy	#0
	lda	(r2),y
	ldx	#0
	sta	r30
	stx	r29
	tax
	tya
	sta	r2
	stx	r3
	ldx	r29
	lda	(r4),y
	ldx	#0
	ora	r2
	sta	_reserve_result_addr
	txa
	ora	r3
	sta	1+_reserve_result_addr
	lda	#6
	clc
	adc	r6
	sta	r2
	lda	#128
	adc	r7
	sta	r3
	lda	(r2),y
	sta	_reserve_result_bank
	lda	r16
	jsr	_bankswitch_prg
	lda	#1
	jmp	l148
l158:
	lda	r4
	clc
	adc	#8
	sta	r4
	bcc	l173
	inc	r4+1
l173:
	lda	r2
	clc
	adc	#8
	sta	r2
	bcc	l174
	inc	r2+1
l174:
	lda	r0
	clc
	adc	#8
	sta	r0
	bcc	l175
	inc	r0+1
l175:
	lda	r5
	cmp	1+_entry_list_offset
	bcc	l164
	bne	l176
	lda	r4
	cmp	_entry_list_offset
	bcc	l164
l176:
l165:
	lda	r16
	jsr	_bankswitch_prg
	lda	#0
l148:
	sta	r31
	pla
	sta	r19
	pla
	sta	r18
	pla
	sta	r16
	clc
	lda	sp
	adc	#4
	sta	sp
	bcc	l177
	inc	sp+1
l177:
	lda	r31
	rts
; stacksize=0+??
;vcprmin=10000
	section	text
	global	_flash_cache_pc_update
_flash_cache_pc_update:
	lda	r16
	pha
	lda	r17
	pha
	lda	r18
	pha
	lda	r19
	pha
	lda	r20
	pha
	lda	r2
	sta	r20
	lda	r0
	ldx	#0
	clc
	adc	_flash_code_address
	pha
	txa
	adc	1+_flash_code_address
	tax
	pla
	clc
	adc	#8
	sta	r18
	txa
	adc	#0
	sta	r19
	lda	_mapper_prg_bank
	sta	r17
	lda	_pc_jump_flag_bank
	jsr	_bankswitch_prg
	lda	#<(_flash_cache_pc_flags)
	clc
	adc	_pc_jump_flag_address
	sta	r2
	lda	#>(_flash_cache_pc_flags)
	adc	1+_pc_jump_flag_address
	sta	r3
	ldy	#0
	lda	(r2),y
	sta	r16
	lda	r17
	jsr	_bankswitch_prg
	lda	r16
	cmp	#255
	bne	l178
	lda	r18
	sta	r31
	lda	#>(_flash_cache_pc)
	sta	r17
	lda	#<(_flash_cache_pc)
	sta	r16
	lda	_pc_jump_address
	clc
	adc	r16
	sta	r0
	lda	1+_pc_jump_address
	adc	r17
	sta	r1
	lda	_pc_jump_bank
	sta	r2
	lda	r31
	sta	r4
	jsr	_flash_byte_program
	lda	r19
	ldx	#0
	sta	r0
	stx	r1
	lda	r0
	sta	r31
	lda	_pc_jump_address
	clc
	adc	r16
	sta	r2
	lda	1+_pc_jump_address
	adc	r17
	sta	r3
	lda	r2
	clc
	adc	#1
	sta	r0
	lda	r3
	adc	#0
	sta	r1
	lda	_pc_jump_bank
	sta	r2
	lda	r31
	sta	r4
	jsr	_flash_byte_program
	lda	r20
	cmp	#128
	bne	l183
	lda	_flash_code_bank
	sta	r3
	jmp	l184
l183:
	lda	#128
	sta	r3
l184:
	ldx	#>(_flash_cache_pc_flags)
	lda	#<(_flash_cache_pc_flags)
	clc
	adc	_pc_jump_flag_address
	sta	r0
	txa
	adc	1+_pc_jump_flag_address
	sta	r1
	lda	_pc_jump_flag_bank
	sta	r2
	lda	r3
	sta	r4
	jsr	_flash_byte_program
l178:
	pla
	sta	r20
	pla
	sta	r19
	pla
	sta	r18
	pla
	sta	r17
	pla
	sta	r16
	rts
; stacksize=0+??
;vcprmin=10000
	section	text
	global	_setup_flash_pc_tables
_setup_flash_pc_tables:
	ldx	r1
	lda	r0
	stx	r31
	asl
	rol	r31
	ldx	r31
	sta	r31
	sta	btmp0
	txa
	sta	btmp0+1
	lda	#0
	sta	btmp0+2
	sta	btmp0+3
	lda	r1
	ldx	#0
	stx	r31
	lsr	r31
	ror
	lsr	r31
	ror
	lsr	r31
	ror
	lsr	r31
	ror
	lsr	r31
	ror
	ldx	r31
	sta	r2
	stx	r3
	lda	r2
	clc
	adc	#19
	sta	_pc_jump_bank
	ldx	btmp0+1
	lda	btmp0
	sta	_pc_jump_address
	txa
	and	#63
	sta	1+_pc_jump_address
	lda	r1
	ldx	#0
	stx	r31
	lsr	r31
	ror
	lsr	r31
	ror
	lsr	r31
	ror
	lsr	r31
	ror
	lsr	r31
	ror
	lsr	r31
	ror
	ldx	r31
	sta	r2
	stx	r3
	lda	r2
	clc
	adc	#27
	sta	_pc_jump_flag_bank
	lda	r0
	sta	_pc_jump_flag_address
	lda	r1
	and	#63
	sta	1+_pc_jump_flag_address
	rts
; stacksize=0+??
;vcprmin=10000
	section	text
	global	_setup_flash_address
_setup_flash_address:
	lda	r1
	sta	r3
	lda	r0
	sta	r2
	lda	r3
	sta	r1
	lda	r2
	sta	r0
	jmp	_setup_flash_pc_tables
; stacksize=0+??
;vcprmin=10000
	section	text
	global	_flash_cache_init_sectors
_flash_cache_init_sectors:
	lda	#0
	sta	r2
l197:
	lda	r2
	ldx	#0
	stx	r31
	asl
	rol	r31
	ldx	r31
	clc
	adc	#<(_sector_free_offset)
	sta	r0
	txa
	adc	#>(_sector_free_offset)
	sta	r1
	lda	#0
	ldy	#1
	sta	(r0),y
	dey
	sta	(r0),y
	inc	r2
	lda	r2
	cmp	#56
	bcc	l197
	lda	#0
	sta	_next_free_sector
	rts
; stacksize=0+??
;vcprmin=10000
	section	text
l199:
	lda	r0
	eor	#32
	rts
; stacksize=0+??
;vcprmin=10000
	section	text
	global	_try_intra_block_branch
_try_intra_block_branch:
	lda	r16
	pha
	lda	r17
	pha
	lda	r18
	pha
	lda	r19
	pha
	lda	r2
	sta	r17
	lda	r1
	sta	r3
	lda	r0
	sta	r2
	lda	_block_has_jsr
	beq	l205
	lda	#0
	jmp	l202
l205:
	lda	l29
	beq	l207
	lda	#0
	jmp	l202
l207:
	lda	#<(_cache_entry_pc_lo)
	clc
	adc	_cache_index
	sta	r4
	lda	#>(_cache_entry_pc_lo)
	adc	#0
	sta	r5
	lda	r4
	sta	r14
	lda	r5
	sta	r15
	ldy	#0
	lda	(r14),y
	sta	r4
	tya
	sta	r5
	lda	#<(_cache_entry_pc_hi)
	clc
	adc	_cache_index
	sta	r6
	lda	#>(_cache_entry_pc_hi)
	adc	#0
	sta	r7
	lda	(r6),y
	tax
	tya
	ora	r4
	sta	r8
	txa
	ora	r5
	sta	r9
	lda	r3
	cmp	r9
	bcc	l208
	bne	l225
	lda	r2
	cmp	r8
	bcc	l208
l225:
	lda	r3
	cmp	1+_pc
	bcc	l209
	bne	l226
	lda	r2
	cmp	_pc
	bcc	l209
l226:
l208:
	lda	#0
	jmp	l202
l209:
	lda	r2
	sta	r31
	lda	r8
	sta	r1
	lda	r31
	sec
	sbc	r1
	and	#63
	sta	r31
	tay
	lda	0+_block_ci_map,y ;am(r31)
	sta	r0
	cmp	#0
	bne	l212
	lda	#0
	jmp	l202
l212:
	lda	r0
	sec
	sbc	#1
	sta	r1
	ldx	#0
	sta	r31
	lda	_code_index
	sta	r4
	txa
	sta	r5
	lda	r31
	inc	r4
	bne	l227
	inc	r5
l227:
	inc	r4
	bne	l228
	inc	r5
l228:
	sec
	sbc	r4
	sta	r18
	txa
	sbc	r5
	sta	r19
	lda	r18
	cmp	#128
	lda	r19
	sbc	#255
	bvc	l229
	eor	#128
l229:
	bmi	l213
	lda	#127
	cmp	r18
	lda	#0
	sbc	r19
	bvc	l230
	eor	#128
l230:
	bpl	l214
l213:
	lda	#0
	jmp	l202
l214:
	lda	_cache_index
	ldx	#0
	sta	r0
	stx	r1
	txa
	sta	r3
	lda	#211
	sta	r2
	jsr	___mulint16
	clc
	adc	#<(_cache_code)
	pha
	txa
	adc	#>(_cache_code)
	tax
	pla
	clc
	adc	_code_index
	sta	r0
	txa
	adc	#0
	sta	r1
	lda	r17
	ldy	#0
	sta	(r0),y
	lda	r18
	sta	r16
	lda	_cache_index
	ldx	#0
	sta	r0
	stx	r1
	txa
	sta	r3
	lda	#211
	sta	r2
	jsr	___mulint16
	clc
	adc	#<(_cache_code)
	pha
	txa
	adc	#>(_cache_code)
	tax
	pla
	sta	r31
	lda	_code_index
	clc
	adc	#1
	sta	r2
	lda	r31
	sta	r0
	stx	r1
	lda	r16
	ldy	r2
	sta	(r0),y ;am(r2)
	lda	1+_pc
	sta	r1
	lda	_pc
	sta	r0
	lda	1+_flash_cache_index
	sta	r3
	lda	_flash_cache_index
	sta	r2
	jsr	_setup_flash_address
	lda	_code_index
	sta	r0
	lda	#128
	sta	r2
	jsr	_flash_cache_pc_update
	inc	_pc
	bne	l231
	inc	1+_pc
l231:
	inc	_pc
	bne	l232
	inc	1+_pc
l232:
	inc	_code_index
	inc	_code_index
	inc	_cache_branches
	bne	l233
	inc	1+_cache_branches
	bne	l233
	inc	2+_cache_branches
	bne	l233
	inc	3+_cache_branches
l233:
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r0
	lda	#>(_cache_flag)
	adc	#0
	sta	r1
	ldy	#0
	lda	(r0),y
	ora	#32
	sta	(r0),y
	lda	#1
l202:
	sta	r31
	pla
	sta	r19
	pla
	sta	r18
	pla
	sta	r17
	pla
	sta	r16
	lda	r31
	rts
; stacksize=0+??
;vcprmin=10000
	section	text
	global	_try_direct_branch
_try_direct_branch:
	lda	r16
	pha
	lda	r17
	pha
	lda	r18
	pha
	lda	r2
	sta	r18
	lda	r1
	sta	r3
	lda	r0
	sta	r2
	lda	r5
	sta	r17
	lda	r4
	sta	r16
	lda	r3
	sta	r1
	lda	r2
	sta	r0
	jsr	_lookup_entry_list
	cmp	#0
	bne	l237
	lda	#0
	jmp	l234
l237:
	lda	_reserve_result_bank
	cmp	_flash_code_bank
	beq	l239
	lda	#0
	jmp	l234
l239:
	lda	_flash_code_address
	clc
	adc	#8
	sta	r2
	lda	1+_flash_code_address
	adc	#0
	sta	r3
	lda	_code_index
	ldx	#0
	clc
	adc	r2
	pha
	txa
	adc	r3
	tax
	pla
	clc
	adc	#2
	bcc	l257
	inx
l257:
	sta	r14
	stx	r15
	lda	_reserve_result_addr
	sec
	sbc	r14
	sta	r0
	lda	1+_reserve_result_addr
	sbc	r15
	sta	r1
	lda	r0
	cmp	#128
	lda	r1
	sbc	#255
	bvc	l258
	eor	#128
l258:
	bmi	l241
	lda	#127
	cmp	r0
	lda	#0
	sbc	r1
	bvc	l259
	eor	#128
l259:
	bmi	l241
	lda	_code_index
	ldx	#0
	clc
	adc	#29
	bcc	l260
	inx
l260:
	pha
	cmp	#211
	txa
	sbc	#0
	bvc	l262
	eor	#128
l262:
	bpl	l261
	pla
	lda	r16
	clc
	adc	_code_index
	sta	r2
	lda	r17
	adc	#0
	sta	r3
	lda	r18
	ldy	#0
	sta	(r2),y
	lda	r0
	sta	r31
	lda	_code_index
	sta	r6
	tya
	sta	r7
	lda	r16
	clc
	adc	#1
	sta	r2
	lda	r17
	adc	#0
	sta	r3
	lda	r2
	clc
	adc	r6
	sta	r2
	lda	r3
	adc	r7
	sta	r3
	lda	r31
	sta	(r2),y
	lda	#2
	jmp	l234
l241:
	lda	_code_index
	ldx	#0
	clc
	adc	#32
	bcc	l263
	inx
l263:
	pha
	cmp	#211
	txa
	sbc	#0
	bvc	l265
	eor	#128
l265:
	bpl	l264
	pla
	lda	r18
	eor	#32
	sta	r31
	lda	r16
	clc
	adc	_code_index
	sta	r2
	lda	r17
	adc	#0
	sta	r3
	lda	r31
	ldy	#0
	sta	(r2),y
	lda	_code_index
	sta	r2
	tya
	sta	r3
	ldx	r17
	lda	r16
	clc
	adc	#1
	bcc	l266
	inx
l266:
	clc
	adc	r2
	sta	r2
	txa
	adc	r3
	sta	r3
	lda	#3
	sta	(r2),y
	lda	_code_index
	sta	r2
	lda	#0
	sta	r3
	ldx	r17
	lda	r16
	clc
	adc	#2
	bcc	l267
	inx
l267:
	clc
	adc	r2
	sta	r2
	txa
	adc	r3
	sta	r3
	lda	#76
	sta	(r2),y
	lda	_reserve_result_addr
	and	#255
	sta	r31
	lda	_code_index
	sta	r6
	lda	#0
	sta	r7
	lda	r16
	clc
	adc	#3
	sta	r2
	lda	r17
	adc	#0
	sta	r3
	lda	r2
	clc
	adc	r6
	sta	r2
	lda	r3
	adc	r7
	sta	r3
	lda	r31
	sta	(r2),y
	lda	1+_reserve_result_addr
	ldx	#0
	sta	r2
	stx	r3
	lda	r2
	and	#255
	sta	r31
	lda	_code_index
	sta	r6
	txa
	sta	r7
	lda	r16
	clc
	adc	#4
	sta	r2
	lda	r17
	adc	#0
	sta	r3
	lda	r2
	clc
	adc	r6
	sta	r2
	lda	r3
	adc	r7
	sta	r3
	lda	r31
	sta	(r2),y
	lda	#5
	jmp	l234
l245:
	lda	#0
l234:
	sta	r31
	pla
	sta	r18
	pla
	sta	r17
	pla
	sta	r16
	lda	r31
	rts
l264:
	pla
	jmp	l245
l261:
	pla
	jmp	l241
; stacksize=0+??
;vcprmin=10000
	section	text
	global	_find_zp_mirror
_find_zp_mirror:
	lda	r0
	sta	r5
	ldx	#0
	txa
	sta	r4
	lda	#>(_mirrored_ptrs)
	sta	r3
	lda	#<(_mirrored_ptrs)
	sta	r2
l281:
	ldy	#0
	lda	(r2),y
	cmp	r5
	beq	l274
	ldy	r4
	lda	1+_mirrored_ptrs,y ;am(r4)
	cmp	r5
	bne	l275
l274:
	ldx	r3
	lda	r2
	rts
l275:
	inx
	lda	r4
	clc
	adc	#4
	sta	r4
	lda	r2
	clc
	adc	#4
	sta	r2
	bcc	l284
	inc	r2+1
l284:
	cpx	#3
	bcc	l281
	ldx	#0
	txa
l268:
	rts
; stacksize=0+??
;vcprmin=10000
	section	text
	global	_find_zp_mirror_lo
_find_zp_mirror_lo:
	lda	r0
	sta	r3
	ldx	#0
	txa
	sta	r2
	lda	#>(_mirrored_ptrs)
	sta	r5
	lda	#<(_mirrored_ptrs)
	sta	r4
l297:
	ldy	r2
	lda	0+_mirrored_ptrs,y ;am(r2)
	cmp	r3
	bne	l292
	ldx	r5
	lda	r4
	rts
l292:
	inx
	lda	r2
	clc
	adc	#4
	sta	r2
	lda	r4
	clc
	adc	#4
	sta	r4
	bcc	l300
	inc	r4+1
l300:
	cpx	#3
	bcc	l297
	ldx	#0
	txa
l285:
	rts
; stacksize=0+??
;vcprmin=10000
	section	text
	global	_emit_zp_mirror_write
_emit_zp_mirror_write:
	lda	r2
	sta	r7
	lda	r4
	sta	r10
	lda	r1
	sta	r9
	lda	r0
	sta	r8
	lda	r6
	sta	r0
	jsr	_find_zp_mirror
	sta	r4
	stx	r5
	txa
	bne	l304
	lda	r4
	bne	l304
	lda	#0
	rts
l304:
	lda	r6
	ldy	#0
	cmp	(r4),y
	bne	l306
	ldy	#2
	lda	(r4),y ;am(2)
	sta	r0
	lda	#0
	sta	r1
	jmp	l307
l306:
	ldy	#2
	lda	(r4),y ;am(2)
	ldx	#0
	clc
	adc	#1
	sta	r0
	txa
	adc	#0
	sta	r1
l307:
	lda	r0
	sta	r31
	lda	r10
	ldy	r7
	sta	(r8),y ;am(r7)
	lda	r7
	sta	r4
	lda	#0
	sta	r5
	lda	r8
	clc
	adc	#1
	sta	r2
	lda	r9
	adc	#0
	sta	r3
	lda	r2
	clc
	adc	r4
	sta	r2
	lda	r3
	adc	r5
	sta	r3
	lda	r31
	ldy	#0
	sta	(r2),y
	lda	#2
l301:
	rts
; stacksize=0+??
;vcprmin=10000
	section	text
	global	_emit_native_sta_indy
_emit_native_sta_indy:
	lda	sp
	bne	l332
	dec	sp+1
l332:
	dec	sp
	lda	r16
	pha
	lda	r17
	pha
	lda	r2
	sta	r9
	ldx	r4
	lda	r1
	sta	r13
	lda	r0
	sta	r12
	stx	r0
	jsr	_find_zp_mirror_lo
	sta	r10
	stx	r11
	txa
	bne	l311
	lda	r10
	bne	l311
	lda	#0
	jmp	l308
l311:
	ldy	#0
	lda	(r10),y
	ldx	#0
	sta	r31
	lda	#>(_RAM_BASE)
	sta	r3
	lda	#<(_RAM_BASE)
	sta	r2
	lda	r31
	clc
	adc	r2
	sta	_native_sta_indy_emu_lo
	txa
	adc	r3
	sta	1+_native_sta_indy_emu_lo
	iny
	lda	(r10),y ;am(1)
	clc
	adc	r2
	sta	_native_sta_indy_emu_hi
	txa
	adc	r3
	sta	1+_native_sta_indy_emu_hi
	lda	_native_sta_indy_tmpl_size
	sta	r7
	txa
	sta	r8
	lda	_native_sta_indy_tmpl_size
	beq	l328
	lda	r12
	clc
	adc	r9
	sta	r0
	lda	r13
	adc	#0
	sta	r1
	lda	r8
	ldy	#0
	sta	(sp),y
	tax
l326:
	lda	0+_native_sta_indy_tmpl,x ;am(x)
	ldy	#0
	sta	(r0),y
	inx
	inc	r0
	bne	l333
	inc	r1
l333:
	cpx	r7
	bcc	l326
l328:
	lda	r10
	clc
	adc	#3
	sta	r4
	lda	r11
	adc	#0
	sta	r5
	ldy	#0
	lda	(r4),y
	beq	l317
	lda	r7
	sta	r1
	inc	r7
	lda	r9
	sta	r14
	lda	#0
	sta	r15
	lda	r12
	clc
	adc	r1
	pha
	lda	r13
	adc	#0
	tax
	pla
	clc
	adc	r14
	sta	r2
	txa
	adc	r15
	sta	r3
	lda	#8
	ldy	#0
	sta	(r2),y
	lda	r12
	clc
	adc	r7
	pha
	lda	r13
	adc	#0
	tax
	pla
	inc	r7
	clc
	adc	r14
	sta	r2
	txa
	adc	r15
	sta	r3
	lda	#230
	sta	(r2),y
	lda	(r4),y
	cmp	#1
	bne	l319
	lda	r6
	sta	r16
	lda	#0
	sta	r17
	jmp	l320
l319:
	ldy	#1
	lda	(sp),y
	sta	r16
	lda	#0
	sta	r17
l320:
	lda	r12
	clc
	adc	r7
	pha
	lda	r13
	adc	#0
	tax
	pla
	inc	r7
	clc
	adc	r14
	sta	r2
	txa
	adc	r15
	sta	r3
	lda	r16
	ldy	#0
	sta	(r2),y
	lda	r12
	clc
	adc	r7
	pha
	lda	r13
	adc	#0
	tax
	pla
	inc	r7
	clc
	adc	r14
	sta	r2
	txa
	adc	r15
	sta	r3
	lda	#40
	sta	(r2),y
l317:
	lda	r7
l308:
	sta	r31
	pla
	sta	r17
	pla
	sta	r16
	inc	sp
	bne	l334
	inc	sp+1
l334:
	lda	r31
	rts
; stacksize=0+??
;vcprmin=10000
	section	text
	global	_init_zp_mirror_table
_init_zp_mirror_table:
	sec
	lda	sp
	sbc	#15
	sta	sp
	bcs	l354
	dec	sp+1
l354:
	lda	l335
	bne	l336
	lda	sp
	clc
	adc	#3
	sta	r14
	lda	sp+1
	adc	#0
	sta	r15
	ldy	#11
l355:
	lda	l340,y
	sta	(r14),y
	dey
	lda	l340,y
	sta	(r14),y
	dey
	lda	l340,y
	sta	(r14),y
	dey
	lda	l340,y
	sta	(r14),y
	dey
	bpl	l355
	ldx	#>(_zp_mirror_0)
	lda	#<(_zp_mirror_0)
	ldy	#0
	sta	(sp),y
	ldx	#>(_zp_mirror_1)
	lda	#<(_zp_mirror_1)
	iny
	sta	(sp),y
	ldx	#>(_zp_mirror_2)
	lda	#<(_zp_mirror_2)
	iny
	sta	(sp),y
	lda	#0
	sta	r5
	lda	sp
	clc
	adc	#3
	sta	r6
	lda	sp+1
	adc	#0
	sta	r7
	lda	sp
	sta	r8
	lda	sp+1
	sta	r9
	lda	#0
	sta	r4
l349:
	lda	r6
	clc
	adc	r4
	sta	r0
	lda	r7
	adc	#0
	sta	r1
	ldy	#3
	lda	(r0),y
	ldy	r4
	sta	3+_mirrored_ptrs,y ;am(r4)
	ldy	#2
	lda	(r0),y
	ldy	r4
	sta	2+_mirrored_ptrs,y ;am(r4)
	ldy	#1
	lda	(r0),y
	ldy	r4
	sta	1+_mirrored_ptrs,y ;am(r4)
	ldy	#0
	lda	(r0),y
	ldy	r4
	sta	0+_mirrored_ptrs,y ;am(r4)
	ldy	r5
	lda	(r8),y ;am(r5)
	ldy	r4
	sta	2+_mirrored_ptrs,y ;am(r4)
	inc	r5
	lda	r4
	clc
	adc	#4
	sta	r4
	lda	r5
	cmp	#3
	bcc	l349
	lda	#1
	sta	l335
l336:
	clc
	lda	sp
	adc	#15
	sta	sp
	bcc	l356
	inc	sp+1
l356:
	rts
; stacksize=0+??
	section	rodata
l340:
	byte	0
	byte	1
	byte	0
	byte	1
	byte	37
	byte	38
	byte	0
	byte	1
	byte	105
	byte	106
	byte	0
	byte	1
;vcprmin=10000
	section	text
	global	_emit_dirty_flag
_emit_dirty_flag:
	lda	r2
	sta	r5
	lda	r4
	sec
	sbc	#140
	cmp	#3
	bcc	l362
	lda	r4
	cmp	#157
	beq	l362
	lda	r4
	cmp	#153
	bne	l360
l362:
	lda	r6
	cmp	#64
	bcc	l360
	lda	r6
	cmp	#80
	bcs	l360
	lda	#8
	ldy	r5
	sta	(r0),y ;am(r5)
	lda	r5
	sta	r8
	lda	#0
	sta	r9
	ldx	r1
	lda	r0
	clc
	adc	#1
	bcc	l373
	inx
l373:
	clc
	adc	r8
	sta	r2
	txa
	adc	r9
	sta	r3
	lda	#230
	ldy	#0
	sta	(r2),y
	lda	r6
	cmp	#72
	bcs	l368
	ldy	#0
	lda	(sp),y
	sta	r10
	tya
	sta	r11
	jmp	l369
l368:
	ldy	#1
	lda	(sp),y
	sta	r10
	lda	#0
	sta	r11
l369:
	ldx	r1
	lda	r0
	clc
	adc	#2
	bcc	l374
	inx
l374:
	clc
	adc	r8
	sta	r2
	txa
	adc	r9
	sta	r3
	lda	r10
	ldy	#0
	sta	(r2),y
	ldx	r1
	lda	r0
	clc
	adc	#3
	bcc	l375
	inx
l375:
	clc
	adc	r8
	sta	r2
	txa
	adc	r9
	sta	r3
	lda	#40
	sta	(r2),y
	lda	#4
	rts
l360:
	lda	#0
l357:
	rts
; stacksize=0+??
;vcprmin=10000
	section	"bank2"
	global	_emit_template
_emit_template:
	lda	r16
	pha
	lda	r17
	pha
	lda	r18
	pha
	lda	r19
	pha
	lda	r1
	sta	r17
	lda	r0
	sta	r16
	lda	#0
	sta	r19
	tay
	lda	(r16),y
	cmp	#8
	bne	l379
	lda	r2
	sta	r4
	lda	#0
	sta	r5
	ldx	r17
	lda	r16
	sec
	sbc	#1
	bcs	l401
	dex
l401:
	clc
	adc	r4
	sta	r4
	txa
	adc	r5
	sta	r5
	ldy	#0
	lda	(r4),y
	cmp	#40
	bne	l379
	lda	r2
	cmp	_opcode_6502_pha_size
	bne	l379
	lda	#1
	sta	r19
l379:
	lda	r2
	sec
	sbc	r19
	sta	r18
	lda	_code_index
	sta	r4
	lda	#0
	sta	r5
	lda	r2
	ldx	#0
	clc
	adc	r4
	pha
	txa
	adc	r5
	tax
	pla
	clc
	adc	#3
	bcc	l402
	inx
l402:
	pha
	cmp	#211
	txa
	sbc	#0
	bvc	l404
	eor	#128
l404:
	bpl	l403
	pla
	lda	_cache_index
	ldx	#0
	sta	r0
	stx	r1
	txa
	sta	r3
	lda	#211
	sta	r2
	jsr	___mulint16
	clc
	adc	#<(_cache_code)
	pha
	txa
	adc	#>(_cache_code)
	tax
	pla
	clc
	adc	_code_index
	sta	r10
	txa
	adc	#0
	sta	r11
	lda	#0
	sta	r8
	lda	r18
	beq	l395
l394:
	ldy	r8
	lda	(r16),y ;am(r8)
	ldy	r8
	sta	(r10),y ;am(r8)
	inc	r8
	lda	r8
	cmp	r18
	bcc	l394
l395:
	inc	_pc
	bne	l405
	inc	1+_pc
l405:
	lda	_code_index
	clc
	adc	r18
	sta	_code_index
; volatile barrier
	lda	r19
	sta	l28
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r4
	lda	#>(_cache_flag)
	adc	#0
	sta	r5
	ldy	#0
	lda	(r4),y
	ora	#32
	sta	(r4),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r4
	lda	#>(_cache_flag)
	adc	#0
	sta	r5
	lda	(r4),y
	jmp	l376
l383:
; volatile barrier
	lda	l28
	beq	l389
	lda	_cache_index
	ldx	#0
	sta	r0
	stx	r1
	txa
	sta	r3
	lda	#211
	sta	r2
	jsr	___mulint16
	clc
	adc	#<(_cache_code)
	pha
	txa
	adc	#>(_cache_code)
	tax
	pla
	clc
	adc	_code_index
	sta	r0
	txa
	adc	#0
	sta	r1
	inc	_code_index
	lda	#40
	ldy	#0
	sta	(r0),y
; volatile barrier
	lda	#0
	sta	l28
l389:
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r4
	lda	#>(_cache_flag)
	adc	#0
	sta	r5
	ldy	#0
	lda	(r4),y
	ora	#6
	sta	(r4),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r4
	lda	#>(_cache_flag)
	adc	#0
	sta	r5
	lda	(r4),y
	and	#223
	sta	(r4),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r4
	lda	#>(_cache_flag)
	adc	#0
	sta	r5
	lda	(r4),y
l376:
	sta	r31
	pla
	sta	r19
	pla
	sta	r18
	pla
	sta	r17
	pla
	sta	r16
	lda	r31
	rts
l403:
	pla
	jmp	l383
; stacksize=0+??
;vcprmin=10000
	section	"bank2"
l406:
	sec
	lda	sp
	sbc	#19
	sta	sp
	bcs	l732
	dec	sp+1
l732:
	ldy	#18
	jsr	___rsave12
	jsr	_init_zp_mirror_table
	lda	_cache_index
	ldx	#0
	sta	r0
	stx	r1
	txa
	sta	r3
	lda	#211
	sta	r2
	jsr	___mulint16
	clc
	adc	#<(_cache_code)
	sta	r18
	txa
	adc	#>(_cache_code)
	sta	r19
	lda	1+_pc
	sta	r1
	lda	_pc
	sta	r0
	jsr	_read6502
	sta	r17
	cmp	#0
	beq	l501
	lda	r17
	cmp	#8
	beq	l493
	lda	r17
	cmp	#16
	beq	l410
	lda	r17
	cmp	#32
	beq	l458
	lda	r17
	cmp	#40
	beq	l494
	lda	r17
	cmp	#48
	beq	l410
	lda	r17
	cmp	#64
	beq	l474
	lda	r17
	cmp	#72
	beq	l490
	lda	r17
	cmp	#76
	beq	l433
	lda	r17
	cmp	#80
	beq	l410
	lda	r17
	cmp	#88
	beq	l495
	lda	r17
	cmp	#96
	beq	l476
	lda	r17
	cmp	#104
	beq	l492
	lda	r17
	cmp	#108
	beq	l474
	lda	r17
	cmp	#112
	beq	l410
	lda	r17
	cmp	#120
	beq	l495
	lda	r17
	cmp	#144
	beq	l410
	lda	r17
	cmp	#148
	beq	l497
	lda	r17
	cmp	#150
	beq	l497
	lda	r17
	cmp	#154
	beq	l486
	lda	r17
	cmp	#176
	beq	l410
	lda	r17
	cmp	#186
	beq	l483
	lda	r17
	cmp	#208
	beq	l410
	lda	r17
	cmp	#234
	beq	l500
	lda	r17
	cmp	#240
	beq	l410
	lda	r17
	cmp	#248
	beq	l499
	jmp	l502
l410:
	lda	_pc
	clc
	adc	#1
	sta	r0
	lda	1+_pc
	adc	#0
	sta	r1
	jsr	_read6502
	ldy	#2
	sta	(sp),y
	sec
	sbc	#0
	bvc	l733
	eor	#128
l733:
	bmi	l419
	lda	_pc
	clc
	adc	#2
	sta	r8
	lda	1+_pc
	adc	#0
	sta	r9
	ldx	#0
	ldy	#2
	lda	(sp),y
	bpl	l734
	dex
l734:
	clc
	adc	r8
	sta	r10
	txa
	adc	r9
	sta	r11
	inc	_branch_forward
	bne	l735
	inc	1+_branch_forward
	bne	l735
	inc	2+_branch_forward
	bne	l735
	inc	3+_branch_forward
l735:
	lda	_sa_compile_pass
	cmp	#2
	bne	l423
	lda	r11
	sta	r1
	lda	r10
	sta	r0
	lda	r17
	sta	r2
	lda	r19
	sta	r5
	lda	r18
	sta	r4
	lda	r11
	ldy	#3
	sta	(sp),y
	lda	r10
	dey
	sta	(sp),y
	jsr	_try_direct_branch
	ldy	#4
	sta	(sp),y
	dey
	lda	(sp),y
	sta	r11
	dey
	lda	(sp),y
	sta	r10
	ldy	#4
	lda	(sp),y
	beq	l423
	inc	_pc
	bne	l736
	inc	1+_pc
l736:
	inc	_pc
	bne	l737
	inc	1+_pc
l737:
	lda	_code_index
	clc
	ldy	#4
	adc	(sp),y
	sta	_code_index
	inc	_cache_branches
	bne	l738
	inc	1+_cache_branches
	bne	l738
	inc	2+_cache_branches
	bne	l738
	inc	3+_cache_branches
l738:
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	ldy	#0
	lda	(r8),y
	ora	#32
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	jmp	l407
l423:
	lda	1+_flash_code_address
	ldy	#3
	sta	(sp),y
	lda	_flash_code_address
	dey
	sta	(sp),y
	lda	_flash_code_bank
	ldy	#4
	sta	(sp),y
	lda	_code_index
	ldy	#6
	sta	(sp),y
	lda	_code_index
	ldx	#0
	clc
	adc	#35
	bcc	l739
	inx
l739:
	pha
	cmp	#211
	txa
	sbc	#0
	bvc	l741
	eor	#128
l741:
	bmi	l740
	pla
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	ldy	#0
	lda	(r8),y
	ora	#2
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	and	#223
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	jmp	l407
l425:
	lda	r17
	sta	r0
	jsr	l199
	sta	r31
	lda	r18
	clc
	adc	_code_index
	sta	r0
	lda	r19
	adc	#0
	sta	r1
	lda	r31
	ldy	#0
	sta	(r0),y
	lda	_code_index
	sta	r0
	tya
	sta	r1
	ldx	r19
	lda	r18
	clc
	adc	#1
	bcc	l742
	inx
l742:
	clc
	adc	r0
	sta	r0
	txa
	adc	r1
	sta	r1
	lda	#19
	sta	(r0),y
	lda	_code_index
	sta	r0
	lda	#0
	sta	r1
	ldx	r19
	lda	r18
	clc
	adc	#2
	bcc	l743
	inx
l743:
	clc
	adc	r0
	sta	r0
	txa
	adc	r1
	sta	r1
	lda	r17
	sta	(r0),y
	lda	_code_index
	sta	r0
	lda	#0
	sta	r1
	ldx	r19
	lda	r18
	clc
	adc	#3
	bcc	l744
	inx
l744:
	clc
	adc	r0
	sta	r0
	txa
	adc	r1
	sta	r1
	lda	#3
	sta	(r0),y
	lda	_code_index
	sta	r0
	lda	#0
	sta	r1
	ldx	r19
	lda	r18
	clc
	adc	#4
	bcc	l745
	inx
l745:
	clc
	adc	r0
	sta	r0
	txa
	adc	r1
	sta	r1
	lda	#76
	sta	(r0),y
	lda	_code_index
	sta	r0
	lda	#0
	sta	r1
	ldx	r19
	lda	r18
	clc
	adc	#5
	bcc	l746
	inx
l746:
	clc
	adc	r0
	sta	r0
	txa
	adc	r1
	sta	r1
	lda	#255
	sta	(r0),y
	lda	_code_index
	sta	r0
	lda	#0
	sta	r1
	ldx	r19
	lda	r18
	clc
	adc	#6
	bcc	l747
	inx
l747:
	clc
	adc	r0
	sta	r0
	txa
	adc	r1
	sta	r1
	lda	#255
	sta	(r0),y
	lda	_code_index
	sta	r0
	lda	#0
	sta	r1
	ldx	r19
	lda	r18
	clc
	adc	#7
	bcc	l748
	inx
l748:
	clc
	adc	r0
	sta	r0
	txa
	adc	r1
	sta	r1
	lda	#133
	sta	(r0),y
	lda	#>(_a)
	sta	r1
	lda	#<(_a)
	sta	r0
	sta	r31
	lda	_code_index
	sta	r2
	lda	#0
	sta	r3
	lda	r18
	clc
	adc	#8
	sta	r0
	lda	r19
	adc	#0
	sta	r1
	lda	r0
	clc
	adc	r2
	sta	r0
	lda	r1
	adc	r3
	sta	r1
	lda	r31
	sta	(r0),y
	lda	_code_index
	sta	r0
	lda	#0
	sta	r1
	ldx	r19
	lda	r18
	clc
	adc	#9
	bcc	l749
	inx
l749:
	clc
	adc	r0
	sta	r0
	txa
	adc	r1
	sta	r1
	lda	#8
	sta	(r0),y
	lda	_code_index
	sta	r0
	lda	#0
	sta	r1
	ldx	r19
	lda	r18
	clc
	adc	#10
	bcc	l750
	inx
l750:
	clc
	adc	r0
	sta	r0
	txa
	adc	r1
	sta	r1
	lda	#169
	sta	(r0),y
	lda	r10
	and	#255
	sta	r31
	lda	_code_index
	sta	r2
	lda	#0
	sta	r3
	lda	r18
	clc
	adc	#11
	sta	r0
	lda	r19
	adc	#0
	sta	r1
	lda	r0
	clc
	adc	r2
	sta	r0
	lda	r1
	adc	r3
	sta	r1
	lda	r31
	sta	(r0),y
	lda	_code_index
	sta	r0
	lda	#0
	sta	r1
	ldx	r19
	lda	r18
	clc
	adc	#12
	bcc	l751
	inx
l751:
	clc
	adc	r0
	sta	r0
	txa
	adc	r1
	sta	r1
	lda	#133
	sta	(r0),y
	lda	#>(_pc)
	sta	r1
	lda	#<(_pc)
	sta	r0
	sta	r31
	lda	_code_index
	sta	r2
	lda	#0
	sta	r3
	lda	r18
	clc
	adc	#13
	sta	r0
	lda	r19
	adc	#0
	sta	r1
	lda	r0
	clc
	adc	r2
	sta	r0
	lda	r1
	adc	r3
	sta	r1
	lda	r31
	sta	(r0),y
	sta	r31
	lda	_code_index
	sta	r2
	lda	#0
	sta	r3
	lda	r18
	clc
	adc	#14
	sta	r0
	lda	r19
	adc	#0
	sta	r1
	lda	r0
	clc
	adc	r2
	sta	r0
	lda	r1
	adc	r3
	sta	r1
	lda	#169
	sta	(r0),y
	lda	r31
	sta	r30
	lda	r11
	ldx	#0
	sta	r2
	stx	r3
	lda	r30
	sta	r31
	lda	r2
	sta	r0
	and	#255
	sta	r0
	lda	_code_index
	sta	r6
	txa
	sta	r7
	lda	r18
	clc
	adc	#15
	sta	r2
	lda	r19
	adc	#0
	sta	r3
	lda	r2
	clc
	adc	r6
	sta	r2
	lda	r3
	adc	r7
	sta	r3
	lda	r0
	sta	(r2),y
	lda	_code_index
	sta	r2
	txa
	sta	r3
	lda	r18
	clc
	adc	#16
	sta	r0
	lda	r19
	adc	#0
	sta	r1
	lda	r0
	clc
	adc	r2
	sta	r0
	lda	r1
	adc	r3
	sta	r1
	lda	#133
	sta	(r0),y
	lda	r31
	clc
	adc	#1
	sta	r31
	lda	_code_index
	sta	r2
	txa
	sta	r3
	lda	r18
	clc
	adc	#17
	sta	r0
	lda	r19
	adc	#0
	sta	r1
	lda	r0
	clc
	adc	r2
	sta	r0
	lda	r1
	adc	r3
	sta	r1
	lda	r31
	sta	(r0),y
	lda	_code_index
	sta	r0
	txa
	sta	r1
	ldx	r19
	lda	r18
	clc
	adc	#18
	bcc	l752
	inx
l752:
	clc
	adc	r0
	sta	r0
	txa
	adc	r1
	sta	r1
	lda	#76
	sta	(r0),y
	ldx	#>(_cross_bank_dispatch)
	lda	#<(_cross_bank_dispatch)
	sta	r0
	sta	r31
	lda	_code_index
	sta	r6
	lda	#0
	sta	r7
	lda	r18
	clc
	adc	#19
	sta	r2
	lda	r19
	adc	#0
	sta	r3
	lda	r2
	clc
	adc	r6
	sta	r2
	lda	r3
	adc	r7
	sta	r3
	lda	r0
	sta	(r2),y
	lda	r31
	sta	r30
	stx	r29
	txa
	ldx	#0
	sta	r0
	stx	r1
	ldx	r29
	lda	r0
	sta	r31
	lda	_code_index
	sta	r2
	lda	#0
	sta	r3
	lda	r18
	clc
	adc	#20
	sta	r0
	lda	r19
	adc	#0
	sta	r1
	lda	r0
	clc
	adc	r2
	sta	r0
	lda	r1
	adc	r3
	sta	r1
	lda	r31
	sta	(r0),y
	ldy	#2
	lda	(sp),y
	clc
	adc	#8
	sta	r0
	iny
	lda	(sp),y
	adc	#0
	sta	r1
	ldy	#6
	lda	(sp),y
	ldx	#0
	clc
	adc	r0
	pha
	txa
	adc	r1
	tax
	pla
	sta	r31
	clc
	adc	#3
	sta	r0
	txa
	adc	#0
	sta	r1
	lda	r31
	clc
	adc	#5
	bcc	l753
	inx
l753:
	sta	r31
	lda	#0
	tay
	sta	(sp),y
	lda	r31
	sta	r2
	stx	r3
	ldy	#4
	lda	(sp),y
	sta	r4
	lda	r11
	sta	r7
	lda	r10
	sta	r6
	jsr	_opt2_record_pending_branch_safe
	lda	1+_pc
	sta	r1
	lda	_pc
	sta	r0
	lda	1+_flash_cache_index
	sta	r3
	lda	_flash_cache_index
	sta	r2
	jsr	_setup_flash_address
	ldy	#6
	lda	(sp),y
	sta	r0
	lda	#128
	sta	r2
	jsr	_flash_cache_pc_update
	inc	_pc
	bne	l754
	inc	1+_pc
l754:
	inc	_pc
	bne	l755
	inc	1+_pc
l755:
	lda	_code_index
	clc
	adc	#21
	sta	_code_index
	inc	_cache_branches
	bne	l756
	inc	1+_cache_branches
	bne	l756
	inc	2+_cache_branches
	bne	l756
	inc	3+_cache_branches
l756:
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r0
	lda	#>(_cache_flag)
	adc	#0
	sta	r1
	ldy	#0
	lda	(r0),y
	ora	#32
	sta	(r0),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r0
	lda	#>(_cache_flag)
	adc	#0
	sta	r1
	lda	(r0),y
	jmp	l407
l419:
	lda	_pc
	clc
	adc	#2
	sta	r2
	lda	1+_pc
	adc	#0
	sta	r3
	ldx	#0
	ldy	#2
	lda	(sp),y
	bpl	l757
	dex
l757:
	clc
	adc	r2
	sta	r24
	txa
	adc	r3
	sta	r25
	ldx	#0
	stx	r31
	lsr	r31
	ror
	lsr	r31
	ror
	lsr	r31
	ror
	lsr	r31
	ror
	lsr	r31
	ror
	lsr	r31
	ror
	ldx	r31
	sta	r2
	stx	r3
	lda	r2
	clc
	adc	#27
	sta	r0
	lda	r24
	pha
	lda	r25
	and	#63
	tax
	pla
	clc
	adc	#<(_flash_cache_pc_flags)
	pha
	txa
	adc	#>(_flash_cache_pc_flags)
	tax
	pla
	sta	r2
	stx	r3
	jsr	_peek_bank_byte
	ldy	#4
	sta	(sp),y
	and	#128
	beq	l429
	inc	_branch_not_compiled
	bne	l758
	inc	1+_branch_not_compiled
	bne	l758
	inc	2+_branch_not_compiled
	bne	l758
	inc	3+_branch_not_compiled
l758:
	lda	1+_flash_code_address
	ldy	#3
	sta	(sp),y
	lda	_flash_code_address
	dey
	sta	(sp),y
	lda	_flash_code_bank
	ldy	#4
	sta	(sp),y
	lda	_code_index
	ldy	#6
	sta	(sp),y
	lda	_code_index
	ldx	#0
	clc
	adc	#35
	bcc	l759
	inx
l759:
	pha
	cmp	#211
	txa
	sbc	#0
	bvc	l761
	eor	#128
l761:
	bmi	l760
	pla
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	ldy	#0
	lda	(r8),y
	ora	#2
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	and	#223
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	jmp	l407
l431:
	lda	r17
	sta	r0
	jsr	l199
	sta	r31
	lda	r18
	clc
	adc	_code_index
	sta	r0
	lda	r19
	adc	#0
	sta	r1
	lda	r31
	ldy	#0
	sta	(r0),y
	lda	_code_index
	sta	r0
	tya
	sta	r1
	ldx	r19
	lda	r18
	clc
	adc	#1
	bcc	l762
	inx
l762:
	clc
	adc	r0
	sta	r0
	txa
	adc	r1
	sta	r1
	lda	#19
	sta	(r0),y
	lda	_code_index
	sta	r0
	lda	#0
	sta	r1
	ldx	r19
	lda	r18
	clc
	adc	#2
	bcc	l763
	inx
l763:
	clc
	adc	r0
	sta	r0
	txa
	adc	r1
	sta	r1
	lda	r17
	sta	(r0),y
	lda	_code_index
	sta	r0
	lda	#0
	sta	r1
	ldx	r19
	lda	r18
	clc
	adc	#3
	bcc	l764
	inx
l764:
	clc
	adc	r0
	sta	r0
	txa
	adc	r1
	sta	r1
	lda	#3
	sta	(r0),y
	lda	_code_index
	sta	r0
	lda	#0
	sta	r1
	ldx	r19
	lda	r18
	clc
	adc	#4
	bcc	l765
	inx
l765:
	clc
	adc	r0
	sta	r0
	txa
	adc	r1
	sta	r1
	lda	#76
	sta	(r0),y
	lda	_code_index
	sta	r0
	lda	#0
	sta	r1
	ldx	r19
	lda	r18
	clc
	adc	#5
	bcc	l766
	inx
l766:
	clc
	adc	r0
	sta	r0
	txa
	adc	r1
	sta	r1
	lda	#255
	sta	(r0),y
	lda	_code_index
	sta	r0
	lda	#0
	sta	r1
	ldx	r19
	lda	r18
	clc
	adc	#6
	bcc	l767
	inx
l767:
	clc
	adc	r0
	sta	r0
	txa
	adc	r1
	sta	r1
	lda	#255
	sta	(r0),y
	lda	_code_index
	sta	r0
	lda	#0
	sta	r1
	ldx	r19
	lda	r18
	clc
	adc	#7
	bcc	l768
	inx
l768:
	clc
	adc	r0
	sta	r0
	txa
	adc	r1
	sta	r1
	lda	#133
	sta	(r0),y
	lda	#>(_a)
	sta	r1
	lda	#<(_a)
	sta	r0
	sta	r31
	lda	_code_index
	sta	r2
	lda	#0
	sta	r3
	lda	r18
	clc
	adc	#8
	sta	r0
	lda	r19
	adc	#0
	sta	r1
	lda	r0
	clc
	adc	r2
	sta	r0
	lda	r1
	adc	r3
	sta	r1
	lda	r31
	sta	(r0),y
	lda	_code_index
	sta	r0
	lda	#0
	sta	r1
	ldx	r19
	lda	r18
	clc
	adc	#9
	bcc	l769
	inx
l769:
	clc
	adc	r0
	sta	r0
	txa
	adc	r1
	sta	r1
	lda	#8
	sta	(r0),y
	lda	_code_index
	sta	r0
	lda	#0
	sta	r1
	ldx	r19
	lda	r18
	clc
	adc	#10
	bcc	l770
	inx
l770:
	clc
	adc	r0
	sta	r0
	txa
	adc	r1
	sta	r1
	lda	#169
	sta	(r0),y
	lda	r24
	and	#255
	sta	r31
	lda	_code_index
	sta	r2
	lda	#0
	sta	r3
	lda	r18
	clc
	adc	#11
	sta	r0
	lda	r19
	adc	#0
	sta	r1
	lda	r0
	clc
	adc	r2
	sta	r0
	lda	r1
	adc	r3
	sta	r1
	lda	r31
	sta	(r0),y
	lda	_code_index
	sta	r0
	lda	#0
	sta	r1
	ldx	r19
	lda	r18
	clc
	adc	#12
	bcc	l771
	inx
l771:
	clc
	adc	r0
	sta	r0
	txa
	adc	r1
	sta	r1
	lda	#133
	sta	(r0),y
	lda	#>(_pc)
	sta	r1
	lda	#<(_pc)
	sta	r0
	sta	r31
	lda	_code_index
	sta	r2
	lda	#0
	sta	r3
	lda	r18
	clc
	adc	#13
	sta	r0
	lda	r19
	adc	#0
	sta	r1
	lda	r0
	clc
	adc	r2
	sta	r0
	lda	r1
	adc	r3
	sta	r1
	lda	r31
	sta	(r0),y
	sta	r31
	lda	_code_index
	sta	r2
	lda	#0
	sta	r3
	lda	r18
	clc
	adc	#14
	sta	r0
	lda	r19
	adc	#0
	sta	r1
	lda	r0
	clc
	adc	r2
	sta	r0
	lda	r1
	adc	r3
	sta	r1
	lda	#169
	sta	(r0),y
	lda	r31
	sta	r30
	lda	r25
	ldx	#0
	sta	r2
	stx	r3
	lda	r30
	sta	r31
	lda	r2
	sta	r0
	and	#255
	sta	r0
	lda	_code_index
	sta	r6
	txa
	sta	r7
	lda	r18
	clc
	adc	#15
	sta	r2
	lda	r19
	adc	#0
	sta	r3
	lda	r2
	clc
	adc	r6
	sta	r2
	lda	r3
	adc	r7
	sta	r3
	lda	r0
	sta	(r2),y
	lda	_code_index
	sta	r2
	txa
	sta	r3
	lda	r18
	clc
	adc	#16
	sta	r0
	lda	r19
	adc	#0
	sta	r1
	lda	r0
	clc
	adc	r2
	sta	r0
	lda	r1
	adc	r3
	sta	r1
	lda	#133
	sta	(r0),y
	lda	r31
	clc
	adc	#1
	sta	r31
	lda	_code_index
	sta	r2
	txa
	sta	r3
	lda	r18
	clc
	adc	#17
	sta	r0
	lda	r19
	adc	#0
	sta	r1
	lda	r0
	clc
	adc	r2
	sta	r0
	lda	r1
	adc	r3
	sta	r1
	lda	r31
	sta	(r0),y
	lda	_code_index
	sta	r0
	txa
	sta	r1
	ldx	r19
	lda	r18
	clc
	adc	#18
	bcc	l772
	inx
l772:
	clc
	adc	r0
	sta	r0
	txa
	adc	r1
	sta	r1
	lda	#76
	sta	(r0),y
	ldx	#>(_cross_bank_dispatch)
	lda	#<(_cross_bank_dispatch)
	sta	r0
	sta	r31
	lda	_code_index
	sta	r6
	lda	#0
	sta	r7
	lda	r18
	clc
	adc	#19
	sta	r2
	lda	r19
	adc	#0
	sta	r3
	lda	r2
	clc
	adc	r6
	sta	r2
	lda	r3
	adc	r7
	sta	r3
	lda	r0
	sta	(r2),y
	lda	r31
	sta	r30
	stx	r29
	txa
	ldx	#0
	sta	r0
	stx	r1
	ldx	r29
	lda	r0
	sta	r31
	lda	_code_index
	sta	r2
	lda	#0
	sta	r3
	lda	r18
	clc
	adc	#20
	sta	r0
	lda	r19
	adc	#0
	sta	r1
	lda	r0
	clc
	adc	r2
	sta	r0
	lda	r1
	adc	r3
	sta	r1
	lda	r31
	sta	(r0),y
	ldy	#2
	lda	(sp),y
	clc
	adc	#8
	sta	r0
	iny
	lda	(sp),y
	adc	#0
	sta	r1
	ldy	#6
	lda	(sp),y
	ldx	#0
	clc
	adc	r0
	pha
	txa
	adc	r1
	tax
	pla
	sta	r31
	clc
	adc	#3
	sta	r0
	txa
	adc	#0
	sta	r1
	lda	r31
	clc
	adc	#5
	bcc	l773
	inx
l773:
	sta	r31
	lda	#0
	tay
	sta	(sp),y
	lda	r31
	sta	r2
	stx	r3
	ldy	#4
	lda	(sp),y
	sta	r4
	lda	r25
	sta	r7
	lda	r24
	sta	r6
	jsr	_opt2_record_pending_branch_safe
	lda	1+_pc
	sta	r1
	lda	_pc
	sta	r0
	lda	1+_flash_cache_index
	sta	r3
	lda	_flash_cache_index
	sta	r2
	jsr	_setup_flash_address
	ldy	#6
	lda	(sp),y
	sta	r0
	lda	#128
	sta	r2
	jsr	_flash_cache_pc_update
	inc	_pc
	bne	l774
	inc	1+_pc
l774:
	inc	_pc
	bne	l775
	inc	1+_pc
l775:
	lda	_code_index
	clc
	adc	#21
	sta	_code_index
	inc	_cache_branches
	bne	l776
	inc	1+_cache_branches
	bne	l776
	inc	2+_cache_branches
	bne	l776
	inc	3+_cache_branches
l776:
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r0
	lda	#>(_cache_flag)
	adc	#0
	sta	r1
	ldy	#0
	lda	(r0),y
	ora	#32
	sta	(r0),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r0
	lda	#>(_cache_flag)
	adc	#0
	sta	r1
	lda	(r0),y
	jmp	l407
l429:
	lda	r25
	sta	r1
	lda	r24
	sta	r0
	lda	r17
	sta	r2
	jsr	_try_intra_block_branch
	cmp	#0
	beq	l435
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	ldy	#0
	lda	(r8),y
	jmp	l407
l435:
	ldy	#4
	lda	(sp),y
	and	#31
	cmp	_flash_code_bank
	bne	l447
	lda	_code_index
	ldx	#0
	clc
	adc	#29
	bcc	l777
	inx
l777:
	pha
	cmp	#211
	txa
	sbc	#0
	bvc	l779
	eor	#128
l779:
	bpl	l778
	pla
	lda	r25
	sta	r1
	lda	r24
	sta	r0
	jsr	_lookup_native_addr_safe
	cmp	#0
	beq	l447
	lda	_flash_code_address
	clc
	adc	#8
	sta	r8
	lda	1+_flash_code_address
	adc	#0
	sta	r9
	lda	_code_index
	ldx	#0
	clc
	adc	r8
	pha
	txa
	adc	r9
	tax
	pla
	clc
	adc	#2
	bcc	l780
	inx
l780:
	sta	r8
	stx	r9
	lda	_reserve_result_addr
	sec
	sbc	r8
	ldy	#2
	sta	(sp),y
	lda	1+_reserve_result_addr
	sbc	r9
	iny
	sta	(sp),y
	lda	#0
	sta	r5
	dey
	lda	(sp),y
	cmp	#128
	iny
	lda	(sp),y
	sbc	#255
	bvc	l781
	eor	#128
l781:
	bmi	l441
	lda	#127
	ldy	#2
	cmp	(sp),y
	lda	#0
	iny
	sbc	(sp),y
	bvc	l782
	eor	#128
l782:
	bmi	l441
	lda	r18
	clc
	adc	_code_index
	sta	r8
	lda	r19
	adc	#0
	sta	r9
	lda	r17
	ldy	#0
	sta	(r8),y
	ldy	#2
	lda	(sp),y
	sta	r31
	lda	_code_index
	sta	r10
	lda	#0
	sta	r11
	lda	r18
	clc
	adc	#1
	sta	r8
	lda	r19
	adc	#0
	sta	r9
	lda	r8
	clc
	adc	r10
	sta	r8
	lda	r9
	adc	r11
	sta	r9
	lda	r31
	ldy	#0
	sta	(r8),y
	lda	#2
	sta	r5
	jmp	l445
l441:
	lda	_code_index
	ldx	#0
	clc
	adc	#32
	bcc	l783
	inx
l783:
	pha
	cmp	#211
	txa
	sbc	#0
	bvc	l785
	eor	#128
l785:
	bpl	l784
	pla
	lda	r17
	sta	r0
	jsr	l199
	sta	r31
	lda	r18
	clc
	adc	_code_index
	sta	r8
	lda	r19
	adc	#0
	sta	r9
	lda	r31
	ldy	#0
	sta	(r8),y
	lda	_code_index
	sta	r8
	tya
	sta	r9
	ldx	r19
	lda	r18
	clc
	adc	#1
	bcc	l786
	inx
l786:
	clc
	adc	r8
	sta	r8
	txa
	adc	r9
	sta	r9
	lda	#3
	sta	(r8),y
	lda	_code_index
	sta	r8
	lda	#0
	sta	r9
	ldx	r19
	lda	r18
	clc
	adc	#2
	bcc	l787
	inx
l787:
	clc
	adc	r8
	sta	r8
	txa
	adc	r9
	sta	r9
	lda	#76
	sta	(r8),y
	lda	_reserve_result_addr
	and	#255
	sta	r31
	lda	_code_index
	sta	r10
	lda	#0
	sta	r11
	lda	r18
	clc
	adc	#3
	sta	r8
	lda	r19
	adc	#0
	sta	r9
	lda	r8
	clc
	adc	r10
	sta	r8
	lda	r9
	adc	r11
	sta	r9
	lda	r31
	sta	(r8),y
	lda	1+_reserve_result_addr
	ldx	#0
	sta	r8
	stx	r9
	lda	r8
	and	#255
	sta	r31
	lda	_code_index
	sta	r10
	txa
	sta	r11
	lda	r18
	clc
	adc	#4
	sta	r8
	lda	r19
	adc	#0
	sta	r9
	lda	r8
	clc
	adc	r10
	sta	r8
	lda	r9
	adc	r11
	sta	r9
	lda	r31
	sta	(r8),y
	lda	#5
	sta	r5
l445:
	lda	r5
	beq	l447
	lda	1+_pc
	sta	r1
	lda	_pc
	sta	r0
	lda	1+_flash_cache_index
	sta	r3
	lda	_flash_cache_index
	sta	r2
	jsr	_setup_flash_address
	lda	_code_index
	sta	r0
	lda	#128
	sta	r2
	lda	r5
	ldy	#2
	sta	(sp),y
	jsr	_flash_cache_pc_update
	ldy	#2
	lda	(sp),y
	sta	r5
	inc	_pc
	bne	l788
	inc	1+_pc
l788:
	inc	_pc
	bne	l789
	inc	1+_pc
l789:
	lda	_code_index
	clc
	adc	r5
	sta	_code_index
	inc	_cache_branches
	bne	l790
	inc	1+_cache_branches
	bne	l790
	inc	2+_cache_branches
	bne	l790
	inc	3+_cache_branches
l790:
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r0
	lda	#>(_cache_flag)
	adc	#0
	sta	r1
	ldy	#0
	lda	(r0),y
	ora	#32
	sta	(r0),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r0
	lda	#>(_cache_flag)
	adc	#0
	sta	r1
	lda	(r0),y
	jmp	l407
l447:
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	ldy	#0
	lda	(r8),y
	ora	#2
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	and	#223
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	jmp	l407
l433:
	lda	_pc
	clc
	adc	#1
	sta	r0
	lda	1+_pc
	adc	#0
	sta	r1
	jsr	_read6502
	sta	r31
	sta	r16
	lda	#0
	sta	r17
	lda	_pc
	clc
	adc	#2
	sta	r0
	lda	1+_pc
	adc	#0
	sta	r1
	jsr	_read6502
	sta	r1
	tax
	lda	#0
	ora	r16
	ldy	#2
	sta	(sp),y
	txa
	ora	r17
	iny
	sta	(sp),y
	lda	_sa_compile_pass
	cmp	#2
	bne	l454
	lda	_code_index
	ldx	#0
	clc
	adc	#3
	bcc	l791
	inx
l791:
	pha
	cmp	#211
	txa
	sbc	#0
	bvc	l793
	eor	#128
l793:
	bpl	l792
	pla
	ldy	#3
	lda	(sp),y
	sta	r1
	dey
	lda	(sp),y
	sta	r0
	jsr	_lookup_entry_list
	cmp	#0
	beq	l454
	lda	_reserve_result_bank
	cmp	_flash_code_bank
	bne	l454
	lda	r18
	clc
	adc	_code_index
	sta	r8
	lda	r19
	adc	#0
	sta	r9
	lda	#76
	ldy	#0
	sta	(r8),y
	lda	_reserve_result_addr
	and	#255
	sta	r31
	lda	_code_index
	sta	r10
	tya
	sta	r11
	lda	r18
	clc
	adc	#1
	sta	r8
	lda	r19
	adc	#0
	sta	r9
	lda	r8
	clc
	adc	r10
	sta	r8
	lda	r9
	adc	r11
	sta	r9
	lda	r31
	sta	(r8),y
	stx	r30
	lda	1+_reserve_result_addr
	ldx	#0
	sta	r8
	stx	r9
	ldx	r30
	lda	r8
	and	#255
	sta	r31
	lda	_code_index
	sta	r10
	tya
	sta	r11
	lda	r18
	clc
	adc	#2
	sta	r8
	lda	r19
	adc	#0
	sta	r9
	lda	r8
	clc
	adc	r10
	sta	r8
	lda	r9
	adc	r11
	sta	r9
	lda	r31
	sta	(r8),y
	ldy	#3
	lda	(sp),y
	sta	1+_pc
	dey
	lda	(sp),y
	sta	_pc
	lda	_code_index
	clc
	adc	#3
	sta	_code_index
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	ldy	#0
	lda	(r8),y
	and	#223
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	jmp	l407
l454:
	lda	_code_index
	ldx	#0
	clc
	adc	#9
	bcc	l794
	inx
l794:
	pha
	cmp	#211
	txa
	sbc	#0
	bvc	l796
	eor	#128
l796:
	bpl	l795
	pla
	lda	r18
	clc
	adc	_code_index
	sta	r0
	lda	r19
	adc	#0
	sta	r1
	lda	#8
	ldy	#0
	sta	(r0),y
	lda	_code_index
	sta	r0
	tya
	sta	r1
	ldx	r19
	lda	r18
	clc
	adc	#1
	bcc	l797
	inx
l797:
	clc
	adc	r0
	sta	r0
	txa
	adc	r1
	sta	r1
	lda	#24
	sta	(r0),y
	lda	_code_index
	sta	r0
	lda	#0
	sta	r1
	ldx	r19
	lda	r18
	clc
	adc	#2
	bcc	l798
	inx
l798:
	clc
	adc	r0
	sta	r0
	txa
	adc	r1
	sta	r1
	lda	#144
	sta	(r0),y
	lda	_code_index
	sta	r0
	lda	#0
	sta	r1
	ldx	r19
	lda	r18
	clc
	adc	#3
	bcc	l799
	inx
l799:
	clc
	adc	r0
	sta	r0
	txa
	adc	r1
	sta	r1
	lda	#4
	sta	(r0),y
	lda	_code_index
	sta	r0
	lda	#0
	sta	r1
	ldx	r19
	lda	r18
	clc
	adc	#4
	bcc	l800
	inx
l800:
	clc
	adc	r0
	sta	r0
	txa
	adc	r1
	sta	r1
	lda	#40
	sta	(r0),y
	lda	_code_index
	sta	r0
	lda	#0
	sta	r1
	ldx	r19
	lda	r18
	clc
	adc	#5
	bcc	l801
	inx
l801:
	clc
	adc	r0
	sta	r0
	txa
	adc	r1
	sta	r1
	lda	#76
	sta	(r0),y
	lda	_code_index
	sta	r0
	lda	#0
	sta	r1
	ldx	r19
	lda	r18
	clc
	adc	#6
	bcc	l802
	inx
l802:
	clc
	adc	r0
	sta	r0
	txa
	adc	r1
	sta	r1
	lda	#255
	sta	(r0),y
	lda	_code_index
	sta	r0
	lda	#0
	sta	r1
	ldx	r19
	lda	r18
	clc
	adc	#7
	bcc	l803
	inx
l803:
	clc
	adc	r0
	sta	r0
	txa
	adc	r1
	sta	r1
	lda	#255
	sta	(r0),y
	lda	_code_index
	sta	r0
	lda	#0
	sta	r1
	ldx	r19
	lda	r18
	clc
	adc	#8
	bcc	l804
	inx
l804:
	clc
	adc	r0
	sta	r0
	txa
	adc	r1
	sta	r1
	lda	#40
	sta	(r0),y
	lda	_flash_code_address
	clc
	adc	#8
	sta	r0
	lda	1+_flash_code_address
	adc	#0
	sta	r1
	lda	_code_index
	ldx	#0
	clc
	adc	r0
	pha
	txa
	adc	r1
	tax
	pla
	sta	r31
	clc
	adc	#3
	sta	r0
	txa
	adc	#0
	sta	r1
	lda	r31
	clc
	adc	#6
	bcc	l805
	inx
l805:
	sta	r31
	lda	#0
	sta	(sp),y
	lda	r31
	sta	r2
	stx	r3
	lda	_flash_code_bank
	sta	r4
	ldy	#3
	lda	(sp),y
	sta	r7
	dey
	lda	(sp),y
	sta	r6
	jsr	_opt2_record_pending_branch_safe
	ldy	#3
	lda	(sp),y
	sta	1+_pc
	dey
	lda	(sp),y
	sta	_pc
	lda	_code_index
	clc
	adc	#9
	sta	_code_index
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r0
	lda	#>(_cache_flag)
	adc	#0
	sta	r1
	ldy	#0
	lda	(r0),y
	and	#223
	sta	(r0),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r0
	lda	#>(_cache_flag)
	adc	#0
	sta	r1
	lda	(r0),y
	jmp	l407
l456:
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	ldy	#0
	lda	(r8),y
	ora	#2
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	and	#223
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	jmp	l407
l458:
	lda	_pc
	clc
	adc	#1
	sta	r0
	lda	1+_pc
	adc	#0
	sta	r1
	jsr	_read6502
	sta	r31
	sta	r16
	lda	#0
	sta	r17
	lda	_pc
	clc
	adc	#2
	sta	r0
	lda	1+_pc
	adc	#0
	sta	r1
	jsr	_read6502
	sta	r1
	tax
	lda	#0
	ora	r16
	sta	r26
	txa
	ora	r17
	sta	r27
	lda	_pc
	clc
	adc	#2
	ldy	#2
	sta	(sp),y
	lda	1+_pc
	adc	#0
	iny
	sta	(sp),y
	lda	r27
	sta	r1
	lda	r26
	sta	r0
	jsr	_sa_subroutine_lookup
	cmp	#0
	bne	l460
	lda	_code_index
	sta	r8
	lda	#0
	sta	r9
	lda	_opcode_6502_njsr_size
	ldx	#0
	clc
	adc	r8
	pha
	txa
	adc	r9
	tax
	pla
	clc
	adc	#27
	bcc	l806
	inx
l806:
	pha
	cmp	#211
	txa
	sbc	#0
	bvc	l808
	eor	#128
l808:
	bpl	l807
	pla
	lda	#0
	sta	r1
	lda	_opcode_6502_njsr_size
	beq	l600
	lda	r19
	sta	r23
	lda	r18
	sta	r22
	lda	r1
	ldy	#4
	sta	(sp),y
	tax
l590:
	lda	r22
	clc
	adc	_code_index
	sta	r10
	lda	r23
	adc	#0
	sta	r11
	lda	0+_opcode_6502_njsr,x ;am(x)
	ldy	#0
	sta	(r10),y
	inx
	inc	r22
	bne	l809
	inc	r23
l809:
	cpx	_opcode_6502_njsr_size
	bcc	l590
l600:
	stx	r30
	ldy	#3
	lda	(sp),y
	ldx	#0
	sta	r8
	stx	r9
	ldx	r30
	lda	r8
	sta	r31
	lda	_code_index
	sta	r10
	lda	#0
	sta	r11
	lda	r18
	clc
	adc	_opcode_6502_njsr_ret_hi
	sta	r8
	lda	r19
	adc	#0
	sta	r9
	lda	r8
	clc
	adc	r10
	sta	r8
	lda	r9
	adc	r11
	sta	r9
	lda	r31
	ldy	#0
	sta	(r8),y
	ldy	#2
	lda	(sp),y
	sta	r31
	lda	_code_index
	sta	r10
	lda	#0
	sta	r11
	lda	r18
	clc
	adc	_opcode_6502_njsr_ret_lo
	sta	r8
	lda	r19
	adc	#0
	sta	r9
	lda	r8
	clc
	adc	r10
	sta	r8
	lda	r9
	adc	r11
	sta	r9
	lda	r31
	ldy	#0
	sta	(r8),y
	lda	r26
	sta	r31
	lda	_code_index
	sta	r10
	tya
	sta	r11
	lda	r18
	clc
	adc	_opcode_6502_njsr_tgt_lo
	sta	r8
	lda	r19
	adc	#0
	sta	r9
	lda	r8
	clc
	adc	r10
	sta	r8
	lda	r9
	adc	r11
	sta	r9
	lda	r31
	sta	(r8),y
	stx	r30
	lda	r27
	ldx	#0
	sta	r8
	stx	r9
	ldx	r30
	lda	r8
	sta	r31
	lda	_code_index
	sta	r10
	tya
	sta	r11
	lda	r18
	clc
	adc	_opcode_6502_njsr_tgt_hi
	sta	r8
	lda	r19
	adc	#0
	sta	r9
	lda	r8
	clc
	adc	r10
	sta	r8
	lda	r9
	adc	r11
	sta	r9
	lda	r31
	sta	(r8),y
	lda	_pc
	clc
	adc	#3
	sta	_pc
	lda	1+_pc
	adc	#0
	sta	1+_pc
	lda	_code_index
	clc
	adc	_opcode_6502_njsr_size
	sta	_code_index
	lda	#1
	sta	_block_has_jsr
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	ora	#32
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	jmp	l407
l460:
	lda	_code_index
	sta	r8
	lda	#0
	sta	r9
	lda	_opcode_6502_jsr_size
	ldx	#0
	clc
	adc	r8
	pha
	txa
	adc	r9
	tax
	pla
	clc
	adc	#27
	bcc	l810
	inx
l810:
	pha
	cmp	#211
	txa
	sbc	#0
	bvc	l812
	eor	#128
l812:
	bpl	l811
	pla
	ldx	#0
	lda	_opcode_6502_jsr_size
	beq	l601
	lda	r19
	sta	r7
	lda	r18
	sta	r6
l596:
	lda	r6
	clc
	adc	_code_index
	sta	r10
	lda	r7
	adc	#0
	sta	r11
	lda	0+_opcode_6502_jsr,x ;am(x)
	ldy	#0
	sta	(r10),y
	inx
	inc	r6
	bne	l813
	inc	r7
l813:
	cpx	_opcode_6502_jsr_size
	bcc	l596
l601:
	stx	r30
	ldy	#3
	lda	(sp),y
	ldx	#0
	sta	r8
	stx	r9
	ldx	r30
	lda	r8
	sta	r31
	lda	_code_index
	sta	r10
	lda	#0
	sta	r11
	lda	r18
	clc
	adc	_opcode_6502_jsr_ret_hi
	sta	r8
	lda	r19
	adc	#0
	sta	r9
	lda	r8
	clc
	adc	r10
	sta	r8
	lda	r9
	adc	r11
	sta	r9
	lda	r31
	ldy	#0
	sta	(r8),y
	ldy	#2
	lda	(sp),y
	sta	r31
	lda	_code_index
	sta	r10
	lda	#0
	sta	r11
	lda	r18
	clc
	adc	_opcode_6502_jsr_ret_lo
	sta	r8
	lda	r19
	adc	#0
	sta	r9
	lda	r8
	clc
	adc	r10
	sta	r8
	lda	r9
	adc	r11
	sta	r9
	lda	r31
	ldy	#0
	sta	(r8),y
	lda	r26
	sta	r31
	lda	_code_index
	sta	r10
	tya
	sta	r11
	lda	r18
	clc
	adc	_opcode_6502_jsr_tgt_lo
	sta	r8
	lda	r19
	adc	#0
	sta	r9
	lda	r8
	clc
	adc	r10
	sta	r8
	lda	r9
	adc	r11
	sta	r9
	lda	r31
	sta	(r8),y
	stx	r30
	lda	r27
	ldx	#0
	sta	r8
	stx	r9
	ldx	r30
	lda	r8
	sta	r31
	lda	_code_index
	sta	r10
	tya
	sta	r11
	lda	r18
	clc
	adc	_opcode_6502_jsr_tgt_hi
	sta	r8
	lda	r19
	adc	#0
	sta	r9
	lda	r8
	clc
	adc	r10
	sta	r8
	lda	r9
	adc	r11
	sta	r9
	lda	r31
	sta	(r8),y
	lda	_pc
	clc
	adc	#3
	sta	_pc
	lda	1+_pc
	adc	#0
	sta	1+_pc
	lda	_code_index
	clc
	adc	_opcode_6502_jsr_size
	sta	_code_index
	lda	#1
	sta	_block_has_jsr
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	ora	#32
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	jmp	l407
l468:
	lda	_pc
	clc
	adc	#1
	sta	r0
	lda	1+_pc
	adc	#0
	sta	r1
	jsr	_read6502
	sta	r31
	lda	#<(_cache_branch_pc_lo)
	clc
	adc	_cache_index
	sta	r0
	lda	#>(_cache_branch_pc_lo)
	adc	#0
	sta	r1
	lda	r31
	ldy	#0
	sta	(r0),y
	lda	_pc
	clc
	adc	#2
	sta	r0
	lda	1+_pc
	adc	#0
	sta	r1
	jsr	_read6502
	sta	r31
	lda	#<(_cache_branch_pc_hi)
	clc
	adc	_cache_index
	sta	r0
	lda	#>(_cache_branch_pc_hi)
	adc	#0
	sta	r1
	lda	r31
	ldy	#0
	sta	(r0),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r0
	lda	#>(_cache_flag)
	adc	#0
	sta	r1
	lda	(r0),y
	ora	#2
	sta	(r0),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r0
	lda	#>(_cache_flag)
	adc	#0
	sta	r1
	lda	(r0),y
	and	#223
	sta	(r0),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r0
	lda	#>(_cache_flag)
	adc	#0
	sta	r1
	lda	(r0),y
	jmp	l407
l474:
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	ldy	#0
	lda	(r8),y
	ora	#2
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	and	#223
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	jmp	l407
l476:
	lda	_code_index
	sta	r8
	lda	#0
	sta	r9
	lda	_opcode_6502_nrts_size
	ldx	#0
	clc
	adc	r8
	pha
	txa
	adc	r9
	tax
	pla
	clc
	adc	#27
	bcc	l814
	inx
l814:
	pha
	cmp	#211
	txa
	sbc	#0
	bvc	l816
	eor	#128
l816:
	bpl	l815
	pla
	lda	#0
	sta	r3
	lda	_opcode_6502_nrts_size
	beq	l602
	lda	r19
	sta	r15
	lda	r18
	sta	r14
	lda	r3
	ldy	#2
	sta	(sp),y
	tax
l592:
	lda	r14
	clc
	adc	_code_index
	sta	r10
	lda	r15
	adc	#0
	sta	r11
	lda	0+_opcode_6502_nrts,x ;am(x)
	ldy	#0
	sta	(r10),y
	inx
	inc	r14
	bne	l817
	inc	r15
l817:
	cpx	_opcode_6502_nrts_size
	bcc	l592
l602:
	inc	_pc
	bne	l818
	inc	1+_pc
l818:
	lda	_code_index
	clc
	adc	_opcode_6502_nrts_size
	sta	_code_index
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	ldy	#0
	lda	(r8),y
	and	#223
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	jmp	l407
l478:
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	ldy	#0
	lda	(r8),y
	ora	#2
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	and	#223
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	jmp	l407
l483:
	lda	_code_index
	ldx	#0
	clc
	adc	#6
	bcc	l819
	inx
l819:
	pha
	cmp	#211
	txa
	sbc	#0
	bvc	l821
	eor	#128
l821:
	bpl	l820
	pla
	lda	r18
	clc
	adc	_code_index
	sta	r8
	lda	r19
	adc	#0
	sta	r9
	lda	#166
	ldy	#0
	sta	(r8),y
	ldx	#>(_sp)
	lda	#<(_sp)
	sta	r31
	lda	_code_index
	sta	r10
	tya
	sta	r11
	lda	r18
	clc
	adc	#1
	sta	r8
	lda	r19
	adc	#0
	sta	r9
	lda	r8
	clc
	adc	r10
	sta	r8
	lda	r9
	adc	r11
	sta	r9
	lda	r31
	sta	(r8),y
	inc	_pc
	bne	l822
	inc	1+_pc
l822:
	inc	_code_index
	inc	_code_index
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	ora	#32
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	jmp	l407
l485:
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	ldy	#0
	lda	(r8),y
	ora	#6
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	and	#223
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	jmp	l407
l486:
	lda	_code_index
	ldx	#0
	clc
	adc	#6
	bcc	l823
	inx
l823:
	pha
	cmp	#211
	txa
	sbc	#0
	bvc	l825
	eor	#128
l825:
	bpl	l824
	pla
	lda	r18
	clc
	adc	_code_index
	sta	r8
	lda	r19
	adc	#0
	sta	r9
	lda	#134
	ldy	#0
	sta	(r8),y
	ldx	#>(_sp)
	lda	#<(_sp)
	sta	r31
	lda	_code_index
	sta	r10
	tya
	sta	r11
	lda	r18
	clc
	adc	#1
	sta	r8
	lda	r19
	adc	#0
	sta	r9
	lda	r8
	clc
	adc	r10
	sta	r8
	lda	r9
	adc	r11
	sta	r9
	lda	r31
	sta	(r8),y
	inc	_pc
	bne	l826
	inc	1+_pc
l826:
	inc	_code_index
	inc	_code_index
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	ora	#32
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	jmp	l407
l489:
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	ldy	#0
	lda	(r8),y
	ora	#6
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	and	#223
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	jmp	l407
l490:
	lda	#>(_opcode_6502_pha)
	sta	r1
	lda	#<(_opcode_6502_pha)
	sta	r0
	lda	_opcode_6502_pha_size
	sta	r2
	jsr	_emit_template
	jmp	l407
l492:
	lda	#>(_opcode_6502_pla)
	sta	r1
	lda	#<(_opcode_6502_pla)
	sta	r0
	lda	_opcode_6502_pla_size
	sta	r2
	jsr	_emit_template
	jmp	l407
l493:
	lda	#>(_opcode_6502_php)
	sta	r1
	lda	#<(_opcode_6502_php)
	sta	r0
	lda	_opcode_6502_php_size
	sta	r2
	jsr	_emit_template
	jmp	l407
l494:
	lda	#>(_opcode_6502_plp)
	sta	r1
	lda	#<(_opcode_6502_plp)
	sta	r0
	lda	_opcode_6502_plp_size
	sta	r2
	jsr	_emit_template
	jmp	l407
l495:
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	ldy	#0
	lda	(r8),y
	ora	#2
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	and	#223
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	jmp	l407
l497:
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	ldy	#0
	lda	(r8),y
	ora	#2
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	and	#223
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	jmp	l407
l499:
	lda	#1
	sta	_decimal_mode
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	ldy	#0
	lda	(r8),y
	ora	#2
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	and	#223
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	jmp	l407
l500:
	inc	_pc
	bne	l827
	inc	1+_pc
l827:
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	ldy	#0
	lda	(r8),y
	ora	#32
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	jmp	l407
l501:
; volatile barrier
	lda	#0
	sta	16417
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	ldy	#0
	lda	(r8),y
	ora	#2
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	and	#223
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	jmp	l407
l502:
	lda	r18
	clc
	adc	_code_index
	sta	r8
	lda	r19
	adc	#0
	sta	r9
	lda	r17
	ldy	#0
	sta	(r8),y
	ldy	r17
	lda	0+_addrmodes,y ;am(r17)
	sta	r4
	cmp	#0
	beq	l562
	lda	r4
	cmp	#1
	beq	l562
	lda	r4
	cmp	#2
	beq	l560
	lda	r4
	cmp	#3
	beq	l526
	lda	r4
	cmp	#4
	beq	l534
	lda	r4
	cmp	#5
	beq	l534
	lda	r4
	cmp	#6
	beq	l560
	lda	r4
	cmp	#7
	beq	l504
	lda	r4
	cmp	#8
	beq	l504
	lda	r4
	cmp	#9
	beq	l504
	lda	r4
	cmp	#10
	beq	l504
	lda	r4
	cmp	#11
	beq	l536
	lda	r4
	cmp	#12
	beq	l537
	jmp	l564
l504:
	lda	_pc
	clc
	adc	#1
	sta	r0
	lda	1+_pc
	adc	#0
	sta	r1
	jsr	_read6502
	sta	r16
	lda	_pc
	clc
	adc	#2
	sta	r0
	lda	1+_pc
	adc	#0
	sta	r1
	jsr	_read6502
	sta	r1
	ldx	#0
	sta	r30
	stx	r29
	tax
	lda	#0
	sta	r0
	stx	r1
	ldx	r29
	lda	r16
	ldx	#0
	ora	r0
	sta	_encoded_address
	txa
	ora	r1
	sta	1+_encoded_address
	sta	r1
	lda	_encoded_address
	sta	r0
	jsr	_translate_address
	sta	_decoded_address
	stx	1+_decoded_address
	lda	_decoded_address
	ora	1+_decoded_address
	beq	l509
	lda	_decoded_address
	sta	r31
	lda	_code_index
	sta	r10
	lda	#0
	sta	r11
	lda	r18
	clc
	adc	#1
	sta	r8
	lda	r19
	adc	#0
	sta	r9
	lda	r8
	clc
	adc	r10
	sta	r8
	lda	r9
	adc	r11
	sta	r9
	lda	r31
	ldy	#0
	sta	(r8),y
	stx	r30
	lda	1+_decoded_address
	ldx	#0
	sta	r8
	stx	r9
	ldx	r30
	lda	r8
	sta	r31
	lda	_code_index
	sta	r10
	tya
	sta	r11
	lda	r18
	clc
	adc	#2
	sta	r8
	lda	r19
	adc	#0
	sta	r9
	lda	r8
	clc
	adc	r10
	sta	r8
	lda	r9
	adc	r11
	sta	r9
	lda	r31
	sta	(r8),y
	jmp	l510
l509:
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	ldy	#0
	lda	(r8),y
	ora	#2
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	and	#223
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	jmp	l407
l510:
	lda	_pc
	clc
	adc	#3
	sta	_pc
	lda	1+_pc
	adc	#0
	sta	1+_pc
	lda	_code_index
	clc
	adc	#3
	sta	_code_index
	lda	_decoded_address
	ora	1+_decoded_address
	beq	l503
	lda	1+_encoded_address
	ldx	#0
	sta	r16
	cmp	#64
	bcc	l503
	lda	r16
	cmp	#80
	bcs	l503
	lda	r16
	cmp	#72
	bcs	l519
	lda	l26
	ldy	#2
	sta	(sp),y
	lda	#0
	iny
	sta	(sp),y
	jmp	l520
l519:
	lda	l27
	ldy	#2
	sta	(sp),y
	lda	#0
	iny
	sta	(sp),y
l520:
	ldy	#2
	lda	(sp),y
	iny
	ora	(sp),y
	bne	l503
	lda	_code_index
	ldx	#0
	clc
	adc	#25
	bcc	l828
	inx
l828:
	pha
	cmp	#211
	txa
	sbc	#0
	bvc	l830
	eor	#128
l830:
	bpl	l829
	pla
	lda	#>(_screen_ram_updated)
	sta	r1
	lda	#<(_screen_ram_updated)
	sta	r0
	ldy	#0
	sta	(sp),y
	lda	#>(_character_ram_updated)
	sta	r1
	lda	#<(_character_ram_updated)
	sta	r0
	iny
	sta	(sp),y
	lda	r19
	sta	r1
	lda	r18
	sta	r0
	lda	_code_index
	sta	r2
	lda	r17
	sta	r4
	lda	r16
	sta	r6
	jsr	_emit_dirty_flag
	sta	r9
	cmp	#0
	beq	l503
	lda	_code_index
	clc
	adc	r9
	sta	_code_index
	lda	r16
	cmp	#72
	bcs	l524
	lda	#1
	sta	l26
	jmp	l503
l524:
	lda	#1
	sta	l27
	jmp	l503
l526:
	lda	r18
	clc
	adc	_code_index
	sta	r0
	lda	r19
	adc	#0
	sta	r1
	ldy	#0
	lda	(r0),y
	ora	#8
	sta	(r0),y
	lda	_pc
	clc
	adc	#1
	sta	r0
	lda	1+_pc
	adc	#0
	sta	r1
	jsr	_read6502
	ldy	#2
	sta	(sp),y
	sta	r0
	lda	#0
	sta	r1
	ldx	#>(_RAM_BASE)
	lda	#<(_RAM_BASE)
	clc
	adc	r0
	pha
	txa
	adc	r1
	tax
	pla
	sta	r1
	sta	r31
	lda	_code_index
	sta	r10
	lda	#0
	sta	r11
	lda	r18
	clc
	adc	#1
	sta	r8
	lda	r19
	adc	#0
	sta	r9
	lda	r8
	clc
	adc	r10
	sta	r8
	lda	r9
	adc	r11
	sta	r9
	lda	r1
	ldy	#0
	sta	(r8),y
	lda	r31
	sta	r30
	stx	r29
	txa
	ldx	#0
	sta	r0
	stx	r1
	ldx	r29
	lda	r0
	sta	r31
	lda	_code_index
	sta	r8
	tya
	sta	r9
	lda	r18
	clc
	adc	#2
	sta	r0
	lda	r19
	adc	#0
	sta	r1
	lda	r0
	clc
	adc	r8
	sta	r0
	lda	r1
	adc	r9
	sta	r1
	lda	r31
	sta	(r0),y
	inc	_pc
	bne	l831
	inc	1+_pc
l831:
	inc	_pc
	bne	l832
	inc	1+_pc
l832:
	lda	_code_index
	clc
	adc	#3
	sta	_code_index
	lda	r17
	sec
	sbc	#132
	cmp	#3
	bcc	l529
	lda	r17
	cmp	#230
	beq	l529
	lda	r17
	cmp	#198
	bne	l503
l529:
	lda	_code_index
	ldx	#0
	clc
	adc	#23
	bcc	l833
	inx
l833:
	pha
	cmp	#211
	txa
	sbc	#0
	bvc	l835
	eor	#128
l835:
	bpl	l834
	pla
	lda	r19
	sta	r1
	lda	r18
	sta	r0
	lda	_code_index
	sta	r2
	lda	r17
	sta	r4
	ldy	#2
	lda	(sp),y
	sta	r6
	jsr	_emit_zp_mirror_write
	clc
	adc	_code_index
	sta	_code_index
	jmp	l503
l534:
	lda	r18
	clc
	adc	_code_index
	sta	r0
	lda	r19
	adc	#0
	sta	r1
	ldy	#0
	lda	(r0),y
	ora	#8
	sta	(r0),y
	lda	_pc
	clc
	adc	#1
	sta	r0
	lda	1+_pc
	adc	#0
	sta	r1
	jsr	_read6502
	sta	r31
	sta	r0
	lda	#0
	sta	r1
	ldx	#>(_RAM_BASE)
	lda	#<(_RAM_BASE)
	clc
	adc	r0
	pha
	txa
	adc	r1
	tax
	pla
	sta	r1
	sta	r31
	lda	_code_index
	sta	r10
	lda	#0
	sta	r11
	lda	r18
	clc
	adc	#1
	sta	r8
	lda	r19
	adc	#0
	sta	r9
	lda	r8
	clc
	adc	r10
	sta	r8
	lda	r9
	adc	r11
	sta	r9
	lda	r1
	ldy	#0
	sta	(r8),y
	lda	r31
	sta	r30
	stx	r29
	txa
	ldx	#0
	sta	r0
	stx	r1
	ldx	r29
	lda	r0
	sta	r31
	lda	_code_index
	sta	r8
	tya
	sta	r9
	lda	r18
	clc
	adc	#2
	sta	r0
	lda	r19
	adc	#0
	sta	r1
	lda	r0
	clc
	adc	r8
	sta	r0
	lda	r1
	adc	r9
	sta	r1
	lda	r31
	sta	(r0),y
	inc	_pc
	bne	l836
	inc	1+_pc
l836:
	inc	_pc
	bne	l837
	inc	1+_pc
l837:
	lda	_code_index
	clc
	adc	#3
	sta	_code_index
	jmp	l503
l536:
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	ldy	#0
	lda	(r8),y
	ora	#2
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	and	#223
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	jmp	l407
l537:
	lda	_pc
	clc
	adc	#1
	sta	r0
	lda	1+_pc
	adc	#0
	sta	r1
	jsr	_read6502
	ldy	#2
	sta	(sp),y
	lda	r17
	cmp	#145
	bne	l539
	lda	#0
	sta	r8
	lda	_code_index
	ldx	#0
	clc
	adc	#46
	bcc	l838
	inx
l838:
	pha
	cmp	#211
	txa
	sbc	#0
	bvc	l840
	eor	#128
l840:
	bpl	l839
	pla
	lda	#>(_character_ram_updated)
	sta	r1
	lda	#<(_character_ram_updated)
	sta	r0
	ldy	#0
	sta	(sp),y
	lda	#>(_screen_ram_updated)
	sta	r1
	lda	#<(_screen_ram_updated)
	sta	r0
	sta	r31
	lda	r19
	sta	r1
	lda	r18
	sta	r0
	lda	_code_index
	sta	r2
	lda	r31
	ldy	#2
	lda	(sp),y
	sta	r4
	lda	r31
	sta	r6
	jsr	_emit_native_sta_indy
	sta	r8
l541:
	lda	r8
	beq	l543
	lda	_code_index
	clc
	adc	r8
	sta	_code_index
	inc	_pc
	bne	l841
	inc	1+_pc
l841:
	inc	_pc
	bne	l842
	inc	1+_pc
l842:
	jmp	l503
l543:
	lda	_code_index
	sta	r8
	lda	#0
	sta	r9
	lda	_sta_indy_template_size
	ldx	#0
	clc
	adc	r8
	pha
	txa
	adc	r9
	tax
	pla
	clc
	adc	#3
	bcc	l843
	inx
l843:
	pha
	cmp	#211
	txa
	sbc	#0
	bvc	l845
	eor	#128
l845:
	bpl	l844
	pla
	ldy	#2
	lda	(sp),y
	sta	_sta_indy_zp_patch
	lda	#0
	sta	r2
	lda	_sta_indy_template_size
	beq	l603
	lda	r19
	sta	r13
	lda	r18
	sta	r12
	lda	r2
	ldy	#2
	sta	(sp),y
	tax
l593:
	lda	r12
	clc
	adc	_code_index
	sta	r10
	lda	r13
	adc	#0
	sta	r11
	lda	0+_sta_indy_template,x ;am(x)
	ldy	#0
	sta	(r10),y
	inx
	inc	r12
	bne	l846
	inc	r13
l846:
	cpx	_sta_indy_template_size
	bcc	l593
l603:
	inc	_pc
	bne	l847
	inc	1+_pc
l847:
	inc	_pc
	bne	l848
	inc	1+_pc
l848:
	lda	_code_index
	clc
	adc	_sta_indy_template_size
	sta	_code_index
	jmp	l503
l546:
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	ldy	#0
	lda	(r8),y
	ora	#6
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	and	#223
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	jmp	l407
l539:
	ldy	#2
	lda	(sp),y
	sta	r8
	lda	#0
	sta	r9
	ldx	#>(_RAM_BASE)
	lda	#<(_RAM_BASE)
	clc
	adc	r8
	ldy	#4
	sta	(sp),y
	txa
	adc	r9
	iny
	sta	(sp),y
	lda	_code_index
	sta	r8
	lda	#0
	sta	r9
	lda	_addr_6502_indy_size
	ldx	#0
	clc
	adc	r8
	pha
	txa
	adc	r9
	tax
	pla
	clc
	adc	#3
	bcc	l849
	inx
l849:
	pha
	cmp	#211
	txa
	sbc	#0
	bvc	l851
	eor	#128
l851:
	bpl	l850
	pla
	lda	r17
	sta	_indy_opcode_location
	ldy	#5
	lda	(sp),y
	sta	1+_indy_address_lo
	dey
	lda	(sp),y
	sta	_indy_address_lo
	lda	(sp),y
	clc
	adc	#1
	sta	_indy_address_hi
	iny
	lda	(sp),y
	adc	#0
	sta	1+_indy_address_hi
	lda	#0
	sta	r0
	lda	_addr_6502_indy_size
	beq	l604
	lda	r19
	sta	r21
	lda	r18
	sta	r20
	lda	r0
	ldy	#2
	sta	(sp),y
	tax
l594:
	lda	r20
	clc
	adc	_code_index
	sta	r10
	lda	r21
	adc	#0
	sta	r11
	lda	0+_addr_6502_indy,x ;am(x)
	ldy	#0
	sta	(r10),y
	inx
	inc	r20
	bne	l852
	inc	r21
l852:
	cpx	_addr_6502_indy_size
	bcc	l594
l604:
	inc	_pc
	bne	l853
	inc	1+_pc
l853:
	inc	_pc
	bne	l854
	inc	1+_pc
l854:
	lda	_code_index
	clc
	adc	_addr_6502_indy_size
	sta	_code_index
	jmp	l503
l554:
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	ldy	#0
	lda	(r8),y
	ora	#6
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	and	#223
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	jmp	l407
l560:
	lda	_pc
	clc
	adc	#1
	sta	r0
	lda	1+_pc
	adc	#0
	sta	r1
	jsr	_read6502
	sta	r31
	lda	_code_index
	sta	r8
	lda	#0
	sta	r9
	lda	r18
	clc
	adc	#1
	sta	r0
	lda	r19
	adc	#0
	sta	r1
	lda	r0
	clc
	adc	r8
	sta	r0
	lda	r1
	adc	r9
	sta	r1
	lda	r31
	ldy	#0
	sta	(r0),y
	inc	_pc
	bne	l855
	inc	1+_pc
l855:
	inc	_pc
	bne	l856
	inc	1+_pc
l856:
	inc	_code_index
	inc	_code_index
	jmp	l503
l562:
	inc	_pc
	bne	l857
	inc	1+_pc
l857:
	inc	_code_index
	jmp	l503
l564:
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	ldy	#0
	lda	(r8),y
	ora	#2
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	and	#223
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
	jmp	l407
l503:
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	ldy	#0
	lda	(r8),y
	ora	#32
	sta	(r8),y
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r8
	lda	#>(_cache_flag)
	adc	#0
	sta	r9
	lda	(r8),y
l407:
	sta	r31
	ldy	#18
	jsr	___rload12
	clc
	lda	sp
	adc	#19
	sta	sp
	bcc	l858
	inc	sp+1
l858:
	lda	r31
	rts
l850:
	pla
	jmp	l554
l844:
	pla
	jmp	l546
l839:
	pla
	jmp	l541
l834:
	pla
	jmp	l503
l829:
	pla
	jmp	l503
l824:
	pla
	jmp	l489
l820:
	pla
	jmp	l485
l815:
	pla
	jmp	l478
l811:
	pla
	jmp	l468
l807:
	pla
	jmp	l460
l795:
	pla
	jmp	l456
l792:
	pla
	jmp	l454
l784:
	pla
	jmp	l445
l778:
	pla
	jmp	l447
l760:
	pla
	jmp	l431
l740:
	pla
	jmp	l425
; stacksize=0+??
;vcprmin=10000
	section	text
	global	_recompile_opcode
_recompile_opcode:
	lda	r16
	pha
	lda	r17
	pha
	lda	_mapper_prg_bank
	sta	r16
	lda	#2
	jsr	_bankswitch_prg
	jsr	l406
	sta	r17
	lda	r16
	jsr	_bankswitch_prg
	lda	_compile_ppu_active
	beq	l862
	lda	_compile_ppu_effect
	eor	#32
	sta	_compile_ppu_effect
	ora	#59
	sta	_lnPPUMASK
; volatile barrier
	lda	_lnPPUMASK
	sta	8193
l862:
	lda	r17
	sta	r31
	pla
	sta	r17
	pla
	sta	r16
	lda	r31
	rts
; stacksize=0+??
;vcprmin=10000
	section	"bank2"
l863:
	lda	r16
	pha
	lda	r17
	pha
	lda	r18
	pha
	lda	r1
	sta	r7
	lda	r0
	sta	r6
	and	#3
	sta	r0
	lda	#0
	sta	r1
	lda	#1
	ldy	r0
	beq	l868
l869:
	asl
	dey
	bne	l869
l868:
	sta	r31
	eor	#-1
	sta	r18
	lsr	r7
	ror	r6
	lsr	r7
	ror	r6
	lsr	r7
	ror	r6
	ldx	#>(_cache_bit_array)
	lda	#<(_cache_bit_array)
	clc
	adc	r6
	sta	r16
	txa
	adc	r7
	sta	r17
	lda	#3
	sta	r0
	lda	r17
	sta	r3
	lda	r16
	sta	r2
	jsr	_peek_bank_byte
	and	r18
	sta	r31
	lda	r17
	sta	r1
	lda	r16
	sta	r0
	lda	#3
	sta	r2
	lda	r31
	sta	r4
	jsr	_flash_byte_program
	pla
	sta	r18
	pla
	sta	r17
	pla
	sta	r16
	rts
; stacksize=0+??
;vcprmin=10000
	section	"bank2"
l870:
	lda	sp
	bne	l885
	dec	sp+1
l885:
	dec	sp
	lda	r16
	pha
	lda	r1
	sta	r5
	lda	r0
	sta	r4
	and	#3
	sta	r16
	lsr	r5
	ror	r4
	lsr	r5
	ror	r4
	lsr	r5
	ror	r4
	ldx	#>(_cache_bit_array)
	lda	#<(_cache_bit_array)
	clc
	adc	r4
	pha
	txa
	adc	r5
	tax
	pla
	sta	r31
	lda	#3
	sta	r0
	lda	r31
	sta	r2
	stx	r3
	jsr	_peek_bank_byte
	sta	r1
	lda	r16
	cmp	#7
	bcc	l874
	lda	r16
	cmp	#7
	bne	l882
l874:
	lda	r1
	and	#128
	ldy	#0
	sta	(sp),y
l882:
	ldy	#0
	lda	(sp),y
	sta	r31
	pla
	sta	r16
	inc	sp
	bne	l886
	inc	sp+1
l886:
	lda	r31
	rts
; stacksize=0+??
;vcprmin=10000
	section	text
	global	_cache_bit_enable
_cache_bit_enable:
	lda	r16
	pha
	lda	r18
	pha
	lda	r19
	pha
	lda	r1
	sta	r19
	lda	r0
	sta	r18
	lda	_mapper_prg_bank
	sta	r16
	lda	#2
	jsr	_bankswitch_prg
	lda	r19
	sta	r1
	lda	r18
	sta	r0
	jsr	l863
	lda	r16
	jsr	_bankswitch_prg
	pla
	sta	r19
	pla
	sta	r18
	pla
	sta	r16
	rts
; stacksize=0+??
;vcprmin=10000
	section	text
	global	_cache_bit_check
_cache_bit_check:
	lda	r16
	pha
	lda	r17
	pha
	lda	r18
	pha
	lda	r19
	pha
	lda	r1
	sta	r19
	lda	r0
	sta	r18
	lda	_mapper_prg_bank
	sta	r17
	lda	#2
	jsr	_bankswitch_prg
	lda	r19
	sta	r1
	lda	r18
	sta	r0
	jsr	l870
	sta	r16
	lda	r17
	jsr	_bankswitch_prg
	lda	r16
	sta	r31
	pla
	sta	r19
	pla
	sta	r18
	pla
	sta	r17
	pla
	sta	r16
	lda	r31
	rts
; stacksize=0+??
	global	_sa_compile_pass
	section	data
_sa_compile_pass:
	byte	0
	global	_sa_block_exit_pc
	section	data
_sa_block_exit_pc:
	word	65535
	global	_entry_list_offset
	section	data
_entry_list_offset:
	word	0
	global	_sa_block_alloc_size
	section	data
_sa_block_alloc_size:
	byte	0
	global	_addrmodes
	section	"data"
_addrmodes:
	byte	0
	byte	11
	byte	0
	byte	11
	byte	3
	byte	3
	byte	3
	byte	3
	byte	0
	byte	2
	byte	1
	byte	2
	byte	7
	byte	7
	byte	7
	byte	7
	byte	6
	byte	12
	byte	0
	byte	12
	byte	4
	byte	4
	byte	4
	byte	4
	byte	0
	byte	9
	byte	0
	byte	9
	byte	8
	byte	8
	byte	8
	byte	8
	byte	7
	byte	11
	byte	0
	byte	11
	byte	3
	byte	3
	byte	3
	byte	3
	byte	0
	byte	2
	byte	1
	byte	2
	byte	7
	byte	7
	byte	7
	byte	7
	byte	6
	byte	12
	byte	0
	byte	12
	byte	4
	byte	4
	byte	4
	byte	4
	byte	0
	byte	9
	byte	0
	byte	9
	byte	8
	byte	8
	byte	8
	byte	8
	byte	0
	byte	11
	byte	0
	byte	11
	byte	3
	byte	3
	byte	3
	byte	3
	byte	0
	byte	2
	byte	1
	byte	2
	byte	7
	byte	7
	byte	7
	byte	7
	byte	6
	byte	12
	byte	0
	byte	12
	byte	4
	byte	4
	byte	4
	byte	4
	byte	0
	byte	9
	byte	0
	byte	9
	byte	8
	byte	8
	byte	8
	byte	8
	byte	0
	byte	11
	byte	0
	byte	11
	byte	3
	byte	3
	byte	3
	byte	3
	byte	0
	byte	2
	byte	1
	byte	2
	byte	10
	byte	7
	byte	7
	byte	7
	byte	6
	byte	12
	byte	0
	byte	12
	byte	4
	byte	4
	byte	4
	byte	4
	byte	0
	byte	9
	byte	0
	byte	9
	byte	8
	byte	8
	byte	8
	byte	8
	byte	2
	byte	11
	byte	2
	byte	11
	byte	3
	byte	3
	byte	3
	byte	3
	byte	0
	byte	2
	byte	0
	byte	2
	byte	7
	byte	7
	byte	7
	byte	7
	byte	6
	byte	12
	byte	0
	byte	12
	byte	4
	byte	4
	byte	5
	byte	5
	byte	0
	byte	9
	byte	0
	byte	9
	byte	8
	byte	8
	byte	9
	byte	9
	byte	2
	byte	11
	byte	2
	byte	11
	byte	3
	byte	3
	byte	3
	byte	3
	byte	0
	byte	2
	byte	0
	byte	2
	byte	7
	byte	7
	byte	7
	byte	7
	byte	6
	byte	12
	byte	0
	byte	12
	byte	4
	byte	4
	byte	5
	byte	5
	byte	0
	byte	9
	byte	0
	byte	9
	byte	8
	byte	8
	byte	9
	byte	9
	byte	2
	byte	11
	byte	2
	byte	11
	byte	3
	byte	3
	byte	3
	byte	3
	byte	0
	byte	2
	byte	0
	byte	2
	byte	7
	byte	7
	byte	7
	byte	7
	byte	6
	byte	12
	byte	0
	byte	12
	byte	4
	byte	4
	byte	4
	byte	4
	byte	0
	byte	9
	byte	0
	byte	9
	byte	8
	byte	8
	byte	8
	byte	8
	byte	2
	byte	11
	byte	2
	byte	11
	byte	3
	byte	3
	byte	3
	byte	3
	byte	0
	byte	2
	byte	0
	byte	2
	byte	7
	byte	7
	byte	7
	byte	7
	byte	6
	byte	12
	byte	0
	byte	12
	byte	4
	byte	4
	byte	4
	byte	4
	byte	0
	byte	9
	byte	0
	byte	9
	byte	8
	byte	8
	byte	8
	byte	8
	global	_cache_hits
	zpage	_cache_hits
	section	zpage
_cache_hits:
	word	0,0
	global	_cache_misses
	zpage	_cache_misses
	section	zpage
_cache_misses:
	word	0,0
	global	_cache_interpret
	section	data
_cache_interpret:
	word	0,0
	global	_cache_branches
	section	data
_cache_branches:
	word	0,0
	global	_branch_not_compiled
	section	data
_branch_not_compiled:
	word	0,0
	global	_branch_wrong_bank
	section	data
_branch_wrong_bank:
	word	0,0
	global	_branch_out_of_range
	section	data
_branch_out_of_range:
	word	0,0
	global	_branch_forward
	section	data
_branch_forward:
	word	0,0
	global	_stats_frame
	section	data
_stats_frame:
	word	0
	global	_cache_index
	zpage	_cache_index
	section	zpage
_cache_index:
	byte	7
	global	_next_new_cache
	zpage	_next_new_cache
	section	zpage
_next_new_cache:
	byte	0
	global	_matched
	zpage	_matched
	section	zpage
_matched:
	byte	0
	global	_decimal_mode
	zpage	_decimal_mode
	section	zpage
_decimal_mode:
	byte	0
	global	_block_has_jsr
	zpage	_block_has_jsr
	section	zpage
_block_has_jsr:
	byte	0
	global	_compile_ppu_effect
	zpage	_compile_ppu_effect
	section	zpage
_compile_ppu_effect:
	byte	0
	global	_compile_ppu_active
	zpage	_compile_ppu_active
	section	zpage
_compile_ppu_active:
	byte	0
	global	_flash_enabled
	section	data
_flash_enabled:
	byte	0
	global	_next_free_sector
	section	data
_next_free_sector:
	byte	0
	global	_cache_branch_long
	section	data
_cache_branch_long:
	word	0,0
	global	_indy_hit_count
	zpage	_indy_hit_count
	section	zpage
_indy_hit_count:
	byte	0
	global	___mulint16
	global	_interpret_6502
	global	_RAM_BASE
	global	_SCREEN_RAM_BASE
	global	_CHARACTER_RAM_BASE
	global	_mapper_prg_bank
	global	_dispatch_on_pc
	global	_cross_bank_dispatch
	global	_xbank_trampoline
	global	_xbank_addr
	global	_addr_6502_indy
	global	_addr_6502_indy_size
	global	_indy_opcode_location
	global	_indy_address_lo
	global	_indy_address_hi
	global	_opcode_6502_pha_size
	global	_opcode_6502_pla_size
	global	_opcode_6502_php_size
	global	_opcode_6502_plp_size
	global	_opcode_6502_pha
	global	_opcode_6502_pla
	global	_opcode_6502_php
	global	_opcode_6502_plp
	global	_opcode_6502_jsr_size
	global	_opcode_6502_jsr
	global	_opcode_6502_jsr_ret_hi
	global	_opcode_6502_jsr_ret_lo
	global	_opcode_6502_jsr_tgt_lo
	global	_opcode_6502_jsr_tgt_hi
	global	_opcode_6502_njsr_size
	global	_opcode_6502_njsr
	global	_opcode_6502_njsr_ret_hi
	global	_opcode_6502_njsr_ret_lo
	global	_opcode_6502_njsr_tgt_lo
	global	_opcode_6502_njsr_tgt_hi
	global	_opcode_6502_nrts_size
	global	_opcode_6502_nrts
	global	_sta_indy_template
	global	_sta_indy_template_size
	global	_sta_indy_zp_patch
	global	_flash_cache_pc
	global	_flash_cache_pc_flags
	global	_reserve_result_addr
	section	bss
_reserve_result_addr:
	reserve	2
	global	_reserve_result_bank
	section	bss
_reserve_result_bank:
	reserve	1
	global	_mirrored_ptrs
	section	bss
_mirrored_ptrs:
	reserve	12
	global	_ROM_NAME
	global	_status
	zpage	_status
	global	_decoded_address
	zpage	_decoded_address
	section	zpage
_decoded_address:
	reserve	2
	global	_encoded_address
	zpage	_encoded_address
	section	zpage
_encoded_address:
	reserve	2
	global	_read6502
	global	_flash_byte_program
	global	_bankswitch_prg
	global	_sa_subroutine_lookup
	global	_opt2_record_pending_branch_safe
	global	_opt2_notify_block_compiled
	global	_opt2_sweep_pending_patches
	global	_opt2_scan_and_patch_epilogues
	global	_lnPPUMASK
	global	_sp
	zpage	_sp
	global	_a
	zpage	_a
	global	_pc
	zpage	_pc
	global	_pc_end
	zpage	_pc_end
	section	zpage
_pc_end:
	reserve	2
	global	_flash_cache_index
	zpage	_flash_cache_index
	section	zpage
_flash_cache_index:
	reserve	2
	global	_block_ci_map
	section	bss
_block_ci_map:
	reserve	64
	global	_sector_free_offset
	section	bss
_sector_free_offset:
	reserve	112
	global	_l1_cache_code
	section	bss
_l1_cache_code:
	reserve	256
	global	_cache_code
	section	bss
_cache_code:
	reserve	1688
	global	_cache_flag
	section	bss
_cache_flag:
	reserve	8
	global	_cache_entry_pc_lo
	zpage	_cache_entry_pc_lo
	section	zpage
_cache_entry_pc_lo:
	reserve	8
	global	_cache_entry_pc_hi
	zpage	_cache_entry_pc_hi
	section	zpage
_cache_entry_pc_hi:
	reserve	8
	global	_cache_exit_pc_lo
	section	bss
_cache_exit_pc_lo:
	reserve	8
	global	_cache_exit_pc_hi
	section	bss
_cache_exit_pc_hi:
	reserve	8
	global	_cache_cycles
	section	bss
_cache_cycles:
	reserve	16
	global	_cache_hit_count
	section	bss
_cache_hit_count:
	reserve	8
	global	_cache_branch_pc_lo
	section	bss
_cache_branch_pc_lo:
	reserve	8
	global	_cache_branch_pc_hi
	section	bss
_cache_branch_pc_hi:
	reserve	8
	global	_cache_vpc
	section	bss
_cache_vpc:
	reserve	8
	global	_code_index
	zpage	_code_index
	section	zpage
_code_index:
	reserve	1
	global	_address_8
	zpage	_address_8
	section	zpage
_address_8:
	reserve	1
	global	_pc_jump_address
	section	bss
_pc_jump_address:
	reserve	2
	global	_pc_jump_bank
	section	bss
_pc_jump_bank:
	reserve	1
	global	_pc_jump_flag_address
	section	bss
_pc_jump_flag_address:
	reserve	2
	global	_pc_jump_flag_bank
	section	bss
_pc_jump_flag_bank:
	reserve	1
	global	_flash_code_address
	section	bss
_flash_code_address:
	reserve	2
	global	_flash_code_bank
	section	bss
_flash_code_bank:
	reserve	1
	global	_cache_bit_array
	section	"bank3"
_cache_bit_array:
	reserve	8192
	global	_sector_free_offset
	global	_next_free_sector
	global	_native_sta_indy_tmpl
	global	_native_sta_indy_tmpl_size
	global	_native_sta_indy_emu_lo
	global	_native_sta_indy_emu_hi
	global	_zp_mirror_0
	zpage	_zp_mirror_0
	global	_zp_mirror_1
	zpage	_zp_mirror_1
	global	_zp_mirror_2
	zpage	_zp_mirror_2
	global	_peek_bank_byte
	global	_screen_ram_updated
	zpage	_screen_ram_updated
	global	_character_ram_updated
	zpage	_character_ram_updated
	section	data
l25:
	word	0
	section	data
l26:
	byte	0
	section	data
l27:
	byte	0
	section	data
l28:
	byte	0
	section	data
l29:
	byte	0
	section	data
l30:
	byte	0
	section	data
l335:
	byte	0
	zpage	sp
	zpage	r0
	zpage	r1
	zpage	r2
	zpage	r3
	zpage	r4
	zpage	r5
	zpage	r6
	zpage	r7
	zpage	r8
	zpage	r9
	zpage	r10
	zpage	r11
	zpage	r12
	zpage	r13
	zpage	r14
	zpage	r15
	zpage	r16
	zpage	r17
	zpage	r18
	zpage	r19
	zpage	r20
	zpage	r21
	zpage	r22
	zpage	r23
	zpage	r24
	zpage	r25
	zpage	r26
	zpage	r27
	zpage	r28
	zpage	r29
	zpage	r30
	zpage	r31
	zpage	btmp0
	zpage	btmp1
	zpage	btmp2
	zpage	btmp3
