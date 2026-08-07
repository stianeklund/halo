bool tiff_get_bounds(file_ref_t *info, int *width_out, int *height_out)
{
  char path[256];
  unsigned char ok;
  int tiff;

  ok = 0;
  tiff = FUN_0006d8e0(file_reference_get_name(info, 0xd, path), "r");
  if (tiff != 0) {
    TIFFGetField(tiff, 0x100, width_out);
    TIFFGetField(tiff, 0x101, height_out);
    FUN_00064ee0(tiff);
    ok = 1;
  }
  return ok;
}

const char *tiff_export(file_ref_t *info, __int16 *bitmap)
{
  const char *error_message = NULL;
  int tiff_format = 0;
  short photometric;
  short samples_per_pixel;
  char path[256];
  int tiff;
  int row_size;
  uint8_t *row_buffer;
  int y;

  switch (*(int16_t *)((char *)bitmap + 0xc)) {
  case 0:
  case 1:
  case 2:
    photometric = 1;
    samples_per_pixel = 1;
    break;
  case 6:
  case 8:
  case 9:
  case 10:
  case 11:
    tiff_format = 11;
    photometric = 2;
    samples_per_pixel = 4;
    break;
  default:
    return "invalid bitmap encoding for tiff export.";
  }

  tiff = FUN_0006d8e0(file_reference_get_name(info, 0xd, path), "w");
  if (tiff == 0)
    return "failed to open tiff";

  row_size =
    (int)(int16_t)(((int)bitmap_format_bits_per_pixel((int16_t)tiff_format) *
                    (int)*(int16_t *)((char *)bitmap + 0x4)) /
                   8);

  row_buffer = (uint8_t *)debug_malloc(
    row_size, 0, "c:\\halo\\SOURCE\\bitmaps\\tiff_file.c", 0x6b);
  if (!row_buffer) {
    FUN_00064ee0(tiff);
    return "out of memory";
  }

  TIFFSetField(tiff, 0x100, (int)*(int16_t *)((char *)bitmap + 0x4));
  TIFFSetField(tiff, 0x101, (int)*(int16_t *)((char *)bitmap + 0x6));
  TIFFSetField(tiff, 0x103, 5);
  TIFFSetField(tiff, 0x106, photometric);
  TIFFSetField(tiff, 0x11c, 1);
  TIFFSetField(tiff, 0x115, samples_per_pixel);
  TIFFSetField(tiff, 0x102, 8);
  TIFFSetField(tiff, 0x112, 1);

  for (y = 0; y < *(int16_t *)((char *)bitmap + 0x6); y++) {
    uint8_t *src_row = (uint8_t *)bitmap_2d_address(bitmap, 0, y, 0);
    short x;

    switch (*(int16_t *)((char *)bitmap + 0xc)) {
    case 8:
      for (x = 0; x < *(int16_t *)((char *)bitmap + 0x4); x++) {
        uint16_t pixel = ((uint16_t *)src_row)[x];
        unsigned short middle = pixel >> 5;

        row_buffer[x * 4 + 2] =
          (((uint8_t)pixel & 0x1f) | ((uint8_t)pixel << 1)) << 2;
        row_buffer[x * 4 + 3] = 0xff;
        row_buffer[x * 4 + 1] = ((middle & 0x1f) | (middle << 1)) << 2;
        row_buffer[x * 4 + 0] =
          (((uint8_t)(pixel >> 7) & 0xfb) | (uint8_t)(pixel >> 8)) & 0xfc;
      }
      break;

    case 6:
      for (x = 0; x < *(int16_t *)((char *)bitmap + 0x4); x++) {
        uint16_t pixel = ((uint16_t *)src_row)[x];
        uint8_t high = (uint8_t)(pixel >> 8);

        row_buffer[x * 4 + 2] =
          ((uint8_t)(pixel >> 2) & 7) | (uint8_t)(pixel << 3);
        row_buffer[x * 4 + 3] = 0xff;
        row_buffer[x * 4 + 1] = ((pixel >> 9) & 3) | (uint8_t)(pixel >> 5) << 2;
        row_buffer[x * 4 + 0] = (high & 0xf8) | (pixel >> 13);
      }
      break;

    case 9:
      for (x = 0; x < *(int16_t *)((char *)bitmap + 0x4); x++) {
        uint16_t pixel = ((uint16_t *)src_row)[x];
        uint8_t high = (uint8_t)(pixel >> 8);
        unsigned short middle = pixel >> 4;
        unsigned short top = pixel >> 12;

        row_buffer[x * 4 + 3] = (top << 4) | (top & 0xf);
        row_buffer[x * 4 + 2] = ((uint8_t)pixel & 0xf) | ((uint8_t)pixel << 4);
        row_buffer[x * 4 + 1] = (middle << 4) | (middle & 0xf);
        row_buffer[x * 4 + 0] = (high & 0xf) | (high << 4);
      }
      break;

    case 10:
      for (x = 0; x < *(int16_t *)((char *)bitmap + 0x4); x++) {
        uint32_t pixel = ((uint32_t *)src_row)[x];

        row_buffer[x * 4 + 2] = (uint8_t)pixel;
        row_buffer[x * 4 + 3] = 0xff;
        row_buffer[x * 4 + 1] = (uint8_t)(pixel >> 8);
        row_buffer[x * 4 + 0] = (uint8_t)(pixel >> 0x10);
      }
      break;

    case 11:
      for (x = 0; x < *(int16_t *)((char *)bitmap + 0x4); x++) {
        uint32_t pixel = ((uint32_t *)src_row)[x];

        row_buffer[x * 4 + 3] = (uint8_t)(pixel >> 0x18);
        row_buffer[x * 4 + 2] = (uint8_t)pixel;
        row_buffer[x * 4 + 1] = (uint8_t)(pixel >> 8);
        row_buffer[x * 4 + 0] = (uint8_t)(pixel >> 0x10);
      }
      break;

    default:
      csmemcpy(row_buffer, src_row, row_size);
      break;
    }

    if (TIFFWriteScanline(tiff, row_buffer, y, 0) < 0) {
      error_message = "failed to write scanline";
      break;
    }
  }

  debug_free(row_buffer, "c:\\halo\\SOURCE\\bitmaps\\tiff_file.c", 0xe7);
  FUN_00064ee0(tiff);
  return error_message;
}

const char *tiff_import(file_ref_t *info, void **bitmap_out,
                        int16_t *bounds_opt, int16_t format)
{
  const char *error_message;
  char path[256];
  int16_t rect[4];
  int width;
  int height;
  short bits_per_sample;
  short orientation;
  short samples_per_pixel;
  short planar_config;
  short photometric;
  int tiff;
  int scanline_size;
  void *bitmap;
  uint8_t *row_buffer;
  int y_pack;
  int row;
  short x;
  int sx;
  uint8_t *dst;
  uint8_t v;
  uint8_t *src_px;
  uint32_t pixel;
  short rect_w;
  short rect_h;

  error_message = NULL;

  if (file_exists(info)) {
    tiff = FUN_0006d8e0(file_reference_get_name(info, 0xd, path), "r");
    if (tiff != 0) {
      scanline_size = TIFFScanlineSize(tiff);
      FUN_00064ec0(tiff, 0x102, &bits_per_sample);
      FUN_00064ec0(tiff, 0x112, &orientation);
      FUN_00064ec0(tiff, 0x115, &samples_per_pixel);
      TIFFGetField(tiff, 0x11c, &planar_config);
      TIFFGetField(tiff, 0x106, &photometric);
      TIFFGetField(tiff, 0x100, &width);
      TIFFGetField(tiff, 0x101, &height);

      if (bounds_opt != NULL) {
        *(int *)&rect[0] = *(int *)bounds_opt;
        *(int *)&rect[2] = *((int *)bounds_opt + 1);
      } else {
        rect[0] = 0;
        rect[1] = 0;
        rect[2] = (int16_t)height;
        rect[3] = (int16_t)width;
      }

      if (orientation == 1) {
        if (bits_per_sample == 8 &&
            (samples_per_pixel == 4 || samples_per_pixel == 3 ||
             samples_per_pixel == 2 || samples_per_pixel == 1)) {
          if (format == -1 || format == 0xb) {
            if (planar_config == 1) {
              rect_w = (short)FUN_00108a10(rect);
              rect_h = (short)FUN_00108a30(rect);
              if (rect_w >= 0 && rect_w <= 30000 && rect_h >= 0 &&
                  rect_h <= 30000) {
                bitmap = bitmap_2d_new((unsigned short)rect_w,
                                       (unsigned short)rect_h, 0, 0xb);
                row_buffer = (uint8_t *)debug_malloc(
                  (uint32_t)scanline_size, 0,
                  "c:\\halo\\SOURCE\\bitmaps\\tiff_file.c", 0x13f);

                if (bitmap != NULL && row_buffer != NULL) {
                  *bitmap_out = bitmap;
                  y_pack = *(int *)&rect[0];
                  if ((short)y_pack < rect[2]) {
                    do {
                      if ((short)y_pack < 0)
                        row = 0;
                      else {
                        row = height - 1;
                        if ((unsigned int)(int)(short)y_pack <=
                            (unsigned int)row)
                          row = (int)(short)y_pack;
                      }

                      if (FUN_0006f040(tiff, row_buffer, row, 0) < 0) {
                        error_message = "failed to read TIFF scan line";
                        bitmap_delete(bitmap);
                        break;
                      }

                      switch ((unsigned short)samples_per_pixel) {
                      case 1:
                        dst = (uint8_t *)bitmap_2d_address(
                          bitmap, 0, (short)(y_pack - *(int *)&rect[0]), 0);
                        for (x = rect[1]; x < rect[3]; x = (short)(x + 1)) {
                          if (x < 0)
                            sx = 0;
                          else {
                            sx = width - 1;
                            if ((unsigned int)(int)x <= (unsigned int)sx)
                              sx = (int)x;
                          }
                          v = row_buffer[(short)sx];
                          pixel = (uint32_t)v;
                          pixel = (pixel << 8) | (uint32_t)v;
                          pixel = (pixel << 8) | (uint32_t)v;
                          pixel = (pixel << 8) | (uint32_t)v;
                          *(uint32_t *)(dst + ((int)x - (int)rect[1]) * 4) =
                            pixel;
                        }
                        break;

                      case 2:
                        dst = (uint8_t *)bitmap_2d_address(
                          bitmap, 0, (short)(y_pack - *(int *)&rect[0]), 0);
                        for (x = rect[1]; x < rect[3]; x = (short)(x + 1)) {
                          if (x < 0)
                            sx = 0;
                          else {
                            sx = width - 1;
                            if ((unsigned int)(int)x <= (unsigned int)sx)
                              sx = (int)x;
                          }
                          src_px = row_buffer + sx * 2;
                          v = src_px[0];
                          pixel = ((uint32_t)src_px[1] << 8) | (uint32_t)v;
                          pixel = (pixel << 8) | (uint32_t)v;
                          pixel = (pixel << 8) | (uint32_t)v;
                          *(uint32_t *)(dst + ((int)x - (int)rect[1]) * 4) =
                            pixel;
                        }
                        break;

                      case 3:
                        dst = (uint8_t *)bitmap_2d_address(
                          bitmap, 0, (short)(y_pack - *(int *)&rect[0]), 0);
                        for (x = rect[1]; x < rect[3]; x = (short)(x + 1)) {
                          if (x < 0)
                            sx = 0;
                          else {
                            sx = width - 1;
                            if ((unsigned int)(int)x <= (unsigned int)sx)
                              sx = (int)x;
                          }
                          src_px = row_buffer + sx * 3;
                          pixel = ((uint32_t)src_px[0] | 0xffffff00u) << 8;
                          pixel = (pixel | (uint32_t)src_px[1]) << 8;
                          pixel = pixel | (uint32_t)src_px[2];
                          *(uint32_t *)(dst + ((int)x - (int)rect[1]) * 4) =
                            pixel;
                        }
                        break;

                      case 4:
                        dst = (uint8_t *)bitmap_2d_address(
                          bitmap, 0, (short)(y_pack - *(int *)&rect[0]), 0);
                        for (x = rect[1]; x < rect[3]; x = (short)(x + 1)) {
                          if (x < 0)
                            sx = 0;
                          else {
                            sx = width - 1;
                            if ((unsigned int)(int)x <= (unsigned int)sx)
                              sx = (int)x;
                          }
                          src_px = row_buffer + sx * 4;
                          pixel =
                            ((uint32_t)src_px[3] << 8) | (uint32_t)src_px[0];
                          pixel = (pixel << 8) | (uint32_t)src_px[1];
                          pixel = (pixel << 8) | (uint32_t)src_px[2];
                          *(uint32_t *)(dst + ((int)x - (int)rect[1]) * 4) =
                            pixel;
                        }
                        break;

                      default:
                        display_assert(NULL,
                                       "c:\\halo\\SOURCE\\bitmaps\\tiff_file.c",
                                       0x196, true);
                        system_exit(-1);
                      }

                      y_pack = y_pack + 1;
                    } while ((short)y_pack < rect[2]);
                  }
                } else {
                  error_message = "out of memory";
                  if (bitmap != NULL)
                    bitmap_delete(bitmap);
                }

                if (row_buffer != NULL)
                  debug_free(row_buffer,
                             "c:\\halo\\SOURCE\\bitmaps\\tiff_file.c", 0x1a6);
                FUN_00064ee0(tiff);
                return error_message;
              }
              error_message = "TIFF too large";
              FUN_00064ee0(tiff);
              return error_message;
            }
            error_message =
              "unsupported TIFF photometric, planar configuration";
            FUN_00064ee0(tiff);
            return error_message;
          }
          error_message = "unsupported format";
          FUN_00064ee0(tiff);
          return error_message;
        }
        snprintf((char *)0x334580, 0x200,
                 "unsupported bits per sample (%d) or sample count (%d)",
                 (unsigned int)(unsigned short)bits_per_sample,
                 (unsigned int)(unsigned short)samples_per_pixel);
        error_message = (const char *)0x334580;
        FUN_00064ee0(tiff);
        return error_message;
      }
      error_message = "unsupported TIFF orientation (must be top left)";
      FUN_00064ee0(tiff);
      return error_message;
    }
    return "not a TIFF file";
  }
  return "file does not exist";
}
