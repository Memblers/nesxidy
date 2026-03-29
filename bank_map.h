/**
 * bank_map.h - Centralized bank assignment map
 *
 * All flash bank numbers in one place.  Every C file and assembly file
 * should reference these constants instead of hard-coding bank numbers.
 *
 * Mapper 30 (UNROM-512): 32 × 16 KB PRG banks.
 *   Banks 0-30 are switchable, mapped at $8000-$BFFF.
 *   Bank 31 is fixed at $C000-$FFFF.
 *
 * PC lookup table banks are determined by the emulated address:
 *   Jump table bank  = (pc >> 13) + BANK_PCTABLE_START   (2 bytes/addr)
 *   Flag table bank  = (pc >> 14) + BANK_PCFLAGS_START   (1 byte/addr)
 *
 * "Dead" PC-table banks cover emulated address ranges that are unused
 * by a given platform.  These banks are repurposed for code and data.
 *
 * Layout:
 *   Bank  0       Interpreter + data init image (copied to WRAM at boot)
 *   Bank  1       Emit helpers / optimizer V2 (BANK_EMIT)
 *   Bank  2       Recompiler / compile-time code (BANK_RECOMPILE)
 *   Bank  3       Metadata: block flags, SA bitmap, cache bits (BANK_META)
 *   Banks 4-16    Code cache (BANK_CACHE_START .. +12)
 *   Bank 17       Compile-time banked code (BANK_COMPILE)
 *   Bank 18       Entry list for two-pass compile (BANK_ENTRY_LIST)
 *   Banks 19-26   PC jump table (BANK_PCTABLE_START .. +7)
 *   Banks 27-30   PC flag table (BANK_PCFLAGS_START .. +3)
 *   Bank 31       Fixed bank (text/rodata, always mapped at $C000)
 *
 * Repurposed dead banks (platform-dependent):
 *   Bank 19       NES-dead ($0000-$1FFF = RAM) → SA code + init code (NES)
 *   Bank 20       NES-dead ($2000-$3FFF = PPU/IO, no code) → NES PRG-ROM lo
 *   Bank 21       NES-dead ($4000-$5FFF = PPU/IO, no NES code)
 *                 Exidy-LIVE ($4000-$5FFF has guest code — do NOT use for Exidy)
 *   Bank 22       Exidy-dead ($6000-$7FFF = no Exidy code here)
 *                 NES-LIVE (PRG-RAM can have code) — do NOT use for NES
 *   Bank 23       Exidy-dead ($8000-$9FFF, Exidy ROM < $8000) → platform ROM
 *                 NES-LIVE ($8000-$9FFF = PRG-ROM mirror on NROM)
 *   Bank 24       Exidy-dead ($A000-$BFFF, Exidy ROM < $8000) → SA code
 *                 NES-LIVE ($A000-$BFFF = PRG-ROM mirror on NROM)
 *   Bank 25       Exidy-dead ($C000-$DFFF, Exidy ROM < $8000) → init code
 *                 NES-LIVE ($C000-$DFFF = PRG-ROM)
 *   Bank 26       NES-LIVE ($E000-$FFFF = PRG-ROM), Exidy PC-table LIVE
 *                 WARNING: do NOT repurpose — it's BANK_PCTABLE_START+7
 *   Bank 28       NES-dead flag table ($4000-$7FFF) → IR optimizer (NES)
 *                 Exidy-LIVE ($4000-$7FFF has guest code)
 *   Bank 29       Exidy-dead flag table ($8000-$BFFF) → IR optimizer (Exidy)
 *                 NES-LIVE ($8000-$BFFF = PRG-ROM)
 */

#ifndef BANK_MAP_H
#define BANK_MAP_H

/* ---- Core system banks ---- */
#define BANK_INTERP             0       /* Interpreter, boot data image */
#define BANK_EMIT               1       /* Emit helpers, optimizer V2 */
#ifdef PLATFORM_NES
#define BANK_IR_OPT             28      /* NES: dead flag-table bank $4000-$7FFF (no NES guest code there) */
#elif defined(PLATFORM_MILLIPEDE)
#define BANK_IR_OPT             29      /* Millipede: dead flag-table bank $8000-$BFFF (ROM mirror, not compiled) */
#elif defined(PLATFORM_ASTEROIDS)
#define BANK_IR_OPT             29      /* Asteroids: dead flag-table bank $8000-$BFFF (ROM ends at $7FFF) */
#else
#define BANK_IR_OPT             29      /* Exidy: dead flag-table bank $8000-$BFFF (no Exidy guest code there) */
#endif
                                        /* WARNING: bank 26 = BANK_PCTABLE_START+7, LIVE on Exidy */
                                        /* WARNING: bank 28 = BANK_PCFLAGS_START+1, LIVE on Exidy ($4000-$7FFF) */
#define BANK_RECOMPILE          2       /* Recompiler (recompile_opcode_b2) */
#define BANK_META               3       /* Block flags, SA bitmap, cache bits */
#define BANK_CACHE_START        4       /* First code-cache bank */
#define BANK_COMPILE            17      /* Compile-time banked code (not runtime) */
#define BANK_ENTRY_LIST         18      /* Two-pass entry list */
/*
 * BANK_SA_CODE / BANK_INIT_CODE: banked code that doesn't run at dispatch
 * time (SA runs at boot/recompile, init runs once at startup).
 *
 * Exidy: banks 24-25 are dead ($A000-$DFFF — Exidy ROM is < $8000).
 * NES:   banks 24-25 are LIVE (PRG-ROM at $8000-$FFFF).  Use bank 19
 *        ($0000-$1FFF = RAM area, NES never executes code from RAM).
 */
#ifdef PLATFORM_NES
#define BANK_SA_CODE            19      /* NES: $0000-$1FFF = RAM, no code here */
#define BANK_INIT_CODE          19      /* NES: shares with SA (init runs once at boot) */
#elif defined(PLATFORM_MILLIPEDE)
#define BANK_SA_CODE            19      /* Millipede: $0000-$1FFF = RAM/IO, no code here */
#define BANK_INIT_CODE          19      /* Millipede: shares with SA */
#elif defined(PLATFORM_ASTEROIDS)
#define BANK_SA_CODE            19      /* Asteroids: $0000-$1FFF = RAM/IO, no code here */
#define BANK_INIT_CODE          19      /* Asteroids: shares with SA */
#else
#define BANK_SA_CODE            24      /* Exidy: $A000-$BFFF dead */
#define BANK_INIT_CODE          25      /* Exidy: $C000-$DFFF dead */
#endif

/* ---- PC lookup tables ---- */
#define BANK_PCTABLE_START      19      /* Jump table: bank = (pc>>13)+19 */
#define BANK_PCFLAGS_START      27      /* Flag table: bank = (pc>>14)+27 */

/* ---- Repurposed dead PC-table banks ---- */

/*
 * BANK_RENDER: render_video, convert_chr, convert_sprites, metrics dump,
 *              platform init code, nes_gamepad — anything once-per-frame
 *              or init-only that was previously crammed into bank 2.
 *
 * Exidy: bank 22 ($6000-$7FFF range is dead — Exidy has no code there)
 * NES:   bank 21 ($4000-$5FFF range is dead — PPU/IO region)
 *
 * The build always picks exactly one platform, so no runtime conflict.
 */
#ifdef PLATFORM_NES
#define BANK_RENDER             21
#elif defined(PLATFORM_MILLIPEDE)
#define BANK_RENDER             20      /* Millipede: $2000-$3FFF = I/O, dead for code */
#elif defined(PLATFORM_ASTEROIDS)
#define BANK_RENDER             22      /* Asteroids: $6000-$7FFF range, ROM is at $6800+ */
#else
#define BANK_RENDER             22
#endif

/*
 * BANK_METRICS: metrics dump functions (metrics_dump_sa_b2,
 *               metrics_dump_runtime_b2).
 *   NES:       bank 21, same as BANK_RENDER ($4000-$5FFF dead)
 *   Exidy:     bank 21 ($4000-$5FFF PC table dead for Exidy)
 *   Millipede: bank 20, same as BANK_RENDER ($2000-$3FFF dead)
 *              Bank 21 is LIVE for Millipede ($4000-$5FFF = ROM code)
 */
#ifdef PLATFORM_MILLIPEDE
#define BANK_METRICS            20      /* shares with BANK_RENDER */
#elif defined(PLATFORM_ASTEROIDS)
#define BANK_METRICS            21      /* Asteroids: $4000-$5FFF = I/O, dead for guest code */
#else
#define BANK_METRICS            21
#endif

/*
 * BANK_PLATFORM_ROM: platform-specific guest ROM data (.incbin files).
 * Exidy ROMs are small enough to fit in one 16 KB bank.
 *
 * Exidy: bank 23 ($8000-$9FFF PC range is dead for Exidy)
 *        Exidy ROM range is $1000-$3FFF, never reaches $8000.
 */
#define BANK_PLATFORM_ROM       23

/*
 * NES PRG-ROM banks (32 KB NROM = two 16 KB halves).
 * Bank 20: $2000-$3FFF PC range is dead on NES (PPU registers).
 * Bank 21: $4000-$5FFF PC range is dead on both platforms.
 *
 * For a 32 KB NROM image:
 *   PRG lo ($8000-$BFFF) → bank 20
 *   PRG hi ($C000-$FFFF) → bank 21
 * For an 8 KB CHR-ROM:
 *   CHR data             → bank 23 (shares with Exidy ROM slot)
 */
#ifdef PLATFORM_NES
#define BANK_NES_PRG_LO        20
#define BANK_NES_PRG_HI        21      /* same as BANK_RENDER for NES */
#define BANK_NES_CHR           23
#endif

/*
 * Millipede arcade ROM banks:
 *   BANK_PLATFORM_ROM (23): 16KB program ROM (fills the whole bank)
 *   BANK_MILLIPEDE_CHR (24): character ROM (4KB) + color PROM (256B)
 *   Bank 24 = PC $A000-$BFFF, dead for Millipede (ROM mirror, not compiled)
 */
#ifdef PLATFORM_MILLIPEDE
#define BANK_MILLIPEDE_CHR     24
#endif

/*
 * Asteroids arcade ROM banks:
 *   BANK_PLATFORM_ROM (23): 6KB program ROM + 2KB vector ROM
 *   Program ROM ($6800-$7FFF) + Vector ROM ($4800-$4FFF)
 */

/* ---- Aliases for existing code ---- */
#define BANK_FLASH_BLOCK_FLAGS  BANK_META
#define BANK_CODE               BANK_CACHE_START
#define BANK_PC                 BANK_PCTABLE_START
#define BANK_PC_FLAGS           BANK_PCFLAGS_START

#endif /* BANK_MAP_H */
