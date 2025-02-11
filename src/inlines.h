#ifdef MSVC_EARLY
#define qmemcpy memcpy strlen
#pragma intrinsic(atan2)
#pragma intrinsic(memcpy)
#pragma intrinsic(strlen)
#else
#define atan2 atan2_

static inline double atan2_(double y, double x)
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

static inline void *qmemcpy(void *s1, const void *s2, size_t n)
{
  char *dest = (char *)s1;
  const char *src = (const char *)s2;
  while (n--) {
    *dest++ = *src++;
  }
  return s1;
}

static inline size_t strlen(const char *s)
{
  size_t c = 0;
  while (*s++) {
    c++;
  }
  return c;
}

#endif // MSVC_EARLY
