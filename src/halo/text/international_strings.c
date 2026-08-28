/* 0x19ce70 — Seed the shared draw-string cursor hit-test search and resolve
 * the text cursor position nearest a screen point.
 *
 * Writes the reference point (*ref_point, a packed pair of shorts) and a
 * "no match yet" sentinel into the small globals block at 0x4d9af0 that
 * FUN_0019b430 (draw_string.c) reads/updates per candidate text element:
 *   0x4d9af0 (short ref_x) / 0x4d9af2 (short ref_y) <- *ref_point
 *   0x4d9af4 (short) cursor marker A -> reset to 0, returned
 *   0x4d9af6 (short) best distance   -> reset to 0x7fff (sentinel)
 *   0x4d9af8 (short) cursor marker B -> reset to 0
 * Then walks the text layout via FUN_0019c5d0 (international_strings.obj,
 * same TU) with FUN_0019b430 as the per-element hit-test callback — same
 * calling idiom as the FUN_0019c5d0 call in rasterizer_text.c. Returns the
 * resulting cursor marker.
 */
int16_t FUN_0019ce70(void *screen_pos, char *text, const void *ref_point)
{
  *(int *)0x4d9af0 = *(const int *)ref_point;
  *(int16_t *)0x4d9af4 = 0;
  *(int16_t *)0x4d9af6 = 0x7fff;
  *(int16_t *)0x4d9af8 = 0;

  FUN_0019c5d0(FUN_0019b430, screen_pos, 0, 0, 0, text);

  return *(int16_t *)0x4d9af4;
}

/* 0x19d060 — Set the language/encoding selector read by
 * unicode_is_multibyte below. Values outside 0..5 (the encodings that
 * switch recognizes: 1=Shift-JIS, 2=Big5, 3=GBK, 4=Johab-like,
 * 5=Thai-like, 0=none) are clamped to 0. Stores into the encoding
 * selector global at 0x4d9be0. */
void set_language_code(short code)
{
  if (code < 0)
    code = 0;
  else if (code >= 6)
    code = 0;

  *(int16_t *)0x4d9be0 = code;
}

/* 0x19d080 — Return true if the two bytes starting at p form a valid
 * multibyte character under the current language encoding.
 *
 * The encoding selector lives at 0x4d9be0 (int16_t):
 *   1 = Shift-JIS  (lead: 0x81..0x9f or 0xe0..0xfe; trail: 0x40..0xfc, !=0x7f)
 *   2 = Big5       (lead: 0xa1..0xfe;                trail: 0xa1..0xfe)
 *   3 = GBK        (lead: 0x81..0xfe;                trail: 0x40..0x7e or
 * 0xa1..0xfe) 4 = Johab-like (lead: 0x81..0xfe;                trail:
 * 0x41..0x5a or 0x61..0x7a or 0x81..0xfe) 5 = Thai-like  (lead: 0x84..0xd3 or
 * 0xd8..0xde or 0xe0..0xf9; trail: 0x41..0x7e or 0x81..0xfe) Any other encoding
 * value returns false.
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
  if (*cursor < 0 || (size_t)*cursor > csstrlen(str)) {
    display_assert(csprintf((char *)0x5ab100,
                            "#%d is out of range in string @%p", (int)*cursor,
                            str),
                   "c:\\halo\\SOURCE\\text\\international_strings.c", 0x20, 1);
    system_exit(-1);
  }

  str += *cursor;
  if (unicode_is_multibyte((const uint8_t *)str)) {
    uint16_t ch = (uint16_t)(((uint8_t)str[0] << 8) | (uint8_t)str[1]);
    *cursor += 2;
    return ch;
  } else {
    uint16_t ch = (uint8_t)str[0];
    *cursor += 1;
    return ch;
  }
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

/* 0x19d380 — Return true if character ch occurs anywhere in str. Walks str
 * with unicode_cursor_forward from position 0, comparing each decoded
 * character against ch, stopping at the terminating 0 (not found) or at the
 * first match (found). Callers (parse_string, draw_string.obj) use this to
 * test a decoded character against small delimiter-set strings. */
bool unicode_string_contains_char(uint16_t ch, const char *str)
{
  int16_t cursor;
  uint16_t c;

  cursor = 0;
  do {
    c = unicode_cursor_forward(str, &cursor);
    if (c == 0)
      return 0;
  } while (c != ch);

  return 1;
}
