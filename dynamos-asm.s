	ifnd GAME_NUMBER
GAME_NUMBER = 0
	endif

	ifnd PLATFORM_NES
PLATFORM_NES = 0
	endif

	ifnd PLATFORM_MILLIPEDE
PLATFORM_MILLIPEDE = 0
	endif

	ifnd PLATFORM_ASTEROIDS
PLATFORM_ASTEROIDS = 0
	endif

	ifnd ENABLE_NATIVE_STACK
ENABLE_NATIVE_STACK = 0
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

	if (GAME_NUMBER == 4)
_ROM_OFFSET = $2800
_ROM_NAME = _rom_cpu6502test
	endif

	if (GAME_NUMBER == 5)
_ROM_OFFSET = $4000
_ROM_NAME = _rom_millipede
	endif

	if (GAME_NUMBER == 6)
_ROM_OFFSET = $6800
_ROM_NAME = _rom_asteroids
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
BLOCK_SENTINEL	=	$AA

; --- Bank assignments: MUST match bank_map.h ---
BANK_PC			=	19
BANK_PC_FLAGS	=	27
	if PLATFORM_MILLIPEDE
BANK_RENDER		=	20
	else
	if PLATFORM_ASTEROIDS
BANK_RENDER		=	20
	else
	ifnd PLATFORM_NES
BANK_RENDER		=	22
	else
BANK_RENDER		=	21
	endif
	endif
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
	if PLATFORM_NES
_RAM_BASE:	reserve $800		; Full 2 KB NES internal RAM
_CHARACTER_RAM_BASE = _RAM_BASE + $400	; alias — keeps C references valid
	else
	if PLATFORM_MILLIPEDE
_RAM_BASE:	reserve $400
_CHARACTER_RAM_BASE = _RAM_BASE	; dummy alias — not used at runtime
	else
	if PLATFORM_ASTEROIDS
_RAM_BASE:	reserve $400
_CHARACTER_RAM_BASE = _RAM_BASE	; dummy alias — not used at runtime
	else
_RAM_BASE:	reserve $400
_CHARACTER_RAM_BASE:	reserve $800
	endif
	endif
	endif
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
; Millipede arcade ROM data
; Program ROM (16KB, 4 × 4KB) in bank23
; Character ROM (4KB, 2 × 2KB planes) + color PROM in bank24
;=======================================================

	if (GAME_NUMBER == 5)
	section "bank23"
	global _rom_millipede
	align 8
_rom_millipede:
	incbin "roms\milliped\136013-104.mn1"	; $4000-$4FFF
	incbin "roms\milliped\136013-103.l1"	; $5000-$5FFF
	incbin "roms\milliped\136013-102.jk1"	; $6000-$6FFF
	incbin "roms\milliped\136013-101.h1"	; $7000-$7FFF

	section "bank24"
	global _chr_millipede, _color_prom_millipede
	align 8
_chr_millipede:
	incbin "roms\milliped\136013-107.r5"	; character plane 0 (2KB)
	incbin "roms\milliped\136013-106.p5"	; character plane 1 (2KB)
	align 8
_color_prom_millipede:
	incbin "roms\milliped\136001-213.e7"	; color PROM (256 bytes)
	endif

;=======================================================
; Asteroids arcade ROM data
; Program ROM (6KB, 3 × 2KB) + Vector ROM (2KB) in bank23
;=======================================================

	if (GAME_NUMBER == 6)
	section "bank23"
	global _rom_asteroids, _rom_asteroids_vec
	align 8
_rom_asteroids:
	incbin "roms\asteroid\035145-04e.ef2"	; $6800-$6FFF (2KB)
	incbin "roms\asteroid\035144-04e.h2"	; $7000-$77FF (2KB)
	incbin "roms\asteroid\035143-02.j2"	; $7800-$7FFF (2KB)
	align 8
_rom_asteroids_vec:
	incbin "roms\asteroid\035127-02.np3"	; $4800-$4FFF vector ROM (2KB)
	endif

;=======================================================
; NES ROM data — bank20 = PRG, bank23 = CHR
; Build script copies the target game's .prg/.chr to
; roms\nes\_active.prg and roms\nes\_active.chr.
;=======================================================

	if (GAME_NUMBER == 10)
	section "bank20"
	global _rom_nes_prg
	align 8
_rom_nes_prg:
	incbin "roms\nes\_active.prg"

	section "bank23"
	global _chr_nes
	align 8
_chr_nes:
	incbin "roms\nes\_active.chr"
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
	global _fff0_dispatch, _fff0_dispatch_a_saved
;-------------------------------------------------------	
; $FFF0 trampoline — patchable template dispatch hub.
;
; Two entry points in the 10-byte space before NES vectors ($FFF0-$FFF9):
;
; _fff0_dispatch ($FFF0) — for patchable branches (guest A is live).
;   Saves A to _a, pushes guest flags, dispatches.
;   Caller must have _pc already set to the branch target.
;
; _fff0_dispatch_a_saved ($FFF6) — for patchable JMPs (_a already saved).
;   Pushes guest flags, dispatches.
;   Caller must have _a and _pc already set.
;
; On entry (both):  X, Y = guest values (saved by flash_dispatch_return)
;                   _pc  = target guest PC (set by caller before JMP)
; On exit:          returns through flash_dispatch_return → C dispatcher
;
; When a target is compiled, the JMP $FFF0/$FFF6 operand is patched in flash
; to jump directly to the compiled native code (2 flash byte writes, no
; branch-byte patch needed).  If the target isn't 16-byte aligned, a local
; JMP $FFFF trampoline is emitted at an aligned address within the block
; and the operand is patched to that instead.
;
_fff0_dispatch:
	sta _a				; save guest A (2B)
	php					; push guest flags (1B)
	jmp _flash_dispatch_return	; save X/Y, pop status, return (3B)

_fff0_dispatch_a_saved:
	php					; push guest flags (1B)
	jmp _flash_dispatch_return	; save X/Y, pop status, return (3B)
	; Total: 10 bytes ($FFF0-$FFF9), vectors follow at $FFFA

;=======================================================	
	section "data"
	global _cross_bank_dispatch
	zpage _dispatch_sp, _nmi_yield
;-------------------------------------------------------
; Cross-bank dispatch trampoline (fixed bank)
; Called from patchable epilogue slow path when exit_pc is in a different
; flash bank.  Instead of returning to C and re-entering dispatch_on_pc,
; saves regs/status and re-dispatches directly — eliminating the C round-trip.
;
; On entry (from epilogue slow path):
;   _a, _pc already saved by epilogue
;   X, Y still live from guest code
;   Stack: [epilogue's PHP] [.dispatch_addr JSR return] [caller return]
;   (PHP may be missing if the IR lowerer produced code with an instruction
;    alignment issue — the epilogue's PHP byte gets consumed as a data
;    operand of the preceding instruction.)
;
; Pops one byte as best-effort _status, then restores SP from _dispatch_sp
; (saved before the JSR) to cleanly discard remaining stack bytes.
_cross_bank_dispatch:
	stx _x				; save X (not saved by epilogue)
	sty _y				; save Y (not saved by epilogue)
	; Pop whatever is on top — epilogue's PHP (correct _status) or
	; the JSR return byte (wrong _status, if PHP was missing due to
	; instruction alignment issues in IR-lowered code).  Either way
	; we get a best-effort _status and avoid crashing.
	pla
	sta _status
	if ENABLE_NATIVE_STACK
	; --- Native stack mode ---
	; PLA was from guest stack.  Save guest SP, restore host SP.
	tsx
	stx _sp				; save guest SP
	endif
	; Restore stack pointer to the level saved before JSR
	; .dispatch_addr_instruction.  This cleanly discards the remaining
	; JSR return bytes (and any stale PHP), regardless of whether the
	; block pushed PHP or not.  Without this, a missing PHP causes the
	; third PLA to consume a byte from the caller's frame, and the
	; subsequent not_recompiled RTS jumps to garbage.
	lda #0
	sta _nmi_yield			; clear VBlank yield flag (prevent re-yield)
	ldx _dispatch_sp
	txs
	if PLATFORM_MILLIPEDE
	; Millipede delivers guest IRQ via C code (irq6502).
	; Must return to C so the main loop can fire the IRQ.
	; Without this, the yield → dispatch_on_pc loop never
	; reaches the C IRQ check, so the VBLANK IRQ never fires
	; and the game hangs at the wait_vblank spin ($4026).
	jmp _flash_dispatch_return_status_saved
	else
	jmp _dispatch_on_pc	; re-dispatch _pc without C round-trip
	endif

;=======================================================	
	section "data"
	global _nmi_yield_hook
;-------------------------------------------------------
; NMI VBlank yield hook (WRAM, called from lazyNES NMI handler)
;
; The lazyNES NMI handler calls JSR _nmiCallback every VBlank.
; A post-build patch redirects that JSR to this hook, which sets
; bit 7 of nmi_yield — signaling compiled backward-branch stubs
; to yield on their next iteration.  A/X/Y are already saved by
; the NMI handler, so we can freely use A here.
;
_nmi_yield_hook:
	lda #$80
	sta _nmi_yield			; set bit 7 — backward branch stubs see BMI
	rts

;=======================================================
	section "data"
	global _nmi_sp_trampoline, _nmi_sp_restore
	global _lnPush_safe
;-------------------------------------------------------
; NMI sp-guard trampolines (WRAM)
;
; The lazyNES NMI handler (___nmi) uses  ASL $20  to process nfOamFull
; flag bits.  ZP $20-$21 is also the vbcc software stack pointer (sp).
; Every NMI fires ~60 Hz and shifts sp's low byte, corrupting it.
;
; Fix: a post-build patch redirects the NMI vector to this entry
; trampoline, which saves sp to WRAM, loads the real nfOamFull shadow
; into $20, then jumps to the original NMI handler.  A second patch
; replaces the NMI handler's final TAX;PLA;RTI with JMP _nmi_sp_restore,
; which saves the updated nfOamFull, restores sp, and does RTI.
;
; Entry trampoline — replaces NMI vector target
_nmi_sp_trampoline:
	pha					; save A  (NMI handler expects it unsaved)
	lda $20
	sta _nmi_sp_save		; save sp lo
	lda $21
	sta _nmi_sp_save+1		; save sp hi
	lda _nmi_nfOamFull		; load real nfOamFull shadow
	sta $20				; NMI handler will see correct flag bits
	pla					; restore A
	jmp $C192			; jump to original ___nmi (PHA, TXA, PHA…)

; Exit trampoline — replaces TAX;PLA;RTI at end of ___nmi
; On entry: A = value from the PLA that was before TAX in the original.
;           The hardware stack still has the original A value to PLA.
_nmi_sp_restore:
	tax					; finish X restore
	pla					; restore original A
	; Now A/X/Y are the interrupted code's registers.
	; Save updated nfOamFull and restore sp, using the hardware stack.
	pha					; save A temporarily
	lda $20
	sta _nmi_nfOamFull		; save modified nfOamFull back to shadow
	lda _nmi_sp_save
	sta $20				; restore sp lo
	lda _nmi_sp_save+1
	sta $21				; restore sp hi
	pla					; restore A
	rti

;-------------------------------------------------------
; lnPush_safe — sp-safe wrapper for lazyNES lnPush
;
; lnPush ($C23B) also uses ASL $20 for nfOamFull processing.
; This wrapper saves sp, loads the real nfOamFull, calls lnPush,
; saves the updated nfOamFull, and restores sp.
; vbcc arguments (r0-r5) are untouched.
;
	global _lnPush
_lnPush_safe:
	pha					; save A
	lda $20
	sta _lnpush_sp_save		; save sp lo
	lda $21
	sta _lnpush_sp_save+1	; save sp hi
	lda _nmi_nfOamFull
	sta $20				; lnPush expects nfOamFull at $20
	pla					; restore A
	jsr _lnPush			; call real lnPush
	pha					; save A
	lda $20
	sta _nmi_nfOamFull		; save updated nfOamFull
	lda _lnpush_sp_save
	sta $20				; restore sp lo
	lda _lnpush_sp_save+1
	sta $21				; restore sp hi
	pla					; restore A
	rts

; Data: shadow variables for the sp guard
_nmi_sp_save:		ds 2	; sp save area for NMI trampoline
_nmi_nfOamFull:		db 0	; real nfOamFull shadow (starts 0 = no flags)
_lnpush_sp_save:	ds 2	; sp save area for lnPush wrapper

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
_dispatch_on_pc:
	if PLATFORM_MILLIPEDE
	; --- Millipede ROM mirror guard ---
	; Real hardware: A14 chip-select ⇒ ROM at $4000-$7FFF and $C000-$FFFF.
	; $8000-$BFFF is NOT ROM (A14=0).  Canonicalize mirrors before the
	; bank-number math, otherwise (pc>>14)+BANK_PC_FLAGS hits live code/data
	; banks (BANK_IR_OPT, BANK_MILLIPEDE_CHR) instead of PC tables → crash.
	lda _pc+1
	cmp #$C0
	bcc .mp_no_c_mirror
	; $C0-$FF: ROM mirror → fold to $40-$7F
	and #$3F
	ora #$40
	sta _pc+1
	bne .mp_dispatch		; always taken (result $40-$7F)
.mp_no_c_mirror:
	cmp #$40
	bcc .mp_interpret		; $00-$3F: RAM / IO → interpret
	cmp #$80
	bcc .mp_dispatch		; $40-$7F: canonical ROM range
.mp_interpret:
	lda #2					; return "interpret"
	rts
.mp_dispatch:
	endif

	; D0-D13 - address in bank   pc_flags
	lda #0
	sta temp		; INIT temp to 0 (required: ROL temp uses residual value)
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
	
	lda target_bank
	if PLATFORM_NES
	; (flash_exec_bank removed — flash data copy lives in same bank as code)
	endif
	sta $C000

	; --- Block cycle counting (DISABLED) ---
	; DISABLED: clockticks6502 ($3F-$42) overlaps guest zero-page state.
	; For normal block entries, header byte +6 = $FF (>= $FE → skip),
	; so no corruption.  But fence mid-block entries read random compiled
	; code bytes at *(native_addr-2); values < $FE get added to $3F and
	; carry propagates into $40/$41 — corrupting guest variables (e.g.
	; DK's nfTrigger at $40).  ENABLE_BLOCK_CYCLES is disabled, so
	; skip unconditionally.  When re-enabled, fence entries will need
	; a proper solution (mini-headers or a non-ZP cycle accumulator).
.no_cycles:

	; NOTE: Block-complete sentinel ($AA at header+7 = dispatch_addr-1)
	; is written by the C compile path but NOT checked here.  An asm
	; guard was attempted (LDY #1 / LDA (addr_lo),Y / CMP #$AA / BNE
	; not_recompiled) but adding an in-range BNE to the same label as
	; the earlier relaxed BEQ/BMI caused vasm -opt-branch to produce
	; inconsistent JMP targets, breaking ALL dispatches.  The deferred
	; PC table update (writing the table entry only after code is in
	; flash) is the primary crash fix; the sentinel write is kept as
	; forensic metadata for offline flash dumps.

	; Save stack pointer before JSR so cross_bank_dispatch can
	; restore it cleanly, even if the block's epilogue is missing PHP.
	tsx
	stx _dispatch_sp

	if ENABLE_NATIVE_STACK
	; --- Native stack mode ---
	; Swap to guest stack before restoring guest state.  The compiled
	; block runs entirely on the guest stack ($0100-$017F).  Dispatch
	; returns via _flash_dispatch_return which swaps back to host SP.
	ldx _sp				; load guest SP
	bpl .sp_ok			; $00-$7F is in guest range
	ldx #$7F			; clamp to guest top (game did TXS with SP>$7F)
	stx _sp
.sp_ok:
	txs					; switch to guest stack
	lda _status
	pha					; push status on GUEST stack
	lda _a
	ldx _x
	ldy _y
	plp					; pop status from GUEST stack
	; JMP (not JSR) — avoids pushing dispatch return addr on guest stack.
	; _flash_dispatch_return restores host SP and does RTS back to caller.
.dispatch_addr_instruction:
.dispatch_addr = * + 1
	jmp $FFFF			; self-modifying: target native code address

	else
	; --- Emulated stack mode ---
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
	endif
	
_flash_dispatch_return:	
	; Note: _a, _pc, and status (on stack) are already set by the code block epilogue
	stx _x
	sty _y	
	
_flash_dispatch_return_no_regs:
	; Entry point when _a, _x, _y are already saved (used by normal epilogues)
	pla
	sta _status

	global _flash_dispatch_return_status_saved
_flash_dispatch_return_status_saved:
	; Entry point when _a, _x, _y AND _status are already saved
	; (used by NS_JSR/NS_RTS templates which save flags via php/pla/sta)

	if ENABLE_NATIVE_STACK
	; --- Native stack mode ---
	; We just PLA'd the epilogue's PHP from the GUEST stack.
	; Save updated guest SP, restore host SP for the dispatch RTS.
	tsx
	stx _sp				; save guest SP (may have changed from push/pull)
	ldx _dispatch_sp
	txs					; back to host stack
	endif

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
; with _last_nmi_frame.  If different, a real vblank has occurred.
; Always bail to C on VBlank so the C main loop can handle render_video(),
; nmi6502(), gamepad refresh, and nmi_active lifecycle (sp == nmi_sp_guard
; clearing).  Absorbing VBlanks here is unsafe because nmi_active may be
; stale (set to 1 by nmi6502() but not yet cleared since the C main loop
; hasn't run the sp == nmi_sp_guard check), causing permanent deadlock.
; The 60Hz bail overhead is negligible vs. correctness.
;
; On entry:  _pc = target, _sp = post-push SP, _native_jsr_saved_sp = pre-push SP
; On exit:   A = 0 if subroutine completed, non-zero if needs C
;-------------------------------------------------------
	zpage _last_nmi_frame, _native_jsr_saved_sp
	global _native_jsr_trampoline
_native_jsr_trampoline:
	; Save outer _native_jsr_saved_sp and _dispatch_sp on NES stack
	; for nesting safety.  If an inner NJSR fires, it overwrites both;
	; we restore the outer values on exit.
	lda _native_jsr_saved_sp
	pha
	lda _dispatch_sp
	pha
.njsr_loop:
	jsr _dispatch_on_pc		; dispatch current _pc block
	; A = 0: block ran from flash
	; A = 1: needs recompile
	; A = 2: needs interpret
	beq .njsr_check_vblank	; A=0 → block executed, continue
	cmp #2
	beq .njsr_interpret		; A=2 → inline interpret
	jmp .njsr_exit			; A=1 → bail to C for compile

.njsr_interpret:
	; Inline interpret: switch to bank 0 and run one guest instruction.
	; Avoids expensive C round-trip (~260 cycles) for interpret-only PCs.
	lda #0
	sta _mapper_prg_bank
	lda _mapper_chr_bank
	sta $C000
	jsr _interpret_6502
	; Fall through to check vblank and continue loop

.njsr_check_vblank:
	; Check for vblank: if NMI counter ($26) != last_nmi_frame, bail to C.
	lda $26					; lazyNES NMI counter (incremented every vblank)
	cmp _last_nmi_frame		; has a new vblank occurred?
	beq .njsr_no_vblank		; no → skip
	; VBlank occurred — bail to C main loop unconditionally.
	; Leave _last_nmi_frame stale so the C main loop detects the VBlank.
	; The C main loop handles render_video(), nmi6502(), gamepad refresh,
	; nmi_active clearing (sp == nmi_sp_guard), and recompile triggers.
	lda #1					; non-zero → forces return to C main loop
	jmp .njsr_exit
.njsr_no_vblank:
	
	; Block executed. Check if subroutine returned.
	; If emulated SP >= saved_sp, the RTS has popped back to (or past) entry level.
	lda _sp
	cmp _native_jsr_saved_sp
	bcc .njsr_loop			; SP < saved_sp → still in subroutine, dispatch next block
	
	; Subroutine completed (SP restored). _pc and _status are already
	; set correctly by the nrts template's inner dispatch.
	; Restore outer saved values (pop in reverse push order), return 0.
	pla						; restore outer _dispatch_sp
	sta _dispatch_sp
	pla						; restore outer _native_jsr_saved_sp
	sta _native_jsr_saved_sp
	pla						; discard njsr's PHP (don't overwrite _status!)
	lda #0					; return 0 = executed from flash
	rts						; return to outer dispatch_on_pc's JSR .dispatch_addr_instruction

.njsr_exit:
	; Bail to C — subroutine hit something that needs recompile/interpret.
	; _status was already saved by the last block's epilogue.
	; Preserve dispatch result (A=1 compile, A=2 interpret) across stack cleanup.
	tax						; save dispatch result in X
	pla						; restore outer _dispatch_sp
	sta _dispatch_sp
	pla						; restore outer _native_jsr_saved_sp
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
	; --- Range guard (defensive: VBCC -O2 optimizes away the C-side check) ---
	; Don't return "compile" for PCs outside the guest ROM address range.
	; Compiling garbage addresses (screen RAM, I/O space) writes bogus
	; PC-flag entries into flash, causing JMP-to-$0000 crashes on the
	; next dispatch to that address.
	lda _pc+1                    ; guest PC high byte
	cmp #(_ROM_OFFSET/256)       ; < ROM_ADDR_MIN high byte?
	bcc .out_of_range
	if PLATFORM_MILLIPEDE || PLATFORM_ASTEROIDS
	cmp #$80                     ; >= $8000? (ROM_ADDR_MAX = $7FFF)
	bcs .out_of_range
	else
	if PLATFORM_NES == 0
	cmp #$40                     ; >= $4000? (all Exidy: ROM_ADDR_MAX = $3FFF)
	bcs .out_of_range
	else
	; NES: ROM_ADDR_MAX = $FFFF, only lower bound matters.
	; (vector table $FFFA-$FFFF is harmless to compile — just data.)
	endif
	endif
	lda #1 ; needs recompile
	rts
.out_of_range:
	lda #2 ; out of range — interpret instead
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

;-------------------------------------------------------
; Dirty-flag subroutines (called from JIT blocks via JSR).
; Increments the screen/character dirty flag while preserving
; the processor status register.  Using JSR instead of inline
; PHP/INC/PLP prevents the PLP byte from being lost during
; IR optimisation or flash writes.
;-------------------------------------------------------
	global _dirty_flag_screen, _dirty_flag_char
_dirty_flag_screen:
	php
	inc _screen_ram_updated
	plp
	rts
_dirty_flag_char:
	php
	inc _character_ram_updated
	plp
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
	if !(PLATFORM_NES)
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
	endif  ; !(PLATFORM_NES) — end of screen_diff_build_list + BSS

;-------------------------------------------------------
; BSS for shadow buffer and update list (Exidy only)
	section "bss"
	if !(PLATFORM_NES)
_screen_shadow:		reserve $400	; 1024 bytes
_vram_update_list:	reserve 256	; tiles (96) + attr diff + palette diff + terminator
	endif
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
	section "text"
	global _opcode_6502_ns_jsr, _opcode_6502_ns_jsr_size
	global _opcode_6502_ns_jsr_ret_hi, _opcode_6502_ns_jsr_ret_lo
	global _opcode_6502_ns_jsr_tgt_lo, _opcode_6502_ns_jsr_tgt_hi
;-------------------------------------------------------
; Native Stack JSR template (27 bytes)
; ENABLE_NATIVE_STACK: pushes return address onto hardware guest stack
; using PHA (instead of _RAM_BASE+$100,x), then sets _pc and exits.
; Saves guest flags to _status directly (not on stack) since the PHA
; pushes would bury the PHP status byte below the return address.
; 6502 convention: push (pc+2) hi first, then lo.
_opcode_6502_ns_jsr:
	sta _a						; +0  save A (no flag change)
	php							; +2  save guest flags
	pla							; +3  pop flags → A
	sta _status					; +4  save to _status ZP
	stx _x						; +6  save X
	sty _y						; +8  save Y
_opcode_6502_ns_jsr_ret_hi_loc:
	lda #$FF					; +10 LDA #>(return_addr) - PATCH
	pha							; +12 push hi to GUEST stack
_opcode_6502_ns_jsr_ret_lo_loc:
	lda #$FF					; +13 LDA #<(return_addr) - PATCH
	pha							; +15 push lo to GUEST stack
_opcode_6502_ns_jsr_tgt_lo_loc:
	lda #$FF					; +16 LDA #<target - PATCH
	sta _pc						; +18
_opcode_6502_ns_jsr_tgt_hi_loc:
	lda #$FF					; +20 LDA #>target - PATCH
	sta _pc+1					; +22
	jmp _flash_dispatch_return_status_saved	; +24  (skip PLA — flags already in _status)

_opcode_6502_ns_jsr_end:
_opcode_6502_ns_jsr_size:	db (_opcode_6502_ns_jsr_end - _opcode_6502_ns_jsr)
_opcode_6502_ns_jsr_ret_hi: db (_opcode_6502_ns_jsr_ret_hi_loc - _opcode_6502_ns_jsr + 1)
_opcode_6502_ns_jsr_ret_lo: db (_opcode_6502_ns_jsr_ret_lo_loc - _opcode_6502_ns_jsr + 1)
_opcode_6502_ns_jsr_tgt_lo: db (_opcode_6502_ns_jsr_tgt_lo_loc - _opcode_6502_ns_jsr + 1)
_opcode_6502_ns_jsr_tgt_hi: db (_opcode_6502_ns_jsr_tgt_hi_loc - _opcode_6502_ns_jsr + 1)

;=======================================================
	section "text"
	global _opcode_6502_ns_rts, _opcode_6502_ns_rts_size
;-------------------------------------------------------
; Native Stack RTS template (25 bytes)
; ENABLE_NATIVE_STACK: pops return address from hardware guest stack
; using PLA (instead of _RAM_BASE+$100,x), adds 1, exits.
; Saves guest flags to _status directly (php/pla/sta) before the PLAs,
; so the return address bytes are TOS when we PLA.
; 6502 RTS convention: pull lo, pull hi, PC = (hi:lo) + 1.
_opcode_6502_ns_rts:
	sta _a						; +0  save A (no flag change)
	php							; +2  save guest flags
	pla							; +3  pop flags → A
	sta _status					; +4  save to _status ZP
	stx _x						; +6  save X
	sty _y						; +8  save Y
	pla							; +10 pull lo from GUEST stack
	sta _pc						; +11
	pla							; +13 pull hi from GUEST stack
	sta _pc+1					; +14
	inc _pc						; +16 add 1 (RTS convention)
	bne .ns_rts_no_carry		; +18
	inc _pc+1					; +20
.ns_rts_no_carry:
	jmp _flash_dispatch_return_status_saved	; +22  (skip PLA — flags already in _status)

_opcode_6502_ns_rts_end:
_opcode_6502_ns_rts_size: db (_opcode_6502_ns_rts_end - _opcode_6502_ns_rts)

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
	global _native_jsr_saved_sp, _dispatch_sp
;-------------------------------------------------------
_native_jsr_saved_sp:	reserve 1
_dispatch_sp:			reserve 1

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
	
	; Range check: addresses above safe RAM need full write6502.
	; Exidy: $5000+ is I/O.  NES: $2000+ is PPU / APU / mapper.
	; Writes to NES $8000-$FFFF trigger Mapper 30 bank register,
	; corrupting the flash code bank mid-execution.
	lda _indy_ea + 1
	if PLATFORM_NES
	cmp #$20
	else
	cmp #$50
	endif
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

; (ROM read thunks removed — replaced by flash data copy in dynamos.c)

;=======================================================
	section "data"
	global _trigger_soft_reset
;-------------------------------------------------------
; trigger_soft_reset() — jump to NES reset vector.
; Never returns.  Lives in WRAM so it is always reachable
; regardless of which bank is mapped at $8000-$BFFF.
;
; CRITICAL: Must map PRG bank 0 before jumping.  The C startup
; copies the data-init image from the switchable bank ($8000-$BFFF)
; to WRAM.  If the wrong bank is still mapped, WRAM gets garbage
; (all globals + WRAM code — flash_byte_program, peek_bank_byte,
; dispatch_on_pc, etc. — are corrupted).
;
_trigger_soft_reset:
	lda _mapper_chr_bank	; preserve CHR bank, PRG bank = 0
	sta $C000
	jmp ($FFFC)

;=======================================================
	section "data"
	global _sa_record_subroutine_runtime
;-------------------------------------------------------
; sa_record_subroutine_runtime(uint16_t target) — WRAM trampoline
; Saves current bank, switches to BANK_SA_CODE, calls
; sa_record_subroutine(target), then restores the previous bank.
; Lives in WRAM so it's reachable from any bank (callers in bank2).
; Argument already in r0/r1 per vbcc calling convention — passed
; through to sa_record_subroutine untouched.
;
	if PLATFORM_NES
SA_CODE_BANK = 19
IR_OPT_BANK = 28
	else
	if PLATFORM_MILLIPEDE
SA_CODE_BANK = 19
IR_OPT_BANK = 29
	else
	if PLATFORM_ASTEROIDS
SA_CODE_BANK = 19
IR_OPT_BANK = 29
	else
SA_CODE_BANK = 24
IR_OPT_BANK = 26
	endif
	endif
	endif

_sa_record_subroutine_runtime:
	lda _mapper_prg_bank		; save current PRG bank
	pha
	lda #SA_CODE_BANK			; switch to SA code bank
	sta _mapper_prg_bank
	ora _mapper_chr_bank
	sta $C000
	jsr _sa_record_subroutine	; target still in r0/r1
	pla							; restore previous PRG bank
	sta _mapper_prg_bank
	ora _mapper_chr_bank
	sta $C000
	rts

;=======================================================
	section "data"
	global _sa_ir_pipeline_full, _sa_ir_capture_exit, _sa_ir_lowered_size
;-------------------------------------------------------
; sa_ir_pipeline_full() — WRAM trampoline for SA pass 2 IR pipeline.
; Called from sa_compile_one_block (in BANK_SA_CODE) when the block
; needs IR optimize + lower.  Switches through banks 0, 17, 1 and
; back to BANK_SA_CODE.  Lives in WRAM so bankswitches don't affect
; the code being executed.
;
; On entry:  BANK_SA_CODE is mapped
; On exit:   BANK_SA_CODE restored, _sa_ir_lowered_size set
;

_sa_ir_lowered_size:
	reserve 1

_sa_ir_pipeline_full:
	; Step 1: ir_optimize(&ir_ctx) in BANK_IR_OPT
	lda #IR_OPT_BANK
	sta _mapper_prg_bank
	ora _mapper_chr_bank
	sta $C000
	lda #<_ir_ctx
	sta r0
	lda #>_ir_ctx
	sta r1
	jsr _ir_optimize
	; Step 2: ir_optimize_ext(&ir_ctx) in bank 17
	lda #17
	sta _mapper_prg_bank
	ora _mapper_chr_bank
	sta $C000
	lda #<_ir_ctx
	sta r0
	lda #>_ir_ctx
	sta r1
	jsr _ir_optimize_ext
	; Step 3: ir_opt_rmw_fusion(&ir_ctx) in bank 1
	lda #1
	sta _mapper_prg_bank
	ora _mapper_chr_bank
	sta $C000
	lda #<_ir_ctx
	sta r0
	lda #>_ir_ctx
	sta r1
	jsr _ir_opt_rmw_fusion
	; Step 4: ir_lower(&ir_ctx, cache_code[0], CACHE_CODE_BUF_SIZE)
	; still in bank 1
	lda #<_ir_ctx
	sta r0
	lda #>_ir_ctx
	sta r1
	lda #<_cache_code
	sta r2
	lda #>_cache_code
	sta r3
	lda #250				; CACHE_CODE_BUF_SIZE
	sta r4
	jsr _ir_lower
	; A = return value from ir_lower (pos = lowered byte count).
	; vbcc returns uint8_t in A, NOT r0.  r0 ($0000) is restored to
	; the ctx pointer low byte ($0E = 14) by the callee's register
	; restore epilogue.  Reading r0 here would always give $0E,
	; causing every SA block to get lowered_size=14 regardless of
	; actual size — stale cache_code data (including JMP $FFFF from
	; previous blocks' epilogues) leaks into flash → crash.
	sta _sa_ir_lowered_size	; save lowered_size for C caller
	beq .sa_skip_ci
	sta _code_index		; update code_index for resolution functions
.sa_skip_ci:
	; Step 5: ir_resolve_direct_branches() in bank 1 (no args)
	jsr _ir_resolve_direct_branches
	; Step 6: ir_rebuild_block_ci_map() in bank 1 (no args)
	jsr _ir_rebuild_block_ci_map
	; Step 6.5: ir_compute_instr_offsets() in bank 1 (no args)
	; Fills sa_ir_instr_native_off[] with post-lowering byte offsets
	; for SA pass 2 mid-block PC publishing.
	jsr _ir_compute_instr_offsets
	; Step 7: ir_resolve_deferred_patches() in bank 1 (no args)
	jsr _ir_resolve_deferred_patches
	; Restore BANK_SA_CODE
	lda #SA_CODE_BANK
	sta _mapper_prg_bank
	ora _mapper_chr_bank
	sta $C000
	rts

;-------------------------------------------------------
; sa_ir_capture_exit() — WRAM trampoline for SA pass 1 exit-state capture.
; Called from sa_run_b2 (in BANK_SA_CODE) to run IR optimize passes
; that populate ir_ctx.exit_* fields.
;
; On entry:  BANK_SA_CODE is mapped
; On exit:   BANK_SA_CODE restored, ir_ctx.exit_* populated
;
_sa_ir_capture_exit:
	; Step 1: ir_optimize(&ir_ctx) in BANK_IR_OPT
	lda #IR_OPT_BANK
	sta _mapper_prg_bank
	ora _mapper_chr_bank
	sta $C000
	lda #<_ir_ctx
	sta r0
	lda #>_ir_ctx
	sta r1
	jsr _ir_optimize
	; Step 2: ir_optimize_ext(&ir_ctx) in bank 17
	lda #17
	sta _mapper_prg_bank
	ora _mapper_chr_bank
	sta $C000
	lda #<_ir_ctx
	sta r0
	lda #>_ir_ctx
	sta r1
	jsr _ir_optimize_ext
	; Restore BANK_SA_CODE
	lda #SA_CODE_BANK
	sta _mapper_prg_bank
	ora _mapper_chr_bank
	sta $C000
	rts

;=======================================================	
	section "bank3"
	global _flash_cache_data, _flash_block_flags
;-------------------------------------------------------	
_flash_block_flags:
	reserve	FLASH_CACHE_BLOCKS

;=======================================================	
	section "bank4"	; occupies banks 4-16, 13 banks (bank 17 = BANK_COMPILE)
	global _flash_cache_code
;-------------------------------------------------------	
	align 14
_flash_cache_code:	reserve 16384

;=======================================================	
	section "bank19"	; banks 19-26, 8 banks
	global _flash_cache_pc, _flash_cache_pc_flags
;-------------------------------------------------------	
	ifnd PLATFORM_NES
	ifnd PLATFORM_ASTEROIDS
	align 14		; PC table: $0000-$1FFF (Exidy has no code here)
	endif
	endif
	; bank19 on NES/Asteroids = SA/init code, no align — section used by C code
_flash_cache_pc:	
	section "bank20"
	ifnd PLATFORM_NES
	ifnd PLATFORM_ASTEROIDS
	align 14		; PC table: Exidy ROM $2000-$3FFF
	endif
	endif
	; bank20 on NES = NES PRG-ROM data, no align — section used by asm incbin
	; bank20 on Asteroids = BANK_RENDER code, no align — section used by C code
	ifdef PLATFORM_NES
	; NES: skip bank21 entirely — used for BANK_RENDER C code.
	; If we enter section "bank21" here, the linker overlaps this
	; reserve with the C-emitted render_video_b2 / metrics code.
	; PC table for $4000-$5FFF is dead on NES anyway (I/O space).
	else
	section "bank21"
	; No align: $4000-$5FFF PC table is dead on Exidy.
	; Bank21 is repurposed for BANK_METRICS C code (metrics_dump_*_b2).
	endif
	section "bank22"
	ifdef PLATFORM_NES
	align 14		; PC table: NES PRG-RAM $6000-$7FFF
	endif
	ifdef PLATFORM_ASTEROIDS
	align 14		; PC table: Asteroids ROM $6800-$7FFF
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