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

/* Strip/tile array tags; TIFFGetField1 (0x65af0) shares one handler per pair
 * (byte table at 0x65e34). */
#define TIFFTAG_STRIPOFFSETS 273 /* 0x111 */
#define TIFFTAG_STRIPBYTECOUNTS 279 /* 0x117 */
#define TIFFTAG_TILEOFFSETS 324 /* 0x144 */
#define TIFFTAG_TILEBYTECOUNTS 325 /* 0x145 */

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
  /* File handle, read here as a SIGNED 16-bit value: EstimateStripByteCounts
   * forwards it with `MOVSX ECX,word ptr [ESI+0x4]` (0x66472) into the
   * file-size helper at 0x64f50. tif_open.c's fuller recovery of the same
   * struct declares the same offset `short tif_fd` from independent evidence;
   * upstream libtiff types it `int`, which this build narrowed. */
  short tif_fd; /* 0x04 */
  unsigned char pad_06[0x04]; /* 0x06 */
  /* Flag byte. _TIFFVSetField only ever ORs into it: 0x80 for the two tile
   * dimension tags (`OR byte ptr [EAX+0xa],0x80` at 0x65790/0x657b9) and 0x02
   * once the field bit is set (0x65886). Bit 7 is the same bit TIFFIsTiled
   * (0x6d880) reads, so 0x80 is this build's tiled flag; 0x02 is its
   * dirty-directory flag. Upstream keeps both in a `uint32 tif_flags` with
   * different values (TIFF_ISTILED == 0x400) -- do NOT import upstream's
   * numbering, and keep the field mechanical rather than named tif_flags. */
  char field_0a; /* 0x0a */
  unsigned char pad_0b[0x01]; /* 0x0b */
  /* FUN_00066e70 copies tif_nextdiroff here (`MOV dword ptr [EBX+0xc],EAX` at
   * 0x66e8a) and compares __lseek's result against it (0x66eb0). */
  unsigned long tif_diroff; /* 0x0c */
  /* TIFFSetDirectory stores the next directory offset here
   * (`MOV dword ptr [ESI+0x10],EAX` at 0x662c2); tif_open.c names the same
   * offset tif_nextdiroff from independent evidence. */
  unsigned long tif_nextdiroff; /* 0x10 */
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
  /* FUN_00066e70 stores the strip/tile count per image here (0x671ba) and
   * copies it into 0xb8, multiplied by td_samplesperpixel when
   * td_planarconfig == 2 (0x671e8-0x67202). */
  unsigned long td_stripsperimage; /* 0xb4 */
  unsigned long td_nstrips; /* 0xb8 */
  /* The two per-strip arrays, proven by EstimateStripByteCounts: the malloc'd
   * block is stored to +0xc0 (`MOV dword ptr [ESI+0xc0],EBX` at 0x6646a) and
   * then indexed at [0], and +0xbc is loaded and dereferenced once
   * (`MOV EDX,dword ptr [ESI+0xbc]` / `MOV EDX,dword ptr [EDX]` at
   * 0x664cd-0x664d3) in the same expression upstream spells
   * `td->td_stripoffset[i-1] + td->td_stripbytecount[i-1]`. Both are pointers
   * to 32-bit counts; nothing here observes the element count. */
  unsigned long *td_stripoffset; /* 0xbc */
  unsigned long *td_stripbytecount; /* 0xc0 */
  /* First word of the on-disk file header, which this build stores inline in
   * the handle: TIFFFetchFloat compares it against 0x4d4d
   * (`CMP word ptr [ESI+0xc4],0x4d4d` at 0x6677b) to pick the big-endian
   * extraction arm. tif_open.c's independent recovery of the same struct
   * declares an eight-byte tiff_header_t at 0xc4 whose first member is
   * tiff_magic, which agrees. */
  unsigned short tiff_magic; /* 0xc4 */
  unsigned char pad_c6[0x02]; /* 0xc6 */
  /* First directory offset from the file header: TIFFSetDirectory seeds its
   * walk with `MOV EAX,dword ptr [ESI+0xc8]` (0x66209). tif_open.c names the
   * same offset tiff_diroff. */
  unsigned long tiff_diroff; /* 0xc8 */
  /* Two per-type tables indexed by tdir_type. Both are POINTERS, loaded and
   * then indexed by type*4: `MOV EDI,dword ptr [ESI+0xcc]` /
   * `MOV ECX,dword ptr [EDI+EDX*0x1]` with EDX already scaled (0x6678a-0x66793)
   * and `MOV ECX,dword ptr [ESI+0xd0]` / `MOV ESI,dword ptr [ECX+EDX*0x1]`
   * (0x66798-0x6679e). The 0xcc value supplies a SHR count and the 0xd0 value
   * an AND mask, which is upstream libtiff's tif_typeshift / tif_typemask pair
   * (tiffiop.h); the offsets are Bungie's. Element width is the observed dword
   * load; signedness is unobservable, so upstream's unsigned typing is kept. */
  const unsigned long *tif_typeshift; /* 0xcc */
  const unsigned long *tif_typemask; /* 0xd0 */
  /* 0xd4/0xdc/0xe4/0xe8: only ever observed receiving -1 (0x67648-0x6765e). */
  unsigned long field_d4; /* 0xd4 */
  /* Dword store `MOV dword ptr [ESI+0xd8],EDI` at 0x662c5 in TIFFSetDirectory;
   * tif_open.c names the same offset tif_curdir (32-bit in this build). */
  unsigned long tif_curdir; /* 0xd8 */
  unsigned long field_dc; /* 0xdc */
  unsigned char pad_e0[0x04]; /* 0xe0 */
  unsigned long field_e4; /* 0xe4 */
  unsigned long field_e8; /* 0xe8 */
  /* Receives FUN_0006f910's result (0x6766a). */
  unsigned long field_ec; /* 0xec */
  unsigned char pad_f0[0x2c]; /* 0xf0 */
  /* Codec teardown hook, called before a new compression scheme replaces the
   * current one (`MOV EAX,dword ptr [EAX+0x11c]` / `CALL EAX` at
   * 0x65399-0x653a6, one pushed argument). tif_open.c proves the identity: the
   * slot is loaded with LZWCleanup (0x6cac0) by the codec installer. */
  tiff_void_method_t tif_cleanup; /* 0x11c */
  unsigned char pad_120[0x04]; /* 0x120 */
  /* Receives TIFFScanlineSize's result (0x67679). */
  long tif_scanlinesize; /* 0x124 */
} tiff_t;

/* One row of the tag descriptor table. _TIFFVSetField only ever reads two
 * members of what FUN_00066380 returns: a 16-bit bit index at +0x0c
 * (`MOVZX ECX,word ptr [EAX+0xc]` at 0x6585e, and the same offset as a byte at
 * 0x65874 for the shift count) and a string at +0x10 that every diagnostic
 * prints through "%s" (0x654f0, 0x657e9, 0x6589d). Upstream libtiff names those
 * TIFFFieldInfo::field_bit and ::field_name. Nothing here observes the rest of
 * the row, so its size is unknown and the struct stops at the last read. */
typedef struct tiff_field_info_s {
  /* Matched 16 bits wide by TIFFFindFieldInfo (`CMP word ptr [EAX],SI` at
   * 0x66334 and `CMP CX,SI` at 0x66352, both against a word loaded from the
   * tag argument slot), and tested 16 bits wide as the table terminator
   * (`TEST CX,CX` at 0x66348/0x66367). Upstream libtiff spells this
   * TIFFFieldInfo::field_tag as a 32-bit ttag_t; this build only ever reads
   * the low word, so the width of the rest is unproven and stays padding. */
  unsigned short field_tag; /* 0x00 */
  /* Read as a signed word by FUN_00066e70 (`MOV AX,word ptr [ESI+0x2]` /
   * `MOVSX EAX,AX` at 0x670aa-0x670dd): -1 skips the count check, -2 means
   * td_samplesperpixel, anything else is the required tdir_count. */
  short field_readcount; /* 0x02 */
  unsigned char pad_04[4]; /* 0x04 */
  /* Compared as a full dword against TIFFFindFieldInfo's second argument
   * (`CMP EDX,dword ptr [EAX+0x8]` at 0x6633d, `CMP dword ptr [EAX+0x8],EDX`
   * at 0x6635b). Upstream's TIFFFieldInfo::field_type (TIFFDataType). */
  int field_type; /* 0x08 */
  unsigned short field_bit; /* 0x0c */
  /* Read by TIFFVSetField (0x65a70) as a 16-bit quantity compared against zero
   * (`CMP word ptr [EAX+0xe],DI` at 0x65a9a, DI == 0), gating the
   * "Cannot modify tag while writing" diagnostic. Upstream libtiff puts two
   * bytes here, field_oktochange at 0x0e and field_passcount at 0x0f; the only
   * access this TU has seen covers both at once, so the pair is kept as one
   * mechanical 16-bit field rather than split on upstream's word. */
  unsigned short field_0e; /* 0x0e */
  char *field_name; /* 0x10 */
} tiff_field_info_t;

/* Bit indices into td_fieldsset. Only the two the setter tests are known. */
#define FIELD_TILEDIMENSIONS 1 /* TEST byte ptr [EAX+0x14],0x2 at 0x65563 */
#define FIELD_COMPRESSION 7 /* sign of byte ptr [EBX+0x14] at 0x6537e */

/* field_0a bits, in this build's numbering (see the field comment above). */
#define TIFF_ISTILED 0x80
#define TIFF_DIRTYDIRECT 0x02
/* `TEST byte ptr [EBX+0xa],0x8` at 0x65a86 gates TIFFVSetField's
 * already-writing check, which is upstream's TIFF_BEENWRITING test. Upstream
 * numbers that bit 0x0040 inside a 32-bit tif_flags; this build's bit is 0x08
 * of the byte at 0x0a, so keep this build's numbering. */
#define TIFF_BEENWRITING 0x08

/* Second argument of TIFFFindFieldInfo (0x66320): `XOR EDI,EDI` at 0x65a7c
 * feeds both PUSH EDI sites (0x65a8c, 0x65aa0). Upstream passes TIFF_ANY
 * there, spelled TIFF_NOTYPE (0) in libtiff's TIFFDataType enum. */
#define TIFF_NOTYPE 0

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

/* 0x65250 -- upstream libtiff setString. The slot address arrives in EDI and
 * the new string in EBX (no stack arguments, plain RET). debug_malloc's result
 * is stored into the slot before the NULL test (0x6528e), matching upstream's
 * assignment-in-condition. */
void FUN_00065250(char **destination /* @<edi> */, char *value /* @<ebx> */)
{
  if (*destination) {
    debug_free(*destination, "c:\\halo\\SOURCE\\bitmaps\\libtiff\\tif_dir.c",
               0x45);
    *destination = 0;
  }
  if (value) {
    size_t len = csstrlen(value) + 1;
    if ((*destination = (char *)debug_malloc(
           len, false, "c:\\halo\\SOURCE\\bitmaps\\libtiff\\tif_dir.c",
           0x48)) != NULL)
      csmemcpy(*destination, value, len);
  }
}

/* 0x652a0 -- upstream libtiff setShortArray. The slot address arrives in ESI
 * and the source array in EBX; only the element count is on the stack. The
 * byte count is `count + count` (ADD EDI,EDI at 0x652c3), computed before the
 * NULL test of the source, and debug_malloc's result is stored into the slot
 * before its own NULL test (0x652dd). */
void FUN_000652a0(unsigned short **destination /* @<esi> */,
                  unsigned short *value /* @<ebx> */, long count)
{
  if (*destination) {
    debug_free(*destination, "c:\\halo\\SOURCE\\bitmaps\\libtiff\\tif_dir.c",
               0x51);
    *destination = 0;
  }
  count *= sizeof(unsigned short);
  if (value && (*destination = (unsigned short *)debug_malloc(
                  count, false, "c:\\halo\\SOURCE\\bitmaps\\libtiff\\tif_dir.c",
                  0x53)) != NULL)
    csmemcpy(*destination, value, count);
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
   * (TIFFSetCompressionScheme) takes (tif, scheme) -- EDI is pushed first, so
   * it is the second argument (0x653ae-0x653b0) -- and its EAX lands in
   * `status` before the test (0x653ba), which is the assignment-in-condition
   * below. */
  case TIFFTAG_COMPRESSION:
    v = va_arg(ap, unsigned long) & 0xffff;
    if (TIFFFieldSet(tif, FIELD_COMPRESSION)) {
      if (tif->td_compression == v)
        break;
      if (tif->tif_cleanup)
        (*tif->tif_cleanup)(tif);
    }
    if ((status = TIFFSetCompressionScheme(tif, v)) != 0)
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

/* ---------------------------------------------------------------------------
 * FUN_000659c0 (0x659c0) -- out-of-line copy of the OkToChangeTag test that
 * TIFFSetField (0x659f0) and TIFFVSetField (0x65a70) carry inline. No callers
 * are known.
 *
 * ABI (0x659c0-0x659eb):
 *   - `tag` arrives in EAX (`CMP EAX,0x101` at 0x659c0, then `PUSH EAX` at
 *     0x659cf) and the tif handle in ECX (`TEST byte ptr [ECX+0xa],0x8` at
 *     0x659c7); neither is written first, so both are register arguments.
 *   - TIFFFindFieldInfo takes two cdecl arguments: `PUSH 0x0` (dt) then
 *     `PUSH EAX` (tag), `ADD ESP,0x8`.
 *   - Only a found row whose field_0e word is zero returns 0 (`XOR EAX,EAX`
 *     at 0x659e3); a NULL row returns 1, as in the inline copies.
 * ------------------------------------------------------------------------- */

#if defined(_MSC_VER) && !defined(__clang__)
#pragma inline_depth(0)
#endif
/* 0x659c0 */
int FUN_000659c0(int tag, void *tif_)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_field_info_t *fip;

  if (tag != TIFFTAG_IMAGELENGTH && (tif->field_0a & TIFF_BEENWRITING)) {
    fip = (tiff_field_info_t *)TIFFFindFieldInfo(tag, TIFF_NOTYPE);
    if (fip != 0 && fip->field_0e == 0)
      return 0;
  }
  return 1;
}
#if defined(_MSC_VER) && !defined(__clang__)
#pragma inline_depth()
#endif

/* ---------------------------------------------------------------------------
 * TIFFSetField (0x659f0) -- upstream libtiff tif_dir.c.
 *
 * Upstream's TIFFSetField is va_start + TIFFVSetField; here the TIFFVSetField
 * body (0x65a70) is inlined in full: the same IMAGELENGTH (0x101) exemption,
 * the TIFF_BEENWRITING test (`TEST byte ptr [EBX+0xa],0x8` at 0x65a06), two
 * separate TIFFFindFieldInfo(tag, 0) calls (0x65a0e and 0x65a36), and the tail
 * CALL 0x652f0 with `LEA EAX,[EBP+0x10]` as the va_list (0x65a20-0x65a26).
 * The diagnostic module is the literal "TIFFSetField" (0x25f678) with the
 * shared format at 0x25f688. The error path returns EDI (still zero).
 * ------------------------------------------------------------------------- */

/* TIFFFindFieldInfo is a real out-of-line CALL in the reference (0x65a0e,
 * 0x65a36); keep cl.exe /Ob2 from inlining it into the VC71 scoring build,
 * as for TIFFGetField below. The production clang build is unaffected. */
#if defined(_MSC_VER) && !defined(__clang__)
#pragma inline_depth(0)
#endif
/* 0x659f0 */
int TIFFSetField(int file, int field, ...)
{
  tiff_t *tif = (tiff_t *)file;
  tiff_field_info_t *fip;
  int status = 0;
  int ok = 1;
  va_list ap;

  /* 0x659fe-0x65a1c: the OkToChangeTag test. Only a found row whose
   * field_0e is zero refuses the change (JZ 0x65a33 at 0x65a1e). */
  if (field != TIFFTAG_IMAGELENGTH && (tif->field_0a & TIFF_BEENWRITING)) {
    fip = (tiff_field_info_t *)TIFFFindFieldInfo(field, TIFF_NOTYPE);
    if (fip != 0 && fip->field_0e == 0)
      ok = 0;
  }
  /* The reference lays the va tail (0x65a20-0x65a32) out BEFORE the error
   * block, falling through from the CMP at 0x65a1a; the ok flag with an
   * if/else is what gives cl.exe that order (a goto or an early return
   * sinks the va tail to the end instead). */
  if (ok) {
    /* 0x65a20-0x65a2b. */
    va_start(ap, field);
    status = _TIFFVSetField(tif, field, ap);
    va_end(ap);
  } else {
    /* 0x65a33-0x65a58. */
    fip = (tiff_field_info_t *)TIFFFindFieldInfo(field, TIFF_NOTYPE);
    if (fip != 0)
      FUN_00068a30("TIFFSetField", "%s: Cannot modify tag \"%s\" while writing",
                   tif->tif_name, fip->field_name);
  }
  /* 0x65a5b, MOV EAX,EDI with EDI still zero on the error path: `status`
   * keeps 0 live in EDI (also the `CMP word [EAX+0xe],DI`). */
  return status;
}
#if defined(_MSC_VER) && !defined(__clang__)
#pragma inline_depth()
#endif

/* ---------------------------------------------------------------------------
 * TIFFVSetField (0x65a70) -- upstream libtiff tif_dir.c.
 *
 * Identification: upstream splits this into TIFFVSetField plus a static
 * OkToChangeTag helper; here the helper is inlined and only its
 * "writing has begun and the tag is not ok to change" arm survives -- there is
 * no unknown-tag diagnostic, so a NULL row from TIFFFindFieldInfo simply falls
 * through to _TIFFVSetField (JZ 0x65ace at 0x65a98).
 *
 * Ghidra reports `void FUN_00065a70(void)` with in_stack_ parameters because
 * kb.json declared it `void(void)`; the three arguments are at [EBP+8],
 * [EBP+0xc] and [EBP+0x10], and the tail path returns _TIFFVSetField's EAX
 * unchanged (CALL 0x652f0 / ADD ESP,0xc / epilogue at 0x65ad4-0x65ae0), so the
 * function returns int.
 *
 * TIFFFindFieldInfo is called TWICE for the same tag (0x65a8e and 0x65aa2,
 * each with its own PUSH ESI / PUSH EDI pair and ADD ESP,0x8) -- upstream
 * calls _TIFFFindFieldInfo once for the oktochange test and _TIFFFieldWithTag
 * again inside the error call. Both sites here target 0x66320. Folding them to
 * one call is a shape regression.
 *
 * The call takes two stack arguments only: `PUSH EDI` (0) then `PUSH ESI`
 * (tag), so the first argument is the tag -- this build's TIFFFindFieldInfo
 * does not take the TIFF handle that upstream's _TIFFFindFieldInfo does.
 *
 * Bungie deviation from upstream libtiff 3.5.x: the unknown-tag
 * "%s: Unknown %stag %u" error is absent.
 * ------------------------------------------------------------------------- */

/* 0x65a70 */
int TIFFVSetField(void *tif_, int tag, va_list ap)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_field_info_t *fip;

  /* 0x65a7e-0x65a9e. */
  if (tag != TIFFTAG_IMAGELENGTH && (tif->field_0a & TIFF_BEENWRITING)) {
    fip = (tiff_field_info_t *)TIFFFindFieldInfo(tag, TIFF_NOTYPE);
    if (fip != 0 && fip->field_0e == 0) {
      /* 0x65aa0-0x65ac4. The module argument is the literal "TIFFVSetField"
       * (0x25f6b4), not tif->tif_name as in _TIFFVSetField; the handle name is
       * the first "%s" of the format at 0x25f688. */
      fip = (tiff_field_info_t *)TIFFFindFieldInfo(tag, TIFF_NOTYPE);
      if (fip != 0)
        FUN_00068a30("TIFFVSetField",
                     "%s: Cannot modify tag \"%s\" while writing",
                     tif->tif_name, fip->field_name);
      /* 0x65ac7, MOV EAX,EDI with EDI still zero. */
      return 0;
    }
  }
  /* 0x65ace-0x65ad9. Straight passthrough of all three arguments. */
  return _TIFFVSetField(tif_, tag, ap);
}

/* 0x65af0: register-only callee (EAX = ap, ECX = the directory base, which
 * every caller derives as &tif->td_fieldsset, EDX = tag). Offsets in the
 * disassembly are relative to that base, i.e. handle offset - 0x14. The tag
 * compares are signed (JG at 0x65af7). `ap` is consumed in place: a second
 * va_arg is `ADD EAX,0x4` on the incoming register (0x65c63), and its only
 * callers are in this file, so the compiler chose the register convention. */
void TIFFGetField1(va_list ap, void *field_table, unsigned int tag)
{
  tiff_t *tif =
    (tiff_t *)((char *)field_table - offsetof(tiff_t, td_fieldsset));

  switch ((int)tag) {
  case TIFFTAG_SUBFILETYPE:
    *va_arg(ap, unsigned long *) = tif->td_subfiletype;
    break;
  case TIFFTAG_IMAGEWIDTH:
    *va_arg(ap, unsigned long *) = tif->td_imagewidth;
    break;
  case TIFFTAG_IMAGELENGTH:
    *va_arg(ap, unsigned long *) = tif->td_imagelength;
    break;
  case TIFFTAG_BITSPERSAMPLE:
    *va_arg(ap, unsigned short *) = tif->td_bitspersample;
    break;
  case TIFFTAG_COMPRESSION:
    *va_arg(ap, unsigned short *) = tif->td_compression;
    break;
  case TIFFTAG_PHOTOMETRIC:
    *va_arg(ap, unsigned short *) = tif->td_photometric;
    break;
  case TIFFTAG_THRESHHOLDING:
    *va_arg(ap, unsigned short *) = tif->td_threshholding;
    break;
  case TIFFTAG_FILLORDER:
    *va_arg(ap, unsigned short *) = tif->td_fillorder;
    break;
  case TIFFTAG_DOCUMENTNAME:
    *va_arg(ap, char **) = tif->td_documentname;
    break;
  case TIFFTAG_ARTIST:
    *va_arg(ap, char **) = tif->td_artist;
    break;
  case TIFFTAG_DATETIME:
    *va_arg(ap, char **) = tif->td_datetime;
    break;
  case TIFFTAG_HOSTCOMPUTER:
    *va_arg(ap, char **) = tif->td_hostcomputer;
    break;
  case TIFFTAG_IMAGEDESCRIPTION:
    *va_arg(ap, char **) = tif->td_imagedescription;
    break;
  case TIFFTAG_MAKE:
    *va_arg(ap, char **) = tif->td_make;
    break;
  case TIFFTAG_MODEL:
    *va_arg(ap, char **) = tif->td_model;
    break;
  case TIFFTAG_SOFTWARE:
    *va_arg(ap, char **) = tif->td_software;
    break;
  case TIFFTAG_ORIENTATION:
    *va_arg(ap, unsigned short *) = tif->td_orientation;
    break;
  case TIFFTAG_SAMPLESPERPIXEL:
    *va_arg(ap, unsigned short *) = tif->td_samplesperpixel;
    break;
  case TIFFTAG_ROWSPERSTRIP:
    *va_arg(ap, unsigned long *) = tif->td_rowsperstrip;
    break;
  case TIFFTAG_MINSAMPLEVALUE:
    *va_arg(ap, unsigned short *) = (unsigned short)tif->td_minsamplevalue;
    break;
  case TIFFTAG_MAXSAMPLEVALUE:
    *va_arg(ap, unsigned short *) = (unsigned short)tif->td_maxsamplevalue;
    break;
  case TIFFTAG_XRESOLUTION:
    *va_arg(ap, float *) = tif->td_xresolution;
    break;
  case TIFFTAG_YRESOLUTION:
    *va_arg(ap, float *) = tif->td_yresolution;
    break;
  case TIFFTAG_PLANARCONFIG:
    *va_arg(ap, unsigned short *) = tif->td_planarconfig;
    break;
  case TIFFTAG_XPOSITION:
    *va_arg(ap, float *) = tif->td_xposition;
    break;
  case TIFFTAG_YPOSITION:
    *va_arg(ap, float *) = tif->td_yposition;
    break;
  case TIFFTAG_PAGENAME:
    *va_arg(ap, char **) = tif->td_pagename;
    break;
  case TIFFTAG_GROUP3OPTIONS:
    *va_arg(ap, unsigned long *) = tif->td_group3options;
    break;
  case TIFFTAG_GROUP4OPTIONS:
    *va_arg(ap, unsigned long *) = tif->td_group4options;
    break;
  case TIFFTAG_RESOLUTIONUNIT:
    *va_arg(ap, unsigned short *) = tif->td_resolutionunit;
    break;
  case TIFFTAG_PAGENUMBER:
    *va_arg(ap, unsigned short *) = tif->td_pagenumber[0];
    *va_arg(ap, unsigned short *) = tif->td_pagenumber[1];
    break;
  case TIFFTAG_HALFTONEHINTS:
    *va_arg(ap, unsigned short *) = tif->td_halftonehints[0];
    *va_arg(ap, unsigned short *) = tif->td_halftonehints[1];
    break;
  case TIFFTAG_COLORMAP:
    *va_arg(ap, unsigned short **) = tif->td_colormap[0];
    *va_arg(ap, unsigned short **) = tif->td_colormap[1];
    *va_arg(ap, unsigned short **) = tif->td_colormap[2];
    break;
  case TIFFTAG_PREDICTOR:
    *va_arg(ap, unsigned short *) = tif->td_predictor;
    break;
  case TIFFTAG_STRIPOFFSETS:
  case TIFFTAG_TILEOFFSETS:
    *va_arg(ap, unsigned long **) = tif->td_stripoffset;
    break;
  case TIFFTAG_STRIPBYTECOUNTS:
  case TIFFTAG_TILEBYTECOUNTS:
    *va_arg(ap, unsigned long **) = tif->td_stripbytecount;
    break;
  case TIFFTAG_MATTEING:
    *va_arg(ap, unsigned short *) = tif->td_matteing;
    break;
  case TIFFTAG_EXTRASAMPLES:
    *va_arg(ap, unsigned short *) = tif->td_matteing;
    *va_arg(ap, unsigned short **) = &tif->td_matteing;
    break;
  case TIFFTAG_BADFAXLINES:
    *va_arg(ap, unsigned long *) = tif->td_badfaxlines;
    break;
  case TIFFTAG_CLEANFAXDATA:
    *va_arg(ap, unsigned short *) = tif->td_cleanfaxdata;
    break;
  case TIFFTAG_CONSECUTIVEBADFAXLINES:
    *va_arg(ap, unsigned long *) = tif->td_consecutivebadfaxlines;
    break;
  case TIFFTAG_TILEWIDTH:
    *va_arg(ap, unsigned long *) = tif->td_tilewidth;
    break;
  case TIFFTAG_TILELENGTH:
    *va_arg(ap, unsigned long *) = tif->td_tilelength;
    break;
  case TIFFTAG_TILEDEPTH:
    *va_arg(ap, unsigned long *) = tif->td_tiledepth;
    break;
  case TIFFTAG_DATATYPE:
    *va_arg(ap, unsigned short *) =
      (tif->td_sampleformat == SAMPLEFORMAT_VOID ? 0 : tif->td_sampleformat);
    break;
  case TIFFTAG_SAMPLEFORMAT:
    *va_arg(ap, unsigned short *) = tif->td_sampleformat;
    break;
  case TIFFTAG_IMAGEDEPTH:
    *va_arg(ap, unsigned long *) = tif->td_imagedepth;
    break;
  default:
    /* 0x65d36-0x65d4f. */
    FUN_00068a30(
      "TIFFGetField1", "Internal error, no value returned for tag \"%s\"",
      ((tiff_field_info_t *)FUN_00066380((unsigned short)tag))->field_name);
    break;
  }
}

/* ---------------------------------------------------------------------------
 * TIFFGetField (0x65e90) -- upstream libtiff tif_dir.c, with TIFFVGetField
 * inlined into it.
 *
 * Upstream splits this in two: TIFFGetField is a bare va_start/va_end wrapper
 * around a static TIFFVGetField that does the field lookup, the FIELD_IGNORE
 * test and the TIFFFieldSet test before delegating to _TIFFVGetField. Here
 * there is no separate call for that body -- the whole sequence is in this
 * frame (0x65e9a lookup, 0x65eaa ignore test, 0x65ec5 field-set test, 0x65ed3
 * delegate), so the static was inlined at its single call site.
 *
 * The unknown-tag diagnostic is a Bungie addition: upstream's TIFFVGetField
 * returns 0 silently on a NULL FieldInfo, while this build reports
 * "Unknown field, tag 0x%x" (format at 0x25f714) under the module name
 * "TIFFGetField" (0x25f704) and then returns 0 through the shared
 * `XOR EAX,EAX` exit at 0x65ef3.
 *
 * ABI notes:
 *   - `tag` is read into ESI at 0x65e94 and is the ONLY value passed to both
 *     TIFFFindFieldInfo (0x65e99) and TIFFGetField1 (EDX at 0x65ed1).
 *   - The delegate is a three-register-argument function, already declared in
 *     kb.json as TIFFGetField1(char *ap@<eax>, void *field_table@<ecx>,
 *     unsigned int tag@<edx>). EAX is `LEA EAX,[EBP+0x10]` (0x65ece), the
 *     address of the first vararg slot -- that is the va_list VALUE, not its
 *     address, so `ap` is forwarded directly. ECX is `ADD ECX,0x14` (0x65ecb)
 *     off the tif handle, i.e. &tif->td_fieldsset[0], reusing the base the
 *     field-set test just indexed rather than re-deriving it.
 *   - Ghidra reports this as `void FUN_00065e90(void)` with `extraout_EAX` and
 *     `in_stack_00000004` because kb.json declared no parameters; both
 *     parameters are at [EBP+8] and [EBP+0xc].
 * ------------------------------------------------------------------------- */

/* `CMP AX,0xffff` at 0x65eaa. Upstream libtiff 3.5.x spells the sentinel
 * FIELD_IGNORE ((u_short) -1): a tag whose value is never mirrored into
 * td_fieldsset and so can never satisfy the TIFFFieldSet test below. */
#define FIELD_IGNORE 0xffff

/* TIFFFindFieldInfo is a real out-of-line CALL in the reference (it scores
 * 97.2% standalone) -- cl.exe /Ob2 inlines it into this caller by default,
 * unrolling the scan loop into this frame and tripping a LOADW-WARN on the
 * callee's field_tag. Bracket just this function so the VC71 scoring build
 * keeps the call; the production clang build is unaffected. */
#if defined(_MSC_VER) && !defined(__clang__)
#pragma inline_depth(0)
#endif
/* 0x65e90 */
int TIFFGetField(int file, int field, ...)
{
  tiff_t *tif = (tiff_t *)file;
  tiff_field_info_t *fip;
  unsigned short bit;
  unsigned int tag;
  va_list ap;

  va_start(ap, field);
  /* 0x65e97-0x65ea4. */
  tag = (unsigned int)field;
  fip = (tiff_field_info_t *)TIFFFindFieldInfo((int)tag, TIFF_NOTYPE);
  if (fip != 0) {
    /* 0x65eaa: field_bit loaded once into AX and reused by the set test. */
    bit = fip->field_bit;
    if (bit != FIELD_IGNORE && TIFFFieldSet(tif, bit)) {
      /* 0x65ecb-0x65edf. */
      TIFFGetField1(ap, (void *)tif->td_fieldsset, tag);
      va_end(ap);
      return 1;
    }
  } else {
    /* 0x65ee0-0x65ef0. */
    FUN_00068a30("TIFFGetField", "Unknown field, tag 0x%x", tag);
  }
  va_end(ap);
  /* 0x65ef3. */
  return 0;
}
#if defined(_MSC_VER) && !defined(__clang__)
#pragma inline_depth()
#endif

/* ---------------------------------------------------------------------------
 * TIFFVGetField (0x65f00) -- upstream libtiff tif_dir.c.
 *
 * Same body as the copy inlined into TIFFGetField (0x65e90) above, but with
 * the va_list arrived at as an explicit third stack parameter instead of
 * va_start: `MOV EAX,dword ptr [EBP + 0x10]` at 0x65f3b loads the argument
 * VALUE (contrast the `LEA EAX,[EBP+0x10]` at 0x65ece in TIFFGetField), so
 * `ap` is forwarded straight through to the delegate.
 *
 * ABI notes:
 *   - `tag` is [EBP+0xc], read into ESI at 0x65f04, and is the only value
 *     passed to TIFFFindFieldInfo (PUSH 0x0 / PUSH ESI at 0x65f07-0x65f09,
 *     ADD ESP,0x8) and to TIFFGetField1 (EDX at 0x65f41).
 *   - The delegate at 0x65af0 takes three register arguments, already declared
 *     in kb.json: EAX = ap, ECX = `ADD ECX,0x14` off the tif handle
 *     (&tif->td_fieldsset[0], reusing the base the field-set test indexed),
 *     EDX = tag. Its return value is discarded -- 0x65f48 is a literal
 *     `MOV EAX,0x1`.
 *   - The unknown-tag diagnostic (0x65f50-0x65f60) uses the SAME module string
 *     as TIFFGetField, "TIFFGetField" at 0x25f704, not "TIFFVGetField"; the
 *     format at 0x25f714 is shared too. Both fall through to the shared
 *     `XOR EAX,EAX` exit at 0x65f63.
 * ------------------------------------------------------------------------- */

/* TIFFFindFieldInfo is a real out-of-line CALL in the reference (it scores
 * 97.2% standalone) -- cl.exe /Ob2 inlines it into this caller by default,
 * unrolling the scan loop into this frame and tripping a LOADW-WARN on the
 * callee's field_tag. Bracket just this function so the VC71 scoring build
 * keeps the call; the production clang build is unaffected. */
#if defined(_MSC_VER) && !defined(__clang__)
#pragma inline_depth(0)
#endif
/* 0x65f00 */
int TIFFVGetField(void *tif_, unsigned int tag, char *ap)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_field_info_t *fip;
  unsigned short bit;

  /* 0x65f07-0x65f14. */
  fip = (tiff_field_info_t *)TIFFFindFieldInfo((int)tag, TIFF_NOTYPE);
  if (fip != 0) {
    /* 0x65f16: field_bit loaded once into AX and reused by the set test. */
    bit = fip->field_bit;
    if (bit != FIELD_IGNORE && TIFFFieldSet(tif, bit)) {
      /* 0x65f3b-0x65f48. */
      TIFFGetField1(ap, (void *)tif->td_fieldsset, tag);
      return 1;
    }
  } else {
    /* 0x65f50-0x65f60. */
    FUN_00068a30("TIFFGetField", "Unknown field, tag 0x%x", tag);
  }
  /* 0x65f63. */
  return 0;
}
#if defined(_MSC_VER) && !defined(__clang__)
#pragma inline_depth()
#endif

/* 0x65f70: EDX = tag, ECX = td, EAX = LEA [EBP+0x10] (the va_list value), then
 * tail-JMP to TIFFGetField1. EAX is never set to a result, so the declared
 * return is void. */
void _TIFFgetfield(void *td, int tag, ...)
{
  va_list ap;

  va_start(ap, tag);
  TIFFGetField1(ap, td, (unsigned int)tag);
  va_end(ap);
}

/* ---------------------------------------------------------------------------
 * TIFFFreeDirectory (0x65f90) -- upstream libtiff tif_dir.c.
 *
 * Each pointer member is released through debug_free with this TU's __FILE__
 * (0x25f5c4) and its own __LINE__ (0x367-0x372, then 0x388/0x389), then
 * cleared. Offsets 0x80-0xb0 and 0xbc/0xc0 are read `MOV EAX,[ESI+off]` and
 * zeroed `MOV [ESI+off],EDI` in address order (0x65f97-0x6617e).
 * ------------------------------------------------------------------------- */
void TIFFFreeDirectory(int file)
{
  tiff_t *tif = (tiff_t *)file;

  if (tif->td_colormap[0] != 0) {
    debug_free(tif->td_colormap[0],
               "c:\\halo\\SOURCE\\bitmaps\\libtiff\\tif_dir.c", 0x367);
    tif->td_colormap[0] = 0;
  }
  if (tif->td_colormap[1] != 0) {
    debug_free(tif->td_colormap[1],
               "c:\\halo\\SOURCE\\bitmaps\\libtiff\\tif_dir.c", 0x368);
    tif->td_colormap[1] = 0;
  }
  if (tif->td_colormap[2] != 0) {
    debug_free(tif->td_colormap[2],
               "c:\\halo\\SOURCE\\bitmaps\\libtiff\\tif_dir.c", 0x369);
    tif->td_colormap[2] = 0;
  }
  if (tif->td_documentname != 0) {
    debug_free(tif->td_documentname,
               "c:\\halo\\SOURCE\\bitmaps\\libtiff\\tif_dir.c", 0x36a);
    tif->td_documentname = 0;
  }
  if (tif->td_artist != 0) {
    debug_free(tif->td_artist, "c:\\halo\\SOURCE\\bitmaps\\libtiff\\tif_dir.c",
               0x36b);
    tif->td_artist = 0;
  }
  if (tif->td_datetime != 0) {
    debug_free(tif->td_datetime,
               "c:\\halo\\SOURCE\\bitmaps\\libtiff\\tif_dir.c", 0x36c);
    tif->td_datetime = 0;
  }
  if (tif->td_hostcomputer != 0) {
    debug_free(tif->td_hostcomputer,
               "c:\\halo\\SOURCE\\bitmaps\\libtiff\\tif_dir.c", 0x36d);
    tif->td_hostcomputer = 0;
  }
  if (tif->td_imagedescription != 0) {
    debug_free(tif->td_imagedescription,
               "c:\\halo\\SOURCE\\bitmaps\\libtiff\\tif_dir.c", 0x36e);
    tif->td_imagedescription = 0;
  }
  if (tif->td_make != 0) {
    debug_free(tif->td_make, "c:\\halo\\SOURCE\\bitmaps\\libtiff\\tif_dir.c",
               0x36f);
    tif->td_make = 0;
  }
  if (tif->td_model != 0) {
    debug_free(tif->td_model, "c:\\halo\\SOURCE\\bitmaps\\libtiff\\tif_dir.c",
               0x370);
    tif->td_model = 0;
  }
  if (tif->td_software != 0) {
    debug_free(tif->td_software,
               "c:\\halo\\SOURCE\\bitmaps\\libtiff\\tif_dir.c", 0x371);
    tif->td_software = 0;
  }
  if (tif->td_pagename != 0) {
    debug_free(tif->td_pagename,
               "c:\\halo\\SOURCE\\bitmaps\\libtiff\\tif_dir.c", 0x372);
    tif->td_pagename = 0;
  }
  if (tif->td_stripoffset != 0) {
    debug_free(tif->td_stripoffset,
               "c:\\halo\\SOURCE\\bitmaps\\libtiff\\tif_dir.c", 0x388);
    tif->td_stripoffset = 0;
  }
  if (tif->td_stripbytecount != 0) {
    debug_free(tif->td_stripbytecount,
               "c:\\halo\\SOURCE\\bitmaps\\libtiff\\tif_dir.c", 0x389);
    tif->td_stripbytecount = 0;
  }
}

/* ---------------------------------------------------------------------------
 * TIFFDefaultDirectory (0x66190) -- upstream libtiff tif_dir.c.
 *
 * Identification: the body is upstream TIFFDefaultDirectory's default-value
 * block, field for field and IN UPSTREAM'S ORDER, followed by the
 * `TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE)` call and the
 * `tif->tif_flags &= ~TIFF_DIRTYDIRECT` that upstream documents as undoing the
 * dirty bit that setup just set. No other libtiff function has that shape.
 *
 * ABI / call evidence:
 *   - One stack parameter, `MOV EDI,dword ptr [EBP + 0x8]` at 0x66196.
 *   - Returns 1: EBX is loaded with 1 at 0x661a9, reused as the immediate for
 *     every `= 1` store, and moved to EAX at 0x661f9.
 *   - `LEA ESI,[EDI + 0x14]` (0x6619e) is upstream's
 *     `TIFFDirectory* td = &tif->tif_dir;`. Every directory store below is
 *     `[ESI + off]`, i.e. handle offset 0x14 + off; this TU models the
 *     directory inline on tiff_t, so the offsets are given at their handle
 *     values.
 *   - csmemset(tif + 0x14, 0, 0xb0) at 0x66199-0x661a4 is
 *     `_TIFFmemset(td, 0, sizeof (*td))`; 0xb0 is this build's sizeof
 *     TIFFDirectory, so the cleared range is 0x14..0xc4 and runs past the last
 *     field this TU has recovered (td_pagename at 0xb0).
 *   - TIFFSetField's three pushes (EBX=1, 0x103, EDI) are scheduled at
 *     0x661ae-0x661b7, BEFORE the stores, and share one `ADD ESP,0x18` with
 *     csmemset's three at 0x661f0. That is MSVC hoisting the pushes, not a
 *     source-level reordering: the call itself is at 0x661eb, after every
 *     store.
 *   - `AND byte ptr [EDI + 0xa],0xfd` at 0x661f3 clears bit 0x02 of the flag
 *     byte, which this TU already names TIFF_DIRTYDIRECT.
 *
 * Store widths, proven one by one (handle offset = ESI offset + 0x14):
 *   0x40 td_fillorder=1, 0x36 td_bitspersample=1, 0x3e td_threshholding=1,
 *   0x42 td_orientation=1, 0x44 td_samplesperpixel=1, 0x46 td_predictor=1
 *     -- all `MOV word ptr [...],BX` (0x661b8-0x661cc), 16-bit.
 *   0x48 td_rowsperstrip, 0x28 td_tilewidth, 0x2c td_tilelength -- all
 *     `MOV dword ptr [...],EAX` with EAX = `OR EAX,0xffffffff` (0x661af),
 *     i.e. upstream's `(uint32) -1`.
 *   0x30 td_tiledepth=1 and 0x24 td_imagedepth=1 -- `MOV dword ptr [...],EBX`,
 *     32-bit, matching the widths this TU already recovered for both.
 *   0x5c td_resolutionunit=2 and 0x38 td_sampleformat=4 -- 16-bit immediate
 *     stores (0x661dc, 0x661e2).
 *
 * Bungie deviations from upstream: the ycbcrsubsampling / ycbcrpositioning /
 * inkset / ninks / stripbytecountsorted defaults are absent, and there is no
 * _TIFFextender hook call -- the only call between the memset and the return
 * is TIFFSetField.
 * ------------------------------------------------------------------------- */

/* Tag and value immediates carried by this function. */
#define COMPRESSION_NONE 1 /* PUSH EBX (== 1) at 0x661ae */
#define THRESHHOLD_BILEVEL 1 /* MOV word ptr [ESI+0x2a],BX at 0x661bc */
#define RESUNIT_INCH 2 /* MOV word ptr [ESI+0x48],0x2 at 0x661dc */

/* 0x66190 */
int TIFFDefaultDirectory(void *tif_)
{
  tiff_t *tif = (tiff_t *)tif_;

  /* 0x661a4. */
  csmemset(tif->td_fieldsset, 0, 0xb0);
  /* 0x661b8-0x661e8, in the original's store order. */
  tif->td_fillorder = FILLORDER_MSB2LSB;
  tif->td_bitspersample = 1;
  tif->td_threshholding = THRESHHOLD_BILEVEL;
  tif->td_orientation = ORIENTATION_TOPLEFT;
  tif->td_samplesperpixel = 1;
  tif->td_predictor = 1;
  tif->td_rowsperstrip = (unsigned long)-1;
  tif->td_tilewidth = (long)-1;
  tif->td_tilelength = (long)-1;
  tif->td_tiledepth = 1;
  tif->td_resolutionunit = RESUNIT_INCH;
  tif->td_sampleformat = SAMPLEFORMAT_VOID;
  tif->td_imagedepth = 1;
  /* 0x661eb: return value discarded. */
  TIFFSetField((int)tif_, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
  /* 0x661f3. */
  tif->field_0a &= ~TIFF_DIRTYDIRECT;
  /* 0x661f9. */
  return 1;
}

/* ---------------------------------------------------------------------------
 * TIFFSetDirectory (0x66200) -- upstream libtiff tif_dir.c, with the static
 * TIFFAdvanceDirectory helper inlined into the loop.
 *
 * Evidence from 0x66200-0x66315:
 *   - Two stack arguments: the handle (`MOV ESI,[EBP+0x8]`) and the directory
 *     index (`MOV EDI,[EBP+0xc]`), compared signed (`TEST EDI,EDI` / `JLE`).
 *   - Returns FUN_00066e70(tif) on success and 0 (`XOR EAX,EAX`) on both error
 *     arms; the kb decl `void (void)` did not match the binary.
 *   - Directory entries are 12 bytes: `LEA ECX,[EAX+EAX*2]` / `SHL ECX,2` on
 * the zero-extended count before the SEEK_CUR (`PUSH 1`) lseek at 0x66283,
 *     whose return is discarded.
 *   - Byte swaps run only when bit 0x10 of field_0a is set (0x6625f, 0x662a0).
 *   - Both error messages use module 0x2c9a20 ("TIFFSetDirectory") and pass
 *     `[ESI]` (tif_name).
 * ------------------------------------------------------------------------- */
/* 0x66200 */
int TIFFSetDirectory(void *tif_, int dirn)
{
  tiff_t *tif = (tiff_t *)tif_;
  unsigned long nextdir;
  unsigned short dircount;
  int n;

  nextdir = tif->tiff_diroff;
  for (n = dirn; n > 0 && nextdir != 0; n--) {
    /* 0x66228-0x66259. */
    if (__lseek(tif->tif_fd, (long)nextdir, 0) != (long)nextdir ||
        __read(tif->tif_fd, &dircount, sizeof(unsigned short)) != 2) {
      FUN_00068a30("TIFFSetDirectory", "%s: Error fetching directory count",
                   tif->tif_name);
      return 0;
    }
    if ((tif->field_0a & 0x10) != 0) {
      FUN_0006f1b0(&dircount);
    }
    /* 0x66283: SEEK_CUR past the entries, return value discarded. */
    __lseek(tif->tif_fd, (long)(dircount * 12), 1);
    if (__read(tif->tif_fd, &nextdir, sizeof(unsigned long)) != 4) {
      FUN_00068a30("TIFFSetDirectory", "%s: Error fetching directory link",
                   tif->tif_name);
      return 0;
    }
    if ((tif->field_0a & 0x10) != 0) {
      FUN_0006f1d0(&nextdir);
    }
  }
  /* 0x662be-0x662cb. */
  tif->tif_nextdiroff = nextdir;
  tif->tif_curdir = (unsigned long)((dirn - n) - 1);
  return FUN_00066e70(tif);
}

/* ---------------------------------------------------------------------------
 * TIFFFindFieldInfo (0x66320) -- upstream libtiff's tag lookup, reduced by
 * Bungie to a single static descriptor table with no TIFF handle argument.
 *
 * Confirmed from 0x66320-0x66378:
 *   - Two cdecl stack arguments and nothing else: the tag is read 16 bits wide
 *     (`MOV SI,word ptr [EBP+0x8]` at 0x6632e) and the data type as a dword
 *     (`MOV EDX,dword ptr [EBP+0xc]` at 0x6632a). kb.json keeps both slots
 *     declared `int`; the narrowing is done in the body, as the original does.
 *   - 0x3340ac is the one-entry lookup cache. Upstream spells it as a
 *     function-local `static const TIFFFieldInfo *last`; here it has its own
 *     data address, so it is declared as a global.
 *   - The cache hit path (0x66323-0x66340) tests pointer, tag and type and
 *     returns the cached row unchanged in EAX (0x6636e).
 *   - The scan walks 0x2c9a98 with stride 0x14 (`ADD EAX,0x14` at 0x66364)
 *     and stops on a zero tag (`TEST CX,CX` at 0x66348 and 0x66367), i.e.
 *     upstream's `for (fip = table; fip->field_tag; fip++)`. The table in the
 *     pristine image holds 70 rows plus that zero terminator.
 *   - The two type comparisons keep upstream's asymmetric operand order:
 *     `CMP EDX,[EAX+0x8]` on the cache path (`dt == last->field_type`) and
 *     `CMP [EAX+0x8],EDX` in the loop (`fip->field_type == dt`).
 *   - A hit stores the row back into the cache (0x66371) and returns it; a
 *     dry scan returns NULL (`XOR EAX,EAX` at 0x6636c).
 * ------------------------------------------------------------------------- */

/* Upstream's wildcard type, the value every call site in this TU passes. */
#define TIFF_ANY TIFF_NOTYPE

/* 0x66320 */
void *TIFFFindFieldInfo(int tag, int dt)
{
  unsigned short t;
  tiff_field_info_t *fip;

  /* 0x6632e: the original only ever compares the low word of the slot. */
  t = (unsigned short)tag;
  /* 0x66323-0x66340. */
  fip = (tiff_field_info_t *)tiff_find_field_info_last;
  if (fip != NULL && fip->field_tag == t &&
      (dt == TIFF_ANY || dt == fip->field_type))
    return fip;
  /* 0x66342-0x6636a. */
  for (fip = (tiff_field_info_t *)tiffFieldInfo; fip->field_tag != 0; fip++) {
    if (fip->field_tag == t && (dt == TIFF_ANY || fip->field_type == dt)) {
      /* 0x66371. */
      tiff_find_field_info_last = fip;
      return fip;
    }
  }
  /* 0x6636c. */
  return NULL;
}

/* ---------------------------------------------------------------------------
 * FUN_00066380 (0x66380) -- upstream libtiff's _TIFFFieldWithTag, with the
 * TIFFFindFieldInfo(tag, TIFF_ANY) lookup expanded in place (the body has no
 * CALL to 0x66320; the wildcard type makes both type tests vanish).
 *
 * Confirmed from 0x66380-0x663e1:
 *   - One cdecl stack argument read 16 bits wide (`MOV DX,word ptr [EBP+0x8]`
 *     at 0x6638a).
 *   - Cache test on 0x3340ac (0x66383-0x66393), then the 0x2c9a98 table walk
 *     with stride 0x14 ending on a zero tag (0x66395-0x663b5). A table hit
 *     stores the row back into the cache (0x663b7).
 *   - Miss: `MOVZX EAX,DX`, PUSH 0x25fabc (format), PUSH 0x25faa8 (module),
 *     CALL 0x68a30, `ADD ESP,0xc`, then `PUSH -0x1; CALL 0x1d980b`.
 *   - Hit: the row is returned in EAX (0x663e0).
 * ------------------------------------------------------------------------- */

/* 0x66380 */
void *FUN_00066380(unsigned short tag)
{
  tiff_field_info_t *fip;
  tiff_field_info_t *p;

  /* 0x66383-0x66393. */
  fip = (tiff_field_info_t *)tiff_find_field_info_last;
  if (fip == NULL || fip->field_tag != tag) {
    fip = NULL;
    /* 0x66395-0x663b5. */
    for (p = (tiff_field_info_t *)tiffFieldInfo; p->field_tag != 0; p++) {
      if (p->field_tag == tag) {
        /* 0x663b7. */
        tiff_find_field_info_last = p;
        fip = p;
        break;
      }
    }
  }
  /* 0x663bf-0x663e0. */
  if (fip == NULL) {
    FUN_00068a30("TIFFFieldWithTag", "Internal error, unknown tag 0x%x",
                 (unsigned int)tag);
    FUN_001d980b(-1);
  }
  return fip;
}

/* ---------------------------------------------------------------------------
 * CheckMalloc (0x663f0) -- upstream libtiff's directory-read allocation
 * helper, reshaped by Bungie to return the block instead of a boolean.
 *
 * Confirmed from 0x663f0-0x66427:
 *   - Size arrives in EAX and is never written before `PUSH EAX` at 0x663fd,
 *     so it is an incoming register argument (@<eax>). The two stack
 *     parameters are `[EBP+0x8]` (the TIFF handle, dereferenced at 0x66412 as
 *     `MOV EAX,dword ptr [EDX]` == tif_name) and `[EBP+0xc]` (the `what`
 *     string forwarded as the `%s` argument).
 *   - debug_malloc pushes at 0x663f4-0x663fd are line 0x60, file
 *     "c:\halo\SOURCE\bitmaps\libtiff\tif_dirread.c", zero-flag 0, size EAX;
 *     `ADD ESP,0x10` confirms four cdecl arguments. The __FILE__ string is
 *     tif_dirread.c, so the function's real translation unit is tif_dirread;
 *     kb.json groups it under tif_dir.obj, which is why the body lives here.
 *   - The result is parked in ESI (0x66403) and returned unchanged in EAX
 *     (0x66423) on both paths -- the error arm falls through to the same
 *     `MOV EAX,ESI`, so a failed allocation returns NULL rather than a flag.
 *   - Error arm: `PUSH ECX` (what), `PUSH 0x25fae0` ("No space %s"),
 *     `PUSH EAX` (tif->tif_name), `CALL 0x68a30`, `ADD ESP,0xc` -- three
 *     cdecl arguments, module-first, same as every other TIFFError site here.
 */

/* 0x663f0 */
void *CheckMalloc(uint32_t size, void *tif_, const char *what)
{
  tiff_t *tif = (tiff_t *)tif_;
  void *cp;

  /* 0x663f4-0x66405. */
  cp = debug_malloc(size, 0,
                    "c:\\halo\\SOURCE\\bitmaps\\libtiff\\tif_dirread.c", 0x60);
  /* 0x66408-0x66420. */
  if (cp == 0) {
    FUN_00068a30(tif->tif_name, "No space %s", what);
  }
  /* 0x66423. */
  return cp;
}

/* ---------------------------------------------------------------------------
 * EstimateStripByteCounts (0x66430) -- upstream libtiff tif_dirread.c helper
 * that invents a StripByteCounts value for a directory that omitted the tag.
 *
 * ABI, from 0x66430-0x6651c: three arguments, two of them in registers.
 *   - EAX holds the directory-entry count. It is copied to EDI at 0x66440
 *     before anything writes EAX, and EDI is both the `n*12` multiplicand
 *     (0x66476) and the loop counter (`TEST EDI,EDI` / `JLE` at 0x66489, so a
 *     SIGNED 32-bit count) -- an incoming register argument (@<eax>).
 *   - ESI is never written and is the TIFF handle: `[ESI]` is the module
 *     string pushed to TIFFError, and every directory field is read off it
 *     (@<esi>).
 *   - `[EBP+0x8]` (0x6648d) is the directory-entry array; the only stack
 *     argument, and the frame keeps no other.
 *
 * Confirmed details:
 *   - CheckMalloc (0x663f0) is INLINED here, not called: the four debug_malloc
 *     pushes at 0x66435-0x66442 are that helper's own (size 4, zero-flag 0,
 *     file "...tif_dirread.c", line 0x60), and the NULL arm at 0x66450-0x66462
 *     is its TIFFError verbatim, with the "for \"StripByteCounts\" array"
 *     string it is always called with. Calling CheckMalloc from here would add
 *     a CALL the original does not have.
 *   - The estimate is for ONE strip only. Upstream divides by
 *     td_samplesperpixel for PLANARCONFIG_SEPARATE and loops over td_nstrips;
 *     neither survives here -- there is no division and no second store, only
 *     `[0]`.
 *   - `space` = 8 (TIFFHeader) + 2 (dir count) + nDirEntries*12 + 4 (next-dir
 *     offset) == the `LEA EBX,[EDI+EDI*2]` / `LEA EBX,[EBX*4+0xe]` pair at
 *     0x66476/0x6647a.
 *   - The indirect-value sweep compares `cc` against 4 with JBE (0x664ae), so
 *     the threshold test is UNSIGNED, matching upstream's `cc >
 * sizeof(uint32)`.
 *   - The final clamp compares `stripoffset[0] + stripbytecount[0]` against
 *     the file size with JBE (0x664db): also unsigned.
 * ------------------------------------------------------------------------- */

/* One entry of the on-disk directory, 12 bytes: the loop strides by 0xc
 * (0x664b5) and reads a 16-bit type at +0x2 (`MOVZX ECX,word ptr [EDX+-0x2]`
 * against a cursor primed to base+4) and a 32-bit count at +0x4. The tag at
 * +0x0 and the value/offset at +0x8 are never touched here. */
typedef struct tiff_dir_entry_s {
  unsigned short tdir_tag; /* 0x00 */
  unsigned short tdir_type; /* 0x02 */
  unsigned long tdir_count; /* 0x04 */
  unsigned long tdir_offset; /* 0x08 */
} tiff_dir_entry_t;

/* Per-type byte width, indexed by tdir_type: `MOV ECX,dword ptr
 * [ECX*0x4 + 0x2ca024]` at 0x664a4. Upstream calls this table tiffDataWidth. */
#define tiffDataWidth ((const long *)0x2ca024)

/* Bits of td_fieldsset[0] carried by this function. */
#define FIELD_ROWSPERSTRIP_BIT 0x00020000 /* TEST EAX,0x20000 at 0x66509 */
#define FIELD_STRIPBYTECOUNTS_BIT 0x04000000 /* OR ECX,0x4000000 at 0x66501 */

/* 0x66430 */
void EstimateStripByteCounts(int nDirEntries, void *tif_, void *dir_)
{
  tiff_t *tif = (tiff_t *)tif_;
  const tiff_dir_entry_t *dp;
  unsigned long *cp;
  unsigned long space;
  unsigned long filesize;
  unsigned long cc;
  unsigned long rowbytes;
  int n;

  /* 0x66435-0x66462: CheckMalloc, inlined. */
  cp = (unsigned long *)debug_malloc(
    4, 0, "c:\\halo\\SOURCE\\bitmaps\\libtiff\\tif_dirread.c", 0x60);
  if (cp == 0) {
    FUN_00068a30(tif->tif_name, "No space %s", "for \"StripByteCounts\" array");
  }
  /* 0x6646a. */
  tif->td_stripbytecount = cp;
  /* 0x66465. */
  if (tif->td_compression != COMPRESSION_NONE) {
    /* 0x66476-0x6647a. */
    space = (unsigned long)(8 + 2 + nDirEntries * 12 + 4);
    /* 0x66472-0x66486. */
    filesize = (unsigned long)TIFFGetFileSize((int)tif->tif_fd);
    /* 0x66489-0x664b9: amount of space used by indirect values. */
    for (dp = (const tiff_dir_entry_t *)dir_, n = nDirEntries; n > 0;
         n--, dp++) {
      cc = (unsigned long)tiffDataWidth[dp->tdir_type] * dp->tdir_count;
      if (cc > 4) {
        space += cc;
      }
    }
    /* 0x664bb-0x664c5. */
    tif->td_stripbytecount[0] = filesize - space;
    /* 0x664c7-0x664df: the last strip must not run past the end of file. */
    if (tif->td_stripoffset[0] + tif->td_stripbytecount[0] > filesize) {
      tif->td_stripbytecount[0] = filesize - tif->td_stripoffset[0];
    }
  } else {
    /* 0x664e3-0x664fc. */
    rowbytes = ((unsigned long)tif->td_samplesperpixel * tif->td_bitspersample *
                  tif->td_imagewidth +
                7) /
               8;
    tif->td_stripbytecount[0] = tif->td_imagelength * rowbytes;
  }
  /* 0x664fe-0x66518. */
  tif->td_fieldsset[0] |= FIELD_STRIPBYTECOUNTS_BIT;
  if ((tif->td_fieldsset[0] & FIELD_ROWSPERSTRIP_BIT) == 0) {
    tif->td_rowsperstrip = tif->td_imagelength;
  }
}

/* Directory-entry type codes, as the 0x665c1-0x665d0 jump table groups them:
 * `type - 3` bounded by `CMP EAX,0x8 / JA default`, so 3..11 only. The three
 * arms are byte-swap widths, which fixes the upstream libtiff identity of each
 * code (2-byte, 4-byte, 4-byte-pair). */
#define TIFF_SHORT 3
#define TIFF_LONG 4
#define TIFF_RATIONAL 5
#define TIFF_SSHORT 8
#define TIFF_SLONG 9
#define TIFF_SRATIONAL 10
#define TIFF_FLOAT 11

/* `TEST byte ptr [EBX+0xa],0x10` at 0x665b7 gates the byte-swap arms, so bit 4
 * of field_0a is this build's "file byte order differs from host" flag --
 * upstream's TIFF_SWAB, at a different value than upstream's own numbering. */
#define TIFF_SWAB_BIT 0x10

/* 0x66550
 *
 * ABI: three arguments, two in registers.
 *   - EBX is never written and is the TIFF handle: `[EBX]` is the module string
 *     pushed to TIFFError (0x665a1), `[EBX+0x4]` the file handle, `[EBX+0xa]`
 *     the flag byte (@<ebx>).
 *   - ESI is never written and is the directory entry: +0x0 tag, +0x2 type,
 *     +0x4 count, +0x8 offset (@<esi>).
 *   - `[EBP+0x8]` (0x6657d) is the destination buffer; the only stack argument.
 * Returns EAX: `cc` on every success path (`MOV EAX,EDI` at 0x665e7/0x665fc/
 * 0x66613) and 0 on the error path (`XOR EAX,EAX` at 0x665b2).
 *
 * Confirmed details:
 *   - The two I/O calls go straight to the CRT (0x1e24d2 lseek, 0x1e209e read),
 *     not through upstream's tif_seekproc/tif_readproc indirection, and there
 *     is no memory-mapped arm at all -- this build compiled the unmapped path
 *     only.
 *   - `cc` is computed BEFORE the seek (0x6655f-0x66566) and the file handle is
 *     re-loaded for each call (`MOVSX` at 0x6655a and again at 0x66580), as is
 *     tdir_offset for the comparison (0x66573).
 *   - The error arm's tag lookup at 0x66599 has NO `ADD ESP` of its own: its
 *     one pushed argument is cleaned by the `ADD ESP,0x10` at 0x665af together
 *     with TIFFError's three. TIFFError therefore takes three arguments here,
 *     not four.
 *   - TIFF_DOUBLE never appears: the switch range stops at type 11, so this
 *     build has no TIFFSwabArrayOfDouble arm.
 * ------------------------------------------------------------------------- */
long TIFFFetchData(void *tif_, void *dp_, char *cp)
{
  tiff_t *tif = (tiff_t *)tif_;
  const tiff_dir_entry_t *dp = (const tiff_dir_entry_t *)dp_;
  long cc;

  /* 0x66553-0x66566. */
  cc = tiffDataWidth[dp->tdir_type] * dp->tdir_count;
  /* 0x6656a-0x66591: seek to the indirect value and read it whole. */
  if (__lseek(tif->tif_fd, (long)dp->tdir_offset, 0) != (long)dp->tdir_offset ||
      __read(tif->tif_fd, cp, (unsigned int)cc) != (int)cc) {
    /* 0x66593-0x665b6. */
    FUN_00068a30(tif->tif_name, "Error fetching data for field \"%s\"",
                 ((tiff_field_info_t *)FUN_00066380(dp->tdir_tag))->field_name);
    return 0;
  }
  /* 0x665b7-0x66610. */
  if ((tif->field_0a & TIFF_SWAB_BIT) != 0) {
    switch (dp->tdir_type) {
    case TIFF_SHORT:
    case TIFF_SSHORT:
      FUN_0006f1f0(cp, (int)dp->tdir_count);
      break;
    case TIFF_LONG:
    case TIFF_SLONG:
    case TIFF_FLOAT:
      FUN_0006f220(cp, (int)dp->tdir_count);
      break;
    case TIFF_RATIONAL:
    case TIFF_SRATIONAL:
      FUN_0006f220(cp, (int)(2 * dp->tdir_count));
      break;
    default:
      break;
    }
  }
  /* 0x66613. */
  return cc;
}

/* 0x66640
 *
 * ABI: three arguments, two in registers plus one on the stack.
 *   - ECX is the TIFF handle (`MOV EBX,ECX` at 0x6664c, then
 *     `TEST byte ptr [EBX+0xa],0x10` at 0x66656) (@<ecx>).
 *   - EAX is the directory entry (`MOV ESI,EAX` at 0x66646; +0x4 count,
 *     +0x8 offset) (@<eax>).
 *   - EDI is the destination buffer: pushed unchanged as the last csmemcpy
 *     argument at 0x66670 and as TIFFFetchData's stack argument at 0x66684
 *     (@<edi>).
 *   Returns EAX: `MOV EAX,0x1` at 0x6667a on the inline arm; the out-of-line
 *   arm falls through with TIFFFetchData's own EAX (0x66685, no reload).
 *
 * Confirmed details:
 *   - `CMP dword ptr [ESI+0x4],0x4 / JA` (0x66648): counts of four bytes or
 *     fewer live inline in tdir_offset; anything larger is fetched indirectly.
 *   - The inline value is copied to `[EBP-0x4]` first (0x66650-0x66653) and the
 *     byte-swap helper takes the address of that copy (`LEA ECX,[EBP-0x4]`),
 *     so the swap is applied to the local, never to the directory entry.
 *   - The copy length is re-loaded from `[ESI+0x4]` at 0x66668, after the
 *     optional swap call. */
long TIFFFetchString(void *tif_, void *dp_, char *cp)
{
  tiff_t *tif = (tiff_t *)tif_;
  const tiff_dir_entry_t *dp = (const tiff_dir_entry_t *)dp_;
  unsigned long l;

  /* 0x66648-0x6664e. */
  if (dp->tdir_count <= 4) {
    /* 0x66650-0x66665. */
    l = dp->tdir_offset;
    if ((tif->field_0a & TIFF_SWAB_BIT) != 0) {
      FUN_0006f1d0(&l);
    }
    /* 0x66668-0x6667a. */
    csmemcpy(cp, &l, dp->tdir_count);
    return 1;
  }
  /* 0x66684-0x66685. */
  return TIFFFetchData(tif, (void *)dp, cp);
}

/* 0x666a0
 *
 * ABI: EDX is the directory entry (`MOV AX,word ptr [EDX]` at 0x666af,
 * `CMP word ptr [EDX+0x2],0x5` at 0x666d7) (@<edx>); ESI is the result
 * pointer (`FSTP float ptr [ESI]` at 0x66705/0x66714) (@<esi>). */
long cvtRational(void *tif, void *dir, unsigned long num, unsigned long denom,
                 float *rv)
{
  const tiff_dir_entry_t *dp = (const tiff_dir_entry_t *)dir;

  if (denom == 0) {
    FUN_00068a30(((tiff_t *)tif)->tif_name,
                 "%s: Rational with zero denominator (num = %lu)",
                 ((tiff_field_info_t *)FUN_00066380(dp->tdir_tag))->field_name,
                 num);
    return 0;
  }
  if (dp->tdir_type == TIFF_RATIONAL) {
    *rv = (float)num / (float)denom;
  } else {
    *rv = (float)(long)num / (float)(long)denom;
  }
  return 1;
}

/* 0x66720
 *
 * ABI: two register arguments, no stack arguments.
 *   - EAX is the TIFF handle: `MOV EBX,EAX` at 0x66727, and EBX is then
 *     TIFFFetchData's @<ebx> argument at 0x66730 and cvtRational's first
 *     stack argument (`PUSH EBX`, 0x66744) (@<eax>).
 *   - EDI is the directory entry: `MOV ESI,EDI` at 0x6672e supplies
 *     TIFFFetchData's @<esi> argument, and `MOV EDX,EDI` at 0x66748 supplies
 *     cvtRational's @<edx> argument (@<edi>).
 *   Returns ST(0): `FLD dword ptr [EBP-0x4]` on the success arm (0x66756) and
 *   `FLD qword ptr [0x2573d8]` (1.0) on both failure arms (0x6675f).
 *
 * Confirmed details:
 *   - The frame is `SUB ESP,0xc`: the two-word fetch buffer lives at
 *     [EBP-0xc]/[EBP-0x8] (its address is the only pushed argument to
 *     TIFFFetchData at 0x6672a) and the float result at [EBP-0x4] (address
 *     taken by `LEA ESI,[EBP-0x4]` at 0x66745).
 *   - cvtRational takes five arguments: `ADD ESP,0xc` at 0x6674f covers the
 *     three pushes (numerator [EBP-0xc] then denominator [EBP-0x8] are pushed
 *     in reverse order at 0x66742/0x66743, the handle at 0x66744); EDX and ESI
 *     carry the remaining two.
 *   - Both failure tests are `TEST EAX,EAX / JZ 0x6675f`, so the two arms share
 *     one epilogue and the buffer is never re-read on failure. */
float TIFFFetchRational(void *tif_, void *dp_)
{
  unsigned long l[2];
  float v;

  /* 0x66726-0x6673a. */
  if (TIFFFetchData(tif_, dp_, (char *)l) != 0) {
    /* 0x6673c-0x66754. */
    if (cvtRational(tif_, dp_, l[0], l[1], &v) != 0) {
      /* 0x66756. */
      return v;
    }
  }
  /* 0x6675f. */
  return 1.0;
}

/* Magic of a big-endian ("MM") TIFF file, the immediate compared at 0x6677b.
 * tif_open.c defines the same constant from TIFFFdOpen's own comparisons. */
#define TIFF_BIGENDIAN_MAGIC 0x4d4d

/* Pull a directory entry's inline value out of tdir_offset. Upstream libtiff
 * spells this as the TIFFExtractData macro in tiffiop.h; both arms are present
 * verbatim here (0x6677b-0x667b2): on a big-endian file the value is first
 * shifted right by tif_typeshift[type] and then masked, otherwise it is only
 * masked. The shift is SHR (unsigned) at 0x66796. */
#define TIFFExtractData(tif, type, v)                          \
  ((unsigned long)((tif)->tiff_magic == TIFF_BIGENDIAN_MAGIC ? \
                     ((v) >> (tif)->tif_typeshift[type]) &     \
                       (tif)->tif_typemask[type] :             \
                     (v) & (tif)->tif_typemask[type]))

/* 0x66770
 *
 * ABI: two register arguments, no stack arguments.
 *   - EAX is the TIFF handle: `MOV ESI,EAX` at 0x66779, and ESI is the base of
 *     every handle load that follows (+0xc4, +0xcc, +0xd0) (@<eax>).
 *   - ECX is the directory entry: `MOVZX EDX,word ptr [ECX+0x2]` (tdir_type) at
 *     0x66774 and `MOV EAX,dword ptr [ECX+0x8]` (tdir_offset) at 0x66786 /
 *     0x667af (@<ecx>).
 *   Returns ST(0): `FILD dword ptr [EBP-0x4]` at 0x667b7 with no FSTP before
 *   the epilogue.
 *
 * Confirmed details:
 *   - The type index is scaled by four on both arms: `SHL EDX,0x2` at 0x66790
 *     followed by `[EDI+EDX*0x1]` on the big-endian arm, and an unscaled EDX
 *     with `[EAX+EDX*0x4]` on the little-endian arm -- the same C subscript.
 *   - There is no call. Upstream's TIFFCvtIEEEFloatToNative is a no-op on an
 *     IEEE host and compiled away; nothing in the range 0x66770-0x667c6 is an
 *     E8/E9.
 *   - `TEST EAX,EAX / MOV [EBP-0x4],EAX / FILD / JGE / FADD [0x25fb8c]` is the
 *     unsigned-32-to-float conversion MSVC emits for `(float)(unsigned long)x`
 *     (the constant at 0x25fb8c is 4294967296.0), not a source-level branch. */
float TIFFFetchFloat(void *tif_, void *dp_)
{
  tiff_t *tif = (tiff_t *)tif_;
  const tiff_dir_entry_t *dp = (const tiff_dir_entry_t *)dp_;

  /* 0x66774-0x667c6. */
  return (float)TIFFExtractData(tif, dp->tdir_type, dp->tdir_offset);
}

/* 0x667d0
 *
 * ABI: three register arguments, no stack arguments.
 *   - ECX is the directory entry: `MOV ESI,ECX` at 0x667d2, then `[ESI+0x4]`
 *     (tdir_count, 0x667d4) and the inline value bytes `[ESI+0x8]`..`[ESI+0xb]`
 *     (@<ecx>).
 *   - EDX is the TIFF handle: `MOV EBX,EDX` at 0x667da, then
 *     `CMP word ptr [EBX+0xc4],0x4d4d` at 0x667e3 (@<edx>).
 *   - EAX is the destination array, never reloaded: `MOV word ptr [EAX+0x6]`,
 *     `[EAX+0x4]`, `[EAX+0x2]`, `[EAX]`, and the single `PUSH EAX` at 0x66868
 *     that is TIFFFetchData's stack argument (@<eax>).
 *   Returns EAX: `MOV EAX,0x1` at 0x66824 / 0x66861 on the inline arms; the
 *   out-of-line arm falls through with TIFFFetchData's own EAX (0x6686e, no
 *   reload).
 *
 * Confirmed details:
 *   - `CMP ECX,0x4 / JA 0x00066868` (0x667d7): counts above four go to
 *     TIFFFetchData, exactly as in TIFFFetchString.
 *   - `DEC ECX / CMP ECX,0x3 / JA <ret 1> / JMP [ECX*4 + jumptable]` is the
 *     switch on tdir_count with cases 1..4; the jump tables live at 0x66874
 *     (big-endian arm) and 0x66884, and both are fall-through chains, so no
 *     `break` appears between cases.
 *   - The byte offsets run in opposite directions between the two arms:
 *     big-endian takes v[3] from `[ESI+0x8]` down to v[0] from `[ESI+0xb]`,
 *     little-endian the reverse -- upstream's shift/mask pair on tdir_offset,
 *     which MSVC folds into the single byte loads seen here.
 *   - The magic test is the same `word ptr [handle+0xc4] == 0x4d4d` used by
 *     TIFFFetchFloat at 0x6677b. */
long TIFFFetchByteArray(void *tif_, void *dp_, unsigned short *v)
{
  tiff_t *tif = (tiff_t *)tif_;
  const tiff_dir_entry_t *dp = (const tiff_dir_entry_t *)dp_;

  /* 0x667d4-0x667dc. */
  if (dp->tdir_count <= 4) {
    /* 0x667e3-0x667ec. */
    if (tif->tiff_magic == TIFF_BIGENDIAN_MAGIC) {
      /* 0x667f3-0x66821. */
      switch (dp->tdir_count) {
      case 4:
        v[3] = (unsigned short)(dp->tdir_offset & 0xff);
      case 3:
        v[2] = (unsigned short)((dp->tdir_offset >> 8) & 0xff);
      case 2:
        v[1] = (unsigned short)((dp->tdir_offset >> 16) & 0xff);
      case 1:
        v[0] = (unsigned short)(dp->tdir_offset >> 24);
      }
    } else {
      /* 0x66830-0x6685d. */
      switch (dp->tdir_count) {
      case 4:
        v[3] = (unsigned short)(dp->tdir_offset >> 24);
      case 3:
        v[2] = (unsigned short)((dp->tdir_offset >> 16) & 0xff);
      case 2:
        v[1] = (unsigned short)((dp->tdir_offset >> 8) & 0xff);
      case 1:
        v[0] = (unsigned short)(dp->tdir_offset & 0xff);
      }
    }
    /* 0x66824 / 0x66861. */
    return 1;
  }
  /* 0x66868-0x6686e. */
  return TIFFFetchData(tif, (void *)dp, (char *)v);
}

/* TIFFFetchShortArray, 0x668a0. Same shape as TIFFFetchByteArray one entry
 * earlier, for SHORT values: up to two 16-bit values live inline in
 * tdir_offset, anything larger is fetched indirectly.
 *
 * ABI: three register arguments, no stack arguments.
 *   - EAX is the directory entry: `MOV ESI,EAX` at 0x668a2, then `[ESI+0x4]`
 *     (tdir_count, 0x668a4) and the inline halves `[ESI+0x8]` / `[ESI+0xa]`
 *     (@<eax>).
 *   - EDX is the TIFF handle: `MOV EBX,EDX` at 0x668aa, then
 *     `CMP word ptr [EBX+0xc4],0x4d4d` at 0x668ae (@<edx>).
 *   - ECX is the destination array, never reloaded: `MOV word ptr [ECX+0x2]`,
 *     `MOV word ptr [ECX]`, and the single `PUSH ECX` at 0x668f3 that is
 *     TIFFFetchData's stack argument (@<ecx>).
 *   Returns EAX: `MOV EAX,0x1` at 0x668cf / 0x668ec on the inline arms; the
 *   out-of-line arm falls through with TIFFFetchData's own EAX (0x668f4, no
 *   reload or TEST, so the result is returned directly).
 *
 * Confirmed details:
 *   - `CMP EAX,0x2 / JA 0x000668f3` (0x668a7): counts above two go to
 *     TIFFFetchData; the magic test is only reached for counts 0..2.
 *   - Each arm is `DEC EAX / JZ <v[0] store> / DEC EAX / JNZ <ret 1>`, a
 *     fall-through chain of cases 2 and 1 (count 0 stores nothing and still
 *     returns 1), so no `break` appears between cases.
 *   - The halves run in opposite directions between the two arms: big-endian
 *     takes v[1] from `[ESI+0x8]` and v[0] from `[ESI+0xa]`, little-endian the
 *     reverse -- upstream's shift/mask pair on tdir_offset, which MSVC folds
 *     into the single 16-bit loads seen here. */
long TIFFFetchShortArray(void *tif_, void *dp_, unsigned short *v)
{
  tiff_t *tif = (tiff_t *)tif_;
  const tiff_dir_entry_t *dp = (const tiff_dir_entry_t *)dp_;

  /* 0x668a4-0x668ac. */
  if (dp->tdir_count <= 2) {
    /* 0x668ae-0x668b7. */
    if (tif->tiff_magic == TIFF_BIGENDIAN_MAGIC) {
      /* 0x668b9-0x668cc. */
      switch (dp->tdir_count) {
      case 2:
        v[1] = (unsigned short)(dp->tdir_offset & 0xffff);
      case 1:
        v[0] = (unsigned short)(dp->tdir_offset >> 16);
      }
    } else {
      /* 0x668d6-0x668e8. */
      switch (dp->tdir_count) {
      case 2:
        v[1] = (unsigned short)(dp->tdir_offset >> 16);
      case 1:
        v[0] = (unsigned short)(dp->tdir_offset & 0xffff);
      }
    }
    /* 0x668cf / 0x668ec. */
    return 1;
  }
  /* 0x668f3-0x668f9. */
  return TIFFFetchData(tif, (void *)dp, (char *)v);
}

/* TIFFFetchLongArray, 0x66900. The LONG counterpart of TIFFFetchShortArray:
 * a single 32-bit value lives inline in tdir_offset, anything else is fetched
 * indirectly. No byte-order test is needed because the inline value is already
 * a whole dword.
 *
 * ABI: three register arguments, no stack arguments.
 *   - ECX is the directory entry: `MOV ESI,ECX` at 0x66901, then `[ESI+0x4]`
 *     (tdir_count, 0x66903) and `[ESI+0x8]` (tdir_offset, 0x66909). ESI is
 *     also TIFFFetchData's @<esi> argument at the 0x66916 call, unchanged
 *     (@<ecx>).
 *   - EBX is the TIFF handle: never written in this function, yet it is
 *     TIFFFetchData's @<ebx> argument, so it arrives live from the caller
 *     (@<ebx>).
 *   - EAX is the destination array: `MOV dword ptr [EAX],ECX` at 0x6690c and
 *     the single `PUSH EAX` at 0x66915 that is TIFFFetchData's stack argument
 *     (@<eax>).
 *   Returns EAX: `MOV EAX,0x1` at 0x6690e on the inline arm; the out-of-line
 *   arm falls through with TIFFFetchData's own EAX (0x6691b-0x6691f is only
 *   `ADD ESP,0x4 / POP ESI / RET`, no reload or TEST, so the result is
 *   returned directly).
 *
 * Confirmed details:
 *   - `CMP dword ptr [ESI+0x4],0x1 / JNZ` (0x66903): only an exact count of 1
 *     takes the inline path; count 0 goes to TIFFFetchData as well. */
long TIFFFetchLongArray(void *tif_, void *dp_, unsigned long *lp)
{
  const tiff_dir_entry_t *dp = (const tiff_dir_entry_t *)dp_;

  /* 0x66903-0x66914. */
  if (dp->tdir_count == 1) {
    lp[0] = dp->tdir_offset;
    return 1;
  }
  /* 0x66915-0x6691f. */
  return TIFFFetchData(tif_, (void *)dp, (char *)lp);
}

/* TIFFFetchRationalArray, 0x66920. Fetches a RATIONAL array into a float
 * array: the raw numerator/denominator dwords are read into a scratch buffer,
 * then converted one pair at a time by cvtRational.
 *
 * ABI: three cdecl stack arguments, no register arguments.
 *   - [EBP+0x8] is the TIFF handle: `[ECX]` is the module string pushed to
 *     TIFFError at 0x66958 and it is TIFFFetchData's @<ebx> argument
 *     (0x66976) and cvtRational's first stack argument (0x669a4).
 *   - [EBP+0xc] is the directory entry: +0x2 tdir_type (0x66929), +0x4
 *     tdir_count (0x66934 / 0x66989 / 0x669b7); it is TIFFFetchData's @<esi>
 *     argument (0x66926) and cvtRational's @<edx> argument (0x669a1).
 *   - [EBP+0x10] is the destination float array (`MOV ESI,[EBP+0x10]` at
 *     0x66992, `ADD ESI,0x4` per iteration at 0x669bb), carried as
 *     cvtRational's @<esi> argument.
 *   Returns EAX: `MOV EAX,EBX` (zero) at 0x6696f on the allocation-failure
 *   arm, `MOV EAX,[EBP-0x4]` at 0x669d2 otherwise.
 *
 * Confirmed details:
 *   - CheckMalloc is INLINED, as in EstimateStripByteCounts: the debug_malloc
 *     pushes at 0x66939-0x66943 carry file tif_dirread.c and line 0x60, and
 *     the failure arm reports "No space %s" / "to fetch array of rationals"
 *     through TIFFError directly (0x66965).
 *   - The allocation-failure arm returns WITHOUT calling debug_free; the free
 *     at 0x669cd (line 0x345) is reached from every other path, including the
 *     TIFFFetchData-failed arm (`JZ 0x669c2` at 0x66984).
 *   - `ok` lives at [EBP-0x4], zeroed at 0x66944 and written from cvtRational's
 *     EAX at 0x669af on every iteration, so a zero return both breaks the loop
 *     (0x669b2) and becomes the function result.
 *   - The pair subscript is `[EDI+EBX*0x8]` / `[EDI+EBX*0x8+0x4]` (0x66995 /
 *     0x66999), i.e. l[2*i+0] and l[2*i+1]; the denominator is pushed first
 *     (0x6699f) so the argument order is (num, denom).
 *   - tdir_count is re-loaded from the directory entry for the loop test each
 *     iteration (0x669b4-0x669be), not cached. */
long TIFFFetchRationalArray(void *tif_, void *dp_, float *v)
{
  tiff_t *tif = (tiff_t *)tif_;
  const tiff_dir_entry_t *dp = (const tiff_dir_entry_t *)dp_;
  unsigned long *l;
  unsigned long i;
  long ok;

  /* 0x66929-0x66951: CheckMalloc, inlined. */
  ok = 0;
  l = (unsigned long *)debug_malloc(
    (unsigned long)tiffDataWidth[dp->tdir_type] * dp->tdir_count, 0,
    "c:\\halo\\SOURCE\\bitmaps\\libtiff\\tif_dirread.c", 0x60);
  if (l == 0) {
    /* 0x66955-0x66975. */
    FUN_00068a30(tif->tif_name, "No space %s", "to fetch array of rationals");
    return 0;
  }
  /* 0x66976-0x66984. */
  if (TIFFFetchData(tif_, (void *)dp, (char *)l) != 0) {
    /* 0x66986-0x669c0. */
    for (i = 0; i < dp->tdir_count; i++) {
      ok = cvtRational(tif_, (void *)dp, l[2 * i + 0], l[2 * i + 1], &v[i]);
      if (ok == 0) {
        break;
      }
    }
  }
  /* 0x669c2-0x669de. */
  debug_free(l, "c:\\halo\\SOURCE\\bitmaps\\libtiff\\tif_dirread.c", 0x345);
  return ok;
}

/* ---------------------------------------------------------------------------
 * FUN_00066e70 (0x66e70) -- upstream libtiff's TIFFReadDirectory, unmapped-I/O
 * build. Every debug_free names "...\libtiff\tif_dirread.c" (0x194, 0x1fe,
 * 0x212), so like its helpers above it belongs to tif_dirread.c.
 *
 * Confirmed from 0x66e70-0x67689:
 *   - One cdecl argument, kept in EBX. Returns 0 on every failure path
 *     (`XOR EAX,EAX` at 0x66ecc/0x66efd/0x6759e; the empty-directory exit at
 *     0x66e8f reuses the zero just tested) and 1 on success (0x67680).
 *   - The next-directory link is read straight into tif_nextdiroff (`PUSH ESI`
 *     with ESI = EBX+0x10, 0x66f79) and zeroed on a short read (0x66f88).
 *   - First pass (0x66fd1-0x6704a): swab, sorted tiffFieldInfo walk with a
 *     one-shot TIFFWarning, silent drop of unknown tags (no warning call),
 *     FIELD_IGNORE / type-retry / count checks, then setting the strip/tile
 *     field bits and fetching the geometry tags (jump table 0x6768c/0x67698,
 *     plus 0x142-0x145 and 0x80e5-0x80e6). A zero TIFFFetchNormalTag return
 *     is fatal here (0x6717b) but ignored in the second pass (0x674f4).
 *   - Second pass (0x67252-0x67538): tables 0x676b0/0x676cc (0xff-0x129) and
 *     0x676f8 (0x140-0x145). The colormap/transfer arm passes FOUR arrays to
 *     TIFFSetField (six pushes, `ADD ESP,0x18` at 0x67474).
 *   - dir is freed unconditionally on both exits (0x67603, 0x67586).
 * ------------------------------------------------------------------------- */

#define TIFFTAG_OSUBFILETYPE 255 /* 0xff, slot 0 of the table at 0x676cc */
#define TIFFTAG_STRIPOFFSETS 273 /* 0x111, first-pass field-bit arm */
#define TIFFTAG_STRIPBYTECOUNTS 279 /* 0x117, PUSH 0x117 at 0x675a5 */
#define TIFFTAG_TRANSFERFUNCTION 301 /* 0x12d, JZ at 0x67278 */
#define TIFFTAG_TILEOFFSETS 324 /* 0x144, slot 4 of the table at 0x676f8 */
#define TIFFTAG_TILEBYTECOUNTS 325 /* 0x145, slot 5 of the table at 0x676f8 */

/* A consumed/dropped directory entry has its tag zeroed (0x67039) and the
 * second pass skips it (`TEST AX,AX` at 0x6725c). */
#define TIFF_DIR_IGNORE 0
/* field_readcount sentinels compared at 0x670ae / 0x670b4. */
#define TIFF_VARIABLE (-1)
#define TIFF_SPP (-2)
#define PHOTOMETRIC_PALETTE 3 /* CMP word ptr [EBX+0x3c],0x3 at 0x6753e */
/* Old-style subfile type mapping (0x67355-0x67368). */
#define OFILETYPE_REDUCEDIMAGE 2
#define OFILETYPE_PAGE 3
#define FILETYPE_REDUCEDIMAGE 1
#define FILETYPE_PAGE 2

/* Field bits tested or set by FUN_00066e70. */
#define FIELD_IMAGEDIMENSIONS 0 /* TEST AL,0x1 at 0x6704f, "ImageLength" */
#define FIELD_MAXSAMPLEVALUE 19 /* TEST EAX,0x80000 at 0x6761c */
#define FIELD_PLANARCONFIG 20 /* TEST EAX,0x100000 at 0x67186 */
#define FIELD_PAGENUMBER 25 /* OR [EBX+0x14],0x2000000 at 0x67303 */
#define FIELD_STRIPBYTECOUNTS 26 /* TEST [EBX+0x14],0x4000000 at 0x6755d */
#define FIELD_STRIPOFFSETS 27 /* TEST [EBX+0x14],0x8000000 at 0x67212 */
#define FIELD_COLORMAP 28 /* TEST [EBX+0x14],0x10000000 at 0x67545 */
#define FIELD_HALFTONEHINTS 43 /* OR [EBX+0x18],0x800 at 0x674ab */

/* 0x66e70 */
int FUN_00066e70(void *tif_)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_dir_entry_t *dp;
  int n;
  tiff_dir_entry_t *dir;
  int v;
  tiff_field_info_t *fip;
  unsigned short dircount;
  char *cp;
  int diroutoforderwarning;
  unsigned long expected;

  diroutoforderwarning = 0;
  /* 0x66e7a-0x66e8d. */
  tif->tif_diroff = tif->tif_nextdiroff;
  if (tif->tif_diroff == 0) {
    return 0;
  }
  tif->tif_curdir++;
  /* 0x66e96-0x66ed2. */
  if (__lseek(tif->tif_fd, (long)tif->tif_diroff, 0) != (long)tif->tif_diroff) {
    FUN_00068a30(tif->tif_name, "Seek error accessing TIFF directory");
    return 0;
  }
  /* 0x66ed3-0x66f03. */
  if (__read(tif->tif_fd, &dircount, sizeof(unsigned short)) != 2) {
    FUN_00068a30(tif->tif_name, "Can not read TIFF directory count");
    return 0;
  }
  if ((tif->field_0a & TIFF_SWAB_BIT) != 0) {
    FUN_0006f1b0(&dircount);
  }
  /* 0x66f16-0x66f35. */
  dir = (tiff_dir_entry_t *)CheckMalloc(dircount * sizeof(tiff_dir_entry_t),
                                        tif, "to read TIFF directory");
  if (dir == NULL) {
    return 0;
  }
  /* 0x66f37-0x66f6e. */
  if (__read(tif->tif_fd, dir, dircount * sizeof(tiff_dir_entry_t)) !=
      (int)(dircount * sizeof(tiff_dir_entry_t))) {
    FUN_00068a30(tif->tif_name, "Can not read TIFF directory");
    goto bad;
  }
  /* 0x66f73-0x66f9a. */
  if (__read(tif->tif_fd, &tif->tif_nextdiroff, sizeof(unsigned long)) != 4) {
    tif->tif_nextdiroff = 0;
  }
  if ((tif->field_0a & TIFF_SWAB_BIT) != 0) {
    FUN_0006f1d0(&tif->tif_nextdiroff);
  }

  /* 0x66f9d-0x66fb5. */
  tif->field_0a &= ~TIFF_BEENWRITING;
  TIFFFreeDirectory((int)tif);
  TIFFDefaultDirectory(tif);
  TIFFSetField((int)tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);

  /* First pass, 0x66fba-0x6704a. */
  fip = (tiff_field_info_t *)tiffFieldInfo;
  for (dp = dir, n = dircount; n > 0; n--, dp++) {
    if ((tif->field_0a & TIFF_SWAB_BIT) != 0) {
      FUN_0006f1f0(&dp->tdir_tag, 2);
      FUN_0006f220(&dp->tdir_count, 2);
    }
    if (dp->tdir_tag < fip->field_tag) {
      if (!diroutoforderwarning) {
        FUN_0006f9d0(
          tif->tif_name,
          "invalid TIFF directory; tags are not sorted in ascending order");
        diroutoforderwarning = 1;
      }
      fip = (tiff_field_info_t *)tiffFieldInfo;
    }
    while (fip->field_tag != 0 && fip->field_tag < dp->tdir_tag) {
      fip++;
    }
    if (fip->field_tag == 0 || fip->field_tag != dp->tdir_tag) {
      /* 0x67034: unknown tag, dropped without a diagnostic. */
      fip = (tiff_field_info_t *)tiffFieldInfo;
      goto ignore;
    }
    if (fip->field_bit == FIELD_IGNORE) {
      goto ignore;
    }
    /* 0x6707e-0x670d8. */
    while (dp->tdir_type != (unsigned short)fip->field_type) {
      if (fip->field_type == TIFF_ANY) {
        break;
      }
      fip++;
      if (fip->field_tag == 0 || fip->field_tag != dp->tdir_tag) {
        FUN_0006f9d0(tif->tif_name,
                     "wrong data type %d for \"%s\"; tag ignored",
                     dp->tdir_type, fip[-1].field_name);
        goto ignore;
      }
    }
    /* 0x670aa-0x670e3. */
    if (fip->field_readcount != TIFF_VARIABLE) {
      expected = (fip->field_readcount == TIFF_SPP) ?
                   (unsigned long)tif->td_samplesperpixel :
                   (unsigned long)fip->field_readcount;
      if (expected != dp->tdir_count) {
        goto ignore;
      }
    }
    /* 0x670e9-0x67181. */
    switch (dp->tdir_tag) {
    case TIFFTAG_STRIPOFFSETS:
    case TIFFTAG_STRIPBYTECOUNTS:
    case TIFFTAG_TILEOFFSETS:
    case TIFFTAG_TILEBYTECOUNTS:
      TIFFSetFieldBit(tif, fip->field_bit);
      break;
    case TIFFTAG_IMAGEWIDTH:
    case TIFFTAG_IMAGELENGTH:
    case TIFFTAG_IMAGEDEPTH:
    case TIFFTAG_TILELENGTH:
    case TIFFTAG_TILEWIDTH:
    case TIFFTAG_TILEDEPTH:
    case TIFFTAG_PLANARCONFIG:
    case TIFFTAG_SAMPLESPERPIXEL:
    case TIFFTAG_ROWSPERSTRIP:
      if (!TIFFFetchNormalTag(tif, dp)) {
        goto bad;
      }
      break;
    }
    continue;
  ignore:
    dp->tdir_tag = TIFF_DIR_IGNORE;
  }

  /* 0x6704c-0x67197. */
  if (!TIFFFieldSet(tif, FIELD_IMAGEDIMENSIONS)) {
    FUN_00068a30(tif->tif_name,
                 "TIFF directory is missing required \"%s\" field",
                 "ImageLength");
    goto bad;
  }
  if (!TIFFFieldSet(tif, FIELD_PLANARCONFIG)) {
    FUN_00068a30(tif->tif_name,
                 "TIFF directory is missing required \"%s\" field",
                 "PlanarConfiguration");
    goto bad;
  }
  /* 0x67197-0x671e4. */
  if (!TIFFFieldSet(tif, FIELD_TILEDIMENSIONS)) {
    tif->td_stripsperimage =
      (tif->td_rowsperstrip == 0xffffffff ?
         (tif->td_imagelength != 0 ? 1 : 0) :
         (tif->td_imagelength + tif->td_rowsperstrip - 1) /
           tif->td_rowsperstrip);
    tif->td_tilewidth = tif->td_imagewidth;
    tif->td_tilelength = tif->td_rowsperstrip;
    tif->td_tiledepth = tif->td_imagedepth;
    tif->field_0a &= ~TIFF_ISTILED;
  } else {
    tif->td_stripsperimage = TIFFNumberOfTiles(tif);
    tif->field_0a |= TIFF_ISTILED;
  }
  /* 0x671e8-0x67202. */
  tif->td_nstrips = tif->td_stripsperimage;
  if (tif->td_planarconfig == PLANARCONFIG_SEPARATE) {
    tif->td_nstrips *= tif->td_samplesperpixel;
  }
  /* 0x67208-0x67235. */
  if (tif->td_nstrips != 0 && !TIFFFieldSet(tif, FIELD_STRIPOFFSETS)) {
    FUN_00068a30(
      tif->tif_name, "TIFF directory is missing required \"%s\" field",
      (tif->field_0a & TIFF_ISTILED) != 0 ? "TileOffsets" : "StripOffsets");
    goto bad;
  }

  /* Second pass, 0x6723a-0x67538. */
  for (dp = dir, n = dircount; n > 0; n--, dp++) {
    if (dp->tdir_tag == TIFF_DIR_IGNORE) {
      continue;
    }
    switch (dp->tdir_tag) {
    case TIFFTAG_IMAGELENGTH:
    case TIFFTAG_SAMPLESPERPIXEL:
    case TIFFTAG_ROWSPERSTRIP:
    case TIFFTAG_PLANARCONFIG:
    case TIFFTAG_TILEWIDTH:
    case TIFFTAG_TILELENGTH:
    case TIFFTAG_TILEDEPTH:
      break;
    case TIFFTAG_MINSAMPLEVALUE:
    case TIFFTAG_MAXSAMPLEVALUE:
    case TIFFTAG_BITSPERSAMPLE:
    case TIFFTAG_COMPRESSION:
      /* 0x6729a-0x672ea. */
      if (dp->tdir_count == 1) {
        v = (int)TIFFExtractData(tif, dp->tdir_type, dp->tdir_offset);
        if (!TIFFSetField((int)tif, dp->tdir_tag, v)) {
          goto bad;
        }
        break;
      }
      /* fall through */
    case TIFFTAG_SAMPLEFORMAT:
    case TIFFTAG_DATATYPE:
      /* 0x674ff-0x67524. */
      if (!TIFFFetchPerSampleShorts(tif, dp, &v) ||
          !TIFFSetField((int)tif, dp->tdir_tag, v)) {
        goto bad;
      }
      break;
    case TIFFTAG_STRIPOFFSETS:
    case TIFFTAG_TILEOFFSETS:
      /* 0x673aa. */
      if (!TIFFFetchStripThing(tif, dp, (long)tif->td_nstrips,
                               &tif->td_stripoffset)) {
        goto bad;
      }
      break;
    case TIFFTAG_STRIPBYTECOUNTS:
    case TIFFTAG_TILEBYTECOUNTS:
      /* 0x673c7. */
      if (!TIFFFetchStripThing(tif, dp, (long)tif->td_nstrips,
                               &tif->td_stripbytecount)) {
        goto bad;
      }
      break;
    case TIFFTAG_COLORMAP:
      /* 0x673e4-0x673f9. */
      if (dp->tdir_count != 3 * (1UL << tif->td_bitspersample)) {
        break;
      }
      /* fall through */
    case TIFFTAG_TRANSFERFUNCTION:
      /* 0x673ff-0x6748f. */
      v = (int)((1UL << tif->td_bitspersample) * sizeof(unsigned short));
      cp = (char *)CheckMalloc(dp->tdir_count * sizeof(unsigned short), tif,
                               "to read \"TransferFunction\" tag");
      if (cp != NULL) {
        if (TIFFFetchData(tif, dp, cp)) {
          if (dp->tdir_count == 1UL << tif->td_bitspersample) {
            v = 0;
          }
          TIFFSetField((int)tif, dp->tdir_tag, cp, cp + v, cp + 2 * v,
                       cp + 3 * v);
        }
        debug_free(cp, "c:\\halo\\SOURCE\\bitmaps\\libtiff\\tif_dirread.c",
                   0x194);
      }
      break;
    case TIFFTAG_OSUBFILETYPE:
      /* 0x6730f-0x6737f. */
      v = 0;
      switch (TIFFExtractData(tif, dp->tdir_type, dp->tdir_offset)) {
      case OFILETYPE_REDUCEDIMAGE:
        v = FILETYPE_REDUCEDIMAGE;
        break;
      case OFILETYPE_PAGE:
        v = FILETYPE_PAGE;
        break;
      }
      if (v) {
        TIFFSetField((int)tif, TIFFTAG_SUBFILETYPE, v);
      }
      break;
    case TIFFTAG_PAGENUMBER:
      /* 0x672ef-0x6730a. */
      if (TIFFFetchShortArray(tif, dp, tif->td_pagenumber)) {
        TIFFSetFieldBit(tif, FIELD_PAGENUMBER);
      }
      break;
    case TIFFTAG_HALFTONEHINTS:
      /* 0x67494-0x674b2. */
      if (TIFFFetchShortArray(tif, dp, tif->td_halftonehints)) {
        TIFFSetFieldBit(tif, FIELD_HALFTONEHINTS);
      }
      break;
    case TIFFTAG_EXTRASAMPLES:
      /* 0x674b4-0x674df. */
      if (dp->tdir_count != 1) {
        FUN_00068a30(tif->tif_name,
                     "Can not handle more than 1 extra sample/pixel");
      } else {
        TIFFSetField((int)tif, TIFFTAG_MATTEING, 1);
      }
      break;
    default:
      /* 0x674f4: return value discarded. */
      TIFFFetchNormalTag(tif, dp);
      break;
    }
  }

  /* 0x6753e-0x6755b. */
  if (tif->td_photometric == PHOTOMETRIC_PALETTE &&
      !TIFFFieldSet(tif, FIELD_COLORMAP)) {
    FUN_00068a30(tif->tif_name,
                 "TIFF directory is missing required \"%s\" field", "Colormap");
    goto bad;
  }
  /* 0x6755d-0x67600. */
  if (!TIFFFieldSet(tif, FIELD_STRIPBYTECOUNTS)) {
    if (tif->td_nstrips > 1) {
      FUN_00068a30(tif->tif_name,
                   "TIFF directory is missing required \"%s\" field",
                   "StripByteCounts");
      goto bad;
    }
    FUN_0006f9d0(
      tif->tif_name,
      "TIFF directory is missing required \"%s\" field, "
      "calculating from imagelength",
      ((tiff_field_info_t *)FUN_00066380(TIFFTAG_STRIPBYTECOUNTS))->field_name);
    EstimateStripByteCounts(dircount, tif, dir);
  } else if (tif->td_nstrips == 1 && tif->td_stripbytecount[0] == 0) {
    FUN_0006f9d0(
      tif->tif_name,
      "Bogus \"%s\" field, ignoring and calculating from imagelength",
      ((tiff_field_info_t *)FUN_00066380(TIFFTAG_STRIPBYTECOUNTS))->field_name);
    EstimateStripByteCounts(dircount, tif, dir);
  }
  /* 0x67603. */
  debug_free(dir, "c:\\halo\\SOURCE\\bitmaps\\libtiff\\tif_dirread.c", 0x1fe);
  /* 0x67616-0x67645. */
  if (!TIFFFieldSet(tif, FIELD_MAXSAMPLEVALUE)) {
    tif->td_maxsamplevalue = (1UL << tif->td_bitspersample) - 1;
  }
  if (!TIFFFieldSet(tif, FIELD_COMPRESSION)) {
    TIFFSetField((int)tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
  }
  /* 0x67648-0x6767f. */
  tif->field_d4 = (unsigned long)-1;
  tif->field_dc = (unsigned long)-1;
  tif->field_e4 = (unsigned long)-1;
  tif->field_e8 = (unsigned long)-1;
  tif->field_ec = FUN_0006f910(tif);
  tif->tif_scanlinesize = TIFFScanlineSize((int)tif);
  return 1;

bad:
  /* 0x67586-0x675a4. */
  debug_free(dir, "c:\\halo\\SOURCE\\bitmaps\\libtiff\\tif_dirread.c", 0x212);
  return 0;
}
