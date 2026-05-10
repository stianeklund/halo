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

float *FUN_0007ace0(float *hsv, float *rgb_out)
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

bool FUN_0007b020(float *rgb)
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
