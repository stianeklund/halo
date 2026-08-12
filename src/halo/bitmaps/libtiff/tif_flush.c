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
 * 0xf0 and 0xf4 take FUN_00068d80 / FUN_00069420, whose own prototypes are
 * still unrecovered, so their `int` return is INFERRED from upstream's
 * `tif_setupdecode` / `tif_setupencode` rather than proven.
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
 * 0x04..0x13 is untouched by this body, hence pad_ rather than field_. */
typedef struct tiff_codec_bits_s {
  short data; /* 0x00 */
  short bit; /* 0x02 */
  char pad_004[16];
  const unsigned char *bitmap; /* 0x14 */
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
