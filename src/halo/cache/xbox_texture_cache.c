/* Xbox texture cache: retrieve and block on hardware texture data.
 * Source: c:\halo\SOURCE\cache\xbox_texture_cache.c */
void *xbox_texture_cache_steal_memory(unsigned int size)
{
  int page_count = ((int)(size + (((int)size >> 0x1f) & 0x3fffU)) >> 0xe) + 1;
  int remaining_page_count = 0x4fe - page_count;
  char *base = (char *)FUN_001bdd60() + (remaining_page_count << 0xe);
  int stolen_size = page_count << 0xe;

  if (remaining_page_count < 1) {
    display_assert("remaining_page_count>0",
                   "c:\\halo\\SOURCE\\cache\\xbox_texture_cache.c", 0x13f,
                   true);
    system_exit(-1);
  }

  if (*(int8_t *)0x4ea984 != 0) {
    display_assert("!xbox_texture_cache_globals.stolen_memory",
                   "c:\\halo\\SOURCE\\cache\\xbox_texture_cache.c", 0x140,
                   true);
    system_exit(-1);
  }

  FUN_0011db00(*(void **)0x4ea980, remaining_page_count);
  physical_memory_protect(base + 0x104000, stolen_size, 4);
  physical_memory_protect(base, 0x104000, 2);
  physical_memory_protect(base + 0x104000 + stolen_size, 0x104000, 2);
  *(int8_t *)0x4ea984 = 1;
  return base + 0x104000;
}

void xbox_texture_cache_return_memory(void)
{
  if (*(int8_t *)0x4ea984 == 0) {
    display_assert("xbox_texture_cache_globals.stolen_memory",
                   "c:\\halo\\SOURCE\\cache\\xbox_texture_cache.c", 0x159,
                   true);
    system_exit(-1);
  }

  FUN_0011db00(*(void **)0x4ea980, 0x580);
  physical_memory_protect(FUN_001bdd60(), 0x1600000, 0x404);
  *(int8_t *)0x4ea984 = 0;
}

/* bitmap_format_to_d3d_linear_format (0x1beba0)
 *
 * Look up the linear D3D texture format code for a bitmap format index.
 * Table at 0x2b9618 maps format indices 0..17 to D3D format codes.
 * If flags bit 0x20 is set and format is 10 or 11 (DXT4/DXT5), returns 0x33. */
int bitmap_format_to_d3d_linear_format(int16_t format, uint16_t flags)
{
  int *table = (int *)0x2b9618;

  if (format < 0 || format >= 0x12) {
    display_assert("format>=0 && format<NUMBER_OF_BITMAP_FORMATS",
                   "c:\\halo\\SOURCE\\cache\\xbox_texture_cache.c", 0x1e1, 1);
    system_exit(-1);
  }

  if (table[format] == -1) {
    display_assert("table[format]!=NONE",
                   "c:\\halo\\SOURCE\\cache\\xbox_texture_cache.c", 0x1e2, 1);
    system_exit(-1);
  }

  if ((flags & 0x20) && (format == 10 || format == 11))
    return 0x33;

  return table[format];
}

/* FUN_001bec30 (0x1bec30)
 *
 * Look up the swizzled D3D texture format code for a bitmap format index.
 * Table at 0x2b9660 maps format indices 0..17 to D3D format codes.
 * If flags bit 0x20 is set and format is 10 or 11 (DXT4/DXT5), returns 0x36. */
int FUN_001bec30(int16_t format, uint16_t flags)
{
  int *table = (int *)0x2b9660;

  if (format < 0 || format >= 0x12) {
    display_assert("format>=0 && format<NUMBER_OF_BITMAP_FORMATS",
                   "c:\\halo\\SOURCE\\cache\\xbox_texture_cache.c", 0x206, 1);
    system_exit(-1);
  }

  if (table[format] == -1) {
    display_assert("table[format]!=NONE",
                   "c:\\halo\\SOURCE\\cache\\xbox_texture_cache.c", 0x207, 1);
    system_exit(-1);
  }

  if ((flags & 0x20) && (format == 10 || format == 11))
    return 0x36;

  return table[format];
}

/* xbox_texture_cache_setup_d3d_texture (0x1bee30)
 *
 * Populate a D3D texture resource header from a bitmap hardware format.
 * Two paths: if bitmap flags bit 0x10 is set, builds a swizzled texture
 * descriptor with pitch-based size encoding; otherwise builds a linear
 * texture descriptor using log2 dimensions and mipmap level count.
 * Finishes by registering the resource with D3DResource_Register.
 *
 * bitmap  is passed in ESI (hardware_format pointer).
 * texture is passed in EDI (D3D texture header, 5 dwords / 20 bytes). */
void xbox_texture_cache_setup_d3d_texture(void *bitmap /* @<esi> */,
                                          void *texture /* @<edi> */)
{
  int *tex = (int *)texture;
  char *bmp = (char *)bitmap;
  int format_bits;
  int pitch;
  int height;
  int width;

  assert_halt(bitmap);
  assert_halt(texture);

  tex[1] = 0;
  tex[2] = 0;
  tex[0] = 0x40001;

  if (*(uint16_t *)(bmp + 0xe) & 0x10) {
    format_bits =
      FUN_001bec30(*(int16_t *)(bmp + 0xc), *(uint16_t *)(bmp + 0xe));
    tex[3] = (format_bits << 8) | 0x10029;

    pitch = FUN_0007d9f0(bitmap, 0);
    height = (int)*(int16_t *)(bmp + 0x6);
    width = (int)*(int16_t *)(bmp + 0x4);
    tex[4] =
      ((((pitch + ((pitch >> 31) & 0x3f)) >> 6) - 1) << 12 | (height - 1))
        << 12 |
      (width - 1);
  } else {
    int16_t log2_depth = FUN_00108db0((int)*(int16_t *)(bmp + 0x8));
    int16_t log2_height = FUN_00108db0((int)*(int16_t *)(bmp + 0x6));
    int16_t log2_width = FUN_00108db0((int)*(int16_t *)(bmp + 0x4));
    int linear_fmt = bitmap_format_to_d3d_linear_format(
      *(int16_t *)(bmp + 0xc), *(uint16_t *)(bmp + 0xe));
    int16_t mipmap_count = *(int16_t *)(bmp + 0xa);
    int16_t dim_level = FUN_00183120(bitmap);
    int dim_type = (mipmap_count != 1) ? 2 : 3;
    int cubemap_flag = (mipmap_count == 2) ? 4 : 0;

    format_bits = (int)log2_depth;
    format_bits = (format_bits << 4) | (int)log2_height;
    format_bits = (format_bits << 4) | (int)log2_width;
    format_bits = (format_bits << 12) | linear_fmt;
    format_bits = (format_bits << 4) | dim_type;
    format_bits = (format_bits << 4) | (((int)dim_level + 1) << 16);
    format_bits |= cubemap_flag | 0x9;

    tex[4] = 0;
    tex[3] = format_bits;
  }

  D3DResource_Register(texture, *(void **)(bmp + 0x2c));
}

bool xbox_texture_cache_request(void *hardware_format @<eax>, bool block)
{
  int cache_block_index = FUN_00183290(hardware_format);

  if (cache_block_index <= *(int32_t *)((char *)hardware_format + 0x1c)) {
    cache_block_index = *(int32_t *)((char *)hardware_format + 0x1c);
  }

  cache_block_index = FUN_0011de10(*(void **)0x4ea980, cache_block_index);
  if (cache_block_index != -1) {
    int cache_page_index = FUN_0011da00(*(void **)0x4ea980, cache_block_index) +
                           *(int32_t *)0x4ea97c;
    int new_texture_index = FUN_00119570(*(void **)0x4ea978, cache_block_index);
    char *cache_entry = datum_get(*(void **)0x4ea978, cache_block_index);

    if (new_texture_index != cache_block_index) {
      display_assert("new_texture_index==cache_block_index",
                     "c:\\halo\\SOURCE\\cache\\xbox_texture_cache.c", 0x1af,
                     true);
      system_exit(-1);
    }

    *(int32_t *)((char *)hardware_format + 0x24) = cache_block_index;
    *(int32_t *)((char *)hardware_format + 0x2c) = cache_page_index;
    *(void **)(cache_entry + 8) = hardware_format;
    xbox_texture_cache_setup_d3d_texture(hardware_format, cache_entry + 0xc);
    *(int16_t *)(cache_entry + 2) =
      FUN_001bc9e0(*(int32_t *)((char *)hardware_format + 0x20),
                   *(int32_t *)((char *)hardware_format + 0x18),
                   *(int32_t *)((char *)hardware_format + 0x1c),
                   cache_page_index, cache_entry + 4, block);
    return true;
  }

  return false;
}

void *xbox_texture_cache_get_hardware_format(void *hardware_format, bool block,
                                             bool load)
{
  void *result = NULL;

  if (!load && block) {
    display_assert("load || !block",
                   "c:\\halo\\SOURCE\\cache\\xbox_texture_cache.c", 0xd2, true);
    system_exit(-1);
  }

  if (*(int8_t *)((char *)hardware_format + 0xe) < 0) {
    if (*(int32_t *)((char *)hardware_format + 0x24) == -1 && load) {
      xbox_texture_cache_request(hardware_format, block);
    }
    if (*(int32_t *)((char *)hardware_format + 0x24) != -1) {
      void *entry = datum_get(*(void **)0x4ea978,
                              *(int32_t *)((char *)hardware_format + 0x24));
      lruv_debug_to_file(*(void **)0x4ea980,
                         *(int32_t *)((char *)hardware_format + 0x24));
      if (block) {
        if (*(int8_t *)((char *)entry + 4) != 0)
          goto loaded;
        if (*(uint8_t *)0x4ea98a) {
          const char *name =
            tag_get_name(*(int32_t *)((char *)hardware_format + 0x20));
          console_warning((const char *)0x257984, name);
        }
        cache_files_io_request_enable(*(int16_t *)((char *)entry + 2));
      }
      do {
        if (*(int8_t *)((char *)entry + 4) == 0) {
          unsigned int t0 = FUN_001cb8e0();
          unsigned int t1 = system_milliseconds();
          if (t1 - t0 > 0x84u) {
            FUN_001cf2f0();
          }
          SwitchToThread();
        } else {
        loaded:
          if (*(int8_t *)((char *)entry + 5) == 0) {
            *(int8_t *)((char *)entry + 5) = 1;
          }
          result = (char *)entry + 0xc;
          if (result)
            break;
        }
        if (!block)
          return result;
      } while (true);
    }
  } else {
    result = *(void **)((char *)hardware_format + 0x28);
  }

  if (block && !result) {
    unsigned int now = system_milliseconds();
    if (now - *(unsigned int *)0x4ea98c > 10000u) {
      terminal_output(
        *(void **)0x2ee6f4,
        "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!",
        NULL);
      error(2, "YOU GOT STABBED!!!! double-click \"GETSTABBED.BAT\" on your PC "
               "now!!!");
      terminal_output(
        *(void **)0x2ee6f4,
        "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!",
        NULL);
      FUN_0011db90("d:\\stabbed.txt",
                   tag_get_name(*(int32_t *)((char *)hardware_format + 0x20)),
                   *(int32_t *)((char *)hardware_format + 0x1c),
                   *(void **)0x4ea980, (void *)0x18ef30, (void *)0x1beb70);
      *(unsigned int *)0x4ea98c = system_milliseconds();
    }
    result = FUN_00155580(hardware_format);
    if (!result) {
      display_assert("hardware_format",
                     "c:\\halo\\SOURCE\\cache\\xbox_texture_cache.c", 0x127,
                     true);
      system_exit(-1);
    }
  }

  return result;
}
