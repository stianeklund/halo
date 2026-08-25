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

/* Formats CRT strerror output into the shared wide-character buffer. */
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
