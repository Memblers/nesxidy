# Bug #2: RECOMPILED Flag Logic - Detailed Analysis

## Flag Definition (dynamos.h)

```c
#define RECOMPILED		0x80	// 0 = has been recompiled.  bits D0-D5 contain PRG bank number
#define INTERPRETED		0x40	// 0 = interpret this instruction
#define	CODE_DATA		0x20	// 0 = code, 1 = data
```

**Critical Comment**: "0 = has been recompiled"
- This means: **RECOMPILED bit = 0 when code HAS been recompiled**
- Conversely: **RECOMPILED bit = 1 when code has NOT been recompiled (invalid/unclaimed)**

---

## Assembly Logic - dispatch_on_pc() (dynamos-asm.s:256-346)

```asm
_dispatch_on_pc:
    ...
    lya (addr_lo),y   ; get pc_flag (read flash_cache_pc_flags for this PC)
    bmi not_recompiled
    ...
```

**Analysis**:
- `bmi not_recompiled` = "Branch if Minus" = jump if bit 7 (RECOMPILED) is SET
- If RECOMPILED = 1 (bit 7 SET) → branch to `not_recompiled`
- If RECOMPILED = 0 (bit 7 CLEAR) → continue to execute compiled code

### not_recompiled Handler

```asm
not_recompiled:
    and #INTERPRETED
    bne not_interpreted
    lda #2 ; interpret this PC
    rts
    
not_interpreted:
    lda #1 ; recompile from this PC
    rts
```

**Return Values**:
- Return 2: Code NOT recompiled AND NOT interpreted → interpret this instruction
- Return 1: Code NOT recompiled BUT interpreted → recompile from this PC
- Return 0: (implicit) Code HAS been recompiled → code executed, PC advanced

---

## C Code Logic - flash_cache_search() (dynamos.c:328-348)

```c
uint8_t flash_cache_search(uint16_t emulated_pc)
{	
    lookup_pc_jump_flag(emulated_pc);
    bankswitch_prg(pc_jump_flag_bank);
    uint8_t test = (flash_cache_pc_flags[pc_jump_flag_address] & RECOMPILED);
    if (test)  // D7 clear if RECOMPILED
        return 0; // not found
        
    // run native code, return through flash_dispatch_return
    ...
    void (*code_ptr)(void) = (void*) flash_cache_pc[pc_jump_address];
    (*code_ptr)();
    //unreachable, returns through flash_dispatch_return
}
```

**Logic Flow**:
1. Read `flash_cache_pc_flags[pc_jump_flag_address]` 
2. Extract RECOMPILED bit (0x80)
3. If RECOMPILED bit is SET (1) → return 0 ("not found")
4. If RECOMPILED bit is CLEAR (0) → call compiled code

---

## The Discrepancy - CRITICAL BUG FOUND

### Assembly Behavior
- **RECOMPILED = 1** (bit 7 SET): Jump to `not_recompiled` → return 1 or 2
- **RECOMPILED = 0** (bit 7 CLEAR): Execute compiled code → return 0 (after PC advance)

### C Code Behavior (WRONG)
- **RECOMPILED = 1** (bit 7 SET): return 0 ("not found")
- **RECOMPILED = 0** (bit 7 CLEAR): execute compiled code → return 0

### Problem
The C code `flash_cache_search()` returns wrong values:
- When RECOMPILED = 1, it returns 0 (but assembly would return 1 or 2)
- When RECOMPILED = 0, it returns 0 (matches assembly)

**PLUS**: flash_cache_search() is **completely bypassed** - it's never called in the dispatch path anymore!

---

## Deeper Issue: flash_cache_pc_update()

```c
void flash_cache_pc_update(uint8_t code_address, uint8_t flags)
{		
    flash_byte_program((uint16_t) &flash_cache_pc[0] + pc_jump_address + 0, 
                       pc_jump_bank, 
                       (uint8_t) flash_code_address + code_address);
    flash_byte_program((uint16_t) &flash_cache_pc[0] + pc_jump_address + 1, 
                       pc_jump_bank, 
                       (uint8_t) ((flash_code_address + code_address) >> 8));			
    
    flash_byte_program((uint16_t) &flash_cache_pc_flags[0] + pc_jump_flag_address, 
                       pc_jump_flag_bank, 
                       flash_code_bank | ((~flags) & 0xE0));
}
```

**Key Line**: `flash_code_bank | ((~flags) & 0xE0)`

**Analysis**:
- Input `flags` is either RECOMPILED (0x80) or INTERPRETED (0x40)
- `~flags`:
  - If RECOMPILED (0x80): ~0x80 = 0x7F
  - If INTERPRETED (0x40): ~0x40 = 0xBF
- `(~flags) & 0xE0`:
  - If RECOMPILED (0x80): 0x7F & 0xE0 = 0x60 (clears bit 7!)
  - If INTERPRETED (0x40): 0xBF & 0xE0 = 0xA0 (sets bit 7!)

**Written to flash_cache_pc_flags**:
- `flash_code_bank | 0x60` = bank (3-bits) | 0x60
- `flash_code_bank | 0xA0` = bank (3-bits) | 0xA0

So:
- When RECOMPILED flag is passed: writes with bit 7 CLEAR (0) ✓ Correct
- When INTERPRETED flag is passed: writes with bit 7 SET (1) ✗ Wrong!

---

## Called With Which Flags? (run_6502)

```c
if (flash_enabled)
{
    setup_flash_address(pc_old, flash_cache_index);
    if (cache_flag[cache_index] & INTERPRET_NEXT_INSTRUCTION)
        flash_cache_pc_update(code_index_old, INTERPRETED);
    else if (code_index)
        flash_cache_pc_update(code_index_old, RECOMPILED);
}
```

**Case 1**: Code successfully compiled but next instruction needs interpretation
- Calls: `flash_cache_pc_update(code_index_old, INTERPRETED)`
- Writes: bit 7 SET (incorrect!)
- Assembly will see: RECOMPILED bit SET → go to `not_recompiled`
- **This is wrong** - code WAS recompiled, just next instruction needs interpretation

**Case 2**: Code successfully compiled
- Calls: `flash_cache_pc_update(code_index_old, RECOMPILED)`
- Writes: bit 7 CLEAR (correct!)
- Assembly will see: RECOMPILED bit CLEAR → execute compiled code
- **This is correct**

---

## Summary of Issues in Bug #2

### Issue 2a: flash_cache_pc_update() Flag Encoding
**Severity**: CRITICAL

When INTERPRETED flag is passed, the function writes RECOMPILED bit = 1 (SET), which tells the dispatcher "don't use this code". This is backwards.

**Semantic Problem**:
The INTERPRET_NEXT_INSTRUCTION is a C-level flag that marks "after this compiled block finishes, interpret one more instruction." It's checked in `ready()` after cache execution:

```c
if (cache_flag[cache_index] & INTERPRET_NEXT_INSTRUCTION)
{
    bankswitch_prg(0);
    interpret_6502();  // Interpret ONE instruction
}
```

However, when storing to flash, this gets encoded **incorrectly**:
- Should mean: "Code exists (bit 7=0) AND next instruction needs interpretation (bit 6=1)"
- Actually means: "Code doesn't exist (bit 7=1) AND next instruction needs interpretation (bit 6=1)"

Result: Assembly dispatcher sees bit 7=1, jumps to `not_recompiled`, returns 1 (recompile), and **never executes the existing code** that was already compiled.

**Fix Required**: 
- When storing INTERPRETED status, should still write RECOMPILED = 0 (to indicate code exists)
- Then separately encode INTERPRETED bit to mark "interpret next"
- Result: bit 7=0, bit 6=1 means "code ready, then interpret next instruction"

### Issue 2b: flash_cache_search() Return Logic
**Severity**: MEDIUM (currently not used)

The function's return values don't match the ASM logic:
- RECOMPILED=1 returns 0 (wrong)
- RECOMPILED=0 returns 0 (correct)

However, this function is currently bypassed by the new dispatch_on_pc dispatch, so it's not causing immediate problems.

### Issue 2c: Assembly dispatch_on_pc Logic
**Severity**: NONE (appears correct)

The assembly correctly interprets the flag:
- bmi (branch on minus) jumps if bit 7 SET
- Branches to `not_recompiled` handlers
- Otherwise executes compiled code

---

## Recommended Fix

### Priority 1: Fix flash_cache_pc_update() - FIXED ✅

**Key Constraint**: Flash memory can only program bits from 1→0, not 0→1

**Bit State Encoding** (bits 7, 6):
- **11**: Recompile needed (erased initial state)
- **01**: Execute compiled code (bit 7 cleared)
- **00**: Interpret override (bits 7,6 cleared)
- **10**: Interpret override (bit 6 cleared)

**Original Broken Code**:
```c
flash_code_bank | ((~flags) & 0xE0)
```
- RECOMPILED (0x80): `(~0x80) & 0xE0 = 0x60` → writes 01 ✓
- INTERPRETED (0x40): `(~0x40) & 0xE0 = 0xA0` → writes 10 ✗ (should be 00)

**Fixed Code**:
```c
uint8_t flag_byte;
if (flags == RECOMPILED)
    flag_byte = flash_code_bank | 0x40;  // Bits 7,6 = 01: execute
else  // INTERPRETED
    flag_byte = flash_code_bank;         // Bits 7,6 = 00: interpret
    
flash_byte_program((uint16_t) &flash_cache_pc_flags[0] + pc_jump_flag_address, 
                   pc_jump_flag_bank, 
                   flag_byte);
```

**Result**: 
- RECOMPILED flag: 01 → dispatcher executes compiled code
- INTERPRETED flag: 00 → dispatcher interprets ROM instruction
- Assembly dispatch_on_pc() correctly branches based on bit 7

### Priority 2: Review flash_cache_search()

Either:
- Fix the logic to match ASM behavior
- Or remove it entirely if dispatch_on_pc is the primary dispatch path

---

## Testing Points

After fix, verify:
1. ✓ Code that can be compiled executes (bits 7,6 = 01)
2. ✓ Code marked for interpretation is interpreted (bits 7,6 = 00 or 10)
3. ✓ Unclaimed flash entries trigger recompilation (bits 7,6 = 11)
4. ✓ Recompilation happens when needed
5. ✓ Interpretation happens when marked
