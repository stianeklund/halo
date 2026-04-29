/* bipeds.c — biped-specific unit functions.
 *
 * Corresponds to bipeds.obj. Functions sorted by XBE address.
 * Debug assertion path: c:\halo\SOURCE\units\bipeds.c
 */

#include "../../common.h"

/* biped_get_camera_height_and_offset (0x1a0890)
 *
 * Computes camera height parameters for a biped unit. Gets the world position
 * into out_pos, optionally adds the biped tag's camera height to the Z
 * coordinate, then calculates a height offset based on the biped's crouch
 * state and tag parameters.
 *
 * Confirmed: CALL 0x13d680 (object_get_and_verify_type) with type 1 (biped
 * only). Confirmed: CALL 0x1ba140 (tag_get) with 'bipd' signature (0x62697064).
 * Confirmed: CALL 0x1412f0 (object_get_world_position).
 * Confirmed: Tag offsets 0x2f4 (flags), 0x424 (crouch height), 0x428 (stand
 * height), 0x42c (camera height). Confirmed: Unit offset 0x1c8 (unk_456 datum
 * handle), 0x4 (object flags), 0x464 (crouch fraction).
 */
void biped_get_camera_height_and_offset(int unit_handle, vector3_t *out_pos,
                                        float *out_height_offset,
                                        float *out_camera_height)
{
  char *unit_obj;
  char *biped_tag;
  float crouch_fraction;
  float crouching_height;
  float standing_height;
  float camera_height;

  unit_obj = (char *)object_get_and_verify_type(unit_handle, 1);
  biped_tag = (char *)tag_get(0x62697064, *(int *)unit_obj);

  object_get_world_position(unit_handle, out_pos);

  /* Add camera height to Z position unless flag bit 3 is set */
  if ((*(uint8_t *)(biped_tag + 0x2f4) & 0x8) == 0) {
    out_pos->z = *(float *)(biped_tag + 0x42c) + out_pos->z;
  }

  /* Calculate height offset based on crouch state */
  if ((*(uint8_t *)(biped_tag + 0x2f4) & 0x10) == 0 &&
      (*(int *)(unit_obj + 0x1c8) != -1 ||
       (*(uint32_t *)(unit_obj + 0x4) & 0x400000) != 0)) {
    /* Unit has actor or is player-controlled: interpolate based on crouch */
    standing_height = *(float *)(biped_tag + 0x428);
    crouching_height = *(float *)(biped_tag + 0x424);
    camera_height = *(float *)(biped_tag + 0x42c);
    crouch_fraction = *(float *)(unit_obj + 0x464);

    *out_height_offset =
      (standing_height - crouching_height) * crouch_fraction +
      crouching_height - (camera_height + camera_height);
    *out_camera_height = camera_height;
  } else {
    /* No actor and not player-controlled: zero offset */
    *out_height_offset = 0.0f;
    *out_camera_height = *(float *)(biped_tag + 0x42c);
  }
}

/* biped_estimate_position (0x1a1140)
 *
 * Estimates the position of a biped based on the estimate mode. Used for
 * camera targeting, AI perception, and gun positioning. Different modes
 * control whether the position is computed from scratch or transformed.
 *
 * Estimate modes:
 *   0 = none: just get world position + height adjustment
 *   1 = crouching: use 0.0 crouch factor for height
 *   2 = standing: use 1.0 crouch factor for height
 *   3 = gun position: apply facing/offset transform, then height
 *   else: use actual unit crouch fraction
 *
 * Confirmed: CALL 0x13d680 (object_get_and_verify_type) with type 1.
 * Confirmed: CALL 0x1ba140 (tag_get) with 'bipd' signature.
 * Confirmed: CALL 0x1412f0 (object_get_world_position) for mode 0.
 * Confirmed: CALL 0x8d9f0 (display_assert) with bipeds.c path.
 * Confirmed: Tag offsets 0x400 (crouching height), 0x404 (standing height).
 * Confirmed: Unit offset 0x464 (crouch fraction).
 * Confirmed: Globals 0x2533c0 (0.0f), 0x2533c8 (1.0f).
 */
void biped_estimate_position(int unit_handle, int16_t estimate_mode,
                             vector3_t *estimated_body_position,
                             vector3_t *desired_facing,
                             vector3_t *desired_gun_offset,
                             vector3_t *out_position)
{
  char *unit_obj;
  char *biped_tag;
  float crouch_value;
  float neg_facing_y;
  float facing_x;
  float gun_y;

  unit_obj = (char *)object_get_and_verify_type(unit_handle, 1);
  biped_tag = (char *)tag_get(0x62697064, *(int *)unit_obj);

  /* Assert: if estimate_mode != 0, must have estimated_body_position */
  if (estimate_mode != 0 && estimated_body_position == NULL) {
    display_assert("(estimate_mode == _unit_estimate_none) || "
                   "(estimated_body_position != NULL)",
                   "c:\\halo\\SOURCE\\units\\bipeds.c", 0x2f7, true);
    system_exit(-1);
  }

  /* Assert: if estimate_mode == 3, must have desired_facing */
  if (estimate_mode == 3) {
    if (desired_facing == NULL) {
      display_assert("(estimate_mode != _unit_estimate_gun_position) || "
                     "(desired_facing != NULL)",
                     "c:\\halo\\SOURCE\\units\\bipeds.c", 0x2f8, true);
      system_exit(-1);
    }
  } else if (estimate_mode == 0) {
    /* Mode 0: just get world position and apply height */
    object_get_world_position(unit_handle, out_position);
    goto height_adjustment;
  }

  /* Copy estimated body position to output */
  out_position->x = estimated_body_position->x;
  out_position->y = estimated_body_position->y;
  out_position->z = estimated_body_position->z;

  if (estimate_mode == 3) {
    /* Gun position mode: apply facing rotation and gun offset */
    if (desired_facing == NULL || desired_gun_offset == NULL) {
      display_assert("desired_facing && desired_gun_offset",
                     "c:\\halo\\SOURCE\\units\\bipeds.c", 0x307, true);
      system_exit(-1);
    }

    /* Transform position by facing direction and gun offset.
     * This applies a 2D rotation in XY plane using the facing vector,
     * then adds the Z component of the gun offset. */
    neg_facing_y = -desired_facing->y;
    facing_x = desired_facing->x;

    /* Apply gun offset X component along facing direction */
    out_position->x =
      desired_gun_offset->x * desired_facing->x + out_position->x;
    out_position->y =
      desired_gun_offset->x * desired_facing->y + out_position->y;
    out_position->z =
      desired_gun_offset->x * desired_facing->z + out_position->z;

    /* Apply gun offset Y component perpendicular to facing (rotated 90 deg) */
    gun_y = desired_gun_offset->y;
    out_position->x = gun_y * neg_facing_y + out_position->x;
    out_position->y = gun_y * facing_x + out_position->y;
    /* Note: gun_y * 0.0 for Z is optimized out, just add gun offset Z */
    out_position->z = gun_y * 0.0f + out_position->z;
    out_position->z = out_position->z + desired_gun_offset->z;
    return;
  }

height_adjustment:
  /* Calculate crouch value based on estimate mode */
  if (estimate_mode == 1) {
    crouch_value = 0.0f; /* Crouching */
  } else if (estimate_mode == 2) {
    crouch_value = 1.0f; /* Standing */
  } else {
    crouch_value = *(float *)(unit_obj + 0x464); /* Actual crouch fraction */
  }

  /* Interpolate height between crouching and standing based on crouch value */
  out_position->z = crouch_value * *(float *)(biped_tag + 0x404) +
                    (1.0f - crouch_value) * *(float *)(biped_tag + 0x400) +
                    out_position->z;
}
