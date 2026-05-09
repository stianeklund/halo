#include "common.h"

/* Global flag for compressed animation data support (0x322600, init = 1) */
#define animation_compressed_data_enabled (*(volatile char *)0x322600)

/* animation_build_node_data (0x121d60) — Decode a single animation frame
 * into per-node rotation/translation/scale data.
 *
 * For each node (up to animation+0x2c count), decodes:
 *   - Rotation quaternion (4 floats) at output + node*0x20 + 0x00
 *   - Translation vector (3 floats) at output + node*0x20 + 0x10
 *   - Scale factor (1 float) at output + node*0x20 + 0x1c
 *
 * Three bitfields at animation+0x5c/0x6c/0x7c (indexed by node/32)
 * control whether each node's rotation, translation, and scale are
 * "animated" (stored in frame data) or "default" (stored in default data).
 * If animation is compressed (flag byte at animation+0x3a bit 0 and global
 * animation_compressed_data_enabled != 0), keyframed interpolation functions
 * are used instead.
 * If the animation type (animation+0x20) is nonzero or node counts
 * mismatch, falls back to FUN_00123aa0 (set default data from model tag).
 *
 * Confirmed: assert strings reference "animation_get_frame_data",
 *   "animation_get_default_data", and "animation->frame_size".
 * Confirmed: source path "c:\halo\SOURCE\models\model_animations.c" line 0x141.
 * Confirmed: push-then-fstp pattern for float arg to 0x121330/121640/121940.
 */

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

void FUN_00121d60(void *model_tag, void *animation, int frame_index,
                  void *out_node_data)
{
  char *frame_data;
  char *default_data;
  int rotation_counter;
  unsigned int rotation_animated;
  int translation_counter;
  unsigned int translation_keyframed;
  unsigned int scale_keyframed;
  int scale_counter;
  int local_24;
  unsigned int unode;
  char compressed;
  char *node_out;
  short snode;
  char *src;

  unode = 0;

  if (*(short *)((char *)animation + 0x20) == 0 &&
      (model_tag == 0 ||
       ((*(int *)((char *)animation + 0x28) == 0 ||
         *(int *)((char *)animation + 0x28) == *(int *)((char *)model_tag + 4) ||
         *(int *)((char *)model_tag + 4) == 0) &&
        *(int *)((char *)model_tag + 0xb8) ==
            (int)*(short *)((char *)animation + 0x2c)))) {

    /* Determine if animation data is compressed */
    if ((*(unsigned char *)((char *)animation + 0x3a) & 1) == 0 ||
        (animation_compressed_data_enabled == '\0' &&
         *(int *)((char *)animation + 0x88) != 0)) {
      compressed = 0;
    } else {
      compressed = 1;
    }

    frame_data = (char *)FUN_00120500(animation, frame_index);
    default_data = *(char **)((char *)animation + 0x98);
    rotation_counter = 0;
    translation_counter = 0;
    scale_counter = 0;

    if (0 < *(short *)((char *)animation + 0x2c)) {
      do {
        snode = (short)unode;
        node_out = (char *)out_node_data + (int)snode * 0x20;

        /* Reload bitfields every 32 nodes */
        if ((unode & 0x1f) == 0) {
          local_24 = (int)(snode >> 5);
          translation_keyframed =
              *(unsigned int *)((char *)animation + 0x5c + local_24 * 4);
          rotation_animated =
              *(unsigned int *)((char *)animation + 0x6c + local_24 * 4);
          scale_keyframed =
              *(unsigned int *)((char *)animation + 0x7c + local_24 * 4);
        }

        /* --- Rotation --- */
        if ((rotation_animated & 1) != 0) {
          if (compressed) {
            local_24 = (int)(short)frame_index;
            FUN_00121330(animation, (float)local_24, rotation_counter,
                         unode, node_out);
            rotation_counter = rotation_counter + 1;
          } else {
            FUN_00120810(frame_data, node_out);
            frame_data = frame_data + 8;
          }
        } else {
          if (compressed) {
            src = frame_data + *(int *)(frame_data + 4) +
                  snode * 6;
            FUN_00120870(src, node_out);
            FUN_0010ca30(node_out);
          } else {
            FUN_00120810(default_data, node_out);
            default_data = default_data + 8;
          }
        }
        rotation_animated = rotation_animated >> 1;

        /* --- Translation --- */
        if ((translation_keyframed & 1) != 0) {
          if (compressed) {
            local_24 = (int)(short)frame_index;
            FUN_00121640(animation, (float)local_24, translation_counter,
                         unode, (void *)(node_out + 0x10));
            translation_counter = translation_counter + 1;
          } else {
            *(int *)(node_out + 0x10) = *(int *)frame_data;
            *(int *)(node_out + 0x14) = *(int *)(frame_data + 4);
            *(int *)(node_out + 0x18) = *(int *)(frame_data + 8);
            frame_data = frame_data + 0xc;
          }
        } else {
          if (compressed) {
            src = frame_data + *(int *)(frame_data + 0x14) +
                  snode * 0xc;
            *(int *)(node_out + 0x10) = *(int *)src;
            *(int *)(node_out + 0x14) = *(int *)(src + 4);
            *(int *)(node_out + 0x18) = *(int *)(src + 8);
          } else {
            *(int *)(node_out + 0x10) = *(int *)default_data;
            *(int *)(node_out + 0x14) = *(int *)(default_data + 4);
            *(int *)(node_out + 0x18) = *(int *)(default_data + 8);
            default_data = default_data + 0xc;
          }
        }
        translation_keyframed = translation_keyframed >> 1;

        /* --- Scale --- */
        if ((scale_keyframed & 1) != 0) {
          if (compressed) {
            local_24 = (int)(short)frame_index;
            FUN_00121940(animation, (float)local_24, scale_counter,
                         unode, (void *)(node_out + 0x1c));
            scale_counter = scale_counter + 1;
          } else {
            *(int *)(node_out + 0x1c) = *(int *)frame_data;
            frame_data = frame_data + 4;
          }
        } else {
          if (compressed) {
            *(unsigned int *)(node_out + 0x1c) = 0x3f800000;
          } else {
            *(int *)(node_out + 0x1c) = *(int *)default_data;
            default_data = default_data + 4;
          }
        }
        scale_keyframed = scale_keyframed >> 1;

        unode = unode + 1;
      } while ((short)unode < *(short *)((char *)animation + 0x2c));
    }

    /* Verify data consumption for uncompressed case */
    if (!compressed) {
      if ((int)(frame_data -
               (char *)FUN_00120500(animation, frame_index)) !=
          (int)*(short *)((char *)animation + 0x24)) {
        display_assert(
          "compressed || (byte *)data-(byte *)animation_get_frame_data"
          "(animation, frame_index)==animation->frame_size",
          "c:\\halo\\SOURCE\\models\\model_animations.c", 0x141, 1);
        system_exit(-1);
      }
      if ((int)(default_data - *(char **)((char *)animation + 0x98)) !=
          *(int *)((char *)animation + 0x8c)) {
        display_assert(
          "compressed || (byte *)default_data-(byte *)animation_get_"
          "default_data(animation)==animation->default_data.size",
          "c:\\halo\\SOURCE\\models\\model_animations.c", 0x142, 1);
        system_exit(-1);
        return;
      }
    }
  } else {
    FUN_00123aa0(model_tag, out_node_data);
  }
}
