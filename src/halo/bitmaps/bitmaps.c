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

  if ((flags & 1) == 0) {
    /* Linear RGB interpolation. */
    out_color[0] = blend * rgb_upper_bound[0] + t_inv * rgb_lower_bound[0];
    out_color[1] = blend * rgb_upper_bound[1] + t_inv * rgb_lower_bound[1];
    out_color[2] = blend * rgb_upper_bound[2] + t_inv * rgb_lower_bound[2];
  } else {
    /* HSV interpolation. Convert both endpoints to HSV. */
    bitmap_clone(rgb_lower_bound, hsv_lower);
    bitmap_clone(rgb_upper_bound, hsv_upper);

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

    real_hsv_color_to_real_rgb_color(hsv_result, out_color);
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

  if (bump_height <= 0.0f) {
    crt_fprintf(
      (void *)0x331050,
      (const char *)0x2648d8); /* L"### WARNING importing special-effect bump
                                  map with zero-height\r\n" */
    crt_fflush((void *)0x331050);
    return;
  }

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
