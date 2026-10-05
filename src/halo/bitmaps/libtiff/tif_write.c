/* ===========================================================================
 * tif_write.c -- vendored libtiff (Sam Leffler, rev 1.41 92/02/10),
 * scanline-oriented write support.
 *
 * kb.json's tif_write.obj bucket spans 0x6e740-0x74a30, but only the twelve
 * functions at 0x6f9f0-0x704b6 are upstream tif_write.c; the rest of the
 * bucket is tif_read/tif_strip/tif_swab/tif_thunder/tif_tile/tif_warning and
 * the s3tc/bitmap code that follows. Those twelve are transcribed from
 * upstream libtiff tif_write.c (the vendored-library rule). The binary
 * carries the `c:\halo\SOURCE\bitmaps\libtiff\tif_write.c` __FILE__ string
 * (0x2612f0, pushed with lines 390/392/484/509/511 exactly as upstream's
 * malloc/realloc sites), the per-function module names at 0x2ed00c-0x2ed078,
 * and every function's size and order match the PAL build 2342
 * reconstruction (halo-pal-2342 source/bitmaps/libtiff/tif_write.c). As in
 * PAL, the file-static helpers are emitted first (0x6f9f0-0x6fe0f), then
 * TIFFFlushData1 and the public entry points; 0x703f0 is TIFFWriteRawTile and
 * 0x70460 TIFFWriteTile (their 2276-dump names had landed on 0x71cc0/0x74a30,
 * which are s3tc/bitmap code).
 *
 * ABI: the file-statics carry custom register conventions from the original
 * link, recorded in kb.json as `@<reg>` and read off entry code and call
 * sites:
 *   TIFFSetupStrips    tif@esi                              (0x6f9f0)
 *   TIFFWriteCheck     tif@eax tiles@ecx module@edi         (0x6faf0)
 *   TIFFBufferSetup    tif@esi, module on the stack         (0x6fbd0)
 *   TIFFGrowStrips     tif@esi delta@edi, module on stack   (0x6fc60)
 *   TIFFAppendToStrip  tif@esi strip@edi cc@ebx, data stack (0x6fd30)
 * The public functions are plain cdecl.
 * ======================================================================== */

#include <stdarg.h>
#include "halo/bitmaps/bitmap_data.h"

#define TIFF_BUFFERSETUP 0x4
#define TIFF_BEENWRITING 0x8
#define TIFF_NOBITREV 0x20
#define TIFF_MYBUFFER 0x40 /* `test/or/and byte ptr [esi+0xa],0x40` 0x6e881 */
#define TIFF_ISTILED 0x80
#define TIFF_POSTENCODE 0x200 /* `or byte ptr [esi+0xb],2` */

#define PLANARCONFIG_CONTIG 1 /* CMP word [ecx+0x5e],0x1 at 0x6f8ab */
#define PLANARCONFIG_SEPARATE 2
#define FIELD_IMAGEDIMENSIONS 0
#define FIELD_PLANARCONFIG 20
#define FIELD_STRIPBYTECOUNTS 26
#define FIELD_STRIPOFFSETS 27

#define O_RDONLY 0
#define O_WRONLY 1 /* CMP word [esi+0x6],0x1 at 0x6ea57 */
#define L_SET 0
#define L_XTND 2

typedef int (*tiff_bool_method_t)(void *tif);
typedef int (*tiff_code_method_t)(void *tif, char *buf, int cc, int s);
typedef int (*tiff_seek_method_t)(void *tif, int n);

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

#define TIFF_WRITE_FILE "c:\\halo\\SOURCE\\bitmaps\\libtiff\\tif_write.c"
#define TIFF_READ_FILE "c:\\halo\\SOURCE\\bitmaps\\libtiff\\tif_read.c"

#define BITn(n) (((unsigned)1L) << ((n) & 0x1f))
#define BITFIELDn(tif, n) ((tif)->tif_dir.td_fieldsset[(n) / 32])
#define TIFFFieldSet(tif, field) (BITFIELDn(tif, field) & BITn(field))
#define TIFFSetFieldBit(tif, field) (BITFIELDn(tif, field) |= BITn(field))
#define isTiled(tif) (((tif)->tif_flags & TIFF_ISTILED) != 0)
#define howmany(x, y) \
  ((((unsigned int)(x)) + (((unsigned int)(y)) - 1)) / ((unsigned int)(y)))
#define SeekOK(fd, off) (__lseek(fd, (long)off, L_SET) == (long)off)
#define WriteOK(fd, buf, size) (__write(fd, (char *)buf, size) == size)

/* FUN_00068a30 is TIFFError, FUN_0006a210 TIFFFlushData and FUN_0006f910
 * TIFFTileSize. */

/* 0x6e740 -- upstream libtiff tif_read.c TIFFReadRawStrip1 (file-static
 * there), placed in this kb.json bucket. tif@esi strip@edi size@ebx from
 * entry code; buf at [ebp+8], module at [ebp+0xc]. Callers 0x6ead9 and
 * 0x6eb62. Seek to the strip's offset (+0xbc) and read size bytes into buf;
 * returns size, or -1 after reporting the error. */
int TIFFReadRawStrip1(void *tif_, unsigned int strip, void *buf, int size,
                      const char *module)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_directory_t *td = &tif->tif_dir;

  if (!SeekOK(tif->tif_fd, td->td_stripoffset[strip])) {
    FUN_00068a30(module, "%s: Seek error at scanline %d, strip %d",
                 tif->tif_name, tif->tif_row, strip);
    return (-1);
  }
  if (__read(tif->tif_fd, buf, (unsigned int)size) != size) {
    FUN_00068a30(module, "%s: Read error at scanline %d", tif->tif_name,
                 tif->tif_row);
    return (-1);
  }
  return (size);
}

/* 0x6e7d0 -- upstream libtiff tif_read.c TIFFReadRawTile1 (file-static
 * there), placed in this kb.json bucket. tif@esi tile@edi size@ebx from
 * entry code; buf at [ebp+8], module at [ebp+0xc]. Seek to the tile's offset
 * (+0xbc) and read size bytes into buf; the seek error reports row (+0xd4),
 * col (+0xe4) and tile. Returns size, or -1 after reporting the error. */
int TIFFReadRawTile1(void *tif_, unsigned int tile, void *buf, int size,
                     const char *module)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_directory_t *td = &tif->tif_dir;

  if (!SeekOK(tif->tif_fd, td->td_stripoffset[tile])) {
    FUN_00068a30(module, "%s: Seek error at row %d, col %d, tile %d",
                 tif->tif_name, tif->tif_row, tif->tif_col, tile);
    return (-1);
  }
  if (__read(tif->tif_fd, buf, (unsigned int)size) != size) {
    FUN_00068a30(module, "%s: Read error at row %d, col %d", tif->tif_name,
                 tif->tif_row, tif->tif_col);
    return (-1);
  }
  return (size);
}

/* 0x6e870 -- upstream libtiff tif_read.c TIFFReadBufferSetup, placed in this
 * kb.json bucket. cdecl: tif [ebp+8], bp [ebp+0xc], size [ebp+0x10]; returns
 * 1 in EAX, or 0 after reporting the error. Frees an owned (TIFF_MYBUFFER)
 * tif_rawdata (0x12c), then adopts bp or allocates size rounded up to 1K
 * (shr/shl 10); tif_rawdatasize is 0x130. Assert lines 0x203/0x20c. */
int TIFFReadBufferSetup(void *tif_, void *bp, int size)
{
  tiff_t *tif = (tiff_t *)tif_;

  if (tif->tif_rawdata) {
    if (tif->tif_flags & TIFF_MYBUFFER)
      debug_free(tif->tif_rawdata, TIFF_READ_FILE, 515);
    tif->tif_rawdata = NULL;
  }
  if (bp) {
    tif->tif_flags &= ~TIFF_MYBUFFER;
    tif->tif_rawdatasize = size;
  } else {
    tif->tif_rawdatasize = howmany(size, 1024) * 1024;
    bp = debug_malloc(tif->tif_rawdatasize, 0, TIFF_READ_FILE, 524);
    tif->tif_flags |= TIFF_MYBUFFER;
  }
  tif->tif_rawdata = (char *)bp;
  if (tif->tif_rawdata == NULL) {
    FUN_00068a30("TIFFReadBufferSetup",
                 "%s: No space for data buffer at scanline %d", tif->tif_name,
                 tif->tif_row);
    tif->tif_rawdatasize = 0;
    return (0);
  }
  return (1);
}

/* 0x6e930 -- upstream libtiff tif_read.c TIFFStartStrip (file-static there),
 * placed in this kb.json bucket. tif@ecx strip@esi from entry code; callers
 * 0x6eb98/0x6ed8f. Returns 1 when tif_predecode (0xf0) is NULL, otherwise
 * the callback's result normalized to 0/1. */
int TIFFStartStrip(void *tif_, unsigned int strip)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_directory_t *td = &tif->tif_dir;

  tif->tif_curstrip = strip;
  tif->tif_row = (strip % td->td_stripsperimage) * td->td_rowsperstrip;
  tif->tif_rawcp = tif->tif_rawdata;
  tif->tif_rawcc = td->td_stripbytecount[strip];
  return (tif->tif_predecode == NULL || (*tif->tif_predecode)(tif));
}

/* 0x6e980 -- upstream libtiff tif_read.c TIFFStartTile (file-static there),
 * placed in this kb.json bucket. tif@ecx tile@esi from entry code; caller
 * 0x6ecf8. Row/col use unsigned DIV of howmany(width|length, tilewidth|
 * tilelength). Returns 1 when tif_predecode (0xf0) is NULL, otherwise the
 * callback's result normalized to 0/1. */
int TIFFStartTile(void *tif_, unsigned int tile)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_directory_t *td = &tif->tif_dir;

  tif->tif_curtile = tile;
  tif->tif_row =
    (tile % howmany(td->td_imagewidth, td->td_tilewidth)) * td->td_tilelength;
  tif->tif_col =
    (tile % howmany(td->td_imagelength, td->td_tilelength)) * td->td_tilewidth;
  tif->tif_rawcp = tif->tif_rawdata;
  tif->tif_rawcc = td->td_stripbytecount[tile];
  return (tif->tif_predecode == NULL || (*tif->tif_predecode)(tif));
}

/* 0x6ea50 -- upstream libtiff tif_read.c TIFFReadRawStrip, placed in this
 * kb.json bucket. cdecl: tif [ebp+8], strip [ebp+0xc], buf [ebp+0x10], size
 * [ebp+0x14]. Rejects write-only files (tif_mode == 1, string 0x2610f4),
 * tiled images (flags bit 0x80, 0x26109c) and strip >= td_nstrips (0xb8,
 * 0x261110); each error returns -1. Otherwise clamps td_stripbytecount[strip]
 * (0xc0) to size when size != -1 (unsigned JNC) and calls
 * TIFFReadRawStrip1 with module = the .data string 0x2ecad8. */
int TIFFReadRawStrip(void *tif_, unsigned int strip, void *buf,
                     unsigned long size)
{
  static char module[] = "TIFFReadRawStrip";
  tiff_t *tif = (tiff_t *)tif_;
  tiff_directory_t *td = &tif->tif_dir;
  unsigned long bytecount;

  if (tif->tif_mode == O_WRONLY) {
    FUN_00068a30(tif->tif_name, "File not open for reading");
    return (-1);
  }
  if (isTiled(tif)) {
    FUN_00068a30(tif->tif_name, "Can not read scanlines from a tiled image");
    return (-1);
  }
  if (strip >= td->td_nstrips) {
    FUN_00068a30(tif->tif_name, "%d: Strip out of range, max %d", strip,
                 td->td_nstrips);
    return (-1);
  }
  bytecount = td->td_stripbytecount[strip];
  if (size != (unsigned long)-1 && size < bytecount)
    bytecount = size;
  return (TIFFReadRawStrip1(tif, strip, buf, (int)bytecount, module));
}

/* 0x6eaf0 -- upstream libtiff tif_read.c TIFFFillStrip, placed in this
 * kb.json bucket. cdecl: tif [ebp+8], strip [ebp+0xc]. bytecount is
 * td_stripbytecount[strip] (0xc0), compared unsigned (JBE) with
 * tif_rawdatasize (0x130). module is the .data string 0x2ecaec; the error
 * format is 0x261130. tif_curstrip (0xdc) is set to -1 before the
 * TIFF_MYBUFFER test. Tail calls TIFFStartStrip (tif@ecx strip@esi). */
int TIFFFillStrip(void *tif_, unsigned int strip)
{
  static char module[] = "TIFFFillStrip";
  tiff_t *tif = (tiff_t *)tif_;
  tiff_directory_t *td = &tif->tif_dir;
  unsigned long bytecount;

  bytecount = td->td_stripbytecount[strip];
  if (bytecount > (unsigned long)tif->tif_rawdatasize) {
    tif->tif_curstrip = -1;
    if ((tif->tif_flags & TIFF_MYBUFFER) == 0) {
      FUN_00068a30(module, "%s: Data buffer too small to hold strip %d",
                   tif->tif_name, strip);
      return (0);
    }
    if (!TIFFReadBufferSetup(tif, 0, howmany(bytecount, 1024) * 1024))
      return (0);
  }
  if ((unsigned long)TIFFReadRawStrip1(tif, strip, tif->tif_rawdata,
                                       (int)bytecount, module) != bytecount)
    return (0);
  if (td->td_fillorder != tif->tif_fillorder &&
      (tif->tif_flags & TIFF_NOBITREV) == 0)
    TIFFReverseBits((unsigned char *)tif->tif_rawdata, (int)bytecount);
  return (TIFFStartStrip(tif, strip));
}

/* 0x6ebb0 -- upstream libtiff tif_read.c TIFFReadRawTile, placed in this
 * kb.json bucket. cdecl: tif [ebp+8], tile [ebp+0xc], buf [ebp+0x10], size
 * [ebp+0x14]. Rejects write-only files (tif_mode == 1, string 0x2610f4),
 * stripped images (flags bit 0x80 clear, 0x2610c8) and tile >= td_nstrips
 * (0xb8, 0x26115c); each error returns -1. Otherwise clamps
 * td_stripbytecount[tile] (0xc0) to size when size != -1 (unsigned JNC) and
 * calls TIFFReadRawTile1 (tif@esi tile@edi size@ebx) with module = the .data
 * string 0x2ecafc. */
int TIFFReadRawTile(void *tif_, unsigned int tile, void *buf,
                    unsigned long size)
{
  static char module[] = "TIFFReadRawTile";
  tiff_t *tif = (tiff_t *)tif_;
  tiff_directory_t *td = &tif->tif_dir;
  unsigned long bytecount;

  if (tif->tif_mode == O_WRONLY) {
    FUN_00068a30(tif->tif_name, "File not open for reading");
    return (-1);
  }
  if (!isTiled(tif)) {
    FUN_00068a30(tif->tif_name, "Can not read tiles from a stripped image");
    return (-1);
  }
  if (tile >= td->td_nstrips) {
    FUN_00068a30(tif->tif_name, "%d: Tile out of range, max %d", tile,
                 td->td_nstrips);
    return (-1);
  }
  bytecount = td->td_stripbytecount[tile];
  if (size != (unsigned long)-1 && size < bytecount)
    bytecount = size;
  return (TIFFReadRawTile1(tif, tile, buf, (int)bytecount, module));
}

/* 0x6ec50 -- upstream libtiff tif_read.c TIFFFillTile, placed in this
 * kb.json bucket. cdecl: tif [ebp+8], tile [ebp+0xc]. bytecount is
 * td_stripbytecount[tile] (0xc0), compared unsigned (JBE) with
 * tif_rawdatasize (0x130). module is the .data string 0x2ecb0c; the error
 * format is 0x26117c. tif_curtile (0xe8) is set to -1 before the
 * TIFF_MYBUFFER test. TIFFReadRawTile1 takes tif@esi tile@edi size@ebx;
 * tail calls TIFFStartTile (tif@ecx tile@esi). */
int TIFFFillTile(void *tif_, unsigned int tile)
{
  static char module[] = "TIFFFillTile";
  tiff_t *tif = (tiff_t *)tif_;
  tiff_directory_t *td = &tif->tif_dir;
  unsigned long bytecount;

  bytecount = td->td_stripbytecount[tile];
  if (bytecount > (unsigned long)tif->tif_rawdatasize) {
    tif->tif_curtile = -1;
    if ((tif->tif_flags & TIFF_MYBUFFER) == 0) {
      FUN_00068a30(module, "%s: Data buffer too small to hold tile %d",
                   tif->tif_name, tile);
      return (0);
    }
    if (!TIFFReadBufferSetup(tif, 0, howmany(bytecount, 1024) * 1024))
      return (0);
  }
  if ((unsigned long)TIFFReadRawTile1(tif, tile, tif->tif_rawdata,
                                      (int)bytecount, module) != bytecount)
    return (0);
  if (td->td_fillorder != tif->tif_fillorder &&
      (tif->tif_flags & TIFF_NOBITREV) == 0)
    TIFFReverseBits((unsigned char *)tif->tif_rawdata, (int)bytecount);
  return (TIFFStartTile(tif, tile));
}

/* 0x6ed10 -- upstream libtiff tif_read.c TIFFSeek (file-static there),
 * placed in this kb.json bucket. tif@edi row@ebx sample@ecx from entry code
 * (no prologue; first use MOV EAX,[EDI+0x20] / CMP EBX,EAX / CMP ECX,EAX);
 * caller 0x6f08b. Row test vs td_imagelength (0x20) and sample test vs
 * td_samplesperpixel (0x44, movzx) are unsigned (JC); formats 0x261200 and
 * 0x2611e0. strip = row / td_rowsperstrip (0x48, DIV) plus
 * td_stripsperimage (0xb4) * sample when td_planarconfig (0x5e) == 2.
 * Refill via TIFFFillStrip (cdecl) when strip != tif_curstrip (0xdc), else
 * TIFFStartStrip (tif@ecx strip@esi) when row < tif_row (0xd4, JNC).
 * tif_seek (0x118) is called with row - tif_row; NULL reports 0x2611a8. */
int TIFFSeek(void *tif_, unsigned int row, unsigned int sample)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_directory_t *td = &tif->tif_dir;
  unsigned int strip;

  if (row >= td->td_imagelength) {
    FUN_00068a30(tif->tif_name, "%d: Row out of range, max %d", row,
                 td->td_imagelength);
    return (0);
  }
  if (td->td_planarconfig == PLANARCONFIG_SEPARATE) {
    if (sample >= td->td_samplesperpixel) {
      FUN_00068a30(tif->tif_name, "%d: Sample out of range, max %d", sample,
                   td->td_samplesperpixel);
      return (0);
    }
    strip = sample * td->td_stripsperimage + row / td->td_rowsperstrip;
  } else
    strip = row / td->td_rowsperstrip;
  if (strip != (unsigned int)tif->tif_curstrip) {
    if (!TIFFFillStrip(tif, strip))
      return (0);
  } else if (row < (unsigned long)tif->tif_row) {
    if (!TIFFStartStrip(tif, strip))
      return (0);
  }
  if (row != (unsigned long)tif->tif_row) {
    if (tif->tif_seek == NULL) {
      FUN_00068a30(tif->tif_name,
                   "Compression algorithm does not support random access");
      return (0);
    }
    if (!(*tif->tif_seek)(tif, row - tif->tif_row))
      return (0);
    tif->tif_row = row;
  }
  return (1);
}

/* 0x6ede0 -- upstream libtiff tif_read.c TIFFReadEncodedStrip, placed in this
 * kb.json bucket. cdecl: tif [ebp+8], strip [ebp+0xc], buf [ebp+0x10], size
 * [ebp+0x14]. TIFFStripSize(tif) is called FIRST (0x6ede8) and its EAX is
 * kept live across the guards. Guards (strings 0x2610f4, 0x26109c, 0x261110;
 * strip vs td_nstrips 0xb8 unsigned JC) each return -1. size == -1 or
 * size > stripsize (unsigned JBE) clamps to stripsize. TIFFFillStrip(tif,
 * strip) then tif_decodestrip (0x104) with sample = strip /
 * td_stripsperimage (0xb4, DIV); returns size on success, else -1. No
 * tif_postdecode call in this build. */
long TIFFReadEncodedStrip(void *tif_, unsigned long strip, void *buf, long size)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_directory_t *td = &tif->tif_dir;
  unsigned long stripsize = TIFFStripSize(tif);

  if (tif->tif_mode == O_WRONLY) {
    FUN_00068a30(tif->tif_name, "File not open for reading");
    return (-1);
  }
  if (isTiled(tif)) {
    FUN_00068a30(tif->tif_name, "Can not read scanlines from a tiled image");
    return (-1);
  }
  if (strip >= td->td_nstrips) {
    FUN_00068a30(tif->tif_name, "%d: Strip out of range, max %d", strip,
                 td->td_nstrips);
    return (-1);
  }
  if (size == (long)-1 || (unsigned long)size > stripsize)
    size = (long)stripsize;
  return ((TIFFFillStrip(tif, (unsigned int)strip) &&
           (*tif->tif_decodestrip)(tif, (char *)buf, (int)size,
                                   (int)(strip / td->td_stripsperimage))) ?
            size :
            -1);
}

/* 0x6eea0 -- upstream libtiff tif_read.c TIFFReadTile shape (TIFFCheckRead
 * and TIFFReadEncodedTile inlined). cdecl: tif [ebp+8], buf [ebp+0xc], x
 * [ebp+0x10], y [ebp+0x14], z [ebp+0x18], s [ebp+0x1c]. Guards (strings
 * 0x2610f4, 0x2610c8) return -1. TIFFCheckTile(tif,x,y,z,s) (0x6f780) == 0
 * returns -1. TIFFComputeTile is pushed (tif,x,y,z,s) positionally
 * (0x6ef12-0x6ef16); tile vs td_nstrips (0xb8, unsigned JC) else error
 * 0x26115c and -1. TIFFFillTile(tif, tile) then tif_decodetile (0x10c) with
 * (tif, buf, tif_tilesize (0xec), s); returns tif_tilesize reloaded at
 * 0x6ef73 on success, else -1. */
int FUN_0006eea0(void *tif_, void *buf, unsigned long x, unsigned long y,
                 unsigned long z, unsigned long s)
{
  tiff_t *tif = (tiff_t *)tif_;
  unsigned int tile;

  if (tif->tif_mode == O_WRONLY) {
    FUN_00068a30(tif->tif_name, "File not open for reading");
    return (-1);
  }
  if (!isTiled(tif)) {
    FUN_00068a30(tif->tif_name, "Can not read tiles from a stripped image");
    return (-1);
  }
  if (!TIFFCheckTile(tif, x, y, z, (unsigned int)s))
    return (-1);
  tile = TIFFComputeTile(tif, x, y, (unsigned int)z, s);
  if (tile >= tif->tif_dir.td_nstrips) {
    FUN_00068a30(tif->tif_name, "%d: Tile out of range, max %d", tile,
                 tif->tif_dir.td_nstrips);
    return (-1);
  }
  if (TIFFFillTile(tif, tile) &&
      (*tif->tif_decodetile)(tif, (char *)buf, (int)tif->tif_tilesize, (int)s))
    return ((int)tif->tif_tilesize);
  return (-1);
}

/* 0x6ef80 -- upstream libtiff tif_read.c TIFFReadEncodedTile, placed in this
 * kb.json bucket. cdecl: tif [ebp+8], tile [ebp+0xc], buf [ebp+0x10], size
 * [ebp+0x14]; no callers. Same three guards as TIFFReadRawTile (strings
 * 0x2610f4, 0x2610c8, 0x26115c; tile vs td_nstrips 0xb8 unsigned JC), each
 * returning -1. size == -1 or size > tif_tilesize (0xec, unsigned JBE)
 * clamps to tif_tilesize. TIFFFillTile(tif, tile) then tif_decodetile
 * (0x10c) with sample = tile / td_stripsperimage (0xb4, DIV, pushed as a
 * full dword); returns size on success, else -1. */
long TIFFReadEncodedTile(void *tif_, unsigned int tile, void *buf, long size)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_directory_t *td = &tif->tif_dir;
  unsigned long tilesize = (unsigned long)tif->tif_tilesize;

  if (tif->tif_mode == O_WRONLY) {
    FUN_00068a30(tif->tif_name, "File not open for reading");
    return (-1);
  }
  if (1 ^ isTiled(tif)) {
    FUN_00068a30(tif->tif_name, "Can not read tiles from a stripped image");
    return (-1);
  }
  if (tile >= td->td_nstrips) {
    FUN_00068a30(tif->tif_name, "%d: Tile out of range, max %d", tile,
                 td->td_nstrips);
    return (-1);
  }
  if (size == (long)-1)
    size = (long)tilesize;
  else if ((unsigned long)size > tilesize)
    size = (long)tilesize;
  return ((TIFFFillTile(tif, tile) &&
           (*tif->tif_decodetile)(tif, (char *)buf, (int)size,
                                  (int)(tile / td->td_stripsperimage))) ?
            size :
            -1);
}

/* 0x6f040 -- upstream libtiff tif_read.c TIFFReadScanline shape (TIFFCheckRead
 * inlined). cdecl: tif [ebp+8], buf [ebp+0xc], row [ebp+0x10], sample
 * [ebp+0x14] (full dword). Guards (strings 0x2610f4, 0x26109c) return -1.
 * TIFFSeek(tif@edi, row@ebx, sample@ecx); on success tif_decoderow (0xfc)
 * with (tif, buf, tif_scanlinesize 0x124, sample), then tif_row (0xd4) is
 * incremented unconditionally. Returns (e != 0) ? 1 : -1 (SETNZ/LEA). */
int FUN_0006f040(int file, void *buf, int row, int sample)
{
  tiff_t *tif = (tiff_t *)file;
  int e;

  if (tif->tif_mode == O_WRONLY) {
    FUN_00068a30(tif->tif_name, "File not open for reading");
    return (-1);
  }
  if (isTiled(tif)) {
    FUN_00068a30(tif->tif_name, "Can not read scanlines from a tiled image");
    return (-1);
  }
  if ((e = TIFFSeek(tif, (unsigned int)row, (unsigned int)sample)) != 0) {
    e = (*tif->tif_decoderow)(tif, (char *)buf, tif->tif_scanlinesize, sample);
    tif->tif_row++;
  }
  return (e ? 1 : -1);
}

/* 0x6f0d0 -- upstream libtiff tif_strip.c TIFFComputeStrip, placed in this
 * kb.json bucket. The DIV by td_rowsperstrip precedes the planarconfig test;
 * the error string at 0x2611e0 is "%d: Sample out of range, max %d". */
unsigned long TIFFComputeStrip(void *tif_, unsigned long row,
                               unsigned int sample)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_directory_t *td = &tif->tif_dir;
  unsigned long strip;

  strip = row / td->td_rowsperstrip;
  if (td->td_planarconfig == PLANARCONFIG_SEPARATE) {
    if (sample >= td->td_samplesperpixel) {
      FUN_00068a30(tif->tif_name, "%d: Sample out of range, max %d", sample,
                   td->td_samplesperpixel);
      return (0);
    }
    strip += sample * td->td_stripsperimage;
  }
  return (strip);
}

/* 0x6f120 -- upstream libtiff tif_strip.c TIFFNumberOfStrips (this build has
 * no PLANARCONFIG_SEPARATE multiply): rowsperstrip (+0x48) == -1 yields
 * imagelength (+0x20) != 0, else TIFFhowmany(imagelength, rowsperstrip). */
unsigned long TIFFNumberOfStrips(void *tif_)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_directory_t *td = &tif->tif_dir;
  unsigned long rowsperstrip = td->td_rowsperstrip;

  if (rowsperstrip == (unsigned long)-1) {
    return (td->td_imagelength != 0);
  }
  return ((td->td_imagelength + (rowsperstrip - 1)) / rowsperstrip);
}

/* 0x6f150 -- upstream libtiff tif_strip.c TIFFVStripSize (no YCbCr
 * subsampling branch in this build): nrows == -1 (CMP ESI,-0x1) yields
 * imagelength (+0x20), then IMUL TIFFScanlineSize(tif), nrows. */
unsigned long TIFFVStripSize(void *tif_, unsigned long nrows)
{
  tiff_t *tif = (tiff_t *)tif_;

  if (nrows == (unsigned long)-1)
    nrows = tif->tif_dir.td_imagelength;
  return (TIFFScanlineSize((int)tif) * nrows);
}

/* 0x6f180 -- strip size in bytes: rowsperstrip (+0x48), or imagelength
 * (+0x20) when rowsperstrip == -1, times TIFFScanlineSize(tif). Binary
 * computes IMUL scanline_size, rows (call result first operand). */
__declspec(noinline) unsigned long TIFFStripSize(void *tif_)
{
  tiff_t *tif = (tiff_t *)tif_;
  unsigned long rps = tif->tif_dir.td_rowsperstrip;

  if (rps == (unsigned long)-1)
    rps = tif->tif_dir.td_imagelength;
  return (TIFFScanlineSize((int)tif) * rps);
}

/* 0x6f1b0 -- byte-swap one 16-bit word in place (shape of upstream libtiff
 * tif_swab.c TIFFSwabShort): MOVZX cp[1] into an int temp, cp[1] = cp[0],
 * cp[0] = temp. */
void FUN_0006f1b0(unsigned short *wp)
{
  unsigned char *cp = (unsigned char *)wp;
  int t;

  t = cp[1];
  cp[1] = cp[0];
  cp[0] = (unsigned char)t;
}

/* 0x6f1d0 -- byte-swap one 32-bit long in place (shape of upstream libtiff
 * tif_swab.c TIFFSwabLong): swap cp[0]<->cp[3], then cp[1]<->cp[2]. */
void FUN_0006f1d0(unsigned long *lp)
{
  unsigned char *cp = (unsigned char *)lp;
  int t;

  t = cp[3];
  cp[3] = cp[0];
  cp[0] = (unsigned char)t;
  t = cp[2];
  cp[2] = cp[1];
  cp[1] = (unsigned char)t;
}

/* 0x6f1f0 -- byte-swap n 16-bit words in place (shape of upstream libtiff
 * tif_swab.c TIFFSwabArrayOfShort): per word MOVZX cp[1], cp[1] = cp[0],
 * cp[0] = temp, advance 2 bytes; no work when n <= 0. */
void FUN_0006f1f0(void *wp, int n)
{
  unsigned char *cp = (unsigned char *)wp;
  int t;

  while (n-- > 0) {
    t = cp[1];
    cp[1] = cp[0];
    cp[0] = (unsigned char)t;
    cp += 2;
  }
}

/* 0x6f220 -- byte-swap n 32-bit longs in place (shape of upstream libtiff
 * tif_swab.c TIFFSwabArrayOfLong): per long swap cp[0]<->cp[3] and
 * cp[1]<->cp[2], advance 4 bytes; no work when n <= 0. */
void FUN_0006f220(void *lp, int n)
{
  unsigned char *cp = (unsigned char *)lp;
  int t;

  while (n-- > 0) {
    t = cp[3];
    cp[3] = cp[0];
    cp[0] = (unsigned char)t;
    t = cp[2];
    cp[2] = cp[1];
    cp[1] = (unsigned char)t;
    cp += 4;
  }
}

/* 0x6f260 -- upstream libtiff tif_swab.c TIFFReverseBits: replace each byte
 * with its bit-reversed value from the 256-byte table at 0x2ecbe0
 * (`mov dl,[edx+0x2ecbe0]`), 8 bytes per pass while n > 8 (signed JLE at
 * 0x6f26a), then one byte at a time while n > 0 (signed JLE at 0x6f2f4). */
void TIFFReverseBits(unsigned char *cp, int n)
{
  const unsigned char *bitrev = (const unsigned char *)0x2ecbe0;

  for (; n > 8; n -= 8) {
    cp[0] = bitrev[cp[0]];
    cp[1] = bitrev[cp[1]];
    cp[2] = bitrev[cp[2]];
    cp[3] = bitrev[cp[3]];
    cp[4] = bitrev[cp[4]];
    cp[5] = bitrev[cp[5]];
    cp[6] = bitrev[cp[6]];
    cp[7] = bitrev[cp[7]];
    cp += 8;
  }
  while (n-- > 0)
    *cp = bitrev[*cp], cp++;
}

/* 0x6f620 -- upstream libtiff tif_thunder.c ThunderDecodeRow: while occ > 0
 * (signed JLE/JG), decode one row via ThunderDecode(tif, row, imagewidth
 * +0x1c) (3 cdecl pushes, ADD ESP,0xc, EAX tested); a zero return yields 0.
 * occ and row advance by tif_scanlinesize (+0x124). s is never read. */
int ThunderDecodeRow(void *tif_, char *buf, int occ, int s)
{
  tiff_t *tif = (tiff_t *)tif_;
  char *row = buf;

  (void)s;
  while (occ > 0) {
    if (!ThunderDecode(tif, row, tif->tif_dir.td_imagewidth))
      return (0);
    occ -= tif->tif_scanlinesize;
    row += tif->tif_scanlinesize;
  }
  return (1);
}

/* 0x6f670 -- upstream libtiff tif_thunder.c TIFFInitThunderScan: stores
 * ThunderDecodeRow (0x6f620) into tif_decoderow (+0xfc) then
 * tif_decodestrip (+0x104); returns 1 (MOV EAX,0x1). scheme is never read. */
int TIFFInitThunderScan(void *tif_, int scheme)
{
  tiff_t *tif = (tiff_t *)tif_;

  (void)scheme;
  tif->tif_decoderow = ThunderDecodeRow;
  tif->tif_decodestrip = ThunderDecodeRow;
  return (1);
}

/* 0x6f690 -- upstream libtiff tif_tile.c TIFFComputeTile, but with the binary
 * slot order (tif, x, y, s, z): [ebp+0x18] is zeroed when td_imagedepth
 * (+0x24) == 1 and divided by dz, [ebp+0x14] is multiplied by zpt. dx/dy/dz
 * (+0x28/+0x2c/+0x30) fall back to imagewidth/length/depth when -1. The
 * multiply chain is Horner-ordered (IMUL ECX,s ... IMUL ECX,ypt ... IMUL
 * ECX,xpt); zpt is only computed when td_planarconfig (+0x5e) == 2. */
unsigned int TIFFComputeTile(void *tif_, unsigned long x, unsigned long y,
                             unsigned int s, unsigned long z)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_directory_t *td = &tif->tif_dir;
  unsigned long dx = td->td_tilewidth;
  unsigned long dy = td->td_tilelength;
  unsigned long dz = td->td_tiledepth;
  unsigned int tile = 1;

  if (td->td_imagedepth == 1)
    z = 0;
  if (dx == (unsigned long)-1)
    dx = td->td_imagewidth;
  if (dy == (unsigned long)-1)
    dy = td->td_imagelength;
  if (dz == (unsigned long)-1)
    dz = td->td_imagedepth;
  if (dx != 0 && dy != 0 && dz != 0) {
    unsigned long xpt = (td->td_imagewidth + dx - 1) / dx;
    unsigned long ypt = (td->td_imagelength + dy - 1) / dy;

    if (td->td_planarconfig == PLANARCONFIG_SEPARATE) {
      unsigned long zpt = (td->td_imagedepth + dz - 1) / dz;

      tile = ((zpt * s + z / dz) * ypt + y / dy) * xpt + x / dx;
    } else {
      tile = x / dx + (s + ((z / dz) * ypt + y / dy) * xpt);
    }
  }
  return (tile);
}

/* 0x6f780 -- upstream libtiff tif_tile.c TIFFCheckTile: x/y/z tested
 * unsigned (JC) against td_imagewidth/length/depth (+0x1c/+0x20/+0x24);
 * sample s against td_samplesperpixel (+0x44, movzx) only when
 * td_planarconfig (+0x5e) == 2. Each failure reports via TIFFError
 * (0x68a30, cdecl, 4 pushes: name, fmt 0x2612c8/0x2612ac/0x26128c/
 * 0x26126c, value, max) and returns 0; success returns 1. */
int TIFFCheckTile(void *tif_, unsigned long x, unsigned long y, unsigned long z,
                  unsigned int s)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_directory_t *td = &tif->tif_dir;

  if (x >= td->td_imagewidth) {
    FUN_00068a30(tif->tif_name, "Col %d out of range, max %d", x,
                 td->td_imagewidth);
    return (0);
  }
  if (y >= td->td_imagelength) {
    FUN_00068a30(tif->tif_name, "Row %d out of range, max %d", y,
                 td->td_imagelength);
    return (0);
  }
  if (z >= td->td_imagedepth) {
    FUN_00068a30(tif->tif_name, "Depth %d out of range, max %d", z,
                 td->td_imagedepth);
    return (0);
  }
  if (td->td_planarconfig == PLANARCONFIG_SEPARATE &&
      s >= td->td_samplesperpixel) {
    FUN_00068a30(tif->tif_name, "Sample %d out of range, max %d", s,
                 td->td_samplesperpixel);
    return (0);
  }
  return (1);
}

/* 0x6f820 -- upstream libtiff tif_tile.c TIFFNumberOfTiles (this build has
 * no PLANARCONFIG_SEPARATE multiply): tilewidth/length/depth (+0x28/+0x2c/
 * +0x30) == -1 fall back to imagewidth/length/depth (+0x1c/+0x20/+0x24); any
 * zero tile dimension yields 0, else the product of unsigned howmany(). */
unsigned int TIFFNumberOfTiles(void *tif_)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_directory_t *td = &tif->tif_dir;
  unsigned long dx = td->td_tilewidth;
  unsigned long dy = td->td_tilelength;
  unsigned long dz = td->td_tiledepth;

  if (dx == (unsigned long)-1)
    dx = td->td_imagewidth;
  if (dy == (unsigned long)-1)
    dy = td->td_imagelength;
  if (dz == (unsigned long)-1)
    dz = td->td_imagedepth;
  return ((dx == 0 || dy == 0 || dz == 0) ?
            0 :
            (howmany(td->td_imagewidth, dx) * howmany(td->td_imagelength, dy) *
             howmany(td->td_imagedepth, dz)));
}

/* 0x6f890 -- upstream libtiff tif_tile.c TIFFTileRowSize: 0 when
 * tilelength (+0x2c) or tilewidth (+0x28) is zero, else
 * howmany(bitspersample * tilewidth [* samplesperpixel if contig], 8). */
int FUN_0006f890(void *tif_)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_directory_t *td = &tif->tif_dir;
  int rowsize;

  if (td->td_tilelength == 0 || td->td_tilewidth == 0)
    return (0);
  rowsize = td->td_bitspersample * td->td_tilewidth;
  if (td->td_planarconfig == PLANARCONFIG_CONTIG)
    rowsize *= td->td_samplesperpixel;
  return ((int)howmany(rowsize, 8));
}

/* 0x6f8d0 -- upstream libtiff tif_tile.c TIFFVTileSize: 0 when tilelength
 * (+0x2c), tilewidth (+0x28) or tiledepth (+0x30) is zero, else
 * TIFFTileRowSize(tif) * tiledepth * nrows (IMUL EAX,ESI then
 * IMUL [EBP+0xc] at 0x6f8f2-0x6f8f8). */
unsigned long TIFFVTileSize(void *tif_, unsigned long nrows)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_directory_t *td = &tif->tif_dir;

  if (td->td_tilelength == 0 || td->td_tilewidth == 0 || td->td_tiledepth == 0)
    return (0);
  return ((unsigned long)FUN_0006f890(tif) * td->td_tiledepth * nrows);
}

/* 0x6f910 -- upstream libtiff tif_tile.c TIFFTileSize: 0 when tilelength
 * (+0x2c), tilewidth (+0x28) or tiledepth (+0x30) is zero, else
 * TIFFTileRowSize * tilelength * tiledepth (IMUL [esi+0x2c], IMUL edi). */
unsigned long FUN_0006f910(void *tif_)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_directory_t *td = &tif->tif_dir;

  if (td->td_tilelength == 0 || td->td_tilewidth == 0 || td->td_tiledepth == 0)
    return (0);
  return ((unsigned long)FUN_0006f890(tif) * td->td_tilelength *
          td->td_tiledepth);
}

/* 0x6f950 -- upstream libtiff tif_warning.c defaultHandler (the default
 * warning handler). Same shape as tif_error.c's FUN_000689c0 plus the
 * "Warning, " fprintf that upstream carries:
 *   0x6f953 mov eax,[ebp+8]; test; jz  -> if (module != NULL)
 *   0x6f95a push module / 0x259f68 "%s: " / 0x331070; call fprintf; add 0xc
 *   0x6f96d push 0x2612e4 "Warning, " / 0x331070; call fprintf
 *   0x6f97c push ap / fmt / 0x331070; call vfprintf (0x1d9850)
 *   0x6f98e push 0x260020 ".\n" / 0x331070; call fprintf
 *   0x6f99d add esp,0x1c -- coalesced cleanup (8+0xc+8), no EAX set.
 * 0x331070 is the ADDRESS of MSVC's stderr record (&_iob[2]). */
#define tif_write_crt_stderr ((void *)0x331070)

void defaultHandler(const char *module, const char *fmt, char *ap)
{
  if (module != NULL)
    crt_fprintf(tif_write_crt_stderr, "%s: ", module);
  crt_fprintf(tif_write_crt_stderr, "Warning, ");
  FUN_001d9850(tif_write_crt_stderr, fmt, ap); /* vfprintf */
  crt_fprintf(tif_write_crt_stderr, ".\n");
}

/* 0x6f9d0 -- upstream libtiff tif_warning.c TIFFWarning (tif_dir.c and
 * tif_dirwrite.c call sites identify it). Same shape as TIFFError
 * (FUN_00068a30): load the handler word at 0x2ecfac, skip when NULL, else
 * push (&args, fmt, module) and CALL EAX with ADD ESP,0xc (0x6f9d3-0x6f9ea).
 * The slot's pointee shape is inferred from that 3-arg cdecl call site and
 * the TIFFError sibling; no static initializer was checked for this word. */
typedef void (*tiff_warning_handler_t)(const char *module, const char *fmt,
                                       char *ap);

#define _TIFFwarningHandler (*(tiff_warning_handler_t *)0x2ecfac)

/* 0x6f9b0 -- upstream libtiff tif_warning.c TIFFSetWarningHandler: EAX gets
 * the old 0x2ecfac word, then [ebp+8] is stored there (0x6f9b3-0x6f9bb).
 * Parameter/return kept as void * because decl.h cannot see the local
 * handler typedef. */
void *TIFFSetWarningHandler(void *handler)
{
  tiff_warning_handler_t prev = _TIFFwarningHandler;

  _TIFFwarningHandler = (tiff_warning_handler_t)handler;
  return (void *)prev;
}

void FUN_0006f9d0(const char *module, const char *format, ...)
{
  va_list ap;

  if (_TIFFwarningHandler != NULL) {
    va_start(ap, format);
    (*_TIFFwarningHandler)(module, format, (char *)ap);
    va_end(ap);
  }
}

/* 0x6f9f0-- size and allocate the strip (or tile) offset/bytecount
 * arrays for the first write. */
int TIFFSetupStrips(void *tif_)
{
#define isUnspecified(td, v) (td->v == 0xffffffff || (td)->td_imagelength == 0)
  tiff_t *tif = (tiff_t *)tif_;
  tiff_directory_t *td = &tif->tif_dir;

  if (!isTiled(tif))
    td->td_stripsperimage = isUnspecified(td, td_rowsperstrip) ?
                              1 :
                              howmany(td->td_imagelength, td->td_rowsperstrip);
  else
    td->td_stripsperimage =
      isUnspecified(td, td_tilelength) ? 1 : TIFFNumberOfTiles(tif);
  td->td_nstrips = td->td_stripsperimage;
  if (td->td_planarconfig == PLANARCONFIG_SEPARATE)
    td->td_nstrips *= td->td_samplesperpixel;
  td->td_stripoffset = (unsigned long *)debug_malloc(
    td->td_nstrips * sizeof(unsigned long), 0, TIFF_WRITE_FILE, 390);
  td->td_stripbytecount = (unsigned long *)debug_malloc(
    td->td_nstrips * sizeof(unsigned long), 0, TIFF_WRITE_FILE, 392);
  if (td->td_stripoffset == NULL || td->td_stripbytecount == NULL)
    return (0);
  /*
   * Place data at the end-of-file
   * (by setting offsets to zero).
   */
  csmemset((char *)td->td_stripoffset, 0,
           td->td_nstrips * sizeof(unsigned long));
  csmemset((char *)td->td_stripbytecount, 0,
           td->td_nstrips * sizeof(unsigned long));
  TIFFSetFieldBit(tif, FIELD_STRIPOFFSETS);
  TIFFSetFieldBit(tif, FIELD_STRIPBYTECOUNTS);
  return (1);
#undef isUnspecified
}

/* 0x6faf0 -- verify the file is writable and that the directory information
 * is setup properly; on the first write also "freeze" the directory. */
int TIFFWriteCheck(void *tif_, int tiles, char *module)
{
  tiff_t *tif = (tiff_t *)tif_;

  if (tif->tif_mode == O_RDONLY) {
    FUN_00068a30(module, "%s: File not open for writing", tif->tif_name);
    return (0);
  }
  if (tiles ^ isTiled(tif)) {
    FUN_00068a30(tif->tif_name, tiles ?
                                  "Can not write tiles to a stripped image" :
                                  "Can not write scanlines to a tiled image");
    return (0);
  }
  /*
   * On the first write verify all the required information
   * has been setup and initialize any data structures that
   * had to wait until directory information was set.
   * Note that a lot of our work is assumed to remain valid
   * because we disallow any of the important parameters
   * from changing after we start writing (i.e. once
   * TIFF_BEENWRITING is set, TIFFSetField will only allow
   * the image's length to be changed).
   */
  if ((tif->tif_flags & TIFF_BEENWRITING) == 0) {
    if (!TIFFFieldSet(tif, FIELD_IMAGEDIMENSIONS)) {
      FUN_00068a30(module, "%s: Must set \"ImageWidth\" before writing data",
                   tif->tif_name);
      return (0);
    }
    if (!TIFFFieldSet(tif, FIELD_PLANARCONFIG)) {
      FUN_00068a30(module,
                   "%s: Must set \"PlanarConfiguration\" before writing data",
                   tif->tif_name);
      return (0);
    }
    if (tif->tif_dir.td_stripoffset == NULL && !TIFFSetupStrips(tif)) {
      tif->tif_dir.td_nstrips = 0;
      FUN_00068a30(module, "%s: No space for %s arrays", tif->tif_name,
                   isTiled(tif) ? "tile" : "strip");
      return (0);
    }
    tif->tif_flags |= TIFF_BEENWRITING;
  }
  return (1);
}

/* 0x6fbd0 -- setup the raw data buffer used for encoding. */
int TIFFBufferSetup(void *tif_, char *module)
{
  tiff_t *tif = (tiff_t *)tif_;
  int size;

  if (isTiled(tif))
    tif->tif_tilesize = size = FUN_0006f910(tif);
  else
    tif->tif_scanlinesize = size = TIFFScanlineSize((int)tif);
  /*
   * Make raw data buffer at least 8K
   */
  if (size < 8 * 1024)
    size = 8 * 1024;
  tif->tif_rawdata = (char *)debug_malloc(size, 0, TIFF_WRITE_FILE, 484);
  if (tif->tif_rawdata == NULL) {
    FUN_00068a30(module, "%s: No space for output buffer", tif->tif_name);
    return (0);
  }
  tif->tif_rawdatasize = size;
  tif->tif_rawcc = 0;
  tif->tif_rawcp = tif->tif_rawdata;
  return (1);
}

/* 0x6fc60 -- grow the strip data structures by delta strips. */
int TIFFGrowStrips(void *tif_, int delta, char *module)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_directory_t *td = &tif->tif_dir;

  td->td_stripoffset = (unsigned long *)debug_realloc(
    td->td_stripoffset, (td->td_nstrips + delta) * sizeof(unsigned long),
    TIFF_WRITE_FILE, 509);
  td->td_stripbytecount = (unsigned long *)debug_realloc(
    td->td_stripbytecount, (td->td_nstrips + delta) * sizeof(unsigned long),
    TIFF_WRITE_FILE, 511);
  if (td->td_stripoffset == NULL || td->td_stripbytecount == NULL) {
    td->td_nstrips = 0;
    FUN_00068a30(module, "%s: No space to expand strip arrays", tif->tif_name);
    return (0);
  }
  csmemset((char *)td->td_stripoffset + td->td_nstrips, 0,
           delta * sizeof(unsigned long));
  csmemset((char *)td->td_stripbytecount + td->td_nstrips, 0,
           delta * sizeof(unsigned long));
  td->td_nstrips += delta;
  return (1);
}

/* 0x6fd30 -- append the data to the specified strip. We don't check that
 * there's space in the file (i.e. that strips do not overlap). */
int TIFFAppendToStrip(void *tif_, unsigned int strip, unsigned char *data,
                      unsigned int cc)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_directory_t *td = &tif->tif_dir;

  if (td->td_stripoffset[strip] == 0 || tif->tif_curoff == 0) {
    /*
     * No current offset, set the current strip.
     */
    if (td->td_stripoffset[strip] != 0) {
      if (!SeekOK(tif->tif_fd, td->td_stripoffset[strip])) {
        FUN_00068a30("TIFFAppendToStrip", "%s: Seek error at scanline %d",
                     tif->tif_name, tif->tif_row);
        return (0);
      }
    } else
      td->td_stripoffset[strip] = __lseek(tif->tif_fd, 0L, L_XTND);
    tif->tif_curoff = td->td_stripoffset[strip];
  }
  if (!WriteOK(tif->tif_fd, data, cc)) {
    FUN_00068a30("TIFFAppendToStrip", "%s: Write error at scanline %d",
                 tif->tif_name, tif->tif_row);
    return (0);
  }
  tif->tif_curoff += cc;
  td->td_stripbytecount[strip] += cc;
  return (1);
}

/* 0x6fe10 -- internal version of TIFFFlushData that can be called by
 * ``encodestrip routines'' w/o concern for infinite recursion. */
int TIFFFlushData1(void *tif_)
{
  tiff_t *tif = (tiff_t *)tif_;

  if (tif->tif_rawcc > 0) {
    if (tif->tif_dir.td_fillorder != tif->tif_fillorder &&
        (tif->tif_flags & TIFF_NOBITREV) == 0)
      TIFFReverseBits((unsigned char *)tif->tif_rawdata, tif->tif_rawcc);
    if (!TIFFAppendToStrip(tif,
                           isTiled(tif) ? tif->tif_curtile : tif->tif_curstrip,
                           (unsigned char *)tif->tif_rawdata, tif->tif_rawcc))
      return (0);
    tif->tif_rawcc = 0;
    tif->tif_rawcp = tif->tif_rawdata;
  }
  return (1);
}

/* 0x6fea0 -- write one scanline, growing the image (PlanarConfig=1 only)
 * and the strip arrays as needed. */
int TIFFWriteScanline(int file, void *buffer, unsigned int row,
                      unsigned int sample)
{
  tiff_t *tif = (tiff_t *)file;
  tiff_directory_t *td;
  int strip, status, imagegrew = 0;

  if (!TIFFWriteCheck(tif, 0, "TIFFWriteScanline"))
    return (-1);
  /*
   * Handle delayed allocation of data buffer.  This
   * permits it to be sized more intelligently (using
   * directory information).
   */
  if ((tif->tif_flags & TIFF_BUFFERSETUP) == 0) {
    if (!TIFFBufferSetup(tif, "TIFFWriteScanline"))
      return (-1);
    tif->tif_flags |= TIFF_BUFFERSETUP;
  }
  td = &tif->tif_dir;
  /*
   * Extend image length if needed
   * (but only for PlanarConfig=1).
   */
  if (row >= td->td_imagelength) { /* extend image */
    if (td->td_planarconfig == PLANARCONFIG_SEPARATE) {
      FUN_00068a30(tif->tif_name,
                   "Can not change \"ImageLength\" when using separate planes");
      return (-1);
    }
    td->td_imagelength = row + 1;
    imagegrew = 1;
  }
  /*
   * Calculate strip and check for crossings.
   */
  if (td->td_planarconfig == PLANARCONFIG_SEPARATE) {
    if (sample >= td->td_samplesperpixel) {
      FUN_00068a30(tif->tif_name, "%d: Sample out of range, max %d", sample,
                   td->td_samplesperpixel);
      return (-1);
    }
    strip = sample * td->td_stripsperimage + row / td->td_rowsperstrip;
  } else
    strip = row / td->td_rowsperstrip;
  if (strip != tif->tif_curstrip) {
    /*
     * Changing strips -- flush any data present.
     */
    if (tif->tif_rawcc > 0 && !FUN_0006a210(tif))
      return (-1);
    tif->tif_curstrip = strip;
    /*
     * Watch out for a growing image.  The value of
     * strips/image will initially be 1 (since it
     * can't be deduced until the imagelength is known).
     */
    if (strip >= td->td_stripsperimage && imagegrew)
      td->td_stripsperimage = howmany(td->td_imagelength, td->td_rowsperstrip);
    tif->tif_row = (strip % td->td_stripsperimage) * td->td_rowsperstrip;
    if (tif->tif_preencode && !(*tif->tif_preencode)(tif))
      return (-1);
    tif->tif_flags |= TIFF_POSTENCODE;
  }
  /*
   * Check strip array to make sure there's space.
   * We don't support dynamically growing files that
   * have data organized in separate bitplanes because
   * it's too painful.  In that case we require that
   * the imagelength be set properly before the first
   * write (so that the strips array will be fully
   * allocated above).
   */
  if (strip >= td->td_nstrips && !TIFFGrowStrips(tif, 1, "TIFFWriteScanline"))
    return (-1);
  /*
   * Ensure the write is either sequential or at the
   * beginning of a strip (or that we can randomly
   * access the data -- i.e. no encoding).
   */
  if (row != tif->tif_row) {
    if (tif->tif_seek) {
      if (row < tif->tif_row) {
        /*
         * Moving backwards within the same strip:
         * backup to the start and then decode
         * forward (below).
         */
        tif->tif_row = (strip % td->td_stripsperimage) * td->td_rowsperstrip;
        tif->tif_rawcp = tif->tif_rawdata;
      }
      /*
       * Seek forward to the desired row.
       */
      if (!(*tif->tif_seek)(tif, row - tif->tif_row))
        return (-1);
      tif->tif_row = row;
    } else {
      FUN_00068a30(tif->tif_name,
                   "Compression algorithm does not support random access");
      return (-1);
    }
  }
  status = (*tif->tif_encoderow)(tif, buffer, tif->tif_scanlinesize, sample);
  tif->tif_row++;
  return (status);
}

/* 0x700c0 -- encode the supplied data and write it to the specified strip.
 * There must be space for the data; we don't check if strips overlap! */
int TIFFWriteEncodedStrip(void *tif_, unsigned int strip, unsigned char *data,
                          unsigned int cc)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_directory_t *td = &tif->tif_dir;

  if (!TIFFWriteCheck(tif, 0, "TIFFWriteEncodedStrip"))
    return (-1);
  if (strip >= td->td_nstrips) {
    FUN_00068a30("TIFFWriteEncodedStrip", "%s: Strip %d out of range, max %d",
                 tif->tif_name, strip, td->td_nstrips);
    return (-1);
  }
  /*
   * Handle delayed allocation of data buffer.  This
   * permits it to be sized according to the directory
   * info.
   */
  if ((tif->tif_flags & TIFF_BUFFERSETUP) == 0) {
    if (!TIFFBufferSetup(tif, "TIFFWriteEncodedStrip"))
      return (-1);
    tif->tif_flags |= TIFF_BUFFERSETUP;
  }
  tif->tif_curstrip = strip;
  tif->tif_flags &= ~TIFF_POSTENCODE;
  if (tif->tif_preencode && !(*tif->tif_preencode)(tif))
    return (-1);
  if (!(*tif->tif_encodestrip)(tif, (char *)data, cc,
                               strip / td->td_stripsperimage))
    return (0);
  if (tif->tif_postencode && !(*tif->tif_postencode)(tif))
    return (-1);
  if (td->td_fillorder != tif->tif_fillorder &&
      (tif->tif_flags & TIFF_NOBITREV) == 0)
    TIFFReverseBits((unsigned char *)tif->tif_rawdata, tif->tif_rawcc);
  if (tif->tif_rawcc > 0 &&
      !TIFFAppendToStrip(tif, strip, (unsigned char *)tif->tif_rawdata,
                         tif->tif_rawcc))
    return (-1);
  tif->tif_rawcc = 0;
  tif->tif_rawcp = tif->tif_rawdata;
  return (cc);
}

/* 0x701f0 -- write the supplied data to the specified strip. */
int TIFFWriteRawStrip(void *tif_, unsigned int strip, unsigned char *data,
                      unsigned int cc)
{
  tiff_t *tif = (tiff_t *)tif_;

  if (!TIFFWriteCheck(tif, 0, "TIFFWriteRawStrip"))
    return (-1);
  if (strip >= tif->tif_dir.td_nstrips) {
    FUN_00068a30("TIFFWriteRawStrip", "%s: Strip %d out of range, max %d",
                 tif->tif_name, strip, tif->tif_dir.td_nstrips);
    return (-1);
  }
  return (TIFFAppendToStrip(tif, strip, data, cc) ? cc : -1);
}

/* 0x70260 -- encode the supplied data and write it to the specified tile,
 * clamping the write to the tile size. */
int TIFFWriteEncodedTile(void *tif_, unsigned int tile, unsigned char *data,
                         unsigned int cc)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_directory_t *td;

  if (!TIFFWriteCheck(tif, 1, "TIFFWriteEncodedTile"))
    return (-1);
  td = &tif->tif_dir;
  if (tile >= td->td_nstrips) {
    FUN_00068a30("TIFFWriteEncodedTile", "%s: Tile %d out of range, max %d",
                 tif->tif_name, tile, td->td_nstrips);
    return (-1);
  }
  /*
   * Handle delayed allocation of data buffer.  This
   * permits it to be sized more intelligently (using
   * directory information).
   */
  if ((tif->tif_flags & TIFF_BUFFERSETUP) == 0) {
    if (!TIFFBufferSetup(tif, "TIFFWriteEncodedTile"))
      return (-1);
    tif->tif_flags |= TIFF_BUFFERSETUP;
  }
  tif->tif_curtile = tile;
  /*
   * Compute tiles per row & per column to compute
   * current row and column
   */
  tif->tif_row =
    (tile % howmany(td->td_imagelength, td->td_tilelength)) * td->td_tilelength;
  tif->tif_col =
    (tile % howmany(td->td_imagewidth, td->td_tilewidth)) * td->td_tilewidth;

  tif->tif_flags &= ~TIFF_POSTENCODE;
  if (tif->tif_preencode && !(*tif->tif_preencode)(tif))
    return (-1);
  /*
   * Clamp write amount to the tile size.  This is mostly
   * done so that callers can pass in some large number
   * (e.g. -1) and have the tile size used instead.
   */
  if (cc > tif->tif_tilesize)
    cc = tif->tif_tilesize;
  if (!(*tif->tif_encodetile)(tif, (char *)data, cc,
                              tile / td->td_stripsperimage))
    return (0);
  if (tif->tif_postencode && !(*tif->tif_postencode)(tif))
    return (-1);
  if (td->td_fillorder != tif->tif_fillorder &&
      (tif->tif_flags & TIFF_NOBITREV) == 0)
    TIFFReverseBits((unsigned char *)tif->tif_rawdata, tif->tif_rawcc);
  if (tif->tif_rawcc > 0 &&
      !TIFFAppendToStrip(tif, tile, (unsigned char *)tif->tif_rawdata,
                         tif->tif_rawcc))
    return (-1);
  tif->tif_rawcc = 0;
  tif->tif_rawcp = tif->tif_rawdata;
  return (cc);
}

/* 0x703f0 -- write the supplied data to the specified tile. */
int TIFFWriteRawTile(void *tif_, unsigned int tile, unsigned char *data,
                     unsigned int cc)
{
  tiff_t *tif = (tiff_t *)tif_;

  if (!TIFFWriteCheck(tif, 1, "TIFFWriteRawTile"))
    return (-1);
  if (tile >= tif->tif_dir.td_nstrips) {
    FUN_00068a30("TIFFWriteRawTile", "%s: Tile %d out of range, max %d",
                 tif->tif_name, tile, tif->tif_dir.td_nstrips);
    return (-1);
  }
  return (TIFFAppendToStrip(tif, tile, data, cc) ? cc : -1);
}

/* 0x70460 -- write and compress a tile of data selected by (x,y,z,s).
 * Upstream passes (x, y, z, s) to a TIFFComputeTile whose K&R definition
 * names its parameters (x, y, s, z); the binary pushes the caller's order
 * unchanged (0x70497-0x7049e), so the call is transcribed as written.
 *
 * NB: A tile size of -1 is used instead of tif_tilesize knowing that
 *     TIFFWriteEncodedTile will clamp this to the tile size. This is done
 *     because the tile size may not be defined until after the output
 *     buffer is setup in TIFFBufferSetup. */
int TIFFWriteTile(void *tif, unsigned char *buf, unsigned long x,
                  unsigned long y, unsigned long z, unsigned int s)
{
  if (!TIFFCheckTile(tif, x, y, z, s))
    return (-1);
  return (TIFFWriteEncodedTile(tif, TIFFComputeTile(tif, x, y, z, s), buf,
                               (unsigned int)-1));
}

/* 0x704c0 -- expand a 3-byte color into 3 floats: out[i] = (float)color[i]
 * * per-channel scale (0x2ed08c/0x2ed090/0x2ed094) * shared scale (0x261518).
 * color arrives in ECX, out in EAX (read at entry, 0x704c4/0x704d9); no stack
 * args. Scale constant meanings are unproven. */
void ColorToFcolor(unsigned char *color, float *out)
{
  out[0] = (float)color[0] * *(float *)0x2ed08c * *(float *)0x261518;
  out[1] = (float)color[1] * *(float *)0x2ed090 * *(float *)0x261518;
  out[2] = (float)color[2] * *(float *)0x2ed094 * *(float *)0x261518;
}

/* 0x70570 -- pack a 3-byte color into a 16-bit 5:6:5 word: color[2] in
 * bits 11-15, color[1] in bits 5-10, color[0] in bits 0-4. color arrives in
 * EAX (read at entry, 0x70573-0x70580); out is the only stack arg ([EBP+8]). */
void ColorToRGB(unsigned char *color, unsigned short *out)
{
  *out = (unsigned short)((((unsigned short)(color[2] >> 3) << 6 |
                            (unsigned short)(color[1] >> 2))
                           << 5) |
                          (unsigned short)(color[0] >> 3));
}

typedef struct s3tc_color_s {
  unsigned char rgba[4];
} s3tc_color_t;

/* 0x705b0 -- expand a 16-bit 5:6:5 word into a 4-byte color, replicating
 * each channel's high bits into its low bits; byte 3 is zero. rgb arrives
 * in EAX; color is the only stack arg. */
void RGBToColor(unsigned short *rgb, unsigned char *color)
{
  unsigned short value;
  unsigned char c;
  s3tc_color_t result;

  value = *rgb;
  c = (unsigned char)(value << 3);
  c |= c >> 5;
  result.rgba[0] = c;
  value >>= 5;
  c = (unsigned char)(value << 2);
  c |= c >> 6;
  result.rgba[1] = c;
  value >>= 6;
  c = (unsigned char)(value << 3);
  c |= c >> 5;
  result.rgba[2] = c;
  result.rgba[3] = 0;
  *(s3tc_color_t *)color = result;
}


/* 0x70a00 -- fill a 4x4 color block whose pixels share one color. pixels
 * (16 dwords) arrives in EDX, block in ESI, mask in DI (read at entry,
 * 0x70a06-0x70a39). Stores dword 0 at block+4, then the 5:6:5 packing of
 * pixels[0] (same formula as ColorToRGB) into block[0] and block[1]. When
 * mask != 0xffff, walks 16 pixels (0x70a60-0x70b31): a clear mask bit ORs
 * 3 << (2*i) into the dword at block+4, a set bit records pixels[i]; the
 * last recorded pixel (pixels[0] if none) is re-packed into block[0] and
 * block[1] (0x70b37-0x70b65). Param meanings beyond these operations are
 * unproven. */
void AllSame(unsigned long *pixels, unsigned short *block, unsigned short mask)
{
  unsigned long last;
  unsigned short color;
  unsigned short bit;
  unsigned long code;
  int i;
  int j;

  last = pixels[0];
  color =
    (unsigned short)((((unsigned short)(((unsigned char *)pixels)[2] >> 3)
                         << 6 |
                       (unsigned short)(((unsigned char *)pixels)[1] >> 2))
                      << 5) |
                     (unsigned short)(((unsigned char *)pixels)[0] >> 3));
  *(unsigned long *)(block + 2) = 0;
  block[0] = color;
  block[1] = color;
  if (mask != 0xffff) {
    bit = 1;
    code = 3;
    for (i = 0; i < 2; i++) {
      for (j = 0; j < 8; j++) {
        if ((bit & mask) == 0) {
          *(unsigned long *)(block + 2) |= code;
        } else {
          last = pixels[i * 8 + j];
        }
        bit <<= 1;
        code <<= 2;
      }
    }
    color =
      (unsigned short)((((unsigned short)(((unsigned char *)&last)[2] >> 3)
                           << 6 |
                         (unsigned short)(((unsigned char *)&last)[1] >> 2))
                        << 5) |
                       (unsigned short)(((unsigned char *)&last)[0] >> 3));
    block[0] = color;
    block[1] = color;
  }
}

/* 0x71400 -- decode an 8-byte 5:6:5 color block into 16 four-byte pixels.
 * NULL block zero-fills 0x40 bytes of out_pixels (0x7140e-0x71415). Word 0
 * is expanded inline (same byte formula as RGBToColor, 0x71426-0x71470);
 * word 1 via RGBToColor(EAX=block+2, &colors[1]) (0x7146a-0x71473). Alpha
 * of colors 0-2 is 0xff. word0 > word1 (JBE, 0x7147b): colors 2/3 are the
 * signed /3 blends (c1 + 2*c0 + 1), (c0 + 2*c1 + 1), alpha 0xff; else
 * color 2 is the signed /2 average and color 3 is all zero. Then 16 two-bit
 * indices from the dword at block+4 pick each pixel (0x71572-0x715aa). */
void FUN_00071400(void *block, unsigned int *out_pixels)
{
  unsigned short *words;
  unsigned char colors[4][4];
  unsigned short word0;
  unsigned short c0;
  unsigned short c1;
  unsigned long value;
  unsigned char b;
  unsigned char g;
  unsigned char r;
  unsigned long bits;
  unsigned char *out;
  int idx;
  int i;

  words = (unsigned short *)block;
  if (words == NULL) {
    csmemset(out_pixels, 0, 0x40);
    return;
  }
  word0 = words[0];
  value = word0;
  b = (unsigned char)(((unsigned char *)&value)[0] << 3);
  b |= b >> 5;
  *(unsigned short *)&value >>= 5;
  g = (unsigned char)(((unsigned char *)&value)[0] << 2);
  g |= g >> 6;
  r = (unsigned char)((unsigned char)(value >> 6) << 3);
  r |= r >> 5;
  ((unsigned char *)&value)[0] = b;
  ((unsigned char *)&value)[1] = g;
  ((unsigned char *)&value)[2] = r;
  *(unsigned long *)colors[0] = value;
  RGBToColor(&words[1], colors[1]);
  colors[2][3] = 0xff;
  colors[1][3] = 0xff;
  colors[0][3] = 0xff;
  if (word0 > words[1]) {
    c0 = colors[0][0];
    c1 = colors[1][0];
    colors[2][0] = (unsigned char)((c1 + c0 * 2 + 1) / 3);
    colors[3][0] = (unsigned char)((c0 + c1 * 2 + 1) / 3);
    c0 = colors[0][1];
    c1 = colors[1][1];
    colors[2][1] = (unsigned char)((c1 + c0 * 2 + 1) / 3);
    colors[3][1] = (unsigned char)((c0 + c1 * 2 + 1) / 3);
    c0 = colors[0][2];
    c1 = colors[1][2];
    colors[2][2] = (unsigned char)((c1 + c0 * 2 + 1) / 3);
    colors[3][2] = (unsigned char)((c0 + c1 * 2 + 1) / 3);
    colors[3][3] = 0xff;
  } else {
    for (i = 0; i < 3; i++) {
      colors[2][i] =
        (unsigned char)(((int)colors[0][i] + (int)colors[1][i]) / 2);
      colors[3][i] = 0;
    }
    colors[3][3] = 0;
  }
  bits = *(unsigned long *)(words + 2);
  out = (unsigned char *)out_pixels;
  for (i = 0; i < 16; i++) {
    idx = bits & 3;
    out[0] = colors[idx][0];
    out[1] = colors[idx][1];
    out[2] = colors[idx][2];
    out[3] = colors[idx][3];
    bits >>= 2;
    out += 4;
  }
}

/* 0x715c0 -- single-pixel variant of FUN_00071400: builds the same four-color
 * table (word 0 inline 0x715e4-0x71630, word 1 via RGBToColor(EAX=block+2,
 * &colors[1]) 0x71627-0x71638), asserts (short)x and (short)y are in [0,4]
 * (s3tc.c 0x305/0x306), then stores colors[(bits >> ((x + y*4)*2)) & 3] as a
 * dword to *pixel (0x71790-0x717a7). NULL block memsets 0x40 bytes of pixel
 * (0x715ce-0x715da). cdecl, four stack args. */
void DecodeBlockRGB__single_pixel(void *block, uint32_t *pixel, int x, int y)
{
  unsigned short *words;
  unsigned char colors[4][4];
  unsigned short word0;
  unsigned short c0;
  unsigned short c1;
  unsigned long value;
  unsigned char b;
  unsigned char g;
  unsigned char r;
  short u;
  short v;
  int i;

  words = (unsigned short *)block;
  if (words == NULL) {
    csmemset(pixel, 0, 0x40);
    return;
  }
  word0 = words[0];
  value = word0;
  b = (unsigned char)(((unsigned char *)&value)[0] << 3);
  b |= b >> 5;
  *(unsigned short *)&value >>= 5;
  g = (unsigned char)(((unsigned char *)&value)[0] << 2);
  g |= g >> 6;
  r = (unsigned char)((unsigned char)(value >> 6) << 3);
  r |= r >> 5;
  ((unsigned char *)&value)[0] = b;
  ((unsigned char *)&value)[1] = g;
  ((unsigned char *)&value)[2] = r;
  *(unsigned long *)colors[0] = value;
  RGBToColor(&words[1], colors[1]);
  colors[2][3] = 0xff;
  colors[1][3] = 0xff;
  colors[0][3] = 0xff;
  if (word0 > words[1]) {
    c0 = colors[0][0];
    c1 = colors[1][0];
    colors[2][0] = (unsigned char)((c1 + c0 * 2 + 1) / 3);
    colors[3][0] = (unsigned char)((c0 + c1 * 2 + 1) / 3);
    c0 = colors[0][1];
    c1 = colors[1][1];
    colors[2][1] = (unsigned char)((c1 + c0 * 2 + 1) / 3);
    colors[3][1] = (unsigned char)((c0 + c1 * 2 + 1) / 3);
    c0 = colors[0][2];
    c1 = colors[1][2];
    colors[2][2] = (unsigned char)((c1 + c0 * 2 + 1) / 3);
    colors[3][2] = (unsigned char)((c0 + c1 * 2 + 1) / 3);
    colors[3][3] = 0xff;
  } else {
    for (i = 0; i < 3; i++) {
      colors[2][i] =
        (unsigned char)(((int)colors[0][i] + (int)colors[1][i]) / 2);
      colors[3][i] = 0;
    }
    colors[3][3] = 0;
  }
  u = (short)x;
  assert_halt_msg_at("u>=0 && u<=4", "c:\\halo\\SOURCE\\bitmaps\\s3tc\\s3tc.c",
                     0x305, u >= 0 && u <= 4);
  v = (short)y;
  assert_halt_msg_at("v>=0 && v<=4", "c:\\halo\\SOURCE\\bitmaps\\s3tc\\s3tc.c",
                     0x306, v >= 0 && v <= 4);
  *pixel = *(
    uint32_t *)colors[(*(unsigned long *)(words + 2) >> ((x + y * 4) * 2)) & 3];
}

/* 0x717b0 -- decode a 16-byte block whose first 8 bytes are four 16-bit
 * rows of 4-bit values: FUN_00071400 decodes block+8 into out_pixels
 * (0x717bc-0x717c1), then each 4-bit value n becomes byte (n | n << 4) at
 * byte 3 of out_pixels[i * 4 + j], low nibble first (0x717d0-0x7182c).
 * cdecl, two stack args ([EBP+8] block, [EBP+0xc] out_pixels). */
void FUN_000717b0(void *block, unsigned int *out_pixels)
{
  int i;
  int j;
  unsigned short value;

  FUN_00071400((char *)block + 8, out_pixels);
  for (i = 0; i < 4; i++) {
    value = ((unsigned short *)block)[i];
    for (j = 0; j < 4; j++) {
      ((unsigned char *)&out_pixels[i * 4 + j])[3] =
        (unsigned char)((value & 0xf) | (value << 4));
      value >>= 4;
    }
  }
}

/* 0x71840 -- single-pixel variant of FUN_000717b0: calls
 * DecodeBlockRGB__single_pixel(block + 8, pixel, x, y) (0x71852-0x71859),
 * then takes the 4-bit value at nibble x of 16-bit row (short)y of block
 * (MOVSX EDX,SI; SHL CL,0x2; SHR AX,CL; AND 0xf) and stores (n << 4 | n)
 * at byte 3 of *pixel (0x7185e-0x7187d). cdecl, four stack args. */
void FUN_00071840(void *block, uint32_t *pixel, int x, int y)
{
  unsigned char n;

  DecodeBlockRGB__single_pixel((char *)block + 8, pixel, x, y);
  n = (unsigned char)(((unsigned short *)block)[(short)y] >>
                      (unsigned char)((unsigned char)x << 2)) &
      0xf;
  ((unsigned char *)pixel)[3] = (unsigned char)(n << 4 | n);
}

/* 0x71890 -- alpha-block decode: FUN_00071400(block + 8, out_pixels)
 * (0x7189f-0x718b2), then builds an 8-entry table from block[0]/block[1]
 * (MOVZX, signed IMUL-magic /7 when block[0] > block[1] via JLE at 0x718bd,
 * else /5 with entries 6/7 = 0/0xff, 0x718c3-0x719e6). For each of the 16
 * pixels, 3-bit indices come from the 24-bit little-endian value at
 * block + 2 (i == 0) or block + 5 (i == 8), reloaded when (i & 7) == 0
 * (0x719f0-0x71a15); byte 3 of out_pixels[i] = table[bits & 7], bits >>= 3.
 * cdecl, two stack args ([EBP+8] block, [EBP+0xc] out_pixels). */
void FUN_00071890(void *block, unsigned int *out_pixels)
{
  unsigned char *src;
  unsigned int bits;
  int i;
  int alpha[8];

  src = (unsigned char *)block;
  bits = 0;
  FUN_00071400(src + 8, out_pixels);
  alpha[0] = src[0];
  alpha[1] = src[1];
  if (alpha[0] > alpha[1]) {
    alpha[2] = (alpha[0] * 6 + alpha[1]) / 7;
    alpha[3] = (alpha[0] * 5 + alpha[1] * 2) / 7;
    alpha[4] = (alpha[0] * 4 + alpha[1] * 3) / 7;
    alpha[5] = (alpha[0] * 3 + alpha[1] * 4) / 7;
    alpha[6] = (alpha[0] * 2 + alpha[1] * 5) / 7;
    alpha[7] = (alpha[0] + alpha[1] * 6) / 7;
  } else {
    alpha[2] = (alpha[0] * 4 + alpha[1]) / 5;
    alpha[3] = (alpha[0] * 3 + alpha[1] * 2) / 5;
    alpha[4] = (alpha[0] * 2 + alpha[1] * 3) / 5;
    alpha[5] = (alpha[0] + alpha[1] * 4) / 5;
    alpha[6] = 0;
    alpha[7] = 0xff;
  }
  for (i = 0; i < 16; i++) {
    if ((i & 7) == 0) {
      if (i == 0) {
        bits = src[2] | ((src[4] << 8 | src[3]) << 8);
      } else {
        bits = src[5] | ((src[7] << 8 | src[6]) << 8);
      }
    }
    ((unsigned char *)&out_pixels[i])[3] = (unsigned char)alpha[bits & 7];
    bits >>= 3;
  }
}

/* 0x71af0 -- single-pixel variant of FUN_00071890: calls
 * DecodeBlockRGB__single_pixel(block + 8, pixel, x, y) (0x71b05-0x71b0c),
 * then builds an 8-entry 16-bit table ([EBP-0x10]) from block[0]/block[1]
 * (MOVZX AX/DX; unsigned CMP AX,DX + JBE at 0x71b2e selects signed
 * IMUL-magic /7 when block[0] > block[1], else /5 with entries 6/7 = 0/0xff).
 * The 24-bit index word comes from block + 2 when (short)y < 2 (CMP DI,0x2;
 * JGE at 0x71c5e), else block + 5 with the index biased by -8; the entry
 * table[(bits >> ((x + y * 4) * 3)) & 7] low byte is stored at byte 3 of
 * *pixel (0x71c87-0x71c99). cdecl, four stack args. */
void FUN_00071af0(void *block, uint32_t *pixel, int x, int y)
{
  unsigned char *src;
  unsigned int bits;
  int shift;
  int a0;
  int a1;
  unsigned short alpha[8];

  src = (unsigned char *)block;
  DecodeBlockRGB__single_pixel(src + 8, pixel, x, y);
  alpha[0] = src[0];
  alpha[1] = src[1];
  a1 = alpha[1];
  a0 = alpha[0];
  if (alpha[0] > alpha[1]) {
    alpha[2] = (unsigned short)((a0 * 6 + a1) / 7);
    alpha[3] = (unsigned short)((a0 * 5 + a1 * 2) / 7);
    alpha[4] = (unsigned short)((a0 * 4 + a1 * 3) / 7);
    alpha[5] = (unsigned short)((a0 * 3 + a1 * 4) / 7);
    alpha[6] = (unsigned short)((a0 * 2 + a1 * 5) / 7);
    alpha[7] = (unsigned short)((a0 + a1 * 6) / 7);
  } else {
    alpha[2] = (unsigned short)((a0 * 4 + a1) / 5);
    alpha[3] = (unsigned short)((a0 * 3 + a1 * 2) / 5);
    alpha[4] = (unsigned short)((a0 * 2 + a1 * 3) / 5);
    alpha[5] = (unsigned short)((a0 + a1 * 4) / 5);
    alpha[6] = 0;
    alpha[7] = 0xff;
  }
  if ((short)y < 2) {
    bits = src[2] | ((src[4] << 8 | src[3]) << 8);
    shift = x + y * 4;
  } else {
    bits = src[5] | ((src[7] << 8 | src[6]) << 8);
    shift = x + y * 4 - 8;
  }
  ((unsigned char *)pixel)[3] = (unsigned char)alpha[(bits >> (shift * 3)) & 7];
}

/* 0x71ca0 -- forward both stack args ([EBP+8], [EBP+0xc]) to
 * EncodeBlockRGBColorKey with a third arg of 0 (PUSH 0x0, 0x71ca9;
 * ADD ESP,0xc after the call). cdecl; param types are unproven. */
void EncodeBlockRGB(void *param_1, void *param_2)
{
  EncodeBlockRGBColorKey(param_1, param_2, 0);
}

/* 0x71cc0 -- pack the high nibble of byte 3 of each of 16 four-byte
 * pixels into four 16-bit words at out[0..3]; per word the four source bytes
 * are read at pixels + 0xf + 16*i, stepping back 4 bytes each
 * (0x71ce0-0x71d11). Each word is shifted left 4 in place before OR-ing in the
 * next nibble. Then calls EncodeBlockRGBColorKey(pixels, (char *)out + 8, 0)
 * (0x71d13-0x71d25). cdecl, two stack args ([EBP+8] pixels, [EBP+0xc] out);
 * param meanings beyond this arithmetic are unproven. */
void FUN_00071cc0(unsigned char *pixels, unsigned short *out)
{
  unsigned char *row;
  unsigned char *src;
  unsigned short *dst;
  int i;
  int j;

  row = pixels + 0xf;
  dst = out;
  for (i = 4; i != 0; i--) {
    src = row;
    for (j = 4; j != 0; j--) {
      *dst <<= 4;
      *dst |= (unsigned short)(*src >> 4);
      src -= 4;
    }
    dst++;
    row += 0x10;
  }
  EncodeBlockRGBColorKey(pixels, (char *)out + 8, 0);
}

/* 0x71d30 -- encode an 8-byte 3-bit-index alpha block from byte 3 of each
 * of 16 four-byte pixels, then EncodeBlockRGBColorKey(pixels, out + 8, 0)
 * (0x71f81-0x71f8d). cdecl, two stack args ([EBP+8] pixels, [EBP+0xc] out).
 * Pass 1 (0x71d50-0x71d9e) takes max/min alpha. When max == 0xff and
 * min == 0, pass 2 (0x71db8-0x71e7b) restarts at 0xff/0 and takes the
 * smallest alpha != 0 and the largest alpha != 0xff; if that range is
 * non-empty the 6-step form (key, codes 6/7 for 0/0xff) is used, else
 * 0xff/0. out[0]/out[1] = endpoints; equal endpoints zero out[2..7]
 * (0x71f69). Otherwise each 3-bit code is packed from pixel 15 down to 0,
 * 24 bits flushed to out[5..7] at i == 8 and out[2..4] at i == 0. */
void EncodeBlockAlpha3(unsigned char *pixels, unsigned char *out)
{
  unsigned char *p;
  unsigned char hi;
  unsigned char lo;
  unsigned char a;
  unsigned int bits;
  int key;
  int range;
  int steps;
  int half;
  int q;
  int i;

  lo = pixels[3];
  bits = 0;
  hi = lo;
  p = pixels + 0xb;
  for (i = 3; i != 0; i--) {
    a = p[-4];
    if (a > hi) {
      hi = a;
    }
    if (a < lo) {
      lo = a;
    }
    a = p[0];
    if (a > hi) {
      hi = a;
    }
    if (a < lo) {
      lo = a;
    }
    a = p[4];
    if (a > hi) {
      hi = a;
    }
    if (a < lo) {
      lo = a;
    }
    a = p[8];
    if (a > hi) {
      hi = a;
    }
    if (a < lo) {
      lo = a;
    }
    a = p[0xc];
    if (a > hi) {
      hi = a;
    }
    if (a < lo) {
      lo = a;
    }
    p += 0x14;
  }
  if (hi == 0xff && lo == 0) {
    p = pixels + 7;
    for (i = 2; i != 0; i--) {
      a = p[-4];
      if (a < hi && a != 0) {
        hi = a;
      }
      if (a > lo && a != 0xff) {
        lo = a;
      }
      a = p[0];
      if (a < hi && a != 0) {
        hi = a;
      }
      if (a > lo && a != 0xff) {
        lo = a;
      }
      a = p[4];
      if (a < hi && a != 0) {
        hi = a;
      }
      if (a > lo && a != 0xff) {
        lo = a;
      }
      a = p[8];
      if (a < hi && a != 0) {
        hi = a;
      }
      if (a > lo && a != 0xff) {
        lo = a;
      }
      a = p[0xc];
      if (a < hi && a != 0) {
        hi = a;
      }
      if (a > lo && a != 0xff) {
        lo = a;
      }
      a = p[0x10];
      if (a < hi && a != 0) {
        hi = a;
      }
      if (a > lo && a != 0xff) {
        lo = a;
      }
      a = p[0x14];
      if (a < hi && a != 0) {
        hi = a;
      }
      if (a > lo && a != 0xff) {
        lo = a;
      }
      a = p[0x18];
      if (a < hi && a != 0) {
        hi = a;
      }
      if (a > lo && a != 0xff) {
        lo = a;
      }
      p += 0x20;
    }
    if (hi < lo) {
      key = 1;
    } else {
      hi = 0xff;
      lo = 0;
      key = 0;
    }
  } else {
    key = 0;
  }
  out[0] = hi;
  out[1] = lo;
  if (hi != lo) {
    range = (int)hi - (int)lo;
    half = range >> 1;
    steps = key ? 5 : 7;
    for (i = 15; i >= 0; i--) {
      bits <<= 3;
      if (key != 0 && pixels[i * 4 + 3] == 0) {
        bits |= 6;
      } else if (key != 0 && pixels[i * 4 + 3] == 0xff) {
        bits |= 7;
      } else {
        q = (((int)hi - (int)pixels[i * 4 + 3]) * steps + half) / range;
        if (q >= steps) {
          bits |= 1;
        } else if (q > 0) {
          bits |= (unsigned int)q + 1;
        }
      }
      if ((i & 7) == 0) {
        if (i == 8) {
          out[5] = (unsigned char)bits;
          bits >>= 8;
          out[6] = (unsigned char)bits;
          bits >>= 8;
          out[7] = (unsigned char)bits;
        } else {
          out[2] = (unsigned char)bits;
          bits >>= 8;
          out[3] = (unsigned char)bits;
          bits >>= 8;
          out[4] = (unsigned char)bits;
        }
      }
    }
  } else {
    out[2] = out[3] = out[4] = out[5] = out[6] = out[7] = 0;
  }
  EncodeBlockRGBColorKey(pixels, out + 8, 0);
}

/* 0x71fa0 -- initialize a Bresenham-style line record from two 16-bit
 * (x,y) points. cdecl, three stack args ([EBP+8] line, [EBP+0xc] point0,
 * [EBP+0x10] point1); no calls. Line layout, all int16 unless noted (field
 * meanings beyond the arithmetic are unproven):
 *   +0x0 2*|dx|  +0x2 2*|dy|  +0x4 sign(dx)  +0x6 sign(dy)
 *   +0x8 dx      +0xa dy      +0xc initial error term
 *   +0xe point0 (dword copy)  +0x12 point1 (dword copy)
 * dx/dy are point1 - point0. */
void bitmap_initialize_line(short *line, short *point0, short *point1)
{
  short adx2;
  short ady2;

  line[4] = (short)(point1[0] - point0[0]);
  line[5] = (short)(point1[1] - point0[1]);
  adx2 = (short)((line[4] < 0 ? -line[4] : line[4]) * 2);
  line[0] = adx2;
  ady2 = (short)((line[5] < 0 ? -line[5] : line[5]) * 2);
  line[1] = ady2;
  line[2] = (short)(line[4] != 0 ? (line[4] >= 0 ? 1 : -1) : 0);
  line[3] = (short)(line[5] != 0 ? (line[5] >= 0 ? 1 : -1) : 0);
  *(unsigned long *)(line + 7) = *(unsigned long *)point0;
  *(unsigned long *)(line + 9) = *(unsigned long *)point1;
  line[6] = (short)(adx2 > ady2 ? ady2 - (adx2 >> 1) : adx2 - (ady2 >> 1));
}

/* 0x72060 -- advance a line record built by bitmap_initialize_line by one
 * step. cdecl, two stack args ([EBP+8] line, [EBP+0xc] int16 mode read with
 * MOVSX); no calls. Returns the byte local [EBP-1] in AL: 1 when the current
 * point on the major axis (+0xe x-major, +0x10 y-major) already equals the
 * end point (+0x12 / +0x14), else 0. x-major when +0x0 > +0x2 (0x72071).
 * Mode 0 takes one step; mode 2 (x-major, 0x720a2) or mode 1 (y-major,
 * 0x72139) steps the major axis only while the error term +0xc is negative.
 * Other modes do nothing. Mode meanings are unproven. */
bool bitmap_step_line(short *line, short mode)
{
  short adx2;
  short ady2;

  bool done = 0;

  adx2 = line[0];
  ady2 = line[1];

  if (adx2 > ady2) {
    if (line[7] == line[9]) {
      done = 1;
    } else {
      switch (mode) {
      case 0:

        if (line[6] >= 0) {
          line[8] += line[3];
          line[6] -= adx2;
        }
        line[7] = (short)(line[2] + line[7]);
        line[6] += ady2;
        break;
      case 2:
        while (line[6] < 0 && line[7] != line[9]) {
          line[6] += ady2;
          line[7] = (short)(line[2] + line[7]);
        }
        break;
      }
    }
  } else {
    if (line[8] == line[10]) {
      done = 1;
    } else {
      switch (mode) {
      case 0:

        if (line[6] >= 0) {
          line[7] += line[2];
          line[6] -= ady2;
        }
        line[8] = (short)(line[3] + line[8]);
        line[6] += adx2;
        break;
      case 1:
        while (line[6] < 0 && line[8] != line[10]) {
          line[6] += adx2;
          line[8] = (short)(line[3] + line[8]);
        }
        break;
      }
    }
  }
  return done;
}

/* 0x721a0 -- fill a rectangle of a 16-bit 5:6:5 bitmap (encoding #6 at
 * +0xc) with an A8R8G8B8 color. cdecl, four stack args: destination
 * ([EBP+8]), color ([EBP+0xc]), rectangle ([EBP+0x10], four int16 copied as
 * two dwords to [EBP-0x30]: [0]/[2] are clamped to the height at +0x6,
 * [1]/[3] to the width at +0x4), and an optional clip rectangle ([EBP+0x14])
 * passed with the original rectangle and the local copy to
 * intersect_rectangles2d (returns early when it returns 0 in AL).
 * Alpha 0xff selects mode 0 (plain fill of each row, REP STOSD/STOSW);
 * otherwise mode 2 (5:6:5 blend). Mode 1 (5:5:5 blend) is unreachable here
 * (0x72283: SETZ/DEC/AND 2). The blend cases never advance the row pointer:
 * the binary loads the first pixel once (0x72378 / 0x723fc), blends it
 * width times and stores it once (0x723c6 / 0x72448). Kept as found. */
void bitmap_fill_rectangle(void *destination, unsigned int color,
                           short *rectangle, short *clip_rectangle)
{
  short bounds[4];
  short alpha;
  short inverse_alpha;
  unsigned int color16;
  short mode;
  short x0;
  short x1;
  short y;
  short y1;
  short width;
  short x;
  short format;
  unsigned short *pixel;

  *(unsigned long *)&bounds[0] = *(unsigned long *)&rectangle[0];
  *(unsigned long *)&bounds[2] = *(unsigned long *)&rectangle[2];
  assert_halt_at("c:\\halo\\SOURCE\\bitmaps\\bitmap_drawing.c", 0x113,
                 destination);
  if (clip_rectangle != NULL &&
      !intersect_rectangles2d(rectangle, clip_rectangle, bounds)) {
    return;
  }
  alpha = (short)(color >> 24);
  inverse_alpha = (short)(0xff - (color >> 24));
  format = *(short *)((char *)destination + 0xc);
  if (format != 6) {
    display_assert(csprintf((char *)0x5ab100,
                            "bitmap @%p has bad encoding #%d for "
                            "fill_rectangle()",
                            destination, (int)format),
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_drawing.c", 0x12d, 1);
    system_exit(-1);
  }
  color16 = ((((color >> 16) & 0xf8) << 5) | ((color >> 8) & 0xfc)) << 3 |
            ((color >> 3) & 0x1f);
  mode = (short)(alpha == 0xff ? 0 : 2);
  if (bounds[1] < 0) {
    x0 = 0;
  } else if (bounds[1] > (short)*(unsigned short *)((char *)destination + 4)) {
    x0 = (short)*(unsigned short *)((char *)destination + 4);
  } else {
    x0 = bounds[1];
  }
  if (bounds[3] < 0) {
    x1 = 0;
  } else if (bounds[3] > (short)*(unsigned short *)((char *)destination + 4)) {
    x1 = (short)*(unsigned short *)((char *)destination + 4);
  } else {
    x1 = bounds[3];
  }
  if (bounds[0] < 0) {
    y = 0;
  } else if (bounds[0] > (short)*(unsigned short *)((char *)destination + 6)) {
    y = (short)*(unsigned short *)((char *)destination + 6);
  } else {
    y = bounds[0];
  }
  if (bounds[2] < 0) {
    y1 = 0;
  } else if (bounds[2] > (short)*(unsigned short *)((char *)destination + 6)) {
    y1 = (short)*(unsigned short *)((char *)destination + 6);
  } else {
    y1 = bounds[2];
  }
  width = (short)(x1 - x0);
  for (; y < y1; y++) {
    pixel = (unsigned short *)bitmap_2d_address(destination, x0, y, 0);
    switch (mode) {
    case 0:
      for (x = 0; x < width; x++) {
        pixel[x] = (unsigned short)color16;
      }
      break;
    case 1:
      for (x = 0; x < width; x++) {
        *pixel =
          (unsigned short)((((int)(*pixel * inverse_alpha + color16 * alpha) >>
                             8) &
                            0x7c00) |
                           (((int)((*pixel & 0x3ff) * inverse_alpha +
                                   (color16 & 0x3ff) * alpha) >>
                             8) &
                            0x3e0) |
                           (((int)((*pixel & 0x1f) * inverse_alpha +
                                   (color16 & 0x1f) * alpha) >>
                             8) &
                            0x1f));
      }
      break;
    case 2:
      for (x = 0; x < width; x++) {
        *pixel =
          (unsigned short)((((int)(*pixel * inverse_alpha + color16 * alpha) >>
                             8) &
                            0xf800) |
                           (((int)((*pixel & 0x7ff) * inverse_alpha +
                                   (color16 & 0x7ff) * alpha) >>
                             8) &
                            0x7e0) |
                           (((int)((*pixel & 0x1f) * inverse_alpha +
                                   (color16 & 0x1f) * alpha) >>
                             8) &
                            0x1f));
      }
      break;
    }
  }
}

/* 0x72f70 -- tile the frames of one sequence of a 'bitm' tag into a
 * rectangle of the destination bitmap. cdecl, seven stack args:
 * destination ([EBP+8]; int16 +4/+6 give the default rectangle {0,0,+6,+4}
 * when rectangle [EBP+0x14] is NULL), bitmap tag index ([EBP+0xc], -1 does
 * nothing), sequence index ([EBP+0x10], checked against the block count at
 * tag+0x54, element size 0x40, frame count int16 at element+0x22), optional
 * clip rectangle ([EBP+0x18], intersect_rectangles2d(clip, bounds, bounds)),
 * param_6 ([EBP+0x1c], forwarded as FUN_00072490's sixth arg and ORs 2 into
 * its seventh) and param_7 ([EBP+0x20], a mask tested with bit
 * table_261564[i]*2 to select a frame and bit table_261564[i]*2+1 to set 1 in
 * FUN_00072490's seventh arg). For each of 9 entries i, flags come from the
 * dword table at 0x2edae8: bits 4/8/1/2 place the frame's height (+6) or
 * width (+4) against one rectangle edge, 0x20 clamps with the running
 * limits, 0x10 selects the clipped copy and updates the limits. Each flag
 * slot is reused for a later value (0x732e1/0x732e6/0x732fe), kept as found.
 * Meanings of the tables and param_6/param_7 are unproven. */
void bitmap_tile_and_bevel_rectangle(void *destination, int bitmap_tag_index,
                                     short sequence_index, short *rectangle,
                                     short *clip_rectangle, int param_6,
                                     unsigned int param_7)
{
  short default_rectangle[4];
  short bounds[4];
  short clipped[4];
  short limit_max[4];
  short limit_min[4];
  short tile[4];
  short point[2];
  int edge4;
  void *sequence;
  int edge8;
  int i;
  int edge2;
  int frame;
  int edge1;
  void *tag;
  void *frame_bitmap;
  unsigned int flags;
  short *limits;
  short width;
  short height;
  short columns;
  short column;

  if (rectangle == NULL) {
    rectangle = default_rectangle;
    default_rectangle[1] = 0;
    default_rectangle[0] = 0;
    default_rectangle[3] = *(short *)((char *)destination + 4);
    default_rectangle[2] = *(short *)((char *)destination + 6);
  }
  *(unsigned long *)&bounds[0] = *(unsigned long *)&rectangle[0];
  *(unsigned long *)&bounds[2] = *(unsigned long *)&rectangle[2];
  limit_min[2] = rectangle[1];
  limit_min[0] = rectangle[1];
  limit_max[2] = rectangle[3];
  limit_max[0] = rectangle[3];
  limit_min[3] = rectangle[0];
  limit_min[1] = rectangle[0];
  limit_max[3] = rectangle[2];
  limit_max[1] = rectangle[2];
  if (bitmap_tag_index == -1) {
    return;
  }
  if (clip_rectangle != NULL &&
      !intersect_rectangles2d(clip_rectangle, bounds, bounds)) {
    return;
  }
  tag = tag_get(0x6269746d, bitmap_tag_index);
  if (sequence_index >= *(int *)((char *)tag + 0x54)) {
    return;
  }
  sequence = tag_block_get_element((char *)tag + 0x54, sequence_index, 0x40);
  i = 0;
  frame = 0;
  do {
    if ((short)frame >= *(short *)((char *)sequence + 0x22)) {
      return;
    }
    if ((param_7 & (1 << (((unsigned short *)0x261564)[(short)i] << 1))) != 0) {
      frame_bitmap =
        FUN_00077040(bitmap_tag_index, sequence_index, (short)frame);
      frame++;
      if (frame_bitmap != NULL) {
        *(unsigned long *)&clipped[0] = *(unsigned long *)&bounds[0];
        flags = ((unsigned int *)0x2edae8)[(short)i];
        *(unsigned long *)&tile[2] = *(unsigned long *)&rectangle[2];
        *(unsigned long *)&tile[0] = *(unsigned long *)&rectangle[0];
        *(unsigned long *)&clipped[2] = *(unsigned long *)&bounds[2];
        edge4 = flags & 4;
        if (edge4 != 0) {
          tile[2] = *(short *)((char *)frame_bitmap + 6) + rectangle[0];
        }
        edge8 = flags & 8;
        if (edge8 != 0) {
          tile[0] = rectangle[2] - *(short *)((char *)frame_bitmap + 6);
        }
        edge1 = flags & 1;
        if (edge1 != 0) {
          tile[3] = *(short *)((char *)frame_bitmap + 4) + rectangle[1];
        }
        edge2 = flags & 2;
        if (edge2 != 0) {
          tile[1] = rectangle[3] - *(short *)((char *)frame_bitmap + 4);
        }
        if ((flags & 0x20) != 0) {
          limits = (flags & 0x10) != 0 ? clipped : tile;
          if (edge4 != 0) {
            limits[1] = limit_min[0] > limits[1] ? limit_min[0] : limits[1];
            limits[3] = limit_max[0] > limits[3] ? limits[3] : limit_max[0];
          }
          if (edge8 != 0) {
            limits[1] = limit_min[2] > limits[1] ? limit_min[2] : limits[1];
            limits[3] = limit_max[2] > limits[3] ? limits[3] : limit_max[2];
          }
          if (edge1 != 0) {
            limits[0] = limit_min[1] > limits[0] ? limit_min[1] : limits[0];
            limits[2] = limit_max[1] > limits[2] ? limits[2] : limit_max[1];
          }
          if (edge2 != 0) {
            limits[0] = limit_min[3] > limits[0] ? limit_min[3] : limits[0];
            limits[2] = limit_max[3] > limits[2] ? limits[2] : limit_max[3];
          }
        }
        if ((flags & 0x10) != 0) {
          if (edge4 != 0 && edge1 != 0) {
            limit_min[0] = tile[3];
            limit_min[1] = tile[2];
          }
          if (edge8 != 0 && edge1 != 0) {
            limit_min[2] = tile[3];
            limit_max[1] = tile[0];
          }
          if (edge4 != 0 && edge2 != 0) {
            limit_max[0] = tile[1];
            limit_min[3] = tile[2];
          }
          if (edge8 != 0 && edge2 != 0) {
            limit_max[2] = tile[1];
            limit_max[3] = tile[0];
          }
        }
        if (intersect_rectangles2d(tile, clipped, clipped)) {
          width = (short)rect2d_width(tile);
          height = (short)rect2d_height(tile);
          columns = (short)((width + *(short *)((char *)frame_bitmap + 4) - 1) /
                            *(short *)((char *)frame_bitmap + 4));
          /* edge4 now holds the row count, edge1 the FUN_00072490 mode
           * and edge2 the row counter (same stack slots in the binary). */
          edge4 = (height + *(short *)((char *)frame_bitmap + 6) - 1) /
                  *(short *)((char *)frame_bitmap + 6);
          edge1 = 0;
          if ((param_7 &
               (1 << (((unsigned short *)0x261564)[(short)i] * 2 + 1))) != 0) {
            edge1 = 1;
          }
          if (param_6 != 0) {
            edge1 |= 2;
          }
          for (edge2 = 0; (short)edge2 < (short)edge4; edge2++) {
            for (column = 0; column < columns; column++) {
              set_point2d(
                point,
                (short)(tile[1] +
                        (short)(*(unsigned short *)((char *)frame_bitmap + 4) *
                                column)),
                (short)(tile[0] +
                        (short)(*(unsigned short *)((char *)frame_bitmap + 6) *
                                (short)edge2)));
              FUN_00072490(destination, point, (int)clipped, frame_bitmap, 0,
                           param_6, edge1);
            }
          }
        }
      }
    }
    i++;
  } while ((short)i < 9);
}

/* 0x73770 -- draw the four edges of a float rectangle with bitmap_draw_line
 * (0x73390). cdecl, four stack args: destination ([EBP+8]), color
 * ([EBP+0xc]), rectangle ([EBP+0x10], four floats; [1] and [3] are reduced
 * by 1.0f from 0x2533c8) and clip_rectangle ([EBP+0x14], forwarded as the
 * callee's third arg). Two 8-byte point locals at [EBP-0x10] (point0) and
 * [EBP-0x8] (point1) alternate as the line endpoints. */
void bitmap_frame_rectangle(void *destination, unsigned int color,
                            float *rectangle, short *clip_rectangle)
{
  float point0[2];
  float point1[2];

  point1[0] = rectangle[0];
  point0[0] = rectangle[1] - 1.0f;
  point1[1] = rectangle[2];
  point0[1] = rectangle[2];
  bitmap_draw_line(destination, color, clip_rectangle, point1, point0);
  point1[0] = rectangle[1] - 1.0f;
  point1[1] = rectangle[3] - 1.0f;
  bitmap_draw_line(destination, color, clip_rectangle, point0, point1);
  point0[1] = rectangle[3] - 1.0f;
  point0[0] = rectangle[0];
  bitmap_draw_line(destination, color, clip_rectangle, point1, point0);
  point1[0] = rectangle[0];
  point1[1] = rectangle[2];
  bitmap_draw_line(destination, color, clip_rectangle, point0, point1);
}

/* 0x73830 -- sample the color keys of the source plate bitmap held at
 * 0x334150 (same global bitmap_utilities.c reads as the plate). cdecl, no
 * args, no return. Pixel (0,0), (1,0), (2,0) are read through
 * bitmap_2d_address and masked to 24 bits into dwords 0x33413c/0x334140/
 * 0x334144 (0x7384a-0x7389f). Byte 0x334148 starts at 1 and is cleared when
 * any check fails; byte 0x334149 is set when key0 == key1 (0x738b5-0x738c7).
 * Columns 3..width-1 (int16 at plate+4, re-read each pass, 0x738d9/0x7392d)
 * clear 0x334148 when row 0 differs from key0 AND row 1 differs from key1.
 * On failure all three keys become 0xff000000 (0x7393e-0x7394d). Meanings of
 * the globals beyond these operations are unproven. */
void FUN_00073830(void)
{
  short x;

  unknown_334148 = 1;
  unknown_334149 = 0;
  unknown_33413c =
    *(unsigned long *)bitmap_2d_address(unknown_334150, 0, 0, 0) & 0xffffff;
  unknown_334140 =
    *(unsigned long *)bitmap_2d_address(unknown_334150, 1, 0, 0) & 0xffffff;
  unknown_334144 =
    *(unsigned long *)bitmap_2d_address(unknown_334150, 2, 0, 0) & 0xffffff;
  if (unknown_334144 == unknown_334140 && unknown_334140 != 0xff) {
    unknown_334148 = 0;
  }
  if (unknown_33413c == unknown_334140) {
    unknown_334144 = 0xffff;
    unknown_334149 = 1;
  }
  for (x = 3; x < *(short *)((char *)unknown_334150 + 4); x++) {
    unsigned long row0;
    unsigned long row1;

    row0 =
      *(unsigned long *)bitmap_2d_address(unknown_334150, x, 0, 0) & 0xffffff;
    row1 =
      *(unsigned long *)bitmap_2d_address(unknown_334150, x, 1, 0) & 0xffffff;
    if (row0 != unknown_33413c && row1 != unknown_334140) {
      unknown_334148 = 0;
    }
  }
  if (unknown_334148 == 0) {
    unknown_334144 = 0xff000000;
    unknown_334140 = 0xff000000;
    unknown_33413c = 0xff000000;
  }
}

/* 0x73960 -- advance a 16-bit row cursor through the plate bitmap at
 * 0x334150. Asserts param_1 != NULL ("top_reference", bitmap_extract.c line
 * 0x1d9). The cursor is read and written as a word (0x739a0/0x73a1e,
 * 0x73a03/0x73a62) even though the declared type is int *. Return value is
 * the final row in AX (0x73a12/0x73a72); upper EAX bits are unspecified in
 * the original, the only caller (0x76704) consumes the low 16 bits.
 * When byte 0x334149 is set: per row, any pixel of the row whose low 24 bits
 * differ from dword 0x33413c marks the row; the first marked row sets a
 * latch, and the first unmarked row after the latch stops the walk; every
 * unmarked row before the latch stores row+1 to the cursor.
 * Otherwise: only column 0 is sampled; a pixel equal to 0x33413c sets the
 * latch, a pixel equal to 0x334140 after the latch stops the walk, any other
 * pixel stores row+1. Meanings of the globals are unproven. */
short FUN_00073960(int *param_1)
{
  short y;
  short x;
  char latched;
  char marked;
  unsigned long pixel;

  assert_halt_msg_at("top_reference",
                     "c:\\halo\\SOURCE\\bitmaps\\bitmap_extract.c", 0x1d9,
                     param_1 != NULL);
  if (unknown_334149 != 0) {
    latched = 0;
    for (y = *(short *)param_1; y < *(short *)((char *)unknown_334150 + 6);
         y++) {
      marked = 0;
      for (x = 0; x < *(short *)((char *)unknown_334150 + 4); x++) {
        if ((*(unsigned long *)bitmap_2d_address(unknown_334150, x, y, 0) &
             0xffffff) != unknown_33413c) {
          marked = 1;
        }
      }
      if (marked) {
        latched = 1;
      } else {
        if (latched) {
          break;
        }
        *(short *)param_1 = (short)(y + 1);
      }
    }
  } else {
    latched = 0;
    for (y = *(short *)param_1; y < *(short *)((char *)unknown_334150 + 6);
         y++) {
      pixel =
        *(unsigned long *)bitmap_2d_address(unknown_334150, 0, y, 0) & 0xffffff;
      if (pixel == unknown_33413c) {
        latched = 1;
      } else {
        if (pixel == unknown_334140 && latched) {
          break;
        }
        *(short *)param_1 = (short)(y + 1);
      }
    }
  }
  return y;
}

/* 0x73a80 -- scan row y of the plate bitmap at 0x334150 (y arrives in DI,
 * caller 0x76709 passes FUN_00073960's result). Skips when y < 0 or
 * y >= int16 plate+6. Walks x from 0 while x < int16 plate+4 (re-read each
 * pass, 0x73abe-0x73ac4); a pixel whose low 24 bits differ from dword
 * 0x334140 prints the warning to the stream at 0x331050 and flushes
 * (0x73acc-0x73aed). Meaning of the globals beyond these operations is
 * unproven. */
void FUN_00073a80(short y)
{
  short x;

  if (y < 0 || y >= *(short *)((char *)unknown_334150 + 6)) {
    return;
  }
  for (x = 0; x < *(short *)((char *)unknown_334150 + 4); x++) {
    if ((*(unsigned long *)bitmap_2d_address(unknown_334150, x, y, 0) &
         0xffffff) != unknown_334140) {
      crt_fprintf((void *)0x331050,
                  "### WARNING horizontal border broken at (#%d,#%d)\r\n",
                  (int)x, (int)y);
      crt_fflush((void *)0x331050);
      return;
    }
  }
}

/* 0x73b00 -- split a plateless cube map bitmap (stack arg [EBP+8]) into six
 * square temporary bitmaps. Width (int16 +4) must be a multiple of 4 and a
 * power of two, and height (int16 +6) at least 3 * width/4 (0x73b45-0x73b81).
 * Temporary entries are 16 bytes in the array at *(char **)0x334134, count
 * int16 at 0x334138 (limit 0x400, 0x73b87-0x73b96). The per-face table is
 * 6 x 8 int16 stored on the stack (0x73ba1-0x73c67): [0]/[1] and [2]/[3]
 * are the source x/y start multipliers of size and size-1, [4]/[5] the
 * source x/y step per destination column, [6]/[7] per destination row.
 * Each face gets bitmap_2d_new(size, size, 0, 0xb); on success entry +4
 * gets int16 0x33415c, +6/+8 int16 -1 and +0xc dword -1. Returns AL: the
 * success byte [EBP-1], cleared only on a failed allocation, or 0 on the
 * two early error exits. Field meanings beyond these operations are
 * unproven. */
bool extract_plateless_cube_map(void *bitmap)
{
  bool result;
  short width;
  short face_size;
  short face_index;
  short destination_y;
  short destination_x;
  short source_x;
  short source_y;
  char *entry;
  void *source;
  void *temporary;
  struct {
    short source_x_block;
    short source_y_block;
    short source_x_edge;
    short source_y_edge;
    short source_x_column_delta;
    short source_y_column_delta;
    short source_x_row_delta;
    short source_y_row_delta;
  } faces[6];

  result = 1;
  assert_halt_msg_at("bitmap_verify(bitmap, TRUE)",
                     "c:\\halo\\SOURCE\\bitmaps\\bitmap_extract.c", 0x2c2,
                     bitmap_verify(bitmap, 1));
  width = *(short *)((char *)bitmap + 4);
  if (width % 4 == 0 &&
      (face_size = (short)(width / 4),
       *(short *)((char *)bitmap + 6) >= width / 4 * 3) &&
      (width & (width - 1)) == 0) {
    if (unknown_334138 + 6 <= 0x400) {
      faces[0].source_x_block = 0;
      faces[0].source_y_block = 1;
      faces[0].source_x_edge = 1;
      faces[0].source_y_edge = 0;
      faces[0].source_x_column_delta = 0;
      faces[0].source_y_column_delta = 1;
      faces[0].source_x_row_delta = -1;
      faces[0].source_y_row_delta = 0;
      faces[1].source_x_block = 1;
      faces[1].source_y_block = 1;
      faces[1].source_x_edge = 1;
      faces[1].source_y_edge = 1;
      faces[1].source_x_column_delta = -1;
      faces[1].source_y_column_delta = 0;
      faces[1].source_x_row_delta = 0;
      faces[1].source_y_row_delta = -1;
      faces[2].source_x_block = 2;
      faces[2].source_y_block = 1;
      faces[2].source_x_edge = 0;
      faces[2].source_y_edge = 1;
      faces[2].source_x_column_delta = 0;
      faces[2].source_y_column_delta = -1;
      faces[2].source_x_row_delta = 1;
      faces[2].source_y_row_delta = 0;
      faces[3].source_x_block = 3;
      faces[3].source_y_block = 1;
      faces[3].source_x_edge = 0;
      faces[3].source_y_edge = 0;
      faces[3].source_x_column_delta = 1;
      faces[3].source_y_column_delta = 0;
      faces[3].source_x_row_delta = 0;
      faces[3].source_y_row_delta = 1;
      faces[4].source_x_block = 0;
      faces[4].source_y_block = 0;
      faces[4].source_x_edge = 1;
      faces[4].source_y_edge = 0;
      faces[4].source_x_column_delta = 0;
      faces[4].source_y_column_delta = 1;
      faces[4].source_x_row_delta = -1;
      faces[4].source_y_row_delta = 0;
      faces[5].source_x_block = 0;
      faces[5].source_y_block = 2;
      faces[5].source_x_edge = 1;
      faces[5].source_y_edge = 0;
      faces[5].source_x_column_delta = 0;
      faces[5].source_y_column_delta = 1;
      faces[5].source_x_row_delta = -1;
      faces[5].source_y_row_delta = 0;
      for (face_index = 0; face_index < 6; face_index++) {
        entry = (char *)unknown_334134 + unknown_334138 * 0x10;
        unknown_334138++;
        temporary = bitmap_2d_new(face_size, face_size, 0, 0xb);
        *(void **)entry = temporary;
        if (temporary != NULL) {
          for (destination_y = 0; destination_y < face_size; destination_y++) {
            source_x = (short)((unsigned short)(faces[face_index].source_x_block * face_size) +
                               (unsigned short)(faces[face_index].source_x_edge * (face_size - 1)) +
                               (unsigned short)(faces[face_index].source_x_row_delta * destination_y));
            source_y = (short)((unsigned short)(faces[face_index].source_y_block * face_size) +
                               (unsigned short)(faces[face_index].source_y_edge * (face_size - 1)) +
                               (unsigned short)(faces[face_index].source_y_row_delta * destination_y));
            for (destination_x = 0; destination_x < face_size;
                 destination_x++) {
              source = bitmap_2d_address(bitmap, source_x, source_y, 0);
              *(unsigned long *)bitmap_2d_address(*(void **)entry,
                                                  destination_x, destination_y,
                                                  0) = *(unsigned long *)source;
              source_x += faces[face_index].source_x_column_delta;
              source_y += faces[face_index].source_y_column_delta;
            }
          }
          *(short *)(entry + 4) = *(short *)0x33415c;
          *(short *)(entry + 6) = -1;
          *(short *)(entry + 8) = -1;
          *(long *)(entry + 0xc) = -1;
        } else {
          error(2, "### ERROR extract: failed to allocate temporary bitmap");
          result = 0;
        }
      }
      return result;
    } else {
      error(2,
            "### ERROR extract: can't handle more than (#%d) temporary bitmaps",
            0x400);
      return 0;
    }
  }
  error(2,
        "### ERROR extract: plateless cube map had invalid dimensions "
        "#%dx#%d",
        (int)width, (int)*(short *)((char *)bitmap + 6));
  return 0;
}

/* 0x73e40 -- grow adjusted_bounds_reference (ESI, four int16: [0]/[2] track
 * the y range, [1]/[3] the x range) to the pixels of the plate bitmap at
 * 0x334150 inside bounds (stack arg, same layout, half-open). Both pointers
 * are asserted non-NULL (bitmap_extract.c lines 0x3f7/0x3f8). The result is
 * reset to 0x7fff,0x7fff,0x8000,0x8000 (0x73e95-0x73ea9). Pixels outside the
 * plate are skipped (0x73ed5-0x73ef5). When byte 0x334148 is set, pixels
 * whose low 24 bits equal one of the three color-key dwords 0x33413c/
 * 0x334140/0x334144 are skipped, and when int16 at (*(void **)0x33414c)+4 is
 * zero, pixels whose top byte is zero are skipped as well (0x73f17-0x73f45).
 * The two max entries are incremented on exit (0x73fb7/0x73fbb). Returns AL
 * = 1 when any pixel was accepted (byte [EBP-1], 0x73e4b/0x73f95). */
bool extract_adjust_bounds(short *bounds, short *adjusted_bounds_reference)
{
  bool found;
  short y;
  short x;
  void *plate;
  unsigned long pixel;
  unsigned long color;

  found = 0;
  assert_halt_msg_at("bounds", "c:\\halo\\SOURCE\\bitmaps\\bitmap_extract.c",
                     0x3f7, bounds != NULL);
  assert_halt_msg_at("adjusted_bounds_reference",
                     "c:\\halo\\SOURCE\\bitmaps\\bitmap_extract.c", 0x3f8,
                     adjusted_bounds_reference != NULL);
  adjusted_bounds_reference[0] = 0x7fff;
  adjusted_bounds_reference[1] = 0x7fff;
  adjusted_bounds_reference[2] = (short)0x8000;
  adjusted_bounds_reference[3] = (short)0x8000;
  plate = unknown_334150;
  for (y = bounds[0]; y < bounds[2]; y++) {
    for (x = bounds[1]; x < bounds[3]; x++) {
      if (x < 0 || x >= *(short *)((char *)plate + 4) || y < 0 ||
          y >= *(short *)((char *)plate + 6)) {
        continue;
      }
      pixel = *(unsigned long *)bitmap_2d_address(plate, x, y, 0);
      color = pixel & 0xffffff;
      if (unknown_334148 == 0 ||
          (color != unknown_33413c && color != unknown_334140 &&
           color != unknown_334144 &&
           (*(short *)(*(char **)0x33414c + 4) != 0 ||
            (pixel & 0xff000000) != 0))) {
        adjusted_bounds_reference[1] =
          x > adjusted_bounds_reference[1] ? adjusted_bounds_reference[1] : x;
        adjusted_bounds_reference[0] =
          y > adjusted_bounds_reference[0] ? adjusted_bounds_reference[0] : y;
        adjusted_bounds_reference[3] =
          x > adjusted_bounds_reference[3] ? x : adjusted_bounds_reference[3];
        adjusted_bounds_reference[2] =
          y > adjusted_bounds_reference[2] ? y : adjusted_bounds_reference[2];
        found = 1;
      }
      plate = unknown_334150;
    }
  }
  adjusted_bounds_reference[3]++;
  adjusted_bounds_reference[2]++;
  return found;
}

/* 0x73fd0 -- pick a bitmap format for the a8r8g8b8 bitmap in EAX (MOV
 * ESI,EAX 0x73fd9). Scans every pixel against the first pixel (0x74040-
 * 0x740cc): DI classifies the alpha byte (0 none, 1 only 0/0xff opposite the
 * first pixel, 8 intermediate), BX does the same for byte 2, and byte
 * [EBP-1] is set when the two bytes ever differ. The int16 at
 * (*(void **)0x33414c)+2 selects the format through the 6-entry jump table
 * at 0x741f0 (bitmap_extract.c 0x466 error on others). Word +0 == 4 remaps
 * format 8->9 and 10->0xb; word +4 of 2 or 5 with bit 1 of byte +6 clear
 * returns 0x11. The format word is returned in AX (0x741e8). */
int FUN_00073fd0(void *bitmap)
{
  uint32_t *pixels;
  uint32_t first_pixel;
  uint32_t pixel;
  int pixel_count;
  int i;
  uint32_t alpha;
  uint32_t red;
  short alpha_type;
  short red_type;
  bool channels_differ;
  short format;

  format = -1;
  alpha_type = 0;
  red_type = 0;
  channels_differ = 0;
  assert_halt_msg_at("bitmap_verify(bitmap, TRUE)",
                     "c:\\halo\\SOURCE\\bitmaps\\bitmap_extract.c", 0x429,
                     bitmap_verify(bitmap, 1));
  pixels = (uint32_t *)bitmap_mipmap_address(bitmap, 0);
  first_pixel = *pixels;
  pixel_count = bitmap_get_pixel_count(bitmap);
  for (i = 0; i < pixel_count; i++) {
    pixel = pixels[i];
    alpha = pixel >> 24;
    switch (alpha) {
    case 0:
      if ((first_pixel & 0xff000000) == 0xff000000 && alpha_type <= 1) {
        alpha_type = 1;
      }
      break;
    case 0xff:
      if ((first_pixel & 0xff000000) == 0 && alpha_type <= 1) {
        alpha_type = 1;
      }
      break;
    default:
      alpha_type = 8;
    }
    red = (uint8_t)(pixel >> 16);
    switch (red) {
    case 0:
      if ((first_pixel & 0xff0000) == 0xff0000 && red_type <= 1) {
        red_type = 1;
      }
      break;
    case 0xff:
      if ((first_pixel & 0xff0000) == 0 && red_type <= 1) {
        red_type = 1;
      }
      break;
    default:
      red_type = 8;
    }
    if (alpha != red) {
      channels_differ = 1;
    }
  }
  switch (*(short *)((char *)unknown_33414c + 2)) {
  case 0:
    format = 0xe;
    break;
  case 1:
    format = (alpha_type > 0) + 0xe;
    break;
  case 2:
    format = (alpha_type > 0) * 2 + 0xe;
    break;
  case 3:
    if (alpha_type == 0) {
      format = 6;
    } else {
      format = (alpha_type != 1) + 8;
    }
    break;
  case 4:
    format = (alpha_type != 0) + 10;
    break;
  case 5:
    if (alpha_type == 0) {
      format = 1;
    } else if (red_type == 0) {
      format = 0;
    } else {
      format = (channels_differ != 0) + 2;
    }
    break;
  default:
    display_assert("### ERROR extract: unsupported bitmap group format",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_extract.c", 0x466, 1);
    system_exit(-1);
  }
  if (*(short *)unknown_33414c == 4) {
    switch (format) {
    case 8:
      format = 9;
      break;
    case 10:
      format = 0xb;
      break;
    }
  }
  if ((*(short *)((char *)unknown_33414c + 4) == 2 ||
       *(short *)((char *)unknown_33414c + 4) == 5) &&
      (*((unsigned char *)unknown_33414c + 6) & 2) == 0) {
    return 0x11;
  }
  return format;
}

/* 0x74210 extract_pixels_to_mipmap -- inverse of extract_pixels_from_mipmap:
 * convert the a8r8g8b8 dwords of source_bitmap into mipmap
 * destination_mipmap_index of destination_bitmap. ABI read off the entry
 * code: destination_mipmap_index arrives in AX (MOV EBX,EAX 0x7421e, used as
 * BX), source_bitmap in ECX (MOV ESI,ECX 0x74219, passed to
 * bitmap_verify(...,1) under the "source_bitmap" assert), destination_bitmap
 * on the stack ([EBP+8]). Asserts are bitmap_extract.c 0x6a6-0x6ae. When bit
 * 1 of byte +0xe is set the work is delegated to bitmap_compress_to_mipmap
 * with a fourth arg of &unknown_334144 when byte 0x334148 is nonzero, else 0
 * (NEG/SBB/AND 0x743dd-0x743e6). Otherwise the word format at +0xc is
 * re-read per pixel (0x74440) and dispatched through the jump table at
 * 0x7457c/0x745a4; unsupported formats hit the 0x6ec error. */
void extract_pixels_to_mipmap(short destination_mipmap_index,
                              void *source_bitmap, void *destination_bitmap)
{
  bitmap_data *source = (bitmap_data *)source_bitmap;
  bitmap_data *destination = (bitmap_data *)destination_bitmap;
  uint32_t *source_pixels;
  char *destination_pixels;
  int pixel_count;
  int pixel_index;
  uint32_t pixel;

  assert_halt_msg_at("bitmap_verify(source_bitmap, TRUE)",
                     "c:\\halo\\SOURCE\\bitmaps\\bitmap_extract.c", 0x6a6,
                     bitmap_verify(source_bitmap, 1));
  assert_halt_msg_at("source_bitmap->width ==MAX(1, destination_bitmap->width "
                     ">>destination_mipmap_index)",
                     "c:\\halo\\SOURCE\\bitmaps\\bitmap_extract.c", 0x6a7,
                     source->width ==
                       ((short)(destination->width >>
                                destination_mipmap_index) < 1 ?
                          1 :
                          destination->width >>
                            destination_mipmap_index));
  assert_halt_msg_at("source_bitmap->height==MAX(1, "
                     "destination_bitmap->height>>destination_mipmap_index)",
                     "c:\\halo\\SOURCE\\bitmaps\\bitmap_extract.c", 0x6a8,
                     source->height ==
                       ((short)(destination->height >>
                                destination_mipmap_index) < 1 ?
                          1 :
                          destination->height >>
                            destination_mipmap_index));
  assert_halt_msg_at("source_bitmap->depth ==MAX(1, destination_bitmap->depth "
                     ">>destination_mipmap_index)",
                     "c:\\halo\\SOURCE\\bitmaps\\bitmap_extract.c", 0x6a9,
                     source->depth ==
                       ((short)(destination->depth >>
                                destination_mipmap_index) < 1 ?
                          1 :
                          destination->depth >>
                            destination_mipmap_index));
  assert_halt_msg_at("bitmap_verify(destination_bitmap, FALSE)",
                     "c:\\halo\\SOURCE\\bitmaps\\bitmap_extract.c", 0x6ab,
                     bitmap_verify(destination_bitmap, 0));
  assert_halt_msg_at("destination_bitmap->type==source_bitmap->type",
                     "c:\\halo\\SOURCE\\bitmaps\\bitmap_extract.c", 0x6ac,
                     destination->type ==
                       source->type);
  assert_halt_msg_at("destination_mipmap_index>=0 && "
                     "destination_mipmap_index<=destination_bitmap->mipmap_"
                     "count",
                     "c:\\halo\\SOURCE\\bitmaps\\bitmap_extract.c", 0x6ad,
                     destination_mipmap_index >= 0 &&
                       destination_mipmap_index <=
                         destination->mipmap_count);
  assert_halt_msg_at("!TEST_FLAG(destination_bitmap->flags, "
                     "_bitmap_swizzled_bit)",
                     "c:\\halo\\SOURCE\\bitmaps\\bitmap_extract.c", 0x6ae,
                     (destination->flags & FLAG(_bitmap_swizzled_bit)) == 0);
  if ((destination->flags & FLAG(_bitmap_compressed_bit)) != 0) {
    bitmap_compress_to_mipmap(source_bitmap, destination_bitmap,
                              destination_mipmap_index,
                              unknown_334148 != 0 ? (int)&unknown_334144 : 0);
    return;
  }
  source_pixels = (uint32_t *)bitmap_mipmap_address(source_bitmap, 0);
  destination_pixels =
    (char *)bitmap_mipmap_address(destination_bitmap, destination_mipmap_index);
  pixel_count = bitmap_get_pixel_count(source_bitmap);
  for (pixel_index = 0; pixel_index < pixel_count; pixel_index++) {
    pixel = source_pixels[pixel_index];
    switch (destination->format) {
    case 6:
      ((unsigned short *)destination_pixels)[pixel_index] =
        (unsigned short)((((((pixel >> 16) & 0xff) >> 3) << 11) |
                          ((((pixel >> 8) & 0xff) >> 2) << 5) |
                          ((pixel & 0xff) >> 3)));
      break;
    case 8:
      ((unsigned short *)destination_pixels)[pixel_index] =
        (unsigned short)(((((pixel >> 24) ? 0x80 : 0) << 8) |
                          ((((pixel >> 16) & 0xff) >> 3) << 10) |
                          ((((pixel >> 8) & 0xff) >> 3) << 5) |
                          ((pixel & 0xff) >> 3)));
      break;
    case 9:
      ((unsigned short *)destination_pixels)[pixel_index] =
        (unsigned short)((((pixel >> 24) >> 4) << 12) |
                         ((((pixel >> 16) & 0xff) >> 4) << 8) |
                         ((((pixel >> 8) & 0xff) >> 4) << 4) |
                         ((pixel & 0xff) >> 4));
      break;
    case 10:
      pixel |= 0xff000000;
      /* fall through (0x7450c -> 0x74511) */
    case 11:
      ((uint32_t *)destination_pixels)[pixel_index] = pixel;
      break;
    case 0:
      destination_pixels[pixel_index] = (char)(pixel >> 24);
      break;
    case 1:
    case 2:
      destination_pixels[pixel_index] = (char)(pixel >> 16);
      break;
    case 3:
      ((short *)destination_pixels)[pixel_index] = (short)(pixel >> 16);
      break;
    case 0x11:
      destination_pixels[pixel_index] =
        (char)palette_find_closest_match((const uint32_t *)0x2ee0a0, pixel);
      break;
    default:
      display_assert("### ERROR unsupported bitmap format",
                     "c:\\halo\\SOURCE\\bitmaps\\bitmap_extract.c", 0x6ec, 1);
      system_exit(-1);
    }
  }
}

/* 0x745c0 extract_pixels_from_mipmap -- copy mipmap source_mipmap_index of
 * source_bitmap into destination_bitmap as a8r8g8b8 dwords. ABI read off the
 * entry code: source_mipmap_index arrives in AX (MOV EBX,EAX 0x745cc, used as
 * BX), destination_bitmap in ECX (MOV EDI,ECX 0x745c7, passed to
 * bitmap_verify(...,1) under the "destination_bitmap" assert), source_bitmap
 * on the stack ([EBP+8]). Asserts (bitmap_extract.c 0x6f9-0x700) check
 * width/height/depth (int16 +4/+6/+8) against MAX(1, source >> index), type
 * (int16 +0xa) equality and index against int16 +0x14 (mipmap_count). When
 * bit 1 of byte +0xe is set the work is delegated to
 * bitmap_3d_compress_to_mipmap; otherwise each destination pixel is converted
 * from the source mipmap using the word format at +0xc (re-read per pixel,
 * 0x747a5). */
void extract_pixels_from_mipmap(short source_mipmap_index,
                                void *destination_bitmap, void *source_bitmap)
{
  bitmap_data *source = (bitmap_data *)source_bitmap;
  bitmap_data *destination = (bitmap_data *)destination_bitmap;
  void *source_address;
  uint32_t *destination_address;
  int pixel_count;
  int i;

  assert_halt_msg_at("bitmap_verify(destination_bitmap, TRUE)",
                     "c:\\halo\\SOURCE\\bitmaps\\bitmap_extract.c", 0x6f9,
                     bitmap_verify(destination_bitmap, 1));
  assert_halt_msg_at(
    "destination_bitmap->width ==MAX(1, source_bitmap->width "
    ">>source_mipmap_index)",
    "c:\\halo\\SOURCE\\bitmaps\\bitmap_extract.c", 0x6fa,
    destination->width ==
      ((short)(source->width >> source_mipmap_index) <
           1 ?
         1 :
         source->width >> source_mipmap_index));
  assert_halt_msg_at(
    "destination_bitmap->height==MAX(1, "
    "source_bitmap->height>>source_mipmap_index)",
    "c:\\halo\\SOURCE\\bitmaps\\bitmap_extract.c", 0x6fb,
    destination->height ==
      ((short)(source->height >> source_mipmap_index) <
           1 ?
         1 :
         source->height >> source_mipmap_index));
  assert_halt_msg_at(
    "destination_bitmap->depth ==MAX(1, source_bitmap->depth "
    ">>source_mipmap_index)",
    "c:\\halo\\SOURCE\\bitmaps\\bitmap_extract.c", 0x6fc,
    destination->depth ==
      ((short)(source->depth >> source_mipmap_index) <
           1 ?
         1 :
         source->depth >> source_mipmap_index));
  assert_halt_msg_at("bitmap_verify(source_bitmap, FALSE)",
                     "c:\\halo\\SOURCE\\bitmaps\\bitmap_extract.c", 0x6fe,
                     bitmap_verify(source_bitmap, 0));
  assert_halt_msg_at("source_bitmap->type==destination_bitmap->type",
                     "c:\\halo\\SOURCE\\bitmaps\\bitmap_extract.c", 0x6ff,
                     source->type ==
                       destination->type);
  assert_halt_msg_at("source_mipmap_index>=0 && "
                     "source_mipmap_index<=source_bitmap->mipmap_count",
                     "c:\\halo\\SOURCE\\bitmaps\\bitmap_extract.c", 0x700,
                     source_mipmap_index >= 0 &&
                       source_mipmap_index <=
                         source->mipmap_count);
  if ((source->flags & FLAG(_bitmap_compressed_bit)) != 0) {
    bitmap_3d_compress_to_mipmap(source_bitmap, destination_bitmap,
                                 source_mipmap_index);
    return;
  }
  source_address = bitmap_mipmap_address(source_bitmap, source_mipmap_index);
  destination_address =
    (uint32_t *)bitmap_mipmap_address(destination_bitmap, 0);
  pixel_count = bitmap_get_pixel_count(destination_bitmap);
  for (i = 0; i < pixel_count; i++) {
    destination_address[i] = bitmap_format_to_a8r8g8b8(
      source->format, source_address, i);
  }
}

/* 0x747d0 -- pack the 16-byte entries of the table at dword 0x334134 (count
 * int16 0x334138) into texture pages, one sequence at a time for
 * seq < int dword (0x33414c)+0x54. Entry layout read off the loop:
 * +0x0 bitmap pointer (int16 width +4, height +6), +0x4 int16 sequence,
 * +0x8 int16 page index written, +0xc dword FUN_00120250 result written
 * (0x74930/0x74934). New pages are sized max(page_dimension, bitmap dim)
 * rounded by ceiling_power2 and clamped to 0x200 (unsigned JNC compare),
 * created with texture_page_new(0, w, h, spacing) and appended to
 * out_pages while page_count < 0x20. texture_page_textures_begin gets the
 * page as its one cdecl stack arg (PUSH ESI / ADD ESP,4 at 0x74821,
 * 0x748f2). After packing, each page is shrunk by halves through
 * FUN_001204a0 while both halved dims stay >= 0x20. Prints the number of
 * texture pages spanned to the stream at 0x331050 and stores page_count to
 * *out_page_count. Entry/table meanings beyond these operations are
 * unproven. */
void FUN_000747d0(int *out_pages, short *out_page_count, int page_dimension,
                  int spacing)
{
  short page_count;
  short sequence_count;
  short sequence;
  short start_index;
  short page_index;
  short entry_index;
  char is_new_page;
  char done;
  void *page;
  char *entry;
  void *bitmap;
  short width;
  short height;
  int result;
  unsigned int remaining;

  page_count = 0;
  sequence_count = 1;
  for (sequence = 0; sequence < *(int *)((char *)unknown_33414c + 0x54);
       sequence++) {
    start_index = 0;
    page_index = 0;
    do {
      if (page_index < page_count) {
        page = ((void **)out_pages)[page_index];
        is_new_page = 0;
        if (page != NULL) {
          texture_page_textures_begin(page);
        }
      } else {
        if (page_count >= 0x20) {
          break;
        }
        page = NULL;
        is_new_page = 1;
      }
      done = 1;
      for (entry_index = start_index; entry_index < unknown_334138;
           entry_index++) {
        entry = (char *)unknown_334134 + entry_index * 0x10;
        if (*(short *)(entry + 4) != sequence) {
          continue;
        }
        if (page == NULL) {
          bitmap = *(void **)entry;
          width = (short)page_dimension;
          if ((short)page_dimension <= *(short *)((char *)bitmap + 4)) {
            width = *(short *)((char *)bitmap + 4);
          }
          height = (short)page_dimension;
          if ((short)page_dimension <= *(short *)((char *)bitmap + 6)) {
            height = *(short *)((char *)bitmap + 6);
          }
          width = (unsigned int)ceiling_power2(width) < 0x200 ?
                    (short)ceiling_power2(width) :
                    0x200;
          height = (unsigned int)ceiling_power2(height) < 0x200 ?
                     (short)ceiling_power2(height) :
                     0x200;
          page = texture_page_new(0, width, height, (int16_t)spacing);
          if (page == NULL) {
            goto next_page;
          }
          texture_page_textures_begin(page);
          ((void **)out_pages)[page_count++] = page;
        }
        bitmap = *(void **)entry;
        result = FUN_00120250(page, *(short *)((char *)bitmap + 4),
                              *(short *)((char *)bitmap + 6), 1);
        if (result == -1) {
          if (is_new_page) {
            sequence_count++;
            start_index = entry_index;
            FUN_00120400(page);
          } else {
            FUN_00120340(page);
          }
          done = 0;
          break;
        }
        *(short *)(entry + 8) = page_index;
        *(int *)(entry + 0xc) = result;
      }
      if (page != NULL && done) {
        FUN_00120400(page);
      }
    next_page:
      page_index++;
    } while (!done);
  }
  if (page_count > 0) {
    remaining = (unsigned short)page_count;
    do {
      page = *(void **)out_pages;
      do {
        width = *(short *)((char *)page + 8) >> 1;
        height = *(short *)((char *)page + 0xa) >> 1;
        if (width < 0x20 || height < 0x20) {
          break;
        }
      } while (FUN_001204a0(page, width, height));
      out_pages++;
      remaining--;
    } while (remaining != 0);
  }
  crt_fprintf((void *)0x331050, "sequence spanned %d texture pages\r\n",
              (int)sequence_count);
  crt_fflush((void *)0x331050);
  *out_page_count = page_count;
}
