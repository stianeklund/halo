/* 0x19d1b0 — Read the character at *cursor and advance cursor forward.
 * If the byte is a multibyte lead byte (per unicode_is_multibyte), reads
 * two bytes big-endian and advances by 2; otherwise reads one byte and
 * advances by 1. Returns the character as uint16_t. */
uint16_t unicode_cursor_forward(const char *str, int16_t *cursor)
{
  const uint8_t *p;

  if (*cursor < 0 || (unsigned int)(int)*cursor > csstrlen(str)) {
    display_assert(csprintf((char *)0x5ab100,
                            "#%d is out of range in string @%p", (int)*cursor,
                            str),
                   "c:\\halo\\SOURCE\\text\\international_strings.c", 0x20, 1);
    system_exit(-1);
  }

  p = (const uint8_t *)(str + *cursor);
  if (unicode_is_multibyte(p)) {
    uint8_t b1 = p[0];
    uint8_t b2 = p[1];
    *cursor += 2;
    return (uint16_t)((b1 << 8) | b2);
  }

  *cursor += 1;
  return (uint16_t)p[0];
}

/* 0x19d240 — Move cursor backward by one character. Scans forward from
 * position 0 using unicode_cursor_forward, tracking the previous position.
 * Warns if *cursor falls between multibyte character bytes. Sets *cursor
 * to the start of the preceding character and returns it. */
uint16_t unicode_cursor_backward(const char *str, int16_t *cursor)
{
  int16_t pos;
  int16_t prev;
  uint16_t ch;

  if (*cursor <= 0 || (unsigned int)(int)*cursor > csstrlen(str)) {
    display_assert(csprintf((char *)0x5ab100,
                            "#%d is out of range in string @%p", (int)*cursor,
                            str),
                   "c:\\halo\\SOURCE\\text\\international_strings.c", 0x37, 1);
    system_exit(-1);
  }

  pos = 0;
  do {
    prev = pos;
    ch = unicode_cursor_forward(str, &pos);
  } while (pos < *cursor);

  if (pos != *cursor) {
    display_assert(csprintf((char *)0x5ab100,
                            "index #%d is inbetween characters in string %p",
                            (int)*cursor, str),
                   "c:\\halo\\SOURCE\\text\\international_strings.c", 0x43, 0);
  }

  *cursor = prev;
  return ch;
}

/* 0x19d300 — Snap cursor to a valid character boundary. Scans forward from
 * position 0 using unicode_cursor_forward until reaching or passing *cursor,
 * then writes the last valid position back to *cursor. */
void unicode_snap_cursor(const char *str, int16_t *cursor)
{
  int16_t pos;

  if (*cursor < 0 || (unsigned int)(int)*cursor > csstrlen(str)) {
    display_assert(csprintf((char *)0x5ab100,
                            "#%d is out of range in string @%p", (int)*cursor,
                            str),
                   "c:\\halo\\SOURCE\\text\\international_strings.c", 0x55, 1);
    system_exit(-1);
  }

  pos = 0;
  if (*cursor > 0) {
    do {
      unicode_cursor_forward(str, &pos);
    } while (pos < *cursor);
  }

  *cursor = pos;
}
