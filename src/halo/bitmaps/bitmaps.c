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

  if (!FUN_0007b020(rgb_lower_bound)) {
    csprintf((char *)0x5ab100, "%s: assert_valid_real_rgb_color(%f, %f, %f)",
             "rgb_lower_bound", (double)rgb_lower_bound[0],
             (double)rgb_lower_bound[1], (double)rgb_lower_bound[2]);
    display_assert((const char *)0x5ab100,
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x939,
                   true);
    system_exit(-1);
  }

  if (!FUN_0007b020(rgb_upper_bound)) {
    csprintf((char *)0x5ab100, "%s: assert_valid_real_rgb_color(%f, %f, %f)",
             "rgb_upper_bound", (double)rgb_upper_bound[0],
             (double)rgb_upper_bound[1], (double)rgb_upper_bound[2]);
    display_assert((const char *)0x5ab100,
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x93a,
                   true);
    system_exit(-1);
  }

  if ((flags & 1) == 0) {
    /* Linear RGB interpolation. */
    out_color[0] = blend * rgb_upper_bound[0] + t_inv * rgb_lower_bound[0];
    out_color[1] = blend * rgb_upper_bound[1] + t_inv * rgb_lower_bound[1];
    out_color[2] = blend * rgb_upper_bound[2] + t_inv * rgb_lower_bound[2];
  } else {
    /* HSV interpolation. Convert both endpoints to HSV. */
    FUN_0007ab50(rgb_lower_bound, hsv_lower);
    FUN_0007ab50(rgb_upper_bound, hsv_upper);

    /* Decide whether to wrap one hue up by +1.0 so the mix takes the
     * short (or long, depending on bit 1) arc around the hue circle. */
    hue_diff = hsv_upper[0] - hsv_lower[0];
    if (hue_diff < 0.0f)
      hue_diff = -hue_diff;
    wrap_flag = (hue_diff > *(double *)0x25fea8) ? 1 : 0;

    if (wrap_flag != (int)((flags >> 1) & 1)) {
      if (hsv_upper[0] <= hsv_lower[0])
        hsv_upper[0] = hsv_upper[0] + *(float *)0x2533c8;
      else
        hsv_lower[0] = hsv_lower[0] + *(float *)0x2533c8;
    }

    hsv_result[0] = hsv_upper[0] * blend + hsv_lower[0] * t_inv;
    if (hsv_result[0] > *(float *)0x2533c8)
      hsv_result[0] = hsv_result[0] - *(float *)0x2533c8;
    hsv_result[1] = hsv_upper[1] * blend + hsv_lower[1] * t_inv;
    hsv_result[2] = hsv_upper[2] * blend + hsv_lower[2] * t_inv;

    FUN_0007ace0(hsv_result, out_color);
  }

  if (!FUN_0007b020(out_color)) {
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

/* Release a bitmap's D3D texture resource and free its memory if it
 * was dynamically allocated (flag bit 0x40 at byte offset 0xe). */
void bitmap_delete(void *bitmap)
{
  if (bitmap == NULL)
    return;

  /* release D3D texture */
  ((void (*)(void *))0x168ae0)(bitmap);

  if ((*(uint8_t *)((char *)bitmap + 0xe) & 0x40) != 0) {
    /* free associated pixel data if present */
    if (*(void **)((char *)bitmap + 0x2c) != NULL)
      debug_free(*(void **)((char *)bitmap + 0x2c),
                 "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x18b);
    /* free the bitmap struct itself */
    debug_free(bitmap, "c:\\halo\\SOURCE\\bitmaps\\bitmaps.c", 0x18e);
  }
}

/* FUN_0007c940 -- bitmap pixel address
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
void *FUN_0007c940(void *bitmap, short x, short y, short mipmap_index)
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

/*
 * FUN_0007d9f0 — compute the byte size of one scanline at a given mipmap
 * level for an uncompressed, unswizzled bitmap.
 *
 * Confirmed: bitmap_verify(bitmap, FALSE) at 0x7d9fb.
 * Confirmed: mipmap_index range check against bitmap+0x14 (mipmap_count).
 * Confirmed: flags byte at +0xe checked for compressed (bit 1) and swizzled
 * (bit 3). Confirmed: bitmap_mipmap_width * bitmap_format_bits_per_pixel / 8.
 */
int FUN_0007d9f0(void *bitmap, int mipmap_index)
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
