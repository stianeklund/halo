# SEH Handling in Lifted Code

## Background

The original binary uses MSVC 7.1's *compact SEH* form: `__SEH_prolog` (0x1dd5c8)
and `__SEH_epilog` (0x1dd601) are runtime thunks that restructure the stack frame
to install a per-function SEH record in `FS:[0]`.  Ghidra surfaces these as
ordinary function calls, but they have a non-standard ABI and cannot be called
from lifted C code.

Approximately 74 functions in the binary use this pattern — all from the LIBCMT
and XAPILIB static libraries, not game-logic TUs.

## Approach: `__try/__except` with clang

Clang's `-target i386-pc-win32` target supports `__try/__except` natively without
any extra flags (ms-extensions are implied for Windows targets).  We use this to
preserve the *semantics* of the original SEH:

```c
/* FUN_001d789a — XAPI guarded strncpy, 101 bytes, __stdcall */
char * __stdcall FUN_001d789a(char *dst, const char *src, int count)
{
    __try {
        char *d = dst;
        const char *s = src;
        if (count != 0) {
            while (count != 0) {
                if (*s == '\0') { if (count != 0) goto done_null; break; }
                *d++ = *s++;
                count--;
            }
            d--;
done_null:
            *d = '\0';
        }
    }
    __except(1) {   /* EXCEPTION_EXECUTE_HANDLER — catch all AV faults */
        return 0;
    }
    return dst;
}
```

## Frame shape mismatch

Clang emits an inline SEH frame (`PUSH -1; PUSH table; PUSH handler; MOV FS:[0],ESP`),
whereas the original uses the two-instruction compact form (`PUSH N; PUSH table;
CALL __SEH_prolog`).  VC71 `__try/__except` also emits an inline frame, so
`vc71_verify` reports ~55% match — the mismatch is entirely in the 7-instruction
frame prologue/epilogue, not in the function body.  The happy-path logic is a
100% behavioral match.

**This is acceptable for CRT/XAPI safety-net wrappers.** The SEH is purely
defensive; in normal game execution the `__try` body runs to completion without
faulting.

## Scaling to all 74 functions

All 74 callers are in LIBCMT/XAPILIB (C runtime and Xbox API wrappers), identified
via `mcp__ghidra__get_function_callers(0x1dd5c8)`.  They fall into two patterns:

1. **Simple guard** — filter returns 1, handler restores ESP and returns 0/NULL.
   Lift with `__try { ... } __except(1) { return 0; }`.
2. **Cleanup on fault** — handler unlocks a mutex or closes a file before returning.
   Lift with `__try { ...; __lock(); ... } __except(1) { __unlock(); return 0; }`.

Estimated effort: ~2 hours for the remaining 73 (30 min RE + 90 min implementation),
since each function is a well-typed CRT primitive.  The main blocker is that
`vc71_verify` will never reach >60% on these because the frame shape is
structurally different — this should be documented as an expected SEH exception
in the verify policy.
