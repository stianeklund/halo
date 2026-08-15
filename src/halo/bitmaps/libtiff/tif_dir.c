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
 *   - TIFFTAG_GROUP4OPTIONS (0x125) IS defaulted here, from a 32-bit field at
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
/* 293 is upstream's GROUP4OPTIONS/T6OPTIONS, not GROUP3OPTIONS (292). The
 * offset arbitrates: _TIFFVSetField's jump table sends 0x124 to the 32-bit
 * field at +0x68 and 0x125 to the one at +0x6c, in that order, so +0x6c is
 * td_group4options and 0x125 is the tag that reaches it. The getter below was
 * spelling the same tag GROUP3OPTIONS; only the name was wrong. */
#define TIFFTAG_GROUP4OPTIONS 293 /* 0x125,  SUB EBX,0x9 at 0x64ddc */
#define TIFFTAG_RESOLUTIONUNIT 296 /* 0x128,  SUB EBX,0x3 at 0x64de1 */
#define TIFFTAG_PREDICTOR 317 /* 0x13d,  JZ at 0x64dd2 */
#define TIFFTAG_DATATYPE 32996 /* 0x80e4, SUB EBX,0x80e4 at 0x64e30 */
#define TIFFTAG_IMAGEDEPTH 32997 /* 0x80e5, DEC EBX at 0x64e38 */
#define TIFFTAG_TILEDEPTH 32998 /* 0x80e6, DEC EBX at 0x64e3b */

/* Tags only _TIFFVSetField (0x652f0) handles. Every value below is proven by
 * the byte index table at 0x65964 (0x56 entries, biased by 0xfe) feeding the
 * jump table at 0x658bc, decoded entry by entry against the handler bodies. */
#define TIFFTAG_IMAGEWIDTH 256 /* 0x100, jump table slot 1 -> 0x65344 */
#define TIFFTAG_IMAGELENGTH 257 /* 0x101, slot 2 -> 0x65354 */
#define TIFFTAG_COMPRESSION 259 /* 0x103, slot 4 -> 0x65376 */
#define TIFFTAG_PHOTOMETRIC 262 /* 0x106, slot 5 -> 0x653cc */
#define TIFFTAG_DOCUMENTNAME 269 /* 0x10d, slot 8 -> 0x6540f */
#define TIFFTAG_IMAGEDESCRIPTION 270 /* 0x10e, slot 9 -> 0x6546f */
#define TIFFTAG_MAKE 271 /* 0x10f, slot 10 -> 0x65487 */
#define TIFFTAG_MODEL 272 /* 0x110, slot 11 -> 0x6549f */
#define TIFFTAG_XRESOLUTION 282 /* 0x11a, slot 17 -> 0x655a5 */
#define TIFFTAG_YRESOLUTION 283 /* 0x11b, slot 18 -> 0x655b5 */
#define TIFFTAG_PAGENAME 285 /* 0x11d, slot 20 -> 0x655e4 */
#define TIFFTAG_XPOSITION 286 /* 0x11e, slot 21 -> 0x655fc */
#define TIFFTAG_YPOSITION 287 /* 0x11f, slot 22 -> 0x6560c */
#define TIFFTAG_GROUP3OPTIONS 292 /* 0x124, slot 23 -> 0x6561c (+0x68) */
#define TIFFTAG_PAGENUMBER 297 /* 0x129, slot 26 -> 0x6565f */
#define TIFFTAG_SOFTWARE 305 /* 0x131, slot 27 -> 0x654b7 */
#define TIFFTAG_DATETIME 306 /* 0x132, slot 28 -> 0x6543f */
#define TIFFTAG_ARTIST 315 /* 0x13b, slot 29 -> 0x65427 */
#define TIFFTAG_HOSTCOMPUTER 316 /* 0x13c, slot 30 -> 0x65457 */
#define TIFFTAG_COLORMAP 320 /* 0x140, slot 32 -> 0x6569d */
#define TIFFTAG_HALFTONEHINTS 321 /* 0x141, slot 33 -> 0x6567b */
#define TIFFTAG_TILEWIDTH 322 /* 0x142, slot 34 -> 0x65771 */
#define TIFFTAG_TILELENGTH 323 /* 0x143, slot 35 -> 0x65799 */
#define TIFFTAG_BADFAXLINES 326 /* 0x146, slot 36 -> 0x6573d */
#define TIFFTAG_CLEANFAXDATA 327 /* 0x147, slot 37 -> 0x6574d */
#define TIFFTAG_CONSECUTIVEBADFAXLINES 328 /* 0x148, slot 38 -> 0x6575f */
#define TIFFTAG_EXTRASAMPLES 338 /* 0x152, slot 39 -> 0x65709 */
#define TIFFTAG_SAMPLEFORMAT 339 /* 0x153, slot 40 -> 0x6582f */
#define TIFFTAG_MATTEING 32995 /* 0x80e3, peeled by the JZ at 0x6530f */

/* Tag values the setter range-checks. Each pair is the two immediates the
 * corresponding CMP instructions carry, in the order they are compared. */
#define FILLORDER_MSB2LSB 1 /* CMP EDI,0x1 at 0x653fa */
#define FILLORDER_LSB2MSB 2 /* CMP EDI,0x2 at 0x653f5 (compared first) */
#define ORIENTATION_TOPLEFT 1 /* CMP EDI,0x1 at 0x654d4 */
#define ORIENTATION_LEFTBOT 8 /* CMP EDI,0x8 at 0x654d9 */
#define PLANARCONFIG_CONTIG 1 /* CMP EDI,0x1 at 0x655ca (compared first) */
#define PLANARCONFIG_SEPARATE 2 /* CMP EDI,0x2 at 0x655cf */
#define RESUNIT_NONE 1 /* CMP EDI,0x1 at 0x65641 */
#define RESUNIT_CENTIMETER 3 /* CMP EDI,0x3 at 0x6564a */
#define SAMPLEFORMAT_UINT 1 /* CMP EDI,0x1 at 0x65847 */
#define SAMPLEFORMAT_VOID                             \
  4 /* MOV EDI,0x4 at 0x65840, CMP EDI,0x4 at 0x6584c \
     */
#define EXTRASAMPLE_ASSOCALPHA 1 /* CMP EDI,0x1 at 0x6572b */

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
typedef void (*tiff_void_method_t)(void *tif);

typedef struct tiff_s {
  /* Handle-level members. TIFFFileName (0x6d850) returns the dword at 0x00, so
   * that offset is upstream's `char* tif_name` on `struct tiff` and not a
   * directory member; every TIFFError/TIFFWarning call in _TIFFVSetField passes
   * `MOV ECX,dword ptr [tif]` as the module argument, which agrees. */
  char *tif_name; /* 0x00 */
  unsigned char pad_04[0x06]; /* 0x04 */
  /* Flag byte. _TIFFVSetField only ever ORs into it: 0x80 for the two tile
   * dimension tags (`OR byte ptr [EAX+0xa],0x80` at 0x65790/0x657b9) and 0x02
   * once the field bit is set (0x65886). Bit 7 is the same bit TIFFIsTiled
   * (0x6d880) reads, so 0x80 is this build's tiled flag; 0x02 is its
   * dirty-directory flag. Upstream keeps both in a `uint32 tif_flags` with
   * different values (TIFF_ISTILED == 0x400) -- do NOT import upstream's
   * numbering, and keep the field mechanical rather than named tif_flags. */
  char field_0a; /* 0x0a */
  unsigned char pad_0b[0x09]; /* 0x0b */
  /* "Which tags are present" bit array, indexed as dwords off 0x14: the field
   * bit set at the tail of _TIFFVSetField is
   * `LEA EDI,[EBX + ECX*0x4 + 0x14]` with ECX = field_bit >> 5 (0x65869). The
   * bit numbering is this build's, not upstream's: bit 7 is COMPRESSION
   * (0x6537e reads the low byte and branches on its sign) and bit 1 is the tile
   * dimensions (`TEST byte ptr [EAX+0x14],0x2` at 0x65563). */
  unsigned long td_fieldsset[2]; /* 0x14 */
  long td_imagewidth; /* 0x1c */
  long td_imagelength; /* 0x20 */
  /* 32-bit here even though the getter above reads it with a word load: the
   * setter stores a full dword (`MOV dword ptr [ECX+0x24],EAX` at 0x6582a), and
   * a 16-bit field would leave 0x26 clobbered. The getter's narrow load is the
   * compiler folding a 32-bit load into the 16-bit store it feeds. */
  long td_imagedepth; /* 0x24 */
  long td_tilewidth; /* 0x28 */
  long td_tilelength; /* 0x2c */
  long td_tiledepth; /* 0x30 */
  unsigned short td_subfiletype; /* 0x34 */
  unsigned short td_bitspersample; /* 0x36 */
  unsigned short td_sampleformat; /* 0x38 */
  unsigned short td_compression; /* 0x3a */
  unsigned short td_photometric; /* 0x3c */
  unsigned short td_threshholding; /* 0x3e */
  unsigned short td_fillorder; /* 0x40 */
  unsigned short td_orientation; /* 0x42 */
  unsigned short td_samplesperpixel; /* 0x44 */
  unsigned short td_predictor; /* 0x46 */
  unsigned long td_rowsperstrip; /* 0x48 */
  /* Both 32-bit, for the same reason as td_imagedepth: the setter masks a dword
   * to 16 bits and stores the whole dword (`AND EAX,0xffff` then
   * `MOV dword ptr [ECX+0x4c],EAX` at 0x65583, and the same at 0x65598). */
  unsigned long td_minsamplevalue; /* 0x4c */
  unsigned long td_maxsamplevalue; /* 0x50 */
  float td_xresolution; /* 0x54 */
  float td_yresolution; /* 0x58 */
  unsigned short td_resolutionunit; /* 0x5c */
  unsigned short td_planarconfig; /* 0x5e */
  float td_xposition; /* 0x60 */
  float td_yposition; /* 0x64 */
  unsigned long td_group3options; /* 0x68 */
  unsigned long td_group4options; /* 0x6c */
  unsigned short td_pagenumber[2]; /* 0x70 */
  unsigned short td_matteing; /* 0x74 */
  unsigned short td_cleanfaxdata; /* 0x76 */
  unsigned short td_consecutivebadfaxlines; /* 0x78 */
  unsigned char pad_7a[0x02]; /* 0x7a */
  unsigned long td_badfaxlines; /* 0x7c */
  unsigned short *td_colormap[3]; /* 0x80 */
  unsigned short td_halftonehints[2]; /* 0x8c */
  /* String slots, in the order the nine setString calls address them
   * (0x65415 .. 0x655ec). The NAMES come from tif_open.c's recovery of the same
   * struct, where TIFFPrintDirectory labels each offset; the order the setter's
   * cases visit them is upstream _TIFFVSetField's case order exactly, which
   * cross-checks all nine. */
  char *td_documentname; /* 0x90 */
  char *td_artist; /* 0x94 */
  char *td_datetime; /* 0x98 */
  char *td_hostcomputer; /* 0x9c */
  char *td_imagedescription; /* 0xa0 */
  char *td_make; /* 0xa4 */
  char *td_model; /* 0xa8 */
  char *td_software; /* 0xac */
  char *td_pagename; /* 0xb0 */
  unsigned char pad_b4[0x68]; /* 0xb4 */
  /* Codec teardown hook, called before a new compression scheme replaces the
   * current one (`MOV EAX,dword ptr [EAX+0x11c]` / `CALL EAX` at
   * 0x65399-0x653a6, one pushed argument). tif_open.c proves the identity: the
   * slot is loaded with LZWCleanup (0x6cac0) by the codec installer. */
  tiff_void_method_t tif_cleanup; /* 0x11c */
} tiff_t;

/* One row of the tag descriptor table. _TIFFVSetField only ever reads two
 * members of what FUN_00066380 returns: a 16-bit bit index at +0x0c
 * (`MOVZX ECX,word ptr [EAX+0xc]` at 0x6585e, and the same offset as a byte at
 * 0x65874 for the shift count) and a string at +0x10 that every diagnostic
 * prints through "%s" (0x654f0, 0x657e9, 0x6589d). Upstream libtiff names those
 * TIFFFieldInfo::field_bit and ::field_name. Nothing here observes the rest of
 * the row, so its size is unknown and the struct stops at the last read. */
typedef struct tiff_field_info_s {
  unsigned char pad_00[0x0c]; /* 0x00 */
  unsigned short field_bit; /* 0x0c */
  unsigned char pad_0e[0x02]; /* 0x0e */
  char *field_name; /* 0x10 */
} tiff_field_info_t;

/* Bit indices into td_fieldsset. Only the two the setter tests are known. */
#define FIELD_TILEDIMENSIONS 1 /* TEST byte ptr [EAX+0x14],0x2 at 0x65563 */
#define FIELD_COMPRESSION 7 /* sign of byte ptr [EBX+0x14] at 0x6537e */

/* field_0a bits, in this build's numbering (see the field comment above). */
#define TIFF_ISTILED 0x80
#define TIFF_DIRTYDIRECT 0x02

/* Upstream libtiff's accessors, verbatim. TIFFSetFieldBit must stay a macro
 * that expands `field` twice: the binary calls FUN_00066380 TWICE for the same
 * tag at the tail (0x65859 and 0x6586d, sharing one ADD ESP,0x8 at 0x6587c),
 * once for the word index and once for the shift count. Folding it to a single
 * call is a shape regression, not a cleanup. */
/* The setter's 16-bit arms fetch their vararg with a WORD load out of the
 * 4-byte stack slot (`MOV DX,word ptr [ECX]`), which is what upstream's
 * `va_arg(ap, uint16)` compiles to under MSVC. clang rejects a promotable type
 * in va_arg (-Werror,-Wvarargs), so only the clang path is rewritten to read
 * the promoted slot and narrow; on i386 both forms read the same value and
 * advance `ap` by the same 4 bytes. CL still sees the upstream spelling, so
 * VC71 codegen is unaffected. */
#if defined(__clang__)
#define TIFF_VA_ARG_UINT16(ap) ((unsigned short)va_arg(ap, unsigned int))
#else
#define TIFF_VA_ARG_UINT16(ap) va_arg(ap, unsigned short)
#endif

/* The shift count is masked, not reduced: 0x6587f is `AND ECX,0x1f` on the low
 * byte of field_bit, which is BITn's `& 0x1f`. Spelling the same thing as
 * `field % 32` instead compiles to `AND ECX,0x8000001f` plus the signed-
 * remainder correction, because the promoted unsigned short is a signed int. */
#define BITn(n) (((unsigned long)1L) << ((n) & 0x1f))
#define TIFFFieldSet(tif, field) \
  ((tif)->td_fieldsset[(field) / 32] & BITn(field))
#define TIFFSetFieldBit(tif, field) \
  ((tif)->td_fieldsset[(field) / 32] |= BITn(field))

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
  case TIFFTAG_GROUP4OPTIONS:
    *va_arg(ap, unsigned int *) = tif->td_group4options;
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

/* TIFFGetFieldDefaulted (0x64ec0) -- upstream libtiff tif_aux.c, transcribed
 * verbatim rather than reshaped from the decompiler (Ghidra dropped the
 * signature entirely and reported `void FUN_00064ec0(void)`).
 *
 * The whole body is 13 instructions with no `sub esp`:
 *   0x64ec3 MOV ECX,[EBP+0xc]   ; tag
 *   0x64ec6 MOV EDX,[EBP+0x8]   ; tif
 *   0x64ec9 LEA EAX,[EBP+0x10]  ; &first vararg == va_start(ap, tag)
 *   0x64ecc PUSH EAX / PUSH ECX / PUSH EDX
 *   0x64ecf CALL 0x64cd0 / ADD ESP,0xc / POP EBP / RET
 * The LEA is what proves the ABI is variadic: the third pushed dword is the
 * ADDRESS of the first vararg slot, not the third parameter's value. kb.json
 * previously declared this as a fixed `int FUN_00064ec0(int, int, void *)`,
 * which would have passed the caller's out-pointer by value and left the
 * callee one level of indirection short. EAX is untouched between the CALL
 * and the RET, so the return is a straight passthrough. ECX/EDX are only
 * scheduling scratch for the pushes -- this is not a register-arg function. */
int TIFFGetFieldDefaulted(void *tif, unsigned int tag, ...)
{
  int ok;
  va_list ap;

  va_start(ap, tag);
  ok = TIFFVGetFieldDefaulted(tif, tag, ap);
  va_end(ap);
  return (ok);
}

/* ---------------------------------------------------------------------------
 * _TIFFVSetField (0x652f0) -- upstream libtiff tif_dir.c.
 *
 * Identification: the tail is `TIFFSetFieldBit(tif, _TIFFFieldWithTag(tif,
 * tag)->field_bit); tif->tif_flags |= TIFF_DIRTYDIRECT;` and the body is one
 * switch over the writable directory tags, which is _TIFFVSetField and nothing
 * else. Individual arms are upstream verbatim: COMPRESSION's
 * "notify the previous module" cleanup hook plus `else goto end`, ROWSPERSTRIP
 * deriving tile dimensions when FIELD_TILEDIMENSIONS is unset, COLORMAP's
 * `v32 = 1L << td_bitspersample` feeding three _TIFFsetShortArray calls, and
 * DATATYPE falling into SAMPLEFORMAT through a `tag == TIFFTAG_DATATYPE` test.
 *
 * Ghidra reports this as `void FUN_000652f0(void)` because kb.json declared it
 * that way; the three parameters are at [EBP+8], [EBP+0xc] and [EBP+0x10] and
 * the return value is the `status` local at [EBP-4], loaded into EAX by all
 * three epilogues (0x65810, 0x65893, 0x658ba). `tag` is signed: the dispatch
 * opens `CMP ESI,0x80e3 / JG` (0x65309), a SIGNED compare, which an unsigned
 * switch expression could not produce.
 *
 * The case set is exact, not upstream's: the byte index table at 0x65964 was
 * decoded entry by entry (0x56 entries biased by 0xfe) against the jump table
 * at 0x658bc, and the four 0x80e3-0x80e6 tags come from the pre-switch compare
 * chain. Cases appear below in the order their handler blocks appear in the
 * binary, which is also upstream's case order.
 *
 * Bungie deviations from upstream libtiff 3.5.x:
 *   - SAMPLESPERPIXEL rejects more than four channels with its own
 *     "Cannot handle %ld-channel data" error and returns 0.
 *   - EXTRASAMPLES is not upstream's setExtraSamples loop: it accepts exactly
 *     one extra sample of type EXTRASAMPLE_ASSOCALPHA (0x65709-0x65738) and
 *     stores no sampleinfo array.
 *   - TILEWIDTH/TILELENGTH require a multiple of 8, not upstream's 16, and have
 *     no O_RDONLY warning path -- a bad value goes straight to badvalue.
 *   - MINSAMPLEVALUE/MAXSAMPLEVALUE mask a 32-bit vararg instead of reading a
 *     16-bit one.
 *   - Absent entirely: SUBIFD, INKSET, INKNAMES, DOTRANGE, TARGETPRINTER,
 *     YCBCR*, REFERENCEBLACKWHITE, STRIP/TILEOFFSETS. They reach the
 *     "Internal error, tag value botch" default.
 * ------------------------------------------------------------------------- */

/* 0x652f0 */
int _TIFFVSetField(void *tif_, int tag, va_list ap)
{
  tiff_t *tif = (tiff_t *)tif_;
  int status = 1;
  unsigned long v32;
  int v;

  switch (tag) {
  /* 0x65332. Upstream passes a uint32 here; the field is 16-bit in this build,
   * so the load is a word (`MOV DX,word ptr [ECX]`). Every 16-bit arm below has
   * the same shape. */
  case TIFFTAG_SUBFILETYPE:
    tif->td_subfiletype = TIFF_VA_ARG_UINT16(ap);
    break;
  /* 0x65344 */
  case TIFFTAG_IMAGEWIDTH:
    tif->td_imagewidth = va_arg(ap, unsigned long);
    break;
  /* 0x65354 */
  case TIFFTAG_IMAGELENGTH:
    tif->td_imagelength = va_arg(ap, unsigned long);
    break;
  /* 0x65364 */
  case TIFFTAG_BITSPERSAMPLE:
    tif->td_bitspersample = TIFF_VA_ARG_UINT16(ap);
    break;
  /* 0x65376. The masked vararg is 0x6537c-0x65381; the field-set test is the
   * signed byte branch at 0x6537e-0x65389; `break` on an unchanged scheme is
   * the JZ straight to the field-bit tail at 0x65391. TIFFSetCompressionScheme
   * (FUN_000651a0) takes (tif, scheme) -- EDI is pushed first, so it is the
   * second argument (0x653ae-0x653b0) -- and its EAX lands in `status` before
   * the test (0x653ba), which is the assignment-in-condition below. */
  case TIFFTAG_COMPRESSION:
    v = va_arg(ap, unsigned long) & 0xffff;
    if (TIFFFieldSet(tif, FIELD_COMPRESSION)) {
      if (tif->td_compression == v)
        break;
      if (tif->tif_cleanup)
        (*tif->tif_cleanup)(tif);
    }
    if ((status = FUN_000651a0(tif, v)) != 0)
      tif->td_compression = (unsigned short)v;
    else
      goto end;
    break;
  /* 0x653cc */
  case TIFFTAG_PHOTOMETRIC:
    tif->td_photometric = TIFF_VA_ARG_UINT16(ap);
    break;
  /* 0x653de */
  case TIFFTAG_THRESHHOLDING:
    tif->td_threshholding = TIFF_VA_ARG_UINT16(ap);
    break;
  /* 0x653f0. LSB2MSB is compared first (0x653f5). */
  case TIFFTAG_FILLORDER:
    v = va_arg(ap, int);
    if (v != FILLORDER_LSB2MSB && v != FILLORDER_MSB2LSB)
      goto badvalue;
    tif->td_fillorder = (unsigned short)v;
    break;
  /* 0x6540f .. 0x654b7 and 0x655e4. The nine string arms pass the slot address
   * in EDI and the vararg in EBX with no pushes, hence the @<edi>/@<ebx>
   * declaration of FUN_00065250 (upstream _TIFFsetString). */
  case TIFFTAG_DOCUMENTNAME:
    FUN_00065250(&tif->td_documentname, va_arg(ap, char *));
    break;
  /* 0x65427 */
  case TIFFTAG_ARTIST:
    FUN_00065250(&tif->td_artist, va_arg(ap, char *));
    break;
  /* 0x6543f */
  case TIFFTAG_DATETIME:
    FUN_00065250(&tif->td_datetime, va_arg(ap, char *));
    break;
  /* 0x65457 */
  case TIFFTAG_HOSTCOMPUTER:
    FUN_00065250(&tif->td_hostcomputer, va_arg(ap, char *));
    break;
  /* 0x6546f */
  case TIFFTAG_IMAGEDESCRIPTION:
    FUN_00065250(&tif->td_imagedescription, va_arg(ap, char *));
    break;
  /* 0x65487 */
  case TIFFTAG_MAKE:
    FUN_00065250(&tif->td_make, va_arg(ap, char *));
    break;
  /* 0x6549f */
  case TIFFTAG_MODEL:
    FUN_00065250(&tif->td_model, va_arg(ap, char *));
    break;
  /* 0x654b7 */
  case TIFFTAG_SOFTWARE:
    FUN_00065250(&tif->td_software, va_arg(ap, char *));
    break;
  /* 0x654cf. An out-of-range orientation only warns and leaves the field alone,
   * yet still falls through to the field-bit tail (JMP 0x65858 at 0x65508).
   * FUN_0006f9d0 is TIFFWarning; the argument order is fixed by the push order
   * at 0x654f8-0x654ff, last argument first. */
  case TIFFTAG_ORIENTATION:
    v = va_arg(ap, int);
    if (v < ORIENTATION_TOPLEFT || ORIENTATION_LEFTBOT < v)
      FUN_0006f9d0(tif->tif_name, "Bad value %ld for \"%s\" tag ignored", v,
                   ((tiff_field_info_t *)FUN_00066380(tag))->field_name);
    else
      tif->td_orientation = (unsigned short)v;
    break;
  /* 0x6550d. The >4 arm reports through TIFFError (FUN_00068a30) and returns
   * status, not 0 directly: 0x65534 stores 0 into the status slot and 0x6553b
   * reads it straight back out. */
  case TIFFTAG_SAMPLESPERPIXEL:
    v = va_arg(ap, int);
    if (v == 0)
      goto badvalue;
    if (v > 4) {
      FUN_00068a30(tif->tif_name, "Cannot handle %ld-channel data", v);
      status = 0;
      goto end;
    }
    tif->td_samplesperpixel = (unsigned short)v;
    break;
  /* 0x65550 */
  case TIFFTAG_ROWSPERSTRIP:
    v = va_arg(ap, int);
    if (v == 0)
      goto badvalue;
    tif->td_rowsperstrip = v;
    if (!TIFFFieldSet(tif, FIELD_TILEDIMENSIONS)) {
      tif->td_tilelength = v;
      tif->td_tilewidth = tif->td_imagewidth;
    }
    break;
  /* 0x6557b */
  case TIFFTAG_MINSAMPLEVALUE:
    tif->td_minsamplevalue = va_arg(ap, unsigned long) & 0xffff;
    break;
  /* 0x65590 */
  case TIFFTAG_MAXSAMPLEVALUE:
    tif->td_maxsamplevalue = va_arg(ap, unsigned long) & 0xffff;
    break;
  /* 0x655a5. `FLD qword ptr [EDX]` then `FSTP dword ptr [EAX+0x54]`: the
   * vararg is a double and the field is a float. All four resolution/position
   * arms are this shape. */
  case TIFFTAG_XRESOLUTION:
    tif->td_xresolution = (float)va_arg(ap, double);
    break;
  /* 0x655b5 */
  case TIFFTAG_YRESOLUTION:
    tif->td_yresolution = (float)va_arg(ap, double);
    break;
  /* 0x655c5. CONTIG is compared first (0x655ca). */
  case TIFFTAG_PLANARCONFIG:
    v = va_arg(ap, int);
    if (v != PLANARCONFIG_CONTIG && v != PLANARCONFIG_SEPARATE)
      goto badvalue;
    tif->td_planarconfig = (unsigned short)v;
    break;
  /* 0x655e4 */
  case TIFFTAG_PAGENAME:
    FUN_00065250(&tif->td_pagename, va_arg(ap, char *));
    break;
  /* 0x655fc */
  case TIFFTAG_XPOSITION:
    tif->td_xposition = (float)va_arg(ap, double);
    break;
  /* 0x6560c */
  case TIFFTAG_YPOSITION:
    tif->td_yposition = (float)va_arg(ap, double);
    break;
  /* 0x6561c */
  case TIFFTAG_GROUP3OPTIONS:
    tif->td_group3options = va_arg(ap, unsigned long);
    break;
  /* 0x6562c */
  case TIFFTAG_GROUP4OPTIONS:
    tif->td_group4options = va_arg(ap, unsigned long);
    break;
  /* 0x6563c */
  case TIFFTAG_RESOLUTIONUNIT:
    v = va_arg(ap, int);
    if (v < RESUNIT_NONE || RESUNIT_CENTIMETER < v)
      goto badvalue;
    tif->td_resolutionunit = (unsigned short)v;
    break;
  /* 0x6565f. Two word varargs, one `ADD EAX,0x4` between them (0x65668). */
  case TIFFTAG_PAGENUMBER:
    tif->td_pagenumber[0] = TIFF_VA_ARG_UINT16(ap);
    tif->td_pagenumber[1] = TIFF_VA_ARG_UINT16(ap);
    break;
  /* 0x6567b */
  case TIFFTAG_HALFTONEHINTS:
    tif->td_halftonehints[0] = TIFF_VA_ARG_UINT16(ap);
    tif->td_halftonehints[1] = TIFF_VA_ARG_UINT16(ap);
    break;
  /* 0x6569d. `MOV CL,byte ptr [EDX+0x36]` / `MOV EAX,1` / `SHL EAX,CL` is the
   * shift below; the count is spilled to [EBP-8] (0x656b9) because it is live
   * across all three calls, which is the second stack local the frame reserves.
   * FUN_000652a0 (upstream _TIFFsetShortArray) takes the slot address in ESI
   * and the vararg in EBX, with the count pushed -- three pushes cleaned by one
   * ADD ESP,0xc at 0x656ef. */
  case TIFFTAG_COLORMAP:
    v32 = (1L << tif->td_bitspersample);
    FUN_000652a0(&tif->td_colormap[0], va_arg(ap, unsigned short *), v32);
    FUN_000652a0(&tif->td_colormap[1], va_arg(ap, unsigned short *), v32);
    FUN_000652a0(&tif->td_colormap[2], va_arg(ap, unsigned short *), v32);
    break;
  /* 0x656f7 */
  case TIFFTAG_PREDICTOR:
    tif->td_predictor = TIFF_VA_ARG_UINT16(ap);
    break;
  /* 0x65709. Two varargs into the same variable: the first is the count, tested
   * against td_samplesperpixel and then against 1; the second is the sample
   * type. The store is `MOV word ptr [EBX+0x74],DI` -- the SECOND value, not a
   * literal 1, which is what proves the reuse. */
  case TIFFTAG_EXTRASAMPLES:
    v = va_arg(ap, int);
    if (v > tif->td_samplesperpixel || v != 1)
      goto badvalue;
    v = va_arg(ap, int);
    if (v != EXTRASAMPLE_ASSOCALPHA)
      goto badvalue;
    tif->td_matteing = (unsigned short)v;
    break;
  /* 0x6573d */
  case TIFFTAG_BADFAXLINES:
    tif->td_badfaxlines = va_arg(ap, unsigned long);
    break;
  /* 0x6574d */
  case TIFFTAG_CLEANFAXDATA:
    tif->td_cleanfaxdata = TIFF_VA_ARG_UINT16(ap);
    break;
  /* 0x6575f */
  case TIFFTAG_CONSECUTIVEBADFAXLINES:
    tif->td_consecutivebadfaxlines = TIFF_VA_ARG_UINT16(ap);
    break;
  /* 0x65771. `AND EAX,0x80000007 / JNS / DEC / OR 0xfffffff8 / INC / JNZ` is
   * MSVC's SIGNED remainder-by-8 test, so the vararg is a signed int and the
   * source condition is `v % 8`. Writing the decompiler's flag dance instead
   * would not reproduce it. */
  case TIFFTAG_TILEWIDTH:
    v = va_arg(ap, int);
    if (v % 8)
      goto badvalue;
    tif->td_tilewidth = v;
    tif->field_0a |= TIFF_ISTILED;
    break;
  /* 0x65799 */
  case TIFFTAG_TILELENGTH:
    v = va_arg(ap, int);
    if (v % 8)
      goto badvalue;
    tif->td_tilelength = v;
    tif->field_0a |= TIFF_ISTILED;
    break;
  /* 0x657c2, reached by the JZ at 0x6530f rather than the jump table. */
  case TIFFTAG_MATTEING:
    tif->td_matteing = TIFF_VA_ARG_UINT16(ap);
    break;
  /* 0x65811 */
  case TIFFTAG_TILEDEPTH:
    v = va_arg(ap, int);
    if (v == 0)
      goto badvalue;
    tif->td_tiledepth = v;
    break;
  /* 0x65822 */
  case TIFFTAG_IMAGEDEPTH:
    tif->td_imagedepth = va_arg(ap, unsigned long);
    break;
  /* 0x6582f, shared by both tags: the `CMP ESI,0x80e4` at 0x6582f re-tests the
   * tag after the vararg is read, and only the DATATYPE spelling maps 0 onto
   * SAMPLEFORMAT_VOID. The range check is the else arm (0x65847). */
  case TIFFTAG_DATATYPE:
  case TIFFTAG_SAMPLEFORMAT:
    v = va_arg(ap, int);
    if (tag == TIFFTAG_DATATYPE && v == 0)
      v = SAMPLEFORMAT_VOID;
    else if (v < SAMPLEFORMAT_UINT || SAMPLEFORMAT_VOID < v)
      goto badvalue;
    tif->td_sampleformat = (unsigned short)v;
    break;
  /* 0x657e3, reached both by the `JA` range check on the jump table index
   * (0x6531e) and by falling off the end of the 0x80e4-0x80e6 compare chain. */
  default:
    FUN_00068a30(tif->tif_name, "Internal error, tag value botch, tag \"%s\"",
                 ((tiff_field_info_t *)FUN_00066380(tag))->field_name);
    status = 0;
    goto end;
  }
  /* 0x65858-0x65886. */
  TIFFSetFieldBit(tif, ((tiff_field_info_t *)FUN_00066380(tag))->field_bit);
  tif->field_0a |= TIFF_DIRTYDIRECT;
end:
  /* 0x6588a. */
  return status;
badvalue:
  /* 0x65894. Both entries into this block report the offending value, which is
   * why every arm above funnels the same `v` here. */
  FUN_00068a30(tif->tif_name, "%ld: Bad value for \"%s\"", v,
               ((tiff_field_info_t *)FUN_00066380(tag))->field_name);
  return 0;
}
