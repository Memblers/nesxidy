;vcprmin=10000
	section	"text0"
	global	_push16
_push16:
	lda	r16
	pha
	lda	r17
	pha
	lda	r1
	sta	r17
	lda	r0
	sta	r16
	lda	r17
	ldx	#0
	sta	r0
	stx	r1
	lda	r0
	and	#255
	sta	r31
	lda	_sp
	sta	r2
	txa
	sta	r3
	lda	r2
	clc
	adc	#0
	sta	r0
	lda	r3
	adc	#1
	sta	r1
	lda	r31
	sta	r2
	jsr	_write6502
	lda	r16
	and	#255
	sta	r31
	lda	_sp
	sta	r2
	lda	#0
	sta	r3
	lda	r2
	bne	l4
	dec	r3
l4:
	dec	r2
	lda	#0
	sta	r3
	lda	r2
	clc
	adc	#0
	sta	r0
	lda	r3
	adc	#1
	sta	r1
	lda	r31
	sta	r2
	jsr	_write6502
	dec	_sp
	dec	_sp
	pla
	sta	r17
	pla
	sta	r16
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
	global	_push8
_push8:
	lda	r0
	sta	r3
	lda	_sp
	ldx	#0
	sta	r31
	dec	_sp
	lda	r31
	clc
	adc	#0
	sta	r0
	txa
	adc	#1
	sta	r1
	lda	r3
	sta	r2
	jmp	_write6502
; stacksize=0+??
;vcprmin=10000
	section	"text0"
	global	_pull16
_pull16:
	lda	r16
	pha
	lda	_sp
	ldx	#0
	clc
	adc	#1
	bcc	l11
	inx
l11:
	ldx	#0
	clc
	adc	#0
	sta	r0
	txa
	adc	#1
	sta	r1
	jsr	_read6502
	sta	r16
	lda	_sp
	ldx	#0
	clc
	adc	#2
	bcc	l12
	inx
l12:
	ldx	#0
	clc
	adc	#0
	sta	r0
	txa
	adc	#1
	sta	r1
	jsr	_read6502
	sta	r0
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
	pha
	txa
	ora	r1
	tax
	pla
	inc	_sp
	inc	_sp
	sta	r31
	pla
	sta	r16
	lda	r31
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
	global	_pull8
_pull8:
	inc	_sp
	lda	_sp
	ldx	#0
	clc
	adc	#0
	sta	r0
	txa
	adc	#1
	sta	r1
	jmp	_read6502
; stacksize=0+??
;vcprmin=10000
	section	"text0"
	global	_reset6502
_reset6502:
	lda	r16
	pha
	lda	r17
	pha
	lda	#255
	sta	r1
	lda	#252
	sta	r0
	jsr	_read6502
	sta	r31
	sta	r16
	lda	#0
	sta	r17
	lda	#255
	sta	r1
	lda	#253
	sta	r0
	jsr	_read6502
	sta	r0
	tax
	lda	#0
	ora	r16
	sta	_pc
	txa
	ora	r17
	sta	1+_pc
	lda	#0
	sta	_a
	sta	_x
	sta	_y
	lda	#253
	sta	_sp
	lda	_status
	ora	#32
	sta	_status
	pla
	sta	r17
	pla
	sta	r16
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l19:
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l22:
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l25:
	lda	1+_pc
	sta	1+_ea
	lda	_pc
	sta	_ea
	inc	_pc
	bne	l29
	inc	1+_pc
l29:
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l30:
	lda	1+_pc
	sta	r1
	lda	_pc
	sta	r0
	inc	_pc
	bne	l34
	inc	1+_pc
l34:
	jsr	_read6502
	sta	r31
	sta	_ea
	lda	#0
	sta	1+_ea
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l35:
	lda	1+_pc
	sta	r1
	lda	_pc
	sta	r0
	inc	_pc
	bne	l39
	inc	1+_pc
l39:
	jsr	_read6502
	sta	r31
	sta	r0
	lda	#0
	sta	r1
	lda	_x
	ldx	#0
	clc
	adc	r0
	pha
	txa
	adc	r1
	tax
	pla
	sta	_ea
	lda	#0
	sta	1+_ea
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l40:
	lda	1+_pc
	sta	r1
	lda	_pc
	sta	r0
	inc	_pc
	bne	l44
	inc	1+_pc
l44:
	jsr	_read6502
	sta	r31
	sta	r0
	lda	#0
	sta	r1
	lda	_y
	ldx	#0
	clc
	adc	r0
	pha
	txa
	adc	r1
	tax
	pla
	sta	_ea
	lda	#0
	sta	1+_ea
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l45:
	lda	1+_pc
	sta	r1
	lda	_pc
	sta	r0
	inc	_pc
	bne	l51
	inc	1+_pc
l51:
	jsr	_read6502
	sta	r31
	sta	_reladdr
	lda	#0
	sta	1+_reladdr
	lda	r31
	and	#128
	beq	l49
	lda	#255
	sta	1+_reladdr
l49:
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l52:
	lda	r16
	pha
	lda	r17
	pha
	lda	1+_pc
	sta	r1
	lda	_pc
	sta	r0
	jsr	_read6502
	sta	r31
	sta	r16
	lda	#0
	sta	r17
	lda	_pc
	clc
	adc	#1
	sta	r0
	lda	1+_pc
	adc	#0
	sta	r1
	jsr	_read6502
	sta	r0
	tax
	lda	#0
	ora	r16
	sta	_ea
	txa
	ora	r17
	sta	1+_ea
	inc	_pc
	bne	l57
	inc	1+_pc
l57:
	inc	_pc
	bne	l58
	inc	1+_pc
l58:
	pla
	sta	r17
	pla
	sta	r16
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l59:
	lda	r16
	pha
	lda	r17
	pha
	lda	1+_pc
	sta	r1
	lda	_pc
	sta	r0
	jsr	_read6502
	sta	r31
	sta	r16
	lda	#0
	sta	r17
	lda	_pc
	clc
	adc	#1
	sta	r0
	lda	1+_pc
	adc	#0
	sta	r1
	jsr	_read6502
	sta	r0
	tax
	lda	#0
	ora	r16
	sta	_ea
	txa
	ora	r17
	sta	1+_ea
	lda	_x
	ldx	#0
	clc
	adc	_ea
	sta	_ea
	txa
	adc	1+_ea
	sta	1+_ea
	inc	_pc
	bne	l64
	inc	1+_pc
l64:
	inc	_pc
	bne	l65
	inc	1+_pc
l65:
	pla
	sta	r17
	pla
	sta	r16
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l66:
	lda	r16
	pha
	lda	r17
	pha
	lda	1+_pc
	sta	r1
	lda	_pc
	sta	r0
	jsr	_read6502
	sta	r31
	sta	r16
	lda	#0
	sta	r17
	lda	_pc
	clc
	adc	#1
	sta	r0
	lda	1+_pc
	adc	#0
	sta	r1
	jsr	_read6502
	sta	r0
	tax
	lda	#0
	ora	r16
	sta	_ea
	txa
	ora	r17
	sta	1+_ea
	lda	_y
	ldx	#0
	clc
	adc	_ea
	sta	_ea
	txa
	adc	1+_ea
	sta	1+_ea
	inc	_pc
	bne	l71
	inc	1+_pc
l71:
	inc	_pc
	bne	l72
	inc	1+_pc
l72:
	pla
	sta	r17
	pla
	sta	r16
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l73:
	lda	r16
	pha
	lda	r17
	pha
	lda	r18
	pha
	lda	r19
	pha
	lda	1+_pc
	sta	r1
	lda	_pc
	sta	r0
	jsr	_read6502
	sta	r31
	sta	r16
	lda	#0
	sta	r17
	lda	_pc
	clc
	adc	#1
	sta	r0
	lda	1+_pc
	adc	#0
	sta	r1
	jsr	_read6502
	sta	r0
	tax
	lda	#0
	ora	r16
	pha
	txa
	ora	r17
	tax
	pla
	sta	r31
	txa
	sta	r1
	lda	#0
	sta	r0
	lda	r31
	clc
	adc	#1
	sta	r18
	txa
	adc	#0
	sta	r19
	lda	#0
	sta	r19
	lda	r18
	ora	r0
	sta	r18
	lda	r19
	ora	r1
	sta	r19
	lda	r31
	sta	r0
	stx	r1
	jsr	_read6502
	sta	r31
	sta	r16
	lda	#0
	sta	r17
	lda	r19
	sta	r1
	lda	r18
	sta	r0
	jsr	_read6502
	sta	r0
	tax
	lda	#0
	ora	r16
	sta	_ea
	txa
	ora	r17
	sta	1+_ea
	inc	_pc
	bne	l78
	inc	1+_pc
l78:
	inc	_pc
	bne	l79
	inc	1+_pc
l79:
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
	section	"text0"
l80:
	lda	r16
	pha
	lda	r17
	pha
	lda	r18
	pha
	lda	r19
	pha
	lda	1+_pc
	sta	r1
	lda	_pc
	sta	r0
	inc	_pc
	bne	l85
	inc	1+_pc
l85:
	jsr	_read6502
	sta	r31
	sta	r0
	lda	#0
	sta	r1
	lda	_x
	ldx	#0
	clc
	adc	r0
	pha
	txa
	adc	r1
	tax
	pla
	sta	r18
	lda	#0
	sta	r19
	sta	r1
	lda	r18
	sta	r0
	jsr	_read6502
	sta	r31
	sta	r16
	lda	#0
	sta	r17
	ldx	r19
	lda	r18
	clc
	adc	#1
	bcc	l86
	inx
l86:
	sta	r0
	lda	#0
	sta	r1
	jsr	_read6502
	sta	r0
	tax
	lda	#0
	ora	r16
	sta	_ea
	txa
	ora	r17
	sta	1+_ea
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
	section	"text0"
l87:
	lda	r16
	pha
	lda	r17
	pha
	lda	r18
	pha
	lda	r19
	pha
	lda	1+_pc
	sta	r1
	lda	_pc
	sta	r0
	inc	_pc
	bne	l91
	inc	1+_pc
l91:
	jsr	_read6502
	sta	r0
	ldx	#0
	sta	r31
	txa
	sta	r1
	txa
	sta	r0
	lda	r31
	clc
	adc	#1
	sta	r18
	txa
	adc	#0
	sta	r19
	txa
	sta	r19
	lda	r18
	ora	r0
	sta	r18
	lda	r19
	ora	r1
	sta	r19
	lda	r31
	sta	r0
	stx	r1
	jsr	_read6502
	sta	r31
	sta	r16
	lda	#0
	sta	r17
	lda	r19
	sta	r1
	lda	r18
	sta	r0
	jsr	_read6502
	sta	r0
	tax
	lda	#0
	ora	r16
	sta	_ea
	txa
	ora	r17
	sta	1+_ea
	lda	_y
	ldx	#0
	clc
	adc	_ea
	sta	_ea
	txa
	adc	1+_ea
	sta	1+_ea
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
	section	"text0"
l92:
	lda	_opcode
	ldx	#0
	stx	r31
	asl
	rol	r31
	ldx	r31
	clc
	adc	#<(l17)
	sta	r2
	txa
	adc	#>(l17)
	sta	r3
	ldy	#1
	lda	(r2),y
	cmp	#>(l22)
	bne	l96
	dey
	lda	(r2),y
	cmp	#<(l22)
	bne	l96
	lda	_a
	ldx	#0
	rts
l96:
	lda	1+_ea
	sta	r1
	lda	_ea
	sta	r0
	jsr	_read6502
	sta	r0
	ldx	#0
l97:
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l98:
	lda	r16
	pha
	lda	r17
	pha
	lda	1+_ea
	sta	r1
	lda	_ea
	sta	r0
	jsr	_read6502
	sta	r31
	sta	r16
	lda	#0
	sta	r17
	lda	_ea
	clc
	adc	#1
	sta	r0
	lda	1+_ea
	adc	#0
	sta	r1
	jsr	_read6502
	sta	r0
	tax
	lda	#0
	ora	r16
	pha
	txa
	ora	r17
	tax
	pla
	sta	r31
	pla
	sta	r17
	pla
	sta	r16
	lda	r31
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l101:
	lda	r1
	sta	r5
	lda	r0
	sta	r4
	lda	_opcode
	ldx	#0
	stx	r31
	asl
	rol	r31
	ldx	r31
	clc
	adc	#<(l17)
	sta	r2
	txa
	adc	#>(l17)
	sta	r3
	ldy	#1
	lda	(r2),y
	cmp	#>(l22)
	bne	l105
	dey
	lda	(r2),y
	cmp	#<(l22)
	bne	l105
	lda	r4
	and	#255
	sta	_a
	rts
l105:
	lda	1+_ea
	sta	1+_last_write_ea
	lda	_ea
	sta	_last_write_ea
	ldx	1+_ea
	lda	#0
	cpx	#80
	bne	l108
	cmp	#0
	bne	l108
	inc	_write_50xx_count
l108:
	lda	r4
	and	#255
	sta	r31
	lda	1+_ea
	sta	r1
	lda	_ea
	sta	r0
	lda	r31
	sta	r2
	jsr	_write6502
l106:
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l109:
	lda	#1
	sta	_penaltyop
	jsr	l92
	sta	_value
	stx	1+_value
	lda	_a
	sta	r6
	lda	#0
	sta	r7
	lda	r6
	clc
	adc	_value
	sta	r4
	lda	r7
	adc	1+_value
	sta	r5
	lda	_status
	ldx	#0
	and	#1
	clc
	adc	r4
	sta	_result
	txa
	adc	r5
	sta	1+_result
	lda	_result
	sta	r1
	and	#255
	beq	l113
	lda	_status
	and	#253
	sta	_status
	jmp	l114
l113:
	lda	_status
	ora	#2
	sta	_status
l114:
	lda	r6
	eor	r1
	sta	r0
	lda	_value
	eor	r1
	and	r0
	and	#128
	beq	l116
	lda	_status
	ora	#64
	sta	_status
	jmp	l117
l116:
	lda	_status
	and	#191
	sta	_status
l117:
	lda	r1
	and	#128
	beq	l119
	lda	_status
	ora	#128
	sta	_status
	jmp	l120
l119:
	lda	_status
	and	#127
	sta	_status
l120:
	lda	_status
	and	#8
	beq	l122
	ldx	1+_result
	lda	_result
	clc
	adc	#102
	bcc	l128
	inx
l128:
	eor	r6
	pha
	txa
	eor	r7
	tax
	pla
	eor	_value
	pha
	txa
	eor	1+_value
	tax
	pla
	stx	r31
	lsr	r31
	ror
	lsr	r31
	ror
	lsr	r31
	ror
	ldx	r31
	and	#34
	ldx	#0
	sta	r0
	stx	r1
	txa
	sta	r3
	lda	#3
	sta	r2
	jsr	___muluint16
	clc
	adc	_result
	sta	_result
	txa
	adc	1+_result
	sta	1+_result
l122:
	ldx	1+_result
	lda	#0
	cpx	#0
	bne	l129
	cmp	#0
	beq	l124
l129:
	lda	_status
	ora	#1
	sta	_status
	jmp	l125
l124:
	lda	_status
	and	#254
	sta	_status
l125:
	lda	_result
	and	#255
	sta	_a
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l130:
	lda	#1
	sta	_penaltyop
	jsr	l92
	sta	_value
	stx	1+_value
	lda	_a
	ldx	#0
	and	_value
	sta	_result
	txa
	and	1+_value
	sta	1+_result
	lda	_result
	sta	r1
	and	#255
	sta	r0
	cmp	#0
	beq	l134
	lda	_status
	and	#253
	sta	_status
	jmp	l135
l134:
	lda	_status
	ora	#2
	sta	_status
l135:
	lda	r1
	and	#128
	beq	l137
	lda	_status
	ora	#128
	sta	_status
	jmp	l138
l137:
	lda	_status
	and	#127
	sta	_status
l138:
	lda	r0
	sta	_a
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l139:
	jsr	l92
	sta	_value
	stx	1+_value
	lda	_value
	stx	r31
	asl
	rol	r31
	ldx	r31
	sta	_result
	stx	1+_result
	lda	#0
	cpx	#0
	bne	l152
	cmp	#0
	beq	l143
l152:
	lda	_status
	ora	#1
	sta	_status
	jmp	l144
l143:
	lda	_status
	and	#254
	sta	_status
l144:
	lda	_result
	tax
	and	#255
	beq	l146
	lda	_status
	and	#253
	sta	_status
	jmp	l147
l146:
	lda	_status
	ora	#2
	sta	_status
l147:
	txa
	and	#128
	beq	l149
	lda	_status
	ora	#128
	sta	_status
	jmp	l150
l149:
	lda	_status
	and	#127
	sta	_status
l150:
	lda	1+_result
	sta	r1
	lda	_result
	sta	r0
	jmp	l101
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l153:
	lda	_status
	and	#1
	bne	l157
	lda	1+_pc
	sta	1+_oldpc
	lda	_pc
	sta	_oldpc
	lda	_pc
	clc
	adc	_reladdr
	sta	_pc
	lda	1+_pc
	adc	1+_reladdr
	sta	1+_pc
l157:
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l158:
	lda	_status
	and	#1
	cmp	#1
	bne	l162
	lda	1+_pc
	sta	1+_oldpc
	lda	_pc
	sta	_oldpc
	lda	_pc
	clc
	adc	_reladdr
	sta	_pc
	lda	1+_pc
	adc	1+_reladdr
	sta	1+_pc
l162:
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l163:
	lda	_status
	and	#2
	cmp	#2
	bne	l167
	lda	1+_pc
	sta	1+_oldpc
	lda	_pc
	sta	_oldpc
	lda	_pc
	clc
	adc	_reladdr
	sta	_pc
	lda	1+_pc
	adc	1+_reladdr
	sta	1+_pc
l167:
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l168:
	jsr	l92
	sta	_value
	stx	1+_value
	lda	_a
	ldx	#0
	and	_value
	sta	_result
	txa
	and	1+_value
	sta	1+_result
	lda	_result
	and	#255
	beq	l172
	lda	_status
	and	#253
	sta	_status
	jmp	l173
l172:
	lda	_status
	ora	#2
	sta	_status
l173:
	lda	_status
	and	#63
	sta	r0
	lda	_value
	and	#192
	ora	r0
	sta	_status
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l174:
	lda	_status
	and	#128
	cmp	#128
	bne	l178
	lda	1+_pc
	sta	1+_oldpc
	lda	_pc
	sta	_oldpc
	lda	_pc
	clc
	adc	_reladdr
	sta	_pc
	lda	1+_pc
	adc	1+_reladdr
	sta	1+_pc
l178:
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l179:
	lda	_status
	and	#2
	bne	l183
	lda	1+_pc
	sta	1+_oldpc
	lda	_pc
	sta	_oldpc
	lda	_pc
	clc
	adc	_reladdr
	sta	_pc
	lda	1+_pc
	adc	1+_reladdr
	sta	1+_pc
l183:
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l184:
	lda	_status
	and	#128
	bne	l188
	lda	1+_pc
	sta	1+_oldpc
	lda	_pc
	sta	_oldpc
	lda	_pc
	clc
	adc	_reladdr
	sta	_pc
	lda	1+_pc
	adc	1+_reladdr
	sta	1+_pc
l188:
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l189:
	lda	r16
	pha
	lda	r17
	pha
	inc	_pc
	bne	l193
	inc	1+_pc
l193:
	lda	1+_pc
	sta	r1
	lda	_pc
	sta	r0
	jsr	_push16
	lda	_status
	ora	#16
	sta	r0
	jsr	_push8
	lda	_status
	ora	#4
	sta	_status
	lda	#255
	sta	r1
	lda	#254
	sta	r0
	jsr	_read6502
	sta	r31
	sta	r16
	lda	#0
	sta	r17
	lda	#255
	sta	r1
	sta	r0
	jsr	_read6502
	sta	r0
	tax
	lda	#0
	ora	r16
	sta	_pc
	txa
	ora	r17
	sta	1+_pc
	pla
	sta	r17
	pla
	sta	r16
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l194:
	lda	_status
	and	#64
	bne	l198
	lda	1+_pc
	sta	1+_oldpc
	lda	_pc
	sta	_oldpc
	lda	_pc
	clc
	adc	_reladdr
	sta	_pc
	lda	1+_pc
	adc	1+_reladdr
	sta	1+_pc
l198:
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l199:
	lda	_status
	and	#64
	cmp	#64
	bne	l203
	lda	1+_pc
	sta	1+_oldpc
	lda	_pc
	sta	_oldpc
	lda	_pc
	clc
	adc	_reladdr
	sta	_pc
	lda	1+_pc
	adc	1+_reladdr
	sta	1+_pc
l203:
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l204:
	lda	_status
	and	#254
	sta	_status
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l207:
	lda	_status
	and	#247
	sta	_status
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l210:
	lda	_status
	and	#251
	sta	_status
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l213:
	lda	_status
	and	#191
	sta	_status
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l216:
	lda	#1
	sta	_penaltyop
	jsr	l92
	sta	_value
	stx	1+_value
	lda	_a
	ldx	#0
	sec
	sbc	_value
	sta	_result
	txa
	sbc	1+_value
	sta	1+_result
	lda	_value
	and	#255
	sta	r0
	lda	_a
	cmp	r0
	bcc	l220
	lda	_status
	ora	#1
	sta	_status
	jmp	l221
l220:
	lda	_status
	and	#254
	sta	_status
l221:
	lda	_a
	cmp	r0
	bne	l223
	lda	_status
	ora	#2
	sta	_status
	jmp	l224
l223:
	lda	_status
	and	#253
	sta	_status
l224:
	lda	_result
	and	#128
	beq	l226
	lda	_status
	ora	#128
	sta	_status
	rts
l226:
	lda	_status
	and	#127
	sta	_status
l227:
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l228:
	jsr	l92
	sta	_value
	stx	1+_value
	lda	_x
	ldx	#0
	sec
	sbc	_value
	sta	_result
	txa
	sbc	1+_value
	sta	1+_result
	lda	_value
	and	#255
	sta	r0
	lda	_x
	cmp	r0
	bcc	l232
	lda	_status
	ora	#1
	sta	_status
	jmp	l233
l232:
	lda	_status
	and	#254
	sta	_status
l233:
	lda	_x
	cmp	r0
	bne	l235
	lda	_status
	ora	#2
	sta	_status
	jmp	l236
l235:
	lda	_status
	and	#253
	sta	_status
l236:
	lda	_result
	and	#128
	beq	l238
	lda	_status
	ora	#128
	sta	_status
	rts
l238:
	lda	_status
	and	#127
	sta	_status
l239:
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l240:
	jsr	l92
	sta	_value
	stx	1+_value
	lda	_y
	ldx	#0
	sec
	sbc	_value
	sta	_result
	txa
	sbc	1+_value
	sta	1+_result
	lda	_value
	and	#255
	sta	r0
	lda	_y
	cmp	r0
	bcc	l244
	lda	_status
	ora	#1
	sta	_status
	jmp	l245
l244:
	lda	_status
	and	#254
	sta	_status
l245:
	lda	_y
	cmp	r0
	bne	l247
	lda	_status
	ora	#2
	sta	_status
	jmp	l248
l247:
	lda	_status
	and	#253
	sta	_status
l248:
	lda	_result
	and	#128
	beq	l250
	lda	_status
	ora	#128
	sta	_status
	rts
l250:
	lda	_status
	and	#127
	sta	_status
l251:
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l252:
	jsr	l92
	sta	_value
	stx	1+_value
	lda	_value
	sec
	sbc	#1
	sta	_result
	lda	1+_value
	sbc	#0
	sta	1+_result
	lda	_result
	tax
	and	#255
	beq	l256
	lda	_status
	and	#253
	sta	_status
	jmp	l257
l256:
	lda	_status
	ora	#2
	sta	_status
l257:
	txa
	and	#128
	beq	l259
	lda	_status
	ora	#128
	sta	_status
	jmp	l260
l259:
	lda	_status
	and	#127
	sta	_status
l260:
	lda	1+_result
	sta	r1
	lda	_result
	sta	r0
	jmp	l101
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l261:
	dec	_x
	lda	_x
	and	#255
	beq	l265
	lda	_status
	and	#253
	sta	_status
	jmp	l266
l265:
	lda	_status
	ora	#2
	sta	_status
l266:
	lda	_x
	and	#128
	beq	l268
	lda	_status
	ora	#128
	sta	_status
	rts
l268:
	lda	_status
	and	#127
	sta	_status
l269:
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l270:
	dec	_y
	lda	_y
	and	#255
	beq	l274
	lda	_status
	and	#253
	sta	_status
	jmp	l275
l274:
	lda	_status
	ora	#2
	sta	_status
l275:
	lda	_y
	and	#128
	beq	l277
	lda	_status
	ora	#128
	sta	_status
	rts
l277:
	lda	_status
	and	#127
	sta	_status
l278:
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l279:
	lda	#1
	sta	_penaltyop
	jsr	l92
	sta	_value
	stx	1+_value
	lda	_a
	ldx	#0
	eor	_value
	sta	_result
	txa
	eor	1+_value
	sta	1+_result
	lda	_result
	sta	r1
	and	#255
	sta	r0
	cmp	#0
	beq	l283
	lda	_status
	and	#253
	sta	_status
	jmp	l284
l283:
	lda	_status
	ora	#2
	sta	_status
l284:
	lda	r1
	and	#128
	beq	l286
	lda	_status
	ora	#128
	sta	_status
	jmp	l287
l286:
	lda	_status
	and	#127
	sta	_status
l287:
	lda	r0
	sta	_a
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l288:
	jsr	l92
	sta	_value
	stx	1+_value
	lda	_value
	clc
	adc	#1
	sta	_result
	lda	1+_value
	adc	#0
	sta	1+_result
	lda	_result
	tax
	and	#255
	beq	l292
	lda	_status
	and	#253
	sta	_status
	jmp	l293
l292:
	lda	_status
	ora	#2
	sta	_status
l293:
	txa
	and	#128
	beq	l295
	lda	_status
	ora	#128
	sta	_status
	jmp	l296
l295:
	lda	_status
	and	#127
	sta	_status
l296:
	lda	1+_result
	sta	r1
	lda	_result
	sta	r0
	jmp	l101
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l297:
	inc	_x
	lda	_x
	and	#255
	beq	l301
	lda	_status
	and	#253
	sta	_status
	jmp	l302
l301:
	lda	_status
	ora	#2
	sta	_status
l302:
	lda	_x
	and	#128
	beq	l304
	lda	_status
	ora	#128
	sta	_status
	rts
l304:
	lda	_status
	and	#127
	sta	_status
l305:
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l306:
	inc	_y
	lda	_y
	and	#255
	beq	l310
	lda	_status
	and	#253
	sta	_status
	jmp	l311
l310:
	lda	_status
	ora	#2
	sta	_status
l311:
	lda	_y
	and	#128
	beq	l313
	lda	_status
	ora	#128
	sta	_status
	rts
l313:
	lda	_status
	and	#127
	sta	_status
l314:
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l315:
	lda	1+_ea
	sta	1+_pc
	lda	_ea
	sta	_pc
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l318:
	lda	_pc
	sec
	sbc	#1
	sta	r0
	lda	1+_pc
	sbc	#0
	sta	r1
	jsr	_push16
	lda	1+_ea
	sta	1+_pc
	lda	_ea
	sta	_pc
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l321:
	lda	#1
	sta	_penaltyop
	jsr	l92
	sta	_value
	stx	1+_value
	lda	_value
	and	#255
	tax
	stx	_a
	cpx	#0
	beq	l325
	lda	_status
	and	#253
	sta	_status
	jmp	l326
l325:
	lda	_status
	ora	#2
	sta	_status
l326:
	txa
	and	#128
	beq	l328
	lda	_status
	ora	#128
	sta	_status
	rts
l328:
	lda	_status
	and	#127
	sta	_status
l329:
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l330:
	lda	#1
	sta	_penaltyop
	jsr	l92
	sta	_value
	stx	1+_value
	lda	_value
	and	#255
	tax
	stx	_x
	cpx	#0
	beq	l334
	lda	_status
	and	#253
	sta	_status
	jmp	l335
l334:
	lda	_status
	ora	#2
	sta	_status
l335:
	txa
	and	#128
	beq	l337
	lda	_status
	ora	#128
	sta	_status
	rts
l337:
	lda	_status
	and	#127
	sta	_status
l338:
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l339:
	lda	#1
	sta	_penaltyop
	jsr	l92
	sta	_value
	stx	1+_value
	lda	_value
	and	#255
	tax
	stx	_y
	cpx	#0
	beq	l343
	lda	_status
	and	#253
	sta	_status
	jmp	l344
l343:
	lda	_status
	ora	#2
	sta	_status
l344:
	txa
	and	#128
	beq	l346
	lda	_status
	ora	#128
	sta	_status
	rts
l346:
	lda	_status
	and	#127
	sta	_status
l347:
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l348:
	jsr	l92
	sta	_value
	stx	1+_value
	stx	r30
	ldx	1+_value
	lda	_value
	stx	r31
	lsr	r31
	ror
	ldx	r31
	sta	_result
	stx	1+_result
	ldx	r30
	lda	_value
	and	#1
	beq	l352
	lda	_status
	ora	#1
	sta	_status
	jmp	l353
l352:
	lda	_status
	and	#254
	sta	_status
l353:
	lda	_result
	tax
	and	#255
	beq	l355
	lda	_status
	and	#253
	sta	_status
	jmp	l356
l355:
	lda	_status
	ora	#2
	sta	_status
l356:
	txa
	and	#128
	beq	l358
	lda	_status
	ora	#128
	sta	_status
	jmp	l359
l358:
	lda	_status
	and	#127
	sta	_status
l359:
	lda	1+_result
	sta	r1
	lda	_result
	sta	r0
	jmp	l101
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l360:
	lda	_opcode
	cmp	#28
	beq	l364
	lda	_opcode
	cmp	#60
	beq	l364
	lda	_opcode
	cmp	#92
	beq	l364
	lda	_opcode
	cmp	#124
	beq	l364
	lda	_opcode
	cmp	#220
	beq	l364
	lda	_opcode
	cmp	#252
	bne	l363
l364:
	lda	#1
	sta	_penaltyop
l363:
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l370:
	lda	#1
	sta	_penaltyop
	jsr	l92
	sta	_value
	stx	1+_value
	lda	_a
	ldx	#0
	ora	_value
	sta	_result
	txa
	ora	1+_value
	sta	1+_result
	lda	_result
	sta	r1
	and	#255
	sta	r0
	cmp	#0
	beq	l374
	lda	_status
	and	#253
	sta	_status
	jmp	l375
l374:
	lda	_status
	ora	#2
	sta	_status
l375:
	lda	r1
	and	#128
	beq	l377
	lda	_status
	ora	#128
	sta	_status
	jmp	l378
l377:
	lda	_status
	and	#127
	sta	_status
l378:
	lda	r0
	sta	_a
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l379:
	lda	_a
	sta	r0
	jmp	_push8
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l382:
	lda	_status
	ora	#16
	sta	r0
	jmp	_push8
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l385:
	jsr	_pull8
	sta	_a
	and	#255
	beq	l389
	lda	_status
	and	#253
	sta	_status
	jmp	l390
l389:
	lda	_status
	ora	#2
	sta	_status
l390:
	lda	_a
	and	#128
	beq	l392
	lda	_status
	ora	#128
	sta	_status
	rts
l392:
	lda	_status
	and	#127
	sta	_status
l393:
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l394:
	jsr	_pull8
	ora	#32
	sta	_status
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l397:
	jsr	l92
	sta	_value
	stx	1+_value
	lda	_value
	stx	r31
	asl
	rol	r31
	ldx	r31
	sta	r2
	stx	r3
	lda	_status
	ldx	#0
	and	#1
	ora	r2
	sta	_result
	txa
	ora	r3
	sta	1+_result
	tax
	lda	#0
	cpx	#0
	bne	l410
	cmp	#0
	beq	l401
l410:
	lda	_status
	ora	#1
	sta	_status
	jmp	l402
l401:
	lda	_status
	and	#254
	sta	_status
l402:
	lda	_result
	tax
	and	#255
	beq	l404
	lda	_status
	and	#253
	sta	_status
	jmp	l405
l404:
	lda	_status
	ora	#2
	sta	_status
l405:
	txa
	and	#128
	beq	l407
	lda	_status
	ora	#128
	sta	_status
	jmp	l408
l407:
	lda	_status
	and	#127
	sta	_status
l408:
	lda	1+_result
	sta	r1
	lda	_result
	sta	r0
	jmp	l101
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l411:
	jsr	l92
	sta	_value
	stx	1+_value
	lda	_value
	stx	r31
	lsr	r31
	ror
	ldx	r31
	sta	r2
	stx	r3
	lda	_status
	ldx	#0
	and	#1
	stx	r31
	asl
	rol	r31
	asl
	rol	r31
	asl
	rol	r31
	asl
	rol	r31
	asl
	rol	r31
	asl
	rol	r31
	asl
	rol	r31
	ldx	r31
	ora	r2
	sta	_result
	txa
	ora	r3
	sta	1+_result
	lda	_value
	and	#1
	beq	l415
	lda	_status
	ora	#1
	sta	_status
	jmp	l416
l415:
	lda	_status
	and	#254
	sta	_status
l416:
	lda	_result
	tax
	and	#255
	beq	l418
	lda	_status
	and	#253
	sta	_status
	jmp	l419
l418:
	lda	_status
	ora	#2
	sta	_status
l419:
	txa
	and	#128
	beq	l421
	lda	_status
	ora	#128
	sta	_status
	jmp	l422
l421:
	lda	_status
	and	#127
	sta	_status
l422:
	lda	1+_result
	sta	r1
	lda	_result
	sta	r0
	jmp	l101
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l423:
	jsr	_pull8
	sta	_status
	jsr	_pull16
	sta	_value
	stx	1+_value
	txa
	sta	1+_pc
	lda	_value
	sta	_pc
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l426:
	jsr	_pull16
	sta	_value
	stx	1+_value
	lda	_value
	clc
	adc	#1
	sta	_pc
	lda	1+_value
	adc	#0
	sta	1+_pc
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l429:
	lda	#1
	sta	_penaltyop
	jsr	l92
	eor	#255
	sta	_value
	txa
	sta	1+_value
	lda	_status
	and	#8
	beq	l433
	lda	_value
	sec
	sbc	#102
	sta	_value
	lda	1+_value
	sbc	#0
	sta	1+_value
l433:
	lda	_a
	sta	r6
	lda	#0
	sta	r7
	lda	r6
	clc
	adc	_value
	sta	r4
	lda	r7
	adc	1+_value
	sta	r5
	lda	_status
	ldx	#0
	and	#1
	clc
	adc	r4
	sta	_result
	txa
	adc	r5
	sta	1+_result
	lda	_result
	sta	r1
	and	#255
	beq	l435
	lda	_status
	and	#253
	sta	_status
	jmp	l436
l435:
	lda	_status
	ora	#2
	sta	_status
l436:
	lda	r6
	eor	r1
	sta	r0
	lda	_value
	eor	r1
	and	r0
	and	#128
	beq	l438
	lda	_status
	ora	#64
	sta	_status
	jmp	l439
l438:
	lda	_status
	and	#191
	sta	_status
l439:
	lda	r1
	and	#128
	beq	l441
	lda	_status
	ora	#128
	sta	_status
	jmp	l442
l441:
	lda	_status
	and	#127
	sta	_status
l442:
	lda	_status
	and	#8
	beq	l444
	ldx	1+_result
	lda	_result
	clc
	adc	#102
	bcc	l450
	inx
l450:
	eor	r6
	pha
	txa
	eor	r7
	tax
	pla
	eor	_value
	pha
	txa
	eor	1+_value
	tax
	pla
	stx	r31
	lsr	r31
	ror
	lsr	r31
	ror
	lsr	r31
	ror
	ldx	r31
	and	#34
	ldx	#0
	sta	r0
	stx	r1
	txa
	sta	r3
	lda	#3
	sta	r2
	jsr	___muluint16
	clc
	adc	_result
	sta	_result
	txa
	adc	1+_result
	sta	1+_result
l444:
	ldx	1+_result
	lda	#0
	cpx	#0
	bne	l451
	cmp	#0
	beq	l446
l451:
	lda	_status
	ora	#1
	sta	_status
	jmp	l447
l446:
	lda	_status
	and	#254
	sta	_status
l447:
	lda	_result
	and	#255
	sta	_a
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l452:
	lda	_status
	ora	#1
	sta	_status
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l455:
	lda	_status
	ora	#8
	sta	_status
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l458:
	lda	_status
	ora	#4
	sta	_status
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l461:
	lda	_a
	sta	r0
	lda	#0
	sta	r1
	jmp	l101
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l464:
	lda	_x
	sta	r0
	lda	#0
	sta	r1
	jmp	l101
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l467:
	lda	_y
	sta	r0
	lda	#0
	sta	r1
	jmp	l101
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l470:
	lda	_a
	sta	_x
	lda	_a
	and	#255
	beq	l474
	lda	_status
	and	#253
	sta	_status
	jmp	l475
l474:
	lda	_status
	ora	#2
	sta	_status
l475:
	lda	_a
	and	#128
	beq	l477
	lda	_status
	ora	#128
	sta	_status
	rts
l477:
	lda	_status
	and	#127
	sta	_status
l478:
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l479:
	lda	_a
	sta	_y
	lda	_a
	and	#255
	beq	l483
	lda	_status
	and	#253
	sta	_status
	jmp	l484
l483:
	lda	_status
	ora	#2
	sta	_status
l484:
	lda	_a
	and	#128
	beq	l486
	lda	_status
	ora	#128
	sta	_status
	rts
l486:
	lda	_status
	and	#127
	sta	_status
l487:
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l488:
	lda	_sp
	sta	_x
	lda	_sp
	and	#255
	beq	l492
	lda	_status
	and	#253
	sta	_status
	jmp	l493
l492:
	lda	_status
	ora	#2
	sta	_status
l493:
	lda	_sp
	and	#128
	beq	l495
	lda	_status
	ora	#128
	sta	_status
	rts
l495:
	lda	_status
	and	#127
	sta	_status
l496:
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l497:
	lda	_x
	sta	_a
	lda	_x
	and	#255
	beq	l501
	lda	_status
	and	#253
	sta	_status
	jmp	l502
l501:
	lda	_status
	ora	#2
	sta	_status
l502:
	lda	_x
	and	#128
	beq	l504
	lda	_status
	ora	#128
	sta	_status
	rts
l504:
	lda	_status
	and	#127
	sta	_status
l505:
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l506:
	lda	_x
	sta	_sp
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
l509:
	lda	_y
	sta	_a
	lda	_y
	and	#255
	beq	l513
	lda	_status
	and	#253
	sta	_status
	jmp	l514
l513:
	lda	_status
	ora	#2
	sta	_status
l514:
	lda	_y
	and	#128
	beq	l516
	lda	_status
	ora	#128
	sta	_status
	rts
l516:
	lda	_status
	and	#127
	sta	_status
l517:
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
	global	_nmi6502
_nmi6502:
	lda	r16
	pha
	lda	r17
	pha
	lda	1+_pc
	sta	r1
	lda	_pc
	sta	r0
	jsr	_push16
	lda	_status
	sta	r0
	jsr	_push8
	lda	_status
	ora	#4
	sta	_status
	lda	#255
	sta	r1
	lda	#250
	sta	r0
	jsr	_read6502
	sta	r31
	sta	r16
	lda	#0
	sta	r17
	lda	#255
	sta	r1
	lda	#251
	sta	r0
	jsr	_read6502
	sta	r0
	tax
	lda	#0
	ora	r16
	sta	_pc
	txa
	ora	r17
	sta	1+_pc
	pla
	sta	r17
	pla
	sta	r16
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
	global	_irq6502
_irq6502:
	lda	r16
	pha
	lda	r17
	pha
	lda	1+_pc
	sta	r1
	lda	_pc
	sta	r0
	jsr	_push16
	lda	_status
	sta	r0
	jsr	_push8
	lda	_status
	ora	#4
	sta	_status
	lda	#255
	sta	r1
	lda	#254
	sta	r0
	jsr	_read6502
	sta	r31
	sta	r16
	lda	#0
	sta	r17
	lda	#255
	sta	r1
	sta	r0
	jsr	_read6502
	sta	r0
	tax
	lda	#0
	ora	r16
	sta	_pc
	txa
	ora	r17
	sta	1+_pc
	pla
	sta	r17
	pla
	sta	r16
	rts
; stacksize=0+??
;vcprmin=10000
	section	"text0"
	global	_exec6502
_exec6502:
	lda	r16
	pha
	lda	r17
	pha
	clc
	lda	_clockgoal6502
	adc	btmp0
	sta	_clockgoal6502
	lda	1+_clockgoal6502
	adc	btmp0+1
	sta	1+_clockgoal6502
	lda	2+_clockgoal6502
	adc	btmp0+2
	sta	2+_clockgoal6502
	lda	3+_clockgoal6502
	adc	btmp0+3
	sta	3+_clockgoal6502
	lda	3+_clockticks6502
	cmp	3+_clockgoal6502
	bcc	l541
	bne	l533
	lda	2+_clockticks6502
	cmp	2+_clockgoal6502
	bcc	l541
	bne	l533
	lda	1+_clockticks6502
	cmp	1+_clockgoal6502
	bcc	l541
	bne	l533
	lda	_clockticks6502
	cmp	_clockgoal6502
	bcs	l533
l541:
l532:
	lda	1+_pc
	sta	r1
	lda	_pc
	sta	r0
	inc	_pc
	bne	l542
	inc	1+_pc
l542:
	jsr	_read6502
	sta	_opcode
	lda	_status
	ora	#32
	sta	_status
	lda	#0
	sta	_penaltyop
	sta	_penaltyaddr
	lda	_opcode
	ldx	#0
	stx	r31
	asl
	rol	r31
	ldx	r31
	clc
	adc	#<(l17)
	sta	r16
	txa
	adc	#>(l17)
	sta	r17
	lda	r16
	sta	r14
	lda	r17
	sta	r15
	ldy	#1
	lda	(r14),y
	sta	r17
	dey
	lda	(r14),y
	sta	r16
	jsr	l543
	lda	_opcode
	ldx	#0
	stx	r31
	asl
	rol	r31
	ldx	r31
	clc
	adc	#<(l18)
	sta	r16
	txa
	adc	#>(l18)
	sta	r17
	lda	r16
	sta	r14
	lda	r17
	sta	r15
	ldy	#1
	lda	(r14),y
	sta	r17
	dey
	lda	(r14),y
	sta	r16
	jsr	l544
	inc	_instructions
	bne	l545
	inc	1+_instructions
	bne	l545
	inc	2+_instructions
	bne	l545
	inc	3+_instructions
l545:
	lda	_callexternal
	beq	l528
	lda	_loopexternal
	sta	r14
	lda	1+_loopexternal
	sta	r15
	jsr	l546
l528:
	lda	3+_clockticks6502
	cmp	3+_clockgoal6502
	bcc	l532
	bne	l547
	lda	2+_clockticks6502
	cmp	2+_clockgoal6502
	bcc	l532
	bne	l547
	lda	1+_clockticks6502
	cmp	1+_clockgoal6502
	bcc	l532
	bne	l547
	lda	_clockticks6502
	cmp	_clockgoal6502
	bcc	l532
l547:
l533:
	pla
	sta	r17
	pla
	sta	r16
	rts
l546:
	jmp	(r14)
l544:
	jmp	(r16)
l543:
	jmp	(r16)
; stacksize=0+??
;vcprmin=10000
	section	"text0"
	global	_step6502
_step6502:
	lda	r16
	pha
	lda	r17
	pha
	lda	1+_pc
	sta	r1
	lda	_pc
	sta	r0
	inc	_pc
	bne	l557
	inc	1+_pc
l557:
	jsr	_read6502
	sta	_opcode
	lda	_status
	ora	#32
	sta	_status
	lda	_opcode
	ldx	#0
	stx	r31
	asl
	rol	r31
	ldx	r31
	clc
	adc	#<(l17)
	sta	r16
	txa
	adc	#>(l17)
	sta	r17
	lda	r16
	sta	r14
	lda	r17
	sta	r15
	ldy	#1
	lda	(r14),y
	sta	r17
	dey
	lda	(r14),y
	sta	r16
	jsr	l558
	lda	_opcode
	ldx	#0
	stx	r31
	asl
	rol	r31
	ldx	r31
	clc
	adc	#<(l18)
	sta	r16
	txa
	adc	#>(l18)
	sta	r17
	lda	r16
	sta	r14
	lda	r17
	sta	r15
	ldy	#1
	lda	(r14),y
	sta	r17
	dey
	lda	(r14),y
	sta	r16
	jsr	l559
	inc	_instructions
	bne	l560
	inc	1+_instructions
	bne	l560
	inc	2+_instructions
	bne	l560
	inc	3+_instructions
l560:
	lda	_callexternal
	beq	l551
	lda	_loopexternal
	sta	r14
	lda	1+_loopexternal
	sta	r15
	jsr	l561
l551:
	pla
	sta	r17
	pla
	sta	r16
	rts
l561:
	jmp	(r14)
l559:
	jmp	(r16)
l558:
	jmp	(r16)
; stacksize=0+??
;vcprmin=10000
	section	"text0"
	global	_interpret_6502
_interpret_6502:
	lda	r16
	pha
	lda	r17
	pha
	inc	_interpret_count
	bne	l569
	inc	1+_interpret_count
l569:
	lda	1+_pc
	sta	r1
	lda	_pc
	sta	r0
	inc	_pc
	bne	l570
	inc	1+_pc
l570:
	jsr	_read6502
	sta	_opcode
	sta	_last_interpreted_opcode
	lda	_status
	ora	#32
	sta	_status
	lda	_opcode
	ldx	#0
	stx	r31
	asl
	rol	r31
	ldx	r31
	clc
	adc	#<(l17)
	sta	r16
	txa
	adc	#>(l17)
	sta	r17
	lda	r16
	sta	r14
	lda	r17
	sta	r15
	ldy	#1
	lda	(r14),y
	sta	r17
	dey
	lda	(r14),y
	sta	r16
	jsr	l571
	lda	_opcode
	ldx	#0
	stx	r31
	asl
	rol	r31
	ldx	r31
	clc
	adc	#<(l18)
	sta	r16
	txa
	adc	#>(l18)
	sta	r17
	lda	r16
	sta	r14
	lda	r17
	sta	r15
	ldy	#1
	lda	(r14),y
	sta	r17
	dey
	lda	(r14),y
	sta	r16
	jsr	l572
	inc	_instructions
	bne	l573
	inc	1+_instructions
	bne	l573
	inc	2+_instructions
	bne	l573
	inc	3+_instructions
l573:
	pla
	sta	r17
	pla
	sta	r16
	rts
l572:
	jmp	(r16)
l571:
	jmp	(r16)
; stacksize=0+??
;vcprmin=10000
	section	"text0"
	global	_hookexternal
_hookexternal:
	lda	r1
	bne	l580
	lda	r0
	beq	l577
l580:
	lda	r1
	sta	1+_loopexternal
	lda	r0
	sta	_loopexternal
	lda	#1
	sta	_callexternal
	rts
l577:
	lda	#0
	sta	_callexternal
l578:
	rts
; stacksize=0+??
	global	_instructions
	zpage	_instructions
	section	zpage
_instructions:
	word	0,0
	global	_clockticks6502
	zpage	_clockticks6502
	section	zpage
_clockticks6502:
	word	0,0
	global	_clockgoal6502
	zpage	_clockgoal6502
	section	zpage
_clockgoal6502:
	word	0,0
	global	_interpret_count
	zpage	_interpret_count
	section	zpage
_interpret_count:
	word	0
	global	_last_interpreted_opcode
	zpage	_last_interpreted_opcode
	section	zpage
_last_interpreted_opcode:
	byte	0
	global	_last_write_ea
	zpage	_last_write_ea
	section	zpage
_last_write_ea:
	word	0
	global	_write_50xx_count
	zpage	_write_50xx_count
	section	zpage
_write_50xx_count:
	byte	0
	global	_callexternal
	section	"text0"
_callexternal:
	byte	0
	global	___muluint16
	global	_pc
	zpage	_pc
	section	zpage
_pc:
	reserve	2
	global	_sp
	zpage	_sp
	section	zpage
_sp:
	reserve	1
	global	_a
	zpage	_a
	section	zpage
_a:
	reserve	1
	global	_x
	zpage	_x
	section	zpage
_x:
	reserve	1
	global	_y
	zpage	_y
	section	zpage
_y:
	reserve	1
	global	_status
	zpage	_status
	section	zpage
_status:
	reserve	1
	global	_oldpc
	zpage	_oldpc
	section	zpage
_oldpc:
	reserve	2
	global	_ea
	zpage	_ea
	section	zpage
_ea:
	reserve	2
	global	_reladdr
	zpage	_reladdr
	section	zpage
_reladdr:
	reserve	2
	global	_value
	zpage	_value
	section	zpage
_value:
	reserve	2
	global	_result
	zpage	_result
	section	zpage
_result:
	reserve	2
	global	_opcode
	zpage	_opcode
	section	zpage
_opcode:
	reserve	1
	global	_oldstatus
	zpage	_oldstatus
	section	zpage
_oldstatus:
	reserve	1
	global	_read6502
	global	_write6502
	global	_penaltyop
	zpage	_penaltyop
	section	zpage
_penaltyop:
	reserve	1
	global	_penaltyaddr
	zpage	_penaltyaddr
	section	zpage
_penaltyaddr:
	reserve	1
	global	_loopexternal
	section	"text0"
_loopexternal:
	reserve	2
	section	"text0"
l17:
	word	l19
	word	l80
	word	l19
	word	l80
	word	l30
	word	l30
	word	l30
	word	l30
	word	l19
	word	l25
	word	l22
	word	l25
	word	l52
	word	l52
	word	l52
	word	l52
	word	l45
	word	l87
	word	l19
	word	l87
	word	l35
	word	l35
	word	l35
	word	l35
	word	l19
	word	l66
	word	l19
	word	l66
	word	l59
	word	l59
	word	l59
	word	l59
	word	l52
	word	l80
	word	l19
	word	l80
	word	l30
	word	l30
	word	l30
	word	l30
	word	l19
	word	l25
	word	l22
	word	l25
	word	l52
	word	l52
	word	l52
	word	l52
	word	l45
	word	l87
	word	l19
	word	l87
	word	l35
	word	l35
	word	l35
	word	l35
	word	l19
	word	l66
	word	l19
	word	l66
	word	l59
	word	l59
	word	l59
	word	l59
	word	l19
	word	l80
	word	l19
	word	l80
	word	l30
	word	l30
	word	l30
	word	l30
	word	l19
	word	l25
	word	l22
	word	l25
	word	l52
	word	l52
	word	l52
	word	l52
	word	l45
	word	l87
	word	l19
	word	l87
	word	l35
	word	l35
	word	l35
	word	l35
	word	l19
	word	l66
	word	l19
	word	l66
	word	l59
	word	l59
	word	l59
	word	l59
	word	l19
	word	l80
	word	l19
	word	l80
	word	l30
	word	l30
	word	l30
	word	l30
	word	l19
	word	l25
	word	l22
	word	l25
	word	l73
	word	l52
	word	l52
	word	l52
	word	l45
	word	l87
	word	l19
	word	l87
	word	l35
	word	l35
	word	l35
	word	l35
	word	l19
	word	l66
	word	l19
	word	l66
	word	l59
	word	l59
	word	l59
	word	l59
	word	l25
	word	l80
	word	l25
	word	l80
	word	l30
	word	l30
	word	l30
	word	l30
	word	l19
	word	l25
	word	l19
	word	l25
	word	l52
	word	l52
	word	l52
	word	l52
	word	l45
	word	l87
	word	l19
	word	l87
	word	l35
	word	l35
	word	l40
	word	l40
	word	l19
	word	l66
	word	l19
	word	l66
	word	l59
	word	l59
	word	l66
	word	l66
	word	l25
	word	l80
	word	l25
	word	l80
	word	l30
	word	l30
	word	l30
	word	l30
	word	l19
	word	l25
	word	l19
	word	l25
	word	l52
	word	l52
	word	l52
	word	l52
	word	l45
	word	l87
	word	l19
	word	l87
	word	l35
	word	l35
	word	l40
	word	l40
	word	l19
	word	l66
	word	l19
	word	l66
	word	l59
	word	l59
	word	l66
	word	l66
	word	l25
	word	l80
	word	l25
	word	l80
	word	l30
	word	l30
	word	l30
	word	l30
	word	l19
	word	l25
	word	l19
	word	l25
	word	l52
	word	l52
	word	l52
	word	l52
	word	l45
	word	l87
	word	l19
	word	l87
	word	l35
	word	l35
	word	l35
	word	l35
	word	l19
	word	l66
	word	l19
	word	l66
	word	l59
	word	l59
	word	l59
	word	l59
	word	l25
	word	l80
	word	l25
	word	l80
	word	l30
	word	l30
	word	l30
	word	l30
	word	l19
	word	l25
	word	l19
	word	l25
	word	l52
	word	l52
	word	l52
	word	l52
	word	l45
	word	l87
	word	l19
	word	l87
	word	l35
	word	l35
	word	l35
	word	l35
	word	l19
	word	l66
	word	l19
	word	l66
	word	l59
	word	l59
	word	l59
	word	l59
	section	"text0"
l18:
	word	l189
	word	l370
	word	l360
	word	l360
	word	l360
	word	l370
	word	l139
	word	l360
	word	l382
	word	l370
	word	l139
	word	l360
	word	l360
	word	l370
	word	l139
	word	l360
	word	l184
	word	l370
	word	l360
	word	l360
	word	l360
	word	l370
	word	l139
	word	l360
	word	l204
	word	l370
	word	l360
	word	l360
	word	l360
	word	l370
	word	l139
	word	l360
	word	l318
	word	l130
	word	l360
	word	l360
	word	l168
	word	l130
	word	l397
	word	l360
	word	l394
	word	l130
	word	l397
	word	l360
	word	l168
	word	l130
	word	l397
	word	l360
	word	l174
	word	l130
	word	l360
	word	l360
	word	l360
	word	l130
	word	l397
	word	l360
	word	l452
	word	l130
	word	l360
	word	l360
	word	l360
	word	l130
	word	l397
	word	l360
	word	l423
	word	l279
	word	l360
	word	l360
	word	l360
	word	l279
	word	l348
	word	l360
	word	l379
	word	l279
	word	l348
	word	l360
	word	l315
	word	l279
	word	l348
	word	l360
	word	l194
	word	l279
	word	l360
	word	l360
	word	l360
	word	l279
	word	l348
	word	l360
	word	l210
	word	l279
	word	l360
	word	l360
	word	l360
	word	l279
	word	l348
	word	l360
	word	l426
	word	l109
	word	l360
	word	l360
	word	l360
	word	l109
	word	l411
	word	l360
	word	l385
	word	l109
	word	l411
	word	l360
	word	l315
	word	l109
	word	l411
	word	l360
	word	l199
	word	l109
	word	l360
	word	l360
	word	l360
	word	l109
	word	l411
	word	l360
	word	l458
	word	l109
	word	l360
	word	l360
	word	l360
	word	l109
	word	l411
	word	l360
	word	l360
	word	l461
	word	l360
	word	l360
	word	l467
	word	l461
	word	l464
	word	l360
	word	l270
	word	l360
	word	l497
	word	l360
	word	l467
	word	l461
	word	l464
	word	l360
	word	l153
	word	l461
	word	l360
	word	l360
	word	l467
	word	l461
	word	l464
	word	l360
	word	l509
	word	l461
	word	l506
	word	l360
	word	l360
	word	l461
	word	l360
	word	l360
	word	l339
	word	l321
	word	l330
	word	l360
	word	l339
	word	l321
	word	l330
	word	l360
	word	l479
	word	l321
	word	l470
	word	l360
	word	l339
	word	l321
	word	l330
	word	l360
	word	l158
	word	l321
	word	l360
	word	l360
	word	l339
	word	l321
	word	l330
	word	l360
	word	l213
	word	l321
	word	l488
	word	l360
	word	l339
	word	l321
	word	l330
	word	l360
	word	l240
	word	l216
	word	l360
	word	l360
	word	l240
	word	l216
	word	l252
	word	l360
	word	l306
	word	l216
	word	l261
	word	l360
	word	l240
	word	l216
	word	l252
	word	l360
	word	l179
	word	l216
	word	l360
	word	l360
	word	l360
	word	l216
	word	l252
	word	l360
	word	l207
	word	l216
	word	l360
	word	l360
	word	l360
	word	l216
	word	l252
	word	l360
	word	l228
	word	l429
	word	l360
	word	l360
	word	l228
	word	l429
	word	l288
	word	l360
	word	l297
	word	l429
	word	l360
	word	l429
	word	l228
	word	l429
	word	l288
	word	l360
	word	l163
	word	l429
	word	l360
	word	l360
	word	l360
	word	l429
	word	l288
	word	l360
	word	l455
	word	l429
	word	l360
	word	l360
	word	l360
	word	l429
	word	l288
	word	l360
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
