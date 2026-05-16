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