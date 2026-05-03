/* 0x19d080 — Return true if the two bytes starting at p form a valid
 * multibyte character under the current language encoding.
 *
 * The encoding selector lives at 0x4d9be0 (int16_t):
 *   1 = Shift-JIS  (lead: 0x81..0x9f or 0xe0..0xfe; trail: 0x40..0xfc, !=0x7f)
 *   2 = Big5       (lead: 0xa1..0xfe;                trail: 0xa1..0xfe)
 *   3 = GBK        (lead: 0x81..0xfe;                trail: 0x40..0x7e or 0xa1..0xfe)
 *   4 = Johab-like (lead: 0x81..0xfe;                trail: 0x41..0x5a or 0x61..0x7a or 0x81..0xfe)
 *   5 = Thai-like  (lead: 0x84..0xd3 or 0xd8..0xde or 0xe0..0xf9; trail: 0x41..0x7e or 0x81..0xfe)
 * Any other encoding value returns false.
 *
 * A leading '|' byte (0x7c) followed by a byte in "ibukprlctn" is treated
 * as multibyte regardless of the encoding setting. */
bool unicode_is_multibyte(const uint8_t *p)
{
    uint8_t b0 = p[0];
    uint8_t b1 = p[1];

    if (b0 == 0)
        return 0;

    /* '|' escape-sequence prefix */
    if (b0 == 0x7c && b1 != 0 && crt_strchr("ibukprlctn", (int)b1) != (char *)0x0)
        return 1;

    switch (*(int16_t *)0x4d9be0) {
    case 1: /* Shift-JIS */
        if (!((b0 >= 0x81 && b0 <= 0x9f) || (b0 >= 0xe0 && b0 <= 0xfe)))
            return 0;
        if (b1 < 0x40 || b1 > 0xfc || b1 == 0x7f)
            return 0;
        return 1;
    case 2: /* Big5 */
        if (b0 < 0xa1 || b0 > 0xfe)
            return 0;
        if (b1 < 0xa1 || b1 > 0xfe)
            return 0;
        return 1;
    case 3: /* GBK */
        if (b0 < 0x81 || b0 > 0xfe)
            return 0;
        if ((b1 >= 0x40 && b1 <= 0x7e) || (b1 >= 0xa1 && b1 <= 0xfe))
            return 1;
        return 0;
    case 4: /* Johab-like */
        if (b0 < 0x81 || b0 > 0xfe)
            return 0;
        if ((b1 >= 0x41 && b1 <= 0x5a) || (b1 >= 0x61 && b1 <= 0x7a) ||
            (b1 >= 0x81 && b1 <= 0xfe))
            return 1;
        return 0;
    case 5: /* Thai-like */
        if (!((b0 >= 0x84 && b0 <= 0xd3) || (b0 >= 0xd8 && b0 <= 0xde) ||
              (b0 >= 0xe0 && b0 <= 0xf9)))
            return 0;
        if ((b1 >= 0x41 && b1 <= 0x7e) || (b1 >= 0x81 && b1 <= 0xfe))
            return 1;
        return 0;
    default:
        return 0;
    }
}

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
