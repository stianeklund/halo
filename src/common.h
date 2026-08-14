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
 * expression string. Recover (file,line) via
 * tools/audit/check_assert_targets.py --emit-asserts. Readable-lift Phase 0. */
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
#define MAXIMUM_NUMBER_OF_LOCAL_PLAYERS 4
#define MAXIMUM_STRING_SIZE            0x2000
#define MAXIMUM_MEMSET_SIZE            0x10000000
#define MAXIMUM_MEMCPY_MEMMOVE_SIZE    0x10000000
#define PAGE_READWRITE                 0x04
#define TICKS_PER_SECOND               (*(float *)0x253394) /* 30.0f */

#include "nv097.h"

static const int _scenario_type_main_menu = 2;

#ifdef DEBUG_BUILD
#undef strlen
#define strlen csstrlen
#endif

#endif
