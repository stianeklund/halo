/* ===========================================================================
 * tif_dir.c -- vendored libtiff, TIFF directory tag accessors.
 *
 * kb.json groups FUN_00064cd0 with tif_dir.obj, so its body lives here. That
 * grouping is a house decision, not a proven one: the binary carries no
 * `__FILE__` string for this function (the libtiff source strings present are
 * tif_close/dir/dirread/dirwrite/fax3/getimage/lzw/open/read/write), and the
 * function has no assert to stamp one. By shape it is upstream libtiff 3.x
 * `TIFFVGetFieldDefaulted`, which upstream keeps in tif_aux.c rather than
 * tif_dir.c -- the identification itself is solid (see below), only its
 * translation unit is inferred.
 *
 * Identification evidence (0x64cd0-0x64e7e):
 *   - The body opens with `if (TIFFVGetField(tif, tag, ap)) return 1;` --
 *     three pushes EDI,EBX,ESI at 0x64cdf-0x64ce1, `CALL 0x65f00`,
 *     `ADD ESP,0xc`, `TEST EAX,EAX`, `JNZ` to the shared `MOV EAX,1`
 *     epilogue. Delegate-then-default is TIFFVGetFieldDefaulted's whole
 *     purpose; nothing else in libtiff has that prologue.
 *   - Every one of the 16 switch arms is `*va_arg(ap, T *) = td->td_<field>;`
 *     over the standard directory tags, and the tag->offset->width map below
 *     matches upstream field-for-field.
 *   - TIFFTAG_DATATYPE yields `td_sampleformat - 1` (the `DEC CX` at
 *     0x64e6f), which is upstream TIFFVGetFieldDefaulted verbatim and is
 *     unique to that function.
 * Transcribed from upstream and adapted to the observed widths and offsets
 * rather than reshaped from the decompiler, which lost all three parameters
 * and reported the body as `void(void)` with `extraout_EAX`.
 *
 * Bungie deviations from upstream libtiff 3.5.x:
 *   - Only 16 arms survive; upstream also defaults DOTRANGE, INKSET,
 *     NUMBEROFINKS, EXTRASAMPLES and MATTEING. Those tags fall through to
 *     the `return 0` default here (0x64e3e).
 *   - TIFFTAG_GROUP3OPTIONS (0x125) IS defaulted here, from a 32-bit field at
 *     +0x6c.
 *   - Several fields are narrower than upstream: SUBFILETYPE and IMAGEDEPTH
 *     are 16-bit loads AND 16-bit stores here (`MOV AX,word ptr [ESI+0x34]` /
 *     `MOV word ptr [EDX],AX` at 0x64d23-0x64d29 and 0x64e58-0x64e5e), where
 *     upstream uses `uint32` for both. TILEDEPTH and ROWSPERSTRIP stay 32-bit.
 * ======================================================================== */

#include <stdarg.h>

/* Tag numbers, in the upstream decimal spelling with the immediate the
 * dispatch actually compares against in the comment. */
#define TIFFTAG_SUBFILETYPE 254 /* 0xfe,   jump table slot at 0x64d21 */
#define TIFFTAG_BITSPERSAMPLE 258 /* 0x102,  0x64d34 */
#define TIFFTAG_THRESHHOLDING 263 /* 0x107,  0x64d47 */
#define TIFFTAG_FILLORDER 266 /* 0x10a,  0x64d5a */
#define TIFFTAG_ORIENTATION 274 /* 0x112,  0x64d6d */
#define TIFFTAG_SAMPLESPERPIXEL 277 /* 0x115,  0x64d80 */
#define TIFFTAG_ROWSPERSTRIP 278 /* 0x116,  0x64d93 */
#define TIFFTAG_MINSAMPLEVALUE 280 /* 0x118,  0x64da4 */
#define TIFFTAG_MAXSAMPLEVALUE 281 /* 0x119,  peeled by JZ at 0x64cfe */
#define TIFFTAG_PLANARCONFIG 284 /* 0x11c,  SUB EBX,0x11c at 0x64dd4 */
#define TIFFTAG_GROUP3OPTIONS 293 /* 0x125,  SUB EBX,0x9 at 0x64ddc */
#define TIFFTAG_RESOLUTIONUNIT 296 /* 0x128,  SUB EBX,0x3 at 0x64de1 */
#define TIFFTAG_PREDICTOR 317 /* 0x13d,  JZ at 0x64dd2 */
#define TIFFTAG_DATATYPE 32996 /* 0x80e4, SUB EBX,0x80e4 at 0x64e30 */
#define TIFFTAG_IMAGEDEPTH 32997 /* 0x80e5, DEC EBX at 0x64e38 */
#define TIFFTAG_TILEDEPTH 32998 /* 0x80e6, DEC EBX at 0x64e3b */

/* Partial view of the TIFF handle. The fuller recovery of this struct lives in
 * tif_open.c (`tiff_t`); this TU repeats only the offsets it touches, at the
 * same offsets and widths, because the struct is still a file-scope typedef in
 * each libtiff TU rather than a shared tiffiop.h.
 *
 * Upstream reads these through `TIFFDirectory *td = &tif->tif_dir;`. Here the
 * directory fields are addressed straight off the handle pointer -- no arm
 * dereferences arg1, every one is `MOV <reg>,[ESI+off]` -- so the directory is
 * modelled inline at its absolute handle offsets and the `td` indirection is
 * dropped. The `pad_` runs are not a claim that those bytes are unused, only
 * that THIS function never reads them.
 *
 * Offsets and widths proven from the disassembly of FUN_00064cd0 (the width of
 * each store matches the width of its load in every arm):
 *   +0x24  `MOV AX,word ptr [ESI+0x24]`     (0x64e58)  -- 16-bit
 *   +0x30  `MOV ECX,dword ptr [ESI+0x30]`   (0x64e47)  -- 32-bit
 *   +0x34  `MOV DX,word ptr [ESI+0x34]`     (0x64d23)
 *   +0x36  `MOV CX,word ptr [ESI+0x36]`     (0x64d36)
 *   +0x38  `MOV CX,word ptr [ESI+0x38]`     (0x64e69)
 *   +0x3e  `MOV AX,word ptr [ESI+0x3e]`     (0x64d49)
 *   +0x40  `MOV DX,word ptr [ESI+0x40]`     (0x64d5c)
 *   +0x42  `MOV CX,word ptr [ESI+0x42]`     (0x64d6f)
 *   +0x44  `MOV AX,word ptr [ESI+0x44]`     (0x64d82)
 *   +0x46  `MOV DX,word ptr [ESI+0x46]`     (0x64e1f)
 *   +0x48  `MOV EDX,dword ptr [ESI+0x48]`   (0x64d95)  -- 32-bit
 *   +0x4c  `MOV CX,word ptr [ESI+0x4c]`     (0x64da6)
 *   +0x50  `MOV AX,word ptr [ESI+0x50]`     (0x64db9)
 *   +0x5c  `MOV DX,word ptr [ESI+0x5c]`     (0x64de8)
 *   +0x5e  `MOV AX,word ptr [ESI+0x5e]`     (0x64e0c)
 *   +0x6c  `MOV ECX,dword ptr [ESI+0x6c]`   (0x64dfb)  -- 32-bit
 * The field names come from the tag each offset answers, which pins all 16
 * unambiguously.
 */
typedef struct tiff_s {
  unsigned char pad_00[0x24]; /* 0x00 */
  unsigned short td_imagedepth; /* 0x24 */
  unsigned char pad_26[0x0a]; /* 0x26 */
  unsigned int td_tiledepth; /* 0x30 */
  unsigned short td_subfiletype; /* 0x34 */
  unsigned short td_bitspersample; /* 0x36 */
  unsigned short td_sampleformat; /* 0x38 */
  unsigned char pad_3a[0x04]; /* 0x3a */
  unsigned short td_threshholding; /* 0x3e */
  unsigned short td_fillorder; /* 0x40 */
  unsigned short td_orientation; /* 0x42 */
  unsigned short td_samplesperpixel; /* 0x44 */
  unsigned short td_predictor; /* 0x46 */
  unsigned int td_rowsperstrip; /* 0x48 */
  unsigned short td_minsamplevalue; /* 0x4c */
  unsigned char pad_4e[0x02]; /* 0x4e */
  unsigned short td_maxsamplevalue; /* 0x50 */
  unsigned char pad_52[0x0a]; /* 0x52 */
  unsigned short td_resolutionunit; /* 0x5c */
  unsigned short td_planarconfig; /* 0x5e */
  unsigned char pad_60[0x0c]; /* 0x60 */
  unsigned int td_group3options; /* 0x6c */
} tiff_t;

/* TIFFVGetFieldDefaulted (0x64cd0). `ap` is a va_list; the kb.json prototype
 * spells it `char *` because the generated decl.h has no <stdarg.h> in scope,
 * and MSVC 7.1 / clang-i386 both define va_list as exactly `char *`, so the
 * two spellings are the same type. `ap` is walked with va_arg even though the
 * original shows no pointer bump: MSVC kept it in EDI and dropped the dead
 * increment, which CL reproduces from this source. */
int TIFFVGetFieldDefaulted(void *tif_, unsigned int tag, va_list ap)
{
  tiff_t *tif = (tiff_t *)tif_;

  /* 0x64cdf-0x64cec. Straight passthrough of all three arguments. */
  if (TIFFVGetField(tif_, tag, ap))
    return 1;

  switch (tag) {
  case TIFFTAG_SUBFILETYPE:
    *va_arg(ap, unsigned short *) = tif->td_subfiletype;
    return 1;
  case TIFFTAG_BITSPERSAMPLE:
    *va_arg(ap, unsigned short *) = tif->td_bitspersample;
    return 1;
  case TIFFTAG_THRESHHOLDING:
    *va_arg(ap, unsigned short *) = tif->td_threshholding;
    return 1;
  case TIFFTAG_FILLORDER:
    *va_arg(ap, unsigned short *) = tif->td_fillorder;
    return 1;
  case TIFFTAG_ORIENTATION:
    *va_arg(ap, unsigned short *) = tif->td_orientation;
    return 1;
  case TIFFTAG_SAMPLESPERPIXEL:
    *va_arg(ap, unsigned short *) = tif->td_samplesperpixel;
    return 1;
  case TIFFTAG_ROWSPERSTRIP:
    *va_arg(ap, unsigned int *) = tif->td_rowsperstrip;
    return 1;
  case TIFFTAG_MINSAMPLEVALUE:
    *va_arg(ap, unsigned short *) = tif->td_minsamplevalue;
    return 1;
  case TIFFTAG_MAXSAMPLEVALUE:
    *va_arg(ap, unsigned short *) = tif->td_maxsamplevalue;
    return 1;
  case TIFFTAG_PLANARCONFIG:
    *va_arg(ap, unsigned short *) = tif->td_planarconfig;
    return 1;
  case TIFFTAG_GROUP3OPTIONS:
    *va_arg(ap, unsigned int *) = tif->td_group3options;
    return 1;
  case TIFFTAG_RESOLUTIONUNIT:
    *va_arg(ap, unsigned short *) = tif->td_resolutionunit;
    return 1;
  case TIFFTAG_PREDICTOR:
    *va_arg(ap, unsigned short *) = tif->td_predictor;
    return 1;
  /* 0x64e69. The `- 1` is the DEC CX between the load and the store, and it
   * stays 16-bit -- upstream `*va_arg(ap, uint16*) = td->td_sampleformat-1`. */
  case TIFFTAG_DATATYPE:
    *va_arg(ap, unsigned short *) = (unsigned short)(tif->td_sampleformat - 1);
    return 1;
  case TIFFTAG_IMAGEDEPTH:
    *va_arg(ap, unsigned short *) = tif->td_imagedepth;
    return 1;
  case TIFFTAG_TILEDEPTH:
    *va_arg(ap, unsigned int *) = tif->td_tiledepth;
    return 1;
  }

  /* 0x64e3e. */
  return 0;
}
