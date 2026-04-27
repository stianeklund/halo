#define XDK_BUILD 1

float __cdecl sinf(float);
float __cdecl cosf(float);
float __cdecl sqrtf(float);
float __cdecl fabsf(float);

#include "types.h"
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

#define assert_halt(cond)                                    \
    do {                                                     \
        if (!(cond)) {                                       \
            display_assert(#cond, __FILE__, __LINE__, true); \
            system_exit(-1);                                 \
        }                                                    \
    } while (0)
