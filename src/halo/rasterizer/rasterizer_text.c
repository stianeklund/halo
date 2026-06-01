/* rasterizer_memory_pool.c */

/* rasterizer_memory_pool_new: allocate global rasterizer memory pool (0x1824e0)
 */
int rasterizer_memory_pool_new(void)
{
  void *pool;
  char result;
  result = 1;
  pool = (void *)debug_malloc(
    0x18000, 0, "c:\\halo\\SOURCE\\rasterizer\\rasterizer_memory_pool.c", 0x13);
  *(void **)0x4d0488 = pool;
  if (pool == 0) {
    error(2, "### ERROR rasterizer failed to allocate global memory pool");
    return 0;
  }
  return result;
}

/* rasterizer_memory_pool_reset: reset pool allocation cursor to zero (0x182520)
 */
void rasterizer_memory_pool_reset(void)
{
  *(int *)0x4d048c = 0;
}

/* rasterizer_memory_pool_alloc: allocate from memory pool, optionally copying
 * data (0x182530) */
int rasterizer_memory_pool_alloc(int data, int size)
{
  unsigned int new_offset;
  int result;

  new_offset = *(unsigned int *)0x4d048c + size;
  result = 0;
  if (new_offset < 0x18001) {
    result = *(int *)0x4d0488 + *(int *)0x4d048c;
    *(unsigned int *)0x4d048c = new_offset;
    if (data != 0) {
      csmemcpy((void *)result, (void *)data, size);
      return result;
    }
  } else {
    error(2, "### ERROR rasterizer memory pool exceeded");
  }
  return result;
}

/* rasterizer_memory_pool_copy: assert data non-null then copy into pool
 * (0x182590) */
void rasterizer_memory_pool_copy(int data, int size)
{
  if (data == 0) {
    display_assert("data",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_memory_pool.c",
                   0x42, 1);
    system_exit(-1);
  }
  rasterizer_memory_pool_alloc(data, size);
}

/* rasterizer_memory_pool_delete: free the global rasterizer memory pool
 * (0x1825e0) */
void rasterizer_memory_pool_delete(void)
{
  if (*(void **)0x4d0488 != 0) {
    debug_free(*(void **)0x4d0488,
               "c:\\halo\\SOURCE\\rasterizer\\rasterizer_memory_pool.c", 0x50);
  }
  *(void **)0x4d0488 = 0;
  *(int *)0x4d048c = 0;
}

/* rasterizer_text_cache_initialize: init hardware text cache (0x183650) */
int rasterizer_text_cache_initialize(void)
{
  int texture_handle;
  char success;

  if (*(char *)0x4d04a0 != 0) {
    display_assert("!hardware_character_cache.initialized",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_text.c", 0x76, 1);
    system_exit(-1);
  }

  texture_handle = (int)bitmap_2d_new(128, 128, 0, 9);
  if (texture_handle != 0) {
    csmemset((void *)0x4d04a0, 0, 0x810);
    success = FUN_00168370((void *)texture_handle);
    if (success != 0) {
      *(int *)0x4d04ac = texture_handle;
      *(char *)0x4d04a0 = 1;
      return 1;
    }
  }

  error(2, "### ERROR failed to initialize hardware text cache");
  return 0;
}

/* rasterizer_text_set_shadow_color: set text shadow color (0x1836e0) */
void rasterizer_text_set_shadow_color(const void *color)
{
  *(int *)0x4d0cb0 = (int)color;
}

/* rasterizer_text_cache_flush: invalidate all cached characters (0x1836f0) */
void rasterizer_text_cache_flush(void)
{
  int *slot;
  int i;

  if (*(char *)0x4d04a0 != 0) {
    slot = (int *)0x4d04b0;
    for (i = 0; i < 256; i++) {
      if (*slot != 0) {
        *(short *)(*slot + 0xc) = -1;
      }
      *slot = 0;
      slot += 2;
    }
  }
}

/* FUN_00183720: dispose hardware character cache (0x183720) */
void rasterizer_text_cache_dispose(void)
{
  if (*(char *)0x4d04a0 != 0) {
    rasterizer_text_cache_flush();
    bitmap_delete(*(void **)0x4d04ac);
    *(char *)0x4d04a0 = 0;
  }
}

/* rasterizer_text.c — hardware character cache and text rendering.
 *
 * Address range: 0x183650 - 0x184060
 */

#define HARDWARE_CHARACTER_CACHE_BITMAP_WIDTH 128
#define HARDWARE_CHARACTER_CACHE_BITMAP_HEIGHT 128
#define MAXIMUM_HARDWARE_CHARACTERS 256

/* Hardware character cache (0x4d04a0, 0x810 bytes):
 *   +0x00 (byte):   initialized
 *   +0x02 (ushort): read_index   (wraps at 256)
 *   +0x04 (ushort): write_index  (wraps at 256)
 *   +0x06 (short):  cursor_x
 *   +0x08 (short):  cursor_y
 *   +0x0a (short):  max_char_height
 *   +0x0c (int):    texture_handle
 *   +0x10-0x810:    character_table[256] entries (8 bytes each):
 *       +0x0 (int*):   character pointer
 *       +0x4 (short):  screen_x
 *       +0x6 (short):  screen_y
 */

/* FUN_00183770: get hardware character screen position.
 * Original ABI: AX=index, EBX=*out_y, stack=*out_x
 */
void rasterizer_text_get_character_position(short index, short *out_y,
                                            short *out_x)
{
  if (*(char *)0x4d04a0 == 0) {
    display_assert("hardware_character_cache.initialized",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_text.c", 0x255, 1);
    system_exit(-1);
  }
  if (index < 0 || index >= 256) {
    display_assert("hardware_character_index>=0 && "
                   "hardware_character_index<MAXIMUM_HARDWARE_CHARACTERS",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_text.c", 0x256, 1);
    system_exit(-1);
  }
  if (out_x == (short *)0 || out_y == (short *)0) {
    display_assert("x0 && y0",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_text.c", 599, 1);
    system_exit(-1);
  }
  *out_x = *(short *)(0x4d04b4 + index * 8);
  *out_y = *(short *)(0x4d04b6 + index * 8);
}

/* FUN_00183820: evict a hardware character from the cache.
 * Original ABI: ESI=slot (pointer to character pointer in cache)
 */
void rasterizer_text_evict_character(int **slot)
{
  int *character;

  if (slot == (int **)0) {
    display_assert("hardware_character",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_text.c", 0x262, 1);
    system_exit(-1);
  }

  character = *slot;
  if (character != (int *)0) {
    *(short *)((char *)character + 0xc) = -1;
    if (*(short *)((char *)character + 0xe) == *(short *)0x325748) {
      error(3, "font cache overwrote character in use");
    }
    *slot = (int *)0;
  }
}

/* FUN_00183880: cache a hardware character into the texture cache.
 * Original ABI: EDI=character pointer, stack=font pointer
 */
void rasterizer_text_cache_character(void *font_character, void *font)
{
  int character = (int)font_character;
  short char_width;
  short char_height;
  int **character_slot;
  short hw_index;
  short y;
  short x;
  short *pixel_out;
  int font_pixels;
  int i;
  int cache_top;
  int cache_bottom;
  unsigned short read_index;
  unsigned short write_index;

  if (*(char *)0x4d04a0 == 0) {
    display_assert("hardware_character_cache.initialized",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_text.c", 0x279, 1);
    system_exit(-1);
  }

  hw_index = *(short *)(character + 0xc);

  if (hw_index == -1) {
    char_width = *(short *)(character + 4);
    char_height = *(short *)(character + 6);

    if (char_width > 128) {
      display_assert(
        "font_character->bitmap_width<=HARDWARE_CHARACTER_CACHE_BITMAP_WIDTH",
        "c:\\halo\\SOURCE\\rasterizer\\rasterizer_text.c", 0x285, 1);
      system_exit(-1);
    }
    if (char_height > 128) {
      display_assert(
        "font_character->bitmap_height<=HARDWARE_CHARACTER_CACHE_BITMAP_HEIGHT",
        "c:\\halo\\SOURCE\\rasterizer\\rasterizer_text.c", 0x286, 1);
      system_exit(-1);
    }

    *(short *)(character + 0xe) = *(short *)0x325748;

    /* Advance to next row if needed */
    if (128 < (int)*(short *)0x4d04a6 + (int)char_width) {
      *(short *)0x4d04a8 += *(short *)0x4d04aa;
      *(short *)0x4d04a6 = 0;
    }

    /* Wrap back to top if needed, evicting characters */
    if (128 < (int)*(short *)0x4d04a8 + (int)char_height) {
      *(short *)0x4d04a6 = 0;
      *(short *)0x4d04a8 = 0;

      read_index = *(unsigned short *)0x4d04a2;
      write_index = *(unsigned short *)0x4d04a4;

      if (read_index != write_index) {
        i = read_index & 0xFF;
        while (i != (write_index & 0xFF)) {
          if (*(short *)(0x4d04b6 + i * 8) <= 0) {
            break;
          }
          rasterizer_text_evict_character((int **)(0x4d04b0 + i * 8));
          i = (i + 1) & 0xFF;
        }
        *(unsigned short *)0x4d04a2 = (unsigned short)i;
      }
    }

    /* Evict characters that overlap */
    if (*(short *)0x4d04aa < char_height) {
      cache_top = *(short *)0x4d04a8 + *(short *)0x4d04aa;
      cache_bottom = char_height + (int)*(short *)0x4d04a8;

      read_index = *(unsigned short *)0x4d04a2;
      write_index = *(unsigned short *)0x4d04a4;

      if (read_index != write_index) {
        i = read_index & 0xFF;
        while (i != (write_index & 0xFF)) {
          if ((*(short *)(0x4d04b6 + i * 8) >= (short)cache_top) &&
              (*(short *)(0x4d04b6 + i * 8) <= (short)cache_bottom)) {
            rasterizer_text_evict_character((int **)(0x4d04b0 + i * 8));
          }
          i = (i + 1) & 0xFF;
        }
        *(unsigned short *)0x4d04a2 = (unsigned short)i;
      }
      *(short *)0x4d04a8 += char_height;
    }

    /* Handle full cache: evict oldest character */
    if ((*(unsigned char *)0x4d04a4 + 1u) == *(unsigned char *)0x4d04a2) {
      character_slot = (int **)(0x4d04b0 + *(short *)0x4d04a2 * 8);
      rasterizer_text_evict_character(character_slot);
      *(unsigned short *)0x4d04a2 =
        (unsigned short)(*(unsigned char *)0x4d04a2 + 1);
    }

    /* Allocate slot and copy bitmap to texture */
    i = *(short *)0x4d04a4;
    *(short *)(character + 0xc) = (short)i;
    *(int *)(0x4d04b0 + i * 8) = character;
    *(short *)(0x4d04b4 + i * 8) = *(short *)0x4d04a6;
    *(short *)(0x4d04b6 + i * 8) = *(short *)0x4d04a8;

    font_pixels =
      *(int *)(*(int *)((int)font + 0x94) + *(int *)(character + 0x10));

    for (y = 0; y < char_height; y++) {
      pixel_out = (short *)bitmap_2d_address(
        *(void **)0x4d04ac, *(short *)(0x4d04b4 + i * 8),
        *(short *)(0x4d04b6 + i * 8) + y, 0);
      for (x = 0; x < char_width; x++) {
        *pixel_out =
          (short)((*(unsigned char *)(font_pixels + x) << 8) | 0xfff);
        pixel_out++;
      }
    }

    FUN_00168b10(*(void **)0x4d04ac);

    *(short *)0x4d04a6 += char_width;
    *(unsigned short *)0x4d04a4 =
      (unsigned short)(*(unsigned char *)0x4d04a4 + 1);
  } else {
    if (hw_index < 0 || hw_index >= 256) {
      display_assert(
        "font_character->hardware_character_index>=0 && "
        "font_character->hardware_character_index<MAXIMUM_HARDWARE_CHARACTERS",
        "c:\\halo\\SOURCE\\rasterizer\\rasterizer_text.c", 0x27d, 1);
      system_exit(-1);
    }
    if (character != *(int *)(0x4d04b0 + hw_index * 8)) {
      display_assert("font_character==hardware_character_cache.characters[font_"
                     "character->hardware_character_index].character",
                     "c:\\halo\\SOURCE\\rasterizer\\rasterizer_text.c", 0x27e,
                     1);
      system_exit(-1);
    }
  }
}

/* FUN_00183c00: draw a single cached character quad. */
void rasterizer_text_draw_cached_char(void *arg0, void *font,
                                      void *font_character, unsigned int color,
                                      short x, short y, int screen_x,
                                      int screen_y, short width, short height)
{
  float quad_verts[28];
  short cache_x;
  short cache_y;

  rasterizer_text_cache_character(font_character, font);

  if (*(short *)((int)font_character + 0xc) != -1) {
    rasterizer_text_get_character_position(
      *(short *)((int)font_character + 0xc), &cache_y, &cache_x);

    quad_verts[0] = (float)x;
    quad_verts[1] = (float)y;
    quad_verts[2] = (float)cache_x;
    quad_verts[3] = (float)cache_y;
    quad_verts[4] = *(float *)&color;
    quad_verts[5] = 1.0f;
    quad_verts[6] = 1.0f;

    quad_verts[7] = (float)(x + width);
    quad_verts[8] = (float)y;
    quad_verts[9] = (float)(cache_x + width);
    quad_verts[10] = (float)cache_y;
    quad_verts[11] = *(float *)&color;
    quad_verts[12] = 1.0f;
    quad_verts[13] = 1.0f;

    quad_verts[14] = (float)x;
    quad_verts[15] = (float)(y + height);
    quad_verts[16] = (float)cache_x;
    quad_verts[17] = (float)(cache_y + height);
    quad_verts[18] = *(float *)&color;
    quad_verts[19] = 1.0f;
    quad_verts[20] = 1.0f;

    quad_verts[21] = (float)(x + width);
    quad_verts[22] = (float)(y + height);
    quad_verts[23] = (float)(cache_x + width);
    quad_verts[24] = (float)(cache_y + height);
    quad_verts[25] = *(float *)&color;
    quad_verts[26] = 1.0f;
    quad_verts[27] = 1.0f;

    FUN_001741d0(quad_verts);
  }
}

/* FUN_00183cf0: draw character string via hardware cache.
 * This is the callback used by the text drawing system.
 * Shadow pass draws with shadow_color, then second pass draws with actual
 * color.
 */
void rasterizer_text_draw_cached_chars(void *arg0, void *font,
                                       void *font_character, unsigned int color,
                                       short x, short y, int offset_x,
                                       int offset_y, short width, short height)
{
  float quad_verts[28];
  short cache_x;
  short cache_y;
  unsigned int draw_color;
  float x_pos;
  float y_pos;
  float shadow_x;
  float shadow_y;
  int has_shadow;
  int c;

  rasterizer_text_cache_character(font_character, font);

  if (*(short *)((int)font_character + 0xc) != -1) {
    x_pos = (float)(x + offset_x);
    y_pos = (float)(y + offset_y);
    shadow_x = 0.0f;
    shadow_y = 0.0f;
    has_shadow = 1;

    draw_color = *(unsigned int *)0x4d0cb0;
    if (*(unsigned int *)0x4d0cb0 == 0) {
      draw_color = color & 0xff000000;
    }

    /* First pass: shadow, second pass: actual color */
    c = 0;
    while (c < 2 && has_shadow != 0) {
      rasterizer_text_get_character_position(
        *(short *)((int)font_character + 0xc), &cache_y, &cache_x);

      if (has_shadow == 1) {
        /* first pass uses shadow color */
      } else {
        draw_color = color;
      }

      quad_verts[0] = x_pos + shadow_x;
      quad_verts[1] = y_pos + shadow_y;
      quad_verts[2] = (float)cache_x;
      quad_verts[3] = (float)cache_y;
      quad_verts[4] = *(float *)&draw_color;
      quad_verts[5] = 1.0f;
      quad_verts[6] = 1.0f;

      quad_verts[7] = (x_pos + (float)width) + shadow_x;
      quad_verts[8] = y_pos + shadow_y;
      quad_verts[9] = (float)(cache_x + width);
      quad_verts[10] = (float)cache_y;
      quad_verts[11] = *(float *)&draw_color;
      quad_verts[12] = 1.0f;
      quad_verts[13] = 1.0f;

      quad_verts[14] = x_pos + shadow_x;
      quad_verts[15] = (y_pos + (float)height) + shadow_y;
      quad_verts[16] = (float)cache_x;
      quad_verts[17] = (float)(cache_y + height);
      quad_verts[18] = *(float *)&draw_color;
      quad_verts[19] = 1.0f;
      quad_verts[20] = 1.0f;

      quad_verts[21] = (x_pos + (float)width) + shadow_x;
      quad_verts[22] = (y_pos + (float)height) + shadow_y;
      quad_verts[23] = (float)(cache_x + width);
      quad_verts[24] = (float)(cache_y + height);
      quad_verts[25] = *(float *)&draw_color;
      quad_verts[26] = 1.0f;
      quad_verts[27] = 1.0f;

      FUN_001741d0(quad_verts);

      has_shadow = 0;
      shadow_x = 0.0f;
      shadow_y = 0.0f;
      c++;
    }
  }
}

/* rasterizer_text_draw: draw ASCII string (0x183e60) */
void rasterizer_text_draw(void *screen_pos, short *bounds, const void *color,
                          int flags, const char *text)
{
  int draw_bounds[4];
  int clip_bounds[4];
  float texel_width;
  float texel_height;
  float widget_params[35];
  void *texture;
  int font_width;
  int font_height;
  int max_width;
  int max_height;
  int clamp_x;
  int clamp_y;

  if (*(char *)0x3256da == 0 || *(int *)0x5a5bc0 != 0) {
    return;
  }

  *(short *)0x325748 += 1;

  if (text == (const char *)0) {
    display_assert("string", "c:\\halo\\SOURCE\\rasterizer\\rasterizer_text.c",
                   0xb4, 1);
    system_exit(-1);
  }

  texture = (void *)(*(int *)0x4d04ac);
  if ((*(char *)0x4d04a0 != 0) && texture != (void *)0 && *text != 0) {
    csstrlen(text);

    if (screen_pos == (void *)0) {
      draw_bounds[0] = *(int *)0x506584;
      draw_bounds[1] = *(int *)0x506588;
      rect2d_offset((short *)draw_bounds, (short)(-*(short *)0x50657e),
                    (short)(-*(short *)0x50657c));
    } else {
      draw_bounds[0] = *(int *)screen_pos;
      draw_bounds[1] = *(int *)((char *)screen_pos + 4);
    }

    if (bounds == (short *)0) {
      clip_bounds[0] = *(int *)0x50657c;
      clip_bounds[1] = *(int *)0x506580;
      rect2d_offset((short *)clip_bounds, (short)(-*(short *)0x50657e),
                    (short)(-*(short *)0x50657c));
    } else {
      max_width = (int)bounds[2];
      if ((int)(*(short *)0x506580 - *(short *)0x50657c) <= (int)bounds[2]) {
        max_width = (int)(*(short *)0x506580 - *(short *)0x50657c);
      }
      max_height = (int)(*(short *)0x506582 - *(short *)0x50657e);
      if ((int)bounds[3] < max_height) {
        max_height = (int)bounds[3];
      }
      clamp_x = (int)bounds[0];
      if (bounds[0] < 0) {
        clamp_x = 0;
      }
      clamp_y = (int)bounds[1];
      if (bounds[1] < 0) {
        clamp_y = 0;
      }
      FUN_001089a0(clip_bounds, clamp_y, clamp_x, max_height, max_width);
      texture = (void *)(*(int *)0x4d04ac);
    }

    csmemset(widget_params, 0, 0x8c);
    font_width = (int)*(short *)((int)texture + 4);
    font_height = (int)*(short *)((int)texture + 6);
    texel_width = *(float *)0x2533c8 / (float)font_width;
    texel_height = *(float *)0x2533c8 / (float)font_height;

    widget_params[0] = 0;
    widget_params[1] = *(int *)&texel_width;
    widget_params[2] = *(int *)&texel_height;
    widget_params[3] = 0x3f800000;
    widget_params[4] = 0x3f800000;
    widget_params[5] = 0;
    widget_params[6] = 0;
    widget_params[7] = (int)texture;

    FUN_00173b40(widget_params);
    FUN_0019c5d0(rasterizer_text_draw_cached_chars, draw_bounds, color,
                 clip_bounds, flags, (char *)text);
    FUN_00173ae0();
  }
}

/* rasterizer_draw_string: draw wide-character string (0x184060) */
void rasterizer_draw_string(void *screen_pos, short *bounds, const void *color,
                            int flags, unsigned short *text)
{
  int draw_bounds[4];
  int clip_bounds[4];
  float texel_width;
  float texel_height;
  float widget_params[35];
  void *texture;
  int font_width;
  int font_height;
  int max_width;
  int max_height;
  int clamp_x;
  int clamp_y;

  if (*(char *)0x3256da == 0 || *(int *)0x5a5bc0 != 0) {
    return;
  }

  *(short *)0x325748 += 1;

  if (text == (unsigned short *)0) {
    display_assert("string", "c:\\halo\\SOURCE\\rasterizer\\rasterizer_text.c",
                   0x136, 1);
    system_exit(-1);
  }

  texture = (void *)(*(int *)0x4d04ac);
  if ((*(char *)0x4d04a0 != 0) && texture != (void *)0 && *text != 0) {
    ustrlen(text);

    if (screen_pos == (void *)0) {
      draw_bounds[0] = *(int *)0x506584;
      draw_bounds[1] = *(int *)0x506588;
      rect2d_offset((short *)draw_bounds, (short)(-*(short *)0x50657e),
                    (short)(-*(short *)0x50657c));
    } else {
      draw_bounds[0] = *(int *)screen_pos;
      draw_bounds[1] = *(int *)((char *)screen_pos + 4);
    }

    if (bounds == (short *)0) {
      clip_bounds[0] = *(int *)0x50657c;
      clip_bounds[1] = *(int *)0x506580;
      rect2d_offset((short *)clip_bounds, (short)(-*(short *)0x50657e),
                    (short)(-*(short *)0x50657c));
    } else {
      max_width = (int)bounds[2];
      if ((int)(*(short *)0x506580 - *(short *)0x50657c) <= (int)bounds[2]) {
        max_width = (int)(*(short *)0x506580 - *(short *)0x50657c);
      }
      max_height = (int)(*(short *)0x506582 - *(short *)0x50657e);
      if ((int)bounds[3] < max_height) {
        max_height = (int)bounds[3];
      }
      clamp_x = (int)bounds[0];
      if (bounds[0] < 0) {
        clamp_x = 0;
      }
      clamp_y = (int)bounds[1];
      if (bounds[1] < 0) {
        clamp_y = 0;
      }
      FUN_001089a0(clip_bounds, clamp_y, clamp_x, max_height, max_width);
      texture = (void *)(*(int *)0x4d04ac);
    }

    csmemset(widget_params, 0, 0x8c);
    font_width = (int)*(short *)((int)texture + 4);
    font_height = (int)*(short *)((int)texture + 6);
    texel_width = *(float *)0x2533c8 / (float)font_width;
    texel_height = *(float *)0x2533c8 / (float)font_height;

    widget_params[0] = 0;
    widget_params[1] = *(int *)&texel_width;
    widget_params[2] = *(int *)&texel_height;
    widget_params[3] = 0x3f800000;
    widget_params[4] = 0x3f800000;
    widget_params[5] = 0;
    widget_params[6] = 0;
    widget_params[7] = (int)texture;

    FUN_00173b40(widget_params);
    FUN_0019c960(rasterizer_text_draw_cached_chars, draw_bounds, color,
                 clip_bounds, flags, text);
    FUN_00173ae0();
  }
}

/* rasterizer_transparent_geometry.c */

/* rasterizer_transparent_geometry_new: allocate transparent geometry buffers
 * and init vertex cache (0x184260) */
int rasterizer_transparent_geometry_new(void)
{
  int success;

  *(void **)0x4d0cec = debug_malloc(
    0xf000, 0,
    "c:\\halo\\SOURCE\\rasterizer\\rasterizer_transparent_geometry.c", 0x29);
  *(void **)0x4d0cfc = debug_malloc(
    0x300, 0, "c:\\halo\\SOURCE\\rasterizer\\rasterizer_transparent_geometry.c",
    0x2b);
  *(void **)0x4d0cf0 = debug_malloc(
    0x1400, 0,
    "c:\\halo\\SOURCE\\rasterizer\\rasterizer_transparent_geometry.c", 0x2e);
  *(int *)0x4d0cf8 = 0;
  *(int *)0x4d0cf4 = 0;
  if (*(int *)0x4d0cec == 0 || *(int *)0x4d0cfc == 0 || *(int *)0x4d0cf0 == 0) {
    error(2, "### ERROR failed to allocate transparent geometry buffer");
  } else {
    success = FUN_00174bd0();
    if (success != 0) {
      return 1;
    }
  }
  return 0;
}

/* rasterizer_transparent_geometry_begin: reset group counts and stats for new
 * frame (0x184300) */
void rasterizer_transparent_geometry_begin(void)
{
  *(int *)0x4d0cf4 = 0;
  *(short *)0x4d0d00 = 0;
  csmemset((void *)0x4d0cbc, 0, 0x30);
  *(int *)0x4d0cf8 = 0;
}

/* rasterizer_transparent_geometry_group_new: allocate next transparent geometry
 * group slot (0x184330) */
void *rasterizer_transparent_geometry_group_new(void)
{
  void *group;

  group = (void *)0;
  if (*(int *)0x4d0cf4 < 0x180) {
    group = (void *)(*(int *)0x4d0cf4 * 0xa0 + *(int *)0x4d0cec);
    *(int *)((char *)group + 0x90) = *(int *)0x4d0cf4;
    *(int *)0x4d0cf4 = *(int *)0x4d0cf4 + 1;
  }
  return group;
}

/* rasterizer_secondary_geometry_group_new: allocate next secondary geometry
 * group slot (0x184360) */
void *rasterizer_secondary_geometry_group_new(void)
{
  void *group;

  group = (void *)0;
  if (*(int *)0x4d0cf8 < 0x20) {
    group = (void *)(*(int *)0x4d0cf8 * 0xa0 + *(int *)0x4d0cf0);
    *(int *)((char *)group + 0x90) = *(int *)0x4d0cf8;
    *(int *)0x4d0cf8 = *(int *)0x4d0cf8 + 1;
  }
  return group;
}

/* rasterizer_secondary_geometry_groups_get: return secondary groups buffer;
 * optionally write count (0x184390) */
void *rasterizer_secondary_geometry_groups_get(short *out_count)
{
  if (out_count != (short *)0) {
    *out_count = (short)*(int *)0x4d0cf8;
  }
  return *(void **)0x4d0cf0;
}

/* rasterizer_transparent_geometry_next_group: return next sorted group after
 * given group (0x1843b0) */
void *rasterizer_transparent_geometry_next_group(void *group)
{
  short next_index;
  short sorted_index;

  if (group != (void *)0) {
    sorted_index = *(short *)((char *)group + 0x90);
    next_index = (short)(sorted_index + 1);
    if (*(int *)((char *)group + 0x90) < 0 ||
        *(int *)0x4d0cf4 <= *(int *)((char *)group + 0x90)) {
      display_assert(
        "group->sorted_index>=0 && "
        "group->sorted_index<transparent_geometry_group_count",
        "c:\\halo\\SOURCE\\rasterizer\\rasterizer_transparent_geometry.c", 0x89,
        1);
      system_exit(-1);
    }
    if (next_index < *(int *)0x4d0cf4) {
      if (next_index < 0) {
        display_assert(
          "next_group_sorted_index>=0",
          "c:\\halo\\SOURCE\\rasterizer\\rasterizer_transparent_geometry.c",
          0x8d, 1);
        system_exit(-1);
      }
      return (void *)(*(short *)(*(int *)0x4d0cfc + next_index * 2) * 0xa0 +
                      *(int *)0x4d0cec);
    }
  }
  return (void *)0;
}

/* rasterizer_transparent_geometry_group_get: return group by presorted index
 * (0x184460) */
void *rasterizer_transparent_geometry_group_get(short group_presorted_index)
{
  if (group_presorted_index < 0 || *(int *)0x4d0cf4 <= group_presorted_index) {
    display_assert(
      "group_presorted_index>=0 && "
      "group_presorted_index<transparent_geometry_group_count",
      "c:\\halo\\SOURCE\\rasterizer\\rasterizer_transparent_geometry.c", 0xbc,
      1);
    system_exit(-1);
  }
  return (void *)(group_presorted_index * 0xa0 + *(int *)0x4d0cec);
}

/* rasterizer_transparent_geometry_group_to_presorted_index: convert group
 * pointer to presorted index (0x1844b0) */
short rasterizer_transparent_geometry_group_to_presorted_index(
  unsigned int group)
{
  unsigned int base;
  int count;
  short index;
  unsigned int offset;
  unsigned int remainder;

  base = *(unsigned int *)0x4d0cec;
  count = *(int *)0x4d0cf4;
  index = -1;
  if (group >= base && group < base + (unsigned int)(count * 0xa0)) {
    index = (short)((int)(group - base) / 0xa0);
    if (index < 0 || count <= index) {
      display_assert(
        "group_presorted_index>=0 && "
        "group_presorted_index<transparent_geometry_group_count",
        "c:\\halo\\SOURCE\\rasterizer\\rasterizer_transparent_geometry.c", 0xcb,
        1);
      system_exit(-1);
    }
    offset = group - base;
    remainder = offset % 0xa0;
    if (remainder != 0) {
      display_assert(
        "((unsigned long)group-(unsigned "
        "long)transparent_geometry_groups)%sizeof(struct "
        "transparent_geometry_group)==0",
        "c:\\halo\\SOURCE\\rasterizer\\rasterizer_transparent_geometry.c", 0xcc,
        1);
      system_exit(-1);
    }
  }
  return index;
}
