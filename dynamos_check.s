;vcprmin=10000
	section	text
l1:
	lda	sp
	bne	l22
	dec	sp+1
l22:
	dec	sp
	lda	r1
	ldx	#0
	sta	r2
	cmp	#4
	bcs	l5
	ldx	#>(_RAM_BASE)
	lda	#<(_RAM_BASE)
	clc
	adc	r0
	pha
	txa
	adc	r1
	tax
	pla
	jmp	l2
l5:
	lda	r2
	cmp	#64
	bcs	l8
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
	bcc	l10
	bne	l23
	lda	r4
	cmp	#0
	bcc	l10
l23:
	lda	r5
	cmp	#192
	bcc	l24
	bne	l10
	lda	r4
	cmp	#0
	bcs	l10
l24:
	ldx	#0
	txa
	jmp	l2
l10:
	ldx	r5
	lda	r4
	jmp	l2
l8:
	lda	r2
	ldy	#0
	sta	(sp),y
	lda	r2
	cmp	#72
	bcs	l14
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
	jmp	l2
l14:
	lda	r2
	cmp	#80
	bcs	l17
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
	jmp	l2
l17:
	ldx	#0
	txa
l2:
	sta	r31
	inc	sp
	bne	l25
	inc	sp+1
l25:
	lda	r31
	rts
; stacksize=0+??
;vcprmin=10000
	section	"bank2"
l27:
	sec
	lda	sp
	sbc	#4
	sta	sp
	bcs	l36
	dec	sp+1
l36:
	lda	r16
	pha
	lda	r18
	pha
	lda	r19
	pha
; volatile barrier
	lda	3+_cache_hits
	sta	32259
	lda	2+_cache_hits
	sta	32258
	lda	1+_cache_hits
	sta	32257
	lda	_cache_hits
	sta	32256
; volatile barrier
	lda	3+_cache_misses
	sta	32263
	lda	2+_cache_misses
	sta	32262
	lda	1+_cache_misses
	sta	32261
	lda	_cache_misses
	sta	32260
; volatile barrier
	lda	3+_cache_interpret
	sta	32267
	lda	2+_cache_interpret
	sta	32266
	lda	1+_cache_interpret
	sta	32265
	lda	_cache_interpret
	sta	32264
; volatile barrier
	lda	3+_cache_branches
	sta	32271
	lda	2+_cache_branches
	sta	32270
	lda	1+_cache_branches
	sta	32269
	lda	_cache_branches
	sta	32268
; volatile barrier
	lda	3+_branch_not_compiled
	sta	32275
	lda	2+_branch_not_compiled
	sta	32274
	lda	1+_branch_not_compiled
	sta	32273
	lda	_branch_not_compiled
	sta	32272
; volatile barrier
	lda	3+_branch_wrong_bank
	sta	32279
	lda	2+_branch_wrong_bank
	sta	32278
	lda	1+_branch_wrong_bank
	sta	32277
	lda	_branch_wrong_bank
	sta	32276
; volatile barrier
	lda	3+_branch_out_of_range
	sta	32283
	lda	2+_branch_out_of_range
	sta	32282
	lda	1+_branch_out_of_range
	sta	32281
	lda	_branch_out_of_range
	sta	32280
; volatile barrier
	lda	3+_branch_forward
	sta	32287
	lda	2+_branch_forward
	sta	32286
	lda	1+_branch_forward
	sta	32285
	lda	_branch_forward
	sta	32284
	lda	#>(_opt2_stat_total)
	sta	r19
	lda	#<(_opt2_stat_total)
	sta	r18
	lda	#1
	sta	r0
	lda	r19
	sta	r3
	lda	r18
	sta	r2
	jsr	_peek_bank_byte
	sta	r16
	ldx	r19
	lda	r18
	clc
	adc	#1
	bcc	l37
	inx
l37:
	sta	r31
	lda	#1
	sta	r0
	lda	r31
	sta	r2
	stx	r3
	jsr	_peek_bank_byte
; volatile barrier
	sta	r31
	ldy	#0
	sta	(sp),y
	tya
	iny
	sta	(sp),y
; volatile barrier
	dey
	lda	(sp),y
	tax
	lda	#0
	sta	(sp),y
	txa
	iny
	sta	(sp),y
; volatile barrier
	lda	r16
	iny
	sta	(sp),y
	lda	#0
	iny
	sta	(sp),y
; volatile barrier
	dey
	lda	(sp),y
	ldy	#0
	ora	(sp),y
	sta	32288
	ldy	#3
	lda	(sp),y
	ldy	#1
	ora	(sp),y
	sta	32289
	lda	#>(_opt2_stat_direct)
	sta	r19
	lda	#<(_opt2_stat_direct)
	sta	r18
	tya
	sta	r0
	lda	r19
	sta	r3
	lda	r18
	sta	r2
	jsr	_peek_bank_byte
	sta	r16
	ldx	r19
	lda	r18
	clc
	adc	#1
	bcc	l38
	inx
l38:
	sta	r31
	lda	#1
	sta	r0
	lda	r31
	sta	r2
	stx	r3
	jsr	_peek_bank_byte
; volatile barrier
	sta	r31
	ldy	#2
	sta	(sp),y
	lda	#0
	iny
	sta	(sp),y
; volatile barrier
	dey
	lda	(sp),y
	tax
	lda	#0
	sta	(sp),y
	txa
	iny
	sta	(sp),y
; volatile barrier
	lda	r16
	ldy	#0
	sta	(sp),y
	tya
	iny
	sta	(sp),y
; volatile barrier
	dey
	lda	(sp),y
	ldy	#2
	ora	(sp),y
	sta	32290
	dey
	lda	(sp),y
	ldy	#3
	ora	(sp),y
	sta	32291
	lda	#>(_opt2_stat_pending)
	sta	r19
	lda	#<(_opt2_stat_pending)
	sta	r18
	lda	#1
	sta	r0
	lda	r19
	sta	r3
	lda	r18
	sta	r2
	jsr	_peek_bank_byte
	sta	r16
	ldx	r19
	lda	r18
	clc
	adc	#1
	bcc	l39
	inx
l39:
	sta	r31
	lda	#1
	sta	r0
	lda	r31
	sta	r2
	stx	r3
	jsr	_peek_bank_byte
; volatile barrier
	sta	r31
	ldy	#0
	sta	(sp),y
	tya
	iny
	sta	(sp),y
; volatile barrier
	dey
	lda	(sp),y
	tax
	lda	#0
	sta	(sp),y
	txa
	iny
	sta	(sp),y
; volatile barrier
	lda	r16
	iny
	sta	(sp),y
	lda	#0
	iny
	sta	(sp),y
; volatile barrier
	dey
	lda	(sp),y
	ldy	#0
	ora	(sp),y
	sta	32292
	ldy	#3
	lda	(sp),y
	ldy	#1
	ora	(sp),y
	sta	32293
; volatile barrier
	lda	1+_next_free_block
	sta	32295
	lda	_next_free_block
	sta	32294
; volatile barrier
	lda	1+_stats_frame
	sta	32297
	lda	_stats_frame
	sta	32296
	inc	_stats_frame
	bne	l40
	inc	1+_stats_frame
l40:
; volatile barrier
	lda	#219
	sta	32298
; volatile barrier
	lda	#87
	sta	32299
; volatile barrier
	lda	1+_sa_blocks_total
	sta	32301
	lda	_sa_blocks_total
	sta	32300
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
	bcc	l41
	inc	sp+1
l41:
	rts
; stacksize=0+??
;vcprmin=10000
	section	text
	global	_debug_stats_update
_debug_stats_update:
	lda	r16
	pha
	lda	_mapper_prg_bank
	sta	r16
	lda	#2
	jsr	_bankswitch_prg
	jsr	l27
	lda	r16
	jsr	_bankswitch_prg
	pla
	sta	r16
	rts
; stacksize=0+??
;vcprmin=10000
	section	text
	global	_run_6502
_run_6502:
	sec
	lda	sp
	sbc	#12
	sta	sp
	bcs	l110
	dec	sp+1
l110:
	ldy	#11
	jsr	___rsave12
	lda	1+_pc
	cmp	#43
	bne	l47
	lda	_pc
	cmp	#14
	bne	l47
	inc	_pc_2b27_count
l47:
	lda	_decimal_mode
	beq	l52
	lda	_status
	and	#8
	beq	l51
	lda	#0
	jsr	_bankswitch_prg
	jsr	_interpret_6502
	jmp	l84
l51:
	lda	#0
	sta	_decimal_mode
l52:
	jsr	_dispatch_on_pc
	tax
	beq	l55
	cpx	#1
	beq	l56
	cpx	#2
	bne	l56
	inc	_cache_interpret
	bne	l111
	inc	1+_cache_interpret
	bne	l111
	inc	2+_cache_interpret
	bne	l111
	inc	3+_cache_interpret
l111:
	lda	#0
	jsr	_bankswitch_prg
	jsr	_interpret_6502
	jmp	l84
l55:
	inc	_cache_hits
	bne	l112
	inc	1+_cache_hits
	bne	l112
	inc	2+_cache_hits
	bne	l112
	inc	3+_cache_hits
l112:
	jmp	l84
l56:
	inc	_cache_misses
	bne	l113
	inc	1+_cache_misses
	bne	l113
	inc	2+_cache_misses
	bne	l113
	inc	3+_cache_misses
l113:
	jsr	_flash_cache_select
	sta	_flash_cache_index
	stx	1+_flash_cache_index
	lda	_flash_cache_index
	ora	1+_flash_cache_index
	beq	l58
	lda	_flash_cache_index
	bne	l114
	dec	1+_flash_cache_index
l114:
	dec	_flash_cache_index
	jmp	l59
l58:
	lda	#0
	jsr	_bankswitch_prg
	jsr	_interpret_6502
	jmp	l84
l59:
	lda	1+_pc
	sta	r27
	lda	_pc
	sta	r26
	lda	#0
	sta	_cache_index
	sta	_code_index
	sta	_cache_flag
	lda	1+_pc
	sta	r1
	lda	_pc
	sta	r0
	lda	1+_flash_cache_index
	sta	r3
	lda	_flash_cache_index
	sta	r2
	jsr	_setup_flash_address
l97:
	lda	1+_pc
	sta	r21
	lda	_pc
	sta	r20
	lda	_code_index
	sta	r16
	jsr	_recompile_opcode
	lda	_code_index
	sec
	sbc	r16
	sta	r18
	lda	r21
	sta	r1
	lda	r20
	sta	r0
	lda	1+_flash_cache_index
	sta	r3
	lda	_flash_cache_index
	sta	r2
	jsr	_setup_flash_address
	lda	#0
	sta	r17
	lda	r18
	beq	l101
	lda	r16
	sta	r22
	lda	#0
	sta	r23
	lda	r16
	sta	r19
l98:
	ldy	r19
	lda	0+_cache_code,y ;am(r19)
	sta	r31
	lda	_flash_code_address
	clc
	adc	r22
	sta	r6
	lda	1+_flash_code_address
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
	bcc	l98
l101:
	lda	_code_index
	cmp	#224
	bcc	l68
	lda	_cache_flag
	ora	#4
	sta	_cache_flag
	and	#223
	sta	_cache_flag
l68:
	lda	_cache_flag
	and	#2
	beq	l70
	lda	r16
	sta	r0
	lda	#64
	sta	r2
	jsr	_flash_cache_pc_update
	jmp	l73
l70:
	lda	r18
	bne	l72
	lda	1+_pc
	cmp	r21
	bne	l115
	lda	_pc
	cmp	r20
	beq	l73
l115:
l72:
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
l73:
	lda	_cache_flag
	and	#32
	bne	l97
	lda	_code_index
	beq	l76
	lda	1+_pc
	sta	r17
	lda	_pc
	sta	r16
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
	sta	r21
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
	lda	r16
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
	lda	#133
	sta	(r0),y
	lda	#>(_pc)
	sta	r1
	lda	#<(_pc)
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
	lda	r31
	sta	r30
	lda	r17
	ldx	#0
	sta	r2
	stx	r3
	lda	r30
	sta	r31
	lda	r2
	sta	r1
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r2
	lda	#>(_cache_code)
	adc	#0
	sta	r3
	lda	r31
	inc	_code_index
	sta	r31
	lda	r1
	sta	(r2),y
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
	lda	r31
	clc
	adc	#1
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
	lda	#76
	sta	(r0),y
	ldx	#>(_cross_bank_dispatch)
	lda	#<(_cross_bank_dispatch)
	sta	r1
	sta	r31
	lda	#<(_cache_code)
	clc
	adc	_code_index
	sta	r2
	lda	#>(_cache_code)
	adc	#0
	sta	r3
	lda	r31
	inc	_code_index
	sta	r31
	lda	r1
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
	lda	r21
	sta	r18
	lda	r21
	cmp	_code_index
	bcs	l102
l99:
	ldy	r18
	lda	0+_cache_code,y ;am(r18)
	sta	r31
	lda	r18
	sta	r4
	lda	#0
	sta	r5
	lda	r4
	clc
	adc	_flash_code_address
	sta	r0
	lda	r5
	adc	1+_flash_code_address
	sta	r1
	lda	_flash_code_bank
	sta	r2
	lda	r31
	sta	r4
	jsr	_flash_byte_program
	inc	r18
	lda	r18
	cmp	_code_index
	bcc	l99
l102:
	lda	_flash_code_address
	clc
	adc	#255
	sta	r0
	lda	1+_flash_code_address
	adc	#0
	sta	r1
	lda	_flash_code_bank
	sta	r2
	lda	r21
	sta	r4
	jsr	_flash_byte_program
	lda	#3
	jsr	_bankswitch_prg
	lda	#<(_flash_block_flags)
	clc
	adc	_flash_cache_index
	sta	r0
	lda	#>(_flash_block_flags)
	adc	1+_flash_cache_index
	sta	r1
	ldy	#0
	lda	(r0),y
	and	#254
	sta	r31
	lda	#>(_flash_block_flags)
	sta	r5
	lda	#<(_flash_block_flags)
	sta	r4
	clc
	adc	_flash_cache_index
	sta	r0
	lda	r5
	adc	1+_flash_cache_index
	sta	r1
	lda	_mapper_prg_bank
	sta	r2
	lda	r31
	sta	r4
	jsr	_flash_byte_program
	inc	l81
	lda	l81
	cmp	#8
	bcc	l83
	lda	#0
	sta	l81
	jsr	_opt2_sweep_pending_patches
	jsr	_opt2_scan_and_patch_epilogues
l83:
	lda	r27
	sta	1+_pc
	lda	r26
	sta	_pc
	jsr	_dispatch_on_pc
	jmp	l84
l76:
	lda	#0
	jsr	_bankswitch_prg
	jsr	_interpret_6502
l84:
	ldy	#11
	jsr	___rload12
	clc
	lda	sp
	adc	#12
	sta	sp
	bcc	l116
	inc	sp+1
l116:
	rts
; stacksize=0+??
	section	data
l81:
	byte	0
;vcprmin=10000
	section	"bank2"
l117:
	lda	r16
	pha
	lda	r17
	pha
	lda	r18
	pha
	lda	r20
	pha
	lda	r21
	pha
	lda	r1
	sta	r21
	lda	r0
	sta	r20
	lda	r21
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
	lda	r20
	sta	_pc_jump_flag_address
	lda	r21
	and	#63
	sta	1+_pc_jump_flag_address
	lda	#<(_flash_cache_pc_flags)
	clc
	adc	_pc_jump_flag_address
	pha
	lda	#>(_flash_cache_pc_flags)
	adc	1+_pc_jump_flag_address
	tax
	pla
	sta	r31
	lda	_pc_jump_flag_bank
	sta	r0
	lda	r31
	sta	r2
	stx	r3
	jsr	_peek_bank_byte
	and	#128
	beq	l121
	lda	#0
	jmp	l118
l121:
	ldx	r21
	lda	r20
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
	lda	r21
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
; volatile barrier
	lda	#37
	sta	16416
	lda	#<(_flash_cache_pc)
	clc
	adc	_pc_jump_address
	pha
	lda	#>(_flash_cache_pc)
	adc	1+_pc_jump_address
	tax
	pla
	sta	r31
	lda	_pc_jump_bank
	sta	r0
	lda	r31
	sta	r2
	stx	r3
	jsr	_peek_bank_byte
	sta	r18
	lda	#<(1+_flash_cache_pc)
	clc
	adc	_pc_jump_address
	pha
	lda	#>(1+_flash_cache_pc)
	adc	1+_pc_jump_address
	tax
	pla
	sta	r31
	lda	_pc_jump_bank
	sta	r0
	lda	r31
	sta	r2
	stx	r3
	jsr	_peek_bank_byte
	sta	r0
	ldx	#0
	sta	r30
	stx	r29
	tax
	lda	#0
	sta	r16
	stx	r17
	ldx	r29
	lda	r18
	ldx	#0
	ora	r16
	sta	r16
	txa
	ora	r17
	sta	r17
	jsr	l123
l118:
	sta	r31
	pla
	sta	r21
	pla
	sta	r20
	pla
	sta	r18
	pla
	sta	r17
	pla
	sta	r16
	lda	r31
	rts
l123:
	jmp	(r16)
; stacksize=0+??
;vcprmin=10000
	section	text
	global	_flash_cache_search
_flash_cache_search:
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
	jsr	l117
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
;vcprmin=10000
	section	text
	global	_flash_cache_select
_flash_cache_select:
	lda	#3
	jsr	_bankswitch_prg
	lda	1+_next_free_block
	sta	r5
	lda	_next_free_block
	sta	r4
	lda	1+_next_free_block
	cmp	#3
	bcc	l144
	bne	l138
	lda	_next_free_block
	cmp	#192
	bcs	l138
l144:
	lda	_next_free_block
	clc
	adc	#1
	sta	r2
	lda	1+_next_free_block
	adc	#0
	sta	r3
	lda	#<(_flash_block_flags)
	clc
	adc	_next_free_block
	sta	r0
	lda	#>(_flash_block_flags)
	adc	1+_next_free_block
	sta	r1
l137:
	ldy	#0
	lda	(r0),y
	and	#1
	beq	l133
	lda	r3
	sta	1+_next_free_block
	lda	r2
	sta	_next_free_block
	ldx	r3
	lda	r2
	rts
l133:
	inc	r4
	bne	l145
	inc	r5
l145:
	inc	r0
	bne	l146
	inc	r1
l146:
	inc	r2
	bne	l147
	inc	r3
l147:
	lda	r5
	cmp	#3
	bcc	l137
	bne	l148
	lda	r4
	cmp	#192
	bcc	l137
l148:
l138:
	ldx	#0
	txa
l126:
	rts
; stacksize=0+??
;vcprmin=10000
	section	text
	global	_reserve_block_for_pc
_reserve_block_for_pc:
	sec
	lda	sp
	sbc	#7
	sta	sp
	bcs	l174
	dec	sp+1
l174:
	ldy	#6
	jsr	___rsave6
	lda	r1
	sta	r19
	lda	r0
	sta	r18
	ldx	#0
	lda	_reservation_count
	beq	l167
	lda	#0
	sta	r0
l166:
	txa
	ldy	#0
	sta	(sp),y
	ldy	r0
	lda	1+_reserved_pc,y ;am(r0)
	cmp	r19
	bne	l156
	ldy	r0
	lda	0+_reserved_pc,y ;am(r0)
	cmp	r18
	bne	l156
	ldy	r0
	lda	1+_reserved_block,y ;am(r0)
	tax
	ldy	r0
	lda	0+_reserved_block,y ;am(r0)
	clc
	adc	#1
	bcc	l175
	inx
l175:
	jmp	l149
l156:
	inx
	inc	r0
	inc	r0
	cpx	_reservation_count
	bcc	l166
l167:
	ldx	r19
	lda	r18
	stx	r31
	lsr	r31
	ror
	lsr	r31
	ror
	lsr	r31
	ror
	ldx	r31
	sta	r2
	stx	r3
	lda	r18
	and	#7
	ldx	#0
	sta	r14
	stx	r15
	lda	#1
	ldy	r14
	beq	l176
l177:
	asl
	dey
	bne	l177
l176:
	sta	r16
	ldx	#>(_sa_code_bitmap)
	lda	#<(_sa_code_bitmap)
	clc
	adc	r2
	pha
	txa
	adc	r3
	tax
	pla
	sta	r31
	lda	#3
	sta	r0
	lda	r31
	sta	r2
	stx	r3
	jsr	_peek_bank_byte
	sta	r31
	sta	r2
	lda	#0
	sta	r3
	lda	r16
	ldx	#0
	and	r2
	pha
	txa
	and	r3
	tax
	pla
	cpx	#0
	bne	l178
	cmp	#0
	beq	l158
l178:
	ldx	#0
	txa
	jmp	l149
l158:
	lda	_reservation_count
	cmp	#32
	bcc	l160
	ldx	#0
	txa
	jmp	l149
l160:
	jsr	_flash_cache_select
	sta	r20
	stx	r21
	lda	r20
	ora	r21
	bne	l162
	ldx	#0
	txa
	jmp	l149
l162:
	lda	r20
	sec
	sbc	#1
	sta	r16
	lda	r21
	sbc	#0
	sta	r17
	lda	#3
	jsr	_bankswitch_prg
	lda	#<(_flash_block_flags)
	clc
	adc	r16
	sta	r0
	lda	#>(_flash_block_flags)
	adc	r17
	sta	r1
	ldy	#0
	lda	(r0),y
	and	#254
	sta	r31
	lda	#>(_flash_block_flags)
	sta	r5
	lda	#<(_flash_block_flags)
	sta	r4
	clc
	adc	r16
	sta	r0
	lda	r5
	adc	r17
	sta	r1
	lda	_mapper_prg_bank
	sta	r2
	lda	r31
	sta	r4
	jsr	_flash_byte_program
	lda	_reservation_count
	asl
	sta	r31
	lda	r19
	ldy	r31
	sta	1+_reserved_pc,y ;am(r31)
	lda	r18
	ldy	r31
	sta	0+_reserved_pc,y ;am(r31)
	lda	r17
	ldy	r31
	sta	1+_reserved_block,y ;am(r31)
	lda	r16
	ldy	r31
	sta	0+_reserved_block,y ;am(r31)
	inc	_reservation_count
	ldx	r21
	lda	r20
l149:
	sta	r31
	ldy	#6
	jsr	___rload6
	clc
	lda	sp
	adc	#7
	sta	sp
	bcc	l179
	inc	sp+1
l179:
	lda	r31
	rts
; stacksize=0+??
;vcprmin=10000
	section	text
	global	_consume_reservation
_consume_reservation:
	sec
	lda	sp
	sbc	#3
	sta	sp
	bcs	l197
	dec	sp+1
l197:
	lda	r1
	sta	r5
	lda	r0
	sta	r4
	ldx	#0
	lda	_reservation_count
	beq	l194
	lda	_reservation_count
	sec
	sbc	#1
	sta	r1
	asl
	sta	r31
	lda	#<(_reserved_pc)
	clc
	adc	r31
	sta	r8
	lda	#>(_reserved_pc)
	adc	#0
	sta	r9
	lda	#<(_reserved_block)
	clc
	adc	r31
	sta	r6
	lda	#>(_reserved_block)
	adc	#0
	sta	r7
	lda	#0
	sta	r0
l193:
	txa
	ldy	#0
	sta	(sp),y
	lda	r5
	ldy	#2
	sta	(sp),y
	lda	r4
	dey
	sta	(sp),y
	ldy	r0
	lda	1+_reserved_pc,y ;am(r0)
	cmp	r5
	bne	l187
	ldy	r0
	lda	0+_reserved_pc,y ;am(r0)
	cmp	r4
	bne	l187
	lda	#<(_reserved_block)
	clc
	adc	r0
	sta	r2
	lda	#>(_reserved_block)
	adc	#0
	sta	r3
	ldy	#1
	lda	(r2),y
	tax
	dey
	lda	(r2),y
	sta	r31
	lda	r1
	sta	_reservation_count
	lda	r31
	iny
	lda	(r8),y
	ldy	r0
	sta	1+_reserved_pc,y ;am(r0)
	ldy	#0
	lda	(r8),y
	ldy	r0
	sta	0+_reserved_pc,y ;am(r0)
	lda	r31
	ldy	#1
	lda	(r6),y
	sta	(r2),y
	dey
	lda	(r6),y
	sta	(r2),y
	lda	r31
	jmp	l180
l187:
	inx
	inc	r0
	inc	r0
	cpx	_reservation_count
	bcc	l193
l194:
	ldx	#255
	txa
l180:
	sta	r31
	clc
	lda	sp
	adc	#3
	sta	sp
	bcc	l198
	inc	sp+1
l198:
	lda	r31
	rts
; stacksize=0+??
;vcprmin=10000
	section	text
	global	_reserve_block_for_pc_safe
_reserve_block_for_pc_safe:
	lda	r16
	pha
	lda	r1
	sta	r3
	lda	r0
	sta	r2
	lda	_reservations_enabled
	bne	l202
	lda	#0
	jmp	l199
l202:
	lda	_reservation_count
	beq	l204
	lda	#0
	jmp	l199
l204:
	lda	_mapper_prg_bank
	sta	r16
	lda	r3
	sta	r1
	lda	r2
	sta	r0
	jsr	_reserve_block_for_pc
	sta	r4
	stx	r5
	lda	r4
	ora	r5
	bne	l206
	lda	r16
	jsr	_bankswitch_prg
	lda	#0
	jmp	l199
l206:
	ldx	r5
	lda	r4
	sec
	sbc	#1
	bcs	l208
	dex
l208:
	sta	r30
	stx	r29
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
	ldx	r29
	lda	r30
	sta	r31
	lda	r2
	sta	r0
	clc
	adc	#4
	sta	_reserve_result_bank
	lda	r31
	and	#63
	tax
	lda	#0
	clc
	adc	#0
	sta	_reserve_result_addr
	txa
	adc	#128
	sta	1+_reserve_result_addr
	lda	r16
	jsr	_bankswitch_prg
	lda	#1
l199:
	sta	r31
	pla
	sta	r16
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
	sta	r18
	txa
	adc	1+_flash_code_address
	sta	r19
	lda	_mapper_prg_bank
	sta	r16
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
	sta	r17
	lda	r16
	jsr	_bankswitch_prg
	lda	r17
	cmp	#255
	beq	l212
	lda	r17
	ldx	#0
	and	#128
	cpx	#0
	bne	l218
	cmp	#0
	beq	l209
l218:
l212:
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
	bne	l215
	lda	_flash_code_bank
	sta	r3
	jmp	l216
l215:
	lda	#128
	sta	r3
l216:
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
l209:
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
	global	_setup_flash_address
_setup_flash_address:
	lda	r1
	sta	r7
	lda	r0
	sta	r6
	ldx	r3
	lda	r2
	sta	r30
	stx	r29
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
	sta	r4
	stx	r5
	ldx	r29
	lda	r30
	sta	r31
	lda	r4
	sta	r0
	clc
	adc	#4
	sta	_flash_code_bank
	lda	r31
	and	#63
	tax
	lda	#0
	clc
	adc	#0
	sta	_flash_code_address
	txa
	adc	#128
	sta	1+_flash_code_address
	ldx	r7
	lda	r6
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
	lda	r7
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
	sta	r4
	stx	r5
	lda	r4
	clc
	adc	#19
	sta	_pc_jump_bank
	ldx	btmp0+1
	lda	btmp0
	sta	_pc_jump_address
	txa
	and	#63
	sta	1+_pc_jump_address
	lda	r7
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
	sta	r4
	stx	r5
	lda	r4
	clc
	adc	#27
	sta	_pc_jump_flag_bank
	lda	r6
	sta	_pc_jump_flag_address
	lda	r7
	and	#63
	sta	1+_pc_jump_flag_address
	rts
; stacksize=0+??
;vcprmin=10000
	section	text
l221:
	lda	r0
	eor	#32
	rts
; stacksize=0+??
;vcprmin=10000
	section	"bank2"
l224:
	sec
	lda	sp
	sbc	#19
	sta	sp
	bcs	l539
	dec	sp+1
l539:
	ldy	#18
	jsr	___rsave12
	lda	_cache_index
	ldx	#0
	sta	r0
	stx	r1
	txa
	sta	r3
	lda	#229
	sta	r2
	jsr	___mulint16
	clc
	adc	#<(_cache_code)
	sta	r20
	txa
	adc	#>(_cache_code)
	sta	r21
	lda	1+_pc
	sta	r1
	lda	_pc
	sta	r0
	jsr	_read6502
	sta	r22
	cmp	#0
	beq	l323
	lda	r22
	cmp	#8
	beq	l314
	lda	r22
	cmp	#16
	beq	l228
	lda	r22
	cmp	#32
	beq	l266
	lda	r22
	cmp	#40
	beq	l314
	lda	r22
	cmp	#48
	beq	l228
	lda	r22
	cmp	#64
	beq	l282
	lda	r22
	cmp	#72
	beq	l298
	lda	r22
	cmp	#76
	beq	l259
	lda	r22
	cmp	#80
	beq	l228
	lda	r22
	cmp	#88
	beq	l314
	lda	r22
	cmp	#96
	beq	l284
	lda	r22
	cmp	#104
	beq	l306
	lda	r22
	cmp	#108
	beq	l282
	lda	r22
	cmp	#112
	beq	l228
	lda	r22
	cmp	#120
	beq	l314
	lda	r22
	cmp	#144
	beq	l228
	lda	r22
	cmp	#148
	beq	l319
	lda	r22
	cmp	#150
	beq	l319
	lda	r22
	cmp	#154
	beq	l294
	lda	r22
	cmp	#176
	beq	l228
	lda	r22
	cmp	#186
	beq	l291
	lda	r22
	cmp	#208
	beq	l228
	lda	r22
	cmp	#234
	beq	l322
	lda	r22
	cmp	#240
	beq	l228
	lda	r22
	cmp	#248
	beq	l321
	jmp	l324
l228:
	lda	_pc
	clc
	adc	#1
	sta	r0
	lda	1+_pc
	adc	#0
	sta	r1
	jsr	_read6502
	sta	r9
	sec
	sbc	#0
	bvc	l540
	eor	#128
l540:
	bmi	l237
	lda	_pc
	clc
	adc	#2
	sta	r4
	lda	1+_pc
	adc	#0
	sta	r5
	ldx	#0
	lda	r9
	bpl	l541
	dex
l541:
	clc
	adc	r4
	ldy	#1
	sta	(sp),y
	txa
	adc	r5
	iny
	sta	(sp),y
	inc	_branch_forward
	bne	l542
	inc	1+_branch_forward
	bne	l542
	inc	2+_branch_forward
	bne	l542
	inc	3+_branch_forward
l542:
	lda	_code_index
	ldx	#0
	clc
	adc	#32
	bcc	l543
	inx
l543:
	pha
	cmp	#229
	txa
	sbc	#0
	bvc	l545
	eor	#128
l545:
	bpl	l544
	pla
	ldy	#2
	lda	(sp),y
	sta	r1
	dey
	lda	(sp),y
	sta	r0
	jsr	_reserve_block_for_pc_safe
	cmp	#0
	beq	l239
	lda	_reserve_result_bank
	cmp	_flash_code_bank
	bne	l239
	lda	r22
	sta	r0
	jsr	l221
	sta	r31
	lda	r20
	clc
	adc	_code_index
	sta	r4
	lda	r21
	adc	#0
	sta	r5
	lda	r31
	ldy	#0
	sta	(r4),y
	lda	_code_index
	sta	r4
	tya
	sta	r5
	ldx	r21
	lda	r20
	clc
	adc	#1
	bcc	l546
	inx
l546:
	clc
	adc	r4
	sta	r4
	txa
	adc	r5
	sta	r5
	lda	#3
	sta	(r4),y
	lda	_code_index
	sta	r4
	lda	#0
	sta	r5
	ldx	r21
	lda	r20
	clc
	adc	#2
	bcc	l547
	inx
l547:
	clc
	adc	r4
	sta	r4
	txa
	adc	r5
	sta	r5
	lda	#76
	sta	(r4),y
	lda	_reserve_result_addr
	and	#255
	sta	r31
	lda	_code_index
	sta	r8
	lda	#0
	sta	r9
	lda	r20
	clc
	adc	#3
	sta	r4
	lda	r21
	adc	#0
	sta	r5
	lda	r4
	clc
	adc	r8
	sta	r4
	lda	r5
	adc	r9
	sta	r5
	lda	r31
	sta	(r4),y
	lda	1+_reserve_result_addr
	ldx	#0
	sta	r4
	stx	r5
	lda	r4
	and	#255
	sta	r31
	lda	_code_index
	sta	r8
	txa
	sta	r9
	lda	r20
	clc
	adc	#4
	sta	r4
	lda	r21
	adc	#0
	sta	r5
	lda	r4
	clc
	adc	r8
	sta	r4
	lda	r5
	adc	r9
	sta	r5
	lda	r31
	sta	(r4),y
	inc	_pc
	bne	l548
	inc	1+_pc
l548:
	inc	_pc
	bne	l549
	inc	1+_pc
l549:
	lda	_code_index
	clc
	adc	#5
	sta	_code_index
	inc	_cache_branches
	bne	l550
	inc	1+_cache_branches
	bne	l550
	inc	2+_cache_branches
	bne	l550
	inc	3+_cache_branches
l550:
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r4
	lda	#>(_cache_flag)
	adc	#0
	sta	r5
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
	jmp	l225
l239:
	lda	1+_flash_code_address
	ldy	#4
	sta	(sp),y
	lda	_flash_code_address
	dey
	sta	(sp),y
	lda	_flash_code_bank
	ldy	#5
	sta	(sp),y
	lda	_code_index
	iny
	sta	(sp),y
	lda	_code_index
	ldx	#0
	clc
	adc	#35
	bcc	l551
	inx
l551:
	pha
	cmp	#229
	txa
	sbc	#0
	bvc	l553
	eor	#128
l553:
	bmi	l552
	pla
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r4
	lda	#>(_cache_flag)
	adc	#0
	sta	r5
	ldy	#0
	lda	(r4),y
	ora	#2
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
	jmp	l225
l243:
	lda	r22
	sta	r0
	jsr	l221
	sta	r31
	lda	r20
	clc
	adc	_code_index
	sta	r0
	lda	r21
	adc	#0
	sta	r1
	lda	r31
	ldy	#0
	sta	(r0),y
	lda	_code_index
	sta	r0
	tya
	sta	r1
	ldx	r21
	lda	r20
	clc
	adc	#1
	bcc	l554
	inx
l554:
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
	ldx	r21
	lda	r20
	clc
	adc	#2
	bcc	l555
	inx
l555:
	clc
	adc	r0
	sta	r0
	txa
	adc	r1
	sta	r1
	lda	r22
	sta	(r0),y
	lda	_code_index
	sta	r0
	lda	#0
	sta	r1
	ldx	r21
	lda	r20
	clc
	adc	#3
	bcc	l556
	inx
l556:
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
	ldx	r21
	lda	r20
	clc
	adc	#4
	bcc	l557
	inx
l557:
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
	ldx	r21
	lda	r20
	clc
	adc	#5
	bcc	l558
	inx
l558:
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
	ldx	r21
	lda	r20
	clc
	adc	#6
	bcc	l559
	inx
l559:
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
	ldx	r21
	lda	r20
	clc
	adc	#7
	bcc	l560
	inx
l560:
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
	lda	r20
	clc
	adc	#8
	sta	r0
	lda	r21
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
	ldx	r21
	lda	r20
	clc
	adc	#9
	bcc	l561
	inx
l561:
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
	ldx	r21
	lda	r20
	clc
	adc	#10
	bcc	l562
	inx
l562:
	clc
	adc	r0
	sta	r0
	txa
	adc	r1
	sta	r1
	lda	#169
	sta	(r0),y
	iny
	lda	(sp),y
	and	#255
	sta	r31
	lda	_code_index
	sta	r2
	lda	#0
	sta	r3
	lda	r20
	clc
	adc	#11
	sta	r0
	lda	r21
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
	dey
	sta	(r0),y
	lda	_code_index
	sta	r0
	lda	#0
	sta	r1
	ldx	r21
	lda	r20
	clc
	adc	#12
	bcc	l563
	inx
l563:
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
	lda	r20
	clc
	adc	#13
	sta	r0
	lda	r21
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
	lda	r20
	clc
	adc	#14
	sta	r0
	lda	r21
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
	ldy	#2
	lda	(sp),y
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
	sta	r4
	txa
	sta	r5
	lda	r20
	clc
	adc	#15
	sta	r2
	lda	r21
	adc	#0
	sta	r3
	lda	r2
	clc
	adc	r4
	sta	r2
	lda	r3
	adc	r5
	sta	r3
	lda	r0
	ldy	#0
	sta	(r2),y
	lda	_code_index
	sta	r2
	txa
	sta	r3
	lda	r20
	clc
	adc	#16
	sta	r0
	lda	r21
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
	lda	r20
	clc
	adc	#17
	sta	r0
	lda	r21
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
	ldx	r21
	lda	r20
	clc
	adc	#18
	bcc	l564
	inx
l564:
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
	sta	r4
	lda	#0
	sta	r5
	lda	r20
	clc
	adc	#19
	sta	r2
	lda	r21
	adc	#0
	sta	r3
	lda	r2
	clc
	adc	r4
	sta	r2
	lda	r3
	adc	r5
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
	lda	r20
	clc
	adc	#20
	sta	r0
	lda	r21
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
	ldy	#6
	lda	(sp),y
	ldx	#0
	clc
	ldy	#3
	adc	(sp),y
	pha
	txa
	iny
	adc	(sp),y
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
	bcc	l565
	inx
l565:
	sta	r31
	lda	#0
	tay
	sta	(sp),y
	lda	r31
	sta	r2
	stx	r3
	ldy	#5
	lda	(sp),y
	sta	r4
	ldy	#2
	lda	(sp),y
	sta	r7
	dey
	lda	(sp),y
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
	bne	l566
	inc	1+_pc
l566:
	inc	_pc
	bne	l567
	inc	1+_pc
l567:
	lda	_code_index
	clc
	adc	#21
	sta	_code_index
	inc	_cache_branches
	bne	l568
	inc	1+_cache_branches
	bne	l568
	inc	2+_cache_branches
	bne	l568
	inc	3+_cache_branches
l568:
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
	jmp	l225
l237:
	lda	_pc
	clc
	adc	#2
	sta	r2
	lda	1+_pc
	adc	#0
	sta	r3
	ldx	#0
	lda	r9
	bpl	l569
	dex
l569:
	clc
	adc	r2
	ldy	#1
	sta	(sp),y
	txa
	adc	r3
	iny
	sta	(sp),y
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
	dey
	lda	(sp),y
	pha
	iny
	lda	(sp),y
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
	sta	r4
	and	#128
	sta	r31
	lda	r4
	ldy	#3
	sta	(sp),y
	lda	r31
	beq	l247
	inc	_branch_not_compiled
	bne	l570
	inc	1+_branch_not_compiled
	bne	l570
	inc	2+_branch_not_compiled
	bne	l570
	inc	3+_branch_not_compiled
l570:
	lda	1+_flash_code_address
	ldy	#4
	sta	(sp),y
	lda	_flash_code_address
	dey
	sta	(sp),y
	lda	_flash_code_bank
	ldy	#5
	sta	(sp),y
	lda	_code_index
	iny
	sta	(sp),y
	lda	_code_index
	ldx	#0
	clc
	adc	#35
	bcc	l571
	inx
l571:
	pha
	cmp	#229
	txa
	sbc	#0
	bvc	l573
	eor	#128
l573:
	bmi	l572
	pla
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r4
	lda	#>(_cache_flag)
	adc	#0
	sta	r5
	ldy	#0
	lda	(r4),y
	ora	#2
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
	jmp	l225
l249:
	lda	r22
	sta	r0
	jsr	l221
	sta	r31
	lda	r20
	clc
	adc	_code_index
	sta	r0
	lda	r21
	adc	#0
	sta	r1
	lda	r31
	ldy	#0
	sta	(r0),y
	lda	_code_index
	sta	r0
	tya
	sta	r1
	ldx	r21
	lda	r20
	clc
	adc	#1
	bcc	l574
	inx
l574:
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
	ldx	r21
	lda	r20
	clc
	adc	#2
	bcc	l575
	inx
l575:
	clc
	adc	r0
	sta	r0
	txa
	adc	r1
	sta	r1
	lda	r22
	sta	(r0),y
	lda	_code_index
	sta	r0
	lda	#0
	sta	r1
	ldx	r21
	lda	r20
	clc
	adc	#3
	bcc	l576
	inx
l576:
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
	ldx	r21
	lda	r20
	clc
	adc	#4
	bcc	l577
	inx
l577:
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
	ldx	r21
	lda	r20
	clc
	adc	#5
	bcc	l578
	inx
l578:
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
	ldx	r21
	lda	r20
	clc
	adc	#6
	bcc	l579
	inx
l579:
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
	ldx	r21
	lda	r20
	clc
	adc	#7
	bcc	l580
	inx
l580:
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
	lda	r20
	clc
	adc	#8
	sta	r0
	lda	r21
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
	ldx	r21
	lda	r20
	clc
	adc	#9
	bcc	l581
	inx
l581:
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
	ldx	r21
	lda	r20
	clc
	adc	#10
	bcc	l582
	inx
l582:
	clc
	adc	r0
	sta	r0
	txa
	adc	r1
	sta	r1
	lda	#169
	sta	(r0),y
	iny
	lda	(sp),y
	and	#255
	sta	r31
	lda	_code_index
	sta	r2
	lda	#0
	sta	r3
	lda	r20
	clc
	adc	#11
	sta	r0
	lda	r21
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
	dey
	sta	(r0),y
	lda	_code_index
	sta	r0
	lda	#0
	sta	r1
	ldx	r21
	lda	r20
	clc
	adc	#12
	bcc	l583
	inx
l583:
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
	lda	r20
	clc
	adc	#13
	sta	r0
	lda	r21
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
	lda	r20
	clc
	adc	#14
	sta	r0
	lda	r21
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
	ldy	#2
	lda	(sp),y
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
	sta	r4
	txa
	sta	r5
	lda	r20
	clc
	adc	#15
	sta	r2
	lda	r21
	adc	#0
	sta	r3
	lda	r2
	clc
	adc	r4
	sta	r2
	lda	r3
	adc	r5
	sta	r3
	lda	r0
	ldy	#0
	sta	(r2),y
	lda	_code_index
	sta	r2
	txa
	sta	r3
	lda	r20
	clc
	adc	#16
	sta	r0
	lda	r21
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
	lda	r20
	clc
	adc	#17
	sta	r0
	lda	r21
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
	ldx	r21
	lda	r20
	clc
	adc	#18
	bcc	l584
	inx
l584:
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
	sta	r4
	lda	#0
	sta	r5
	lda	r20
	clc
	adc	#19
	sta	r2
	lda	r21
	adc	#0
	sta	r3
	lda	r2
	clc
	adc	r4
	sta	r2
	lda	r3
	adc	r5
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
	lda	r20
	clc
	adc	#20
	sta	r0
	lda	r21
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
	ldy	#6
	lda	(sp),y
	ldx	#0
	clc
	ldy	#3
	adc	(sp),y
	pha
	txa
	iny
	adc	(sp),y
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
	bcc	l585
	inx
l585:
	sta	r31
	lda	#0
	tay
	sta	(sp),y
	lda	r31
	sta	r2
	stx	r3
	ldy	#5
	lda	(sp),y
	sta	r4
	ldy	#2
	lda	(sp),y
	sta	r7
	dey
	lda	(sp),y
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
	bne	l586
	inc	1+_pc
l586:
	inc	_pc
	bne	l587
	inc	1+_pc
l587:
	lda	_code_index
	clc
	adc	#21
	sta	_code_index
	inc	_cache_branches
	bne	l588
	inc	1+_cache_branches
	bne	l588
	inc	2+_cache_branches
	bne	l588
	inc	3+_cache_branches
l588:
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
	jmp	l225
l247:
	lda	r4
	and	#31
	cmp	_flash_code_bank
	beq	l253
	inc	_branch_wrong_bank
	bne	l589
	inc	1+_branch_wrong_bank
	bne	l589
	inc	2+_branch_wrong_bank
	bne	l589
	inc	3+_branch_wrong_bank
l589:
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r4
	lda	#>(_cache_flag)
	adc	#0
	sta	r5
	ldy	#0
	lda	(r4),y
	ora	#2
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
	jmp	l225
l253:
	ldy	#2
	lda	(sp),y
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
	sta	r17
	lda	(sp),y
	tax
	dey
	lda	(sp),y
	stx	r31
	asl
	rol	r31
	ldx	r31
	sta	r18
	txa
	and	#63
	sta	r19
	lda	#<(_flash_cache_pc)
	clc
	adc	r18
	pha
	lda	#>(_flash_cache_pc)
	adc	r19
	tax
	pla
	sta	r31
	lda	r17
	sta	r0
	lda	r31
	sta	r2
	stx	r3
	jsr	_peek_bank_byte
	sta	r16
	lda	#<(1+_flash_cache_pc)
	clc
	adc	r18
	pha
	lda	#>(1+_flash_cache_pc)
	adc	r19
	tax
	pla
	sta	r31
	lda	r17
	sta	r0
	lda	r31
	sta	r2
	stx	r3
	jsr	_peek_bank_byte
	sta	r0
	ldx	#0
	sta	r30
	stx	r29
	tax
	lda	#0
	sta	r2
	stx	r3
	ldx	r29
	lda	r16
	ldx	#0
	ora	r2
	pha
	txa
	ora	r3
	tax
	pla
	sta	r31
	lda	_code_index
	sta	r2
	lda	#0
	sta	r3
	lda	r2
	clc
	adc	_flash_code_address
	sta	r2
	lda	r3
	adc	1+_flash_code_address
	sta	r3
	lda	r31
	inc	r2
	bne	l590
	inc	r3
l590:
	inc	r2
	bne	l591
	inc	r3
l591:
	sec
	sbc	r2
	ldy	#3
	sta	(sp),y
	txa
	sbc	r3
	iny
	sta	(sp),y
	dey
	lda	(sp),y
	cmp	#128
	iny
	lda	(sp),y
	sbc	#255
	bvc	l592
	eor	#128
l592:
	bmi	l256
	lda	#127
	ldy	#3
	cmp	(sp),y
	lda	#0
	iny
	sbc	(sp),y
	bvc	l593
	eor	#128
l593:
	bmi	l256
	lda	r20
	clc
	adc	_code_index
	sta	r4
	lda	r21
	adc	#0
	sta	r5
	lda	r22
	ldy	#0
	sta	(r4),y
	ldy	#3
	lda	(sp),y
	sta	r31
	lda	_code_index
	sta	r8
	lda	#0
	sta	r9
	lda	r20
	clc
	adc	#1
	sta	r4
	lda	r21
	adc	#0
	sta	r5
	lda	r4
	clc
	adc	r8
	sta	r4
	lda	r5
	adc	r9
	sta	r5
	lda	r31
	ldy	#0
	sta	(r4),y
	inc	_pc
	bne	l594
	inc	1+_pc
l594:
	inc	_pc
	bne	l595
	inc	1+_pc
l595:
	inc	_code_index
	inc	_code_index
	inc	_cache_branches
	bne	l596
	inc	1+_cache_branches
	bne	l596
	inc	2+_cache_branches
	bne	l596
	inc	3+_cache_branches
l596:
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r4
	lda	#>(_cache_flag)
	adc	#0
	sta	r5
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
	jmp	l225
l256:
	inc	_branch_out_of_range
	bne	l597
	inc	1+_branch_out_of_range
	bne	l597
	inc	2+_branch_out_of_range
	bne	l597
	inc	3+_branch_out_of_range
l597:
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r4
	lda	#>(_cache_flag)
	adc	#0
	sta	r5
	ldy	#0
	lda	(r4),y
	ora	#2
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
	jmp	l225
l259:
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
	ldy	#1
	sta	(sp),y
	txa
	ora	r17
	iny
	sta	(sp),y
	lda	_code_index
	ldx	#0
	clc
	adc	#9
	bcc	l598
	inx
l598:
	pha
	cmp	#229
	txa
	sbc	#0
	bvc	l600
	eor	#128
l600:
	bpl	l599
	pla
	lda	r20
	clc
	adc	_code_index
	sta	r0
	lda	r21
	adc	#0
	sta	r1
	lda	#8
	ldy	#0
	sta	(r0),y
	lda	_code_index
	sta	r0
	tya
	sta	r1
	ldx	r21
	lda	r20
	clc
	adc	#1
	bcc	l601
	inx
l601:
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
	ldx	r21
	lda	r20
	clc
	adc	#2
	bcc	l602
	inx
l602:
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
	ldx	r21
	lda	r20
	clc
	adc	#3
	bcc	l603
	inx
l603:
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
	ldx	r21
	lda	r20
	clc
	adc	#4
	bcc	l604
	inx
l604:
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
	ldx	r21
	lda	r20
	clc
	adc	#5
	bcc	l605
	inx
l605:
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
	ldx	r21
	lda	r20
	clc
	adc	#6
	bcc	l606
	inx
l606:
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
	ldx	r21
	lda	r20
	clc
	adc	#7
	bcc	l607
	inx
l607:
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
	ldx	r21
	lda	r20
	clc
	adc	#8
	bcc	l608
	inx
l608:
	clc
	adc	r0
	sta	r0
	txa
	adc	r1
	sta	r1
	lda	#40
	sta	(r0),y
	lda	_code_index
	ldx	#0
	clc
	adc	_flash_code_address
	pha
	txa
	adc	1+_flash_code_address
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
	bcc	l609
	inx
l609:
	sta	r31
	lda	#0
	sta	(sp),y
	lda	r31
	sta	r2
	stx	r3
	lda	_flash_code_bank
	sta	r4
	ldy	#2
	lda	(sp),y
	sta	r7
	dey
	lda	(sp),y
	sta	r6
	jsr	_opt2_record_pending_branch_safe
	ldy	#2
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
	dey
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
	jmp	l225
l264:
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r4
	lda	#>(_cache_flag)
	adc	#0
	sta	r5
	ldy	#0
	lda	(r4),y
	ora	#2
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
	jmp	l225
l266:
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
	ldy	#1
	sta	(sp),y
	txa
	ora	r17
	iny
	sta	(sp),y
	lda	_pc
	clc
	adc	#2
	iny
	sta	(sp),y
	lda	1+_pc
	adc	#0
	iny
	sta	(sp),y
	ldy	#2
	lda	(sp),y
	sta	r1
	dey
	lda	(sp),y
	sta	r0
	jsr	_sa_subroutine_lookup
	cmp	#0
	bne	l268
	lda	_code_index
	sta	r4
	lda	#0
	sta	r5
	lda	_opcode_6502_njsr_size
	ldx	#0
	clc
	adc	r4
	pha
	txa
	adc	r5
	tax
	pla
	clc
	adc	#27
	bcc	l610
	inx
l610:
	pha
	cmp	#229
	txa
	sbc	#0
	bvc	l612
	eor	#128
l612:
	bpl	l611
	pla
	lda	#0
	sta	r2
	lda	_opcode_6502_njsr_size
	beq	l409
	lda	r21
	sta	r25
	lda	r20
	sta	r24
	lda	r2
	ldy	#5
	sta	(sp),y
	tax
l395:
	lda	r24
	clc
	adc	_code_index
	sta	r8
	lda	r25
	adc	#0
	sta	r9
	lda	0+_opcode_6502_njsr,x ;am(x)
	ldy	#0
	sta	(r8),y
	inx
	inc	r24
	bne	l613
	inc	r25
l613:
	cpx	_opcode_6502_njsr_size
	bcc	l395
l409:
	stx	r30
	ldy	#4
	lda	(sp),y
	ldx	#0
	sta	r4
	stx	r5
	ldx	r30
	lda	r4
	sta	r31
	lda	_code_index
	sta	r8
	lda	#0
	sta	r9
	lda	r20
	clc
	adc	_opcode_6502_njsr_ret_hi
	sta	r4
	lda	r21
	adc	#0
	sta	r5
	lda	r4
	clc
	adc	r8
	sta	r4
	lda	r5
	adc	r9
	sta	r5
	lda	r31
	ldy	#0
	sta	(r4),y
	ldy	#3
	lda	(sp),y
	sta	r31
	lda	_code_index
	sta	r8
	lda	#0
	sta	r9
	lda	r20
	clc
	adc	_opcode_6502_njsr_ret_lo
	sta	r4
	lda	r21
	adc	#0
	sta	r5
	lda	r4
	clc
	adc	r8
	sta	r4
	lda	r5
	adc	r9
	sta	r5
	lda	r31
	ldy	#0
	sta	(r4),y
	iny
	lda	(sp),y
	sta	r31
	lda	_code_index
	sta	r8
	lda	#0
	sta	r9
	lda	r20
	clc
	adc	_opcode_6502_njsr_tgt_lo
	sta	r4
	lda	r21
	adc	#0
	sta	r5
	lda	r4
	clc
	adc	r8
	sta	r4
	lda	r5
	adc	r9
	sta	r5
	lda	r31
	dey
	sta	(r4),y
	stx	r30
	ldy	#2
	lda	(sp),y
	ldx	#0
	sta	r4
	stx	r5
	ldx	r30
	lda	r4
	sta	r31
	lda	_code_index
	sta	r8
	lda	#0
	sta	r9
	lda	r20
	clc
	adc	_opcode_6502_njsr_tgt_hi
	sta	r4
	lda	r21
	adc	#0
	sta	r5
	lda	r4
	clc
	adc	r8
	sta	r4
	lda	r5
	adc	r9
	sta	r5
	lda	r31
	ldy	#0
	sta	(r4),y
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
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r4
	lda	#>(_cache_flag)
	adc	#0
	sta	r5
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
	jmp	l225
l268:
	lda	_code_index
	sta	r4
	lda	#0
	sta	r5
	lda	_opcode_6502_jsr_size
	ldx	#0
	clc
	adc	r4
	pha
	txa
	adc	r5
	tax
	pla
	clc
	adc	#27
	bcc	l614
	inx
l614:
	pha
	cmp	#229
	txa
	sbc	#0
	bvc	l616
	eor	#128
l616:
	bpl	l615
	pla
	lda	#0
	sta	r3
	lda	_opcode_6502_jsr_size
	beq	l410
	lda	r21
	sta	r17
	lda	r20
	sta	r16
	lda	r3
	ldy	#5
	sta	(sp),y
	tax
l396:
	lda	r16
	clc
	adc	_code_index
	sta	r8
	lda	r17
	adc	#0
	sta	r9
	lda	0+_opcode_6502_jsr,x ;am(x)
	ldy	#0
	sta	(r8),y
	inx
	inc	r16
	bne	l617
	inc	r17
l617:
	cpx	_opcode_6502_jsr_size
	bcc	l396
l410:
	stx	r30
	ldy	#4
	lda	(sp),y
	ldx	#0
	sta	r4
	stx	r5
	ldx	r30
	lda	r4
	sta	r31
	lda	_code_index
	sta	r8
	lda	#0
	sta	r9
	lda	r20
	clc
	adc	_opcode_6502_jsr_ret_hi
	sta	r4
	lda	r21
	adc	#0
	sta	r5
	lda	r4
	clc
	adc	r8
	sta	r4
	lda	r5
	adc	r9
	sta	r5
	lda	r31
	ldy	#0
	sta	(r4),y
	ldy	#3
	lda	(sp),y
	sta	r31
	lda	_code_index
	sta	r8
	lda	#0
	sta	r9
	lda	r20
	clc
	adc	_opcode_6502_jsr_ret_lo
	sta	r4
	lda	r21
	adc	#0
	sta	r5
	lda	r4
	clc
	adc	r8
	sta	r4
	lda	r5
	adc	r9
	sta	r5
	lda	r31
	ldy	#0
	sta	(r4),y
	iny
	lda	(sp),y
	sta	r31
	lda	_code_index
	sta	r8
	lda	#0
	sta	r9
	lda	r20
	clc
	adc	_opcode_6502_jsr_tgt_lo
	sta	r4
	lda	r21
	adc	#0
	sta	r5
	lda	r4
	clc
	adc	r8
	sta	r4
	lda	r5
	adc	r9
	sta	r5
	lda	r31
	dey
	sta	(r4),y
	stx	r30
	ldy	#2
	lda	(sp),y
	ldx	#0
	sta	r4
	stx	r5
	ldx	r30
	lda	r4
	sta	r31
	lda	_code_index
	sta	r8
	lda	#0
	sta	r9
	lda	r20
	clc
	adc	_opcode_6502_jsr_tgt_hi
	sta	r4
	lda	r21
	adc	#0
	sta	r5
	lda	r4
	clc
	adc	r8
	sta	r4
	lda	r5
	adc	r9
	sta	r5
	lda	r31
	ldy	#0
	sta	(r4),y
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
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r4
	lda	#>(_cache_flag)
	adc	#0
	sta	r5
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
	jmp	l225
l276:
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
	jmp	l225
l282:
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r4
	lda	#>(_cache_flag)
	adc	#0
	sta	r5
	ldy	#0
	lda	(r4),y
	ora	#2
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
	jmp	l225
l284:
	lda	_code_index
	sta	r4
	lda	#0
	sta	r5
	lda	_opcode_6502_nrts_size
	ldx	#0
	clc
	adc	r4
	pha
	txa
	adc	r5
	tax
	pla
	clc
	adc	#27
	bcc	l618
	inx
l618:
	pha
	cmp	#229
	txa
	sbc	#0
	bvc	l620
	eor	#128
l620:
	bpl	l619
	pla
	lda	#0
	sta	r7
	lda	_opcode_6502_nrts_size
	beq	l411
	lda	r21
	sta	r13
	lda	r20
	sta	r12
	lda	r7
	ldy	#1
	sta	(sp),y
	tax
l397:
	lda	r12
	clc
	adc	_code_index
	sta	r8
	lda	r13
	adc	#0
	sta	r9
	lda	0+_opcode_6502_nrts,x ;am(x)
	ldy	#0
	sta	(r8),y
	inx
	inc	r12
	bne	l621
	inc	r13
l621:
	cpx	_opcode_6502_nrts_size
	bcc	l397
l411:
	inc	_pc
	bne	l622
	inc	1+_pc
l622:
	lda	_code_index
	clc
	adc	_opcode_6502_nrts_size
	sta	_code_index
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r4
	lda	#>(_cache_flag)
	adc	#0
	sta	r5
	ldy	#0
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
	jmp	l225
l286:
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r4
	lda	#>(_cache_flag)
	adc	#0
	sta	r5
	ldy	#0
	lda	(r4),y
	ora	#2
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
	jmp	l225
l291:
	lda	_code_index
	ldx	#0
	clc
	adc	#6
	bcc	l623
	inx
l623:
	pha
	cmp	#229
	txa
	sbc	#0
	bvc	l625
	eor	#128
l625:
	bpl	l624
	pla
	lda	r20
	clc
	adc	_code_index
	sta	r4
	lda	r21
	adc	#0
	sta	r5
	lda	#166
	ldy	#0
	sta	(r4),y
	ldx	#>(_sp)
	lda	#<(_sp)
	sta	r31
	lda	_code_index
	sta	r8
	tya
	sta	r9
	lda	r20
	clc
	adc	#1
	sta	r4
	lda	r21
	adc	#0
	sta	r5
	lda	r4
	clc
	adc	r8
	sta	r4
	lda	r5
	adc	r9
	sta	r5
	lda	r31
	sta	(r4),y
	inc	_pc
	bne	l626
	inc	1+_pc
l626:
	inc	_code_index
	inc	_code_index
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r4
	lda	#>(_cache_flag)
	adc	#0
	sta	r5
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
	jmp	l225
l293:
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
	jmp	l225
l294:
	lda	_code_index
	ldx	#0
	clc
	adc	#6
	bcc	l627
	inx
l627:
	pha
	cmp	#229
	txa
	sbc	#0
	bvc	l629
	eor	#128
l629:
	bpl	l628
	pla
	lda	r20
	clc
	adc	_code_index
	sta	r4
	lda	r21
	adc	#0
	sta	r5
	lda	#134
	ldy	#0
	sta	(r4),y
	ldx	#>(_sp)
	lda	#<(_sp)
	sta	r31
	lda	_code_index
	sta	r8
	tya
	sta	r9
	lda	r20
	clc
	adc	#1
	sta	r4
	lda	r21
	adc	#0
	sta	r5
	lda	r4
	clc
	adc	r8
	sta	r4
	lda	r5
	adc	r9
	sta	r5
	lda	r31
	sta	(r4),y
	inc	_pc
	bne	l630
	inc	1+_pc
l630:
	inc	_code_index
	inc	_code_index
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r4
	lda	#>(_cache_flag)
	adc	#0
	sta	r5
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
	jmp	l225
l297:
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
	jmp	l225
l298:
	lda	_code_index
	sta	r4
	lda	#0
	sta	r5
	lda	_opcode_6502_pha_size
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
	bcc	l631
	inx
l631:
	pha
	cmp	#229
	txa
	sbc	#0
	bvc	l633
	eor	#128
l633:
	bpl	l632
	pla
	lda	#0
	sta	r6
	lda	_opcode_6502_pha_size
	beq	l412
	lda	r21
	sta	r15
	lda	r20
	sta	r14
	lda	r6
	ldy	#1
	sta	(sp),y
	tax
l398:
	lda	r14
	clc
	adc	_code_index
	sta	r8
	lda	r15
	adc	#0
	sta	r9
	lda	0+_opcode_6502_pha,x ;am(x)
	ldy	#0
	sta	(r8),y
	inx
	inc	r14
	bne	l634
	inc	r15
l634:
	cpx	_opcode_6502_pha_size
	bcc	l398
l412:
	inc	_pc
	bne	l635
	inc	1+_pc
l635:
	lda	_code_index
	clc
	adc	_opcode_6502_pha_size
	sta	_code_index
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
	jmp	l225
l301:
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
	jmp	l225
l306:
	lda	_code_index
	sta	r4
	lda	#0
	sta	r5
	lda	_opcode_6502_pla_size
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
	bcc	l636
	inx
l636:
	pha
	cmp	#229
	txa
	sbc	#0
	bvc	l638
	eor	#128
l638:
	bpl	l637
	pla
	ldx	#0
	lda	_opcode_6502_pla_size
	beq	l413
	lda	r21
	sta	r11
	lda	r20
	sta	r10
l406:
	lda	r10
	clc
	adc	_code_index
	sta	r8
	lda	r11
	adc	#0
	sta	r9
	lda	0+_opcode_6502_pla,x ;am(x)
	ldy	#0
	sta	(r8),y
	inx
	inc	r10
	bne	l639
	inc	r11
l639:
	cpx	_opcode_6502_pla_size
	bcc	l406
l413:
	inc	_pc
	bne	l640
	inc	1+_pc
l640:
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
	lda	_code_index
	clc
	adc	_opcode_6502_pla_size
	sta	_code_index
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r4
	lda	#>(_cache_flag)
	adc	#0
	sta	r5
	lda	(r4),y
	jmp	l225
l309:
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
	jmp	l225
l314:
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r4
	lda	#>(_cache_flag)
	adc	#0
	sta	r5
	ldy	#0
	lda	(r4),y
	ora	#2
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
	jmp	l225
l319:
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r4
	lda	#>(_cache_flag)
	adc	#0
	sta	r5
	ldy	#0
	lda	(r4),y
	ora	#2
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
	jmp	l225
l321:
	lda	#1
	sta	_decimal_mode
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r4
	lda	#>(_cache_flag)
	adc	#0
	sta	r5
	ldy	#0
	lda	(r4),y
	ora	#2
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
	jmp	l225
l322:
	inc	_pc
	bne	l641
	inc	1+_pc
l641:
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
	jmp	l225
l323:
; volatile barrier
	lda	#0
	sta	16417
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r4
	lda	#>(_cache_flag)
	adc	#0
	sta	r5
	ldy	#0
	lda	(r4),y
	ora	#2
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
	jmp	l225
l324:
	lda	r20
	clc
	adc	_code_index
	sta	r4
	lda	r21
	adc	#0
	sta	r5
	lda	r22
	ldy	#0
	sta	(r4),y
	ldy	r22
	lda	0+_addrmodes,y ;am(r22)
	sta	r8
	cmp	#0
	beq	l357
	lda	r8
	cmp	#1
	beq	l357
	lda	r8
	cmp	#2
	beq	l355
	lda	r8
	cmp	#3
	beq	l333
	lda	r8
	cmp	#4
	beq	l334
	lda	r8
	cmp	#5
	beq	l334
	lda	r8
	cmp	#6
	beq	l355
	lda	r8
	cmp	#7
	beq	l326
	lda	r8
	cmp	#8
	beq	l326
	lda	r8
	cmp	#9
	beq	l326
	lda	r8
	cmp	#10
	beq	l326
	lda	r8
	cmp	#11
	beq	l336
	lda	r8
	cmp	#12
	beq	l337
	jmp	l359
l326:
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
	jsr	l1
	sta	_decoded_address
	stx	1+_decoded_address
	lda	_decoded_address
	ora	1+_decoded_address
	beq	l331
	lda	_decoded_address
	sta	r31
	lda	_code_index
	sta	r8
	lda	#0
	sta	r9
	lda	r20
	clc
	adc	#1
	sta	r4
	lda	r21
	adc	#0
	sta	r5
	lda	r4
	clc
	adc	r8
	sta	r4
	lda	r5
	adc	r9
	sta	r5
	lda	r31
	ldy	#0
	sta	(r4),y
	stx	r30
	lda	1+_decoded_address
	ldx	#0
	sta	r4
	stx	r5
	ldx	r30
	lda	r4
	sta	r31
	lda	_code_index
	sta	r8
	tya
	sta	r9
	lda	r20
	clc
	adc	#2
	sta	r4
	lda	r21
	adc	#0
	sta	r5
	lda	r4
	clc
	adc	r8
	sta	r4
	lda	r5
	adc	r9
	sta	r5
	lda	r31
	sta	(r4),y
	jmp	l332
l331:
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r4
	lda	#>(_cache_flag)
	adc	#0
	sta	r5
	ldy	#0
	lda	(r4),y
	ora	#2
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
	jmp	l225
l332:
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
	jmp	l325
l333:
	lda	r20
	clc
	adc	_code_index
	sta	r0
	lda	r21
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
	sta	r8
	lda	#0
	sta	r9
	lda	r20
	clc
	adc	#1
	sta	r4
	lda	r21
	adc	#0
	sta	r5
	lda	r4
	clc
	adc	r8
	sta	r4
	lda	r5
	adc	r9
	sta	r5
	lda	r1
	ldy	#0
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
	lda	_code_index
	sta	r4
	tya
	sta	r5
	lda	r20
	clc
	adc	#2
	sta	r0
	lda	r21
	adc	#0
	sta	r1
	lda	r0
	clc
	adc	r4
	sta	r0
	lda	r1
	adc	r5
	sta	r1
	lda	r31
	sta	(r0),y
	inc	_pc
	bne	l642
	inc	1+_pc
l642:
	inc	_pc
	bne	l643
	inc	1+_pc
l643:
	lda	_code_index
	clc
	adc	#3
	sta	_code_index
	jmp	l325
l334:
	lda	r20
	clc
	adc	_code_index
	sta	r0
	lda	r21
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
	sta	r8
	lda	#0
	sta	r9
	lda	r20
	clc
	adc	#1
	sta	r4
	lda	r21
	adc	#0
	sta	r5
	lda	r4
	clc
	adc	r8
	sta	r4
	lda	r5
	adc	r9
	sta	r5
	lda	r1
	ldy	#0
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
	lda	_code_index
	sta	r4
	tya
	sta	r5
	lda	r20
	clc
	adc	#2
	sta	r0
	lda	r21
	adc	#0
	sta	r1
	lda	r0
	clc
	adc	r4
	sta	r0
	lda	r1
	adc	r5
	sta	r1
	lda	r31
	sta	(r0),y
	inc	_pc
	bne	l644
	inc	1+_pc
l644:
	inc	_pc
	bne	l645
	inc	1+_pc
l645:
	lda	_code_index
	clc
	adc	#3
	sta	_code_index
	jmp	l325
l336:
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r4
	lda	#>(_cache_flag)
	adc	#0
	sta	r5
	ldy	#0
	lda	(r4),y
	ora	#2
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
	jmp	l225
l337:
	lda	_pc
	clc
	adc	#1
	sta	r0
	lda	1+_pc
	adc	#0
	sta	r1
	jsr	_read6502
	sta	r23
	lda	r22
	cmp	#145
	bne	l339
	lda	_code_index
	sta	r4
	lda	#0
	sta	r5
	lda	_sta_indy_template_size
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
	bcc	l646
	inx
l646:
	pha
	cmp	#229
	txa
	sbc	#0
	bvc	l648
	eor	#128
l648:
	bpl	l647
	pla
	lda	r23
	sta	_sta_indy_zp_patch
	lda	#0
	sta	r1
	lda	_sta_indy_template_size
	beq	l414
	lda	r21
	sta	r27
	lda	r20
	sta	r26
	lda	r1
	ldy	#1
	sta	(sp),y
	tax
l400:
	lda	r26
	clc
	adc	_code_index
	sta	r8
	lda	r27
	adc	#0
	sta	r9
	lda	0+_sta_indy_template,x ;am(x)
	ldy	#0
	sta	(r8),y
	inx
	inc	r26
	bne	l649
	inc	r27
l649:
	cpx	_sta_indy_template_size
	bcc	l400
l414:
	inc	_pc
	bne	l650
	inc	1+_pc
l650:
	inc	_pc
	bne	l651
	inc	1+_pc
l651:
	lda	_code_index
	clc
	adc	_sta_indy_template_size
	sta	_code_index
	jmp	l325
l341:
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
	jmp	l225
l339:
	lda	r23
	sta	r4
	lda	#0
	sta	r5
	ldx	#>(_RAM_BASE)
	lda	#<(_RAM_BASE)
	clc
	adc	r4
	ldy	#1
	sta	(sp),y
	txa
	adc	r5
	iny
	sta	(sp),y
	lda	_code_index
	sta	r4
	lda	#0
	sta	r5
	lda	_addr_6502_indy_size
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
	bcc	l652
	inx
l652:
	pha
	cmp	#229
	txa
	sbc	#0
	bvc	l654
	eor	#128
l654:
	bpl	l653
	pla
	lda	r22
	sta	_indy_opcode_location
	ldy	#2
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
	beq	l415
	lda	r21
	sta	r19
	lda	r20
	sta	r18
	lda	r0
	ldy	#1
	sta	(sp),y
	tax
l401:
	lda	r18
	clc
	adc	_code_index
	sta	r8
	lda	r19
	adc	#0
	sta	r9
	lda	0+_addr_6502_indy,x ;am(x)
	ldy	#0
	sta	(r8),y
	inx
	inc	r18
	bne	l655
	inc	r19
l655:
	cpx	_addr_6502_indy_size
	bcc	l401
l415:
	inc	_pc
	bne	l656
	inc	1+_pc
l656:
	inc	_pc
	bne	l657
	inc	1+_pc
l657:
	lda	_code_index
	clc
	adc	_addr_6502_indy_size
	sta	_code_index
	jmp	l325
l349:
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
	jmp	l225
l355:
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
	sta	r4
	lda	#0
	sta	r5
	lda	r20
	clc
	adc	#1
	sta	r0
	lda	r21
	adc	#0
	sta	r1
	lda	r0
	clc
	adc	r4
	sta	r0
	lda	r1
	adc	r5
	sta	r1
	lda	r31
	ldy	#0
	sta	(r0),y
	inc	_pc
	bne	l658
	inc	1+_pc
l658:
	inc	_pc
	bne	l659
	inc	1+_pc
l659:
	inc	_code_index
	inc	_code_index
	jmp	l325
l357:
	inc	_pc
	bne	l660
	inc	1+_pc
l660:
	inc	_code_index
	jmp	l325
l359:
	lda	#<(_cache_flag)
	clc
	adc	_cache_index
	sta	r4
	lda	#>(_cache_flag)
	adc	#0
	sta	r5
	ldy	#0
	lda	(r4),y
	ora	#2
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
	jmp	l225
l325:
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
l225:
	sta	r31
	ldy	#18
	jsr	___rload12
	clc
	lda	sp
	adc	#19
	sta	sp
	bcc	l661
	inc	sp+1
l661:
	lda	r31
	rts
l653:
	pla
	jmp	l349
l647:
	pla
	jmp	l341
l637:
	pla
	jmp	l309
l632:
	pla
	jmp	l301
l628:
	pla
	jmp	l297
l624:
	pla
	jmp	l293
l619:
	pla
	jmp	l286
l615:
	pla
	jmp	l276
l611:
	pla
	jmp	l268
l599:
	pla
	jmp	l264
l572:
	pla
	jmp	l249
l552:
	pla
	jmp	l243
l544:
	pla
	jmp	l239
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
	sta	r17
	lda	#2
	jsr	_bankswitch_prg
	jsr	l224
	sta	r16
	lda	r17
	jsr	_bankswitch_prg
	lda	r16
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
l664:
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
	beq	l669
l670:
	asl
	dey
	bne	l670
l669:
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
l671:
	lda	sp
	bne	l686
	dec	sp+1
l686:
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
	bcc	l675
	lda	r16
	cmp	#7
	bne	l683
l675:
	lda	r1
	and	#128
	ldy	#0
	sta	(sp),y
l683:
	ldy	#0
	lda	(sp),y
	sta	r31
	pla
	sta	r16
	inc	sp
	bne	l687
	inc	sp+1
l687:
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
	jsr	l664
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
	jsr	l671
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
	global	_reservation_count
	section	data
_reservation_count:
	byte	0
	global	_reservations_enabled
	section	data
_reservations_enabled:
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
	global	_pc_2b27_count
	zpage	_pc_2b27_count
	section	zpage
_pc_2b27_count:
	byte	0
	global	_flash_enabled
	section	data
_flash_enabled:
	byte	0
	global	_next_free_block
	section	data
_next_free_block:
	word	0
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
	global	_addr_6502_indy
	global	_addr_6502_indy_size
	global	_indy_opcode_location
	global	_indy_address_lo
	global	_indy_address_hi
	global	_opcode_6502_pha_size
	global	_opcode_6502_pla_size
	global	_opcode_6502_pha
	global	_opcode_6502_pla
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
	global	_reserved_pc
	section	bss
_reserved_pc:
	reserve	64
	global	_reserved_block
	section	bss
_reserved_block:
	reserve	64
	global	_reserve_result_addr
	section	bss
_reserve_result_addr:
	reserve	2
	global	_reserve_result_bank
	section	bss
_reserve_result_bank:
	reserve	1
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
	global	_l1_cache_code
	section	bss
_l1_cache_code:
	reserve	256
	global	_cache_code
	section	bss
_cache_code:
	reserve	1832
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
	global	_flash_block_flags
	global	_peek_bank_byte
	global	_opt2_stat_total
	global	_opt2_stat_direct
	global	_opt2_stat_pending
	global	_next_free_block
	global	_stats_frame
	global	_sa_blocks_total
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
	global	_sa_code_bitmap
	global	_peek_bank_byte
	section	data
l26:
	word	0
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
