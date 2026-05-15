/*
 * xbox_crt.c — LIBCMT/XAPI CRT helper replacements
 *
 * These are compact SEH functions from the original LIBCMT/XAPI libraries.
 * The originals use __SEH_prolog/__SEH_epilog (compact MSVC SEH thunks).
 * We replace them with __try/__except using clang's built-in SEH support
 * (-target i386-pc-win32 implies ms-extensions; __try/__except compiles
 * without -fms-extensions on Windows targets).
 *
 * The SEH frame shape differs from the MSVC compact form, but semantics
 * are preserved: the __try body matches the original logic exactly; the
 * __except handler catches access violations on bad pointer arguments.
 *
 * See docs/seh-handling.md for the full design rationale.
 */

/*
 * FUN_001d789a  (XAPI strncpy helper, 101 bytes)
 *
 * Copies up to `count` characters from `src` to `dst`.  Stops at NUL or
 * when `count` is exhausted.  If `count` is non-zero on entry and the loop
 * exits with chars remaining (src ran short), the last written byte is
 * backed up by one and overwritten with NUL — matching the original loop
 * shape.  Returns `dst`.  On access violation returns NULL.
 *
 * Confirmed:
 *   - __stdcall: RET 0xC pops 3 args (dst@[EBP+8], src@[EBP+C],
 *     count@[EBP+10])
 *   - SEH table at 0x2c1f28: filter = 0x1d78ea (XOR EAX,EAX;INC EAX;RET
 *     → EXCEPTION_EXECUTE_HANDLER=1), handler restores ESP and returns 0
 *   - Loop uses EBX=0 sentinel (XOR EBX,EBX), ECX=src ptr, EAX=dst ptr
 *   - Tracks both ptr copies in the SEH frame ([EBP-0x1c]/[EBP-0x20])
 *
 * Inferred:
 *   - Called from XapiMapLetterToDirectory, XLaunchNewImageA — XAPI layer
 *   - Function is a safe strncpy variant with access-violation guard
 */
char *__stdcall FUN_001d789a(char *dst, const char *src, int count)
{
  __try {
    char *d;
    const char *s;
    d = dst;
    s = src;
    if (count != 0) {
      while (count != 0) {
        if (*s == '\0') {
          if (count != 0)
            goto done_null;
          break;
        }
        *d = *s;
        d++;
        s++;
        count--;
      }
      d--;
    done_null:
      *d = '\0';
    }
  } __except (1) {
    return 0;
  }
  return dst;
}

/* strncmp (CRT stub) — provided here since _strncmp is internal to the original
 * LIBCMT and not exported from the XBE import table. Behaviorally equivalent to
 * the standard strncmp; called only by csstrncmp in cseries.c. */
int strncmp(const char *s1, const char *s2, unsigned int n)
{
  unsigned int i;
  unsigned char c1;
  unsigned char c2;

  for (i = 0; i < n; i++) {
    c1 = (unsigned char)s1[i];
    c2 = (unsigned char)s2[i];
    if (c1 != c2)
      return c1 < c2 ? -1 : 1;
    if (c1 == '\0')
      return 0;
  }
  return 0;
}
