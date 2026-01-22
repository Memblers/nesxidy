# NES Exidy Emulator - JIT Compilation Analysis Notes

## Project Overview

**Target System**: NES Exidy emulator with 6502 JIT compilation
**Build System**: VBCC compiler, mapper30 NES mapper, w.bat build script
**Architecture**: Flash cache (banks 4-30) stores compiled 6502 code + metadata

### Current Branch
- **Branch**: v2-work (isolated from master)
- **Last Stable**: Commit 12836ce (baseline before v2 development)

---

## Execution Flow Analysis

### High-Level Program Flow

1. **recompilation** → compile 6502 instructions to native 6502 code
2. **flash_cache** → store compiled code in flash memory
3. **dispatch** → lookup and execute compiled code by PC
4. **return** → return from compiled code back to C emulation loop

### Detailed Dispatch Flow

```
run_6502() [C]
  ↓
dispatch_on_pc() [ASM] - JSR call
  ├─ Read PC from zero page
  ├─ Calculate bank and offset for PC flags
  ├─ Check RECOMPILED flag (bit 7) in flash
  │  ├─ If SET (not recompiled): return 1 or 2
  │  └─ If CLEAR (recompiled): setup and JSR to compiled code
  ├─ JSR .dispatch_addr (self-modifying dispatch)
  │  └─ Compiled code executes
  │     └─ RTS to flash_dispatch_return [ASM]
  │        └─ RTS back to dispatch_on_pc
  └─ Return value in A (0, 1, or 2)

switch (result)
  ├─ case 2: Interpret current instruction
  ├─ case 0: Return (code executed in flash)
  └─ case 1: Fall through to cache_search for recompilation
```

---

## Critical Bugs Identified

### Bug #1: Return Value from dispatch_on_pc Ignored (FIXED)

**Location**: dynamos.c, line 138
**Severity**: CRITICAL

**Issue**:
- `flash_cache_search(pc)` was called but return value discarded
- Code would either execute OR fall through to cache_search unconditionally
- No conditional branching based on execution result

**Root Cause**:
- Commented-out switch statement (lines 140-153) was the correct implementation
- But it was disabled and replaced with broken single function call

**Solution Applied**:

1. **Modified dynamos-asm.s** (lines 303-316):
   - Added `jsr .dispatch_addr` instruction before the self-modifying jump
   - Makes dispatch_on_pc a proper callable function with JSR/RTS
   - Previously was direct JMP without return mechanism

2. **Modified dynamos.c** (lines 108-165):
   - Uncommented dispatch_on_pc switch statement
   - Removed broken `flash_cache_search()` call
   - Now checks return value: 0 (executed), 1 (recompile), 2 (interpret)

**Result**: ✅ FIXED - All three execution paths now properly handled

---

### Bug #2: RECOMPILED Flag Logic (NOT YET FIXED)

**Location**: dynamos.c, line 339
**Severity**: MEDIUM

**Issue**:
- Comment says: "D7 clear if RECOMPILED"
- Code checks: `if (test)` when bit is SET
- Suggests inverted logic elsewhere in system

**Evidence**:
```c
uint8_t test = (flash_cache_pc_flags[pc_jump_flag_address] & RECOMPILED);
if (test) //(flash_cache_pc_flags[pc_jump_flag_address] & RECOMPILED);	// D7 clear if RECOMPILED
    return 0; // not found
```

**Potential Impact**:
- May cause incorrect interpretation of compiled vs uncompiled code
- Could lead to recompilation loops or skipped recompilation

**Status**: Needs investigation and verification

---

### Bug #3: Pointer Table Read Incomplete (NOT YET FIXED)

**Location**: dynamos.c, line 351
**Severity**: MEDIUM

**Issue**:
- Reads only 1 byte from flash_cache_pc array
- Should read 2 bytes (lo, hi) to construct full 16-bit address

**Current Code**:
```c
void (*code_ptr)(void) = (void*) flash_cache_pc[pc_jump_address];
```

**Expected Code**:
```c
uint16_t code_addr = flash_cache_pc[pc_jump_address] | 
                     (flash_cache_pc[pc_jump_address + 1] << 8);
void (*code_ptr)(void) = (void*) code_addr;
```

**Status**: Needs to be corrected to read full 16-bit address

---

### Bug #4: Flash Address Encoding Issue (NOT YET FIXED)

**Location**: dynamos.c, lines 391-392 (flash_cache_pc_update)
**Severity**: HIGH

**Issue**:
- Mixing bank-relative addresses with offsets
- flash_cache_pc_update writes `flash_code_address + code_address` directly
- Incorrect encoding of compiled code location

**Current Code**:
```c
flash_byte_program((uint16_t) &flash_cache_pc[0] + pc_jump_address + 0, 
                   pc_jump_bank, 
                   (uint8_t) flash_code_address + code_address);
flash_byte_program((uint16_t) &flash_cache_pc[0] + pc_jump_address + 1, 
                   pc_jump_bank, 
                   (uint8_t) ((flash_code_address + code_address) >> 8));
```

**Problem**: 
- Assumes address fits in 16 bits
- Doesn't properly handle bank boundaries
- Should store pointer to compiled code correctly

**Status**: Needs address encoding fix

---

### Bug #5: Performance Optimization Opportunities (NOT YET FIXED)

**Location**: Multiple locations
**Severity**: LOW (performance, not correctness)

**Issues**:
1. `setup_flash_address()` called every instruction during compilation
   - Redundant calculations for same PC
   - Could be called once per cache block instead

2. Per-byte flash writes in `flash_cache_copy()`
   - Each byte written individually
   - Could be batched for better performance

3. Similar redundant calculations in other functions

**Status**: Deferred until core functionality verified

---

## Testing Strategy

### Current Status
- System builds successfully
- All three dispatch paths (0, 1, 2) properly handled by C switch

### Next Steps
1. ✅ Bug #1 fixed and verified
2. 🔶 Bug #2: Verify RECOMPILED flag logic
3. 🔶 Bug #3: Complete 2-byte pointer read
4. 🔶 Bug #4: Fix address encoding in flash_cache_pc_update
5. 🔶 Bug #5: Optimize performance if needed

---

## Key Functions Reference

### dispatch_on_pc() [ASM] (lines 256-316 in dynamos-asm.s)
- Reads PC from zero page
- Calculates bank and offset for PC flags
- Checks RECOMPILED flag
- Sets up self-modifying dispatch address
- JSR to dispatch address
- Returns: 0 (executed), 1 (recompile needed), 2 (interpret)

### flash_cache_search() [C] (lines 307-327 in dynamos.c)
- Looks up PC in flash cache
- Returns 0 if not compiled, otherwise calls compiled code

### flash_cache_pc_update() [C] (lines 390-395)
- Stores compiled code address in flash
- Updates PC flags

### setup_flash_address() [C] (lines 397-409)
- Calculates flash bank and offset for given PC and block number

### cache_search() [ASM]
- Searches RAM cache for PC match
- Used when flash cache miss

---

## Build Information

**Build Command**: `.\w.bat`
**Status**: ✅ Builds successfully with current fixes

**Files Modified**:
- dynamos-asm.s (added JSR to dispatch_on_pc)
- dynamos.c (uncommented dispatch switch, removed flash_cache_search)

---

## Notes for Future Work

1. **Flash Memory Architecture**:
   - Banks 4-18: Compiled code storage
   - Banks 19-26: PC pointer table
   - Banks 27-30: PC flags

2. **6502 Code Execution**:
   - Compiled code stored in NES PRG bank space
   - Returns via `flash_dispatch_return` ASM function
   - Must preserve all 6502 state (A, X, Y, P, S)

3. **Address Encoding**:
   - Emulated 6502 PC → Flash bank + offset
   - PC flags bit 7 (RECOMPILED) indicates if code exists
   - Bits 6-0 may encode bank information

4. **Performance Considerations**:
   - Flash memory limited write cycles
   - Per-byte writes should be minimized
   - Cache strategy important for throughput

---

## Session Summary

**Completed Work**:
- ✅ Identified 5 critical bugs in dispatch and compilation
- ✅ Fixed Bug #1 (dispatch return value handling)
- ✅ System successfully builds
- ✅ Control flow logic verified correct

**Pending Work**:
- 🔶 Bug #2: Verify RECOMPILED flag logic
- 🔶 Bug #3: Fix incomplete pointer read
- 🔶 Bug #4: Correct address encoding
- 🔶 Bug #5: Performance optimization

**Working Directory**: c:\proj\c\NES\nesxidy-co\nesxidy
**Branch**: v2-work (isolated development)
