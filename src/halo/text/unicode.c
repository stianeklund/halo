#include <stdarg.h>

/* ustrcmp (0x19d810) — wide-string ordinal compare.
 * Halts if either string is NULL (assert reason "string1 && string2",
 * matching the recovered .rdata literal for the combined condition), or if
 * wcslen(string1)/wcslen(string2) is not < 0x8000 (same MAXIMUM_STRING_SIZE
 * bound as ustrlen/ustrnlen/ustrchr/ustrcoll/ustrcspn; checked separately
 * per string, each with its own recovered assert text and line). Calls the
 * LIBCMT _wcscmp entry at 0x1dbf75 directly (disassembly targets the
 * function body itself, not the 0x1dbfa7 JMP thunk that ustrcoll's
 * thunk__wcscmp uses) and returns its result unmodified. */
int ustrcmp(const wchar_t *string1, const wchar_t *string2)
{
  unsigned int length;

  assert_halt_msg_at("string1 && string2", "c:\\halo\\SOURCE\\text\\unicode.c",
                     0xb5, string1 != NULL && string2 != NULL);
  length = _wcslen(string1);
  assert_halt_msg_at("wcslen(string1) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0xb6,
                     length < 0x8000);
  length = _wcslen(string2);
  assert_halt_msg_at("wcslen(string2) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0xb7,
                     length < 0x8000);
  return _wcscmp(string1, string2);
}

/* ustrlen (0x19d8c0) — wide-string length with bounds validation.
 * Halts if string is NULL, or if the computed length is not < 0x8000
 * (assert text reads "size < MAXIMUM_STRING_SIZE", but the immediate
 * compared against is 0x8000, not this TU's MAXIMUM_STRING_SIZE macro
 * (0x2000) — matches the 0x8000 bound used by every other function in
 * this file). */
int ustrlen(const unsigned short *string)
{
  unsigned int size;

  assert_halt_at("c:\\halo\\SOURCE\\text\\unicode.c", 0xc2, string);
  size = _wcslen((const wchar_t *)string);
  assert_halt_msg_at("size < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0xc4, size < 0x8000);
  return size;
}

/* ustrnlen (0x19d930) — wide-string length bounded by an explicit maximum.
 * Halts if string is NULL (assert reason is the stringized condition
 * "string", matching the recovered .rdata literal). When max_count is
 * nonzero, scans at most max_count wide chars for a NUL terminator, then
 * halts if the resulting length is not < 0x8000 (same MAXIMUM_STRING_SIZE
 * bound as ustrlen). The size assert sits inside the max_count != 0 branch
 * in the original, so a zero max_count returns 0 without applying it. */
int ustrnlen(const unsigned short *string, unsigned int max_count)
{
  unsigned int size;
  const unsigned short *p;
  unsigned short ch;

  size = 0;
  assert_halt_at("c:\\halo\\SOURCE\\text\\unicode.c", 0xd0, string);
  if (max_count != 0) {
    p = string;
    do {
      ch = *p;
      p++;
      if (ch == 0) {
        break;
      }
      size++;
    } while (size < max_count);
    assert_halt_msg_at("size < MAXIMUM_STRING_SIZE",
                       "c:\\halo\\SOURCE\\text\\unicode.c", 0xd6,
                       size < 0x8000);
  }
  return size;
}

/* ustrchr (0x19d9b0) — locate the first occurrence of ch in string.
 * Halts if string is NULL (assert reason "string", matching the recovered
 * .rdata literal), or if wcslen(string) is not < 0x8000 (same
 * MAXIMUM_STRING_SIZE bound as ustrlen/ustrnlen; the recovered assert text
 * names wcslen(string) directly rather than a local variable, so
 * assert_halt_msg_at reproduces the literal while the condition uses the
 * already-computed length). Forwards to the CRT _wcschr and returns its
 * result unmodified. */
wchar_t *ustrchr(const wchar_t *string, wchar_t ch)
{
  unsigned int length;

  assert_halt_at("c:\\halo\\SOURCE\\text\\unicode.c", 0xe0, string);
  length = _wcslen(string);
  assert_halt_msg_at("wcslen(string) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0xe1,
                     length < 0x8000);
  return _wcschr(string, ch);
}

/* ustrcoll (0x19da20) — wide-string collation compare.
 * Halts if either string is NULL (assert reason "string1 && string2",
 * matching the recovered .rdata literal for the combined condition), or if
 * wcslen(string1)/wcslen(string2) is not < 0x8000 (same MAXIMUM_STRING_SIZE
 * bound as ustrlen/ustrnlen/ustrchr; checked separately per string, each
 * with its own recovered assert text and line). Forwards to the CRT wcscmp
 * via a 5-byte JMP thunk at 0x1dbfa7 (confirmed by direct disassembly: the
 * thunk is `jmp 0x1dbf75`, the entry of the LIBCMT _wcscmp already in this
 * TU's callee set) and returns its result unmodified — this build has no
 * locale support, so collation reduces to ordinal comparison, matching the
 * original. */
int ustrcoll(const wchar_t *string1, const wchar_t *string2)
{
  unsigned int length;

  assert_halt_msg_at("string1 && string2", "c:\\halo\\SOURCE\\text\\unicode.c",
                     0xeb, string1 != NULL && string2 != NULL);
  length = _wcslen(string1);
  assert_halt_msg_at("wcslen(string1) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0xec,
                     length < 0x8000);
  length = _wcslen(string2);
  assert_halt_msg_at("wcslen(string2) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0xed,
                     length < 0x8000);
  return thunk__wcscmp(string1, string2);
}

/* ustrcspn (0x19dad0) — wide-string complementary span.
 * Halts if either string or character_set is NULL (assert reason
 * "string && character_set", matching the recovered .rdata literal for the
 * combined condition), or if wcslen(string)/wcslen(character_set) is not <
 * 0x8000 (same MAXIMUM_STRING_SIZE bound as ustrlen/ustrnlen/ustrchr/ustrcoll;
 * checked separately per string, each with its own recovered assert text and
 * line). Forwards to the CRT _wcscspn and returns its result unmodified. */
size_t ustrcspn(const wchar_t *string, const wchar_t *character_set)
{
  unsigned int length;

  assert_halt_msg_at("string && character_set",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0xf7,
                     string != NULL && character_set != NULL);
  length = _wcslen(string);
  assert_halt_msg_at("wcslen(string) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0xf8,
                     length < 0x8000);
  length = _wcslen(character_set);
  assert_halt_msg_at("wcslen(character_set) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0xf9,
                     length < 0x8000);
  return _wcscspn(string, character_set);
}

/* ustrncat (0x19db80) — bounded wide-string concatenation.
 * Halts if dest or src is NULL (assert reason "dest && src", matching the
 * recovered .rdata literal for the combined condition), if wcslen(dest) is
 * not < 0x8000 (same MAXIMUM_STRING_SIZE bound as ustrlen/ustrncmp/
 * ustrcspn), or if count is not < 0x8000 (assert reason "(count >= 0) &&
 * (count < MAXIMUM_STRING_SIZE)" — count is unsigned, so only the upper
 * bound is live in the compiled CMP/JC). Forwards to the CRT _wcsncat and
 * discards its return value (matches the original: no MOV EAX after the
 * CALL, and the function is declared void). */
void ustrncat(wchar_t *dest, wchar_t *src, size_t count)
{
  unsigned int length;

  assert_halt_msg_at("dest && src", "c:\\halo\\SOURCE\\text\\unicode.c", 0x111,
                     dest != NULL && src != NULL);
  length = _wcslen(dest);
  assert_halt_msg_at("wcslen(dest) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x112,
                     length < 0x8000);
  assert_halt_msg_at("(count >= 0) && (count < MAXIMUM_STRING_SIZE)",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x113,
                     count < 0x8000);
  _wcsncat(dest, src, count);
}

/* ustrncmp (0x19dc20) — bounded wide-string ordinal compare.
 * Halts if either string is NULL (assert reason "string1 && string2",
 * matching the recovered .rdata literal for the combined condition), or if
 * count is not < 0x8000 (same MAXIMUM_STRING_SIZE bound as ustrncat/
 * ustrncpy; assert reason "(count >= 0) && (count < MAXIMUM_STRING_SIZE)" —
 * count is unsigned, so only the upper bound is live in the compiled
 * CMP/JC). Unlike ustrcmp/ustrcoll, there is no wcslen check on either
 * string here — the disassembly has no _wcslen call, only the two guards
 * above. Forwards to the CRT _wcsncmp and returns its result unmodified
 * (no MOV EAX after the CALL; EAX passes through). */
int ustrncmp(const wchar_t *s1, const wchar_t *s2, size_t count)
{
  assert_halt_msg_at("string1 && string2", "c:\\halo\\SOURCE\\text\\unicode.c",
                     0x12a, s1 != NULL && s2 != NULL);
  assert_halt_msg_at("(count >= 0) && (count < MAXIMUM_STRING_SIZE)",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x12b,
                     count < 0x8000);
  return _wcsncmp(s1, s2, count);
}

wchar_t *ustrncpy(wchar_t *dest, wchar_t *src, size_t count)
{
  assert_halt(dest && src);
  assert_halt(count < 0x8000);
  _wcsncpy(dest, src, count);
  return dest;
}

/* ustrpbrk (0x19dd00) — wide-string pointer-break search.
 * Halts if either string or character_set is NULL (assert reason
 * "string && character_set", matching the recovered .rdata literal for the
 * combined condition), or if wcslen(string)/wcslen(character_set) is not <
 * 0x8000 (same MAXIMUM_STRING_SIZE bound as ustrlen/ustrnlen/ustrchr/
 * ustrcoll/ustrcspn; checked separately per string, each with its own
 * recovered assert text and line). Forwards to the CRT _wcspbrk and returns
 * its result unmodified (no MOV EAX after the CALL; EAX passes through). */
wchar_t *ustrpbrk(const wchar_t *string, const wchar_t *character_set)
{
  unsigned int length;

  assert_halt_msg_at("string && character_set",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x141,
                     string != NULL && character_set != NULL);
  length = _wcslen(string);
  assert_halt_msg_at("wcslen(string) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x142,
                     length < 0x8000);
  length = _wcslen(character_set);
  assert_halt_msg_at("wcslen(character_set) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x143,
                     length < 0x8000);
  return _wcspbrk(string, character_set);
}

/* ustrrchr (0x19ddb0) — locate the last occurrence of ch in string.
 * Halts if string is NULL (assert reason "string", matching the recovered
 * .rdata literal), or if wcslen(string) is not < 0x8000 (same
 * MAXIMUM_STRING_SIZE bound as ustrchr/ustrlen/ustrnlen). Forwards to the
 * CRT _wcsrchr and returns its result unmodified (no MOV EAX after the
 * CALL; EAX passes through). */
wchar_t *ustrrchr(const wchar_t *string, wchar_t ch)
{
  unsigned int length;

  assert_halt_at("c:\\halo\\SOURCE\\text\\unicode.c", 0x14d, string);
  length = _wcslen(string);
  assert_halt_msg_at("wcslen(string) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x14e,
                     length < 0x8000);
  return _wcsrchr(string, ch);
}

/* ustrspn (0x19de20) — count of leading characters in string that are drawn
 * from character_set. Halts if either string or character_set is NULL
 * (assert reason "string && character_set", matching the recovered .rdata
 * literal for the combined condition -- same guard as ustrpbrk/ustrcspn), or
 * if wcslen(string)/wcslen(character_set) is not < 0x8000 (same
 * MAXIMUM_STRING_SIZE bound as the other ustr* helpers; checked separately
 * per string, each with its own recovered assert text and line). Forwards to
 * the CRT _wcsspn and returns its result unmodified (no MOV EAX after the
 * CALL; EAX passes through). */
size_t ustrspn(const wchar_t *string, const wchar_t *character_set)
{
  unsigned int length;

  assert_halt_msg_at("string && character_set",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x158,
                     string != NULL && character_set != NULL);
  length = _wcslen(string);
  assert_halt_msg_at("wcslen(string) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x159,
                     length < 0x8000);
  length = _wcslen(character_set);
  assert_halt_msg_at("wcslen(character_set) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x15a,
                     length < 0x8000);
  return _wcsspn(string, character_set);
}

/* ustrstr (0x19ded0) — wide-string substring search.
 * Halts if either string or character_set is NULL (assert reason
 * "string && character_set", matching the recovered .rdata literal for the
 * combined condition -- same guard as ustrpbrk/ustrcspn/ustrspn), or if
 * wcslen(string)/wcslen(character_set) is not < 0x8000 (same
 * MAXIMUM_STRING_SIZE bound as the other ustr* helpers; checked separately
 * per string, each with its own recovered assert text and line). Forwards to
 * the CRT _wcsstr and returns its result unmodified (no MOV EAX after the
 * CALL; EAX passes through). */
wchar_t *ustrstr(const wchar_t *string, const wchar_t *character_set)
{
  unsigned int length;

  assert_halt_msg_at("string && character_set",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x164,
                     string != NULL && character_set != NULL);
  length = _wcslen(string);
  assert_halt_msg_at("wcslen(string) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x165,
                     length < 0x8000);
  length = _wcslen(character_set);
  assert_halt_msg_at("wcslen(character_set) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x166,
                     length < 0x8000);
  return _wcsstr(string, character_set);
}

/* ustrtok (0x19df80) — wide-string tokenizer. Halts if delimiters is NULL
 * (assert reason "delimiters", matching the recovered .rdata literal --
 * note only delimiters is checked, not string, matching strtok's contract
 * that string may be NULL to continue tokenizing a previous call), or if
 * wcslen(delimiters) is not < 0x8000 (same MAXIMUM_STRING_SIZE bound as the
 * other ustr* helpers). Forwards to the CRT _wcstok(string, delimiters) and
 * returns its result unmodified (no MOV EAX after the CALL; EAX passes
 * through). */
wchar_t *ustrtok(wchar_t *string, const wchar_t *delimiters)
{
  unsigned int length;

  assert_halt_at("c:\\halo\\SOURCE\\text\\unicode.c", 0x170, delimiters);
  length = _wcslen(delimiters);
  assert_halt_msg_at("wcslen(delimiters) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x171,
                     length < 0x8000);
  return _wcstok(string, delimiters);
}

/* ustrxfrm (0x19dff0) — wide-string locale transform. Halts if dest or src is
 * NULL (assert reason "dest && src", matching the recovered .rdata literal
 * for the combined condition), if wcslen(dest) is not < 0x8000 (same
 * MAXIMUM_STRING_SIZE bound as the other ustr* helpers), if wcslen(src) is
 * not < 0x8000, or if count is not < 0x8000 (each guard has its own
 * recovered assert text and line). Forwards to FUN_001dc257(dest, src,
 * count) and discards its return value (matches the original: no MOV EAX
 * after the CALL, and the function is declared void). Ghidra's own
 * decompile resolves the FUN_001dc257 call site to the literal name
 * "wcsxfrm" (a bound CRT symbol, not an inferred neighbor), consistent with
 * this call shape and its position among the other _wcs* CRT entries in
 * this TU, but the identity is not confirmed as a PDB/string anchor so the
 * kb.json symbol stays FUN_001dc257 pending a formal rename. */
void ustrxfrm(wchar_t *dest, wchar_t *src, size_t count)
{
  unsigned int length;

  assert_halt_msg_at("dest && src", "c:\\halo\\SOURCE\\text\\unicode.c", 0x17c,
                     dest != NULL && src != NULL);
  length = _wcslen(dest);
  assert_halt_msg_at("wcslen(dest) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x17d,
                     length < 0x8000);
  length = _wcslen(src);
  assert_halt_msg_at("wcslen(src) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x17e,
                     length < 0x8000);
  assert_halt_msg_at("count < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x17f,
                     count < 0x8000);
  FUN_001dc257(dest, src, count);
}

/* ustrlwr (0x19e0c0) — in-place wide-string lowercase conversion. Halts if
 * string is NULL (assert reason "string", matching the recovered .rdata
 * literal -- same simple-pointer-check shape as ustrlen/ustrrchr/ustrtok), or
 * if wcslen(string) is not < 0x8000 (same MAXIMUM_STRING_SIZE bound as the
 * other ustr* helpers). Forwards to the CRT __wcslwr(string) and discards its
 * return value (matches the original: ADD ESP,4 with no MOV EAX after the
 * CALL, and the function is declared void). */
void ustrlwr(wchar_t *string)
{
  unsigned int length;

  assert_halt_at("c:\\halo\\SOURCE\\text\\unicode.c", 0x188, string);
  length = _wcslen(string);
  assert_halt_msg_at("wcslen(string) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x189,
                     length < 0x8000);
  __wcslwr(string);
}

/* ustrupr (0x19e130) — in-place wide-string uppercase conversion. Halts if
 * string is NULL (assert reason "string", matching the recovered .rdata
 * literal -- same simple-pointer-check shape as ustrlwr), or if
 * wcslen(string) is not < 0x8000 (same MAXIMUM_STRING_SIZE bound as the
 * other ustr* helpers, same recovered message text as ustrchr/ustrlwr).
 * Forwards to the CRT __wcsupr(string) and discards its return value
 * (matches the original: ADD ESP,4 with no MOV EAX after the CALL, and the
 * function is declared void). */
void ustrupr(wchar_t *string)
{
  unsigned int length;

  assert_halt_at("c:\\halo\\SOURCE\\text\\unicode.c", 0x192, string);
  length = _wcslen(string);
  assert_halt_msg_at("wcslen(string) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x193,
                     length < 0x8000);
  __wcsupr(string);
}

/* ustrnlwr (0x19e1a0) — in-place wide-string lowercase conversion with an
 * extra bounds-checked count parameter. Halts if string is NULL (assert
 * reason "string", same literal as ustrlwr/ustrupr), if wcslen(string) is
 * not < MAXIMUM_STRING_SIZE (0x8000), or if count is not < MAXIMUM_STRING_SIZE
 * (each guard has its own recovered assert text and line). The count
 * parameter is only validated, never used as the loop bound -- the
 * disassembly's conversion loop runs off the NUL terminator only (CMP word
 * ptr [ESI],0x0 / JNZ), with no comparison against count anywhere inside it.
 * Unlike ustrlwr (which forwards whole-string to the CRT __wcslwr), this
 * function walks the string itself and calls FUN_001dc27c per character,
 * passing the zero-extended wchar_t (XOR EAX,EAX; MOV AX,[ESI]) and storing
 * the low 16 bits of the result back in place. Returns the original string
 * pointer (MOV EAX,EDI before the epilogue; EDI is loaded once from the
 * incoming pointer and never advanced -- only ESI walks the string), unlike
 * ustrlwr/ustrupr which are void. Ghidra's own decompile resolves the
 * FUN_001dc27c call to the literal name "towupper" -- a bound CRT symbol per
 * its own database, not an inferred neighbor -- but that identity is
 * unconfirmed by a PDB/string anchor (and sits oddly against this function's
 * own PDB-confirmed "lower" name), so the kb.json symbol stays FUN_001dc27c
 * pending a formal rename, same convention as FUN_001dc257 in ustrxfrm
 * above. */
wchar_t *ustrnlwr(wchar_t *string, size_t count)
{
  unsigned int length;
  wchar_t *p;

  assert_halt_at("c:\\halo\\SOURCE\\text\\unicode.c", 0x19f, string);
  length = _wcslen(string);
  assert_halt_msg_at("wcslen(string) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x1a0,
                     length < 0x8000);
  assert_halt_msg_at("count < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x1a1,
                     count < 0x8000);

  p = string;
  while (*p != L'\0') {
    *p = (wchar_t)FUN_001dc27c(*p);
    p++;
  }

  return string;
}

/* ustrnupr (0x19e250) — in-place wide-string uppercase conversion with an
 * extra bounds-checked count parameter. Halts if string is NULL (assert
 * reason "string", same literal as ustrlwr/ustrupr/ustrnlwr), if
 * wcslen(string) is not < MAXIMUM_STRING_SIZE (0x8000), or if count is not
 * < MAXIMUM_STRING_SIZE (each guard has its own recovered assert text and
 * line, matching ustrnlwr's shape one-for-one). The count parameter is only
 * validated, never used as the loop bound -- the disassembly's conversion
 * loop runs off the NUL terminator only (CMP word ptr [ESI],0x0 / JNZ), with
 * no comparison against count anywhere inside it. Like ustrnlwr (and unlike
 * ustrlwr/ustrupr which forward the whole string to the CRT in one call and
 * are void), this function walks the string itself and calls FUN_001da8e3
 * per character, passing the zero-extended wchar_t (XOR EAX,EAX; MOV
 * AX,[ESI]) and storing the low 16 bits of the result back in place.
 * Returns the original string pointer (MOV EAX,EDI before the epilogue; EDI
 * is loaded once from the incoming pointer and never advanced -- only ESI
 * walks the string). Ghidra's own decompile resolves the FUN_001da8e3 call
 * to the literal name "towlower" -- a bound CRT symbol per its own
 * database, not an inferred neighbor -- but that identity is unconfirmed by
 * a PDB/string anchor (and sits oddly against this function's own
 * PDB-confirmed "upper" name, the mirror image of ustrnlwr's
 * towupper-vs-"lower" mismatch), so the kb.json symbol stays FUN_001da8e3
 * pending a formal rename, same convention as FUN_001dc27c in ustrnlwr
 * above. */
wchar_t *ustrnupr(wchar_t *string, size_t count)
{
  unsigned int length;
  wchar_t *p;

  assert_halt_at("c:\\halo\\SOURCE\\text\\unicode.c", 0x1b3, string);
  length = _wcslen(string);
  assert_halt_msg_at("wcslen(string) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x1b4,
                     length < 0x8000);
  assert_halt_msg_at("count < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x1b5,
                     count < 0x8000);

  p = string;
  while (*p != L'\0') {
    *p = (wchar_t)FUN_001da8e3(*p);
    p++;
  }

  return string;
}

/* ustrcasecmp (0x19e300) — wide-string case-insensitive compare.
 * Halts if either string is NULL (assert reason "string1 && string2", same
 * combined-condition literal as ustrcmp/ustrcoll), or if wcslen(string1)/
 * wcslen(string2) is not < 0x8000 (MAXIMUM_STRING_SIZE bound, checked
 * separately per string with its own recovered assert text and line,
 * matching ustrcmp's shape one-for-one). Calls __wcsicmp(string1, string2)
 * (disassembly pushes EDI (string2) then ESI (string1), so string1 is the
 * first cdecl argument) and returns its result unmodified. */
int ustrcasecmp(const wchar_t *string1, const wchar_t *string2)
{
  unsigned int length;

  assert_halt_msg_at("string1 && string2", "c:\\halo\\SOURCE\\text\\unicode.c",
                     0x1c7, string1 != NULL && string2 != NULL);
  length = _wcslen(string1);
  assert_halt_msg_at("wcslen(string1) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x1c8,
                     length < 0x8000);
  length = _wcslen(string2);
  assert_halt_msg_at("wcslen(string2) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x1c9,
                     length < 0x8000);
  return __wcsicmp(string1, string2);
}

/* ustrncasecmp (0x19e3b0) — bounded wide-string case-insensitive compare.
 * Halts if either string is NULL (assert reason "string1 && string2", same
 * combined-condition literal as ustrcmp/ustrcoll/ustrcasecmp), or if
 * wcslen(string1)/wcslen(string2) is not < 0x8000 (MAXIMUM_STRING_SIZE bound,
 * checked separately per string with its own recovered assert text and line,
 * matching ustrcasecmp's shape one-for-one). Unlike ustrncmp/ustrncat, count
 * itself is never compared against MAXIMUM_STRING_SIZE -- only the two
 * wcslen() results are guarded. Calls __wcsnicmp(string1, string2, count)
 * (disassembly pushes count, then EDI (string2), then ESI (string1) last, so
 * string1 is the first cdecl argument) and returns its result unmodified (no
 * MOV EAX after the CALL; EAX passes through). */
int ustrncasecmp(const wchar_t *string1, const wchar_t *string2, size_t count)
{
  unsigned int length;

  assert_halt_msg_at("string1 && string2", "c:\\halo\\SOURCE\\text\\unicode.c",
                     0x1d8, string1 != NULL && string2 != NULL);
  length = _wcslen(string1);
  assert_halt_msg_at("wcslen(string1) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x1d9,
                     length < 0x8000);
  length = _wcslen(string2);
  assert_halt_msg_at("wcslen(string2) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x1da,
                     length < 0x8000);
  return __wcsnicmp(string1, string2, count);
}

/* uisalpha (0x19e460) — wide-char alphabetic classification.
 * Thin wrapper: forwards the incoming character unmodified as the first
 * cdecl argument to the CRT _iswctype(c, 0x0103), where 0x0103 is the MS CRT
 * wctype.h _ALPHA mask (0x0100 | _UPPER(0x0001) | _LOWER(0x0002)). Disassembly
 * pushes the mask first, then EAX (the parameter, loaded unmodified from
 * [EBP+8]) last, so the parameter is the first cdecl argument. No MOV EAX
 * after the CALL -- the CRT return value passes through as this function's
 * return. */
int uisalpha(int c)
{
  return _iswctype(c, 0x0103);
}

/* uisupper (0x19e480) — wide-char uppercase classification.
 * Thin wrapper: forwards the incoming character unmodified as the first
 * cdecl argument to the CRT _iswctype(c, 0x0001), where 0x0001 is the MS CRT
 * wctype.h _UPPER mask. Disassembly pushes the mask first, then EAX (the
 * parameter, loaded unmodified from [EBP+8]) last, so the parameter is the
 * first cdecl argument. No MOV EAX after the CALL -- the CRT return value
 * passes through as this function's return. */
int uisupper(int c)
{
  return _iswctype(c, 0x0001);
}

/* uislower (0x19e4a0) — wide-char lowercase classification.
 * Thin wrapper: forwards the incoming character unmodified as the first
 * cdecl argument to the CRT _iswctype(c, 0x0002), where 0x0002 is the MS CRT
 * wctype.h _LOWER mask. Disassembly pushes the mask first, then EAX (the
 * parameter, loaded unmodified from [EBP+8]) last, so the parameter is the
 * first cdecl argument. No MOV EAX after the CALL -- the CRT return value
 * passes through as this function's return. */
int uislower(int c)
{
  return _iswctype(c, 0x0002);
}

/* uisdigit (0x19e4c0) — wide-char decimal-digit classification.
 * Thin wrapper: forwards the incoming character unmodified as the first
 * cdecl argument to the CRT _iswctype(c, 0x0004), where 0x0004 is the MS CRT
 * wctype.h _DIGIT mask. Disassembly pushes the mask first, then EAX (the
 * parameter, loaded unmodified from [EBP+8]) last, so the parameter is the
 * first cdecl argument. No MOV EAX after the CALL -- the CRT return value
 * passes through as this function's return. */
int uisdigit(int c)
{
  return _iswctype(c, 0x0004);
}

/* uisxdigit (0x19e4e0) — wide-char hexadecimal-digit classification.
 * Thin wrapper: forwards the incoming character unmodified as the first
 * cdecl argument to the CRT _iswctype(c, 0x0080), where 0x0080 is the MS CRT
 * wctype.h _HEX mask. Disassembly pushes the mask first, then EAX (the
 * parameter, loaded unmodified from [EBP+8]) last, so the parameter is the
 * first cdecl argument. No MOV EAX after the CALL -- the CRT return value
 * passes through as this function's return. */
int uisxdigit(int c)
{
  return _iswctype(c, 0x0080);
}

/* uisspace (0x19e500) — wide-char whitespace classification.
 * Thin wrapper: forwards the incoming character unmodified as the first
 * cdecl argument to the CRT _iswctype(c, 0x0008), where 0x0008 is the MS CRT
 * wctype.h _SPACE mask. Disassembly pushes the mask first, then EAX (the
 * parameter, loaded unmodified from [EBP+8]) last, so the parameter is the
 * first cdecl argument. No MOV EAX after the CALL -- the CRT return value
 * passes through as this function's return. */
int uisspace(int c)
{
  return _iswctype(c, 0x0008);
}

/* uispunct (0x19e520) — wide-char punctuation classification.
 * Thin wrapper: forwards the incoming character unmodified as the first
 * cdecl argument to the CRT _iswctype(c, 0x0010), where 0x0010 is the MS CRT
 * wctype.h _PUNCT mask. Disassembly pushes the mask first, then EAX (the
 * parameter, loaded unmodified from [EBP+8]) last, so the parameter is the
 * first cdecl argument. No MOV EAX after the CALL -- the CRT return value
 * passes through as this function's return. */
int uispunct(int c)
{
  return _iswctype(c, 0x0010);
}

/* uisalnum (0x19e540) — wide-char alphanumeric classification.
 * Thin wrapper: forwards the incoming character unmodified as the first
 * cdecl argument to the CRT _iswctype(c, 0x0107), where 0x0107 is the MS CRT
 * wctype.h _ALPHA(0x0100) | _DIGIT(0x0004) | _LOWER(0x0002) | _UPPER(0x0001)
 * mask (i.e. the same _ALPHA(0x0103) mask as uisalpha, OR'd with _DIGIT).
 * Disassembly pushes the mask first, then EAX (the parameter, loaded
 * unmodified from [EBP+8]) last, so the parameter is the first cdecl
 * argument. No MOV EAX after the CALL -- the CRT return value passes
 * through as this function's return. */
int uisalnum(int c)
{
  return _iswctype(c, 0x0107);
}

/* uisprint (0x19e560) — wide-char printable classification.
 * Thin wrapper: forwards the incoming character unmodified as the first
 * cdecl argument to the CRT _iswctype(c, 0x0157), where 0x0157 is the MS CRT
 * wctype.h _BLANK(0x0040) | _PUNCT(0x0010) | _ALPHA(0x0100) | _DIGIT(0x0004) |
 * _LOWER(0x0002) | _UPPER(0x0001) mask. Disassembly pushes the mask first,
 * then EAX (the parameter, loaded unmodified from [EBP+8]) last, so the
 * parameter is the first cdecl argument. No MOV EAX after the CALL -- the
 * CRT return value passes through as this function's return. */
int uisprint(int c)
{
  return _iswctype(c, 0x0157);
}

/* uisgraph (0x19e580) — wide-char printable-except-space classification.
 * Thin wrapper: forwards the incoming character unmodified as the first
 * cdecl argument to the CRT _iswctype(c, 0x0117), where 0x0117 is the MS CRT
 * wctype.h _ALPHA(0x0100) | _UPPER(0x0001) | _LOWER(0x0002) | _DIGIT(0x0004) |
 * _PUNCT(0x0010) mask (i.e. the same _ALPHA(0x0103) mask as uisalpha, OR'd
 * with _DIGIT and _PUNCT -- alnum plus punctuation, no _BLANK/_SPACE).
 * Disassembly pushes the mask first, then EAX (the parameter, loaded
 * unmodified from [EBP+8]) last, so the parameter is the first cdecl
 * argument. No MOV EAX after the CALL -- the CRT return value passes
 * through as this function's return. */
int uisgraph(int c)
{
  return _iswctype(c, 0x0117);
}

/* uiscntrl (0x19e5a0) — wide-char control-character classification.
 * Thin wrapper: forwards the incoming character unmodified as the first
 * cdecl argument to the CRT _iswctype(c, 0x0020), where 0x0020 is the MS CRT
 * wctype.h _CONTROL mask. Disassembly pushes the mask first, then EAX (the
 * parameter, loaded unmodified from [EBP+8]) last, so the parameter is the
 * first cdecl argument. No MOV EAX after the CALL -- the CRT return value
 * passes through as this function's return. */
int uiscntrl(int c)
{
  return _iswctype(c, 0x0020);
}

/* utoupper (0x19e5c0) — single wide-char uppercase conversion.
 * Thin wrapper: forwards the incoming character unmodified (a plain dword
 * load from [EBP+8], not a zero-extended 16-bit load) as the sole cdecl
 * argument to FUN_001dc27c. Ghidra's own decompile resolves this callee to
 * the literal name "towupper" -- a bound CRT symbol per its own database,
 * not an inferred neighbor -- but that identity is unconfirmed by a
 * PDB/string anchor, same caveat as the FUN_001dc27c call site in
 * ustrnlwr above. The result is truncated to 16 bits (MOVZX EAX,AX) before
 * returning. */
int utoupper(int character)
{
  return (unsigned short)FUN_001dc27c(character);
}

/* utolower (0x19e5e0) — single wide-char lowercase conversion.
 * Thin wrapper: forwards the incoming character unmodified (a plain dword
 * load from [EBP+8], not a zero-extended 16-bit load) as the sole cdecl
 * argument to FUN_001da8e3. Ghidra's own decompile resolves this callee to
 * the literal name "towlower" -- a bound CRT symbol per its own database,
 * not an inferred neighbor -- but that identity is unconfirmed by a
 * PDB/string anchor, same caveat as the FUN_001da8e3 call site in
 * ustrnupr above. The result is truncated to 16 bits (MOVZX EAX,AX) before
 * returning -- the mirror image of utoupper's FUN_001dc27c call above. */
int utolower(int character)
{
  return (unsigned short)FUN_001da8e3(character);
}

/* ufgetc — wide-char getc. Asserts the caller's "stream" argument is
 * non-null (the literal "stream" is the stringized Bungie parameter name,
 * recovered from the .rdata assert text), then forwards to the CRT's
 * internal wide-getc entry point. */
int ufgetc(void *stream)
{
  assert_halt_at("c:\\halo\\SOURCE\\text\\unicode.c", 0x24d, stream);
  return _fgetwc(stream);
}

/* ufputc — wide-char putc. Asserts the caller's "stream" argument is
 * non-null (the literal "stream" is the stringized Bungie parameter name,
 * recovered from the .rdata assert text), then forwards both arguments,
 * character first, to the CRT's internal wide-putc entry point. The
 * disassembly has no instruction between the CALL and the epilogue, so
 * the callee's EAX return value passes through unmodified as ufputc's
 * return -- a plain `return _fputwc(...)`, matching the ufgetc sibling. */
int ufputc(int character, void *stream)
{
  assert_halt_at("c:\\halo\\SOURCE\\text\\unicode.c", 0x257, stream);
  return _fputwc(character, stream);
}

/* uungetc — wide-char ungetc. Asserts the caller's "stream" argument is
 * non-null (the literal "stream" is the stringized Bungie parameter name,
 * recovered from the .rdata assert text), then forwards both arguments,
 * character first, to the CRT's internal wide-ungetc entry point. The
 * disassembly has no instruction between the CALL and the epilogue, so
 * the callee's EAX return value passes through unmodified as uungetc's
 * return -- a plain `return _ungetwc(...)`, matching the ufgetc/ufputc
 * siblings. */
int uungetc(int character, void *stream)
{
  assert_halt_at("c:\\halo\\SOURCE\\text\\unicode.c", 0x261, stream);
  return _ungetwc(character, stream);
}

/* ufgets (0x19e6c0) — wide-char fgets. Asserts "string" is non-null and
 * wcslen(string) < MAXIMUM_STRING_SIZE (0x8000, same bound as the ustr*
 * helpers elsewhere in this file), and that "size" is also < 0x8000 (a
 * signed compare in the disassembly, unlike the unsigned wcslen check),
 * then forwards all three arguments to the CRT's internal wide-fgets
 * entry point. The disassembly has no instruction between the CALL and
 * the epilogue, so the callee's EAX return value passes through
 * unmodified as ufgets's return -- a plain `return _fgetws(...)`,
 * matching the ufgetc/ufputc/uungetc siblings. */
wchar_t *ufgets(wchar_t *string, int size, void *stream)
{
  unsigned int length;

  assert_halt_at("c:\\halo\\SOURCE\\text\\unicode.c", 0x26c, string);
  length = _wcslen(string);
  assert_halt_msg_at("wcslen(string) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x26d,
                     length < 0x8000);
  assert_halt_msg_at("size < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x26e, size < 0x8000);
  return _fgetws(string, size, stream);
}

/* ufputs (0x19e760) — wide-char fputs. Asserts "string" is non-null,
 * wcslen(string) < MAXIMUM_STRING_SIZE (0x8000 bound, same as ufgets/
 * ustrlen), and "stream" is non-null (all three assert literals recovered
 * from .rdata at this call site), then forwards both arguments to the
 * CRT's internal wide-fputs entry point, string first then stream (cdecl
 * push order: PUSH ESI(stream) then PUSH EDI(string)). The disassembly has
 * no instruction between the CALL and the epilogue that stores into EAX,
 * and ufputs's own decompile is void-returning -- unlike the ufgetc/
 * ufputc/uungetc/ufgets siblings, the callee's return value is discarded
 * here. */
void ufputs(const wchar_t *string, void *stream)
{
  unsigned int length;

  assert_halt_at("c:\\halo\\SOURCE\\text\\unicode.c", 0x278, string);
  length = _wcslen(string);
  assert_halt_msg_at("wcslen(string) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x279,
                     length < 0x8000);
  assert_halt_at("c:\\halo\\SOURCE\\text\\unicode.c", 0x27a, stream);
  _fputws(string, stream);
}

/* ugets (0x19e800) — wide-char gets. Asserts "string" is non-null and
 * wcslen(string) < MAXIMUM_STRING_SIZE (0x8000 bound, same as ufgets/
 * ufputs), then forwards the buffer to the CRT's internal wide-gets entry
 * point. The disassembly has no instruction between the CALL and the
 * epilogue, and ugets's own decompile is void-returning, so the callee's
 * return value is discarded here (unlike ufgets, which forwards it). */
void ugets(wchar_t *string)
{
  unsigned int length;

  assert_halt_at("c:\\halo\\SOURCE\\text\\unicode.c", 0x283, string);
  length = _wcslen(string);
  assert_halt_msg_at("wcslen(string) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x284,
                     length < 0x8000);
  __getws(string);
}

/* uputs (0x19e870) — wide-char puts. Asserts "string" is non-null and
 * wcslen(string) < MAXIMUM_STRING_SIZE (0x8000 bound, same as ufgets/
 * ufputs/ugets), then forwards the buffer to the CRT's internal wide-puts
 * entry point (__putws, 0x1dc914). The disassembly has no instruction
 * between the CALL and the epilogue, and uputs's own decompile is
 * void-returning, so the callee's return value is discarded here (same
 * shape as ugets). __putws's kb.json decl was corrected from `void(void)`
 * to take the wchar_t* argument — the call site pushes ESI (the string)
 * immediately before the CALL and cleans up 4 bytes (ADD ESP,0x4)
 * afterward, i.e. one cdecl argument. */
void uputs(const wchar_t *string)
{
  unsigned int length;

  assert_halt_at("c:\\halo\\SOURCE\\text\\unicode.c", 0x299, string);
  length = _wcslen(string);
  assert_halt_msg_at("wcslen(string) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x29a,
                     length < 0x8000);
  __putws(string);
}

/* ufprintf (0x19e8e0) — wide-char formatted print to a stream. Asserts
 * "stream" and "format" are non-null (assert reasons match the bare
 * parameter names, same as ufputs's stream check), and wcslen(format) <
 * MAXIMUM_STRING_SIZE (0x8000, same bound as the other ufxxx helpers), then
 * forwards the caller's va_list to the CRT's internal wide vfwprintf entry
 * (0x1dc9a3) as (stream, format, argptr) -- the same three-argument shape
 * unicode_sprintf uses for _vsnwprintf, and the CRT's return value is
 * discarded here exactly as ufprintf's own disassembly has no use of EAX
 * after the CALL. */
void ufprintf(void *stream, const wchar_t *format, ...)
{
  va_list args;
  unsigned int length;

  assert_halt_at("c:\\halo\\SOURCE\\text\\unicode.c", 0x2a8, stream);
  assert_halt_at("c:\\halo\\SOURCE\\text\\unicode.c", 0x2a9, format);
  length = _wcslen(format);
  assert_halt_msg_at("wcslen(format) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x2aa,
                     length < 0x8000);

  va_start(args, format);
  _vfwprintf(stream, format, (char *)args);
  va_end(args);
}

/* uprintf (0x19e980) — wide-char formatted print. Asserts "format" is
 * non-null and wcslen(format) < MAXIMUM_STRING_SIZE (0x8000, same bound as
 * the other uxxxprintf helpers), then forwards the caller's va_list to the
 * CRT's internal wide vprintf entry (0x1dca00) as (format, argptr) -- the
 * same shape ufprintf uses for _vfwprintf, minus the stream argument. The
 * CRT's return value is discarded here exactly as uprintf's own disassembly
 * has no use of EAX after the CALL. */
void uprintf(const wchar_t *format, ...)
{
  va_list args;
  unsigned int length;

  assert_halt_at("c:\\halo\\SOURCE\\text\\unicode.c", 0x2bb, format);
  length = _wcslen(format);
  assert_halt_msg_at("wcslen(format) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x2bc,
                     length < 0x8000);

  va_start(args, format);
  _vprintf(format, (char *)args);
  va_end(args);
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

/* usprintf (0x19eaa0) — wide-char formatted print into a caller-supplied
 * buffer. Asserts "string && format" (combined-condition literal, same
 * pattern as ustrcmp's "string1 && string2"), then wcslen(string) <
 * MAXIMUM_STRING_SIZE and wcslen(format) < MAXIMUM_STRING_SIZE (0x8000,
 * same bound as the other uxxxprintf helpers), before forwarding the
 * caller's va_list to FUN_001dcace(string, format, argptr) -- the same
 * three-argument shape ufprintf uses for _vfwprintf. Unlike
 * ufprintf/uprintf/unicode_sprintf, which discard the CRT formatter's
 * return value, usprintf returns it directly: the decompile shows EAX
 * flowing straight through as extraout_EAX with no other use, matching
 * usprintf's own wchar_t* declared return type. FUN_001dcace itself is
 * not yet lifted (kept as an explicit unknown -- no binary evidence at
 * this address names it -- only its call-site ABI is proven here). */
wchar_t *usprintf(wchar_t *string, const wchar_t *format, ...)
{
  va_list args;
  unsigned int length;
  wchar_t *result;

  assert_halt_msg_at("string && format", "c:\\halo\\SOURCE\\text\\unicode.c",
                     0x2ef, string != NULL && format != NULL);
  length = _wcslen(string);
  assert_halt_msg_at("wcslen(string) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x2f0,
                     length < 0x8000);
  length = _wcslen(format);
  assert_halt_msg_at("wcslen(format) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x2f1,
                     length < 0x8000);

  va_start(args, format);
  result = FUN_001dcace(string, format, (char *)args);
  va_end(args);

  return result;
}

/* uvfprintf (0x19eb50) — wide-char formatted print to a stream, taking an
 * already-captured argument-list pointer instead of a variadic tail (the
 * "v" CRT-naming convention, same relationship _vfwprintf has to a variadic
 * vfwprintf). Asserts "stream && format" as a single combined check -- one
 * TEST/JZ then TEST/JNZ pair feeding one display_assert call, the same
 * shape usprintf uses for its "string && format" check, not the two
 * separate display_assert calls ufprintf's split stream/format checks use
 * -- then wcslen(format) < MAXIMUM_STRING_SIZE (0x8000, same bound as every
 * other uxxxprintf helper in this file), and forwards (stream, format,
 * args) straight through to the CRT's internal wide vfwprintf entry
 * (0x1dc9a3) -- the same three-argument shape ufprintf uses for
 * _vfwprintf. The CRT's return value is discarded here exactly as
 * uvfprintf's own disassembly has no use of EAX after the CALL, matching
 * ufprintf/uprintf rather than usprintf. The two assert reason-string
 * literals below follow this file's established stringified-expression
 * convention (every sibling assert names its own condition this way); their
 * exact bytes were not independently read from .rdata for this address, so
 * treat the literal text (not the control flow, which is disassembly-
 * derived) as the one explicit unknown in this lift. */
void uvfprintf(void *stream, const wchar_t *format, char *args)
{
  unsigned int length;

  assert_halt_msg_at("stream && format", "c:\\halo\\SOURCE\\text\\unicode.c",
                     0x318, stream != NULL && format != NULL);
  length = _wcslen(format);
  assert_halt_msg_at("wcslen(format) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x319,
                     length < 0x8000);

  _vfwprintf(stream, format, args);
}

/* uvprintf (0x19ebd0) — wide-char formatted print to the default output,
 * taking an already-captured argument-list pointer instead of a variadic
 * tail (the "v" CRT-naming convention, same relationship uvfprintf has to
 * ufprintf and uvsprintf has to usprintf). Asserts "format" via the plain
 * stringized-condition form (TEST ESI,ESI/JNZ feeding one display_assert
 * call, matching uprintf's own "format" assert shape, not the combined
 * "x && y" single-check form uvfprintf/uvsprintf use since uvprintf has
 * only one pointer parameter to guard), then wcslen(format) <
 * MAXIMUM_STRING_SIZE (0x8000, same bound as every other uxxxprintf helper
 * in this file), and forwards (format, args) straight through to the CRT's
 * internal wide vprintf entry (0x1dca00) -- the same two-argument shape and
 * order uprintf uses for this callee's (format, argptr) parameters: the
 * disassembly pushes EBP+0xc (the already-captured args pointer) then ESI
 * (format) right-to-left before the CALL. The CRT's return value is
 * discarded here exactly as uvprintf's own disassembly has no use of EAX
 * after the CALL, matching uprintf/uvfprintf rather than
 * usprintf/uvsprintf. */
void uvprintf(const wchar_t *format, char *args)
{
  unsigned int length;

  assert_halt_at("c:\\halo\\SOURCE\\text\\unicode.c", 0x323, format);
  length = _wcslen(format);
  assert_halt_msg_at("wcslen(format) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x324,
                     length < 0x8000);

  _vprintf(format, args);
}

/* uvsnprintf (0x19ec40) — bounded wide-char formatted print into a
 * caller-supplied buffer, taking an already-captured argument-list pointer
 * instead of a variadic tail (the "v" CRT-naming convention, same
 * relationship uvprintf has to uprintf and uvsprintf has to usprintf).
 * Asserts "string && format" as a single combined check -- one TEST/JZ then
 * TEST/JNZ pair feeding one display_assert call, the same shape
 * usprintf/uvfprintf/uvsprintf use for this combined check -- then
 * wcslen(string) < MAXIMUM_STRING_SIZE and wcslen(format) <
 * MAXIMUM_STRING_SIZE (0x8000, same bound as every other uxxxprintf helper
 * in this file), before forwarding (string, count, format, args) straight
 * through to the CRT's _vsnwprintf entry (0x1dca5f). The disassembly pushes
 * EBP+0x14 (args) then EDI (format, EBP+0x10) then ECX (count, EBP+0xc)
 * then ESI (string, EBP+8) right-to-left before the CALL, matching
 * _vsnwprintf's (buffer, count, format, argptr) parameter order exactly.
 * Unlike usprintf/uvsprintf, which return the CRT formatter's result
 * directly, uvsnprintf discards it here exactly as its own disassembly has
 * no use of EAX after the CALL, matching uvfprintf/uvprintf/uprintf/
 * ufprintf rather than usprintf/uvsprintf. The count parameter itself is
 * forwarded unchecked -- only string and format are asserted non-null and
 * length-bounded. */
void uvsnprintf(wchar_t *string, unsigned int count, const wchar_t *format,
                char *args)
{
  unsigned int length;

  assert_halt_msg_at("string && format", "c:\\halo\\SOURCE\\text\\unicode.c",
                     0x330, string != NULL && format != NULL);
  length = _wcslen(string);
  assert_halt_msg_at("wcslen(string) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x331,
                     length < 0x8000);
  length = _wcslen(format);
  assert_halt_msg_at("wcslen(format) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x332,
                     length < 0x8000);

  _vsnwprintf(string, count, format, args);
}

/* uvsprintf (0x19ecf0) — wide-char formatted print into a caller-supplied
 * buffer, taking an already-captured argument-list pointer instead of a
 * variadic tail (the "v" CRT-naming convention, same relationship uvfprintf
 * has to ufprintf and this file's other uvxxx/uxxx pairs). Asserts
 * "string && format" as a single combined check -- one TEST/JZ then
 * TEST/JNZ pair feeding one display_assert call, the same shape usprintf
 * uses for its own "string && format" check -- then wcslen(string) <
 * MAXIMUM_STRING_SIZE and wcslen(format) < MAXIMUM_STRING_SIZE (0x8000,
 * same bound as every other uxxxprintf helper in this file), before
 * forwarding (string, format, args) straight through to
 * FUN_001dcace(dst, format, argptr) -- the same three-argument shape and
 * argument order usprintf uses for this callee. Like usprintf (and unlike
 * uvfprintf, which discards the CRT return value), uvsprintf returns it
 * directly: the disassembly leaves EAX untouched between the CALL and the
 * epilogue, so FUN_001dcace's return value flows straight through as
 * uvsprintf's own wchar_t* return, matching usprintf's declared return
 * type for the same callee. */
wchar_t *uvsprintf(wchar_t *string, const wchar_t *format, char *args)
{
  unsigned int length;
  wchar_t *result;

  assert_halt_msg_at("string && format", "c:\\halo\\SOURCE\\text\\unicode.c",
                     0x349, string != NULL && format != NULL);
  length = _wcslen(string);
  assert_halt_msg_at("wcslen(string) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x34a,
                     length < 0x8000);
  length = _wcslen(format);
  assert_halt_msg_at("wcslen(format) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x34b,
                     length < 0x8000);

  result = FUN_001dcace(string, format, args);

  return result;
}

/* ufdopen (0x19eda0) — wide-string wrapper around a two-argument CRT wide
 * file-descriptor-open helper. Halts if path is NULL (assert reason "path",
 * a bare-pointer condition matching the recovered .rdata literal -- same
 * single-symbol shape ustrlen/ustrnlen/ustrchr use for their own NULL
 * checks), if wcslen(path) is not < 0x8000 (assert text names
 * MAXIMUM_STRING_SIZE, but the compiled immediate is 0x8000, same
 * bound/discrepancy as every other uxxx helper in this file), or if fd is
 * not > 0 (assert text "fd > 0"). Forwards (fd, path) straight through to
 * FUN_001dcb6c (LIBCMT:vsnwprint.obj) in that order (PUSH EDI/path then PUSH
 * ESI/fd immediately before the CALL -- cdecl's last-pushed-is-first-arg
 * puts fd first) and discards its return value: the disassembly leaves EAX
 * untouched between the CALL and the epilogue (ADD ESP,0x8/POP EDI/POP
 * ESI/POP EBP/RET), matching this file's declared-void discard convention
 * for ufopen/_wcsncat/_vfwprintf. */
void ufdopen(int fd, wchar_t *path)
{
  unsigned int length;

  assert_halt_at("c:\\halo\\SOURCE\\text\\unicode.c", 0x36d, path);
  length = _wcslen(path);
  assert_halt_msg_at("wcslen(path) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x36e,
                     length < 0x8000);
  assert_halt_at("c:\\halo\\SOURCE\\text\\unicode.c", 0x36f, fd > 0);

  FUN_001dcb6c(fd, path);
}

/* ufopen (0x19ee40) — wide-string wrapper around a two-argument CRT wide
 * file-open helper. Halts if path or mode is NULL (combined "path && mode"
 * assert -- one TEST/JZ then TEST/JNZ pair feeding a single display_assert,
 * the same shape this file's other combined-NULL-check helpers use), if
 * wcslen(path) is not < 0x8000 (assert text names MAXIMUM_STRING_SIZE, but
 * the compiled immediate is 0x8000, same bound/discrepancy as every other
 * uxxx helper in this file), or if wcslen(mode) is not < 4 (assert text
 * "wcslen(mode) < MAXIMUM_MODE_STRING_SIZE", CMP EAX,0x4/JC -- no
 * MAXIMUM_MODE_STRING_SIZE macro exists in this codebase, so the literal 4
 * is used directly, matching this file's convention of using the compiled
 * immediate rather than an unrelated same-named macro). Forwards (path,
 * mode) straight through to FUN_001dccf5 (LIBCMT:vsnwprint.obj) in that
 * push order (PUSH EDI/mode then PUSH ESI/path immediately before the
 * CALL -- cdecl's last-pushed-is-first-arg puts path first) and discards
 * its return value: the disassembly leaves EAX untouched between the CALL
 * and the epilogue (ADD ESP,0x8/POP EDI/POP ESI/POP EBP/RET), matching this
 * file's declared-void discard convention for _wcsncat/_vfwprintf. */
void ufopen(wchar_t *path, wchar_t *mode)
{
  unsigned int length;

  assert_halt_msg_at("path && mode", "c:\\halo\\SOURCE\\text\\unicode.c", 0x379,
                     path != NULL && mode != NULL);
  length = _wcslen(path);
  assert_halt_msg_at("wcslen(path) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x37a,
                     length < 0x8000);
  length = _wcslen(mode);
  assert_halt_msg_at("wcslen(mode) < MAXIMUM_MODE_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x37b, length < 4);

  FUN_001dccf5(path, mode);
}

/* ufclose (0x19eee0) — wide-char wrapper around the CRT's fclose. Halts if
 * stream is NULL (assert text "stream"), then forwards straight to
 * crt_fclose and discards its return value: the disassembly leaves EAX
 * untouched between the CALL and the epilogue (ADD ESP,0x4/POP ESI/POP
 * EBP/RET), matching this file's declared-void discard convention for
 * ufopen/_vfwprintf. */
void ufclose(void *stream)
{
  assert_halt_msg_at("stream", "c:\\halo\\SOURCE\\text\\unicode.c", 0x384,
                     stream != NULL);

  crt_fclose(stream);
}

/* ufreopen (0x19ef20) — wide-string wrapper around a three-argument CRT wide
 * file-reopen helper. Halts if path or mode is NULL (combined "path && mode"
 * assert -- one TEST/JZ then TEST/JNZ pair feeding a single display_assert,
 * the same shape ufopen's combined-NULL-check uses), if wcslen(path) is not
 * < 0x8000 (assert text names MAXIMUM_STRING_SIZE, but the compiled
 * immediate is 0x8000, same bound/discrepancy as every other uxxx helper in
 * this file), or if wcslen(mode) is not < 4 (assert text "wcslen(mode) <
 * MAXIMUM_MODE_STRING_SIZE" -- no such macro exists in this codebase, so the
 * literal 4 is used directly, matching ufopen's convention of using the
 * compiled immediate rather than an unrelated same-named macro). Forwards
 * (path, mode, stream) straight through to FUN_001dcd08
 * (LIBCMT:vsnwprint.obj) in that order (PUSH EAX/stream, PUSH EDI/mode, PUSH
 * ESI/path immediately before the CALL -- cdecl's last-pushed-is-first-arg
 * puts path first, then mode, then stream) and discards its return value:
 * the disassembly leaves EAX untouched between the CALL and the epilogue
 * (ADD ESP,0xc/POP EDI/POP ESI/POP EBP/RET), matching this file's
 * declared-void discard convention for ufopen/ufdopen. */
void ufreopen(wchar_t *path, wchar_t *mode, void *stream)
{
  unsigned int length;

  assert_halt_msg_at("path && mode", "c:\\halo\\SOURCE\\text\\unicode.c", 0x38f,
                     path != NULL && mode != NULL);
  length = _wcslen(path);
  assert_halt_msg_at("wcslen(path) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x390,
                     length < 0x8000);
  length = _wcslen(mode);
  assert_halt_msg_at("wcslen(mode) < MAXIMUM_MODE_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x391, length < 4);

  __wfreopen(path, mode, stream);
}

/* uperror (0x19efd0) — wide-string wrapper around a CRT perror-style helper.
 * Halts if string is NULL (assert text "string", same simple-pointer-check
 * shape as ustrupr/ustrlwr), or if wcslen(string) is not < 0x8000 (assert
 * text names MAXIMUM_STRING_SIZE, compiled immediate is 0x8000, same
 * bound/discrepancy as every other uxxx helper in this file). Forwards
 * string straight through to FUN_001dcd6e and discards its return value:
 * the disassembly leaves EAX untouched between the CALL and the epilogue
 * (ADD ESP,0x4/POP ESI/POP EBP/RET), matching this file's declared-void
 * discard convention. */
void uperror(wchar_t *string)
{
  unsigned int length;

  assert_halt_msg_at("string", "c:\\halo\\SOURCE\\text\\unicode.c", 0x39a,
                     string != NULL);
  length = _wcslen(string);
  assert_halt_msg_at("wcslen(string) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x39b,
                     length < 0x8000);

  FUN_001dcd6e(string);
}

/* upopen (0x19f040) — validate-only wide-string front end for a
 * popen-style command/mode pair. Halts if command or mode is NULL
 * (combined "command && mode" assert -- one TEST/JZ then TEST/JNZ pair
 * feeding a single display_assert, same short-circuit shape as
 * ufopen/ufreopen's combined-NULL-check: command==NULL jumps straight to
 * the halt without ever testing mode), if wcslen(command) is not < 0x8000
 * (assert text names MAXIMUM_STRING_SIZE, compiled immediate 0x8000, same
 * bound/discrepancy as every other uxxx helper in this file), or if
 * wcslen(mode) is not < 4 (assert text "wcslen(mode) <
 * MAXIMUM_MODE_STRING_SIZE", CMP EAX,0x4/JC -- no such macro exists in
 * this codebase, same convention as ufopen/ufreopen). After the three
 * checks pass the function returns 0 directly (XOR EAX,EAX before the
 * epilogue) -- unlike ufopen/ufdopen/uremove there is no CRT forwarding
 * call anywhere in the disassembly; this is the whole function. */
int upopen(wchar_t *command, wchar_t *mode)
{
  unsigned int length;

  assert_halt_msg_at("command && mode", "c:\\halo\\SOURCE\\text\\unicode.c",
                     0x3a7, command != NULL && mode != NULL);
  length = _wcslen(command);
  assert_halt_msg_at("wcslen(command) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x3a8,
                     length < 0x8000);
  length = _wcslen(mode);
  assert_halt_msg_at("wcslen(mode) < MAXIMUM_MODE_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x3a9, length < 4);

  return 0;
}

/* uremove (0x19f0e0) — wide-string wrapper around a one-argument CRT
 * wide file-remove helper. Halts if path is NULL (assert text "path",
 * same bare-pointer-check shape as ufdopen's first assert), or if
 * wcslen(path) is not < 0x8000 (assert text names MAXIMUM_STRING_SIZE,
 * compiled immediate is 0x8000, same bound/discrepancy as every other
 * uxxx helper in this file). Forwards path straight through to
 * FUN_001dce6e (LIBCMT:vsnwprint.obj, single PUSH ESI before the CALL,
 * ADD ESP,0x4 after -- one stack arg) and discards its return value:
 * the disassembly leaves EAX untouched between the CALL and the
 * epilogue (ADD ESP,0x4/POP ESI/POP EBP/RET), matching this file's
 * declared-void discard convention for ufopen/ufdopen/uperror. */
void uremove(wchar_t *path)
{
  unsigned int length;

  assert_halt_at("c:\\halo\\SOURCE\\text\\unicode.c", 0x3b2, path);
  length = _wcslen(path);
  assert_halt_msg_at("wcslen(path) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x3b3,
                     length < 0x8000);

  FUN_001dce6e(path);
}

/* utmpnam (0x19f150) — bare tail-call wrapper around the CRT wide
 * temporary-name generator, no arguments, no assert, no locals. The
 * disassembly is the whole function: PUSH EBP/MOV EBP,ESP/POP EBP/JMP
 * 0x1dcf51 (__wtmpnam, LIBCMT:vsnwprint.obj) -- the original compiler
 * folded this into a tail call (JMP, not CALL+RET) since there is
 * nothing to do after the callee returns. Unlike uremove/ustrtol there
 * is no path/length validation of any kind; this is a pure passthrough. */
void utmpnam(void)
{
  __wtmpnam();
}

/* ustrtol (0x19f160) — wide-string wrapper around the CRT wide
 * string-to-long converter. Halts if nptr is NULL (assert reason "nptr",
 * bare-pointer-check shape as uremove's "path" assert), or if wcslen(nptr)
 * is not < 0x8000 (assert text names MAXIMUM_STRING_SIZE, compiled
 * immediate is 0x8000, same bound/discrepancy as every other uxxx helper
 * in this file). Forwards nptr, endptr, and base straight through to
 * FUN_001dd1d1 (LIBCMT-merged CRT wcstol core, same stack order as this
 * function's own parameters -- PUSH EAX(base)/PUSH ECX(endptr)/PUSH
 * ESI(nptr) before the CALL) and returns its result unmodified: the
 * disassembly leaves EAX untouched between the CALL and the epilogue
 * (ADD ESP,0xc/POP ESI/POP EBP/RET). */
long ustrtol(const wchar_t *nptr, wchar_t **endptr, int base)
{
  unsigned int length;

  assert_halt_at("c:\\halo\\SOURCE\\text\\unicode.c", 0x3c7, nptr);
  length = _wcslen(nptr);
  assert_halt_msg_at("wcslen(nptr) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x3c8,
                     length < 0x8000);

  return FUN_001dd1d1(nptr, endptr, base);
}

/* ustrtoul (0x19f1d0) — wide-string wrapper around the CRT wide
 * string-to-unsigned-long converter. Halts if nptr is NULL (assert reason
 * "nptr", same bare-pointer-check shape as ustrtol's own "nptr" assert), or
 * if wcslen(nptr) is not < 0x8000 (assert text names MAXIMUM_STRING_SIZE,
 * compiled immediate is 0x8000, same bound as every other uxxx helper in
 * this file). Forwards nptr, endptr, and base straight through to
 * FUN_001dd1e8 (LIBCMT-merged CRT wcstoul core immediately following
 * ustrtol's FUN_001dd1d1 wcstol core in stricmp.obj; same stack order as
 * ustrtol's call -- PUSH EAX(base)/PUSH ECX(endptr)/PUSH ESI(nptr) before
 * the CALL) and returns its result unmodified: the disassembly leaves EAX
 * untouched between the CALL and the epilogue (ADD ESP,0xc/POP ESI/POP
 * EBP/RET). */
unsigned long ustrtoul(const wchar_t *nptr, wchar_t **endptr, int base)
{
  unsigned int length;

  assert_halt_at("c:\\halo\\SOURCE\\text\\unicode.c", 0x3d3, nptr);
  length = _wcslen(nptr);
  assert_halt_msg_at("wcslen(nptr) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x3d4,
                     length < 0x8000);

  return FUN_001dd1e8(nptr, endptr, base);
}

/* ustrtod (0x19f240) — wide-string wrapper around the CRT wide
 * string-to-double converter. Halts if nptr is NULL (assert reason "nptr",
 * bare-pointer-check shape as ustrtol's own "nptr" assert), or if
 * wcslen(nptr) is not < 0x8000 (assert text names MAXIMUM_STRING_SIZE,
 * compiled immediate is 0x8000, same bound as every other uxxx helper in
 * this file). Forwards nptr and endptr straight through to FUN_001dd1ff
 * (LIBCMT:stricmp.obj CRT wcstod core, same stack order as this function's
 * own parameters -- PUSH EAX(endptr)/PUSH ESI(nptr) before the CALL, ADD
 * ESP,0x8 after) and returns its result unmodified: the double return value
 * is left in ST(0) by the callee and the disassembly does not touch it
 * between the CALL and the epilogue (POP ESI/POP EBP/RET). */
double ustrtod(const wchar_t *nptr, wchar_t **endptr)
{
  unsigned int length;

  assert_halt_at("c:\\halo\\SOURCE\\text\\unicode.c", 0x3de, nptr);
  length = _wcslen(nptr);
  assert_halt_msg_at("wcslen(nptr) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x3df,
                     length < 0x8000);

  return FUN_001dd1ff(nptr, endptr);
}

/* uatoi (0x19f2b0) — wide-string wrapper around the CRT wide
 * string-to-integer converter. Halts if string is NULL (assert reason
 * "string", bare-pointer-check shape shared with ustrtol/ustrtod's own
 * "nptr" assert), or if wcslen(string) is not < MAXIMUM_STRING_SIZE
 * (0x8000), same bound as every other uxxx helper in this file. Forwards
 * string straight through to FUN_001dd3d4 (LIBCMT:stricmp.obj CRT
 * wide-string-to-long core, single PUSH ESI before the CALL, ADD ESP,0x4
 * after) and returns its result unmodified: EAX is left untouched between
 * the CALL and the epilogue (POP ESI/POP EBP/RET). */
int uatoi(const wchar_t *string)
{
  unsigned int length;

  assert_halt_at("c:\\halo\\SOURCE\\text\\unicode.c", 0x3ea, string);
  length = _wcslen(string);
  assert_halt_msg_at("wcslen(string) < MAXIMUM_STRING_SIZE",
                     "c:\\halo\\SOURCE\\text\\unicode.c", 0x3eb,
                     length < 0x8000);

  return (int)FUN_001dd3d4(string);
}

/* uctime (0x19f320) — wide-char ctime wrapper. Halts if timer is NULL
 * (assert reason "timer", bare-pointer-check shape shared with
 * uatoi/uasctime's own asserts), then tail-calls the CRT wide ctime core
 * __wctime (LIBCMT:stricmp.obj, same TU as uasctime's __wasctime) and
 * returns its result unmodified: EAX is left untouched between the CALL and
 * the epilogue (POP ESI/POP EBP/RET), single PUSH ESI before the CALL,
 * ADD ESP,0x4 after. timer's pointee (time_t) is never dereferenced by this
 * function itself, so it stays an opaque pointer — no binary evidence for
 * its layout in this TU. */
wchar_t *uctime(const void *timer)
{
  assert_halt_at("c:\\halo\\SOURCE\\text\\unicode.c", 0x3f6, timer);
  return __wctime(timer);
}

/* uasctime (0x19f360) — wide-char asctime wrapper. Halts if timeptr is NULL
 * (assert reason "timeptr", bare-pointer-check shape shared with
 * uatoi/ustrtod's own asserts), then tail-calls the CRT wide asctime core
 * __wasctime (LIBCMT:stricmp.obj, same TU as uatoi's FUN_001dd3d4) and
 * returns its result unmodified: EAX is left untouched between the CALL and
 * the epilogue (POP ESI/POP EBP/RET), single PUSH ESI before the CALL,
 * ADD ESP,0x4 after. timeptr's pointee (struct tm) is never dereferenced by
 * this function itself, so it stays an opaque pointer — no binary evidence
 * for its layout in this TU. */
wchar_t *uasctime(const void *timeptr)
{
  assert_halt_at("c:\\halo\\SOURCE\\text\\unicode.c", 0x3ff, timeptr);
  return __wasctime(timeptr);
}

/* wide_to_ascii — convert a wide string to an ASCII byte string.
 * Returns NULL if the string won't fit in the buffer or contains
 * any non-ASCII characters (code points >= 0x80). Otherwise copies
 * the low byte of each wide character and null-terminates the result. */
char *wide_to_ascii(const wchar_t *unicode, char *ascii, int size)
{
  unsigned int length;
  unsigned int i;

  assert_halt(unicode && ascii);
  length = _wcslen(unicode);
  assert_halt(length < 0x8000);

  if (length > (unsigned int)(size - 1))
    return NULL;

  for (i = 0; i < length; i++) {
    if ((unicode[i] & 0xFF80) != 0)
      return NULL;
  }

  for (i = 0; i < length; i++) {
    ascii[i] = (char)unicode[i];
  }

  ascii[i] = '\0';
  return ascii;
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

/* FUN_0019f4f0 (0x19f4f0) -- format the CRT strerror(errnum) message into
 * a wide-char scratch buffer and return it.
 *
 * Confirmed from disassembly: the incoming int param is pushed as the sole
 * arg to strerror (0x1dd576, LIBCMT -- decompiler resolves this call by
 * import name), whose char* return value is forwarded as the varargs
 * operand of unicode_sprintf(buffer, 0x100, L"%hs", errstr). The two CALLs
 * share one coalesced ADD ESP,0x14 cleanup (5 dwords: the leftover
 * strerror arg plus the 4 unicode_sprintf args) -- normal cdecl stack
 * coalescing, not a dropped push.
 *
 * DAT_004d9be8 is a 0x100-wchar_t static scratch buffer; its identity is
 * unconfirmed (no name/xref evidence), so it is addressed by raw literal
 * VA like other un-named globals in this codebase (e.g. units.c
 * 0x5ac9ca) rather than given an invented name. */
wchar_t *FUN_0019f4f0(int errnum)
{
  *(wchar_t *)0x4d9be8 = 0;
  unicode_sprintf((wchar_t *)0x4d9be8, 0x100, L"%hs", strerror(errnum));
  return (wchar_t *)0x4d9be8;
}

/* FUN_0019f530 (0x19f530) — returns constant 0x14; parameter is unused. */
int FUN_0019f530(int unit_handle)
{
  (void)unit_handle;
  return 0x14;
}
