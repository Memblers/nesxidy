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

// Native JSR mode - JSR calls a trampoline that runs subroutine blocks in a
// tight native loop until RTS, avoiding C round-trips for each block dispatch.
// When disabled, JSR uses standard 6502 emulation (push return addr, exit to C).
//#define ENABLE_NATIVE_JSR

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

