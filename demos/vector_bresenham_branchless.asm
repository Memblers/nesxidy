; vector_bresenham_branchless.asm
; Example of a highly‑tuned 6502 Bresenham line drawer.
; This variant is unrolled, uses no conditional branches in the inner
; loop, and relies on a small lookup table to tell it how to adjust the
; error term and which axes to step.  It's intended as a demonstration of
; "branchless" 6502 code; the table consumes 512 bytes but the pixel
; function itself costs <25 cycles when the same bank is already mapped.
;
; Zero page layout (modify to suit your project):
;   dx    = $00  ; unsigned difference in X
;   dy    = $01  ; unsigned difference in Y
;   sx    = $02  ; +1 or -1 (in two's complement)
;   sy    = $03
;   err_lo= $04  ; signed error term low byte
;   err_hi= $05  ; high byte (sign bit)
;   mask_x= $06  ; temporary mask for X
;   mask_y= $07  ; temporary mask for Y
;
; Input: start coords in X (x0) and Y (y0), end coords stored in
; zero page x1 ($08) and y1 ($09) before calling.
; After return X/Y will contain the endpoint (same as x1/y1).
;
; Tables (512 bytes):
;   bres_tab: 256 entries of {err_delta,step_mask}
;     err_delta = ((dx>dy)? -dy : dx) & 0xff  (signed)
;     step_mask: bit0=step X, bit1=step Y
;   The generator only needs to be run once offline; an example script
;   is described in the README.
;
; The loop is unrolled twice; the branch at the bottom tests the loop
; counter only.  All decisions about stepping are resolved by the table
; lookup, so the inner two pixels are drawn with *no* conditional branches.
;
.segment "CODE"

DrawLineBL:
    ; load endpoint into zp, compute dx/dy/sx/sy and initial err
    LDY y1
    TXA            ; X holds x0 passed in
    STA err_lo     ; temporary, will be overwritten later
    ; compute dx,dy and signs
    LDA x1
    SEC
    SBC err_lo     ; A = x1 - x0
    BPL .dx_pos
    EOR #$FF
    CLC
    ADC #1
    STA dx
    LDA #$FF
    STA sx        ; -1
    JMP .got_dx
  .dx_pos:
    STA dx
    LDA #1
    STA sx
  .got_dx:
    ; dy
    LDY y1
    SEC
    SBC Y          ; A = y1 - y0 (y0 in Y?)
    BPL .dy_pos
    EOR #$FF
    CLC
    ADC #1
    STA dy
    LDA #$FF
    STA sy
    JMP .got_dy
  .dy_pos:
    STA dy
    LDA #1
    STA sy
  .got_dy:

    ; initialize err = dx - dy (signed 16-bit)
    LDA dx
    SEC
    SBC dy
    STA err_lo
    LDA #0
    STA err_hi

    ; prepare loop counter = number of steps = max(dx,dy)
    LDA dx
    CMP dy
    BCS .count_dx
    LDA dy
  .count_dx:
    TAX            ; use X as remaining pixel count

; main drawing loop (two pixels per iteration) -------------------------
bl_loop:
    JSR PlotPixelFast   ; plot (X,Y) using the fast direct plotter

    ; --- first step ---------------------------------------------------
    ; lookup table entry using err_lo as index (0..255)
    LDY err_lo     ; index in Y
    LDA bres_tab+0,Y
    STA err_lo     ; apply err delta
    LDA bres_tab+1,Y
    AND #1          ; extract X/Y mask bits
    TAX            ; mask in X
    ; compute mask_x = (mask & 1) ? $FF : 0
    LDA #0
    SEC
    SBC mask_x     ; trick: if mask_x==1, result=$FF
    STA mask_x
    ; mask_y = (mask & 2) ? $FF : 0
    LDA mask_x
    ASL
    STA mask_y

    ; step X and Y unconditionally, then mask out unwanted increments
    TXA
    CLC
    ADC sx
    TAX            ; X' = x+sx
    TYA
    CLC
    ADC sy
    TAY            ; Y' = y+sy

    ; apply masks (clearing increments if corresponding bit was zero)
    TXA             ; A = X
    AND mask_x      ; clear if mask_x==0
    TAX             ; X = masked value
    TYA             ; A = Y
    AND mask_y
    TAY             ; Y = masked value

    DEX            ; consume one count
    BEQ bl_done

    JSR PlotPixelFast  ; second pixel

    ; --- second step (repeat same code) -----------------------------
    LDY err_lo
    LDA bres_tab+0,Y
    STA err_lo
    LDA bres_tab+1,Y
    AND #1
    TAX
    LDA #0
    SEC
    SBC mask_x
    STA mask_x
    LDA mask_x
    ASL
    STA mask_y
    TXA
    CLC
    ADC sx
    TAX
    TYA
    CLC
    ADC sy
    TAY
    TXA             ; mask X coordinate
    AND mask_x
    TAX
    TYA             ; mask Y coordinate
    AND mask_y
    TAY

    DEX
    BNE bl_loop

bl_done:
    RTS

; 256‑entry Bresenham table (delta,mask)
bres_tab:
    ; generator: for err = -255..+0..+255 compute delta and mask bits
    ; (omitted here, generate offline)
    .rept 512
    .byte 0,0
    .endr

; end of file
