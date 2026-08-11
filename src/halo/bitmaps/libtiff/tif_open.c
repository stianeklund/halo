/* ===========================================================================
 * tif_getimage.c -- upstream libtiff RGBA image reader.
 *
 * kb.json lumps every vendored libtiff translation unit into a single
 * tif_open.obj, so this body lives here alongside the tif_predict.c,
 * tif_lzw.c and tif_open.c neighbours (same arrangement as FUN_0006cac0).
 * Its own __FILE__ is proven by the string pushed at 0x6c55d / 0x6c576
 * (VA 0x260264): "c:\halo\SOURCE\bitmaps\libtiff\tif_getimage.c".
 * ======================================================================== */

#define TIFFTAG_IMAGEWIDTH 256 /* 0x100, pushed at 0x6c4f9 */
#define TIFFTAG_IMAGELENGTH 257 /* 0x101, pushed at 0x6c508 */
#define TIFFTAG_BITSPERSAMPLE 258 /* 0x102, pushed at 0x6c40e */
#define TIFFTAG_PHOTOMETRIC 262 /* 0x106, pushed at 0x6c478 */
#define TIFFTAG_SAMPLESPERPIXEL 277 /* 0x115, pushed at 0x6c444 */

#define PHOTOMETRIC_MINISBLACK 1 /* stored to 0x3340f4 in the 1-channel arm */
#define PHOTOMETRIC_RGB 2 /* stored to 0x3340f4 in the 3/4-channel arm */

/* File-static state of the original tif_getimage.c. This build of libtiff
 * predates the TIFFRGBAImage struct: the decoder state is a block of file
 * statics that TIFFReadRGBAImage fills in and the gtImage worker (0x6c080,
 * still unported) reads back, so these must alias the original addresses.
 *
 * The three tag values are 16 bit -- every read in the binary is
 * `movzx reg, word ptr [addr]` (0x6c41d, 0x6c451, 0x6c489) -- while
 * stoponerr is a dword store (0x6c4f0) and Map/BWmap are pointers.
 *
 * Which of the two freed pointers is upstream's `Map` (the 8-bit sample
 * lookup table) and which is `BWmap` (the bilevel row table) is INFERRED
 * from upstream's free order, not proven: the binary only shows 0x3340c8
 * freed at source line 125 and 0x3340c4 at line 127, in that order. */
#define bitspersample (*(unsigned short *)0x3340fc)
#define samplesperpixel (*(unsigned short *)0x3340f8)
#define photometric (*(unsigned short *)0x3340f4)
#define stoponerr (*(int *)0x3340e0)
#define Map (*(void **)0x3340c8)
#define BWmap (*(void **)0x3340c4)

/**
 * Read a whole TIFF image into a caller-supplied 32-bit RGBA raster.
 *
 * Transcribed from the vendored libtiff (tif_getimage.c TIFFReadRGBAImage)
 * rather than reshaped from the decompiler, which lost every parameter and
 * reported the body as `void(void)`. The real ABI is recovered from the
 * frame at 0x6c400: `push ebp / mov ebp,esp / sub esp,8`, five stack
 * arguments at [ebp+8]..[ebp+0x18], cdecl (all cleanup is caller-side), and
 * an EAX return -- `xor eax,eax` on all three error exits, `mov eax,esi` on
 * the success exit where ESI carries gtImage's result across the two frees.
 *
 * The bits-per-sample filter really is a jump table in the binary (byte index
 * at 0x6c5b4, targets at 0x6c5ac, guarded by `cmp ecx,0xf / ja`), so the
 * upstream switch is kept verbatim rather than folded into comparisons.
 *
 * The raster origin is bottom-adjusted before the worker runs:
 * 0x6c512-0x6c52f computes `raster + (rheight - height) * rwidth` in uint32
 * elements (`sub edx,eax / imul edx,rwidth / lea ..., [raster+edx*4]`), so a
 * short image lands at the bottom of a taller destination buffer.
 *
 * NOTE on the epilogue: the single `add esp,0x28` at 0x6c553 retires THREE
 * call frames at once (both 12-byte TIFFGetField frames plus gtImage's
 * 16-byte frame). It is not a ten-argument call.
 *
 * @param tif     TIFF handle (declared void* so decl.h needs no libtiff
 *                types); held in ESI for the whole body.
 * @param rwidth  destination raster pitch in pixels.
 * @param rheight destination raster height in pixels.
 * @param raster  destination, rwidth*rheight uint32 pixels of caller memory.
 * @param stop    non-zero to abort on the first decode error; published to
 *                the worker through the `stoponerr` file static.
 * @return non-zero on success, 0 if the image cannot be handled or decoded.
 */
int TIFFReadRGBAImage(void *tif, unsigned long rwidth, unsigned long rheight,
                      unsigned long *raster, int stop)
{
  int ok;
  unsigned long width, height;
  const char *photoname;

  FUN_00064ec0((int)tif, TIFFTAG_BITSPERSAMPLE, &bitspersample);
  switch (bitspersample) {
  case 1:
  case 2:
  case 4:
  case 8:
  case 16:
    break;
  default:
    FUN_00068a30(TIFFFileName(tif), "Sorry, can not handle %d-bit pictures",
                 bitspersample);
    return (0);
  }
  FUN_00064ec0((int)tif, TIFFTAG_SAMPLESPERPIXEL, &samplesperpixel);
  switch (samplesperpixel) {
  case 1:
  case 3:
  case 4:
    break;
  default:
    FUN_00068a30(TIFFFileName(tif), "Sorry, can not handle %d-channel images",
                 samplesperpixel);
    return (0);
  }
  if (!TIFFGetField((int)tif, TIFFTAG_PHOTOMETRIC, &photometric)) {
    switch (samplesperpixel) {
    case 1:
      photometric = PHOTOMETRIC_MINISBLACK;
      photoname = "min-is-black";
      break;
    case 3:
    case 4:
      photometric = PHOTOMETRIC_RGB;
      photoname = "RGB"; /* 0x260408, loaded straight into EAX at 0x6c4d1 */
      break;
    default:
      FUN_00068a30(TIFFFileName(tif),
                   "Missing needed \"PhotometricInterpretation\" tag");
      return (0);
    }
    /* Upstream selects the name with a
     * `photometric == PHOTOMETRIC_RGB ? "RGB" : "min-is-black"` ternary at
     * this point. The binary has no such compare -- each arm materialises its
     * own string pointer -- so the name is carried out of the switch instead.
     * Restoring the ternary costs a `cmpw $2, 0x3340f4` plus a branch that
     * the original does not have. */
    FUN_00068a30(TIFFFileName(tif),
                 "No \"PhotometricInterpretation\" tag, assuming %s\n",
                 photoname);
  }
  TIFFGetField((int)tif, TIFFTAG_IMAGEWIDTH, &width);
  TIFFGetField((int)tif, TIFFTAG_IMAGELENGTH, &height);
  stoponerr = stop;
  Map = 0;
  BWmap = 0;
  ok = FUN_0006c080(tif, rwidth, height, raster + (rheight - height) * rwidth);
  /* Line numbers are the original tif_getimage.c __LINE__ stamps (0x7d/0x7f
   * at 0x6c568 and 0x6c581); this file's own line numbers are meaningless
   * here, so they are written literally rather than via __LINE__. */
  if (Map)
    debug_free(Map, "c:\\halo\\SOURCE\\bitmaps\\libtiff\\tif_getimage.c", 125);
  if (BWmap)
    debug_free(BWmap, "c:\\halo\\SOURCE\\bitmaps\\libtiff\\tif_getimage.c",
               127);
  return (ok);
}

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
  /* Current strip index. TIFFCurrentStrip (0x6d8c0) reads it with a plain
   * dword `mov eax,[eax+0xdc]` -- no MOVSX/MOVZX, so this is a full 32-bit
   * field and no narrower spelling is admissible. Upstream libtiff types the
   * member `tstrip_t tif_curstrip`, i.e. a uint32, which agrees with the
   * observed width; the OFFSET is Bungie's (upstream places tif_curstrip far
   * earlier in TIFF), so only the name is transcribed. Signedness is
   * unobservable from a bare dword load; the field keeps upstream's unsigned
   * typing, matching tif_row and tif_curdir above. */
  unsigned long tif_curstrip; /* 0xdc */
  char pad_0e0[8];
  /* Current tile index. TIFFCurrentTile (0x6d8d0) reads it with a plain dword
   * `mov eax,[eax+0xe8]` -- no MOVSX/MOVZX, so this is a full 32-bit field and
   * no narrower spelling is admissible. Upstream libtiff types the member
   * `ttile_t tif_curtile`, i.e. a uint32, which agrees with the observed
   * width; the OFFSET is Bungie's, so only the name is transcribed.
   * Signedness is unobservable from a bare dword load; the field keeps
   * upstream's unsigned typing, matching tif_curstrip/tif_curdir/tif_row
   * above. */
  unsigned long tif_curtile; /* 0xe8 */
  char pad_0ec[4];
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
  /* Cached bytes per decoded scanline. NeXTDecode (0x6d340) loads it once
   * before its row loop (`mov esi,[ecx+0x124]`, 0x6d36e) and then uses it as
   * BOTH the `occ` decrement and the `row` advance (0x6d455/0x6d457), i.e. it
   * is a byte count and not a pixel count. A plain dword load with no
   * MOVSX/MOVZX, and the `cmp ebx,esi / jl` short-data test at 0x6d47f is
   * signed, so this is a signed 32-bit field. Upstream libtiff names the
   * member `tsize_t tif_scanlinesize`; the OFFSET is Bungie's. */
  long tif_scanlinesize; /* 0x124 */
  char pad_128[4];
  unsigned char *tif_rawcp; /* 0x12c */
  char pad_130[4];
  /* 0x134 -- raw-buffer READ cursor. PackBitsDecode (0x6dbf0) loads it at
   * 0x6dc02, walks it one byte at a time, and writes the advanced value back
   * at 0x6dcc8, paired with the byte count at 0x138 (loaded 0x6dbf9, stored
   * 0x6dcbd). That adjacency is upstream libtiff's tif_rawcp/tif_rawcc pair,
   * which would make the 0x12c pointer above `tif_rawdata` (the buffer base)
   * rather than a cursor -- consistent with the bit writer at 0x6c960 using
   * 0x12c as a base it adds `bitpos >> 3` to and stores the carried partial
   * byte through. That re-attribution of 0x12c is NOT made here; this field
   * keeps a mechanical name until a lift forces the question. */
  unsigned char *field_134;
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

/* Upstream libtiff tif_next.c spellings. The two run-type codes are the only
 * values `n` is compared against (0x6d395 `test eax,eax`, 0x6d39d
 * `cmp eax,0x40`); everything else falls into the run-length arm. */
#define LITERALROW 0x00
#define LITERALSPAN 0x40

/* Upstream's SETPIXEL, transcribed verbatim including its reliance on the
 * caller's `npixels`. The `switch (npixels++ & 3)` is what produces the
 * four-entry jump table at 0x6d4bc -- which lives in .rdata immediately past
 * the end of the function (bounds end 0x6d4ba) -- together with the
 * `cmp eax,3 / ja` range check at 0x6d3c6 that MSVC emits even though the
 * mask makes the index provably in range. Writing this as an if-chain loses
 * the table. Only case 3 advances `op`, so a run writes two bits per pixel
 * MSB-first into each byte. */
#define SETPIXEL(op, v)                  \
  {                                      \
    switch (npixels++ & 3) {             \
    case 0:                              \
      op[0] = (unsigned char)((v) << 6); \
      break;                             \
    case 1:                              \
      op[0] |= (v) << 4;                 \
      break;                             \
    case 2:                              \
      op[0] |= (v) << 2;                 \
      break;                             \
    case 3:                              \
      *op++ |= (v);                      \
      break;                             \
    }                                    \
  }

/**
 * Decode one or more 2-bit-grey NeXT-RLE scanlines into `buf`.
 *
 * Upstream libtiff's NeXTDecode (tif_next.c), transcribed rather than
 * reshaped from the decompiler -- the vendored-source posture this TU already
 * uses for horAcc8 and PackBitsDecode. Each row starts as a byte code: 0x00
 * means the whole scanline follows literally, 0x40 means a literal span at a
 * 16-bit offset, anything else is a run byte <grey:2><count:6> and the row is
 * decoded as a sequence of such runs until `td_imagewidth` pixels are
 * produced.
 *
 * The 0xff prefill (0x6d353-0x6d364) stays as upstream's
 * `for (op = buf, cc = occ; cc-- > 0;) *op++ = 0xff;` byte loop even though
 * the binary carries a `rep stosd`/`rep stosb` pair, for the same reason as
 * PackBitsDecode above: the `test ecx,ecx / jle 0x6d366` at 0x6d349 is the
 * loop's entry guard, and an inline memset expansion needs no guard (`rep`
 * with ECX=0 is already a no-op). The count is shifted LOGICALLY
 * (`shr ecx,2`) inside the expansion while the guard is signed, which is the
 * compiler's fill substitution, not something the C says. `memset` is also
 * not linkable in this build (`-nostdlib -ffreestanding -fno-builtin`).
 *
 * `row` has no stack slot of its own: the frame is `sub esp,0xc` and its
 * three dwords are the inner run counter ([EBP-0x4], 0x6d3bd), the cached
 * scanline size ([EBP-0x8], 0x6d380) and the cached image width ([EBP-0xc],
 * 0x6d3a8). MSVC coalesced `row` into the dead `buf` parameter slot -- hence
 * the `mov [ebp+0xc],edx` at 0x6d383 that seeds it and the `add edx,esi /
 * mov [ebp+0xc],edx` at 0x6d457 that advances it. The separate `row`
 * variable is upstream's; the coalescing is the compiler's.
 *
 * BYTE INDICES in the LITERALSPAN arm are read straight off the
 * disassembly, NOT off the decompiler: with EDI already past the `*bp++` at
 * 0x6d390, 0x6d409-0x6d414 is `movzx esi,[edi+2] / movzx ecx,[edi+3] /
 * shl esi,8 / add esi,ecx`, i.e. n = bp[2]*256 + bp[3], and 0x6d41d-0x6d431
 * is the same shape on [edi+0]/[edi+1] for the destination offset. The
 * decompiler's CONCAT11(pbVar6[3], pbVar6[4]) is shifted one byte by the
 * pre-increment and is wrong in both indices and width.
 *
 * `grey` is extracted with a SIGNED shift (`sar ecx,6`, 0x6d3b2), which is
 * upstream's `n` being a signed tsize_t rather than the unsigned byte it was
 * loaded from; an unsigned `n` would emit `shr`.
 *
 * @param tif_ TIFF handle (declared void* so the generated header needs no
 *             libtiff types). Never null-checked.
 * @param buf  output scanline buffer. Prefilled with 0xff (white under
 *             min-is-black) before any decoding.
 * @param occ  output bytes wanted, consumed `tif_scanlinesize` at a time.
 *             Signed (`test eax,eax / jle` at 0x6d36c, `jg` at 0x6d461).
 * @param s    sample number. Upstream's `(void) s;`: the frame slot at
 *             [EBP+0x14] is never read.
 * @return 1 once `occ` is exhausted, with the raw cursor/count written back
 *         (`mov eax,1` at 0x6d475); 0 after reporting a short scanline
 *         (`xor eax,eax` at 0x6d4b3), leaving them unwritten. The decompiler
 *         types the body void because it drops both EAX writes (void-EAX
 *         hazard, lift-learnings SS16); FUN_0006d4d0 storing this address
 *         into three `tiff_code_method_t` slots settles the shape.
 */
int FUN_0006d340(void *tif_, char *buf, int occ, int s)
{
  tiff_t *tif = (tiff_t *)tif_;
  unsigned char *bp;
  unsigned char *op;
  int cc;
  unsigned char *row;
  int scanline;
  int n;

  (void)s;
  /*
   * Each scanline is assumed to start off as all
   * white (we assume a PhotometricInterpretation
   * of ``min-is-black'').
   */
  for (op = (unsigned char *)buf, cc = occ; cc-- > 0;) {
    *op++ = 0xff;
  }

  bp = tif->field_134;
  cc = tif->tif_rawcc;
  scanline = tif->tif_scanlinesize;
  for (row = (unsigned char *)buf; occ > 0; occ -= scanline, row += scanline) {
    n = *bp++;
    cc--;
    switch (n) {
    case LITERALROW:
      /*
       * The entire scanline is given as literal values.
       */
      if (cc < scanline) {
        goto bad;
      }
      csmemcpy(row, bp, scanline);
      bp += scanline;
      cc -= scanline;
      break;
    case LITERALSPAN: {
      int off;
      /*
       * The scanline has a literal span that begins at some offset.
       */
      off = (bp[0] * 256) + bp[1];
      n = (bp[2] * 256) + bp[3];
      if (cc < 4 + n) {
        goto bad;
      }
      csmemcpy(row + off, bp + 4, n);
      bp += 4 + n;
      cc -= 4 + n;
      break;
    }
    default: {
      int npixels = 0;
      int grey;
      unsigned long imagewidth = tif->td_imagewidth;

      /*
       * The scanline is composed of a sequence of constant
       * color ``runs''.  We shift into ``run mode'' and
       * interpret bytes as codes of the form
       * <color><npixels> until we've filled the scanline.
       */
      op = row;
      for (;;) {
        grey = (int)((n >> 6) & 0x3);
        n &= 0x3f;
        while (n-- > 0) {
          SETPIXEL(op, grey);
        }
        if (npixels >= (int)imagewidth) {
          break;
        }
        if (cc == 0) {
          goto bad;
        }
        n = *bp++;
        cc--;
      }
      break;
    }
    }
  }
  tif->field_134 = bp;
  tif->tif_rawcc = cc;
  return 1;
bad:
  FUN_00068a30(tif->tif_name, "NeXTDecode: Not enough data for scanline %d",
               tif->tif_row);
  return 0;
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
 * FUN_0006d340 is only ever taken by address, never called. Its prototype is
 * now widened from its OWN body (four [EBP+8..0x14] argument slots, `mov
 * eax,1` / `xor eax,eax` on the two epilogues) rather than inferred from the
 * slot it lands in, so it matches `tiff_code_method_t` exactly and the
 * assignment casts that stood in for the old placeholder are gone.
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

  tif->tif_decoderow = FUN_0006d340;
  tif->tif_decodestrip = FUN_0006d340;
  tif->tif_decodetile = FUN_0006d340;
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
/* Not auto-inlinable: the reference at 0x6d992 CALLS this function rather
 * than expanding it, which is only possible if it lived in its own
 * translation unit (upstream libtiff has it in tif_strip.c). kb.json lumps
 * every vendored libtiff object into tif_open.obj, so MSVC 7.1 sees a small
 * same-TU callee and inlines it, costing 6 instructions at every call site.
 * The pragma restores the original call. */
#if defined(_MSC_VER) && !defined(__clang__)
#pragma auto_inline(off)
#endif
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
#if defined(_MSC_VER) && !defined(__clang__)
#pragma auto_inline(on)
#endif

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
/* Not auto-inlinable, same reason as TIFFScanlineSize below: every reference
 * in the binary CALLs this (0x6c4b5, 0x6c4e2, 0x6c592 in TIFFReadRGBAImage
 * above), which is only possible if it lived in its own translation unit --
 * upstream libtiff has it in tif_open.c and the caller in tif_getimage.c.
 * kb.json lumps both into tif_open.obj, so MSVC 7.1 sees a two-instruction
 * same-TU callee and expands it, costing the `push/call/add esp,4` triple at
 * every call site. The pragma restores the original call. */
#if defined(_MSC_VER) && !defined(__clang__)
#pragma auto_inline(off)
#endif
char *TIFFFileName(void *tif)
{
  return ((tiff_t *)tif)->tif_name;
}
#if defined(_MSC_VER) && !defined(__clang__)
#pragma auto_inline(on)
#endif

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

/**
 * Index of the strip the handle's decoder/encoder is currently positioned at.
 *
 * Transcribed from the vendored libtiff (tif_open.c TIFFCurrentStrip), whose
 * body is literally `return tif->tif_curstrip;`. Ghidra's cached listing has
 * 0x6d8c0 as an empty `void(void)` body -- the void-EAX artifact
 * (lift-learnings s16), since nothing in the cached listing consumes the
 * return, and kb.json carried the same wrong `void TIFFCurrentStrip(void)`
 * prototype. The XBE bytes at 0x6d8c0-0x6d8cd are six instructions with one
 * stack argument and an EAX return: `push ebp / mov ebp,esp / mov eax,[ebp+8]
 * / mov eax,[eax+0xdc] / pop ebp / ret`. cdecl, no callee cleanup (plain
 * `ret`, not `ret n`), no register arguments, no locals (there is no `sub
 * esp`), so no `tiff_t *tif` temp is introduced here -- the cast happens
 * inside the return expression, matching TIFFCurrentDirectory and
 * TIFFCurrentRow immediately above.
 *
 * The load is a plain dword MOV, not MOVSX/MOVZX word or byte, so the field at
 * 0xdc is a full 32-bit value and there is no width-narrowing hazard here.
 * Signedness is unobservable from a bare load; the field keeps upstream's
 * unsigned typing.
 *
 * @param tif TIFF handle (declared void* so the generated header needs no
 *            libtiff types). Never null-checked, exactly as upstream.
 * @return the current strip index, in EAX.
 */
unsigned long TIFFCurrentStrip(void *tif)
{
  return ((tiff_t *)tif)->tif_curstrip;
}

/**
 * Index of the tile the handle's decoder/encoder is currently positioned at.
 *
 * Transcribed from the vendored libtiff (tif_open.c TIFFCurrentTile), whose
 * body is literally `return tif->tif_curtile;`. Ghidra's cached listing has
 * 0x6d8d0 as an empty `void(void)` body -- the void-EAX artifact
 * (lift-learnings s16), since nothing in the cached listing consumes the
 * return, and kb.json carried the same wrong `void TIFFCurrentTile(void)`
 * prototype. The XBE bytes at 0x6d8d0-0x6d8dd are six instructions with one
 * stack argument and an EAX return: `push ebp / mov ebp,esp / mov eax,[ebp+8]
 * / mov eax,[eax+0xe8] / pop ebp / ret`. cdecl, no callee cleanup (plain
 * `ret`, not `ret n`), no register arguments, no locals (there is no `sub
 * esp`), so no `tiff_t *tif` temp is introduced here -- the cast happens
 * inside the return expression, matching TIFFCurrentStrip immediately above.
 *
 * The load is a plain dword MOV, not MOVSX/MOVZX word or byte, so the field at
 * 0xe8 is a full 32-bit value and there is no width-narrowing hazard here.
 * Offset 0xe8 previously sat inside pad_0e0[0x10]; a `pad_` field proven read
 * is a recovery bug, so the pad is split here without moving any neighbour
 * (0xe0 + 8 + 4 + 4 == 0xf0, the codec vtable base, unchanged).
 *
 * @param tif TIFF handle (declared void* so the generated header needs no
 *            libtiff types). Never null-checked, exactly as upstream.
 * @return the current tile index, in EAX.
 */
unsigned long TIFFCurrentTile(void *tif)
{
  return ((tiff_t *)tif)->tif_curtile;
}

/* MSVC CRT open() oflag bits, named from the immediates the binary actually
 * pushes/ORs at 0x6d910-0x6d962. _O_RDONLY is 0, so it never appears as an
 * operand -- the 'r' arm materialises it with `xor eax,eax`. Guarded because
 * the CRT's own fcntl.h may already be in scope under some toolchains. */
#ifndef _O_RDONLY
#define _O_RDONLY 0x0000
#endif
#ifndef _O_RDWR
#define _O_RDWR 0x0002
#endif
#ifndef _O_CREAT
#define _O_CREAT 0x0100
#endif
#ifndef _O_TRUNC
#define _O_TRUNC 0x0200
#endif
#ifndef _O_BINARY
#define _O_BINARY 0x8000
#endif

/* pmode passed as open()'s third argument: 0666, pushed literally as 0x1b6. */
#define TIFF_OPEN_PMODE 0x01b6

/**
 * Open a TIFF file by path and return a handle, or 0 on failure.
 *
 * Transcribed from the vendored libtiff (tif_open.c TIFFOpen) rather than
 * reshaped from the decompiler, keeping upstream's separate `_TIFFgetMode`
 * helper above -- which the binary's block layout proves was the real source
 * shape. Folding the helper's switch directly into this function instead
 * compiles to the same instructions in a different order and scores 82.6%;
 * the two-function form scores 100.0% (59/59). Two details give it away:
 *
 *  - `_TIFFgetMode` returns a sentinel `m = -1` that upstream's caller tests
 *    with `if (m == -1) return 0;`. After inlining, that test is statically
 *    true on the default arm and statically false everywhere else, so MSVC
 *    deletes it -- which is why no `cmp eax,-1` survives at 0x6d8e0 even
 *    though the sentinel is what makes the source well-formed.
 *  - The inlined arms are laid out around the deleted test: the 'r' arm at
 *    0x6d910 FALLS THROUGH into the shared open() block at 0x6d91f, while the
 *    'w'/'a' arm is exiled to 0x6d954 (past both error returns) and jumps
 *    back. Writing the switch inline instead makes both arms jump forward to
 *    the shared block, costing an extra `jmp` and reversing the placement.
 *
 * The rest of the body reads directly off the disassembly:
 *
 *  - Upstream's `switch (mode[0])` over 'r' / 'w' / 'a' comes out as an
 *    ascending compare chain at 0x6d8e7-0x6d8f6 (`cmp cl,0x61` 'a',
 *    `cmp cl,0x72` 'r', `cmp cl,0x77` 'w'), so the source case order is not
 *    recoverable from the branch order and upstream's r/w/a is kept.
 *  - The 'r' arm at 0x6d910 is `xor eax,eax / cmp cl,0x2b / jnz / mov eax,2`,
 *    i.e. upstream's `m = O_RDONLY; if (mode[1] == '+') m = O_RDWR;`.
 *  - 'w' and 'a' share ONE tail at 0x6d954 that re-tests mode[0]
 *    (`cmp cl,0x77 / mov eax,0x102 / jnz / mov eax,0x302`). That is upstream's
 *    fallthrough `case 'w': case 'a':` arm, with `m |= O_TRUNC` const-folded
 *    into the 0x302 immediate. Note the '+' suffix is NOT examined for 'w'/'a'
 *    in this libtiff revision.
 *  - All three arms converge on the single block at 0x6d91f, so `O_BINARY` is
 *    OR'd at the call site (`or eax,0x8000` immediately before the push) and
 *    the open() call is NOT duplicated per case.
 *
 * Ghidra's cached listing degrades this into `FID_conflict___open()` with an
 * `extraout_EAX`, purely because kb.json carried `void __open(void)` and
 * `void TIFFFdOpen(void)` -- the void-EAX artifact (lift-learnings s16). Both
 * results are consumed here: open()'s fd is the value tested, and TIFFFdOpen's
 * EAX is this function's return value, flowing straight through with no `mov`.
 * Both prototypes are corrected in kb.json alongside this port; no register
 * arguments are involved anywhere in this function.
 *
 * The failure test at 0x6d937 is `test eax,eax / jge`, i.e. a SIGNED `< 0`
 * check on the descriptor, not a zero check -- open() returns -1 on error.
 *
 * @param path file to open, forwarded to open() and to the error message.
 * @param mode libtiff mode string; only mode[0] (and mode[1] for 'r') is read.
 * @return the TIFF handle from TIFFFdOpen, or 0 if the mode is bad or the
 *         file cannot be opened.
 */
/* Upstream libtiff's mode-string decoder, kept as its own static function
 * because that is what the binary's block layout proves the source looked
 * like -- see the FUN_0006d8e0 comment below. It is inlined into its single
 * caller at /O2, so it contributes no call of its own. */
static int _TIFFgetMode(const char *mode, const char *module)
{
  int m = -1;

  switch (mode[0]) {
  case 'r':
    m = _O_RDONLY;
    if (mode[1] == '+') {
      m = _O_RDWR;
    }
    break;
  case 'w':
  case 'a':
    m = _O_RDWR | _O_CREAT;
    if (mode[0] == 'w') {
      m |= _O_TRUNC;
    }
    break;
  default:
    FUN_00068a30(module, "\"%s\": Bad mode", mode);
    break;
  }
  return m;
}

int FUN_0006d8e0(const char *path, const char *mode)
{
  static const char module[] = "TIFFOpen";
  int m;
  int fd;

  m = _TIFFgetMode(mode, module);
  if (m == -1) {
    return 0;
  }
  fd = __open(path, m | _O_BINARY, TIFF_OPEN_PMODE);
  if (fd < 0) {
    FUN_00068a30(module, "%s: Cannot open", path);
    return 0;
  }
  return TIFFFdOpen(fd, path, mode);
}

/**
 * Recompute the handle's cached row size after the directory changes.
 *
 * Ghidra's cached listing has 0x6d980 as `void(void)`: both the stack
 * parameter and the EAX return are the void-EAX / dropped-parameter artifact
 * (lift-learnings s16), so the disassembly at 0x6d980-0x6d9ba is the only
 * usable evidence. Eleven instructions per arm, cdecl, one stack argument
 * (`mov esi,[ebp+8]`), no locals -- there is no `sub esp`, so no `tiff_t *`
 * temp is introduced, matching TIFFIsTiled and TIFFGetMode above.
 *
 * The argument is pushed ONCE at 0x6d98c, before the `jns` at 0x6d98d, and is
 * shared by whichever CALL runs; each arm then does its own `add esp,4`. That
 * is a scheduling detail of the original, not two different argument lists --
 * both callees take the same single `tif` and return their result in EAX.
 *
 * The branch is a signed test of the byte at 0x0a (`mov al,[esi+0xa] / test
 * al,al / jns`), i.e. the same bit-7 flag TIFFIsTiled returns; the sign-set
 * (tiled) arm falls through to TIFFTileRowSize and the sign-clear arm jumps to
 * TIFFScanlineSize. Written as `< 0` rather than via TIFFIsTiled because the
 * original does not call it -- the flag is tested inline.
 *
 * 0x6f890 is upstream TIFFTileRowSize: its body at 0x6f890-0x6f8c0 matches
 * upstream libtiff instruction for instruction (null-check td_tilelength at
 * 0x2c and td_tilewidth at 0x28, `td_bitspersample * td_tilewidth`, `*=
 * td_samplesperpixel` when td_planarconfig is PLANARCONFIG_CONTIG, then
 * howmany8). It lives in tif_write.obj and keeps its mechanical name because
 * the XBE import library is keyed on it, so only its kb.json prototype is
 * corrected here; its body is not part of this port.
 *
 * UNRESOLVED: the destination offset 0x120 is written here with a byte count,
 * but the same offset is a freeable pointer everywhere else in this TU --
 * 0x6cac6 loads it, passes it to _TIFFfree and stores 0 back, and 0x6c96c
 * dereferences it as the bit-writer state. Both readings are disassembly, not
 * inference, so one of them is not `tif_data`. Upstream libtiff places
 * tif_scanlinesize immediately after tif_data, which would put it at 0x124,
 * not 0x120; nothing local decides between "Bungie reused the slot" and "the
 * surrounding field boundaries are off by one word". Until that is settled the
 * store stays an explicit raw-offset write rather than claiming a field name:
 * spelling it `tif_data` would assert a pointer here, and inventing
 * `tif_scanlinesize` would contradict the free at 0x6cac6. Codegen is
 * identical either way (`mov [esi+0x120],eax`).
 *
 * The strip arm must be a CALL, not an inlined copy of TIFFScanlineSize. Both
 * live in tif_open.c here only because kb.json lumps every vendored libtiff
 * object into tif_open.obj; upstream keeps TIFFScanlineSize in tif_strip.c, and
 * the reference's `call 0x6d820` proves the original saw it across a TU
 * boundary. Left to itself MSVC 7.1 expands it (28 candidate instructions
 * against 22 reference, 72.0%), so its definition is bracketed in
 * `#pragma auto_inline(off)` -- 95.5% with an exact 22/22 instruction count,
 * guarded to cl.exe only because clang rejects the pragma under -Werror and
 * its codegen is not what is scored,
 * and TIFFScanlineSize's own score is unchanged at 100.0%.
 *
 * The single residual instruction is the branch opcode: the reference selects
 * `jns`, our VC71 build `jge`. Both follow the identical `test al,al` and are
 * semantically the same edge here (the `test` clears OF), so this is MSVC's
 * sign-test-versus-signed-relational peephole, not a different condition. The
 * `< 0` spelling is what produces the matching `mov al` / `test al,al` pair in
 * the first place; a mask spelling reaches `jns` only by way of a `movsx`+`and`
 * that costs more than it recovers, the same trade documented on TIFFIsTiled
 * above.
 *
 * @param tif TIFF handle (declared void* so the generated header needs no
 *            libtiff types). Never null-checked, exactly as upstream.
 * @return always 1; both arms end in `mov eax,1`.
 */
int FUN_0006d980(void *tif)
{
  if (((tiff_t *)tif)->field_0a < 0) {
    *(int *)((char *)tif + 0x120) = FUN_0006f890(tif);
    return 1;
  }
  *(int *)((char *)tif + 0x120) = TIFFScanlineSize((int)tif);
  return 1;
}

/**
 * PackBits (RLE) scanline decoder -- upstream libtiff's `PackBitsDecode`
 * from tif_packbits.c, installed into tif_decoderow/decodestrip/decodetile
 * by FUN_0006dd50.
 *
 * This is the OLDER upstream shape, not the 3.5.x one. `cc` is decremented
 * exactly ONCE on the replicate path (`dec eax` at 0x6dc41, ahead of the -128
 * test) and never again for the replicated data byte, whereas 3.5.x carries a
 * second `cc--` alongside `b = *bp++`. Transcribing the newer upstream text
 * here would consume one raw byte too many per run and desynchronise every
 * following scanline. The literal path folds its two decrements into the
 * single `sub eax,esi` at 0x6dca2, using the already pre-incremented count.
 *
 * The `if (n >= 128) n -= 256;` guard is upstream's defence against compilers
 * that do not sign-extend `char`. It is dead on this target -- 0x6dc2b is
 * `movsx esi, byte ptr [ebx]` -- but the compiler cannot know that and emits
 * it anyway (`cmp esi,0x80 / jl / sub esi,0x100`, 0x6dc2f-0x6dc3a), so it is
 * kept rather than folded away.
 *
 * The replicate fill stays as upstream's `while (n-- > 0) *op++ = b;` byte
 * loop even though the binary carries a `rep stosd`/`rep stosb` pair, because
 * both halves of the loop survive around it: the `test edx,edx / jle` at
 * 0x6dc5d is the while's entry guard, and 0x6dc83 RELOADS `op` from its stack
 * slot and adds the count rather than reusing the EDI the rep-string pair
 * already left pointing there -- i.e. the fill was substituted for the loop
 * body while `op`'s live-out value was still recomputed from source. What
 * sits between (0x6dc64-0x6dc81: broadcast the byte through BL/BH into EAX,
 * `shr ecx,2 / rep stosd`, `and ecx,3 / rep stosb`, count shifted LOGICALLY)
 * is the compiler's fill expansion, not something the C says; writing an
 * explicit memset call here is not an option either, since this build is
 * `-nostdlib -ffreestanding -fno-builtin` and has no memset to link against.
 * EBX (`bp`) is spilled at 0x6dc5f and restored at 0x6dc77 because the byte
 * broadcast needs BL/BH.
 *
 * `op` and `occ` are the parameters themselves, mutated in place: the
 * original spills them back into their own argument slots ([EBP+0xc] at
 * 0x6dcad, [EBP+0x10] at 0x6dc56 and 0x6dca4) instead of keeping copies.
 *
 * @param tif_ TIFF handle (declared void* so the generated header needs no
 *             libtiff types). Never null-checked.
 * @param op   output scanline buffer, advanced past each decoded run.
 * @param occ  output bytes still wanted. Signed (`test edx,edx / jle` at
 *             0x6dc23), and deliberately allowed to go negative -- a run that
 *             overshoots ends the loop and lands in the error arm.
 * @param s    sample number. Upstream's `(void) s;`: the frame slot at
 *             [EBP+0x14] is never read.
 * @return 1 when `occ` was fully satisfied (`mov eax,1` at 0x6dcee), 0 after
 *         reporting the short scanline (`xor eax,eax` at 0x6dce8). The
 *         decompiler types the body void because it drops both EAX writes
 *         (void-EAX hazard, lift-learnings SS16); FUN_0006dd50 storing this
 *         address into three `tiff_code_method_t` slots settles the shape.
 */
int FUN_0006dbf0(void *tif_, char *op, int occ, int s)
{
  tiff_t *tif = (tiff_t *)tif_;
  char *bp;
  int cc;
  int n;
  int b;

  (void)s;
  bp = (char *)tif->field_134;
  cc = tif->tif_rawcc;
  while (cc > 0 && occ > 0) {
    n = (int)*bp++;
    /* Watch out for compilers that don't sign extend chars... */
    if (n >= 128) {
      n -= 256;
    }
    if (n < 0) { /* replicate next byte -n+1 times */
      cc--;
      if (n == -128) { /* nop */
        continue;
      }
      n = -n + 1;
      occ -= n;
      b = *bp++;
      while (n-- > 0) {
        *op++ = (char)b;
      }
    } else { /* copy next n+1 bytes literally */
      csmemcpy(op, bp, ++n);
      op += n;
      occ -= n;
      bp += n;
      cc -= n;
    }
  }
  tif->field_134 = (unsigned char *)bp;
  tif->tif_rawcc = cc;
  if (occ > 0) {
    FUN_00068a30(tif->tif_name,
                 "PackBitsDecode: Not enough data for scanline %d",
                 tif->tif_row);
    return 0;
  }
  return 1;
}

/**
 * Decode a whole strip/tile one row at a time.
 *
 * The row size is not recomputed here: it is the dword FUN_0006d980 caches at
 * `tif + 0x120` (tiled handles get the tile row size, strip handles the
 * scanline size), read ONCE at 0x6dd0d into callee-saved ESI and reused for
 * every iteration -- both for the row-decoder argument and for the two
 * advances. That hoist is the binary's, not ours; upstream libtiff recomputes
 * the length per call site instead.
 *
 * Bungie's copy calls the row decoder directly (0x6dd2a `call 0x6d9c0`)
 * rather than through a `tif_decoderow` slot, and reports failure with -1
 * (`or eax,-1` at 0x6dd48) instead of upstream's 0/`cc == 0` convention. The
 * success arm is `mov eax,1` at 0x6dd3e.
 *
 * The buffer advance (`add edi,esi` at 0x6dd38) is absent from the Ghidra
 * decompilation of this function; it is present in the disassembly and is
 * load-bearing -- without it every iteration decodes over the same bytes.
 *
 * @param tif TIFF handle (declared void* so the generated header needs no
 *            libtiff types). Never null-checked.
 * @param bp  strip/tile buffer, advanced one row per iteration.
 * @param cc  byte count remaining. Signed throughout (`test ebx,ebx / jle`
 *            guards entry and `jg` closes the loop), so a row size that
 *            overshoots the remainder ends the loop rather than wrapping.
 * @param s   sample number, reloaded from the frame each iteration and passed
 *            through to the row decoder untouched.
 * @return 1 when every row decoded (including the zero-length case, which
 *         skips the loop entirely), -1 as soon as one row decoder call
 *         returns negative.
 */
int FUN_0006dd00(void *tif, char *bp, int cc, int s)
{
  int rowsize = *(int *)((char *)tif + 0x120);

  while (cc > 0) {
    if (FUN_0006d9c0(tif, bp, rowsize, s) < 0) {
      return -1;
    }
    cc -= rowsize;
    bp += rowsize;
  }
  return 1;
}

/**
 * Install the PackBits codec into the TIFF handle's codec vtable.
 *
 * Writes exactly seven of the twelve code slots: the three decode entries
 * (0xfc/0x104/0x10c) all take FUN_0006dbf0, tif_setupencode (0xf4) takes
 * FUN_0006d980, tif_encoderow (0x100) takes FUN_0006d9c0, and the two chunk
 * encoders (0x108/0x110) take FUN_0006dd00. tif_setupdecode (0xf0),
 * tif_postencode (0xf8) and tif_cleanup (0x11c) are deliberately NOT touched
 * -- unlike the sibling installer FUN_0006d2d0, which writes ten slots.
 *
 * Note the encode side is not uniform: 0x100 gets FUN_0006d9c0 while
 * 0x108/0x110 get FUN_0006dd00 (the row encoder vs. the strip/tile chunk
 * encoder that loops over it). Collapsing all three onto one pointer, the way
 * FUN_0006d2d0 legitimately does, would be wrong here.
 *
 * Inferred (not proven by any string in this build): this is upstream
 * libtiff's TIFFInitPackBits, in a revision predating the tif_postencode
 * assignment -- FUN_0006dbf0 = PackBitsDecode, FUN_0006d980 =
 * PackBitsPreEncode, FUN_0006d9c0 = PackBitsEncode, FUN_0006dd00 =
 * PackBitsEncodeChunk. The store order below is upstream's (decode group,
 * setupencode, then encode group) and it reproduces the listing 1:1. The one
 * apparent discrepancy is scheduling only: MSVC CSEs 0x6dbf0 into ECX for the
 * three decode stores, then hoists the `mov ecx,0x6dd00` reload (0x6dd6d)
 * above the two single-use immediate stores (0x6dd72, 0x6dd7c) that source
 * order places before it. Do not reorder the source to chase that.
 *
 * The upstream `scheme` argument is absent: the frame reads only [EBP+8], and
 * the caller-cleanup RET takes no second slot.
 *
 * @param tif_ TIFF handle (declared void* so the generated header needs no
 *             libtiff types). Never null-checked.
 * @return always 1; `mov eax,0x1` at 0x6dd92, immediately before the epilogue.
 *         The decompiler types this void because nothing in the cached
 *         listing consumes EAX (void-EAX hazard, lift-learnings §16).
 */
int FUN_0006dd50(void *tif_)
{
  tiff_t *tif = (tiff_t *)tif_;

  tif->tif_decoderow = (tiff_code_method_t)FUN_0006dbf0;
  tif->tif_decodestrip = (tiff_code_method_t)FUN_0006dbf0;
  tif->tif_decodetile = (tiff_code_method_t)FUN_0006dbf0;
  tif->tif_setupencode = FUN_0006d980;
  tif->tif_encoderow = FUN_0006d9c0;
  tif->tif_encodestrip = FUN_0006dd00;
  tif->tif_encodetile = FUN_0006dd00;
  return 1;
}
