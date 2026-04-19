#include <stdarg.h>

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
  error(2, "EXCEPTION %s in %s,#%d: %s", halt ? "halt" : "warn", filepath,
        lineno, reason ? reason : "<no reason given>");
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
  size = strlen(s1);
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
