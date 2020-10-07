	macro  inc16, addr
	inline
        inc \addr
        bne .Skip
        inc \addr+1	
.Skip:
	einline
	endmacro	



LF = $0A
CR = $0D

	zpage	temp_x,temp_y,temp_hi,received,addr_lo,addr_hi,temp_lo,temp2_lo,temp2_hi,count_lo,count_hi,cpu_addr_lo,cpu_addr_hi
	section "zpage"

	temp_x: reserve 1
	temp_y: reserve 1
	temp_hi: reserve 1
	received: reserve 1
	addr_lo: reserve 1
	addr_hi: reserve 1
	temp_lo: reserve 2
	temp2_lo: reserve 1
	temp2_hi: reserve 1
	count_lo: reserve 1
	count_hi: reserve 1
	cpu_addr_lo: reserve 1
	cpu_addr_hi: reserve 1	

	
	section "bss"
	
	received_index: reserve 1	
	in_string: reserve 80
	out_string: reserve 80
	out_index: reserve 1
	
	screen_addr: reserve 2
	lines: reserve 1
	
	debug_buffer: reserve 128
	
	section "text"


;======================
debug_loop_init:
   ldx #0
   stx received_index
debug_loop:
   jsr get_char
   bcc debug_loop
   jsr put_char

   lda received
   ldx received_index
   sta in_string,x
   inc received_index
   cmp #CR
   bne debug_loop
   ldx #0
   stx received_index
decode_string:
   ldx received_index
   lda in_string,x
   inc received_index

   cmp #CR
   beq debug_loop_init

   cmp #'?'
   bne skip_q

   jsr disasm
print_end:
skip_q:
   jmp decode_string



hex_table:
   db "0123456789ABCDEF"


;======================

;======================
;----
put_char:
Put_Chr:
		ldx out_index
		bne not_new_buffer
		
		pha		
	new_buffer:
		ldx #0		
		lda screen_addr+1
		ora #$40
		sta out_string,x
		inx
		lda screen_addr
		sta out_string,x
		inx
		inx		
		stx out_index
		pla		
		
	not_new_buffer:
	
		cmp #CR
		bne .1		
		rts
	.1:
		cmp #LF
		bne .2
		
		ldx out_index
		lda #0
	.3:
		sta out_string,x
		inx
		cpx #$23
		bne .3
		lda #$FF
		sta out_string,x		
		txa ;lda out_index
		sec
		sbc #3
		sta out_string+2

		lda #0
		sta out_index
		
		lda screen_addr
		and #$E0
		clc
		adc #($20 + DISASM_BORDER)
		sta screen_addr
		lda #0		
		adc screen_addr+1		
		sta screen_addr+1
		

		lda #<out_string
		ldx #>out_string		
		jsr _lnList
		
		lda #0
		jsr _lnSync
		
		rts
		
		
	.2:
		ldx out_index
		sta out_string,x
		inc out_index	

        rts

;--------
get_char:
Get_Chr:

        rts

;----


IMP	= 0
IMPA	= 1
MARK2	= 2
BRA	= 3
IMM	= 4
ZP		= 5
ZPX	= 6
ZPY	= 7
INDX	= 8
INDY	= 9
IND	= 10
MARK3	= 11
ABS	= 12
ABSX	= 13
ABSY	= 14
IND16	= 15
IND1X	= 16


I_ADC	= 0
I_AND   = 1
I_ASL   = 2
I_BCC   = 3
I_BCS   = 4
I_BEQ   = 5
I_BIT   = 6
I_BMI   = 7
I_BNE   = 8
I_BPL   = 9
I_BRA   = 10
I_BRK   = 11
I_BVC   = 12
I_BVS   = 13
I_CLC   = 14
I_CLD   = 15
I_CLI   = 16
I_CLV	= 17
I_CMP   = 18
I_CPX   = 19
I_CPY   = 20
I_DEC   = 21
I_DEX   = 22
I_DEY   = 23
I_EOR   = 24
I_INC   = 25
I_INX   = 26
I_INY   = 27
I_JMP   = 28
I_JSR   = 29
I_LDA   = 30
I_LDX   = 31
I_LDY   = 32
I_LSR   = 33
I_NOP	= 34
I_ORA   = 35
I_PHA   = 36
I_PHP   = 37
I_PHX   = 38
I_PHY   = 39
I_PLA   = 40
I_PLP   = 41
I_PLX   = 42
I_PLY   = 43
I_ROL   = 44
I_ROR   = 45
I_RTI   = 46
I_RTS   = 47
I_SBC   = 48
I_SEC   = 49
I_SED   = 50
I_SEI	= 51
I_STA   = 52
I_STP   = 53
I_STX   = 54
I_STY   = 55
I_STZ   = 56
I_TAX   = 57
I_TAY   = 58
I_TRB   = 59
I_TSB   = 60
I_TSX   = 61
I_TXA   = 62
I_TXS   = 63
I_TYA   = 64
I_WAI   = 65
I_XXX   = 66


opname:
	db I_BRK, I_ORA, I_XXX, I_XXX, I_TSB, I_ORA, I_ASL, I_XXX, I_PHP, I_ORA, I_ASL, I_XXX, I_TSB, I_ORA, I_ASL, I_XXX
	db I_BPL, I_ORA, I_ORA, I_XXX, I_TRB, I_ORA, I_ASL, I_XXX, I_CLC, I_ORA, I_INC, I_XXX, I_TRB, I_ORA, I_ASL, I_XXX
	db I_JSR, I_AND, I_XXX, I_XXX, I_BIT, I_AND, I_ROL, I_XXX, I_PLP, I_AND, I_ROL, I_XXX, I_BIT, I_AND, I_ROL, I_XXX
	db I_BMI, I_AND, I_AND, I_XXX, I_BIT, I_AND, I_ROL, I_XXX, I_SEC, I_AND, I_DEC, I_XXX, I_BIT, I_AND, I_ROL, I_XXX
	db I_RTI, I_EOR, I_XXX, I_XXX, I_XXX, I_EOR, I_LSR, I_XXX, I_PHA, I_EOR, I_LSR, I_XXX, I_JMP, I_EOR, I_LSR, I_XXX
	db I_BVC, I_EOR, I_EOR, I_XXX, I_XXX, I_EOR, I_LSR, I_XXX, I_CLI, I_EOR, I_PHY, I_XXX, I_XXX, I_EOR, I_LSR, I_XXX
	db I_RTS, I_ADC, I_XXX, I_XXX, I_STZ, I_ADC, I_ROR, I_XXX, I_PLA, I_ADC, I_ROR, I_XXX, I_JMP, I_ADC, I_ROR, I_XXX
	db I_BVS, I_ADC, I_ADC, I_XXX, I_STZ, I_ADC, I_ROR, I_XXX, I_SEI, I_ADC, I_PLY, I_XXX, I_JMP, I_ADC, I_ROR, I_XXX
	db I_BRA, I_STA, I_XXX, I_XXX, I_STY, I_STA, I_STX, I_XXX, I_DEY, I_BIT, I_TXA, I_XXX, I_STY, I_STA, I_STX, I_XXX
	db I_BCC, I_STA, I_STA, I_XXX, I_STY, I_STA, I_STX, I_XXX, I_TYA, I_STA, I_TXS, I_XXX, I_STZ, I_STA, I_STZ, I_XXX
	db I_LDY, I_LDA, I_LDX, I_XXX, I_LDY, I_LDA, I_LDX, I_XXX, I_TAY, I_LDA, I_TAX, I_XXX, I_LDY, I_LDA, I_LDX, I_XXX
	db I_BCS, I_LDA, I_LDA, I_XXX, I_LDY, I_LDA, I_LDX, I_XXX, I_CLV, I_LDA, I_TSX, I_XXX, I_LDY, I_LDA, I_LDX, I_XXX
	db I_CPY, I_CMP, I_XXX, I_XXX, I_CPY, I_CMP, I_DEC, I_XXX, I_INY, I_CMP, I_DEX, I_WAI, I_CPY, I_CMP, I_DEC, I_XXX
	db I_BNE, I_CMP, I_CMP, I_XXX, I_XXX, I_CMP, I_DEC, I_XXX, I_CLD, I_CMP, I_PHX, I_STP, I_XXX, I_CMP, I_DEC, I_XXX
	db I_CPX, I_SBC, I_XXX, I_XXX, I_CPX, I_SBC, I_INC, I_XXX, I_INX, I_SBC, I_NOP, I_XXX, I_CPX, I_SBC, I_INC, I_XXX
	db I_BEQ, I_SBC, I_SBC, I_XXX, I_XXX, I_SBC, I_INC, I_XXX, I_SED, I_SBC, I_PLX, I_XXX, I_XXX, I_SBC, I_INC, I_XXX

opaddr:
	db IMP, INDX,  IMP, IMP,  ZP,   ZP,	   ZP,	 IMP,	IMP,  IMM,   IMPA,  IMP,  ABS,	  ABS,	 ABS,  IMP
	db BRA, INDY,  IND, IMP,  ZP,   ZPX,   ZPX,	 IMP,	IMP,  ABSY,  IMPA,  IMP,  ABS,	  ABSX,	 ABSX, IMP
	db ABS, INDX,  IMP, IMP,  ZP,   ZP,	   ZP,	 IMP,	IMP,  IMM,   IMPA,  IMP,  ABS,	  ABS,	 ABS,  IMP
	db BRA, INDY,  IND, IMP,  ZPX,  ZPX,   ZPX,	 IMP,	IMP,  ABSY,  IMPA,  IMP,  ABSX,	  ABSX,	 ABSX, IMP
	db IMP, INDX,  IMP, IMP,  ZP,   ZP,	   ZP,	 IMP,	IMP,  IMM,   IMPA,  IMP,  ABS,	  ABS,	 ABS,  IMP
	db BRA, INDY,  IND, IMP,  ZP,   ZPX,   ZPX,	 IMP,	IMP,  ABSY,  IMP,   IMP,  ABS,	  ABSX,	 ABSX, IMP
	db IMP, INDX,  IMP, IMP,  ZP,   ZP,	   ZP,	 IMP,	IMP,  IMM,   IMPA,  IMP,  IND16,  ABS,	 ABS,  IMP
	db BRA, INDY,  IND, IMP,  ZPX,  ZPX,   ZPX,	 IMP,	IMP,  ABSY,  IMP,   IMP,  IND1X,  ABSX,	 ABSX, IMP
	db BRA, INDX,  IMP, IMP,  ZP,   ZP,	   ZP,	 IMP,	IMP,  IMM,   IMP,   IMP,  ABS,	  ABS,	 ABS,  IMP
	db BRA, INDY,  IND, IMP,  ZPX,  ZPX,   ZPY,	 IMP,	IMP,  ABSY,  IMP,   IMP,  ABS,	  ABSX,	 ABSX, IMP
	db IMM, INDX,  IMM, IMP,  ZP,   ZP,	   ZP,	 IMP,	IMP,  IMM,   IMP,   IMP,  ABS,	  ABS,	 ABS,  IMP
	db BRA, INDY,  IND, IMP,  ZPX,  ZPX,   ZPY,	 IMP,	IMP,  ABSY,  IMP,   IMP,  ABSX,	  ABSX,	 ABSY, IMP
	db IMM, INDX,  IMP, IMP,  ZP,   ZP,	   ZP,	 IMP,	IMP,  IMM,   IMP,   IMP,  ABS,	  ABS,	 ABS,  IMP
	db BRA, INDY,  IND, IMP,  ZP,   ZPX,   ZPX,	 IMP,	IMP,  ABSY,  IMP,   IMP,  ABS,	  ABSX,	 ABSX, IMP
	db IMM, INDX,  IMP, IMP,  ZP,   ZP,	   ZP,	 IMP,	IMP,  IMM,   IMP,   IMP,  ABS,	  ABS,	 ABS,  IMP
	db BRA, INDY,  IND, IMP,  ZP,   ZPX,   ZPX,	 IMP,	IMP,  ABSY,  IMP,   IMP,  ABS,	  ABSX,	 ABSX, IMP
	


opcodes:
	db "ADC"
	db "AND"
	db "ASL"
	db "BCC"
	db "BCS"
	db "BEQ"
	db "BIT"
	db "BMI"
	db "BNE"
	db "BPL"
	db "BRA"
	db "BRK"
	db "BVC"
	db "BVS"
	db "CLC"
	db "CLD"
	db "CLI"
	db "CLV"
	db "CMP"
	db "CPX"
	db "CPY"
	db "DEC"
	db "DEX"
	db "DEY"
	db "EOR"
	db "INC"
	db "INX"
	db "INY"
	db "JMP"
	db "JSR"
	db "LDA"
	db "LDX"
	db "LDY"
	db "LSR"
	db "NOP"
	db "ORA"
	db "PHA"
	db "PHP"
	db "PHX"
	db "PHY"
	db "PLA"
	db "PLP"
	db "PLX"
	db "PLY"
	db "ROL"
	db "ROR"
	db "RTI"
	db "RTS"
	db "SBC"
	db "SEC"
	db "SED"
	db "SEI"
	db "STA"
	db "STP"
	db "STX"
	db "STY"
	db "STZ"
	db "TAX"
	db "TAY"
	db "TRB"
	db "TSB"
	db "TSX"
	db "TXA"
	db "TXS"
	db "TYA"
	db "WAI"
	db "---"

;=======================================================		
	section "bss"
mode: reserve 1
op: reserve 1
p1: reserve 1
p2: reserve 1

;=======================================================	
	section "text"	
	global _disassemble, _pc
	
DISASM_BORDER = $04
	
_disassemble:
RESET:
disasm:

	lda #<($2460 + DISASM_BORDER)
	sta screen_addr
	lda #>($2460 + DISASM_BORDER)
	sta screen_addr+1

   lda #<(_cache_code + (47 * 32)) ;#<RESET
   sta addr_lo
   lda #>(_cache_code + (47 * 32)) ;#>RESET
   sta addr_hi
   
   lda #0
   sta lines

   ldy #0
disasm_loop:
;   tya
;   clc
;   adc addr_lo
;   sta temp_lo
;   lda addr_hi
;   adc #0
;   sta temp_hi

   lda addr_hi
   jsr put_hex
   lda addr_lo
   jsr put_hex

   lda #':'
   jsr put_char
   lda #' '
   jsr put_char


   lda (addr_lo),y
   sta op
   inc16 addr_lo
;   iny
   tax
   lda opaddr,x
   sta mode   

   lda #<opcodes
   sta count_lo
   lda #>opcodes
   sta count_hi

   ldx op
   lda opname,x
   sta temp_lo
   asl
   clc
   adc temp_lo
   adc count_lo
   sta count_lo
   lda #0
   adc count_hi
   sta count_hi

   sty temp2_lo
   ldy #0
.1
   lda (count_lo),y
   jsr put_char
   iny
   cpy #3
   bne .1
   ldy temp2_lo

   lda #' '
   jsr put_char

   lda mode
   asl
   tax
   lda jumptable+1,x
   pha
   lda jumptable,x
   pha
   rts

jumptable:
	dw pr_imp-1
	dw pr_impa-1
	dw pr_exit-1
	dw pr_bra-1
	dw pr_imm-1
	dw pr_zp-1
	dw pr_zpx-1
	dw pr_zpy-1
	dw pr_ind-1
	dw pr_indx-1
	dw pr_indy-1
	dw pr_exit-1
	dw pr_abs-1
	dw pr_absx-1
	dw pr_absy-1
	dw pr_ind16-1
	dw pr_ind1x-1

pr_impa:
   lda #'A'
   jsr put_char
   jmp pr_exit
pr_bra:

	lda (addr_lo),y
	inc16 addr_lo	
	sta p1
   ldx #0
   lda p1
   bpl .1
   dex
.1:
   clc
   adc addr_lo
   sta p1
   txa
   adc addr_hi
   sta p2
   jsr put_word_relative   
   jmp pr_exit
pr_imm:
   lda #'#'
   jsr put_char
   jsr put_byte
   jmp pr_exit
pr_zp:
   jsr put_byte
   jmp pr_exit
pr_zpx:
   jsr put_byte
   jsr put_comx
   jmp pr_exit
pr_zpy:
   jsr put_byte
   jsr put_comy
   jsr put_char
   jmp pr_exit
pr_ind:
   lda #'('
   jsr put_char
   jsr put_byte
   lda #')'
   jsr put_char
   jmp pr_exit
pr_indx:
   lda #'('
   jsr put_char
   jsr put_byte
   jsr put_comx
   lda #')'
   jsr put_char
   jmp pr_exit
pr_indy:
   lda #'('
   jsr put_char
   jsr put_byte
   lda #')'
   jsr put_char
   jsr put_comy
   jmp pr_exit
pr_abs:
   jsr put_word
   ;lda p1	; follow jump
   ;sta addr_lo
   ;lda p2
   ;sta addr_hi
   jmp pr_exit
pr_absx:
   jsr put_word
   jsr put_comx
   jmp pr_exit
pr_absy:
   jsr put_word
   jsr put_comy
   jmp pr_exit
pr_ind16:
pr_ind1x:
pr_imp:
pr_exit:

   lda #CR
   jsr put_char
   lda #LF
   jsr put_char

   ldy temp2_lo
;   iny
	
	inc lines
	lda lines
	cmp #24
	bne disasm_loop
	rts

put_byte:
   lda #'$'
   jsr put_char
   lda (addr_lo),y
   inc16 addr_lo
   ;lda p1
   jsr put_hex
   rts

put_word:
   lda #'$'
   jsr put_char
   lda (addr_lo),y
   inc16 addr_lo
   sta p1
   lda (addr_lo),y
   inc16 addr_lo
   sta p2
   jsr put_hex
   lda p1
   jsr put_hex
   rts
 
put_word_relative:
   lda #'$'
   jsr put_char
   lda p2
   jsr put_hex
   lda p1
   jsr put_hex
   rts

put_comx:
   lda #','
   jsr put_char
   lda #'X'
   jsr put_char
   rts

put_comy:
   lda #','
   jsr put_char
   lda #'Y'
   jsr put_char
   rts

put_hex:
   pha
   and #$F0
   lsr
   lsr
   lsr
   lsr
   tax
   lda hex_table,x
   jsr put_char
   pla
   and #$0F
   tax
   lda hex_table,x
   jsr put_char
   rts

