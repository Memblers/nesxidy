; vector_demo_fast.asm
; Ultra‑fast pixel plotter using a precomputed lookup table.
; The table contains one 16‑bit value per pixel and therefore occupies
; 128 KB (131072 bytes) total; it is laid out across eight consecutive
; 16 KB PRG banks.  (Each bank holds 8192 entries = 16 KB.)
; The table occupies eight consecutive 16KB PRG banks.  Bank 0 contains
; the first 8192 entries, bank 1 the next 8192, and so on; the bank number
; is simply the high three bits of the index ((y<<8|x) >> 13).  Plane
; information is applied later by offsetting the pointer.  The caller is
; responsible for setting the attribute table appropriately (e.g. left
; half plane0, right half plane1).
;
; Table layout (built offline, not included here):
;  - 65536 entries, each 2 bytes (little endian)
;  - each entry is the 16‑bit *absolute* address of the target byte in
;    `tile_bank` corresponding to pixel (x,y).  The build tool adds
;    plane*8 for x>=128 and computes ((y>>3)*32+(x>>3))<<4+row.  Thus the
;    runtime code need only perform a lookup and store the 16‑bit value
;    directly into PTR_LO/PTR_HI.
;  - banks selected by high three bits of the 16‑bit index ((y<<8)|x)
;
; Coordinates are already scaled and centred; pixel range 0..255.
;
; Zero page helpers:
;   idx_lo = $00    ; low byte of (y<<8)|x
;   idx_hi = $01    ; high byte
;   mask   = $02    ; bitmask (1<<(7-(x&7)))
;   plane  = $03    ; 0 or 1
;   TMPX   = $04    ; scratch (entry low / bank number)
;   PTR_LO = $05    ; indirect pointer construction
;   PTR_HI = $06

.segment "CODE"

; write A to mapper bank register (dummy implementation; adapt to mapper)
.macro SET_BANK
    ; A = bank number (0..7)
    STA $8000
.endmacro

; plot pixel using the 131KB table described above.
; Each table entry already includes plane‑offset and points to the
; final byte in video RAM, so runtime never needs to add plane or base.
; inputs: X = x, Y = y
; clobbers: A,X,Y
PlotPixelFast:
    ; X = x, Y = y
    STX idx_lo          ; low byte of index
    STY idx_hi          ; high byte = y

    ; mask lookup using small table
    LDA idx_lo
    AND #7
    TAX
    LDA mask_table,X
    STA mask


    ; bank number = y >> 5 using table (saves 5 shifts)
    LDA idx_hi         ; y
    TAX
    LDA bank_table,X
    SET_BANK           ; map $8000-$BFFF to correct 16KB chunk

    ; compute offset_bytes = (index & 0x1FFF) * 2
    LDA idx_hi
    AND #$1F           ; clear top 3 bits
    STA idx_hi
    ASL idx_lo          ; shift the 16‑bit word left by one
    ROL idx_hi

    ; pointer into current bank window (write directly to PTR2)
    LDA idx_lo
    CLC
    ADC #$00
    STA PTR2_LO
    LDA idx_hi
    ADC #$80
    STA PTR2_HI

    ; fetch table entry; entry is final WRAM address
    LDY #0
    LDA (PTR2_LO),Y      ; low byte of entry
    STA PTR_LO
    INY
    LDA (PTR2_LO),Y      ; high byte of entry
    STA PTR_HI

    ; OR mask into selected plane byte
    LDY #0
    LDA (PTR_LO),Y
    ORA mask
    STA (PTR_LO),Y
    RTS


; dummy data labels for assembler
.tile_bank_base
.tile_bank_lo .byte <tile_bank>
.tile_bank_hi .byte >tile_bank

; zpage declarations
idx_lo = $00
idx_hi = $01
mask   = $02
PTR2_LO = $03    ; secondary pointer for table fetch
PTR2_HI = $04
PTR_LO = $05
PTR_HI = $06


; bank lookup table: bank = y >> 5  (0..7)
bank_table:
    .rept 32
        .byte 0,0,0,0,0,0,0,0,   1,1,1,1,1,1,1,1,
        .byte 2,2,2,2,2,2,2,2,   3,3,3,3,3,3,3,3,
        .byte 4,4,4,4,4,4,4,4,   5,5,5,5,5,5,5,5,
        .byte 6,6,6,6,6,6,6,6,   7,7,7,7,7,7,7,7
    .endr

; 8‑element mask lookup table (bit 7..0)
mask_table:
    .byte %10000000, %01000000, %00100000, %00010000
    .byte %00001000, %00000100, %00000010, %00000001

; two 256‑byte helper tables for half‑speed variant
;   y32_lo_table[y] = (y * 32) & $FF
;   y32_hi_table[y] = (y * 32) >> 8
; these let you compute the y‑offset without shifts; use them in place of
; the ASL/ROL chain above if you prefer a 512‑byte lookup instead of 5 cycles.

y32_lo_table:
    ; (y * 32) & $FF for y=0..255
    .rept 32
        .byte 0,32,64,96,128,160,192,224
    .endr

; high byte of y*32 = floor(y/8), gives 0..31 each repeated eight times
y32_hi_table:
    .rept 32
        .byte 0,0,0,0,0,0,0,0
        .byte 1,1,1,1,1,1,1,1
        .byte 2,2,2,2,2,2,2,2
        .byte 3,3,3,3,3,3,3,3
        .byte 4,4,4,4,4,4,4,4
        .byte 5,5,5,5,5,5,5,5
        .byte 6,6,6,6,6,6,6,6
        .byte 7,7,7,7,7,7,7,7
        .byte 8,8,8,8,8,8,8,8
        .byte 9,9,9,9,9,9,9,9
        .byte 10,10,10,10,10,10,10,10
        .byte 11,11,11,11,11,11,11,11
        .byte 12,12,12,12,12,12,12,12
        .byte 13,13,13,13,13,13,13,13
        .byte 14,14,14,14,14,14,14,14
        .byte 15,15,15,15,15,15,15,15
        .byte 16,16,16,16,16,16,16,16
        .byte 17,17,17,17,17,17,17,17
        .byte 18,18,18,18,18,18,18,18
        .byte 19,19,19,19,19,19,19,19
        .byte 20,20,20,20,20,20,20,20
        .byte 21,21,21,21,21,21,21,21
        .byte 22,22,22,22,22,22,22,22
        .byte 23,23,23,23,23,23,23,23
        .byte 24,24,24,24,24,24,24,24
        .byte 25,25,25,25,25,25,25,25
        .byte 26,26,26,26,26,26,26,26
        .byte 27,27,27,27,27,27,27,27
        .byte 28,28,28,28,28,28,28,28
        .byte 29,29,29,29,29,29,29,29
        .byte 30,30,30,30,30,30,30,30
        .byte 31,31,31,31,31,31,31,31
    .endr

; end of file
