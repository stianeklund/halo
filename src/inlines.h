#define qmemcpy xbox_memcpy

/* ----------------------------------------------------------------
 * Compiler / architecture detection
 *
 * Clang (nxdk) with -target i386-pc-win32 defines _MSC_VER and
 * _M_IX86 but NOT __i386__. Only real cl.exe should use the MSVC/CRT
 * path; Clang and GCC use the freestanding path with GCC-style asm.
 * ---------------------------------------------------------------- */
#if defined(_MSC_VER) && !defined(__clang__)
  #define XBOX_COMPILER_MSVC_X86 1
#elif defined(__i386__) || defined(_M_IX86)
  #define XBOX_COMPILER_GCC_X86 1
#else
  #error Unsupported compiler/architecture: original Xbox requires 32-bit x86.
#endif


/* ================================================================
 * MSVC / XDK path
 *
 * sinf, cosf, sqrtf, fabsf, log10, pow come from the XDK CRT.
 * atan2 is renamed to avoid the CRT version (we use fpatan directly).
 * memcpy, strlen are provided via pragma intrinsic or function body.
 * xbox_* names are macros to the corresponding CRT names.
 * ================================================================ */
#ifdef XBOX_COMPILER_MSVC_X86

#ifdef MSVC_INTRINSIC_DEFS
  #define _XBOX_PRAGMA_FUNCTIONS
  #define _XBOX_PROVIDE_DEFS
#else
  #define _XBOX_PRAGMA_INTRINSICS
  #define _XBOX_PROVIDE_DECLS
#endif

#define atan2 atan2_

#ifdef _XBOX_PROVIDE_DECLS
double atan2(double y, double x);
double pow(double base, double exponent);
void *memcpy(void *s1, const void *s2, size_t n);
size_t strlen(const char *s);
#undef _XBOX_PROVIDE_DECLS
#endif

#ifdef _XBOX_PRAGMA_FUNCTIONS
#pragma function(memcpy)
#pragma function(strlen)
#undef _XBOX_PRAGMA_FUNCTIONS
#endif

#ifdef _XBOX_PRAGMA_INTRINSICS
#pragma intrinsic(memcpy)
#pragma intrinsic(strlen)
#undef _XBOX_PRAGMA_INTRINSICS
#endif

#ifdef _XBOX_PROVIDE_DEFS

double atan2_(double y, double x)
{
  double r = 0;
  __asm {
    fld y
    fld x
    fpatan
    fstp r
  }
  return r;
}

void *memcpy(void *s1, const void *s2, size_t n)
{
  char *dest = (char *)s1;
  const char *src = (const char *)s2;
  while (n--) { *dest++ = *src++; }
  return s1;
}

size_t strlen(const char *s)
{
  size_t c = 0;
  while (*s++) { c++; }
  return c;
}

#undef _XBOX_PROVIDE_DEFS
#endif

#define xbox_sinf   sinf
#define xbox_cosf   cosf
#define xbox_sqrtf  sqrtf
#define xbox_fabsf  fabsf
#define xbox_atan2  atan2_
#define xbox_log10  log10
#define xbox_pow    pow
#define xbox_memcpy memcpy
#define xbox_strlen strlen

#endif /* XBOX_COMPILER_MSVC_X86 */


/* ================================================================
 * GCC / nxdk path (freestanding, no libm)
 *
 * Math functions use x87 inline asm. Memory/string functions are
 * plain C. All defined as static inline.
 * ================================================================ */
#ifdef XBOX_COMPILER_GCC_X86

static inline float xbox_sinf(float x)
{
  float r;
  asm volatile ("fsin" : "=t"(r) : "0"(x));
  return r;
}

static inline float xbox_cosf(float x)
{
  float r;
  asm volatile ("fcos" : "=t"(r) : "0"(x));
  return r;
}

static inline float xbox_sqrtf(float x)
{
  float r;
  asm volatile ("fsqrt" : "=t"(r) : "0"(x));
  return r;
}

static inline float xbox_fabsf(float x)
{
  float r;
  asm volatile ("fabs" : "=t"(r) : "0"(x));
  return r;
}

static inline double xbox_atan2(double y, double x)
{
  double r = 0;
  asm volatile ("fpatan" : "=t"(r) : "u"(y), "0"(x) : "st(1)");
  return r;
}

static inline double xbox_log10(double x)
{
  double result;
  asm volatile (
    "fldlg2\n\t"
    "fxch %%st(1)\n\t"
    "fyl2x"
    : "=t"(result)
    : "0"(x)
  );
  return result;
}

static inline double xbox_pow(double base, double exponent)
{
  double result;
  asm volatile (
    "fyl2x\n\t"
    "fld %%st(0)\n\t"
    "frndint\n\t"
    "fxch %%st(1)\n\t"
    "fsub %%st(1), %%st\n\t"
    "f2xm1\n\t"
    "fld1\n\t"
    "faddp\n\t"
    "fscale\n\t"
    "fstp %%st(1)\n\t"
    : "=t"(result)
    : "0"(base), "u"(exponent)
    : "st(1)"
  );
  return result;
}

static inline void *xbox_memcpy(void *s1, const void *s2, size_t n)
{
  char *dest = (char *)s1;
  const char *src = (const char *)s2;
  while (n--) { *dest++ = *src++; }
  return s1;
}

static inline size_t xbox_strlen(const char *s)
{
  size_t c = 0;
  while (*s++) { c++; }
  return c;
}

#endif /* XBOX_COMPILER_GCC_X86 */


/* ================================================================
 * Standard name mapping (opt-in)
 *
 * When XBOX_REPLACE_STANDARD_NAMES is defined before including this
 * header, standard C names map to xbox_* implementations.
 * On MSVC this is a no-op since xbox_* already aliases CRT names.
 * ================================================================ */
#ifdef XBOX_REPLACE_STANDARD_NAMES
#ifdef XBOX_COMPILER_GCC_X86
  #define sinf    xbox_sinf
  #define cosf    xbox_cosf
  #define sqrtf   xbox_sqrtf
  #define fabsf   xbox_fabsf
  #define atan2   xbox_atan2
  #define log10   xbox_log10
  #define pow     xbox_pow
  #define memcpy  xbox_memcpy
  #define strlen  xbox_strlen
#endif
#endif
