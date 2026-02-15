//#define DEBUG_AUDIO 1

//#define DEBUG_CPU_WRITE 1
//#define DEBUG_CPU_READ 1

//#define DEBUG_OUT 1

//#define TRACK_TICKS	//disable this to stop tracking run clock cycles

// Normal build (linking + optimizer enabled)
#define ENABLE_LINKING
//#define INTERPRETER_ONLY

// Master optimizer toggle - comment out to disable entire optimizer system
//#define ENABLE_OPTIMIZER   // DISABLED: v1 sector evacuation approach

// V2 optimizer - in-place branch patching (no sector evacuation)
#define ENABLE_OPTIMIZER_V2

// Patchable epilogue - block chaining via patchable epilogues (requires V2)
#ifdef ENABLE_OPTIMIZER_V2
#define ENABLE_PATCHABLE_EPILOGUE
#endif

// Cache persistence - skip flash_format() at boot if valid cache signature found
// Reuses previously compiled blocks, eliminating cold-start recompilation cost.
// Signature includes ROM hash to invalidate when game ROM changes.
//#define ENABLE_CACHE_PERSIST

// Native JSR mode - for stack-clean subroutines (no TSX/TXS), JSR calls a
// WRAM trampoline that dispatches subroutine blocks in a tight assembly loop
// until RTS, avoiding C round-trips for each block dispatch.
// Requires ENABLE_STATIC_ANALYSIS for the subroutine stack-safety table.
// Disable this if a game misbehaves (e.g. stack tricks the analysis missed).
#ifdef ENABLE_STATIC_ANALYSIS
#define ENABLE_NATIVE_JSR
#endif

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

#define GAME_SIDE_TRACK
//#define GAME_TARG
//#define GAME_TARG_TEST_ROM
//#define GAME_SPECTAR

// --- Static analysis pass ---
// Run a one-time BFS walk of the guest ROM at power-on to discover all
// reachable code, then batch-compile discovered entry points before
// starting execution.  Results are persisted in flash bank 3 so that
// subsequent resets benefit from runtime-discovered indirect-jump targets.
#define ENABLE_STATIC_ANALYSIS

// After the walk, compile every discovered entry point in address order.
// Gated separately so the walker can be tested without the compile pass.
#define ENABLE_STATIC_COMPILE

// --- Pointer swizzling (per-game) ---
// Replace Exidy high-byte immediates (LDA #$4x) with NES-translated values
// at compile time, eliminating the runtime decode_address_asm call from
// every subsequent (zp),Y use of the swizzled pointer.
#ifdef GAME_SIDE_TRACK
#define ENABLE_POINTER_SWIZZLE
//#define ENABLE_POINTER_READBACK_GUARD
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

