#define qmemcpy memcpy

#ifdef MSVC
  #ifdef MSVC_INTRINSIC_DEFS
    #define INCLUDE_PRAGMA_FUNCTIONS
    #define INCLUDE_DEFS
  #else
    #define INCLUDE_PRAGMA_INTRINSICS
    #define INCLUDE_DECLS
  #endif
  #define INLINE
  #define atan2 atan2_
#else
  #define INLINE static inline
  #define INCLUDE_DEFS
#endif


#ifdef INCLUDE_DECLS
double atan2(double y, double x);
void *memcpy(void *s1, const void *s2, size_t n);
size_t strlen(const char *s);
#undef INCLUDE_DECLS
#endif


#ifdef INCLUDE_PRAGMA_FUNCTIONS
// #pragma function(atan2)
#pragma function(memcpy)
#pragma function(strlen)
#undef INCLUDE_PRAGMA_FUNCTIONS
#endif


#ifdef INCLUDE_PRAGMA_INTRINSICS
// #pragma intrinsic(atan2)
#pragma intrinsic(memcpy)
#pragma intrinsic(strlen)
#undef INCLUDE_PRAGMA_INTRINSICS
#endif


#ifdef INCLUDE_DEFS

INLINE double atan2(double y, double x)
{
  double r = 0;
#ifdef MSVC
  __asm {
    fld y
    fld x
    fpatan
    fstp r
  }
#else
  asm volatile ("fpatan" : "=t"(r) : "u"(y), "0"(x) : "st(1)");
#endif
  return r;
}

INLINE float sinf(float x)
{
  float r;
  asm volatile ("fsin" : "=t"(r) : "0"(x));
  return r;
}

INLINE float cosf(float x)
{
  float r;
  asm volatile ("fcos" : "=t"(r) : "0"(x));
  return r;
}

INLINE float sqrtf(float x)
{
  float r;
  asm volatile ("fsqrt" : "=t"(r) : "0"(x));
  return r;
}

INLINE float fabsf(float x)
{
  float r;
  asm volatile ("fabs" : "=t"(r) : "0"(x));
  return r;
}

/* pow(x, y) = 2^(y * log2(x)) via x87 FYL2X + F2XM1 + FSCALE.
 * Valid for x > 0 and any y. No edge-case handling (NaN, inf, x<=0)
 * because the game only calls this for attenuation/progress curves with
 * base in (0,1] and small positive exponents. */
INLINE double pow(double base, double exponent)
{
  double result;
  asm volatile (
    "fyl2x\n\t"              /* ST(0) = exponent * log2(base) */
    "fld %%st(0)\n\t"        /* duplicate */
    "frndint\n\t"             /* ST(0) = integer part */
    "fxch %%st(1)\n\t"       /* swap: ST(0) = full, ST(1) = int */
    "fsub %%st(1), %%st\n\t" /* ST(0) = fractional part */
    "f2xm1\n\t"              /* ST(0) = 2^frac - 1 */
    "fld1\n\t"                /* push 1.0 */
    "faddp\n\t"               /* ST(0) = 2^frac */
    "fscale\n\t"              /* ST(0) = 2^frac * 2^int = 2^(exp*log2(base)) */
    "fstp %%st(1)\n\t"       /* pop the integer part */
    : "=t"(result)
    : "0"(base), "u"(exponent)
    : "st(1)"
  );
  return result;
}

INLINE void *memcpy(void *s1, const void *s2, size_t n)
{
  char *dest = (char *)s1;
  const char *src = (const char *)s2;
  while (n--) {
    *dest++ = *src++;
  }
  return s1;
}

INLINE size_t strlen(const char *s)
{
  size_t c = 0;
  while (*s++) {
    c++;
  }
  return c;
}

#undef INCLUDE_DEFS
#endif // INCLUDE_DEFS

#ifdef INLINE
#undef INLINE
#endif
