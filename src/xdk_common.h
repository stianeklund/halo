#ifndef COMMON_H
#define COMMON_H

#define XDK_BUILD 1
#define DECOMP_CUSTOM 1
#define DEBUG_BUILD 1

#define TICKS_PER_SECOND               (*(float *)0x253394)

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

#if defined(_MSC_VER) && !defined(__clang__)
#pragma intrinsic(sqrt)
#endif

#include "types.h"
#define XBOX_REPLACE_STANDARD_NAMES
#include "inlines.h"
#ifdef __cplusplus
extern "C" {
#endif
#include "decl.h"
#ifdef __cplusplus
}
#undef NULL
#define NULL 0
#endif

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

/* Screen-bounds rect at 0x50657c — four int16 in the engine's standard 2D
 * rect order {top, left, bottom, right}.  Kept in sync with src/common.h;
 * this header is the one the VC71 compare lane force-includes. */
#define screen_bounds_top (*(int16_t *)0x50657c)
#define screen_bounds_left (*(int16_t *)0x50657e)
#define structure_decals_globals (*(structure_decals_globals_t **)0x4d8ec8)

/* network_server_manager.c globals. Kept in sync with src/common.h. */
#define network_game_server_memory_do_not_use_directly_in_use (*(char *)0x46eed4)
#define network_game_server_next_team (*(int *)0x46eed8)
#define network_game_server_connection (*(int *)0x5a90e0)
#define network_game_server_state (*(short *)0x5a90e4)
#define network_game_server_flags (*(short *)0x5a90e6)
#define network_game_server_difficulty (*(short *)0x5a91f8)
#define network_game_server_reset_counter (*(int *)0x5a9514)
#define network_game_server_loading_flag (*(char *)0x5a9599)
#define network_game_server_all_loaded_time (*(int *)0x5a9564)

/* hs boolean global "allow_out_of_sync". Kept in sync with src/common.h. */
#define allow_out_of_sync (*(char *)0x46e8b8)

/* assert_halt_at(file, line, cond) — byte-match-faithful assert (see common.h). */
#define assert_halt_at(file, line, cond)                       \
    do {                                                     \
        if (!(cond)) {                                       \
            display_assert(#cond, file, line, true);         \
            system_exit(-1);                                 \
        }                                                    \
    } while (0)

/* assert_halt_msg_at(msg, file, line, cond) — see common.h. Mirrored here so
 * the VC71 lane (which force-includes this header, not common.h, and compiles
 * at /W0) does not silently turn the macro into an implicit function call. */
#define assert_halt_msg_at(msg, file, line, cond)            \
    do {                                                     \
        if (!(cond)) {                                       \
            display_assert(msg, file, line, true);           \
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

#define assert_halt_msg(cond, msg)                           \
    do {                                                     \
        if (!(cond)) {                                       \
            display_assert(msg, __FILE__, __LINE__, true);   \
            system_exit(-1);                                 \
        }                                                    \
    } while (0)

#endif
