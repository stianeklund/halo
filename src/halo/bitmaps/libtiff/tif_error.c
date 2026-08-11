/* ===========================================================================
 * tif_error.c -- vendored libtiff error reporting.
 *
 * FUN_000689c0 is upstream libtiff 3.x `TIFFDefaultErrorHandler` transcribed
 * verbatim from tif_error.c rather than reshaped from the decompiler (the
 * vendored-library rule: for a public library, the upstream text is better
 * evidence of the original source form than Ghidra output). The upstream
 * body is:
 *
 *     static void
 *     TIFFDefaultErrorHandler(const char* module, const char* fmt, va_list ap)
 *     {
 *             if (module != NULL)
 *                     fprintf(stderr, "%s: ", module);
 *             vfprintf(stderr, fmt, ap);
 *             fprintf(stderr, ".\n");
 *     }
 *
 * Identified as the ERROR handler, not the warning handler: tif_warning.c's
 * `TIFFDefaultWarningHandler` emits an extra "Warning, " literal before the
 * format, and no such push exists here. `TIFFSetErrorHandler` at 0x68a10 is
 * now ported below; `TIFFError` at 0x68a30 -- the third resident, which reads
 * the same `_TIFFerrorHandler` slot and calls through it -- is not yet ported.
 *
 * Disassembly of 0x689c0 (pristine cachebeta.xbe, capstone), which the
 * transcription is checked against instruction for instruction:
 *
 *   0689c0  push ebp                       ; no _chkstk, no locals, no
 *   0689c1  mov  ebp, esp                  ; `sub esp` -- frame is params only
 *   0689c3  mov  eax, [ebp+8]              ; module
 *   0689c6  test eax, eax
 *   0689c8  je   0x689dd                   ; if (module != NULL)
 *   0689ca  push eax                       ;   arg3 module
 *   0689cb  push 0x259f68                  ;   arg2 "%s: "
 *   0689d0  push 0x331070                  ;   arg1 stderr
 *   0689d5  call 0x1d98ad                  ;   fprintf
 *   0689da  add  esp, 0xc                  ;   own cleanup, inside the guard
 *   0689dd  mov  eax, [ebp+0x10]           ; ap
 *   0689e0  mov  ecx, [ebp+0xc]            ; fmt
 *   0689e3  push eax                       ; arg3 ap
 *   0689e4  push ecx                       ; arg2 fmt
 *   0689e5  push 0x331070                  ; arg1 stderr
 *   0689ea  call 0x1d9850                  ; vfprintf
 *   0689ef  push 0x260020                  ; arg2 ".\n"
 *   0689f4  push 0x331070                  ; arg1 stderr
 *   0689f9  call 0x1d98ad                  ; fprintf
 *   0689fe  add  esp, 0x14                 ; COALESCED cleanup for the last
 *                                          ; two calls (0xc + 0x8), NOT five
 *                                          ; arguments to the trailing
 *                                          ; fprintf -- the decompiler
 *                                          ; mis-attributes the surplus and
 *                                          ; invents three extra arguments
 *   068a01  pop  ebp
 *   068a02  ret                            ; cdecl, caller cleans; no EAX set
 *                                          ; on exit => void return, and both
 *                                          ; fprintf results are discarded
 *                                          ; exactly as upstream discards them
 *
 * String literals read out of the pristine image: 0x259f68 = "%s: ",
 * 0x260020 = ".\n". They are written as literals here so the compiler pools
 * them normally instead of hard-casting their addresses.
 * ======================================================================== */

/* CRT `stderr`. MSVC's `stderr` expands to `&_iob[2]`, so the argument is the
 * ADDRESS of the FILE record, which is why 0x331070 is pushed as an immediate
 * at 0x689d0/0x689e5/0x689f4 rather than loaded from memory. kb.json carries
 * this slot under the zlib-side name `z_stderr`; the literal is used here
 * because what is passed is the record's address, not a pointer value stored
 * inside it. Same form as the other vendored TUs (real_math.c/zlib). */
#define crt_stderr ((void *)0x331070)

/* `_TIFFerrorHandler`, upstream libtiff's module-global error-handler slot.
 * Its address is PROVEN to be 0x2ca1f4 and its identity as the ERROR (not
 * warning) slot is proven by its static initializer: the pristine image's
 * .data holds `c0 89 06 00` at 0x2ca1f4, i.e. 0x000689c0 -- the address of
 * `TIFFDefaultErrorHandler` in this very TU, exactly as upstream's
 *
 *     TIFFErrorHandler _TIFFerrorHandler = TIFFDefaultErrorHandler;
 *
 * writes it. That initializer also fixes the slot's real type: a
 * `void (*)(const char *module, const char *fmt, char *ap)`, the signature of
 * FUN_000689c0 above. It is nevertheless declared `void *` here because
 * neither resident of this TU that touches the slot ever calls through it
 * -- 0x68a10 only swaps the raw word in and out -- so an opaque pointer is
 * the narrowest type the code actually requires, and it keeps the setter free
 * of function-pointer/`void *` conversions that C89 does not guarantee.
 * `TIFFError` at 0x68a30 (`mov eax,[0x2ca1f4] / test eax,eax / call eax`) is
 * the one that will need the function-pointer type; it belongs with that lift.
 * Same file-scope address-macro form the other vendored libtiff TUs use for
 * their statics (see tif_open.c's `photometric`/`BWmap`). */
#define _TIFFerrorHandler (*(void **)0x2ca1f4)

/* `va_list` is `char *` on i386 MSVC 7.1, which is how kb.json declares the
 * third parameter of both this function and the vfprintf it forwards to. `ap`
 * must be forwarded as an opaque pointer -- routing it through a `...` slot
 * would push the pointer as a single ordinary argument and print garbage. */
void FUN_000689c0(const char *module, const char *fmt, char *ap)
{
  if (module != NULL)
    crt_fprintf(crt_stderr, "%s: ", module);
  FUN_001d9850(crt_stderr, fmt, ap); /* vfprintf */
  crt_fprintf(crt_stderr, ".\n");
}

/**
 * Install a new error handler and hand back the one it displaced.
 *
 * Upstream libtiff tif_error.c's `TIFFSetErrorHandler`, transcribed verbatim
 * rather than reshaped from the decompiler (the vendored-library rule):
 *
 *     TIFFErrorHandler
 *     TIFFSetErrorHandler(TIFFErrorHandler handler)
 *     {
 *             TIFFErrorHandler prev = _TIFFerrorHandler;
 *             _TIFFerrorHandler = handler;
 *             return (prev);
 *     }
 *
 * Disassembly of 0x68a10 (7 instructions, 0x68a10-0x68a22):
 *
 *   068a10  push ebp                  ; no `sub esp`, no locals, no _chkstk --
 *   068a11  mov  ebp, esp             ; frame is the one parameter and nothing
 *   068a13  mov  ecx, [ebp+8]         ; else; `handler` staged in ECX first
 *   068a16  mov  eax, [0x2ca1f4]      ; OLD value -> EAX == the return value
 *   068a1b  mov  [0x2ca1f4], ecx      ; ...then the new one is stored
 *   068a21  pop  ebp
 *   068a22  ret                       ; no immediate => cdecl, caller cleans
 *
 * Two things the decompiler got wrong, both load-bearing, and both the same
 * failures already recorded for this object's other residents:
 *
 *   1. It reported `void FUN_00068a10(void)` and surfaced the parameter only
 *      as `in_stack_00000004`, because kb.json carried a `(void)` prototype.
 *      There is exactly one stack argument, at [ebp+8].
 *   2. It dropped the return entirely. The `mov eax,[0x2ca1f4]` at 0x68a16 is
 *      never overwritten before the RET, so EAX carries the previous handler
 *      out -- the classic void-EAX implicit return. Shipping the `void(void)`
 *      prototype would have dropped the argument at every call site AND
 *      discarded the value libtiff's callers use to chain/restore handlers.
 *
 * Consequently the read of `_TIFFerrorHandler` must stay ABOVE the assignment:
 * the ordering is not stylistic, it is what puts the old value in EAX. Writing
 * it as a `prev` temporary rather than reading the slot twice matches the
 * binary, which touches 0x2ca1f4 once for the load and once for the store.
 */
void *FUN_00068a10(void *handler)
{
  /* 0x68a16. Load before store -- this value is the return. */
  void *prev = _TIFFerrorHandler;

  /* 0x68a1b. */
  _TIFFerrorHandler = handler;

  /* EAX from 0x68a16, untouched. */
  return prev;
}
