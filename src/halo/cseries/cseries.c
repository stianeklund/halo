#include <stdarg.h>

/* Convert a string to lowercase in place and return it. */
char *csstr_tolower(char *s)
{
  char *p = s;
  while (*p != '\0') {
    if (*p >= 'A' && *p <= 'Z')
      *p = *p + ('a' - 'A');
    p++;
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

/* Byte-compare two buffers with assertions on non-null pointers and
 * reasonable size. Returns 0 if equal, non-zero otherwise. */
int csmemcmp(const void *a, const void *b, int size)
{
  const uint8_t *pa = a;
  const uint8_t *pb = b;
  int i;

  assert_halt(a && b);
  assert_halt((unsigned int)size <= 0x10000000);

  for (i = 0; i < size; i++) {
    if (pa[i] != pb[i])
      return (pa[i] < pb[i]) ? -1 : 1;
  }
  return 0;
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

  assert_halt(s1 && s2);

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
char *csstrcat(char *destination, const char *source, size_t max_size)
{
  assert_halt(destination && source);
  assert_halt(max_size < MAXIMUM_STRING_SIZE);

  crt_strncat(destination, source, max_size);
  return destination;
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
