#include <stdarg.h>

wchar_t *ustrncpy(wchar_t *dest, wchar_t *src, size_t count)
{
  assert_halt(dest && src);
  assert_halt(count < 0x8000);
  _wcsncpy(dest, src, count);
  return dest;
}

/* unicode_sprintf — bounded wide-char printf.
 * Validates buffer, buffer_size in (0, 0x8000], and wcslen(format) < 0x8000,
 * then forwards to the CRT _vsnwprintf with the caller's va_list. */
void unicode_sprintf(wchar_t *buffer, int buffer_size, const wchar_t *format,
                     ...)
{
  va_list args;

  assert_halt(buffer);
  assert_halt((unsigned int)buffer_size > 0 &&
              (unsigned int)buffer_size <= 0x8000);
  assert_halt(_wcslen(format) < 0x8000);

  va_start(args, format);
  _vsnwprintf(buffer, buffer_size, format, (char *)args);
  va_end(args);
}

wchar_t *ascii_to_wide(const char *ascii, wchar_t *unicode, size_t length)
{
  int len;
  int i;

  assert_halt(ascii && unicode);
  len = csstrlen(ascii);
  assert_halt(len < 0x8000);

  if (length < (size_t)(len * 2 + 2))
    return NULL;

  unicode[len] = 0;
  for (i = len - 1; i >= 0; i--)
    unicode[i] = (int16_t)ascii[i];

  return unicode;
}
