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
 * format, and no such push exists here. The remaining tif_error.c residents
 * (`TIFFError` at 0x68a30 and `TIFFSetErrorHandler` at 0x68a10, which
 * together own the `_TIFFerrorHandler` function-pointer global at 0x2ca1f4
 * that holds this address) are not yet ported, so this TU currently carries
 * only the handler itself.
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
