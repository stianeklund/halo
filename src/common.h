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

/* Tag group four-character codes.  Each literal spells its own name in ASCII
 * (0x77656170 == 'weap'), so the code IS the evidence — nothing is inferred.
 * Defined in the two force-included headers (clang -include src/common.h,
 * VC71 /FI src/xdk_common.h) rather than in a .c: a definition added above an
 * assert_halt would shift __FILE__/__LINE__ and move .text. */
#define TAG_GROUP_DECAL 0x64656361 /* 'deca' */
#define TAG_GROUP_BITM  0x6269746d /* 'bitm' */
#define TAG_GROUP_ELEC 0x656c6563 /* 'elec' */
#define TAG_GROUP_FONT 0x666f6e74 /* 'font' */
#define TAG_GROUP_GLW  0x676c7721 /* 'glw!' */
#define TAG_GROUP_HUDG 0x68756467 /* 'hudg' */
#define TAG_GROUP_ITEM 0x6974656d /* 'item' */
#define TAG_GROUP_ITMC 0x69746d63 /* 'itmc' */
#define TAG_GROUP_LIGH 0x6c696768 /* 'ligh' */
#define TAG_GROUP_PHYS 0x70687973 /* 'phys' */
#define TAG_GROUP_SHDR 0x73686472 /* 'shdr' */
#define TAG_GROUP_SND  0x736e6421 /* 'snd!' */
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
