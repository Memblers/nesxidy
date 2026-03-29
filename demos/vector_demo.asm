; vector_demo.asm
; Optimized 6502 Bresenham line drawer suitable for NES.
; Uses zero page for frequently updated variables to save cycles.
; Entry conventions:
;   X = x0 (start x), Y = y0 (start y)
;   zero page bytes 'x1' and 'y1' contain the endpoint coordinates.
;   After return, X/Y will hold the end coordinates (same as x1/y1).
;
; Zero page layout (feel free to rearrange to fit your project):
;   dx      = $00
;   dy      = $01
;   err_lo  = $02    ; signed 16‑bit error term (low byte)
;   err_hi  = $03    ; high byte (sign bit)
;   sx      = $04    ; +1 or -1
;   sy      = $05
;   x1      = $06    ; endpoint (caller must store)
;   y1      = $07
;   e2_lo   = $08    ; temporary copy of err (low byte)
;   e2_hi   = $09    ; temporary copy (high byte)
;   PIX_X   = $0A    ; X coordinate for PLOTPIXEL
;   PIX_Y   = $0B    ; Y coordinate for PLOTPIXEL
;   TMPX    = $0C    ; temporary byte
;   PTR_LO  = $0D    ; pointer to frame buffer low
;   PTR_HI  = $0E    ; pointer to frame buffer high
;   ; $0F remains free for other use
;
; The actual drawing callback is an external routine `PLOTPIXEL` which must
; read the desired X,Y from PIX_X/PIX_Y (or from registers if you modify
; the code).  It is invoked with a JSR in the inner loop and should return
; quickly (ideally <20 cycles) so the entire loop costs ~25 cycles/step.
;
.segment "CODE"

DrawLineZP:
    ; calculate dx = abs(x1 - X); sx = sign
    TXA
    STA TMPX         ; save x0 temporarily
    LDA x1
    SEC
    SBC TMPX         ; A = x1 - x0
    BPL dl_dx_pos    ; positive or zero
    ; negative: make positive
    EOR #$FF
    CLC
    ADC #1
    STA dx           ; store abs
    LDX #$FF         ; sx = -1
    JMP dl_dx_done
  dl_dx_pos:
    STA dx           ; dx >= 0
    LDX #1           ; sx = +1
  dl_dx_done:

    ; calculate dy = abs(y1 - Y); sy = sign
    TYA
    STA TMPX         ; save y0
    LDA y1
    SEC
    SBC TMPX         ; A = y1 - y0
    BPL dl_dy_pos
    EOR #$FF
    CLC
    ADC #1
    STA dy
    LDY #$FF         ; sy = -1
    JMP dl_dy_done
  dl_dy_pos:
    STA dy
    LDY #1           ; sy = +1
  dl_dy_done:

    ; err = (dx>dy ? dx : -dy) / 2  (signed 16‑bit result)
    LDA dx
    CMP dy
    BCS dl_err_dx   ; dx >= dy
    ; err = -dy/2
    LDA dy
    EOR #$FF
    CLC
    ADC #1           ; A = -dy (8‑bit)
    STA err_lo
    LDA #$FF         ; high byte = 0xFF for negative
    STA err_hi
    ; arithmetic right shift by 1
    LSR err_lo
    ROR err_hi
    JMP dl_err_done
  dl_err_dx:
    LDA dx
    LSR              ; low byte = dx/2
    STA err_lo
    LDA #0
    STA err_hi       ; high byte = 0
  dl_err_done:

; main loop --------------------------------------------------------------
dl_loop:
    ; plot pixel at X,Y
    TXA
    STA PIX_X
    TYA
    STA PIX_Y
    JSR PLOTPIXEL

    ; check for finish
    TXA
    CMP x1
    BNE dl_check_y
    TYA
    CMP y1
    BEQ dl_done
  dl_check_y:

    ; copy err to e2 (we'll use original error for both tests)
    LDA err_lo
    STA e2_lo
    LDA err_hi
    STA e2_hi

    ; test x step: e2 > -dx  <=> e2 + dx >= 0
    LDA e2_lo
    CLC
    ADC dx
    STA TMPX        ; TMPX = low result
    LDA e2_hi
    ADC #0          ; add carry
    ; if high byte >= 0 (bit7 clear) then e2 + dx >=0
    BPL dl_xstep
  dl_no_xstep:
    ; test y step: e2 < dy  (dy unsigned)
    LDA e2_hi
    BMI dl_ystep    ; negative => < dy
    LDA e2_lo
    CMP dy
    BMI dl_ystep
    JMP dl_loop     ; no steps

  dl_xstep:
    ; err -= dy  (16‑bit)
    LDA err_lo
    SEC
    SBC dy
    STA err_lo
    LDA err_hi
    SBC #0
    STA err_hi
    ; x += sx
    TXA
    CLC
    ADC sx
    TAX
    ; fall through to y check using original e2
    LDA e2_hi
    BMI dl_ystep        ; negative -> < dy
    LDA e2_lo
    CMP dy
    BMI dl_ystep
    JMP dl_loop

  dl_ystep:
    ; err += dx  (16‑bit)
    LDA err_lo
    CLC
    ADC dx
    STA err_lo
    LDA err_hi
    ADC #0
    STA err_hi
    ; y += sy
    TYA
    CLC
    ADC sy
    TAY
    JMP dl_loop

  dl_done:
    RTS

; complete PLOTPIXEL implementation using fb_bits base address.
; "fb_bits" is defined in vector_demo.c; link both objects together so the
; symbol is available.  Routine calculates byte offset = y*32 + (x>>3) and
; sets the corresponding bit in the framebuffer.
;
; registers/clobbers: A,X,Y,PTCLOP
; uses TMPX, PTR_LO, PTR_HI.
;
PLOTPIXEL:
    ; compute 16‑bit offset = y*32 + (x>>3)
    ; high byte = y>>3, low byte = (y<<5) + (x>>3)
    LDA PIX_Y
    LSR
    LSR
    LSR            ; A = y>>3
    STA TMPX       ; TMPX holds high byte of offset

    LDA PIX_Y
    ASL
    ASL
    ASL
    ASL
    ASL           ; A = y<<5
    STA PTR_LO    ; low part

    LDA PIX_X
    LSR
    LSR
    LSR           ; A = x>>3
    CLC
    ADC PTR_LO
    STA PTR_LO    ; complete low byte

    LDA TMPX
    ADC #0        ; add carry from low add
    STA PTR_HI    ; high byte of offset

    ; add base address of fb_bits to the 16‑bit offset
    LDA PTR_LO
    CLC
    ADC #<fb_bits
    STA PTR_LO
    LDA PTR_HI
    ADC #>fb_bits
    STA PTR_HI

    ; compute mask = 1 << (x & 7)
    LDA PIX_X
    AND #7
    TAX
    LDA bitmask_table,X
    STA TMPX          ; save mask temporarily

    ; fetch byte, OR mask, store back
    ; (PTR_LO),Y: zero page pair PTR_LO/PTR_HI holds the address; Y=0 means no index offset
    LDY #0
    LDA (PTR_LO),Y
    ORA TMPX          ; combine with saved mask
    STA (PTR_LO),Y
    RTS

; 8‑element mask lookup table for bits0..7
bitmask_table:
    .byte %00000001, %00000010, %00000100, %00001000
    .byte %00010000, %00100000, %01000000, %10000000

; end of file
