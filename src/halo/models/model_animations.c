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
 * Confirmed: Assert "frame_index>=0 && frame_index<animation->frame_count" at
 * 0x120555. Confirmed: CALL display_assert at 0x120555, system_exit(-1) at
 * 0x12055c. Confirmed: Compressed path returns ESI + [EDI+0x88] at 0x12056b.
 * Confirmed: Uncompressed path returns ESI + MOVSX([EDI+0x24]) * MOVSX(BX) at
 * 0x120578-0x120582.
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

/* FUN_001205f0 (0x1205f0) — look up a string in an indexed string table.
 * Returns "#<invalid>" if the index is out of range or the entry is NULL. */
const char *FUN_001205f0(void *string_table, int16_t index)
{
  int16_t *table = (int16_t *)string_table;
  const char *result;

  if (index >= *table || (result = *(const char **)(*(int32_t *)(table + 2) +
                                                    index * 8)) == NULL) {
    result = "#<invalid>";
  }
  return result;
}

/* quaternion_decompress_8byte (0x120810) — Convert 4 packed int16 values to
 * normalized floats.
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
void quaternion_decompress_8byte(short *src, float *dest)
{
  dest[0] = (float)(int)src[0] * (1.0f / 32767.0f);
  dest[1] = (float)(int)src[1] * (1.0f / 32767.0f);
  dest[2] = (float)(int)src[2] * (1.0f / 32767.0f);
  dest[3] = (float)(int)src[3] * (1.0f / 32767.0f);
}

/* quaternion_decompress_6byte (0x120870) — Decompress 3 packed uint16s into 4 normalized
 * floats.
 *
 * Extracts 4 values from 3 consecutive unsigned shorts (48 bits total) by
 * interleaved bit manipulation, sign-extends each to int, converts to float,
 * and multiplies by (1.0f / 32767.0f). Used to decompress compressed
 * quaternion rotation data in animation frames (compressed path in
 * FUN_00121d60).
 *
 * Confirmed: cdecl, 2 args (compressed_data ushort ptr, dest floats ptr).
 * Confirmed: Leaf function, no callees.
 * Confirmed: Multiplies by float constant at 0x290dd8 = 1.0f/32767.0f.
 * Confirmed: 4 outputs via interleaved bit extraction + MOVSX + FILD + FMUL +
 * FSTP. Confirmed: Bit operations verified against disassembly at
 * 0x120870-0x12092a.
 */
void quaternion_decompress_6byte(void *compressed_data, float *dest)
{
  unsigned short *src;
  unsigned short w0, w1, w2;
  short s0, s1, s2, s3;

  src = (unsigned short *)compressed_data;
  w0 = src[0];
  w1 = src[1];
  w2 = src[2];

  s0 = (short)((w0 >> 12) | (w0 & 0xFFF0));
  s1 = (short)(((w1 >> 4) & 0xFF0) | (w0 & 0xF) | (w0 << 12));
  s2 = (short)((((w2 >> 4) & 0xF00) | (w1 & 0xF0)) >> 4 | (w1 << 8));
  s3 = (short)(((w2 >> 8) & 0xF) | (w2 << 4));

  dest[0] = (float)(int)s0 * (1.0f / 32767.0f);
  dest[1] = (float)(int)s1 * (1.0f / 32767.0f);
  dest[2] = (float)(int)s2 * (1.0f / 32767.0f);
  dest[3] = (float)(int)s3 * (1.0f / 32767.0f);
}

/* find_keyframe_index (0x120d10) — Binary search for a keyframe by frame index.
 *
 * Given a sorted array of keyframe frame indices and a target frame, returns
 * the keyframe index i such that:
 *   keyframe_frame_indices[i] <= target_frame_index <
 * keyframe_frame_indices[i+1]
 *
 * Uses binary search with lo/hi bounds. The keyframe_count parameter is passed
 * in EDI (register arg @<edi>).
 *
 * Asserts:
 *   keyframe_count > 1
 *   keyframe_frame_indices != NULL
 *   keyframe_frame_indices[0] > 0
 *   target_frame_index >= 0 && target_frame_index <
 * keyframe_frame_indices[keyframe_count-1] Infinite loop killer at 200
 * iterations Post-search: keyframe_index >= 0 && keyframe_index <
 * keyframe_count-1 Post-search: target in range [keyframe_frame_indices[i],
 * keyframe_frame_indices[i+1])
 *
 * Confirmed: keyframe_count@<edi> register arg per disassembly.
 * Confirmed: Binary search with lo=local_8, hi=keyframe_count-1.
 * Confirmed: RETURNS uint masked to 0xffff (MOVZX EAX,AX pattern at return).
 */
short FUN_00120d10(unsigned short *keyframe_frame_indices,
                   short target_frame_index, short keyframe_count)
{
  int lo;
  int hi;
  int mid;
  short kfc;
  int kf_idx;
  int i;

  kfc = keyframe_count;
  if (kfc < 2) {
    display_assert("keyframe_count>1",
                   "c:\\halo\\SOURCE\\models\\model_animations.c", 0x536, 1);
    system_exit(-1);
  }
  if (keyframe_frame_indices == (void *)0) {
    display_assert("keyframe_frame_indices",
                   "c:\\halo\\SOURCE\\models\\model_animations.c", 0x537, 1);
    system_exit(-1);
  }
  if ((short)keyframe_frame_indices[0] < 1) {
    display_assert("keyframe_frame_indices[0]>0",
                   "c:\\halo\\SOURCE\\models\\model_animations.c", 0x538, 1);
    system_exit(-1);
  }
  if (target_frame_index < 0 ||
      (short)keyframe_frame_indices[kfc - 1] <= target_frame_index) {
    display_assert(
      "target_frame_index>=0 && "
      "target_frame_index<keyframe_frame_indices[keyframe_count-1]",
      "c:\\halo\\SOURCE\\models\\model_animations.c", 0x539, 1);
    system_exit(-1);
  }

  lo = 0;
  hi = kfc - 1;
  i = 0;
  while (1) {
    mid = lo + hi;
    mid = mid >> 1;
    kf_idx = mid;

    if ((short)kf_idx < 0 || kfc <= (short)kf_idx) {
      display_assert("keyframe_index>=0 && keyframe_index<keyframe_count",
                     "c:\\halo\\SOURCE\\models\\model_animations.c", 0x53f, 1);
      system_exit(-1);
    }

    if ((short)(kf_idx + 1) >= kfc ||
        target_frame_index < (short)keyframe_frame_indices[kf_idx + 1]) {
      if (target_frame_index >= (short)keyframe_frame_indices[kf_idx]) {
        break;
      }
      hi = mid;
    } else {
      lo = mid;
    }

    i++;
    if (i > 199) {
      display_assert("++infinite_loop_killer<200",
                     "c:\\halo\\SOURCE\\models\\model_animations.c", 0x54c, 1);
      system_exit(-1);
    }
  }

  if ((short)kf_idx < 0 || kfc - 1 <= (short)kf_idx) {
    display_assert("keyframe_index>=0 && keyframe_index<keyframe_count-1",
                   "c:\\halo\\SOURCE\\models\\model_animations.c", 0x550, 1);
    system_exit(-1);
  }
  if (target_frame_index < (short)keyframe_frame_indices[kf_idx] ||
      (short)keyframe_frame_indices[kf_idx + 1] <= target_frame_index) {
    display_assert(
      "target_frame_index>=keyframe_frame_indices[keyframe_index] && "
      "target_frame_index<keyframe_frame_indices[keyframe_index+1]",
      "c:\\halo\\SOURCE\\models\\model_animations.c", 0x551, 1);
    system_exit(-1);
  }

  return (short)kf_idx;
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

/* floor: the original calls MSVC CRT floor (0x1d9c2b).
 * We provide a local implementation since we don't link the CRT math lib. */
static double anim_floor(double x)
{
  int i = (int)x;
  return (double)((x < (double)i) ? (i - 1) : i);
}

/* animation_get_node_orientations (0x121640) — Interpolate keyframed
 * translation data for a single node in a compressed animation.
 *
 * Given an animation structure, a fractional frame index, a translation
 * keyframe count, a node index, and an output buffer, this function resolves
 * the two bracketing keyframes and either copies the exact keyframe data or
 * interpolates between them using points_interpolate (vec3 lerp).
 *
 * The animation's tag_data (at animation+0xa0) contains:
 *   +0x0c: offset to a per-component packed descriptor array (4 bytes each,
 *          low 12 bits = keyframe_count, high 4 bits = data_offset_index).
 *   +0x10: offset to keyframe_frame_indices (unsigned short array).
 *   +0x14: offset to default_translations (vec3 array, 12 bytes per node).
 *   +0x18: offset to keyframe_data (vec3 array, 12 bytes per keyframe).
 *
 * Three branches for the frame position:
 *   1. Before the first keyframe: interpolate between default_translation[node]
 *      and keyframe_data[0], with kf0_frame=0 and
 * kf1_frame=first_keyframe_frame.
 *   2. At the last keyframe: interpolate between keyframe_data[last] and
 *      default_translation[node], with kf0_frame=last_frame and
 * kf1_frame=last+1.
 *   3. Between two keyframes: binary-search via FUN_00120d10, then interpolate
 *      between the two bracketing keyframe entries.
 *
 * If frame == kf0_frame exactly, copies this_kf_data directly (no blend).
 *
 * Confirmed: cdecl, 5 args, void return.
 * Confirmed: CALL tag_data_get_pointer(animation+0xa0, *(int*)(animation+0x88),
 * 0) at 0x12165b. Confirmed: CALL floor() at 0x12175f (CRT 0x1d9c2b), result
 * truncated to short frame_index. Confirmed: CALL
 * FUN_00120d10(keyframe_frame_indices, frame_index) at 0x12182d with
 * keyframe_count in EDI. Confirmed: CALL points_interpolate(this_kf_data,
 * next_kf_data, blend, out) at 0x121928. Confirmed: Assert strings at 0x5f2,
 * 0x5f4, 0x609, 0x60a, 0x61e, 0x62f, 0x630. Confirmed: Before-first-keyframe
 * branch saves kf0_frame to kf1_frame before zeroing (MOV EBX,EDX at 0x1217fa).
 */
void animation_get_node_orientations(void *animation, float frame,
                                     unsigned short translation_count,
                                     short node_index, void *out_translation)
{
  char *anim;
  char *tag_data_base;
  unsigned int descriptor;
  unsigned short keyframe_count;
  int data_offset_index;
  char *default_translations;
  char *keyframe_data;
  unsigned short *keyframe_frame_indices;
  int frame_count_i;
  float frame_floor_f;
  short frame_index;
  int kf_count_i;
  unsigned short kf0_frame;
  unsigned short kf1_frame;
  char *this_kf_data;
  char *next_kf_data;
  short kf_idx;
  float this_frame_f;
  float blend;

  anim = (char *)animation;

  /* Resolve tag_data pointer */
  tag_data_base =
    (char *)tag_data_get_pointer(anim + 0xa0, *(int *)(anim + 0x88), 0);

  /* Read packed descriptor for this translation component */
  descriptor =
    *(unsigned int *)(tag_data_base + *(int *)(tag_data_base + 0x0c) +
                      (short)translation_count * 4);
  keyframe_count = (unsigned short)(descriptor & 0xfff);
  data_offset_index = (int)(short)(descriptor >> 0xc);

  /* Default translations and keyframe arrays are relative to tag_data_base */
  default_translations = tag_data_base + *(int *)(tag_data_base + 0x14);
  keyframe_data =
    tag_data_base + *(int *)(tag_data_base + 0x18) + data_offset_index * 0xc;
  keyframe_frame_indices =
    (unsigned short *)(tag_data_base + *(int *)(tag_data_base + 0x10) +
                       data_offset_index * 2);

  /* Assert: real_frame_index >= 0.0f */
  if (frame < 0.0f) {
    display_assert("real_frame_index>=0.0f",
                   "c:\\halo\\SOURCE\\models\\model_animations.c", 0x5f2, 1);
    system_exit(-1);
  }

  /* Assert: real_frame_index < (real)animation->frame_count */
  frame_count_i = (int)*(short *)(anim + 0x22);
  if (frame >= (float)frame_count_i) {
    display_assert("real_frame_index<(real)animation->frame_count",
                   "c:\\halo\\SOURCE\\models\\model_animations.c", 0x5f4, 1);
    system_exit(-1);
  }

  /* If keyframe_count == 0, return the default translation for this node */
  if (keyframe_count == 0) {
    char *def = default_translations + (int)node_index * 0xc;
    int *out = (int *)out_translation;
    out[0] = *(int *)(def);
    out[1] = *(int *)(def + 4);
    out[2] = *(int *)(def + 8);
    return;
  }

  /* Compute integer frame index from floor(frame) */
  frame_floor_f = (float)anim_floor((double)frame);
  frame_index = (short)(int)frame_floor_f;

  /* Assert: frame_index >= 0 && frame_index <=
   * keyframe_frame_indices[keyframe_count-1] */
  kf_count_i = (int)(short)keyframe_count;
  if (frame_index < 0 ||
      (int)frame_index >
        (int)(unsigned int)keyframe_frame_indices[kf_count_i - 1]) {
    display_assert(
      "frame_index>=0 && frame_index<=keyframe_frame_indices[keyframe_count-1]",
      "c:\\halo\\SOURCE\\models\\model_animations.c", 0x609, 1);
    system_exit(-1);
  }

  /* Assert: keyframe_frame_indices[keyframe_count-1] == animation->frame_count
   * - 1 */
  if ((unsigned int)keyframe_frame_indices[kf_count_i - 1] !=
      (unsigned int)((int)*(short *)(anim + 0x22) - 1)) {
    display_assert(
      "keyframe_frame_indices[keyframe_count-1]==animation->frame_count-1",
      "c:\\halo\\SOURCE\\models\\model_animations.c", 0x60a, 1);
    system_exit(-1);
  }

  /* Determine which two keyframes bracket the current frame */
  kf0_frame = keyframe_frame_indices[0];

  if ((int)frame_index < (int)(unsigned int)kf0_frame) {
    /* Before the first keyframe: interpolate default -> first keyframe */
    this_kf_data = default_translations + (int)node_index * 0xc;
    kf1_frame = kf0_frame;
    kf0_frame = 0;
    next_kf_data = keyframe_data;
  } else {
    kf1_frame = keyframe_frame_indices[kf_count_i - 1];

    if ((int)frame_index == (int)(unsigned int)kf1_frame) {
      /* At the last keyframe: interpolate last keyframe -> default */
      this_kf_data = keyframe_data + (kf_count_i - 1) * 0xc;
      kf0_frame = kf1_frame;
      kf1_frame = kf1_frame + 1;
      next_kf_data = default_translations + (int)node_index * 0xc;
    } else {
      /* Between two keyframes: binary search */
      kf_idx = FUN_00120d10(keyframe_frame_indices, (short)(int)frame_floor_f,
                            keyframe_count);

      if (kf_idx < 0 || (int)kf_idx >= kf_count_i - 1) {
        display_assert("keyframe_index>=0 && keyframe_index<keyframe_count-1",
                       "c:\\halo\\SOURCE\\models\\model_animations.c", 0x61e,
                       1);
        system_exit(-1);
      }

      kf0_frame = keyframe_frame_indices[(int)kf_idx];
      kf1_frame = keyframe_frame_indices[(int)kf_idx + 1];
      this_kf_data = keyframe_data + (int)kf_idx * 0xc;
      next_kf_data = this_kf_data + 0xc;
    }
  }

  /* If frame == this_keyframe_frame exactly, copy directly */
  this_frame_f = (float)(int)(short)kf0_frame;
  if (frame == this_frame_f) {
    int *out = (int *)out_translation;
    out[0] = *(int *)(this_kf_data);
    out[1] = *(int *)(this_kf_data + 4);
    out[2] = *(int *)(this_kf_data + 8);
    return;
  }

  /* Compute blend factor and interpolate */
  blend = (frame - this_frame_f) /
          (float)((int)(short)kf1_frame - (int)(short)kf0_frame);

  /* Assert: real_frame_index >= (real)this_keyframe_frame_index */
  if (frame < this_frame_f) {
    display_assert("real_frame_index>=(real)this_keyframe_frame_index",
                   "c:\\halo\\SOURCE\\models\\model_animations.c", 0x62f, 1);
    system_exit(-1);
  }

  /* Assert: real_frame_index < (real)next_keyframe_frame_index */
  if (frame >= (float)(int)(short)kf1_frame) {
    display_assert("real_frame_index< (real)next_keyframe_frame_index",
                   "c:\\halo\\SOURCE\\models\\model_animations.c", 0x630, 1);
    system_exit(-1);
  }

  points_interpolate((float *)this_kf_data, (float *)next_kf_data, blend,
               (float *)out_translation);
}

/* overlay_animation_apply_continuous_scaled (0x121940) — Interpolate keyframed
 * scale data for a single node in a compressed animation.
 *
 * Scalar (single-float) sibling of animation_get_node_orientations. Resolves
 * the two bracketing keyframes for a given fractional frame and either copies
 * the exact keyframe scale or interpolates between two scales using
 * scalars_interpolate (scalar lerp).
 *
 * The animation's tag_data (at animation+0xa0) contains:
 *   +0x1c: offset to a per-component packed descriptor array (4 bytes each,
 *          low 12 bits = keyframe_count, high 4 bits = data_offset_index).
 *   +0x20: offset to keyframe_frame_indices (unsigned short array).
 *   +0x24: offset to default_scales (float array, 4 bytes per node).
 *   +0x28: offset to keyframe_data (float array, 4 bytes per keyframe).
 *
 * Branch structure mirrors animation_get_node_orientations:
 *   1. Before first keyframe: lerp default_scale[scale_count] -> keyframe[0]
 *      with kf0_frame=0, kf1_frame=first_keyframe_frame.
 *   2. At last keyframe: lerp keyframe[last] -> default_scale[scale_count]
 *      with kf0_frame=last_frame, kf1_frame=last_frame+1.
 *   3. Between keyframes: binary search via FUN_00120d10, then lerp the two
 *      bracketing keyframe entries.
 * If frame == kf0_frame exactly, copy this_kf_scale directly (no blend).
 *
 * Note: scale_count is a 16-bit selector indexing the descriptor array; it
 * is also used as the index into default_scales (default_scales[scale_count]).
 * After masking the descriptor, the local param_3 slot holds keyframe_count.
 *
 * Confirmed: cdecl, 5 args, void return (stack cleanup ADD ESP,0x10 at
 * 0x121c1c). Confirmed: CALL tag_data_get_pointer(animation+0xa0,
 * *(int*)(animation+0x88), 0) at 0x12195b. Confirmed: CALL anim_floor (CRT
 * 0x1d9c2b) at 0x121a4f via push double + FSTP [ESP]. Confirmed: CALL
 * FUN_00120d10(keyframe_frame_indices=EBX, target_frame=ECX,
 *            keyframe_count@<edi>=[EBP+0x10]) at 0x121b1d.
 * Confirmed: CALL scalars_interpolate(this_kf, next_kf, blend, out) at 0x121c17.
 * Confirmed: Assert lines 0x64a, 0x64c, 0x662, 0x663, 0x677, 0x688, 0x689
 *            (the 0x64e "keyframe_count>=0" assert is dead after the &0xfff
 *            mask and was eliminated by the optimizer).
 * Confirmed: Element size 4 bytes (float) — LEA EDX+EAX*0x4 at 0x121a39.
 * Confirmed: Frame indices array stride 2 bytes — LEA ECX+EAX*0x2 at 0x121a44.
 */
void overlay_animation_apply_continuous_scaled(void *animation, float frame,
                                               unsigned short scale_count,
                                               short node_index,
                                               void *out_scale)
{
  char *anim;
  char *tag_data_base;
  unsigned int descriptor;
  unsigned short keyframe_count;
  int data_offset_index;
  char *default_scales;
  char *keyframe_data;
  unsigned short *keyframe_frame_indices;
  int frame_count_i;
  float frame_floor_f;
  short frame_index;
  int kf_count_i;
  unsigned short kf0_frame;
  unsigned short kf1_frame;
  float this_kf_scale;
  float next_kf_scale;
  short kf_idx;
  float this_frame_f;
  float blend;
  (void)node_index;

  anim = (char *)animation;

  /* Resolve tag_data pointer */
  tag_data_base =
    (char *)tag_data_get_pointer(anim + 0xa0, *(int *)(anim + 0x88), 0);

  /* Read packed descriptor for this scale component */
  descriptor =
    *(unsigned int *)(tag_data_base + *(int *)(tag_data_base + 0x1c) +
                      (short)scale_count * 4);
  keyframe_count = (unsigned short)(descriptor & 0xfff);
  data_offset_index = (int)(short)(descriptor >> 0xc);

  /* Default scales and keyframe arrays are relative to tag_data_base */
  default_scales = tag_data_base + *(int *)(tag_data_base + 0x24);
  keyframe_data =
    tag_data_base + *(int *)(tag_data_base + 0x28) + data_offset_index * 4;
  keyframe_frame_indices =
    (unsigned short *)(tag_data_base + *(int *)(tag_data_base + 0x20) +
                       data_offset_index * 2);

  /* Assert: real_frame_index >= 0.0f */
  if (frame < 0.0f) {
    display_assert("real_frame_index>=0.0f",
                   "c:\\halo\\SOURCE\\models\\model_animations.c", 0x64a, 1);
    system_exit(-1);
  }

  /* Assert: real_frame_index < (real)animation->frame_count */
  frame_count_i = (int)*(short *)(anim + 0x22);
  if (frame >= (float)frame_count_i) {
    display_assert("real_frame_index<(real)animation->frame_count",
                   "c:\\halo\\SOURCE\\models\\model_animations.c", 0x64c, 1);
    system_exit(-1);
  }

  /* If keyframe_count == 0, return the default scale for this slot */
  if (keyframe_count == 0) {
    *(int *)out_scale = *(int *)(default_scales + (int)(short)scale_count * 4);
    return;
  }

  /* Compute integer frame index from floor(frame) */
  frame_floor_f = (float)anim_floor((double)frame);
  frame_index = (short)(int)frame_floor_f;

  /* Assert: frame_index >= 0 && frame_index <=
   * keyframe_frame_indices[keyframe_count-1] */
  kf_count_i = (int)(short)keyframe_count;
  if (frame_index < 0 ||
      (int)frame_index >
        (int)(unsigned int)keyframe_frame_indices[kf_count_i - 1]) {
    display_assert(
      "frame_index>=0 && frame_index<=keyframe_frame_indices[keyframe_count-1]",
      "c:\\halo\\SOURCE\\models\\model_animations.c", 0x662, 1);
    system_exit(-1);
  }

  /* Assert: keyframe_frame_indices[keyframe_count-1] == animation->frame_count
   * - 1 */
  if ((unsigned int)keyframe_frame_indices[kf_count_i - 1] !=
      (unsigned int)((int)*(short *)(anim + 0x22) - 1)) {
    display_assert(
      "keyframe_frame_indices[keyframe_count-1]==animation->frame_count-1",
      "c:\\halo\\SOURCE\\models\\model_animations.c", 0x663, 1);
    system_exit(-1);
  }

  /* Determine which two keyframes bracket the current frame */
  kf0_frame = keyframe_frame_indices[0];

  if ((int)frame_index < (int)(unsigned int)kf0_frame) {
    /* Before the first keyframe: interpolate default_scale -> first keyframe */
    this_kf_scale = *(float *)(default_scales + (int)(short)scale_count * 4);
    next_kf_scale = *(float *)keyframe_data;
    kf1_frame = kf0_frame;
    kf0_frame = 0;
  } else {
    kf1_frame = keyframe_frame_indices[kf_count_i - 1];

    if ((int)frame_index == (int)(unsigned int)kf1_frame) {
      /* At the last keyframe: interpolate last keyframe -> default_scale */
      this_kf_scale = *(float *)(keyframe_data + (kf_count_i - 1) * 4);
      next_kf_scale = *(float *)(default_scales + (int)(short)scale_count * 4);
      kf0_frame = kf1_frame;
      kf1_frame = kf1_frame + 1;
    } else {
      /* Between two keyframes: binary search */
      kf_idx = FUN_00120d10(keyframe_frame_indices, (short)(int)frame_floor_f,
                            keyframe_count);

      if (kf_idx < 0 || (int)kf_idx >= kf_count_i - 1) {
        display_assert("keyframe_index>=0 && keyframe_index<keyframe_count-1",
                       "c:\\halo\\SOURCE\\models\\model_animations.c", 0x677,
                       1);
        system_exit(-1);
      }

      kf0_frame = keyframe_frame_indices[(int)kf_idx];
      kf1_frame = keyframe_frame_indices[(int)kf_idx + 1];
      this_kf_scale = *(float *)(keyframe_data + (int)kf_idx * 4);
      next_kf_scale = *(float *)(keyframe_data + ((int)kf_idx + 1) * 4);
    }
  }

  /* If frame == this_keyframe_frame exactly, copy directly */
  this_frame_f = (float)(int)(short)kf0_frame;
  if (frame == this_frame_f) {
    *(float *)out_scale = this_kf_scale;
    return;
  }

  /* Compute blend factor and interpolate */
  blend = (frame - this_frame_f) /
          (float)((int)(short)kf1_frame - (int)(short)kf0_frame);

  /* Assert: real_frame_index >= (real)this_keyframe_frame_index */
  if (frame < this_frame_f) {
    display_assert("real_frame_index>=(real)this_keyframe_frame_index",
                   "c:\\halo\\SOURCE\\models\\model_animations.c", 0x688, 1);
    system_exit(-1);
  }

  /* Assert: real_frame_index < (real)next_keyframe_frame_index */
  if (frame >= (float)(int)(short)kf1_frame) {
    display_assert("real_frame_index< (real)next_keyframe_frame_index",
                   "c:\\halo\\SOURCE\\models\\model_animations.c", 0x689, 1);
    system_exit(-1);
  }

  scalars_interpolate(this_kf_scale, next_kf_scale, blend, (float *)out_scale);
}

/* FUN_00121d60 (0x121d60) — Decode a single animation frame into per-node
 * rotation/translation/scale data.
 *
 * For each node in the animation (animation+0x2c count), decodes rotation
 * (quaternion), translation (vec3), and scale (float) from either:
 *   - Compressed keyframed data (when flag bit 0 is set and compression is
 *     active), using
 * FUN_00121330/animation_get_node_orientations/overlay_animation_apply_continuous_scaled
 * interpolators.
 *   - Uncompressed frame data via quaternion_decompress_8byte/quaternion_decompress_6byte or
 * raw memcpy from default data (animation+0x98).
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
 * Confirmed: CALL FUN_00120500 at 0x121dca and 0x121fd5 (2 args: animation,
 * frame_index). Confirmed: CALL quaternion_decompress_8byte at 0x121e63 and
 * 0x121e9d (2 args: src_shorts, dest_floats). Confirmed: CALL quaternion_decompress_6byte at
 * 0x121e89 (2 args: compressed_data, dest_floats). Confirmed: CALL
 * sphere_intersects_rectangle3d at 0x121e8f (1 arg: quaternion). Confirmed:
 * CALL FUN_00121330 at 0x121e51 (5 args: animation, frame_float, count, node,
 * out). Confirmed: CALL animation_get_node_orientations at 0x121edc (5 args:
 * animation, frame_float, count, node, out). Confirmed: CALL
 * overlay_animation_apply_continuous_scaled at 0x121f78 (5 args: animation,
 * frame_float, count, node, out). Confirmed: CALL FUN_00123aa0 at 0x12204a (2
 * args: mode_tag, out_node_data).
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
            quaternion_decompress_6byte((void *)(local_8[1] + sVar5 * 6 + (int)local_8),
                         (float *)iVar7);
            sphere_intersects_rectangle3d((float *)iVar7);
          } else {
            quaternion_decompress_8byte((short *)local_c, (float *)iVar7);
            local_c = (int *)((char *)local_c + 8);
          }
        } else if (bVar2) {
          FUN_00121330(animation, (float)(int)(short)animation_index,
                       (unsigned short)local_10, sVar5, (void *)iVar7);
          local_10 = local_10 + 1;
        } else {
          quaternion_decompress_8byte((short *)local_8, (float *)iVar7);
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
          animation_get_node_orientations(
            animation, (float)(int)(short)animation_index,
            (unsigned short)local_18, sVar5, (void *)(iVar7 + 0x10));
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
          overlay_animation_apply_continuous_scaled(
            animation, (float)(int)(short)animation_index,
            (unsigned short)local_24, sVar5, (void *)(iVar7 + 0x1c));
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
        display_assert("compressed || (byte *)data-(byte "
                       "*)animation_get_frame_data(animation, "
                       "frame_index)==animation->frame_size",
                       "c:\\halo\\SOURCE\\models\\model_animations.c", 0x141,
                       1);
        system_exit(-1);
      }
      if ((int)local_c - *(int *)(param_2 + 0x98) != *(int *)(param_2 + 0x8c)) {
        display_assert("compressed || (byte *)default_data-(byte "
                       "*)animation_get_default_data(animation)==animation->"
                       "default_data.size",
                       "c:\\halo\\SOURCE\\models\\model_animations.c", 0x142,
                       1);
        system_exit(-1);
      }
    }
  } else {
    FUN_00123aa0(mode_tag, out_node_data);
  }
}

/* FUN_00123aa0 (0x123aa0) — Fill default node transforms from mode tag.
 *
 * Iterates over the nodes in a model mode tag (tag block at mode_tag+0xb8,
 * element size 0x9c). For each node, copies the default rotation quaternion
 * from element+0x34 (4 floats) and default translation from element+0x28
 * (3 floats) into the output node_data array (stride 0x20 per node).
 * Sets scale to 1.0f for each node.
 *
 * This is the fallback path used by FUN_00121d60 when the animation type is
 * nonzero or the mode_tag node count doesn't match the animation.
 *
 * Confirmed: cdecl, 2 args (mode_tag ptr, out_node_data ptr).
 * Confirmed: CALL tag_block_get_element(mode_tag+0xb8, index, 0x9c) at
 * 0x123ac7. Confirmed: Copies element+0x34..0x43 (rotation) to out+0x00..0x0F.
 * Confirmed: Copies element+0x28..0x33 (translation) to out+0x10..0x1B.
 * Confirmed: Sets out+0x1c = 0x3f800000 (1.0f scale).
 * Confirmed: Loop counter is short via MOVSX at 0x123b0c; compared to [EDI] at
 * 0x123b19.
 */
void FUN_00123aa0(void *mode_tag, void *out_node_data)
{
  int param_1;
  int param_2;
  short sVar1;
  int iVar4;
  char *element;
  int *out;

  param_1 = (int)mode_tag;
  param_2 = (int)out_node_data;
  iVar4 = 0;
  sVar1 = 0;

  if (0 < *(int *)(param_1 + 0xb8)) {
    do {
      element =
        (char *)tag_block_get_element((void *)(param_1 + 0xb8), iVar4, 0x9c);
      out = (int *)(param_2 + iVar4 * 0x20);

      /* rotation quaternion from element+0x34 */
      out[0] = *(int *)(element + 0x34);
      out[1] = *(int *)(element + 0x38);
      out[2] = *(int *)(element + 0x3c);
      out[3] = *(int *)(element + 0x40);

      /* translation from element+0x28 */
      out[4] = *(int *)(element + 0x28);
      out[5] = *(int *)(element + 0x2c);
      out[6] = *(int *)(element + 0x30);

      /* scale = 1.0f */
      out[7] = 0x3f800000;

      sVar1 = sVar1 + 1;
      iVar4 = (int)sVar1;
    } while (iVar4 < *(int *)(param_1 + 0xb8));
  }
}
