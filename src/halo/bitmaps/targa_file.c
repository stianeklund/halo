/* On-disk Truevision Targa (.tga) file header, written ahead of the pixel
 * data. Natural alignment already reproduces the 18-byte on-disk layout --
 * every 16-bit field lands on an even offset -- so no packing pragma is
 * required. Offsets confirmed against the stores at 0x7f47c-0x7f49c. */
typedef struct {
  uint8_t id_length; ///< offset=0x00
  uint8_t color_map_type; ///< offset=0x01
  uint8_t image_type; ///< offset=0x02
  uint8_t color_map_spec[5]; ///< offset=0x03 (left zeroed by the memset)
  int16_t x_origin; ///< offset=0x08
  int16_t y_origin; ///< offset=0x0a
  int16_t width; ///< offset=0x0c
  int16_t height; ///< offset=0x0e
  uint8_t bits_per_pixel; ///< offset=0x10
  uint8_t image_descriptor; ///< offset=0x11
} targa_header_t;
cs(targa_header_t, 0x12);
co(targa_header_t, image_type, 0x02);
co(targa_header_t, x_origin, 0x08);
co(targa_header_t, y_origin, 0x0a);
co(targa_header_t, width, 0x0c);
co(targa_header_t, height, 0x0e);
co(targa_header_t, bits_per_pixel, 0x10);
co(targa_header_t, image_descriptor, 0x11);

/* Write a 2D x8r8g8b8 bitmap out as an uncompressed 32-bit Targa file.
 *
 * Returns NULL on success, or a static string describing the first failure.
 * The file is closed on every path that managed to open it; the
 * "couldn't open file" path returns before any close (confirmed: the epilogue
 * at 0x7f561 does not call file_close).
 *
 * The bitmap is addressed through raw offsets (+4 width, +6 height, +0xa type,
 * +0xc format) to stay consistent with tiff_export in the neighbouring TU --
 * the bitmap struct itself has not been recovered yet.
 */
const char *targa_export(file_ref_t *file, __int16 *bitmap)
{
  const char *error_message = NULL;
  targa_header_t header;
  int row_size;
  int y;
  void *pixels;

  assert_halt_at("c:\\halo\\SOURCE\\bitmaps\\targa_file.c", 0x24, file);
  assert_halt_at("c:\\halo\\SOURCE\\bitmaps\\targa_file.c", 0x25, bitmap);

  /* Conditions below are asserted with the original expression text; the
   * bitmap fields are still raw offsets here, so #cond would not reproduce
   * the string the binary references. */
  if (*(int16_t *)((char *)bitmap + 0xa) != 0) {
    display_assert("bitmap->type==_bitmap_type_2d",
                   "c:\\halo\\SOURCE\\bitmaps\\targa_file.c", 0x26, true);
    system_exit(-1);
  }
  if (*(int16_t *)((char *)bitmap + 0xc) != 10) {
    display_assert("bitmap->format==_bitmap_format_x8r8g8b8",
                   "c:\\halo\\SOURCE\\bitmaps\\targa_file.c", 0x27, true);
    system_exit(-1);
  }

  if (file_create(file)) {
    if (file_open(file, 2)) {
      csmemset(&header, 0, sizeof(header));
      header.id_length = 0;
      header.color_map_type = 0;
      header.image_type = 2; /* uncompressed true-color */
      header.x_origin = 0;
      header.y_origin = 0;
      header.width = *(int16_t *)((char *)bitmap + 4);
      header.height = *(int16_t *)((char *)bitmap + 6);
      header.bits_per_pixel = 0x20;
      header.image_descriptor = 0x28; /* 8 alpha bits, top-left origin */

      /* Success is the fall-through: the original places the header-failure
       * assignment late, at 0x7f547 after the row loop (JZ 0x7f547). */
      if (file_write(file, sizeof(header), &header)) {
        row_size = (int)*(int16_t *)((char *)bitmap + 4) * 4;
        for (y = 0; y < *(int16_t *)((char *)bitmap + 6); y++) {
          pixels = bitmap_2d_address(bitmap, 0, y, 0);
          assert_halt_at("c:\\halo\\SOURCE\\bitmaps\\targa_file.c", 0x43,
                         pixels);
          if (!file_write(file, row_size, pixels)) {
            error_message = "couldn't write row";
            break;
          }
        }
      } else {
        error_message = "couldn't write header";
      }

      file_close(file);
      return error_message;
    }
  }

  return "couldn't open file";
}
