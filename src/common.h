//
// This header is included by default in all source files.
//
#ifndef COMMON_H
#define COMMON_H

#ifdef MSVC
#pragma runtime_checks("scu", off)
#endif

#define DECOMP_CUSTOM 1 // Logic that is added to aid decompilation, etc
#define DEBUG_BUILD 1 // Logic that appears only in debug builds

extern const char *build_rev;
extern const char *build_date;
extern const char *build_ui_widget_text;

float __cdecl sinf(float);
float __cdecl cosf(float);
float __cdecl sqrtf(float);
float __cdecl fabsf(float);
double __cdecl sin(double);
double __cdecl cos(double);
double __cdecl sqrt(double);
double __cdecl fabs(double);

#include "types.h"

#ifndef _pi
#define _pi ((real)3.14159265358979323846)
#define DEGREES_TO_RADIANS(angle) ((real)(((real)(angle)) * _pi / 180.f))
#define RADIANS_TO_DEGREES(angle) ((real)(((real)(angle)) * 180.f / _pi))
#endif
#define XBOX_REPLACE_STANDARD_NAMES
#include "inlines.h"
#include "decl.h"

/* assert_halt_at(file, line, cond) — byte-match-faithful assert.
 * assert_halt stamps OUR __FILE__/__LINE__, so the emitted .rdata path string
 * and the `push <line>` immediate never match the original binary (an
 * [IMM-WARN] on every assert site, plus LCS churn from the wrong string ref).
 * assert_halt_at takes the ORIGINAL Bungie source path and assert line recovered
 * from the XBE, reproducing the exact string and line immediate. The message is
 * #cond, so a condition written with real names also reproduces the original
 * expression string. Rewrite implicit sites from the XBE with
 * tools/audit/recover_assert_sites.py --apply. Readable-lift Phase 0. */
#define assert_halt_at(file, line, cond)                       \
    do {                                                     \
        if (!(cond)) {                                       \
            display_assert(#cond, file, line, true);         \
            system_exit(-1);                                 \
        }                                                    \
    } while (0)

#define assert_halt(cond)                                    \
    do {                                                     \
        if (!(cond)) {                                       \
            display_assert(#cond, __FILE__, __LINE__, true); \
            system_exit(-1);                                 \
        }                                                    \
    } while (0)

/* assert_halt_msg_at(msg, file, line, cond) — assert_halt_at for the case where
 * the original .rdata assert text cannot be produced by stringizing our C
 * condition. The usual cause is spacing: Bungie wrote `a==b`, and clang-format
 * rewrites that to `a == b` inside a macro argument, silently changing the
 * emitted string literal. `msg` is the literal recovered from the XBE; `cond`
 * is the recovered C condition. Keep the two in sync by hand. */
#define assert_halt_msg_at(msg, file, line, cond)            \
    do {                                                     \
        if (!(cond)) {                                       \
            display_assert(msg, file, line, true);           \
            system_exit(-1);                                 \
        }                                                    \
    } while (0)

#define assert_halt_msg(cond, msg)                         \
    do {                                                   \
        if (!(cond)) {                                     \
            display_assert(msg, __FILE__, __LINE__, true); \
            system_exit(-1);                               \
        }                                                  \
    } while (0)

#define CLAMP(x, low, high) \
  ((x) < (low) ? (low) : ((x) > (high) ? (high) : (x)))

#define MAXIMUM_GAMEPADS 4
#define NUMBER_OF_GAMEPAD_BUTTONS 0x10
#define MAXIMUM_NUMBER_OF_LOCAL_PLAYERS 4
#define MAXIMUM_STRING_SIZE            0x2000
#define MAXIMUM_MEMSET_SIZE            0x10000000
#define MAXIMUM_MEMCPY_MEMMOVE_SIZE    0x10000000
#define PAGE_READWRITE                 0x04
#define TICKS_PER_SECOND               (*(float *)0x253394) /* 30.0f */

/* Player powerup slots.  The BOUND is proven by our own binary: the assert at
 * players.c line 0xaea reads
 * "powerup_type>=0 && powerup_type<NUMBER_OF_PLAYER_POWERUPS" against a
 * `cmp 2`, and player_update_powerups walks exactly two int16_t entries
 * from player+0x68.
 *
 * The member SPELLINGS are borrowed from halocea's player_powerup.h
 * (name_source: halocea, T2 — DB-verified there via types_enum_values
 * _7631F5BC00EB2EC7672D4CE153F5F765, but the identifiers themselves appear
 * nowhere in 2276).  What 2276 proves independently is the slot MAPPING:
 * hud.c formats "ACTIVE-CAMOUFLAGE " from the int16_t at player+0x68 and
 * "FULL-SPECTRUM VISION " from the one at player+0x6a, so slot 0 is active
 * camouflage and slot 1 is full-spectrum vision.  Corroborating a mapping via
 * display text is weaker evidence than an identifier in an assert string, so
 * these stay T2 rather than being promoted the way batch 3's
 * _name_filename_bit was.
 *
 * Defined here (and in the mirror force-included header) rather than at the
 * top of players.c: that TU has four `assert_halt` sites, and every line
 * inserted above one shifts its __LINE__ immediate. */
enum player_powerup {
  _player_powerup_active_camouflage = 0,
  _player_powerup_full_spectrum_vision = 1,
  NUMBER_OF_PLAYER_POWERUPS = 2
};

#include "nv097.h"

static const int _scenario_type_main_menu = 2;
#define REAL_NEG_5000_POOL (*(float *)0x266e98) /* -5000.0f */
#define REAL_5000_POOL (*(float *)0x266e94) /* 5000.0f */
#define REAL_3600_POOL (*(float *)0x266e90) /* 3600.0f */
#define REAL_0_001_POOL (*(float *)0x255ef8) /* 0.001f */
#define REAL_2_0_POOL (*(float *)0x253f40) /* 2.0f */
#define REAL_ZERO_POOL (*(float *)0x2533c0) /* 0.0f */
#define REAL_HALF_PI_POOL (*(float *)0x2568bc) /* 1.5707964f */
#define REAL_ONE_POOL (*(float *)0x2533c8) /* 1.0f */
#define REAL_NEGATIVE_ONE_POOL (*(float *)0x255e94) /* -1.0f */
#define REAL_0_5_POOL (*(float *)0x253398) /* 0.5f */
#define REAL_0_6_POOL (*(float *)0x253f3c) /* 0.6f */
#define REAL_0_4_POOL (*(float *)0x253524) /* 0.4f */
#define REAL_0_2_POOL (*(float *)0x2549d4) /* 0.2f */
#define REAL_0_1_POOL (*(float *)0x25496c) /* 0.1f */
#define REAL_NEG_0_1_POOL (*(float *)0x25e884) /* -0.1f */
#define REAL_0_05_POOL (*(float *)0x2533e8) /* 0.05f */
#define REAL_ONE_THIRTIETH_POOL (*(float *)0x2546a4) /* 0.033333335f */
#define REAL_ONE_OVER_24_POOL (*(float *)0x28af18) /* 0.041666668f */
#define REAL_ONE_NINETIETH_POOL (*(float *)0x26f2e0) /* 0.011111111f */
#define REAL_ONE_OVER_120_POOL (*(float *)0x28ac20) /* 0.008333334f */
#define REAL_ONE_OVER_128_POOL (*(float *)0x25f0d0) /* 0.0078125f */
#define REAL_ONE_OVER_255_POOL (*(float *)0x261518) /* 0.003921569f */
#define REAL_ONE_OVER_900_POOL (*(float *)0x25620c) /* 0.0011111111f */
#define REAL_QUARTER_PI_POOL (*(float *)0x254a58) /* 0.7853982f */
#define REAL_TWO_PI_POOL (*(float *)0x255a54) /* 6.2831855f */
#define REAL_MAX_POOL (*(float *)0x2548fc) /* 3.4028235e38f */
#define REAL_0_25_POOL (*(float *)0x25337c) /* 0.25f */
#define REAL_0_3_POOL (*(float *)0x2533e4) /* 0.3f */
#define REAL_0_8_POOL (*(float *)0x2533f0) /* 0.8f */
#define REAL_1_5_POOL (*(float *)0x2533ec) /* 1.5f */
#define REAL_3_0_POOL (*(float *)0x254644) /* 3.0f */
#define REAL_4_0_POOL (*(float *)0x2533d8) /* 4.0f */
#define REAL_0_0001_POOL (*(float *)0x253f44) /* 0.0001f */
#define REAL_PI_POOL (*(float *)0x256980) /* 3.1415927f */
#define REAL_NEGATIVE_PI_POOL (*(float *)0x26e280) /* -3.1415927f */
#define REAL_0_01_POOL (*(float *)0x255d1c) /* 0.01f */
#define REAL_0_01_25BB10_POOL (*(float *)0x25bb10) /* 0.01f */
#define REAL_30_POOL (*(float *)0x253394) /* 30.0f */
#define REAL_ONE_OVER_3600_POOL (*(float *)0x253f2c) /* 0.00027777778f */
#define REAL_6_0_POOL (*(float *)0x254640) /* 6.0f */
#define REAL_5_0_POOL (*(float *)0x254cc4) /* 5.0f */
#define REAL_TEN_THIRDS_POOL (*(float *)0x254e6c) /* 3.3333333f */
#define REAL_ONE_TWELFTH_POOL (*(float *)0x255960) /* 0.083333336f */
#define REAL_NEG_0_5_POOL (*(float *)0x255964) /* -0.5f */
#define REAL_NEG_ONE_THIRTIETH_POOL (*(float *)0x256348) /* -0.033333335f */
#define REAL_0_04_POOL (*(float *)0x25bb0c) /* 0.04f */
#define REAL_255_POOL (*(float *)0x2602c8) /* 255.0f */
#define REAL_ONE_SIXTH_POOL (*(float *)0x2647d4) /* 0.16666667f */
#define REAL_THREE_DEGREES_POOL (*(float *)0x26e2ec) /* 0.05235988f */
#define REAL_NEG_2000_POOL (*(float *)0x2b4d1c) /* -2000.0f */
#define REAL_1800_POOL (*(float *)0x2b4ee0) /* 1800.0f */
#define REAL_60_POOL (*(float *)0x2b4ee4) /* 60.0f */
#define REAL_FIVE_SEVENTHS_POOL (*(float *)0x2b4fbc) /* 0.71428573f */
#define REAL_1E_8_POOL (*(float *)0x2b5098) /* 1e-8f */
#define REAL_0_053333_POOL (*(float *)0x2b509c) /* 0.053333335f */
#define REAL_0_02_POOL (*(float *)0x2b6af4) /* 0.02f */
#define REAL_1_2_DEGREES_POOL (*(float *)0x2b7248) /* 0.020943951f */
#define REAL_NEG_1_2_DEGREES_POOL (*(float *)0x2b724c) /* -0.020943951f */
#define REAL_100_OVER_TWO_PI_POOL (*(float *)0x2b7250) /* 15.915494f */
#define REAL_12_DEGREES_POOL (*(float *)0x2b7254) /* 0.20943952f */
#define REAL_4_2441316_POOL (*(float *)0x2b7258) /* 4.2441316f */
#define REAL_20_OVER_17_POOL (*(float *)0x2b7d40) /* 1.1764706f */

/* Tag group four-character codes.  Each literal spells its own name in ASCII
 * (0x77656170 == 'weap'), so the code IS the evidence — nothing is inferred.
 * Defined in the two force-included headers (clang -include src/common.h,
 * VC71 /FI src/xdk_common.h) rather than in a .c: a definition added above an
 * assert_halt would shift __FILE__/__LINE__ and move .text. */
#define TAG_GROUP_ANTR  0x616e7472 /* 'antr' */
#define TAG_GROUP_BIPD  0x62697064 /* 'bipd' */
#define TAG_GROUP_BITM  0x6269746d /* 'bitm' */
#define TAG_GROUP_DECAL 0x64656361 /* 'deca' */
#define TAG_GROUP_ELEC 0x656c6563 /* 'elec' */
#define TAG_GROUP_EQIP  0x65716970 /* 'eqip' */
#define TAG_GROUP_FONT 0x666f6e74 /* 'font' */
#define TAG_GROUP_GLW  0x676c7721 /* 'glw!' */
#define TAG_GROUP_HUDG 0x68756467 /* 'hudg' */
#define TAG_GROUP_ITEM 0x6974656d /* 'item' */
#define TAG_GROUP_ITMC 0x69746d63 /* 'itmc' */
#define TAG_GROUP_JPT   0x6a707421 /* 'jpt!' */
#define TAG_GROUP_LIGH 0x6c696768 /* 'ligh' */
#define TAG_GROUP_MACH  0x6d616368 /* 'mach' */
#define TAG_GROUP_MODE  0x6d6f6465 /* 'mode' */
#define TAG_GROUP_OBJE  0x6f626a65 /* 'obje' */
#define TAG_GROUP_PHYS 0x70687973 /* 'phys' */
#define TAG_GROUP_SHDR 0x73686472 /* 'shdr' */
#define TAG_GROUP_SND  0x736e6421 /* 'snd!' */
#define TAG_GROUP_UDLG  0x75646c67 /* 'udlg' */
#define TAG_GROUP_UNIT 0x756e6974 /* 'unit' */
#define TAG_GROUP_VEHI 0x76656869 /* 'vehi' */
#define TAG_GROUP_WEAP 0x77656170 /* 'weap' */

/* The original source's FLAG(bit) macro, quoted verbatim by binary assert
 * strings ("server_connection->flags&FLAG(_connection_create_server_bit)"). */
#ifndef FLAG
#define FLAG(b) (1 << (b))
#endif

#ifdef DEBUG_BUILD
#undef strlen
#define strlen csstrlen
#endif

/* Screen-bounds rect at 0x50657c — four int16 in the engine's standard 2D
 * rect order {top, left, bottom, right}.  Names and spellings are taken
 * verbatim from src/halo/cutscene/cinematics.c:401-404, which recovered them;
 * the two entries below are the ones referenced outside that TU, so they live
 * here (common.h is force-included) rather than being re-coined per file.
 * Body must stay token-identical to the code it replaces — that is what makes
 * the substitution codegen-neutral. */
#define screen_bounds_top (*(int16_t *)0x50657c)
#define screen_bounds_left (*(int16_t *)0x50657e)
#define structure_decals_globals (*(structure_decals_globals_t **)0x4d8ec8)

/* network_server_manager.c global server instance (base 0x5a90e0) and two
 * standalone flags.  network_game_server_create() returns (void *)0x5a90e0
 * as its "server" pointer, so these are the same fields other functions in
 * that TU reach via a server/s parameter at the matching relative offset
 * (e.g. server+0 = connection, server+4 = state, server+6 = flags,
 * server+0x484 = all_loaded_time, server+0x4b9 = loading_flag). Bodies must
 * stay token-identical to the code they replace. */
#define network_game_server_memory_do_not_use_directly_in_use (*(char *)0x46eed4)
#define network_game_server_next_team (*(int *)0x46eed8)
#define network_game_server_connection (*(int *)0x5a90e0)
#define network_game_server_state (*(short *)0x5a90e4)
#define network_game_server_flags (*(short *)0x5a90e6)
#define network_game_server_difficulty (*(short *)0x5a91f8)
#define network_game_server_reset_counter (*(int *)0x5a9514)
#define network_game_server_loading_flag (*(char *)0x5a9599)
#define network_game_server_all_loaded_time (*(int *)0x5a9564)

/* hs boolean global "allow_out_of_sync" (hs globals table entry 0x2f36e4).
 * Kept in sync with src/xdk_common.h. */
#define allow_out_of_sync (*(char *)0x46e8b8)

#endif
