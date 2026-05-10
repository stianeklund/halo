/* FUN_00120500 (0x120500) — Get a pointer to a specific animation frame's data.
 *
 * Given an animation structure and a frame index, returns a pointer to the
 * frame data for that frame. If compression is active (flag bit 0 at
 * animation+0x3a set, and DAT_00322600 is nonzero), returns a pointer
 * offset by the compressed data offset (animation+0x88). Otherwise,
 * returns a pointer offset by frame_size * frame_index.
 *
 * The frame data itself lives in tag_data at animation+0xa0, resolved
 * via tag_data_get_pointer.
 *
 * Confirmed: cdecl, 2 args (animation ptr, frame_index short).
 * Confirmed: CALL tag_data_get_pointer(animation+0xa0, 0, 0) at 0x12052b.
 * Confirmed: Assert "frame_index>=0 && frame_index<animation->frame_count" at 0x120555.
 * Confirmed: CALL display_assert at 0x120555, system_exit(-1) at 0x12055c.
 * Confirmed: Compressed path returns ESI + [EDI+0x88] at 0x12056b.
 * Confirmed: Uncompressed path returns ESI + MOVSX([EDI+0x24]) * MOVSX(BX) at 0x120578-0x120582.
 */
void *FUN_00120500(void *animation, short frame_index)
{
  int compressed;
  char *data;
  char *anim;

  anim = (char *)animation;

  if (((*(unsigned char *)(anim + 0x3a) & 1) != 0) && DAT_00322600 != '\0') {
    compressed = 1;
  } else {
    compressed = 0;
  }

  data = (char *)tag_data_get_pointer(anim + 0xa0, 0, 0);

  if (frame_index < 0 || frame_index >= *(short *)(anim + 0x22)) {
    display_assert("frame_index>=0 && frame_index<animation->frame_count",
                   "c:\\halo\\SOURCE\\models\\model_animation_definitions.c",
                   0x47a, 1);
    system_exit(-1);
  }

  if (compressed) {
    return (void *)(data + *(int *)(anim + 0x88));
  }
  return (void *)(data + (int)*(short *)(anim + 0x24) * (int)frame_index);
}

/* model_animation_choose_random (0x120f20) — Choose a weighted random
 * animation.
 *
 * Gets the animation graph tag ('antr'), generates a random float [0,1) using
 * either the global or local random seed (based on update_kind), then walks the
 * animation chain starting at animation_index. For each animation element:
 * compares the random value against the weight threshold at element+0x44.
 * If random <= threshold, returns the current animation index. Otherwise
 * advances to the next animation via element+0x38.
 *
 * Confirmed: tag_get('antr', animation_graph_tag_index) at 0x120f2e.
 * Confirmed: update_kind==1 → get_global_random_seed_address() at 0x120f40.
 * Confirmed: update_kind==0 → random_math_get_local_seed_address() at 0x120f53.
 * Confirmed: random_math_real(seed) at 0x120f46/0x120f59.
 * Confirmed: tag_block_get_element(antr_tag+0x74, index, 0xb4) at 0x120f9e.
 * Confirmed: FCOMP [ECX+0x44] + JNP loop exit at 0x120fab-0x120fb3.
 * Confirmed: next animation at element+0x38 (int16_t) at 0x120fb5.
 */
int model_animation_choose_random(int update_kind,
                                  int animation_graph_tag_index,
                                  int16_t animation_index)
{
  char *antr_tag;
  float random_value;
  char *element;

  antr_tag = (char *)tag_get(0x616e7472, animation_graph_tag_index);
  if (update_kind == 1) {
    random_value =
      random_math_real((unsigned int *)get_global_random_seed_address());
  } else {
    random_value = random_math_real(random_math_get_local_seed_address());
    if (update_kind != 0) {
      display_assert("ASSERTION_SERIES(update_kind, 2)",
                     "c:\\halo\\SOURCE\\models\\model_animations.c", 0x3f0, 1);
      system_exit(-1);
    }
  }
  while (animation_index != -1) {
    element = (char *)tag_block_get_element(antr_tag + 0x74,
                                            (int)animation_index, 0xb4);
    if (random_value <= *(float *)(element + 0x44))
      break;
    animation_index = *(int16_t *)(element + 0x38);
  }
  return (int)animation_index;
}

/* FUN_00120810 (0x120810) — Convert 4 packed int16 values to normalized floats.
 *
 * Reads 4 consecutive short values from src and writes 4 floats to dest,
 * each multiplied by (1.0f / 32767.0f) to normalize from [-32767,32767]
 * to approximately [-1.0, 1.0]. Used to decompress quaternion rotation
 * components stored as 16-bit integers in animation frame data.
 *
 * Confirmed: cdecl, 2 args (src shorts ptr, dest floats ptr).
 * Confirmed: Leaf function, no callees.
 * Confirmed: Multiplies by float constant at 0x290dd8 = 1.0f/32767.0f.
 * Confirmed: 4 iterations via MOVSX+FILD+FMUL+FSTP pattern in disassembly.
 */
void FUN_00120810(short *src, float *dest)
{
    dest[0] = (float)(int)src[0] * (1.0f / 32767.0f);
    dest[1] = (float)(int)src[1] * (1.0f / 32767.0f);
    dest[2] = (float)(int)src[2] * (1.0f / 32767.0f);
    dest[3] = (float)(int)src[3] * (1.0f / 32767.0f);
}

/* FUN_00121d60 (0x121d60) — Decode a single animation frame into per-node
 * rotation/translation/scale data.
 *
 * For each node in the animation (animation+0x2c count), decodes rotation
 * (quaternion), translation (vec3), and scale (float) from either:
 *   - Compressed keyframed data (when flag bit 0 is set and compression is
 *     active), using FUN_00121330/FUN_00121640/FUN_00121940 interpolators.
 *   - Uncompressed frame data via FUN_00120810/FUN_00120870 or raw memcpy
 *     from default data (animation+0x98).
 *
 * Three bitmask arrays at animation offsets 0x5c, 0x6c, 0x7c (4 DWORDs each
 * for up to 128 nodes) indicate which nodes have animated rotation,
 * translation, and scale respectively. Bit=1 means animated (read from
 * frame data), bit=0 means static (read from default data).
 *
 * If the animation type (animation+0x20) is nonzero, or the mode_tag check
 * fails, falls back to FUN_00123aa0 which fills default node transforms.
 *
 * After the loop, two assertions verify that exactly the right amount of
 * frame data and default data was consumed.
 *
 * Confirmed: cdecl, 4 args, void return.
 * Confirmed: CALL FUN_00120500 at 0x121dca and 0x121fd5 (2 args: animation, frame_index).
 * Confirmed: CALL FUN_00120810 at 0x121e63 and 0x121e9d (2 args: src_shorts, dest_floats).
 * Confirmed: CALL FUN_00120870 at 0x121e89 (2 args: compressed_data, dest_floats).
 * Confirmed: CALL FUN_0010ca30 at 0x121e8f (1 arg: quaternion).
 * Confirmed: CALL FUN_00121330 at 0x121e51 (5 args: animation, frame_float, count, node, out).
 * Confirmed: CALL FUN_00121640 at 0x121edc (5 args: animation, frame_float, count, node, out).
 * Confirmed: CALL FUN_00121940 at 0x121f78 (5 args: animation, frame_float, count, node, out).
 * Confirmed: CALL FUN_00123aa0 at 0x12204a (2 args: mode_tag, out_node_data).
 */
void FUN_00121d60(void *mode_tag, void *animation, int animation_index,
                  void *out_node_data)
{
  int param_1;
  int param_2;
  int param_4;
  unsigned int uVar6;
  int iVar7;
  short sVar5;
  int iVar3;
  unsigned int local_1c;
  unsigned int local_20;
  unsigned int local_14;
  int local_10;
  int local_18;
  int local_24;
  int *local_c;
  int *local_8;
  int bVar2;
  int *puVar4;

  param_1 = (int)mode_tag;
  param_2 = (int)animation;
  param_4 = (int)out_node_data;
  uVar6 = 0;

  if (*(short *)(param_2 + 0x20) == 0 &&
      (param_1 == 0 ||
       (((*(int *)(param_2 + 0x28) == 0 ||
          *(int *)(param_2 + 0x28) == *(int *)(param_1 + 4) ||
          *(int *)(param_1 + 4) == 0) &&
         *(int *)(param_1 + 0xb8) == (int)*(short *)(param_2 + 0x2c))))) {

    if (((*(unsigned char *)(param_2 + 0x3a) & 1) == 0) ||
        (DAT_00322600 == '\0' && *(int *)(param_2 + 0x88) != 0)) {
      bVar2 = 0;
    } else {
      bVar2 = 1;
    }

    local_8 = (int *)FUN_00120500(animation, (short)animation_index);
    local_c = *(int **)(param_2 + 0x98);
    local_10 = 0;
    local_18 = 0;
    local_24 = 0;
    if (0 < *(short *)(param_2 + 0x2c)) {
      do {
        sVar5 = (short)uVar6;
        iVar7 = sVar5 * 0x20 + param_4;
        if ((uVar6 & 0x1f) == 0) {
          iVar3 = (int)(sVar5 >> 5);
          local_1c = *(unsigned int *)(param_2 + 0x5c + iVar3 * 4);
          local_14 = *(unsigned int *)(param_2 + 0x6c + iVar3 * 4);
          local_20 = *(unsigned int *)(param_2 + 0x7c + iVar3 * 4);
        }
        if ((local_14 & 1) == 0) {
          if (bVar2) {
            FUN_00120870(
              (void *)(local_8[1] + sVar5 * 6 + (int)local_8),
              (float *)iVar7);
            FUN_0010ca30((float *)iVar7);
          } else {
            FUN_00120810((short *)local_c, (float *)iVar7);
            local_c = (int *)((char *)local_c + 8);
          }
        } else if (bVar2) {
          FUN_00121330(animation, (float)(int)(short)animation_index,
                       (unsigned short)local_10, sVar5, (void *)iVar7);
          local_10 = local_10 + 1;
        } else {
          FUN_00120810((short *)local_8, (float *)iVar7);
          local_8 = (int *)((char *)local_8 + 8);
        }
        local_14 = local_14 >> 1;

        if ((local_1c & 1) == 0) {
          if (bVar2) {
            puVar4 = (int *)(local_8[5] + sVar5 * 0xc + (int)local_8);
            *(int *)(iVar7 + 0x10) = puVar4[0];
            *(int *)(iVar7 + 0x14) = puVar4[1];
            *(int *)(iVar7 + 0x18) = puVar4[2];
          } else {
            *(int *)(iVar7 + 0x10) = local_c[0];
            *(int *)(iVar7 + 0x14) = local_c[1];
            *(int *)(iVar7 + 0x18) = local_c[2];
            local_c = (int *)((char *)local_c + 0xc);
          }
        } else if (bVar2) {
          FUN_00121640(animation, (float)(int)(short)animation_index,
                       (unsigned short)local_18, sVar5,
                       (void *)(iVar7 + 0x10));
          local_18 = local_18 + 1;
        } else {
          *(int *)(iVar7 + 0x10) = local_8[0];
          *(int *)(iVar7 + 0x14) = local_8[1];
          *(int *)(iVar7 + 0x18) = local_8[2];
          local_8 = (int *)((char *)local_8 + 0xc);
        }
        local_1c = local_1c >> 1;

        if ((local_20 & 1) == 0) {
          if (bVar2) {
            *(int *)(iVar7 + 0x1c) = 0x3f800000;
          } else {
            *(int *)(iVar7 + 0x1c) = *local_c;
            local_c = (int *)((char *)local_c + 4);
          }
        } else if (bVar2) {
          FUN_00121940(animation, (float)(int)(short)animation_index,
                       (unsigned short)local_24, sVar5,
                       (void *)(iVar7 + 0x1c));
          local_24 = local_24 + 1;
        } else {
          *(int *)(iVar7 + 0x1c) = *local_8;
          local_8 = (int *)((char *)local_8 + 4);
        }
        local_20 = local_20 >> 1;

        uVar6 = uVar6 + 1;
      } while ((short)uVar6 < *(short *)(param_2 + 0x2c));
    }
    if (!bVar2) {
      iVar7 = (int)FUN_00120500(animation, (short)animation_index);
      if ((int)local_8 - iVar7 != (int)*(short *)(param_2 + 0x24)) {
        display_assert(
          "compressed || (byte *)data-(byte *)animation_get_frame_data(animation, frame_index)==animation->frame_size",
          "c:\\halo\\SOURCE\\models\\model_animations.c", 0x141, 1);
        system_exit(-1);
      }
      if ((int)local_c - *(int *)(param_2 + 0x98) !=
          *(int *)(param_2 + 0x8c)) {
        display_assert(
          "compressed || (byte *)default_data-(byte *)animation_get_default_data(animation)==animation->default_data.size",
          "c:\\halo\\SOURCE\\models\\model_animations.c", 0x142, 1);
        system_exit(-1);
      }
    }
  } else {
    FUN_00123aa0(mode_tag, out_node_data);
  }
}
