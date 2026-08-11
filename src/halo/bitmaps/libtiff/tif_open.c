/* Horizontal differencing predictor accumulator, 8-bit samples.
 *
 * Transcribed from the vendored libtiff (tif_predict.c horAcc8) rather than
 * reshaped from the decompiler: the switch/fallthrough chain below is the
 * REPEAT4 Duff-device macro, and the binary reproduces it exactly -- jump
 * table at 0x6c6d4 with five entries (stride 0..4) plus a `cmp ecx,4 / ja`
 * bound check that lands on the counted-loop default arm.
 *
 * Bungie's copy takes the stride as an explicit third stack argument instead
 * of fetching it from PredictorState(tif)->stride, so there is no TIFF*
 * parameter here. Confirmed against 0x6c680-0x6c6d1.
 */

/* switch(n) with intentional fallthrough -- cases 4..1 emit `op` once each and
 * fall into the next, so `n` copies of `op` run for n <= 4, and the default
 * arm covers the remaining n-4 with a counted loop. Do NOT insert `break`. */
#define REPEAT4(n, op)              \
  switch (n) {                      \
  default: {                        \
    int i;                          \
    for (i = (n) - 4; i > 0; i--) { \
      op;                           \
    }                               \
  }                                 \
  case 4:                           \
    op;                             \
  case 3:                           \
    op;                             \
  case 2:                           \
    op;                             \
  case 1:                           \
    op;                             \
  case 0:;                          \
  }

/**
 * Undo horizontal differencing over a scanline of 8-bit samples in place.
 *
 * Each sample is replaced by the sum of itself and the sample `stride` bytes
 * before it, walking forward so the accumulation carries across the whole row.
 * The byte add wraps (the binary uses `add byte ptr [eax+ecx], dl`).
 *
 * @param cp     scanline base; at least `cc` bytes of caller memory.
 * @param cc     byte count of the scanline.
 * @param stride bytes between a sample and its horizontal predecessor
 *               (samples-per-pixel). Signed compares throughout.
 */
void FUN_0006c680(char *cp, int cc, int stride)
{
  if (cc > stride) {
    cc -= stride;
    do {
      REPEAT4(stride, cp[stride] = (char)(cp[stride] + *cp); cp++)
      cc -= stride;
    } while (cc > 0);
  }
}

/**
 * Undo horizontal differencing over a scanline of 16-bit samples in place.
 *
 * The 16-bit twin of FUN_0006c680: same REPEAT4 Duff device (jump table at
 * 0x6c764, five entries for stride 0..4, reached through `cmp edx,4 / ja`),
 * but every access is word-wide -- `mov si,[ecx]` / `add [ecx+edx*2], si` /
 * `add ecx,2` -- so the element type is 16-bit, not int.
 *
 * `cc` is a byte count that is halved to a word count before the compare.
 * The halving is SIGNED in the binary (`cdq / sub eax,edx / sar eax,1`), so
 * `cc` is a signed int; an unsigned count would emit a bare `shr`.
 *
 * The accumulation direction is `wp[stride] += wp[0]` -- the destination is
 * the FAR element and the source the near one. Reversing it still compiles
 * and still scores, so it is checked against 0x6c730 explicitly.
 *
 * @param wp     scanline base; at least `cc` bytes of caller memory.
 * @param cc     byte count of the scanline (halved internally to words).
 * @param stride words between a sample and its horizontal predecessor.
 */
void FUN_0006c6f0(unsigned short *wp, int cc, int stride)
{
  int wc = cc / 2;

  if (wc > stride) {
    wc -= stride;
    do {
      REPEAT4(stride, wp[stride] += wp[0]; wp++)
      wc -= stride;
    } while (wc > 0);
  }
}

/**
 * Apply horizontal differencing over a scanline of 8-bit samples in place.
 *
 * The encode-side inverse of FUN_0006c680 (libtiff tif_predict.c horDiff8):
 * each sample has its horizontal predecessor subtracted from it. Because the
 * predecessor must still hold its ORIGINAL value when it is read, the walk
 * runs BACKWARD from the end of the row -- `lea eax,[eax+edi-1]` at 0x6c873
 * seeds the cursor at cp + (cc - stride) - 1 and every body ends in `dec eax`.
 * Walking forward here would feed already-differenced bytes back in.
 *
 * Bungie's copy omits the upstream stride==3 / stride==4 pipelined arms and
 * keeps only the generic REPEAT4 tail: the binary goes straight from the LEA
 * to the `cmp ecx,4 / ja` bound check and the five-entry jump table at
 * 0x6c8bc. Same Duff device as the accumulate twins above.
 *
 * The loop head is the `cmp ecx,4` at 0x6c878, not the switch body, so the
 * stride dispatch is re-evaluated on every outer pass -- the do/while below
 * reproduces that. Direction is `cp[stride] -= cp[0]` (`mov dl,[eax]` then
 * `sub byte ptr [eax+ecx],dl`); the subtraction is NOT reversible. Byte math
 * wraps, and both compares are signed (`jle` / `jg`), so `cc` and `stride`
 * stay `int`. stride==0 with cc>0 spins forever here exactly as upstream does.
 *
 * @param cp     scanline base; at least `cc` bytes of caller memory.
 * @param cc     byte count of the scanline.
 * @param stride bytes between a sample and its horizontal predecessor
 *               (samples-per-pixel).
 */
void FUN_0006c860(char *cp, int cc, int stride)
{
  if (cc > stride) {
    cc -= stride;
    cp += cc - 1;
    do {
      REPEAT4(stride, cp[stride] = (char)(cp[stride] - *cp); cp--)
      cc -= stride;
    } while (cc > 0);
  }
}

/**
 * Apply horizontal differencing over a scanline of 16-bit samples in place.
 *
 * The 16-bit twin of FUN_0006c860 (libtiff tif_predict.c horDiff16), and the
 * encode-side inverse of FUN_0006c6f0. Every access is word-wide -- the body
 * is `mov di,[ecx]` / `sub word ptr [ecx+edx*2],di` / `sub ecx,2` -- so the
 * element type is 16-bit, not int. A 32-bit element type here is a silent
 * width bug that still compiles.
 *
 * `cc` is a byte count halved to a word count before the compare. The halving
 * is SIGNED in the binary (`cdq / sub eax,edx / sar eax,1` at 0x6c8d3-0x6c8dc),
 * so `cc` stays a signed int; an unsigned count would emit a bare `shr`.
 *
 * Like the 8-bit differencer the walk runs BACKWARD, because the predecessor
 * must still hold its ORIGINAL value when it is read: `lea ecx,[ecx+eax*2-2]`
 * at 0x6c8e8 seeds the cursor at wp + (wc - stride) - 1 words and every body
 * ends in `sub ecx,2`. Walking forward would feed already-differenced samples
 * back in, and would still compile and still score.
 *
 * Bungie's copy omits the upstream stride==3 / stride==4 pipelined arms and
 * keeps only the generic REPEAT4 tail: the binary goes from the LEA straight
 * to the `cmp edx,4 / ja` bound check and the five-entry jump table at
 * 0x6c944. The loop head is that `cmp edx,4`, not the switch body, so the
 * stride dispatch is re-evaluated on every outer pass -- the do/while below
 * reproduces that. Direction is `wp[stride] -= wp[0]`, destination FAR and
 * source near; the subtraction is NOT reversible. Both compares are signed
 * (`jle` / `jg`). ESI/EDI are saved only inside the taken branch, which is
 * the MSVC shape of the guard wrapping the whole body -- a plain `if`, not an
 * early `return`.
 *
 * @param wp     scanline base; at least `cc` bytes of caller memory.
 * @param cc     byte count of the scanline (halved internally to words).
 * @param stride words between a sample and its horizontal predecessor.
 */
void FUN_0006c8d0(unsigned short *wp, int cc, int stride)
{
  int wc = cc / 2;

  if (wc > stride) {
    wc -= stride;
    wp += wc - 1;
    do {
      REPEAT4(stride, wp[stride] -= wp[0]; wp--)
      wc -= stride;
    } while (wc > 0);
  }
}

/* Bit masks used by the packer below. Both live in .rdata in the original
 * image and both are NINE bytes, not eight -- element 8 (0xff) is present at
 * 0x2ec7d8 and 0x2ec7e4 respectively, each followed by alignment padding.
 *
 * tiff_msbmask[n]  = 0x2ec7d0, keeps the LOW n bits of a value.
 * tiff_leadmask[n] = 0x2ec7dc, keeps the HIGH n bits of a byte, i.e. the bits
 *                    already written at a sub-byte bit position.
 *
 * Declared static rather than imported at their original VAs: they are
 * read-only constants, and the direct `mov al, table[reg]` addressing form
 * that a static reproduces is what the original emits (an HDATA import would
 * add an __imp_ indirection the binary does not have).
 */
static const unsigned char tiff_msbmask[9] = { 0x00, 0x01, 0x03, 0x07, 0x0f,
                                               0x1f, 0x3f, 0x7f, 0xff };

static const unsigned char tiff_leadmask[9] = { 0x00, 0x80, 0xc0, 0xe0, 0xf0,
                                                0xf8, 0xfc, 0xfe, 0xff };

/* Codec private bit-writer state, reached through TIFF::tif_data.
 * Only the four offsets below are touched by FUN_0006c960; everything else
 * is padding as far as the recovery is concerned. Offsets are proven by
 * 0x6c972 (+0x06 movzx word), 0x6c976 (+0x14 dword), 0x6c979 (+0x18 dword)
 * and 0x6ca25 (+0x2c dword). +0x00 is proven separately by FUN_0006cda0
 * (0x6cda9 `mov eax,[esi]`, 0x6cdbc `mov dword ptr [esi],0xffffffff`). */
typedef struct tiff_bitstate_s {
  int oldcode; /* 0x00 code matched but not yet emitted, -1 when none pending */
  char pad_04[2];
  unsigned short nbits; /* 0x06 bits emitted per call, constant per strip */
  char pad_08[12];
  int bitpos; /* 0x14 write cursor, in bits from tif_rawdata */
  int bitlimit; /* 0x18 capacity of the raw buffer, in bits */
  char pad_1c[16];
  int bitcount; /* 0x2c running total of bits emitted */
} tiff_bitstate_t;

/* The three TIFF fields this function reaches: +0x120 codec private state
 * (0x6c96c), +0x12c raw write pointer (0x6c991/0x6c9a7/0x6c9bb/0x6c9cc) and
 * +0x138 raw byte count (0x6c99a/0x6ca40). This layout is NOT upstream
 * libtiff's -- there tif_rawcc immediately follows tif_rawcp; here they are
 * 12 bytes apart -- so nothing between them is named. */
/* Codec method pointer types, from upstream libtiff's tiffiop.h. `tif` is
 * declared void* throughout this TU so the generated header needs no libtiff
 * types; the six installed stubs already carry exactly these shapes in
 * kb.json (0x6cb00/0x6cfa0 are (tif, buf, cc, s), 0x6cda0 is (tif) -> int,
 * 0x6cac0 is (tif) -> void). */
typedef int (*tiff_bool_method_t)(void *tif);
typedef int (*tiff_code_method_t)(void *tif, char *buf, int cc, int s);
typedef void (*tiff_void_method_t)(void *tif);

typedef struct tiff_s {
  /* UNRESOLVED layout conflict in 0x00-0xef. The td_* fields below were split
   * out of this range on the inference that Bungie inlined the directory at
   * the TIFF base (upstream libtiff reaches them through a nested
   * `TIFFDirectory tif_dir` member), so they carry their upstream td_* names
   * at their absolute offsets. That inference cannot hold at 0x00 as well:
   * TIFFFileName (0x6d850) is `mov eax,[ebp+8] / mov eax,[eax]`, i.e. it
   * returns the dword at offset 0x00 as the file name, which is upstream's
   * `tif_name` -- the first member of `struct tiff`, not of TIFFDirectory.
   * Only the offsets TIFFScanlineSize (0x6d820) and TIFFFileName touch are
   * split out; everything else in this range is still unobserved, and which
   * of the two readings is right for the rest of it is unproven.
   * Widths are load-bearing: 0x36/0x44/0x5e are read with `movzx ... word`
   * (0x6d823, 0x6d836, 0x6d82d) and 0x1c with a dword `imul` operand
   * (0x6d827). */
  char *tif_name; /* 0x00 */
  /* Upstream libtiff declares tif_fd as `int`. This binary reads it with
   * `movsx eax, word ptr [eax+4]` (TIFFFileno, 0x6d866), i.e. a SIGNED 16-bit
   * load, so Bungie's field is a short. Declaring it `int` produces a plain
   * dword MOV and does not match. */
  short tif_fd; /* 0x04 */
  /* Upstream libtiff declares tif_mode as `int`, and places it immediately
   * after tif_fd. This binary reads offset 0x06 with `movsx eax, word ptr
   * [eax+6]` (TIFFGetMode, 0x6d876) -- a SIGNED 16-bit load at exactly the
   * offset upstream's field order predicts once tif_fd is 16-bit, so this is
   * the same `int`-narrowed-to-`short` pattern as tif_fd. Declaring it `int`
   * produces a plain dword MOV and does not match. */
  short tif_mode; /* 0x06 */
  char pad_008[2];
  /* Flags byte. TIFFIsTiled (0x6d880) reads it with `movsx eax, byte ptr
   * [eax+0xa]` and tests bit 7 (`and eax,0x80 / shr eax,7`), so this offset IS
   * accessed and cannot stay padding. The load is a SIGNED byte, hence `char`
   * and not `unsigned char` -- an unsigned field produces `movzx` and does not
   * match.
   *
   * Upstream libtiff has a single `uint32 tif_flags` here with
   * TIFF_ISTILED == 0x0400, i.e. bit 10. This binary tests bit 7 of the byte
   * at 0x0a, which is bit 23 of a dword at 0x08 -- not an upstream flag bit at
   * all. Either Bungie renumbered the flag word or 0x0a is a separate byte
   * field; nothing local proves which, so the field keeps a mechanical name
   * and the surrounding bytes stay padding. Do not import upstream's
   * TIFF_ISTILED value. */
  char field_0a; /* 0x0a */
  char pad_00b[0x11];
  long td_imagewidth; /* 0x1c */
  char pad_020[0x16];
  unsigned short td_bitspersample; /* 0x36 */
  char pad_038[0x0c];
  unsigned short td_samplesperpixel; /* 0x44 */
  char pad_046[0x18];
  unsigned short td_planarconfig; /* 0x5e */
  char pad_060[0x74];
  /* Current scanline. TIFFCurrentRow (0x6d8a0) reads it with a plain dword
   * `mov eax,[eax+0xd4]` -- no MOVSX/MOVZX, so this is a full 32-bit field and
   * no narrower spelling is admissible. Upstream libtiff types the member
   * `uint32 tif_row`; the OFFSET is Bungie's, not upstream's (their tif_row
   * sits far earlier in TIFF), so only the name is transcribed. */
  unsigned long tif_row; /* 0xd4 */
  /* Current directory index. TIFFCurrentDirectory (0x6d8b0) reads it with a
   * plain dword `mov eax,[eax+0xd8]` -- no MOVSX/MOVZX, so this is a full
   * 32-bit field. Upstream libtiff types the member `tdir_t tif_curdir`, i.e.
   * a uint16; a 16-bit field here would compile to `movzx eax,word ptr` and
   * would not match, so only the NAME is transcribed from upstream, not the
   * width. The OFFSET is Bungie's (upstream places tif_curdir far earlier in
   * TIFF). Signedness is unobservable from a bare dword load; the field keeps
   * upstream's unsigned typing, matching tif_row above. */
  unsigned long tif_curdir; /* 0xd8 */
  char pad_0dc[0x14];
  /* Codec vtable, 0xf0-0x11c, installed wholesale by FUN_0006d2d0. Upstream
   * libtiff orders these setupdecode, predecode, setupencode, preencode,
   * postencode, then the six code methods, then close/seek/cleanup; Bungie's
   * copy has no predecode/preencode slot here (nothing writes 0xf0-0x11c
   * except FUN_0006d2d0, and it writes exactly ten of the twelve dwords).
   * Identities are proven by the installed stub bodies: 0xf8 gets
   * FUN_0006cda0, which is upstream LZWPostEncode verbatim, and 0x11c gets
   * FUN_0006cac0, which is LZWCleanup; 0xf0/0xf4 get the two tif_data
   * allocators at tif_lzw.c lines 308 and 619, i.e. LZWSetupDecode and
   * LZWSetupEncode. The row/strip/tile suborder of the six code slots is
   * INFERRED from upstream's field order -- locally the binary only proves
   * that 0xfc/0x104/0x10c take the decoder and 0x100/0x108/0x110 the
   * encoder. */
  tiff_bool_method_t tif_setupdecode; /* 0xf0 */
  tiff_bool_method_t tif_setupencode; /* 0xf4 */
  tiff_bool_method_t tif_postencode; /* 0xf8 */
  tiff_code_method_t tif_decoderow; /* 0xfc */
  tiff_code_method_t tif_encoderow; /* 0x100 */
  tiff_code_method_t tif_decodestrip; /* 0x104 */
  tiff_code_method_t tif_encodestrip; /* 0x108 */
  tiff_code_method_t tif_decodetile; /* 0x10c */
  tiff_code_method_t tif_encodetile; /* 0x110 */
  char
    pad_114[8]; /* 0x114/0x118 -- close/seek in upstream; never written here */
  tiff_void_method_t tif_cleanup; /* 0x11c */
  tiff_bitstate_t *tif_data; /* 0x120 */
  char pad_124[8];
  unsigned char *tif_rawcp; /* 0x12c */
  char pad_130[8];
  long tif_rawcc; /* 0x138 */
} tiff_t;

/**
 * Append `sp->nbits` bits of `value` to the raw output buffer, MSB first,
 * flushing the buffer first if the write would run past the bit limit.
 *
 * The write lands at the current bit cursor and spans one, two or three
 * bytes: the head byte is merged with the bits already present (preserving
 * the top `bitpos & 7` of them via tiff_leadmask), an optional whole middle
 * byte follows, and any residual bits are left-justified into the next byte
 * through tiff_msbmask. `cp` is advanced past every byte actually stored.
 *
 * Flush handling has two shapes, and the difference matters. When the cursor
 * sits on a byte boundary the buffer is simply flushed. When it does not, the
 * partially-filled byte must survive: the OLD write pointer plus the whole-
 * byte offset is captured BEFORE the flush, tif_rawcc is trimmed to the whole
 * bytes only, and after the flush that saved byte is copied to the front of
 * the freshly-reset buffer. Reading it after the flush instead would read the
 * new buffer -- the load at 0x6c9ad uses the pre-call ESI on purpose.
 *
 * @param tif   TIFF handle (declared void* so the generated header needs no
 *              libtiff types); tif->tif_data must be the bit-writer state.
 * @param value right-justified bit payload. SIGNED: both extractions are
 *              `sar` (0x6c9ef, 0x6ca03), so the sign bit propagates.
 */
void FUN_0006c960(void *tif_, long value)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_bitstate_t *sp = tif->tif_data;
  int nbits = sp->nbits;
  int bitpos = sp->bitpos;
  unsigned char *cp;
  int shift;

  /* signed compare (`jle` at 0x6c984) */
  if (nbits + bitpos > sp->bitlimit) {
    if ((bitpos & 7) == 0) {
      TIFFFlushData1(tif);
    } else {
      unsigned char *op = tif->tif_rawcp + (bitpos >> 3);
      tif->tif_rawcc = bitpos >> 3;
      TIFFFlushData1(tif);
      *tif->tif_rawcp = *op; /* carry the partial byte across the flush */
    }
    cp = tif->tif_rawcp;
    bitpos &= 7;
    sp->bitpos = bitpos;
  } else {
    cp = tif->tif_rawcp + (bitpos >> 3);
    bitpos &= 7;
  }

  shift = nbits + bitpos - 8;
  *cp = (unsigned char)((tiff_leadmask[bitpos] & *cp) | (value >> shift));
  cp++;
  if (shift >= 8) {
    shift -= 8;
    *cp++ = (unsigned char)(value >> shift);
  }
  if (shift != 0) {
    *cp = (unsigned char)((tiff_msbmask[shift] & value) << (8 - shift));
  }

  /* Re-read sp->nbits rather than reusing the local: the tail issues a fresh
   * `movzx eax, word ptr [edi+6]` at 0x6ca21. */
  sp->bitpos += sp->nbits;
  sp->bitcount += sp->nbits;
  tif->tif_rawcc = (sp->bitpos + 7) >> 3;
}

/**
 * Release the codec private state hanging off a TIFF handle.
 *
 * Upstream libtiff's LZWCleanup: `if (tif->tif_data) {
 * _TIFFfree(tif->tif_data); tif->tif_data = NULL; }`. Bungie's _TIFFfree
 * expands to the debug allocator, so the call carries __FILE__/__LINE__ -- and
 * that __FILE__ is tif_lzw.c, not tif_open.c: kb.json lumps every vendored
 * libtiff translation unit into a single tif_open.obj, so this body lives here
 * alongside the tif_predict.c and tif_lzw.c neighbours. Line 925 (0x39d) is the
 * free site.
 *
 * The handle field is loaded once (0x6cac6 `mov eax,[esi+0x120]`) and the same
 * EAX is both tested and pushed as the free argument, hence the local.
 * The store-back to 0x120 is inside the taken branch, after the free.
 *
 * @param tif_ TIFF handle; may carry a null tif_data, in which case nothing
 *             happens. The handle pointer itself is never checked.
 */
void FUN_0006cac0(void *tif_)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_bitstate_t *sp = tif->tif_data;

  if (sp) {
    debug_free(sp, "c:\\halo\\SOURCE\\bitmaps\\libtiff\\tif_lzw.c", 0x39d);
    tif->tif_data = 0;
  }
}

/* Predictor private state, reached through TIFF::tif_data. Only two offsets
 * are touched here: +0x08, read as a zero-extended word (0x6cd18
 * `movzx edx, word ptr [esi+8]`) and handed to the accumulator as its third
 * argument -- the same slot the already-recovered horizontal accumulators
 * FUN_0006c680/FUN_0006c6f0 take their `stride` in -- and +0x0c, the
 * accumulator itself (0x6cd1f `call dword ptr [esi+0xc]`). This is upstream
 * libtiff's TIFFPredictorState with Bungie's explicit-stride accumulator
 * signature; everything before +0x08 is unproven from this function.
 * +0x0a comes from FUN_0006cd40, which reads it as a word at 0x6cd71 and
 * 0x6cd7f and uses it as both the accumulator's byte count and the stride
 * by which the tile buffer advances -- upstream's TIFFPredictorState.rowsize
 * narrowed to 16 bits. Offsets 0x00-0x07 remain unobserved. */
typedef struct tiff_predictor_state_s {
  char pad_00[8];
  unsigned short stride; /* 0x08 samples between a value and its predecessor */
  unsigned short rowsize; /* 0x0a bytes per decoded row (0x6cd71/0x6cd7f) */
  void (*pfunc)(char *cp, int cc, int stride); /* 0x0c horizontal accumulator */
} tiff_predictor_state_t;

/**
 * Decode one scanline through the predictor: run the parent codec, then undo
 * the horizontal differencing in place.
 *
 * Upstream libtiff's PredictorDecodeRow. Two differences from stock, both
 * proven by the disassembly: the parent codec row decoder is called directly
 * (0x6cd0c `call 0x6cb00`) rather than through a coderow slot in the state,
 * and the accumulator receives the stride as an explicit third argument
 * instead of fetching it from the state itself.
 *
 * The state pointer is loaded from the handle BEFORE the codec call (0x6ccfe,
 * kept in callee-saved ESI across it) and is never re-read afterwards, so the
 * accumulator runs against the state as it was on entry.
 *
 * @param tif_ TIFF handle (declared void* so the generated header needs no
 *             libtiff types); tif->tif_data must be the predictor state.
 * @param op0  scanline buffer, decoded in place by the codec then accumulated.
 * @param occ0 byte count of the scanline.
 * @param s    sample number, passed through to the parent codec untouched.
 * @return 1 when the parent codec succeeded and the row was accumulated,
 *         0 when it failed (the accumulator is then not run).
 */
int FUN_0006ccf0(void *tif_, char *op0, int occ0, int s)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_predictor_state_t *sp = (tiff_predictor_state_t *)tif->tif_data;

  if (FUN_0006cb00(tif_, op0, occ0, s)) {
    (*sp->pfunc)(op0, occ0, sp->stride);
    return 1;
  }
  return 0;
}

/**
 * Decode a whole tile/strip through the predictor: run the parent codec once,
 * then undo the horizontal differencing one row at a time.
 *
 * Upstream libtiff's PredictorDecodeTile, with the same two Bungie deviations
 * seen in FUN_0006ccf0: the parent codec is called directly (0x6cd5c
 * `call 0x6cb00`) instead of through a codetile slot, and the accumulator
 * takes the stride as an explicit third argument.
 *
 * The state pointer is loaded from the handle BEFORE the codec call (0x6cd4e,
 * kept in callee-saved ESI across it), so the loop runs against the state as
 * it was on entry. Both words are re-read from the state inside the loop --
 * `stride` at the top of every iteration (0x6cd75) and `rowsize` after every
 * accumulator call (0x6cd7f), whose value then feeds BOTH the count decrement
 * and the buffer advance. Hoisting either read out of the loop would change
 * behaviour if the accumulator mutates the state, and it is not what the
 * binary does.
 *
 * @param tif_ TIFF handle (declared void* so the generated header needs no
 *             libtiff types); tif->tif_data must be the predictor state.
 * @param op0  tile buffer, decoded in place by the codec then accumulated.
 * @param occ0 byte count of the tile. Signed throughout (`test edi,edi / jg`),
 *             so a rowsize that overshoots the remainder ends the loop.
 * @param s    sample number, passed through to the parent codec untouched.
 * @return 1 when the parent codec succeeded, 0 when it failed (the loop is
 *         then not run).
 */
int FUN_0006cd40(void *tif_, char *op0, int occ0, int s)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_predictor_state_t *sp = (tiff_predictor_state_t *)tif->tif_data;

  if (!FUN_0006cb00(tif_, op0, occ0, s)) {
    return 0;
  }
  while (occ0 > 0) {
    (*sp->pfunc)(op0, sp->rowsize, sp->stride);
    occ0 -= sp->rowsize;
    op0 += sp->rowsize;
  }
  return 1;
}

/* LZW end-of-information code -- upstream libtiff's CODE_EOI (tif_lzw.c).
 * The binary pushes it as the literal 0x101 at 0x6cdc5. */
#define CODE_EOI 257

/**
 * Finish an LZW-encoded strip: flush the pending code, then emit EOI.
 *
 * Upstream libtiff's LZWPostEncode. `oldcode` is the last string code the
 * encoder matched but had not yet written out; the sentinel -1 means there is
 * none pending, and it is reset to -1 after being flushed so the pending half
 * of a second call is a no-op.
 *
 * The state field is loaded once (0x6cda9 `mov eax,[esi]`) and that same EAX
 * is both compared against -1 and pushed as the code argument, hence the
 * local. Bungie's copy keeps the code as a full dword -- the test is
 * `cmp eax,-1` and the reset is a dword store -- not upstream's u_short
 * hcode_t.
 *
 * @param tif_ TIFF handle (declared void* so the generated header needs no
 *             libtiff types); tif->tif_data must be the LZW encoder state.
 * @return always 1; this stage reports no failure (`mov eax,1` at 0x6cdd4,
 *         scheduled between the two epilogue pops).
 */
int FUN_0006cda0(void *tif_)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_bitstate_t *sp = tif->tif_data;
  int oldcode = sp->oldcode;

  if (oldcode != -1) {
    FUN_0006c960(tif_, oldcode);
    sp->oldcode = -1;
  }
  FUN_0006c960(tif_, CODE_EOI);
  return 1;
}

/**
 * Encode one scanline through the predictor: apply the horizontal differencing
 * in place, then hand the differenced row to the parent codec.
 *
 * Upstream libtiff's PredictorEncodeRow, and the encode-side mirror of
 * FUN_0006ccf0. The same two Bungie deviations seen on the decode side hold
 * here, both proven by the disassembly: the parent codec row encoder is called
 * directly (0x6d166 `call 0x6cfa0`) rather than through a coderow slot in the
 * state, and the differencer receives the stride as an explicit third argument
 * (0x6d151 `movzx ecx, word ptr [eax+8]` -- a zero-extended word, not a dword)
 * instead of fetching it from the state itself.
 *
 * Ordering is the reverse of the decode path: the differencer runs FIRST and
 * rewrites the caller's buffer in place -- upstream's own comment flags this
 * as an abuse of user data -- and only then does the codec consume it.
 *
 * The state pointer is read once, before the differencer call (0x6d14b), and
 * is never re-read afterwards.
 *
 * @param tif_ TIFF handle (declared void* so the generated header needs no
 *             libtiff types); tif->tif_data must be the predictor state.
 * @param bp0  scanline buffer, differenced in place then encoded.
 * @param cc0  byte count of the scanline.
 * @param s    sample number, passed through to the parent codec untouched.
 * @return the parent codec's status, forwarded untouched -- the binary leaves
 *         EAX from `call 0x6cfa0` alone through the whole epilogue
 *         (0x6d16b-0x6d172), so this is a value-returning function despite the
 *         decompiler typing it void.
 */
int FUN_0006d140(void *tif_, char *bp0, int cc0, int s)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_predictor_state_t *sp = (tiff_predictor_state_t *)tif->tif_data;

  /* XXX horizontal differencing alters user's data XXX */
  (*sp->pfunc)(bp0, cc0, sp->stride);
  return FUN_0006cfa0(tif_, bp0, cc0, s);
}

/**
 * Encode a whole tile/strip through the predictor: apply the horizontal
 * differencing one row at a time, then hand the whole differenced buffer to
 * the parent codec.
 *
 * Upstream libtiff's PredictorEncodeTile, and the encode-side mirror of
 * FUN_0006cd40. The same two Bungie deviations hold: the parent codec tile
 * encoder is called directly (0x6d1c9 `call 0x6cfa0`) instead of through an
 * encodetile slot in the state, and the differencer receives the stride as an
 * explicit third argument (0x6d1a0 `movzx ecx, word ptr [esi+8]`).
 *
 * The state pointer is loaded once in the prologue (0x6d18b, callee-saved ESI)
 * and never re-read. Both words ARE re-read from the state inside the loop --
 * `stride` at the top of every iteration and `rowsize` after every differencer
 * call (0x6d1aa), whose value then feeds BOTH the count decrement and the
 * buffer advance. Hoisting either read would change behaviour if the
 * differencer mutates the state, and it is not what the binary does.
 *
 * The loop walks COPIES: the binary keeps the running pointer in EBX and the
 * running count in EDI, and reloads the untouched originals from the frame for
 * the codec call (0x6d1b9 `mov edi,[ebp+0x10]`, 0x6d1bf `mov eax,[ebp+0xc]`),
 * so the codec sees the full buffer and the full byte count, not the loop
 * residue.
 *
 * @param tif_ TIFF handle (declared void* so the generated header needs no
 *             libtiff types); tif->tif_data must be the predictor state.
 * @param bp0  tile buffer, differenced in place then encoded.
 * @param cc0  byte count of the tile. Signed throughout (`test edi,edi / jle`
 *             on entry, `jg` on the back edge), so a rowsize that overshoots
 *             the remainder ends the loop.
 * @param s    sample number, passed through to the parent codec untouched.
 * @return the parent codec's status, forwarded untouched -- the binary leaves
 *         EAX from `call 0x6cfa0` alone through the whole epilogue
 *         (0x6d1ce-0x6d1d5), so this is a value-returning function despite the
 *         decompiler typing it void.
 */
int FUN_0006d180(void *tif_, char *bp0, int cc0, int s)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_predictor_state_t *sp = (tiff_predictor_state_t *)tif->tif_data;
  char *bp = bp0;
  int cc = cc0;

  /* XXX horizontal differencing alters user's data XXX */
  while (cc > 0) {
    (*sp->pfunc)(bp, sp->rowsize, sp->stride);
    cc -= sp->rowsize;
    bp += sp->rowsize;
  }
  return FUN_0006cfa0(tif_, bp0, cc0, s);
}

/**
 * Install the LZW codec method table into a TIFF handle.
 *
 * The vtable-install half of upstream libtiff's TIFFInitLZW (tif_lzw.c --
 * confirmed as this TU's __FILE__ at 0x2604d8, which the neighbouring
 * allocators stamp into their debug-allocator calls). Bungie split the
 * scheme check, the tif_data reset and the TIFFPredictorInit call out of it:
 * what is left at 0x6d2d0 is ten stores and `return 1`, with no CALL, no
 * branch and no locals -- the frame is a bare push-ebp/mov-ebp,esp.
 *
 * Two upstream slots are absent from Bungie's copy: tif_predecode and
 * tif_preencode are never written here, and the two dwords upstream would
 * place at 0x114/0x118 (close/seek) are likewise untouched. That is
 * consistent with the rest of this TU, where the predictor wrappers call the
 * parent codec directly (FUN_0006d140/FUN_0006d180 `call 0x6cfa0`) instead of
 * dispatching through a coderow slot.
 *
 * Store order follows upstream: the decode group first, then the encode
 * group. That grouping -- not ascending offset -- is what the binary's
 * scheduling shows, since the three FUN_0006cb00 stores are emitted back to
 * back off one `mov ecx,0x6cb00`, and the three FUN_0006cfa0 stores back to
 * back off one `mov ecx,0x6cfa0`, with the three immediate stores hoisted in
 * between the two ECX loads.
 *
 * @param tif_ TIFF handle (declared void* so the generated header needs no
 *             libtiff types). Never null-checked.
 * @return always 1; `mov eax,1` at 0x6d32b, immediately before the epilogue.
 *         The decompiler types this void because nothing in the cached
 *         listing consumes EAX (void-EAX hazard).
 */
int FUN_0006d2d0(void *tif_)
{
  tiff_t *tif = (tiff_t *)tif_;

  tif->tif_setupdecode = FUN_0006ce60;
  tif->tif_decoderow = FUN_0006cb00;
  tif->tif_decodestrip = FUN_0006cb00;
  tif->tif_decodetile = FUN_0006cb00;
  tif->tif_setupencode = FUN_0006d1e0;
  tif->tif_postencode = FUN_0006cda0;
  tif->tif_encoderow = FUN_0006cfa0;
  tif->tif_encodestrip = FUN_0006cfa0;
  tif->tif_encodetile = FUN_0006cfa0;
  tif->tif_cleanup = FUN_0006cac0;
  return 1;
}

/**
 * Repoint the three decode slots of the codec vtable at FUN_0006d340.
 *
 * Overwrites only tif_decoderow/tif_decodestrip/tif_decodetile (0xfc, 0x104,
 * 0x10c), leaving the interleaved encode slots at 0x100/0x108/0x110 and the
 * rest of the vtable installed by FUN_0006d2d0 untouched. The three offsets
 * are 8 bytes apart, so this is deliberately three independent field stores
 * and not a run over an array of pointers.
 *
 * The whole body is ten instructions at 0x6d4d0-0x6d4f3: `push ebp / mov
 * ebp,esp` with no `sub esp`, `mov eax,[ebp+8]`, one `mov ecx,0x6d340`, three
 * stores off that single ECX, `mov eax,1`, `pop ebp / ret`. Keeping the same
 * expression in all three assignments is what reproduces the single constant
 * materialisation; introducing a local temp adds a frame the original has not
 * got.
 *
 * FUN_0006d340 is only ever taken by address, never called, so kb.json still
 * carries its placeholder `void (void)` prototype and the assignment casts.
 * Widening that prototype would be inferred from the slot it lands in rather
 * than from its own body, so it is left alone.
 *
 * @param tif_ TIFF handle (declared void* so the generated header needs no
 *             libtiff types). Never null-checked.
 * @return always 1; `mov eax,1` at 0x6d4ed, after the stores and immediately
 *         before the epilogue. The decompiler types this void because nothing
 *         in the cached listing consumes EAX (void-EAX hazard).
 */
int FUN_0006d4d0(void *tif_)
{
  tiff_t *tif = (tiff_t *)tif_;

  tif->tif_decoderow = (tiff_code_method_t)FUN_0006d340;
  tif->tif_decodestrip = (tiff_code_method_t)FUN_0006d340;
  tif->tif_decodetile = (tiff_code_method_t)FUN_0006d340;
  return 1;
}

/* Upstream libtiff spellings (tiff.h / tiffiop.h). TIFFhowmany casts to
 * unsigned before dividing, which is what makes the divide a plain `shr`
 * rather than the signed power-of-two sequence; the binary ends on
 * `add eax,7 / shr eax,3` at 0x6d83c-0x6d841, so the unsigned form is the
 * one Bungie compiled. The alternative `((x)&7)?((x)>>3)+1:((x)>>3)` spelling
 * of TIFFhowmany8 would emit a test and a branch, and there is none. */
#define PLANARCONFIG_CONTIG 1
#define TIFFhowmany(x, y) \
  ((((unsigned long)(x)) + (((unsigned long)(y)) - 1)) / ((unsigned long)(y)))
#define TIFFhowmany8(x) (TIFFhowmany((x), 8))

/**
 * Size in bytes of one decoded scanline of the currently open directory.
 *
 * Transcribed from the vendored libtiff (tif_strip.c TIFFScanlineSize) rather
 * than reshaped from the decompiler -- Ghidra's cached listing has 0x6d820 as
 * an empty `void(void)` body, so the disassembly at 0x6d820-0x6d843 is the
 * only usable evidence. It matches upstream instruction for instruction.
 *
 * The multiply order is bits-per-sample first: `movzx eax,[ecx+0x36]` then
 * `imul eax,[ecx+0x1c]`. Swapping the operands changes which one lands in EAX
 * and therefore the IMUL form.
 *
 * @param file TIFF handle. kb.json types this `int` because the caller in
 *             tiff_file.c holds it as one; it is really a tiff_t*, cast here
 *             rather than widening the prototype and every call site.
 * @return bytes per scanline, rounded up to a whole byte. Returned in EAX.
 */
int TIFFScanlineSize(int file)
{
  tiff_t *tif = (tiff_t *)file;
  int scanline;

  scanline = tif->td_bitspersample * tif->td_imagewidth;
  if (tif->td_planarconfig == PLANARCONFIG_CONTIG) {
    scanline *= tif->td_samplesperpixel;
  }
  return (int)TIFFhowmany8(scanline);
}

/**
 * Name of the file backing an open TIFF handle.
 *
 * Transcribed from the vendored libtiff (tif_open.c TIFFFileName), whose body
 * is literally `return tif->tif_name;`. Ghidra's cached listing has 0x6d850 as
 * an empty `void(void)` body -- a decl artifact, since the disassembly at
 * 0x6d850-0x6d859 is six instructions with one stack argument and an EAX
 * return: `push ebp / mov ebp,esp / mov eax,[ebp+8] / mov eax,[eax] / pop ebp
 * / ret`. cdecl, no callee cleanup, no register arguments.
 *
 * The dereference is at struct offset 0x00, which is upstream's `tif_name`;
 * see the layout note on tiff_t for the conflict this creates with the
 * directory fields split out of the same range.
 *
 * There is no `sub esp` in the original, so no local is introduced here --
 * the cast happens inside the return expression. A `tiff_t *tif` temp of the
 * kind the rest of this file uses would cost frame shape on a six-instruction
 * function.
 *
 * @param tif TIFF handle (declared void* so the generated header needs no
 *            libtiff types). Never null-checked, exactly as upstream.
 * @return the stored file name pointer, returned in EAX.
 */
char *TIFFFileName(void *tif)
{
  return ((tiff_t *)tif)->tif_name;
}

/**
 * File descriptor backing an open TIFF handle.
 *
 * Transcribed from the vendored libtiff (tif_open.c TIFFFileno), whose body is
 * literally `return tif->tif_fd;`. Ghidra's cached listing has 0x6d860 as an
 * empty `void(void)` body -- the void-EAX artifact (lift-learnings s16), since
 * nothing in the cached listing consumes the return. The disassembly at
 * 0x6d860-0x6d86b is six instructions with one stack argument and an EAX
 * return: `push ebp / mov ebp,esp / mov eax,[ebp+8] / movsx eax,word [eax+4] /
 * pop ebp / ret`. cdecl, no callee cleanup, no register arguments, no locals
 * (there is no `sub esp`), so no `tiff_t *tif` temp is introduced here -- the
 * cast happens inside the return expression, matching TIFFFileName above.
 *
 * The load is MOVSX word, not a dword MOV: the field at 0x04 is a signed
 * 16-bit value in this build even though upstream types tif_fd as `int`. See
 * the tiff_t layout note.
 *
 * @param tif TIFF handle (declared void* so the generated header needs no
 *            libtiff types). Never null-checked, exactly as upstream.
 * @return the stored descriptor, sign-extended into EAX.
 */
int TIFFFileno(void *tif)
{
  return ((tiff_t *)tif)->tif_fd;
}

/**
 * Open mode (the O_* flags) an open TIFF handle was created with.
 *
 * Transcribed from the vendored libtiff (tif_open.c TIFFGetMode), whose body
 * is literally `return tif->tif_mode;`. Ghidra's cached listing has 0x6d870 as
 * an empty `void(void)` body -- the void-EAX artifact (lift-learnings s16),
 * since nothing in the cached listing consumes the return. The XBE bytes at
 * 0x6d870-0x6d87b are `55 8b ec 8b 45 08 0f bf 40 06 5d c3`, i.e. six
 * instructions with one stack argument and an EAX return: `push ebp / mov
 * ebp,esp / mov eax,[ebp+8] / movsx eax,word [eax+6] / pop ebp / ret`. cdecl,
 * no callee cleanup, no register arguments, no locals (there is no `sub esp`),
 * so no `tiff_t *tif` temp is introduced here -- the cast happens inside the
 * return expression, matching TIFFFileName and TIFFFileno above.
 *
 * The load is MOVSX word, not a dword MOV: the field at 0x06 is a signed
 * 16-bit value in this build even though upstream types tif_mode as `int`.
 * See the tiff_t layout note.
 *
 * @param tif TIFF handle (declared void* so the generated header needs no
 *            libtiff types). Never null-checked, exactly as upstream.
 * @return the stored open mode, sign-extended into EAX.
 */
int TIFFGetMode(void *tif)
{
  return ((tiff_t *)tif)->tif_mode;
}

/**
 * Whether an open TIFF handle describes a tiled image (rather than
 * strip-based).
 *
 * Upstream libtiff exposes TIFFIsTiled as a macro over tif_flags; Bungie's copy
 * is an out-of-line function, so it is transcribed from the disassembly rather
 * than from upstream source. Ghidra's cached listing has 0x6d880 as an empty
 * `void(void)` body -- the void-EAX artifact (lift-learnings s16), since
 * nothing in the cached listing consumes the return. The eight instructions at
 * 0x6d880-0x6d893 are `push ebp / mov ebp,esp / mov eax,[ebp+8] / movsx eax,
 * byte ptr [eax+0xa] / and eax,0x80 / shr eax,7 / pop ebp / ret`: cdecl, one
 * stack argument, no callee cleanup, no register arguments, no locals (there is
 * no `sub esp`), so no `tiff_t *tif` temp is introduced here -- the cast
 * happens inside the return expression, matching TIFFFileName, TIFFFileno and
 * TIFFGetMode above.
 *
 * The returned value is the shifted bit, i.e. literally 0 or 1, NOT a
 * normalised boolean: the original emits `and`/`shr`, so writing `!= 0` or a
 * ternary here would generate a setcc/test sequence instead. See the field_0a
 * note in the tiff_t layout for why bit 7 of the byte at 0x0a (and not
 * upstream's TIFF_ISTILED == 0x0400) is the tested flag.
 *
 * The `u` on the mask is load-bearing, not decoration. With a plain `0x80`
 * MSVC 7.1 proves the mask redundant against a zero-extended byte load and
 * folds `movsx`+`and` into a bare `movzx`, dropping the `and` entirely (7
 * instructions, 80.0%). Forcing the mask into the unsigned domain keeps the
 * `and` and reproduces the reference's 8-instruction shape (87.5%). Measured,
 * not assumed: `(int)` casts, a named local, `/ 128`, and a volatile-qualified
 * read all fold the same way and all score 80.0%.
 *
 * The single residual instruction is the load: the reference uses `movsx`, our
 * VC71 build `movzx`. That is unreachable from source here -- every form that
 * keeps the signed load also lets MSVC drop the `and`. The reference's
 * `movsx`+`and 0x80` pair is itself the proof that the field is a SIGNED byte
 * in Bungie's header (an unsigned field would have been loaded with `movzx`
 * there too). The alternative reading -- a dword flags word at 0x08 with the
 * mask 0x00800000 narrowed to a byte access, which would match upstream's
 * field order -- was tested and disproven: MSVC 7.1 does not narrow it, it
 * keeps the dword load and scores 75.0% for both signed and unsigned spellings.
 *
 * @param tif TIFF handle (declared void* so the generated header needs no
 *            libtiff types). Never null-checked, exactly as upstream.
 * @return 1 when the handle is tiled, 0 when it is strip-based, in EAX.
 */
int TIFFIsTiled(void *tif)
{
  return (((tiff_t *)tif)->field_0a & 0x80u) >> 7;
}

/**
 * Scanline index the handle's decoder/encoder is currently positioned at.
 *
 * Transcribed from the vendored libtiff (tif_open.c TIFFCurrentRow), whose
 * body is literally `return tif->tif_row;`. Ghidra's cached listing has
 * 0x6d8a0 as an empty `void(void)` body -- the void-EAX artifact
 * (lift-learnings s16), since nothing in the cached listing consumes the
 * return, and kb.json carried the same wrong `void TIFFCurrentRow(void)`
 * prototype. The XBE bytes at 0x6d8a0-0x6d8ad are six instructions with one
 * stack argument and an EAX return: `push ebp / mov ebp,esp / mov eax,[ebp+8]
 * / mov eax,[eax+0xd4] / pop ebp / ret`. cdecl, no callee cleanup (plain
 * `ret`, not `ret n`), no register arguments, no locals (there is no `sub
 * esp`), so no `tiff_t *tif` temp is introduced here -- the cast happens
 * inside the return expression, matching TIFFFileName, TIFFFileno,
 * TIFFGetMode and TIFFIsTiled above.
 *
 * The load is a plain dword MOV, not MOVSX/MOVZX word or byte, so the field at
 * 0xd4 is a full 32-bit value and there is no width-narrowing hazard here (the
 * signed 16-bit reads that TIFFFileno and TIFFGetMode perform have no analogue
 * in this function). Signedness is unobservable from a bare load; the field
 * keeps upstream's unsigned typing.
 *
 * @param tif TIFF handle (declared void* so the generated header needs no
 *            libtiff types). Never null-checked, exactly as upstream.
 * @return the current scanline index, in EAX.
 */
unsigned long TIFFCurrentRow(void *tif)
{
  return ((tiff_t *)tif)->tif_row;
}

/**
 * Index of the IFD (image file directory) the handle is currently positioned
 * at.
 *
 * Transcribed from the vendored libtiff (tif_open.c TIFFCurrentDirectory),
 * whose body is literally `return tif->tif_curdir;`. Ghidra's cached listing
 * has 0x6d8b0 as an empty `void(void)` body -- the void-EAX artifact
 * (lift-learnings s16), since nothing in the cached listing consumes the
 * return, and kb.json carried the same wrong `void TIFFCurrentDirectory(void)`
 * prototype. The XBE bytes at 0x6d8b0-0x6d8bd are six instructions with one
 * stack argument and an EAX return: `push ebp / mov ebp,esp / mov eax,[ebp+8]
 * / mov eax,[eax+0xd8] / pop ebp / ret`. cdecl, no callee cleanup (plain
 * `ret`, not `ret n`), no register arguments, no locals (there is no `sub
 * esp`), so no `tiff_t *tif` temp is introduced here -- the cast happens
 * inside the return expression, matching TIFFCurrentRow immediately above and
 * the four accessors before it.
 *
 * The load is a plain dword MOV, not MOVZX word, so the field at 0xd8 is a
 * full 32-bit value even though upstream's `tdir_t` is 16-bit; see the field
 * comment on tif_curdir in the tiff_t layout.
 *
 * @param tif TIFF handle (declared void* so the generated header needs no
 *            libtiff types). Never null-checked, exactly as upstream.
 * @return the current directory index, in EAX.
 */
unsigned long TIFFCurrentDirectory(void *tif)
{
  return ((tiff_t *)tif)->tif_curdir;
}
