//#define DEBUG_AUDIO 1

//#define DEBUG_CPU_WRITE 1
//#define DEBUG_CPU_READ 1

//#define DEBUG_OUT 1

// Collect and report compilation metrics (BFS discovery, optimizer stats, cache behavior)
// Minimal overhead when not printing — metrics_*() calls are very fast
// Enable ENABLE_METRICS_DISPLAY to print stats to WRAM debug area every N frames
#define ENABLE_METRICS

// Display metrics on-screen (writes to WRAM $7E00 area every frame)
// Requires ENABLE_METRICS
//#define ENABLE_METRICS_DISPLAY

// Write JIT stats (hit/miss/branch counters, opt2 stats, block count)
// to WRAM $7E00 every frame.  Costs ~840 NES cycles/frame (~2.8%)
// due to 6 peek_bank_byte calls (12 bank switches) for opt2 stats.
// Disable to eliminate per-frame overhead when not debugging.
//#define ENABLE_DEBUG_STATS

// Block cycle counting — pre-compute 6502 cycle count per JIT block
// and add to clockticks6502 at dispatch time.  Provides accurate-enough
// frame timing without per-instruction overhead.  Cycle count is stored
// in block header byte +6 (uint8_t, max 255).  Over-counts when a
// branch exits the block early (acceptable approximation).
//#define ENABLE_BLOCK_CYCLES

// TRACK_TICKS: enable cycle-based frame timing.  Auto-enabled by
// ENABLE_BLOCK_CYCLES.  Can also be enabled standalone for interpreter.
#ifdef ENABLE_BLOCK_CYCLES
#define TRACK_TICKS
#else
//#define TRACK_TICKS	//disable this to stop tracking run clock cycles
#endif

// Normal build (linking + optimizer enabled)
#define ENABLE_LINKING
//#define INTERPRETER_ONLY

// Master optimizer toggle - comment out to disable entire optimizer system
//#define ENABLE_OPTIMIZER   // DISABLED: v1 sector evacuation approach

// V2 optimizer - in-place branch patching (no sector evacuation)
#define ENABLE_OPTIMIZER_V2

// --- IR optimisation layer ---
// Buffer JIT output as IR nodes in WRAM, run optimisation passes, then
// lower to native bytes before writing to flash.  When disabled, the
// compile path writes raw bytes directly to cache_code[] as before.
#define ENABLE_IR

// Individual IR optimisation passes (all require ENABLE_IR).
// Disable any pass independently to isolate regressions.
#ifdef ENABLE_IR
#define ENABLE_IR_OPT_REDUNDANT_LOAD   // shadow-track A/X/Y, kill dup loads, const-fold
#define ENABLE_IR_OPT_DEAD_STORE       // STA/STX/STY zp killed if overwritten before read
#define ENABLE_IR_OPT_DEAD_LOAD        // LDR zp killed if register+flags overwritten before read
#define ENABLE_IR_OPT_PHP_PLP          // generalised PLP→PHP elision (superset of old peephole)
#define ENABLE_IR_OPT_PAIR_REWRITE     // 27 pair rewrite rules from peephole_patterns.txt
#endif

// Skip IR optimisation passes during dynamic (JIT) compilation.
// The dynamic code cache is thrown away before the static analysis
// two-pass compile, which re-runs the full IR pipeline from scratch.
// Skipping optimisation during the dynamic pass saves ~78 M NES cycles
// (~9% of boot) at the cost of slightly larger JIT code in the cache.
// The static pass is unaffected — it always optimises.
#define SKIP_DYNAMIC_IR_OPT

// Peephole PLP/PHP elimination - elide redundant PLP/PHP pairs between
// consecutive PHA/PLA templates.  Saves 2 bytes + 2 cycles per pair.
// NOTE: When ENABLE_IR + ENABLE_IR_OPT_PHP_PLP are active, this legacy
// pass is superseded — the IR pass handles it in a more general way.
// Disabled: IR_OPT_PHP_PLP covers both dynamic and static paths.
// The peephole adds ~4 M cycles of inline overhead for results that
// are either redundant (static pass re-optimises) or discarded
// (dynamic cache erased before static compile).
//#define ENABLE_PEEPHOLE
// Sub-option: defer trailing PLP (trim).  Without this, the peephole
// codepath is compiled but never activates — tests for vbcc miscompilation.
//#define ENABLE_PEEPHOLE_TRIM
// Sub-option: skip leading PHP when flags already saved (full optimisation).
// Requires TRIM.  Without this, trim defers PLP but every PHP is still
// emitted — tests the defer/flush machinery in isolation.
//#define ENABLE_PEEPHOLE_SKIP

// Patchable epilogue - block chaining via patchable epilogues (requires V2)
#ifdef ENABLE_OPTIMIZER_V2
#define ENABLE_PATCHABLE_EPILOGUE
#endif

// $FFF0 patchable templates — use the ROM trampoline at $FFF0 for
// patchable branch/JMP templates instead of the old 21-byte/9-byte
// inline patterns.  Saves 4-6 bytes per template and eliminates the
// branch-byte flash write (2 writes instead of 3).  The trade-off is
// ~13 extra cycles on the fast path for branches (pre-set _pc overhead)
// but patching is cheaper (saves ~200 NES cycles per SST39SF040 write).
// Requires ENABLE_OPTIMIZER_V2 and ENABLE_PATCHABLE_EPILOGUE.
#ifdef ENABLE_PATCHABLE_EPILOGUE
#define ENABLE_FFF0_TEMPLATES
#endif

// FFF0 trampoline addresses (ROM bank 31 fixed bank, always mapped)
#ifdef ENABLE_FFF0_TEMPLATES
#define FFF0_DISPATCH       0xFFF0  /* entry: STA _a / PHP / JMP dispatch */
#define FFF0_DISPATCH_A_SAVED 0xFFF6  /* entry: PHP / JMP dispatch (_a already saved) */
#endif

// Cache persistence - skip flash_format() at boot if valid cache signature found
// Reuses previously compiled blocks, eliminating cold-start recompilation cost.
// Signature includes ROM hash to invalidate when game ROM changes.
//#define ENABLE_CACHE_PERSIST

// Idle loop detection — detect tight backward-branch loops (busy-waits,
// delay loops, random seed incrementers) and bypass the dispatch overhead.
// When a guest PC is seen as a backward-branch target IDLE_DETECT_THRESHOLD
// times, subsequent visits interpret a full loop iteration directly instead
// of going through flash dispatch (saving bank-switch + lookup each time).
#define ENABLE_IDLE_DETECT
#define IDLE_DETECT_THRESHOLD  8   // backward branches to same PC before activating

// Automatic idle-loop detection via static analysis.
// After the BFS ROM walk, scan for short backward-branch loops whose bodies
// consist solely of loads/compares/branches with no side effects — the
// classic "poll RAM/IO and wait for interrupt" pattern.  Detected idle PCs
// are stored in a small flash table (sa_idle_list) and checked at dispatch
// time, replacing (or supplementing) the manual GAME_IDLE_PC define.
// Requires ENABLE_STATIC_ANALYSIS.
#ifdef ENABLE_STATIC_ANALYSIS
#define ENABLE_AUTO_IDLE_DETECT
#define SA_IDLE_MAX  8    // max auto-detected idle PCs per game
#endif

// Native JSR mode - for stack-clean subroutines (no TSX/TXS, no unbalanced
// PLA/PLP), JSR calls a WRAM trampoline that dispatches subroutine blocks
// in a tight assembly loop until RTS, avoiding C round-trips for each block
// dispatch.  Requires ENABLE_STATIC_ANALYSIS for the subroutine table.
#ifdef ENABLE_STATIC_ANALYSIS
#define ENABLE_NATIVE_JSR
#endif

// Native stack mode — use the real NES hardware stack page ($0100-$01FF)
// for emulated 6502 stack operations.  The stack page is split:
//   Guest: $0100-$017F (SP starts at $7F, grows down) — PHA/PLA/PHP/PLP/JSR/RTS
//   Host:  $0180-$01FF (SP starts at $FF, grows down) — dispatch, C runtime
// When active, PHA/PLA/PHP/PLP compile to single native instructions (1 byte
// each instead of 9-14), and JSR/RTS for stack-clean subroutines compile to
// native JSR abs / RTS (3/1 bytes instead of 34/32).  TSX/TXS compile to
// native TSX/TXS since the real SP *is* the guest SP during block execution.
//
// When ENABLE_NATIVE_STACK is active, ENABLE_NATIVE_JSR's trampoline is
// superseded — stack-clean JSR/RTS use real hardware JSR/RTS instead.
// Stack-dirty or cross-bank calls fall back to the emulated JSR template.
//
// Opt-in: only safe for games with "generic" stack usage — no direct reads
// of $01xx addresses expecting specific layout, no JSR-into-middle tricks.
// Requires ENABLE_STATIC_ANALYSIS.
//#define ENABLE_NATIVE_STACK

// Per-game subroutine blacklist — force specific addresses to SA_SUB_DIRTY
// even if the static analysis classifies them as clean.  Safety valve for
// edge cases the heuristic misses.  List of 16-bit addresses, 0-terminated.
// Example: #define FORCE_DIRTY_SUBS  0xF32D, 0xE107, 0
//#define FORCE_DIRTY_SUBS  0

// Zero page index wrapping - when enabled, zpx/zpy instructions are interpreted
// to preserve correct 6502 behavior where (zp_addr + index) wraps within $00-$FF.
// When disabled (default), zpx/zpy are compiled as absolute indexed (absx/absy),
// which is faster but won't wrap at the zero page boundary.
// Most games don't rely on ZP index wrapping, so leaving this off is usually safe.
//#define ENABLE_ZP_INDEX_WRAP

// Optimizer features
#define OPT_BLOCK_METADATA   0    // Store metadata after epilogue (required for copy-based optimization)
#define OPT_TRACK_CYCLES     0    // Track emulated cycles per block (requires OPT_BLOCK_METADATA)
#define OPT_COPY_BLOCKS      0    // Copy blocks instead of recompile during optimization

// Game selection — only ONE game should be active.
// Exidy games: define here.  NES games: pass -DGAME_DONKEY_KONG on command line.
#ifndef PLATFORM_NES
#define GAME_SIDE_TRACK
//#define GAME_TARG
//#define GAME_TARG_TEST_ROM
//#define GAME_SPECTAR
//#define GAME_CPU_6502_TEST
#endif

// --- Static analysis pass ---
// Run a one-time BFS walk of the guest ROM at power-on to discover all
// reachable code, then batch-compile discovered entry points before
// starting execution.  Results are persisted in flash bank 3 so that
// subsequent resets benefit from runtime-discovered indirect-jump targets.
#define ENABLE_STATIC_ANALYSIS

// After the walk, compile every discovered entry point in address order.
// Gated separately so the walker can be tested without the compile pass.
#define ENABLE_STATIC_COMPILE

// Visual PPU effect during static analysis: monochrome + toggling
// emphasis bits during BFS walk and batch compile.  Writes the lazynes
// lnPPUMASK shadow so the effect takes effect from the top of screen.
#define ENABLE_COMPILE_PPU_EFFECT

// --- Native ZP pointer mirroring (per-game) ---
// Mirrors guest ZP pointer pairs into NES zero-page slots so that
// STA (zp),Y can use the native 6502 indirect-indexed instruction
// instead of the runtime address-decode handler.
//
// Mirror writes always store raw Exidy-space values (2-byte opcode).
// Native STA (zp),Y reads the pointer from emulated RAM and
// translates the hi byte at runtime via address_decoding_table —
// correct for any Exidy address (screen, RAM, ROM, etc.).
//
// Each entry: { guest_lo, guest_hi, nes_zp, side_effect }
//   guest_lo/hi: guest ZP addresses of the 16-bit pointer
//   nes_zp:      NES ZP slot for the lo mirror (hi = nes_zp+1)
//                Filled from linker-assigned addresses of zp_mirror_N.
//   side_effect: 0=none, 1=inc screen_ram_updated, 2=inc character_ram_updated

#ifdef GAME_SIDE_TRACK
#define ENABLE_POINTER_SWIZZLE
//#define ENABLE_POINTER_READBACK_GUARD

#define ZP_MIRROR_COUNT 3
#define ZP_MIRROR_TABLE \
    { 0x00, 0x01, 0, 1 }, /* screen draw pointer     */ \
    { 0x25, 0x26, 0, 1 }, /* track data pointer (ROM) */ \
    { 0x69, 0x6A, 0, 1 }, /* player car pointer       */

#endif

// Valid ROM address range for the static walker (game-dependent).
// The walker will not follow control flow outside this range.
#ifdef GAME_SIDE_TRACK
#define ROM_ADDR_MIN  0x2800
#define ROM_ADDR_MAX  0x3FFF
#endif
#ifdef GAME_TARG
#define ROM_ADDR_MIN  0x1800
#define ROM_ADDR_MAX  0x3FFF
#endif
#ifdef GAME_TARG_TEST_ROM
#define ROM_ADDR_MIN  0x1800
#define ROM_ADDR_MAX  0x3FFF
#endif
#ifdef GAME_SPECTAR
#define ROM_ADDR_MIN  0x1000
#define ROM_ADDR_MAX  0x3FFF
#endif
#ifdef GAME_CPU_6502_TEST
#define ROM_ADDR_MIN  0x2800
#define ROM_ADDR_MAX  0x3FFF
#endif

// --- NES games ---
// All NROM-128: 16KB PRG mirrored at $8000/$C000, 8KB CHR.
// Pass -DGAME_xxx on the compiler command line (w_nes.bat handles this).
// Common NES settings applied at the bottom of this block.

#ifdef GAME_DONKEY_KONG
// GameLoop_CFE1:  JSR RNG_F4ED / JMP GameLoop_CFE1
// Main thread spins through RNG; ALL game logic runs inside NMI handler.
#define GAME_IDLE_PC  0xCFE1
#endif

#ifdef GAME_BALLOON_FIGHT
// No known idle PC.
#endif

#ifdef GAME_BASEBALL
// No known idle PC.
#endif

#ifdef GAME_BATTLE_CITY
// No known idle PC — main loop processes input every frame.
#endif

#ifdef GAME_BINARY_LAND
// No known idle PC.
#endif

#ifdef GAME_BURGER_TIME
// No known idle PC.
#endif

#ifdef GAME_DEFENDER
// No known idle PC.
#endif

#ifdef GAME_EXCITEBIKE
// No known idle PC.
#endif

#ifdef GAME_EXERION
// No known idle PC.
#endif

#ifdef GAME_GALAXIAN
// No known idle PC.
#endif

#ifdef GAME_HYPER_SPORTS
// No known idle PC.
#endif

#ifdef GAME_LODE_RUNNER
// No known idle PC.
#endif

#ifdef GAME_LUNAR_POOL
// Lunar Pool's NMI handler does PPU work (OAM DMA $CB25, nametable
// flush $CC8F, scroll restore $CA52), sets $01C8=1, then calls the
// main game loop (JSR $C003).  $C003 spins on $01C8 for N frames;
// on real hardware each spin is broken by the next reentrant NMI
// which re-runs the PPU flush and sets the flag.  The $68-bit-1
// guard prevents recursive game logic — inner $C003 returns at once.
//
// In DynaMoS the guest runs much slower so NMIs must be selective:
// fire reentrant nmi6502() ONLY when the guest has cleared the flag
// and entered a spin loop (safe to re-enter), not while the NMI
// handler's own PPU work is in progress.  This avoids corrupting
// PPU state while still giving the guest the reentrant NMI it needs.
#define NES_NMI_VBLANK_FLAG  0x01C8
// Lunar Pool is NROM with horizontal mirroring.  DynaMoS hardware
// has four-screen VRAM — emulate horizontal mirroring by duplicating
// nametable writes: NT0 ↔ NT1, NT2 ↔ NT3 (XOR $0400).
#define NES_MIRROR_HORIZONTAL
#endif

#ifdef GAME_M82
#define GAME_IDLE_PC  0xC34A
// No known idle PC.
#endif

#ifdef GAME_ZIPPY_RACE
// No known idle PC.
#endif

#ifdef GAME_DOOR
// No known idle PC.
#endif

#ifdef GAME_GOLF
// No known idle PC.
#define NES_MIRROR_VERTICAL
#endif

#ifdef GAME_KARATEKA
// No known idle PC.
#endif

#ifdef GAME_MARIO_BROS
// No known idle PC.
#endif

#ifdef GAME_MILLIPEDE
#define GAME_IDLE_PC  0xF09B
// No known idle PC.
#endif

#ifdef GAME_POOYAN
// No known idle PC.
#endif

#ifdef GAME_POPEYE
// No known idle PC.
#endif

#ifdef GAME_RAID_BUNG
// No known idle PC.
// Original ROM uses vertical mirroring, but DynaMoS hardware has
// four-screen VRAM.  Emulate vertical mirroring in software by
// duplicating nametable writes: NT0 ↔ NT2, NT1 ↔ NT3.
#define NES_MIRROR_VERTICAL
#endif

#ifdef GAME_ROAD_FIGHTER
// No known idle PC.
#endif

#ifdef GAME_SKY_DESTROYER
#define GAME_IDLE_PC  0xC5E0
#endif

#ifdef GAME_URBAN_CHAMP
// No known idle PC.
#endif

#ifdef GAME_WARPMAN
// No known idle PC.
//#define GAME_IDLE_PC  0xD050
#endif

#ifdef GAME_YIEAR
// No known idle PC.
#endif

// Common NES config: any GAME_xxx above implies PLATFORM_NES
#ifdef PLATFORM_NES
#ifndef ROM_ADDR_MIN
#define ROM_ADDR_MIN  0xC000
#define ROM_ADDR_MAX  0xFFFF
#endif
// Batch-dispatch: loop inside run_6502() until VBlank, avoiding
// vbcc __rsave12/__rload12 overhead on every single dispatch.
#ifndef ENABLE_BATCH_DISPATCH
#define ENABLE_BATCH_DISPATCH
#endif
#endif

