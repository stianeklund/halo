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
  unsigned char *flags_ptr;
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
      unsigned int limit = max_mipmaps - 1;
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

  flags_ptr = (unsigned char *)(group + 0x6);
  if ((*flags_ptr & 0x8) != 0) {
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
    format_str =
      (char *)bitmap_format_get_string(*(short *)(bitmap_data + 0xc));
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

/*
 * FUN_00075630 -- 3D texture group extraction.
 *
 * Iterates over the pending bitmap array (base at DAT_00334134, count in
 * DAT_00334138). Groups consecutive entries with matching mip_count. For each
 * power-of-two group, creates a 3D bitmap, copies the slices into it via
 * bitmap_cube_map_face_extract, registers it with FUN_00075380, then frees
 * it. Logs warnings for incompatible-dimension or non-power-of-two groups.
 *
 * Returns 1 on success, 0 if a temporary bitmap allocation failed.
 */
char FUN_00075630(void)
{
  short mip_count;
  short width;
  short height;
  int outer;
  int slice_idx;
  char bvar4;
  char success;
  void *new_bitmap;
  short handle;
  void *tag_element;
  char *base;
  int i;

  success = 1;
  outer = 0;
  do {
    if ((short)outer >= *(short *)0x334138)
      break;
    base = *(char **)0x334134;
    mip_count = *(short *)(base + (short)outer * 0x10 + 4);
    width = *(short *)(*(char **)(base + (short)outer * 0x10) + 4);
    height = *(short *)(*(char **)(base + (short)outer * 0x10) + 6);
    slice_idx = 0;
    bvar4 = 0;

    while (*(short *)(base + ((short)outer + (short)slice_idx) * 0x10 + 4) ==
           mip_count) {
      if (*(short *)(*(char **)(base +
                                ((short)outer + (short)slice_idx) * 0x10) +
                     4) != width ||
          *(short *)(*(char **)(base +
                                ((short)outer + (short)slice_idx) * 0x10) +
                     6) != height) {
        bvar4 = 1;
      }
      slice_idx++;
      if (bvar4) {
        crt_fprintf((void *)0x331050,
                    "skipping 3D texture with incompatible slices\r\n");
        crt_fflush((void *)0x331050);
        goto next_group;
      }
    }

    if (slice_idx & (slice_idx - 1)) {
      crt_fprintf((void *)0x331050,
                  "skipping 3D texture with non power-of-two slice count\r\n");
      crt_fflush((void *)0x331050);
      goto next_group;
    }

    new_bitmap = bitmap_3d_new((unsigned short)width, (unsigned short)height,
                               (unsigned short)slice_idx, 0, 0xb);
    if (!new_bitmap || *(int *)((char *)new_bitmap + 0x2c) == 0) {
      error(2, "### ERROR extract: failed to allocate temporary bitmap");
      success = 0;
    } else {
      for (i = 0; (short)i < (short)slice_idx; i++) {
        bitmap_cube_map_face_extract(
          *(void **)(*(char **)0x334134 + ((short)outer + i) * 0x10),
          new_bitmap, 0, i);
      }
      *(short *)0x33415c = mip_count;
      handle = FUN_00075380(new_bitmap);
      if (handle != (short)-1) {
        tag_element = tag_block_get_element(*(char **)0x33414c + 0x54,
                                            (int)mip_count, 0x40);
        if (*(short *)((char *)tag_element + 0x20) == (short)-1) {
          *(short *)((char *)tag_element + 0x20) = handle;
          *(short *)((char *)tag_element + 0x22) = 1;
        } else {
          *(short *)((char *)tag_element + 0x22) += 1;
        }
      }
    }
    bitmap_delete(new_bitmap);

  next_group:
    outer += slice_idx;
  } while (success);
  return success;
}

/*
 * FUN_00075800 -- cube map group extraction.
 *
 * Walks the same pending bitmap array as FUN_00075630 (base at
 * *(char**)0x334134, count at *(short*)0x334138) accumulating six square,
 * same-size faces from a single sequence into one temporary cube map created
 * by bitmap_cube_map_new. On the sixth face the cube map is registered with
 * FUN_00075380 and recorded in the group's sequence block; any mismatch
 * (non-square face, incompatible size, sequence boundary) logs a warning and
 * abandons the partial cube map. Every source bitmap is deleted as it is
 * consumed.
 *
 * Returns 1 on success, 0 if a temporary bitmap allocation failed.
 */
char FUN_00075800(void)
{
  void *cube_map;
  short face_count;
  char success;
  volatile char skip_group;
  short i;
  short sequence_index;
  void *tag_element;
  char *entry;

  cube_map = 0;
  face_count = 0;
  success = 1;
  i = 0;
  sequence_index = 0;
  do {
    if (i >= *(short *)0x334138)
      break;
    entry = *(char **)0x334134 + i * 0x10;
    skip_group = 0;

    if (face_count == 0) {
      if (cube_map) {
        display_assert("!temporary_bitmap",
                       "c:\\halo\\SOURCE\\bitmaps\\bitmap_extract.c", 0x798, 1);
        system_exit(-1);
      }
      if (*(unsigned short *)(*(char **)entry + 4) ==
          *(unsigned short *)(*(char **)entry + 6)) {
        cube_map =
          bitmap_cube_map_new(*(unsigned short *)(*(char **)entry + 4), 0, 0xb);
        sequence_index = *(short *)(entry + 4);
      } else {
        crt_fprintf((void *)0x331050,
                    "skipping cube map with non-square faces\r\n");
        crt_fflush((void *)0x331050);
        skip_group = 1;
      }
    }

    if (!cube_map || *(int *)((char *)cube_map + 0x2c) == 0) {
      if (!skip_group) {
        error(2, "### ERROR extract: failed to create temporary bitmap");
        success = 0;
        goto next_entry;
      }
    } else {
      if (*(short *)(entry + 4) == sequence_index) {
        if (*(short *)(*(char **)entry + 4) ==
              *(short *)((char *)cube_map + 4) &&
            *(short *)(*(char **)entry + 6) ==
              *(short *)((char *)cube_map + 6)) {
          bitmap_cube_map_face_insert(*(void **)entry, cube_map, 0, face_count);
          face_count++;
          if (!skip_group)
            goto check_complete;
        } else {
          crt_fprintf((void *)0x331050,
                      "skipping cube map with incompatible-size faces\r\n");
          crt_fflush((void *)0x331050);
        }
      } else {
        crt_fprintf((void *)0x331050,
                    "skipping cube map which spanned sequence\r\n");
        crt_fflush((void *)0x331050);
      }
      bitmap_delete(cube_map);
      face_count = 0;
    }

  check_complete:
    if (success && face_count == 6) {
      /* The original reuses the face-count register (EBX) to hold the new
       * bitmap handle here; it is reset to 0 before leaving this block. */
      face_count = FUN_00075380(cube_map);
      if (face_count != (short)-1) {
        tag_element = tag_block_get_element(*(char **)0x33414c + 0x54,
                                            (int)sequence_index, 0x40);
        if (*(short *)((char *)tag_element + 0x20) == (short)-1) {
          *(short *)((char *)tag_element + 0x20) = face_count;
          *(short *)((char *)tag_element + 0x22) = 0;
        }
        *(short *)((char *)tag_element + 0x22) =
          *(short *)((char *)tag_element + 0x22) + 1;
      }
      bitmap_delete(cube_map);
      cube_map = 0;
      face_count = 0;
    }

  next_entry:
    bitmap_delete(*(void **)entry);
    i++;
  } while (success);

  if (cube_map) {
    if (success) {
      crt_fprintf((void *)0x331050,
                  "skipping cube map with less than six faces\r\n");
      crt_fflush((void *)0x331050);
    }
    bitmap_delete(cube_map);
  }
  return success;
}

/*
 * FUN_00075a20 (0x75a20) -- bitmap extract: pack sprites into texture pages.
 *
 * Type-3 (sprite) counterpart of FUN_00075630/FUN_00075800.  Derives the page
 * dimension and inter-sprite spacing from the bitmap group globals, verifies
 * every extracted sprite fits inside one page, then asks FUN_000747d0 to lay
 * the sprites out into pages.  For each page it allocates a texture page
 * bitmap, fills it, blits every sprite assigned to that page, writes the
 * normalised sprite rectangle and registration point back into the sequence's
 * sprite block, registers the page with FUN_00075380, and finally reports
 * per-page utilisation and the overall sprite budget.
 *
 * Returns 1 on success, 0 on any failure -- the flag is returned in AL at all
 * five RET sites (0x75b25 MOV AL,DL / 0x75dae / 0x75dfa / 0x75e3c MOV
 * AL,[EBP-1] / 0x75e5a XOR AL,AL), so the declaration is char, not void.
 */
char FUN_00075a20(void)
{
  int pages[32];
  int fill_colors[3];
  char *sprite_entry;
  int spacing;
  int budget_pixels;
  void *sequence_element;
  int sprite_spacing;
  short destination_point[2];
  short page_count;
  int used_pixels;
  short sprite_index;
  void *page_bitmap;
  short page_index;
  char success;
  /* Register-resident in the original; these need no frame slot. */
  int page_dimension;
  short max_sprite_dimension;
  short sprite_offset;
  char *page;
  void *sprite_element;
  char *sprite_rect;
  void *sprite_bitmap;
  float budget_total;
  float budget_used;

  success = 1;
  used_pixels = 0;
  /* DEC SI / NEG SI / SBB ESI,ESI / AND ESI,3 / INC ESI == (u != 1) ? 4 : 1 */
  spacing = (*(short *)(*(char **)0x33414c + 0x4c) != 1) ? 4 : 1;
  page_dimension = 0x20 << ((*(short *)(*(char **)0x33414c + 0x16) == 0) ?
                              4 :
                              *(short *)(*(char **)0x33414c + 0x14));
  budget_pixels =
    *(short *)(*(char **)0x33414c + 0x16) * page_dimension * page_dimension;
  max_sprite_dimension = (short)(page_dimension - spacing * 2);
  fill_colors[0] = 0;
  fill_colors[1] = -1;
  fill_colors[2] = 0x7f7f7f7f;

  /* Pass 1: every sprite must fit inside a page once spacing is reserved. */
  sprite_index = 0;
  do {
    if (sprite_index >= *(short *)0x334138) {
      if (success) {
        if (*(unsigned char *)(*(char **)0x33414c + 6) & 4)
          error(0,
                "### ERROR hey - don't even try it! (uniform sprite "
                "sequences)\ndon't fucking swim in that septic tank with your "
                "mouth open like that");
        FUN_000747d0(pages, &page_count, page_dimension, spacing);
        *(short *)(*(char **)0x33414c + 0x50) = (short)spacing;
      }
      break;
    }
    sprite_bitmap = *(void **)(*(char **)0x334134 + sprite_index * 0x10);
    if (sprite_bitmap != 0 &&
        (*(short *)((char *)sprite_bitmap + 4) > max_sprite_dimension ||
         *(short *)((char *)sprite_bitmap + 6) > max_sprite_dimension)) {
      error(0, "### ERROR one or more sprites do not fit in the requested page "
               "size");
      success = 0;
    }
    sprite_index++;
  } while (success);

  /* Pass 2: build one texture page bitmap per laid-out page. */
  page_index = 0;
  while (success && page_index < page_count) {
    page = (char *)pages[page_index];
    page_bitmap = bitmap_2d_new(*(unsigned short *)(page + 8),
                                *(unsigned short *)(page + 0xa), 0, 0xb);
    if (page_bitmap == 0 || *(int *)((char *)page_bitmap + 0x2c) == 0) {
      error(2,
            "### ERROR extract_sprite: failed to allocate texture page bitmap");
      success = 0;
    } else {
      FUN_00077510(page_bitmap,
                   fill_colors[*(short *)(*(char **)0x33414c + 0x4e)]);
      for (sprite_index = 0; success && sprite_index < *(short *)0x334138;
           sprite_index++) {
        sprite_entry = *(char **)0x334134 + sprite_index * 0x10;
        /* Two back-to-back calls share one ADD ESP,0x18 at 0x75bea; that is
         * a merged cdecl cleanup, not a single six-argument call. */
        sequence_element = tag_block_get_element(
          *(char **)0x33414c + 0x54, (int)*(short *)(sprite_entry + 4), 0x40);
        sprite_element =
          tag_block_get_element((char *)sequence_element + 0x34,
                                (int)*(short *)(sprite_entry + 6), 0x20);
        if (*(short *)(sprite_entry + 8) == page_index) {
          sprite_rect =
            (char *)FUN_0011fef0(page, *(int *)(sprite_entry + 0xc));
          /* MOVSX ECX,CX at 0x75c2e proves a short sits between the int
           * spacing and the int the FILD at 0x75c37 reads. */
          sprite_offset = (short)spacing;
          if (!(*(unsigned char *)(*(char **)0x33414c + 6) & 8) &&
              *(short *)(*(int *)(page + 0x18) + 0x30) == 1)
            sprite_offset = 0;
          *(short *)sprite_element = page_index;
          sprite_spacing = sprite_offset;
          /* Byte offsets are taken from the disassembly (FSTP float [ESI+N]);
           * Ghidra types the element short* so its indices are halved.  The
           * FILD/FIDIV idiom must survive: divide by an int, never by a
           * precomputed float reciprocal. */
          *(float *)((char *)sprite_element + 0x18) =
            ((float)sprite_spacing +
             *(float *)((char *)sprite_element + 0x18)) /
            (float)(int)*(short *)(page + 8);
          *(float *)((char *)sprite_element + 0x1c) =
            ((float)sprite_spacing +
             *(float *)((char *)sprite_element + 0x1c)) /
            (float)(int)*(short *)(page + 0xa);
          *(float *)((char *)sprite_element + 8) =
            (float)(*(short *)(sprite_rect + 4) - sprite_spacing) /
            (float)(int)*(short *)(page + 8);
          *(float *)((char *)sprite_element + 0x10) =
            (float)(*(short *)(sprite_rect + 6) - sprite_spacing) /
            (float)(int)*(short *)(page + 0xa);
          *(float *)((char *)sprite_element + 0xc) =
            (float)((int)*(short *)(sprite_rect + 4) + sprite_spacing +
                    *(short *)(sprite_rect + 8)) /
            (float)(int)*(short *)(page + 8);
          *(float *)((char *)sprite_element + 0x14) =
            (float)((int)*(short *)(sprite_rect + 6) + sprite_spacing +
                    *(short *)(sprite_rect + 0xa)) /
            (float)(int)*(short *)(page + 0xa);
          if (*(short *)((char *)sequence_element + 0x20) == (short)-1) {
            *(short *)((char *)sequence_element + 0x20) = page_index;
            *(short *)((char *)sequence_element + 0x22) = 1;
          } else {
            *(short *)((char *)sequence_element + 0x22) =
              (short)(page_index - *(short *)((char *)sequence_element + 0x20));
          }
          destination_point[0] = *(short *)(sprite_rect + 4);
          destination_point[1] = *(short *)(sprite_rect + 6);
          FUN_00072490(page_bitmap, destination_point, 0,
                       *(void **)sprite_entry, 0, -1, 0);
          bitmap_delete(*(void **)sprite_entry);
        }
      }
      if (FUN_00075380(page_bitmap) != (short)-1) {
        used_pixels += *(short *)(page + 0xa) * *(short *)(page + 8);
        bitmap_delete(page_bitmap);
      }
    }
    crt_fprintf((void *)0x331050,
                "texture page created #%dx#%d (%3.2f%% used)\r\n",
                (int)*(short *)(page + 8), (int)*(short *)(page + 0xa),
                (double)(FUN_0011fd10(page, 1) * 100.0f));
    crt_fflush((void *)0x331050);
    FUN_0011fe80(page);
    page_index++;
  }

  if (success) {
    /* FCOM/FNSTSW/TEST AH,0x44/JP at 0x75dda: JP is taken when the operands
     * are not equal, so the fallthrough (this block) is the == case. */
    /* Naming the converted budget keeps it as a single ST0 value, which is
     * what lets the comparison use the memory-operand form (FCOM m32) the
     * original emits instead of FLD/FLD/FUCOMPP. */
    budget_total = (float)budget_pixels;
    if (budget_total == 0.0f) {
      crt_fprintf((void *)0x331050, "### WARNING no sprite budget set\r\n");
      crt_fflush((void *)0x331050);
      return success;
    }
    /* FIDIVR at 0x75e07 is a reverse divide: ST0 = used / budget. */
    budget_used = used_pixels / budget_total;
    /* TEST AH,0x41/JP at 0x75e1e: JP is taken when ST0 > 1.0f, so the
     * fallthrough (this block) is the <= case. */
    if (budget_used <= 1.0f) {
      crt_fprintf((void *)0x331050, "sprite budget met (%3.0f%%)\r\n",
                  (double)(budget_used * 100.0f));
      crt_fflush((void *)0x331050);
      return success;
    }
    error(2, "### ERROR sprite budget exceeded (%3.0f%%)",
          (double)(budget_used * 100.0f));
    return 0;
  }
  return success;
}

/*
 * FUN_00076300 -- bitmap extract: the no-sequences path.
 *
 * Allocates a single bitmap element in the group's bitmaps block (+0x54) and
 * extracts the whole colour plate into it, using a bounds rectangle that
 * spans the entire plate.  Group types 1 (3D textures) and 3 (sprites) cannot
 * be extracted without a real plate and report an error instead.
 *
 * Source TU: c:\halo\SOURCE\bitmaps\bitmap_extract.c (assert string, line 422).
 * ABI: returns success in AL; the caller at FUN_00076790 branches on it.
 *
 * Globals: 0x33414c = bitmap group tag (+0x00 type, +0x02 plate/usage,
 * +0x54 bitmaps tag_block); 0x334148 = extract-sequences flag;
 * 0x334150 = source plate bitmap (+0x04 / +0x06 dimensions);
 * 0x334158 = current bitmap element; 0x33415c = current bitmap index.
 */
char FUN_00076300(void)
{
  short bounds[4];
  char *group;
  char *bitmaps_block_owner;
  char *plate;
  short plate_field_04;
  short plate_field_06;
  short new_bitmap_index;

  group = *(char **)0x33414c;

  if (*(short *)(group + 2) == 0 && *(char *)0x334148 != 0) {
    error(2, "### ERROR extract: compressed color-key transparency format "
             "must use a valid plate");
    return 0;
  }

  switch (*(short *)group) {
  case 0:
  case 2:
  case 4:
    plate = *(char **)0x334150;
    bounds[0] = 0;
    bounds[1] = 0;
    /* Both plate fields are read before the first store into bounds: the
       buffer's address is taken (it is passed to FUN_00075e70), so a store
       through it between the two reads would force MSVC to reload the
       global.  The original loads +0x04 and +0x06 back to back and stores
       them crossed -- +0x04 lands in the HIGH element. */
    plate_field_04 = *(short *)(plate + 4);
    plate_field_06 = *(short *)(plate + 6);
    bounds[3] = plate_field_04;
    bounds[2] = plate_field_06;

    new_bitmap_index = tag_block_add_element(group + 0x54);
    /* The group pointer is re-read for the second block rather than reusing
       the copy held across the add_element call, and the re-read is issued
       before the index is published to 0x33415c. */
    bitmaps_block_owner = *(char **)0x33414c;
    *(short *)0x33415c = new_bitmap_index;
    *(void **)0x334158 = tag_block_get_element(bitmaps_block_owner + 0x54,
                                               (int)new_bitmap_index, 0x40);
    *(short *)(*(char **)0x334158 + 0x20) = (short)0xffff;
    FUN_00075e70(bounds);
    return 1;

  case 1:
    error(2, "### ERROR can't extract 3D textures without a valid plate");
    return 0;

  case 3:
    error(2, "### ERROR can't extract sprites without a valid plate");
    return 0;

  default:
    display_assert("### ERROR unsupported bitmap group type",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_extract.c", 0x1a6, 1);
    system_exit(-1);
  }
}

/*
 * FUN_000766e0 -- bitmap extract: allocate and process all sequences.
 *
 * Iterates source bitmap rows (up to *(short*)(*(char**)0x334150+6) count),
 * allocating a new sequence element in the group's sequences block for each
 * run, initialising its frame range fields, then calling FUN_00076410 to
 * extract bitmaps into it. Returns 1 on full success, 0 on any failure.
 */
char FUN_000766e0(void)
{
  int local_8;
  int iVar3;
  short sVar2;
  char cVar1;

  cVar1 = 1;
  local_8 = 1;
  while (1) {
    if (*(short *)(*(char **)0x334150 + 6) <= (short)local_8)
      return cVar1;
    iVar3 = FUN_00073960(&local_8);
    FUN_00073a80();
    sVar2 = tag_block_add_element(*(char **)0x33414c + 0x54);
    if (sVar2 == -1) {
      error(2, "### ERROR extract: failed to allocate sequence");
      return 0;
    }
    *(short *)0x33415c = sVar2;
    *(void **)0x334158 =
      tag_block_get_element(*(char **)0x33414c + 0x54, (int)sVar2, 0x40);
    *(short *)(*(char **)0x334158 + 0x20) = (short)0xffff;
    *(short *)(*(char **)0x334158 + 0x22) = 0;
    cVar1 = FUN_00076410(local_8, (short)iVar3);
    local_8 = iVar3 + 1;
    if (!cVar1)
      return 0;
  }
}

/*
 * FUN_00076790 (0x76790) -- bitmap extract: decompress the color plate and run
 * the per-group-type extraction pass.
 *
 * Validates group->type/format/usage against their enum bounds, allocates the
 * 0x4000-byte scratch bitmap array at 0x334134, resizes the group's two tag
 * blocks (+0x60, +0x54) and its pixel tag_data (+0x30) to zero, allocates a
 * 32-bit color plate of the group's import dimensions, decompresses the stored
 * plate pixels into it, then dispatches on group->type. The scratch array is
 * always released on the way out.
 *
 * Returns 1 on success, 0 on any failure. Source: bitmap_extract.c
 */
char FUN_00076790(void *group, int param_3)
{
  unsigned int decompressed_plate_size;
  char extract_sequences;
  char result;

  if (group == 0) {
    display_assert("group", "c:\\halo\\SOURCE\\bitmaps\\bitmap_extract.c", 0xb4,
                   1);
    system_exit(-1);
  }
  if (*(short *)group < 0 || *(short *)group >= 5) {
    display_assert(
      "group->type >=0 && group->type <NUMBER_OF_BITMAP_GROUP_TYPES",
      "c:\\halo\\SOURCE\\bitmaps\\bitmap_extract.c", 0xb5, 1);
    system_exit(-1);
  }
  if (*(short *)((char *)group + 2) < 0 || *(short *)((char *)group + 2) >= 6) {
    display_assert(
      "group->format>=0 && group->format<NUMBER_OF_BITMAP_GROUP_FORMATS",
      "c:\\halo\\SOURCE\\bitmaps\\bitmap_extract.c", 0xb6, 1);
    system_exit(-1);
  }
  if (*(short *)((char *)group + 4) < 0 || *(short *)((char *)group + 4) >= 6) {
    display_assert(
      "group->usage >=0 && group->usage <NUMBER_OF_BITMAP_GROUP_USAGES",
      "c:\\halo\\SOURCE\\bitmaps\\bitmap_extract.c", 0xb7, 1);
    system_exit(-1);
  }

  /* 16-bit store: 0x33413a is a separate datum. */
  *(short *)0x334138 = 0;
  *(void **)0x334134 = debug_malloc(
    0x4000, 0, "c:\\halo\\SOURCE\\bitmaps\\bitmap_extract.c", 0xba);
  if (*(void **)0x334134 == 0) {
    error(2, "### ERROR extract: failed to allocate bitmap array");
    /* Deliberate fallthrough: the original does not branch around the
     * resize-failure error below, so this path prints both messages. */
  } else if (tag_block_resize((char *)group + 0x60, 0) &&
             tag_block_resize((char *)group + 0x54, 0) &&
             tag_data_resize((char *)group + 0x30, 0)) {
    *(void **)0x334158 = 0;
    *(short *)0x33415c = (short)0xffff;
    *(void **)0x33414c = group;
    *(void **)0x334150 =
      bitmap_2d_new(*(unsigned short *)((char *)group + 0x18),
                    *(unsigned short *)((char *)group + 0x1a), 0, 0xb);
    if (*(void **)0x334150 == 0) {
      error(2, "### ERROR extract: failed to allocate color plate");
      result = 0;
    } else {
      decompressed_plate_size =
        FUN_00119bb0(*(unsigned int **)((char *)group + 0x28),
                     *(unsigned int *)((char *)group + 0x1c));
      if (decompressed_plate_size !=
          (unsigned int)((int)*(short *)((char *)group + 0x1a) *
                         (int)*(short *)((char *)group + 0x18) * 4)) {
        display_assert("decompressed_plate_size==sizeof(pixel32)*group->import_"
                       "width*group->import_height",
                       "c:\\halo\\SOURCE\\bitmaps\\bitmap_extract.c", 0x104, 1);
        system_exit(-1);
      }
      if (FUN_00119bf0(
            *(unsigned int **)((char *)group + 0x28),
            *(unsigned int *)((char *)group + 0x1c),
            (int)bitmap_mipmap_address(*(void **)0x334150, 0),
            &decompressed_plate_size,
            /* dup-args-ok: the decompressed size is passed both by
               reference (in/out) and by value (destination capacity). */
            decompressed_plate_size)) {
        FUN_00073830();
        /* The flag is loaded and tested before the param_3 store in the
           original; keeping it in a local reproduces that order. */
        extract_sequences = *(char *)0x334148;
        *(int *)0x334154 = param_3;
        if (extract_sequences)
          result = FUN_000766e0();
        else
          result = FUN_00076300();
        bitmap_delete(*(void **)0x334150);
        if (result) {
          /* type is re-read through the global, not from the parameter. */
          switch (*(short *)*(char **)0x33414c) {
          case 0:
          case 4:
            break;
          case 1:
            result = FUN_00075630();
            break;
          case 2:
            result = FUN_00075800();
            break;
          case 3:
            result = FUN_00075a20();
            break;
          default:
            display_assert("### ERROR unsupported bitmap group type",
                           "c:\\halo\\SOURCE\\bitmaps\\bitmap_extract.c", 0x137,
                           1);
            system_exit(-1);
          }
        }
      } else {
        error(2, "### ERROR extract: failed to decompress color plate");
        result = 0;
      }
    }
    goto cleanup;
  }
  error(2, "### ERROR extract: failed to resize bitmap group tags to zero");
  result = 0;

cleanup:
  if (*(void **)0x334134 != 0)
    debug_free(*(void **)0x334134,
               "c:\\halo\\SOURCE\\bitmaps\\bitmap_extract.c", 0x13d);
  return result;
}

/* FUN_00076a70 (0x76a70) — bitmap extract: compress color plate pixel data into
 * group buffer. Validates plate and group, allocates a temporary buffer,
 * compresses the pixel data using FUN_00119b40, reallocates to compressed size,
 * stores width/height/pointer in group, then calls FUN_00076790 to continue.
 * Source: bitmap_extract.c */
char FUN_00076a70(void *plate, void *group, int param_3)
{
  int size;
  void *t;

  if (!bitmap_verify(plate, 1)) {
    display_assert("bitmap_verify(plate, TRUE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_extract.c", 0x83, 1);
    system_exit(-1);
  }
  if (!group) {
    display_assert("group", "c:\\halo\\SOURCE\\bitmaps\\bitmap_extract.c", 0x84,
                   1);
    system_exit(-1);
  }

  size = bitmap_get_pixel_data_size(plate);
  *(unsigned short *)((char *)group + 0x18) =
    *(unsigned short *)((char *)plate + 4);
  *(unsigned short *)((char *)group + 0x1a) =
    *(unsigned short *)((char *)plate + 6);

  *(int *)((char *)group + 0x28) = (int)debug_malloc(
    size, 0, "c:\\halo\\SOURCE\\bitmaps\\bitmap_extract.c", 0x8a);
  if (*(int *)((char *)group + 0x28)) {
    if (FUN_00119b40((int)bitmap_mipmap_address(plate, 0), (unsigned int)size,
                     (unsigned int *)*(int *)((char *)group + 0x28),
                     &size, /* dup-args-ok: size is both source limit and output
                               capacity. */
                     (unsigned int)size)) {
      t = debug_realloc((void *)*(int *)((char *)group + 0x28), size,
                        "c:\\halo\\SOURCE\\bitmaps\\bitmap_extract.c", 0x90);
      if (t) {
        *(int *)((char *)group + 0x28) = (int)t;
        *(int *)((char *)group + 0x1c) = size;
        return FUN_00076790(group, param_3);
      }
      error(2, "### ERROR extract: failed to realloc color plate");
      return 0;
    }
    error(2, "### ERROR extract: failed to compress color plate");
    return 0;
  }
  error(2, "### ERROR extract: failed to allocate temporary buffer");
  return 0;
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
 * FUN_00076bd0 -- bitmap_group_postprocess: validate and initialize a bitmap
 * group tag.
 *
 * Called during tag loading. For each bitmap in the group: sets FORCE_POW2 flag
 * for interface bitmaps, verifies the bitmap, and registers it with the texture
 * cache. For each sequence: fixes up sprite-type bitmap indices and trims
 * trailing empty sequences. When DAT_00336194 is set (debug/validation mode),
 * emits diagnostic errors for malformed bitmaps, sequences, and sprites.
 *
 * Source TU: bitmap_utilities.c (confirmed by address placement)
 */
char FUN_00076bd0(int tag_index)
{
  char success;
  void *tag;
  void *bmp;
  void *seq;
  void *last_seq;
  void *next_seq;
  void *sprite;
  int *bitmaps_block;
  int *sequences_block;
  int *sprites_block;
  int bitmaps_count;
  int sequences_count;
  int sprites_count;
  int n;
  int i;
  int j;
  int w;
  int h;
  short first_bmp;
  short bmp_count;
  short bmp_idx;

  tag = tag_get(0x6269746d, tag_index);
  success = 1;

  /* Initialize bitmaps: set FORCE_POW2 for interface type, verify, register
   * with texture cache. */
  for (i = 0; i < *(int *)((char *)tag + 0x60); i++) {
    bmp = tag_block_get_element((int *)((char *)tag + 0x60), i, 0x30);
    if (*(short *)tag == 4) {
      /* interface bitmaps must be power-of-two: set FORCE_POW2 flag */
      *(unsigned char *)((char *)bmp + 0x0e) |= 0x10;
    }
    if (!bitmap_verify(bmp, 0)) {
      success = 0;
    } else {
      texture_cache_bitmap_new(tag_index, bmp);
    }
  }

  /* Fix up sequences: for sprite-type groups, clear first_bitmap_index and
   * bitmap_count fields if they are nonzero. */
  for (i = 0; i < *(int *)((char *)tag + 0x54); i++) {
    seq = tag_block_get_element((int *)((char *)tag + 0x54), i, 0x40);
    if (i < *(int *)((char *)tag + 0x54) - 1) {
      /* lookahead: touch next sequence element (side-effect order preservation)
       */
      tag_block_get_element((int *)((char *)tag + 0x54), i + 1, 0x40);
    }
    if (*(short *)tag == 3) {
      if (*(short *)((char *)seq + 0x20) || *(short *)((char *)seq + 0x22)) {
        seq = tag_block_get_element((int *)((char *)tag + 0x54), i, 0x40);
        *(short *)((char *)seq + 0x20) = 0;
        seq = tag_block_get_element((int *)((char *)tag + 0x54), i, 0x40);
        *(short *)((char *)seq + 0x22) = 0;
      }
    }
  }

  /* Trim trailing empty sequence: if the last sequence has bitmap_count == 0
   * and no sprites, shrink the sequences block by one. */
  n = *(int *)((char *)tag + 0x54);
  if (n > 0) {
    last_seq = tag_block_get_element((int *)((char *)tag + 0x54), n - 1, 0x40);
    if (*(short *)((char *)last_seq + 0x22) == 0 &&
        *(int *)((char *)last_seq + 0x34) == 0) {
      if (!tag_block_resize((int *)((char *)tag + 0x54), n - 1)) {
        error(0, "### FATAL_ERROR failed to fix bitmap group '%s'",
              tag_get_name(tag_index));
        success = 0;
      }
    }
  }

  /* Validation section: only active when DAT_00336194 is set. */
  if (*(uint8_t *)0x336194) {
    bitmaps_block = (int *)((char *)tag + 0x60);

    /* Check each bitmap for problematic formats and flags. */
    for (i = 0; i < *bitmaps_block; i++) {
      bmp = tag_block_get_element(bitmaps_block, i, 0x30);
      if (*(short *)((char *)bmp + 0x0c) == 3) {
        /* bitmap format is a8y8 -- must be fixed */
        error(2, "!!MUST BE FIXED: bitmap #%d of group '%s' has a8y8 format", i,
              tag_get_name(tag_index));
      }
      if (*(unsigned char *)((char *)bmp + 0x0e) & 0x10) {
        /* FORCE_POW2 flag set: check if actually power-of-two */
        w = (int)*(short *)((char *)bmp + 0x04);
        h = (int)*(short *)((char *)bmp + 0x06);
        if (!((w - 1) & w) && !((h - 1) & h)) {
          /* both dimensions are powers of two: linear bitmap does not need
           * forcing; flag is redundant/erroneous */
          error(2,
                "!!MUST BE FIXED: bitmap #%d of group '%s' is linear and "
                "power-of-two",
                i, tag_get_name(tag_index));
        }
      }
    }

    /* Check that the group has at least one bitmap. */
    bitmaps_count = *bitmaps_block;
    if (bitmaps_count < 1) {
      /* BUG (faithful reproduction): format and description were split into
       * two args by a missing string concatenation in the original source. */
      error(2, "!!MUST BE FIXED: ", "bitmap group '%s' has %d bitmaps",
            tag_get_name(tag_index), bitmaps_count);
    }

    /* Check that the group has at least one sequence, then validate each. */
    sequences_block = (int *)((char *)tag + 0x54);
    sequences_count = *(int *)((char *)tag + 0x54);
    if (sequences_count < 1) {
      /* BUG (faithful reproduction): same split-format bug as above. */
      error(2, "!!MUST BE FIXED: ", "bitmap group '%s' has %d sequences",
            tag_get_name(tag_index), sequences_count);
    }

    for (i = 0; i < *sequences_block; i++) {
      seq = tag_block_get_element(sequences_block, i, 0x40);
      next_seq = (i < *sequences_block - 1) ?
                   tag_block_get_element(sequences_block, i + 1, 0x40) :
                   (void *)0;

      if (*(short *)tag == 3) {
        /* sprites bitmap group */
        if (*(short *)((char *)seq + 0x20) == 0 &&
            *(short *)((char *)seq + 0x22) == 0) {
          /* sequence correctly has no direct bitmaps -- proceed to sprite
           * block check below */
        } else {
          error(2,
                "!!MUST BE FIXED: bitmap group '%s' (type=%d) sequence #%d"
                " doesn't know it's a sprite sequence",
                tag_get_name(tag_index), (int)*(short *)tag, i);
        }
      } else {
        /* non-sprite bitmap group: validate the bitmap range */
        first_bmp = *(short *)((char *)seq + 0x20);
        bmp_count = *(short *)((char *)seq + 0x22);
        bitmaps_count = *(int *)((char *)tag + 0x60);
        if (first_bmp >= 0 && (int)first_bmp < bitmaps_count &&
            bmp_count >= 1 &&
            (int)first_bmp + (int)bmp_count <= bitmaps_count) {
          /* Range is within bounds. Skip the warning if: the sequence is the
           * first starting at 0, or the next sequence starts exactly where
           * this one ends (contiguous allocation). */
          if ((i == 0 && first_bmp == 0) ||
              (next_seq != (void *)0 &&
               (int)*(short *)((char *)next_seq + 0x20) ==
                 (int)first_bmp + (int)bmp_count)) {
            goto skip_range_warning;
          }
        }
        error(2,
              "!!MUST BE FIXED: bitmap group '%s' sequence #%d"
              " references bitmaps [#%d..#%d]",
              tag_get_name(tag_index), i, (int)*(short *)((char *)seq + 0x20),
              (int)*(short *)((char *)seq + 0x20) +
                (int)*(short *)((char *)seq + 0x22));
      skip_range_warning:;
      }

      /* Check the sprite sub-block regardless of type. */
      sprites_block = (int *)((char *)seq + 0x34);
      sprites_count = *sprites_block;
      if (*(short *)tag == 3) {
        /* sprites group: must have at least one sprite per sequence */
        if (sprites_count < 1) {
          error(
            2, "!!MUST BE FIXED: bitmap group '%s' sequence #%d has %d sprites",
            tag_get_name(tag_index), i, sprites_count);
        } else {
          for (j = 0; j < sprites_count; j++) {
            sprite = tag_block_get_element(sprites_block, j, 0x20);
            bmp_idx = *(short *)sprite;
            bitmaps_count = *(int *)((char *)tag + 0x60);
            if (bmp_idx < 0 || (int)bmp_idx >= bitmaps_count) {
              error(2,
                    "!!MUST BE FIXED: bitmap group '%s' sequence #%d"
                    " sprite #%d references bitmap #%d",
                    tag_get_name(tag_index), i, j, (int)bmp_idx);
            }
          }
        }
      } else {
        /* non-sprite group: must have zero sprites per sequence */
        if (sprites_count > 0) {
          error(2,
                "!!MUST BE FIXED: bitmap group '%s' (type=%d) sequence #%d"
                " has %d sprites",
                tag_get_name(tag_index), (int)*(short *)tag, i, sprites_count);
        }
      }
    }
  }

  return success;
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
      uVar2 =
        tag_block_get_element((int *)(iVar1 + 0x60), (int)bitmap_index, 0x30);
    }
  }
  return uVar2;
}

/*
 * FUN_00077040 -- bitmap_group_get_bitmap: resolve sequence/frame index pair
 * to a bitmap data element in a 'bitm' tag.
 *
 * Walks the tag's sequence block to find the correct bitmap index, handling
 * direct-bitmap sequences (frame_count >= 1) and sprite sequences. Falls back
 * to frame_index if the resolved bitmap index is -1.
 *
 * Source TU: bitmap_group.c (assert strings confirm)
 */
void *FUN_00077040(int tag_index, short sequence_index, short frame_index)
{
  int tag;
  int sequence;
  short bitmap_idx;

  if (tag_index == -1)
    goto cleanup_null;
  if (sequence_index < 0 || frame_index < 0) {
    display_assert("sequence_index>=0 && frame_index>=0",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_group.c", 0x2a6, 1);
    system_exit(-1);
  }
  tag = (int)tag_get(0x6269746d, tag_index);
  if (tag == 0)
    goto cleanup_null;
  if (*(int *)(tag + 0x54) > 0) {
    sequence = (int)tag_block_get_element(
      (int *)(tag + 0x54), (int)sequence_index % *(int *)(tag + 0x54), 0x40);
    if (*(short *)(sequence + 0x22) > 0) {
      bitmap_idx =
        (short)((int)frame_index % (int)*(short *)(sequence + 0x22)) +
        *(short *)(sequence + 0x20);
      goto done;
    }
    if (*(int *)(sequence + 0x34) == 0)
      goto fallback;
    bitmap_idx = *(short *)tag_block_get_element((int *)(sequence + 0x34),
                                                 (int)frame_index, 0x20);
  done:
    if (bitmap_idx != -1)
      goto ret_check;
  }
fallback:
  bitmap_idx = frame_index;
ret_check:
  if (bitmap_idx < 0)
    goto cleanup_null;
  if ((int)bitmap_idx >= *(int *)(tag + 0x60))
    goto cleanup_null;
  return tag_block_get_element((int *)(tag + 0x60), (int)bitmap_idx, 0x30);
cleanup_null:
  return NULL;
}

/*
 * FUN_00077120 -- bitmap_group_add_bitmap: validate and add a new bitmap entry
 * to a bitmap group tag.
 *
 * Validates dimensions (power-of-two, cube-map squareness) and format flags,
 * builds a 0x30-byte bitmap_data struct on the stack, resizes the group's
 * bitmaps block and pixel_data tag_data to hold the new entry, copies the
 * struct into the new slot, sets the base address, and zeroes the pixel data
 * region. Returns the new bitmap index (0-based) on success, -1 on failure.
 *
 * Flags bitmask (at +0x0e): bit 0 = power-of-two, bit 1 = compressed format
 * (14..16), bit 2 = format 17.
 *
 * Disassembly: struct fields initialized in MSVC store order (depth, flags=0,
 * reg_point_y=0, reg_point_x=0, mipmap_count, then fourcc/type/width/height/
 * format). Flags OR'd at byte level (OR byte ptr). Non-POT guard uses TEST
 * byte,1 / JNZ skip / CMP DX,4 / JZ skip / error pattern.
 *
 * Source file: c:\halo\SOURCE\bitmaps\bitmap_group.c (~line 0x2d7)
 */
short FUN_00077120(void *group, short type, short width, short height,
                   short depth, short format, short mipmap_count)
{
  char new_bitmap[0x30];
  short group_type;
  int old_count;
  int new_element;
  int pixel_size;
  char ok;
  short i;
  int cur;
  int prev;
  int prev_size;
  int space;
  int cur_size;
  int last_end;

  last_end = 0;

  if (group == NULL) {
    display_assert("group", "c:\\halo\\SOURCE\\bitmaps\\bitmap_group.c", 0x2db,
                   1);
    system_exit(-1);
  }

  *(short *)(new_bitmap + 0x0a) = depth;
  *(short *)(new_bitmap + 0x0e) = 0;
  *(short *)(new_bitmap + 0x12) = 0;
  *(short *)(new_bitmap + 0x10) = 0;
  *(short *)(new_bitmap + 0x14) = mipmap_count;
  *(int *)(new_bitmap + 0x18) = 0;
  *(int *)(new_bitmap + 0x28) = 0;
  *(int *)(new_bitmap + 0x2c) = 0;

  group_type = *(short *)group;

  *(int *)(new_bitmap + 0x00) = 0x6269746d;
  *(short *)(new_bitmap + 0x04) = type;
  *(short *)(new_bitmap + 0x06) = width;
  *(short *)(new_bitmap + 0x08) = height;
  *(short *)(new_bitmap + 0x0c) = format;

  if (group_type == 4) {
    *(short *)(new_bitmap + 0x0e) = 0x10;
  } else {
    if ((type & (type - 1)) != 0 || (width & (width - 1)) != 0 ||
        (height & (height - 1)) != 0) {
      crt_fprintf((void *)0x331050,
                  "skipping bitmap with non-power-of-two dimensions "
                  "(#%dx#%d#%d)\r\n",
                  (int)type, (int)width, (int)height);
      crt_fflush((void *)0x331050);
      return (short)-1;
    }
    if (group_type == 2 && type != width) {
      crt_fprintf((void *)0x331050,
                  "skipping cube map with non-square faces (#%dx#%d)\r\n",
                  (int)type, (int)width);
      crt_fflush((void *)0x331050);
      return (short)-1;
    }
    *(short *)(new_bitmap + 0x0e) = 1;
  }

  if (format >= 0xe && format <= 0x10) {
    *(unsigned char *)(new_bitmap + 0x0e) |= 2;
  }
  if (format == 0x11) {
    *(unsigned char *)(new_bitmap + 0x0e) |= 4;
  }

  if (group_type == 2 && type != width) {
    crt_fprintf((void *)0x331050,
                "skipping cube map with non-square faces (#%dx#%d)\r\n",
                (int)type, (int)width);
    crt_fflush((void *)0x331050);
    return (short)-1;
  }

  if ((*(unsigned char *)(new_bitmap + 0x0e) & 1) != 0)
    goto after_pow2_guard;
  if (group_type != 4) {
    crt_fprintf((void *)0x331050,
                "skipping bitmap with non power-of-two dimensions "
                "(#%dx#%d)\r\n",
                (int)type, (int)width);
    crt_fflush((void *)0x331050);
    return (short)-1;
  }

after_pow2_guard:
  pixel_size = bitmap_get_pixel_data_size(new_bitmap + 4);

  old_count = *(int *)((char *)group + 0x60);

  ok = tag_block_resize((char *)group + 0x60, old_count + 1);
  if (ok) {
    ok = tag_data_resize((char *)group + 0x30,
                         *(int *)((char *)group + 0x30) + pixel_size);
  }
  if (ok) {
    prev = 0;
    last_end = 0;
    i = 0;
    if (*(int *)((char *)group + 0x60) > 0) {
      do {
        cur = (int)tag_block_get_element((char *)group + 0x60, (int)i, 0x30);
        if (*(int *)((char *)cur + 0x2c) != 0) {
          if (*(int *)((char *)cur + 0x28) != 0) {
            display_assert("!bitmap->hardware_format",
                           "c:\\halo\\SOURCE\\bitmaps\\bitmap_group.c", 0x34d,
                           1);
            system_exit(-1);
          }
          *(int *)((char *)cur + 0x2c) =
            *(int *)((char *)cur + 0x18) + *(int *)((char *)group + 0x1e);
          if ((unsigned int)*(int *)((char *)cur + 0x2c) <
              (unsigned int)*(int *)((char *)group + 0x1e)) {
            display_assert(
              "(byte*)bitmap->base_address>=(byte*)group->pixel_data.address",
              "c:\\halo\\SOURCE\\bitmaps\\bitmap_group.c", 0x352, 1);
            system_exit(-1);
          }
          cur_size = bitmap_get_pixel_data_size((void *)cur);
          if ((unsigned int)(*(int *)((char *)group + 0x30) +
                             *(int *)((char *)group + 0x1e)) <
              (unsigned int)(cur_size + *(int *)((char *)cur + 0x2c))) {
            display_assert(
              "(byte*)bitmap->base_address + bitmap_get_pixel_data_size(bitmap)"
              " <= (byte*)group->pixel_data.address + group->pixel_data.size",
              "c:\\halo\\SOURCE\\bitmaps\\bitmap_group.c", 0x354, 1);
            system_exit(-1);
          }
          if (prev != 0) {
            prev_size = bitmap_get_pixel_data_size((void *)prev);
            space =
              *(int *)((char *)cur + 0x18) - *(int *)((char *)prev + 0x18);
            if (space - prev_size < 0) {
              display_assert("space_between>=0",
                             "c:\\halo\\SOURCE\\bitmaps\\bitmap_group.c", 0x35b,
                             1);
              system_exit(-1);
            }
            if (space != prev_size) {
              error(2, "### WARNING bitmap group pixel data isn't tight");
            }
          }
          last_end = bitmap_get_pixel_data_size((void *)cur) +
                     *(int *)((char *)cur + 0x18);
          prev = cur;
        }
        i = (short)(i + 1);
      } while ((int)i < *(int *)((char *)group + 0x60));
    }

    new_element =
      (int)tag_block_get_element((char *)group + 0x60, old_count, 0x30);
    if (new_element == 0) {
      display_assert("new_bitmap", "c:\\halo\\SOURCE\\bitmaps\\bitmap_group.c",
                     0x371, 1);
      system_exit(-1);
    }

    csmemcpy((void *)new_element, new_bitmap, 0x30);
    *(int *)(new_element + 0x18) = last_end;
    *(int *)(new_element + 0x2c) = *(int *)((char *)group + 0x1e) + last_end;
    csmemset((void *)(*(int *)((char *)group + 0x1e) + last_end), 0,
             pixel_size);

    return (short)old_count;
  }

  error(2, "### ERROR failed to add bitmap to group (tag resize failed)");
  tag_block_resize((char *)group + 0x60, old_count);
  return (short)-1;
}

/*
 * FUN_00077510 -- bitmap_fill: fill all pixels of a bitmap with a dword value.
 *
 * Gets the pixel base address via bitmap_2d_address(0,0,0), gets the pixel
 * count, then fills that many dwords with the given color. The original uses
 * REP STOSD.
 */
/* The original CALLs this from FUN_00075a20 (0x75b86) rather than inlining
 * it; MSVC 7.1 otherwise expands the fill loop (merged ADD ESP,0x14 plus a
 * REP STOSD) into its only in-TU caller. FUN_00075a20 is that sole caller,
 * so suppressing auto-inlining here cannot affect any other function. */
#if defined(_MSC_VER) && !defined(__clang__)
#pragma auto_inline(off)
#endif
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
#if defined(_MSC_VER) && !defined(__clang__)
#pragma auto_inline(on)
#endif

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

/* FUN_00077590 (0x77590) — clone a bitmap: allocates a new bitmap of the same
 * type/format, copies pixel data from source to the clone, copies the flags
 * field (+0xe). */
void *FUN_00077590(void *bitmap)
{
  void *cloned;
  void *src_data;
  void *dst_data;
  int src_size;
  int dst_size;
  short type;

  if (bitmap == 0) {
    display_assert("source_bitmap",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x67, 1);
    system_exit(-1);
  }
  if (*(int *)((char *)bitmap + 0x2c) == 0) {
    display_assert("source_bitmap->base_address",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x68, 1);
    system_exit(-1);
  }

  type = *(short *)((char *)bitmap + 0xa);
  cloned = 0;
  switch (type) {
  case 0:
    cloned = bitmap_2d_new(*(unsigned short *)((char *)bitmap + 4),
                           *(unsigned short *)((char *)bitmap + 6),
                           *(unsigned short *)((char *)bitmap + 0x14),
                           *(unsigned short *)((char *)bitmap + 0xc));
    break;
  case 1:
    cloned = bitmap_3d_new(*(unsigned short *)((char *)bitmap + 4),
                           *(unsigned short *)((char *)bitmap + 6),
                           *(unsigned short *)((char *)bitmap + 8),
                           *(unsigned short *)((char *)bitmap + 0x14),
                           *(unsigned short *)((char *)bitmap + 0xc));
    break;
  case 2:
    cloned = bitmap_cube_map_new(*(unsigned short *)((char *)bitmap + 4),
                                 *(unsigned short *)((char *)bitmap + 0x14),
                                 *(unsigned short *)((char *)bitmap + 0xc));
    break;
  default:
    display_assert("### ERROR unsupported bitmap type",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x83, 1);
    system_exit(-1);
    break;
  }

  if (cloned != 0 && *(int *)((char *)cloned + 0x2c) != 0) {
    src_data = bitmap_mipmap_address(bitmap, 0);
    dst_data = bitmap_mipmap_address(cloned, 0);
    src_size = bitmap_get_pixel_data_size(bitmap);
    dst_size = bitmap_get_pixel_data_size(cloned);
    if (src_size != dst_size) {
      display_assert(
        "bitmap_get_pixel_data_size(cloned_bitmap)==pixel_data_size",
        "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x8d, 1);
      system_exit(-1);
    }
    csmemcpy(dst_data, src_data, src_size);
    *(short *)((char *)cloned + 0xe) = *(short *)((char *)bitmap + 0xe);
    return cloned;
  }
  error(2, "### ERROR failed to allocate temporary bitmap");
  return cloned;
}

/*
 * FUN_00077720 -- box-filter 2x downscale for a 2D ARGB bitmap.
 *
 * Allocates a new ARGB (format 0xb) bitmap at (width/scale)x(height/scale),
 * averaging scale×scale source pixel blocks per output pixel.
 * Only includes non-transparent pixels in the average when alpha_weighted != 0.
 * brightness_adjust is added to the computed alpha channel value.
 * @<eax> = scale: box filter kernel size (must be >= 2).
 */
void *FUN_00077720(short scale /* @<eax> */, void *source_bitmap,
                   short brightness_adjust, char alpha_weighted)
{
  unsigned short src_width;
  int src_height;
  int kernel_x;
  int kernel_y;
  int new_width;
  int new_height;
  void *dst_bitmap;
  short dst_y;
  short dst_x;
  int src_y_base;
  int src_x_base;
  int inner_y;
  int inner_x;
  unsigned int *dst_pixel;
  unsigned int *src_pixel;
  unsigned int src_val;
  unsigned int src_alpha;
  int alpha_sum;
  int ch1_sum;
  int ch2_sum;
  int ch3_sum;
  int count;
  int half;
  int alpha_final;

  if (!bitmap_verify(source_bitmap, 1)) {
    display_assert("bitmap_verify(source_bitmap, TRUE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x105, 1);
    system_exit(-1);
  }
  if (*(short *)((char *)source_bitmap + 0xa) != 0) {
    display_assert("source_bitmap->type==_bitmap_type_2d",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x106, 1);
    system_exit(-1);
  }
  if (scale < 2) {
    display_assert("scale>1", "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c",
                   0x107, 1);
    system_exit(-1);
  }

  src_width = *(unsigned short *)((char *)source_bitmap + 4);
  kernel_x = (int)(unsigned short)src_width;
  if (scale <= (short)src_width)
    kernel_x = (int)scale;

  src_height = (int)*(short *)((char *)source_bitmap + 6);
  kernel_y = src_height;
  if (scale <= (short)src_height)
    kernel_y = (int)scale;

  new_width = (int)(short)src_width / (int)(short)kernel_x;
  new_height = src_height / (int)(short)kernel_y;

  dst_bitmap = bitmap_2d_new((unsigned short)new_width,
                             (unsigned short)new_height, 0, 0xb);

  if (dst_bitmap == 0 || *(int *)((char *)dst_bitmap + 0x2c) == 0) {
    error(2, "### ERROR failed to allocate temporary bitmap");
    return dst_bitmap;
  }

  dst_y = 0;
  if (new_height > 0) {
    src_y_base = 0;
    do {
      if ((short)new_width > 0) {
        dst_x = 0;
        src_x_base = 0;
        do {
          alpha_sum = 0;
          ch1_sum = 0;
          ch2_sum = 0;
          ch3_sum = 0;
          count = 0;
          dst_pixel = (unsigned int *)bitmap_2d_address(dst_bitmap, dst_x,
                                                        (short)dst_y, 0);
          inner_y = 0;
          if ((short)kernel_y < 1)
            goto store_zero;
          do {
            inner_x = 0;
            if ((short)kernel_x > 0) {
              do {
                src_pixel = (unsigned int *)bitmap_2d_address(
                  source_bitmap, (short)(src_x_base + inner_x),
                  (short)(src_y_base + inner_y), 0);
                src_val = *src_pixel;
                src_alpha = src_val >> 0x18;
                if (src_alpha != 0 || alpha_weighted == '\0') {
                  alpha_sum += (int)src_alpha;
                  ch1_sum += (int)((src_val >> 0x10) & 0xff);
                  ch2_sum += (int)((src_val >> 0x8) & 0xff);
                  ch3_sum += (int)(src_val & 0xff);
                  count++;
                }
                inner_x++;
              } while ((short)inner_x < (short)kernel_x);
            }
            inner_y++;
          } while ((short)inner_y < (short)kernel_y);
          if (count == 0)
            goto store_zero;
          half = count / 2;
          alpha_final = (half + alpha_sum) / count + (int)brightness_adjust;
          if (alpha_final < 0)
            alpha_final = 0;
          else if (alpha_final > 0xff)
            alpha_final = 0xff;
          *dst_pixel =
            (unsigned int)((((ch1_sum + half) / count | alpha_final << 8) << 8 |
                            (half + ch2_sum) / count)
                             << 8 |
                           (ch3_sum + half) / count);
          goto skip_zero;
        store_zero:
          *dst_pixel = 0;
        skip_zero:
          dst_x++;
          src_x_base += kernel_x;
        } while ((short)dst_x < (short)new_width);
      }
      dst_y++;
      src_y_base += kernel_y;
      if ((short)dst_y >= (short)new_height)
        return dst_bitmap;
    } while (1);
  }
  return dst_bitmap;
}

/*
 * FUN_000779b0 -- box-filter downscale for a 3D (volume) ARGB bitmap.
 *
 * Allocates a new ARGB (format 0xb) 3D bitmap at
 * (width/scale)x(height/scale)x(depth/scale), averaging scale×scale×scale
 * source voxel blocks per output voxel. Only includes non-transparent voxels in
 * the average when alpha_weighted != 0. brightness_adjust is added to the
 * computed alpha channel value.
 * @<eax> = scale: box filter kernel size (must be >= 2).
 */
void *FUN_000779b0(short scale /* @<eax> */, void *source_bitmap,
                   short brightness_adjust, char alpha_weighted)
{
  unsigned short src_width;
  unsigned short src_height;
  unsigned short src_depth;
  int kernel_x;
  int kernel_y;
  int kernel_z;
  int new_width;
  int new_height;
  int new_depth;
  void *dst_bitmap;
  short dst_z;
  short dst_y;
  short dst_x;
  int src_z_base;
  int src_y_base;
  int src_x_base;
  int inner_z;
  int inner_y;
  int inner_x;
  unsigned int *dst_pixel;
  unsigned int *src_pixel;
  unsigned int src_val;
  unsigned int src_alpha;
  int alpha_sum;
  int ch1_sum;
  int ch2_sum;
  int ch3_sum;
  int count;
  int half;
  int alpha_final;
  short new_width_s;
  short new_height_s;
  short new_depth_s;

  if (!bitmap_verify(source_bitmap, 1)) {
    display_assert("bitmap_verify(source_bitmap, TRUE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x15d, 1);
    system_exit(-1);
  }
  if (*(short *)((char *)source_bitmap + 0xa) != 1) {
    display_assert("source_bitmap->type==_bitmap_type_3d",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x15e, 1);
    system_exit(-1);
  }
  if (scale < 2) {
    display_assert("scale>1", "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c",
                   0x15f, 1);
    system_exit(-1);
  }

  src_width = *(unsigned short *)((char *)source_bitmap + 4);
  kernel_x = (int)(unsigned short)src_width;
  if (scale <= (short)src_width)
    kernel_x = (int)scale;

  src_height = *(unsigned short *)((char *)source_bitmap + 6);
  kernel_y = (int)(unsigned short)src_height;
  if (scale <= (short)src_height)
    kernel_y = (int)scale;

  src_depth = *(unsigned short *)((char *)source_bitmap + 8);
  kernel_z = (int)(unsigned short)src_depth;
  if (scale <= (short)src_depth)
    kernel_z = (int)scale;

  new_width = (int)(short)src_width / (int)(short)kernel_x;
  new_height = (int)(short)src_height / (int)(short)kernel_y;
  new_depth = (int)(short)src_depth / (int)(short)kernel_z;

  dst_bitmap =
    bitmap_3d_new((unsigned short)new_width, (unsigned short)new_height,
                  (unsigned short)new_depth, 0, 0xb);

  if (dst_bitmap == 0 || *(int *)((char *)dst_bitmap + 0x2c) == 0) {
    error(2, "### ERROR failed to allocate temporary bitmap");
    return dst_bitmap;
  }

  new_depth_s = (short)new_depth;
  dst_z = 0;
  if (new_depth_s > 0) {
    src_z_base = 0;
    do {
      new_height_s = (short)new_height;
      dst_y = 0;
      if (new_height_s > 0) {
        src_y_base = 0;
        do {
          new_width_s = (short)new_width;
          dst_x = 0;
          if (new_width_s > 0) {
            src_x_base = 0;
            do {
              alpha_sum = 0;
              ch1_sum = 0;
              ch2_sum = 0;
              ch3_sum = 0;
              count = 0;
              dst_pixel = (unsigned int *)bitmap_3d_address(
                dst_bitmap, dst_x, dst_y, (short)dst_z, 0);
              inner_z = 0;
              if ((short)kernel_z < 1)
                goto store_zero_3d;
              do {
                inner_y = 0;
                if ((short)kernel_y > 0) {
                  do {
                    inner_x = 0;
                    if ((short)kernel_x > 0) {
                      do {
                        src_pixel = (unsigned int *)bitmap_3d_address(
                          source_bitmap, (short)(src_x_base + inner_x),
                          (short)(src_y_base + inner_y),
                          (short)(src_z_base + inner_z), 0);
                        src_val = *src_pixel;
                        src_alpha = src_val >> 0x18;
                        if (src_alpha != 0 || alpha_weighted == '\0') {
                          alpha_sum += (int)src_alpha;
                          ch1_sum += (int)((src_val >> 0x10) & 0xff);
                          ch2_sum += (int)((src_val >> 0x8) & 0xff);
                          ch3_sum += (int)(src_val & 0xff);
                          count++;
                        }
                        inner_x++;
                      } while ((short)inner_x < (short)kernel_x);
                    }
                    inner_y++;
                  } while ((short)inner_y < (short)kernel_y);
                }
                inner_z++;
              } while ((short)inner_z < (short)kernel_z);
              if (count == 0)
                goto store_zero_3d;
              half = count / 2;
              alpha_final = (alpha_sum + half) / count + (int)brightness_adjust;
              if (alpha_final < 0)
                alpha_final = 0;
              else if (alpha_final > 0xff)
                alpha_final = 0xff;
              *dst_pixel =
                (unsigned int)((((ch1_sum + half) / count | alpha_final << 8)
                                  << 8 |
                                (half + ch2_sum) / count)
                                 << 8 |
                               (ch3_sum + half) / count);
              goto skip_zero_3d;
            store_zero_3d:
              *dst_pixel = 0;
            skip_zero_3d:
              dst_x++;
              src_x_base += kernel_x;
            } while ((short)dst_x < new_width_s);
          }
          dst_y++;
          src_y_base += kernel_y;
        } while ((short)dst_y < new_height_s);
      }
      src_z_base += kernel_z;
      dst_z++;
      if ((short)dst_z >= new_depth_s)
        return dst_bitmap;
    } while (1);
  }
  return dst_bitmap;
}

/*
 * FUN_00077cd0 -- box-filter downscale for a cube-map bitmap.
 *
 * Allocates a new ARGB (format 0xb) cube map whose faces are
 * (width / min(width, scale)) on a side, then walks the six faces: each source
 * face is extracted into a temporary 2D bitmap (FUN_0007ea60), downscaled with
 * the 2D box filter (FUN_00077720) and inserted into the new cube map. The
 * per-face scaled bitmap is released every iteration; the temporary extraction
 * bitmap is released after the loop.
 *
 * brightness_adjust / alpha_weighted are forwarded verbatim to the 2D
 * downscaler. Returns the new cube map, which may be NULL or dataless when
 * allocation failed.
 */
void *FUN_00077cd0(void *source_bitmap, short scale, int brightness_adjust,
                   int alpha_weighted)
{
  short src_width;
  short kernel;
  void *dst_bitmap;
  void *temp_2d;
  void *scaled_face;
  short face;

  if (!bitmap_verify(source_bitmap, 1)) {
    display_assert("bitmap_verify(source_bitmap, TRUE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x1bb, 1);
    system_exit(-1);
  }
  if (*(short *)((char *)source_bitmap + 0xa) != 2) {
    display_assert("source_bitmap->type==_bitmap_type_cube_map",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x1bc, 1);
    system_exit(-1);
  }
  if (scale < 2) {
    display_assert("scale>1", "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c",
                   0x1bd, 1);
    system_exit(-1);
  }

  /* Signed 16-bit clamp then a signed divide of the sign-extended shorts. */
  src_width = *(short *)((char *)source_bitmap + 4);
  kernel = src_width;
  if (scale <= src_width)
    kernel = scale;

  dst_bitmap = bitmap_cube_map_new(src_width / kernel, 0, 0xb);

  if (dst_bitmap == 0 || *(int *)((char *)dst_bitmap + 0x2c) == 0) {
    error(2, "### ERROR failed to allocate temporary bitmap");
    return dst_bitmap;
  }

  /* The extraction scratch is allocated from the zero-extended face size. */
  temp_2d =
    bitmap_2d_new(*(unsigned short *)((char *)source_bitmap + 4),
                  *(unsigned short *)((char *)source_bitmap + 6), 0, 0xb);

  if (temp_2d == 0 || *(int *)((char *)temp_2d + 0x2c) == 0) {
    error(2, "### ERROR failed to allocate temporary bitmap");
    bitmap_delete(temp_2d);
    return dst_bitmap;
  }

  for (face = 0; face < 6; face++) {
    FUN_0007ea60(source_bitmap, 0, face, temp_2d);
    scaled_face =
      FUN_00077720(scale, temp_2d, brightness_adjust, alpha_weighted);
    if (scaled_face != 0 && *(int *)((char *)scaled_face + 0x2c) != 0) {
      bitmap_cube_map_face_insert(scaled_face, dst_bitmap, 0, face);
    }
    bitmap_delete(scaled_face);
  }

  bitmap_delete(temp_2d);
  return dst_bitmap;
}

extern double floor(double);

/*
 * bitmap_fade (0x77e60) — Blend every pixel of a bitmap toward a solid color.
 *
 * fade_amount is a [0,1] intensity. If <= 0.0, the function is a no-op.
 * Each pixel channel is blended:
 *   result_n = (pixel_n * inv_alpha + color_n * alpha + 0x7f) >> 8
 * where alpha = (int)floor(clamped * 256.0f + 0.5f), inv_alpha = 256 - alpha.
 * Color channels: b0=blue(7:0), b1=green(15:8), b2=red(23:16), b3=alpha(31:24).
 *
 * Source: bitmap_utilities.obj, assert line 0x1f5 (501).
 */
void bitmap_fade(void *bitmap, unsigned int color, float fade_amount)
{
  float clamped;
  int alpha;
  int inv_alpha;
  int cb0, cb1, cb2, cb3;
  unsigned int *pixels;
  int count;
  int i;
  unsigned int pix;
  unsigned int r, g, b, a;

  /* bitmap_verify(bitmap, TRUE) */
  if (!bitmap_verify(bitmap, 1)) {
    display_assert("bitmap_verify(bitmap, TRUE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x1f5, 1);
    system_exit(-1);
  }

  if (fade_amount > 0.0f) {
    /* Clamp to [0, 1] using a local variable so MSVC emits all 3 fcomps */
    clamped = fade_amount;
    if (clamped < 0.0f) {
      clamped = 0.0f;
    } else if (clamped > 1.0f) {
      clamped = 1.0f;
    }

    /* alpha factor in [0, 256]; round-to-nearest via floor(x + 0.5) */
    alpha = (int)floor(clamped * 256.0f + 0.5f);
    inv_alpha = 0x100 - alpha;

    /* Pre-compute color_channel * alpha once before the pixel loop */
    cb2 = (int)((color >> 16) & 0xff) * alpha; /* red   */
    cb3 = (int)(color >> 24) * alpha; /* alpha */
    cb1 = (int)((color >> 8) & 0xff) * alpha; /* green */
    cb0 = (int)(color & 0xff) * alpha; /* blue  */

    pixels = (unsigned int *)bitmap_mipmap_address(bitmap, 0);
    count = bitmap_get_pixel_count(bitmap);

    for (i = 0; i < count; i++) {
      pix = pixels[i];
      r = ((pix >> 16) & 0xff) * inv_alpha;
      a = (pix >> 24) * inv_alpha;
      g = ((pix >> 8) & 0xff) * inv_alpha;
      b = (pix & 0xff) * inv_alpha;

      r = (r + cb2 + 0x7f) >> 8;
      a = (a + cb3 + 0x7f) >> 8;
      g = (g + cb1 + 0x7f) >> 8;
      b = (b + cb0 + 0x7f) >> 8;

      pixels[i] = (a << 24) | (r << 16) | (g << 8) | b;
    }
  }
}

/*
 * FUN_00077ff0 -- 2D bitmap separable Gaussian filter.
 * Horizontal pass (pixels->tmp) then vertical pass (tmp->pixels).
 * Circular boundary wrapping. filter_coefficients: 2*filter_radius+1 entries.
 */
void FUN_00077ff0(void *bitmap, short filter_radius, short *filter_coefficients)
{
  unsigned int pix_size;
  void *pixels;
  void *tmp;
  short y, x;
  short k;
  short width;
  unsigned char shift;
  int rounding;
  int row_base;
  int wrap_x;
  int y_wrap;
  unsigned int count;
  short *kptr;
  unsigned int pix;
  int coeff;
  int a, r, g, b;

  if (!bitmap_verify(bitmap, 1)) {
    display_assert("bitmap_verify(bitmap, TRUE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x250, 1);
    system_exit(-1);
  }
  if (*(short *)((char *)bitmap + 10) != 0) {
    display_assert("bitmap->type==_bitmap_type_2d",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x251, 1);
    system_exit(-1);
  }
  if (!filter_coefficients) {
    display_assert("filter_coefficients",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x252, 1);
    system_exit(-1);
  }
  if ((filter_radius <= *(short *)((char *)bitmap + 4)) &&
      (filter_radius <= *(short *)((char *)bitmap + 6))) {
    pix_size = (unsigned int)bitmap_get_pixel_data_size(bitmap);
    pixels = bitmap_mipmap_address(bitmap, 0);
    tmp = debug_malloc(pix_size, 0,
                       "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x25d);
    if (!tmp) {
      error(2, "### ERROR failed to allocate temporary buffer");
      return;
    }
    /* horizontal pass: pixels -> tmp */
    y = 0;
    if (0 < *(short *)((char *)bitmap + 6)) {
      width = *(short *)((char *)bitmap + 4);
      do {
        x = 0;
        if (0 < width) {
          shift = (unsigned char)filter_radius * 2;
          k = -filter_radius;
          rounding = 1 << (shift - 1);
          do {
            a = 0;
            r = 0;
            g = 0;
            b = 0;
            if (k <= filter_radius) {
              row_base = (int)y * (int)width;
              wrap_x = (int)x + (int)k + (int)width;
              count = (unsigned int)(unsigned short)(filter_radius - k + 1);
              kptr = filter_coefficients + filter_radius + k;
              do {
                pix =
                  *(unsigned int *)((char *)pixels +
                                    ((short)(wrap_x % (int)width) + row_base) *
                                      4);
                coeff = (int)*kptr;
                a += (pix >> 24) * coeff;
                r += ((pix >> 16) & 0xff) * coeff;
                g += ((pix >> 8) & 0xff) * coeff;
                b += (pix & 0xff) * coeff;
                wrap_x++;
                count--;
                kptr++;
                width = *(short *)((char *)bitmap + 4);
              } while (count != 0);
            }
            *(unsigned int *)((char *)tmp +
                              ((int)width * (int)y + (int)x) * 4) =
              ((((rounding + a) >> shift) << 8 | (r + rounding) >> shift) << 8 |
               (rounding + g) >> shift)
                << 8 |
              (rounding + b) >> shift;
            width = *(short *)((char *)bitmap + 4);
            x++;
          } while (x < width);
        }
        y++;
      } while (y < *(short *)((char *)bitmap + 6));
    }
    /* vertical pass: tmp -> pixels */
    y = 0;
    if (0 < *(short *)((char *)bitmap + 6)) {
      width = *(short *)((char *)bitmap + 4);
      do {
        x = 0;
        if (0 < width) {
          shift = (unsigned char)filter_radius * 2;
          k = -filter_radius;
          rounding = 1 << (shift - 1);
          do {
            a = 0;
            r = 0;
            g = 0;
            b = 0;
            if (k <= filter_radius) {
              y_wrap = (int)k + (int)*(short *)((char *)bitmap + 6) + (int)y;
              count = (unsigned int)(unsigned short)(filter_radius - k + 1);
              kptr = filter_coefficients + filter_radius + k;
              do {
                pix =
                  *(unsigned int *)((char *)tmp +
                                    ((short)(y_wrap %
                                             (int)*(short *)((char *)bitmap +
                                                             6)) *
                                       (int)width +
                                     (int)x) *
                                      4);
                coeff = (int)*kptr;
                a += (pix >> 24) * coeff;
                r += ((pix >> 16) & 0xff) * coeff;
                g += ((pix >> 8) & 0xff) * coeff;
                b += (pix & 0xff) * coeff;
                y_wrap++;
                count--;
                kptr++;
              } while (count != 0);
            }
            *(unsigned int *)((char *)pixels +
                              ((int)width * (int)y + (int)x) * 4) =
              ((((rounding + a) >> shift) << 8 | (rounding + r) >> shift) << 8 |
               (rounding + g) >> shift)
                << 8 |
              (rounding + b) >> shift;
            width = *(short *)((char *)bitmap + 4);
            x++;
          } while (x < width);
        }
        y++;
      } while (y < *(short *)((char *)bitmap + 6));
    }
    debug_free(tmp, "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x2a5);
    return;
  }
  crt_fprintf(
    (void *)0x331050,
    "### WARNING tried to smooth a bitmap with a filter which is too large\n");
  crt_fflush((void *)0x331050);
}

/*
 * FUN_00078460 -- 3D bitmap separable Gaussian filter.
 * X-pass (pixels->tmp), Y-pass (tmp->pixels), Z-pass (pixels->tmp),
 * then csmemcpy(pixels, tmp). Circular boundary wrapping.
 */
void FUN_00078460(void *bitmap, short filter_radius, short *filter_coefficients)
{
  char *bmp;
  unsigned int pix_size;
  void *pixels;
  void *tmp;
  short z, y, x;
  short k;
  short width;
  unsigned char shift;
  int rounding;
  int wrap;
  int row_base;
  unsigned int count;
  short *kptr;
  unsigned int pix;
  int coeff;
  int a, r, g, b;

  bmp = (char *)bitmap;
  if (!bitmap_verify(bitmap, 1)) {
    display_assert("bitmap_verify(bitmap, TRUE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x2ba, 1);
    system_exit(-1);
  }
  if (*(short *)(bmp + 10) != 1) {
    display_assert("bitmap->type==_bitmap_type_3d",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x2bb, 1);
    system_exit(-1);
  }
  if (!filter_coefficients) {
    display_assert("filter_coefficients",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x2bc, 1);
    system_exit(-1);
  }
  if ((filter_radius <= *(short *)(bmp + 4)) &&
      (filter_radius <= *(short *)(bmp + 6)) &&
      (filter_radius <= *(short *)(bmp + 8))) {
    pix_size = (unsigned int)bitmap_get_pixel_data_size(bitmap);
    pixels = bitmap_mipmap_address(bitmap, 0);
    tmp = debug_malloc(pix_size, 0,
                       "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x2c7);
    if (!tmp) {
      error(2, "### ERROR failed to allocate temporary buffer");
      return;
    }
    /* X-pass: pixels -> tmp */
    z = 0;
    if (0 < *(short *)(bmp + 8)) {
      do {
        y = 0;
        if (0 < *(short *)(bmp + 6)) {
          width = *(short *)(bmp + 4);
          do {
            x = 0;
            if (0 < width) {
              shift = (unsigned char)filter_radius * 2;
              k = -filter_radius;
              rounding = 1 << (shift - 1);
              do {
                a = 0;
                r = 0;
                g = 0;
                b = 0;
                if (k <= filter_radius) {
                  row_base = (int)*(short *)(bmp + 6) * (int)z + (int)y;
                  wrap = (int)x + (int)k + (int)width;
                  count = (unsigned int)(unsigned short)(filter_radius - k + 1);
                  kptr = filter_coefficients + filter_radius + k;
                  do {
                    pix = *(unsigned int *)((char *)pixels +
                                            ((short)(wrap % (int)width) +
                                             row_base * (int)width) *
                                              4);
                    coeff = (int)*kptr;
                    a += (pix >> 24) * coeff;
                    r += ((pix >> 16) & 0xff) * coeff;
                    g += ((pix >> 8) & 0xff) * coeff;
                    b += (pix & 0xff) * coeff;
                    wrap++;
                    count--;
                    kptr++;
                    width = *(short *)(bmp + 4);
                  } while (count != 0);
                }
                *(unsigned int *)((char *)tmp +
                                  (((int)*(short *)(bmp + 6) * (int)z +
                                    (int)y) *
                                     (int)width +
                                   (int)x) *
                                    4) =
                  ((((rounding + a) >> shift) << 8 | (rounding + r) >> shift)
                     << 8 |
                   (rounding + g) >> shift)
                    << 8 |
                  (rounding + b) >> shift;
                width = *(short *)(bmp + 4);
                x++;
              } while (x < width);
            }
            y++;
          } while (y < *(short *)(bmp + 6));
        }
        z++;
      } while (z < *(short *)(bmp + 8));
    }
    /* Y-pass: tmp -> pixels */
    z = 0;
    if (0 < *(short *)(bmp + 8)) {
      do {
        y = 0;
        if (0 < *(short *)(bmp + 6)) {
          width = *(short *)(bmp + 4);
          do {
            x = 0;
            if (0 < width) {
              shift = (unsigned char)filter_radius * 2;
              k = -filter_radius;
              rounding = 1 << (shift - 1);
              do {
                a = 0;
                r = 0;
                g = 0;
                b = 0;
                if (k <= filter_radius) {
                  wrap = (int)y + (int)k + (int)*(short *)(bmp + 6);
                  count = (unsigned int)(unsigned short)(filter_radius - k + 1);
                  kptr = filter_coefficients + filter_radius + k;
                  do {
                    pix =
                      *(unsigned int *)((char *)tmp +
                                        (((short)(wrap %
                                                  (int)*(short *)(bmp + 6)) +
                                          (int)z * (int)*(short *)(bmp + 6)) *
                                           (int)width +
                                         (int)x) *
                                          4);
                    coeff = (int)*kptr;
                    a += (pix >> 24) * coeff;
                    r += ((pix >> 16) & 0xff) * coeff;
                    g += ((pix >> 8) & 0xff) * coeff;
                    b += (pix & 0xff) * coeff;
                    kptr++;
                    wrap++;
                    count--;
                  } while (count != 0);
                }
                *(unsigned int *)((char *)pixels +
                                  (((int)*(short *)(bmp + 6) * (int)z +
                                    (int)y) *
                                     (int)width +
                                   (int)x) *
                                    4) =
                  ((((rounding + a) >> shift) << 8 | (rounding + r) >> shift)
                     << 8 |
                   (rounding + g) >> shift)
                    << 8 |
                  (rounding + b) >> shift;
                width = *(short *)(bmp + 4);
                x++;
              } while (x < width);
            }
            y++;
          } while (y < *(short *)(bmp + 6));
        }
        z++;
      } while (z < *(short *)(bmp + 8));
    }
    /* Z-pass: pixels -> tmp */
    z = 0;
    if (0 < *(short *)(bmp + 8)) {
      do {
        y = 0;
        if (0 < *(short *)(bmp + 6)) {
          width = *(short *)(bmp + 4);
          do {
            x = 0;
            if (0 < width) {
              shift = (unsigned char)filter_radius * 2;
              k = -filter_radius;
              rounding = 1 << (shift - 1);
              do {
                a = 0;
                r = 0;
                g = 0;
                b = 0;
                if (k <= filter_radius) {
                  wrap = (int)*(short *)(bmp + 8) + (int)k + (int)z;
                  count = (unsigned int)(unsigned short)(filter_radius - k + 1);
                  kptr = filter_coefficients + filter_radius + k;
                  do {
                    pix =
                      *(unsigned int *)((char *)pixels +
                                        (((short)(wrap %
                                                  (int)*(short *)(bmp + 8)) *
                                            (int)*(short *)(bmp + 6) +
                                          (int)y) *
                                           (int)width +
                                         (int)x) *
                                          4);
                    coeff = (int)*kptr;
                    a += (pix >> 24) * coeff;
                    r += ((pix >> 16) & 0xff) * coeff;
                    g += ((pix >> 8) & 0xff) * coeff;
                    b += (pix & 0xff) * coeff;
                    kptr++;
                    wrap++;
                    count--;
                  } while (count != 0);
                }
                *(unsigned int *)((char *)tmp +
                                  (((int)*(short *)(bmp + 6) * (int)z +
                                    (int)y) *
                                     (int)width +
                                   (int)x) *
                                    4) =
                  ((((rounding + a) >> shift) << 8 | (rounding + r) >> shift)
                     << 8 |
                   (rounding + g) >> shift)
                    << 8 |
                  (rounding + b) >> shift;
                width = *(short *)(bmp + 4);
                x++;
              } while (x < width);
            }
            y++;
          } while (y < *(short *)(bmp + 6));
        }
        z++;
      } while (z < *(short *)(bmp + 8));
    }
    csmemcpy(pixels, tmp, pix_size);
    debug_free(tmp, "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x33e);
    return;
  }
  crt_fprintf(
    (void *)0x331050,
    "### WARNING tried to smooth a bitmap with a filter which is too large\n");
  crt_fflush((void *)0x331050);
}

/*
 * FUN_00078b80 -- cube_map smooth stub.
 *
 * Validates the bitmap (must be cube_map type) and the filter_coefficients
 * pointer, then prints a warning that smoothing a cube map is not supported
 * and returns without doing any work.
 *
 * ABI: bitmap passed in ESI (@ESI). Two stack params: filter_radius,
 * filter_coefficients.
 */
void FUN_00078b80(int filter_radius, short *filter_coefficients,
                  void *bitmap /* @<esi> */)
{
  /* bitmap_verify(bitmap, TRUE) */
  if (!bitmap_verify(bitmap, 1)) {
    display_assert("bitmap_verify(bitmap, TRUE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x353, 1);
    system_exit(-1);
  }

  /* assert bitmap->type == _bitmap_type_cube_map */
  if (*(short *)((char *)bitmap + 0xa) != 2) {
    display_assert("bitmap->type==_bitmap_type_cube_map",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x354, 1);
    system_exit(-1);
  }

  /* assert filter_coefficients != NULL */
  if (filter_coefficients == 0) {
    display_assert("filter_coefficients",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x355, 1);
    system_exit(-1);
  }

  crt_fprintf((void *)0x331050, "### WARNING tried to smooth a cube map",
              (void *)0x261f2c);
  crt_fflush((void *)0x331050);
}

/*
 * FUN_000790b0 -- 3D bitmap sharpen stub.
 *
 * Validates the bitmap (must be 3D type) and positive/negative table pointers,
 * then prints a warning that sharpening a 3D bitmap is not supported
 * and returns without doing any work.
 *
 * ABI: bitmap passed in ESI (@ESI). Three stack params: unused, positive_table,
 * negative_table.
 */
/* The original CALLs this from bitmap_sharpen (0x7b43b) rather than inlining
 * it; MSVC 7.1 otherwise expands the whole assert/warning body into its only
 * in-TU caller, bloating bitmap_sharpen by ~25 instructions. */
#if defined(_MSC_VER) && !defined(__clang__)
#pragma auto_inline(off)
#endif
void FUN_000790b0(int unused, int positive_table, int negative_table,
                  void *bitmap /* @<esi> */)
{
  /* bitmap_verify(bitmap, TRUE) */
  if (!bitmap_verify(bitmap, 1)) {
    display_assert("bitmap_verify(bitmap, TRUE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x3e2, 1);
    system_exit(-1);
  }

  /* assert bitmap->type == _bitmap_type_3d */
  if (*(short *)((char *)bitmap + 0xa) != 1) {
    display_assert("bitmap->type==_bitmap_type_3d",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x3e3, 1);
    system_exit(-1);
  }

  /* assert positive_table != NULL */
  if (positive_table == 0) {
    display_assert("positive_table",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x3e4, 1);
    system_exit(-1);
  }

  /* assert negative_table != NULL */
  if (negative_table == 0) {
    display_assert("negative_table",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x3e5, 1);
    system_exit(-1);
  }

  crt_fprintf((void *)0x331050, "### WARNING tried to sharpen a 3d bitmap",
              (void *)0x261f2c);
  crt_fflush((void *)0x331050);
}
#if defined(_MSC_VER) && !defined(__clang__)
#pragma auto_inline(on)
#endif

/* FUN_00079180 (0x79180) — cube map sharpen stub. Validates bitmap (@esi) is
 * cube type, checks positive/negative table pointers, then prints warning and
 * returns. */
/* Same reason as FUN_000790b0 above: the original CALLs this from
 * bitmap_sharpen (0x7b41e); MSVC 7.1 would otherwise inline it. */
#if defined(_MSC_VER) && !defined(__clang__)
#pragma auto_inline(off)
#endif
void FUN_00079180(int unused, int positive_table, int negative_table,
                  void *bitmap /* @<esi> */)
{
  if (!bitmap_verify(bitmap, 1)) {
    display_assert("bitmap_verify(bitmap, TRUE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x3f3, 1);
    system_exit(-1);
  }
  if (*(short *)((char *)bitmap + 0xa) != 2) {
    display_assert("bitmap->type==_bitmap_type_cube_map",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x3f4, 1);
    system_exit(-1);
  }
  if (positive_table == 0) {
    display_assert("positive_table",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x3f5, 1);
    system_exit(-1);
  }
  if (negative_table == 0) {
    display_assert("negative_table",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x3f6, 1);
    system_exit(-1);
  }
  crt_fprintf((void *)0x331050, "### WARNING tried to sharpen a cube map",
              (void *)0x261f2c);
  crt_fflush((void *)0x331050);
}
#if defined(_MSC_VER) && !defined(__clang__)
#pragma auto_inline(on)
#endif

/* FUN_00079250 (0x79250) — 2D bitmap alpha-bleed: for each transparent pixel
 * (alpha==0), copies RGB from the first non-transparent neighbor found in the
 * 3x3 neighborhood. Runs `passes` iterations over the whole bitmap, writing
 * each pass into a temp buffer then memcpy'ing back.
 * @<eax> = passes (must be > 0). */
void FUN_00079250(short passes /* @<eax> */, void *bitmap)
{
  short width;
  short height;
  int size;
  unsigned int *temp_buf;
  unsigned int pass_counter;
  int y;
  int x;
  unsigned int *row_ptr;
  unsigned int pixel;
  void *dest;
  int dy;
  int iy;
  int dx;
  int ix;
  unsigned int *neighbor_ptr;
  char found;

  if (!bitmap_verify(bitmap, 1)) {
    display_assert("bitmap_verify(bitmap, TRUE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x41d, 1);
    system_exit(-1);
  }
  if (*(short *)((char *)bitmap + 0xa) != 0) {
    display_assert("bitmap->type==_bitmap_type_2d",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x41e, 1);
    system_exit(-1);
  }
  if (passes <= 0) {
    display_assert("passes>0", "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c",
                   0x41f, 1);
    system_exit(-1);
  }
  size = bitmap_get_pixel_data_size(bitmap);
  temp_buf = (unsigned int *)debug_malloc(
    size, 0, "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x422);
  if (temp_buf == NULL) {
    error(2, "### ERROR failed to allocate temporary buffer");
    return;
  }
  pass_counter = (unsigned int)(unsigned short)passes;
  if ((short)pass_counter > 0) {
    do {
      width = *(short *)((char *)bitmap + 4);
      height = *(short *)((char *)bitmap + 6);
      y = 0;
      if (height > 0) {
        do {
          row_ptr = (unsigned int *)bitmap_2d_address(bitmap, 0, y, 0);
          x = 0;
          if (width > 0) {
            do {
              pixel = *(unsigned int *)((unsigned char *)row_ptr + x * 4);
              if ((pixel & 0xff000000) == 0) {
                found = 0;
                dy = -1;
                iy = y - 1;
                do {
                  if (dy > 1)
                    break;
                  if (!found) {
                    dx = -1;
                    ix = x - 1;
                    do {
                      if (dx > 1)
                        break;
                      if ((short)ix >= 0 && (short)iy >= 0 &&
                          (short)ix < width && (short)iy < height) {
                        neighbor_ptr =
                          (unsigned int *)bitmap_2d_address(bitmap, ix, iy, 0);
                        if (*neighbor_ptr != 0) {
                          pixel = *neighbor_ptr & 0x00ffffff;
                          found = 1;
                        }
                      }
                      dx++;
                      ix++;
                    } while (!found);
                  }
                  dy++;
                  iy++;
                } while (!found);
              }
              *(unsigned int *)((unsigned char *)temp_buf + x * 4 +
                                (int)width * y * 4) = pixel;
              x++;
            } while ((short)x < width);
          }
          y++;
        } while ((short)y < height);
      }
      dest = bitmap_mipmap_address(bitmap, 0);
      csmemcpy(dest, temp_buf, size);
      pass_counter--;
    } while (pass_counter != 0);
  }
  debug_free(temp_buf, "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x462);
}

/*
 * FUN_00079480 -- 3D bitmap alpha_bleed.
 *
 * Allocates one temporary 2D bitmap matching the 3D bitmap's width/height and
 * format, then walks every depth slice: read the slice into the temp
 * (bitmap_3d_slice_insert), run the 2D alpha-bleed over it (FUN_00079250),
 * write the temp back into the slice (bitmap_cube_map_face_extract).
 *
 * ABI: bitmap passed in EDI (@EDI). One stack param: passes (short).
 */
void FUN_00079480(short passes, void *bitmap /* @<edi> */)
{
  void *temp;
  short slice;

  /* bitmap_verify(bitmap, TRUE) */
  if (!bitmap_verify(bitmap, 1)) {
    display_assert("bitmap_verify(bitmap, TRUE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x472, 1);
    system_exit(-1);
  }

  /* assert bitmap->type == _bitmap_type_3d */
  if (*(short *)((char *)bitmap + 0xa) != 1) {
    display_assert("bitmap->type==_bitmap_type_3d",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x473, 1);
    system_exit(-1);
  }

  /* assert passes > 0 */
  if (passes <= 0) {
    display_assert("passes>0", "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c",
                   0x474, 1);
    system_exit(-1);
  }

  temp = bitmap_2d_new(*(unsigned short *)((char *)bitmap + 4),
                       *(unsigned short *)((char *)bitmap + 6), 0,
                       *(unsigned short *)((char *)bitmap + 0xc));

  if (temp != 0 && *(int *)((char *)temp + 0x2c) != 0) {
    for (slice = 0; slice < *(short *)((char *)bitmap + 8); slice++) {
      bitmap_3d_slice_insert(bitmap, 0, slice, temp);
      FUN_00079250(passes, temp);
      bitmap_cube_map_face_extract(temp, bitmap, 0, slice);
    }
  } else {
    error(2, "### ERROR failed to allocate temporary bitmap");
  }

  bitmap_delete(temp);
}

/*
 * FUN_00079590 -- cube_map alpha_bleed stub.
 *
 * Validates the bitmap (must be cube_map type) and that passes > 0,
 * then prints a warning that alpha-bleeding a cube map is not supported
 * and returns without doing any work.
 *
 * ABI: bitmap passed in ESI (@ESI). One stack param: passes (short).
 */
void FUN_00079590(short passes, void *bitmap /* @<esi> */)
{
  /* bitmap_verify(bitmap, TRUE) */
  if (!bitmap_verify(bitmap, 1)) {
    display_assert("bitmap_verify(bitmap, TRUE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x4a1, 1);
    system_exit(-1);
  }

  /* assert bitmap->type == _bitmap_type_cube_map */
  if (*(short *)((char *)bitmap + 0xa) != 2) {
    display_assert("bitmap->type==_bitmap_type_cube_map",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x4a2, 1);
    system_exit(-1);
  }

  /* assert passes > 0 */
  if (passes <= 0) {
    display_assert("passes>0", "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c",
                   0x4a3, 1);
    system_exit(-1);
  }

  crt_fprintf((void *)0x331050,
              "### WARNING tried to alpha-bleed a cube map (skipping)");
  crt_fflush((void *)0x331050);
}

/*
 * FUN_00079630 -- cube_map height_map stub.
 *
 * Validates the bitmap (must be cube_map type) and that bump_height > 0.0f,
 * then prints a warning that using a cube map as a height map is not supported
 * and returns without doing any work.
 *
 * ABI: bitmap passed in ESI (@ESI). One stack param: bump_height (float).
 */
void FUN_00079630(float bump_height, void *bitmap /* @<esi> */)
{
  /* bitmap_verify(bitmap, TRUE) */
  if (!bitmap_verify(bitmap, 1)) {
    display_assert("bitmap_verify(bitmap, TRUE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x55c, 1);
    system_exit(-1);
  }

  /* assert bitmap->type == _bitmap_type_cube_map */
  if (*(short *)((char *)bitmap + 0xa) != 2) {
    display_assert("bitmap->type==_bitmap_type_cube_map",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x55d, 1);
    system_exit(-1);
  }

  /* assert bump_height > 0.0f */
  if (!(bump_height > *(float *)0x2533c0)) {
    display_assert("bump_height>0.0f",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x55e, 1);
    system_exit(-1);
  }

  crt_fprintf((void *)0x331050,
              "### WARNING tried to use a cube map as a height map\r\n");
  crt_fflush((void *)0x331050);
}

/*
 * FUN_000796e0 -- unimplemented "compress source 2d bitmap into one mipmap
 * level of a compressed 2d bitmap" path.
 *
 * The body is nothing but the argument-validation preamble (assert lines
 * 0x63c-0x645) followed by an unconditional assert(FALSE) at line 0x69f and
 * system_exit(-1); the function never returns.  Bungie left the compression
 * path unwritten in this build.
 *
 * Source TU: c:\halo\SOURCE\bitmaps\bitmap_utilities.c (assert __FILE__ xref).
 *
 * ABI: no prologue at all -- the first instruction at 0x796e0 is `PUSH 0x1`.
 * source_bitmap in EDI, destination_bitmap in ESI, destination_mipmap_index in
 * BX (16-bit).  All three callees are plain cdecl.
 *
 * bitmap_data_t offsets used: +0x04 width (short), +0x06 height (short),
 * +0x08 depth (short), +0x0a type (short; 0 == _bitmap_type_2d), +0x0e flags
 * (byte; bit 1 == _bitmap_compressed_bit), +0x14 mipmap_count (short).
 */
void FUN_000796e0(void *source_bitmap /* @<edi> */,
                  void *destination_bitmap /* @<esi> */,
                  short destination_mipmap_index /* @<bx> */, int param_4)
{
  /* param_4 is the sole *stack* argument (one dword; the caller's shared
   * `ADD ESP,0x24` accounts for it).  It is never read on this path -- the
   * function asserts out before any use -- so it stays unnamed. */
  (void)param_4;

  /* bitmap_verify(source_bitmap, TRUE) */
  if (!bitmap_verify(source_bitmap, 1)) {
    display_assert("bitmap_verify(source_bitmap, TRUE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x63c, 1);
    system_exit(-1);
  }

  if (*(short *)((char *)source_bitmap + 0xa) != 0) {
    display_assert("source_bitmap->type==_bitmap_type_2d",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x63d, 1);
    system_exit(-1);
  }

  /* bitmap_verify(destination_bitmap, FALSE) */
  if (!bitmap_verify(destination_bitmap, 0)) {
    display_assert("bitmap_verify(destination_bitmap, FALSE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x63f, 1);
    system_exit(-1);
  }

  if (*(short *)((char *)destination_bitmap + 0xa) != 0) {
    display_assert("destination_bitmap->type==_bitmap_type_2d",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x640, 1);
    system_exit(-1);
  }

  if (destination_mipmap_index < 0 ||
      destination_mipmap_index >
        *(short *)((char *)destination_bitmap + 0x14)) {
    display_assert("destination_mipmap_index>=0 && "
                   "destination_mipmap_index<=destination_bitmap->mipmap_count",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x641, 1);
    system_exit(-1);
  }

  /* MAX(1, destination_bitmap->width >>destination_mipmap_index) */
  if ((1 > (*(short *)((char *)destination_bitmap + 4) >>
            destination_mipmap_index) ?
         1 :
         (*(short *)((char *)destination_bitmap + 4) >>
          destination_mipmap_index)) != *(short *)((char *)source_bitmap + 4)) {
    display_assert("MAX(1, destination_bitmap->width "
                   ">>destination_mipmap_index)==source_bitmap->width",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x642, 1);
    system_exit(-1);
  }

  /* MAX(1, destination_bitmap->height>>destination_mipmap_index) */
  if ((1 > (*(short *)((char *)destination_bitmap + 6) >>
            destination_mipmap_index) ?
         1 :
         (*(short *)((char *)destination_bitmap + 6) >>
          destination_mipmap_index)) != *(short *)((char *)source_bitmap + 6)) {
    display_assert("MAX(1, destination_bitmap->height"
                   ">>destination_mipmap_index)==source_bitmap->height",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x643, 1);
    system_exit(-1);
  }

  /* MAX(1, destination_bitmap->depth >>destination_mipmap_index) */
  if ((1 > (*(short *)((char *)destination_bitmap + 8) >>
            destination_mipmap_index) ?
         1 :
         (*(short *)((char *)destination_bitmap + 8) >>
          destination_mipmap_index)) != *(short *)((char *)source_bitmap + 8)) {
    display_assert("MAX(1, destination_bitmap->depth "
                   ">>destination_mipmap_index)==source_bitmap->depth",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x644, 1);
    system_exit(-1);
  }

  /* TEST_FLAG(destination_bitmap->flags, _bitmap_compressed_bit) */
  if ((*(unsigned char *)((char *)destination_bitmap + 0xe) & 2) == 0) {
    display_assert(
      "TEST_FLAG(destination_bitmap->flags, _bitmap_compressed_bit)",
      "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x645, 1);
    system_exit(-1);
  }

  /* assert(FALSE) -- compression path never implemented in this build. */
  display_assert(0, "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x69f, 1);
  system_exit(-1);
}

/*
 * FUN_000798e0 -- compress one mipmap level of an uncompressed 3D bitmap into
 * a compressed 3D bitmap.
 *
 * Mirror image of FUN_0007a1e0 (3D uncompress).  Allocates two temporary 2D
 * bitmaps sized from the *source* -- one in the source's format and one in the
 * destination's -- then for each depth slice: extracts that slice into the
 * first temp, compresses it into the second (FUN_000796e0, which asserts out
 * in this build), and writes the result back into the requested mipmap level
 * of the destination.
 *
 * The kb declaration was `void FUN_000798e0(void)`: Ghidra dropped all four
 * cdecl stack arguments (EBP+8 / +0xc / +0x10 / +0x14).  The fourth is read
 * exactly once (`MOV EDX,[EBP+0x14]; PUSH EDX`) and forwarded as
 * FUN_000796e0's sole *stack* argument; no assert names it, so it keeps a
 * mechanical name.  Arity of the three loop calls cannot be read off per-call
 * cleanup -- they share one `ADD ESP,0x24` (9 dwords = 4 + 1 + 4).  The
 * `XOR EBX,EBX` sitting between that PUSH and the CALL is FUN_000796e0's
 * `@<bx>` argument, not dead scheduling; EDI/ESI carry the two temporaries.
 *
 * bitmap_data_t offsets used: +0x04 width (short), +0x06 height (short),
 * +0x08 depth (short), +0x0a type (short; 1 == _bitmap_type_3d), +0x0c format
 * (unsigned short), +0x0e flags (byte; bit 1 == _bitmap_compressed_bit),
 * +0x14 mipmap_count (short), +0x2c pixel data.
 */
void FUN_000798e0(void *source_bitmap, void *destination_bitmap,
                  short destination_mipmap_index, int param_4)
{
  void *temp_source;
  void *temp_destination;
  short slice;

  /* bitmap_verify(source_bitmap, TRUE) */
  if (!bitmap_verify(source_bitmap, 1)) {
    display_assert("bitmap_verify(source_bitmap, TRUE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x6af, 1);
    system_exit(-1);
  }

  if (*(short *)((char *)source_bitmap + 0xa) != 1) {
    display_assert("source_bitmap->type==_bitmap_type_3d",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x6b0, 1);
    system_exit(-1);
  }

  /* bitmap_verify(destination_bitmap, FALSE) */
  if (!bitmap_verify(destination_bitmap, 0)) {
    display_assert("bitmap_verify(destination_bitmap, FALSE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x6b2, 1);
    system_exit(-1);
  }

  if (*(short *)((char *)destination_bitmap + 0xa) != 1) {
    display_assert("destination_bitmap->type==_bitmap_type_3d",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x6b3, 1);
    system_exit(-1);
  }

  if (destination_mipmap_index < 0 ||
      destination_mipmap_index >
        *(short *)((char *)destination_bitmap + 0x14)) {
    display_assert("destination_mipmap_index>=0 && destination_mipmap_index<="
                   "destination_bitmap->mipmap_count",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x6b4, 1);
    system_exit(-1);
  }

  /* MAX(1, destination_bitmap->width >>destination_mipmap_index) */
  if ((1 > (*(short *)((char *)destination_bitmap + 4) >>
            destination_mipmap_index) ?
         1 :
         (*(short *)((char *)destination_bitmap + 4) >>
          destination_mipmap_index)) != *(short *)((char *)source_bitmap + 4)) {
    display_assert("MAX(1, destination_bitmap->width "
                   ">>destination_mipmap_index)==source_bitmap->width",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x6b5, 1);
    system_exit(-1);
  }

  /* MAX(1, destination_bitmap->height>>destination_mipmap_index) */
  if ((1 > (*(short *)((char *)destination_bitmap + 6) >>
            destination_mipmap_index) ?
         1 :
         (*(short *)((char *)destination_bitmap + 6) >>
          destination_mipmap_index)) != *(short *)((char *)source_bitmap + 6)) {
    display_assert("MAX(1, destination_bitmap->height"
                   ">>destination_mipmap_index)==source_bitmap->height",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x6b6, 1);
    system_exit(-1);
  }

  /* MAX(1, destination_bitmap->depth >>destination_mipmap_index) */
  if ((1 > (*(short *)((char *)destination_bitmap + 8) >>
            destination_mipmap_index) ?
         1 :
         (*(short *)((char *)destination_bitmap + 8) >>
          destination_mipmap_index)) != *(short *)((char *)source_bitmap + 8)) {
    display_assert("MAX(1, destination_bitmap->depth "
                   ">>destination_mipmap_index)==source_bitmap->depth",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x6b7, 1);
    system_exit(-1);
  }

  /* TEST_FLAG(destination_bitmap->flags, _bitmap_compressed_bit) */
  if ((*(unsigned char *)((char *)destination_bitmap + 0xe) & 2) == 0) {
    display_assert(
      "TEST_FLAG(destination_bitmap->flags, _bitmap_compressed_bit)",
      "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x6b8, 1);
    system_exit(-1);
  }

  /* Both temporaries are sized from the source; only the format differs. */
  temp_source = bitmap_2d_new(*(unsigned short *)((char *)source_bitmap + 4),
                              *(unsigned short *)((char *)source_bitmap + 6), 0,
                              *(unsigned short *)((char *)source_bitmap + 0xc));
  temp_destination =
    bitmap_2d_new(*(unsigned short *)((char *)source_bitmap + 4),
                  *(unsigned short *)((char *)source_bitmap + 6), 0,
                  *(unsigned short *)((char *)destination_bitmap + 0xc));

  if (temp_source != 0 && *(int *)((char *)temp_source + 0x2c) != 0 &&
      temp_destination != 0 && *(int *)((char *)temp_destination + 0x2c) != 0) {
    for (slice = 0; slice < *(short *)((char *)source_bitmap + 8); slice++) {
      bitmap_3d_slice_insert(source_bitmap, 0, slice, temp_source);
      FUN_000796e0(temp_source, temp_destination, 0, param_4);
      bitmap_cube_map_face_extract(temp_destination, destination_bitmap,
                                   destination_mipmap_index, slice);
    }
  } else {
    error(2, "### ERROR failed to allocate temporary bitmap");
  }

  bitmap_delete(temp_source);
  bitmap_delete(temp_destination);
}

/*
 * FUN_00079bb0 -- compress one mipmap level of an uncompressed cube map into
 * a compressed cube map.
 *
 * Cube-map twin of FUN_000798e0 (3D compress): identical nine-assert preamble
 * and identical two-temporary allocation, but the per-slice loop becomes a
 * fixed six-face loop and the extract/insert pair swaps to the cube-map
 * helpers.  Both temporaries are sized from the *source*; only the format
 * differs (source format for the first, destination format for the second).
 *
 * The kb declaration was `void FUN_00079bb0(void)`: Ghidra dropped all four
 * cdecl stack arguments (EBP+8 / +0xc / +0x10 / +0x14).  The fourth appears
 * only in the disassembly (`MOV EDX,[EBP+0x14]; PUSH EDX` at 0x79e10) and is
 * forwarded as FUN_000796e0's sole *stack* argument; no assert names it, so it
 * keeps a mechanical name.  Arity of the three loop calls cannot be read off
 * per-call cleanup -- they share one `ADD ESP,0x24` (9 dwords = 4 + 1 + 4).
 * The `XOR EBX,EBX` between that PUSH and the CALL is FUN_000796e0's `@<bx>`
 * argument, not dead scheduling; EDI/ESI carry the two temporaries.
 *
 * The loop counter is compared 16-bit (`CMP BX,6`) against a literal 6, not
 * against a depth field -- a cube map always has six faces.  Both
 * bitmap_delete calls run on the allocation-failure path too: error() does not
 * return early, it falls through.
 *
 * bitmap_data_t offsets used: +0x04 width (short), +0x06 height (short),
 * +0x08 depth (short), +0x0a type (short; 2 == _bitmap_type_cube_map), +0x0c
 * format (unsigned short), +0x0e flags (byte; bit 1 ==
 * _bitmap_compressed_bit), +0x14 mipmap_count (short), +0x2c pixel data.
 */
void FUN_00079bb0(void *source_bitmap, void *destination_bitmap,
                  short destination_mipmap_index, int param_4)
{
  void *temp_source;
  void *temp_destination;
  short face;

  /* bitmap_verify(source_bitmap, TRUE) */
  if (!bitmap_verify(source_bitmap, 1)) {
    display_assert("bitmap_verify(source_bitmap, TRUE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x6fa, 1);
    system_exit(-1);
  }

  if (*(short *)((char *)source_bitmap + 0xa) != 2) {
    display_assert("source_bitmap->type==_bitmap_type_cube_map",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x6fb, 1);
    system_exit(-1);
  }

  /* bitmap_verify(destination_bitmap, FALSE) */
  if (!bitmap_verify(destination_bitmap, 0)) {
    display_assert("bitmap_verify(destination_bitmap, FALSE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x6fd, 1);
    system_exit(-1);
  }

  if (*(short *)((char *)destination_bitmap + 0xa) != 2) {
    display_assert("destination_bitmap->type==_bitmap_type_cube_map",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x6fe, 1);
    system_exit(-1);
  }

  if (destination_mipmap_index < 0 ||
      destination_mipmap_index >
        *(short *)((char *)destination_bitmap + 0x14)) {
    display_assert("destination_mipmap_index>=0 && destination_mipmap_index<="
                   "destination_bitmap->mipmap_count",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x6ff, 1);
    system_exit(-1);
  }

  /* MAX(1, destination_bitmap->width >>destination_mipmap_index) */
  if ((1 > (*(short *)((char *)destination_bitmap + 4) >>
            destination_mipmap_index) ?
         1 :
         (*(short *)((char *)destination_bitmap + 4) >>
          destination_mipmap_index)) != *(short *)((char *)source_bitmap + 4)) {
    display_assert("MAX(1, destination_bitmap->width "
                   ">>destination_mipmap_index)==source_bitmap->width",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x700, 1);
    system_exit(-1);
  }

  /* MAX(1, destination_bitmap->height>>destination_mipmap_index) */
  if ((1 > (*(short *)((char *)destination_bitmap + 6) >>
            destination_mipmap_index) ?
         1 :
         (*(short *)((char *)destination_bitmap + 6) >>
          destination_mipmap_index)) != *(short *)((char *)source_bitmap + 6)) {
    display_assert("MAX(1, destination_bitmap->height"
                   ">>destination_mipmap_index)==source_bitmap->height",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x701, 1);
    system_exit(-1);
  }

  /* MAX(1, destination_bitmap->depth >>destination_mipmap_index) */
  if ((1 > (*(short *)((char *)destination_bitmap + 8) >>
            destination_mipmap_index) ?
         1 :
         (*(short *)((char *)destination_bitmap + 8) >>
          destination_mipmap_index)) != *(short *)((char *)source_bitmap + 8)) {
    display_assert("MAX(1, destination_bitmap->depth "
                   ">>destination_mipmap_index)==source_bitmap->depth",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x702, 1);
    system_exit(-1);
  }

  /* TEST_FLAG(destination_bitmap->flags, _bitmap_compressed_bit) */
  if ((*(unsigned char *)((char *)destination_bitmap + 0xe) & 2) == 0) {
    display_assert(
      "TEST_FLAG(destination_bitmap->flags, _bitmap_compressed_bit)",
      "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x703, 1);
    system_exit(-1);
  }

  /* Both temporaries are sized from the source; only the format differs. */
  temp_source = bitmap_2d_new(*(unsigned short *)((char *)source_bitmap + 4),
                              *(unsigned short *)((char *)source_bitmap + 6), 0,
                              *(unsigned short *)((char *)source_bitmap + 0xc));
  temp_destination =
    bitmap_2d_new(*(unsigned short *)((char *)source_bitmap + 4),
                  *(unsigned short *)((char *)source_bitmap + 6), 0,
                  *(unsigned short *)((char *)destination_bitmap + 0xc));

  if (temp_source != 0 && *(int *)((char *)temp_source + 0x2c) != 0 &&
      temp_destination != 0 && *(int *)((char *)temp_destination + 0x2c) != 0) {
    for (face = 0; face < 6; face++) {
      FUN_0007ea60(source_bitmap, 0, face, temp_source);
      FUN_000796e0(temp_source, temp_destination, 0, param_4);
      bitmap_cube_map_face_insert(temp_destination, destination_bitmap,
                                  destination_mipmap_index, face);
    }
  } else {
    error(2, "### ERROR failed to allocate temporary bitmap");
  }

  bitmap_delete(temp_source);
  bitmap_delete(temp_destination);
}

/*
 * FUN_00079e70 -- uncompress one mipmap level of a compressed 2D bitmap into
 * an uncompressed 2D bitmap.
 *
 * This is the worker under FUN_0007a1e0 (3D) and
 * bitmap_2d_uncompress_from_mipmap (cube map): those wrappers loop slices or
 * faces and call this once per 2D level.
 *
 * Walks the source mipmap's compressed block stream in raster order.  Each
 * block decodes to a 4x4 tile of 32-bit pixels in a stack buffer, which is
 * then scattered into the destination bitmap one pixel at a time.  Blocks on
 * the right/bottom edge are clipped: pixels outside the destination extent are
 * dropped and the tile index does NOT advance for them, so a clipped block
 * consumes fewer than 16 entries.
 *
 * Source TU: c:\halo\SOURCE\bitmaps\bitmap_utilities.c (assert __FILE__ xref).
 * Assert strings and line numbers confirmed from the XBE at 0x79e70-0x7a1d2.
 * Note the verify polarity is the reverse of the compress siblings: source is
 * verified with FALSE (0x766) and destination with TRUE (0x76e).  Line 0x76d
 * is unused.
 *
 * source_mipmap_index is a short, not an int: the parameter slot is loaded as
 * a dword but every use is 16-bit (`TEST BX,BX; JL` at 0x79ed5 and
 * `CMP BX, word ptr [ESI+0x14]` at 0x79eda).  MSVC will not narrow an int
 * parameter to a 16-bit test, so the declared type must be short.
 *
 * bitmap_data_t offsets used: +0x04 width (short), +0x06 height (short),
 * +0x08 depth (short), +0x0a type (short; 0 == _bitmap_type_2d), +0x0c format
 * (short), +0x0e flags (byte; bit 1 == _bitmap_compressed_bit), +0x14
 * mipmap_count (short).
 *
 * Block formats (dispatch at 0x7a0a5 is a SUB 0xe / DEC / DEC compare chain):
 * 0x0e consumes 8 bytes per block, 0x0f and 0x10 consume 16.
 */
void FUN_00079e70(void *source_bitmap, void *destination_bitmap,
                  short source_mipmap_index)
{
  unsigned char *source_address;
  unsigned int *destination_address;
  unsigned int pixels[16];
  short mipmap_width;
  short mipmap_height;
  short x;
  short y;
  short i;
  short j;
  short k;

  /* bitmap_verify(source_bitmap, FALSE) */
  if (!bitmap_verify(source_bitmap, 0)) {
    display_assert("bitmap_verify(source_bitmap, FALSE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x766, 1);
    system_exit(-1);
  }

  if (*(short *)((char *)source_bitmap + 0xa) != 0) {
    display_assert("source_bitmap->type==_bitmap_type_2d",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x767, 1);
    system_exit(-1);
  }

  if (source_mipmap_index < 0 ||
      source_mipmap_index > *(short *)((char *)source_bitmap + 0x14)) {
    display_assert("source_mipmap_index>=0 && "
                   "source_mipmap_index<=source_bitmap->mipmap_count",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x768, 1);
    system_exit(-1);
  }

  /* MAX(1, source_bitmap->width >>source_mipmap_index) */
  if ((1 > (*(short *)((char *)source_bitmap + 4) >> source_mipmap_index) ?
         1 :
         (*(short *)((char *)source_bitmap + 4) >> source_mipmap_index)) !=
      *(short *)((char *)destination_bitmap + 4)) {
    display_assert("MAX(1, source_bitmap->width "
                   ">>source_mipmap_index)==destination_bitmap->width",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x769, 1);
    system_exit(-1);
  }

  /* MAX(1, source_bitmap->height>>source_mipmap_index) */
  if ((1 > (*(short *)((char *)source_bitmap + 6) >> source_mipmap_index) ?
         1 :
         (*(short *)((char *)source_bitmap + 6) >> source_mipmap_index)) !=
      *(short *)((char *)destination_bitmap + 6)) {
    display_assert("MAX(1, source_bitmap->height"
                   ">>source_mipmap_index)==destination_bitmap->height",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x76a, 1);
    system_exit(-1);
  }

  /* MAX(1, source_bitmap->depth >>source_mipmap_index) */
  if ((1 > (*(short *)((char *)source_bitmap + 8) >> source_mipmap_index) ?
         1 :
         (*(short *)((char *)source_bitmap + 8) >> source_mipmap_index)) !=
      *(short *)((char *)destination_bitmap + 8)) {
    display_assert("MAX(1, source_bitmap->depth "
                   ">>source_mipmap_index)==destination_bitmap->depth",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x76b, 1);
    system_exit(-1);
  }

  /* TEST_FLAG(source_bitmap->flags, _bitmap_compressed_bit) */
  if ((*(unsigned char *)((char *)source_bitmap + 0xe) & 2) == 0) {
    display_assert("TEST_FLAG(source_bitmap->flags, _bitmap_compressed_bit)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x76c, 1);
    system_exit(-1);
  }

  /* bitmap_verify(destination_bitmap, TRUE) */
  if (!bitmap_verify(destination_bitmap, 1)) {
    display_assert("bitmap_verify(destination_bitmap, TRUE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x76e, 1);
    system_exit(-1);
  }

  if (*(short *)((char *)destination_bitmap + 0xa) != 0) {
    display_assert("destination_bitmap->type==_bitmap_type_2d",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x76f, 1);
    system_exit(-1);
  }

  source_address =
    (unsigned char *)bitmap_mipmap_address(source_bitmap, source_mipmap_index);
  mipmap_height = bitmap_mipmap_get_height(source_bitmap, source_mipmap_index);

  for (y = 0; y < mipmap_height; y += 4) {
    /* Recomputed every row of blocks, not hoisted (call at 0x7a089 is inside
     * the outer loop body). */
    mipmap_width = bitmap_mipmap_width(source_bitmap, source_mipmap_index);

    for (x = 0; x < mipmap_width; x += 4) {
      k = 0;

      switch (*(short *)((char *)source_bitmap + 0xc)) {
      case 0xe:
        FUN_00071400(source_address, pixels);
        source_address += 8;
        break;
      case 0xf:
        FUN_000717b0(source_address, pixels);
        source_address += 0x10;
        break;
      case 0x10:
        FUN_00071890(source_address, pixels);
        source_address += 0x10;
        break;
      default:
        display_assert("### ERROR unsupported bitmap format",
                       "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x78c,
                       1);
        system_exit(-1);
      }

      for (j = 0; j < 4; j++) {
        for (i = 0; i < 4; i++) {
          /* Clipped edge blocks drop the out-of-range pixels; k advances only
           * for pixels that are actually written (INC EDI at 0x7a16e is inside
           * the guarded arm). */
          if (x + i < *(short *)((char *)destination_bitmap + 4) &&
              y + j < *(short *)((char *)destination_bitmap + 6)) {
            destination_address = (unsigned int *)bitmap_2d_address(
              destination_bitmap, x + i, y + j, 0);
            *destination_address = pixels[k];
            k++;
          }
        }
      }
    }
  }
}

/*
 * FUN_0007a1e0 -- uncompress one mipmap level of a compressed 3D bitmap into
 * an uncompressed 3D bitmap.
 *
 * Same shape as bitmap_2d_uncompress_from_mipmap below, differing only in the
 * bitmap type constant (1 == _bitmap_type_3d), the loop bound (the source
 * bitmap's depth instead of the six cube faces), the assert line numbers
 * (0x7b0-0x7b9; 0x7b7 unused), and the per-slice extract/insert helpers.
 *
 * Allocates two temporary 2D bitmaps sized to the destination -- one in the
 * source bitmap's (compressed) format and one in the destination's format --
 * then for each slice: extracts the requested mipmap level of that slice into
 * the first temp, uncompresses it into the second, and writes the result back
 * as the corresponding slice of the destination.
 *
 * Source TU: c:\halo\SOURCE\bitmaps\bitmap_utilities.c (assert __FILE__ xref).
 * Assert strings and line numbers confirmed from the XBE at 0x7a1e0-0x7a498.
 *
 * bitmap_data_t offsets used: +0x04 width (short), +0x06 height (short),
 * +0x08 depth (short), +0x0a type (short; 1 == _bitmap_type_3d), +0x0c format
 * (unsigned short), +0x0e flags (byte; bit 1 == _bitmap_compressed_bit),
 * +0x14 mipmap_count (short), +0x2c pixel data.
 */
void FUN_0007a1e0(void *source_bitmap, void *destination_bitmap,
                  short source_mipmap_index)
{
  void *temp_source;
  void *temp_destination;
  short slice;

  /* bitmap_verify(source_bitmap, FALSE) */
  if (!bitmap_verify(source_bitmap, 0)) {
    display_assert("bitmap_verify(source_bitmap, FALSE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x7b0, 1);
    system_exit(-1);
  }

  if (*(short *)((char *)source_bitmap + 0xa) != 1) {
    display_assert("source_bitmap->type==_bitmap_type_3d",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x7b1, 1);
    system_exit(-1);
  }

  if (source_mipmap_index < 0 ||
      source_mipmap_index > *(short *)((char *)source_bitmap + 0x14)) {
    display_assert("source_mipmap_index>=0 && "
                   "source_mipmap_index<=source_bitmap->mipmap_count",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x7b2, 1);
    system_exit(-1);
  }

  /* MAX(1, source_bitmap->width >>source_mipmap_index) */
  if ((1 > (*(short *)((char *)source_bitmap + 4) >> source_mipmap_index) ?
         1 :
         (*(short *)((char *)source_bitmap + 4) >> source_mipmap_index)) !=
      *(short *)((char *)destination_bitmap + 4)) {
    display_assert("MAX(1, source_bitmap->width "
                   ">>source_mipmap_index)==destination_bitmap->width",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x7b3, 1);
    system_exit(-1);
  }

  /* MAX(1, source_bitmap->height>>source_mipmap_index) */
  if ((1 > (*(short *)((char *)source_bitmap + 6) >> source_mipmap_index) ?
         1 :
         (*(short *)((char *)source_bitmap + 6) >> source_mipmap_index)) !=
      *(short *)((char *)destination_bitmap + 6)) {
    display_assert("MAX(1, source_bitmap->height"
                   ">>source_mipmap_index)==destination_bitmap->height",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x7b4, 1);
    system_exit(-1);
  }

  /* MAX(1, source_bitmap->depth >>source_mipmap_index) */
  if ((1 > (*(short *)((char *)source_bitmap + 8) >> source_mipmap_index) ?
         1 :
         (*(short *)((char *)source_bitmap + 8) >> source_mipmap_index)) !=
      *(short *)((char *)destination_bitmap + 8)) {
    display_assert("MAX(1, source_bitmap->depth "
                   ">>source_mipmap_index)==destination_bitmap->depth",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x7b5, 1);
    system_exit(-1);
  }

  /* TEST_FLAG(source_bitmap->flags, _bitmap_compressed_bit) */
  if ((*(unsigned char *)((char *)source_bitmap + 0xe) & 2) == 0) {
    display_assert("TEST_FLAG(source_bitmap->flags, _bitmap_compressed_bit)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x7b6, 1);
    system_exit(-1);
  }

  /* bitmap_verify(destination_bitmap, TRUE) */
  if (!bitmap_verify(destination_bitmap, 1)) {
    display_assert("bitmap_verify(destination_bitmap, TRUE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x7b8, 1);
    system_exit(-1);
  }

  if (*(short *)((char *)destination_bitmap + 0xa) != 1) {
    display_assert("destination_bitmap->type==_bitmap_type_3d",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x7b9, 1);
    system_exit(-1);
  }

  /* Both temporaries are sized from the destination; only the format differs.
   */
  temp_source =
    bitmap_2d_new(*(unsigned short *)((char *)destination_bitmap + 4),
                  *(unsigned short *)((char *)destination_bitmap + 6), 0,
                  *(unsigned short *)((char *)source_bitmap + 0xc));
  temp_destination =
    bitmap_2d_new(*(unsigned short *)((char *)destination_bitmap + 4),
                  *(unsigned short *)((char *)destination_bitmap + 6), 0,
                  *(unsigned short *)((char *)destination_bitmap + 0xc));

  if (temp_source != 0 && *(int *)((char *)temp_source + 0x2c) != 0 &&
      temp_destination != 0 && *(int *)((char *)temp_destination + 0x2c) != 0) {
    for (slice = 0; slice < *(short *)((char *)source_bitmap + 8); slice++) {
      bitmap_3d_slice_insert(source_bitmap, source_mipmap_index, slice,
                             temp_source);
      FUN_00079e70(temp_source, temp_destination, 0);
      bitmap_cube_map_face_extract(temp_destination, destination_bitmap, 0,
                                   slice);
    }
  } else {
    error(2, "### ERROR failed to allocate temporary bitmap");
  }

  bitmap_delete(temp_source);
  bitmap_delete(temp_destination);
}

/*
 * bitmap_2d_uncompress_from_mipmap -- uncompress one mipmap level of a
 * compressed cube map into an uncompressed cube map.
 *
 * Allocates two temporary 2D bitmaps sized to the destination -- one in the
 * source bitmap's (compressed) format and one in the destination's format --
 * then for each of the six cube faces: extracts the requested mipmap level of
 * that face into the first temp, uncompresses it into the second, and inserts
 * the result as the corresponding face of the destination cube map.
 *
 * Source TU: c:\halo\SOURCE\bitmaps\bitmap_utilities.c (assert __FILE__ xref).
 * Assert lines 0x7f9-0x802 confirmed from the XBE.
 *
 * bitmap_data_t offsets used: +0x04 width (short), +0x06 height (short),
 * +0x08 depth (short), +0x0a type (short; 2 == _bitmap_type_cube_map),
 * +0x0c format (unsigned short), +0x0e flags (byte; bit 1 ==
 * _bitmap_compressed_bit), +0x14 mipmap_count (short), +0x2c pixel data.
 */
void bitmap_2d_uncompress_from_mipmap(void *source_bitmap,
                                      void *destination_bitmap,
                                      short source_mipmap_index)
{
  void *temp_source;
  void *temp_destination;
  short face;

  /* bitmap_verify(source_bitmap, FALSE) */
  if (!bitmap_verify(source_bitmap, 0)) {
    display_assert("bitmap_verify(source_bitmap, FALSE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x7f9, 1);
    system_exit(-1);
  }

  if (*(short *)((char *)source_bitmap + 0xa) != 2) {
    display_assert("source_bitmap->type==_bitmap_type_cube_map",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x7fa, 1);
    system_exit(-1);
  }

  if (source_mipmap_index < 0 ||
      source_mipmap_index > *(short *)((char *)source_bitmap + 0x14)) {
    display_assert("source_mipmap_index>=0 && "
                   "source_mipmap_index<=source_bitmap->mipmap_count",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x7fb, 1);
    system_exit(-1);
  }

  /* MAX(1, source_bitmap->width >>source_mipmap_index) */
  if ((1 > (*(short *)((char *)source_bitmap + 4) >> source_mipmap_index) ?
         1 :
         (*(short *)((char *)source_bitmap + 4) >> source_mipmap_index)) !=
      *(short *)((char *)destination_bitmap + 4)) {
    display_assert("MAX(1, source_bitmap->width "
                   ">>source_mipmap_index)==destination_bitmap->width",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x7fc, 1);
    system_exit(-1);
  }

  /* MAX(1, source_bitmap->height>>source_mipmap_index) */
  if ((1 > (*(short *)((char *)source_bitmap + 6) >> source_mipmap_index) ?
         1 :
         (*(short *)((char *)source_bitmap + 6) >> source_mipmap_index)) !=
      *(short *)((char *)destination_bitmap + 6)) {
    display_assert("MAX(1, source_bitmap->height"
                   ">>source_mipmap_index)==destination_bitmap->height",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x7fd, 1);
    system_exit(-1);
  }

  /* MAX(1, source_bitmap->depth >>source_mipmap_index) */
  if ((1 > (*(short *)((char *)source_bitmap + 8) >> source_mipmap_index) ?
         1 :
         (*(short *)((char *)source_bitmap + 8) >> source_mipmap_index)) !=
      *(short *)((char *)destination_bitmap + 8)) {
    display_assert("MAX(1, source_bitmap->depth "
                   ">>source_mipmap_index)==destination_bitmap->depth",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x7fe, 1);
    system_exit(-1);
  }

  /* TEST_FLAG(source_bitmap->flags, _bitmap_compressed_bit) */
  if ((*(unsigned char *)((char *)source_bitmap + 0xe) & 2) == 0) {
    display_assert("TEST_FLAG(source_bitmap->flags, _bitmap_compressed_bit)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x7ff, 1);
    system_exit(-1);
  }

  /* bitmap_verify(destination_bitmap, TRUE) */
  if (!bitmap_verify(destination_bitmap, 1)) {
    display_assert("bitmap_verify(destination_bitmap, TRUE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x801, 1);
    system_exit(-1);
  }

  if (*(short *)((char *)destination_bitmap + 0xa) != 2) {
    display_assert("destination_bitmap->type==_bitmap_type_cube_map",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x802, 1);
    system_exit(-1);
  }

  /* Both temporaries are sized from the destination; only the format differs.
   */
  temp_source =
    bitmap_2d_new(*(unsigned short *)((char *)destination_bitmap + 4),
                  *(unsigned short *)((char *)destination_bitmap + 6), 0,
                  *(unsigned short *)((char *)source_bitmap + 0xc));
  temp_destination =
    bitmap_2d_new(*(unsigned short *)((char *)destination_bitmap + 4),
                  *(unsigned short *)((char *)destination_bitmap + 6), 0,
                  *(unsigned short *)((char *)destination_bitmap + 0xc));

  if (temp_source != 0 && *(int *)((char *)temp_source + 0x2c) != 0 &&
      temp_destination != 0 && *(int *)((char *)temp_destination + 0x2c) != 0) {
    for (face = 0; face < 6; face++) {
      FUN_0007ea60(source_bitmap, source_mipmap_index, face, temp_source);
      FUN_00079e70(temp_source, temp_destination, 0);
      bitmap_cube_map_face_insert(temp_destination, destination_bitmap, 0,
                                  face);
    }
  } else {
    error(2, "### ERROR failed to allocate temporary bitmap");
  }

  bitmap_delete(temp_source);
  bitmap_delete(temp_destination);
}

/*
 * real_rgb_color_brightness -- real_rgb_color_brightness: compute luminance of
 * an RGB color.
 *
 * Returns the dot product of the color with standard luminance coefficients
 * (0.299, 0.587, 0.114) stored at globals 0x2647c0-c8.
 */
float real_rgb_color_brightness(float *color)
{
  return color[0] * *(float *)0x2647c0 + color[1] * *(float *)0x2647c4 +
         color[2] * *(float *)0x2647c8;
}

/*
 * rgb_color_to_hsv_color -- convert a 16-bit-per-channel rgb_color to an
 * hsv_color.
 *
 * Integer sibling of the real_rgb_color -> real_hsv_color converter below
 * (0x7ab50): identical max/min/delta/sector algebra, but the channels are
 * loaded as unsigned 16-bit fixed point (MOVZX word + FILD, scaled by
 * 1/65535) and the results are written back as 16-bit fixed point via
 * _ftol2 (hue scaled by 65536, saturation/value by 65535).
 *
 * Layout evidence (rgb_color / hsv_color, three 16-bit fields each):
 *   +0x00 red   / hue        +0x02 green / saturation   +0x04 blue / value
 * Loads are MOVZX (zero-extending), so the source channels are UNSIGNED
 * 16-bit; the stores are plain `mov word ptr`, which reveals no signedness.
 *
 * The max/min pairs keep the duplicated inner comparison of the original
 * MAX()/MIN() macro expansion -- MSVC71 emits the g-vs-b test twice for each
 * (0x7a7c3-0x7a809 and 0x7a809-0x7a846) and collapsing it in C loses the
 * shape.  Delta is max - min: at 0x7a846 ST0=max, ST1=min and `fsub st(1)`
 * makes max the minuend.
 *
 * The two asserts are emitted AFTER all of the min/max FPU work (the
 * `test esi,esi` at 0x7a84c is scheduled into the delta store).  MSVC cannot
 * sink FPU math past a call, so that ordering is the source ordering, and it
 * matches the already-ported float sibling.
 *
 * Source TU: c:\halo\SOURCE\bitmaps\bitmap_utilities.c (assert __FILE__).
 * Assert lines 0x852/0x853 confirmed from the XBE at 0x7a859/0x7a87d.
 * Float constants are the same .rdata cells the reference addresses:
 *   0x2647f4 = 1/65535   0x2647d4 = 1/6   0x2647d0 = 65536   0x2647cc = 65535
 *   0x2533c0 = 0.0   0x2533c8 = 1.0   0x253f40 = 2.0   0x2533d8 = 4.0
 */
short *rgb_color_to_hsv_color(unsigned short *rgb, short *hsv)
{
  float r;
  float g;
  float b;
  float max_component;
  float min_component;
  float delta;
  float hue;
  float saturation;

  r = (float)rgb[0] * *(float *)0x2647f4;
  g = (float)rgb[1] * *(float *)0x2647f4;
  b = (float)rgb[2] * *(float *)0x2647f4;

  if (g > b) {
    max_component = g;
  } else {
    max_component = b;
  }
  if (r > max_component) {
    max_component = r;
  } else {
    if (g > b) {
      max_component = g;
    } else {
      max_component = b;
    }
  }

  if (g > b) {
    min_component = b;
  } else {
    min_component = g;
  }
  if (r > min_component) {
    if (g > b) {
      min_component = b;
    } else {
      min_component = g;
    }
  } else {
    min_component = r;
  }

  delta = max_component - min_component;

  if (hsv == (short *)0) {
    display_assert("hsv", "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c",
                   0x852, true);
    system_exit(-1);
  }
  if (rgb == (unsigned short *)hsv) {
    display_assert("rgb!=(rgb_color *)hsv",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x853,
                   true);
    system_exit(-1);
  }

  if (max_component == *(float *)0x2533c0) {
    saturation = *(float *)0x2533c0;
    hue = *(float *)0x2533c0;
  } else {
    saturation = delta / max_component;
    if (saturation == *(float *)0x2533c0) {
      hue = *(float *)0x2533c0;
    } else {
      if (r == max_component) {
        hue = (g - b) / delta;
      } else if (g == max_component) {
        hue = (b - r) / delta + *(float *)0x253f40;
      } else {
        hue = (r - g) / delta + *(float *)0x2533d8;
      }
      hue = hue * *(float *)0x2647d4;
      if (hue < *(float *)0x2533c0) {
        hue = hue + *(float *)0x2533c8;
      }
    }
  }

  hsv[0] = (short)(hue * *(float *)0x2647d0);
  hsv[1] = (short)(saturation * *(float *)0x2647cc);
  hsv[2] = (short)(max_component * *(float *)0x2647cc);
  return hsv;
}

/*
 * hsv_color_to_rgb_color -- fixed-point (uint16) sibling of
 * real_hsv_color_to_real_rgb_color at 0x7ace0.  Same floor()/p/q/t algebra,
 * but the components arrive and leave as three uint16 at +0/+2/+4.
 *
 * Structural notes derived from the disassembly at 0x7a970, NOT from the
 * float sibling (copying the sibling's shape here would be wrong):
 *   - The three normalizations are issued in the prologue (0x7a97a-0x7a9bb),
 *     ahead of both assert tests (TEST EDI,EDI at 0x7a98e).
 *   - The saturation==0 arm does NOT return early; it falls through to the
 *     shared 65535-scale/store epilogue at 0x7aaef.
 *   - p/q/t are computed before the `cmp eax,5` dispatch at 0x7aa43, so they
 *     are live on the default path too.
 *   - Every switch arm reaches the same store epilogue rather than storing.
 *
 * Hue uses 2^-16 (0x2647fc) while saturation and value use 1/65535
 * (0x2647f4).  These are DIFFERENT constants at adjacent addresses; the hue
 * normalizer is deliberately not 1/65535 because the value is immediately
 * multiplied by 6.0f (0x254640) to select a 60-degree sector.
 *
 * Branch senses confirmed from the flag idioms.  At 0x7aa0d the guard is
 * `FCOMP 0.0f / FNSTSW / TEST AH,0x44 / JP`: TEST yields 0x40 when the values
 * are equal, which is odd parity, so JP is taken only when saturation is
 * non-zero.  The taken edge therefore reaches the chromatic block and the
 * fallthrough is the achromatic one -- i.e. the achromatic case is the `then`
 * arm, which is why this is written `== 0.0f` and not `!= 0.0f`.  Writing it
 * the other way inverts the block order and costs an extra FLD because MSVC
 * then compares two loaded values (FUCOMPP) instead of folding the constant
 * into the FCOMP memory operand.
 *
 * At 0x7aa39 (TEST AH,0x41 / JZ after FILD sector; FCOMP scaled_hue) the test
 * is the floor() correction `(float)sector > scaled_hue`.
 */
unsigned short *hsv_color_to_rgb_color(unsigned short *hsv,
                                       unsigned short *rgb_out)
{
  float scaled_hue;
  float saturation;
  float value;
  float f;
  float p;
  float q;
  float t;
  float r;
  float g;
  float b;
  int sector;

  scaled_hue = (float)hsv[0] * *(float *)0x2647fc * *(float *)0x254640;
  saturation = (float)hsv[1] * *(float *)0x2647f4;
  value = (float)hsv[2] * *(float *)0x2647f4;

  if (rgb_out == (unsigned short *)0) {
    display_assert("rgb", "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c",
                   0x886, true);
    system_exit(-1);
  }

  if (rgb_out == hsv) {
    display_assert("rgb!=(rgb_color *)hsv",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x888,
                   true);
    system_exit(-1);
  }

  if (saturation == *(float *)0x2533c0) {
    r = value;
    g = value;
    b = value;
  } else {
    sector = (int)scaled_hue;
    if ((float)sector > scaled_hue)
      sector--;

    f = scaled_hue - (float)sector;
    p = (*(float *)0x2533c8 - saturation) * value;
    q = (*(float *)0x2533c8 - saturation * f) * value;
    t = (*(float *)0x2533c8 - (*(float *)0x2533c8 - f) * saturation) * value;

    switch (sector) {
    case 0:
      r = value;
      g = t;
      b = p;
      break;
    case 1:
      r = q;
      g = value;
      b = p;
      break;
    case 2:
      r = p;
      g = value;
      b = t;
      break;
    case 3:
      r = p;
      g = q;
      b = value;
      break;
    case 4:
      r = t;
      g = p;
      b = value;
      break;
    case 5:
      r = value;
      g = p;
      b = q;
      break;
    default:
      r = value;
      g = value;
      b = value;
      break;
    }
  }

  rgb_out[0] = (unsigned short)(int)(r * *(float *)0x2647cc);
  rgb_out[1] = (unsigned short)(int)(g * *(float *)0x2647cc);
  rgb_out[2] = (unsigned short)(int)(b * *(float *)0x2647cc);
  return rgb_out;
}

float *bitmap_clone(float *rgb, float *hsv_out)
{
  float max_component;
  float min_component;
  float chroma;

  if (rgb[1] > rgb[2]) {
    max_component = rgb[1];
  } else {
    max_component = rgb[2];
  }
  if (max_component < rgb[0]) {
    max_component = rgb[0];
  } else {
    if (rgb[1] > rgb[2]) {
      max_component = rgb[1];
    } else {
      max_component = rgb[2];
    }
  }

  if (rgb[1] > rgb[2]) {
    min_component = rgb[2];
  } else {
    min_component = rgb[1];
  }
  if (min_component < rgb[0]) {
    if (rgb[1] > rgb[2]) {
      min_component = rgb[2];
    } else {
      min_component = rgb[1];
    }
  } else {
    min_component = rgb[0];
  }

  chroma = max_component - min_component;

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
    hsv_out[0] = (rgb[1] - rgb[2]) / chroma;
  } else if (rgb[1] == max_component) {
    hsv_out[0] = (rgb[2] - rgb[0]) / chroma + *(float *)0x253f40;
  } else {
    hsv_out[0] = (rgb[0] - rgb[1]) / chroma + *(float *)0x2533d8;
  }
  hsv_out[0] = hsv_out[0] * *(float *)0x2647d4;
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

/*
 * argb_color_to_real_argb_color -- argb_color_to_real_argb_color: convert 4
 * unsigned shorts to 4 floats, scaled by 1/65535.
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
 * rgb_color_to_real_rgb_color -- rgb_color_to_real_rgb_color: convert 3
 * unsigned shorts to 3 floats, scaled by 1/65535.
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
 * pixel32_to_real_argb_color -- pixel32_to_real_argb_color: extract ARGB from a
 * packed uint32 into 4 floats, scaled by 1/255.
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
 * pixel32_to_real_rgb_color -- pixel32_to_real_rgb_color: extract RGB from a
 * packed uint32 into 3 floats, scaled by 1/255.
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

  if (rgb[0] >= *(float *)0x2533c0 && rgb[0] <= *(float *)0x2533c8 &&
      rgb[1] >= *(float *)0x2533c0 && rgb[1] <= *(float *)0x2533c8 &&
      rgb[2] >= *(float *)0x2533c0 && rgb[2] <= *(float *)0x2533c8)
    return true;

  return false;
}

/*
 * bitmap_shrink -- bitmap_shrink: dispatcher for bitmap mipmap shrinking.
 *
 * Validates the bitmap. If mipmap_count < 2, delegates to FUN_00077590.
 * Otherwise dispatches based on bitmap->type: 2D -> FUN_00077720,
 * 3D -> FUN_000779b0, cube_map -> FUN_00077cd0.
 * Returns a pointer to the shrunk bitmap (or NULL on error).
 */
void *bitmap_shrink(void *bitmap, short mipmap_count, int param_3, int param_4)
{
  /* bitmap_verify(bitmap, TRUE) */
  if (!bitmap_verify(bitmap, 1)) {
    display_assert("bitmap_verify(source_bitmap, TRUE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0xe1, 1);
    system_exit(-1);
  }

  if (mipmap_count <= 1) {
    return FUN_00077590(bitmap);
  }

  switch (*(short *)((char *)bitmap + 0xa)) {
  case 0:
    return FUN_00077720((short)mipmap_count, bitmap, (short)param_3,
                        (char)param_4);
  case 1:
    return FUN_000779b0((short)mipmap_count, bitmap, (short)param_3,
                        (char)param_4);
  case 2:
    return FUN_00077cd0(bitmap, mipmap_count, param_3, param_4);
  default:
    display_assert("### ERROR unupported bitmap type",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0xf3, 1);
    system_exit(-1);
    return (void *)0;
  }
}

/*
 * bitmap_smooth (0x7b1b0) -- dispatcher for bitmap smoothing by type.
 *
 * Validates the bitmap, checks filter_size range, then builds a 1-D
 * Gaussian kernel of radius floor(smooth_factor) by iterating Pascal's
 * triangle accumulation into a 10-element short array at DAT_00334560.
 * Dispatches: type 0 -> FUN_00077ff0, type 1 -> FUN_00078460,
 * type 2 -> FUN_00078b80 (cube map, stub).
 */
void bitmap_smooth(void *pixel_data, float smooth_factor)
{
  short *psVar4;
  int filter_radius;
  int iVar5;
  unsigned int diameter;
  short *filter_table;

  filter_radius = (int)smooth_factor;

  if (!bitmap_verify(pixel_data, 1)) {
    display_assert("bitmap_verify(bitmap, TRUE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x21e, 1);
    system_exit(-1);
  }

  if (smooth_factor > *(float *)0x253f34) {
    display_assert("filter_size<=(float)MAXIMUM_FILTER_SIZE",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x21f, 1);
    system_exit(-1);
  }

  if ((float)(int16_t)filter_radius <= *(float *)0x2533c0) {
    return;
  }

  filter_table = (short *)0x334560;
  csmemset(filter_table, 0, 0x14);
  if ((int16_t)(filter_radius * 2) >= 0) {
    diameter = (unsigned short)(filter_radius * 2 + 1);
    do {
      psVar4 = (short *)0x334572;
      iVar5 = 9;
      do {
        *psVar4 = *psVar4 + *(psVar4 - 1);
        psVar4 = psVar4 - 1;
        iVar5 = iVar5 - 1;
      } while (iVar5 != 0);
      diameter = diameter - 1;
      *(short *)0x334560 = 1;
    } while (diameter != 0);
  }

  switch ((int16_t)(*(short *)((char *)pixel_data + 0xa))) {
  case 0:
    FUN_00077ff0(pixel_data, filter_radius, filter_table);
    return;
  case 1:
    FUN_00078460(pixel_data, filter_radius, filter_table);
    return;
  case 2:
    FUN_00078b80(filter_radius, filter_table, pixel_data);
    return;
  default:
    display_assert("### ERROR unsupported bitmap type",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x244, 1);
    system_exit(-1);
    return;
  }
}

/*
 * bitmap_sharpen (0x7b310) -- dispatcher for bitmap sharpening by type.
 *
 * Builds the two 256-entry sharpen weight tables (positive at 0x334360,
 * negative at 0x334160 -- adjacent, 0x200 bytes each) from `amount`, then
 * dispatches on bitmap->type: 2D -> bitmap_2d_sharpen, 3D -> FUN_000790b0,
 * cube_map -> FUN_00079180.
 *
 * Confirmed from disassembly at 0x7b310:
 *   - Guard at 0x7b348 is FLD [ebp+0xc]; FCOMP [0x2533c0]; FNSTSW; TEST
 * AH,0x41; JNZ -- the ordinary `amount > 0.0f` form, NOT an inverted parity
 * branch.
 *   - percent is compared 16-bit (TEST AX,AX / CMP AX,0x64) so it is a short,
 *     and the divisor is re-narrowed through AX (MOVSX ESI,AX at 0x7b39c)
 *     before every IDIV -- keep both shorts.
 *   - The negative-table term is a plain 32-bit CDQ/IDIV at 0x7b3c5; Ghidra's
 *     CONCAT44(...)/(longlong) rendering is a decompiler artifact, and the
 *     `+ (x>>31 & 7)` / SAR 3 pair is the MSVC signed divide-by-8 idiom.
 *   - The float argument is forwarded to the callees as a RAW DWORD (MOV
 *     ECX/EDX/EAX, [ebp+0xc] at 0x7b410 / 0x7b42d / 0x7b44a) -- it is a bit
 *     pattern, not a converted int, so it must be bit-punned and never cast.
 *   - The 2D callee has a DIFFERENT register contract from its two siblings:
 *     ESI carries the *negative table* (MOV ESI,0x334160 at 0x7b454, after the
 *     three pushes), not the bitmap.
 */
void bitmap_sharpen(void *bitmap, float amount)
{
  short percent;
  short divisor;
  int positive_accumulator;
  int negative_accumulator;
  int i;

  /* bitmap_verify(bitmap, TRUE) */
  if (!bitmap_verify(bitmap, 1)) {
    display_assert("bitmap_verify(bitmap, TRUE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x361, 1);
    system_exit(-1);
  }

  /* Written as `> 0.0f` rather than an inverted early-return: the reference
   * emits TEST AH,0x41 / JNZ at 0x7b354, and the `<=` form yields a JNP. */
  if (amount > *(float *)0x2533c0) {
    percent = (short)(amount * *(float *)0x253f00);
    if (percent < 0) {
      percent = 0;
    } else if (percent > 100) {
      percent = 100;
    }

    divisor = (short)(100 - percent);
    if (divisor < 1) {
      divisor = 1;
    }

    positive_accumulator = 0;
    negative_accumulator = 0;
    for (i = 0; i < 256; i++) {
      ((short *)0x334360)[i] = (short)(positive_accumulator / divisor);
      positive_accumulator = positive_accumulator + 100;
      ((short *)0x334160)[i] = (short)((negative_accumulator / 8) / divisor);
      negative_accumulator = negative_accumulator + percent;
    }

    switch (*(short *)((char *)bitmap + 0xa)) {
    case 0:
      bitmap_2d_sharpen(bitmap, *(int *)&amount, 0x334360, 0x334160);
      break;
    case 1:
      FUN_000790b0(*(int *)&amount, 0x334360, 0x334160, bitmap);
      break;
    case 2:
      FUN_00079180(*(int *)&amount, 0x334360, 0x334160, bitmap);
      break;
    default:
      display_assert("### ERROR unsupported bitmap type",
                     "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x37f, 1);
      system_exit(-1);
      break;
    }
  }
}

/*
 * bitmap_alpha_bleed -- bitmap_alpha_bleed: dispatcher for alpha bleed by
 * bitmap type.
 *
 * Validates the bitmap, checks that passes > 0, then dispatches based on
 * bitmap->type: 2D -> FUN_00079250, 3D -> FUN_00079480 (bitmap in EDI),
 * cube_map -> FUN_00079590 (bitmap in ESI).
 * On unsupported type, fires an assert.
 */
void bitmap_alpha_bleed(void *bitmap, short passes)
{
  /* bitmap_verify(bitmap, TRUE) */
  if (!bitmap_verify(bitmap, 1)) {
    display_assert("bitmap_verify(bitmap, TRUE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x402, 1);
    system_exit(-1);
  }

  if (passes <= 0) {
    return;
  }

  switch (*(short *)((char *)bitmap + 0xa)) {
  case 0:
    FUN_00079250(passes, bitmap);
    break;
  case 1:
    FUN_00079480(passes, bitmap);
    break;
  case 2:
    FUN_00079590(passes, bitmap);
    break;
  default:
    display_assert("### ERROR unsupported bitmap type",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x410, 1);
    system_exit(-1);
    break;
  }
}

/*
 * FUN_0007b940 -- 3D bitmap height_map -> bump_map conversion.
 *
 * Allocates one temporary 2D bitmap matching the 3D bitmap's width/height and
 * format, then walks every depth slice: read the slice into the temp
 * (bitmap_3d_slice_insert), run the 2D height-to-bump conversion over it
 * (FUN_0007b510), write the temp back into the slice
 * (bitmap_cube_map_face_extract).  Structurally identical to the 3D
 * alpha_bleed at FUN_00079480, which shares this alloc/loop/dispose shape.
 *
 * Confirmed from disassembly at 0x7b940:
 *   - No `sub esp`; ESI/EDI are pushed late (0x7b9cf / 0x7b9d6), after the
 *     three assert blocks -- MSVC hoisted the asserts above the register saves.
 *   - The three loop calls share one `add esp,0x24` at 0x7b91d (9 dwords =
 *     4 + 1 + 4 pushes), so per-call cleanup cannot be used to infer arity.
 *   - The depth loop bound at +0x08 is compared SIGNED (`cmp word[ebx+8],di`
 *     + jle, `cmp di,word[ebx+8]` + jl).
 *   - The alloc-failure path falls through into bitmap_delete with a NULL
 *     temp; the original does this, so no NULL guard is added here.
 *
 * ABI: bitmap passed in EBX (@EBX).  One stack param: bump_height (float),
 * read by FLD at 0x7b999 and re-pushed as an opaque dword at 0x7b90a -- it is
 * never converted to an integer.
 */
void FUN_0007b940(float bump_height, void *bitmap /* @<ebx> */)
{
  void *temp;
  short slice;

  /* bitmap_verify(bitmap, TRUE) */
  if (!bitmap_verify(bitmap, 1)) {
    display_assert("bitmap_verify(bitmap, TRUE)",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x52d, 1);
    system_exit(-1);
  }

  /* assert bitmap->type == _bitmap_type_3d */
  if (*(short *)((char *)bitmap + 0xa) != 1) {
    display_assert("bitmap->type==_bitmap_type_3d",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x52e, 1);
    system_exit(-1);
  }

  /* assert bump_height > 0.0f (DAT_002533c0 == 0.0f) */
  if (!(bump_height > *(float *)0x2533c0)) {
    display_assert("bump_height>0.0f",
                   "c:\\halo\\SOURCE\\bitmaps\\bitmap_utilities.c", 0x52f, 1);
    system_exit(-1);
  }

  temp = bitmap_2d_new(*(unsigned short *)((char *)bitmap + 4),
                       *(unsigned short *)((char *)bitmap + 6), 0,
                       *(unsigned short *)((char *)bitmap + 0xc));

  /* The reference emits TWO dispose+epilogue blocks (0x7ba27 loop-exit,
   * 0x7ba43 shared tail).  Writing that split explicitly in C -- an inner
   * `if (depth > 0) { do{}while; bitmap_delete; return; }` -- does NOT
   * reproduce it: MSVC71 collapses the duplicated return back into the shared
   * tail and the redundant pre-test costs 1.0pp (89.9% -> 88.9%, insn count
   * unchanged at 91).  The single-dispose shape below is the better match and
   * mirrors the structurally identical 3D alpha_bleed at FUN_00079480. */
  if (temp != 0 && *(int *)((char *)temp + 0x2c) != 0) {
    for (slice = 0; slice < *(short *)((char *)bitmap + 8); slice++) {
      bitmap_3d_slice_insert(bitmap, 0, slice, temp);
      /* bitmap passed in ESI (register arg); only bump_height is pushed. */
      FUN_0007b510(bump_height, temp);
      bitmap_cube_map_face_extract(temp, bitmap, 0, slice);
    }
  } else {
    error(2, "### ERROR failed to allocate temporary bitmap");
  }

  /* temp is NULL on the alloc-failure path; the original calls
   * bitmap_delete unconditionally here, so no guard is added. */
  bitmap_delete(temp);
}
