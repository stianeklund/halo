#ifndef COMMON_H
#define COMMON_H

#define XDK_BUILD 1
#define DECOMP_CUSTOM 1
#define DEBUG_BUILD 1

extern const char *build_rev;
extern const char *build_date;
extern const char *build_ui_widget_text;

float __cdecl sinf(float);
float __cdecl cosf(float);
float __cdecl sqrtf(float);
float __cdecl fabsf(float);
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
#define MAXIMUM_NUMBER_OF_LOCAL_PLAYERS 4
#define MAXIMUM_STRING_SIZE            0x2000
#define MAXIMUM_MEMSET_SIZE            0x10000000
#define MAXIMUM_MEMCPY_MEMMOVE_SIZE    0x10000000

static const int _scenario_type_main_menu = 2;

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
