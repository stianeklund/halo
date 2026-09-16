/* cseries_initialize (0x8d830) — initialise debug allocation and profiling
 * subsystems, then clear the profiling-enable flag. */
void cseries_initialize(void)
{
  FUN_0008e650();
  FUN_0008f630();
  *(uint8_t *)0x449ef1 = 0;
}

/* cseries_dispose (0x8d850) — thunk → debug_dump_memory_for_file(NULL). */
void cseries_dispose(void)
{
  FUN_0008f1e0();
}

/* string_to_tag (0x8d860) — byte-swap the 4-character tag stored at *param_1
 * so it can be compared as a uint32 regardless of endianness. */
uint32_t string_to_tag(uint32_t *param_1)
{
  uint32_t v;
  v = *param_1;
  return ((v & 0xff0000) | v >> 0x10) >> 8 | ((v & 0xff00) | v << 0x10) << 8;
}

/* tag_to_string (0x8d890) — byte-swap param_1 and write it to *param_2,
 * then null-terminate the byte immediately after the stored value. */
uint32_t *tag_to_string(uint32_t param_1, uint32_t *param_2)
{
  *param_2 = ((param_1 & 0xff0000) | param_1 >> 0x10) >> 8 |
             ((param_1 & 0xff00) | param_1 << 0x10) << 8;
  *(uint8_t *)(param_2 + 1) = 0;
  return param_2;
}

/* strnlen (0x8d8d0) — bounded strlen: count non-null chars in s up to n.
 * Returns the number of characters before the first null, at most n. */
int strnlen(const char *s, int n)
{
  int count = 0;

  if (n > 0) {
    do {
      if (*s++ == '\0')
        break;
      count++;
    } while (count < n);
  }
  return count;
}

/* strnupr (0x8d8f0) — convert up to n characters of s to uppercase in
 * place using the CRT toupper function. Returns s. */
char *strnupr(char *s, int n)
{
  char c;
  char *p;

  p = s;
  if (*p == '\0')
    return s;
  do {
    if (n-- <= 0)
      break;
    c = (char)crt_toupper((unsigned char)*p);
    *p++ = c;
    c = *p;
  } while (c != '\0');
  return s;
}

/* strnlwr (0x8d930) — convert up to n characters of s to lowercase in
 * place using the CRT tolower function. Returns s. */
char *strnlwr(char *s, int n)
{
  char c;
  char *p;

  p = s;
  if (*p == '\0')
    return s;
  do {
    if (n-- <= 0)
      break;
    c = (char)crt_tolower((unsigned char)*p);
    *p++ = c;
    c = *p;
  } while (c != '\0');
  return s;
}

/* strupr (0x8d970) — convert entire string s to uppercase in place using
 * the CRT toupper function. Returns s. */
char *strupr(char *s)
{
  char *p;
  char c;

  p = s;
  c = *p;
  while (c != '\0') {
    c = crt_toupper((unsigned char)*p);
    *p = c;
    p++;
    c = *p;
  }
  return s;
}

#include <stdarg.h>

/* csstr_tolower (0x8d9a0) — convert entire string s to lowercase in place
 * using the CRT tolower function. Returns s.
 *
 * Structurally identical to strupr (0x8d970) above, callee aside: the two
 * references differ only in the CALL target (0x1da1d8 crt_tolower vs 0x1da19f
 * crt_toupper).  The previous lift hand-rolled an ASCII `>= 'A' && <= 'Z'`
 * range test, which emits no CALL at all -- a SHAPE-WARN call-count delta of
 * ours 0 vs reference 1 -- and is not the same function for bytes outside
 * ASCII, where crt_tolower consults the locale table. */
char *csstr_tolower(char *s)
{
  char *p;
  char c;

  p = s;
  c = *p;
  while (c != '\0') {
    c = crt_tolower((unsigned char)*p);
    *p = c;
    p++;
    c = *p;
  }
  return s;
}

char *csprintf(char *buffer, const char *format, ...)
{
  va_list args;
  va_start(args, format);
  vsprintf(buffer, format, (char *)args);
  va_end(args);
  return buffer;
}

void display_assert(const char *reason, const char *filepath, int lineno,
                    bool halt)
{
  if (halt) {
    stack_walk(0);
  }
  error(2, "EXCEPTION %s in %s,#%d: %s [rev=%s]", halt ? "halt" : "warn",
        filepath, lineno, reason ? reason : "<no reason given>",
        build_rev ? build_rev : "unknown");
}

/* Freestanding fallbacks for compiler-generated libcalls.  The Xbox image
 * has no hosted CRT, but Clang may lower builtin memory operations to these
 * symbols when the size is not a small compile-time constant. */
int memcmp(const void *a, const void *b, size_t size)
{
  const uint8_t *pa;
  const uint8_t *pb;
  size_t i;

  pa = (const uint8_t *)a;
  pb = (const uint8_t *)b;
  for (i = 0; i < size; i++) {
    if (pa[i] != pb[i])
      return pa[i] < pb[i] ? -1 : 1;
  }
  return 0;
}

void *memset(void *buffer, int c, size_t size)
{
  uint8_t *p;
  size_t i;

  p = (uint8_t *)buffer;
  for (i = 0; i < size; i++)
    p[i] = (uint8_t)c;
  return buffer;
}

#ifdef memcpy
#undef memcpy
#endif
void *memcpy(void *destination, const void *source, size_t size)
{
  uint8_t *dst;
  const uint8_t *src;
  size_t i;

  dst = (uint8_t *)destination;
  src = (const uint8_t *)source;
  for (i = 0; i < size; i++)
    dst[i] = src[i];
  return destination;
}
#define memcpy xbox_memcpy

/* Byte-compare two buffers with assertions on non-null pointers and
 * reasonable size. Returns 0 if equal, non-zero otherwise. */
int csmemcmp(const void *a, const void *b, int size)
{
  if (!(a && b)) {
    stack_walk(0);
    error(2, "EXCEPTION %s in %s,#%d: %s", "halt",
          "c:\\halo\\SOURCE\\cseries\\cseries.c", 255,
          "p1 && p2");
    system_exit(-1);
  }
  if ((unsigned int)size > 0x10000000) {
    stack_walk(0);
    error(2, "EXCEPTION %s in %s,#%d: %s", "halt",
          "c:\\halo\\SOURCE\\cseries\\cseries.c", 256,
          "size>=0 && size<=MAXIMUM_MEMCMP_SIZE");
    system_exit(-1);
  }

  return __builtin_memcmp(a, b, (size_t)size);
}

void csmemmove(void *destination, const void *source, unsigned int size)
{
  assert_halt(destination && source);
  assert_halt(size >= 0 && size <= MAXIMUM_MEMCPY_MEMMOVE_SIZE);
  memmove(destination, source, size);
}

void *csmemset(void *buffer, int c, size_t size)
{
  uint32_t *dst32;
  uint8_t *dst8;
  uint32_t fill;
  size_t i;
  uint8_t b;

  assert_halt(buffer);
  assert_halt(size <= MAXIMUM_MEMSET_SIZE);

  b = (uint8_t)c;
  fill = (b << 24) | (b << 16) | (b << 8) | b;

  dst32 = (uint32_t *)buffer;
  for (i = size >> 2; i != 0; i--)
    *dst32++ = fill;

  dst8 = (uint8_t *)dst32;
  for (i = size & 3; i != 0; i--)
    *dst8++ = b;

  return buffer;
}

/* csstrcmp — string comparison with NULL-pointer assertion.
 * Uses an unrolled-by-2 byte comparison loop matching the original MSVC
 * codegen. Returns -1 if s1 < s2, 0 if equal, 1 if s1 > s2. */
int csstrcmp(const char *s1, const char *s2)
{
  unsigned char c1, c2;

  if (!(s1 && s2)) {
    stack_walk(0);
    error(2, "EXCEPTION %s in %s,#%d: %s", "halt",
          "c:\\halo\\SOURCE\\cseries\\cseries.c", 0x12c,
          "s1 && s2");
    system_exit(-1);
  }

  for (;;) {
    c1 = *(unsigned char *)s1;
    c2 = *(unsigned char *)s2;
    if (c1 != c2)
      break;
    if (c1 == 0)
      return 0;

    c1 = ((unsigned char *)s1)[1];
    c2 = ((unsigned char *)s2)[1];
    if (c1 != c2)
      break;
    s1 += 2;
    s2 += 2;
    if (c1 == 0)
      return 0;
  }

  if (c1 < c2)
    return -1;
  return 1;
}

/* csstrcat — bounded string concatenation with assertions. */
char *csstrncat(char *destination, const char *source, size_t max_size)
{
  if (!(destination && source)) {
    stack_walk(0);
    error(2, "EXCEPTION %s in %s,#%d: %s", "halt",
          "c:\\halo\\SOURCE\\cseries\\cseries.c", 0x137,
          "s1 && s2");
    system_exit(-1);
  }
  if (!(max_size < MAXIMUM_STRING_SIZE)) {
    stack_walk(0);
    error(2, "EXCEPTION %s in %s,#%d: %s", "halt",
          "c:\\halo\\SOURCE\\cseries\\cseries.c", 0x138,
          "size>=0 && size<MAXIMUM_STRING_SIZE");
    system_exit(-1);
  }

  crt_strncat(destination, source, max_size);
  return destination;
}

/* Forward declaration for CRT strncmp (available in XDK/CRT but not pulled
 * in by our Xbox target headers by default). */
extern int strncmp(const char *s1, const char *s2, unsigned int n);

/* 0x8ddd0 — Validated strncmp wrapper. Asserts both strings are non-null and
 * size is within MAXIMUM_STRING_SIZE, then delegates to strncmp. */
int csstrncmp(char *s1, char *s2, unsigned int size)
{
  assert_halt(s1 && s2);
  assert_halt(size <= 0x1fff);
  return strncmp(s1, s2, size);
}

#ifdef strncpy
#undef strncpy
#endif

void *csstrncpy(char *destination, const char *source, size_t size)
{
  assert_halt(destination && source);
  assert_halt(size < MAXIMUM_STRING_SIZE);

  strncpy(destination, source, size);

  return destination;
}

/* csstrtok — tokenize a string with an assertion on delimiters. */
char *csstrtok(char *string, const char *delimiters)
{
  assert_halt(delimiters);

  return crt_strtok(string, delimiters);
}

#ifdef strlen
#undef strlen
#endif

int csstrlen(const char *s1)
{
  int size;

  assert_halt(s1);
  size = xbox_strlen(s1);
  assert_halt(size >= 0 && size < MAXIMUM_STRING_SIZE);

  return size;
}

/* csstrcpy — inline string copy with size and overlap assertions.
 * Measures source length, asserts it's within bounds and non-overlapping,
 * then copies byte-by-byte. */
char *csstrcpy(char *destination, const char *source)
{
  const char *s;
  int source_size;
  char c;

  s = source;
  do {
    c = *s;
    s++;
  } while (c != 0);
  source_size = (int)(s - (source + 1));

  assert_halt(source_size >= 0 && source_size < MAXIMUM_STRING_SIZE);
  assert_halt(source + source_size < destination ||
              destination + source_size < source);

  {
    int offset = (int)destination - (int)source;
    const char *p = source;
    do {
      c = *p;
      *(char *)((int)p + offset) = c;
      p++;
    } while (c != 0);
  }

  return destination;
}

void *csmemcpy(void *destination, void *source, size_t size)
{
  uint32_t *dst32;
  uint32_t *src32;
  uint8_t *dst8;
  uint8_t *src8;
  size_t i;

  assert_halt(destination && source);
  assert_halt(size < MAXIMUM_MEMCPY_MEMMOVE_SIZE);
  assert_halt((uint8_t *)source + size <= (uint8_t *)destination ||
              (uint8_t *)destination + size <= (uint8_t *)source);

  dst32 = (uint32_t *)destination;
  src32 = (uint32_t *)source;
  for (i = size >> 2; i != 0; i--)
    *dst32++ = *src32++;

  dst8 = (uint8_t *)dst32;
  src8 = (uint8_t *)src32;
  for (i = size & 3; i != 0; i--)
    *dst8++ = *src8++;

  return destination;
}

/* Case-insensitive string comparison with assertion on non-null pointers.
 * Returns -1 if s1 < s2, 0 if equal, 1 if s1 > s2 (case-insensitive). */
int csstricmp(const char *s1, const char *s2)
{
  int c1;
  int c2;
  int offset;

  assert_halt(s1 && s2);

  c1 = crt_tolower((unsigned char)*s1);
  c2 = crt_tolower((unsigned char)*s2);

  if (c1 == 0)
    goto check_end;

  offset = (int)(s1 - s2);

  for (;;) {
    if (c2 == 0)
      return 1;
    if (c2 != c1)
      goto not_equal;
    s2++;
    c1 = crt_tolower((unsigned char)s2[offset]);
    c2 = crt_tolower((unsigned char)*s2);
    if (c1 == 0)
      break;
  }

check_end:
  if (c2 != 0)
    return -1;
  return 0;

not_equal:
  if (c1 > c2)
    return 1;
  return -1;
}
