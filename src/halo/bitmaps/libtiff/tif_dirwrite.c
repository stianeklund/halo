/* ===========================================================================
 * tif_dirwrite.c -- vendored libtiff (Sam Leffler, rev 1.16 92/03/18),
 * directory write support.
 *
 * All fourteen functions kb.json places in tif_dirwrite.obj (0x67710-0x6871e)
 * are transcribed from upstream libtiff tif_dirwrite.c rather than reshaped
 * from the decompiler (the vendored-library rule). The binary carries the
 * `$Header: .../tif_dirwrite.c,v 1.16 92/03/18 09:36:15 sam Exp $` rcsid and
 * the `c:\halo\SOURCE\bitmaps\libtiff\tif_dirwrite.c` __FILE__ string
 * (0x25fefc), and every function's size and order match the PAL build 2342
 * reconstruction (halo-pal-2342 source/bitmaps/libtiff/tif_dirwrite.c), which
 * is strict byte-exact against its binary. Bungie's copy is compiled without
 * JPEG_SUPPORT, COLORIMETRY_SUPPORT, YCBCR_SUPPORT and CMYK_SUPPORT: the
 * directory ends at td_stripbytecount (tif_open.c proves TIFFHeader right
 * after it at 0xc4), and TIFFWriteDirectory's switch has no case for them.
 *
 * ABI: every file-static function was given a custom register convention by
 * the original link (whole-program optimisation), recorded in kb.json as
 * `@<reg>` parameters and read off each function's entry and every call site:
 *   TIFFSetupShortLong    tif@esi tag@ax dir@edx v@ecx         (0x67710)
 *   TIFFWriteData         tif@ebx dir@edi, cp on the stack     (0x67760)
 *   TIFFLinkDirectory     tif@esi                              (0x677f0)
 *   TIFFWriteRational     type@eax tag@cx dir@edx, tif/v stack (0x67960)
 *   TIFFWriteShortTable   tif@edx tag@ax dir@ecx, n/table      (0x679f0)
 *   TIFFWriteString       tif@ebx tag@ax dir@ecx cp@esi        (0x67a70)
 *   TIFFWriteShortArray   tag@dx n@eax v@ecx, tif/type/dir     (0x67ac0)
 *   TIFFWriteLongArray    tif@ebx tag@dx n@eax v@ecx, type/dir (0x67b40)
 *   TIFFWriteRationalArray tag@cx n@eax, tif/type/dir/v        (0x67b80)
 *   TIFFWriteFloatArray   tif@ebx tag@dx n@eax v@ecx, type/dir (0x67c10)
 *   TIFFWriteNormalTag    fip@eax dir@ecx, tif on the stack    (0x67c50)
 *   TIFFWritePerSampleShorts dir@esi, tif/tag on the stack     (0x67f70)
 *   TIFFSetupShortPair    tif@ebx tag@di dir@esi               (0x68030)
 * LongArray/FloatArray/WriteString never read `tif` themselves; it arrives
 * in EBX only because they forward it to TIFFWriteData. The C below is plain
 * cdecl; the build generates the register thunks from kb.json.
 * ======================================================================== */

/* TIFFDataType (tiff.h). */
#define TIFF_ASCII 2
#define TIFF_SHORT 3
#define TIFF_LONG 4
#define TIFF_RATIONAL 5 /* `cmp eax,5` at 0x67966 */
#define TIFF_SSHORT 8
#define TIFF_SLONG 9
#define TIFF_SRATIONAL 10
#define TIFF_FLOAT 11

#define TIFF_BIGENDIAN 0x4d4d /* cmp [esi+0xc4],0x4d4d at 0x6773d */
#define TIFF_VARIABLE -1

/* tif_flags bits (tiffioP.h). */
#define TIFF_DIRTYDIRECT 0x2
#define TIFF_BUFFERSETUP 0x4
#define TIFF_BEENWRITING 0x8
#define TIFF_SWAB 0x10 /* `mov bl,0x10 / test byte ptr [esi+0xa],bl` */
#define TIFF_MYBUFFER 0x40
#define TIFF_ISTILED 0x80
#define TIFF_POSTENCODE 0x200

#define O_RDONLY 0
#define L_SET 0
#define L_INCR 1
#define L_XTND 2

#define TIFFTAG_IMAGEWIDTH 256
#define TIFFTAG_IMAGELENGTH 257
#define TIFFTAG_STRIPOFFSETS 273
#define TIFFTAG_STRIPBYTECOUNTS 279
#define TIFFTAG_XRESOLUTION 282
#define TIFFTAG_YRESOLUTION 283
#define TIFFTAG_XPOSITION 286
#define TIFFTAG_YPOSITION 287
#define TIFFTAG_COLORMAP 320
#define TIFFTAG_TILEWIDTH 322
#define TIFFTAG_TILELENGTH 323
#define TIFFTAG_TILEOFFSETS 324
#define TIFFTAG_TILEBYTECOUNTS 325

/* Field bits (tiffioP.h); FIELD_IMAGEDIMENSIONS is bit 0 in this build, as
 * TIFFPrintDirectory already shows (see tif_open.c). */
#define FIELD_IMAGEDIMENSIONS 0
#define FIELD_TILEDIMENSIONS 1
#define FIELD_RESOLUTION 3
#define FIELD_POSITION 4
#define FIELD_SUBFILETYPE 5
#define FIELD_BITSPERSAMPLE 6
#define FIELD_MINSAMPLEVALUE 18
#define FIELD_MAXSAMPLEVALUE 19
#define FIELD_PAGENUMBER 25
#define FIELD_STRIPBYTECOUNTS 26
#define FIELD_STRIPOFFSETS 27
#define FIELD_COLORMAP 28
#define FIELD_MATTEING 34
#define FIELD_SAMPLEFORMAT 38
#define FIELD_HALFTONEHINTS 43
#define FIELD_LAST 59
#define FIELD_IGNORE ((unsigned short)-1)

typedef int (*tiff_bool_method_t)(void *tif);
typedef int (*tiff_code_method_t)(void *tif, char *buf, int cc, int s);
typedef int (*tiff_seek_method_t)(void *tif, int n);

/* Upstream libtiff's TIFFDirEntry (tiff.h): the 12-byte on-disk entry.
 * Stores at +0/+2 are word-wide and +4/+8 dword-wide (0x67716-0x67728). */
typedef struct tiff_dir_entry_s {
  unsigned short tdir_tag; /* 0x00 */
  unsigned short tdir_type; /* 0x02 */
  unsigned long tdir_count; /* 0x04 */
  unsigned long tdir_offset; /* 0x08 */
} tiff_dir_entry_t;

/* Upstream libtiff's TIFFFieldInfo (tiffioP.h). Row stride 0x14 is proven by
 * TIFFWriteDirectory's `add ebx,0x14` table walk; writecount at +4 and type
 * at +8 by TIFFWriteNormalTag (0x67c65, 0x67c6c), field_bit at +0x0c and the
 * name at +0x10 by the tif_dir.c users. */
typedef struct tiff_field_info_s {
  unsigned short field_tag; /* 0x00 */
  short field_readcount; /* 0x02 */
  short field_writecount; /* 0x04 */
  int field_type; /* 0x08 */
  unsigned short field_bit; /* 0x0c */
  unsigned short field_oktochange; /* 0x0e */
  char *field_name; /* 0x10 */
} tiff_field_info_t;

/* Upstream libtiff's TIFFDirectory without the optional-support tails. The
 * offsets relative to the TIFF base are the ones tif_open.c proves
 * (td_fieldsset at 0x14 ... td_stripbytecount at 0xc0); _TIFFgetfield is
 * passed `tif + 0x14` (0x67f88), i.e. the directory is a nested member. */
typedef struct tiff_directory_s {
  unsigned long td_fieldsset[2]; /* 0x14 */
  unsigned long td_imagewidth; /* 0x1c */
  unsigned long td_imagelength; /* 0x20 */
  unsigned long td_imagedepth; /* 0x24 */
  unsigned long td_tilewidth; /* 0x28 */
  unsigned long td_tilelength; /* 0x2c */
  unsigned long td_tiledepth; /* 0x30 */
  unsigned short td_subfiletype; /* 0x34 */
  unsigned short td_bitspersample; /* 0x36 -- `mov cl,[ebx+0x36]` 0x67a09 */
  unsigned short td_sampleformat; /* 0x38 */
  unsigned short td_compression; /* 0x3a */
  unsigned short td_photometric; /* 0x3c */
  unsigned short td_threshholding; /* 0x3e */
  unsigned short td_fillorder; /* 0x40 */
  unsigned short td_orientation; /* 0x42 */
  unsigned short td_samplesperpixel; /* 0x44 -- movzx 0x67f7f */
  unsigned short td_predictor; /* 0x46 */
  unsigned long td_rowsperstrip; /* 0x48 */
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
  unsigned short td_badfaxrun; /* 0x78 */
  unsigned long td_badfaxlines; /* 0x7c */
  unsigned short *td_colormap[3]; /* 0x80 */
  unsigned short td_halftonehints[2]; /* 0x8c */
  char *td_documentname; /* 0x90 */
  char *td_artist; /* 0x94 */
  char *td_datetime; /* 0x98 */
  char *td_hostcomputer; /* 0x9c */
  char *td_imagedescription; /* 0xa0 */
  char *td_make; /* 0xa4 */
  char *td_model; /* 0xa8 */
  char *td_software; /* 0xac */
  char *td_pagename; /* 0xb0 */
  unsigned long td_stripsperimage; /* 0xb4 */
  unsigned long td_nstrips; /* 0xb8 */
  unsigned long *td_stripoffset; /* 0xbc */
  unsigned long *td_stripbytecount; /* 0xc0 */
} tiff_directory_t;

typedef struct tiff_header_s {
  unsigned short tiff_magic; /* 0xc4 */
  unsigned short tiff_version; /* 0xc6 */
  unsigned long tiff_diroff; /* 0xc8 */
} tiff_header_t;

/* Upstream libtiff's `struct tiff` (tiffioP.h). tif_fd/tif_mode are 16-bit
 * (`movsx eax,word ptr [esi+4]` throughout), tif_flags is the 16-bit word at
 * 0x0a (byte tests at 0x0a/0x0b), and the rest of the layout is the one
 * tif_open.c proves field by field. */
typedef struct tiff_s {
  char *tif_name; /* 0x00 */
  short tif_fd; /* 0x04 */
  short tif_mode; /* 0x06 */
  char tif_fillorder; /* 0x08 */
  char tif_options; /* 0x09 */
  short tif_flags; /* 0x0a */
  long tif_diroff; /* 0x0c */
  long tif_nextdiroff; /* 0x10 */
  tiff_directory_t tif_dir; /* 0x14 */
  tiff_header_t tif_header; /* 0xc4 */
  const int *tif_typeshift; /* 0xcc -- `mov ecx,[esi+0xcc]` 0x67748 */
  const long *tif_typemask; /* 0xd0 -- `mov eax,[esi+0xd0]` 0x67732 */
  long tif_row; /* 0xd4 */
  int tif_curdir; /* 0xd8 */
  int tif_curstrip; /* 0xdc */
  long tif_curoff; /* 0xe0 */
  long tif_col; /* 0xe4 */
  int tif_curtile; /* 0xe8 */
  long tif_tilesize; /* 0xec */
  tiff_bool_method_t tif_predecode; /* 0xf0 */
  tiff_bool_method_t tif_preencode; /* 0xf4 */
  tiff_bool_method_t tif_postencode; /* 0xf8 */
  tiff_code_method_t tif_decoderow; /* 0xfc */
  tiff_code_method_t tif_encoderow; /* 0x100 */
  tiff_code_method_t tif_decodestrip; /* 0x104 */
  tiff_code_method_t tif_encodestrip; /* 0x108 */
  tiff_code_method_t tif_decodetile; /* 0x10c */
  tiff_code_method_t tif_encodetile; /* 0x110 */
  tiff_bool_method_t tif_close; /* 0x114 */
  tiff_seek_method_t tif_seek; /* 0x118 */
  tiff_bool_method_t tif_cleanup; /* 0x11c */
  char *tif_data; /* 0x120 */
  int tif_scanlinesize; /* 0x124 */
  int tif_scanlineskew; /* 0x128 */
  char *tif_rawdata; /* 0x12c */
  long tif_rawdatasize; /* 0x130 */
  char *tif_rawcp; /* 0x134 */
  long tif_rawcc; /* 0x138 */
} tiff_t;

/* Upstream's tiffDataWidth table, indexed by TIFFDataType
 * (`mov esi,[ecx*4+0x2ca024]` at 0x67774); same spelling as tif_dir.c. */
#define tiffDataWidth ((const int *)0x2ca024)
/* Upstream libtiff's file-scope directory data offset (0x3340b0). */
#define dataoff (*(unsigned long *)0x3340b0)

#define TIFF_DIRWRITE_FILE "c:\\halo\\SOURCE\\bitmaps\\libtiff\\tif_dirwrite.c"
/* Upstream's `module` for TIFFLinkDirectory diagnostics (0x2ca124). */
#define module "TIFFLinkDirectory"

#define BITn(n) (((unsigned)1L) << ((n) & 0x1f))
#define TIFFFieldSet(tif, field) \
  ((tif)->tif_dir.td_fieldsset[(field) / 32] & BITn(field))
#define FieldSet(fields, f) (fields[(f) / 32] & BITn(f))
#define ResetFieldBit(fields, f) (fields[(f) / 32] &= ~BITn(f))
#define isTiled(tif) (((tif)->tif_flags & TIFF_ISTILED) != 0)
#define TIFFInsertData(tif, type, v)                                   \
  ((tif)->tif_header.tiff_magic == TIFF_BIGENDIAN ?                    \
     ((v) & (tif)->tif_typemask[type]) << (tif)->tif_typeshift[type] : \
     (v) & (tif)->tif_typemask[type])
#define SeekOK(fd, off) (__lseek(fd, (long)off, L_SET) == (long)off)
#define WriteOK(fd, buf, size) (__write(fd, (char *)buf, size) == size)
#define ReadOK(fd, buf, size) (__read(fd, (char *)buf, size) == size)

/* FUN_00068a30 is TIFFError, FUN_0006f9d0 TIFFWarning and FUN_00066380
 * TIFFFieldWithTag (see tif_dir.c). */
#define TIFFFieldWithTag(tag) ((tiff_field_info_t *)FUN_00066380(tag))

#define WriteRationalPair(type, tag1, v1, tag2, v2)       \
  {                                                       \
    if (!TIFFWriteRational(tif, type, tag1, dir, v1))     \
      goto bad;                                           \
    if (!TIFFWriteRational(tif, type, tag2, dir + 1, v2)) \
      goto bad;                                           \
    dir++;                                                \
  }

/* 0x67710 -- setup a directory entry with either a SHORT or LONG type
 * according to the value. */
void TIFFSetupShortLong(void *tif_, unsigned short tag, void *dir_,
                        unsigned long v)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_dir_entry_t *dir = (tiff_dir_entry_t *)dir_;

  dir->tdir_tag = tag;
  dir->tdir_count = 1;
  if (v > 0xffffL) {
    dir->tdir_type = (short)TIFF_LONG;
    dir->tdir_offset = v;
  } else {
    dir->tdir_type = (short)TIFF_SHORT;
    dir->tdir_offset = TIFFInsertData(tif, (int)TIFF_SHORT, v);
  }
  return;
}

/* 0x67760 -- write a contiguous directory item at `dataoff`. */
int TIFFWriteData(void *tif_, void *dir_, char *cp)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_dir_entry_t *dir = (tiff_dir_entry_t *)dir_;
  int cc;

  dir->tdir_offset = dataoff;
  cc = dir->tdir_count * tiffDataWidth[dir->tdir_type];
  if (SeekOK(tif->tif_fd, dir->tdir_offset) && WriteOK(tif->tif_fd, cp, cc)) {
    dataoff += (cc + 1) & ~1;
    return (1);
  }
  FUN_00068a30(tif->tif_name, "Error writing data for field \"%s\"",
               TIFFFieldWithTag(dir->tdir_tag)->field_name);
  return (0);
}

/* 0x677f0 -- link the current directory into the directory chain. */
int TIFFLinkDirectory(void *tif_)
{
  tiff_t *tif = (tiff_t *)tif_;
  unsigned short dircount;
  long nextdir;

  tif->tif_diroff = (__lseek(tif->tif_fd, 0L, L_XTND) + 1) & ~1L;
  if (tif->tif_header.tiff_diroff == 0) {
    /*
     * First directory, overwrite header.
     */
    tif->tif_header.tiff_diroff = tif->tif_diroff;
    (void)__lseek(tif->tif_fd, 0L, L_SET);
    if (!WriteOK(tif->tif_fd, &tif->tif_header, sizeof(tif->tif_header))) {
      FUN_00068a30(tif->tif_name, "Error writing TIFF header");
      return (0);
    }
    return (1);
  }
  /*
   * Not the first directory, search to the last and append.
   */
  nextdir = tif->tif_header.tiff_diroff;
  do {
    if (!SeekOK(tif->tif_fd, nextdir) ||
        !ReadOK(tif->tif_fd, &dircount, sizeof(dircount))) {
      FUN_00068a30(module, "Error fetching directory count");
      return (0);
    }
    if (tif->tif_flags & TIFF_SWAB)
      FUN_0006f1b0(&dircount);
    __lseek(tif->tif_fd, dircount * sizeof(tiff_dir_entry_t), L_INCR);
    if (!ReadOK(tif->tif_fd, &nextdir, sizeof(nextdir))) {
      FUN_00068a30(module, "Error fetching directory link");
      return (0);
    }
    if (tif->tif_flags & TIFF_SWAB)
      FUN_0006f1d0((unsigned long *)&nextdir);
  } while (nextdir != 0);
  (void)__lseek(tif->tif_fd, -(long)sizeof(nextdir), L_INCR);
  if (!WriteOK(tif->tif_fd, &tif->tif_diroff, sizeof(tif->tif_diroff))) {
    FUN_00068a30(module, "Error writing directory link");
    return (0);
  }
  return (1);
}

/* 0x67960 -- setup a RATIONAL directory entry and write the associated
 * indirect value. `v` is a genuine float stack argument: it is loaded with
 * `fld dword ptr [ebp+0xc]` (0x67980, 0x67990, 0x679b6). */
int TIFFWriteRational(void *tif_, int type, unsigned short tag, void *dir_,
                      float v)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_dir_entry_t *dir = (tiff_dir_entry_t *)dir_;
  unsigned long t[2];

  dir->tdir_tag = tag;
  dir->tdir_type = (short)type;
  dir->tdir_count = 1;
  if (type == TIFF_RATIONAL && v < 0)
    FUN_0006f9d0(
      tif->tif_name,
      "\"%s\": Information lost writing value (%g) as (unsigned) RATIONAL",
      TIFFFieldWithTag(tag)->field_name, v);
  /* need algorithm to convert ... XXX */
  t[0] = v * 10000.0 + 0.5;
  t[1] = 10000;
  return (TIFFWriteData(tif, dir, (char *)t));
}

/* 0x679f0 -- setup a directory entry for an NxM table of shorts, where M is
 * known to be 2**bitspersample, and write the associated indirect data. */
int TIFFWriteShortTable(void *tif_, unsigned short tag, void *dir_, int n,
                        unsigned short **table)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_dir_entry_t *dir = (tiff_dir_entry_t *)dir_;
  unsigned long off;
  int i;

  dir->tdir_tag = tag;
  /* Upstream reads dataoff after the count store; the original loads it
   * right after the tag store (`mov eax,[0x3340b0]` at 0x679fe). */
  off = dataoff;
  dir->tdir_type = (short)TIFF_SHORT;
  /* XXX -- yech, fool TIFFWriteData */
  dir->tdir_count = 1L << tif->tif_dir.td_bitspersample;
  for (i = 0; i < n; i++)
    if (!TIFFWriteData(tif, dir, (char *)table[i]))
      return (0);
  dir->tdir_count *= n;
  dir->tdir_offset = off;
  return (1);
}

/* 0x67a70 -- setup a directory entry of an ASCII string and write any
 * associated indirect value. */
int TIFFWriteString(void *tif_, unsigned short tag, void *dir_, char *cp)
{
  tiff_dir_entry_t *dir = (tiff_dir_entry_t *)dir_;

  dir->tdir_tag = tag;
  dir->tdir_type = (short)TIFF_ASCII;
  dir->tdir_count = csstrlen(cp) + 1; /* includes \0 byte */
  if (dir->tdir_count > 4) {
    if (!TIFFWriteData(tif_, dir, cp))
      return (0);
  } else
    csmemcpy(&dir->tdir_offset, cp, dir->tdir_count);
  return (1);
}

/* 0x67ac0 -- setup a directory entry of an array of SHORT or SSHORT and
 * write the associated indirect values. */
int TIFFWriteShortArray(void *tif_, int type, unsigned short tag, void *dir_,
                        int n, unsigned short *v)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_dir_entry_t *dir = (tiff_dir_entry_t *)dir_;

  dir->tdir_tag = tag;
  dir->tdir_type = (short)type;
  dir->tdir_count = n;
  if (n <= 2) {
    if (tif->tif_header.tiff_magic == TIFF_BIGENDIAN) {
      dir->tdir_offset = (long)v[0] << 16;
      if (n == 2)
        dir->tdir_offset |= v[1] & 0xffff;
    } else {
      dir->tdir_offset = v[0] & 0xffff;
      if (n == 2)
        dir->tdir_offset |= (long)v[1] << 16;
    }
    return (1);
  } else
    return (TIFFWriteData(tif, dir, (char *)v));
}

/* 0x67b40 -- setup a directory entry of an array of LONG or SLONG and write
 * the associated indirect values. */
int TIFFWriteLongArray(void *tif_, int type, unsigned short tag, void *dir_,
                       int n, unsigned long *v)
{
  tiff_dir_entry_t *dir = (tiff_dir_entry_t *)dir_;

  dir->tdir_tag = tag;
  dir->tdir_type = (short)type;
  dir->tdir_count = n;
  if (n == 1) {
    dir->tdir_offset = v[0];
    return (1);
  } else
    return (TIFFWriteData(tif_, dir, (char *)v));
}

/* 0x67b80 -- setup a directory entry of an array of RATIONAL or SRATIONAL
 * and write the associated indirect values. */
int TIFFWriteRationalArray(void *tif_, int type, unsigned short tag, void *dir_,
                           int n, float *v)
{
  tiff_dir_entry_t *dir = (tiff_dir_entry_t *)dir_;
  int i, status;
  unsigned long *t;

  dir->tdir_tag = tag;
  dir->tdir_type = (short)type;
  dir->tdir_count = n;
  t = (unsigned long *)debug_malloc(2 * n * sizeof(long), 0, TIFF_DIRWRITE_FILE,
                                    634);
  for (i = 0; i < n; i++) {
    /* need algorithm to convert ... XXX */
    t[2 * i + 0] = v[i] * 10000.0 + 0.5;
    t[2 * i + 1] = 10000;
  }
  status = TIFFWriteData(tif_, dir, (char *)t);
  debug_free((char *)t, TIFF_DIRWRITE_FILE, 641);
  return (status);
}

/* 0x67c10 -- setup a directory entry of an array of FLOAT. HAVE_IEEEFP is
 * set, so upstream's TIFFCvtNativeToIEEEFloat is empty. */
int TIFFWriteFloatArray(void *tif_, int type, unsigned short tag, void *dir_,
                        int n, float *v)
{
  tiff_dir_entry_t *dir = (tiff_dir_entry_t *)dir_;

  dir->tdir_tag = tag;
  dir->tdir_type = (short)type;
  dir->tdir_count = n;
  if (n == 1) {
    dir->tdir_offset = *(unsigned long *)&v[0];
    return (1);
  } else
    return (TIFFWriteData(tif_, dir, (char *)v));
}

#define WRITE(x, y) x(tif, fip->field_type, fip->field_tag, dir, wc, y)

/* 0x67c50 -- process tags that are not special cased. */
int TIFFWriteNormalTag(void *tif_, void *dir_, void *fip_)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_dir_entry_t *dir = (tiff_dir_entry_t *)dir_;
  tiff_field_info_t *fip = (tiff_field_info_t *)fip_;
  tiff_directory_t *td = &tif->tif_dir;
  unsigned short wc = (unsigned short)fip->field_writecount;

  dir->tdir_tag = fip->field_tag;
  dir->tdir_type = (unsigned short)fip->field_type;
  dir->tdir_count = wc;
  switch (fip->field_type) {
  case TIFF_SHORT:
  case TIFF_SSHORT:
    if (wc > 1) {
      unsigned short *wp;
      if (wc == (unsigned short)TIFF_VARIABLE) {
        _TIFFgetfield(td, fip->field_tag, &wc, &wp);
        dir->tdir_count = wc;
      } else
        _TIFFgetfield(td, fip->field_tag, &wp);
      if (!WRITE(TIFFWriteShortArray, wp))
        return (0);
    } else {
      unsigned short sv;
      _TIFFgetfield(td, fip->field_tag, &sv);
      dir->tdir_offset = TIFFInsertData(tif, dir->tdir_type, sv);
    }
    break;
  case TIFF_LONG:
  case TIFF_SLONG:
    if (wc > 1) {
      unsigned long *lp;
      if (wc == (unsigned short)TIFF_VARIABLE) {
        _TIFFgetfield(td, fip->field_tag, &wc, &lp);
        dir->tdir_count = wc;
      } else
        _TIFFgetfield(td, fip->field_tag, &lp);
      if (!WRITE(TIFFWriteLongArray, lp))
        return (0);
    } else {
      /* XXX handle LONG->SHORT conversion */
      _TIFFgetfield(td, fip->field_tag, &dir->tdir_offset);
    }
    break;
  case TIFF_RATIONAL:
  case TIFF_SRATIONAL:
    if (wc > 1) {
      float *fp;
      if (wc == (unsigned short)TIFF_VARIABLE) {
        _TIFFgetfield(td, fip->field_tag, &wc, &fp);
        dir->tdir_count = wc;
      } else
        _TIFFgetfield(td, fip->field_tag, &fp);
      if (!WRITE(TIFFWriteRationalArray, fp))
        return (0);
    } else {
      float fv;
      _TIFFgetfield(td, fip->field_tag, &fv);
      if (!TIFFWriteRational(tif, fip->field_type, fip->field_tag, dir, fv))
        return (0);
    }
    break;
  case TIFF_FLOAT:
    if (wc > 1) {
      float *fp;
      int n;
      if (wc == (unsigned short)TIFF_VARIABLE) {
        _TIFFgetfield(td, fip->field_tag, &wc, &fp);
        dir->tdir_count = wc;
      } else
        _TIFFgetfield(td, fip->field_tag, &fp);
      /* WRITE(TIFFWriteFloatArray, fp), inlined by the original:
       * 0x67ea6..0x67edb carries its body and calls TIFFWriteData (0x67760)
       * directly; there is no call to 0x67c10. `n` is the inlined callee's
       * int parameter: the original widens wc once and tests it with
       * `cmp eax,1` (not a 16-bit compare). */
      n = wc;
      dir->tdir_tag = fip->field_tag;
      dir->tdir_type = (short)fip->field_type;
      dir->tdir_count = n;
      if (n == 1)
        dir->tdir_offset = *(unsigned long *)&fp[0];
      else if (!TIFFWriteData(tif, dir, (char *)fp))
        return (0);
    } else {
      float fv;
      _TIFFgetfield(td, fip->field_tag, &fv);
      /* XXX assumes sizeof (long) == sizeof (float) */
      dir->tdir_offset = *(unsigned long *)&fv; /* XXX */
    }
    break;
  case TIFF_ASCII: {
    char *cp;
    _TIFFgetfield(td, fip->field_tag, &cp);
    if (!TIFFWriteString(tif, fip->field_tag, dir, cp))
      return (0);
    break;
  }
  }
  return (1);
}
#undef WRITE

/* 0x67f70 -- setup a directory entry that references a samples/pixel array
 * of SHORT values and (potentially) write the associated indirect values.
 * The four-entry buffer is proven by the `sub esp,0xc` frame. */
int TIFFWritePerSampleShorts(void *tif_, unsigned short tag, void *dir)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_dir_entry_t *d = (tiff_dir_entry_t *)dir;
  unsigned short w[4], v;
  int i, samplesperpixel = tif->tif_dir.td_samplesperpixel;

  _TIFFgetfield(&tif->tif_dir, tag, &v);
  for (i = 0; i < samplesperpixel; i++)
    w[i] = v;
  /* TIFFWriteShortArray(tif, TIFF_SHORT, tag, dir, samplesperpixel, w), inlined
   * by the original: 0x67fb7..0x68022 carries its body and calls TIFFWriteData
   * (0x67760) directly; there is no call to 0x67ac0. */
  d->tdir_tag = tag;
  d->tdir_type = TIFF_SHORT;
  d->tdir_count = samplesperpixel;
  if (samplesperpixel <= 2) {
    if (tif->tif_header.tiff_magic == TIFF_BIGENDIAN) {
      d->tdir_offset = (long)w[0] << 16;
      if (samplesperpixel == 2)
        d->tdir_offset |= w[1] & 0xffff;
    } else {
      d->tdir_offset = w[0] & 0xffff;
      if (samplesperpixel == 2)
        d->tdir_offset |= (long)w[1] << 16;
    }
    return (1);
  } else
    return (TIFFWriteData(tif, d, (char *)w));
}

/* 0x68030 -- setup a pair of shorts that are returned by value, rather than
 * as a reference to an array. */
int TIFFSetupShortPair(void *tif_, unsigned short tag, void *dir)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_dir_entry_t *d = (tiff_dir_entry_t *)dir;
  unsigned short v[2];

  _TIFFgetfield(&tif->tif_dir, tag, &v[0], &v[1]);
  /* TIFFWriteShortArray(tif, TIFF_SHORT, tag, dir, 2, v), inlined by the
   * original with n == 2 folded: 0x68049..0x68094 carries only the in-place
   * (n <= 2) arm and there is no call to 0x67ac0 or 0x67760. v[0] is widened
   * once for both byte orders (`movzx ecx,word ptr [ebp-4]` at 0x68049, ahead
   * of the magic test). */
  d->tdir_tag = tag;
  d->tdir_type = TIFF_SHORT;
  d->tdir_count = 2;
  if (tif->tif_header.tiff_magic == TIFF_BIGENDIAN)
    d->tdir_offset = ((unsigned long)v[0] << 16) | (v[1] & 0xffff);
  else
    d->tdir_offset = v[0] | ((long)v[1] << 16);
  return (1);
}

#undef dataoff
/* 0x680a0 -- write the contents of the current directory to the file. This
 * routine doesn't handle overwriting a directory with auxiliary storage
 * that's been changed. */
int TIFFWriteDirectory(void *tif_)
{
  tiff_t *tif = (tiff_t *)tif_;
  short dircount, tag;
  int nfields, dirsize;
  char *data;
  tiff_field_info_t *fip;
  tiff_dir_entry_t *dir;
  tiff_directory_t *td;
  unsigned long b, fields[sizeof(td->td_fieldsset) / sizeof(unsigned long)];

  if (tif->tif_mode == O_RDONLY)
    return (1);
  /*
   * Clear write state so that subsequent images with
   * different characteristics get the right buffers
   * setup for them.
   */
  if (tif->tif_flags & TIFF_POSTENCODE) {
    tif->tif_flags &= ~TIFF_POSTENCODE;
    if (tif->tif_postencode && !(*tif->tif_postencode)(tif)) {
      FUN_00068a30(tif->tif_name, "Error post-encoding before directory write");
      return (0);
    }
  }
  if (tif->tif_close)
    (*tif->tif_close)(tif);
  if (tif->tif_cleanup)
    (*tif->tif_cleanup)(tif);
  /*
   * Flush any data that might have been written
   * by the compression close+cleanup routines.
   */
  if (tif->tif_rawcc > 0 && !TIFFFlushData1(tif)) {
    FUN_00068a30(tif->tif_name, "Error flushing data before directory write");
    return (0);
  }
  if ((tif->tif_flags & TIFF_MYBUFFER) && tif->tif_rawdata) {
    debug_free(tif->tif_rawdata, TIFF_DIRWRITE_FILE, 154);
    tif->tif_rawdata = NULL;
    tif->tif_rawcc = 0;
  }
  tif->tif_flags &= ~(TIFF_BEENWRITING | TIFF_BUFFERSETUP);

  /*
   * Size the directory so that we can calculate
   * offsets for the data items that aren't kept
   * in-place in each field.
   */
  nfields = 0;
  /* Upstream: for (b = 0; b <= FIELD_LAST; b++) if (TIFFFieldSet(tif, b))
   * nfields += (b < FIELD_SUBFILETYPE ? 2 : 1). The original compiler
   * unrolled it ten-fold (0x68195-0x6830d: ten bit tests per pass, then
   * `add eax,0xa / cmp ecx,0x3b / jbe`); VC7.1 picks six for the same
   * loop regardless of spelling or flags, so the ten-way body is written
   * out. Same fields visited in the same order. */
#define COUNT_FIELD(i)        \
  if (TIFFFieldSet(tif, (i))) \
  nfields += ((i) < FIELD_SUBFILETYPE ? 2 : 1)
  for (b = 0; b <= FIELD_LAST; b += 10) {
    COUNT_FIELD(b);
    COUNT_FIELD(b + 1);
    COUNT_FIELD(b + 2);
    COUNT_FIELD(b + 3);
    COUNT_FIELD(b + 4);
    COUNT_FIELD(b + 5);
    COUNT_FIELD(b + 6);
    COUNT_FIELD(b + 7);
    COUNT_FIELD(b + 8);
    COUNT_FIELD(b + 9);
  }
#undef COUNT_FIELD
  /* Upstream sets td before the sizing loop. The original spills it at once
   * (`lea ecx,[ebx+0x14] / mov [ebp-4],ecx` at 0x68186) and keeps nfields in
   * EDI through the loop; td is not read inside the loop, so assigning it
   * here is equivalent and keeps VC71 from holding it in a register there. */
  td = &tif->tif_dir;
  dirsize = nfields * sizeof(tiff_dir_entry_t);
  data = debug_malloc(dirsize, 0, TIFF_DIRWRITE_FILE, 171);
  if (data == NULL) {
    FUN_00068a30(tif->tif_name, "Cannot write directory, out of space");
    return (0);
  }
  /*
   * Directory hasn't been placed yet, put
   * it at the end of the file and link it
   * into the existing directory structure.
   */
  if (tif->tif_diroff == 0 && !TIFFLinkDirectory(tif))
    return (0);
  dataoff = tif->tif_diroff + sizeof(short) + dirsize + sizeof(long);
  if (dataoff & 1)
    dataoff++;
  (void)__lseek(tif->tif_fd, dataoff, L_SET);
  tif->tif_curdir++;
  dir = (tiff_dir_entry_t *)data;
  /*
   * Setup external form of directory
   * entries and write data items.
   */
  csmemcpy(fields, td->td_fieldsset, sizeof(fields));
  /*BEGIN XXX*/
  /*
   * Write out ExtraSamples tag only if Matteing would
   * be set to 1 (i.e. Associated Alpha data is present).
   */
  if (FieldSet(fields, FIELD_MATTEING) && !td->td_matteing) { /*XXX*/
    ResetFieldBit(fields, FIELD_MATTEING); /*XXX*/
    nfields--; /*XXX*/
    dirsize -= sizeof(tiff_dir_entry_t); /*XXX*/
  } /*XXX*/
  /*END XXX*/
  for (fip = (tiff_field_info_t *)tiffFieldInfo; fip->field_tag; fip++) {
    if (fip->field_bit == FIELD_IGNORE || !FieldSet(fields, fip->field_bit))
      continue;
    switch (fip->field_bit) {
    case FIELD_STRIPOFFSETS:
      /*
       * We use one field bit for both strip and tile
       * offsets, and so must be careful in selecting
       * the appropriate field descriptor (so that tags
       * are written in sorted order).
       */
      tag = isTiled(tif) ? TIFFTAG_TILEOFFSETS : TIFFTAG_STRIPOFFSETS;
      if (tag != fip->field_tag)
        continue;
      if (!TIFFWriteLongArray(tif, TIFF_LONG, tag, dir, (int)td->td_nstrips,
                              td->td_stripoffset))
        goto bad;
      break;
    case FIELD_STRIPBYTECOUNTS:
      /*
       * We use one field bit for both strip and tile
       * byte counts, and so must be careful in selecting
       * the appropriate field descriptor (so that tags
       * are written in sorted order).
       */
      tag = isTiled(tif) ? TIFFTAG_TILEBYTECOUNTS : TIFFTAG_STRIPBYTECOUNTS;
      if (tag != fip->field_tag)
        continue;
      if (!TIFFWriteLongArray(tif, TIFF_LONG, tag, dir, (int)td->td_nstrips,
                              td->td_stripbytecount))
        goto bad;
      break;
    case FIELD_COLORMAP:
      if (!TIFFWriteShortTable(tif, TIFFTAG_COLORMAP, dir, 3, td->td_colormap))
        goto bad;
      break;
    case FIELD_IMAGEDIMENSIONS:
      TIFFSetupShortLong(tif, TIFFTAG_IMAGEWIDTH, dir++, td->td_imagewidth);
      TIFFSetupShortLong(tif, TIFFTAG_IMAGELENGTH, dir, td->td_imagelength);
      break;
    case FIELD_TILEDIMENSIONS:
      TIFFSetupShortLong(tif, TIFFTAG_TILEWIDTH, dir++, td->td_tilewidth);
      TIFFSetupShortLong(tif, TIFFTAG_TILELENGTH, dir, td->td_tilelength);
      break;
    case FIELD_POSITION:
      WriteRationalPair(TIFF_RATIONAL, TIFFTAG_XPOSITION, td->td_xposition,
                        TIFFTAG_YPOSITION, td->td_yposition);
      break;
    case FIELD_RESOLUTION:
      WriteRationalPair(TIFF_RATIONAL, TIFFTAG_XRESOLUTION, td->td_xresolution,
                        TIFFTAG_YRESOLUTION, td->td_yresolution);
      break;
    case FIELD_BITSPERSAMPLE:
    case FIELD_MINSAMPLEVALUE:
    case FIELD_MAXSAMPLEVALUE:
    case FIELD_SAMPLEFORMAT:
      if (!TIFFWritePerSampleShorts(tif, fip->field_tag, dir))
        goto bad;
      break;
    case FIELD_PAGENUMBER:
    case FIELD_HALFTONEHINTS:
      TIFFSetupShortPair(tif, fip->field_tag, dir);
      break;
    default:
      if (!TIFFWriteNormalTag(tif, dir, fip))
        goto bad;
      break;
    }
    dir++;
    ResetFieldBit(fields, fip->field_bit);
  }
  /*
   * Write directory.
   */
  (void)__lseek(tif->tif_fd, tif->tif_diroff, L_SET);
  dircount = nfields;
  if (!WriteOK(tif->tif_fd, &dircount, sizeof(short))) {
    FUN_00068a30(tif->tif_name, "Error writing directory count");
    goto bad;
  }
  if (!WriteOK(tif->tif_fd, data, dirsize)) {
    FUN_00068a30(tif->tif_name, "Error writing directory contents");
    goto bad;
  }
  if (!WriteOK(tif->tif_fd, &tif->tif_nextdiroff, sizeof(long))) {
    FUN_00068a30(tif->tif_name, "Error writing directory link");
    goto bad;
  }
  TIFFFreeDirectory((int)tif);
  debug_free(data, TIFF_DIRWRITE_FILE, 339);
  tif->tif_flags &= ~TIFF_DIRTYDIRECT;

  /*
   * Reset directory-related state for subsequent
   * directories.
   */
  TIFFDefaultDirectory(tif);
  tif->tif_diroff = 0;
  tif->tif_curoff = 0;
  tif->tif_row = -1;
  tif->tif_curstrip = -1;
  return (1);
bad:
  debug_free(data, TIFF_DIRWRITE_FILE, 353);
  return (0);
}
#undef WriteRationalPair
