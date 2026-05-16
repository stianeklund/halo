/*
 * FUN_00075380 -- bitmap_extract: create a new bitmap entry in the group.
 *
 * Validates the source bitmap, determines format and mipmap count,
 * adds a new bitmap entry to the group's tag_block, copies registration
 * point, optionally smooths, then copies pixel data from source to dest.
 *
 * Source TU: bitmap_extract.c (assert strings confirm)
 * ABI: bitmap passed in EAX (@EAX), returns short (new bitmap index or -1).
 */
short FUN_00075380(void *bitmap /* @<eax> */)
{
  char *group;
  short bitmap_type;
  short bitmap_usage;
  short max_mipmaps;
  short mipmap_count;
  short new_bitmap_index;
  int format;
  void *pixel_data;
  char *bitmap_data;
  char *bm;
  int pixel_size;
  int kb_size;
  char *format_str;

  bm = (char *)bitmap;

  /* bitmap_verify(bitmap, TRUE) */
  if (!bitmap_verify(bitmap, 1)) {
    display_assert("bitmap_verify(bitmap, TRUE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_extract.c", 0x487, 1);
    system_exit(-1);
  }

  format = FUN_00073fd0(bitmap);

  group = *(char **)0x33414c;
  bitmap_type = *(short *)(group + 0x0);
  bitmap_usage = *(short *)(group + 0x4);

  if (bitmap_type == 4 || bitmap_usage == 4) {
    mipmap_count = 0;
  } else {
    mipmap_count = (short)bitmap_get_max_mipmap_count(bitmap);

    if (*(short *)(group + 0x0) == 3 && mipmap_count >= 2) {
      mipmap_count = 2;
    }

    max_mipmaps = *(short *)(group + 0x4c);
    if (max_mipmaps > 0) {
      short limit = max_mipmaps - 1;
      if (limit <= (short)mipmap_count) {
        mipmap_count = limit;
      }
    }
  }

  new_bitmap_index = FUN_00077120(
    *(void **)0x33414c, *(short *)(bm + 0x4), *(short *)(bm + 0x6),
    *(short *)(bm + 0x8), *(short *)(bm + 0xa), format, (int)mipmap_count);

  *(short *)0x33415e = new_bitmap_index;

  if (new_bitmap_index == -1) {
    return new_bitmap_index;
  }

  pixel_data = FUN_00077590(bitmap);

  bitmap_data = (char *)tag_block_get_element(*(char **)0x33414c + 0x60,
                                              (int)new_bitmap_index, 0x30);

  group = *(char **)0x33414c;

  if ((*(unsigned char *)(group + 0x6) & 0x8) != 0) {
    *(short *)(bitmap_data + 0x10) = (short)((*(short *)(bm + 0x10) + 1) / 2);
    *(short *)(bitmap_data + 0x12) = (short)((*(short *)(bm + 0x12) + 1) / 2);
  } else {
    *(int *)(bitmap_data + 0x10) = *(int *)(bm + 0x10);
  }

  if (pixel_data == 0) {
    return new_bitmap_index;
  }

  if (*(int *)((char *)pixel_data + 0x2c) == 0) {
    return new_bitmap_index;
  }

  group = *(char **)0x33414c;

  if (*(float *)(group + 0x44) > *(float *)0x2533c0) {
    switch (*(short *)(group + 0x0)) {
    case 0:
    case 1:
    case 2:
      bitmap_smooth(pixel_data, *(float *)(group + 0x44));
      break;
    case 3:
      crt_fprintf((void *)0x331050,
                  "### WARNING tried to smooth a sprite group",
                  (void *)0x261f2c);
      crt_fflush((void *)0x331050);
      break;
    case 4:
      crt_fprintf((void *)0x331050,
                  "### WARNING tried to smooth an interface-bitmap group",
                  (void *)0x261f2c);
      crt_fflush((void *)0x331050);
      break;
    default:
      display_assert("### ERROR unsupported bitmap group type",
                     "c:\\halo\\SOURCE\\bitmaps\\bitmap_extract.c", 0x4d0, 1);
      system_exit(-1);
    }
  }

  FUN_00074fb0(pixel_data, bitmap_data);

  if (*(short *)(bitmap_data + 0xa) == 1) {
    pixel_size = bitmap_get_pixel_data_size(bitmap_data);
    kb_size = (pixel_size + (((unsigned int)pixel_size >> 31) & 0x3ff)) >> 10;
    format_str = (char *)bitmap_format_get_string(*(short *)(bitmap_data + 0xc));
    crt_fprintf(
      (void *)0x331050, "bitmap created: #%dx#%dx#%d, %s, %dK-bytes\r\n",
      (int)*(short *)(bitmap_data + 0x4), (int)*(short *)(bitmap_data + 0x6),
      (int)*(short *)(bitmap_data + 0x8), format_str, kb_size);
    crt_fflush((void *)0x331050);
    return new_bitmap_index;
  }

  pixel_size = bitmap_get_pixel_data_size(bitmap_data);
  kb_size = (pixel_size + (((unsigned int)pixel_size >> 31) & 0x3ff)) >> 10;
  format_str = (char *)bitmap_format_get_string(*(short *)(bitmap_data + 0xc));
  crt_fprintf((void *)0x331050, "bitmap created: #%dx#%d, %s, %dK-bytes\r\n",
              (int)*(short *)(bitmap_data + 0x4),
              (int)*(short *)(bitmap_data + 0x6), format_str, kb_size);
  crt_fflush((void *)0x331050);

  return new_bitmap_index;
}

float *bitmap_clone(float *rgb, float *hsv_out)
{
  float max_component;
  float min_component;
  float chroma;
  float hue;

  if (hsv_out == (float *)0) {
    display_assert("hsv", "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c",
                   0x8b2, true);
    system_exit(-1);
  }

  if (rgb == hsv_out) {
    display_assert("rgb!=(real_rgb_color *)hsv",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x8b3,
                   true);
    system_exit(-1);
  }

  max_component = rgb[0];
  if (max_component < rgb[1])
    max_component = rgb[1];
  if (max_component < rgb[2])
    max_component = rgb[2];

  min_component = rgb[0];
  if (min_component > rgb[1])
    min_component = rgb[1];
  if (min_component > rgb[2])
    min_component = rgb[2];

  chroma = max_component - min_component;
  hsv_out[2] = max_component;
  if (max_component == *(float *)0x2533c0) {
    hsv_out[1] = *(float *)0x2533c0;
  } else {
    hsv_out[1] = chroma / max_component;
  }

  if (hsv_out[1] == *(float *)0x2533c0) {
    hsv_out[0] = 0.0f;
    return hsv_out;
  }

  if (rgb[0] == max_component) {
    hue = (rgb[1] - rgb[2]) / chroma;
  } else if (rgb[1] == max_component) {
    hue = (rgb[2] - rgb[0]) / chroma + *(float *)0x253f40;
  } else {
    hue = (rgb[0] - rgb[1]) / chroma + *(float *)0x2533d8;
  }

  hsv_out[0] = hue * *(float *)0x2647d4;
  if (hsv_out[0] < *(float *)0x2533c0)
    hsv_out[0] = hsv_out[0] + *(float *)0x2533c8;
  return hsv_out;
}

float *real_hsv_color_to_real_rgb_color(float *hsv, float *rgb_out)
{
  float scaled_hue;
  float f;
  float p;
  float q;
  float t;
  int sector;

  if (rgb_out == (float *)0) {
    display_assert("rgb", "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c",
                   0x8df, true);
    system_exit(-1);
  }

  if (rgb_out == hsv) {
    display_assert("rgb!=(real_rgb_color *)hsv",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x8e1,
                   true);
    system_exit(-1);
  }

  if (hsv[1] == *(float *)0x2533c0) {
    rgb_out[0] = hsv[2];
    rgb_out[1] = hsv[2];
    rgb_out[2] = hsv[2];
    return rgb_out;
  }

  scaled_hue = hsv[0] * *(float *)0x254640;
  sector = (int)scaled_hue;
  if ((float)sector > scaled_hue)
    sector--;

  f = scaled_hue - (float)sector;
  p = (*(float *)0x2533c8 - hsv[1]) * hsv[2];
  q = (*(float *)0x2533c8 - f * hsv[1]) * hsv[2];
  t = (*(float *)0x2533c8 - (*(float *)0x2533c8 - f) * hsv[1]) * hsv[2];

  switch (sector) {
  case 0:
    rgb_out[0] = hsv[2];
    rgb_out[1] = t;
    rgb_out[2] = p;
    return rgb_out;
  case 1:
    rgb_out[0] = q;
    rgb_out[1] = hsv[2];
    rgb_out[2] = p;
    return rgb_out;
  case 2:
    rgb_out[0] = p;
    rgb_out[1] = hsv[2];
    rgb_out[2] = t;
    return rgb_out;
  case 3:
    rgb_out[0] = p;
    rgb_out[1] = q;
    rgb_out[2] = hsv[2];
    return rgb_out;
  case 4:
    rgb_out[0] = t;
    rgb_out[1] = p;
    rgb_out[2] = hsv[2];
    return rgb_out;
  case 5:
    rgb_out[0] = hsv[2];
    rgb_out[1] = p;
    rgb_out[2] = q;
    return rgb_out;
  default:
    return rgb_out;
  }
}

bool valid_real_rgb_color(float *rgb)
{
  uint32_t component_bits;

  component_bits = *(uint32_t *)&rgb[0];
  if ((component_bits & 0x7f800000) == 0x7f800000)
    return false;

  component_bits = *(uint32_t *)&rgb[1];
  if ((component_bits & 0x7f800000) == 0x7f800000)
    return false;

  component_bits = *(uint32_t *)&rgb[2];
  if ((component_bits & 0x7f800000) == 0x7f800000)
    return false;

  if (rgb[0] < *(float *)0x2533c0 || rgb[0] > *(float *)0x2533c8)
    return false;
  if (rgb[1] < *(float *)0x2533c0 || rgb[1] > *(float *)0x2533c8)
    return false;
  if (rgb[2] < *(float *)0x2533c0 || rgb[2] > *(float *)0x2533c8)
    return false;

  return true;
}

/*
 * FUN_00076bb0 -- bitmap tag_block element delete wrapper.
 *
 * Gets an element from a tag_block at the given index (element size 0x30),
 * then passes it to bitmap_delete.
 */
void FUN_00076bb0(void *tag_block, int index)
{
    void *element;

    element = tag_block_get_element(tag_block, index, 0x30);
    bitmap_delete(element);
}

/*
 * FUN_00076ff0 -- get bitmap data element from a bitmap tag.
 *
 * Looks up a 'bitm' tag by index, then returns a pointer to the bitmap
 * data entry at the given bitmap_index within the tag's bitmap block
 * (offset 0x60, element size 0x30). Returns NULL on failure.
 */
void *FUN_00076ff0(int tag_index, short bitmap_index)
{
    int iVar1;
    void *uVar2;

    iVar1 = (int)tag_get(0x6269746d, tag_index);
    uVar2 = 0;
    if ((iVar1 != 0) && (bitmap_index >= 0)) {
        if ((int)bitmap_index < *(int *)(iVar1 + 0x60)) {
            uVar2 = tag_block_get_element((int *)(iVar1 + 0x60), (int)bitmap_index, 0x30);
        }
    }
    return uVar2;
}

/*
 * FUN_00077510 -- bitmap_fill: fill all pixels of a bitmap with a dword value.
 *
 * Gets the pixel base address via bitmap_2d_address(0,0,0), gets the pixel
 * count, then fills that many dwords with the given color. The original uses
 * REP STOSD.
 */
void FUN_00077510(void *bitmap, int fill_color)
{
    int *pixels;
    int count;
    int i;

    pixels = (int *)bitmap_2d_address(bitmap, 0, 0, 0);
    count = bitmap_get_pixel_count(bitmap);
    if (count > 0) {
        for (i = 0; i < count; i++) {
            pixels[i] = fill_color;
        }
    }
}

/*
 * FUN_00077540 -- bitmap_alpha_to_rgb: spread alpha byte to all 4 channels.
 *
 * For each pixel, reads byte [+3] (alpha), builds 0xAAAAAAAA by shifting
 * and OR-ing, then stores back as the full pixel. Converts an alpha-only
 * bitmap into a grayscale ARGB bitmap.
 */
void FUN_00077540(void *bitmap)
{
    unsigned int *pixels;
    int count;
    unsigned char alpha;
    unsigned int expanded;

    pixels = (unsigned int *)bitmap_2d_address(bitmap, 0, 0, 0);
    count = bitmap_get_pixel_count(bitmap);
    if (count > 0) {
        do {
            alpha = ((unsigned char *)pixels)[3];
            expanded = alpha;
            expanded = (expanded << 8) | alpha;
            expanded = (expanded << 8) | alpha;
            expanded = (expanded << 8) | alpha;
            *pixels = expanded;
            pixels++;
            count--;
        } while (count != 0);
    }
}

/*
 * FUN_0007a750 -- real_rgb_color_brightness: compute luminance of an RGB color.
 *
 * Returns the dot product of the color with standard luminance coefficients
 * (0.299, 0.587, 0.114) stored at globals 0x2647c0-c8.
 */
float real_rgb_color_brightness(float *color)
{
    return color[0] * *(float *)0x2647c0
         + color[1] * *(float *)0x2647c4
         + color[2] * *(float *)0x2647c8;
}

/*
 * FUN_0007ae70 -- argb_color_to_real_argb_color: convert 4 unsigned shorts
 * to 4 floats, scaled by 1/65535.
 *
 * Each component is zero-extended from ushort to int, then converted to float
 * and multiplied by the scale factor at 0x264154.
 */
void argb_color_to_real_argb_color(unsigned short *src, float *dst)
{
    int val;

    val = src[0];
    dst[0] = (float)val * *(float *)0x264154;
    val = src[1];
    dst[1] = (float)val * *(float *)0x264154;
    val = src[2];
    dst[2] = (float)val * *(float *)0x264154;
    val = src[3];
    dst[3] = (float)val * *(float *)0x264154;
}

/*
 * FUN_0007aed0 -- rgb_color_to_real_rgb_color: convert 3 unsigned shorts
 * to 3 floats, scaled by 1/65535.
 *
 * Same pattern as argb_color_to_real_argb_color but only 3 components.
 */
void rgb_color_to_real_rgb_color(unsigned short *src, float *dst)
{
    int val;

    val = src[0];
    dst[0] = (float)val * *(float *)0x264154;
    val = src[1];
    dst[1] = (float)val * *(float *)0x264154;
    val = src[2];
    dst[2] = (float)val * *(float *)0x264154;
}

/*
 * FUN_0007af20 -- pixel32_to_real_argb_color: extract ARGB from a packed
 * uint32 into 4 floats, scaled by 1/255.
 *
 * Byte layout: bits 31-24 = A, 23-16 = R, 15-8 = G, 7-0 = B.
 * Uses MSVC's unsigned-to-float pattern (FILD + TEST/JGE/FADD fixup).
 */
void pixel32_to_real_argb_color(unsigned int color, float *dst)
{
    unsigned int a, r, g, b;

    a = color >> 24;
    dst[0] = (float)a * *(float *)0x261518;
    r = (color >> 16) & 0xff;
    dst[1] = (float)r * *(float *)0x261518;
    g = (color >> 8) & 0xff;
    dst[2] = (float)g * *(float *)0x261518;
    b = color & 0xff;
    dst[3] = (float)b * *(float *)0x261518;
}

/*
 * FUN_0007afb0 -- pixel32_to_real_rgb_color: extract RGB from a packed
 * uint32 into 3 floats, scaled by 1/255.
 *
 * Byte layout: bits 23-16 = R, 15-8 = G, 7-0 = B (alpha ignored).
 * Uses MSVC's unsigned-to-float pattern (FILD + TEST/JGE/FADD fixup).
 */
void pixel32_to_real_rgb_color(unsigned int color, float *dst)
{
    unsigned int r, g, b;

    r = (color >> 16) & 0xff;
    dst[0] = (float)r * *(float *)0x261518;
    g = (color >> 8) & 0xff;
    dst[1] = (float)g * *(float *)0x261518;
    b = color & 0xff;
    dst[2] = (float)b * *(float *)0x261518;
}