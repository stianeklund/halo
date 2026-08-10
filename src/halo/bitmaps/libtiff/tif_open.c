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
 * Only the four offsets below are touched by this function; everything else
 * is padding as far as the recovery is concerned. Offsets are proven by
 * 0x6c972 (+0x06 movzx word), 0x6c976 (+0x14 dword), 0x6c979 (+0x18 dword)
 * and 0x6ca25 (+0x2c dword). */
typedef struct tiff_bitstate_s {
  char pad_00[6];
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
typedef struct tiff_s {
  char pad_000[0x120];
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
 * signature; everything before +0x08 is unproven from this function. */
typedef struct tiff_predictor_state_s {
  char pad_00[8];
  unsigned short stride; /* 0x08 samples between a value and its predecessor */
  char pad_0a[2];
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
