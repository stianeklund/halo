#include "x87_math.h"

void FUN_0007ba50(void *bitmap)
{
  char *b;
  void *temporary;
  uint32_t size;
  short x;
  short y;
  uint32_t pixel;
  float red;
  float green;
  float blue;
  float magnitude;
  float inverse_magnitude;
  uint32_t packed;

  b = (char *)bitmap;
  if (!bitmap_verify(bitmap, 1)) {
    display_assert("bitmap_verify(bitmap, TRUE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x583, 1);
    system_exit(-1);
  }
  if (*(short *)(b + 0xa) != 0) {
    display_assert("bitmap->type==_bitmap_type_2d",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x584, 1);
    system_exit(-1);
  }
  size = bitmap_get_pixel_data_size(bitmap);
  temporary = debug_malloc(
    size, 0, "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x587);
  if (temporary != NULL) {
    for (y = 0; y < *(short *)(b + 6); y++) {
      for (x = 0; x < *(short *)(b + 4); x++) {
        pixel = *(uint32_t *)bitmap_2d_address(bitmap, x, y, 0);
        red = (float)((pixel >> 16) & 0xff) * *(float *)0x26486c - 1.0f;
        green = (float)((pixel >> 8) & 0xff) * *(float *)0x26486c - 1.0f;
        blue = (float)(pixel & 0xff) * *(float *)0x26486c - 1.0f;
        magnitude = x87_sqrt(red * red + green * green + blue * blue);
        if (fabs((double)magnitude) >= *(double *)0x2533d0) {
          inverse_magnitude = 1.0f / magnitude;
          red *= inverse_magnitude;
          green *= inverse_magnitude;
          blue *= inverse_magnitude;
        }
        packed = (uint32_t)(int)((red + 1.0f) * *(float *)0x264868 +
                                 0.5f); /* hazard-ok: value-add */
        packed =
          (packed << 8) | (uint32_t)(int)((green + 1.0f) * *(float *)0x264868 +
                                          0.5f); /* hazard-ok: value-add */
        packed = (packed << 8) | (pixel & 0xff000000) |
                 (uint32_t)(int)((blue + 1.0f) * *(float *)0x264868 +
                                 0.5f); /* hazard-ok: value-add */
        *(uint32_t *)((char *)temporary +
                      ((int)*(short *)(b + 4) * (int)y + (int)x) * 4) = packed;
      }
    }
    csmemcpy(bitmap_mipmap_address(bitmap, 0), temporary, size);
    debug_free(temporary, "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x5a8);
  } else {
    error(2, "### ERROR failed to allocate temporary buffer");
  }

}

void FUN_0007bcb0(void *bitmap)
{
  char *b;
  void *slice_bitmap;
  int slice_index;

  b = (char *)bitmap;
  if (!bitmap_verify(bitmap, 1)) {
    display_assert("bitmap_verify(bitmap, TRUE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x5b7, 1);
    system_exit(-1);
  }
  if (*(short *)(b + 0xa) != 1) {
    display_assert("bitmap->type==_bitmap_type_3d",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x5b8, 1);
    system_exit(-1);
  }
  slice_bitmap = bitmap_2d_new(*(uint16_t *)(b + 4), *(uint16_t *)(b + 6), 0,
                               *(uint16_t *)(b + 0xc));
  if (slice_bitmap == NULL || *(void **)((char *)slice_bitmap + 0x2c) == NULL) {
    error(2, "### ERROR failed to allocate temporary bitmap");
  } else {
    for (slice_index = 0; slice_index < *(short *)(b + 8); slice_index++) {
      bitmap_3d_slice_insert(bitmap, 0, (short)slice_index, slice_bitmap);
      FUN_0007ba50(slice_bitmap);
      bitmap_cube_map_face_extract(slice_bitmap, bitmap, 0, slice_index);
    }
  }
  bitmap_delete(slice_bitmap);
}

void FUN_0007bd90(void *bitmap)
{
  char *b;
  void *face_bitmap;
  short face_index;

  b = (char *)bitmap;
  if (!bitmap_verify(bitmap, 1)) {
    display_assert("bitmap_verify(bitmap, TRUE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x5e5, 1);
    system_exit(-1);
  }
  if (*(short *)(b + 0xa) != 2) {
    display_assert("bitmap->type==_bitmap_type_cube_map",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x5e6, 1);
    system_exit(-1);
  }
  face_bitmap = bitmap_2d_new(*(uint16_t *)(b + 4), *(uint16_t *)(b + 6), 0,
                              *(uint16_t *)(b + 0xc));
  if (face_bitmap == NULL || *(void **)((char *)face_bitmap + 0x2c) == NULL) {
    error(2, "### ERROR failed to allocate temporary bitmap");
  } else {
    for (face_index = 0; face_index < 6; face_index++) {
      FUN_0007ea60(bitmap, 0, face_index, face_bitmap);
      FUN_0007ba50(face_bitmap);
      bitmap_cube_map_face_insert(face_bitmap, bitmap, 0, face_index);
    }
  }
  bitmap_delete(face_bitmap);
}

void bitmap_compress_to_mipmap(void *source_bitmap, void *destination_bitmap,
                               short destination_mipmap_index, int mode)
{
  char *source;
  char *destination;
  int expected;

  source = (char *)source_bitmap;
  destination = (char *)destination_bitmap;
  if (!bitmap_verify(source_bitmap, 1)) {
    display_assert("bitmap_verify(source_bitmap, TRUE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x619, 1);
    system_exit(-1);
  }
  if (!bitmap_verify(destination_bitmap, 0)) {
    display_assert("bitmap_verify(destination_bitmap, FALSE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x61b, 1);
    system_exit(-1);
  }
  if (destination_mipmap_index < 0 ||
      destination_mipmap_index > *(short *)(destination + 0x14)) {
    display_assert("destination_mipmap_index>=0 && "
                   "destination_mipmap_index<=destination_bitmap->mipmap_count",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x61c, 1);
    system_exit(-1);
  }
  expected = *(short *)(destination + 4) >> (destination_mipmap_index & 0x1f);
  if (expected < 1)
    expected = 1;
  if (expected != *(short *)(source + 4)) {
    display_assert("MAX(1, destination_bitmap->width "
                   ">>destination_mipmap_index)==source_bitmap->width",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x61d, 1);
    system_exit(-1);
  }
  expected = *(short *)(destination + 6) >> (destination_mipmap_index & 0x1f);
  if (expected < 1)
    expected = 1;
  if (expected != *(short *)(source + 6)) {
    display_assert("MAX(1, "
                   "destination_bitmap->height>>destination_mipmap_index)=="
                   "source_bitmap->height",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x61e, 1);
    system_exit(-1);
  }
  expected = *(short *)(destination + 8) >> (destination_mipmap_index & 0x1f);
  if (expected < 1)
    expected = 1;
  if (expected != *(short *)(source + 8)) {
    display_assert("MAX(1, destination_bitmap->depth "
                   ">>destination_mipmap_index)==source_bitmap->depth",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x61f, 1);
    system_exit(-1);
  }
  if ((*(uint8_t *)(destination + 0xe) & 2) == 0) {
    display_assert(
      "TEST_FLAG(destination_bitmap->flags, _bitmap_compressed_bit)",
      "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x620, 1);
    system_exit(-1);
  }

  switch (*(short *)(source + 0xa)) {
  case 0:
    FUN_000796e0(source_bitmap, destination_bitmap, destination_mipmap_index,
                 mode);
    return;
  case 1:
    FUN_000798e0(source_bitmap, destination_bitmap, destination_mipmap_index,
                 mode);
    return;
  case 2:
    FUN_00079bb0(source_bitmap, destination_bitmap, destination_mipmap_index,
                 mode);
    return;
  default:
    display_assert("### ERROR unsupported bitmap type",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x630, 1);
    system_exit(-1);
  }
}

void bitmap_3d_compress_to_mipmap(void *source_bitmap, void *destination_bitmap,
                                  short source_mipmap_index)
{
  char *source;
  char *destination;
  int expected;

  source = (char *)source_bitmap;
  destination = (char *)destination_bitmap;
  if (!bitmap_verify(source_bitmap, 0)) {
    display_assert("bitmap_verify(source_bitmap, FALSE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x746, 1);
    system_exit(-1);
  }
  if (source_mipmap_index < 0 ||
      source_mipmap_index > *(short *)(source + 0x14)) {
    display_assert("source_mipmap_index>=0 && "
                   "source_mipmap_index<=source_bitmap->mipmap_count",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x747, 1);
    system_exit(-1);
  }
  expected = *(short *)(source + 4) >> (source_mipmap_index & 0x1f);
  if (expected < 1)
    expected = 1;
  if (expected != *(short *)(destination + 4)) {
    display_assert("MAX(1, source_bitmap->width "
                   ">>source_mipmap_index)==destination_bitmap->width",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x748, 1);
    system_exit(-1);
  }
  expected = *(short *)(source + 6) >> (source_mipmap_index & 0x1f);
  if (expected < 1)
    expected = 1;
  if (expected != *(short *)(destination + 6)) {
    display_assert(
      "MAX(1, "
      "source_bitmap->height>>source_mipmap_index)==destination_bitmap->height",
      "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x749, 1);
    system_exit(-1);
  }
  expected = *(short *)(source + 8) >> (source_mipmap_index & 0x1f);
  if (expected < 1)
    expected = 1;
  if (expected != *(short *)(destination + 8)) {
    display_assert("MAX(1, source_bitmap->depth "
                   ">>source_mipmap_index)==destination_bitmap->depth",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x74a, 1);
    system_exit(-1);
  }
  if ((*(uint8_t *)(source + 0xe) & 2) == 0) {
    display_assert("TEST_FLAG(source_bitmap->flags, _bitmap_compressed_bit)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x74b, 1);
    system_exit(-1);
  }
  if (!bitmap_verify(destination_bitmap, 1)) {
    display_assert("bitmap_verify(destination_bitmap, TRUE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x74d, 1);
    system_exit(-1);
  }

  switch (*(short *)(source + 0xa)) {
  case 0:
    FUN_00079e70(source_bitmap, destination_bitmap, source_mipmap_index);
    return;
  case 1:
    FUN_0007a1e0(source_bitmap, destination_bitmap, source_mipmap_index);
    return;
  case 2:
    bitmap_2d_uncompress_from_mipmap(source_bitmap, destination_bitmap,
                                     source_mipmap_index);
    return;
  default:
    display_assert("### ERROR unsupported bitmap type",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x75b, 1);
    system_exit(-1);
  }
}

/* Interpolate between two real_rgb_color values.
 *
 * flags:
 *   bit 0 (0x1) -> interpolate in HSV space (otherwise linear RGB).
 *   bit 1 (0x2) -> invert the "shortest hue arc" selection. When the
 *                  absolute hue delta is > 0.5 this bit toggles whether
 *                  to wrap one of the hue endpoints up by +1.0 before
 *                  mixing, so the blend travels the long way around the
 *                  hue circle instead of the short way (or vice versa).
 *
 * out_color, rgb_lower_bound, rgb_upper_bound are real_rgb_color (3 floats).
 * blend is in [0, 1]; t_inv = 1.0f - blend weights the lower bound.
 *
 * Matches c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c lines ~0x939..0x95d. */
float *FUN_0007c270(float *out_color, uint32_t flags, float *rgb_lower_bound,
                    float *rgb_upper_bound, float blend)
{
  float t_inv;
  float hsv_lower[3];
  float hsv_upper[3];
  float hsv_result[3];
  float hue_diff;
  int wrap_flag;

  t_inv = *(float *)0x2533c8 - blend;

  if (!valid_real_rgb_color(rgb_lower_bound)) {
    csprintf((char *)0x5ab100, "%s: assert_valid_real_rgb_color(%f, %f, %f)",
             "rgb_lower_bound", (double)rgb_lower_bound[0],
             (double)rgb_lower_bound[1], (double)rgb_lower_bound[2]);
    display_assert((const char *)0x5ab100,
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x939,
                   true);
    system_exit(-1);
  }

  if (!valid_real_rgb_color(rgb_upper_bound)) {
    csprintf((char *)0x5ab100, "%s: assert_valid_real_rgb_color(%f, %f, %f)",
             "rgb_upper_bound", (double)rgb_upper_bound[0],
             (double)rgb_upper_bound[1], (double)rgb_upper_bound[2]);
    display_assert((const char *)0x5ab100,
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x93a,
                   true);
    system_exit(-1);
  }

  if (flags & 1) {
    /* HSV interpolation. Convert both endpoints to HSV. */
    bitmap_clone(rgb_lower_bound, hsv_lower);
    bitmap_clone(rgb_upper_bound, hsv_upper);

    /* Decide whether to wrap one hue up by +1.0 so the mix takes the
     * short (or long, depending on bit 1) arc around the hue circle. */
    hue_diff = (float)fabs((double)(hsv_upper[0] - hsv_lower[0]));
    wrap_flag = (hue_diff > *(double *)0x25fea8) ? 1 : 0;

    if (wrap_flag != (int)((flags >> 1) & 1)) {
      if (hsv_upper[0] < hsv_lower[0])
        hsv_upper[0] = hsv_upper[0] + *(float *)0x2533c8;
      else
        hsv_lower[0] = hsv_lower[0] + *(float *)0x2533c8;
    }

    hsv_result[0] = hsv_upper[0] * blend + hsv_lower[0] * t_inv;
    if (hsv_result[0] > *(float *)0x2533c8)
      hsv_result[0] = hsv_result[0] - *(float *)0x2533c8;
    hsv_result[1] = hsv_upper[1] * blend + hsv_lower[1] * t_inv;
    hsv_result[2] = hsv_upper[2] * blend + hsv_lower[2] * t_inv;

    real_hsv_color_to_real_rgb_color(hsv_result, out_color);
  } else {
    /* Linear RGB interpolation. */
    out_color[0] = blend * rgb_upper_bound[0] + t_inv * rgb_lower_bound[0];
    out_color[1] = blend * rgb_upper_bound[1] + t_inv * rgb_lower_bound[1];
    out_color[2] = blend * rgb_upper_bound[2] + t_inv * rgb_lower_bound[2];
  }


  if (!valid_real_rgb_color(out_color)) {
    csprintf((char *)0x5ab100, "%s: assert_valid_real_rgb_color(%f, %f, %f)",
             "rgb_result", (double)out_color[0], (double)out_color[1],
             (double)out_color[2]);
    display_assert((const char *)0x5ab100,
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x95d,
                   true);
    system_exit(-1);
  }

  return out_color;
}

void FUN_0007c490(float *rgb_result, uint32_t flags, float *lower_bound,
                  float *upper_bound, float *rgb_scale, float blend)
{
  float alpha;
  float inverse_alpha;

  FUN_0007c270(rgb_result, flags, lower_bound + 1, upper_bound + 1, blend);
  if (rgb_scale != NULL) {
    if (!valid_real_rgb_color(rgb_scale)) {
      display_assert(csprintf((char *)0x5ab100,
                              "%s: assert_valid_real_rgb_color(%f, %f, %f)",
                              "rgb_scale", (double)rgb_scale[0],
                              (double)rgb_scale[1], (double)rgb_scale[2]),
                     "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x96e, 1);
      system_exit(-1);
    }
    if (lower_bound[0] > *(float *)0x253f44 ||
        upper_bound[0] > *(float *)0x253f44) {

      alpha = blend * upper_bound[0] + (1.0f - blend) * lower_bound[0];
      inverse_alpha = 1.0f - alpha;
      rgb_result[0] = inverse_alpha * rgb_scale[0] + alpha * rgb_result[0];
      rgb_result[1] = alpha * rgb_result[1] + inverse_alpha * rgb_scale[1];
      rgb_result[2] = alpha * rgb_result[2] + inverse_alpha * rgb_scale[2];
    } else {
      rgb_result[0] *= rgb_scale[0];
      rgb_result[1] *= rgb_scale[1];
      rgb_result[2] *= rgb_scale[2];
    }
  }
  if (!valid_real_rgb_color(rgb_result)) {
    display_assert(csprintf((char *)0x5ab100,
                            "%s: assert_valid_real_rgb_color(%f, %f, %f)",
                            "rgb_result", (double)rgb_result[0],
                            (double)rgb_result[1], (double)rgb_result[2]),
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x982, 1);
    system_exit(-1);
  }
}

/* FUN_0007c5f0 — apply bump-map height to a bitmap (0x4af in
 * bitmap_utilities.c).
 *
 * Dispatches bump-height processing to the appropriate per-type helper:
 *   type 0 (_bitmap_type_2d)       -> FUN_0007b510 (bitmap in ESI)
 *   type 1 (_bitmap_type_3d)       -> FUN_0007b940 (bitmap in EBX)
 *   type 2 (_bitmap_type_cube_map) -> FUN_00079630 (bitmap in ESI)
 *   other                          -> assert + system_exit
 *
 * bump_height must be > 0.0f (compared against DAT_002533c0 == 0.0f).
 * Confirmed: cdecl, 2 stack args; bitmap loaded into ESI at 0x7c5f4.
 * Confirmed: FID_conflict__fwprintf at 0x1d98ad / crt_fflush at 0x1d9bd2.
 * Source: c:\halo\SOURCE\bitmaps\bitmap_utilities.c, lines 0x4af-0x4bd.
 */
void FUN_0007c5f0(void *bitmap, float bump_height)
{
  short type;

  if (!bitmap_verify(bitmap, 1)) {
    display_assert("bitmap_verify(bitmap, TRUE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x4af, 1);
    system_exit(-1);
  }

  if (bump_height > *(float *)0x2533c0) {
    type = *(short *)((char *)bitmap + 0xa);
    switch (type) {
    case 0:
      /* _bitmap_type_2d: bitmap passed via ESI (register arg). */
      FUN_0007b510(bump_height, bitmap);
      return;
    case 1:
      /* _bitmap_type_3d: bitmap passed via EBX (register arg). */
      FUN_0007b940(bump_height, bitmap);
      return;
    case 2:
      /* _bitmap_type_cube_map: bitmap passed via ESI (register arg). */
      FUN_00079630(bump_height, bitmap);
      return;
    default:
      break;
    }

    display_assert("### ERROR unsupported bitmap type",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x4bd, 1);
    system_exit(-1);
  } else {
    crt_fprintf(
      (void *)0x331050,
      (const char *)0x2648d8); /* L"### WARNING importing special-effect bump
                                  map with zero-height\r\n" */
    crt_fflush((void *)0x331050);
  }

}

/*
 * FUN_0007c6c0 — hardware-upload dispatcher for a bitmap (0x569 in
 * bitmap_utilities.c).
 *
 * Verifies the bitmap, then dispatches to the per-type D3D upload helper:
 *   type 0 (_bitmap_type_2d)       -> FUN_0007ba50 (bitmap in EDI)
 *   type 1 (_bitmap_type_3d)       -> FUN_0007bcb0 (bitmap in ESI)
 *   type 2 (_bitmap_type_cube_map) -> FUN_0007bd90 (bitmap in EBX)
 *   other                          -> assert + system_exit
 *
 * Confirmed: cdecl, 1 stack arg; bitmap in ESI at 0x7c6c4.
 * Confirmed: bitmap_verify(bitmap, TRUE) at 0x7c6ca.
 * Confirmed: type field at bitmap+0xa; dispatch at 0x7c6f6..0x7c74 7.
 * Source: c:\halo\SOURCE\bitmaps\bitmap_utilities.c, line 0x569.
 */
void FUN_0007c6c0(void *bitmap)
{
  int type;

  if (!bitmap_verify(bitmap, 1)) {
    display_assert("bitmap_verify(bitmap, TRUE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x569, 1);
    system_exit(-1);
  }

  type = (int)*(short *)((char *)bitmap + 0xa);
  switch (type) {
  case 0:
    FUN_0007ba50(bitmap);
    return;
  case 1:
    FUN_0007bcb0(bitmap);
    return;
  case 2:
    FUN_0007bd90(bitmap);
    return;
  default:
    break;
  }

  display_assert("### ERROR unsupported bitmap type",
                 "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x577, 1);
  system_exit(-1);
}

/*
 * bitmap_type_get_string — return a display string for a bitmap type index
 * (bitmaps.c line 0x50).
 *
 * The string table at 0x2ee4a0 holds three char* pointers:
 *   [0] = "2d texture"
 *   [1] = "3d texture"
 *   [2] = "cube map"
 * Entry [3] (DAT_002ee4ac) must be NULL, confirming NUMBER_OF_BITMAP_TYPES==3.
 *
 * Confirmed: range check type>=0 && type<3 at 0x7c753.
 * Confirmed: sentinel assert at 0x7c763.
 * Confirmed: return (&PTR_s_2d_texture_002ee4a0)[type] at 0x7c772/0x7c784.
 * Source: c:\halo\SOURCE\bitmaps\bitmaps.c, line 0x50.
 */
const char *bitmap_type_get_string(short type)
{
  if (type < 0 || type > 2) {
    display_assert("type>=0 && type<NUMBER_OF_BITMAP_TYPES",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x50, 1);
    system_exit(-1);
  }

  if (((const char **)0x2ee4a0)[3] != 0) {
    display_assert("bitmap_type_string_table[NUMBER_OF_BITMAP_TYPES]==NULL",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x51, 1);
    system_exit(-1);
  }

  return ((const char **)0x2ee4a0)[type];
}

/*
 * bitmap_format_get_string — return a display string for a bitmap format index
 * (bitmaps.c line 0x86).
 *
 * The string pointer table at 0x2ee4b0 holds 18 char* entries for format
 * indices 0..17 (NUMBER_OF_BITMAP_FORMATS == 18, i.e. 0x12).
 * DAT_002ee4f8 (= &table[18]) must be NULL — used as the sentinel check.
 *
 * Confirmed: range check format<0 || format>0x11 at 0x7c7c6.
 * Confirmed: sentinel at DAT_002ee4f8 (0x2ee4f8 = 0x2ee4b0 + 18*4) at 0x7c7dd.
 * Confirmed: return (&PTR_s_alpha_002ee4b0)[format] at 0x7c7ec.
 * Source: c:\halo\SOURCE\bitmaps\bitmaps.c, line 0x86.
 */
const char *bitmap_format_get_string(short format)
{
  if (format < 0 || format > 0x11) {
    display_assert("format>=0 && format<NUMBER_OF_BITMAP_FORMATS",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x86, 1);
    system_exit(-1);
  }

  if (((const char **)0x2ee4b0)[18] != 0) {
    display_assert("bitmap_format_string_table[NUMBER_OF_BITMAP_FORMATS]==NULL",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x87, 1);
    system_exit(-1);
  }

  return ((const char **)0x2ee4b0)[format];
}

/* Look up the number of bits per pixel for a given bitmap format index.
 * The format must be in range [0, 18) and the table entry must be non-zero
 * (i.e. the format must be a supported/known type).
 * Table at 0x26491c: {8,8,8,16,0,0,16,0,16,16,32,32,0,0,4,8,8,8} */
short bitmap_format_bits_per_pixel(short format)
{
  static const char bitmap_format_bits_per_pixel_table[18] = {
    8, 8, 8, 16, 0, 0, 16, 0, 16, 16, 32, 32, 0, 0, 4, 8, 8, 8
  };

  assert_halt(format >= 0 && format < 18);
  assert_halt(bitmap_format_bits_per_pixel_table[format] != 0);
  return (short)bitmap_format_bits_per_pixel_table[format];
}

/*
 * bitmap_changed — release the hardware (D3D) texture resources for a bitmap
 * (bitmaps.c line 0x179).
 *
 * Asserts bitmap is non-NULL, then dispatches to FUN_00168b10 which
 * releases the D3D surface by bitmap type (2D/3D/cube map).
 * Called separately from bitmap_delete so the hardware resources can be
 * freed without immediately freeing the bitmap struct itself.
 *
 * Confirmed: cdecl, 1 stack arg (void *bitmap).
 * Confirmed: NULL assert at 0x7c8b9 ("bitmap", bitmaps.c line 0x179).
 * Confirmed: CALL FUN_00168b10 at 0x7c8c9 (rasterizer_xbox_hardware_bitmaps).
 * Source: c:\halo\SOURCE\bitmaps\bitmaps.c, line 0x179.
 */
void bitmap_changed(void *bitmap)
{
  if (bitmap == NULL) {
    display_assert("bitmap", "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x179, 1);
    system_exit(-1);
  }

  FUN_00168b10(bitmap);
}

/* Release a bitmap's D3D texture resource and free its memory if it
 * was dynamically allocated (flag bit 0x40 at byte offset 0xe). */
void bitmap_delete(void *bitmap)
{
  if (bitmap == NULL)
    return;

  /* release D3D texture */
  FUN_00168ae0(bitmap);

  if ((*(uint8_t *)((char *)bitmap + 0xe) & 0x40) != 0) {
    /* free associated pixel data if present */
    if (*(void **)((char *)bitmap + 0x2c) != NULL)
      debug_free(*(void **)((char *)bitmap + 0x2c),
                 "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x18b);
    /* free the bitmap struct itself */
    debug_free(bitmap, "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x18e);
  }
}

/* bitmap_2d_address -- bitmap pixel address
 *
 * Computes a pointer to the pixel at (x, y) within a given mipmap level
 * of a 2D bitmap. Accumulates pixel counts for all mipmap levels below
 * the requested one, then adds x + width_at_mipmap * y and converts from
 * pixel offset to byte offset using bits-per-pixel.
 *
 * Confirmed: cdecl, 4 stack args (bitmap, x, y, mipmap_index), returns void*.
 * Confirmed: assert strings at lines 0x1a1-0x1a8 from bitmaps.c.
 * Confirmed: calls bitmap_format_bits_per_pixel at 0x7c840.
 * Confirmed: min_dimension = compressed ? 4 : 1 (same pattern as
 * bitmap_mipmap_width). Confirmed: mipmap loop halves width/height each level,
 * clamping to min_dimension. Confirmed: final offset = (x + accumulated +
 * width_at_mip * y) * bpp / 8 + base_address.
 */
void *bitmap_2d_address(void *bitmap, short x, short y, short mipmap_index)
{
  char *b = (char *)bitmap;
  int pixel_count;
  int min_dim;
  short bpp;
  short width;
  short height;
  int bit_offset;

  pixel_count = 0;

  if (bitmap == NULL) {
    display_assert("bitmap", "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x1a1, 1);
    system_exit(-1);
  }

  if (*(int *)(b + 0x2c) == 0) {
    display_assert("bitmap->base_address",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x1a2, 1);
    system_exit(-1);
  }

  if (*(short *)(b + 0xa) != 0) {
    display_assert("bitmap->type==_bitmap_type_2d",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x1a3, 1);
    system_exit(-1);
  }

  if (x < 0 || x >= *(short *)(b + 0x4)) {
    display_assert("x>=0 && x<bitmap->width",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x1a4, 1);
    system_exit(-1);
  }

  if (y < 0 || y >= *(short *)(b + 0x6)) {
    display_assert("y>=0 && y<bitmap->height",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x1a5, 1);
    system_exit(-1);
  }

  if (mipmap_index < 0 || mipmap_index > *(short *)(b + 0x14)) {
    display_assert("mipmap_index>=0 && mipmap_index<=bitmap->mipmap_count",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x1a6, 1);
    system_exit(-1);
  }

  if ((*(uint8_t *)(b + 0xe) & 2) != 0 && (x != 0 || y != 0)) {
    display_assert(
      "!TEST_FLAG(bitmap->flags, _bitmap_compressed_bit) || (x==0 && y==0)",
      "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x1a7, 1);
    system_exit(-1);
  }

  if ((*(uint8_t *)(b + 0xe) & 8) != 0 && (x != 0 || y != 0)) {
    display_assert(
      "!TEST_FLAG(bitmap->flags, _bitmap_swizzled_bit) || (x==0 && y==0)",
      "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x1a8, 1);
    system_exit(-1);
  }

  width = *(short *)(b + 0x4);
  height = *(short *)(b + 0x6);
  min_dim = ((*(uint8_t *)(b + 0xe) & 2) != 0) ? 4 : 1;
  bpp = bitmap_format_bits_per_pixel(*(short *)(b + 0xc));

  if (mipmap_index > 0) {
    short mip_count = mipmap_index;
    do {
      short w = width;
      short h = height;
      pixel_count = pixel_count + (int)w * (int)h;
      width =
        ((short)min_dim <= (short)(w >> 1)) ? (short)(w >> 1) : (short)min_dim;
      height =
        ((short)min_dim <= (short)(h >> 1)) ? (short)(h >> 1) : (short)min_dim;
      mip_count--;
    } while (mip_count != 0);
  }

  bit_offset = ((int)x + pixel_count + (int)width * (int)y) * (int)bpp;
  return (void *)(bit_offset / 8 + *(int *)(b + 0x2c));
}

void *bitmap_3d_address(void *bitmap, short x, short y, short z,
                        short mipmap_index)
{
  char *b;
  int pixel_count;
  int min_dim;
  short bpp;
  short width;
  short height;
  short depth;
  short mip_count;
  short old_width;
  short old_height;
  short old_depth;
  int bit_offset;

  b = (char *)bitmap;
  pixel_count = 0;
  if (bitmap == NULL) {
    display_assert("bitmap", "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x1c7, 1);
    system_exit(-1);
  }
  if (*(int *)(b + 0x2c) == 0) {
    display_assert("bitmap->base_address",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x1c8, 1);
    system_exit(-1);
  }
  if (*(short *)(b + 0xa) != 1) {
    display_assert("bitmap->type==_bitmap_type_3d",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x1c9, 1);
    system_exit(-1);
  }
  if (x < 0 || x >= *(short *)(b + 4)) {
    display_assert("x>=0 && x<bitmap->width",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x1ca, 1);
    system_exit(-1);
  }
  if (y < 0 || y >= *(short *)(b + 6)) {
    display_assert("y>=0 && y<bitmap->height",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x1cb, 1);
    system_exit(-1);
  }
  if (z < 0 || z >= *(short *)(b + 8)) {
    display_assert("z>=0 && z<bitmap->depth",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x1cc, 1);
    system_exit(-1);
  }
  if (mipmap_index < 0 || mipmap_index > *(short *)(b + 0x14)) {
    display_assert("mipmap_index>=0 && mipmap_index<=bitmap->mipmap_count",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x1cd, 1);
    system_exit(-1);
  }
  if ((*(uint8_t *)(b + 0xe) & 2) != 0 && (x != 0 || y != 0 || z != 0)) {
    display_assert("!TEST_FLAG(bitmap->flags, _bitmap_compressed_bit) || (x==0 "
                   "&& y==0 && z==0)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x1ce, 1);
    system_exit(-1);
  }
  if ((*(uint8_t *)(b + 0xe) & 8) != 0 && (x != 0 || y != 0 || z != 0)) {
    display_assert("!TEST_FLAG(bitmap->flags, _bitmap_swizzled_bit) || (x==0 "
                   "&& y==0 && z==0)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x1cf, 1);
    system_exit(-1);
  }

  width = *(short *)(b + 4);
  height = *(short *)(b + 6);
  depth = *(short *)(b + 8);
  min_dim = ((*(uint8_t *)(b + 0xe) & 2) != 0) ? 4 : 1;
  bpp = bitmap_format_bits_per_pixel(*(short *)(b + 0xc));
  mip_count = mipmap_index;
  while (mip_count > 0) {
    old_width = width;
    old_height = height;
    old_depth = depth;
    pixel_count += (int)old_width * (int)old_height * (int)old_depth;
    width =
      (min_dim <= (old_width >> 1)) ? (short)(old_width >> 1) : (short)min_dim;
    height = (min_dim <= (old_height >> 1)) ? (short)(old_height >> 1) :
                                              (short)min_dim;
    depth = (old_depth > 1) ? (short)(old_depth >> 1) : 1;
    mip_count--;
  }

  bit_offset =
    ((int)x + pixel_count + ((int)height * (int)z + (int)y) * (int)width) *
    (int)bpp;
  return (void *)(bit_offset / 8 + *(int *)(b + 0x2c));
}

void *bitmap_cube_map_address(void *bitmap, short x, short y, short face_index,
                              short mipmap_index)
{
  char *b;
  int pixel_count;
  int min_dim;
  short bpp;
  short width;
  short old_width;
  short mip_count;
  int bit_offset;

  b = (char *)bitmap;
  pixel_count = 0;
  if (bitmap == NULL) {
    display_assert("bitmap", "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x1f0, 1);
    system_exit(-1);
  }
  if (*(int *)(b + 0x2c) == 0) {
    display_assert("bitmap->base_address",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x1f1, 1);
    system_exit(-1);
  }
  if (*(short *)(b + 0xa) != 2) {
    display_assert("bitmap->type==_bitmap_type_cube_map",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x1f2, 1);
    system_exit(-1);
  }
  if (x < 0 || x >= *(short *)(b + 4)) {
    display_assert("x>=0 && x<bitmap->width",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x1f3, 1);
    system_exit(-1);
  }
  if (y < 0 || y >= *(short *)(b + 6)) {
    display_assert("y>=0 && y<bitmap->height",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x1f4, 1);
    system_exit(-1);
  }
  if (mipmap_index < 0 || mipmap_index > *(short *)(b + 0x14)) {
    display_assert("mipmap_index>=0 && mipmap_index<=bitmap->mipmap_count",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x1f5, 1);
    system_exit(-1);
  }
  if ((*(uint8_t *)(b + 0xe) & 2) != 0 && (x != 0 || y != 0)) {
    display_assert(
      "!TEST_FLAG(bitmap->flags, _bitmap_compressed_bit) || (x==0 && y==0)",
      "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x1f6, 1);
    system_exit(-1);
  }
  if ((*(uint8_t *)(b + 0xe) & 8) != 0 && (x != 0 || y != 0)) {
    display_assert(
      "!TEST_FLAG(bitmap->flags, _bitmap_swizzled_bit) || (x==0 && y==0)",
      "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x1f7, 1);
    system_exit(-1);
  }

  width = *(short *)(b + 4);
  min_dim = ((*(uint8_t *)(b + 0xe) & 2) != 0) ? 4 : 1;
  bpp = bitmap_format_bits_per_pixel(*(short *)(b + 0xc));
  mip_count = mipmap_index;
  while (mip_count > 0) {
    old_width = width;
    pixel_count += (int)old_width * (int)old_width * 6;
    width =
      (min_dim <= (old_width >> 1)) ? (short)(old_width >> 1) : (short)min_dim;
    mip_count--;
  }

  bit_offset = ((int)x + pixel_count +
                ((int)face_index * (int)width + (int)y) * (int)width) *
               (int)bpp;
  return (void *)(bit_offset / 8 + *(int *)(b + 0x2c));
}

/* 0x7d000 — dispatch bitmap_pixel_address by bitmap type.
 *
 * Asserts that bitmap != NULL and bitmap->base_address (+0x2c) != NULL,
 * then routes to the appropriate typed pixel-address function based on
 * bitmap->type (+0xa): 0=2D (bitmap_2d_address), 1=cube (bitmap_3d_address),
 * 2=3D (bitmap_cube_map_address). Returns the pixel address at (0,0[,0],
 * mipmap_index).
 *
 * Confirmed: TEST ESI,ESI / display_assert("bitmap",...,0x20d,1) at 0x7d007.
 * Confirmed: TEST [ESI+0x2c] / display_assert("bitmap->base_address",...,0x20e)
 * at 0x7d02e. Confirmed: MOVSX+SUB+JZ/DEC/DEC type switch at 0x7d052.
 * Confirmed: bitmap_2d_address(bitmap,0,0,mipmap_index) via PUSH
 * EDX+3×PUSH0+PUSH ESI at 0x7d0b3. Confirmed:
 * bitmap_3d_address(bitmap,0,0,0,mipmap_index) at 0x7d09d. Confirmed:
 * bitmap_cube_map_address(bitmap,0,0,0,mipmap_index) at 0x7d087. Confirmed:
 * display_assert("### ERROR unsupported bitmap type",...,0x21c,1) + return
 * bitmap at 0x7d061.
 */
void *bitmap_mipmap_address(void *bitmap, short mipmap_index)
{
  if (!bitmap) {
    display_assert("bitmap", "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x20d, 1);
    system_exit(-1);
  }
  if (!*(void **)((char *)bitmap + 0x2c)) {
    display_assert("bitmap->base_address",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x20e, 1);
    system_exit(-1);
  }
  switch ((int)*(short *)((char *)bitmap + 0xa)) {
  case 2:
    return bitmap_cube_map_address(bitmap, 0, 0, 0, mipmap_index);
  case 1:
    return bitmap_3d_address(bitmap, 0, 0, 0, mipmap_index);
  case 0:
    return bitmap_2d_address(bitmap, 0, 0, mipmap_index);
  default:
    break;
  }
  display_assert("### ERROR unsupported bitmap type",
                 "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x21c, 1);
  system_exit(-1);
  return bitmap;
}

uint32_t bitmap_format_to_a8r8g8b8(short format, void *mipmap_address,
                                   int pixel_index)
{
  uint32_t value;
  uint32_t result;
  uint32_t work;
  uint32_t channel;
  uint16_t value16;
  uint8_t ch;

  if (mipmap_address == NULL) {
    display_assert("mipmap_address", "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c",
                   0x22b, 1);
    system_exit(-1);
  }
  switch (format) {
  case 6:
    value = ((uint16_t *)mipmap_address)[pixel_index];
    result = ((value & 0xfffff800) | 0xffff0000) << 3;
    result |= value & 0x7e0;
    result = (result << 2) | (value & 0xffffe01f);
    work = ((value >> 1) & 0xe) | (value & 0x600);
    return (result << 3) | (work >> 1);
  case 8:
    value = ((uint16_t *)mipmap_address)[pixel_index];
    result = (value & 0x7c00) << 3;
    result = (result | (value & 0x3e0)) << 2;
    result = (result | (value & 0x7000)) << 1;
    result |= value & 0x1f;
    result = (result << 2) | (value & 0x380);
    result = (result << 1) | ((value >> 2) & 7);
    return result | (-(int32_t)(value >> 15) << 24);
  case 9:
    value = ((uint16_t *)mipmap_address)[pixel_index];
    work = value >> 8;
    result = (((work & 0xf0) << 12) | value) & 0xfffff000;
    channel = work & 0xf;
    result |= ((channel << 4) | channel) << 4;
    channel = (value >> 4) & 0xf;
    result = ((result | channel) << 4) | channel;
    channel = value & 0xf;
    return ((result << 4) | channel) << 4 | channel;
  case 10:
    return ((uint32_t *)mipmap_address)[pixel_index];
  case 11:
    return ((uint32_t *)mipmap_address)[pixel_index];
  case 0:
    return (uint32_t)((uint8_t *)mipmap_address)[pixel_index] << 24;
  case 1:
    value = ((uint8_t *)mipmap_address)[pixel_index];
    result = value | 0xffffff00;
    result = (result << 8) | value;
    result = (result << 8) | value;
    return result;
  case 2:
    value = ((uint8_t *)mipmap_address)[pixel_index];
    result = value;
    result = (result << 8) | value;
    result = (result << 8) | value;
    result = (result << 8) | value;
    return result;
  case 3:
    value16 = ((uint16_t *)mipmap_address)[pixel_index];
    ch = (uint8_t)value16;
    result = ((uint32_t)value16 & 0xffffff00) | ch;
    result = (result << 8) | ch;
    return (result << 8) | ch;
  case 17:
    value = ((uint8_t *)mipmap_address)[pixel_index];
    return ((uint32_t *)0x2ee0a0)[value];
  default:
    display_assert("### ERROR unsupported bitmap format",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x254, 1);
    system_exit(-1);
  }
}



short palette_find_closest_match(const uint32_t *palette, uint32_t color)
{
  short closest_index;
  short index;
  uint32_t entry;
  const uint8_t *entry_bytes;
  int r;
  int r_color;
  int red_delta;
  int g;
  int g_color;
  int green_delta;
  int b;
  int b_color;
  int blue_delta;
  int distance;
  int closest_distance;

  closest_index = -1;
  closest_distance = 0;
  if ((color & 0xff000000) <= 0x80000000)
    return 0xff;

  index = 0;
  do {
    entry = palette[index];
    entry_bytes = (const uint8_t *)&palette[index];
    if (entry == 0)
      break;
    r = (int)entry_bytes[2];
    r_color = (int)((color >> 16) & 0xff);
    red_delta = r - r_color;
    if (red_delta < 0)
      red_delta = r_color - r;
    g = (int)entry_bytes[1];
    g_color = (int)((color >> 8) & 0xff);
    green_delta = g - g_color;
    if (green_delta < 0)
      green_delta = g_color - g;
    b = (int)(entry & 0xff);
    b_color = (int)(color & 0xff);
    blue_delta = b - b_color;
    if (blue_delta < 0)
      blue_delta = b_color - b;
    distance = red_delta * red_delta + green_delta * green_delta +
               blue_delta * blue_delta;
    if (index == 0 || distance < closest_distance) {
      closest_distance = distance;
      closest_index = index;
    }
    index++;
  } while (index < 0x100);

  if (closest_index == -1) {
    display_assert("closest_match_index!=NONE",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x44d, 1);
    system_exit(-1);
  }
  return closest_index;
}


/* bitmap_validate_depth (0x7d440)
 *
 * Validate the depth field of a bitmap against its type.
 * - depth must be in the signed 16-bit range (0, 256].
 * - A depth of 1 is always valid.
 * - A depth > 1 is only valid when the bitmap type is 1 (3D texture).
 *
 * depth is passed in EAX (register arg); format is received on the stack
 * but is never read by the original implementation.
 */
bool bitmap_validate_depth(int depth /* @<eax> */, int format, int type)
{
  int16_t d = (int16_t)depth;
  int16_t t = (int16_t)type;

  (void)format; /* unused by original; stack slot present for ABI parity. */

  if (d > 0 && d <= 0x100 && (d == 1 || t == 1)) {
    return true;
  }
  return false;
}

/* bitmap_verify (0x7d470)
 *
 * Validate a bitmap_data structure for internal consistency: magic tag,
 * type/format ranges, dimension limits, depth, and mipmap count.
 * If check_hardware is set, also validates hardware-import constraints.
 */
bool bitmap_verify(void *bitmap, int check_hardware)
{
  char *b = (char *)bitmap;
  int16_t type, format, width, height, depth, mipmap_count;
  int max_dim;

  assert_halt(bitmap != NULL);

  if (*(int *)b != 0x6269746d)
    goto invalid;
  if ((*(uint16_t *)(b + 0xe) & 0xff00) != 0)
    goto invalid;

  type = *(int16_t *)(b + 0xa);
  if (type < 0 || type >= 3)
    goto invalid;

  format = *(int16_t *)(b + 0xc);
  if (format < 0 || format >= 0x12)
    goto invalid;

  width = *(int16_t *)(b + 0x4);
  if (width <= 0 || width > 0x7530)
    goto invalid;

  height = *(int16_t *)(b + 0x6);
  if (height <= 0 || height > 0x7530)
    goto invalid;

  depth = *(int16_t *)(b + 0x8);
  if (!bitmap_validate_depth(depth, format, type))
    goto invalid;

  mipmap_count = *(int16_t *)(b + 0x14);
  if (mipmap_count < 0)
    goto invalid;

  max_dim = (height > depth) ? (int)height : (int)depth;
  if ((int)width <= max_dim) {
    max_dim = (height > depth) ? (int)height : (int)depth;
  } else {
    max_dim = (int)width;
  }

  if (mipmap_count > FUN_00108db0(max_dim))
    goto invalid;

  if (check_hardware) {
    if (format == 0xb && *(int *)(b + 0x2c) != 0 && mipmap_count == 0 &&
        (*(uint8_t *)(b + 0xe) & 0xe) == 0)
      return true;
    error(2, "### ERROR bitmap @%p (#%dx#%d) appears to be invalid for import",
          bitmap, (int)width, (int)height);
    return false;
  }

  return true;

invalid:
  error(2, "### ERROR bitmap @%p (#%dx#%d) appears to be invalid", bitmap,
        (int)*(int16_t *)(b + 0x4), (int)*(int16_t *)(b + 0x6));
  return false;
}

/* 0x7d5d0 — bitmap init/validate helper.
 *
 * Asserts bitmap != NULL. If bitmap+0x28 is zero, calls FUN_00168370 to
 * set it up. Then calls FUN_00168b10 (hardware finalize), and asserts
 * bitmap_verify(bitmap, FALSE).
 *
 * Confirmed: TEST ESI,ESI / display_assert("bitmap",...,0x163,1) at 0x7d5d7.
 * Confirmed: [ESI+0x28]==0 / CALL FUN_00168370(bitmap) at 0x7d5fb.
 * Confirmed: CALL FUN_00168b10(bitmap) at 0x7d60c (batched ADD ESP,0xc at
 * 0x7d619). Confirmed: bitmap_verify(bitmap,0) /
 * display_assert("bitmap_verify(bitmap, FALSE)",...,0x171) at 0x7d614.
 */
void bitmap_rebuild(void *bitmap)
{
  if (!bitmap) {
    display_assert("bitmap", "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x163, 1);
    system_exit(-1);
  }
  if (!*(int *)((char *)bitmap + 0x28)) {
    FUN_00168370(bitmap);
  }
  FUN_00168b10(bitmap);
  if (!bitmap_verify(bitmap, 0)) {
    display_assert("bitmap_verify(bitmap, FALSE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x171, 1);
    system_exit(-1);
  }
}

/* 0x7d650 — compute bitmap mipmap level count from max dimension.
 *
 * Asserts bitmap_verify(bitmap, FALSE). If flag bit 0 at bitmap+0xe is not
 * set, returns 0. Otherwise computes max(+6, +8) as sVar3, compares with +4,
 * and calls FUN_00108db0(max_dimension) in each branch.
 *
 * Confirmed: bitmap_verify(bitmap,0) / display_assert(...,0x368,1) at 0x7d65c.
 * Confirmed: TEST [ESI+0xe],1 / JZ return-0 at 0x7d688.
 * Confirmed: MOV AX,[ESI+6]; MOV CX,[ESI+8]; MOVSX EDI,AX/CX at 0x7d68e.
 * Confirmed: MOVSX EDX,[ESI+4] / CMP EDX,EDI / JLE at 0x7d6a1.
 * Confirmed: MOV AX,DI=0 / RET at 0x7d6d0 for bit-not-set path.
 */
short bitmap_get_max_mipmap_count(void *bitmap)
{
  short sVar1;
  short sVar2;
  int sVar3;
  int iWidth;

  if (!bitmap_verify(bitmap, 0)) {
    display_assert("bitmap_verify(bitmap, FALSE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x368, 1);
    system_exit(-1);
  }
  if (*(uint8_t *)((char *)bitmap + 0xe) & 1) {
    sVar1 = *(short *)((char *)bitmap + 6);
    sVar2 = *(short *)((char *)bitmap + 8);
    sVar3 = (int)sVar1;
    if (sVar1 <= sVar2) {
      sVar3 = (int)sVar2;
    }
    iWidth = (int)*(short *)((char *)bitmap + 4);
    if (sVar3 < iWidth) {
      return FUN_00108db0(iWidth);
    }
    if (sVar1 <= sVar2) {
      sVar1 = sVar2;
    }
    return FUN_00108db0((int)sVar1);
  }
  return 0;
}

/* bitmap_mipmap_width (0x7d6e0)
 *
 * Compute the width of a bitmap at a given mipmap level.  Clamps to a
 * minimum of 1.  If the compressed flag (bit 1 of +0xe) is set, rounds
 * up to the next multiple of 4 (DXT block alignment).
 */
short bitmap_mipmap_width(void *bitmap, int mipmap_index)
{
  char *b = (char *)bitmap;
  uint16_t width;
  uint16_t result;

  assert_halt(bitmap_verify(bitmap, 0));
  assert_halt((int16_t)mipmap_index >= 0 &&
              (int16_t)mipmap_index <= *(int16_t *)(b + 0x14));

  width = *(uint16_t *)(b + 0x4);
  result = width >> (mipmap_index & 0x1f);
  if (result < 2)
    result = 1;

  if ((*(uint8_t *)(b + 0xe) & 2) != 0)
    result = result + ((-(uint8_t)result) & 3);

  return (short)result;
}

/* bitmap_mipmap_get_height — bitmap_mipmap_height: height counterpart of
 * bitmap_mipmap_width. Returns the pixel height at the given mipmap level,
 * clamped to 1. If the compressed flag (bit 1 of +0xe) is set, rounds up to the
 * next multiple of 4 (DXT block alignment).
 */
short bitmap_mipmap_get_height(void *bitmap, short mipmap_index)
{
  char *b = (char *)bitmap;
  uint16_t height;
  uint16_t result;

  assert_halt(bitmap_verify(bitmap, 0));
  assert_halt(mipmap_index >= 0 && mipmap_index <= *(short *)(b + 0x14));

  height = *(uint16_t *)(b + 0x6);
  result = height >> (mipmap_index & 0x1f);
  if (result < 2)
    result = 1;

  if ((*(uint8_t *)(b + 0xe) & 2) != 0)
    result = result + ((-(uint8_t)result) & 3);

  return (short)result;
}

/* bitmap_mipmap_get_depth — bitmap_mipmap_depth: depth counterpart of
 * bitmap_mipmap_width. Returns the depth at the given mipmap level as a signed
 * 32-bit int, clamped to 1.  No DXT block-alignment rounding (depth is not
 * block-sized). Field +0x8 is the bitmap depth.
 */
int bitmap_mipmap_get_depth(void *bitmap, short mipmap_index)
{
  char *b = (char *)bitmap;
  short depth;

  assert_halt(bitmap_verify(bitmap, 0));
  assert_halt(mipmap_index >= 0 && mipmap_index <= *(short *)(b + 0x14));

  depth = *(short *)(b + 0x8);
  if (1 < depth >> mipmap_index)
    return depth >> mipmap_index;
  return 1;
}

/* bitmap_mipmap_get_pixel_count — total number of texels in one mipmap slice
 * (pixels per face). Returns width * height * depth at the given mipmap level,
 * multiplied by 6 for cube maps (_bitmap_type_cube_map == 2). Field +0xa is the
 * bitmap type; depth at field +0x8.
 */
int bitmap_mipmap_get_pixel_count(void *bitmap, int mipmap_index)
{
  char *b = (char *)bitmap;
  short width;
  short height;
  short depth;
  int result;

  assert_halt(bitmap_verify(bitmap, 0));
  assert_halt((short)mipmap_index >= 0 &&
              (short)mipmap_index <= *(short *)(b + 0x14));

  width = bitmap_mipmap_width(bitmap, mipmap_index);
  height = bitmap_mipmap_get_height(bitmap, mipmap_index);
  depth = (short)bitmap_mipmap_get_depth(bitmap, mipmap_index);
  result = (int)depth * (int)height * (int)width;
  if (*(short *)(b + 0xa) == 2)
    result *= 6;
  return result;
}

/* bitmap_mipmap_get_pixel_data_size — total byte size of one mipmap slice.
 * Multiplies total texels by bits-per-pixel, then ceiling-divides by 8.
 * Uses MSVC CDQ arithmetic rounding: (bits + (bits>>31 & 7)) >> 3.
 * Field +0xc is the bitmap format index passed to bitmap_format_bits_per_pixel.
 */
int bitmap_mipmap_get_pixel_data_size(void *bitmap, int mipmap_index)
{
  char *b = (char *)bitmap;
  int texels;
  short bpp;
  int total_bits;

  assert_halt(bitmap_verify(bitmap, 0));
  assert_halt((short)mipmap_index >= 0 &&
              (short)mipmap_index <= *(short *)(b + 0x14));

  texels = bitmap_mipmap_get_pixel_count(bitmap, mipmap_index);
  bpp = bitmap_format_bits_per_pixel(*(short *)(b + 0xc));
  total_bits = (int)bpp * texels;
  return (total_bits + (total_bits >> 31 & 7)) >> 3;
}

/*
 * bitmap_mipmap_get_row_pitch — compute the byte size of one scanline at a
 * given mipmap level for an uncompressed, unswizzled bitmap.
 *
 * Confirmed: bitmap_verify(bitmap, FALSE) at 0x7d9fb.
 * Confirmed: mipmap_index range check against bitmap+0x14 (mipmap_count).
 * Confirmed: flags byte at +0xe checked for compressed (bit 1) and swizzled
 * (bit 3). Confirmed: bitmap_mipmap_width * bitmap_format_bits_per_pixel / 8.
 */
int bitmap_mipmap_get_row_pitch(void *bitmap, int mipmap_index)
{
  short width;
  short bpp;
  int total_bits;

  if (!bitmap_verify(bitmap, 0)) {
    display_assert("bitmap_verify(bitmap, FALSE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x3f5, 1);
    system_exit(-1);
  }

  if ((short)mipmap_index < 0 ||
      (short)mipmap_index > *(short *)((char *)bitmap + 0x14)) {
    display_assert("mipmap_index>=0 && mipmap_index<=bitmap->mipmap_count",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x3f6, 1);
    system_exit(-1);
  }

  if ((*(uint8_t *)((char *)bitmap + 0xe) & 2) != 0) {
    display_assert("!TEST_FLAG(bitmap->flags, _bitmap_compressed_bit)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x3f7, 1);
    system_exit(-1);
  }

  if ((*(uint8_t *)((char *)bitmap + 0xe) & 8) != 0) {
    display_assert("!TEST_FLAG(bitmap->flags, _bitmap_swizzled_bit)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x3f8, 1);
    system_exit(-1);
  }

  width = bitmap_mipmap_width(bitmap, mipmap_index);
  bpp = bitmap_format_bits_per_pixel(*(short *)((char *)bitmap + 0xc));
  total_bits = (int)bpp * (int)width;
  return total_bits / 8;
}

uint32_t bitmap_2d_get_pixel(void *bitmap, float *point, float lod)
{
  char *b;
  short mipmap_count;
  short format;
  int width;
  int height;
  int mipmap_index;
  int x;
  int y;
  int unwrapped_x;
  int unwrapped_y;
  int bytes_per_block;
  int pixel_index;
  void *mipmap_address;
  void *block_address;
  uint32_t decoded_pixel;
  uint32_t swizzle_masks[2];

  b = (char *)bitmap;
  if (bitmap == NULL) {
    display_assert("bitmap", "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x261, 1);
    system_exit(-1);
  }
  if (*(short *)(b + 0xa) != 0) {
    display_assert("bitmap->type==_bitmap_type_2d",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x262, 1);
    system_exit(-1);
  }
  if ((*(uint8_t *)(b + 0xe) & 0x10) != 0) {
    display_assert("!TEST_FLAG(bitmap->flags, _bitmap_linear_bit)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x263, 1);
    system_exit(-1);
  }
  if (point == NULL) {
    display_assert("point", "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x264, 1);
    system_exit(-1);
  }
  if (!(lod >= *(const float *)0x2533c0 &&
        lod <= *(const float *)0x2533c8)) {
    display_assert("lod>=0.0f && lod<=1.0f",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x265, 1);
    system_exit(-1);
  }
  if (*(void **)(b + 0x2c) == NULL)
    return 0xffffffff;

  mipmap_count = *(short *)(b + 0x14);
  mipmap_index = 0;
  if (lod < *(const float *)0x2533c8 && mipmap_count > 0) {
    mipmap_index = x87_round_to_int(
      (*(const float *)0x2533c8 - lod) * (float)mipmap_count);
    if ((short)mipmap_index < 0 || (short)mipmap_index > mipmap_count) {
      display_assert("mipmap_index>=0 && mipmap_index<=bitmap->mipmap_count",
                     "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x26f, 1);
      system_exit(-1);
    }
  }

  width = (int)bitmap_mipmap_width(bitmap, mipmap_index);
  height = (int)bitmap_mipmap_get_height(bitmap, (short)mipmap_index);
  unwrapped_x = x87_round_to_int((float)width * point[0] - 0.5f);
  if ((width & (width - 1)) == 0)
    x = (width - 1) & unwrapped_x;
  else
    x = ((unwrapped_x % width) + width) % width;
  unwrapped_y = x87_round_to_int((float)height * point[1] - 0.5f);
  if ((height & (height - 1)) == 0)
    y = (height - 1) & unwrapped_y;
  else
    y = ((unwrapped_y % height) + height) % height;

  mipmap_address = bitmap_mipmap_address(bitmap, (short)mipmap_index);
  format = *(short *)(b + 0xc);
  if ((*(uint16_t *)(b + 0xe) & 2) != 0) {
    bytes_per_block =
      ((int)bitmap_format_bits_per_pixel(format) * 16) / 8;
    block_address =
      (char *)mipmap_address +
      (((y / 4) * width) / 4 + x / 4) * bytes_per_block;
    if ((char *)block_address < *(char **)(b + 0x2c)) {
      display_assert(
        csprintf((char *)0x5ab100,
                 "bitmap_2d_get_pixel tried to access compressed block @ -%d "
                 "bytes from address start (w=%d, h=%d, m=%d, x=%d, y=%d, "
                 "lod=%f)",
                 *(char **)(b + 0x2c) - (char *)block_address,
                 (int)*(short *)(b + 4), (int)*(short *)(b + 6),
                 (int)mipmap_count, unwrapped_x % width,
                 unwrapped_y % height, (int)(short)mipmap_index),
        "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x2a0, 1);
      system_exit(-1);
    }
    if (*(char **)(b + 0x2c) + *(int *)(b + 0x1c) <= (char *)block_address) {
      display_assert(
        csprintf((char *)0x5ab100,
                 "bitmap_2d_get_pixel tried to access compressed block @ -%d "
                 "bytes from address end (w=%d, h=%d, m=%d, x=%d, y=%d, "
                 "lod=%f)",
                 (char *)block_address -
                   (*(char **)(b + 0x2c) + *(int *)(b + 0x1c)),
                 (int)*(short *)(b + 4), (int)*(short *)(b + 6),
                 (int)mipmap_count, unwrapped_x % width,
                 unwrapped_y % height, (int)(short)mipmap_index),
        "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x2a9, 1);
      system_exit(-1);
    }

    decoded_pixel = 0;
    if (format == 0xe) {
      DecodeBlockRGB__single_pixel(block_address, &decoded_pixel, x & 3, y & 3);
    } else if (format == 0xf) {
      FUN_00071840(block_address, &decoded_pixel, x & 3, y & 3);
    } else if (format == 0x10) {
      FUN_00071af0(block_address, &decoded_pixel, x & 3, y & 3);
    } else {
      display_assert("### ERROR unsupported bitmap format",
                     "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x2b7, 1);
      system_exit(-1);
    }
    return decoded_pixel;
  }

  if ((*(uint16_t *)(b + 0xe) & 8) != 0) {
    if ((short)x < 0 || (short)x >= 0x1000) {
      display_assert("x>=0 && x<4096", "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c",
                     0x2c1, 1);
      system_exit(-1);
    }
    if ((short)y < 0 || (short)y >= 0x1000) {
      display_assert("y>=0 && y<4096", "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c",
                     0x2c2, 1);
      system_exit(-1);
    }
    rasterizer_swizzle_compute_masks((short)width, (short)height, (uint16_t)x,
                                     (uint16_t)y, swizzle_masks);
    pixel_index = (int)(swizzle_masks[0] | swizzle_masks[1]);
  } else {
    pixel_index = y * width + x;
  }
  return bitmap_format_to_a8r8g8b8(format, mipmap_address, pixel_index);
}

int bitmap_get_pixel_count(void *bitmap)
{
  int pixel_count;
  int mipmap_index;

  pixel_count = 0;
  if (!bitmap_verify(bitmap, 0)) {
    display_assert("bitmap_verify(bitmap, FALSE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x378, 1);
    system_exit(-1);
  }
  for (mipmap_index = 0; mipmap_index <= *(short *)((char *)bitmap + 0x14);
       mipmap_index++) {
    pixel_count += bitmap_mipmap_get_pixel_count(bitmap, mipmap_index);
  }
  return pixel_count;
}

/*
 * bitmap_get_pixel_data_size — total byte size of a bitmap's pixel data,
 * across every mipmap level (bitmap_get_pixel_count counts all texels).
 *
 * Evidence is read directly from the pristine XBE at 0x7e040..0x7e0a1; the
 * fingerprinted Ghidra bundle for this target contained only connection
 * errors, so every fact below is cited to a raw instruction address.
 *
 * Confirmed 0x7e048..0x7e04b: bitmap_verify(bitmap_data, FALSE) — `push 0;
 * push esi; call 0x7d470`, first push is the last arg.
 * Confirmed 0x7e057..0x7e06f: on AL==0, display_assert("bitmap_verify(bitmap,
 * FALSE)", "c:\halo\SOURCE\bitmaps\bitmaps.c", 0x38a, 1) then system_exit(-1).
 * Both strings read out of .rdata at 0x264da0 / 0x264a74; the line number is
 * the literal 0x38a, NOT this file's __LINE__.
 * Confirmed call order 0x7e078 then 0x7e086: bitmap_get_pixel_count first,
 * bitmap_format_bits_per_pixel second. The single `add esp,8` at 0x7e092
 * cleans BOTH one-arg cdecl pushes; it is not a two-arg call.
 * Confirmed 0x7e07f..0x7e081: `xor eax,eax; mov ax,[esi+0xc]` — the format
 * field is loaded 16-bit and ZERO-extended here (other call sites of 0x7c840
 * in this TU use the partial-register `mov cx,[..]; push ecx` form instead).
 * Confirmed 0x7e08b..0x7e08e: `movsx eax,ax; imul eax,edi` — bpp is a signed
 * short return, multiplied by the pixel count.
 * Confirmed 0x7e091..0x7e09b: `cdq; and edx,7; add eax,edx; sar eax,3` — the
 * MSVC signed divide-by-8 sequence, i.e. total_bits / 8 with round-toward-zero.
 * Frame has no `sub esp`, so the original keeps every temporary in a register.
 */
int bitmap_get_pixel_data_size(void *bitmap_data)
{
  int pixel_count;
  short bpp;
  int total_bits;

  if (!bitmap_verify(bitmap_data, 0)) {
    display_assert("bitmap_verify(bitmap, FALSE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x38a, 1);
    system_exit(-1);
  }

  pixel_count = bitmap_get_pixel_count(bitmap_data);
  bpp = bitmap_format_bits_per_pixel(*(uint16_t *)((char *)bitmap_data + 0xc));
  total_bits = (int)bpp * pixel_count;
  return total_bits / 8;
}

void *bitmap_2d_new(unsigned short width, unsigned short height,
                    unsigned short mipmap_count, unsigned short format)
{
  char *bitmap;
  void *base_address;

  if ((short)width < 1 || (short)width > 30000) {
    display_assert(
      "bitmap_format_type_valid_width (format, _bitmap_type_2d, width)",
      "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0xb5, 1);
    system_exit(-1);
  }
  if ((short)height < 1 || (short)height > 30000) {
    display_assert(
      "bitmap_format_type_valid_height(format, _bitmap_type_2d, height)",
      "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0xb6, 1);
    system_exit(-1);
  }
  bitmap =
    (char *)debug_malloc(0x30, 0, "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0xb8);
  if (bitmap == NULL) {
    error(2, "### ERROR failed to allocate bitmap");
    return NULL;
  }
  csmemset(bitmap, 0, 0x30);
  *(uint32_t *)(bitmap + 0) = 0x6269746d;
  *(uint16_t *)(bitmap + 4) = width;
  *(uint16_t *)(bitmap + 6) = height;
  *(uint16_t *)(bitmap + 8) = 1;
  *(uint16_t *)(bitmap + 0xa) = 0;
  *(uint16_t *)(bitmap + 0xc) = format;
  *(uint16_t *)(bitmap + 0xe) = 0x40;
  *(uint16_t *)(bitmap + 0x14) = mipmap_count;
  if ((((int)(short)width & ((int)(short)width - 1)) == 0) &&
      (((int)(short)height & ((int)(short)height - 1)) == 0)) {
    *(uint16_t *)(bitmap + 0xe) = 0x41;
  }
  if ((short)format > 0xd && (short)format < 0x11)
    *(uint8_t *)(bitmap + 0xe) |= 2;
  if (format == 0x11)
    *(uint8_t *)(bitmap + 0xe) |= 4;
  base_address = debug_malloc(bitmap_get_pixel_data_size(bitmap), 0,
                              "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0xd5);
  *(void **)(bitmap + 0x2c) = base_address;
  if (base_address == NULL) {
    error(2, "### ERROR failed to allocate bitmap->base_address");
    return bitmap;
  }
  if (!bitmap_verify(bitmap, 0)) {
    display_assert("bitmap_verify(bitmap, FALSE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0xd9, 1);
    system_exit(-1);
  }
  return bitmap;
}

void *bitmap_3d_new(unsigned short width, unsigned short height,
                    unsigned short depth, unsigned short mipmap_count,
                    unsigned short format)
{
  char *bitmap;
  void *base_address;

  if ((short)width < 1 || (short)width > 30000) {
    display_assert(
      "bitmap_format_type_valid_width (format, _bitmap_type_3d, width)",
      "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0xf1, 1);
    system_exit(-1);
  }
  if ((short)height < 1 || (short)height > 30000) {
    display_assert(
      "bitmap_format_type_valid_height(format, _bitmap_type_3d, height)",
      "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0xf2, 1);
    system_exit(-1);
  }
  if ((short)depth < 1 || (short)depth > 0x100) {
    display_assert(
      "bitmap_format_type_valid_depth (format, _bitmap_type_3d, depth)",
      "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0xf3, 1);
    system_exit(-1);
  }
  bitmap =
    (char *)debug_malloc(0x30, 0, "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0xf5);
  if (bitmap == NULL) {
    error(2, "### ERROR failed to allocate bitmap");
    return NULL;
  }
  csmemset(bitmap, 0, 0x30);
  *(uint32_t *)(bitmap + 0) = 0x6269746d;
  *(uint16_t *)(bitmap + 4) = width;
  *(uint16_t *)(bitmap + 6) = height;
  *(uint16_t *)(bitmap + 8) = depth;
  *(uint16_t *)(bitmap + 0xa) = 1;
  *(uint16_t *)(bitmap + 0xc) = format;
  *(uint16_t *)(bitmap + 0xe) = 0x40;
  *(uint16_t *)(bitmap + 0x14) = mipmap_count;
  if ((((int)(short)width & ((int)(short)width - 1)) == 0) &&
      (((int)(short)height & ((int)(short)height - 1)) == 0) &&
      (((int)(short)depth & ((int)(short)depth - 1)) == 0)) {
    *(uint16_t *)(bitmap + 0xe) = 0x41;
  }
  if ((short)format > 0xd && (short)format < 0x11)
    *(uint8_t *)(bitmap + 0xe) |= 2;
  if (format == 0x11)
    *(uint8_t *)(bitmap + 0xe) |= 4;
  base_address = debug_malloc(bitmap_get_pixel_data_size(bitmap), 0,
                              "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x112);
  *(void **)(bitmap + 0x2c) = base_address;
  if (base_address == NULL) {
    error(2, "### ERROR failed to allocate bitmap->base_address");
    return bitmap;
  }
  if (!bitmap_verify(bitmap, 0)) {
    display_assert("bitmap_verify(bitmap, FALSE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x116, 1);
    system_exit(-1);
  }
  return bitmap;
}

void *bitmap_cube_map_new(unsigned short width, unsigned short mipmap_count,
                          unsigned short format)
{
  char *bitmap;
  void *base_address;

  if ((short)width < 1 || (short)width > 30000) {
    display_assert(
      "bitmap_format_type_valid_width(format, _bitmap_type_cube_map, width)",
      "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x12c, 1);
    system_exit(-1);
  }
  if (((int)(short)width & ((int)(short)width - 1)) != 0) {
    display_assert("(width&(width-1))==0",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x12d, 1);
    system_exit(-1);
  }
  bitmap = (char *)debug_malloc(0x30, 0, "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c",
                                0x12f);
  if (bitmap == NULL) {
    error(2, "### ERROR failed to allocate bitmap");
    return NULL;
  }
  csmemset(bitmap, 0, 0x30);
  *(uint32_t *)(bitmap + 0) = 0x6269746d;
  *(uint16_t *)(bitmap + 4) = width;
  *(uint16_t *)(bitmap + 6) = width;
  *(uint16_t *)(bitmap + 8) = 1;
  *(uint16_t *)(bitmap + 0xa) = 2;
  *(uint16_t *)(bitmap + 0xc) = format;
  *(uint16_t *)(bitmap + 0xe) = 0x41;
  *(uint16_t *)(bitmap + 0x14) = mipmap_count;
  *(uint32_t *)(bitmap + 0x28) = 0;
  if ((short)format > 0xd && (short)format < 0x11)
    *(uint16_t *)(bitmap + 0xe) = 0x43;
  if (format == 0x11)
    *(uint8_t *)(bitmap + 0xe) |= 4;
  base_address = debug_malloc(bitmap_get_pixel_data_size(bitmap), 0,
                              "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x14d);
  *(void **)(bitmap + 0x2c) = base_address;
  if (base_address == NULL) {
    error(2, "### ERROR failed to allocate bitmap->base_address");
    return bitmap;
  }
  if (!bitmap_verify(bitmap, 0)) {
    display_assert("bitmap_verify(bitmap, FALSE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x151, 1);
    system_exit(-1);
  }
  return bitmap;
}

void bitmap_3d_slice_insert(void *source_bitmap, short source_mipmap_index,
                            short source_slice_index, void *slice_bitmap)
{
  char *source;
  char *slice;
  int expected_width;
  int expected_height;
  uint32_t size;
  void *source_address;
  void *slice_address;

  source = (char *)source_bitmap;
  slice = (char *)slice_bitmap;
  if (!bitmap_verify(source_bitmap, 0)) {
    display_assert("bitmap_verify(source_bitmap, FALSE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x2e3, 1);
    system_exit(-1);
  }
  if (*(short *)(source + 0xa) != 1) {
    display_assert("source_bitmap->type==_bitmap_type_3d",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x2e4, 1);
    system_exit(-1);
  }
  if (source_mipmap_index < 0 ||
      source_mipmap_index > *(short *)(source + 0x14)) {
    display_assert("source_mipmap_index>=0 && "
                   "source_mipmap_index<=source_bitmap->mipmap_count",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x2e5, 1);
    system_exit(-1);
  }
  if (source_slice_index < 0 || source_slice_index >= *(short *)(source + 8)) {
    display_assert(
      "source_slice_index>=0 && source_slice_index<source_bitmap->depth",
      "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x2e6, 1);
    system_exit(-1);
  }
  expected_width = *(short *)(source + 4) >> (source_mipmap_index & 0x1f);
  if (expected_width < 1)
    expected_width = 1;
  if (expected_width != *(short *)(slice + 4)) {
    display_assert(
      "MAX(1, source_bitmap->width >>source_mipmap_index)==slice_bitmap->width",
      "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x2e7, 1);
    system_exit(-1);
  }
  expected_height = *(short *)(source + 6) >> (source_mipmap_index & 0x1f);
  if (expected_height < 1)
    expected_height = 1;
  if (expected_height != *(short *)(slice + 6)) {
    display_assert(
      "MAX(1, "
      "source_bitmap->height>>source_mipmap_index)==slice_bitmap->height",
      "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x2e8, 1);
    system_exit(-1);
  }
  if ((*(uint8_t *)(source + 0xe) & 8) != 0) {
    display_assert("!TEST_FLAG(source_bitmap->flags, _bitmap_swizzled_bit)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x2e9, 1);
    system_exit(-1);
  }
  if (!bitmap_verify(slice_bitmap, 0)) {
    display_assert("bitmap_verify(slice_bitmap, FALSE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x2eb, 1);
    system_exit(-1);
  }
  if (*(short *)(slice + 0x14) != 0) {
    display_assert("slice_bitmap->mipmap_count==0",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x2ec, 1);
    system_exit(-1);
  }
  if (*(short *)(slice + 0xa) != 0) {
    display_assert("slice_bitmap->type==_bitmap_type_2d",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x2ed, 1);
    system_exit(-1);
  }
  if (*(short *)(slice + 0xc) != *(short *)(source + 0xc)) {
    display_assert("slice_bitmap->format==source_bitmap->format",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x2ee, 1);
    system_exit(-1);
  }
  if ((*(uint8_t *)(slice + 0xe) & 8) != 0) {
    display_assert("!TEST_FLAG(slice_bitmap->flags, _bitmap_swizzled_bit)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x2ef, 1);
    system_exit(-1);
  }

  size = bitmap_get_pixel_data_size(slice_bitmap);
  source_address = bitmap_3d_address(source_bitmap, 0, 0, source_slice_index,
                                     source_mipmap_index);
  slice_address = bitmap_mipmap_address(slice_bitmap, 0);
  csmemcpy(slice_address, source_address, size);
}

void bitmap_cube_map_face_extract(void *slice_bitmap, void *destination_bitmap,
                                  int destination_mipmap_index,
                                  int destination_slice_index)
{
  char *slice;
  char *destination;
  short mipmap_index;
  short slice_index;
  int expected_width;
  int expected_height;
  uint32_t size;
  void *source_address;
  void *destination_address;

  slice = (char *)slice_bitmap;
  destination = (char *)destination_bitmap;
  mipmap_index = (short)destination_mipmap_index;
  slice_index = (short)destination_slice_index;
  if (!bitmap_verify(slice_bitmap, 0)) {
    display_assert("bitmap_verify(slice_bitmap, FALSE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x306, 1);
    system_exit(-1);
  }
  if (*(short *)(slice + 0x14) != 0) {
    display_assert("slice_bitmap->mipmap_count==0",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x307, 1);
    system_exit(-1);
  }
  if (*(short *)(slice + 0xa) != 0) {
    display_assert("slice_bitmap->type==_bitmap_type_2d",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x308, 1);
    system_exit(-1);
  }
  if (*(short *)(slice + 0xc) != *(short *)(destination + 0xc)) {
    display_assert("slice_bitmap->format==destination_bitmap->format",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x309, 1);
    system_exit(-1);
  }
  if ((*(uint8_t *)(slice + 0xe) & 8) != 0) {
    display_assert("!TEST_FLAG(slice_bitmap->flags, _bitmap_swizzled_bit)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x30a, 1);
    system_exit(-1);
  }
  if (!bitmap_verify(destination_bitmap, 0)) {
    display_assert("bitmap_verify(destination_bitmap, FALSE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x30c, 1);
    system_exit(-1);
  }
  if (*(short *)(destination + 0xa) != 1) {
    display_assert("destination_bitmap->type==_bitmap_type_3d",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x30d, 1);
    system_exit(-1);
  }
  if (mipmap_index < 0 || mipmap_index > *(short *)(destination + 0x14)) {
    display_assert("destination_mipmap_index>=0 && "
                   "destination_mipmap_index<=destination_bitmap->mipmap_count",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x30e, 1);
    system_exit(-1);
  }
  if (slice_index < 0 || slice_index >= *(short *)(destination + 8)) {
    display_assert("destination_slice_index>=0 && "
                   "destination_slice_index<destination_bitmap->depth",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x30f, 1);
    system_exit(-1);
  }
  expected_width = *(short *)(destination + 4) >> (mipmap_index & 0x1f);
  if (expected_width < 1)
    expected_width = 1;
  if (expected_width != *(short *)(slice + 4)) {
    display_assert("MAX(1, destination_bitmap->width "
                   ">>destination_mipmap_index)==slice_bitmap->width",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x310, 1);
    system_exit(-1);
  }
  expected_height = *(short *)(destination + 6) >> (mipmap_index & 0x1f);
  if (expected_height < 1)
    expected_height = 1;
  if (expected_height != *(short *)(slice + 6)) {
    display_assert("MAX(1, "
                   "destination_bitmap->height>>destination_mipmap_index)=="
                   "slice_bitmap->height",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x311, 1);
    system_exit(-1);
  }
  if ((*(uint8_t *)(destination + 0xe) & 8) != 0) {
    display_assert(
      "!TEST_FLAG(destination_bitmap->flags, _bitmap_swizzled_bit)",
      "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x312, 1);
    system_exit(-1);
  }

  size = bitmap_get_pixel_data_size(slice_bitmap);
  destination_address =
    bitmap_3d_address(destination_bitmap, 0, 0, slice_index, mipmap_index);
  source_address = bitmap_mipmap_address(slice_bitmap, 0);
  csmemcpy(destination_address, source_address, size);
}

void FUN_0007ea60(void *source_bitmap, short source_mipmap_index,
                  short source_face_index, void *face_bitmap)
{
  char *source;
  char *face;
  int expected_width;
  int expected_height;
  uint32_t size;
  void *source_address;
  void *face_address;

  source = (char *)source_bitmap;
  face = (char *)face_bitmap;
  if (!bitmap_verify(source_bitmap, 0)) {
    display_assert("bitmap_verify(source_bitmap, FALSE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x329, 1);
    system_exit(-1);
  }
  if (*(short *)(source + 0xa) != 2) {
    display_assert("source_bitmap->type==_bitmap_type_cube_map",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x32a, 1);
    system_exit(-1);
  }
  if (source_mipmap_index < 0 ||
      source_mipmap_index > *(short *)(source + 0x14)) {
    display_assert("source_mipmap_index>=0 && "
                   "source_mipmap_index<=source_bitmap->mipmap_count",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x32b, 1);
    system_exit(-1);
  }
  if (source_face_index < 0 || source_face_index >= 6) {
    display_assert(
      "source_face_index>=0 && source_face_index<NUMBER_OF_FACES_PER_CUBE",
      "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x32c, 1);
    system_exit(-1);
  }
  expected_width = *(short *)(source + 4) >> (source_mipmap_index & 0x1f);
  if (expected_width < 1)
    expected_width = 1;
  if (expected_width != *(short *)(face + 4)) {
    display_assert(
      "MAX(1, source_bitmap->width >>source_mipmap_index)==face_bitmap->width",
      "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x32d, 1);
    system_exit(-1);
  }
  expected_height = *(short *)(source + 6) >> (source_mipmap_index & 0x1f);
  if (expected_height < 1)
    expected_height = 1;
  if (expected_height != *(short *)(face + 6)) {
    display_assert(
      "MAX(1, source_bitmap->height>>source_mipmap_index)==face_bitmap->height",
      "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x32e, 1);
    system_exit(-1);
  }
  if ((*(uint8_t *)(source + 0xe) & 8) != 0) {
    display_assert("!TEST_FLAG(source_bitmap->flags, _bitmap_swizzled_bit)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x32f, 1);
    system_exit(-1);
  }
  if (!bitmap_verify(face_bitmap, 0)) {
    display_assert("bitmap_verify(face_bitmap, FALSE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x331, 1);
    system_exit(-1);
  }
  if (*(short *)(face + 0x14) != 0) {
    display_assert("face_bitmap->mipmap_count==0",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x332, 1);
    system_exit(-1);
  }
  if (*(short *)(face + 0xa) != 0) {
    display_assert("face_bitmap->type==_bitmap_type_2d",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x333, 1);
    system_exit(-1);
  }
  if (*(short *)(face + 0xc) != *(short *)(source + 0xc)) {
    display_assert("face_bitmap->format==source_bitmap->format",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x334, 1);
    system_exit(-1);
  }
  if ((*(uint8_t *)(face + 0xe) & 8) != 0) {
    display_assert("!TEST_FLAG(face_bitmap->flags, _bitmap_swizzled_bit)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x335, 1);
    system_exit(-1);
  }

  size = bitmap_get_pixel_data_size(face_bitmap);
  source_address = bitmap_cube_map_address(
    source_bitmap, 0, 0, source_face_index, source_mipmap_index);
  face_address = bitmap_mipmap_address(face_bitmap, 0);
  csmemcpy(face_address, source_address, size);
}

void bitmap_cube_map_face_insert(void *face_bitmap, void *destination_bitmap,
                                 short destination_mipmap_index,
                                 short destination_face_index)
{
  char *face;
  char *destination;
  int expected_width;
  int expected_height;
  uint32_t size;
  void *source_address;
  void *destination_address;

  face = (char *)face_bitmap;
  destination = (char *)destination_bitmap;
  if (!bitmap_verify(face_bitmap, 0)) {
    display_assert("bitmap_verify(face_bitmap, FALSE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x34c, 1);
    system_exit(-1);
  }
  if (*(short *)(face + 0x14) != 0) {
    display_assert("face_bitmap->mipmap_count==0",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x34d, 1);
    system_exit(-1);
  }
  if (*(short *)(face + 0xa) != 0) {
    display_assert("face_bitmap->type==_bitmap_type_2d",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x34e, 1);
    system_exit(-1);
  }
  if (*(short *)(face + 0xc) != *(short *)(destination + 0xc)) {
    display_assert("face_bitmap->format==destination_bitmap->format",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x34f, 1);
    system_exit(-1);
  }
  if ((*(uint8_t *)(face + 0xe) & 8) != 0) {
    display_assert("!TEST_FLAG(face_bitmap->flags, _bitmap_swizzled_bit)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x350, 1);
    system_exit(-1);
  }
  if (!bitmap_verify(destination_bitmap, 0)) {
    display_assert("bitmap_verify(destination_bitmap, FALSE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x352, 1);
    system_exit(-1);
  }
  if (*(short *)(destination + 0xa) != 2) {
    display_assert("destination_bitmap->type==_bitmap_type_cube_map",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x353, 1);
    system_exit(-1);
  }
  if (destination_mipmap_index < 0 ||
      destination_mipmap_index > *(short *)(destination + 0x14)) {
    display_assert("destination_mipmap_index>=0 && "
                   "destination_mipmap_index<=destination_bitmap->mipmap_count",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x354, 1);
    system_exit(-1);
  }
  if (destination_face_index < 0 || destination_face_index >= 6) {
    display_assert("destination_face_index>=0 && "
                   "destination_face_index<NUMBER_OF_FACES_PER_CUBE",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x355, 1);
    system_exit(-1);
  }
  expected_width =
    *(short *)(destination + 4) >> (destination_mipmap_index & 0x1f);
  if (expected_width < 1)
    expected_width = 1;
  if (expected_width != *(short *)(face + 4)) {
    display_assert("MAX(1, destination_bitmap->width "
                   ">>destination_mipmap_index)==face_bitmap->width",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x356, 1);
    system_exit(-1);
  }
  expected_height =
    *(short *)(destination + 6) >> (destination_mipmap_index & 0x1f);
  if (expected_height < 1)
    expected_height = 1;
  if (expected_height != *(short *)(face + 6)) {
    display_assert("MAX(1, "
                   "destination_bitmap->height>>destination_mipmap_index)=="
                   "face_bitmap->height",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x357, 1);
    system_exit(-1);
  }
  if ((*(uint8_t *)(destination + 0xe) & 8) != 0) {
    display_assert(
      "!TEST_FLAG(destination_bitmap->flags, _bitmap_swizzled_bit)",
      "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x358, 1);
    system_exit(-1);
  }

  size = bitmap_get_pixel_data_size(face_bitmap);
  destination_address = bitmap_cube_map_address(
    destination_bitmap, 0, 0, destination_face_index, destination_mipmap_index);
  source_address = bitmap_mipmap_address(face_bitmap, 0);
  csmemcpy(destination_address, source_address, size);
}

void FUN_0007ef80(short *bits_per_channel, short *thresholds, short width,
                  short *current, short *next, uint8_t *source_row)
{
  short *bits_ptr;
  short *th;
  short *next_ptr;
  int offset;
  int x;
  int i;
  int val;
  int quantized_val;
  int width_minus_one;
  short err;
  int weighted;
  uint8_t clamped[4];
  uint8_t quantized[4];
  uint8_t *clamped_ptr;
  uint8_t *quantized_ptr;

  if (width <= 0)
    return;

  width_minus_one = (int)width - 1;

  for (x = 0; x < width; x++) {
    clamped_ptr = clamped;
    i = 4;
    do {
      val = *current++;
      if (val < 0)
        val = 0;
      else if (val > 0xff)
        val = 0xff;
      *clamped_ptr++ = (uint8_t)val;
    } while (--i != 0);

    bits_ptr = bits_per_channel;
    quantized_ptr = quantized;
    i = 4;
    do {
      if (*bits_ptr == 0) {
        quantized_val = 0;
      } else {
        quantized_val =
          ((clamped[4 - i] >> (8 - *bits_ptr)) * 0xff) /
          ((1 << *bits_ptr) - 1);
      }
      *quantized_ptr++ = (uint8_t)quantized_val;
      *source_row++ = (uint8_t)quantized_val;
      bits_ptr++;
    } while (--i != 0);

    offset = (char *)current - (char *)next;
    next_ptr = next + 4;
    th = thresholds;
    i = 4;
    do {
      err = (short)clamped[4 - i] - (short)quantized[4 - i];
      if (x < width_minus_one && *th < *(short *)((char *)next_ptr + offset)) {
        weighted = err * 7;
        *(short *)((char *)next_ptr + offset) +=
          (short)((weighted + ((weighted >> 31) & 0xf)) >> 4);
      }
      if (next != NULL) {
        if (x != 0 && *th < next_ptr[-8]) {
          weighted = err * 3;
          next_ptr[-8] +=
            (short)((weighted + ((weighted >> 31) & 0xf)) >> 4);
        }
        if (*th < next_ptr[-4]) {
          weighted = err * 5;
          next_ptr[-4] +=
            (short)((weighted + ((weighted >> 31) & 0xf)) >> 4);
        }
        if (x < width_minus_one && *th < next_ptr[0]) {
          weighted = err;
          next_ptr[0] +=
            (short)((weighted + ((weighted >> 31) & 0xf)) >> 4);
        }
      }
      th++;
      next_ptr++;
    } while (--i != 0);

    if (next != NULL)
      next += 4;
  }
}



void FUN_0007f150(void *bitmap, short *bits_per_channel)
{
  char *b;
  short *current;
  short *next;
  short *swap;
  short thresholds[4];
  short *ps;
  short *pbits;
  short *dst;
  uint8_t *source;
  int i;
  int count;
  short row;
  short next_row;
  short width;
  short height;

  b = (char *)bitmap;
  if (!bitmap_verify(bitmap, 1)) {
    display_assert("bitmap_verify(bitmap, TRUE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmaps_quantitize.c", 0x30, 1);
    system_exit(-1);
  }
  if (bits_per_channel == NULL || *(short *)(b + 0xa) != 0)
    return;

  width = *(short *)(b + 4);
  current = (short *)debug_malloc(
    (uint32_t)width << 3, 0, "c:\\halo\\SOURCE\\bitmaps\\bitmaps_quantitize.c",
    0x34);
  next = (short *)debug_malloc(
    (uint32_t)width << 3, 0, "c:\\halo\\SOURCE\\bitmaps\\bitmaps_quantitize.c",
    0x35);

  i = 4;
  pbits = bits_per_channel;
  do {
    if (*pbits < 0 || *pbits > 8) {
      display_assert("bits_per_channel[channel_index]>=0 && "
                     "bits_per_channel[channel_index]<=CHANNEL_BITS",
                     "c:\\halo\\SOURCE\\bitmaps\\bitmaps_quantitize.c", 0x3f,
                     1);
      system_exit(-1);
    }
    pbits++;
  } while (--i != 0);

  ps = (short *)0x33457a;
  i = 4;
  do {
    *ps = *bits_per_channel;
    bits_per_channel++;
    ps--;
  } while (--i != 0);

  i = 4;
  do {
    thresholds[4 - i] =
      (short)(int)((float)(1 << (8 - ((short *)0x334574)[4 - i])) * 0.25f);
  } while (--i != 0);

  if (current != NULL && next != NULL) {
    source = *(uint8_t **)(b + 0x2c);
    count = (int)*(short *)(b + 4) << 2;
    dst = current;
    if (count > 0) {
      do {
        *dst++ = *source++;
      } while (--count != 0);
    }

    row = 0;
    height = *(short *)(b + 6);
    if (height != 1 && height - 1 >= 0) {
      do {
        width = *(short *)(b + 4);
        next_row = row + 1;
        source = (uint8_t *)bitmap_2d_address(bitmap, 0, next_row, 0);
        count = (int)width << 2;
        dst = next;
        if (count > 0) {
          do {
            *dst++ = *source++;
          } while (--count != 0);
        }
        source = (uint8_t *)bitmap_2d_address(bitmap, 0, row, 0);
        FUN_0007ef80((short *)0x334574, thresholds, width, current, next,
                     source);
        swap = current;
        current = next;
        next = swap;
        row = next_row;
      } while (row < *(short *)(b + 6) - 1);
    }
    source = (uint8_t *)bitmap_2d_address(bitmap, 0, *(short *)(b + 6) - 1, 0);
    FUN_0007ef80((short *)0x334574, thresholds, *(short *)(b + 4), current,
                 NULL, source);
    debug_free(current, "c:\\halo\\SOURCE\\bitmaps\\bitmaps_quantitize.c",
               0x73);
    debug_free(next, "c:\\halo\\SOURCE\\bitmaps\\bitmaps_quantitize.c", 0x74);
  }
}

