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
