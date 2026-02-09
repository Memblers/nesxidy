
; 6502 Comprehensive Test Suite (Exidy platform)
; ROM: $2800-$3FFF
; Writes first failing test code to $0020 (zero page), $AA if all pass.

        .org $2800

RESET:  SEI
        CLD
        LDX #$FF
        TXS

; Clear small working areas
        LDA #$00
        STA $0010
        STA $0011
        STA $0012
        STA $0013
        STA $0014
        STA $0015
        STA $0016
        STA $0017
        STA $0018
        STA $0019
        STA $0020    ; result
        ; Also initialize other watched bytes and stack region to deterministic values
        STA $0026    ; watched
        STA $003F    ; watched
        STA $0044    ; watched
        STA $0101    ; page 1 helper
        STA $0050    ; debug marker (init 0)
        STA $0051    ; debug release sentinel (init 0)
        ; Initialize stack area $01EE..$01F7
        STA $01EE
        STA $01EF
        STA $01F0
        STA $01F1
        STA $01F2
        STA $01F3
        STA $01F4
        STA $01F5
        STA $01F6
        STA $01F7

; --- Helper: on fail, write code and halt ---
; usage: LDA #<code> ; STA $0020 ; JMP End

; ----------------------
; Test 01: LDA/STA & Addressing modes (re-run to be safe)
; ----------------------
        LDA #$12
        CMP #$12
        BEQ T02
        LDA #$01
        STA $0020
        JMP End

T02:    LDA #$34
        STA $0010
        LDA #$34
        STA $004A        ; diagnostic: record attempted write value
        LDA $0010
        STA $0049        ; diagnostic: store value read from $0010
        LDA #$AA
        STA $0050        ; diagnostic marker to capture context in lua watcher
        ; DEBUG: pause/polling removed — continue immediately

T03:    LDX #$01
        LDA #$56
        STA $0011
        LDA $0010,X
        CMP #$56
        BEQ T04
        LDA #$03
        STA $0020
        JMP End

T04:    LDA #$78
        STA $0300
        LDA $0300
        CMP #$78
        BEQ T05
        ; Diagnostic on absolute LDA failure: store address, loaded value, expected
        LDA #$00
        STA $0021        ; addr low
        LDA #$03
        STA $0022        ; addr high
        LDA $0300
        STA $0023        ; loaded value
        LDA #$78
        STA $0024        ; expected value
        LDA #$04
        STA $0020
        JMP End

T05:    LDX #$02
        LDA #$9A
        STA $0302
        LDA $0300,X
        CMP #$9A
        BEQ T06
        LDA #$05
        STA $0020
        JMP End

T06:    LDY #$03
        LDA #$BC
        STA $0303
        LDA $0300,Y
        CMP #$BC
        BEQ T07
        LDA #$06
        STA $0020
        JMP End

T07:    ; (indirect,X)
        LDA #$77
        STA $0040
        LDA #$40
        STA $00F0
        LDA #$00
        STA $00F1
        LDX #$00
        LDA ($00F0,X)
        CMP #$77
        BEQ T08
        LDA #$07
        STA $0020
        JMP End

T08:    ; (indirect),Y
        LDA #$50
        STA $00F2
        LDA #$00
        STA $00F3
        LDY #$01
        LDA #$88
        STA $0051
        LDA ($00F2),Y
        CMP #$88
        BEQ T09
        ; Diagnostic on indirect,Y failure: capture pointer, computed address, and loaded value
T08_fail:
        ; store original pointer low/high for inspection
        LDA $00F2
        STA $0021
        LDA $00F3
        STA $0022
        ; compute effective address = pointer + Y (Y currently = 1)
        LDA $00F2
        CLC
        ADC #$01
        STA $00F6        ; effective low byte
        LDA $00F3
        ADC #$00
        STA $00F7        ; effective high byte
        ; read memory at effective address and store result
        LDY #$00
        LDA ($00F6),Y
        STA $0023        ; loaded value at computed address
        ; store computed bytes for clarity
        LDA $00F6
        STA $0024
        LDA $00F7
        STA $0025
        ; record failure code
        LDA #$08
        STA $0020
        JMP End

T09:    ; Zero page wrap for (indirect,X)
        LDA #$BB
        STA $0100
        LDA #$00
        STA $00FF
        LDA #$01
        STA $0000
        LDX #$02
        LDA ($00FD,X)   ; pointer at $00FF/$0000 -> address $0100
        CMP #$BB
        BEQ T10
        LDA #$09
        STA $0020
        JMP End

; ----------------------
; Test 10: ADC / SBC and flags
; ----------------------
T10:    CLC
        LDA #$01
        ADC #$02
        CMP #$03
        BNE T10_fail
        ; carry test
        LDA #$FF
        ADC #$01
        BCS T11_ok
        LDA #$0A
        STA $0020
        JMP End
T10_fail:
        LDA #$0A
        STA $0020
        JMP End

T11_ok: SEC
        LDA #$05
        SBC #$03
        CMP #$02
        BNE T11_fail
        BCS T12_ok
T11_fail:
        ; Diagnostic: store accumulator (result) and processor status
        STA $0021        ; store A (SBC result)
        PHP
        PLA
        STA $0022        ; store P (status byte)
        LDA #$0B
        STA $0020
        JMP End

T12_ok:
        ; Decimal test (optional): set decimal and do BCD add if supported
        SED
        LDA #$15
        CLC
        ADC #$27   ; 15 + 27 = 42 -> in BCD should be 42
        CMP #$42
        BNE T12_skip
        CLD
        JMP T13
T12_skip:
        CLD
        ; if CPU doesn't support decimal ADC, still continue
        ; don't fail
        NOP

; ----------------------
; Test 11: AND/ORA/EOR
; ----------------------
T13:    LDA #$F0
        AND #$0F
        CMP #$00
        BNE T13_fail
        LDA #$F0
        ORA #$0F
        CMP #$FF
        BNE T13_fail
        LDA #$F0
        EOR #$0F
        CMP #$FF
        BEQ T14
T13_fail:
        LDA #$0C
        STA $0020
        JMP End

; ----------------------
; Test 14: BIT (flags)
; ----------------------
T14:    LDA #$00
        STA $0100
        LDA #$80
        STA $0101      ; bit7=1 -> N=1
        BIT $0101
        BPL T14_fail   ; N should be 1 -> BMI (negative) taken; BPL should not be taken
        BMI T15_ok
T14_fail:
        LDA #$0D
        STA $0020
        JMP End
T15_ok:
        ; test V flag
        LDA #$40
        STA $0102      ; bit6=1 -> V set
        BIT $0102
        BVC T15_fail   ; V should be 1, so BVC (clear) should not be taken
        BVS T16_ok
T15_fail:
        LDA #$0E
        STA $0020
        JMP End

T16_ok:
; ----------------------
; Test 16: CMP/CPX/CPY and branches
; ----------------------
        LDA #$10
        CMP #$10
        BEQ T17
        LDA #$0F
        STA $0020
        JMP End
T17:    LDX #$05
        CPX #$05
        BEQ T18
        LDA #$10
        STA $0020
        JMP End
T18:    LDY #$07
        CPY #$07
        BEQ T19
        LDA #$11
        STA $0020
        JMP End

; ----------------------
; Test 19: ASL/LSR (acc and mem)
; ----------------------
T19:    LDA #$81
        ASL
        CMP #$02
        BNE T19_fail
        LSR
        CMP #$01
        BNE T19_fail
        LDA #$02
        STA $0020
        ; memory modes
        LDA #$05
        STA $0200
        ASL $0200
        LDA $0200
        CMP #$0A
        BNE T19_fail
        LSR $0200
        LDA $0200
        CMP #$05
        BEQ T20
T19_fail:
        LDA #$12
        STA $0020
        JMP End

; ----------------------
; Test 20: ROL/ROR
; ----------------------
T20:    LDA #$80
        CLC
        ROL
        BCC T20_fail   ; carry expected
        ROR
        CMP #$80
        BEQ T21_ok
T20_fail:
        ; Diagnostic: store A and P before reporting
        STA $0021        ; A
        PHP
        PLA
        STA $0022        ; P
        LDA #$13
        STA $0020
        JMP End
T21_ok:
; ----------------------
; Test 21: INC/DEC
; ----------------------
        LDA #$05
        STA $0210
        INC $0210
        LDA $0210
        CMP #$06
        BNE T21_fail
        DEC $0210
        LDA $0210
        CMP #$05
        BEQ T22
T21_fail:
        LDA #$14
        STA $0020
        JMP End

; ----------------------
; Test 22: Transfers
; ----------------------
T22:    LDA #$99
        TAX
        TXA
        CMP #$99
        BNE T22_fail
        TAY
        TYA
        CMP #$99
        BEQ T23
T22_fail:
        LDA #$15
        STA $0020
        JMP End

; ----------------------
; Test 23: Stack PHA/PLA PHP/PLP
; ----------------------
T23:    LDA #$55
        PHA
        LDA #$00
        PLA
        CMP #$55
        BNE T23_fail
        PHP
        SEI     ; affect flags before PLP
        PLP
        ; Can't easily check all flags, but ensure stack works
        BEQ T24
T23_fail:
        LDA #$16
        STA $0020
        JMP End

; ----------------------
; Test 24: JSR/RTS
; ----------------------
T24:    LDA #$00
        JSR Sub01
        CMP #$AB
        BNE T24_fail
        JMP T26
T24_fail:
        LDA #$17
        STA $0020
        JMP End

Sub01:  LDA #$AB
        RTS

; T25: JMP-indirect test removed to free ROM space; continue with other tests
; If you want this test back we can remove some other tests or condense diagnostics to make room

; ----------------------
; Additional edge-case tests (fit in ROM)
; ----------------------

; T26: ADC overflow (signed overflow -> V set)
T26:    CLC
        LDA #$50
        ADC #$50
        CMP #$A0
        BNE T26_fail
        BVS T27_ok
T26_fail:
        LDA #$20
        STA $0020
        JMP End
T27_ok:

; T27: ADC carry-out
T27:    CLC
        LDA #$FF
        ADC #$01
        CMP #$00
        BNE T27_fail
        BCS T28_ok
T27_fail:
        LDA #$21
        STA $0020
        JMP End

T28_ok:
; T28: Decimal BCD ADC (if supported)
T28:    SED
        LDA #$15
        CLC
        ADC #$27
        CMP #$42
        BNE T28_fail
        CLD
        BEQ T29
T28_fail:
        CLD
        LDA #$22
        STA $0020
        JMP End

T29:    ; INC/DEC wrap tests
        LDA #$FF
        STA $0320
        INC $0320
        LDA $0320
        CMP #$00
        BNE T29_fail
        DEC $0320
        LDA $0320
        CMP #$FF
        BEQ T30
T29_fail:
        LDA #$23
        STA $0020
        JMP End

T30:    ; Absolute,X page-cross test ($02FF + X -> $0300) - both addresses are work RAM
        ; Clear $0300 then write and verify via absolute STA
        LDA #$00
        STA $0300
        LDA $0300
        STA $0024        ; mem at $0300 before write
        LDA #$77
        STA $0300
        LDA $0300
        STA $0025        ; mem at $0300 after write
        ; capture memory at $02FF
        LDA $02FF
        STA $0023        ; mem at $02FF
        LDX #$01
        LDA $02FF,X
        STA $0022        ; loaded value (from $0300 if page-cross works)
        STX $0021        ; X value
        ; verify the loaded value before doing any writes (preserve A)
        CMP #$77
        BEQ T31
        ; Try zero-page indirect write to $0300 to compare
        LDA #$00
        STA $00F4        ; pointer low
        LDA #$03
        STA $00F5        ; pointer high
        LDX #$00
        LDA #$66
        STA ($00F4,X)
        LDA $0300
        STA $0026        ; mem at $0300 after indirect write attempt
        ; Diagnostics written: X at $0021, loaded at $0022, mem03FF at $0023,
        ; mem0400_before at $0024, mem0400_after_abs at $0025, mem0400_after_ind at $0026
        ; Additional write-check diagnostics
        LDA #$9A
        STA $03FF        ; write to $03FF
        LDA $03FF
        STA $0027        ; readback from $03FF
        LDA #$88
        STA $0400        ; write to $0400
        LDA $0400
        STA $0028        ; readback from $0400
        ; try indirect write: STA ($00F4,X) with pointer to $0400
        LDA #$00
        STA $00F4
        LDA #$04
        STA $00F5
        LDX #$00
        LDA #$77
        STA ($00F4,X)
        LDA $0400
        STA $0029        ; readback after indirect
        ; record failure code
        LDA #$24
        STA $0020
        JMP End

T31:    ; Zero page,X wrap test: $00FF + X -> $0000
        LDA #$44
        STA $0000
        LDX #$01
        ; capture $0100 for wrap diagnostic
        LDA $0100
        STA $0026        ; $0100
        LDA $00FF,X
        STA $0022        ; loaded value
        STX $0021        ; X value
        ; capture nearby memory for diagnostics
        LDA $0000
        STA $0023        ; $0000
        LDA $00FF
        STA $0024        ; $00FF
        LDA $0300
        STA $0025        ; $0300
        ; Compare against earlier loaded value (LDA $00FF,X stored in $0022)
        LDA $0022
        CMP #$44
        BEQ T32
        ;; record failure and keep diagnostics
        LDA #$25
        STA $0020
        JMP End


T32:    ; BIT zero flag test (Z set)
        LDA #$00
        STA $0500
        LDA #$00
        STA $0501
        BIT $0501
        BEQ T33_ok
        LDA #$26
        STA $0020
        JMP End

T33_ok:
        ; Additional tests: JMP indirect page-boundary wrap (6502 bug) and SBC edge cases
        ; JMP indirect wrap test: set pointer low at $02FF and force wrap-high at $0200 -> target = $3000
        LDA #$00
        STA $02FF
        LDA #$30
        STA $0200
        LDA #$31
        STA $0300
        JMP ($02FF)        ; should wrap and jump to $3000; target sets $002A=1 and JMPs back
T34_check:
        LDA $002A
        CMP #$01
        BEQ T34_ok
        LDA #$27
        STA $0020
        JMP End
T34_ok:
        ; SBC tests: subtraction with and without borrow (carry)
        LDA #$02
        SEC
        SBC #$01
        CMP #$01
        BNE T35_fail
        BCS T35_ok1
T35_fail:
        LDA #$28
        STA $0020
        JMP End
T35_ok1:
        LDA #$00
        CLC
        SBC #$01
        STA $002D        ; save A (SBC result)
        ; Check carry/borrow first (CMP modifies carry), then verify A
        BCC T35_ok_carry
        LDA #$29
        STA $0020
        JMP End
T35_ok_carry:
        LDA $002D
        CMP #$FE
        BNE T35_fail2
        JMP T35_ok2
T35_fail2:
        STA $002D        ; diagnostic: SBC result (A)
        PHP
        PLA
        STA $002E        ; diagnostic: P (status byte)
        LDA #$29
        STA $0020
        JMP End
T35_ok2:

; ==========================================================================
; New tests: exercise all recompiler-compiled paths and verify correctness
; ==========================================================================

; --- T36: ZP addressing (recompiler promotes zp -> abs) ---
T36:    LDA #$CD
        STA $0010        ; STA zp
        LDA $0010        ; LDA zp
        CMP #$CD
        BEQ T36_ok
        LDA #$30
        STA $0020
        JMP End
T36_ok:
        LDX #$CD
        STX $0010        ; STX zp
        LDX $0010        ; LDX zp
        CPX #$CD
        BEQ T36_ok2
        LDA #$31
        STA $0020
        JMP End
T36_ok2:
        LDY #$CD
        STY $0010        ; STY zp
        LDY $0010        ; LDY zp
        CPY #$CD
        BEQ T37
        LDA #$32
        STA $0020
        JMP End

; --- T37: Absolute addressing (address translated) ---
T37:    LDA #$E1
        STA $0300        ; STA abs
        LDA $0300        ; LDA abs
        CMP #$E1
        BEQ T37_ok
        LDA #$33
        STA $0020
        JMP End
T37_ok:
        LDX #$E2
        STX $0300        ; STX abs
        LDX $0300        ; LDX abs
        CPX #$E2
        BEQ T37_ok2
        LDA #$34
        STA $0020
        JMP End
T37_ok2:
        LDY #$E3
        STY $0300        ; STY abs
        LDY $0300        ; LDY abs
        CPY #$E3
        BEQ T38
        LDA #$35
        STA $0020
        JMP End

; --- T38: Absolute,X addressing (address translated + X offset) ---
T38:    LDA #$F1
        LDX #$05
        STA $0300,X      ; STA abs,X -> $0305
        LDA $0300,X      ; LDA abs,X
        CMP #$F1
        BEQ T38_ok
        LDA #$36
        STA $0020
        JMP End
T38_ok:

; --- T39: Absolute,Y addressing (address translated + Y offset) ---
T39:    LDA #$F2
        LDY #$03
        STA $0300,Y      ; STA abs,Y -> $0303
        LDA $0300,Y      ; LDA abs,Y
        CMP #$F2
        BEQ T39_ok
        LDA #$37
        STA $0020
        JMP End
T39_ok:

; --- T40: ASL/LSR/ROL/ROR memory modes (abs, recompiled) ---
T40:    LDA #$41          ; 0100 0001
        STA $0310
        ASL $0310         ; -> 1000 0010 = $82, C=0
        LDA $0310
        CMP #$82
        BEQ T40_ok
        LDA #$38
        STA $0020
        JMP End
T40_ok:
        LSR $0310         ; -> 0100 0001 = $41, C=0
        LDA $0310
        CMP #$41
        BEQ T40_ok2
        LDA #$39
        STA $0020
        JMP End
T40_ok2:
        LDA #$01
        STA $0311
        SEC
        ROL $0311         ; C=1 rotated in: $01 -> $03, C=0
        LDA $0311
        CMP #$03
        BEQ T40_ok3
        LDA #$3A
        STA $0020
        JMP End
T40_ok3:
        SEC
        ROR $0311         ; C=1 rotated in: $03 -> $81, C=1
        LDA $0311
        CMP #$81
        BEQ T41
        LDA #$3B
        STA $0020
        JMP End

; --- T41: INC/DEC memory abs (address translated) ---
T41:    LDA #$10
        STA $0312
        INC $0312
        LDA $0312
        CMP #$11
        BEQ T41_ok
        LDA #$3C
        STA $0020
        JMP End
T41_ok:
        DEC $0312
        DEC $0312
        LDA $0312
        CMP #$0F
        BEQ T42
        LDA #$3D
        STA $0020
        JMP End

; --- T42: INX/INY/DEX/DEY (implied, recompiled) ---
T42:    LDX #$7F
        INX
        CPX #$80
        BEQ T42_ok
        LDA #$3E
        STA $0020
        JMP End
T42_ok:
        DEX
        CPX #$7F
        BEQ T42_ok2
        LDA #$3F
        STA $0020
        JMP End
T42_ok2:
        LDY #$00
        DEY
        CPY #$FF
        BEQ T42_ok3
        LDA #$40
        STA $0020
        JMP End
T42_ok3:
        INY
        CPY #$00
        BEQ T43
        LDA #$41
        STA $0020
        JMP End

; --- T43: TAX/TXA/TAY/TYA flag tests (Z and N) ---
T43:    LDA #$00
        TAX               ; X=0, Z=1, N=0
        BNE T43_fail
        LDA #$80
        TAY               ; Y=$80, Z=0, N=1
        BPL T43_fail
        TYA               ; A=$80
        TAX               ; X=$80
        TXA               ; A=$80
        BMI T43_ok
T43_fail:
        LDA #$42
        STA $0020
        JMP End
T43_ok:

; --- T44: CLC/SEC/CLV (implied, recompiled) ---
T44:    CLC
        BCS T44_fail
        SEC
        BCC T44_fail
        ; Set overflow, then CLV
        CLC
        LDA #$50
        ADC #$50          ; V=1
        CLV               ; V=0
        BVS T44_fail
        JMP T45
T44_fail:
        LDA #$43
        STA $0020
        JMP End

; --- T45: CMP flags: less-than, equal, greater-than ---
T45:    LDA #$10
        CMP #$20          ; A < operand -> C=0, Z=0
        BCS T45_fail
        BEQ T45_fail
        LDA #$20
        CMP #$10          ; A > operand -> C=1, Z=0, N=0
        BCC T45_fail
        BEQ T45_fail
        BMI T45_fail
        LDA #$10
        CMP #$10          ; A = operand -> C=1, Z=1
        BCC T45_fail
        BNE T45_fail
        JMP T46
T45_fail:
        LDA #$44
        STA $0020
        JMP End

; --- T46: CPX/CPY flag checks ---
T46:    LDX #$05
        CPX #$10          ; X < operand
        BCS T46_fail
        CPX #$05          ; X = operand
        BNE T46_fail
        CPX #$01          ; X > operand
        BCC T46_fail
        LDY #$80
        CPY #$00          ; Y > operand, N=1 (result $80)
        BCC T46_fail
        BEQ T46_fail
        JMP T47
T46_fail:
        LDA #$45
        STA $0020
        JMP End

; --- T47: ADC multi-byte (16-bit add via carry chain) ---
T47:    CLC
        LDA #$FF
        ADC #$01          ; A=$00, C=1
        STA $0010         ; low byte = $00
        LDA #$00
        ADC #$00          ; A=$01 (carry in), C=0
        STA $0011         ; high byte = $01
        LDA $0010
        CMP #$00
        BNE T47_fail
        LDA $0011
        CMP #$01
        BEQ T48
T47_fail:
        LDA #$46
        STA $0020
        JMP End

; --- T48: SBC multi-byte (16-bit sub via borrow chain) ---
T48:    SEC
        LDA #$00
        SBC #$01          ; A=$FF, C=0 (borrow)
        STA $0010         ; low byte = $FF
        LDA #$01
        SBC #$00          ; A=$00 (borrow consumed), C=1
        STA $0011         ; high byte = $00
        LDA $0010
        CMP #$FF
        BNE T48_fail
        LDA $0011
        CMP #$00
        BEQ T49
T48_fail:
        LDA #$47
        STA $0020
        JMP End

; --- T49: AND/ORA/EOR with zero-page (compiled as abs) ---
T49:    LDA #$FF
        STA $0012
        LDA #$0F
        AND $0012         ; $0F AND $FF = $0F
        CMP #$0F
        BNE T49_fail
        LDA #$F0
        STA $0012
        LDA #$0F
        ORA $0012         ; $0F ORA $F0 = $FF
        CMP #$FF
        BNE T49_fail
        LDA #$AA
        STA $0012
        LDA #$FF
        EOR $0012         ; $FF EOR $AA = $55
        CMP #$55
        BEQ T50
T49_fail:
        LDA #$48
        STA $0020
        JMP End

; --- T50: BIT zero-page (compiled as abs) N/V/Z flags ---
T50:    LDA #$C0
        STA $0013         ; mem=$C0 (bit7=1, bit6=1)
        LDA #$FF          ; A=$FF (non-zero AND)
        BIT $0013
        BPL T50_fail      ; N should be set (bit 7)
        BVC T50_fail      ; V should be set (bit 6)
        BEQ T50_fail      ; Z should be clear (A AND mem != 0)
        LDA #$00
        BIT $0013         ; A=$00 AND $C0 = $00 -> Z=1
        BNE T50_fail
        JMP T51
T50_fail:
        LDA #$49
        STA $0020
        JMP End

; --- T51: Backward branch (recompiler optimizes these) ---
T51:    LDX #$03
T51_loop:
        DEX
        BNE T51_loop      ; backward branch, should run 3 times
        CPX #$00
        BEQ T51_ok
        LDA #$4A
        STA $0020
        JMP End
T51_ok:

; --- T52: Forward branch (recompiler interprets these) ---
T52:    LDA #$01
        CMP #$01
        BEQ T52_fwd       ; forward branch
        LDA #$4B
        STA $0020
        JMP End
T52_fwd:
        LDA #$00
        CMP #$01
        BEQ T52_fail      ; should NOT be taken
        JMP T53
T52_fail:
        LDA #$4C
        STA $0020
        JMP End

; --- T53: NOP (compiled directly) ---
T53:    LDA #$55
        NOP
        NOP
        NOP
        CMP #$55          ; A unchanged after NOPs
        BEQ T54
        LDA #$4D
        STA $0020
        JMP End

; --- T54: JSR/RTS deep nesting (interpreted path) ---
T54:    LDA #$00
        STA $0014         ; counter
        JSR Sub02
        LDA $0014
        CMP #$02          ; Sub02 calls Sub03, both increment
        BEQ T55
        LDA #$4E
        STA $0020
        JMP End

Sub02:  INC $0014
        JSR Sub03
        RTS
Sub03:  INC $0014
        RTS

; --- T55: Mixed compiled/interpreted sequence ---
; LDA imm (compiled), STA zp (compiled), (indirect,X) load (interpreted), compare
T55:    LDA #$AB
        STA $0015         ; compiled STA zp
        LDA #$15
        STA $00F0         ; pointer low (to $0015)
        LDA #$00
        STA $00F1         ; pointer high
        LDX #$00
        LDA ($00F0,X)     ; interpreted - read from $0015
        CMP #$AB
        BEQ T56
        LDA #$4F
        STA $0020
        JMP End

; --- T56: (indirect),Y read (interpreted) ---
T56:    LDA #$00
        STA $00F2         ; pointer low -> $0300
        LDA #$03
        STA $00F3         ; pointer high
        LDA #$77
        STA $0305         ; target data
        LDY #$05
        LDA ($00F2),Y     ; interpreted - read $0300+5=$0305
        CMP #$77
        BEQ T57
        LDA #$50
        STA $0020
        JMP End

; --- T57: Zero-page,X (interpreted) ---
T57:    LDA #$DD
        STA $0015
        LDX #$05
        LDA $0010,X       ; zpx -> read $0015
        CMP #$DD
        BEQ T58
        LDA #$51
        STA $0020
        JMP End

; --- T58: ADC overflow clear (positive + negative, no overflow) ---
T58:    CLC
        LDA #$50          ; +80
        ADC #$90          ; +(-112) -> $E0 (-32), V=0 (signs differ)
        BVS T58_fail
        CMP #$E0
        BEQ T59
T58_fail:
        LDA #$52
        STA $0020
        JMP End

; --- T59: SBC overflow set (positive - negative overflows) ---
T59:    SEC
        LDA #$50          ; +80
        SBC #$B0          ; -(-80) = +80+80 = $A0, V=1
        BVC T59_fail
        CMP #$A0
        BEQ T60
T59_fail:
        LDA #$53
        STA $0020
        JMP End

; --- T60: ASL carry out test ---
T60:    LDA #$80
        ASL               ; $80 << 1 = $00, C=1
        BCC T60_fail
        CMP #$00
        BNE T60_fail
        ; Now LSR with odd number -> carry set
        LDA #$01
        LSR               ; $01 >> 1 = $00, C=1
        BCC T60_fail
        CMP #$00
        BEQ T61
T60_fail:
        LDA #$54
        STA $0020
        JMP End

; --- T61: PHA/PLA sequence preserves order (compiled path) ---
T61:    LDA #$11
        PHA
        LDA #$22
        PHA
        LDA #$33
        PHA
        PLA               ; should get $33
        CMP #$33
        BNE T61_fail
        PLA               ; should get $22
        CMP #$22
        BNE T61_fail
        PLA               ; should get $11
        CMP #$11
        BEQ T62
T61_fail:
        LDA #$55
        STA $0020
        JMP End

; --- T62: TSX/TXS (compiled - LDX/STX on emulated SP) ---
; Save and restore SP through TSX/TXS
T62:    TSX               ; X = current SP
        STX $0016         ; save original SP
        LDX #$EE
        TXS               ; SP = $EE
        TSX               ; X = SP (should be $EE)
        CPX #$EE
        BNE T62_fail
        LDX $0016         ; restore original SP
        TXS
        JMP T63
T62_fail:
        LDA #$56
        STA $0020
        JMP End

; --- T63: Tight backward loop with multiple compiled ops ---
T63:    LDX #$00
        LDA #$00
T63_loop:
        CLC
        ADC #$03          ; accumulate: A += 3
        INX
        CPX #$04
        BNE T63_loop      ; 4 iterations -> A = 12 = $0C
        CMP #$0C
        BEQ T64
        LDA #$57
        STA $0020
        JMP End

; --- T64: CMP with absolute memory (compiled) ---
T64:    LDA #$99
        STA $0313
        LDA #$99
        CMP $0313         ; abs CMP, should set Z=1, C=1
        BNE T64_fail
        BCC T64_fail
        JMP T65
T64_fail:
        LDA #$58
        STA $0020
        JMP End

; --- T65: EOR to toggle all bits (compiled) ---
T65:    LDA #$A5
        EOR #$FF          ; complement -> $5A
        CMP #$5A
        BNE T65_fail
        EOR #$5A          ; EOR with self -> $00
        BNE T65_fail
        JMP T_DONE
T65_fail:
        LDA #$59
        STA $0020
        JMP End

T_DONE:
        ; All tests passed - mark legacy result and fall through to End handler
        LDA #$AA
        STA $0020

; Results layout (functional test results table)
; $0030..$003E: per-test result bytes (0 = pass, non-zero = fail code)
; $003F: final marker ($AA for all-pass, other for done with failures)
; Legacy failure codes mapping (written at $0020):
;  $25 = failure reported by T31 (legacy test mapping)

RESULTS = $0030
RESULT_LEN = $000F

End:
        ; End handler: translate legacy single-failure code at $0020 into per-test table
        LDA $0020          ; legacy failure code or $AA for all-pass
        CMP #$AA
        BEQ AllPass

        ; Failure path: record context (do this BEFORE clobbering registers)
        STA $0040          ; write failure code into context[0]
        STX $0042          ; store X into context[2]
        STY $0043          ; store Y into context[3]
        TSX
        STX $0044          ; store SP into context[4]
        ; Small memory snapshot for context ($0010..$0013)
        LDA $0010
        STA $0045
        LDA $0011
        STA $0046
        LDA $0012
        STA $0047
        LDA $0013
        STA $0048

        ; Now store failure code into RESULTS[failure_code-1]
        ; compute index = A - 1
        SEC
        LDA $0020
        SBC #$01
        TAX
        LDA $0020
        STA RESULTS,X
        ; write done marker (not all-pass)
        LDA #$55
        STA $003F
        JMP End

AllPass:
        ; All tests passed: zero results table and set all-pass marker
        LDY #$00
PClear:
        LDA #$00
        STA RESULTS,Y
        INY
        CPY #$10
        BNE PClear
        LDA #$AA
        STA $003F
        JMP End

; ----------------------
; All tests passed
; ----------------------
        ; Stubs for JMP indirect page-wrap test (placed in ROM so JMP targets are reachable)
        .org $3000
JMP_WRAPPED_OK:
        LDA #$01
        STA $002A
        JMP T34_check

        .org $3100
JMP_NONWRAP_OK:
        LDA #$02
        STA $002A
        JMP T34_check

        .org $3FFC
        .word RESET
        .word RESET

