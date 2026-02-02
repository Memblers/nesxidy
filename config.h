//#define DEBUG_AUDIO 1

//#define DEBUG_CPU_WRITE 1
//#define DEBUG_CPU_READ 1

//#define DEBUG_OUT 1

//#define TRACK_TICKS	//disable this to stop tracking clock cycles

#define ENABLE_LINKING
//#define INTERPRETER_ONLY

// Master optimizer toggle - comment out to disable entire optimizer system
//#define ENABLE_OPTIMIZER   // DISABLED: Trying new approach

// Optimizer features
#define OPT_BLOCK_METADATA   0    // Store metadata after epilogue (required for copy-based optimization)
#define OPT_TRACK_CYCLES     0    // Track emulated cycles per block (requires OPT_BLOCK_METADATA)
#define OPT_COPY_BLOCKS      0    // Copy blocks instead of recompile during optimization

#define GAME_SIDE_TRACK
//#define GAME_TARG
//#define GAME_TARG_TEST_ROM
//#define GAME_SPECTAR

