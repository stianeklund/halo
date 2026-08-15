/* Xbox texture cache: retrieve and block on hardware texture data.
 * Source: c:\halo\SOURCE\cache\xbox_texture_cache.c */
#ifdef HALO_RETAIL64
#define HALO_TEXTURE_CACHE_SIZE 0x800000
#else
#define HALO_TEXTURE_CACHE_SIZE 0x1600000
#endif

#define HALO_TEXTURE_CACHE_PAGE_BITS 0xe
#define HALO_TEXTURE_CACHE_PAGE_SIZE (1 << HALO_TEXTURE_CACHE_PAGE_BITS)
#define HALO_TEXTURE_CACHE_PAGE_COUNT \
  (HALO_TEXTURE_CACHE_SIZE >> HALO_TEXTURE_CACHE_PAGE_BITS)
#define HALO_TEXTURE_CACHE_STEAL_GUARD_SIZE 0x104000
#define HALO_TEXTURE_CACHE_STEAL_GUARD_PAGES \
  (HALO_TEXTURE_CACHE_STEAL_GUARD_SIZE >> HALO_TEXTURE_CACHE_PAGE_BITS)
#define HALO_TEXTURE_CACHE_STEALABLE_PAGES \
  (HALO_TEXTURE_CACHE_PAGE_COUNT - (HALO_TEXTURE_CACHE_STEAL_GUARD_PAGES * 2))

/* texture_cache_delete (0x1be920)
 *
 * Teardown: dispose the bitmap-entry data array, then the LRU-vector cache.
 * Order is load-bearing (the data array holds cache-block indices). */
void texture_cache_delete(void)
{
  data_dispose(*(data_t **)0x4ea978);
  lruv_cache_dispose(*(void **)0x4ea980);
}

void *xbox_texture_cache_steal_memory(unsigned int size)
{
  int page_count = ((int)size / HALO_TEXTURE_CACHE_PAGE_SIZE) + 1;
  int remaining_page_count = HALO_TEXTURE_CACHE_STEALABLE_PAGES - page_count;
  char *base = (char *)FUN_001bdd60() +
               (remaining_page_count * HALO_TEXTURE_CACHE_PAGE_SIZE);
  unsigned int stolen_size = page_count * HALO_TEXTURE_CACHE_PAGE_SIZE;
  char *guard_end = base + HALO_TEXTURE_CACHE_STEAL_GUARD_SIZE + stolen_size;

  if (remaining_page_count <= 0) {
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

  lruv_resize(*(void **)0x4ea980, remaining_page_count);
  physical_memory_protect(base + HALO_TEXTURE_CACHE_STEAL_GUARD_SIZE,
                          stolen_size, 4);
  physical_memory_protect(base, HALO_TEXTURE_CACHE_STEAL_GUARD_SIZE, 2);
  physical_memory_protect(guard_end, HALO_TEXTURE_CACHE_STEAL_GUARD_SIZE, 2);
  *(int8_t *)0x4ea984 = 1;
  return base + HALO_TEXTURE_CACHE_STEAL_GUARD_SIZE;
}

void xbox_texture_cache_return_memory(void)
{
  if (*(int8_t *)0x4ea984 == 0) {
    display_assert("xbox_texture_cache_globals.stolen_memory",
                   "c:\\halo\\SOURCE\\cache\\xbox_texture_cache.c", 0x159,
                   true);
    system_exit(-1);
  }

  lruv_resize(*(void **)0x4ea980, HALO_TEXTURE_CACHE_PAGE_COUNT);
  physical_memory_protect(FUN_001bdd60(), HALO_TEXTURE_CACHE_SIZE, 0x404);
  *(int8_t *)0x4ea984 = 0;
}

/* bitmap_format_to_d3d_linear_format (0x1beba0)
 *
 * Look up the linear D3D texture format code for a bitmap format index.
 * Table at 0x2b9618 maps format indices 0..17 to D3D format codes.
 * If flags bit 0x20 is set and format is 10 or 11 (DXT4/DXT5), returns 0x33. */
int bitmap_format_to_d3d_linear_format(int16_t format, uint16_t flags)
{
  int table_base;

  if (format < 0 || format >= 0x12) {
    display_assert("format>=0 && format<NUMBER_OF_BITMAP_FORMATS",
                   "c:\\halo\\SOURCE\\cache\\xbox_texture_cache.c", 0x1e1, 1);
    system_exit(-1);
  }

  if (((int *)0x2b9618)[format] == -1) {
    display_assert("table[format]!=NONE",
                   "c:\\halo\\SOURCE\\cache\\xbox_texture_cache.c", 0x1e2, 1);
    system_exit(-1);
  }

  table_base = 0x2b9618;
  if ((flags & 0x20) && (format == 10 || format == 11))
    return 0x33;

  return ((int *)table_base)[format];
}

/* FUN_001bec30 (0x1bec30)
 *
 * Look up the swizzled D3D texture format code for a bitmap format index.
 * Table at 0x2b9660 maps format indices 0..17 to D3D format codes.
 * If flags bit 0x20 is set and format is 10 or 11 (DXT4/DXT5), returns 0x36. */
int FUN_001bec30(int16_t format, uint16_t flags)
{
  int table_base;

  if (format < 0 || format >= 0x12) {
    display_assert("format>=0 && format<NUMBER_OF_BITMAP_FORMATS",
                   "c:\\halo\\SOURCE\\cache\\xbox_texture_cache.c", 0x206, 1);
    system_exit(-1);
  }

  if (((int *)0x2b9660)[format] == -1) {
    display_assert("table[format]!=NONE",
                   "c:\\halo\\SOURCE\\cache\\xbox_texture_cache.c", 0x207, 1);
    system_exit(-1);
  }

  table_base = 0x2b9660;
  if ((flags & 0x20) && (format == 10 || format == 11))
    return 0x36;

  return ((int *)table_base)[format];
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
 * texture is passed in EDI (D3D texture header, 5 dwords / 20 bytes).
 *
 * The second read of bitmap+0xa is deliberately a fresh memory load rather
 * than a reuse of t10: the original re-reads it (cmp word ptr [esi+0xa],2), and
 * ending t10's live range at the first test is what frees the register.
 *
 * VC71 match ceiling ~80%, two causes, neither source-controllable:
 *   1. Both params are @<reg>, so the original has NO EBP frame at all
 *      (push ebx ... pop ebx; ret). The VC71 verify lane cannot pass args in
 *      esi/edi, so our build emits a full frame plus [ebp+N] param reads, and
 *      the extra pressure forces `texture` to be reloaded from [ebp+0xc] in
 *      the else branch where the original just keeps it in EDI.
 *   2. VC71 distributes shifts over the pack chains: ((p-1)<<12 | (h-1))<<12
 *      becomes (p<<24 - 0x1000000) | (h<<12 - 0x1000) (the IMM-WARN), and the
 *      else branch's two `desc <<= 4` steps merge into one `shl 8`. Both are
 *      valid unconditionally and cost the same instruction count, so it is a
 *      canonicalization coin flip driven by scheduling, not by source form:
 *      the sequential chain, the flat absolute-shift form (<<24/<<12/<<0), and
 *      pre-decremented h/w locals all compile to identical distributed code. */
void xbox_texture_cache_setup_d3d_texture(void *bitmap /* @<esi> */,
                                          void *texture /* @<edi> */)
{
  assert_halt(bitmap);
  assert_halt(texture);

  ((int *)texture)[1] = 0;
  ((int *)texture)[2] = 0;
  ((int *)texture)[0] = 0x40001;

  if (*(uint16_t *)((char *)bitmap + 0xe) & 0x10) {
    uint32_t format_bits;
    int height, width, pitch;

    format_bits = FUN_001bec30(*(int16_t *)((char *)bitmap + 0xc),
                               *(uint16_t *)((char *)bitmap + 0xe));
    ((int *)texture)[3] = (format_bits << 8) | 0x10029;

    pitch = bitmap_mipmap_get_row_pitch(bitmap, 0);
    height = *(int16_t *)((char *)bitmap + 0x6);
    width = *(int16_t *)((char *)bitmap + 0x4);

    pitch = (pitch / 64) - 1;
    pitch = (pitch << 12) | (height - 1);
    pitch = (pitch << 12) | (width - 1);
    ((int *)texture)[4] = pitch;
  } else {
    int16_t h, w, fmt, t10;
    uint16_t flg;
    uint32_t desc;

    desc = FUN_00108db0(*(int16_t *)((char *)bitmap + 0x8));
    h = *(int16_t *)((char *)bitmap + 0x6);
    desc <<= 4;
    desc |= FUN_00108db0(h);
    w = *(int16_t *)((char *)bitmap + 0x4);
    desc <<= 4;
    desc |= FUN_00108db0(w);
    flg = *(uint16_t *)((char *)bitmap + 0xe);
    fmt = *(int16_t *)((char *)bitmap + 0xc);
    desc <<= 12;
    desc |= bitmap_format_to_d3d_linear_format(fmt, flg);
    t10 = *(int16_t *)((char *)bitmap + 0xa);
    desc <<= 4;
    desc |= (3 - (t10 != 1));
    desc <<= 4;
    desc |= ((FUN_00183120(bitmap) + 1) << 16);

    ((int *)texture)[4] = 0;
    ((int *)texture)[3] =
      desc | (((*(int16_t *)((char *)bitmap + 0xa) != 2) - 1) & 4) | 9;
  }

  D3DResource_Register(texture, *(void **)((char *)bitmap + 0x2c));
}

bool xbox_texture_cache_request(void *hardware_format, bool block)
{
  int32_t min_block = *(int32_t *)((char *)hardware_format + 0x1c);
  int cache_block_index = FUN_00183290(hardware_format);

  if (cache_block_index <= min_block) {
    cache_block_index = min_block;
  }

  cache_block_index = FUN_0011de10(*(void **)0x4ea980, cache_block_index);
  if (cache_block_index != -1) {
    int cache_page_index =
      lruv_block_get_address(*(void **)0x4ea980, cache_block_index) +
      *(int32_t *)0x4ea97c;
    int new_texture_index =
      data_new_datum(*(void **)0x4ea978, cache_block_index);
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
      cache_file_read(*(int32_t *)((char *)hardware_format + 0x20),
                      *(int32_t *)((char *)hardware_format + 0x18), min_block,
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

  if (*(char *)((char *)hardware_format + 0xe) < 0) {
    if (*(int32_t *)((char *)hardware_format + 0x24) == -1 && load) {
      xbox_texture_cache_request(hardware_format, block);
    }
    if (*(int32_t *)((char *)hardware_format + 0x24) != -1) {
      void *entry = datum_get(*(void **)0x4ea978,
                              *(int32_t *)((char *)hardware_format + 0x24));
      lruv_debug_to_file(*(void **)0x4ea980,
                         *(int32_t *)((char *)hardware_format + 0x24));
      if (block) {
        if (*(char *)((char *)entry + 4) != 0)
          goto loaded;
        if (*(uint8_t *)0x4ea98a) {
          const char *name =
            tag_get_name(*(int32_t *)((char *)hardware_format + 0x20));
          console_warning("%s", name);
        }
        cache_files_io_request_enable(*(int16_t *)((char *)entry + 2));
      }
      do {
        if (*(char *)((char *)entry + 4) != 0) {
        loaded:
          if (*(char *)((char *)entry + 5) == 0) {
            *(char *)((char *)entry + 5) = 1;
          }
          result = (char *)entry + 0xc;
          if (result)
            break;
        } else {
          unsigned int t0 = sound_render_time();
          if (system_milliseconds() - t0 > 0x84u) {
            sound_idle();
          }
          SwitchToThread();
        }
      } while (block);
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
    result = rasterizer_get_default_hardware_format(hardware_format);
    if (!result) {
      display_assert("hardware_format",
                     "c:\\halo\\SOURCE\\cache\\xbox_texture_cache.c", 0x127,
                     true);
      system_exit(-1);
    }
  }

  return result;
}
