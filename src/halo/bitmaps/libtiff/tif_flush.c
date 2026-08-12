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
 *   +0x09  `mov cl,[eax+0x9]` / `mov [eax+0x9],cl`    (0x68a5a, 0x68a62,
 *                                                      0x68a6b)
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
/* The setup/postencode and close/cleanup method shapes, repeated from
 * tif_open.c for the same file-scope-typedef reason. `tiff_bool_method_t` is
 * pinned for 0xf8 by FUN_00069520 in this TU (kb.json: `int (void *tif)`);
 * 0xf0 and 0xf4 take FUN_00068d80 / FUN_00069420. Both int returns are now
 * proven below: each body reaches its epilogue through an explicit `mov eax,1`
 * (0x694f7 and 0x6950a on the encode side) against a zero return on its
 * state-allocation failure path.
 * `tiff_void_method_t` is pinned for 0x114 and 0x11c by FUN_00069590 and
 * FUN_000695c0, both already recovered in this TU as `void (void *tif)`. */
typedef int (*tiff_bool_method_t)(void *tif);
typedef void (*tiff_void_method_t)(void *tif);

/* Private codec state reached through `tif_data` (0x120) by FUN_00069520. This
 * is NOT tif_open.c's `tiff_bitstate_t`: that struct carries `int bitpos` at
 * 0x14, while 0x69520 loads a POINTER from 0x14 (`mov edx,[edi+0x14]` at
 * 0x6954f) and indexes it with a byte load, and its 0x00/0x02 fields are 16-bit
 * where tiff_bitstate_t's 0x00 is a dword. A different libtiff codec owns this
 * block, so it gets its own file-scope type here rather than a retype of the
 * shared one.
 *
 * Offsets proven from the disassembly of FUN_00069520:
 *   +0x00  `movsx ecx,word ptr [edi]`   (0x6954c)  -- SIGNED 16-bit
 *   +0x02  `cmp word ptr [edi+2],8`     (0x6952e)  -- 16-bit
 *          `mov word ptr [edi+2],8`     (0x6957c)
 *   +0x14  `mov edx,[edi+0x14]`         (0x6954f)  -- pointer, consumed as
 *                                                    `mov cl,[ecx+edx]`
 *
 * The field names come from upstream libtiff tif_fax3.c's encoder state
 * (`data` / `bit` / `bitmap`, the bit-reversal table indexed by the
 * accumulator), whose flush shape this body matches exactly; that
 * identification is INFERRED from shape, not proven by a `__FILE__` string.
 * The other members below are promoted by FUN_00068c70/FUN_00068d80; their
 * offsets and access widths are taken from those bodies. */
typedef struct tiff_codec_bits_s {
  short data; /* 0x00 */
  short bit; /* 0x02 */
  unsigned short fill_white; /* 0x04 */
  char pad_006[2];
  int rowbytes; /* 0x08 */
  int rowpixels; /* 0x0c */
  /* Written 0 on reset and then `(first_bit == 0)` by FUN_00068d80; only ever
   * observed holding 0 or 1, so the meaning is unproven -- do NOT read this as
   * a counter. */
  int field_10; /* 0x10 */
  const unsigned char *bitmap; /* 0x14 */
  unsigned char *fill_line; /* 0x18 */
  /* 0x1c / 0x20 -- the two run-length scan tables, promoted by FUN_00069420,
   * which stores 0x2ec3c8 and 0x2ec4c8 into them (`mov dword ptr [eax+0x1c]`
   * / `[eax+0x20]` on both arms of the 0x69455 branch) and SWAPS the pair when
   * `fill_white` is nonzero. The two globals are 0x100 bytes apart and their
   * contents identify them outright against upstream tif_fax3.c: 0x2ec3c8
   * begins `08 07 06 06 05 05 05 05 04 04 04 04 04 04 04 04`, upstream's
   * `zeroruns[256]`, and 0x2ec4c8 begins with sixteen zero bytes, upstream's
   * `oneruns[256]`. Which of the two members is the white scanner and which
   * the black one is NOT proven -- no body observed here READS either slot --
   * so they stay `field_`. */
  const unsigned char *field_1c; /* 0x1c */
  const unsigned char *field_20; /* 0x20 */
  /* 0x24 / 0x26 -- 16-bit, written only by FUN_00069420 as the pair (n-1, n)
   * with n in {2,4}, or as (0,0) when 2D encoding is off. Names are INFERRED
   * from upstream tif_fax3.c's `Fax3PreEncode`, whose tail is
   * `sp->maxk = (res > 150 ? 4 : 2); sp->k = sp->maxk-1;` for 2D and
   * `sp->k = sp->maxk = 0;` otherwise -- the same values, the same store
   * order, and the same declaration order (k below maxk). Upstream declares
   * both `int`; this build stores them as WORDS (`mov word ptr [esi+0x26]`,
   * `mov word ptr [esi+0x24]`), so the width here is 2, not 4. */
  short k; /* 0x24 */
  short maxk; /* 0x26 */
} tiff_codec_bits_t;

typedef struct tiff_s {
  /* 0x00 -- upstream's `char* tif_name`, the first member of `struct tiff`.
   * FUN_00068890 pushes it straight into the TIFFError module slot
   * (`mov ecx,[esi]` at 0x688ab), the same use tif_open.c's TIFFFileName
   * (0x6d850) makes of it. */
  char *tif_name;
  char pad_004[2];
  /* 0x06 -- upstream's `tif_mode`, the open() mode the handle was created with;
   * O_RDONLY is 0, so `!= 0` is upstream's "opened for writing" test.
   * FUN_0006a260 reads it with `cmp word ptr [esi+6],0` (0x6a267), a 16-bit
   * compare, so the field is 2 bytes and NOT the `int` upstream declares -- the
   * offset itself is only where upstream's field order lands once tif_fd at
   * 0x04 is also narrowed to 16 bits. tif_open.c recovers the same offset,
   * width and name independently from TIFFGetMode's `movsx eax,word ptr
   * [eax+6]` (0x6d876); this was inside pad_004[5] until 0x6a267 proved it. */
  short tif_mode;
  char pad_008[1];
  /* 0x09 -- second flags byte, one byte below field_0a. FUN_00068a50 loads it
   * with `mov cl,[eax+9]` (0x68a5a) and stores bit 0 back set or cleared, so
   * the width is BYTE and the offset is accessed -- it was inside pad_004[6]
   * until this body proved it. Whether it is byte 1 of an upstream `uint32
   * tif_flags` based at 0x08 (which would make this bit 0x100) is UNPROVEN
   * from anything recovered so far, so the bit is set with a literal rather
   * than a TIFF_* macro. */
  char field_09;
  /* 0x0a..0x0b -- flags. TWO access widths are proven at this offset, so the
   * field carries both views rather than picking one and casting at the use
   * sites. BYTE: TIFFIsTiled (0x6d880) reads it as a SIGNED byte, and
   * FUN_00068780/FUN_00068890 test bit 4 with `test byte ptr [esi+0xa],0x10`
   * (upstream libtiff's TIFF_SWAB), while FUN_0006a190 sets bit 5 with
   * `or byte ptr [eax+0xa],0x20`. WORD: FUN_0006a210 loads and stores 0x0a as
   * a 16-bit quantity -- `mov ax,word ptr [esi+0xa]` at 0x6a216 and
   * `mov word ptr [esi+0xa],ax` at 0x6a231 -- because the bit it clears
   * (0x200) lives in the HIGH byte at 0x0b and is unreachable through the byte
   * view. Upstream's single `uint16 tif_flags`, with every byte access being an
   * MSVC narrowing of it, is the likely original declaration, but that is
   * INFERRED; the union keeps each recovered body at the width its own
   * disassembly shows. Bit numbering still does not match stock libtiff (see
   * `field_09`), so bits stay literals rather than TIFF_* macros. */
  union {
    char b; /* 0x0a -- byte view */
    unsigned short w; /* 0x0a..0x0b -- word view */
  } field_0a;
  char pad_00c[42];
  unsigned short td_bitspersample; /* 0x36 */
  char pad_038[156];
  /* 0xd4 -- upstream's `uint32 tif_row`. Read as a full dword with no
   * MOVSX/MOVZX (`mov eax,[esi+0xd4]` at 0x688a5), matching the same field
   * recovered at the same offset in tif_open.c. */
  unsigned long tif_row;
  char pad_0d8[24];
  /* 0xf0..0xf8 -- upstream's tif_setupdecode / tif_setupencode /
   * tif_postencode. All three are promoted out of the pad_0d8[36] run by
   * FUN_0006a190, which stores immediates into them
   * (`mov dword ptr [eax+0xf0],0x68d80` and the two following); tif_open.c
   * recovers the same three offsets and names independently from the
   * LZW/predictor codec installers. Upstream's tif_predecode (0xfc in a stock
   * build) is absent from Bungie's copy -- the six code slots start at 0xfc
   * here -- which tif_open.c already notes. */
  tiff_bool_method_t tif_setupdecode; /* 0xf0 */
  tiff_bool_method_t tif_setupencode; /* 0xf4 */
  tiff_bool_method_t tif_postencode; /* 0xf8 */
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
  /* 0x114 -- upstream's tif_close. Promoted out of pad_114 by FUN_0006a190,
   * which stores FUN_00069590 (recovered in this TU as `void (void *tif)`)
   * into it with `mov dword ptr [eax+0x114],0x69590`. tif_open.c still carries
   * it inside a pad_114[8] run. */
  tiff_void_method_t tif_close; /* 0x114 */
  /* 0x118 -- upstream's tif_seek. Written with an immediate at 0x68986
   * (`mov dword ptr [eax+0x118],0x68940`), which is what promotes it out of
   * the pad_114[8] run tif_open.c still carries. */
  tiff_seek_method_t tif_seek; /* 0x118 */
  /* 0x11c -- upstream's tif_cleanup. Promoted out of pad_11c by FUN_0006a190
   * (`mov dword ptr [eax+0x11c],0x695c0`); FUN_000695c0 is the Fax3 cleanup
   * recovered at the bottom of this TU, and tif_open.c recovers the same offset
   * and name from the LZW installer. */
  tiff_void_method_t tif_cleanup; /* 0x11c */
  /* 0x120 -- upstream's `tidata_t tif_data`, the codec's private state block.
   * `mov edi,[esi+0x120]` at 0x69528; tif_open.c recovers the same offset and
   * name independently. Typed to this TU's own state shape (see
   * tiff_codec_bits_t) rather than tif_open.c's tiff_bitstate_t, because the
   * codec that owns the block here is a different one. It was inside
   * pad_11c[8] until FUN_00069520 proved the access. */
  tiff_codec_bits_t *tif_data; /* 0x120 */
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
    if (tif->field_0a.b & 0x10) {
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
  if (tif->field_0a.b & 0x10) {
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

/**
 * Set or clear bit 0 of the flags byte at tif+0x9 according to `flag`.
 *
 * ABI recovered from the frame at 0x68a50: bare `push ebp / mov ebp,esp` with
 * NO `sub esp` -- no locals -- two stack arguments at [ebp+8] and [ebp+0xc],
 * `pop ebp / ret` with no immediate (cdecl), and no EAX write on either path,
 * so the return is void. The decompiler lost both parameters and reported them
 * as `in_stack_00000004` / `in_stack_00000008` with the prototype
 * `void(void)`; they are ordinary cdecl stack slots, not register arguments.
 *
 *   068a50  push ebp
 *   068a51  mov  ebp, esp
 *   068a53  mov  eax, [ebp+0xc]     ; flag
 *   068a56  test eax, eax
 *   068a58  mov  eax, [ebp+8]       ; tif -- EAX reloaded, `flag` only tested
 *   068a5b  mov  cl,  [eax+9]       ; BYTE load, once, BEFORE the branch
 *   068a5e  jz   068a68
 *   068a60  or   cl,  1
 *   068a62  mov  [eax+9], cl
 *   068a65  pop  ebp
 *   068a66  ret
 *   068a68  and  cl,  0xfe
 *   068a6b  mov  [eax+9], cl
 *   068a6e  pop  ebp
 *   068a6f  ret
 *
 * Three shape details worth keeping:
 *   1. The load is `mov cl,` -- a BYTE, not a word and not a dword. Typing
 *      the field wider would be a field-width bug, and the immediate mask
 *      would grow with it.
 *   2. The byte is loaded ONCE, above the branch, and each arm stores its own
 *      result, so the source is a plain if/else with two stores rather than a
 *      single store of a conditional expression (which risks a merge/CMOV
 *      shape).
 *   3. Two separate epilogues (duplicated `pop ebp / ret`) is what MSVC 7.1
 *      emits for that if/else; it is not evidence of a shared tail.
 *
 * Which upstream libtiff entry point this is remains UNKNOWN: the flag's
 * meaning is unproven (see `field_09`), so the bit is set with a literal 1 and
 * cleared with 0xfe rather than a named TIFF_* macro.
 */
void FUN_00068a50(void *tif_, int flag)
{
  tiff_t *tif = (tiff_t *)tif_;

  /* 0x68a56-0x68a6b. */
  if (flag != 0) {
    tif->field_09 = (char)(tif->field_09 | 1);
  } else {
    tif->field_09 = (char)(tif->field_09 & 0xfe);
  }
}

/**
 * Consume input bytes until at least twelve bits have been accumulated, or
 * until the accumulator becomes positive. This is the bit-reversal and
 * bit-count helper used by the fax codec.
 *
 * ABI: `bit_count` arrives in EAX and `tif_` arrives in EDI; there are no
 * stack arguments. The original does not write a deliberate return value.
 * The two register arguments are ordered by their kb.json parameter slots,
 * not by the order in which the callee first reads them.
 *
 * The switch/fall-through shape is intentional. Each case consumes one bit
 * from the current reversed byte, and a non-positive accumulator falls into
 * the next case without re-dispatching. The default arm obtains another raw
 * byte and retries case zero. All offsets outside the existing partial
 * tiff_t view are kept as raw accesses so this TU's established layout stays
 * unchanged.
 */
void FUN_00068a70(int bit_count /* @<eax> */, void *tif_ /* @<edi> */)
{
  tiff_t *tif;
  tiff_codec_bits_t *sp;
  int accumulator;
  int bit;
  int data;

  tif = (tiff_t *)tif_;
  sp = tif->tif_data;
  bit = (short)sp->bit;
  data = (short)sp->data;
  if (bit == 0)
    bit = 8;
  accumulator = 0;

dispatch:
  switch (bit) {
  case 0:
    goto bit_0;
  case 1:
    goto bit_1;
  case 2:
    goto bit_2;
  case 3:
    goto bit_3;
  case 4:
    goto bit_4;
  case 5:
    goto bit_5;
  case 6:
    goto bit_6;
  case 7:
    goto bit_7;
  default:
    goto get_byte;
  }

bit_0:
  accumulator <<= 1;
  if ((data & 0x80) != 0)
    accumulator |= 1;
  bit_count++;
  if (accumulator > 0) {
    bit = 1;
    goto finish_bits;
  }
bit_1:
  accumulator <<= 1;
  if ((data & 0x40) != 0)
    accumulator |= 1;
  bit_count++;
  if (accumulator > 0) {
    bit = 2;
    goto finish_bits;
  }
bit_2:
  accumulator <<= 1;
  if ((data & 0x20) != 0)
    accumulator |= 1;
  bit_count++;
  if (accumulator > 0) {
    bit = 3;
    goto finish_bits;
  }
bit_3:
  accumulator <<= 1;
  if ((data & 0x10) != 0)
    accumulator |= 1;
  bit_count++;
  if (accumulator > 0) {
    bit = 4;
    goto finish_bits;
  }
bit_4:
  accumulator <<= 1;
  if ((data & 0x08) != 0)
    accumulator |= 1;
  bit_count++;
  if (accumulator > 0) {
    bit = 5;
    goto finish_bits;
  }
bit_5:
  accumulator <<= 1;
  if ((data & 0x04) != 0)
    accumulator |= 1;
  bit_count++;
  if (accumulator > 0) {
    bit = 6;
    goto finish_bits;
  }
bit_6:
  accumulator <<= 1;
  if ((data & 0x02) != 0)
    accumulator |= 1;
  bit_count++;
  if (accumulator > 0) {
    bit = 7;
    goto finish_bits;
  }
bit_7:
  accumulator <<= 1;
  if ((data & 0x01) != 0)
    accumulator |= 1;
  bit_count++;
  if (accumulator <= 0)
    goto get_byte;
  bit = 8;

finish_bits:
  if (bit_count >= 12 && accumulator == 1) {
    sp->data = (short)data;
    sp->bit = (short)((bit > 7) ? 0 : bit);
    return;
  }
  bit_count = 0;
  accumulator = 0;
  goto dispatch;

get_byte:
  if (tif->tif_rawcc <= 0)
    return;
  tif->tif_rawcc--;
  data = sp->bitmap[*tif->tif_rawcp];
  tif->tif_rawcp++;
  goto bit_0;
}

/**
 * Read one bit from fax decoder state and advance its bit cursor.
 *
 * ABI: TIFF handle arrives in EAX, with no stack arguments. The return is the
 * masked bit value left in EAX. The raw input cursor is advanced only when
 * the state cursor was zero and raw input remains.
 */
int FUN_00068bd0(void *tif_ /* @<eax> */)
{
  tiff_t *tif;
  tiff_codec_bits_t *sp;
  int data;
  short bit;
  int result;

  tif = (tiff_t *)tif_;
  sp = tif->tif_data;
  if (sp->bit == 0 && tif->tif_rawcc > 0) {
    tif->tif_rawcc--;
    sp->data = sp->bitmap[*tif->tif_rawcp];
    tif->tif_rawcp++;
  }
  data = (short)sp->data;
  bit = (short)sp->bit;
  result = *(const unsigned char *)(0x2ec370 + bit) & data;
  bit++;
  sp->bit = (short)bit;
  if (bit > 7)
    sp->bit = 0;
  return result;
}

/**
 * Allocate and initialize the private Group 3/4 fax codec state.
 *
 * ABI: TIFF handle arrives in ESI and `extra_size` is the sole cdecl stack
 * argument. Successful return is the allocated state pointer in EAX; all
 * rejected or allocation-failure paths return zero. The 0x68c40 CRT REP STOS
 * helper is deliberately not implemented: this body has no call to it.
 *
 * State offsets 0x04, 0x08, 0x0c and 0x18 are accessed through raw offsets
 * because tiff_codec_bits_t intentionally exposes only its already-proven
 * data/bit/bitmap members. The existing tiff_t and state layouts are not
 * reordered or repadded.
 */
void *FUN_00068c70(void *tif_ /* @<esi> */, int extra_size)
{
  char *tif;
  int scanline_size;
  int row_pixels;
  unsigned int allocation_size;
  tiff_codec_bits_t *state;
  char *state_end;
  const unsigned char *bitmap;

  tif = (char *)tif_;
  if (*(short *)(tif + 0x36) != 1) {
    FUN_00068a30(*(const char **)tif,
                 "Bits/sample must be 1 for Group 3/4 encoding/decoding");
    return 0;
  }

  if (*(char *)(tif + 0x0a) < 0) {
    scanline_size = FUN_0006f890(tif_);
    row_pixels = *(int *)(tif + 0x28);
  } else {
    scanline_size = TIFFScanlineSize((int)tif_);
    row_pixels = *(int *)(tif + 0x1c);
  }

  allocation_size = (unsigned int)extra_size;
  if ((*(unsigned char *)(tif + 0x68) & 1) != 0 || *(short *)(tif + 0x3a) == 4)
    allocation_size = (unsigned int)(scanline_size + 1 + extra_size);

  state = (tiff_codec_bits_t *)debug_malloc(
    allocation_size, false, "c:\\halo\\SOURCE\\bitmaps\\libtiff\\tif_fax3.c",
    0xfc);
  *(tiff_codec_bits_t **)(tif + 0x120) = state;
  if (state == 0) {
    FUN_00068a30("Fax3SetupState", "%s: No space for Fax3 state block",
                 *(const char **)tif);
    return 0;
  }

  state->rowbytes = scanline_size;
  state->rowpixels = row_pixels;
  bitmap = (const unsigned char *)0x2ecbe0;
  if ((int)*(char *)(tif + 0x08) ==
      (unsigned int)*(unsigned short *)(tif + 0x40))
    bitmap = (const unsigned char *)0x2ecce0;
  state->bitmap = bitmap;
  state->fill_white = (unsigned short)(*(short *)(tif + 0x3c) == 1);

  if ((*(unsigned char *)(tif + 0x68) & 1) == 0 &&
      *(short *)(tif + 0x3a) != 4) {
    state->fill_line = 0;
    return state;
  }

  state_end = (char *)state + extra_size + 1;
  state->fill_line = (unsigned char *)state_end;
  *(char *)(state_end - 1) = (state->fill_white == 0) ? 0 : (char)-1;
  return state;
}

/**
 * Reset fax codec state, initialize its reference line, and prime decoding.
 *
 * ABI: one cdecl TIFF pointer on the stack; returns 1 on success and 0 when
 * state allocation fails. Calls below are verified against the binary: the
 * setup call receives ESI=tif and stack extra_size 0x1c, the bit accumulator
 * call receives EAX=0 and EDI=tif, and the bit reader receives EAX=tif.
 */
int FUN_00068d80(void *tif_)
{
  char *tif;
  tiff_codec_bits_t *state;
  unsigned char *fill_ptr;
  unsigned char fill_byte;
  int fill_count;
  int bit_result;

  tif = (char *)tif_;
  state = *(tiff_codec_bits_t **)(tif + 0x120);
  if (state == 0) {
    state = (tiff_codec_bits_t *)FUN_00068c70(tif_, 0x1c);
    if (state == 0)
      return 0;
  }

  fill_ptr = state->fill_line;
  state->bit = 0;
  state->data = 0;
  state->field_10 = 0;

  if (fill_ptr != 0) {
    /* Paint the whole reference line white (0xff) for a photometrically
     * inverted image, black (0x00) otherwise.  The reference widens the flag
     * to a full byte mask with neg/sbb rather than branching on it. */
    fill_count = state->rowbytes;
    fill_byte = (unsigned char)-(state->fill_white != 0);
    while (fill_count > 0) {
      *fill_ptr = fill_byte;
      fill_ptr++;
      fill_count--;
    }
  }

  if ((*(unsigned char *)(tif + 0x09) & 2) == 0) {
    FUN_00068a70(0, tif_);
    if ((*(unsigned char *)(tif + 0x68) & 1) != 0) {
      bit_result = FUN_00068bd0(tif_);
      state->field_10 = (bit_result == 0);
    }
  }
  return 1;
}

/* The two 256-byte run-scan tables the codec state caches at 0x1c/0x20. Their
 * identity is proven by content, not by shape -- see the tiff_codec_bits_t
 * comment above. */
#define TIFF_FAX_ZERORUNS ((const unsigned char *)0x2ec3c8)
#define TIFF_FAX_ONERUNS ((const unsigned char *)0x2ec4c8)

/**
 * Reset fax codec state for encoding and pick the 2D row budget.
 *
 * By shape this is upstream libtiff tif_fax3.c's `Fax3PreEncode`: the
 * `sp->bit = 8; sp->data = 0; sp->tag = G3_1D;` triple, the reference-line
 * fill, the `is2DEncoding` guard around a yresolution-derived
 * `sp->maxk = (res > 150 ? 4 : 2); sp->k = sp->maxk-1;`, the `sp->k = sp->maxk
 * = 0;` else arm and the unconditional `return 1` all line up member for
 * member. The upstream identity is INFERRED from shape; kb.json maps the
 * address into tif_flush.obj, which is why the body lives here rather than in
 * a tif_fax3.c of its own. It is the `tif_setupencode` (0xf4) counterpart of
 * FUN_00068d80 above, which is the `tif_setupdecode` (0xf0) side, and the two
 * bodies differ in exactly four places: the extra_size handed to the state
 * allocator (0x28 here against 0x1c), `bit` being primed to 8 rather than 0,
 * the run-table pair cached on the fresh-allocation path, and this tail
 * instead of the decoder's bit-reader priming.
 *
 * Two encode-side deviations from stock upstream are real, not translation
 * artifacts. First, upstream clears the reference line to a literal 0x00;
 * this build fills it with the same photometric mask the decoder uses
 * (`fill_white ? 0xff : 0x00`). Second, upstream converts a
 * RESUNIT_CENTIMETER resolution with a single `res *= 2.54f`; this build
 * applies TWO double multiplies, 0.3937 then 1/2.54, in that order. Both
 * multipliers are separate .rdata doubles (0x260140 = 0.3937 exactly,
 * 0x260138 = 0x3fd93264c993264c, the double nearest 1/2.54) and MSVC cannot
 * fold them into one, because `(res * A) * B` is not reassociable without
 * /fp:fast -- so the pair of FMULs is what the source says, however odd the
 * resulting scale factor (~0.155) looks against upstream's intent. The
 * threshold at 0x260134 is a FLOAT 150.0, compared with `fcomp dword ptr`, so
 * the widths are not interchangeable here.
 *
 * ABI recovered from the frame at 0x69420: `push ebp / mov ebp,esp / push ebx
 * / push esi` with no `sub esp` (zero locals), so cdecl with a single stack
 * argument at [EBP+8] -- loaded into EBX at 0x69424 and RELOADED at 0x694ad
 * because BL/BH are borrowed to broadcast the fill byte in between. The EDI
 * pushed at 0x6946d and popped at 0x694bd is a mid-body save around that same
 * fill, not a prologue register. Ghidra reported `void(void)`: it saw neither
 * the `[ebp+8]` load nor the return, which is where its phantom
 * `in_stack_00000004` and `extraout_EAX` came from.
 *
 * The only call is FUN_00068c70 at 0x69436, which takes the handle in ESI
 * (`mov esi,ebx` at 0x69434) and `extra_size` on the stack (`push 0x28`,
 * `add esp,4` after). 0x28 is exactly sizeof(tiff_codec_bits_t), which
 * corroborates the four members promoted above: the decoder asks for 0x1c and
 * never touches 0x1c..0x27. A null result is returned to the caller
 * unchanged, before any state store happens.
 *
 * The reference expands the reference-line fill inline (`shr ecx,2 / rep
 * stosd`, then `and ecx,3 / rep stosb` at 0x694b0-0x694b7, with the count
 * shifted LOGICALLY while its `test ecx,ecx / jle` entry guard is SIGNED).
 * That is the compiler's fill substitution, not something the C says, and it
 * is kept here as the same byte loop FUN_00068d80 uses: an inline memset
 * expansion needs no entry guard at all, and `memset` is not linkable in this
 * build (`-nostdlib -ffreestanding -fno-builtin`).
 *
 * @param tif_ TIFF handle (declared void* so the generated header needs no
 *             libtiff types). Never null-checked.
 * @return 1 on success, 0 when the codec state could not be allocated.
 */
int FUN_00069420(void *tif_)
{
  char *tif;
  tiff_codec_bits_t *state;
  unsigned char *fill_ptr;
  unsigned char fill_byte;
  int fill_count;
  float res;
  short maxk;

  tif = (char *)tif_;
  state = *(tiff_codec_bits_t **)(tif + 0x120);
  if (state == 0) {
    state = (tiff_codec_bits_t *)FUN_00068c70(tif_, 0x28);
    if (state == 0)
      return 0;
    /* Only the freshly-allocated block gets the tables; a state the decoder
     * side already built keeps whatever pair it was given. */
    if (state->fill_white == 0) {
      state->field_1c = TIFF_FAX_ZERORUNS;
      state->field_20 = TIFF_FAX_ONERUNS;
    } else {
      state->field_1c = TIFF_FAX_ONERUNS;
      state->field_20 = TIFF_FAX_ZERORUNS;
    }
  }

  /* 0x2 is primed to a full byte so the first putbits sees an empty
   * accumulator; the decode side primes it to 0 instead. */
  state->bit = 8;
  state->data = 0;
  state->field_10 = 0;

  fill_ptr = state->fill_line;
  if (fill_ptr != 0) {
    /* Paint the whole reference line white (0xff) for a photometrically
     * inverted image, black (0x00) otherwise. The reference widens the flag to
     * a full byte mask with neg/sbb rather than branching on it. */
    fill_count = state->rowbytes;
    fill_byte = (unsigned char)-(state->fill_white != 0);
    while (fill_count > 0) {
      *fill_ptr = fill_byte;
      fill_ptr++;
      fill_count--;
    }
  }

  if ((*(unsigned char *)(tif + 0x68) & 1) != 0) {
    res = *(float *)(tif + 0x58);
    if (*(short *)(tif + 0x5c) == 3)
      res = res * 0.3937 * 0.39370078740157477;
    /* `fcomp dword ptr [0x260134] / fnstsw ax / test ah,0x41 / jz` -- the mask
     * covers C0|C3, so BOTH clear means ST(0) is strictly GREATER than the
     * threshold. This is a `>` test, not the equality the bare `jz` suggests.
     */
    maxk = (res > 150.0f) ? 4 : 2;
    state->maxk = maxk;
    state->k = (short)(maxk - 1);
    return 1;
  }
  state->maxk = 0;
  state->k = 0;
  return 1;
}

/**
 * Flush the codec's partially-filled bit accumulator into the raw output
 * buffer, so an encoded strip ends on a byte boundary.
 *
 * By shape this is upstream libtiff tif_fax3.c's `Fax3PostEncode`, whose whole
 * body is `if (sp->bit != 8) Fax3FlushBits(tif, sp); return (1);` -- the guard,
 * the flush-check-before-store order inside the macro, and the unconditional
 * `return 1` all line up. One codec difference: upstream stores the accumulator
 * itself, this build stores `bitmap[data]` (`mov cl,[ecx+edx]` at 0x69552),
 * i.e. it pushes the bit-reversal table through the same store. The upstream
 * identity is INFERRED from shape; kb.json maps the address into tif_flush.obj,
 * which is why the body lives here rather than in a tif_fax3.c of its own.
 *
 * ABI recovered from the frame at 0x69520: `push ebp / mov ebp,esp / push esi /
 * mov esi,[ebp+8] / push edi` with no `sub esp` (zero locals -- ESI=tif,
 * EDI=state), plain `ret` with no immediate and caller-side cleanup, so cdecl
 * with a single stack argument. Ghidra reported `void(void)`: it saw neither
 * the
 * `[ebp+8]` load nor the return. The return really is an int -- `mov eax,1` at
 * 0x69583 -- and it is 1 on BOTH paths, because the early-exit `jz` at 0x69533
 * lands at 0x69582, one instruction ABOVE that `mov`.
 *
 * TIFFFlushData1's int result is deliberately discarded (`add esp,4` at 0x69549
 * with no test), matching upstream's `(void) TIFFFlushData1(tif)`. The
 * DumpModeEncode neighbour in this TU tests the same call, so the difference is
 * real rather than a branch lost in translation.
 */
int FUN_00069520(void *tif_)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_codec_bits_t *sp = tif->tif_data; /* 0x69528 */

  /* 0x6952e: 16-bit compare against 8 -- a full byte means nothing is
   * buffered, so there is nothing to flush. */
  if (sp->bit != 8) {
    /* 0x69535-0x69549: SIGNED `cmp eax,[esi+0x130]` + `jl`, so the flush runs
     * once rawcc has caught up with the buffer size. Result discarded. */
    if (tif->tif_rawcc >= tif->tif_rawdatasize) {
      (void)TIFFFlushData1(tif);
    }
    /* 0x6954c-0x69575: store bitmap[data], then bump rawcp and rawcc in that
     * order (each is reloaded, incremented and stored back). */
    *tif->tif_rawcp++ = sp->bitmap[sp->data];
    tif->tif_rawcc++;
    /* 0x69577, 0x6957c: two adjacent 16-bit stores. */
    sp->data = 0;
    sp->bit = 8;
  }
  return 1; /* 0x69583 -- reached from both paths */
}

/**
 * Codec close hook: emit the six-EOL end-of-block sequence, then flush the
 * partially-filled bit accumulator, unless the caller asked to suppress it.
 *
 * By shape this is upstream libtiff tif_fax3.c's `Fax3Close`, whose whole body
 * is a `NORTC` guard around `for (i = 0; i < 6; i++) <put one EOL>;` followed
 * by a bit flush -- the six-iteration count, the single-argument helper, and
 * the trailing call to this TU's flush (0x69520, the Fax3PostEncode-shaped
 * neighbour above) all line up. The upstream identity is INFERRED from that
 * shape only: no `__FILE__` string for tif_fax3.c is stamped anywhere in this
 * body, and kb.json maps the address into tif_flush.obj, which is why the body
 * lives here. FUN_000693b0 is UNNAMED on purpose -- upstream's `Fax3PutEOL`
 * takes the same lone `TIFF*`, but nothing recovered so far proves it writes
 * an EOL code rather than some other fixed bit pattern.
 *
 * ABI recovered from the frame at 0x69590: `push ebp / mov ebp,esp / push esi /
 * mov esi,[ebp+8]` with no `sub esp` (zero locals -- ESI=tif, EDI=counter),
 * plain `ret` with caller-side `add esp,4` at each call, so cdecl with a single
 * stack argument, and both callees take that same lone pointer (one `push esi`
 * per CALL, each followed by `add esp,4`). Ghidra reported `void(void)` here as
 * it did for the flush above: it saw neither the `[ebp+8]` load nor the pushes,
 * and folded them into a following call. The return really is void -- there is
 * no `mov eax` on either path, so the 1 that 0x69520 leaves in EAX just falls
 * through untouched, matching upstream's `void (*tif_close)(TIFF*)` slot.
 *
 * `push edi` sits INSIDE the taken branch (0x6959d, popped at 0x695b8), so the
 * register-save shape differs between the two paths; the guard is written as an
 * `if` block rather than an early `return` to keep that layout.
 */
void FUN_00069590(void *tif_)
{
  tiff_t *tif = (tiff_t *)tif_;
  int i;

  /* 0x69597: `test byte ptr [esi+9],1` -- bit 0 of the second flags byte, the
   * same bit FUN_00068a50 sets and clears at 0x68a5a. Upstream's guard is
   * `(mode & FAXMODE_NORTC) == 0`, but that macro lives in the codec's private
   * state, not in the handle, so the bit is tested with a literal rather than a
   * FAXMODE_* name. */
  if ((tif->field_09 & 1) == 0) {
    /* 0x6959d-0x695ad: `mov edi,6`, then a rolled loop -- the `push esi` is
     * inside it at 0x695a3 and `dec edi / jnz` closes it, so the six calls are
     * a real loop in the binary rather than an unrolled run. */
    i = 6;
    do {
      FUN_000693b0(tif);
      i--;
    } while (i != 0);
    /* 0x695af: int result discarded with no test, as at 0x69549 above. */
    (void)FUN_00069520(tif);
  }
}

/**
 * Release the codec private state hanging off a TIFF handle.
 *
 * Upstream libtiff tif_fax3.c's `Fax3Cleanup`: `if (tif->tif_data) {
 * _TIFFfree(tif->tif_data); tif->tif_data = NULL; }`. Bungie's _TIFFfree
 * expands to the debug allocator, so the call carries __FILE__/__LINE__ -- and
 * that __FILE__ is `c:\halo\SOURCE\bitmaps\libtiff\tif_fax3.c`, the string at
 * 0x260058 pushed here. That literal is what CONFIRMS the tif_fax3.c origin of
 * this body; the two neighbours above are the same codec by shape only, with no
 * `__FILE__` of their own. Line 1077 (0x435) is the free site. kb.json maps the
 * address into tif_flush.obj, which is why the body lives here.
 *
 * The structural twin FUN_0006cac0 (tif_open.c) is the same cleanup hook for a
 * different codec: same handle field, same allocator call, differing only in
 * the file/line literals.
 *
 * ABI recovered from the frame at 0x695c0: `push ebp / mov ebp,esp / push esi /
 * mov esi,[ebp+8]` with no `sub esp` (zero locals -- ESI=tif), plain `ret` with
 * the argument cleanup done caller-side (`add esp,0xc` after the CALL), so
 * cdecl with a single stack argument. Ghidra reported `void(void)` here as it
 * did for the two bodies above: it saw neither the `[ebp+8]` load nor the push,
 * and surfaced the parameter as `in_stack_00000004`. There is no `mov eax` on
 * either path, so the return really is void, matching upstream's
 * `void (*tif_cleanup)(TIFF*)` slot.
 *
 * @param tif_ TIFF handle; may carry a null tif_data, in which case nothing
 *             happens. The handle pointer itself is never checked.
 */
void FUN_000695c0(void *tif_)
{
  tiff_t *tif = (tiff_t *)tif_;
  /* 0x695c7 `mov eax,[esi+0x120]`: the field is loaded ONCE, and the same EAX
   * is both tested and pushed as the free argument -- hence the local. Reading
   * `tif->tif_data` again at the call site would emit a second load. */
  tiff_codec_bits_t *sp = tif->tif_data;

  if (sp) {
    debug_free(sp, "c:\\halo\\SOURCE\\bitmaps\\libtiff\\tif_fax3.c", 0x435);
    /* 0x695e7: the null store is inside the taken branch, AFTER the free, and
     * writes a dword immediate to the field rather than going through `sp`. */
    tif->tif_data = 0;
  }
}

/**
 * Install one of the CCITT Group 3/4 ("fax") codecs into a TIFF handle.
 *
 * This is a `TIFFInitCCITT*` entry point from upstream libtiff tif_fax3.c --
 * the same TU that stamps the `c:\halo\SOURCE\bitmaps\libtiff\tif_fax3.c`
 * string used by FUN_000695c0 above -- with upstream's shared `InitCCITTFax3`
 * helper fully inlined: this body issues NO CALL at all, it only takes the six
 * addresses below as function-pointer VALUES
 * (`mov ecx,0x6a070` / `mov dword ptr [eax+imm],0x68d80`) and stores them.
 *
 * WHICH variant (RLE / RLEW / Fax3 / Fax4) is UNRESOLVED. The three sibling
 * installers at 0x6a210, 0x6a2a0 and 0x6a310 are still unrecovered, and
 * nothing here writes a `mode`/`groupoptions` field that would separate them,
 * so the decode/encode pair 0x6a070 / 0x69f30 is the only discriminator and it
 * is not yet identified. The name therefore stays FUN_0006a190.
 *
 * The batch that scheduled this lift labelled the target `tif_dumpmode.c`.
 * That is wrong: upstream's TIFFInitDumpMode is FUN_00068970, already recovered
 * higher up in this TU (it installs the dump encode/decode pair 0x68780/0x68890
 * plus tif_seek, and touches no setup or cleanup slot). This body installs a
 * disjoint set -- setupdecode/setupencode/postencode and close/cleanup, all of
 * them Fax3 helpers already resident here -- so it is filed with its kb.json
 * object (tif_flush.obj -> this file) rather than in a new tif_dumpmode.c.
 *
 * ABI recovered from the frame at 0x6a190 (0x6a190-0x6a20a): `push ebp /
 * mov ebp,esp` with NO `sub esp` (the only temporaries are the ECX the two
 * shared immediates are staged in and the CL the +0x9 flag byte is read into),
 * one stack argument read at [ebp+8] into EAX, `mov eax,1 / pop ebp / ret` with
 * no immediate -- cdecl, caller cleans, and the return type is `int`, not the
 * `void` kb.json carried. The decompiler surfaced the parameter as
 * `in_stack_00000004` and dropped the `mov eax,1` at 0x6a205 purely because of
 * that stale `(void)` prototype; every upstream `TIFFInitCCITT*` returns a
 * success flag its caller tests, so shipping this as `void` would have silently
 * dropped it (lift-silent-bugs section 16, void-EAX).
 *
 * Upstream takes a second `int scheme` argument which it discards. Nothing here
 * reads [ebp+0xc], so only the first parameter is binary-proven and only it is
 * declared; cdecl makes an extra pushed argument harmless, exactly as for
 * FUN_00068970.
 *
 * Store order below is the emitted order. MSVC stages 0x6a070 in ECX once for
 * the three decode slots (0x6a19b-0x6a1a9) and 0x69f30 in ECX once for the
 * three encode slots (0x6a1af-0x6a1bd), then writes the five remaining slots as
 * plain immediates. The two flag bytes are handled asymmetrically and are left
 * in source order for the scheduler to split: `[eax+0xa] |= 0x20` is a
 * read-modify-write in place at 0x6a1ca, while `[eax+9]` is read into CL and
 * OR'd with 1 early (0x6a1c4-0x6a1c7) but not written back until 0x6a200.
 *
 * 0x118 (tif_seek) is NOT written here -- it is the dump codec's slot and this
 * body leaves it alone.
 */
int FUN_0006a190(void *tif_)
{
  tiff_t *tif = (tiff_t *)tif_;

  /* 0x6a19b-0x6a1a9. One ECX load, three stores. */
  tif->tif_decoderow = (tiff_code_method_t)FUN_0006a070;
  tif->tif_decodestrip = (tiff_code_method_t)FUN_0006a070;
  tif->tif_decodetile = (tiff_code_method_t)FUN_0006a070;

  /* 0x6a1af-0x6a1bd. A second ECX load, three more stores. */
  tif->tif_encoderow = (tiff_code_method_t)FUN_00069f30;
  tif->tif_encodestrip = (tiff_code_method_t)FUN_00069f30;
  tif->tif_encodetile = (tiff_code_method_t)FUN_00069f30;

  /* 0x6a1ca: `or byte ptr [eax+0xa],0x20`. Upstream's
   * `tif->tif_flags |= TIFF_NOBITREV` -- "we handle bit reversal ourselves" --
   * which is how every fax installer opens after the codec slots. The bit is
   * set with a literal rather than a TIFF_* macro because whether field_0a is
   * byte 2 of an upstream `uint32 tif_flags` based at 0x08 is UNPROVEN: under
   * that reading TIFF_NOBITREV (0x100) would land in field_09, not here, so the
   * numbering in Bungie's copy does not match stock libtiff and naming the bit
   * would overstate what the binary shows. Same reasoning as field_09 above. */
  tif->field_0a.b |= 0x20;

  /* 0x6a1cd-0x6a1e1. Immediate stores, no register staging. Upstream's inlined
   * InitCCITTFax3 body: Fax3SetupState into both setup slots in stock libtiff,
   * but this build stores two DIFFERENT addresses (0x68d80 decode, 0x69420
   * encode), so the two slots are kept distinct here rather than collapsed. */
  tif->tif_setupdecode = (tiff_bool_method_t)FUN_00068d80;
  tif->tif_setupencode = (tiff_bool_method_t)FUN_00069420;
  tif->tif_postencode = FUN_00069520;

  /* 0x6a1e8-0x6a1f9. */
  tif->tif_close = FUN_00069590;
  tif->tif_cleanup = FUN_000695c0;

  /* Read into CL at 0x6a1c4-0x6a1c7 and stored back at 0x6a200 -- MSVC hoists
   * the load and OR ahead of the +0xa flag byte and keeps the value live in CL
   * across the five immediate stores above, none of which touch CL. The
   * statement is written LAST anyway, matching the decompiler's ordering (which
   * ranks the two flag bytes by their STORE addresses, 0x6a1ca vs 0x6a200):
   * moving it up to where the hoisted read lands was MEASURED and made the
   * match WORSE (95.7% -> 95.5%, operand-normalized 95.7% -> 90.9%, and the
   * instruction count moved off 23/23), because VC71 then materializes the
   * read-modify-write in place instead of sinking the store. The residual ~4.3%
   * gap is exactly this one hoisted `movb 0x9(%eax),%cl`, and it is a
   * scheduling artifact, not a logic difference.
   *
   * Bit 0 of field_09 is the same bit FUN_00068a50 sets and clears; its
   * upstream identity is unproven, so it too is a literal. */
  tif->field_09 |= 1;

  /* 0x6a205: `mov eax,1`. Unconditional success -- nothing above can fail. */
  return 1;
}

/* FUN_0006a210 @ 0x6a210-0x6a259 (74 bytes) -- upstream libtiff's
 * `TIFFFlushData` from tif_flush.c, the routine this object is named after.
 * Transcribed from upstream and adapted to the observed control flow rather
 * than reshaped from the decompiler, which lost the parameter and the return
 * value and reported the body as `void(void)` with an `in_stack_00000004`.
 *
 * Frame is `push ebp; mov ebp,esp; push esi` with no `sub esp` -- no locals get
 * a slot, and ESI holds the single stack parameter for the whole body
 * (`mov esi,[ebp+8]` at 0x6a213). cdecl, one pushed argument at both call
 * sites, EAX live at every RET, so the prototype is `int (void *tif)`.
 *
 * Two deviations from stock libtiff 3.x, both proven from the disassembly:
 *
 *  1. The early-out returns 0, not 1. `test al,8; jz 0x6a249` lands on
 *     `xor eax,eax; pop esi; pop ebp; ret`. Stock TIFFFlushData returns 1 when
 *     TIFF_BEENWRITING is clear (nothing buffered is not an error); Bungie's
 *     copy returns 0, which is the same value its failure paths return.
 *
 *  2. The postencode method pointer is NULL-checked, and a NULL pointer does
 *     NOT abort the flush -- `mov eax,[esi+0xf8]; test eax,eax; jz 0x6a24e`
 *     jumps to the TIFFFlushData1 tail, not to the `xor eax,eax` epilogue.
 *     Stock libtiff calls through the slot unconditionally. The short-circuit
 *     `&&` reproduces both edges: NULL skips the call and still flushes, a
 *     zero return from the call bails with 0.
 *
 * Bit identities: 0x8 is upstream's TIFF_BEENWRITING and 0x200 upstream's
 * TIFF_POSTENCODE under the reading that 0x0a is a `uint16 tif_flags`, but this
 * build's bit numbering does not match stock libtiff (see `field_09` and
 * `field_0a`), so both stay literals.
 *
 * noinline (VC71 verification only): the original build emits this out of line
 * and CALLs it -- FUN_0006a260 at 0x6a26f has a real `call 0x6a210` with
 * `push esi` / `add esp,4` around it. Because that caller now lives in this
 * same TU with this body in scope, cl.exe inlines the whole thing into it: that
 * alone held FUN_0006a260 at 76.5% (42 insns against the reference's 26) and
 * produced a spurious [LOADW-WARN] blaming FUN_0006a260 for the `movw
 * 0xa(%esi),%ax` at 0x6a216 that belongs to THIS function's word-width flags
 * access. MEASURED both ways.
 *
 * The guard is `_MSC_VER && !__clang__` because our clang build targets
 * i386-pc-win32 and therefore also defines _MSC_VER; this must apply to cl.exe
 * ONLY and must never change the shipped binary's codegen. */
#if defined(_MSC_VER) && !defined(__clang__)
__declspec(noinline)
#endif
int FUN_0006a210(void *tif_)
{
  tiff_t *tif = (tiff_t *)tif_;

  /* 0x6a216-0x6a21d: `mov ax,word ptr [esi+0xa]; test al,8; jz 0x6a249`.
   * The flags word is re-read at each use below rather than cached in a local:
   * MSVC folds the three reads into the ONE load at 0x6a216, and caching it in
   * a `unsigned short` local was MEASURED to score WORSE (81.5% -> the local
   * form let VC71 narrow the 0x200 test to a single `test ah,2` and shrink the
   * write-back immediate to 0xfdff, costing three instructions against the
   * reference's `mov ecx,eax; and ecx,0x200; testw cx,cx` and its full-width
   * `and eax,0xfffffdff`). Reading the member directly is also the upstream
   * form, which spells all three uses as `tif->tif_flags`. */
  if ((tif->field_0a.w & 8) == 0) {
    return 0;
  }

  /* 0x6a21f-0x6a226: `mov ecx,eax; and ecx,0x200; test cx,cx; jz 0x6a24e` --
   * the postencode bit is masked into a scratch register (EAX still holds the
   * value the write-back needs) and tested 16 bits wide, which is what pins the
   * field to `unsigned short`; a clear bit jumps straight to the tail flush.
   *
   * VC71 collapses this to the single `test ah,2` and will not emit the
   * three-instruction form from any C spelling. MEASURED, by compiling the TU
   * and disassembling the object: reading the member directly, caching it in an
   * `unsigned short` local, and materialising the masked value into a separate
   * `unsigned short` temp all produce `test $0x2,%ah`. Bit 9 sits in AH, so the
   * narrowing is always available to the optimiser, and the original's scratch
   * copy is an artifact of Bungie's compiler settings rather than of the
   * source. Those two instructions -- plus the write-back immediate below --
   * are the ENTIRE residual gap: the compiled body is otherwise
   * instruction-identical to the reference, 29 instructions against its 31,
   * same order, same branches. */
  if ((tif->field_0a.w & 0x200) != 0) {
    /* 0x6a228-0x6a231: `and eax,0xfffffdff; mov word ptr [esi+0xa],ax` --
     * cleared from the value already in hand and stored back as a WORD, which
     * is why the byte view at 0x0a cannot express this statement. VC71 narrows
     * the immediate to `and $0xfdff` because only AX is stored -- MEASURED with
     * both `&= ~0x200` and `= (unsigned short)(flags & ~0x200)`, so the 32-bit
     * 0xfffffdff is unreachable here. Operand-level only: the mnemonic and the
     * store both match, so this costs the operand-normalized % and not the
     * official one. */
    tif->field_0a.w &= ~0x200;

    /* 0x6a234-0x6a247: load the slot once into EAX, test it, then
     * `push esi; call eax; add esp,4; test eax,eax; jnz 0x6a24e`. */
    if (tif->tif_postencode != (tiff_bool_method_t)0 &&
        (*tif->tif_postencode)(tif) == 0) {
      return 0;
    }
  }

  /* 0x6a24e-0x6a259: `push esi; call 0x6fe10; add esp,4; pop esi; pop ebp;
   * ret` -- EAX is untouched after the call, so its result is the return
   * value. */
  return TIFFFlushData1(tif);
}

/* FUN_0006a260 @ 0x6a260-0x6a29a (59 bytes, 27 instructions) -- upstream
 * libtiff's `TIFFFlush` from tif_flush.c, the other half of the pair this
 * object is named after. Transcribed from upstream rather than reshaped from
 * the decompiler: with both callees mis-declared `void(void)` in kb.json the
 * decompile rendered the body as `FUN_0006a210(); if (extraout_EAX == 0)`, a
 * comma-expression artifact of the lost parameter and return value, not the
 * original's shape. Upstream is reproduced verbatim in structure:
 *
 *     if (tif->tif_mode != O_RDONLY) {
 *       if (!TIFFFlushData(tif)) return (0);
 *       if ((tif->tif_flags & TIFF_DIRTYDIRECT) && !TIFFWriteDirectory(tif))
 *         return (0);
 *     }
 *     return (1);
 *
 * Frame is `push ebp; mov ebp,esp; push esi` with no `sub esp` -- zero locals
 * get a slot, and ESI holds the single stack parameter for the whole body
 * (`mov esi,[ebp+8]` at 0x6a264). cdecl, one pushed argument at both call
 * sites with `add esp,4` cleanup at each, EAX live at both RETs, so the
 * prototype is `int (void *tif)`. The `tiff_t *` bind below is deliberately
 * the ONLY local: any second local would earn a `sub esp` the reference does
 * not have.
 *
 * Two epilogues, not one: `xor eax,eax; pop esi; pop ebp; ret` at 0x6a28e and
 * `mov eax,1; pop esi; pop ebp; ret` at 0x6a293. Both `return 0;` statements
 * share the first and both `return 1;` paths share the second -- MSVC folds
 * each pair, so upstream's two literal `return (0);` statements are the right
 * spelling and do not cost an instruction.
 *
 * O_RDONLY is 0 and TIFF_DIRTYDIRECT is 0x2 upstream, and both values match
 * this build, but the flags bit stays a literal for the same reason the rest of
 * this TU's bits do: the numbering at 0x0a does not otherwise line up with
 * stock libtiff (see `field_09` and `field_0a`), so a TIFF_* macro would assert
 * an identity that is not proven here. */
int FUN_0006a260(void *tif_)
{
  tiff_t *tif = (tiff_t *)tif_;

  /* 0x6a267-0x6a26c: `cmp word ptr [esi+6],0; je 0x6a293` -- a WORD compare,
   * which is what pins tif_mode to 16 bits; read-only handles skip the entire
   * body and fall straight to `mov eax,1`. */
  if (tif->tif_mode != 0) {
    /* 0x6a26e-0x6a279: `push esi; call 0x6a210; add esp,4; test eax,eax;
     * je 0x6a28e`. The result IS consumed -- a zero flush is the failure edge
     * and returns 0, so this call must not be spelled as a bare statement. */
    if (!FUN_0006a210(tif)) {
      return 0;
    }

    /* 0x6a27b-0x6a28c: `test byte ptr [esi+0xa],2; je 0x6a293` then
     * `push esi; call 0x680a0; add esp,4; test eax,eax; jne 0x6a293`.
     *
     * The flags test is a BYTE load here, so this uses the union's `.b` view
     * and not the `.w` view FUN_0006a210 needs for its 0x200 bit -- bit 1 is
     * reachable through the low byte, and widening the access to 16 bits would
     * emit `test word` against a `test byte` reference.
     *
     * Both edges of the `&&` land on `mov eax,1`, not on the failure epilogue:
     * a clear bit jumps to 0x6a293 directly, and a NON-zero return from
     * TIFFWriteDirectory jumps there too. Only the taken-and-failed path falls
     * through into `xor eax,eax`, which is exactly what the short-circuit
     * form expresses. */
    if ((tif->field_0a.b & 2) && !FUN_000680a0(tif)) {
      return 0;
    }
  }

  /* 0x6a293: `mov eax,1`. */
  return 1;
}

/* ---------------------------------------------------------------------------
 * tif_getimage.c state touched by setorientation (0x6a310).
 *
 * kb.json maps 0x6a310 into tif_flush.obj, but by shape the body is upstream
 * libtiff tif_getimage.c:setorientation -- see the doc comment below. Both
 * globals it touches are absolute addresses with no register base, so this
 * build still keeps the RGBA-decoder state in file statics rather than in the
 * TIFFRGBAImage struct later libtiff versions use. The same static block is
 * partially recovered in tif_open.c (bitspersample 0x3340fc, samplesperpixel
 * 0x3340f8, photometric 0x3340f4, stoponerr 0x3340e0); 0x3340f0 slots into
 * that run directly below photometric, which is what makes `orientation` the
 * name rather than a guess.
 *
 * `orientation` is 16 bit and that width is PROVEN, not assumed: the only read
 * is `movzx eax, word ptr [0x3340f0]` (0x6a326) and both stores are
 * `mov word ptr [0x3340f0], imm` (0x6a351, 0x6a372). Widening it to int would
 * emit a dword load against a word reference.
 *
 * `filename` is a dword load (`mov ecx,[0x3340dc]` at 0x6a33d, `mov edx,...`
 * at 0x6a35e) feeding TIFFWarning's `module` parameter, which upstream fills
 * with `TIFFFileName(tif)`. That the global is a cached copy of the file name
 * is INFERRED from that role -- the binary proves only that 0x3340dc holds a
 * pointer TIFFWarning is happy to take as its module string. Note there is no
 * `call TIFFFileName` here at all, so this build hoisted the name into a
 * static instead of re-deriving it per warning. */
#define orientation (*(unsigned short *)0x3340f0)
#define filename (*(char **)0x3340dc)

/* The bilevel/greyscale expansion table, same static tif_open.c frees at
 * tif_getimage.c line 127. tif_open.c only ever assigns and frees it, so it is
 * declared `void *` there; FUN_0006a910 below is what proves the pointee type,
 * and the proof is three chained loads per pixel:
 *   mov   edi, [0x3340c4]      0x6a92b  the table, loaded ONCE for the whole
 *                                       call, ahead of both loops
 *   movzx ebx, byte ptr [ecx]  0x6a950  the source byte
 *   mov   ebx, [edi+ebx*4]     0x6a953  BWmap[byte] -- the *4 scale makes this
 *                                       an array of POINTERS, not of pixels
 *   mov   ebx, [ebx]           0x6a956  *that pointer -- the pixel value
 * so the storage at 0x3340c4 has type `unsigned long **`. The double
 * indirection is load-bearing: collapsing it to one load would store the row
 * pointer itself as a pixel, which is a wrong-pixel bug no match score would
 * notice. */
#define BWmap (*(unsigned long ***)0x3340c4)

#define TIFFTAG_ORIENTATION 274 /* 0x112, pushed at 0x6a31b */

/* The standard TIFF tag-6 enumeration. The jump table at 0x6a384 spans exactly
 * eight entries -- `dec eax; cmp eax,7; ja` at 0x6a330 -- so the switch covers
 * orientation values 1..8, and the two target groups it forms (idx 2/6/7 ->
 * 0x6a33d, idx 1/4/5 -> 0x6a35e) are what pin these names onto the values:
 * only the ORIENTATION_* assignment below puts the three bottom-row codes and
 * the three top-row codes in those groups. */
#define ORIENTATION_TOPLEFT 1 /* jump idx 0 -> 0x6a37b (y = h-1) */
#define ORIENTATION_TOPRIGHT                       \
  2 /* jump idx 1 -> 0x6a35e (warn, force TOPLEFT) \
     */
#define ORIENTATION_BOTRIGHT                       \
  3 /* jump idx 2 -> 0x6a33d (warn, force BOTLEFT) \
     */
#define ORIENTATION_BOTLEFT 4 /* jump idx 3 -> 0x6a35a (y = 0) */
#define ORIENTATION_LEFTTOP 5 /* jump idx 4 -> 0x6a35e */
#define ORIENTATION_RIGHTTOP 6 /* jump idx 5 -> 0x6a35e */
#define ORIENTATION_RIGHTBOT 7 /* jump idx 6 -> 0x6a33d */
#define ORIENTATION_LEFTBOT 8 /* jump idx 7 -> 0x6a33d */

/**
 * Normalise the image orientation and return the first raster row to fill.
 *
 * Upstream libtiff tif_getimage.c:setorientation, transcribed from the
 * upstream source rather than reshaped from the decompiler -- which lost both
 * parameters and the return value and reported the body as `void(void)`. The
 * real ABI is recovered from the frame at 0x6a310: `push ebp / mov ebp,esp`
 * with NO `sub esp` (no locals, `y` lives entirely in EAX), two stack
 * arguments at [ebp+8] and [ebp+0xc], cdecl (both callee arg blocks are
 * cleaned up caller-side with `add esp`), and an EAX return.
 *
 * The second parameter and the return value are what the decompiler's
 * `void(void)` hid, and they are the whole point of the function: the
 * bottom-left arm returns 0 (`xor eax,eax` at 0x6a35a) and the top-left arm
 * returns `h - 1` (`mov eax,[ebp+0xc]; dec eax` at 0x6a37b). That pair is
 * upstream's `y = 0` / `y = h-1`, i.e. the row the caller starts writing at,
 * counting up from the bottom of the raster or down from the top. Nothing
 * else in the body explains a `param - 1` return, and no caller is recovered
 * yet to cross-check it, so the row-origin reading is INFERRED from the
 * upstream shape -- but it is the only reading consistent with both exits.
 *
 * Upstream takes a `TIFFRGBAImage*` and reaches the handle as `img->tif`.
 * Here [ebp+8] is pushed straight through to TIFFGetFieldDefaulted with no
 * dereference (0x6a313, 0x6a320), so in this build the first parameter IS the
 * TIFF handle -- consistent with the orientation living in a file static
 * instead of in an img struct.
 *
 * The fall-through switch is load-bearing and must not be flattened into
 * per-case returns: the 3/7/8 arm falls into case 4 (its `xor eax,eax` sits
 * directly after the `mov word ptr [0x3340f0],4` store at 0x6a351) and the
 * 2/5/6/default arm falls into case 1 (its `mov eax,[ebp+0xc]` sits directly
 * after the `mov word ptr [0x3340f0],1` store at 0x6a372). Both groupings are
 * read off the jump table at 0x6a384, not from upstream: 3/7/8 go bottom-left
 * and 2/5/6 go top-left in THIS build.
 */
unsigned long FUN_0006a310(void *tif, unsigned long h)
{
  unsigned long y;

  /* 0x6a316-0x6a321: `push 0x3340f0; push 0x112; push eax; call 0x64ec0;
   * add esp,0xc` -- cdecl, C-order (tif, tag, &value). The int return is
   * genuinely discarded: the next instruction (0x6a326) reloads the global
   * into EAX, and upstream ignores TIFFGetFieldDefaulted's result here too,
   * because the tag is defaulted and cannot fail. */
  FUN_00064ec0((int)tif, TIFFTAG_ORIENTATION, &orientation);

  /* 0x6a326-0x6a336: `movzx eax, word ptr [0x3340f0]` -- note the load is
   * scheduled BEFORE the `add esp,0xc` -- then `dec eax; cmp eax,7; ja
   * <default>; jmp [eax*4+0x6a384]`. */
  switch (orientation) {
  case ORIENTATION_BOTRIGHT:
  case ORIENTATION_RIGHTBOT: /* XXX */
  case ORIENTATION_LEFTBOT: /* XXX */
    /* 0x6a33d-0x6a351: `mov ecx,[0x3340dc]; push 0x260224; push ecx;
     * call 0x6f9d0; add esp,8` then `mov word ptr [0x3340f0],4`. The
     * `add esp,8` matches exactly two pushes: TIFFWarning is variadic but is
     * called here with module + format only and no variadic values, so the
     * two-arg call is correct and must NOT be padded to three. */
    FUN_0006f9d0(filename, "using bottom-left orientation");
    orientation = ORIENTATION_BOTLEFT;
    /* fall through */
  case ORIENTATION_BOTLEFT:
    /* 0x6a35a: `xor eax,eax`. */
    y = 0;
    break;
  case ORIENTATION_TOPRIGHT:
  case ORIENTATION_RIGHTTOP: /* XXX */
  case ORIENTATION_LEFTTOP: /* XXX */
  default:
    /* 0x6a35e-0x6a372: the mirror of the arm above, with EDX instead of ECX
     * as the scratch register and 0x260208 as the format string. */
    FUN_0006f9d0(filename, "using top-left orientation");
    orientation = ORIENTATION_TOPLEFT;
    /* fall through */
  case ORIENTATION_TOPLEFT:
    /* 0x6a37b-0x6a37e: `mov eax,[ebp+0xc]; dec eax`. */
    y = h - 1;
    break;
  }
  return y;
}

/**
 * Expand one contiguous 8-bit greyscale/palette tile into the 32bpp raster.
 *
 * Upstream libtiff tif_getimage.c putgreytile -- the `UNROLL8(w, ,
 * *tp++ = BWmap[*pp++][0])` body -- grouped by kb.json into tif_flush.obj like
 * setorientation above, and like setorientation it reaches its table through a
 * file static because this build predates the TIFFRGBAImage struct.
 *
 * ABI recovered from the frame at 0x6a910: `push ebp / mov ebp,esp / push ecx`
 * (that single push is the one 4-byte local at EBP-4, see toskew below), plus
 * `push ebx / esi / edi` scheduled *after* the zero-row guard, and a plain
 * `mov esp,ebp / pop ebp / ret` with no `add esp` -- cdecl, void return, no
 * register arguments. The decompiler reported this as `void(void)` and put
 * every parameter in an `in_stack_...` pseudo-local, so the kb.json decl it
 * seeded (`void FUN_0006a910(void)`) was a placeholder; the seven stack slots
 * below are read straight off the disassembly:
 *   EBP+0x08  cp        `mov eax,[ebp+8]`     0x6a93a  dword cursor, `mov
 *                                                      [eax],ebx` + `add eax,4`
 *   EBP+0x0c  pp        `mov ecx,[ebp+0xc]`   0x6a926  byte cursor, `movzx
 *                                                      ebx,byte ptr [ecx]`
 *   EBP+0x10  (unread)  --                             see below
 *   EBP+0x14  w         `mov edx,[ebp+0x14]`  0x6a934  and again at 0x6a964
 *   EBP+0x18  h         `mov eax,[ebp+0x18]`  0x6a914  row counter, written
 *                                                      back at 0x6a987
 *   EBP+0x1c  fromskew  `mov ebx,[ebp+0x1c]`  0x6a97c  `add ecx,ebx` 0x6a984
 *   EBP+0x20  toskew    `mov ecx,[ebp+0x20]`  0x6a91b  `lea edx,[ecx*4]`
 *
 * The slot at EBP+0x10 is never read anywhere in the body. It is one of
 * upstream's `(void) x; (void) y;` tile-origin arguments, but WHICH one is
 * unknown -- upstream's put routines take both and this build's typedef takes
 * only one, so there is nothing to disambiguate against and the parameter
 * keeps a deliberately empty name. It must stay in the signature regardless:
 * dropping it would shift w/h/fromskew/toskew down one slot each and silently
 * mis-call the function from every future caller.
 *
 * Every counter is unsigned, and that is read off the branch mnemonics rather
 * than assumed: the row guard is `test eax,eax; jbe` (0x6a917), the unrolled
 * entry is `cmp edx,8; jb` (0x6a940) and the remainder test is `test esi,esi;
 * jbe` (0x6a967). Signed counters would emit jle/jl and invert the zero-trip
 * and remainder guards on a negative w or h.
 *
 * toskew is scaled ONCE, before the row loop: `mov ecx,[ebp+0x20]; lea
 * edx,[ecx*4]; mov [ebp-4],edx` (0x6a91b-0x6a931), and each row then does
 * `mov esi,[ebp-4]; add eax,esi` (0x6a979, 0x6a97f). That hoisted `toskew * 4`
 * is the sole stack local and it is the compiler's doing, not the source's --
 * `cp` is a dword pointer, so `cp += toskew` is the correct spelling here.
 * Pre-scaling it in C would advance the raster four times too far.
 *
 * WARNING -- the unrolled loop below is deliberately NOT eight copies, and
 * this is the one thing to leave alone. Upstream's UNROLL8 expands op2 through
 * REPEAT8, so upstream consumes eight source bytes and writes eight raster
 * pixels per iteration of the `_x >= 8` loop, with a Duff's-device REPEAT for
 * the tail. This build emits the op2 body exactly ONCE in each arm
 * (0x6a950-0x6a95d for the loop, 0x6a96b-0x6a978 for the tail) while still
 * counting `sub esi,8` (0x6a95e) and running `w >> 3` iterations (`shr edx,3`,
 * 0x6a947), so a row of w pixels advances the cursors by only w/8 (+1) pixels.
 * Whether Bungie reduced the REPEAT macros for size or the paths are simply
 * dead here is not recoverable from the binary, but the emitted arithmetic is
 * unambiguous. "Fixing" this into eight copies would change behaviour.
 *
 * The `mov edx,[ebp+0x14]` at 0x6a964 sits on the taken path only: EDX doubles
 * as the `w >> 3` trip counter and is left at zero by `dec edx; jne`, so w has
 * to be reloaded for the next row's `cmp edx,8`, whereas the w < 8 path never
 * touched it. That is register allocation around a live-after-loop counter,
 * not a second source read.
 */
void FUN_0006a910(unsigned long *cp, unsigned char *pp,
                  unsigned long unused_arg, unsigned long w, unsigned long h,
                  long fromskew, long toskew)
{
  unsigned long **bwmap;
  unsigned long x;

  /* Upstream discards the tile-origin arguments the same way. */
  (void)unused_arg;

  /* 0x6a92b: hoisted out of both loops into EDI for the whole call. */
  bwmap = BWmap;

  /* 0x6a914-0x6a919 guard, 0x6a981-0x6a98a decrement-and-branch: the counter
   * lives in its own argument slot (`mov [ebp+0x18],esi` at 0x6a987) because
   * all six usable GPRs are already taken by cp, pp, w, the table and the two
   * scratch values, so h is genuinely modified in place here. */
  while (h-- > 0) {
    /* 0x6a940-0x6a962. `x` is upstream's UNROLL8 `_x`; it stays live past the
     * loop for the tail test, which is why the compiler keeps `sub esi,8`
     * alongside the separate `dec edx` trip count. */
    for (x = w; x >= 8; x -= 8)
      *cp++ = bwmap[*pp++][0];

    /* 0x6a967-0x6a978. One copy, not REPEAT(_x, op2) -- see the warning
     * above. */
    if (x > 0)
      *cp++ = bwmap[*pp++][0];

    /* 0x6a97f: `add eax,esi` with ESI = the pre-scaled toskew*4. */
    cp += toskew;
    /* 0x6a984: `add ecx,ebx`, unscaled -- pp is a byte cursor. */
    pp += fromskew;
  }
}

/**
 * Expand one 4-bit colormapped tile into the 32-bit RGBA raster.
 *
 * Transcribed from the vendored libtiff (tif_getimage.c `put4bitcmaptile`)
 * rather than reshaped from the decompiler, which lost every parameter and
 * reported the body as `void(void)`. One build-specific delta: the expansion
 * table comes from the file-scope static at 0x3340c4 instead of an `img`
 * struct field.
 *
 * Which writer this is, is proven by the dispatcher at 0x6b858 (FUN_0006b780
 * in this file): the PHOTOMETRIC_PALETTE inner table selects this address for
 * `bitspersample == 4`, alongside FUN_0006a910 for 8. So the table this
 * function reads -- 0x3340c4, spelled `BWmap` above -- is upstream's PALmap,
 * not upstream's BWmap; the grey writers (FUN_0006ac60 and the 4/2/1-bit
 * MINISWHITE/MINISBLACK arms) read the OTHER static, 0x3340c8, spelled `Map`
 * above. The two macro names in this TU are therefore swapped relative to
 * upstream. Left alone here rather than renamed, to keep this lift to one
 * function; the rename is codegen-neutral and belongs in its own pass.
 *
 * The seven cdecl stack slots match the sibling writers exactly; only the
 * ORDER is INFERRED, from upstream's prototype collapsed onto this build's
 * frame. Slot +0x10 is NEVER READ (upstream's discarded tile origin) and must
 * stay, or w/h/fromskew/toskew all shift down one slot.
 *   EBP+0x08 -> ecx  cp        the dword raster cursor
 *   EBP+0x0c -> esi  pp        the byte source cursor
 *   EBP+0x10         NEVER READ
 *   EBP+0x14 -> edi  w         reloaded per row at 0x6aa02 for the next
 *                              `cmp`, because the unrolled loop leaves its
 *                              trip counter at zero
 *   EBP+0x18         h         counted down in its own argument slot
 *                              (`dec dword ptr [ebp+0x18]` at 0x6aa26)
 *   EBP+0x1c         fromskew  halved in place at entry, see below
 *   EBP+0x20         toskew    pre-scaled `<< 2` into the single EBP-4 local
 *
 * Two things the shape depends on, both read off the disassembly:
 *
 * 1. `fromskew /= 2` is SIGNED and happens at entry, BEFORE the h guard:
 *    `cdq; sub eax,edx; sar eax,1` at 0x6a9a7-0x6a9ac, stored back to
 *    [ebp+0x1c] at 0x6a9b1, with the `test ecx,ecx; jbe` row guard at
 *    0x6a9ae/0x6a9b4 after it. Spelling it `>> 1` would drop the CDQ pair and
 *    round the wrong way for a negative skew.
 *
 * 2. The loop is upstream's UNROLL2 in its op1/op2 form -- the table lookup is
 *    op1 (once per source byte) and the pixel store is op2 (REPEAT2'd in the
 *    body, emitted once in the tail). The binary settles this: 0x6a9e0-0x6aa00
 *    loads [0x3340c4] exactly ONCE per iteration and keeps the resolved entry
 *    pointer in EDX across the first store (`mov eax,[edx]` / `mov [ecx],eax`
 *    / `mov edx,[edx+4]`), which the two-subscript form `PALmap[*pp][0]` /
 *    `PALmap[*pp][1]` cannot produce -- the intervening store to *cp would
 *    force MSVC to reload the global. The tail at 0x6aa0c-0x6aa1d writes ONE
 *    dword and still advances pp, which is op1+op2 once, not the body again.
 *
 * The two guard mnemonics are load-bearing and differ from FUN_0006a910's:
 * the tail here is `test ebx,ebx; jz` (0x6aa07), upstream UNROLL2's `if (_x)`,
 * where UNROLL8 uses `if (_x > 0)` and gets a `jbe`. Writing `x > 0` here
 * costs mnemonic-LCS points. All counters are unsigned.
 */
void FUN_0006a9a0(unsigned long *cp, unsigned char *pp,
                  unsigned long unused_arg, unsigned long w, unsigned long h,
                  long fromskew, long toskew)
{
  unsigned long *bw;
  unsigned long x;

  /* Upstream discards the tile-origin arguments the same way. */
  (void)unused_arg;

  /* 0x6a9a7: signed halve, see (1) above. Two source bytes per output pair, so
   * the row skew is expressed in source bytes here. */
  fromskew /= 2;

  /* 0x6a9ae guard, 0x6aa26 decrement-and-branch. Note the guard sits ahead of
   * the callee-saved pushes (0x6a9b9-0x6a9c1), so a zero-height tile leaves
   * EBX/ESI/EDI untouched -- that is what the guard-first `while (h-- > 0)`
   * form reproduces. */
  while (h-- > 0) {
    /* 0x6a9e0-0x6aa00. `x` is upstream's UNROLL2 `_x`; it stays live past the
     * loop for the tail test, which is why the compiler keeps `sub ebx,2`
     * alongside the separate `dec edi` w>>1 trip count. The table is re-read
     * from the global every iteration (0x6a9e3) -- do NOT hoist it into a
     * local the way FUN_0006a910 legitimately does, that moves the load out of
     * the loop. */
    for (x = w; x >= 2; x -= 2) {
      bw = BWmap[*pp++];
      *cp++ = *bw++;
      *cp++ = *bw++;
    }

    /* 0x6aa0c-0x6aa1d. One store, and pp still advances a whole byte: the odd
     * final column consumes the high nibble's byte and discards the low
     * nibble's pixel. */
    if (x) {
      bw = BWmap[*pp++];
      *cp++ = *bw++;
    }

    /* 0x6aa20: `add ecx,[ebp-0x4]` with the local holding the pre-scaled
     * toskew*4 -- keep this unscaled in C, cp is a dword cursor. */
    cp += toskew;
    /* 0x6aa23: `add esi,eax` with EAX still carrying the halved fromskew from
     * the reload at 0x6aa02; unscaled, pp is a byte cursor. */
    pp += fromskew;
  }
}

/**
 * Expand one 2-bit colormapped tile into the 32-bit RGBA raster.
 *
 * Transcribed from the vendored libtiff (tif_getimage.c `put2bitcmaptile`)
 * rather than reshaped from the decompiler, which lost every parameter and
 * reported the body as `void(void)`. Same build-specific delta as its two
 * siblings above: the expansion table comes from the file-scope static at
 * 0x3340c4 instead of an `img` struct field.
 *
 * Which writer this is, is proven by the dispatcher at 0x6b858 (FUN_0006b780
 * in this file): the PHOTOMETRIC_PALETTE inner table selects this address for
 * `bitspersample == 2`, between FUN_0006a9a0 (4) and FUN_0006ab10 (1). So the
 * table read here is upstream's PALmap -- spelled `BWmap` in this TU, see the
 * note on that macro above -- and four pixels come out of every source byte.
 *
 * The seven cdecl stack slots are the same shape as every sibling writer; only
 * the ORDER is INFERRED, from upstream's prototype collapsed onto this build's
 * frame. Slot +0x10 is NEVER READ (upstream's discarded tile origin) and must
 * stay, or w/h/fromskew/toskew all shift down one slot and every caller --
 * including the cast at the dispatcher below -- mis-passes them.
 *   EBP+0x08 -> ecx  cp        `mov ecx,[ebp+8]` 0x6aa6c, dword raster cursor
 *   EBP+0x0c -> esi  pp        `mov esi,[ebp+0xc]` 0x6aa63, byte source cursor
 *   EBP+0x10         NEVER READ
 *   EBP+0x14 -> edi  w         `mov edi,[ebp+0x14]` 0x6aa73, reloaded per row
 *                              because the unrolled loop leaves EDI at zero
 *   EBP+0x18         h         `mov ecx,[ebp+0x18]` 0x6aa47, written back at
 *                              0x6aafb -- counted down in its own slot
 *   EBP+0x1c         fromskew  `mov eax,[ebp+0x1c]` 0x6aa44, quartered in
 *                              place at entry, see (1)
 *   EBP+0x20         toskew    `mov edx,[ebp+0x20]` 0x6aa5e, `shl edx,2` into
 *                              the single EBP-4 local, see (3)
 *
 * Frame: `push ebp / mov ebp,esp / push ecx` (one dword local), with
 * EBX/ESI/EDI pushed only AFTER the zero-row guard, and `mov esp,ebp / pop ebp
 * / ret` with no `add esp` -- plain cdecl, void, no register arguments.
 *
 * Four things the shape depends on, all read off the disassembly:
 *
 * 1. `fromskew /= 4` is a SIGNED DIVIDE, not a shift, and it is hoisted ahead
 *    of the row loop: `cdq; and edx,3; add eax,edx; sar eax,2` at
 *    0x6aa4a-0x6aa50, ahead of the `test ecx,ecx; jbe` row guard at
 *    0x6aa53. Spelling it `>> 2` emits a bare SAR, drops the CDQ/AND/ADD
 *    triple, and rounds the wrong way for a negative skew. It is reloaded at
 *    0x6aab8 on the unrolled path only.
 *
 * 2. Unlike FUN_0006a910, the table is NOT hoisted into one local: the global
 *    is re-read separately in each arm -- `mov edx,[0x3340c4]` at 0x6aa83 for
 *    the unrolled body and `mov ebx,[0x3340c4]` at 0x6aac2 for the tail. Using
 *    the macro inline in both arms is what reproduces that; introducing a
 *    shared `bwmap` local would hoist the load out of the loop.
 *
 * 3. toskew is pre-scaled ONCE (`shl edx,2` -> [EBP-4]) because cp is a dword
 *    cursor. Keep `cp += toskew` unscaled in C -- pre-scaling here would
 *    advance the raster four times too far. pp's advance stays unscaled, it is
 *    a byte cursor.
 *
 * 4. Every counter is UNSIGNED, off the mnemonics: row guard `test ecx,ecx;
 *    jbe` (0x6aa53), unrolled entry `cmp edi,4; jc` (0x6aa76), remainder
 *    `test edi,edi; jbe` (0x6aabb). Signed spellings would emit jle/jl/jl.
 *    The `jbe` on the remainder is upstream's `if (_x > 0)`; the sibling
 *    UNROLL2 writer's `if (_x)` gets a `jz` instead, so the two forms are not
 *    interchangeable for mnemonic-LCS purposes.
 *
 * The tail at 0x6aabf-0x6aaed is upstream 3.5.7's CASE4 -- a `switch (_x)`
 * with cases 3/2/1 falling through and NO default, lowered by MSVC to one
 * `movzx`/`inc esi` followed by a `dec edi; jz` chain. A width of 4 or more
 * cannot reach it, which is why the chain's last test branches clean out of
 * all three stores rather than looping. The three tail stores read CONSECUTIVE
 * entries of the same resolved BWmap row, exactly as the unrolled body does.
 */
void FUN_0006aa40(unsigned long *cp, unsigned char *pp,
                  unsigned long unused_arg, unsigned long w, unsigned long h,
                  long fromskew, long toskew)
{
  unsigned long *bw;
  unsigned long x;

  /* Upstream discards the tile-origin arguments the same way. */
  (void)unused_arg;

  /* 0x6aa4a: signed quarter, see (1) above. Four pixels per source byte, so
   * the row skew is expressed in source bytes here. */
  fromskew /= 4;

  /* 0x6aa53 guard, 0x6aafb store-back of the decremented count. The guard sits
   * ahead of the callee-saved pushes, so a zero-height tile leaves EBX/ESI/EDI
   * untouched -- that is what the guard-first `while (h-- > 0)` reproduces. */
  while (h-- > 0) {
    /* 0x6aa76-0x6aab6: upstream UNROLL4 in op1/op2 form. op1 is the table
     * lookup (once per source byte), op2 is REPEAT4'd into four stores off the
     * one resolved row pointer. */
    for (x = w; x >= 4; x -= 4) {
      bw = BWmap[*pp++];
      *cp++ = *bw++;
      *cp++ = *bw++;
      *cp++ = *bw++;
      *cp++ = *bw++;
    }

    /* 0x6aabb-0x6aaed: op1 once more, then CASE4. The odd final byte is
     * consumed whole and its unused low bit-pairs are discarded. */
    if (x > 0) {
      bw = BWmap[*pp++];
      switch (x) {
      case 3:
        *cp++ = *bw++;
        /* fall through */
      case 2:
        *cp++ = *bw++;
        /* fall through */
      case 1:
        *cp++ = *bw++;
      }
    }

    /* 0x6aaf0: `add ecx,[ebp-0x4]` with the local holding the pre-scaled
     * toskew*4 -- keep this unscaled in C, cp is a dword cursor. */
    cp += toskew;
    /* 0x6aaf3-0x6aaf8: unscaled add of the quartered fromskew; pp is a byte
     * cursor. */
    pp += fromskew;
  }
}

/* The 8-bit colormap expansion table, the other static tif_open.c frees at
 * tif_getimage.c line 125 (which is what pins this TU's upstream source file
 * to tif_getimage.c, not tif_flush.c -- kb.json's object grouping is what puts
 * the body here). tif_open.c declares it `void *` because it only ever assigns
 * and frees it; FUN_0006ac60 below proves the pointee type with the same three
 * chained loads per pixel that FUN_0006a910 uses for BWmap:
 *   mov   esi, [0x3340c8]      0x6ac6f  the table, loaded ONCE ahead of both
 *                                       loops
 *   movzx ebx, byte ptr [ecx]  0x6ac94  the source byte
 *   mov   ebx, [esi+ebx*4]     0x6ac97  Map[byte] -- the *4 scale makes this an
 *                                       array of POINTERS, not of pixels
 *   mov   ebx, [ebx]           0x6ac9a  *that pointer -- the pixel value
 * so the storage at 0x3340c8 has type `unsigned long **`. Collapsing the
 * double indirection would store the row pointer itself as a pixel -- a
 * wrong-pixel bug that no match score would notice. */
#define Map (*(unsigned long ***)0x3340c8)

/**
 * Expand one 8-bit colormapped tile into the 32-bit RGBA raster.
 *
 * Transcribed from the vendored libtiff (tif_getimage.c `put8bitcmaptile`)
 * rather than reshaped from the decompiler, which lost every parameter and
 * reported the body as `void(void)`. Bungie's build differs from upstream in
 * two ways, both read off the disassembly: the colormap comes from the
 * file-scope static at 0x3340c8 instead of an `img` struct field, and the
 * source cursor advances by a plain `inc ecx` (0x6ac9f) rather than upstream's
 * `pp += samplesperpixel`, so samplesperpixel is folded to a compile-time 1
 * here. Do not reintroduce the multiply.
 *
 * The seven cdecl stack slots are proven by their reads; only the ORDER is
 * INFERRED, from upstream's (img, cp, x, y, w, h, fromskew, toskew, pp)
 * prototype collapsed onto this build's frame:
 *   +0x08 -> eax  cp        the dword raster cursor
 *   +0x0c -> ecx  pp        the byte source cursor
 *   +0x10         NEVER READ -- upstream's tile-origin y. Kept as a parameter
 *                 because dropping it would shift every later slot.
 *   +0x14 -> edx  w         inner count, re-loaded per row at 0x6aca5
 *   +0x18 -> edi  h         outer count; the SLOT is then reused as scratch
 *   +0x1c         fromskew  added to the byte cursor at 0x6acab
 *   +0x20         toskew    added to the dword cursor at 0x6aca8
 *
 * Both counters are unsigned, read off the branch mnemonics rather than
 * assumed: the row guard is `test eax,eax; jbe` (0x6ac68, skipping the whole
 * body before the callee-saved registers are even pushed) and the pixel guard
 * is `test edx,edx; jbe` (0x6ac92). Signed counters would emit jle/jl and
 * invert the zero-trip behaviour for w or h of 0.
 *
 * toskew is scaled ONCE before the loops -- `lea edx,[ecx*4]` (0x6ac75) stored
 * into the now-dead h slot at 0x6ac80 -- and added back each row at 0x6aca8.
 * That x4 is the compiler's doing: `cp` is a dword pointer, so `cp += toskew`
 * is the correct spelling. Pre-scaling it in C would advance the raster four
 * times too far.
 *
 * The PUSH EBX/ESI/EDI order is interleaved with the setup (0x6ac6d, 0x6ac6e,
 * 0x6ac7f) because the h == 0 early-out precedes the saves; that is scheduling,
 * not something the source expresses.
 */
void FUN_0006ac60(unsigned long *cp, unsigned char *pp,
                  unsigned long unused_arg, unsigned long w, unsigned long h,
                  long fromskew, long toskew)
{
  unsigned long **palmap;
  unsigned long x;

  /* Upstream discards the tile-origin arguments the same way. */
  (void)unused_arg;

  /* 0x6ac6f: hoisted into ESI for the whole call, ahead of both loops. */
  palmap = Map;

  /* 0x6ac68 guard, 0x6acae-0x6acb3 decrement-and-branch. */
  while (h-- > 0) {
    /* 0x6ac92 guard, 0x6ac94-0x6aca2 body: one source byte, one raster
     * dword, both cursors incremented by one element. */
    for (x = w; x-- > 0;)
      *cp++ = palmap[*pp++][0];

    /* 0x6aca8: `add eax,[ebp+0x18]` with the slot holding toskew*4. */
    cp += toskew;
    /* 0x6acab: `add ecx,[ebp+0x1c]`, unscaled -- pp is a byte cursor. */
    pp += fromskew;
  }
}

/**
 * Expand one 1-bit greyscale (min-is-white / min-is-black) tile into the
 * 32-bit RGBA raster.
 *
 * Transcribed from the vendored libtiff (tif_getimage.c `put1bitbwtile`, the
 * bitspersample == 1 arm of FUN_0006b780's MINISWHITE/MINISBLACK dispatch --
 * the store at tif_flush.c:1770 in this file) rather than reshaped from the
 * decompiler, which lost every parameter and reported the body as
 * `void(void)`. Eight dword stores per source byte is what makes this the
 * 1-bit writer: each entry of the table at 0x3340c8 is the eight-pixel run for
 * one packed source byte, so `bw` walks forward inside the entry while `pp`
 * advances one byte per eight pixels (`inc esi`, 0x6ad5e / 0x6ad7b). The same
 * fold the other writers in this TU show applies -- samplesperpixel is a
 * compile-time 1 here, so the source cursor is a plain increment rather than
 * upstream's `pp += samplesperpixel`.
 *
 * Bungie's single delta from upstream: the expansion table is the file-scope
 * static at 0x3340c8 (`Map` above) instead of an `img->BWmap` field.
 *
 * Frame, read off the disassembly (Ghidra's `in_stack_...` labels are
 * ESP-relative junk -- EBX/ESI/EDI are pushed at 0x6ace1/0x6ace2/0x6acef,
 * INSIDE the h == 0 early-out, so its frame reconstruction is off by the
 * saves). One 4-byte local at EBP-4 holds the pre-scaled toskew; no _chkstk,
 * no calls, no FPU. The seven cdecl slots are proven by their reads; only the
 * ORDER is INFERRED, from upstream's (img, cp, x, y, w, h, fromskew, toskew,
 * pp) prototype collapsed onto this build's frame:
 *   +0x08 -> ecx  cp        the dword raster cursor (`add ecx,4`)
 *   +0x0c -> esi  pp        the byte source cursor (`inc esi`)
 *   +0x10         NEVER READ -- upstream's tile-origin y. Kept as a parameter
 *                 because dropping it would shift every later slot, and all
 *                 eleven dispatch sites in this TU cast through
 *                 tiff_put_contig_proc, so a shifted slot would not even warn.
 *   +0x14 -> edi  w         inner count, re-loaded per row at 0x6acf3
 *   +0x18         h         counted down in its own argument slot
 *                           (0x6add6-0x6add7)
 *   +0x1c         fromskew  divided by 8 once up front (see below)
 *   +0x20         toskew    scaled `shl edx,2` once at 0x6ace6 into EBP-4, and
 *                           added back per row at 0x6add2
 *
 * Both counters are unsigned, read off the branch mnemonics rather than
 * assumed: the row guard is `test ecx,ecx; jbe` (0x6acd3/0x6acd8) and the
 * remainder guard is `test edi,edi; jbe` (0x6ad6b). Signed counters would emit
 * jle/jl and invert the zero-trip behaviour for w or h of 0.
 *
 * 0x6acc4-0x6acd0: the SIGNED divide-by-8 (`cdq; and edx,7; add eax,edx;
 * sar eax,3`) runs ONCE, ahead of the h == 0 test, and is written back over
 * this parameter's own frame slot at 0x6acd5 (re-read into EAX at 0x6ad68).
 * Upstream spells the same thing as `fromskew /= 8` before the row loop.
 * An unsigned `>> 3` would drop the round-toward-zero the CDQ/AND pair
 * encodes and change behaviour for a negative skew.
 *
 * toskew is pre-scaled x4 by the COMPILER; `cp` is a dword pointer, so
 * `cp += toskew` is the correct spelling. Pre-scaling it in C would advance
 * the raster four times too far.
 *
 * The table is re-loaded from 0x3340c8 INSIDE both loops here (0x6ad03 and
 * 0x6ad72), so `Map` is indexed in place rather than hoisted into a local the
 * way FUN_0006ac60 legitimately does -- hoisting would move the load out of
 * the loops and out of the reference's shape.
 *
 * The double indirection is load-bearing: `mov edx,[0x3340c8]` /
 * `mov edx,[edx+eax*4]` gives the row POINTER, and the eight
 * `mov eax,[edx]` / `[edx+4]` ... loads with interleaved `add edx,4` walk
 * eight SEQUENTIAL dwords inside that one row. Collapsing it to eight copies
 * of `Map[*pp][0]` would store the same pixel eight times -- a wrong-pixel bug
 * that no match score would notice.
 *
 * The remainder is upstream's CASE8: `dec edi; cmp edi,6; ja; jmp
 * [edi*4+0x6ade8]` is a 7-entry jump table, i.e. a switch with 7 -> 1
 * fallthrough entered after the table lookup has already advanced `pp`.
 */
void FUN_0006acc0(unsigned long *cp, unsigned char *pp,
                  unsigned long unused_arg, unsigned long w, unsigned long h,
                  long fromskew, long toskew)
{
  unsigned long *bw;
  unsigned long x;

  /* Upstream discards the tile-origin arguments the same way. */
  (void)unused_arg;

  /* 0x6acc4-0x6acd0, written back over the parameter's own slot. */
  fromskew /= 8;

  /* 0x6acd3 guard, 0x6add6-0x6add9 decrement-and-branch back to 0x6acf3. */
  while (h-- > 0) {
    /* 0x6acf6 guard (EDI = w >> 3 is the unrolled trip count, DEC/JNZ at
     * 0x6ad61), 0x6ad03-0x6ad5f body: one table load, one `inc esi`, and
     * eight sequential dwords out of the resolved row. */
    for (x = w; x >= 8; x -= 8) {
      bw = Map[*pp++];
      *cp++ = *bw++;
      *cp++ = *bw++;
      *cp++ = *bw++;
      *cp++ = *bw++;
      *cp++ = *bw++;
      *cp++ = *bw++;
      *cp++ = *bw++;
      *cp++ = *bw++;
    }
    /* 0x6ad6b guard, 0x6ad72-0x6adc9: one table load, `inc esi`, then the
     * jump table at 0x6ade8 that is this switch with its 7 -> 1 fallthrough. */
    if (x > 0) {
      bw = Map[*pp++];
      switch (x) {
      case 7:
        *cp++ = *bw++;
      case 6:
        *cp++ = *bw++;
      case 5:
        *cp++ = *bw++;
      case 4:
        *cp++ = *bw++;
      case 3:
        *cp++ = *bw++;
      case 2:
        *cp++ = *bw++;
      case 1:
        *cp++ = *bw++;
      }
    }

    /* 0x6add2: `add ecx,ebx` with EBX holding the pre-scaled toskew*4. */
    cp += toskew;
    /* 0x6add4: `add esi,eax` with EAX holding the quotient computed above;
     * unscaled -- pp is a byte cursor. */
    pp += fromskew;
  }
}

/**
 * Expand one 2-bit greyscale (min-is-white / min-is-black) tile into the
 * 32-bit RGBA raster.
 *
 * Transcribed from the vendored libtiff (tif_getimage.c `put2bitbwtile`, the
 * bitspersample == 2 arm of FUN_0006b780's inner dispatch) rather than
 * reshaped from the decompiler, which lost every parameter and reported the
 * body as `void(void)`. The four dword stores per source byte are UNROLL4
 * expanded verbatim; each table entry at 0x3340c8 is the four-pixel run for
 * one packed source byte, so `bw` walks forward inside the entry while `pp`
 * advances one byte per four pixels (`inc esi` once per unrolled block,
 * 0x6ae7f).
 *
 * Frame, read off the disassembly (Ghidra's `in_stack_...` labels are
 * ESP-relative junk -- EBX/ESI/EDI are pushed INSIDE the h != 0 guard at
 * 0x6ae31/0x6ae32/0x6ae3f, after the early-out, so the decompiler's frame
 * base is wrong):
 *   +0x08 -> ecx  cp        the dword raster cursor
 *   +0x0c -> esi  pp        the packed byte source cursor
 *   +0x10         NEVER READ -- upstream's tile origin. Kept as a parameter
 *                 because dropping it would shift every later slot.
 *   +0x14 -> edi  w         re-loaded per row at the 0x6ae43 loop re-entry
 *   +0x18         h         outer count, spilled back each row (0x6aeca)
 *   +0x1c         fromskew, OVERWRITTEN in place at 0x6ae25 with fromskew/4
 *   +0x20         toskew, pre-scaled by `shl edx,2` (0x6ae36) into EBP-4
 *
 * Signedness is read off the mnemonics, not assumed: the row guard is
 * `test ecx,ecx; jbe` (0x6ae23) and the unroll trip count is `shr` -- w and h
 * are UNSIGNED. fromskew is SIGNED: the divide-by-4 at 0x6ae1a-0x6ae20 is the
 * `cdq; and edx,3; add eax,edx; sar eax,2` sequence, which only exists for a
 * signed operand. Making it unsigned would collapse it to a bare shr.
 *
 * Both the x4 on toskew and the /4 on fromskew stay unspelled in C: `cp` is a
 * dword pointer so the compiler supplies the scale, and `pp` advances one byte
 * per four pixels so upstream's `fromskew/4` is the source-level form.
 *
 * Unlike the 8-bit sibling above, the table pointer is re-loaded from 0x3340c8
 * inside BOTH loops (0x6ae53 and 0x6ae92) instead of being hoisted into a
 * register for the whole call, so it is spelled as a direct `Map[...]` here.
 */
void FUN_0006ae10(unsigned long *cp, unsigned char *pp,
                  unsigned long unused_arg, unsigned long w, unsigned long h,
                  long fromskew, long toskew)
{
  unsigned long *bw;
  unsigned long x;

  /* Upstream discards the tile-origin arguments the same way. */
  (void)unused_arg;

  /* 0x6ae1a-0x6ae25: the signed divide-by-4 runs ONCE, ahead of the h == 0
   * test, and its result is written back over this parameter's own frame slot
   * (`mov [ebp+0x1c],eax` at 0x6ae25) before being held in EAX for the rest of
   * the call. Upstream spells the same thing as `pp += fromskew/4` inside the
   * row loop; folding it into the parameter here is what reproduces the
   * reference's single-slot frame and its entry-block quotient. */
  fromskew /= 4;

  /* 0x6ae23 guard, 0x6aec9-0x6aece decrement-and-branch back to 0x6ae43. */
  while (h-- > 0) {
    /* 0x6ae50-0x6ae86: EBX = w >> 2 trip count, EDI keeps the residue via
     * `sub edi,4`, and the last of the four loads folds into
     * `mov edx,[edx+4]`. */
    for (x = w; x >= 4; x -= 4) {
      bw = Map[*pp++];
      *cp++ = *bw++;
      *cp++ = *bw++;
      *cp++ = *bw++;
      *cp++ = *bw++;
    }
    /* 0x6ae8f-0x6aebd: one table load, `inc esi`, then the DEC EDI / JZ chain
     * that is this switch with its case 3 -> 2 -> 1 fallthrough. */
    if (x > 0) {
      bw = Map[*pp++];
      switch (x) {
      case 3:
        *cp++ = *bw++;
      case 2:
        *cp++ = *bw++;
      case 1:
        *cp++ = *bw++;
      }
    }

    /* 0x6aec6: `add ecx,[ebp-4]` with the slot holding toskew*4. */
    cp += toskew;
    /* 0x6aec9: `add esi,eax` with EAX holding the quotient computed above. */
    pp += fromskew;
  }
}

/**
 * Expand one 4-bit colormapped tile into the 32-bit RGBA raster.
 *
 * Transcribed from the vendored libtiff (tif_getimage.c `put4bitcmaptile`, the
 * bitspersample == 4 arm of FUN_0006b780's palette dispatch) rather than
 * reshaped from the decompiler, which lost every parameter and reported the
 * body as `void(void)`. Bungie's build differs from upstream the same single
 * way FUN_0006ac60 does: the colormap comes from the file-scope static at
 * 0x3340c8 (`Map` above) instead of an `img->PALmap` field. Two dword stores
 * per source byte is what makes this the 4-bit writer -- each table entry is
 * the two-pixel run for one packed source byte, so `bw` walks forward inside
 * the entry while `pp` advances one byte per two pixels (`inc esi`, 0x6af2c).
 *
 * The table is read at 0x3340c8, NOT the 0x3340c4 of FUN_0006a910 -- those are
 * two different statics (BWmap vs Map) and confusing them would silently index
 * the greyscale table with palette indices.
 *
 * Frame, read off the disassembly (Ghidra's `in_stack_...` labels are
 * ESP-relative junk -- EBX/ESI/EDI are pushed INSIDE the h != 0 guard at
 * 0x6aef9-0x6af01, after the early-out, so its frame reconstruction is off by
 * the saves). One 4-byte local at EBP-4 holds the pre-scaled toskew; no
 * _chkstk. The seven cdecl slots are proven by their reads; only the ORDER is
 * INFERRED, from upstream's (img, cp, x, y, w, h, fromskew, toskew, pp)
 * prototype collapsed onto this build's frame:
 *   +0x08 -> ecx  cp        the dword raster cursor (0x6af08, `add ecx,4`)
 *   +0x0c -> esi  pp        the byte source cursor (0x6aefb, `inc esi`)
 *   +0x10         NEVER READ -- upstream's tile-origin y. Kept as a parameter
 *                 because dropping it would shift every later slot, and all
 *                 eleven dispatch sites in this TU cast through
 *                 tiff_put_contig_proc, so a shifted slot would not even warn.
 *   +0x14 -> edi  w         inner count, re-loaded per row at 0x6af45
 *   +0x18 -> ecx  h         outer count; the SLOT is then reused as the row
 *                 counter (0x6af05, then DEC/store/JNZ at 0x6af68-0x6af6c)
 *   +0x1c         fromskew  halved once up front (see below)
 *   +0x20         toskew    scaled once, added per row at 0x6af60
 *
 * Both counters are unsigned, read off the branch mnemonics rather than
 * assumed: the row guard is `test ecx,ecx; jbe` (0x6aeef) and the pixel guard
 * is `cmp edi,2; jc` (0x6af10). Signed counters would emit jle/jl and invert
 * the zero-trip behaviour for w or h of 0.
 *
 * 0x6aee4-0x6aef1: the signed divide-by-2 (`cdq; sub eax,edx; sar eax,1`) runs
 * ONCE, ahead of the h == 0 test, and is written back over this parameter's own
 * frame slot before being held for the rest of the call. Upstream spells the
 * same thing as `pp += fromskew/2` inside the row loop; folding it into the
 * parameter here is what reproduces the reference's single-slot frame. An
 * unsigned `>> 1` would drop the round-toward-zero the CDQ/SUB pair encodes.
 *
 * toskew is scaled ONCE -- `shl edx,2` (0x6aefe) stored to EBP-4 -- and added
 * back each row at 0x6af60. That x4 is the compiler's doing: `cp` is a dword
 * pointer, so `cp += toskew` is the correct spelling. Pre-scaling it in C would
 * advance the raster four times too far.
 *
 * Unlike FUN_0006ac60, this variant re-loads the table from 0x3340c8 INSIDE the
 * pixel loop (`mov edx,[0x3340c8]`, 0x6af23), so `Map` is indexed in place here
 * rather than hoisted into a local -- hoisting would move the load out of the
 * loop and out of the reference's shape.
 *
 * Each pixel is three chained loads (0x6af20-0x6af36): the source byte, then
 * `mov edx,[edx+eax*4]` for the row POINTER, then `[edx]` / `[edx+4]` for the
 * two pixels. Collapsing that double indirection would store the row pointer
 * itself as a pixel -- a wrong-pixel bug that no match score would notice.
 */
void FUN_0006aee0(unsigned long *cp, unsigned char *pp,
                  unsigned long unused_arg, unsigned long w, unsigned long h,
                  long fromskew, long toskew)
{
  unsigned long *bw;
  unsigned long x;

  /* Upstream discards the tile-origin arguments the same way. */
  (void)unused_arg;

  /* 0x6aee4-0x6aef1, written back over the parameter's own slot. */
  fromskew /= 2;

  /* 0x6aeef guard, 0x6af68-0x6af6c decrement-and-branch back to 0x6af05. */
  while (h-- > 0) {
    /* 0x6af10 guard, 0x6af20-0x6af3e body: EDI = w >> 1 is the unrolled trip
     * count (DEC EDI / JNZ 0x6af40) and EBX = w carries the residue via
     * `sub ebx,2` (0x6af3c). That is libtiff's UNROLL2 verbatim. */
    for (x = w; x >= 2; x -= 2) {
      bw = Map[*pp++];
      *cp++ = *bw++;
      *cp++ = *bw++;
    }
    /* 0x6af48 `test ebx,ebx; jz`, then 0x6af4c-0x6af5d: the same table load
     * and `inc esi`, but only the first pixel of the entry. */
    if (x > 0) {
      bw = Map[*pp++];
      *cp++ = *bw;
    }

    /* 0x6af60: `add ecx,[ebp-4]` with the slot holding toskew*4. */
    cp += toskew;
    /* 0x6af63: unscaled -- pp is a byte cursor. */
    pp += fromskew;
  }
}

/* Read as a WORD (`movzx edx, word ptr [0x3340f8]`, 0x6af86), so this is the
 * same `unsigned short` decoder-state global tif_open.c recovered for the
 * other two tags at 0x3340f4 / 0x3340fc. Widening it to int would change the
 * load width. */
#define samplesperpixel (*(unsigned short *)0x3340f8)

/* Upstream's PACK, MINUS the alpha term. Upstream libtiff spells it
 * `((uint32)(r)|((uint32)(g)<<8)|((uint32)(b)<<16)|0xff000000)`; this build
 * leaves the high byte ZERO -- the two pack sites below (0x6afc4-0x6afe3 and
 * 0x6b030-0x6b046) end at `shl ebx,8; or ebx,edx; mov [esi-4],ebx` with no
 * `or ...,0xff000000` anywhere in the function, and the bounds table stops at
 * 0x6b093. Adding the alpha would be a wrong-pixel bug no match score
 * notices. */
#define PACK(r, g, b) \
  ((unsigned long)(r) | ((unsigned long)(g) << 8) | ((unsigned long)(b) << 16))

/**
 * Expand one 8-bit-per-sample RGB contiguous tile into the 32-bit RGBA raster,
 * optionally through a byte lookup table.
 *
 * Transcribed from the vendored libtiff (tif_getimage.c) rather than reshaped
 * from the decompiler. Upstream ships this as TWO routines that
 * PickContigCase selects between on `img->Map`; Bungie merged them into one
 * body with the branch taken inside, which is the `if (map != 0)` at 0x6afc4
 * (fall-through) versus 0x6b026 (the taken arm). The two arms are
 * putRGBcontig8bitMaptile and putRGBcontig8bittile respectively -- note the
 * asymmetry is upstream's, not a transcription artifact: only the plain arm is
 * UNROLL8'd, the Map arm is a flat `for (x = w; x-- > 0;)`.
 *
 * This is the target FUN_0006b780 stores for PHOTOMETRIC_RGB with
 * bitspersample == 8 (tif_flush.c:2285 in this file).
 *
 * Frame, read off the disassembly. Ghidra had no prototype at all here (the
 * seeded kb.json decl was the placeholder `void FUN_0006af80(void)`), so every
 * argument came back as an ESP-relative `in_stack_...` label; the seven cdecl
 * slots below are proven by their reads and match the two writers in this TU
 * that were lifted first (FUN_0006a910, FUN_0006ac60):
 *   +0x08  cp        the dword raster cursor (`add esi,4`, stored `[esi-4]`)
 *   +0x0c  pp        the byte source cursor
 *   +0x10  map       USED HERE. The sibling writers never read this slot and
 *                    name it `unused_arg` (upstream's discarded tile origin);
 *                    this variant indexes it as a BYTE table -- `mov
 * bh,[edx+ecx]` with ECX = the slot and EDX = a zero-extended source byte
 *                    (0x6afd4). That byte-sized load is what types it
 *                    `unsigned char *`, i.e. upstream's `TIFFRGBValue *Map`,
 *                    and not the `unsigned long` the shared
 *                    tiff_put_contig_proc typedef declares for slot 3. The
 *                    typedef is deliberately left alone: all eleven dispatch
 *                    sites already cast through it.
 *   +0x14  w         inner count, re-loaded per row (0x6aff5 / 0x6b058) because
 *                    the counter register was consumed -- register allocation,
 *                    not a second semantic read.
 *   +0x18  h         row counter
 *   +0x1c  fromskew  multiplied by samplesperpixel ONCE up front (see below)
 *   +0x20  toskew    scaled `shl ...,2` once up front because `cp` is a dword
 *                    pointer
 * One 4-byte local at EBP-4 (`push ecx` for the frame) holds the h counter.
 * No _chkstk, no CALLs, no FPU, no SEH -- pure byte/word integer work.
 *
 * Both row guards are `test eax,eax; jbe` (0x6af9f for the Map arm, 0x6b00f
 * for the plain arm) and the UNROLL8 remainder guard is `test edi,edi; jbe`
 * (0x6b05b), so every counter is UNSIGNED. Signed counters would emit
 * jle/jl and invert the zero-trip behaviour for a w or h of 0. The remainder
 * guard's `jbe` is upstream UNROLL8's `if (_x > 0)`; the sibling
 * FUN_0006a9a0 documents that UNROLL2's `if (_x)` gets a `jz` instead, so the
 * spelling is load-bearing for mnemonic-LCS.
 *
 * Three things here are NOT upstream and must not be "corrected" to it:
 *
 * 1. The pack has no 0xff alpha (see the PACK macro above).
 *
 * 2. The UNROLL8 remainder writes exactly ONE pixel, not REPEAT(_x, op2).
 *    The block at 0x6b05f-0x6b07c has NO back-edge, so a tile row whose
 *    width leaves a remainder of 2..7 is short by that many pixels. The same
 *    quirk is already recorded for the identical tail in FUN_0006a910.
 *
 * 3. samplesperpixel is re-read from 0x3340f8 INSIDE both loops (0x6afe6,
 *    0x6b049, 0x6b075) in addition to the once-up-front read for the fromskew
 *    fold. Upstream hoists it into a local `int samplesperpixel` at the top;
 *    doing that here would move the load out of the loops and out of the
 *    reference's shape, so it stays a macro read at every use.
 *
 * The pack ORDER is proven, and it is the one place a plausible-looking
 * transcription would silently swap channels. Map arm, 0x6afc4-0x6afe3:
 * `movzx edx,[eax+2]` then `xor ebx,ebx` / `mov bh,[edx+ecx]` puts map[pp[2]]
 * in BH (bits 8-15 pre-shift), `mov bl,[edx+ecx]` with EDX = pp[1] puts
 * map[pp[1]] in BL, and `shl ebx,8` / `or ebx,edx` folds map[pp[0]] in at the
 * bottom -- so BH lands at bits 16-23. The plain arm (0x6b030-0x6b046) is the
 * same shape on the raw bytes (EBX = pp[0], DH = pp[2], DL = pp[1]).
 *
 * Ghidra's rendering of the source cursor as a `uint3 *` with a single
 * `*puVar1 = (uint)*pp` is a FICTION: the binary issues three separate
 * zero-extending BYTE loads at [eax], [eax+1], [eax+2]. A 3-byte struct load
 * (or a `*(unsigned long *)pp`) would also over-read the tile by one byte on
 * the last pixel. Its CONCAT21(CONCAT11(...)) spelling of the pack must not
 * reach the source either.
 */
void FUN_0006af80(unsigned long *cp, unsigned char *pp, unsigned char *map,
                  unsigned long w, unsigned long h, long fromskew, long toskew)
{
  unsigned long x;

  /* 0x6af86-0x6af8e: `movzx edx,word[0x3340f8]; imul edx,[ebp+0x1c]`, written
   * back over this parameter's own frame slot. Upstream is `fromskew *=
   * samplesperpixel` ahead of the row loop; the fold means `pp += fromskew`
   * below is already in source bytes. */
  fromskew *= samplesperpixel;

  /* 0x6afc4 falls through with the table, 0x6b026 is the taken arm. */
  if (map != 0) {
    /* 0x6af9f guard. */
    while (h-- > 0) {
      for (x = w; x-- > 0;) {
        *cp++ = PACK(map[pp[0]], map[pp[1]], map[pp[2]]);
        pp += samplesperpixel;
      }

      cp += toskew; /* the slot holds toskew*4 -- `cp` is a dword pointer */
      pp += fromskew; /* pre-scaled above; `pp` is a byte cursor */
    }
  } else {
    /* 0x6b00f guard. */
    while (h-- > 0) {
      /* Upstream UNROLL8 with op1 = NOP: the trip count is w >> 3 in its own
       * register (DEC / JNZ) while `x` carries the residue via `sub ...,8`. */
      for (x = w; x >= 8; x -= 8) {
        *cp++ = PACK(pp[0], pp[1], pp[2]);
        pp += samplesperpixel;
      }
      /* 0x6b05b `test edi,edi; jbe`, then 0x6b05f-0x6b07c: ONE pixel, see
       * note (2) above -- not REPEAT(_x, op2). */
      if (x > 0) {
        *cp++ = PACK(pp[0], pp[1], pp[2]);
        pp += samplesperpixel;
      }

      cp += toskew;
      pp += fromskew;
    }
  }
}

/*
 * FUN_0006b190 -- 0x6b190, upstream libtiff's putRGBseparate8bitMaptile and
 * putRGBseparate8bittile fused into one routine, exactly as FUN_0006af80 above
 * fuses the two contiguous-sample variants. The same asymmetry appears: only
 * the plain arm is UNROLL8'd, the Map arm is a flat `for (x = w; x-- > 0;)`.
 *
 * Ghidra had no prototype here (the seeded kb.json decl was the placeholder
 * `void FUN_0006b190(void)`), so every argument came back as an ESP-relative
 * `in_stack_...` label. The nine cdecl slots are proven by their reads:
 *   +0x08  cp        the dword raster cursor (`add edi,4`, stored `[edi-4]`)
 *   +0x0c  r         red plane cursor -- advanced by INC, one byte per pixel
 *   +0x10  g         green plane cursor
 *   +0x14  b         blue plane cursor
 *   +0x18  map       BOTH the branch selector (`mov eax,[ebp+0x18]; test
 *                    eax,eax; jz`) AND a BYTE lookup table in the fall-through
 *                    arm: `mov bh,[edx+eax]` at 0x6b1d1 with EAX = this slot
 *                    and EDX = a zero-extended plane byte. That byte-sized
 *                    load is what types it `unsigned char *`, i.e. upstream's
 *                    `TIFFRGBValue *Map`, and not the `unsigned long` the
 *                    shared tiff_put_contig_proc typedef declares for slot 3.
 *   +0x1c  w         inner count, re-loaded per row because the counter
 *                    register was consumed -- register allocation, not a
 *                    second semantic read.
 *   +0x20  h         row counter
 *   +0x24  fromskew  added RAW to all three plane cursors. Unlike the
 *                    contiguous writers there is NO `fromskew *=
 *                    samplesperpixel` fold and no `imul` in the function:
 *                    separate planes carry one byte per pixel each, so the
 *                    skew is already in source bytes.
 *   +0x28  toskew    scaled by four once per row advance (`shl edx,2` /
 *                    `lea edx,[ecx*4]`) into the EBP-4 local because `cp` is a
 *                    dword pointer. That scale is exactly what C pointer
 *                    arithmetic on `unsigned long *cp` emits, so it is spelled
 *                    `cp += toskew` below and never `toskew * 4`.
 * Frame is `push ebp; mov ebp,esp; sub esp,0xc` plus EBX/ESI/EDI: three dword
 * slots (EBP-4 the scaled toskew, EBP-8 the w>>3 trip count, EBP-0xc the plain
 * arm's row counter). No _chkstk, no CALLs, no FPU, no SEH -- pure byte work.
 *
 * Every loop guard is unsigned: `cmp ebx,8; jc` for the UNROLL8 entry and
 * `test ebx,ebx; jbe` for its remainder at 0x6b290, `jbe` on the row guards.
 * Signed counters would emit jl/jle and invert the zero-trip behaviour for a w
 * or h of 0.
 *
 * Two things here are NOT upstream and must not be "corrected" to it:
 *
 * 1. The pack has no 0xff alpha (see the PACK macro above). There is no
 *    `or ...,0xff000000` anywhere in the function, in either arm.
 *
 * 2. The UNROLL8 remainder writes exactly ONE pixel, not REPEAT(_x, op2).
 *    The block at 0x6b290 has NO back-edge -- one dword store, three INCs --
 *    so a row whose width leaves a remainder of 2..7 is short by that many
 *    pixels. The identical quirk is already recorded for the tails of
 *    FUN_0006a910 and FUN_0006af80.
 *
 * The pack ORDER is proven, and it is the one place a plausible-looking
 * transcription would silently swap channels. Map arm, 0x6b1d1: `movzx
 * edx,[edi]` (the b cursor) then `xor ebx,ebx` / `mov bh,[edx+eax]` puts
 * map[*b] in BH (bits 8-15 pre-shift), `mov bl,[edx+eax]` with EDX = *g puts
 * map[*g] in BL, and `shl ebx,8` / `or ebx,edx` folds map[*r] in at the bottom
 * -- so BH lands at bits 16-23. Result: map[*r] | map[*g]<<8 | map[*b]<<16.
 * The plain arm (0x6b264) is the same shape on the raw bytes: EBX = *r, DH =
 * *b, DL = *g, `shl edx,8`, `or edx,ebx`.
 *
 * Ghidra's CONCAT21(CONCAT11(...)) spelling of both packs must not reach the
 * source.
 */
void FUN_0006b190(unsigned long *cp, unsigned char *r, unsigned char *g,
                  unsigned char *b, unsigned char *map, unsigned long w,
                  unsigned long h, long fromskew, long toskew)
{
  unsigned long x;

  /* 0x6b1d1 falls through with the table; 0x6b254 is the taken arm. */
  if (map != 0) {
    while (h-- > 0) {
      for (x = w; x-- > 0;)
        *cp++ = PACK(map[*r++], map[*g++], map[*b++]);

      /* Upstream's SKEW(r, g, b, fromskew), then the raster advance. */
      r += fromskew;
      g += fromskew;
      b += fromskew;
      cp += toskew; /* the slot holds toskew*4 -- `cp` is a dword pointer */
    }
  } else {
    while (h-- > 0) {
      /* Upstream UNROLL8 with op1 = NOP: `cmp ebx,8; jc` then `mov edx,ebx;
       * shr edx,3` as the trip count in its own register (DEC / JNZ) while `x`
       * carries the residue via `sub ebx,8`. */
      for (x = w; x >= 8; x -= 8)
        *cp++ = PACK(*r++, *g++, *b++);
      /* 0x6b290 `test ebx,ebx; jbe`, then ONE pixel -- see note (2) above,
       * this is not REPEAT(_x, op2). */
      if (x > 0)
        *cp++ = PACK(*r++, *g++, *b++);

      cp += toskew;
      r += fromskew;
      g += fromskew;
      b += fromskew;
    }
  }
}

/*
 * FUN_0006b2d0 -- 0x6b2d0, the 16-bit-per-sample twin of FUN_0006b190 above:
 * upstream libtiff's putRGBseparate16bittile, fused with a no-table arm the
 * same way every other put-tile routine in this TU is fused.
 *
 * Ghidra had no prototype here either (the seeded kb.json decl was the
 * placeholder `void FUN_0006b2d0(void)`), so every argument came back as an
 * ESP-relative `in_stack_...` label. The nine cdecl slots are proven by their
 * reads at EBP+8..EBP+0x28; only the ORDER is INFERRED, and it is inferred
 * from the sibling at 0x6b190 having the identical nine-slot frame:
 *   +0x08  cp        the dword raster cursor
 *   +0x0c  r  -> eax the plane OR'd in at bits 0..7
 *   +0x10  g  -> ecx the plane OR'd in at bits 8..15
 *   +0x14  b  -> esi the plane OR'd in at bits 16..23
 *   +0x18  map -> edi BOTH the branch selector (`test edi,edi; jz 0x6b376`)
 *                 AND the lookup table itself.
 *   +0x1c  w         inner count, re-loaded per row (0x6b394 on the plain arm)
 *                    because the counter register was consumed -- register
 *                    allocation, not a second semantic read.
 *   +0x20  h         row count, copied into a reused arg slot at 0x6b309 and
 *                    0x6b388.
 *   +0x24  fromskew  in uint16 ELEMENTS: `add edx,edx` (0x6b2fb) / `add
 *                    edi,edi` (0x6b383) doubles it once, ahead of both loops,
 *                    which is exactly what `r += fromskew` emits for an
 *                    `unsigned short *`.
 *   +0x28  toskew    in uint32 elements: `shl ebx,2` (0x6b2fd / 0x6b385), i.e.
 *                    what `cp += toskew` emits for an `unsigned long *`. Both
 *                    scales are spelled as element-count pointer arithmetic
 *                    below and never as `* 2` / `* 4`.
 * Frame is `push ebp; mov ebp,esp` plus EBX/ESI/EDI and NO `sub esp` -- zero
 * locals. Both pre-scaled skews are parked back into ARGUMENT slots
 * ([EBP+0xc] and [EBP+0x10]) because all six usable GPRs are live, and
 * [EBP+0x14]/[EBP+0x18] are reused as the two trip counters. That is register
 * pressure, not extra state.
 *
 * The samples are 16-bit, and that is proven rather than assumed: every source
 * load is `movzx r32, word ptr` (0x6b320, 0x6b328, 0x6b337 on the table arm;
 * 0x6b3a0, 0x6b3a3, 0x6b3ab on the plain one) and every plane cursor advances
 * by `add reg,2`. Typing the planes `unsigned char *` would halve all three
 * strides and silently mis-sample the tile.
 *
 * `map` is indexed by the FULL zero-extended 16-bit sample -- `mov bh, byte
 * ptr [edx+edi]` with EDX holding the movzx'd word -- so it is upstream's
 * Bitdepth16To8: a >=64K BYTE table, not a 256-entry one. The byte-sized load
 * is also what keeps it `unsigned char *` rather than the `unsigned long` the
 * shared put-proc typedef declares for that slot.
 *
 * Two things here are NOT upstream and must not be "corrected" to it:
 *
 * 1. The plain arm does NOT narrow its samples. Upstream ships no no-table
 *    16-bit separate writer at all, so the tempting repair is to shift each
 *    sample down by 8 first. The binary does not: 0x6b3a0-0x6b3b3 is
 *    `movzx; movzx; shl 8; or; movzx; shl 8; or` on the RAW words, so any
 *    sample above 0xff overlaps into the next channel. Adding a `>> 8` would
 *    be a behaviour change dressed as a cleanup.
 *
 * 2. No 0xff alpha term, same as every other pack in this TU (see PACK above).
 *
 * The pack ORDER is the one place a plausible-looking transcription silently
 * swaps channels. Table arm, 0x6b320: `movzx edx,word[esi]` (the b cursor)
 * then `xor ebx,ebx` / `mov bh,[edx+edi]` puts map[*b] at bits 8..15, `mov
 * bl,[edx+edi]` with EDX = *g puts map[*g] at bits 0..7, then `shl ebx,8`
 * lifts those to 16..23 and 8..15 and `or ebx,edx` folds map[*r] in at the
 * bottom. Result map[*r] | map[*g]<<8 | map[*b]<<16, i.e. PACK(map[*r],
 * map[*g], map[*b]) -- the same channel order as the 8-bit sibling. The plain
 * arm is the identical shape on raw words (`((*b<<8)|*g)<<8|*r`); MSVC factors
 * the common <<8 out of PACK's b<<16 and g<<8 exactly the way it uses BH/BL to
 * do it for free in the byte-sample sibling. Ghidra's CONCAT21(CONCAT11(...))
 * spelling of the table pack must not reach the source (lift-learnings 13).
 *
 * Every loop guard is `jbe` (0x6b2ef, 0x6b316, 0x6b37b, 0x6b399), so w and h
 * are UNSIGNED; signed counters would emit jl/jle and invert the zero-trip
 * behaviour for a w or h of 0. There are two separate epilogues (0x6b371 and
 * 0x6b3df) and both h==0 early-outs jump to the second one.
 */
void FUN_0006b2d0(unsigned long *cp, unsigned short *r, unsigned short *g,
                  unsigned short *b, unsigned char *map, unsigned long w,
                  unsigned long h, long fromskew, long toskew)
{
  unsigned long x;

  /* 0x6b2e1 `test edi,edi; jz 0x6b376` -- the table arm is the fall-through,
   * so it is spelled first here as well. */
  if (map != 0) {
    while (h-- > 0) {
      for (x = w; x-- > 0;)
        *cp++ = PACK(map[*r++], map[*g++], map[*b++]);

      /* Upstream's SKEW(r, g, b, fromskew) BEFORE the raster advance. The
       * plain arm below does it the other way round -- see 0x6b35a-0x6b369
       * versus 0x6b3cd, the same asymmetry the 8-bit sibling has. */
      r += fromskew;
      g += fromskew;
      b += fromskew;
      cp += toskew;
    }
  } else {
    while (h-- > 0) {
      for (x = w; x-- > 0;)
        *cp++ = PACK(*r++, *g++, *b++);

      cp += toskew; /* 0x6b3cd, ahead of the three plane advances */
      r += fromskew;
      g += fromskew;
      b += fromskew;
    }
  }
}

/*
 * FUN_0006b780 -- 0x6b780, upstream libtiff's PickContigCase.
 *
 * Selects the packed-sample tile writer for the decoder state that
 * tif_open.c's reader already populated. Both selectors are read as WORDS
 * (`movzx eax, word ptr [0x3340f4]` at 0x6b780, `movzx eax, word ptr
 * [0x3340fc]` in each arm), so they are the same `unsigned short` globals
 * tif_open.c recovered; widening either to int would change the load width.
 *
 * The return value is PROVEN, and it is not upstream's 0/1 success flag: the
 * epilogue is `mov eax,esi; pop esi; ret` at 0x6b838 with ESI zeroed by the
 * `xor esi,esi` at 0x6b78a, so the routine hands its caller the selected
 * routine address and NULL when the format is unsupported. Ghidra reports
 * this function as returning void, which would silently leak whatever EAX
 * happened to hold (lift-learnings section 16).
 *
 * Dispatch shape, all from the disassembly:
 *   0x6b787  cmp eax,6 / ja  -> the error tail; a 7-entry outer table lives at
 *            0x6b83c (just past the function end recorded in the bounds
 *            table), so photometric 4 and 5 land on default.
 *   inner    `movzx eax,word[0x3340fc]; dec eax; cmp eax,7; ja; jmp [eax*4+T]`
 *            with T = 0x6b858 for the palette arm and T = 0x6b878 for the
 *            min-is-white/min-is-black arm -- 8 entries biased by one, only
 *            1/2/4/8 distinct. The palette table is emitted first, which is
 *            why the arms are ordered RGB, palette, grey, YCbCr here: that is
 *            upstream's source order and it puts the two blocks in the
 *            observed order.
 *   0x6b82x  the RGB and YCbCr arms use a direct `cmp word ptr [0x3340fc],8`
 *            instead of a table, so they are spelled as compares, not as
 *            switches over 8/16.
 *
 * Both paths share one tail: the error report FALLS THROUGH into
 * `mov eax,esi`, so the failure case returns NULL rather than returning
 * early. Do not hoist a `return NULL;` into the error branch.
 *
 * The photometric numbers are proven; the PHOTOMETRIC_* spellings are the
 * TIFF-spec names for those values (tif_open.c already committed to
 * MINISBLACK == 1 and RGB == 2 from its own stores). Nothing here proves what
 * the individual writers do with the samples.
 */
#define photometric (*(unsigned short *)0x3340f4)
#define bitspersample (*(unsigned short *)0x3340fc)

#define PHOTOMETRIC_MINISWHITE 0
#define PHOTOMETRIC_MINISBLACK 1
#define PHOTOMETRIC_RGB 2
#define PHOTOMETRIC_PALETTE 3
#define PHOTOMETRIC_YCBCR 6

/* The pointee signature is carried over from the two writers in this file that
 * have been lifted -- FUN_0006a910 (palette, 8bpp) and FUN_0006ac60 (grey,
 * 8bpp) both take (cp, pp, tile-origin, w, h, fromskew, toskew). The other
 * nine dispatch targets are still seeded `void (void)` in kb.json, so each
 * assignment below is cast; the pointer-ness is proven, the argument list is
 * INFERRED from those two siblings. */
typedef void (*tiff_put_contig_proc)(unsigned long *cp, unsigned char *pp,
                                     unsigned long unused_arg, unsigned long w,
                                     unsigned long h, long fromskew,
                                     long toskew);

void *FUN_0006b780(void)
{
  /* 0x6b78a: `xor esi,esi` ahead of the dispatch -- ESI is the result slot and
   * the only callee-saved register the function touches. */
  tiff_put_contig_proc put = NULL;

  switch (photometric) {
  case PHOTOMETRIC_RGB:
    /* 0x6b7bf: one `cmp word ptr [0x3340fc],8` with both arms populated. */
    if (bitspersample == 8)
      put = (tiff_put_contig_proc)FUN_0006af80;
    else
      put = (tiff_put_contig_proc)FUN_0006b0a0;
    break;
  case PHOTOMETRIC_PALETTE:
    /* Inner table at 0x6b858. */
    switch (bitspersample) {
    case 8:
      put = (tiff_put_contig_proc)FUN_0006a910;
      break;
    case 4:
      put = (tiff_put_contig_proc)FUN_0006a9a0;
      break;
    case 2:
      put = (tiff_put_contig_proc)FUN_0006aa40;
      break;
    case 1:
      put = (tiff_put_contig_proc)FUN_0006ab10;
      break;
    }
    break;
  case PHOTOMETRIC_MINISWHITE:
  case PHOTOMETRIC_MINISBLACK:
    /* Inner table at 0x6b878; the two photometrics share one arm. */
    switch (bitspersample) {
    case 8:
      put = (tiff_put_contig_proc)FUN_0006ac60;
      break;
    case 4:
      put = (tiff_put_contig_proc)FUN_0006aee0;
      break;
    case 2:
      put = (tiff_put_contig_proc)FUN_0006ae10;
      break;
    case 1:
      put = (tiff_put_contig_proc)FUN_0006acc0;
      break;
    }
    break;
  case PHOTOMETRIC_YCBCR:
    /* 0x6b810: same direct compare as the RGB arm, but with no else -- any
     * other depth falls through to the error tail. */
    if (bitspersample == 8)
      put = (tiff_put_contig_proc)FUN_0006b610;
    break;
  }

  /* 0x6b821 `test esi,esi; jnz` joins the default arms here; 0x6b825 loads the
   * name as a dword (`mov eax,[0x3340dc]`) and pushes the format string FIRST,
   * so the name is argument zero. `add esp,8` proves the variadic call passes
   * no extra arguments. */
  if (put == NULL)
    FUN_00068a30(filename, "Can not handle format");

  return (void *)put;
}
