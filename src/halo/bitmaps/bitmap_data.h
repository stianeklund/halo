/* bitmap_data layout shared by src/halo/bitmaps/, cache/ and rasterizer/.
 *
 * Repo-local header: the original header path is not binary-proven.  It is
 * not in types.h because adding a struct definition there moves the VC71
 * code of unrelated functions (ai_communication_update_speech_timers
 * -7 bytes) while the same text in a separate header does not.
 * Evidence table: recovery/evidence/bitmap_data.json. */

#ifndef HALO_BITMAPS_BITMAP_DATA_H
#define HALO_BITMAPS_BITMAP_DATA_H

#include "../../types.h"

/* bitmap_data.flags bit indices.  Names are the identifiers in the binary's
 * TEST_FLAG assert strings; indices are the masks those asserts test. */
enum {
  _bitmap_has_power_of_two_dimensions_bit = 0,
  _bitmap_compressed_bit = 1,
  _bitmap_swizzled_bit = 3,
  _bitmap_linear_bit = 4,
  _bitmap_cached_bit = 7
};

/* bitmap_data (one element of the 'bitm' tag's bitmap_data block; also the
 * runtime bitmap built by bitmap_2d_new).  Evidence table:
 * recovery/evidence/bitmap_data.json.  Names: assert strings (T1). */
typedef struct bitmap_data {
  uint32_t field_00;         ///< offset=0x00  'bitm' constant stored by bitmap_2d_new
  int16_t width;             ///< offset=0x04
  int16_t height;            ///< offset=0x06
  int16_t depth;             ///< offset=0x08
  int16_t type;              ///< offset=0x0A
  int16_t format;            ///< offset=0x0C
  uint16_t flags;            ///< offset=0x0E  _bitmap_*_bit
  int16_t field_10;          ///< offset=0x10  word pair with field_12
  int16_t field_12;          ///< offset=0x12
  int16_t mipmap_count;      ///< offset=0x14
  uint8_t pad_16[2];         ///< offset=0x16  never observed accessed
  int32_t field_18;          ///< offset=0x18  pixel data offset (texture_cache_bitmap_new)
  int32_t field_1c;          ///< offset=0x1C  pixel data size
  int32_t field_20;          ///< offset=0x20  owning 'bitm' tag index
  int32_t cache_block_index; ///< offset=0x24
  void *hardware_format;     ///< offset=0x28
  void *base_address;        ///< offset=0x2C
} bitmap_data;
cs(bitmap_data, 0x30);
co(bitmap_data, field_00, 0x00);
co(bitmap_data, width, 0x04);
co(bitmap_data, height, 0x06);
co(bitmap_data, depth, 0x08);
co(bitmap_data, type, 0x0A);
co(bitmap_data, format, 0x0C);
co(bitmap_data, flags, 0x0E);
co(bitmap_data, field_10, 0x10);
co(bitmap_data, field_12, 0x12);
co(bitmap_data, mipmap_count, 0x14);
co(bitmap_data, pad_16, 0x16);
co(bitmap_data, field_18, 0x18);
co(bitmap_data, field_1c, 0x1C);
co(bitmap_data, field_20, 0x20);
co(bitmap_data, cache_block_index, 0x24);
co(bitmap_data, hardware_format, 0x28);
co(bitmap_data, base_address, 0x2C);

#endif /* HALO_BITMAPS_BITMAP_DATA_H */
