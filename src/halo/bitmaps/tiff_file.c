const char *tiff_export(file_ref_t *info, __int16 *bitmap)
{
  const char *error_message = NULL;
  int tiff_format = 0;
  int photometric = 1;
  int samples_per_pixel = 1;
  char path[256];
  int tiff;
  int row_size;
  uint8_t *row_buffer;
  int y;

  switch (*(int16_t *)((char *)bitmap + 0xc)) {
  case 0:
  case 1:
  case 2:
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

  file_reference_get_name(info, 0xd, path);
  tiff = FUN_0006d8e0(path, "w");
  if (tiff == 0)
    return "failed to open tiff";

  {
    int bits_per_pixel = bitmap_format_bits_per_pixel((int16_t)tiff_format);
    int width = (int)*(int16_t *)((char *)bitmap + 0x4);
    int row_bits = bits_per_pixel * width;
    row_size = (int)(int16_t)((row_bits + ((row_bits >> 0x1f) & 7)) >> 3);
  }

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
    int x;

    switch (*(int16_t *)((char *)bitmap + 0xc)) {
    case 6:
      for (x = 0; x < *(int16_t *)((char *)bitmap + 0x4); x++) {
        uint16_t pixel = ((uint16_t *)src_row)[x];
        uint8_t high = (uint8_t)(pixel >> 8);

        row_buffer[x * 4 + 2] =
          ((uint8_t)(pixel >> 2) & 7) | (uint8_t)(pixel << 3);
        row_buffer[x * 4 + 1] = (high >> 1 & 3) | (uint8_t)(pixel >> 5) << 2;
        row_buffer[x * 4 + 0] = (high & 0xf8) | (high >> 5);
        row_buffer[x * 4 + 3] = 0xff;
      }
      break;

    case 8:
      for (x = 0; x < *(int16_t *)((char *)bitmap + 0x4); x++) {
        uint16_t pixel = ((uint16_t *)src_row)[x];
        uint8_t middle = (uint8_t)(pixel >> 5);

        row_buffer[x * 4 + 2] =
          (((uint8_t)pixel & 0x1f) | ((uint8_t)pixel << 1)) << 2;
        row_buffer[x * 4 + 1] = ((middle & 0x1f) | (middle << 1)) << 2;
        row_buffer[x * 4 + 0] =
          (((uint8_t)(pixel >> 7) & 0xfb) | (uint8_t)(pixel >> 8)) & 0xfc;
        row_buffer[x * 4 + 3] = 0xff;
      }
      break;

    case 9:
      for (x = 0; x < *(int16_t *)((char *)bitmap + 0x4); x++) {
        uint16_t pixel = ((uint16_t *)src_row)[x];
        uint8_t high = (uint8_t)(pixel >> 8);
        uint8_t middle = (uint8_t)(pixel >> 4);

        row_buffer[x * 4 + 3] = (high >> 4) | ((high >> 4) << 4);
        row_buffer[x * 4 + 2] = ((uint8_t)pixel & 0xf) | ((uint8_t)pixel << 4);
        row_buffer[x * 4 + 1] = (middle & 0xf) | (middle << 4);
        row_buffer[x * 4 + 0] = (high & 0xf) | (high << 4);
      }
      break;

    case 10:
      for (x = 0; x < *(int16_t *)((char *)bitmap + 0x4); x++) {
        uint32_t pixel = ((uint32_t *)src_row)[x];

        row_buffer[x * 4 + 2] = (uint8_t)pixel;
        row_buffer[x * 4 + 1] = (uint8_t)(pixel >> 8);
        row_buffer[x * 4 + 0] = (uint8_t)(pixel >> 0x10);
        row_buffer[x * 4 + 3] = 0xff;
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
