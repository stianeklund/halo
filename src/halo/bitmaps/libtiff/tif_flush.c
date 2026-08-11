/* ===========================================================================
 * tif_flush.c -- vendored libtiff, "dump mode" (uncompressed) codec.
 *
 * kb.json places FUN_00068780, FUN_00068890 and FUN_00068940 in
 * tif_flush.obj, so all three
 * bodies live here. Their own upstream translation unit is INFERRED, not
 * proven: the binary carries
 * no `__FILE__` string for either tif_flush.c or tif_dumpmode.c (the only
 * libtiff source strings present are tif_close/dir/dirread/dirwrite/fax3/
 * getimage/lzw/open/read/write), and this function has no assert to stamp
 * one. By shape they are upstream libtiff 3.x tif_dumpmode.c:DumpModeEncode
 * and DumpModeDecode (plus DumpModeSeek at 0x68940), modified by Bungie:
 * upstream clamps the copy with
 * `tif_rawdatasize - tif_rawcc` and has no byte-swap step, while this build
 * clamps with a plain `tif_rawdatasize` and swabs the freshly copied bytes
 * when TIFF_SWAB is set. Transcribed from upstream and adapted to the
 * observed control flow rather than reshaped from the decompiler, which lost
 * every parameter and reported the body as `void(void)`.
 * ======================================================================== */

#include <stdarg.h>

/* Partial view of the TIFF handle. The fuller recovery of this struct lives
 * in tif_open.c (`tiff_t`, offsets 0x00..0x138 with per-field evidence); this
 * TU repeats only the five offsets it touches, at the same offsets and with
 * the same field names and widths, because the struct is still a file-scope
 * typedef in each libtiff TU rather than a shared tiffiop.h. Everything else
 * is padding as far as THIS file is concerned -- the pad_ runs here are not a
 * claim that those bytes are unused, only that this function never reads
 * them. Offsets proven from the disassembly of FUN_00068780, FUN_00068890 and
 * FUN_00068940:
 *   +0x00  `mov ecx,[esi]`                            (0x688ab)
 *   +0x0a  `mov al,[esi+0xa]`   / `test al,0x10`      (0x687df, 0x687e5)
 *   +0x36  `movzx eax,word ptr [esi+0x36]`            (0x687e9, 0x688e1)
 *   +0xd4  `mov eax,[esi+0xd4]`                       (0x688a5)
 *   +0x124 `mov ecx,[eax+0x124]`                      (0x68947)
 *   +0x130 `mov eax,[esi+0x130]`                      (0x68791, 0x687c0)
 *   +0x134 `mov edx,[esi+0x134]`                      (0x687d1, 0x68828,
 *                                                      0x688c2, 0x68914)
 *   +0x138 `mov eax,[esi+0x138]`                      (0x6878b, 0x68831,
 *                                                      0x68897, 0x6891a)
 */
/* Codec method shapes. Both are repeated from tif_open.c (which carries the
 * fuller `tiff_t` recovery) rather than shared, because the struct is still a
 * file-scope typedef per libtiff TU. `tiff_code_method_t` is pinned by the
 * three bodies above -- FUN_00068780/FUN_00068890 read (tif, buf, cc) and
 * ignore a fourth stack slot -- and `tiff_seek_method_t` by FUN_00068940,
 * which reads exactly (tif, nrows). */
typedef int (*tiff_code_method_t)(void *tif, char *buf, int cc, int s);
typedef int (*tiff_seek_method_t)(void *tif, unsigned long nrows);

typedef struct tiff_s {
  /* 0x00 -- upstream's `char* tif_name`, the first member of `struct tiff`.
   * FUN_00068890 pushes it straight into the TIFFError module slot
   * (`mov ecx,[esi]` at 0x688ab), the same use tif_open.c's TIFFFileName
   * (0x6d850) makes of it. */
  char *tif_name;
  char pad_004[6];
  /* 0x0a -- flags byte. TIFFIsTiled (0x6d880) reads it as a SIGNED byte, so
   * the field is `char`; bit 4 (0x10) is upstream libtiff's TIFF_SWAB. */
  char field_0a;
  char pad_00b[43];
  unsigned short td_bitspersample; /* 0x36 */
  char pad_038[156];
  /* 0xd4 -- upstream's `uint32 tif_row`. Read as a full dword with no
   * MOVSX/MOVZX (`mov eax,[esi+0xd4]` at 0x688a5), matching the same field
   * recovered at the same offset in tif_open.c. */
  unsigned long tif_row;
  char pad_0d8[36];
  /* 0xfc..0x110 -- the six codec code slots. FUN_00068970 stores 0x68890
   * (the decoder) into 0xfc/0x104/0x10c and 0x68780 (the encoder) into
   * 0x100/0x108/0x110, which proves the decode/encode split but not the
   * row/strip/tile suborder within each triple; that suborder is INFERRED
   * from upstream's field order, exactly as in tif_open.c, which recovers
   * the same six offsets independently from the LZW/NeXT/PackBits codec
   * installers. */
  tiff_code_method_t tif_decoderow; /* 0xfc */
  tiff_code_method_t tif_encoderow; /* 0x100 */
  tiff_code_method_t tif_decodestrip; /* 0x104 */
  tiff_code_method_t tif_encodestrip; /* 0x108 */
  tiff_code_method_t tif_decodetile; /* 0x10c */
  tiff_code_method_t tif_encodetile; /* 0x110 */
  /* 0x114 -- upstream's tif_close. Never written by anything recovered so
   * far, in this TU or in tif_open.c. */
  char pad_114[4];
  /* 0x118 -- upstream's tif_seek. Written with an immediate at 0x68986
   * (`mov dword ptr [eax+0x118],0x68940`), which is what promotes it out of
   * the pad_114[8] run tif_open.c still carries. */
  tiff_seek_method_t tif_seek; /* 0x118 */
  char pad_11c[8];
  /* 0x124 -- upstream's `tsize_t tif_scanlinesize`, the byte size of one
   * decoded scanline. Read as a full dword with no widening
   * (`mov ecx,[eax+0x124]` at 0x68947). Same offset, name and width as the
   * field recovered independently in tif_open.c from 0x6d36e; upstream
   * declares it immediately before tif_scanlineskew (0x128, unread here) and
   * tif_rawdata (0x12c), which is what fixes this offset. */
  long tif_scanlinesize; /* 0x124 */
  char pad_128[8];
  /* 0x130..0x138 -- upstream's tif_rawdatasize / tif_rawcp / tif_rawcc.
   * tif_rawdata itself sits at 0x12c (see tif_open.c) and is not touched
   * here, so it falls inside pad_038. */
  long tif_rawdatasize; /* 0x130 */
  unsigned char *tif_rawcp; /* 0x134 */
  long tif_rawcc; /* 0x138 */
} tiff_t;

/**
 * Copy `cc` bytes of uncompressed sample data into the raw output buffer,
 * flushing it to the file whenever it fills up.
 *
 * ABI recovered from the frame at 0x68780: `push ebp / mov ebp,esp` with no
 * `sub esp` (every local lives in a callee-saved register: ESI=tif,
 * EBX=cc, EDI=n), plain `ret` with no immediate, and all callee cleanup
 * caller-side -- cdecl. Three stack arguments are read, at [ebp+8], [ebp+0xc]
 * and [ebp+0x10]; the fourth (`s`) is never read, and is kept because 0x68780
 * is installed as a codec method pointer (its address is the immediate stored
 * at 0x6898e, inside the codec-init routine FUN_00068970), and this repo's
 * codec method shape is already pinned to (tif, buf, cc, s) by tif_open.c's
 * `tiff_code_method_t`.
 *
 * `pp` really is a stack variable rather than a register: it is reloaded from
 * [ebp+0xc] on every iteration (0x687ce, 0x6882e) and written back at
 * 0x68843.
 *
 * Return value is EAX and has three distinct sites: `or eax,-1` at 0x687ab
 * (entry flush failed), `mov eax,1` at 0x68871 (all bytes consumed) and
 * `or eax,-1` at 0x6887b (a mid-loop flush failed).
 */
int FUN_00068780(void *tif_, char *pp, int cc, int s)
{
  tiff_t *tif = (tiff_t *)tif_;

  (void)s;

  /* 0x6878b-0x687b0. Note this tests the WHOLE pending write against the
   * buffer capacity (`tif_rawcc + cc`), unlike the per-chunk watermark test
   * at the bottom of the loop. */
  if (tif->tif_rawcc + cc > tif->tif_rawdatasize && !TIFFFlushData1(tif))
    return -1;

  while (cc > 0) {
    int n;

    /* 0x687c0-0x687cc. Clamped to the buffer capacity, not to the space
     * remaining -- the entry guard above has already made room. */
    n = cc;
    if (n > tif->tif_rawdatasize)
      n = tif->tif_rawdatasize;
    csmemcpy(tif->tif_rawcp, pp, n);

    /* 0x687df-0x68825. Byte-swap in place, over the bytes just copied. The
     * element count is a SIGNED divide in the original (`cdq / sub / sar 1`
     * at 0x68819 and `cdq / and 3 / add / sar 2` at 0x687f9), i.e. plain
     * `n / 2` and `n / 4` on a signed int -- not `>> 1` / `>> 2`, which
     * would drop the rounding correction. The two callees are shaped like
     * upstream's TIFFSwabArrayOfShort / TIFFSwabArrayOfLong (buffer, count),
     * but they carry no name evidence of their own, so they keep their
     * mechanical kb.json names. */
    if (tif->field_0a & 0x10) {
      switch (tif->td_bitspersample) {
      case 16:
        FUN_0006f1f0(tif->tif_rawcp, n / 2);
        break;
      case 32:
        FUN_0006f220(tif->tif_rawcp, n / 4);
        break;
      }
    }

    /* 0x68828-0x68865. Store order follows the original: tif_rawcp, then the
     * spilled pp, then tif_rawcc; cc stays in EBX and is never stored. */
    tif->tif_rawcp += n;
    pp += n;
    cc -= n;
    tif->tif_rawcc += n;
    if (tif->tif_rawcc >= tif->tif_rawdatasize && !TIFFFlushData1(tif))
      return -1;
  }
  return 1;
}

/**
 * Consume `occ` bytes of already-decoded (uncompressed) sample data from the
 * raw input buffer, byte-swapping in place when the file's endianness differs
 * from the host's.
 *
 * Transcribed from upstream libtiff 3.x tif_dumpmode.c:DumpModeDecode rather
 * than reshaped from the decompiler, which lost every parameter and reported
 * the body as `void(void)`. The swab step is Bungie's addition -- upstream
 * updates the pointers and returns -- and is the same edit they made to
 * DumpModeEncode above.
 *
 * ABI recovered from the frame at 0x68890: `push ebp / mov ebp,esp` with no
 * `sub esp` (ESI=tif, EBX=buf, EDI=occ), plain `ret`, caller-side cleanup on
 * all three calls -- cdecl. Three stack arguments are read, at [ebp+8],
 * [ebp+0xc] and [ebp+0x10]; the fourth (`s`) is never read, and is kept for
 * the same reason as in DumpModeEncode -- 0x68890 is installed as a codec
 * method pointer by FUN_00068970, and the method shape is pinned to
 * (tif, buf, occ, s) by tif_open.c's `tiff_code_method_t`.
 *
 * The return really is an int, not void: `xor eax,eax` at 0x688bd on the
 * short-data path and `mov eax,1` at 0x68932 on the success path.
 *
 * EBX is pushed late (0x688c8), after the short-data branch has already
 * returned -- MSVC shrink-wraps the save because `buf` is only live past the
 * guard. That is a codegen detail of this shape, not something the source
 * expresses.
 *
 * `occ` is signed: the guard at 0x688a1 is `cmp eax,edi / jge`, and both
 * element counts are the signed-divide idiom (`cdq / sub / sar 1` at 0x68905
 * and `cdq / and 3 / add / sar 2` at 0x688f1), i.e. `occ / 2` and `occ / 4`
 * on a signed int -- an unsigned type would emit a bare SHR.
 */
int FUN_00068890(void *tif_, unsigned char *buf, int occ, int s)
{
  tiff_t *tif = (tiff_t *)tif_;

  (void)s;

  /* 0x68897-0x688c1. Note the module argument is tif_name (offset 0x00), the
   * first member of the handle -- upstream's TIFFError(tif->tif_name, ...). */
  if (tif->tif_rawcc < (long)occ) {
    FUN_00068a30(tif->tif_name,
                 "DumpModeDecode: Not enough data for scanline %d",
                 tif->tif_row);
    return 0;
  }

  /* 0x688c2-0x688d8. Decoding is in place when the caller already handed us
   * the raw buffer, so the copy is skipped on pointer equality. */
  if (tif->tif_rawcp != buf)
    csmemcpy(buf, tif->tif_rawcp, occ);

  /* 0x688db-0x68911. Swab over the bytes just delivered. Same two callees,
   * same signed element counts, as the encoder above. */
  if (tif->field_0a & 0x10) {
    switch (tif->td_bitspersample) {
    case 16:
      FUN_0006f1f0(buf, occ / 2);
      break;
    case 32:
      FUN_0006f220(buf, occ / 4);
      break;
    }
  }

  /* 0x68914-0x68932. tif_rawcp is a byte pointer -- the original adds occ to
   * it directly (`add ecx,edi`), with no element scaling. */
  tif->tif_rawcp += occ;
  tif->tif_rawcc -= occ;
  return 1;
}

/**
 * Skip forwards `nrows` scanlines in the current strip without decoding them.
 *
 * This is upstream libtiff tif_dumpmode.c's `DumpModeSeek`, unmodified: in
 * dump (uncompressed) mode a scanline occupies exactly `tif_scanlinesize`
 * bytes of the raw buffer, so seeking is pure cursor arithmetic -- advance
 * tif_rawcp and shrink tif_rawcc by the same amount. It is the codec's
 * `tif_seek` method, which is why it returns 1 unconditionally (there is no
 * failure mode) rather than void.
 *
 * ABI recovered from the frame at 0x68940 (10 instructions, 0x68940-0x68963):
 * `push ebp / mov ebp,esp` with NO `sub esp` (the one temporary lives in ECX),
 * two stack arguments at [ebp+8] and [ebp+0xc], `pop ebp / ret` with no
 * immediate -- cdecl, caller cleans. The decompiler reported the body as
 * `void(void)` and surfaced both parameters as `in_stack_00000004/8`, purely
 * because kb.json carried a `(void)` prototype; it also dropped the
 * `mov eax,1` at 0x6895c, so the return type is `int`, not void.
 *
 * The product is materialized once (`mov ecx,[eax+0x124]` then
 * `imul ecx,[ebp+0xc]`) and consumed by both the ADD and the SUB, so it is
 * written here as a single local rather than as upstream's two copies of
 * `nrows * tif->tif_scanlinesize`; the operand order follows the binary,
 * which loads tif_scanlinesize into the destination register first. Two-
 * operand `imul r32,r/m32` keeps only the low 32 bits, so it is emitted for
 * signed and unsigned multiplies alike -- the instruction does not witness
 * the signedness of `nrows`, and upstream's `uint32` is kept.
 */
int FUN_00068940(void *tif_, unsigned long nrows)
{
  tiff_t *tif = (tiff_t *)tif_;
  long nbytes;

  /* 0x68943-0x6894e. */
  nbytes = (long)(tif->tif_scanlinesize * nrows);

  /* 0x6894f-0x6895b. ADD then SUB, on adjacent fields, in this order. */
  tif->tif_rawcp += nbytes;
  tif->tif_rawcc -= nbytes;

  /* 0x6895c. */
  return 1;
}

/**
 * Install the "dump mode" (uncompressed) codec methods into a TIFF handle.
 *
 * This is upstream libtiff tif_dumpmode.c's `TIFFInitDumpMode`, unmodified:
 * the same decoder goes into all three decode slots, the same encoder into
 * all three encode slots, the seek method into tif_seek, and it returns 1
 * unconditionally (there is nothing here that can fail).
 *
 * ABI recovered from the frame at 0x68970 (15 instructions, 0x68970-0x689b4):
 * `push ebp / mov ebp,esp` with NO `sub esp` (the only temporary is the ECX
 * the two shared immediates are staged in), one stack argument read at
 * [ebp+8], `pop ebp / ret` with no immediate -- cdecl, caller cleans. The
 * decompiler reported the body as `void(void)` and surfaced the parameter as
 * `in_stack_00000004`, purely because kb.json carried a `(void)` prototype;
 * it also dropped the `mov eax,1` at 0x689af, so the return type is `int`,
 * not void.
 *
 * Upstream takes a second `int scheme` argument which it discards with a
 * `(void)` cast. Nothing here reads [ebp+0xc], so only the first parameter is
 * binary-proven and only it is declared -- cdecl makes an extra pushed
 * argument harmless either way, so this costs no compatibility with whatever
 * codec-registration table installs it.
 *
 * The store order below is upstream's and is also exactly the emitted order:
 * MSVC stages 0x68890 in ECX once for the three decode slots (0x68975-0x68983)
 * and 0x68780 in ECX once for the three encode slots (0x6898e-0x6899c),
 * then writes tif_seek as a plain immediate (0x68986... the 0x118 store is the
 * only one that never goes through the shared register).
 */
int FUN_00068970(void *tif_)
{
  tiff_t *tif = (tiff_t *)tif_;

  /* 0x68975-0x68983. One ECX load, three stores. */
  tif->tif_decoderow = (tiff_code_method_t)FUN_00068890;
  tif->tif_decodestrip = (tiff_code_method_t)FUN_00068890;
  tif->tif_decodetile = (tiff_code_method_t)FUN_00068890;

  /* 0x6898e-0x6899c. A second ECX load, three more stores. */
  tif->tif_encoderow = (tiff_code_method_t)FUN_00068780;
  tif->tif_encodestrip = (tiff_code_method_t)FUN_00068780;
  tif->tif_encodetile = (tiff_code_method_t)FUN_00068780;

  /* 0x689a2. Immediate store, no register staging. */
  tif->tif_seek = (tiff_seek_method_t)FUN_00068940;

  /* 0x689af. */
  return 1;
}

/* ===========================================================================
 * `TIFFError`, upstream libtiff tif_error.c.
 *
 * kb.json places this address in tif_flush.obj, so the body lives here even
 * though its two upstream TU-mates (`TIFFDefaultErrorHandler` at 0x689c0 and
 * `TIFFSetErrorHandler` at 0x68a10) are mapped to tif_error.obj. The object
 * assignment is INFERRED for every resident of this libtiff cluster -- the
 * binary carries no `__FILE__` string for tif_error.c or tif_flush.c -- so the
 * mapping is followed as-is rather than second-guessed here.
 *
 * Transcribed from upstream and adapted to the observed control flow, per the
 * vendored-library rule, rather than reshaped from the decompiler (which lost
 * the varargs entirely and reported `void(undefined4,undefined4)`):
 *
 *     void
 *     TIFFError(const char* module, const char* fmt, ...)
 *     {
 *             if (_TIFFerrorHandler) {
 *                     va_list ap;
 *                     va_start(ap, fmt);
 *                     (*_TIFFerrorHandler)(module, fmt, ap);
 *                     va_end(ap);
 *             }
 *     }
 *
 * Disassembly of 0x68a30 (15 instructions, 0x68a30-0x68a4e):
 *
 *   068a30  push ebp                  ; NO `sub esp` -- zero locals, zero
 *   068a31  mov  ebp, esp             ; callee-saved pushes, no _chkstk. `ap`
 *   068a33  mov  eax, [0x2ca1f4]      ; never gets a stack slot; it is the LEA
 *   068a38  test eax, eax             ; below, staged straight into ECX.
 *   068a3a  jz   0x68a4d              ; handler unset => do nothing
 *   068a3c  mov  edx, [ebp+0xc]       ; fmt
 *   068a3f  lea  ecx, [ebp+0x10]      ; &first vararg == va_start(ap, fmt)
 *   068a42  push ecx                  ;   arg3 = ap
 *   068a43  mov  ecx, [ebp+8]         ; module (ECX REUSED -- see below)
 *   068a46  push edx                  ;   arg2 = fmt
 *   068a47  push ecx                  ;   arg1 = module
 *   068a48  call eax
 *   068a4a  add  esp, 0xc             ; 3 args, cdecl => the handler is cdecl
 *   068a4d  pop  ebp
 *   068a4e  ret                       ; no immediate, no EAX write => void
 *
 * Three load-bearing details:
 *
 *   1. The varargs are real. The decompiler's third argument
 *      `&stack0x0000000c` is `lea ecx,[ebp+0x10]`, the address of the first
 *      vararg slot -- i.e. a va_list, not a third named parameter. Lifting
 *      this as a fixed 3-parameter function would push the caller's first
 *      variadic value BY VALUE and print garbage.
 *   2. ECX is loaded twice, in interleaved order (LEA for `ap` at 0x68a3f,
 *      then MOV for `module` at 0x68a43), so load order is NOT push order.
 *      Traced per-push: 0x68a47 pushes the 0x68a43 load (module), 0x68a46
 *      pushes the 0x68a3c load (fmt), 0x68a42 pushes the 0x68a3f LEA (ap).
 *      Reverse-order cdecl therefore gives the logical order (module, fmt, ap)
 *      -- which is also the signature of FUN_000689c0, the handler this slot
 *      is statically initialized to.
 *   3. The NULL test at 0x68a38/0x68a3a is load-bearing and must stay: the
 *      handler slot is a writable .data word that `TIFFSetErrorHandler`
 *      (0x68a10) can clear, so the guard is reachable.
 *
 * `_TIFFerrorHandler` is the same 0x2ca1f4 slot tif_error.c documents (that
 * TU's `#define` proves the address and the ERROR-vs-warning identity from the
 * static initializer 0x000689c0 = FUN_000689c0). It is declared with its real
 * function-pointer type here -- unlike in tif_error.c, which only swaps the
 * raw word and so needs nothing sharper than `void *` -- because this is the
 * resident that calls through it, and the pointee shape is what fixes the push
 * count and the cdecl cleanup. `va_list` is `char *` on i386 MSVC 7.1, matching
 * the third parameter of FUN_000689c0 and of the vfprintf it forwards to.
 * Address-macro form is the one the other vendored libtiff TUs use for their
 * file-scope statics (tif_open.c's `photometric`/`BWmap`, tif_error.c's own
 * `_TIFFerrorHandler`).
 */
typedef void (*tiff_error_handler_t)(const char *module, const char *fmt,
                                     char *ap);

#define _TIFFerrorHandler (*(tiff_error_handler_t *)0x2ca1f4)

void FUN_00068a30(const char *module, const char *format, ...)
{
  va_list ap;

  /* 0x68a33-0x68a3a. */
  if (_TIFFerrorHandler != NULL) {
    /* 0x68a3f. Folds to the bare LEA -- no stack slot for `ap`. */
    va_start(ap, format);

    /* 0x68a42-0x68a4a. Pushes traced individually above. */
    (*_TIFFerrorHandler)(module, format, (char *)ap);

    va_end(ap);
  }
}
