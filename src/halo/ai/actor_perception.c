/* actor_perception.c — AI actor perception and prop evaluation.
 *
 * Corresponds to actor_perception.obj.
 * Assertion path: c:\halo\SOURCE\ai\actor_perception.c
 */

#include "../../common.h"
#include "../../x87_math.h"

/* actor_move_halt: set actor movement destination or refresh path.
 *
 * If the actor is moving-to-point (field_15e == 4) and has a pending
 * destination (field_504 != 0), delegates to actor_move_to_point with
 * the actor's position at +0x12c, surface index at +0x164, and -1.
 *
 * Otherwise sets field_3b8 = -1, copies the 6-dword block from +0x400
 * to +0x46c (after setting field_400 = 1 as a short), and calls
 * actor_path_refresh(actor_handle, 1, NULL).
 *
 * No __FILE__ string. */
void actor_move_halt(int actor_handle)
{
  char *actor;
  int i;

  actor = (char *)datum_get(actor_data, actor_handle);

  if (((actor_t *)actor)->field_15e == 4 &&
      ((actor_t *)actor)->field_504 != '\0') {
    actor_move_to_point(actor_handle, (float *)(actor + 0x12c),
                        ((actor_t *)actor)->field_164, -1);
    return;
  }

  ((actor_t *)actor)->firing_positions_current_position_index = -1;

  if (((actor_t *)actor)->field_46c != 1) {
    ((actor_t *)actor)->field_400 = 1;
    for (i = 0; i < 6; i++) {
      *(int *)(actor + 0x46c + i * 4) = *(int *)(actor + 0x400 + i * 4);
    }
  }

  actor_path_refresh(actor_handle, 1, NULL);
}

/* actor_move_halt_at_firing_position (0x2f230): refresh actor path or dispatch
 * to move/firing position.
 *
 * If actor is NOT in move-to-point mode (field_15e != 4):
 *   copies 6-dword block from +0x400 to +0x46c (if not already done),
 *   then calls actor_path_refresh(actor_handle, 1, NULL).
 * If in move-to-point mode and field_3b8 != -1:
 *   calls actor_move_to_firing_position.
 * Otherwise falls through to actor_move_halt. */
void actor_move_halt_at_firing_position(int actor_handle)
{
  char *actor;

  actor = (char *)datum_get(actor_data, actor_handle);

  if (((actor_t *)actor)->field_15e == 4) {
    if (((actor_t *)actor)->firing_positions_current_position_index == -1) {
      actor_move_halt(actor_handle);
      return;
    }
    actor_move_to_firing_position(
      actor_handle, ((actor_t *)actor)->firing_positions_current_position_index,
      0);
    return;
  }

  if (((actor_t *)actor)->field_46c != 1) {
    ((actor_t *)actor)->field_400 = 1;
    memcpy(actor + 0x46c, actor + 0x400, 24);
  }
  actor_path_refresh(actor_handle, 1, NULL);
}

/* actor_perception_acknowledge (0x2f2b0)
 * Acknowledge a damaging prop for an actor. Validates ownership and prop type,
 * clears acknowledgement fields, sets the acknowledged flag, then dispatches
 * to the update function.
 *
 * Asserts: prop->owner_actor_index == actor_index (line 0x40d)
 *          prop_acknowledged(prop) — type in [2,3] (line 0x40e)
 *          prop->orphan_prop_index == NONE (line 0x40f) */
void actor_perception_acknowledge(int actor_handle, int prop_handle,
                                  int param_3, char param_4)
{
  char *prop;

  prop = (char *)datum_get(*(data_t **)0x5ab23c, prop_handle);

  if (*(int *)(prop + 4) != actor_handle) {
    display_assert("prop->owner_actor_index == actor_index",
                   "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0x40d, 1);
    system_exit(-1);
  }

  if (*(short *)(prop + 0x24) < 2 || *(short *)(prop + 0x24) > 3) {
    display_assert("prop_acknowledged(prop)",
                   "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0x40e, 1);
    system_exit(-1);
  }

  if (*(int *)(prop + 0xc) != -1) {
    display_assert("prop->orphan_prop_index == NONE",
                   "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0x40f, 1);
    system_exit(-1);
  }

  *(char *)(prop + 0xba) = 0;
  *(char *)(prop + 0xb9) = 0;
  *(char *)(prop + 0xbb) = 0;
  *(char *)(prop + 0x64) = 1;

  actor_stimulus_prop_acknowledged(actor_handle, prop_handle, param_3, param_4);
}

/* actor_get_perception_knowledge (0x2f380)
 * Returns the engagement level (0-3) for a prop relative to actor.
 * 3 = actively targeting/seen; 2/3 = based on orphan state; 0/1/2 = based
 * on actor awareness level when no prop or no orphan.
 */
uint16_t actor_get_perception_knowledge(int actor_handle, int prop_handle)
{
  char *actor;
  char *prop;
  char *orphan;
  uint16_t r;

  actor = (char *)datum_get(actor_data, actor_handle);
  if (prop_handle != -1) {
    prop = (char *)datum_get(*(data_t **)0x5ab23c, prop_handle);
    if (*(int *)(prop + 4) != actor_handle) {
      display_assert("prop->owner_actor_index == actor_index",
                     "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0x572, 1);
      system_exit(-1);
    }
    if (*(short *)(prop + 0x24) >= 2 && *(short *)(prop + 0x24) <= 3) {
      r = 3;
      goto done;
    }
    if (*(short *)(prop + 0x66) == 1 || *(short *)(prop + 0x66) == 2) {
      r = 3;
      goto done;
    }
    if (*(char *)(prop + 0x60) == 0 &&
        (*(char *)(prop + 0x127) == 0 || ((actor_t *)actor)->field_06a >= 3)) {
      r = 3;
      goto done;
    }
    if (*(int *)(prop + 0xc) != -1) {
      orphan = (char *)datum_get(*(data_t **)0x5ab23c, *(int *)(prop + 0xc));
      r = (uint16_t)((*(char *)(orphan + 0xb8) != 0) + 2);
      if (r != 0xffff) {
        goto done;
      }
    }
  }
  if (((actor_t *)actor)->field_06e >= 2) {
    r = 2;
    goto done;
  }
  r = (uint16_t)(((actor_t *)actor)->field_06a >= 3);
done:
  return r;
}

/* actor_get_vision_distances (0x2f470)
 * Turn an absolute azimuth `angle` (radians off the looking axis) into the
 * actor's certain (out_certain) and full (out_range) vision distances.
 *
 * 'actr' tag floats: +0x1c inner cone, +0x20 outer cone, +0x28 maximum
 * angle, +0x2c peripheral range.  Beyond +0x28 both outputs are 0.  With
 * A = range * light_scale and B = light_scale * tag+0x2c: beyond +0x20 the
 * outputs are B and 0.7f*B capped to 3.5f (when above *(float *)0x253f30);
 * otherwise A is blended toward B from +0x1c to +0x20, and the certain
 * distance 0.7f*A is blended toward that capped 0.7f*B from
 * *(float *)0x2533f0 * (+0x1c) to +0x20.  0x2533c4 = 0.7f.
 * Confirmed: 6 cdecl args (ADD ESP,0x18 at 0x317b2); floats 2-4 (FLD/FMUL
 * [EBP+0xc/0x10/0x14]); out_range ([EBP+0x1c]) is always stored before
 * out_certain ([EBP+0x18]). */
void actor_get_vision_distances(int actor_handle, float range,
                                float light_scale, float angle,
                                float *out_certain, float *out_range)
{
  char *actor;
  char *actor_tag;
  float full_distance;
  float partial_distance;
  float lit_range;
  float peripheral_range;
  float certain_range;
  float peripheral_certain;
  float inner;
  float blend_start;
  float t;

  actor = (char *)datum_get(actor_data, actor_handle);
  actor_tag = (char *)tag_get('actr', ((actor_t *)actor)->field_058);
  if (angle > *(float *)(actor_tag + 0x28)) {
    partial_distance = 0.0f;
    full_distance = 0.0f;
  } else {
    lit_range = range * light_scale;
    peripheral_range = light_scale * *(float *)(actor_tag + 0x2c);
    certain_range = *(float *)0x2533c4 * lit_range;
    peripheral_certain = peripheral_range * *(float *)0x2533c4;
    if (peripheral_certain > *(float *)0x253f30) {
      peripheral_certain = 3.5f;
    }
    if (angle > *(float *)(actor_tag + 0x20)) {
      partial_distance = peripheral_range;
      full_distance = peripheral_certain;
    } else {
      inner = *(float *)(actor_tag + 0x1c);
      blend_start = *(float *)0x2533f0 * inner;
      if (angle < inner) {
        partial_distance = lit_range;
      } else {
        t = (angle - inner) / (*(float *)(actor_tag + 0x20) - inner);
        partial_distance =
          (*(float *)0x2533c8 - t) * lit_range + peripheral_range * t;
      }
      if (angle < blend_start) {
        full_distance = certain_range;
      } else {
        t = (angle - blend_start) /
            (*(float *)(actor_tag + 0x20) - blend_start);
        full_distance =
          (*(float *)0x2533c8 - t) * certain_range + peripheral_certain * t;
      }
    }
  }
  *out_range = partial_distance;
  *out_certain = full_distance;
}

/* actor_perception_qsort_compare_optional_props (0x2f5b0)
 * Compare two prop-like structs by their float[2] field (offset +8).
 * Returns -1, 0, or 1 (strcmp-style).
 */
int actor_perception_qsort_compare_optional_props(int param_1, int param_2)
{
  float f1;
  float f2;

  f1 = *(float *)(param_1 + 8);
  f2 = *(float *)(param_2 + 8);
  if (f1 < f2)
    return -1;
  if (f1 > f2)
    return 1;
  return 0;
}

/* actor_perception_assess_suicide_danger (0x2f5f0)
 * Register args: actor datum handle in EAX, object handle in EDI.
 *
 * Records a new perception entry in the actor's 0x6c-byte block at +0x280
 * when the incoming event beats what is already stored there.  Rejected
 * (returns false) when param_4 >= param_3 + *(float *)0x253f34, or when the
 * stored state at +0x280 is > 1, or the state is exactly 1 and either the
 * stored object at +0x28c is the same object or param_4 >= *(float *)(+0x2d4).
 *
 * On acceptance the block is zeroed, state set to 1, and the object handle,
 * param_3, the object's world position (+0x298) and the object's +0x18..+0x20
 * triple (+0x2a4) are stored, then +0x284 = 6, +0x286 = param_6 and
 * +0x282 = (param_5 == 0).
 *
 * Raw offsets are used for the +0x280 block to match the rest of this file
 * (lines using actor + 0x282 etc.); +0x282 is proven written here.
 *
 * No __FILE__ string. */
bool actor_perception_assess_suicide_danger(int actor_handle /* @<eax> */,
                                            int object_handle /* @<edi> */,
                                            float param_3, float param_4,
                                            char param_5, char param_6)
{
  char *actor;
  char *object;
  int16_t state;
  uint32_t *src;
  uint32_t *dst;
  char result;

  actor = (char *)datum_get(actor_data, actor_handle);
  result = 0;
  if (param_4 < param_3 + *(float *)0x253f34) {
    state = *(int16_t *)(actor + 0x280);
    if (state < 1 || (state == 1 && *(int *)(actor + 0x28c) != object_handle &&
                      param_4 < *(float *)(actor + 0x2d4))) {
      object = (char *)object_get_and_verify_type(object_handle, 3);
      csmemset(actor + 0x280, 0, 0x6c);
      *(int16_t *)(actor + 0x280) = 1;
      *(int *)(actor + 0x28c) = object_handle;
      *(float *)(actor + 0x294) = param_3;
      object_get_world_position(object_handle, (vector3_t *)(actor + 0x298));
      src = (uint32_t *)(object + 0x18);
      dst = (uint32_t *)(actor + 0x2a4);
      dst[0] = src[0];
      dst[1] = src[1];
      dst[2] = src[2];
      *(int16_t *)(actor + 0x284) = 6;
      *(char *)(actor + 0x286) = param_6;
      *(uint16_t *)(actor + 0x282) = (uint16_t)(param_5 == 0);
      return 1;
    }
  }
  return result;
}

/* actor_perception_desire_prop (0x2f6e0)
 * Caller: prop_new_unacknowledged (0x6466c, 13 stack args, ADD ESP,0x34).
 * Returns the desire verdict in BL->AL; *out_flag (when non-NULL) receives the
 * byte at [EBP+0x17].  Binary reads param_2/param_9/param_12 as signed words
 * (CMP word + JL/JG/JLE) and param_10 as a float (FLD [EBP+0x2c]); the kb
 * declaration carries those widths, so no casts are needed below.
 *
 * Unknown thresholds are left as raw constants:
 *   0x255fe0 (max distance_squared), 0x255fdc, 0x255fd8, 0x254e74, 0x254df8,
 *   0x2533c0 (param_10 upper bound). */
bool actor_perception_desire_prop(int actor_handle, int16_t param_2,
                                  int param_3, int param_4,
                                  unsigned char param_5, unsigned char param_6,
                                  unsigned char param_7, unsigned char param_8,
                                  int16_t param_9, float param_10,
                                  float distance_squared, int16_t param_12,
                                  bool *out_flag)
{
  char *actor;
  char *other;
  char *encounter;
  char *object;
  int limit;
  int object_value;
  bool unflagged;
  bool result;
  bool out_value;
  float threshold;

  actor = (char *)datum_get(actor_data, actor_handle);
  if (param_4 == -1) {
    other = NULL;
  } else {
    other = (char *)datum_get(actor_data, param_4);
  }
  out_value = 0;
  if ((!param_7 || param_8) && param_2 >= 4 && param_2 <= 5) {
    result = 0;
  } else if (param_6) {
    result = 1;
  } else if (other != NULL &&
             (*(char *)(other + 8) == 0 || *(char *)(other + 0x13) != 0)) {
    result = 0;
  } else if (param_2 == -1 && (param_5 || param_12 > 0)) {
    result = 1;
  } else if (distance_squared > *(float *)0x255fe0) {
    result = 0;
  } else if (param_8) {
    result = 1;
    if (*(int *)(actor + 0x34) != -1) {
      encounter =
        (char *)datum_get(*(data_t **)0x5ab270, *(int *)(actor + 0x34));
      object = (char *)object_get_and_verify_type(param_3, 3);
      limit = *(int *)(encounter + 0x58);
      if (limit <= *(int *)(actor + 0x3a0)) {
        limit = *(int *)(actor + 0x3a0);
      }
      if (limit != -1) {
        object_value = *(int *)(object + 0x3cc);
        if (object_value == -1 || object_value < limit) {
          result = 0;
        }
      }
      if (*(char *)(encounter + 0x45) == 0 &&
          *(char *)(encounter + 0x44) == 0 &&
          *(char *)(encounter + 0x42) == 0) {
        unflagged = 1;
      } else {
        unflagged = 0;
      }
      if (!result) {
        goto done;
      }
      if (unflagged) {
        result = distance_squared < *(float *)0x255fdc;
        goto done;
      }
    }
    if (param_10 > *(float *)0x2533c0) {
      result = 1;
    } else if (param_7 && param_9 > 0x96) {
      result = 0;
    } else if (actor_get_action_priority_flag(actor_handle) > 1) {
      result = 0;
    } else {
      threshold = *(float *)0x254e74;
      if (!param_7 && *(int16_t *)(actor + 0x6a) < 3) {
        threshold = *(float *)0x254df8;
      }
      result = distance_squared < threshold;
    }
  } else if (param_7) {
    result = 1;
    out_value = distance_squared > *(float *)0x255fd8;
  } else {
    result = distance_squared < *(float *)0x255fdc;
    if (*(int16_t *)(actor + 0x6e) >= 4) {
      out_value = 1;
    } else if (*(char *)(actor + 0x1cc) != 0) {
      out_value = 0;
    } else {
      out_value = distance_squared > *(float *)0x254e74;
    }
  }
done:
  if (out_flag != NULL) {
    *out_flag = out_value;
  }
  return result;
}

/* actor_perception_find_prop_pathfinding_location (0x2f910)
 * Fills prop->pathfinding_surface_index (+0xec) if not already set.
 * If prop has a vehicle handle (+0x110), uses vehicle_get_estimated_position;
 * otherwise if unit is a biped, uses biped_find_pathfinding_surface_index.
 * Output position written to prop->pathfinding_position (+0xf0).
 */
void actor_perception_find_prop_pathfinding_location(int actor_handle,
                                                     int prop_handle)
{
  char *prop;

  prop = (char *)datum_get(*(data_t **)0x5ab23c, prop_handle);
  if (*(int *)(prop + 4) != actor_handle) {
    display_assert("prop->owner_actor_index == actor_index",
                   "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0xe01, 1);
    system_exit(-1);
  }
  if (*(int *)(prop + 0xec) == -1) {
    if (*(int *)(prop + 0x110) != -1) {
      *(int *)(prop + 0xec) = vehicle_get_estimated_position(
        *(int *)(prop + 0x110), (vector3_t *)(prop + 0xf0));
      return;
    }
    if (object_try_and_get_and_verify_type(*(int *)(prop + 0x18), 1) != NULL) {
      *(int *)(prop + 0xec) = biped_find_pathfinding_surface_index(
        *(int *)(prop + 0x18), (vector3_t *)(prop + 0xf0));
    }
  }
}

/* actor_perception_find_killer_prop_index (0x2f9b0)
 * Find the highest-scoring active damaging prop visible to the unit that owns
 * the given prop. Similar to actor_perception_find_recent_damaging_prop_index
 * but uses the prop's owning unit as the source of weapon slots. flag: when
 * non-zero, require prop visibility; when 0, accept any.
 */
int actor_perception_find_killer_prop_index(int actor_handle, int prop_handle,
                                            int flag)
{
  char *prop_rec;
  char *unit;
  char *cand_prop;
  int *slot;
  int score;
  int responsible;
  int cand_handle;
  int best_handle;
  int best_score;
  short count;
  short prop_type;

  prop_rec = (char *)datum_get(*(data_t **)0x5ab23c, prop_handle);
  unit = (char *)object_get_and_verify_type(*(int *)(prop_rec + 0x18), 3);
  best_handle = -1;
  best_score = 0;
  for (count = 0; count < 4; count++) {
    slot = (int *)(unit + 0x3e8) + count * 4;
    score = slot[-2];
    responsible = ai_get_responsible_unit((unsigned int)*slot, 1);
    if (responsible != -1) {
      cand_handle = prop_get_active_by_unit_index(actor_handle, responsible);
      if (cand_handle != -1) {
        cand_prop = (char *)datum_get(*(data_t **)0x5ab23c, cand_handle);
        prop_type = *(short *)(cand_prop + 0x24);
        if (prop_type >= 2 && prop_type <= 3) {
          if (*(char *)(cand_prop + 0x60) != '\0' || (char)flag == '\0') {
            if (score > best_score) {
              best_handle = cand_handle;
              best_score = score;
            }
          }
        }
      }
    }
  }
  return best_handle;
}

/* actor_perception_find_recent_damaging_prop_index (0x2fa70)
 * Find the highest-scoring active damaging prop visible to the actor's unit.
 *
 * Iterates up to 4 weapon slots on the actor's unit object (+0x3e0),
 * calling ai_get_responsible_unit and prop_get_active_by_unit_index for
 * each slot. Selects the prop whose slot score (*slot) is greatest among
 * those with type in [2,3] and either a visibility flag or no-filter mode.
 *
 * param_2 (prefer_visible): when 0, accept props regardless of visibility
 * flag; when non-zero, require prop visibility byte (+0x60) != 0.
 *
 * Returns the best damaging prop handle, or -1 if none found.
 * Asserts damaging_prop_index != 0 (handle 0 is reserved/invalid). */
int actor_perception_find_recent_damaging_prop_index(int actor_handle,
                                                     char prefer_visible)
{
  char *unit;
  char *prop_rec;
  unsigned int *slot;
  int unit_handle;
  int unit_result;
  int prop_handle;
  unsigned int best_score;
  int damaging_prop_index;
  short prop_type;
  short iter;

  unit_handle = *(int *)((char *)datum_get(actor_data, actor_handle) + 0x18);
  damaging_prop_index = -1;
  if (unit_handle != -1) {
    unit = (char *)object_get_and_verify_type(unit_handle, 3);
    best_score = 0;
    for (iter = 0; iter < 4; iter++) {
      slot = (unsigned int *)(unit + 0x3e0) + iter * 4;
      unit_result = ai_get_responsible_unit(slot[2], 1);
      if (unit_result != -1) {
        prop_handle = prop_get_active_by_unit_index(actor_handle, unit_result);
        if (prop_handle != -1) {
          prop_rec = (char *)datum_get(prop_data, prop_handle);
          prop_type = *(short *)(prop_rec + 0x24);
          if (prop_type >= 2 && prop_type <= 3) {
            if (*(char *)(prop_rec + 0x60) != '\0' || prefer_visible == '\0') {
              if (*slot > best_score) {
                damaging_prop_index = prop_handle;
                best_score = *slot;
              }
            }
          }
        }
      }
    }

    if (damaging_prop_index == 0) {
      display_assert("damaging_prop_index != 0x00000000",
                     "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0xe8e, 1);
      system_exit(-1);
    }
  }
  return damaging_prop_index;
}

/* arctangent (0x2fb60) -- FLD [esp+4]; FLD [esp+8]; FPATAN; result in ST0.
 * FPATAN computes atan(ST1/ST0), so this is atan2(y, x) with y the first
 * stack arg. No callers are recorded in the binary. */
float arctangent(float y, float x)
{
  return (float)x87_atan2((double)y, (double)x);
}

/* actor_perception_forget_recent_damage (0x2fb70) — Clear the recent-damage
 * tracking for all props visible to this actor. Resets field +0x74 to 0 and
 * field +0x6c to -1 for each prop in the iterator. */
__declspec(noinline) void
actor_perception_forget_recent_damage(int actor_handle)
{
  int iter[2];
  char *prop;

  prop_iterator_new(iter, actor_handle);
  prop = (char *)prop_iterator_next(iter);
  while (prop != NULL) {
    *(char *)(prop + 0x74) = 0;
    *(int16_t *)(prop + 0x6c) = -1;
    prop = (char *)prop_iterator_next(iter);
  }
}

/* actor_perception_retreat_successful (0x2fbc0) — Clear pursuit/retreat timers
 * for all props tracked by this actor. Zeros fields +0xaa, +0xae, +0xac on
 * each prop datum. */
__declspec(noinline) void actor_perception_retreat_successful(int actor_handle)
{
  int iter[2];
  char *prop;

  datum_get(actor_data, actor_handle);
  prop_iterator_new(iter, actor_handle);
  prop = (char *)prop_iterator_next(iter);
  while (prop != NULL) {
    *(int16_t *)(prop + 0xaa) = 0;
    *(int16_t *)(prop + 0xae) = 0;
    *(int16_t *)(prop + 0xac) = 0;
    prop = (char *)prop_iterator_next(iter);
  }
}

/* actor_compute_prop_unopposable (0x2fc20)
 * Evaluate whether an actor should engage a prop. Checks prop type,
 * visibility flags, and actor state to determine engagement eligibility.
 * Side effects: clears prop tracking fields when engagement drops,
 * and clears actor pursuit fields when target is lost. */
bool actor_compute_prop_unopposable(int actor_handle, int prop_handle)
{
  char *actor;
  char *prop;
  short type;
  char result;

  actor = (char *)datum_get(actor_data, actor_handle);
  prop = (char *)datum_get(*(data_t **)0x5ab23c, prop_handle);
  type = *(short *)(prop + 0x24);
  result = 0;

  /* prop->state */
  if (type >= 2 && type <= 3 && *(char *)(prop + 0x60) != 0 &&
      *(char *)(prop + 0x127) == 0) {
    if (*(short *)(prop + 0x9c) != 0 &&
        (((actor_t *)actor)->target_target_prop_index == prop_handle ||
         ((actor_t *)actor)->field_1ed == 0)) {
      result = 1;
    } else if ((*(char *)(prop + 0x135) != 0 || *(char *)(prop + 0x136) != 0) &&
               ((actor_t *)actor)->field_161 == 0 &&
               ((actor_t *)actor)->field_202 == 0) {
      result = 1;
    } else if (*(short *)(prop + 0x10) == 0xf) {
      result = 1;
    }
  }

  if (*(char *)(prop + 0xa4) != 0 && result == 0) {
    *(uint16_t *)(prop + 0xaa) = 0;
    *(uint16_t *)(prop + 0xae) = 0;
    *(uint16_t *)(prop + 0xac) = 0;
  }

  /* prop->state */
  if (type >= 2 && type <= 3 && result == 0 &&
      ((actor_t *)actor)->field_3a8 > 0 &&
      ((actor_t *)actor)->field_3ac == prop_handle) {
    *(uint16_t *)(actor + 0x3a8) = 0;
    ((actor_t *)actor)->field_3ac = -1;
  }

  *(char *)(prop + 0xa4) = result;
  return result;
}

/* actor_compute_prop_target_weight (0x2fd10)
 * Compute a perception priority score for an actor evaluating a prop.
 * Returns 0.0f immediately if the prop is filtered out by various
 * early-exit conditions. Otherwise computes a score from a vision level,
 * an awareness level, a distance-based term, and optional bonuses.
 * Assertion: "prop_orphaned(prop)" at line 0x1086. */
float actor_compute_prop_target_weight(int actor_handle, int clump_item_handle)
{
  char *actor;
  prop_t *prop;
  char *actr_tag;
  char *actv_tag;
  short vision_level; /* EDI in the binary */
  short awareness; /* EAX in the binary */
  struct {
    int target_weight; /* [EBP-0x14], 0 or 1, then the summed weight */
    int preferred_weight; /* [EBP-0x10], 0 or 2 */
    float bonus_weight; /* [EBP-0xc], 0.0f or 3.0f */
  } weights;
  float actv_threshold;

  actor = (char *)datum_get(actor_data, actor_handle);
  prop = (prop_t *)datum_get(*(data_t **)0x5ab23c, clump_item_handle);

  /* Early-exit conditions: return 0.0f */
  if (prop->ignore != 0 || prop->enemy == 0 ||
      (prop->state >= 0 && prop->state <= 1) ||
      (prop->dead != 0 && prop->dead_ticks >= 0x96) ||
      prop->type == 0xf) {
    return 0.0f;
  }
  /* (single combined filter; the shared 0.0f return is
   * sunk past the main epilogue at 0x30004) */

  actr_tag = (char *)tag_get(0x61637472, ((actor_t *)actor)->field_058);
  actv_tag = (char *)tag_get(0x61637476, ((actor_t *)actor)->field_05c);

  weights.target_weight = 0;
  weights.preferred_weight = 0;
  weights.bonus_weight = 0.0f;

  /* Compute vision_level (cVar4 / EDI) */
  if (*(char *)(actor + 6) != 0) {
    vision_level = 0;
  } else if (prop->field_9c > 0) {
    vision_level = 0;
  } else {
    if (actor_has_ranged_weapon(actor_handle) == 0) {
      /* Actor does not have a weapon in hand */
      if (((actor_t *)actor)->field_378 != 0) {
        actv_threshold = *(float *)(actv_tag + 0x160);
      } else {
        actv_threshold = *(float *)(actv_tag + 0x170);
      }
      if (prop->distance < *(float *)0x253f40) {
        /* prop distance < 2.0f */
        vision_level = 5;
        if (prop->state != 5)
          goto done_vision;
      }
      /* prop distance >= 2.0f (or prop type == 5) */
      if (prop->vehicle_index != -1) {
        vision_level = 0;
      } else if (prop->flying != 0 &&
                 /* melee_leap_velocity */

                 *(float *)(actr_tag + 0x38c) == *(float *)0x2533c0) {
        vision_level = 0;
      } else if ((char)prop->underwater != ((actor_t *)actor)->field_15d) {
        vision_level = 1;
      } else if (prop->distance < actv_threshold) {
        vision_level = 3;
      } else {
        vision_level = 2;
      }
    } else {
      /* Actor has a weapon in hand */
      char *weapon_tag = actor_get_weapon_definition(actor_handle);
      char *actv_tag2 =
        actor_combat_get_firing_variant_definition(actor_handle);

      if (weapon_tag == 0 ||
          /* minimum_target_range */
          !(prop->distance < *(float *)(weapon_tag + 0x40c))) {
        /* prop distance >= weapon range (or no weapon tag) */
        if ((char)prop->underwater != ((actor_t *)actor)->field_15d) {
          vision_level = 2;
        } else {
          if (prop->distance < *(float *)0x253f40) {
            /* 2.0f; a type-5 prop falls through to the range checks below */
            vision_level = 5;
            if (prop->state != 5)
              goto done_vision;
          }
          if (prop->distance < *(float *)(actv_tag2 + 0xa0)) {
            vision_level = 3;
          } else {
            vision_level = 2;
            /* maximum_firing_range */
            if (!(prop->distance < *(float *)(actv_tag2 + 0x74))) {
              vision_level = 1;
            }
          }
        }
      } else {
        vision_level = 2;
      }
    }
  }
done_vision:

  /* Compute awareness (cVar5 / EAX) */
  if (prop->dead != 0) {
    awareness = 1;
  } else if (*(char *)(actor + 6) == 0 && prop->field_74 != 0 &&
             prop->field_9c == 0) {
    awareness = 6;
  } else {
    short prop_type = prop->state;
    if (prop_type >= 2 && prop_type <= 3) {
      if (*(char *)(actor + 6) != 0) {
        awareness = 4;
      } else if (prop->field_9c > 0) {
        awareness = 3;
      } else if (prop->line_of_sight != 0 && prop->line_of_sight != 1) {
        awareness = 3;
      } else if (prop->shooting != 0 && prop->quantized_facing <= 1) {
        awareness = 5;
      } else {
        awareness = 4;
      }
    } else {
      if (prop_type < 4 || prop_type > 5) {
        assert_halt_msg_at("prop_orphaned(prop)",
                           "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0x1086,
                           0);
      }
      if (prop->definitely_located != 0) {
        awareness = 3;
      } else {
        awareness = (prop->state == 4) + 1;
      }
    }
  }

  /* Bonus computations */
  if (((actor_t *)actor)->target_target_prop_index == -1) {
    if (prop->player != 0 ||
        clump_item_handle == ((actor_t *)actor)->field_054) {
      weights.bonus_weight = 3.0f;
    }
  } else if (clump_item_handle ==
               ((actor_t *)actor)->target_target_prop_index &&
             /* combat_status */
             ((actor_t *)actor)->field_06e >= 3) {
    weights.target_weight = 1;
  }

  if (prop->preferred_target != 0) {
    weights.preferred_weight = 2;
  }

  /* Final score computation:
   * score = (int)(bonus_flag + extra_flag + vision_level + awareness) * 10.0f
   *       + 5.0f / (prop->field_11c * 0.1f + 1.0f)
   *       + local_c
   */
  weights.target_weight =
    ((short)weights.target_weight + (short)weights.preferred_weight) +
    (vision_level + awareness);
  /* 0x30092: FADD [EBP-0xc] joins the bonus to the distance term before the
   * FILD/FMUL product is added (a + (b + c), not (a + b) + c). */
  return (float)weights.target_weight * 10.0f +
         (5.0f / (prop->distance * 0.1f + 1.0f) +
          weights.bonus_weight);
}

/* actor_situation_update_target_status (0x300b0)
 * Recompute the actor's cached target status word (+0x268), the auxiliary
 * dword at +0x26c, and the visibility byte at +0x27c from the current target
 * prop's state.
 *
 * With no target (target_target_prop_index == -1) the three fields are reset
 * (status 0, +0x26c = -1, +0x27c = 0) and the function returns.
 *
 * object_get_and_verify_type(prop->field_18, 3) is called BEFORE the
 * "target_prop->enemy" assert at line 0x10c3 (CALL 0x30106, TEST at 0x30114)
 * and its result is only consumed on the non-2/3 tail; the call order is
 * preserved deliberately.
 *
 * The status is a switch on the int16 at prop+0x24 (0..5, default asserts with
 * a NULL reason at line 0x110a).
 *
 * Case 2/3 tail (0x301bd): CMP byte [ESI+0x122],2 / JG selects 8; otherwise
 * FLD [ESI+0x11c] / FCOMP [0x254640] / TEST AH,5 / JP selects 8 when the field
 * is >= the constant (the parity branch is taken when C0 and C2 are both
 * clear), else 9.
 *
 * Case 5 (0x301f8) is NEG AL / SBB EAX,EAX / ADD EAX,4, i.e. 4 minus a bool;
 * case 4 (0x30207) is SETNZ / ADD EAX,5.
 *
 * Tail: when prop+0x24 is in [2,3] the visibility byte is (prop[0x127] == 0)
 * and, if prop's int16 at +0x32 is > 0, +0x26c takes prop's dword at +0x8c
 * and the function returns early. Otherwise the byte is
 * ~(object[0xb6] >> 2) & 1.
 * Assertion: "target_prop->enemy" at line 0x10c3. */
void actor_situation_update_target_status(int actor_handle)
{
  actor_t *actor;
  prop_t *prop;
  char *object;
  short status;

  actor = (actor_t *)datum_get(actor_data, actor_handle);
  if (actor->target_target_prop_index == -1) {
    actor->target_target_type = 0;
    actor->field_26c = -1;
    actor->field_27c = 0;
    return;
  }

  prop = (prop_t *)datum_get(prop_data, actor->target_target_prop_index);
  object = (char *)object_get_and_verify_type(prop->unit_index, 3);
  if (prop->enemy == 0) {
    display_assert("target_prop->enemy",
                   "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0x10c3, true);
    system_exit(-1);
  }

  switch (prop->state) {
  case 0:
    status = 0;
    actor->target_target_prop_index = -1;
    actor->field_26c = -1;
    break;
  case 1:
    status = 1;
    break;
  case 2:
  case 3:
    if (prop->dead != 0) {
      status = 2;
    } else if (prop->field_74 != 0) {
      status = 11;
    } else if (prop->visibility >= 2) {
      status = 10;
    } else if (prop->line_of_sight != 0 && prop->line_of_sight != 1) {
      status = 7;
    } else if (prop->quantized_facing > 2 ||
               !(prop->distance < *(float *)0x254640)) {
      status = 8;
    } else {
      status = 9;
    }
    break;
  case 5:
    if (prop->dead != 0) {
      status = 2;
    } else {
      status = prop->abandoned_search ? 3 : 4;
    }
    break;
  case 4:
    status = (short)((prop->definitely_located != 0) + 5);
    break;
  default:
    display_assert((const char *)0, "c:\\halo\\SOURCE\\ai\\actor_perception.c",
                   0x110a, true);
    system_exit(-1);
    break;
  }

  actor->target_target_type = status;
  if (prop->state >= 2 && prop->state <= 3) {
    actor->field_27c = (char)(prop->dead == 0);
    if (prop->visibility > 0) {
      actor->field_26c = prop->last_visible_time;
      return;
    }
  } else {
    actor->field_27c = (char)(~(*(unsigned char *)(object + 0xb6) >> 2) & 1);
  }
}

/* actor_situation_combat_status_update (0x302b0)
 * Folds the pending status at +0x34a/+0x34c into +0x74/+0x78, then derives
 * the combat status +0x6e = max(+0x74, max(+0x72, table[target_type])),
 * where the int16 table lives at 0x255f18 (indexed by MOVSX [ESI+0x268]).
 * The inner max is evaluated twice in the binary (0x3034f and 0x3036b),
 * i.e. a macro-style nested max.
 * Counters: +0x7c counts ticks with +0x6a >= 3, +0x80 ticks with status != 0,
 * +0x84 ticks with status >= 4 (which also zeroes +0x88); otherwise +0x88
 * increments unless it is -1.  Status >= 7 latches byte +0x8c.
 * Assertion: target_type range at line 0x1138. */
void actor_situation_combat_status_update(int actor_handle)
{
  actor_t *actor;

  actor = (actor_t *)datum_get(actor_data, actor_handle);
  if (actor->field_34a > 0) {
    if (actor->field_074 < actor->field_34a) {
      actor->field_074 = actor->field_34a;
      actor->field_078 = actor->field_34c;
    } else if (actor->field_074 == actor->field_34a) {
      actor->field_078 = actor->field_078 > actor->field_34c ?
                           actor->field_078 :
                           actor->field_34c;
    }
    actor->field_34a = 0;
  }

  if (actor->target_target_type < 0 || actor->target_target_type >= 12) {
    display_assert("(actor->target.target_type >= 0) && "
                   "(actor->target.target_type < NUMBER_OF_ACTOR_TARGET_TYPES)",
                   "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0x1138, true);
    system_exit(-1);
  }

  actor->field_06e =
    actor->field_074 >
        (actor->field_072 > ((short *)0x255f18)[actor->target_target_type] ?
           actor->field_072 :
           ((short *)0x255f18)[actor->target_target_type]) ?
      actor->field_074 :
      (actor->field_072 > ((short *)0x255f18)[actor->target_target_type] ?
         actor->field_072 :
         ((short *)0x255f18)[actor->target_target_type]);
  if (actor->field_06e > actor->field_074) {
    actor->field_074 = 0;
  }

  if (actor->field_06a < 3) {
    actor->field_07c = 0;
  } else {
    actor->field_07c++;
  }

  if (actor->field_06e == 0) {
    actor->field_080 = 0;
  } else {
    actor->field_080++;
  }

  if (actor->field_06e >= 4) {
    actor->field_084++;
    actor->field_088 = 0;
  } else {
    actor->field_084 = 0;
    if (actor->field_088 != -1) {
      actor->field_088++;
    }
  }

  if (actor->field_06e >= 7) {
    actor->field_08c = 1;
  }
}

/* actor_situation_update (0x303f0)
 * Rebuild the actor's per-tick situation counters (0x7b bytes at +0x1ec,
 * zeroed by csmemset) from every prop in the actor's prop iterator, track the
 * prop with the highest weight (prop+0x50), and retarget when that prop
 * differs from target_target_prop_index (+0x270).  Finishes with
 * actor_situation_update_target_status and
 * actor_situation_combat_status_update.
 *
 * Only props whose int16 at +0x24 is 2 or 3 and whose byte +0x127 is clear
 * are counted.  Enemy props (byte +0x60 set) bump +0x1ec and one bucket of
 * the byte array at +0x1ee indexed by a max-level (0..8, CMP CX,n / JG).
 * Non-enemy props classify the prop's unit by actor type (6 when
 * object+0x1c8 != -1, else the other actor's int16 +4, else 0xe) and bump
 * three families of 16-entry arrays (+0x203/+0x213, +0x225/+0x235,
 * +0x247/+0x257) plus their scalar totals.
 * Float compares are FCOMP + TEST AH,5 (JP/JNP), i.e. strict less-than.
 * Best-prop update is FCOMP + TEST AH,0x41 / JNZ: strictly greater.
 * The best-prop handle is the iterator's first dword (MOV EAX,[EBP-0x1c]).
 * Retarget store order at 0x3085a..0x30869 is +0x268, +0x270, +0x26c.
 * Assertion: actor_type range at line 0x11dc. */
void actor_situation_update(int actor_handle)
{
  actor_t *actor;
  char *counts;
  prop_t *prop;
  char *object;
  char *other_actor;
  prop_t *old_target;
  prop_t *new_target;
  int iter[2];
  int best_prop;
  int old_prop;
  float best_weight;
  char charging;
  char enemy_visible;
  short level;
  short actor_type;
  char area_friend;
  char visible_friend;
  char close_friend;

  actor = (actor_t *)datum_get(actor_data, actor_handle);
  counts = (char *)actor;
  best_prop = -1;
  best_weight = 0.0f;
  charging = actor->field_378 != 0 || actor->state_action == 10;
  csmemset(counts + 0x1ec, 0, 0x7b);
  prop_iterator_new(iter, actor_handle);
  prop = (prop_t *)prop_iterator_next(iter);
  while (prop != NULL) {
    if (prop->state >= 2 && prop->state <= 3 &&
        prop->dead == 0) {
      if (prop->enemy != 0) {
        enemy_visible = prop->visibility >= 2;
        level = 0;
        counts[0x1ec]++;
        if (enemy_visible) {
          if (prop->field_9c == 0) {
            counts[0x1ed]++;
          }
          counts[0x1f8]++;
          level = 1;
        }
        if (enemy_visible ||
            (prop->shooting != 0 && prop->line_of_sight == 0)) {
          if (prop->field_74 != 0) {
            counts[0x1ff]++;
            if (level <= 8) {
              level = 8;
            }
          }
          if (prop->shooting != 0) {
            counts[0x1fb]++;
            if (level <= 4) {
              level = 4;
            }
          }
          if (prop->quantized_facing <= 2) {
            if (enemy_visible) {
              counts[0x1f9]++;
              if (level <= 2) {
                level = 2;
              }
              if (!charging &&
                  prop->distance < 2.0f) {
                counts[0x1fe]++;
                if (level <= 7) {
                  level = 7;
                }
              }
            }
            if (prop->quantized_facing <= 1) {
              if (prop->shooting != 0) {
                counts[0x1fc]++;
                if (level <= 5) {
                  level = 5;
                }
              }
              if (prop->quantized_facing <= 0) {
                if (enemy_visible) {
                  counts[0x1fa]++;
                  if (level <= 3) {
                    level = 3;
                  }
                }
                if (prop->shooting != 0) {
                  counts[0x1fd]++;
                  if (level <= 6) {
                    level = 6;
                  }
                }
              }
            }
          }
        }
        counts[0x1ee + level]++;
      } else {
        object = (char *)object_get_and_verify_type(prop->unit_index, 3);
        if (*(int *)(object + 0x1a4) == -1) {
          other_actor = NULL;
        } else {
          other_actor = (char *)datum_get(actor_data, *(int *)(object + 0x1a4));
        }
        if (*(int *)(object + 0x1c8) != -1) {
          actor_type = 6;
        } else if (other_actor != NULL) {
          actor_type = *(short *)(other_actor + 4);
        } else {
          actor_type = 0xe;
        }
        area_friend = 0;
        visible_friend = 0;
        close_friend = 0;
        if (actor_type < 0 || actor_type >= 16) {
          display_assert(
            "(actor_type >= 0) && (actor_type < NUMBER_OF_ACTOR_TYPES)",
            "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0x11dc, true);
          system_exit(-1);
        }
        if (prop->distance < 8.0f) {
          area_friend = 1;
        } else if (prop->fighting != 0 && other_actor != NULL &&
                   actor->target_target_prop_index != -1 &&
                   *(int *)(other_actor + 0x270) != -1) {
          old_target =
            (prop_t *)datum_get(prop_data, actor->target_target_prop_index);
          new_target =
            (prop_t *)datum_get(prop_data, *(int *)(other_actor + 0x270));
          if (old_target->unit_index == new_target->unit_index) {
            area_friend = 1;
          }
        }
        if (prop->line_of_sight == 0 || prop->line_of_sight == 1) {
          visible_friend = 1;
          if (prop->distance < 3.0f) {
            close_friend = 1;
          }
        }
        if (area_friend) {
          counts[0x200]++;
          if (prop->fighting != 0) {
            counts[0x201]++;
          }
          if (prop->fighting != 0 && prop->vehicle_gunner != 0) {
            counts[0x202]++;
          }
          counts[0x203 + actor_type]++;
          if (prop->fighting != 0) {
            counts[0x213 + actor_type]++;
          }
        }
        if (visible_friend) {
          counts[0x223]++;
          if (prop->fighting != 0) {
            counts[0x224]++;
          }
          counts[0x225 + actor_type]++;
          if (prop->fighting != 0) {
            counts[0x235 + actor_type]++;
          }
        }
        if (close_friend) {
          counts[0x245]++;
          if (prop->fighting != 0) {
            counts[0x246]++;
          }
          counts[0x247 + actor_type]++;
          if (prop->fighting != 0) {
            counts[0x257 + actor_type]++;
          }
        }
      }
    }
    if (prop->target_weight > best_weight) {
      best_prop = iter[0];
      best_weight = prop->target_weight;
    }
    prop = (prop_t *)prop_iterator_next(iter);
  }

  if (best_prop != actor->target_target_prop_index) {
    old_prop = actor->target_target_prop_index;
    actor->target_target_type = 0;
    actor->target_target_prop_index = best_prop;
    actor->field_26c = -1;
    if (old_prop != -1) {
      old_target = (prop_t *)datum_get(prop_data, old_prop);
      old_target->target_weight =
        actor_compute_prop_target_weight(actor_handle, old_prop);
    }
    if (best_prop != -1) {
      new_target = (prop_t *)datum_get(prop_data, best_prop);
      new_target->target_weight = actor_compute_prop_target_weight(
        actor_handle, actor->target_target_prop_index);
    }
  }
  actor_situation_update_target_status(actor_handle);
  actor_situation_combat_status_update(actor_handle);
}

/* actor_situation_try_new_target (0x308e0)
 * Score prop `target` for `actor_handle` and adopt it as the actor's combat
 * target when it beats the currently-held target.
 *
 * The freshly computed weight is always stored to new_prop+0x50 (FST, not
 * FSTP, at 0x3093d).  Adoption requires weight > *(float *)0x2533c0 and, when
 * a previous target exists, new_prop+0x50 >= old_prop+0x50 (FCOMP + TEST AH,1
 * at 0x30981 exits only on strictly-less).
 * Assertion: "new_prop->enemy" at line 0x124d.
 * Return is a live bool (MOV AL,1 at 0x309b8 / XOR AL,AL at 0x309c1).
 * Store order at 0x3098f..0x3099e is 0x268, 0x270, 0x26c — MSVC rotation,
 * preserved deliberately. */
bool actor_situation_try_new_target(int actor_handle, int target)
{
  actor_t *actor;
  char *new_prop;
  char *old_prop;
  float weight;

  actor = (actor_t *)datum_get(actor_data, actor_handle);
  new_prop = (char *)datum_get(prop_data, target);
  if (actor->target_target_prop_index == -1) {
    old_prop = (char *)0;
  } else {
    old_prop = (char *)datum_get(prop_data, actor->target_target_prop_index);
  }

  weight = actor_compute_prop_target_weight(actor_handle, target);
  *(float *)(new_prop + 0x50) = weight;
  if (weight > *(float *)0x2533c0) {
    if (*(char *)(new_prop + 0x60) == 0) {
      display_assert("new_prop->enemy",
                     "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0x124d, true);
      system_exit(-1);
    }

    if (old_prop == (char *)0 ||
        *(float *)(new_prop + 0x50) >= *(float *)(old_prop + 0x50)) {
      actor->target_target_type = 0;
      actor->target_target_prop_index = target;
      actor->field_26c = -1;
      actor_situation_update_target_status(actor_handle);
      actor_situation_combat_status_update(actor_handle);
      return true;
    }
  }
  return false;
}

/* actor_perception_friend_prop_is_attacking (0x309d0): asserts the prop at
 * iter_handle is an acknowledged friend (type +0x24 in [2,3], +0x60 enemy == 0,
 * +0x127 dead == 0).  Returns 0 when +0x14 is set.  When +0x12e is set, the
 * prop's unit (+0x18) aiming vector is written to out_pos and the +0x12f byte
 * (read before the call) is the result; if that is 0 and the actor's +0x1ec
 * signed byte is > 0, each enemy prop of the actor whose normalized offset
 * from this prop has length > *0x2533c0 and dot with out_pos > *0x253398
 * returns 1.  Otherwise, a valid +0x1c actor handle defers to
 * actor_attacking_target(). */
char actor_perception_friend_prop_is_attacking(int actor_handle,
                                               int iter_handle, float *out_pos)
{
  char *actor;
  char *prop;
  char *other;
  char result;
  float delta[3];
  int iter[2];

  actor = (char *)datum_get(actor_data, actor_handle);
  prop = (char *)datum_get(prop_data, iter_handle);
  if (*(short *)(prop + 0x24) < 2 || *(short *)(prop + 0x24) > 3 ||
      *(char *)(prop + 0x60) != 0 || *(char *)(prop + 0x127) != 0) {
    display_assert("prop_acknowledged(friend_prop) && !friend_prop->enemy && "
                   "!friend_prop->dead",
                   "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0x1268, true);
    system_exit(-1);
  }
  if (*(char *)(prop + 0x14) == 0) {
    if (*(char *)(prop + 0x12e) != 0) {
      result = *(char *)(prop + 0x12f);
      unit_get_aiming_vector(*(int *)(prop + 0x18), out_pos);
      if (result == 0 && *(signed char *)(actor + 0x1ec) > 0) {
        prop_iterator_new(iter, actor_handle);
        other = (char *)prop_iterator_next(iter);
        while (other != NULL) {
          if (*(short *)(other + 0x24) >= 2 && *(short *)(other + 0x24) <= 3 &&
              *(char *)(other + 0x60) != 0) {
            delta[0] = *(float *)(other + 0xbc) - *(float *)(prop + 0xbc);
            delta[1] = *(float *)(other + 0xc0) - *(float *)(prop + 0xc0);
            delta[2] = *(float *)(other + 0xc4) - *(float *)(prop + 0xc4);
            if (normalize3d(delta) > *(float *)0x2533c0 &&
                delta[2] * out_pos[2] + delta[1] * out_pos[1] +
                    delta[0] * out_pos[0] >
                  *(float *)0x253398) {
              return 1;
            }
          }
          other = (char *)prop_iterator_next(iter);
        }
      }
      return result;
    }
    if (*(int *)(prop + 0x1c) != -1) {
      return (char)actor_attacking_target(*(int *)(prop + 0x1c),
                                          (int *)out_pos);
    }
  }
  return 0;
}

/* actor_perception_aiming_vector_test_blockage (0x30b80)
 * Classify how much `target_point` blocks the horizontal aiming line from
 * `point` along `aiming_vector`.
 *
 * The aiming vector's XY part is normalized (return 0 when |len| <
 * *(double *)0x2533d0 or len <= 0).  With d = target_point - point, the
 * target must lie inside a cone: dot(d.xy, aim.xy) > |d.xy| * 0.866f
 * (0x2533dc), else 0.  The offset of the target from the aiming line is
 * e = d - dot * aiming_vector (the ORIGINAL, unnormalized 3D vector); when
 * out_offset is non-NULL it receives -e.  Vertical bands on e.z:
 * (-0.5f, 0.9f) -> level 2, (-0.8f, 1.2f) -> level 1, else 0.  Then with
 * h = e.x^2 + e.y^2: h < 0.36f returns the level, h < 1.21f returns 1, else 0.
 * Constants: 0x255964 = -0.5f, 0x2555d0 = 0.9f, 0x25612c = -0.8f,
 * 0x253f48 = 1.2f, 0x256134 = 0.36f, 0x256130 = 1.21f.
 * Confirmed: all four params are pointers (callers LEA/PUSH addresses at
 * 0x242b7, 0x32f85, 0x3306e, 0x330f4; arg 4 may be 0). Return is 16-bit
 * (MOV AX,SI / MOV AX,CX; EAX=1). */
short actor_perception_aiming_vector_test_blockage(float *point,
                                                   float *aiming_vector,
                                                   int object_index,
                                                   int param_4)
{
  /* Params 3 and 4 are pointers (kb decl keeps the original int slots). */
  float *target_point = (float *)object_index;
  float *out_offset = (float *)param_4;
  float horizontal[2];
  float length;
  float inverse;
  float d0;
  float d1;
  float d2;
  float along;
  float e0;
  float e1;
  float e2;
  float lateral;
  short level;

  horizontal[0] = aiming_vector[0];
  horizontal[1] = aiming_vector[1];
  length =
    x87_sqrt(horizontal[1] * horizontal[1] + horizontal[0] * horizontal[0]);
  if ((double)x87_fabs(length) < *(double *)0x2533d0) {
    return 0;
  }
  inverse = *(float *)0x2533c8 / length;
  horizontal[0] = horizontal[0] * inverse;
  horizontal[1] = horizontal[1] * inverse;
  if (!(length > 0.0f)) {
    return 0;
  }

  d0 = target_point[0] - point[0];
  d1 = target_point[1] - point[1];
  d2 = target_point[2] - point[2];
  along = d1 * horizontal[1] + d0 * horizontal[0];
  if (!(along > x87_sqrt(d1 * d1 + d0 * d0) * *(float *)0x2533dc)) {
    return 0;
  }

  along = -along;
  e0 = along * aiming_vector[0] + d0;
  e1 = along * aiming_vector[1] + d1;
  e2 = along * aiming_vector[2] + d2;
  if (out_offset != NULL) {
    out_offset[0] = -e0;
    out_offset[1] = -e1;
    out_offset[2] = -e2;
  }

  if (e2 > *(float *)0x255964 && e2 < *(float *)0x2555d0) {
    level = 2;
  } else if (e2 > *(float *)0x25612c && e2 < *(float *)0x253f48) {
    level = 1;
  } else {
    return 0;
  }

  lateral = e1 * e1 + e0 * e0;
  if (lateral < *(float *)0x256134) {
    return level;
  }
  if (lateral < *(float *)0x256130) {
    return 1;
  }
  return 0;
}

/* actor_emotion_flee_with_friends (0x30d10): walk the actor's props and
 * decide whether to flee, scaling *chance_inout otherwise.
 *
 * For each prop with type (+0x24) in [2,3], +0x60 byte == 0, +0x10 word equal
 * to actor->field_004, and a valid +0x1c actor handle, the referenced actor is
 * looked up.  If it is panicking (+0x308 > 0) or in state_action 4 with
 * field_0a8 > 0, the "fleeing" count increments; otherwise the prop's +0x12c
 * byte bumps the other count.  Both counters are 16-bit (CMP word / DI).
 *
 * More than one fleeing friend returns 1 immediately without touching
 * *chance_inout.  Otherwise a factor is computed from the other count,
 * clamped to [*0x2533c0, *0x253f40], and multiplied into *chance_inout;
 * returns 0.  Only AL is set on return (MOV AL,1 / XOR AL,AL).
 *
 * No __FILE__ string. */
bool actor_emotion_flee_with_friends(int actor_handle, float *chance_inout)
{
  actor_t *actor;
  actor_t *other;
  char *prop;
  short other_count;
  short flee_count;
  int iter[2];
  float factor;

  actor = (actor_t *)datum_get(actor_data, actor_handle);
  other_count = 0;
  flee_count = 0;
  prop_iterator_new(iter, actor_handle);
  prop = (char *)prop_iterator_next(iter);
  if (prop != NULL) {
    do {
      /* prop->state */
      if (*(short *)(prop + 0x24) >= 2 && *(short *)(prop + 0x24) <= 3 &&
          *(char *)(prop + 0x60) == 0 &&
          *(short *)(prop + 0x10) == actor->field_004 &&
          *(int *)(prop + 0x1c) != -1) {
        other = (actor_t *)datum_get(actor_data, *(int *)(prop + 0x1c));
        if (other->stimuli_panic_type > 0 ||
            (other->state_action == 4 && other->field_0a8 > 0)) {
          flee_count++;
        } else if (*(char *)(prop + 0x12c) != 0) {
          other_count++;
        }
      }
      prop = (char *)prop_iterator_next(iter);
    } while (prop != NULL);

    if (flee_count > 1) {
      return 1;
    }
    if (other_count > 1) {
      factor =
        *(float *)0x002533c8 - (float)(other_count - 1) * *(float *)0x0025337c;
      goto clamp;
    }
  }
  factor =
    (float)(1 - other_count) * *(float *)0x00253398 + *(float *)0x002533c8;
clamp:
  if (factor < *(float *)0x002533c0) {
    factor = *(float *)0x002533c0;
  } else if (factor > *(float *)0x00253f40) {
    factor = *(float *)0x00253f40;
  }
  *chance_inout = factor * *chance_inout;
  return 0;
}

/* actor_emotion_get_unopposable_enemy (0x30e60): find-or-append a 0x1c-byte
 * record in a caller-owned array, keyed by the dword at record+0x8.
 *
 * @<eax> = array base, @<edi> = search key.  Stack: param_1 at [EBP+0x8] is
 * pushed by the caller (0x3102e) but NEVER read by this function — it is
 * declared so that p_count ([EBP+0xc]) and max_count ([EBP+0x10]) land on the
 * right slots.  Meaning unknown.
 *
 * Returns the record index as a short (caller at 0x3103d does CMP AX,0xffff),
 * or -1 when the key is absent and the array is already full.  The key itself
 * is NOT stored here; the caller writes record+0x8 = key at 0x31068.
 *
 * Store order at 0x30eb9..0x30ecc is +0x4, +0x8, +0x18, +0x0, +0xc, +0x10,
 * +0x14 — MSVC rotation, preserved deliberately.  0x7f7fffff is the FLT_MAX
 * bit pattern but the field is written as a dword immediate; its type is
 * unproven.
 *
 * Single exit: the reference presets EAX = -1 (0x30e6d) and every path falls
 * through the shared epilogue at 0x30ed6, so the append is nested under
 * `if (index == -1)` rather than written as an early return.
 *
 * No __FILE__ string. */
short actor_emotion_get_unopposable_enemy(void *records /* @<eax> */,
                                          int key /* @<edi> */, int param_1,
                                          short *p_count, short max_count)
{
  char *base;
  short count;
  short index;
  short i;
  char *rec;

  base = (char *)records;
  count = *p_count;
  index = -1;

  if (count > 0) {
    i = 0;
    do {
      if (*(int *)(base + i * 0x1c + 8) == key) {
        index = i;
        break;
      }
      i = i + 1;
    } while (i < count);
  }

  if (index == -1) {
    if (count < max_count) {
      *p_count = count + 1;
      rec = base + count * 0x1c;
      *(int *)(rec + 0x4) = -1;
      *(int *)(rec + 0x8) = -1;
      *(int *)(rec + 0x18) = -1;
      *(short *)(rec + 0x0) = 0;
      *(int *)(rec + 0xc) = 0;
      *(short *)(rec + 0x10) = 0;
      *(int *)(rec + 0x14) = 0x7f7fffff; /* FLT_MAX bit pattern */
      index = count;
    }
  }

  return index;
}

/* actor_emotion_unopposable_retreat (0x30f50): collect up to 16 0x1c-byte
 * records (via actor_emotion_get_unopposable_enemy) from the actor's props,
 * promote each record's priority from actr-tag thresholds, then either count
 * down actor+0x3a8 or pick the best record (priority > 5) and arm it.
 *
 * Record layout as used here (meanings unproven): +0x0 short priority,
 * +0x4 prop handle, +0x8 key, +0xc prop pointer, +0x10 short counter,
 * +0x14 float min squared prop+0x11c, +0x18 dword from prop+0x1c.
 *
 * Stack arg at [EBP+0x8] is the actor handle (datum_get(actor_data, ...) at
 * 0x30f66).  0x61637472 = 'actr'.  FCOMP/FCOM + TEST AH,5 + JP skip means
 * the path is taken only when ST < mem.  *0x253394 = TICKS_PER_SECOND.
 *
 * No __FILE__ string. */
void actor_emotion_unopposable_retreat(int actor_index)

{
  char
    targets[16 * 0x1c];
  actor_t *actor;
  short target_count;
  char *definition;
  char *prop;
  short target_index;

  actor = (actor_t *)datum_get(actor_data, actor_index);
  definition = (char *)tag_get(0x61637472, actor->field_058);
  target_count = 0;
  {
    int iter[2];

    prop_iterator_new(iter, actor_index);
    while ((prop = (char *)prop_iterator_next(iter)) != NULL) {
      char *known;
      short priority;

      known = (char *)datum_get(prop_data, iter[0]);
      priority = 0;
      if (*(short *)(known + 0x24) >= 2 && *(short *)(known + 0x24) <= 3 &&
          *(char *)(known + 0xa4) != 0) {
        if (*(char *)(known + 0x74) != 0) {
          priority = 4;
        } else if (*(char *)(known + 0x12f) != 0) {
          priority = (short)((*(char *)(known + 0x122) <= 1) + 2);
        } else if (*(short *)(known + 0x32) >= 2) {
          priority = 1;
        }
      }
      if (priority > 0) {
        int key;
        char *rec;

        key = *(int *)(prop + 0x18);
        target_index = actor_emotion_get_unopposable_enemy(
          targets, key, actor_index, &target_count, 0x10);
        if (target_index != -1) {
          rec = targets + target_index * 0x1c;
          if (*(short *)rec < priority) {
            *(int *)(rec + 0x4) = iter[0];
            *(int *)(rec + 0x8) = key;
            *(char **)(rec + 0xc) = prop;
            *(short *)rec = priority;
          }
        }
      } else if (*(short *)(prop + 0x24) >= 2 && *(short *)(prop + 0x24) <= 3 &&
                 *(char *)(prop + 0x60) == 0 && *(int *)(prop + 0x1c) != -1 &&
                 *(float *)(prop + 0x11c) < 8.0f) {
        actor_t *other;

        other = (actor_t *)datum_get(actor_data, *(int *)(prop + 0x1c));
        if (other->field_3a8 != 0 && other->field_3ac != -1 &&
            (actor->field_3a4 == -1 || other->field_3b0 >= actor->field_3a4)) {
          char *target;
          int enemy_handle;

          target = (char *)datum_get(prop_data, other->field_3ac);
          enemy_handle =
            prop_get_active_by_unit_index(actor_index, *(int *)(target + 0x18));
          if (enemy_handle != -1) {
            char *enemy;

            enemy = (char *)datum_get(prop_data, enemy_handle);
            if (*(short *)(enemy + 0x24) >= 2 && *(short *)(enemy + 0x24) <= 3 &&
                *(char *)(enemy + 0xa4) != 0) {
              char *rec;
              float dist_sq;

              target_index = actor_emotion_get_unopposable_enemy(
                targets, *(int *)(target + 0x18), actor_index, &target_count,
                0x10);
              if (target_index != -1) {
                rec = targets + target_index * 0x1c;
                dist_sq = *(float *)(target + 0x11c);
                *(short *)(rec + 0x10) = *(short *)(rec + 0x10) + 1;
                dist_sq = dist_sq * dist_sq;
                if (dist_sq < *(float *)(rec + 0x14)) {
                  *(float *)(rec + 0x14) = dist_sq;
                  *(int *)(rec + 0x18) = *(int *)(prop + 0x1c);
                }
                if (*(int *)(rec + 0x4) == -1) {
                  *(int *)(rec + 0x4) = enemy_handle;
                  *(int *)(rec + 0x8) = *(int *)(enemy + 0x18);
                  *(char **)(rec + 0xc) = enemy;
                }
              }
            }
          }
        }
      }
    }
  }

  for (target_index = 0; target_index < target_count; target_index++) {
    char *rec;
    char *known;
    short threshold;
    char promote;
    char flag;
    short priority;
    float upper_bound;
    float lower_bound;

    rec = targets + (int)target_index * 0x1c;
    known = *(char **)(rec + 0xc);
    threshold = *(short *)(definition + 0x268);
    promote = 0;
    if (*(char *)(known + 0x135) != 0 || *(char *)(known + 0x136) != 0) {
      threshold = *(short *)(definition + 0x26a);
    }
    flag = *(char *)(known + 0x12e);
    if (flag != 0) {
      short cap;

      cap = *(short *)(definition + 0x26c);
      if (cap > 0 && threshold > cap) {
        threshold = cap;
      }
    }
    if (threshold > 0 && *(short *)rec >= threshold) {
      if (flag == 0) {
        *(short *)(known + 0xaa) = 0x16;
      } else {
        promote = 1;
      }
    } else if (flag != 0) {
      *(short *)(known + 0xaa) = 0x16;
    }
    if (*(short *)(known + 0xaa) > 0) {
      if (*(short *)(known + 0xac) == 0) {
        upper_bound = *(float *)(definition + 0x274);
        lower_bound = *(float *)(definition + 0x270);
        *(short *)(known + 0xae) =
          (short)(int)(random_real_range(get_global_random_seed_address(),
                                         lower_bound, upper_bound) *
                       TICKS_PER_SECOND);
      }
      *(short *)(known + 0xaa) = *(short *)(known + 0xaa) - 1;
      *(short *)(known + 0xac) = *(short *)(known + 0xac) + 1;
    }
    if (*(short *)(known + 0x78) >= 0x2d || *(short *)rec >= 4) {
      if (*(short *)(known + 0xae) > 0 &&
          *(short *)(known + 0xac) >= *(short *)(known + 0xae)) {
        priority = *(short *)rec;
        *(short *)rec = (short)(priority > 7 ? priority : 7);
      }
      if (promote != 0) {
        priority = *(short *)rec;
        *(short *)rec = (short)(priority > 8 ? priority : 8);
      }
      if (*(short *)(definition + 0x278) > 0 &&
          *(short *)(known + 0xa6) >= *(short *)(definition + 0x278)) {
        priority = *(short *)rec;
        *(short *)rec = (short)(priority > 9 ? priority : 9);
      }
      if (*(short *)(definition + 0x27a) > 0 &&
          *(short *)(rec + 0x10) >= *(short *)(definition + 0x27a)) {
        priority = *(short *)rec;
        *(short *)rec = (short)(priority > 6 ? priority : 6);
      }
    }
  }

  if (actor->field_3a8 > 0) {
    actor->field_3a8 = actor->field_3a8 - 1;
    if (actor->field_3a8 == 0) {
      actor->field_3a4 = game_time_get();
      return;
    }
  } else {
    int best_handle;
    short best_priority;

    best_handle = -1;
    best_priority = 5;
    for (target_index = 0; target_index < target_count; target_index++) {
      char *rec;

      rec = targets + (int)target_index * 0x1c;
      if (*(short *)rec > best_priority && *(int *)(rec + 0x4) != -1) {
        best_priority = *(short *)rec;
        best_handle = *(int *)(rec + 0x4);
      }
    }
    if (best_handle != -1) {
      float upper_bound;
      float lower_bound;

      upper_bound = *(float *)(definition + 0x28c);
      lower_bound = *(float *)(definition + 0x288);
      actor->field_3a8 =
        (short)(int)(random_real_range(get_global_random_seed_address(),
                                       lower_bound, upper_bound) *
                     TICKS_PER_SECOND);
      actor->field_3ac = best_handle;
      actor->field_3b0 = game_time_get();
    }
  }
}

/* actor_berserk (0x31440): set or clear the actor's byte at +0x378 from the
 * low byte of berserk_flag (MOV BL,[EBP+0xc]).  No-op when unchanged.  On a
 * change: +0x379 = 0; if actor+0x6 is zero, set/clear bit 0x80 of the dword at
 * +0x1b4 of the object at actor+0x18 (the reference jump-threads the
 * clear path straight to its own epilogue); otherwise
 * walk the object chain from actor+0x24 via object+0x1ac, OR-ing 0x80 into
 * object+0xb6.  Then actor+0x375 = 1 when the flag is nonzero.  Object fields
 * and the meaning of actor+0x6 are unproven.
 *
 * No __FILE__ string. */
void actor_berserk(int actor_handle, int berserk_flag)
{
  char *actor;
  char *object;
  int object_handle;
  char flag;

  actor = (char *)datum_get(actor_data, actor_handle);
  flag = (char)berserk_flag;

  if (flag != ((actor_t *)actor)->field_378) {
    ((actor_t *)actor)->field_378 = flag;
    ((actor_t *)actor)->field_379 = 0;

    if (((actor_t *)actor)->field_006 != '\0') {
      object_handle = ((actor_t *)actor)->field_024;
      while (object_handle != -1) {
        object = (char *)object_get_and_verify_type(object_handle, 3);
        *(unsigned short *)(object + 0xb6) |= 0x80;
        object_handle = *(int *)(object + 0x1ac);
      }
    } else {
      object =
        (char *)object_get_and_verify_type(((actor_t *)actor)->field_018, 3);
      if (flag != '\0') {
        *(unsigned int *)(object + 0x1b4) |= 0x80;
      } else {
        *(unsigned int *)(object + 0x1b4) &= 0xffffff7fu;
      }
    }

    if (flag != '\0') {
      ((actor_t *)actor)->field_375 = 1;
    }
  }
}

/* actor_visibility_at_point (0x314f0)
 * Rate how well the actor can see `position` from the sense block (eye point
 * at +0x0, location at +0x24): 0 = not seen, 1 = peripheral, 2/3 = seen
 * (3 when mode == 0 and the squared distance is under *(float *)0x255fd8).
 *
 * mode (param_5) must be 0 or 1, else 0.  The base range is the 'actr' tag's
 * +0x18 overridden by the firing-variant definition's +0x150 when > 0, scaled
 * by the per-knowledge factor (param_8: 0x253524 / 0x253f3c / 0x2533f0 /
 * 1.0f; other values assert "!\"unreachable\"" at 0x4f4).  Outside that range
 * returns 0.  The light factor starts at 1.0f, becomes 0.3f / 0.7f for
 * param_4 == 0 / 1 unless the tag's flag bit 0 is set, is scaled down by the
 * location darkness from FUN_0018e690 and floored to 0.15f.  When param_7 is
 * set the actor's perception record (+0x656c/+0x6570/+0x6574 in the 0x657c
 * stride table at *(char **)0x331f58) gets game time, range and light.
 * For non-swarm actors with param_6 set, the elevation of the target in the
 * actor's looking frame (+0x18c forward, +0x198 left, +0x1a4 up) must lie in
 * [*(float *)0x256138, *(float *)0x25613c] and actor_get_vision_distances
 * turns |azimuth| into the certain/peripheral ranges; otherwise the certain
 * range is 0.7f * the peripheral range.
 * Confirmed: arguments of actor_get_vision_distances (0x317a9..0x317aa):
 * EBX actor, [EBP-8] range, [EBP-4] light, FSTP [ESP] fabs(atan2(left, fwd)),
 * &range, &peripheral. */
short actor_visibility_at_point(int actor_handle, void *input_block,
                                float *position, char param_4, short param_5,
                                char param_6, char param_7, short param_8)
{
  char *actor;
  unsigned char *actor_tag;
  char *variant;
  char *record;
  float range;
  float knowledge_scale;
  float light;
  float darkness;
  float peripheral;
  float d0;
  float d1;
  float d2;
  float distance_squared;
  float forward;
  float left;
  float up;
  float elevation;

  if (param_5 != 0 && param_5 != 1) {
    return 0;
  }

  actor = (char *)datum_get(actor_data, actor_handle);
  actor_tag = (unsigned char *)tag_get('actr', ((actor_t *)actor)->field_058);
  variant = actor_combat_get_firing_variant_definition(actor_handle);
  range = *(float *)(actor_tag + 0x18);
  if (*(float *)(variant + 0x150) > 0.0f) {
    range = *(float *)(variant + 0x150);
  }

  switch (param_8) {
  case 0:
    knowledge_scale = *(float *)0x253524;
    break;
  case 1:
    knowledge_scale = *(float *)0x253f3c;
    break;
  case 2:
    knowledge_scale = *(float *)0x2533f0;
    break;
  case 3:
    knowledge_scale = *(float *)0x2533c8;
    break;
  default:
    display_assert("!\"unreachable\"",
                   "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0x4f4, 1);
    system_exit(-1);
  }
  range = knowledge_scale * range;

  d0 = position[0] - ((float *)input_block)[0];
  d1 = position[1] - ((float *)input_block)[1];
  d2 = position[2] - ((float *)input_block)[2];
  distance_squared = d2 * d2 + d1 * d1 + d0 * d0;
  if (!(distance_squared < range * range)) {
    return 0;
  }

  light = 1.0f;
  if ((actor_tag[0] & 1) == 0) {
    switch (param_4) {
    case 0:
      light = 0.3f;
      break;
    case 1:
      light = 0.7f;
      break;
    }
  }

  darkness = FUN_0018e690((float *)((char *)input_block + 0x24),
                          (float *)input_block, position);
  if (darkness > *(float *)0x2533f0) {
    light = 0.15f;
  } else {
    if (darkness > *(float *)0x2549d4) {
      light = (*(float *)0x2533f0 - darkness) * light * *(float *)0x256144;
    }
    if (!(light > *(float *)0x256140)) {
      light = 0.15f;
    }
  }

  record = *(char **)0x331f58 + (actor_handle & 0xffff) * 0x657c;
  if (param_7 != '\0') {
    *(int *)(record + 0x656c) = game_time_get();
    *(float *)(record + 0x6570) = range;
    *(float *)(record + 0x6574) = light;
  }

  peripheral = light * range;
  if (!(distance_squared < peripheral * peripheral)) {
    return 0;
  }

  if (((actor_t *)actor)->field_006 == '\0' && param_6 != '\0') {
    forward = d2 * *(float *)(actor + 0x194) + d1 * *(float *)(actor + 0x190) +
              d0 * *(float *)(actor + 0x18c);
    left = d2 * *(float *)(actor + 0x1a0) + d1 * *(float *)(actor + 0x19c) +
           d0 * *(float *)(actor + 0x198);
    up = d2 * *(float *)(actor + 0x1ac) + d1 * *(float *)(actor + 0x1a8) +
         d0 * *(float *)(actor + 0x1a4);
    elevation = x87_fatan2f(up, x87_sqrt(left * left + forward * forward));
    if (elevation > *(float *)0x25613c || elevation < *(float *)0x256138) {
      peripheral = 0.0f;
      range = 0.0f;
    } else {
      actor_get_vision_distances(actor_handle, range, light,
                                 x87_fabs(x87_fatan2f(left, forward)), &range,
                                 &darkness);
      peripheral = darkness;
    }
  } else {
    range = *(float *)0x2533c4 * peripheral;
  }

  if (param_5 == 0 && distance_squared < range * range) {
    if (distance_squared < *(float *)0x255fd8) {
      return 3;
    }
    return 2;
  }
  if (distance_squared < peripheral * peripheral) {
    return 1;
  }
  return 0;
}

/* actor_audibility_at_point (0x31850)
 * Rate whether the actor hears a sound of `volume` at `position`/`location`:
 * 0 = not heard, 2 = heard, 3 = heard loud (volume >= 3).
 *
 * Returns 0 without touching the perception record when volume is 0 or
 * either cluster index (input_block+0x28, location+4) is NONE.  The hearing
 * range starts at the 'actr' tag's +0x4c and is scaled: behind the listener
 * (dot with input_block+0x18 < 0) by 0x2533f0; actor state +0x6a == 2 by
 * 0.7f, == 1 by 0x253524; volume 4 / 1 / 3 by 0x2549d4 / 0x25614c / 0.7f; an
 * obstructed location (FUN_0018e5c0 on either side) by 0x25337c; flags other
 * than 0/1 by 0.7f.  Within range, the BSP cluster sound encoding (bit 7 =
 * blocked) gives c = (enc & 0x7f) * 0x256148; the effective distance
 * max(2c, sqrt(d2)) must be < range to hear.  Always stores range, distance,
 * 1, result, effective distance (-1.0f when not evaluated) and c (-1.0f when
 * not evaluated) into record+0xa8/+0xac/+0xa4/+0xa6/+0xb4/+0xb0.
 * range_scale (arg 6) is never read; every caller pushes 0x3f800000 (1.0f).
 * Return is 16-bit (MOV AX,CX); callers compare AX. */
int actor_audibility_at_point(int actor_handle, void *input_block,
                              float *position, void *location, short volume,
                              int range_scale, short flags)
{
  char *actor;
  char *actor_tag;
  char *record;
  float range;
  float d0;
  float d1;
  float d2;
  float distance_squared;
  float distance;
  float cluster_distance;
  float effective_distance;
  unsigned char encoding;
  short result;

  actor = (char *)datum_get(actor_data, actor_handle);
  actor_tag = (char *)tag_get('actr', ((actor_t *)actor)->field_058);
  result = 0;
  if (volume != 0 && *(short *)((char *)input_block + 0x28) != -1 &&
      *(short *)((char *)location + 4) != -1) {
    range =*(float *)(actor_tag + 0x4c);
    cluster_distance = -1.0f;
    effective_distance = -1.0f;
    d0 = position[0] - ((float *)input_block)[0];
    d1 = position[1] - ((float *)input_block)[1];
    d2 = position[2] - ((float *)input_block)[2];
    distance_squared = d2 * d2 + d0 * d0 + d1 * d1;
    if (d2 * *(float *)((char *)input_block + 0x20) +
          d1 * *(float *)((char *)input_block + 0x1c) +
          d0 * *(float *)((char *)input_block + 0x18) <
        0.0f) {
      range = range * *(float *)0x2533f0;
    }
    if (((actor_t *)actor)->field_06a == 2) {
      range = range * *(float *)0x2533c4;
    } else if (((actor_t *)actor)->field_06a == 1) {
      range = range * *(float *)0x253524;
    }
    if (volume == 4) {
      range = range * *(float *)0x2549d4;
    } else if (volume == 1) {
      range = range * *(float *)0x25614c;
    } else if (volume == 3) {
      range = range * *(float *)0x2533c4;
    }
    if (FUN_0018e5c0((int)((char *)input_block + 0x24)) != '\0' ||
        FUN_0018e5c0((int)location) != '\0') {
      range = range * *(float *)0x25337c;
    }
    if (flags != 0 && flags != 1) {
      range = range * *(float *)0x2533c4;
    }

    if (range * range > distance_squared) {
      encoding = structure_bsp_cluster_sound_encoding(
        scenario_get(), *(short *)((char *)location + 4),
        *(short *)((char *)input_block + 0x28));
      if (!(encoding & 0x80)) {
        cluster_distance = (float)(encoding & ~0x80) * *(float *)0x256148;
        effective_distance = cluster_distance + cluster_distance;
        distance = x87_sqrt(distance_squared);
        if (!(effective_distance > distance)) {
          effective_distance = distance;
        }
        if (effective_distance < range) {
          result = (short)((volume >= 3) + 2);
        }
      }
    }

    record = *(char **)0x331f58 + (actor_handle & 0xffff) * 0x657c;
    *(float *)(record + 0xa8) = range;
    *(float *)(record + 0xac) = x87_sqrt(distance_squared);
    *(char *)(record + 0xa4) = 1;
    *(short *)(record + 0xa6) = result;
    *(float *)(record + 0xb4) = effective_distance;
    *(float *)(record + 0xb0) = cluster_distance;
  }
  return result;
}

/* actor_perception_find_sense_position (0x31a90): copy the actor's cached
 * 14-dword input block for a normal actor.  For a swarm actor, select the
 * swarm unit whose component position is closest to position and sample that
 * unit's input block.  param_3 is not read by the original. */
void actor_perception_find_sense_position(int actor_handle, float *position,
                                          int param_3, void *input_block_out)
{
  actor_t *actor;
  char *swarm;
  char *component;
  float dx;
  float dy;
  float dz;
  float distance_squared;
  float best_distance_squared;
  int best_unit_handle;
  short i;

  (void)param_3;
  actor = (actor_t *)datum_get(actor_data, actor_handle);
  if (actor->field_006 != 0) {
    swarm =
      (char *)datum_get(*(data_t **)0x6325a0, actor->meta_swarm_cache_index);
    best_distance_squared = 3.4028235e+38f;
    best_unit_handle = -1;

    if (actor->field_01e <= 0) {
      display_assert("actor->meta.swarm_unit_count > 0",
                     "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0x665, true);
      system_exit(-1);
    }
    if (actor->field_024 == -1) {
      display_assert("actor->meta.swarm_unit_index != NONE",
                     "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0x666, true);
      system_exit(-1);
    }

    for (i = 0; i < *(short *)(swarm + 0x2); i++) {
      float *component_pos;

      component =
        (char *)datum_get(*(data_t **)0x63259c, *(int *)(swarm + 0x58 + i * 4));
      component_pos = (float *)component;
      component_pos += 1;
      dx = position[0] - component_pos[0];
      dy = position[1] - component_pos[1];
      dz = position[2] - component_pos[2];
      distance_squared = dx * dx;
      distance_squared += dy * dy;
      distance_squared += dz * dz;
      if (distance_squared < best_distance_squared) {
        best_distance_squared = distance_squared;
        best_unit_handle = *(int *)(swarm + 0x18 + i * 4);
      }
    }

    if (best_unit_handle == -1) {
      display_assert("best_unit_index != NONE",
                     "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0x677, true);
      system_exit(-1);
    }
    actor_input_sample_position(actor_handle, best_unit_handle,
                                (char *)input_block_out);
  } else {
    memcpy(input_block_out, &actor->field_120, 0x38);
  }
}

/* actor_perception_unit_from_swarm (0x31c00): pick the swarm unit closest to
 * the sense block's body position (input_block+0xc).  EAX = swarm actor,
 * EDI = input block; actor_handle ([EBP+8]) is not read by the original.
 *
 * With a swarm cache (actor+0x28 != NONE) the cached components are scanned
 * (distance weighted by 2.25 when the component is attached to a unit, flag
 * bit 1 of component+0x2, else by 0.36 for existing_unit_index); otherwise the
 * unit chain at actor+0x24 / unit+0x1ac is walked.  Every candidate is passed
 * to object_mark when mark_objects is set. */
int actor_perception_unit_from_swarm(int swarm_actor_handle, void *input_block,
                                     int actor_handle, int existing_unit_index,
                                     char mark_objects)
{
  char *swarm_actor;
  char *swarm;
  char *component;
  char *unit;
  float *position;
  float origin[3];
  float dx;
  float dy;
  float dz;
  float distance_squared;
  float best_distance_squared;
  int best_unit_index;
  int unit_index;
  int i;

  (void)actor_handle;
  swarm_actor = (char *)datum_get(actor_data, swarm_actor_handle);
  best_unit_index = -1;
  position = (float *)input_block;
  if (*(char *)(swarm_actor + 0x6) == 0) {
    display_assert("swarm_actor->meta.swarm",
                   "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0x68d, true);
    system_exit(-1);
  }

  if (*(int *)(swarm_actor + 0x28) != -1) {
    swarm =
      (char *)datum_get(*(data_t **)0x6325a0, *(int *)(swarm_actor + 0x28));
    best_distance_squared = 3.4028235e+38f;
    for (i = 0; (short)i < *(short *)(swarm + 0x2); i++) {
      component = (char *)datum_get(*(data_t **)0x63259c,
                                    *(int *)(swarm + 0x58 + (short)i * 4));
      dx = position[3] - *(float *)(component + 0x4);
      dy = position[4] - *(float *)(component + 0x8);
      dz = position[5] - *(float *)(component + 0xc);
      distance_squared = dy * dy + dz * dz + dx * dx;
      if ((*(unsigned char *)(component + 0x2) & 2) != 0)
        distance_squared *= *(float *)0x2561f0;
      else if (*(int *)(swarm + 0x18 + (short)i * 4) == existing_unit_index)
        distance_squared *= *(float *)0x256134;
      if (distance_squared < best_distance_squared) {
        best_distance_squared = distance_squared;
        best_unit_index = *(int *)(swarm + 0x18 + (short)i * 4);
      }
      if (mark_objects)
        object_mark(*(int *)(swarm + 0x18 + (short)i * 4));
    }
  } else {
    unit_index = *(int *)(swarm_actor + 0x24);
    best_distance_squared = 3.4028235e+38f;
    while (unit_index != -1) {
      unit = (char *)object_get_and_verify_type(unit_index, 3);
      object_get_world_position(unit_index, (vector3_t *)origin);
      dx = position[3] - origin[0];
      dy = position[4] - origin[1];
      dz = position[5] - origin[2];
      distance_squared = dz * dz + dy * dy + dx * dx;
      if (unit_index == existing_unit_index)
        distance_squared *= *(float *)0x256134;
      if (distance_squared < best_distance_squared) {
        best_distance_squared = distance_squared;
        best_unit_index = unit_index;
      }
      if (mark_objects)
        object_mark(unit_index);
      unit_index = *(int *)(unit + 0x1ac);
    }
  }

  if (existing_unit_index != -1 && best_unit_index == -1) {
    display_assert("(existing_unit_index == NONE) || (best_unit_index != NONE)",
                   "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0x6d9, true);
    system_exit(-1);
  }
  return best_unit_index;
}

/* prop_position_refresh (0x31df0): refresh a prop's cached positions and
 * the actor->prop vector.
 *
 * Skipped entirely when the actor is inactive (actor+0x8 byte == 0).  For an
 * orphan prop (state 4..5) with refresh_position == 0 the unit data is kept
 * unless the corpse has stopped (damage flag 4 at unit+0xb6, unit+0x3d0 word
 * zero, prop->perception zero and |velocity|^2 < *(float *)0x255d1c); a
 * stopped corpse is marked cheated/dead and its position is not refreshed.
 * For swarm props with refresh_vehicle set, the swarm unit is re-picked at
 * most every 90 ticks (actor_perception_unit_from_swarm, EAX = prop
 * actor_index, EDI = actor+0x120).  The unit's head/body/center-of-mass,
 * velocity (unit+0x18), root-parent location (object+0x48/+0x4c), underwater
 * flag, vehicle (parent type word 1) or attached-to-unit (type bit 0..1)
 * state and child_units_attached count are then refreshed.
 *
 * Always ends (label at 0x320f9) by resolving the actor's sense position into
 * out_data (actor_perception_find_sense_position), storing
 * actor_to_prop = body_position - out_data+0xc, distance = normalize3d(), and
 * substituting the global forward vector (*(float **)0x31fc3c) when the
 * distance is exactly 0.  actor_to_prop feeds the evade / berserk alignment
 * vectors, so it must never stay zero. */
void prop_position_refresh(int actor_handle, int prop_handle, void *out_data,
                           char param_4, char refresh_status)
{
  char *actor;
  prop_t *prop;
  prop_t *parent_prop;
  char *unit;
  char *object;
  float *sense;
  float *forward;
  int now;
  int new_unit;
  int child;
  char corpse_stopped;

  actor = (char *)datum_get(actor_data, actor_handle);
  if (*(char *)(actor + 0x8) == 0)
    return;

  prop = (prop_t *)datum_get(*(data_t **)0x5ab23c, prop_handle);
  unit = (char *)object_get_and_verify_type(prop->unit_index, 3);

  if ((char)param_4 == 0 && prop->state >= 4 && prop->state <= 5) {
    if (prop->orphan_corpse_cheated)
      goto update_actor_to_prop;
    if ((*(unsigned char *)(unit + 0xb6) & 4) != 0 &&
        *(int16_t *)(unit + 0x3d0) == 0 && prop->perception == 0 &&
        FUN_00012170((float *)(unit + 0x18)) < *(float *)0x255d1c)
      corpse_stopped = 1;
    else
      corpse_stopped = 0;
    if (((actor_t *)actor)->target_target_prop_index == prop_handle &&
        (!prop->tried_to_uncover || !prop->tried_to_search))
      goto update_actor_to_prop;
    if (!corpse_stopped)
      goto update_actor_to_prop;
    prop->orphan_corpse_cheated = 1;
    prop->dead = 1;
  }

  if (prop->swarm && prop->actor_index != -1 && refresh_status != 0) {
    now = game_time_get();
    if (prop->swarm_unit_selected_time + 0x5a <= now) {
      prop->swarm_unit_selected_time = now;
      new_unit = actor_perception_unit_from_swarm(
        prop->actor_index, actor + 0x120, actor_handle, prop->unit_index, 0);
      if (new_unit != prop->unit_index) {
        prop->unit_index = new_unit;
        unit = (char *)object_get_and_verify_type(new_unit, 3);
        /* 0x31f3d: both arms share one datum_get; orphans skip the NONE
         * test on +0xc. */
        if ((prop->state >= 4 && prop->state <= 5) ||
            prop->orphan_prop_index != -1) {
          parent_prop =
            (prop_t *)datum_get(*(data_t **)0x5ab23c, prop->orphan_prop_index);
          parent_prop->unit_index = prop->unit_index;
        }
      }
    }
  }

  unit_get_head_position(prop->unit_index, (float *)&prop->head_position);
  object_get_world_position(prop->unit_index,
                            (vector3_t *)&prop->body_position);
  unit_get_center_of_mass(prop->unit_index, (float *)&prop->center_of_mass);
  prop->velocity = *(real_vector3d *)(unit + 0x18);
  prop->pathfinding_surface_index = -1;
  object = (char *)object_get_and_verify_type(
    object_get_root_parent(prop->unit_index), -1);
  prop->body_location_leaf_index = *(int32_t *)(object + 0x48);
  *(int32_t *)&prop->body_location_cluster_index = *(int32_t *)(object + 0x4c);
  prop->underwater =
    FUN_0018f3e0(&prop->body_location_leaf_index, &prop->center_of_mass, 0);
  prop->vehicle_index = -1;
  prop->vehicle_gunner = 0;
  prop->dangerous_vehicle_driver = 0;
  prop->attached_to_unit_index = -1;

  if (*(int *)(unit + 0xcc) != -1) {
    object = (char *)object_get_and_verify_type(*(int *)(unit + 0xcc), -1);
    if (*(int16_t *)(object + 0x64) == 1) {
      prop->vehicle_index = *(int *)(unit + 0xcc);
      prop->vehicle_gunner =
        *(int *)(object + 0x2d8) == prop->unit_index || prop->type == 0xf;
      prop->dangerous_vehicle_driver =
        *(int *)(object + 0x2d4) == prop->unit_index &&
        vehicle_hover(prop->vehicle_index);
    } else if (((1 << *(char *)(object + 0x64)) & 3) != 0) {
      prop->attached_to_unit_index = *(int *)(unit + 0xcc);
    }
  }

  prop->child_units_attached = 0;
  for (child = *(int *)(unit + 0xc8); child != -1;
       child = *(int *)(object + 0xc4)) {
    object = (char *)object_get_and_verify_type(child, -1);
    if (((1 << *(char *)(object + 0x64)) & 3) != 0)
      prop->child_units_attached++;
  }

update_actor_to_prop:
  actor_perception_find_sense_position(
    actor_handle, (float *)&prop->body_position, prop_handle, out_data);
  sense = (float *)out_data;
  prop->actor_to_prop.i = prop->body_position.x - sense[3];
  prop->actor_to_prop.j = prop->body_position.y - sense[4];
  prop->actor_to_prop.k = prop->body_position.z - sense[5];
  prop->distance = normalize3d((float *)&prop->actor_to_prop);
  if (prop->distance == 0.0f) {
    forward = *(float **)0x31fc3c;
    prop->actor_to_prop = *(real_vector3d *)forward;
  }
}

/* actor_perception_assess_vehicle_danger (0x32170)
 * Record a moving vehicle as the actor's danger (danger type 3) when it beats
 * the danger already stored in the 0x6c-byte block at actor+0x280.
 *
 * Confirmed ABI (both call sites, 0x33f95 and 0x3473b): EAX = sense block
 * (MOV EDI,EAX at entry; NULL at 0x3473b, prop_status_refresh's out_data at
 * 0x33f95), then three cdecl stack args (ADD ESP,0xc).  The third is a byte
 * (MOV CL,[EBP+0x10]) stored at actor+0x286.  Returns AL: 1 when the danger
 * block was written, else 0 (XOR AL,AL at 0x32196 / 0x3236e).
 * Confirmed: returns 0 at once when the actor rides a vehicle (+0x158).
 * Confirmed: vehicle tag byte +0x2f0 must have its sign bit set (JNS), and
 * |vehicle velocity|^2 (object+0x18) must exceed 0.00111f (0x25620c).
 * Confirmed: a NULL sense block is replaced by a local one filled by
 * actor_perception_find_sense_position(actor, &vehicle_position, -1, .).
 * Confirmed: distance = sqrt of the x87 sum dz*dz + dx*dx + dy*dy (that
 * order, 0x32241..0x32251); accepted when distance < tag+0x4 + 10.0f
 * (0x253f34), and the stored type is < 3, or is 3 for another object with
 * distance < actor+0x2d4.
 * Confirmed stores: +0x28c object, +0x280 = 3, +0x290 = vehicle+0x2d4,
 * +0x294 = tag+0x4, +0x298 world position, +0x2a4 velocity (dword copies),
 * +0x284 = 0x14, +0x286 = param_4, +0x282 = 0, then +0x282 = 1 when the
 * vehicle+0x2d4 unit's team (+0x68) fails game_allegiance_get_team_is_friendly
 * against actor->meta_team_index.
 * Uncertain: the meaning of vehicle+0x2d4 (driver per prop_position_refresh)
 * and tag+0x4; raw offsets are kept for them and for the unnamed danger-zone
 * bytes (+0x282, +0x290, +0x298, +0x2a4).
 * Dormant (ported:false) body for the standalone build. */
bool actor_perception_assess_vehicle_danger(void *input_block /* @<eax> */,
                                            int actor_handle,
                                            int vehicle_handle, char param_4)
{
  actor_t *actor;
  char *vehicle;
  char *vehicle_tag;
  char *driver;
  float *velocity;
  float *sense_position;
  vector3_t position;
  char sense_block[0x38];
  float dx;
  float dy;
  float dz;
  float distance;
  bool result;

  actor = (actor_t *)datum_get(actor_data, actor_handle);
  result = 0;
  if (actor->vehicle_index != -1) {
    return result;
  }
  vehicle = (char *)object_get_and_verify_type(vehicle_handle, 2);
  vehicle_tag = (char *)tag_get(0x76656869, *(int *)vehicle);
  if (*(signed char *)(vehicle_tag + 0x2f0) >= 0) {
    return 0;
  }
  velocity = (float *)(vehicle + 0x18);
  if (!(velocity[0] * velocity[0] + velocity[1] * velocity[1] +
          velocity[2] * velocity[2] >
        *(float *)0x25620c)) {
    return 0;
  }

  object_get_world_position(vehicle_handle, &position);
  if (input_block == (void *)0) {
    actor_perception_find_sense_position(actor_handle, (float *)&position, -1,
                                         sense_block);
    input_block = sense_block;
  }
  sense_position = (float *)((char *)input_block + 0xc);
  dx = position.x - sense_position[0];
  dy = position.y - sense_position[1];
  dz = position.z - sense_position[2];
  distance = x87_sqrt(dz * dz + dx * dx + dy * dy);
  if (!(distance < *(float *)(vehicle_tag + 4) + *(float *)0x253f34)) {
    return 0;
  }

  if (actor->danger_zone_danger_type < 3 ||
      (actor->danger_zone_danger_type == 3 &&
       actor->danger_zone_object_index != vehicle_handle &&
       distance < actor->field_2d4)) {
    csmemset(&actor->danger_zone_danger_type, 0, 0x6c);
    actor->danger_zone_object_index = vehicle_handle;
    actor->danger_zone_danger_type = 3;
    *(int *)((char *)actor + 0x290) = *(int *)(vehicle + 0x2d4);
    *(int *)&actor->field_294 = *(int *)(vehicle_tag + 4);
    ((int *)((char *)actor + 0x298))[0] = *(int *)&position.x;
    ((int *)((char *)actor + 0x298))[1] = *(int *)&position.y;
    ((int *)((char *)actor + 0x298))[2] = *(int *)&position.z;
    ((int *)((char *)actor + 0x2a4))[0] = ((int *)velocity)[0];
    ((int *)((char *)actor + 0x2a4))[1] = ((int *)velocity)[1];
    ((int *)((char *)actor + 0x2a4))[2] = ((int *)velocity)[2];
    actor->field_284 = 0x14;
    actor->field_286 = param_4;
    *(int16_t *)((char *)actor + 0x282) = 0;
    if (*(int *)((char *)actor + 0x290) != -1) {
      driver = (char *)object_get_and_verify_type(*(int *)(vehicle + 0x2d4), 3);
      if (!game_allegiance_get_team_is_friendly(actor->meta_team_index,
                                                *(int16_t *)(driver + 0x68))) {
        *(int16_t *)((char *)actor + 0x282) = 1;
      }
    }
    return 1;
  }
  return 0;
}

/* actor_perception_refresh_danger_zone (0x32380, 0x5c0 bytes)
 * Refresh the actor's danger-zone block (+0x280..+0x2e8) for the current
 * danger object (+0x28c) and decide whether the actor perceives it.
 *
 * danger_type (+0x280): 1 = suicide unit, 2 = projectile, 3 = vehicle
 * (assert order at line 0xc9b is projectile, vehicle, suicide).  A vanished
 * object clears the type (MOV word [ESI+0x280],AX with AX = 0 at 0x323c6).
 *
 * Geometry: +0x2b0 object position, +0x2bc object velocity (3-dword integer
 * copy of object+0x18), +0x2d4 distance from the sense position (sense block
 * +0xc), +0x2c8 = velocity * 45 + position, +0x2dc = midpoint of +0x2b0 and
 * +0x2c8, +0x2d8 = |position - midpoint| + actor+0x294.  The x87 sums are
 * z*z + y*y + x*x in that order (0x3245e / 0x32508).
 *
 * +0x2e8 (word): projectile = round((1 - object+0x1f0) / object+0x1f4) via
 * FSTP float / FISTP (current rounding mode, not _ftol2), or -1; suicide =
 * animation frames remaining when the unit animation state is 0x19, else -1.
 *
 * Tail: +0x286 takes the "perceived" flag and +0x28a the "projectile is ours"
 * flag.  A newly perceived danger (old +0x286 == 0) triggers
 * actor_look_secondary(actor, 0xc, 1, {5,...}); only the look type word is
 * written, the other 14 buffer bytes are uninitialized in the original too.
 *
 * Constants: 0x2548f8 = 45.0f, 0x253398 = 0.5f, 0x256240 = 4.4444e-5f,
 * 0x253f34 = 10.0f, 0x2533c0 = 0.0f, 0x2533c8 = 1.0f.
 * Assertions: danger type at line 0xc9b, "object->object.type ==
 * _object_type_projectile" at line 0xcb9. */
void actor_perception_refresh_danger_zone(int actor_handle)
{
  char *actor;
  char *object;
  float *position;
  float *velocity;
  char sense_block[0x38];
  short look_buf[8];
  float dx;
  float dy;
  float dz;
  char perceived;
  char is_own_projectile;

  actor = (char *)datum_get(actor_data, actor_handle);
  if (*(short *)(actor + 0x280) <= 0) {
    return;
  }
  object =
    (char *)object_try_and_get_and_verify_type(*(int *)(actor + 0x28c), -1);
  if (object == (char *)0) {
    *(short *)(actor + 0x280) = 0;
    return;
  }
  if (*(short *)(actor + 0x280) != 2 && *(short *)(actor + 0x280) != 3 &&
      *(short *)(actor + 0x280) != 1) {
    display_assert(
      "(actor->danger_zone.danger_type == _actor_danger_zone_projectile) || "
      "(actor->danger_zone.danger_type == _actor_danger_zone_vehicle) || "
      "(actor->danger_zone.danger_type == _actor_danger_zone_suicide)",
      "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0xc9b, true);
    system_exit(-1);
  }

  position = (float *)(actor + 0x2b0);
  object_get_world_position(*(int *)(actor + 0x28c), (vector3_t *)position);
  actor_perception_find_sense_position(actor_handle, position, -1, sense_block);
  velocity = (float *)(actor + 0x2bc);
  *(vector3_t *)velocity = *(vector3_t *)(object + 0x18);

  dx = position[0] - *(float *)(sense_block + 0xc);
  dy = position[1] - *(float *)(sense_block + 0x10);
  dz = position[2] - *(float *)(sense_block + 0x14);
  *(float *)(actor + 0x2d4) = (float)x87_sqrtd(dz * dz + dy * dy + dx * dx);

  *(float *)(actor + 0x2c8) = velocity[0] * *(float *)0x2548f8 + position[0];
  *(float *)(actor + 0x2cc) = velocity[1] * *(float *)0x2548f8 + position[1];
  *(float *)(actor + 0x2d0) = velocity[2] * *(float *)0x2548f8 + position[2];
  *(float *)(actor + 0x2dc) =
    (position[0] + *(float *)(actor + 0x2c8)) * *(float *)0x253398;
  *(float *)(actor + 0x2e0) =
    (*(float *)(actor + 0x2cc) + position[1]) * *(float *)0x253398;
  *(float *)(actor + 0x2e4) =
    (*(float *)(actor + 0x2d0) + position[2]) * *(float *)0x253398;

  dx = position[0] - *(float *)(actor + 0x2dc);
  dy = position[1] - *(float *)(actor + 0x2e0);
  dz = position[2] - *(float *)(actor + 0x2e4);
  perceived = 0;
  is_own_projectile = 0;
  *(float *)(actor + 0x2d8) =
    (float)x87_sqrtd(dz * dz + dy * dy + dx * dx) + *(float *)(actor + 0x294);

  switch (*(short *)(actor + 0x280)) {
  case 3: /* vehicle */ {
    char *tag;
    char *encounter;
    char *location_object;
    int prop_index;
    int los_result;
    char ignore_visibility;

      tag = (char *)tag_get(0x76656869, *(int *)object);
      if (FUN_00012170((float *)(object + 0x18)) < *(float *)0x256240 ||
          *(float *)(tag + 4) + *(float *)0x253f34 < *(float *)(actor + 0x2d4)) {
        *(short *)(actor + 0x280) = 0;
        break;
      }
      perceived = *(char *)(actor + 0x286);
      if (perceived == 0) {
        if (*(int *)(object + 0x2d4) != -1 &&
            (prop_index = prop_get_active_by_unit_index(
               actor_handle, *(int *)(object + 0x2d4))) != -1) {
          perceived = (char)(*(short *)((char *)datum_get(prop_data, prop_index) +
                                        0x30) >= 2);
        } else {
          if (*(int *)(actor + 0x34) == -1) {
            encounter = (char *)0;
          } else {
            encounter = (char *)datum_get(encounter_data, *(int *)(actor + 0x34));
          }
          ignore_visibility = 0;
          if (*(short *)(actor + 0x6a) == 1 ||
              (encounter != (char *)0 && *(char *)(encounter + 0x40) != 0)) {
            ignore_visibility = 1;
          }
          if (*(int *)(object + 0xcc) == -1) {
            location_object = object;
          } else {
            location_object = (char *)object_get_and_verify_type(
              object_get_root_parent(*(int *)(actor + 0x28c)), -1);
          }
          los_result = ai_test_line_of_sight(
            (float *)sense_block, *(int *)(sense_block + 0x28), position,
            (int)*(unsigned short *)(location_object + 0x4c), 0, 0,
            *(int *)(actor + 0x28c), *(int *)(actor + 0x158) != -1);
          if (!ignore_visibility &&
              actor_visibility_at_point(
                actor_handle, sense_block, position, 0, (short)los_result, 1, 0,
                (short)actor_get_perception_knowledge(actor_handle, -1)) >= 2) {
            perceived = 1;
          } else if ((short)actor_audibility_at_point(
                       actor_handle, sense_block, position,
                       location_object + 0x48,
                       (short)*(unsigned short *)(tag + 0x182),
                       0x3f800000 /* 1.0f bits; never read */,
                       (short)los_result) >= 2) {
            perceived = 1;
          }
        }
      }
      break;
  }

  case 2: /* projectile */ {
    char *encounter;
    char *location_object;
    float frames;
    int los_result;
    short object_type_word;

      if (*(int *)(actor + 0x18) != -1 &&
          *(int *)(object + 0xcc) == *(int *)(actor + 0x18)) {
        is_own_projectile = 1;
      }
      if (*(short *)(object + 0x64) != 5) {
        display_assert("object->object.type == _object_type_projectile",
                       "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0xcb9, true);
        system_exit(-1);
      }
      if (*(float *)(object + 0x1f0) > *(float *)0x2533c0 &&
          *(float *)(object + 0x1f4) > *(float *)0x2533c0) {
        frames = (*(float *)0x2533c8 - *(float *)(object + 0x1f0)) /
                 *(float *)(object + 0x1f4);
        *(short *)(actor + 0x2e8) = (short)x87_round_to_int(frames);
      } else {
        *(short *)(actor + 0x2e8) = -1;
      }
      perceived = *(char *)(actor + 0x286) != 0 || is_own_projectile;
      if (perceived == 0) {
        if (*(int *)(actor + 0x34) == -1) {
          encounter = (char *)0;
        } else {
          encounter = (char *)datum_get(encounter_data, *(int *)(actor + 0x34));
        }
        if (*(short *)(actor + 0x6a) != 1 &&
            (encounter == (char *)0 || *(char *)(encounter + 0x40) == 0) &&
            *(float *)(actor + 0x2d4) <
              *(float *)((char *)tag_get(0x70726f6a, *(int *)object) + 0x19c)) {
          object_type_word = *(short *)(object + 0x4c);
          if (*(int *)(object + 0xcc) != -1) {
            location_object = (char *)object_get_and_verify_type(
              object_get_root_parent(*(int *)(actor + 0x28c)), -1);
            object_type_word = *(short *)(location_object + 0x4c);
          }
          los_result = ai_test_line_of_sight(
            (float *)sense_block, *(int *)(sense_block + 0x28), position,
            (int)(unsigned short)object_type_word, 0, 0, *(int *)(actor + 0x28c),
            *(int *)(actor + 0x158) != -1);
          if (actor_visibility_at_point(
                actor_handle, sense_block, position, 0, (short)los_result, 1, 0,
                (short)actor_get_perception_knowledge(actor_handle, -1)) >= 2) {
            perceived = 1;
          }
        }
      }
      break;
  }

  case 1: /* suicide */ {
    int prop_index;
    int16_t animation_state;
    int animation_frames;

      perceived = *(char *)(actor + 0x286);
      if (perceived == 0) {
        prop_index =
          prop_get_active_by_unit_index(actor_handle, *(int *)(actor + 0x28c));
        if (prop_index != -1) {
          perceived = (char)(*(short *)((char *)datum_get(prop_data, prop_index) +
                                        0x30) >= 2);
        }
      }
      animation_frames = unit_get_animation_frames_remaining(
        *(int *)(actor + 0x28c), &animation_state);
      *(short *)(actor + 0x2e8) =
        animation_state == 0x19 ? (short)animation_frames : -1;
      break;
  }
  }

  if (perceived != 0 && *(char *)(actor + 0x286) == 0) {
    look_buf[0] = 5;
    actor_look_secondary(actor_handle, 0xc, 1, look_buf);
  }

  *(char *)(actor + 0x286) = perceived;
  *(char *)(actor + 0x28a) = is_own_projectile;
}

/* actor_expected_acknowledgement (0x32940): walk the actor's props looking
 * for another prop that is "close" to prop_handle.
 *
 * The first datum_get(actor_data, actor_handle) result is discarded (EAX is
 * overwritten).  Asserts !prop_orphaned(prop) (prop +0x24 word not in [4,5],
 * line 0xe22).  For each iterated prop other than prop_handle (iterator[0]
 * compared at 0x329c3): it qualifies when its +0x18 or +0x1c dword matches
 * the prop's, or when both +0x60 bytes are nonzero and its +0x24 word is in
 * [4,5] or [2,3].  A qualifying prop sets the result (BL) to 1 when the
 * squared XY distance of the +0xbc/+0xc0 floats is < *0x253dcc, |dz| (+0xc4)
 * is < *(double *)0x256310, and the dot of the +0xe0 vectors is
 * > *0x253398.  The loop does not break early.  Field meanings are
 * unproven. */
bool actor_expected_acknowledgement(int actor_handle, int prop_handle)
{
  char *prop;
  char *current_prop;
  bool result;
  float dx;
  float dy;
  int iterator[2];

  (void)datum_get(actor_data, actor_handle);
  prop = (char *)datum_get(prop_data, prop_handle);
  result = 0;
  if (*(short *)(prop + 0x24) >= 4 && *(short *)(prop + 0x24) <= 5) {
    display_assert("!prop_orphaned(prop)",
                   "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0xe22, true);
    system_exit(-1);
  }
  prop_iterator_new(iterator, actor_handle);
  current_prop = (char *)prop_iterator_next(iterator);
  while (current_prop != NULL) {
    if (iterator[0] != prop_handle) {
      if (*(int *)(current_prop + 0x18) == *(int *)(prop + 0x18) ||
          *(int *)(current_prop + 0x1c) == *(int *)(prop + 0x1c) ||
          (*(char *)(prop + 0x60) != 0 &&
           *(char *)(current_prop + 0x60) != 0 &&
           ((*(short *)(current_prop + 0x24) >= 4 &&
             *(short *)(current_prop + 0x24) <= 5) ||
            (*(short *)(current_prop + 0x24) >= 2 &&
             *(short *)(current_prop + 0x24) <= 3)))) {
        dx = *(float *)(prop + 0xbc) - *(float *)(current_prop + 0xbc);
        dy = *(float *)(prop + 0xc0) - *(float *)(current_prop + 0xc0);
        if (dy * dy + dx * dx < *(float *)0x253dcc &&
            fabs((double)(*(float *)(current_prop + 0xc4) -
                          *(float *)(prop + 0xc4))) <
              *(double *)0x256310 &&
            *(float *)(current_prop + 0xe8) * *(float *)(prop + 0xe8) +
                *(float *)(current_prop + 0xe4) * *(float *)(prop + 0xe4) +
                *(float *)(current_prop + 0xe0) * *(float *)(prop + 0xe0) >
              *(float *)0x253398) {
          result = 1;
        }
      }
    }
    current_prop = (char *)prop_iterator_next(iterator);
  }
  return result;
}

/* actor_perception_unreachable (0x32ac0): mark a prop as reachable or not.
 *
 * The first datum_get(actor_data, actor_handle) result is discarded by the
 * original (EAX is immediately overwritten by the second call); the call is
 * kept for side-effect/order fidelity.  flag == 0 clears the unreachable
 * state (+0x9c word = 0, +0xa0 timestamp = NONE); otherwise the state is
 * raised to 1 only when currently 0, and the timestamp is refreshed from
 * game_time_get().  Both the knowledge byte (+0xa4) and the target weight
 * (+0x50) are then recomputed, in that order. */
void actor_perception_unreachable(int actor_handle, int leader_handle,
                                  char flag)
{
  char *prop;

  (void)datum_get(actor_data, actor_handle);
  prop = (char *)datum_get(*(data_t **)0x5ab23c, leader_handle);

  if (flag == 0) {
    *(uint16_t *)(prop + 0x9c) = 0;
    *(int *)(prop + 0xa0) = -1;
  } else {
    if (*(int16_t *)(prop + 0x9c) == 0)
      *(uint16_t *)(prop + 0x9c) = 1;
    *(int *)(prop + 0xa0) = game_time_get();
  }

  *(char *)(prop + 0xa4) =
    (char)actor_compute_prop_unopposable(actor_handle, leader_handle);
  *(float *)(prop + 0x50) =
    actor_compute_prop_target_weight(actor_handle, leader_handle);
}

/* actor_perception_tried_to_uncover: mark a prop as having been uncovered.
 *
 * Early-out when prop_handle == -1 (CMP ESI,-1 / JZ epilogue).
 * Fetches the actor record (datum_get(actor_data, actor_handle), EDI) and the
 * prop record (datum_get(prop_data, prop_handle), EAX), sets the prop's
 * +0xb9 byte flag to 1, then — if the prop is the actor's current target
 * (actor_t.target_target_prop_index, +0x270) — refreshes target and combat
 * status.
 *
 * Confirmed: both cdecl cleanups are coalesced (ADD ESP,0x10 for the two
 * 2-arg datum_get calls; ADD ESP,0x8 for the two 1-arg situation calls), so
 * the ARG_COUNT audit hazards are cdecl mis-grouping, not extra arguments.
 *
 * No __FILE__ string. */
void actor_perception_tried_to_uncover(int actor_handle, int prop_handle)
{
  char *actor;
  char *prop;

  if (prop_handle == -1)
    return;

  actor = (char *)datum_get(actor_data, actor_handle);
  prop = (char *)datum_get(prop_data, prop_handle);
  *(char *)(prop + 0xb9) = 1;

  if (prop_handle == ((actor_t *)actor)->target_target_prop_index) {
    actor_situation_update_target_status(actor_handle);
    actor_situation_combat_status_update(actor_handle);
  }
}

/* actor_perception_tried_to_search (0x32bb0): mark a prop as having been
 * searched for.
 *
 * Same shape as actor_perception_tried_to_uncover, one offset apart.
 * Early-out when prop_handle == -1 (CMP ESI,-1 / JZ epilogue at 0x32bba).
 * Fetches the actor record (datum_get(actor_data, actor_handle) -> EDI at
 * 0x32bc8) and the prop record (datum_get(prop_data, prop_handle) -> EAX at
 * 0x32bd7), sets the prop's +0xba byte flag to 1 (MOV byte ptr [EAX+0xba],1
 * at 0x32bdc), then - only when the prop is the actor's current target
 * (actor_t.target_target_prop_index, +0x270; CMP ESI,EAX at 0x32bec) -
 * refreshes target and combat status.
 *
 * Confirmed: both cdecl cleanups are coalesced (ADD ESP,0x10 at 0x32be9 for
 * the two 2-arg datum_get calls; ADD ESP,0x8 at 0x32bfc for the two 1-arg
 * situation calls), so the ARG_COUNT audit hazards are cdecl mis-grouping,
 * not extra arguments.
 *
 * No __FILE__ string. */
void actor_perception_tried_to_search(int actor_handle, int prop_handle)
{
  char *actor;
  char *prop;

  if (prop_handle == -1)
    return;

  actor = (char *)datum_get(actor_data, actor_handle);
  prop = (char *)datum_get(prop_data, prop_handle);
  *(char *)(prop + 0xba) = 1;

  if (prop_handle == ((actor_t *)actor)->target_target_prop_index) {
    actor_situation_update_target_status(actor_handle);
    actor_situation_combat_status_update(actor_handle);
  }
}

/* actor_perception_abandoned_search (0x32c10): the actor gives up on a search.
 *
 * prop_handle == -1 is the "no prop" path: clear the actor's search
 * bookkeeping (+0x3c4 word, +0x3bc/+0x3bd bytes, +0x72/+0x74 words, all
 * zeroed from a single XOR ECX,ECX at 0x32c2b) and refresh combat status.
 * Store order 0x3c4, 0x3bc, 0x3bd, 0x72, 0x74 is the reference order at
 * 0x32c2e..0x32c45 and is preserved deliberately.
 *
 * Otherwise: demote the prop's state at prop+0x24 from 4 to 5 (CMP word
 * [EAX+0x24],0x4 at 0x32c78) and set prop+0xbb = 1.  Only when the prop is
 * the actor's current target (actor+0x270, CMP ESI,[EDI+0x270] at 0x32c8c)
 * are the target/combat status refreshes run.
 *
 * Both datum_get calls in the second path share one ADD ESP,0x10 at 0x32c75;
 * the actor lookup lands in EDI and the prop lookup in EAX.
 *
 * No __FILE__ string. */
void actor_perception_abandoned_search(int actor_handle, int prop_handle)
{
  actor_t *actor;
  char *prop;

  if (prop_handle == -1) {
    actor = (actor_t *)datum_get(actor_data, actor_handle);
    actor->field_3c4 = 0;
    actor->field_3bc = 0;
    actor->field_3bd = 0;
    actor->field_072 = 0;
    actor->field_074 = 0;
    actor_situation_combat_status_update(actor_handle);
    return;
  }

  actor = (actor_t *)datum_get(actor_data, actor_handle);
  prop = (char *)datum_get(prop_data, prop_handle);
  if (*(short *)(prop + 0x24) == 4) {
    *(short *)(prop + 0x24) = 5;
  }
  *(prop + 0xbb) = 1;
  if (prop_handle == actor->target_target_prop_index) {
    actor_situation_update_target_status(actor_handle);
    actor_situation_combat_status_update(actor_handle);
  }
}

/* actor_emotion_update (0x32cb0): per-tick emotion bookkeeping for an actor.
 *
 * Order (all from the reference disassembly; field meanings unproven):
 *  1. actor+0x378 set and (+0x6e word == 0, +0x6a word < 3, or +0x1bc dword
 *     == 0x3f800000 with +0x6e < 3) -> actor_berserk(actor, 0).  The +0x1bc
 *     test is an integer CMP dword,0x3f800000 at 0x32cff.
 *  2. Mirror +0x1c9 into +0x374; on change, when the unit (+0x18) != NONE,
 *     ai_communication_event((flag != 0) + 0x16, unit, -1, -1, -1, -1, 0).
 *  3. +0x375 from +0x378 / actr flag 0x800, forced 0 when +0x158 != NONE,
 *     else 1 when actr flag 0x1000000 and +0x374 == 0.
 *  4. Scan the +0x1ee byte array down from index 9 for the first positive
 *     entry; the index picks the +0x350 target (2.0/1.8/1.6/1.2/0.7/0.0), and
 *     +0x354 eases toward it by (1 - exp(*(double *)0x256330)).
 *  5. With actr flags 0xc0000000: recompute the +0x35c..+0x35f bytes by
 *     walking the actor's props (actor_perception_aiming_vector_test_blockage
 *     calls; the 3rd/4th args are pointers passed through the kb int params).
 *  6. +0x35f -> actor_discard_firing_position + (+0x360 = 0x16), else count
 *     +0x360 down.  +0x35a timer: count down, or re-evaluate the actr
 *     switch at tag+0x2f8 (5-way jump table at 0x331de) and toggle +0x358,
 *     rearming +0x35a from tag+0x304/+0x308 seconds (0x2d when <= 0).
 *  7. Count +0x368 down; actor_emotion_unopposable_retreat(actor).
 *
 * FCOMP + TEST AH,0x41 + JNZ skip = taken when ST > mem; TEST AH,5 + JP skip
 * = taken when ST < mem.  0x61637472 = 'actr'.  No __FILE__ string. */
void actor_emotion_update(int actor_handle)
{
  char *actor;
  char *actr_tag;
  char *prop;
  char *target;
  int iter[2];
  real_vector3d friend_position;
  real_vector3d scaled_position;
  real_vector3d target_vector;
  real_vector3d blockage_vector;
  real_vector3d direction;
  float dot;
  float threshold;
  float limit;
  short level;
  short blocked;
  short blocked2;
  char flag;
  char check_target;
  char result;

  actor = (char *)datum_get(actor_data, actor_handle);
  actr_tag = (char *)tag_get(0x61637472, *(int *)(actor + 0x58));

  if (*(char *)(actor + 0x378) != 0 &&
      (*(short *)(actor + 0x6e) == 0 || *(short *)(actor + 0x6a) < 3 ||
       (*(int *)(actor + 0x1bc) == 0x3f800000 &&
        *(short *)(actor + 0x6e) < 3))) {
    actor_berserk(actor_handle, 0);
  }

  flag = *(char *)(actor + 0x1c9);
  if (*(char *)(actor + 0x374) != flag) {
    *(char *)(actor + 0x374) = flag;
    if (*(int *)(actor + 0x18) != -1) {
      ai_communication_event((flag != 0) + 0x16, *(int *)(actor + 0x18), -1, -1,
                             -1, -1, 0);
    }
  }

  *(char *)(actor + 0x375) =
    *(char *)(actor + 0x378) != 0 || (*(unsigned int *)actr_tag & 0x800) != 0;
  /* actor index != NONE */
  if (*(int *)(actor + 0x158) != -1) {
    *(char *)(actor + 0x375) = 0;
  } else if ((*(unsigned int *)actr_tag & 0x1000000) != 0 &&
             *(char *)(actor + 0x374) == 0) {
    *(char *)(actor + 0x375) = 1;
  }

  level = 9;
  do {
    if (*(signed char *)(actor + 0x1ee + level) > 0)
      break;
    level--;
  } while (level > 0);
  if (level >= 8) {
    *(float *)(actor + 0x350) = 2.0f;
  } else if (level >= 7) {
    *(float *)(actor + 0x350) = 1.8f;
  } else if (level >= 6) {
    *(float *)(actor + 0x350) = 1.6f;
  } else if (level >= 5) {
    *(float *)(actor + 0x350) = 1.2f;
  } else if (level >= 3) {
    *(float *)(actor + 0x350) = 0.7f;
  } else {
    *(float *)(actor + 0x350) = 0.0f;
  }
  *(float *)(actor + 0x354) =
    (*(float *)(actor + 0x350) - *(float *)(actor + 0x354)) *
      (*(float *)0x2533c8 - x87_exp(*(double *)0x256330)) +
    *(float *)(actor + 0x354);

  if (*(char *)(actor + 0x1c8) != 0) {
    *(int *)(actor + 0x3b4) = *(int *)(actor + 0x1b8);
  }

  if ((*(unsigned int *)actr_tag & 0xc0000000) != 0) {
    if (*(int *)(actor + 0x158) == -1 && *(short *)(actor + 0x6e) >= 3) {
      *(char *)(actor + 0x35d) = 0;
      *(char *)(actor + 0x35c) = 0;
      *(char *)(actor + 0x35e) = 0;
      *(char *)(actor + 0x35f) = 0;
      check_target = *(short *)(actor + 0x268) > 8;
      if (check_target) {
        /* actor+0x270 = actor_t.target_target_prop_index */
        target = (char *)datum_get(prop_data, *(int *)(actor + 0x270));
        target_vector = *(real_vector3d *)(target + 0xe0);
      }
      prop_iterator_new(iter, actor_handle);
      prop = (char *)prop_iterator_next(iter);
      while (prop != NULL) {
        if (*(short *)(prop + 0x24) >= 2 && *(short *)(prop + 0x24) <= 3 &&
            *(char *)(prop + 0x60) == 0 && *(char *)(prop + 0x127) == 0 &&
            *(char *)(prop + 0x14) == 0 &&
            (*(char *)(prop + 0x12e) != 0 || *(int *)(prop + 0x110) == -1)) {
          if (actor_perception_friend_prop_is_attacking(
                actor_handle, iter[0], (float *)&friend_position)) {
            blocked = actor_perception_aiming_vector_test_blockage(
              (float *)(prop + 0xbc), (float *)&friend_position,
              (int)(actor + 0x12c), (int)&blockage_vector);
            if (blocked >= 1) {
              *(char *)(actor + 0x35d) = 1;
              if (*(char *)(prop + 0x12e) != 0) {
                *(char *)(actor + 0x35c) = 1;
              }
            }
            if ((*(unsigned int *)actr_tag & 0x80000000) != 0 &&
                *(char *)(prop + 0x12e) != 0 && *(char *)(prop + 0x12f) != 0 &&
                FUN_00012170((float *)&blockage_vector) < *(float *)0x2533c8 &&
                (*(char *)(actor + 0x504) != 0 ||
                 *(short *)(actor + 0x360) > 0)) {
              direction = *(real_vector3d *)(actor + 0x518);
              if (normalize3d((float *)&direction) > *(float *)0x2533c0) {
                vector3d_scale_add((float *)(actor + 0x12c),
                                   (float *)&direction, 0.4f,
                                   (float *)&scaled_position);
                blocked2 = actor_perception_aiming_vector_test_blockage(
                  (float *)(prop + 0xbc), (float *)&friend_position,
                  (int)&scaled_position, 0);
                if (blocked2 <= blocked) {
                  blocked2 = blocked;
                }
                if (blocked2 >= 1) {
                  dot = direction.k * blockage_vector.k +
                        direction.j * blockage_vector.j +
                        direction.i * blockage_vector.i;
                  if (FUN_00012170((float *)&blockage_vector) <
                      *(float *)0x25337c) {
                    threshold = *(float *)0x2533c0;
                  } else {
                    threshold = *(float *)0x2533dc;
                  }
                  if (dot > threshold) {
                    *(char *)(actor + 0x35f) = 1;
                  }
                }
              }
            }
          }
          if (check_target) {
            blocked = actor_perception_aiming_vector_test_blockage(
              (float *)(actor + 0x12c), (float *)&target_vector,
              (int)(prop + 0xbc), 0);
            if (blocked >= 2) {
              *(char *)(actor + 0x35e) = 1;
            }
          }
        }
        prop = (char *)prop_iterator_next(iter);
      }
    } else {
      *(char *)(actor + 0x35d) = 0;
      *(char *)(actor + 0x35c) = 0;
      *(char *)(actor + 0x35e) = 0;
      *(char *)(actor + 0x35f) = 0;
    }
  }

  if (*(char *)(actor + 0x35f) != 0) {
    actor_discard_firing_position(actor_handle, *(short *)(actor + 0x3b8), 1);
    *(short *)(actor + 0x360) = 0x16;
  } else if (*(short *)(actor + 0x360) > 0) {
    *(short *)(actor + 0x360) = *(short *)(actor + 0x360) - 1;
  }

  if (*(short *)(actor + 0x35a) > 0) {
    *(short *)(actor + 0x35a) = *(short *)(actor + 0x35a) - 1;
  } else {
    if (*(char *)(actor + 0x374) != 0 && *(char *)(actor + 0x378) == 0) {
      limit = *(float *)(actr_tag + 0x300);
    } else {
      limit = *(float *)(actr_tag + 0x2fc);
    }
    switch (*(short *)(actr_tag + 0x2f8)) {
    case 1:
      result = *(float *)(actor + 0x354) > limit;
      break;
    case 2:
      result = *(float *)(actor + 0x1bc) < limit;
      break;
    case 3:
      result = *(float *)(actor + 0x1bc) > limit &&
               *(signed char *)(actor + 0x1f9) > 0;
      break;
    case 4:
      result = *(short *)(actor + 0x6e) > 0;
      break;
    case 5:
      result = actor_type_flood_desire_shamble(actor_handle);
      break;
    default:
      result = 0;
      break;
    }
    if ((*(unsigned int *)actr_tag & 0x40000000) != 0) {
      if (*(char *)(actor + 0x35c) != 0) {
        result = 1;
      } else if (*(char *)(actor + 0x35e) != 0) {
        result = 0;
      } else if (*(char *)(actor + 0x35d) != 0) {
        result = 1;
      }
    }
    if (*(char *)(actor + 0x358) != 0 && result == 0) {
      *(char *)(actor + 0x358) = result;
      if (*(float *)(actr_tag + 0x304) > *(float *)0x2533c0) {
        *(short *)(actor + 0x35a) =
          (short)(int)(*(float *)(actr_tag + 0x304) * TICKS_PER_SECOND);
      } else {
        *(short *)(actor + 0x35a) = 0x2d;
      }
    } else if (*(char *)(actor + 0x358) == 0 && result != 0) {
      *(char *)(actor + 0x358) = 1;
      if (*(float *)(actr_tag + 0x308) > *(float *)0x2533c0) {
        *(short *)(actor + 0x35a) =
          (short)(int)(*(float *)(actr_tag + 0x308) * TICKS_PER_SECOND);
      } else {
        *(short *)(actor + 0x35a) = 0x2d;
      }
    }
  }

  if (*(short *)(actor + 0x368) > 0) {
    *(short *)(actor + 0x368) = *(short *)(actor + 0x368) - 1;
  }
  actor_emotion_unopposable_retreat(actor_handle);
}

/* actor_perception_become_acknowledged (0x33330): promote a prop to the
 * "acknowledged" state (prop+0x24 == 3).
 *
 * Does nothing when the prop is already in state 2 or 3.  Otherwise it asks
 * actor_expected_acknowledgement whether the acknowledgement was expected,
 * and — when the prop still has a parent prop (prop+0xc != NONE) — folds the
 * parent's target weight block (+0x50..+0x5c) and its acknowledgement
 * bookkeeping (+0x9c, +0xa0, +0xa4, +0xa6, +0xa8) into this prop, retires the
 * parent link through actor_switch_props/prop_delete, and clears prop+0xc.
 *
 * Returns 1 when the promotion ran, 0 when the prop was already in state 2/3.
 * out_acknowledged (optional) receives the actor_expected_acknowledgement
 * result, or 0 on the skipped path.
 *
 * ADD ESP,0x1c at 0x33409 coalesces three cdecl cleanups: datum_get (8) +
 * actor_switch_props (12) + prop_delete (8) = 28.  A cleanup=7 ARG_COUNT
 * hazard on prop_delete is that coalescing, not a real arg mismatch.
 *
 * No __FILE__ string. */
char actor_perception_become_acknowledged(int actor_handle, int prop_handle,
                                          int out_acknowledged)
{
  char *prop;
  char *parent_prop;
  char has_parent;
  char acknowledged;
  char promoted;

  prop = (char *)datum_get(prop_data, prop_handle);
  promoted = 0;
  acknowledged = 0;
  if (*(short *)(prop + 0x24) < 2 || *(short *)(prop + 0x24) > 3) {
    has_parent = (char)(*(int *)(prop + 0xc) != -1);
    acknowledged =
      (char)actor_expected_acknowledgement(actor_handle, prop_handle);
    if (has_parent != 0) {
      parent_prop = (char *)datum_get(prop_data, *(int *)(prop + 0xc));
      *(int *)(prop + 0x50) = *(int *)(parent_prop + 0x50);
      *(int *)(prop + 0x54) = *(int *)(parent_prop + 0x54);
      *(int *)(prop + 0x58) = *(int *)(parent_prop + 0x58);
      *(int *)(prop + 0x5c) = *(int *)(parent_prop + 0x5c);
      *(short *)(prop + 0x9c) = *(short *)(parent_prop + 0x9c);
      *(int *)(prop + 0xa0) = *(int *)(parent_prop + 0xa0);
      *(prop + 0xa4) = *(parent_prop + 0xa4);
      *(short *)(prop + 0xa6) = *(short *)(parent_prop + 0xa6);
      *(short *)(prop + 0xa8) = *(short *)(parent_prop + 0xa8);
      actor_switch_props(actor_handle, *(int *)(prop + 0xc), prop_handle);
      prop_delete(actor_handle, *(int *)(prop + 0xc));
      *(int *)(prop + 0xc) = -1;
    }
    *(short *)(prop + 0x24) = 3;
    actor_perception_acknowledge(actor_handle, prop_handle, has_parent,
                                 acknowledged);
    promoted = 1;
  }
  if (out_acknowledged != 0) {
    *(char *)out_acknowledged = acknowledged;
  }
  return promoted;
}

/* prop_status_refresh (0x33440): refresh a prop's quantized motion/facing/
 * distance state and its perception (visibility, audibility, ineffability)
 * as seen by actor_handle.  out_data is the actor's 14-dword sense block
 * (head position at +0x0, cluster index word at +0x28, velocity at +0x2c)
 * that prop_position_refresh has just filled.
 *
 * Skipped when the actor is inactive (actor+0x8 byte == 0).  Orphan props
 * (state 4..5) only get a line-of-sight test and a searching-knowledge (2)
 * visibility sample.  The actor-perception desire test for a delayed
 * requirement decision (actor_perception_desire_prop) is inlined in the
 * original; the range-scale argument of actor_audibility_at_point is pushed
 * as the raw 1.0f bit pattern and is never read by the callee. */
void prop_status_refresh(int actor_handle, int prop_handle, void *out_data)
{
  actor_t *actor;
  char *definition;
  char *encounter;
  prop_t *prop;
  prop_t *orphan;
  char *unit;
  char *position;
  char *sound_unit;
  actor_t *source_actor;
  int game_time;
  int unit_actor_index;
  int line_of_sight_type;
  char blind;
  char noticed;
  char dead;
  char really_dead;
  char swarm;
  char noncombat;
  char in_combat;
  char fighting;
  char invisible;
  char use_maximum_distance;
  char lighting;
  short previous_quantized_speed;
  short visibility;
  short look_buf[8];
  float velocity[3];
  float speed;
  float relative_i;
  float relative_j;
  float relative_k;
  float closing_speed;
  float facing;
  float facing_distance;

  actor = (actor_t *)datum_get(actor_data, actor_handle);
  if (actor->field_008 == 0)
    return;

  definition = (char *)tag_get(0x61637472, actor->field_058);
  if (actor->field_034 == -1)
    encounter = NULL;
  else
    encounter = (char *)datum_get(*(data_t **)0x5ab270, actor->field_034);
  prop = (prop_t *)datum_get(*(data_t **)0x5ab23c, prop_handle);
  unit = (char *)object_get_and_verify_type(prop->unit_index, 3);
  game_time = game_time_get();
  position = (char *)out_data;
  blind = (encounter != NULL && *(char *)(encounter + 0x40) != 0) ||
          actor->field_06a == 1;

  prop->ignore = (char)((*(uint32_t *)(unit + 0x1b4) >> 10) & 1);
  if (prop->player && game_connection() == 0 && *(char *)0x5ac9c6 != 0)
    prop->ignore = 1;

  if (prop->enemy) {
    prop->preferred_target = (char)((*(uint32_t *)(unit + 0x1b4) >> 11) & 1);
    switch (actor->field_1d4) {
    case 1:
      if (prop->actor_index != -1 && actor->field_1d8 != (uint32_t)-1) {
        actor_t *prop_actor;

        prop_actor = (actor_t *)datum_get(actor_data, prop->actor_index);
        if ((prop_actor->field_034 & 0xffff) == (actor->field_1d8 & 0xffff)) {
          switch (actor->field_1d8 >> 30) {
          case 0:
            prop->preferred_target = 1;
            break;
          case 1:
            if ((short)((actor->field_1d8 >> 16) & 0xff) ==
                prop_actor->field_03a)
              prop->preferred_target = 1;
            break;
          case 2:
            if ((short)((actor->field_1d8 >> 16) & 0xff) ==
                prop_actor->field_03c)
              prop->preferred_target = 1;
            break;
          }
        }
      }
      break;
    case 2:
      if (prop->player)
        prop->preferred_target = 1;
      break;
    }
  }

  previous_quantized_speed = prop->quantized_speed;
  object_get_root_location(prop->unit_index, velocity, NULL);

  speed = x87_sqrt(velocity[1] * velocity[1] + velocity[0] * velocity[0] +
                   velocity[2] * velocity[2]);
  if (speed < *(float *)0x256350)
    prop->quantized_speed = 0;
  else if (speed < *(float *)0x25634c)
    prop->quantized_speed = 1;
  else if (speed < *(float *)0x2546a4)
    prop->quantized_speed = 2;
  else
    prop->quantized_speed = 3;

  relative_i = velocity[0] - *(float *)(position + 0x2c);
  relative_j = velocity[1] - *(float *)(position + 0x30);
  relative_k = velocity[2] - *(float *)(position + 0x34);
  closing_speed = -(relative_k * prop->actor_to_prop.k +
                    relative_j * prop->actor_to_prop.j +
                    relative_i * prop->actor_to_prop.i);
  if (closing_speed < *(float *)0x256348)
    prop->quantized_closing_speed = 0;
  else if (closing_speed < *(float *)0x256344)
    prop->quantized_closing_speed = 1;
  else if (closing_speed < *(float *)0x256340)
    prop->quantized_closing_speed = 2;
  else if (closing_speed < *(float *)0x256350)
    prop->quantized_closing_speed = 3;
  else if (closing_speed < *(float *)0x25634c)
    prop->quantized_closing_speed = 4;
  else if (closing_speed < *(float *)0x2546a4)
    prop->quantized_closing_speed = 5;
  else
    prop->quantized_closing_speed = 6;

  if (prop->state >= 2 && prop->state <= 3 && previous_quantized_speed <= 1 &&
      prop->quantized_speed > 1) {
    look_buf[0] = 1;
    *(int *)&look_buf[2] = prop_handle;
    actor_look_secondary(actor_handle, 2, 1, look_buf);
  }

  if (prop->distance < *(float *)0x2533c8)
    prop->quantized_distance = 0;
  else if (prop->distance < *(float *)0x254640)
    prop->quantized_distance = 1;
  else if (prop->distance < *(float *)0x253f34)
    prop->quantized_distance = 2;
  else if (prop->distance < *(float *)0x253394)
    prop->quantized_distance = 3;
  else
    prop->quantized_distance = 4;

  unit_get_aiming_vector(prop->unit_index, velocity);
  facing = -(velocity[1] * prop->actor_to_prop.j +
             velocity[2] * prop->actor_to_prop.k +
             velocity[0] * prop->actor_to_prop.i);
  if (facing <= *(float *)0x2533c0)
    facing_distance = *(float *)0x2548fc;
  else if (facing >= *(float *)0x2533c8)
    facing_distance = *(float *)0x2533c0;
  else
    facing_distance =
      x87_sqrt(*(float *)0x2533c8 - facing * facing) * prop->distance;

  if (facing > *(float *)0x25633c || facing_distance < *(float *)0x253398)
    prop->quantized_facing = 0;
  else if (facing > *(float *)0x256338 || facing_distance < *(float *)0x2533ec)
    prop->quantized_facing = 1;
  else if (facing > *(float *)0x253398)
    prop->quantized_facing = 2;
  else if (facing > *(float *)0x2533c0)
    prop->quantized_facing = 3;
  else
    prop->quantized_facing = 4;

  prop->shooting = prop->unit_effect == 1;

  if (prop->state >= 4 && prop->state <= 5) {
    if (prop->player && prop->enemy)
      line_of_sight_type = 2;
    else
      line_of_sight_type = 0;
    prop->line_of_sight = (int16_t)ai_test_line_of_sight(
      (float *)position, *(uint16_t *)(position + 0x28),
      (float *)&prop->head_position,
      (uint16_t)prop->body_location_cluster_index, (short)line_of_sight_type, 0,
      prop->vehicle_index, actor->vehicle_index != -1);
    if (prop->ignore || blind) {
      prop->perception = 0;
      prop->ineffability = 0;
      prop->audibility = 0;
      prop->visibility = 0;
    } else {
      prop->visibility = actor_visibility_at_point(
        actor_handle, position, (float *)&prop->head_position, prop->lighting,
        prop->line_of_sight, 1, 0, 2);
      prop->audibility = 0;
      prop->ineffability = 0;
      prop->perception = prop->visibility;
    }
  } else {
    noticed = 0;
    if (prop->player && prop->enemy)
      line_of_sight_type = 2;
    else
      line_of_sight_type = 0;
    prop->line_of_sight = (int16_t)ai_test_line_of_sight(
      (float *)position, *(uint16_t *)(position + 0x28),
      (float *)&prop->head_position,
      (uint16_t)prop->body_location_cluster_index, (short)line_of_sight_type, 0,
      prop->vehicle_index, actor->vehicle_index != -1);
    prop->lighting = 2;

    if (*(int16_t *)(unit + 0x64) == 0)
      prop->flying =
        (char)((*(uint32_t *)((char *)tag_get(0x62697064, *(int *)unit) +
                              0x2f4) >>
                2) &
               1);
    else
      prop->flying = 0;

    prop->active_camouflage = *(float *)(unit + 0x32c) > *(float *)0x253398;
    prop->flashlight = (char)((*(uint32_t *)(unit + 0x1b4) >> 19) & 1);

    dead = (char)((*(unsigned char *)(unit + 0xb6) >> 2) & 1);
    really_dead = dead && *(int16_t *)(unit + 0x3d0) == 0;
    prop->just_killed = dead && !prop->dead;
    prop->dead = dead;
    prop->really_dead = really_dead;

    if (prop->just_killed && !prop->enemy && actor->field_06a < 3)
      noticed = 1;

    if (dead)
      prop->required_ticks = 0;

    unit_actor_index = *(int *)(unit + 0x1a8);
    if (unit_actor_index != -1) {
      swarm = 1;
    } else {
      unit_actor_index = *(int *)(unit + 0x1a4);
      swarm = 0;
    }

    if (unit_actor_index != prop->actor_index) {
      prop->swarm = swarm;
      prop->actor_index = unit_actor_index;
      if (prop->orphan_prop_index != -1) {
        orphan =
          (prop_t *)datum_get(*(data_t **)0x5ab23c, prop->orphan_prop_index);
        orphan->actor_index = prop->actor_index;
        orphan->swarm = prop->swarm;
      }
    }

    if (unit_actor_index == -1) {
      noncombat = 0;
      in_combat = 0;
      fighting = !prop->dead;
    } else {
      noncombat = actor_is_noncombat(prop->actor_index);
      in_combat = actor_in_combat(prop->actor_index);
      fighting = actor_is_fighting(prop->actor_index);
      if (in_combat && !prop->in_combat && !prop->enemy && actor->field_06a < 3)
        noticed = 1;
    }

    prop->noncombat = noncombat;
    prop->in_combat = in_combat;
    prop->fighting = fighting;

    if (noticed) {
      visibility = 0;
      if (!blind) {
        lighting = prop->flashlight ? 2 : prop->lighting;
        visibility = actor_visibility_at_point(
          actor_handle, position, (float *)&prop->head_position, lighting,
          prop->line_of_sight, 1, 0,
          (short)actor_get_perception_knowledge(actor_handle, prop_handle));
      }
      if (visibility < 2) {
        prop->perception = visibility;
        prop->visibility = visibility;
        prop->state = 0;
      }
    }

    if (prop->ignore) {
      prop->perception = 0;
      prop->ineffability = 0;
      prop->audibility = 0;
      prop->visibility = 0;
    } else {
      invisible = blind;
      if ((game_connection() == 0 && *(char *)0x5ac9cb != 0) ||
          (game_connection() == 0 && *(char *)0x5ac9c7 != 0 && prop->player))
        invisible = 1;

      if (prop->active_camouflage)
        invisible =
          prop->enemy || (prop->player && prop->distance > *(float *)0x2533d8);

      if (invisible) {
        prop->visibility = 0;
        prop->just_became_visible = 0;
      } else {
        use_maximum_distance = 1;
        if (actor->field_15e == 4 || actor->meta_type == 0xf) {
          use_maximum_distance = 0;
        } else if (!prop->enemy) {
          use_maximum_distance = 0;
          if (actor->field_06a < 3 && (prop->dead || prop->in_combat))
            use_maximum_distance = 1;
        } else if (prop->state >= 2 && prop->state <= 3) {
          use_maximum_distance = 0;
        }

        lighting = prop->flashlight ? 2 : prop->lighting;
        visibility = actor_visibility_at_point(
          actor_handle, position, (float *)&prop->head_position, lighting,
          prop->line_of_sight, use_maximum_distance, prop->player,
          (short)actor_get_perception_knowledge(actor_handle, prop_handle));
        prop->just_became_visible = prop->visibility == 0 && visibility > 0;
        prop->visibility = visibility;
        if (visibility != 0) {
          prop->last_visible_head_position = prop->head_position;
          prop->last_visible_time = game_time;
        }
      }

      if ((game_connection() == 0 && *(char *)0x5ac9cc != 0) ||
          (encounter != NULL && *(char *)(encounter + 0x41) != 0)) {
        prop->audibility = 0;
      } else if (prop->unit_effect == 1 || prop->unit_effect == 2) {
        prop->audibility = 3;
      } else {
        sound_unit = (char *)object_get_and_verify_type(
          prop->vehicle_index != -1 ? prop->vehicle_index : prop->unit_index,
          3);
        prop->audibility = (int16_t)actor_audibility_at_point(
          actor_handle, position, (float *)&prop->body_position,
          &prop->body_location_leaf_index,
          *(int16_t *)((char *)tag_get(0x756e6974, *(int *)sound_unit) + 0x182),
          0x3f800000, prop->line_of_sight);
      }

      prop->ineffability = 0;
      if (prop->unit_effect == 0)
        prop->ineffability = 3;

      if (prop->flashlight && prop->quantized_facing <= 2 &&
          prop->quantized_distance <= 2 &&
          (prop->line_of_sight == 0 || prop->line_of_sight == 1))
        prop->ineffability = prop->ineffability > 1 ? prop->ineffability : 1;

      prop->perception =
        prop->visibility > (prop->audibility > prop->ineffability ?
                              prop->audibility :
                              prop->ineffability) ?
          prop->visibility :
          (prop->audibility > prop->ineffability ? prop->audibility :
                                                   prop->ineffability);
      if (prop->perception == 1 && prop->state >= 2 && prop->state <= 3)
        prop->perception = 2;
    }

    if (prop->perception != 0) {
      prop->last_perceived_body_position = prop->body_position;
      prop->last_perceived_time = game_time;
    }

    if (prop->state >= 2 && prop->state <= 3) {
      if (prop->visibility >= 2 ||
          (prop->definitely_located &&
           prop->definite_knowledge_source_actor != -1 &&
           (source_actor = (actor_t *)datum_absolute_index_to_index(
              actor_data, prop->definite_knowledge_source_actor)) != NULL &&
           source_actor->target_target_type >= 10 &&
           source_actor->target_target_prop_index != -1 &&
           source_actor->field_454 &&
           ((prop_t *)datum_get(*(data_t **)0x5ab23c,
                                source_actor->target_target_prop_index))
               ->unit_index == prop->unit_index)) {
        prop->definitely_located = 1;
        prop->ticks_since_definitely_located = 0;
      }
    }
  }

  if (prop->dangerous_vehicle_driver)
    actor_perception_assess_vehicle_danger(
      position, actor_handle, prop->vehicle_index, prop->perception >= 2);

  if (prop->suicide_radius > *(float *)0x2533c0 &&
      (prop->dead || *(unsigned char *)(unit + 0x253) == 0x1e))
    actor_perception_assess_suicide_danger(actor_handle, prop->unit_index,
                                           prop->suicide_radius, prop->distance,
                                           prop->enemy, prop->perception >= 2);

  if (prop->enemy && prop->state >= 2 && prop->state <= 3) {
    if ((actor_has_ranged_weapon(actor_handle) &&
         prop->distance < actor->control_weapon_maximum_range) ||
        ((*(uint32_t *)definition & 0x8000000) != 0 &&
         prop->distance < *(float *)(definition + 0x37c)))
      actor_perception_unreachable(actor_handle, prop_handle, 0);
  }

  if (prop->last_unreachable_time != -1 &&
      prop->last_unreachable_time + 150 < game_time)
    actor_perception_unreachable(actor_handle, prop_handle, 0);

  if (prop->delay_requirement_decision) {
    if (!actor_perception_desire_prop(
          actor_handle, -1, prop->unit_index, prop->actor_index, prop->in_use,
          prop->player, prop->enemy, prop->dead, prop->dead_ticks,
          prop->suicide_radius, prop->distance * prop->distance, 0,
          NULL))
      prop->required_ticks = 0;
    prop->delay_requirement_decision = 0;
  }

  prop->unopposable_enemy =
    actor_compute_prop_unopposable(actor_handle, prop_handle);
  prop->target_weight =
    actor_compute_prop_target_weight(actor_handle, prop_handle);
  prop->look_interest =
    actor_look_compute_prop_interest(actor_handle, prop_handle);
  prop->refresh_stimuli = 1;
}

/* Candidate list shared by actor_perception_refresh (0x34c80) and
 * actor_perception_refresh_test_object (0x342a0).  Two of these live in
 * actor_perception_refresh's frame at EBP-0x6c0 and EBP-0xcc4 (0x604 bytes
 * each; the four header words are zeroed at 0x34cab..0x34cc0).
 * Confirmed: +0x2 is the entry count (capped at 0x80, INC word [EDI+2]);
 * entries start at +0x4 with a 0xc stride (LEA [EAX+EAX*2] * 4), sorted by
 * qsort(list + 4, count, 0xc, actor_perception_qsort_compare_optional_props)
 * which compares the float at entry +0x8.  Entry +0x0 is a unit handle
 * (prop->unit_index / the tested unit), +0x4 a prop handle or -1 (a unit with
 * no prop yet).  +0x0 is a word counter bumped for props that stay (INC word
 * [EDI]); it caps how many candidates the refresh keeps.
 * Confirmed: the list at EBP-0x6c0 takes props with prop->enemy set (overflow
 * message "enemies" at 0x2564b8), the one at EBP-0xcc4 the rest ("friends").
 * Names are mechanical; the original type name is unknown. */
typedef struct {
  int32_t unit_index; /* +0x0 */
  int32_t prop_index; /* +0x4  NONE for a unit without a prop */
  float field_08; /* +0x8  sort key (distance squared) */
} actor_perception_candidate_t;

typedef struct {
  int16_t field_00; /* +0x0  props kept (word counter) */
  int16_t count; /* +0x2 */
  actor_perception_candidate_t entries[0x80]; /* +0x4 */
} actor_perception_candidate_list_t;


/* actor_perception_refresh_test_object (0x342a0)
 * Test one object (and, recursively, its children) for perception by an
 * actor: units become props or sorted candidates, vehicles and projectiles
 * feed the danger zone.
 *
 * Confirmed ABI: cdecl, 4 stack args (ADD ESP,0x10 at the recursive call
 * 0x3494b and at 0x352b0 / 0x352f0), void.  The last two are the enemy and
 * friend candidate lists of actor_perception_refresh (EBP-0x6c0 and
 * EBP-0xcc4 there).
 * Confirmed: walks the sibling chain object+0xc4, recursing into the first
 * child object+0xc8, and tests only objects object_mark returns true for.
 * Confirmed: object type (word object+0x64) 0 = unit path, 1 = vehicle (only
 * without a unit at vehicle+0x2d4; assess_vehicle_danger with EAX = NULL),
 * 5 = projectile.
 * Unit path: swarm units (unit+0x1a8 != NONE) resolve through
 * actor_perception_unit_from_swarm(EAX = unit+0x1a8, EDI = the sense block,
 * actor_handle, -1, 1); otherwise the unit's actor is unit+0x1a4.  The dead
 * test is unit byte +0xb6 bit 2 with word +0x3d0 == 0; dead ticks are
 * game_time - unit+0x3cc as a 16-bit SUB (0x7fff when +0x3cc is NONE).
 * Distance squared is the x87 sum dz*dz + dy*dy + dx*dx (0x34417..0x34425).
 * Constants: 0x255fe0 = 1600.0f, 0x255fdc = 225.0f, 0x255fd8 = 36.0f,
 * 0x254e74 = 16.0f, 0x254df8 = 64.0f, 0x253f34 = 10.0f.
 * Assert "!dead" at line 0xb96.
 * Projectile path: tag 'proj' +0x1a8 > 0 and (object+0xcc == NONE or byte
 * object+0x1dc bit 5); danger type 2 written like the vehicle case with
 * +0x284 = 0x1e and +0x290 = the projectile's object+0x74 when that object is
 * a unit type ((1 << type) & 3); +0x282 = 2 when it is the actor's own unit.
 * Uncertain: game_allegiance_get_team_is_friendly's result selects the ENEMY
 * list and is passed as prop_new_unacknowledged's flag, which the refresh
 * passes as 1 for enemies, so the kb name's "friendly" sense conflicts with
 * this use; the local is named team_flag to stay neutral.
 * Dormant (ported:false) body for the standalone build. */
void actor_perception_refresh_test_object(int actor_handle, int object_handle,
                                          void *enemy_list, void *friend_list)
{
  actor_t *actor;
  actor_t *unit_actor;
  char *object;
  char *unit;
  char *unit_tag;
  char *encounter;
  char *encounter_unit;
  char *projectile_tag;
  char *owner;
  actor_perception_candidate_list_t *list;
  actor_perception_candidate_t *entry;
  float *sense_position;
  vector3_t position;
  vector3_t projectile_position;
  char sense_block[0x38];
  char projectile_sense_block[0x38];
  float dx;
  float dy;
  float dz;
  float distance_squared;
  float distance;
  float suicide_radius;
  float threshold;
  int unit_handle;
  int unit_actor_handle;
  int prop_handle;
  int owner_handle;
  int last_time;
  int16_t object_type;
  short dead_ticks;
  char is_player;
  char team_flag;
  char dead;
  char add_candidate;
  char eligible;
  char encounter_idle;

  actor = (actor_t *)datum_get(actor_data, actor_handle);
  while (object_handle != -1) {
    object = (char *)object_get_and_verify_type(object_handle, -1);
    if ((char)object_mark(object_handle)) {
      unit = object;
      object_type = *(int16_t *)(object + 0x64);
      if (object_type == 0) {
        unit_handle = object_handle;
        object_get_world_position(object_handle, &position);
        actor_perception_find_sense_position(actor_handle, (float *)&position,
                                             -1, sense_block);
        if (*(int *)(unit + 0x1a8) != -1) {
          unit_actor_handle = *(int *)(unit + 0x1a8);
          unit_handle = actor_perception_unit_from_swarm(
            unit_actor_handle, sense_block, actor_handle, -1, 1);
          if (unit_handle == -1) {
            goto next;
          }
          unit = (char *)object_get_and_verify_type(unit_handle, 3);
          object_get_world_position(unit_handle, &position);
        } else {
          unit_actor_handle = *(int *)(unit + 0x1a4);
        }
        if (unit_handle == -1 || unit_actor_handle == actor_handle) {
          goto next;
        }

        unit_tag = (char *)tag_get(0x756e6974, *(int *)unit);
        is_player = *(int *)(unit + 0x1c8) != -1;
        team_flag = game_allegiance_get_team_is_friendly(
          actor->meta_team_index, *(int16_t *)(unit + 0x68));
        if ((*(unsigned char *)(unit + 0xb6) & 4) != 0 &&
            *(int16_t *)(unit + 0x3d0) == 0) {
          dead = 1;
          if (*(int *)(unit + 0x3cc) == -1) {
            dead_ticks = 0x7fff;
          } else {
            dead_ticks = (short)(game_time_get() - *(int *)(unit + 0x3cc));
          }
        } else {
          dead = 0;
          dead_ticks = 0;
        }
        suicide_radius = *(float *)(unit_tag + 0x284);
        sense_position = (float *)(sense_block + 0xc);
        dx = position.x - sense_position[0];
        dy = position.y - sense_position[1];
        dz = position.z - sense_position[2];
        distance_squared = dz * dz + dy * dy + dx * dx;
        if (suicide_radius > 0.0f &&
            (dead || *(char *)(unit + 0x253) == 0x1e)) {
          actor_perception_assess_suicide_danger(
            actor_handle, unit_handle, suicide_radius,
            x87_sqrt(distance_squared), team_flag, 0);
        }

        actor = (actor_t *)datum_get(actor_data, actor_handle);
        if (unit_actor_handle == -1) {
          unit_actor = (actor_t *)0;
        } else {
          unit_actor = (actor_t *)datum_get(actor_data, unit_actor_handle);
        }
        add_candidate = 0;
        if (is_player) {
          goto add;
        }
        if (unit_actor != (actor_t *)0 &&
            (unit_actor->field_008 == '\0' || unit_actor->field_013 != '\0')) {
          goto next;
        }
        if (distance_squared > *(float *)0x255fe0) {
          goto next;
        }
        if (dead) {
          eligible = 1;
          if (actor->field_034 != (uint32_t)-1) {
            encounter =
              (char *)datum_get(encounter_data, (int)actor->field_034);
            encounter_unit = (char *)object_get_and_verify_type(unit_handle, 3);
            last_time = *(int *)(encounter + 0x58);
            if (!(last_time > actor->field_3a0)) {
              last_time = actor->field_3a0;
            }
            if (last_time != -1 &&
                (*(int *)(encounter_unit + 0x3cc) == -1 ||
                 *(int *)(encounter_unit + 0x3cc) < last_time)) {
              eligible = 0;
            }
            if (*(char *)(encounter + 0x45) == '\0' &&
                *(char *)(encounter + 0x44) == '\0' &&
                *(char *)(encounter + 0x42) == '\0') {
              encounter_idle = 1;
            } else {
              encounter_idle = 0;
            }
            if (!eligible) {
              goto next;
            }
            if (encounter_idle) {
              if (distance_squared < *(float *)0x255fdc) {
                goto add;
              }
              goto next;
            }
          }
          if (suicide_radius > 0.0f) {
            goto add;
          }
          if (team_flag && dead_ticks > 0x96) {
            goto next;
          }
          if (actor_get_action_priority_flag(actor_handle) > 1) {
            goto next;
          }
          threshold = *(float *)0x254e74;
          if (!team_flag && actor->field_06a < 3) {
            threshold = *(float *)0x254df8;
          }
          if (distance_squared < threshold) {
            goto add;
          }
          goto next;
        }
        if (team_flag) {
          add_candidate = distance_squared > *(float *)0x255fd8;
          goto add;
        }
        eligible = distance_squared < *(float *)0x255fdc;
        if (actor->field_06e >= 4) {
          add_candidate = 1;
        } else {
          add_candidate = 1;
          if (actor->field_1cc != '\0' ||
              !(distance_squared > *(float *)0x254e74)) {
            add_candidate = 0;
          }
        }
        if (!eligible) {
          goto next;
        }

      add:
        list = (actor_perception_candidate_list_t *)(team_flag ? enemy_list :
                                                                 friend_list);
        if (add_candidate) {
          if (dead) {
            display_assert("!dead", "c:\\halo\\SOURCE\\ai\\actor_perception.c",
                           0xb96, 1);
            system_exit(-1);
          }
          if (list->count < 0x80) {
            entry = &list->entries[list->count];
            entry->prop_index = -1;
            entry->unit_index = unit_handle;
            entry->field_08 = distance_squared;
            list->count++;
          }
        } else {
          prop_handle =
            prop_new_unacknowledged(actor_handle, unit_handle, team_flag);
          if (prop_handle != -1) {
            prop_position_refresh(actor_handle, prop_handle, sense_block, 0, 0);
            if (!dead) {
              list->field_00++;
            }
          }
        }
      } else if (object_type == 1) {
        if (*(int *)(object + 0x2d4) == -1) {
          actor_perception_assess_vehicle_danger((void *)0, actor_handle,
                                                 object_handle, 0);
        }
      } else if (object_type == 5) {
        projectile_tag = (char *)tag_get(0x70726f6a, *(int *)object);
        if (*(float *)(projectile_tag + 0x1a8) > 0.0f &&
            (*(int *)(object + 0xcc) == -1 ||
             (*(unsigned char *)(object + 0x1dc) & 0x20) != 0)) {
          object_get_world_position(object_handle, &projectile_position);
          actor_perception_find_sense_position(actor_handle,
                                               (float *)&projectile_position,
                                               -1, projectile_sense_block);
          sense_position = (float *)(projectile_sense_block + 0xc);
          dx = projectile_position.x - sense_position[0];
          dy = projectile_position.y - sense_position[1];
          dz = projectile_position.z - sense_position[2];
          distance = x87_sqrt(dz * dz + dy * dy + dx * dx);
          if (distance <
                *(float *)(projectile_tag + 0x1a8) + *(float *)0x253f34 &&
              (actor->danger_zone_danger_type < 2 ||
               (actor->danger_zone_danger_type == 2 &&
                actor->danger_zone_object_index != object_handle &&
                distance < actor->field_2d4))) {
            csmemset(&actor->danger_zone_danger_type, 0, 0x6c);
            actor->danger_zone_danger_type = 2;
            actor->danger_zone_object_index = object_handle;
            *(int *)&actor->field_294 = *(int *)(projectile_tag + 0x1a8);
            ((int *)((char *)actor + 0x298))[0] =
              *(int *)&projectile_position.x;
            ((int *)((char *)actor + 0x298))[1] =
              *(int *)&projectile_position.y;
            ((int *)((char *)actor + 0x298))[2] =
              *(int *)&projectile_position.z;
            ((int *)((char *)actor + 0x2a4))[0] = ((int *)(object + 0x18))[0];
            ((int *)((char *)actor + 0x2a4))[1] = ((int *)(object + 0x18))[1];
            ((int *)((char *)actor + 0x2a4))[2] = ((int *)(object + 0x18))[2];
            actor->field_284 = 0x1e;
            actor->field_286 = 0;
            *(int16_t *)((char *)actor + 0x282) = 0;
            owner_handle = -1;
            if (*(int *)(object + 0x74) != -1) {
              owner = (char *)object_try_and_get_and_verify_type(
                *(int *)(object + 0x74), -1);
              if (owner != (char *)0 &&
                  ((1 << *(unsigned char *)(owner + 0x64)) & 3) != 0) {
                owner_handle = *(int *)(object + 0x74);
                if (actor->meta_unit_index != -1 &&
                    owner_handle == actor->meta_unit_index) {
                  *(int16_t *)((char *)actor + 0x282) = 2;
                } else if (!game_allegiance_get_team_is_friendly(
                             actor->meta_team_index,
                             *(int16_t *)(object + 0x68))) {
                  *(int16_t *)((char *)actor + 0x282) = 1;
                }
              }
            }
            *(int *)((char *)actor + 0x290) = owner_handle;
          }
        }
      }
    }

  next:
    if (*(int *)(object + 0xc8) != -1) {
      actor_perception_refresh_test_object(
        actor_handle, *(int *)(object + 0xc8), enemy_list, friend_list);
    }
    object_handle = *(int *)(object + 0xc4);
  }
}

/* actor_perception_create_orphan_from_friend (0x34970): a friend reported
 * the unit (unit_handle is the unit/object handle passed to
 * prop_get_base_by_unit_index).  Finds or creates the actor's base prop for
 * that unit; unless it is already acknowledged (state 2..3) it either reuses
 * the existing orphan (resetting it to uninspected when there is no friend
 * prop, else copying the friend's information into it) or creates a new
 * orphan (prop_orphan_from_friend with friend_prop_handle as the friend prop,
 * or prop_orphan_transition when there is none).  The resulting prop is then
 * marked definitely located (when there is no source actor, or the friend
 * prop's visibility >= 2) and has its unopposable / target weight refreshed.
 *
 * Returns a byte in AL: 1 when there is no base prop and on the normal paths,
 * 0 when the base prop was already acknowledged (the refresh below still runs)
 * and when the new orphan could not be created.
 * position_data is the 0x38-byte sense block at EBP-0x44. */
bool actor_perception_create_orphan_from_friend(int actor_handle,
                                                int unit_handle,
                                                int source_actor_handle,
                                                int friend_prop_handle)
{
  prop_t *prop;
  char position_data[0x38];
  int prop_handle;
  int orphan_handle;
  bool result;

  result = 1;
  prop_handle = prop_get_base_by_unit_index(actor_handle, unit_handle, 1, 0);
  if (prop_handle == -1) {
    goto done;
  }
  prop = (prop_t *)datum_get(prop_data, prop_handle);
  if (prop->state >= 2 && prop->state <= 3) {
    result = 0;
  } else if (prop->orphan_prop_index != -1) {
    prop_t *orphan;
    char orphan_reset;

    orphan_handle = prop->orphan_prop_index;
    orphan = (prop_t *)datum_get(prop_data, orphan_handle);
    orphan_reset = 0;
    if (prop->state < 0 || prop->state > 1) {
      display_assert("prop_unacknowledged(current_prop)",
                     "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0xeb4, 1);
      system_exit(-1);
    }
    if (orphan->state < 4 || orphan->state > 5) {
      display_assert("prop_orphaned(current_orphan)",
                     "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0xeb5, 1);
      system_exit(-1);
    }
    if (prop->owner_actor_index != actor_handle) {
      display_assert("current_prop->owner_actor_index == actor_index",
                     "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0xeb6, 1);
      system_exit(-1);
    }
    if (orphan->owner_actor_index != actor_handle) {
      display_assert("current_orphan->owner_actor_index == actor_index",
                     "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0xeb7, 1);
      system_exit(-1);
    }
    if (prop->orphan_prop_index != orphan_handle) {
      display_assert(
        "current_prop->orphan_prop_index == current_orphan_index",
        "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0xeb8, 1);
      system_exit(-1);
    }
    if (orphan->parent_prop_index != prop_handle) {
      display_assert(
        "current_orphan->parent_prop_index == current_prop_index",
        "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0xeb9, 1);
      system_exit(-1);
    }
    if (friend_prop_handle != -1) {
      prop_orphan_update_information(actor_handle, orphan_handle,
                                     friend_prop_handle);
      prop->unit_index = orphan->unit_index;
    } else {
      orphan->state = _prop_state_uninspected_orphan;
      orphan->orphan_inspection_ticks = 0;
      orphan_reset = 1;
    }
    prop_position_refresh(actor_handle, orphan_handle, position_data,
                          orphan_reset, 1);
    prop_status_refresh(actor_handle, orphan_handle, position_data);
    prop_handle = orphan_handle;
    prop = (prop_t *)datum_get(prop_data, orphan_handle);
  } else {
    if (prop->state < 0 || prop->state > 1) {
      display_assert("prop_unacknowledged(current_prop)",
                     "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0xedf, 1);
      system_exit(-1);
    }
    if (friend_prop_handle != -1) {
      prop_handle =
        prop_orphan_from_friend(actor_handle, prop_handle, friend_prop_handle);
      if (prop_handle != -1) {
        prop_t *new_prop = (prop_t *)datum_get(prop_data, prop_handle);

        new_prop->unit_index = prop->unit_index;
        new_prop->actor_index = prop->actor_index;
        new_prop->swarm = prop->swarm;
      }
    } else {
      prop_position_refresh(actor_handle, prop_handle, position_data, 0, 0);
      prop_handle = prop_orphan_transition(actor_handle, prop_handle);
    }
    if (prop_handle == -1) {
      result = 0;
      goto done;
    }
    prop = (prop_t *)datum_get(prop_data, prop_handle);
  }
  if (prop != NULL) {
    if (source_actor_handle == -1 ||
        (friend_prop_handle != -1 &&
         ((prop_t *)datum_get(prop_data, friend_prop_handle))->visibility >=
           2)) {
      prop->definitely_located = 1;
      prop->ticks_since_definitely_located = 0;
      prop->definite_knowledge_source_actor = source_actor_handle;
    }
    prop->unopposable_enemy =
      actor_compute_prop_unopposable(actor_handle, prop_handle);
    prop->target_weight =
      actor_compute_prop_target_weight(actor_handle, prop_handle);
  }
done:
  return result;
}

/* actor_perception_refresh (0x34c80)
 * Rebuilds the actor's prop list: gathers the cluster sound bit vector
 * (swarm members OR'd, or the actor's own cluster), walks every prop and
 * decides keep/add via actor_perception_desire_prop (inlined in the original),
 * marks relevant objects, queues props for the enemy/friend lists, tests every
 * object in audible clusters via 0x342a0, then promotes the closest queued
 * candidates and deletes the rest.
 *
 * Candidate lists (EBP-0x6c0 enemies, EBP-0xcc4 friends) are
 * {int16 active, int16 count, {object, prop, weight}[0x80]}; the list is
 * chosen by prop+0x60 and the overflow message names "enemies" for the
 * EBP-0x6c0 list.  Unknown thresholds are left as raw constants. */
void actor_perception_refresh(int actor_handle)
{
  struct candidate_list {
    int16_t active; /* +0x0 */
    int16_t count; /* +0x2 */
    struct {
      int object_handle; /* +0x0 */
      int prop_handle; /* +0x4 */
      float weight; /* +0x8 */
    } entries[0x80]; /* +0x4 */
  };
  struct candidate_list friends; /* EBP-0xcc4 */
  struct candidate_list enemies; /* EBP-0x6c0 */
  char position_data[0x38]; /* EBP-0xbc */
  uint32_t cluster_bits_buffer[0x10]; /* EBP-0x84 */
  char cluster_iter[8]; /* EBP-0x44 */
  char *actor; /* EBP-0x34 */
  int iter[2]; /* EBP-0x28 */
  char *bsp; /* EBP-0x1c */
  uint32_t *cluster_bits; /* EBP-0x10 */
  float distance_squared; /* EBP-0xc */
  bool optional; /* EBP-0x5 */
  bool desired;
  char found;
  prop_t *prop;
  char *swarm;
  char *unit;
  char *owner_actor;
  struct candidate_list *list;
  int unit_handle;
  int new_prop;
  int16_t cluster;
  int16_t i;
  int16_t total;
  int16_t friend_limit;

  bsp = (char *)scenario_get();
  actor = (char *)datum_get(actor_data, actor_handle);
  cluster_bits = NULL;
  enemies.count = 0;
  enemies.active = 0;
  friends.count = 0;
  friends.active = 0;
  if (*(char *)(actor + 6) != 0) {
    swarm = (char *)datum_get(*(data_t **)0x6325a0, *(int *)(actor + 0x28));
    found = 0;
    csmemset(cluster_bits_buffer, 0, 0x40);
    for (i = 0; i < *(int16_t *)(swarm + 2); i++) {
      uint32_t *cluster_pvs;

      unit =
        (char *)object_get_and_verify_type(*(int *)(swarm + i * 4 + 0x18), 3);
      if (*(int16_t *)(unit + 0x4c) != -1) {
        cluster_pvs =
          structure_bsp_get_cluster_sound_data(bsp, *(int16_t *)(unit + 0x4c));
        bit_vector_or(*(uint16_t *)(bsp + 0x134), (int)cluster_pvs,
                      (int)cluster_bits_buffer, (int)cluster_bits_buffer);
        found = 1;
      }
    }
    if (found) {
      cluster_bits = cluster_bits_buffer;
    }
  } else if (*(int16_t *)(actor + 0x148) != -1) {
    cluster_bits =
      structure_bsp_get_cluster_sound_data(bsp, *(int16_t *)(actor + 0x148));
  }

  object_reset_markers();
  prop_iterator_new(iter, actor_handle);
  prop = (prop_t *)prop_iterator_next(iter);
  while (prop != NULL) {
    if (prop->state < 4 || prop->state > 5) {
      distance_squared = prop->distance * prop->distance;
      desired = actor_perception_desire_prop(
        actor_handle, -1, prop->unit_index, prop->actor_index, prop->in_use,
        prop->player, prop->enemy, prop->dead, prop->dead_ticks,
        prop->suicide_radius, distance_squared, prop->required_ticks,
        &optional);
      if (desired && cluster_bits != NULL) {
        desired = 0;
        cluster = object_get_first_cluster(cluster_iter, prop->unit_index);
        while (cluster != -1) {
          if ((cluster_bits[cluster >> 5] & (1 << (cluster & 0x1f))) != 0) {
            desired = 1;
            break;
          }
          cluster =
            object_get_next_cluster(cluster_iter, prop->unit_index);
        }
      }
      if (prop->swarm != 0 && prop->actor_index != -1) {
        owner_actor = (char *)datum_get(actor_data, prop->actor_index);
        if (*(int *)(owner_actor + 0x28) != -1) {
          swarm = (char *)datum_get(*(data_t **)0x6325a0,
                                    *(int *)(owner_actor + 0x28));
          for (i = 0; i < *(int16_t *)(swarm + 2); i++) {
            object_mark(*(int *)(swarm + i * 4 + 0x18));
          }
        } else {
          unit_handle = *(int *)(actor + 0x24);
          while (unit_handle != -1) {
            unit = (char *)object_get_and_verify_type(unit_handle, 3);
            object_mark(unit_handle);
            unit_handle = *(int *)(unit + 0x1ac);
          }
        }
      }
      object_mark(prop->unit_index);
      if (desired) {
        list = prop->enemy != 0 ? &enemies : &friends;
        if (optional) {
          if (prop->dead != 0) {
            display_assert("!prop->dead",
                           "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0xa6d,
                           1);
            system_exit(-1);
          }
          if (list->count < 0x80) {
            list->entries[list->count].object_handle = prop->unit_index;
            list->entries[list->count].prop_handle = iter[0];
            list->entries[list->count].weight =
              distance_squared * 0.69444442f;
            list->count++;
          } else if (*(int *)0x2c8520 == -1 ||
                     game_time_get() > *(int *)0x2c8520 + 0x96) {
            error(2,
                  "actor_perception_refresh overflowed max %s (%d), "
                  "discarding",
                  prop->enemy != 0 ? "enemies" : "friends", 0x80);
            *(int *)0x2c8520 = game_time_get();
          }
        } else if (prop->dead == 0) {
          list->active++;
        }
      } else {
        if ((prop->state < 4 || prop->state > 5) &&
            prop->orphan_prop_index != -1) {
          actor_switch_props(actor_handle, prop->orphan_prop_index, -1);
          prop_delete(actor_handle, prop->orphan_prop_index);
        }
        actor_switch_props(actor_handle, iter[0], -1);
        prop_delete(actor_handle, iter[0]);
      }
    }
    prop = (prop_t *)prop_iterator_next(iter);
  }

  if (cluster_bits != NULL) {
    for (i = 0; i < *(int *)(bsp + 0x134); i++) {
      if ((cluster_bits[i >> 5] & (1 << (i & 0x1f))) != 0) {
        int cluster_state;
        int object_handle;

        object_handle = cluster_partition_object_iter_first(&cluster_state, i);
        while (object_handle != -1) {
          actor_perception_refresh_test_object(actor_handle, object_handle,
                                               &enemies, &friends);
          object_handle = cluster_partition_object_iter_next(&cluster_state);
        }
        object_handle =
          cluster_get_first_noncollideable_object(&cluster_state, i);
        while (object_handle != -1) {
          actor_perception_refresh_test_object(actor_handle, object_handle,
                                               &enemies, &friends);
          object_handle =
            cluster_get_next_noncollideable_object(&cluster_state);
        }
      }
    }
  }

  if (enemies.count > 0) {
    i = 0;
    if (enemies.active < 4) {
      qsort(enemies.entries, enemies.count, 0xc,
            (qsort_compar_proc)actor_perception_qsort_compare_optional_props);
      for (i = 0; i < enemies.count; i++) {
        if (enemies.entries[i].prop_handle == -1) {
          new_prop = prop_new_unacknowledged(
            actor_handle, enemies.entries[i].object_handle, 1);
          if (new_prop == -1) {
            continue;
          }
          prop_position_refresh(actor_handle, new_prop, position_data, 0, 0);
        }
        enemies.active++;
        if (enemies.active >= 4) {
          break;
        }
      }
    }
    for (; i < enemies.count; i++) {
      if (enemies.entries[i].prop_handle != -1) {
        prop = (prop_t *)datum_get(*(data_t **)0x5ab23c,
                                 enemies.entries[i].prop_handle);
        if ((prop->state < 4 || prop->state > 5) &&
            prop->orphan_prop_index != -1) {
          actor_switch_props(actor_handle, prop->orphan_prop_index, -1);
          prop_delete(actor_handle, prop->orphan_prop_index);
        }
        actor_switch_props(actor_handle, enemies.entries[i].prop_handle, -1);
        prop_delete(actor_handle, enemies.entries[i].prop_handle);
      }
    }
  }

  if (friends.count > 0) {
    total = friends.active + enemies.active;
    i = 0;
    friend_limit = (int16_t)(enemies.active + 2 > 4 ? enemies.active + 2 : 4);
    if (total < friend_limit) {
      qsort(friends.entries, friends.count, 0xc,
            (qsort_compar_proc)actor_perception_qsort_compare_optional_props);
      for (i = 0; i < friends.count; i++) {
        if (friends.entries[i].prop_handle == -1) {
          new_prop = prop_new_unacknowledged(
            actor_handle, friends.entries[i].object_handle, 0);
          if (new_prop == -1) {
            continue;
          }
          prop_position_refresh(actor_handle, new_prop, position_data, 0, 0);
        }
        friends.active++;
        total++;
        if (total >= friend_limit) {
          break;
        }
      }
    }
    for (; i < friends.count; i++) {
      if (friends.entries[i].prop_handle != -1) {
        prop = (prop_t *)datum_get(*(data_t **)0x5ab23c,
                                 friends.entries[i].prop_handle);
        if ((prop->state < 4 || prop->state > 5) &&
            prop->orphan_prop_index != -1) {
          actor_switch_props(actor_handle, prop->orphan_prop_index, -1);
          prop_delete(actor_handle, prop->orphan_prop_index);
        }
        actor_switch_props(actor_handle, friends.entries[i].prop_handle, -1);
        prop_delete(actor_handle, friends.entries[i].prop_handle);
      }
    }
  }
  object_marker_end();
}

/* actor_perception_update (0x355f0): actor_perception_update — the per-tick
 * perception pass for one actor.
 *
 * Phase 1 (skipped when actor+0x13 is set): refresh perception and the danger
 * zone, then advance the alertness/awareness ramp on actor+0x280..0x28c using
 * the actr definition's two probabilities (+0x50 for alertness 2, +0x54 for
 * alertness 3) and the global random seed.  Finally clamp actor+0x546 to 5
 * when actor+0x544 == 0xc.
 *
 * Phase 2 (always): walk every prop of the actor.  Per prop: age the timers
 * (+0x66/0x68, +0x6c, +0xb0, +0x76, +0x4c, +0x6a, +0x9c, +0xa8/0xa6, +0x78),
 * recompute the "seen" bookkeeping (+0x26 -> awareness_ticks, +0x63), refresh
 * position/status, then run the prop state machine on prop+0x24 (states 0..5)
 * producing new_state, apply it, recompute prop+0xa4 and prop+0x50, generate
 * events, and track the closest orphan prop.
 *
 * Both results are written on every return path:
 *   actor+0x4e = winning awareness slot, actor+0x54 = best orphan prop handle.
 *
 * TU: c:\halo\SOURCE\ai\actor_perception.c.  Asserts at lines 0x13c, 0x192,
 * 0x1a0, 0x1e9, 0x1ea, 0x204, 0x2b6, 0x2d1, 0x2da, 0x2ea, 0x2ef. */
void actor_perception_update(int actor_handle)
{
  char debug_desc_a[256]; /* EBP-0x4f4 */
  char debug_desc_b[256]; /* EBP-0x3f4 */
  char debug_desc_c[256]; /* EBP-0x2f4 */
  char debug_desc_d[256]; /* EBP-0x1f4 */
  char position_data_b[0x38]; /* EBP-0xf4  (second refresh site)  */
  char position_data_a[0x38]; /* EBP-0xbc  (shared with status refresh) */
  const char *awareness_names[5]; /* EBP-0x84 */
  const char *perception_names[4]; /* EBP-0x70 */
  const char *knowledge_names[4]; /* EBP-0x60 */
  struct {
    int16_t actor_team; /* +0x0 */
    int16_t prop_team; /* +0x2 */
    char is_friendly; /* +0x4 */
  } team_info; /* EBP-0x50, passed to ai_communication_event arg7 */
  int best_prop; /* EBP-0x48 */
  char acknowledge_flag; /* EBP-0x44, pushed as a dword by MSVC */
  float best_weight; /* EBP-0x40 */
  char *actor_defn; /* EBP-0x3c */
  int16_t awareness_slot; /* EBP-0x38 */
  int16_t new_awareness; /* EBP-0x34 (state 1 path) */
  float distance_squared; /* EBP-0x34 (state 2/3 paths) */
  float alert_probability; /* EBP-0x30 (phase 1) */
  char refresh_status; /* EBP-0x30 (phase 2) */
  float awareness_delta; /* EBP-0x2c */
  char *actor; /* EBP-0x28 / ESI in phase 1 */
  int new_state; /* EBP-0x24 */
  char orphan_expired; /* EBP-0x1f */
  char acknowledge_out; /* EBP-0x1e, out param of 0x33330 */
  char claimed_awareness; /* EBP-0x1d */
  int iter[2]; /* EBP-0x1c, prop iterator */
  int acknowledged_object; /* EBP-0x18, stored but never read */
  int16_t awareness_ticks; /* EBP-0x14 (loop head) */
  char *debug_awareness_cache; /* EBP-0x14 (state 0/1 path) */
  char become_acknowledged_result; /* EBP-0xd */
  char scratch_10; /* EBP-0xc  */
  char scratch_c; /* EBP-0x8  */
  char refresh_position; /* EBP-0x4  */
  char *prop; /* ESI in phase 2 */
  char *other_actor;
  char *parent_prop;
  char *encounter;
  int other_actor_handle;
  int new_prop_handle;
  int16_t alertness;
  int16_t remaining;
  int16_t prop_state;
  int16_t awareness_penalty;
  int16_t retire_threshold;
  uint16_t knowledge_type;
  char ramp_ready;
  char is_friendly;
  char in_event_range;
  char is_visible;
  float event_threshold;
  float delta_x;
  float delta_y;

  actor = (char *)datum_get(actor_data, actor_handle);
  actor_defn = (char *)tag_get(0x61637472, ((actor_t *)actor)->field_058);
  awareness_slot = 1;
  claimed_awareness = 0;
  best_prop = -1;
  best_weight = 3.4028235e+38f;

  if (((actor_t *)actor)->field_013 != 0)
    goto iterate_props;

  if (((actor_t *)actor)->field_04c != 0)
    actor_perception_refresh(actor_handle);
  actor_perception_refresh_danger_zone(actor_handle);

  alertness = ((actor_t *)actor)->danger_zone_danger_type;
  if (alertness < 1)
    goto iterate_props;

  if (((actor_t *)actor)->field_28a == 0 && *(int16_t *)(actor + 0x282) == 0) {
    if (((actor_t *)actor)->field_284 > 0 &&
        ((actor_t *)actor)->field_286 != 0) {
      if (((actor_t *)actor)->field_088 == -1 ||
          ((actor_t *)actor)->field_088 > 0x3b) {
        remaining = (int16_t)(((actor_t *)actor)->field_284 - 1);
        ramp_ready = (char)(remaining == 0);
        ((actor_t *)actor)->field_284 = remaining;
        goto ramp_gate;
      }
      ((actor_t *)actor)->field_284 = 0;
      goto ramp_run;
    }
  } else {
    ((actor_t *)actor)->danger_zone_noticed_danger = 1;
    ramp_ready = (char)(((actor_t *)actor)->field_284 > 0);
    ((actor_t *)actor)->field_284 = 0;
  ramp_gate:
    if (ramp_ready) {
    ramp_run:
      if (alertness == 1) {
      ramp_promote:
        ((actor_t *)actor)->danger_zone_noticed_danger = 1;
      } else if (alertness == 2) {
        alert_probability = *(float *)(actor_defn + 0x50);
      ramp_roll:
        if (*(float *)0x2533c0 < alert_probability) {
          if (random_math_real(
                (unsigned int *)get_global_random_seed_address()) <
              alert_probability)
            goto ramp_promote;
        }
      } else if (alertness == 3) {
        alert_probability = *(float *)(actor_defn + 0x54);
        goto ramp_roll;
      }

      if (((actor_t *)actor)->danger_zone_noticed_danger != 0) {
        if (((actor_t *)actor)->field_28a == 0) {
          if (*(int16_t *)(actor + 0x282) == 0 &&
              ((actor_t *)actor)->danger_zone_danger_type != 3 &&
              ((actor_t *)actor)->danger_zone_danger_type != 1) {
            if (random_math_real(
                  (unsigned int *)get_global_random_seed_address()) <
                *(float *)(actor_defn + 0x88))
              ((actor_t *)actor)->field_288 = 1;
            else
              ((actor_t *)actor)->field_288 = 0;
          } else {
            ((actor_t *)actor)->field_288 = 1;
          }
        } else {
          ((actor_t *)actor)->field_288 = 0;
        }
        actor_stimulus_noticed_danger_zone(
          actor_handle, *(uint16_t *)(actor + 0x280),
          *(uint16_t *)(actor + 0x282),
          ((actor_t *)actor)->danger_zone_object_index,
          (float *)(actor + 0x2b0));
      }
    }
  }

  if (((actor_t *)actor)->field_284 == 0) {
    if (((actor_t *)actor)->control_secondary_look_type == 0xc) {
      remaining = ((actor_t *)actor)->secondary_look_priority;
      if (remaining > 5)
        remaining = 5;
      ((actor_t *)actor)->secondary_look_priority = remaining;
    }
    if (((actor_t *)actor)->field_28a != 0) {
      ((actor_t *)actor)->danger_zone_noticed_danger = 1;
      ((actor_t *)actor)->field_288 = 0;
    }
  }

iterate_props:
  prop_iterator_new(iter, actor_handle);
  prop = (char *)prop_iterator_next(iter);
  while (prop != NULL) {
    new_state = -1;
    orphan_expired = 0;
    refresh_position = 0;
    refresh_status = 0;
    become_acknowledged_result = 0;
    acknowledge_out = 0;

    if (*(int16_t *)(prop + 0x68) > 0 &&
        (*(int16_t *)(prop + 0x68) = (int16_t)(*(int16_t *)(prop + 0x68) - 1),
         *(int16_t *)(prop + 0x68) == 0))
      *(uint16_t *)(prop + 0x66) = 0xffff;

    if (*(int16_t *)(prop + 0x6c) != -1 &&
        (*(int16_t *)(prop + 0x6c) = (int16_t)(*(int16_t *)(prop + 0x6c) + 1),
         *(int16_t *)(prop + 0x6c) > 0x2c))
      *(char *)(prop + 0x74) = 0;

    if (*(int16_t *)(prop + 0xb0) != -1 &&
        (*(int16_t *)(prop + 0xb0) = (int16_t)(*(int16_t *)(prop + 0xb0) + 1),
         *(int16_t *)(prop + 0xb0) > 0x3b)) {
      *(char *)(prop + 0xb8) = 0;
      *(int *)(prop + 0xb4) = -1;
    }

    if (*(char *)(prop + 0x127) == 0)
      *(int16_t *)(prop + 0x76) = 0;
    else
      *(int16_t *)(prop + 0x76) = (int16_t)(*(int16_t *)(prop + 0x76) + 1);

    if (*(int16_t *)(prop + 0x4c) > 0)
      *(int16_t *)(prop + 0x4c) = (int16_t)(*(int16_t *)(prop + 0x4c) - 1);

    if (*(int16_t *)(prop + 0x6a) > 0 && *(char *)(prop + 0x126) == 0)
      *(int16_t *)(prop + 0x6a) = (int16_t)(*(int16_t *)(prop + 0x6a) - 1);

    if (*(int16_t *)(prop + 0x9c) > 0 && *(int16_t *)(prop + 0x9c) < 0x7fff)
      *(int16_t *)(prop + 0x9c) = (int16_t)(*(int16_t *)(prop + 0x9c) + 1);

    if (*(int16_t *)(prop + 0xa8) > 0 &&
        (*(int16_t *)(prop + 0xa8) = (int16_t)(*(int16_t *)(prop + 0xa8) - 1),
         *(int16_t *)(prop + 0xa8) == 0)) {
      if (*(int16_t *)(prop + 0xa6) < 1) {
        display_assert("prop->unopposable_casualties_inflicted > 0",
                       "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0x13c, true);
        system_exit(-1);
      }
      *(int16_t *)(prop + 0xa6) = (int16_t)(*(int16_t *)(prop + 0xa6) - 1);
      if (*(int16_t *)(prop + 0xa6) > 0)
        *(uint16_t *)(prop + 0xa8) = 0x2ee;
    }

    if (*(int16_t *)(prop + 0x32) < 2)
      *(int16_t *)(prop + 0x78) = 0;
    else if (*(int16_t *)(prop + 0x78) < 0x7fff)
      *(int16_t *)(prop + 0x78) = (int16_t)(*(int16_t *)(prop + 0x78) + 1);

    if (((actor_t *)actor)->field_013 != 0) {
      *(char *)(prop + 0x63) = 0;
      *(int16_t *)(prop + 0x26) = 0;
      goto run_state_machine;
    }

    *(int16_t *)(prop + 0x26) = (int16_t)(*(int16_t *)(prop + 0x26) + 1);
    awareness_ticks = (int16_t) * (uint16_t *)(prop + 0x26);
    if (*(char *)(prop + 0x60) == 0)
      awareness_ticks = (int16_t)(awareness_ticks >> 3);
    if (*(char *)(prop + 0x121) > 2)
      awareness_ticks = (int16_t)(awareness_ticks >> 1);

    if (claimed_awareness == 0 &&
        awareness_ticks >= ((actor_t *)actor)->field_04e) {
      refresh_status = 1;
      refresh_position = 1;
      awareness_ticks = 0;
      *(int16_t *)(prop + 0x26) = 0;
      claimed_awareness = 1;
    }
    if (awareness_ticks > awareness_slot)
      awareness_slot = awareness_ticks;

    prop_state = *(int16_t *)(prop + 0x24);
    if (prop_state < 0 || prop_state > 1 || *(int *)(prop + 0xc) != -1) {
      if (*(char *)(actor + 6) == 0) {
        if (((actor_t *)actor)->target_target_prop_index == iter[0] ||
            ((actor_t *)actor)->field_054 == iter[0] ||
            ((actor_t *)actor)->field_3ac == iter[0] ||
            ((actor_t *)actor)->field_1d0 == iter[0] ||
            (((actor_t *)actor)->control_secondary_look_type != 0 &&
             ((actor_t *)actor)->control_secondary_look_direction_type == 1 &&
             ((actor_t *)actor)->control_secondary_look_direction_prop_index ==
               iter[0]) ||
            (((actor_t *)actor)->control_idle_major_active != 0 &&
             ((actor_t *)actor)->control_idle_major_direction_type == 1 &&
             ((actor_t *)actor)->control_idle_major_direction_prop_index ==
               iter[0]) ||
            (((actor_t *)actor)->control_idle_minor_active != 0 &&
             ((actor_t *)actor)->control_idle_minor_direction_type == 1 &&
             ((actor_t *)actor)->control_idle_minor_direction_prop_index ==
               iter[0]))
          *(char *)(prop + 0x63) = 1;
        else
          *(char *)(prop + 0x63) = 0;

        if (prop_state > 3 && prop_state < 6) {
          parent_prop =
            (char *)datum_get(*(data_t **)0x5ab23c, *(int *)(prop + 0xc));
          if (*(int *)(parent_prop + 0xc) != iter[0]) {
            display_assert("parent_prop->orphan_prop_index == iterator.index",
                           "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0x192,
                           true);
            system_exit(-1);
          }
          *(char *)(parent_prop + 0x63) = *(char *)(prop + 0x63);
        }
      } else {
        *(char *)(prop + 0x63) = 0;
      }
    }

    is_visible = refresh_position;
    if (*(char *)(prop + 0x63) != 0 &&
        (*(int16_t *)(prop + 0x24) < 0 || *(int16_t *)(prop + 0x24) > 1))
      is_visible = 1;

    if (refresh_status == 0) {
      if (is_visible == 0)
        goto run_state_machine;
    } else if (is_visible == 0) {
      display_assert("!refresh_status || refresh_position",
                     "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0x1a0, true);
      system_exit(-1);
    }
    prop_position_refresh(actor_handle, iter[0], position_data_a, 0,
                          refresh_status);
    if (refresh_status != 0)
      prop_status_refresh(actor_handle, iter[0], position_data_a);

  run_state_machine:
    switch (*(int16_t *)(prop + 0x24)) {
    case 0:
      if (*(int16_t *)(prop + 0x30) > 0) {
        new_state = 1;
        *(int *)(prop + 0x2c) = 0;
        if (*(char *)(prop + 0x12e) != 0 && *(char *)0x5aca61 != 0) {
          ai_debug_describe_actor(actor_handle, ((actor_t *)actor)->field_018,
                                  (char)0xff, debug_desc_b, 0x100);
          error(2, "%s: start to become aware", debug_desc_b);
        }
        goto becoming_aware;
      }
      break;

    case 1:
    becoming_aware:
      debug_awareness_cache =
        (char *)(*(int *)0x331f58 + (actor_handle & 0xffff) * 0x657c);
      if (*(int16_t *)(prop + 0x30) == 0) {
        *(int *)(prop + 0x2c) = 0;
        new_state = 0;
        if (*(char *)(prop + 0x12e) != 0 &&
            (*(uint16_t *)(debug_awareness_cache + 0x6578) = 0xffff,
             *(char *)0x5aca61 != 0)) {
          ai_debug_describe_actor(actor_handle, ((actor_t *)actor)->field_018,
                                  (char)0xff, debug_desc_d, 0x100);
          error(2, "%s: stop becoming aware", debug_desc_d);
        }
      } else {
        knowledge_type = actor_get_perception_knowledge(actor_handle, iter[0]);
        if ((int16_t)knowledge_type < 0 || (int16_t)knowledge_type > 3) {
          display_assert("(knowledge_type >= 0) && (knowledge_type < "
                         "NUMBER_OF_ACTOR_KNOWLEDGE_TYPES)",
                         "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0x1e9,
                         true);
          system_exit(-1);
        }
        if (*(int16_t *)(prop + 0x30) < 0 || *(int16_t *)(prop + 0x30) > 3) {
          display_assert("(prop->perception >= 0) && (prop->perception < "
                         "NUMBER_OF_ACTOR_PERCEPTION_TYPES)",
                         "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0x1ea,
                         true);
          system_exit(-1);
        }
        new_awareness =
          (int16_t) *
          (uint16_t *)(0x255f30 + ((int)*(int16_t *)(prop + 0x30) +
                                   (int)(int16_t)knowledge_type * 4) *
                                    2);
        switch ((int)new_awareness) {
        case 0:
          awareness_delta = 0.0f;
          break;
        case 1:
          awareness_delta = *(float *)(actor_defn + 0x74);
          break;
        case 2:
          awareness_delta = *(float *)(actor_defn + 0x70);
          break;
        case 3:
          awareness_delta = *(float *)(actor_defn + 0x6c);
          break;
        case 4:
          awareness_delta = 1.0f;
          break;
        default:
          display_assert("!\"unreachable\"",
                         "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0x204,
                         true);
          system_exit(-1);
        }

        if (*(char *)(prop + 0x12e) != 0 &&
            *(uint16_t *)(debug_awareness_cache + 0x6578) !=
              (uint16_t)new_awareness &&
            (*(uint16_t *)(debug_awareness_cache + 0x6578) =
               (uint16_t)new_awareness,
             *(char *)0x5aca61 != 0)) {
          awareness_names[0] = "never";
          awareness_names[1] = "noncombat";
          awareness_names[2] = "guard";
          awareness_names[3] = "combat";
          awareness_names[4] = "instant";
          perception_names[0] = "none";
          perception_names[1] = "partial";
          perception_names[2] = "full";
          perception_names[3] = "unmistakable";
          knowledge_names[0] = "noncombat";
          knowledge_names[1] = "guard";
          knowledge_names[2] = "searching";
          knowledge_names[3] = "definite";
          ai_debug_describe_actor(actor_handle, ((actor_t *)actor)->field_018,
                                  (char)0xff, debug_desc_c, 0x100);
          error(2, "%s: knowledge %s percep %s -> awareness %s", debug_desc_c,
                knowledge_names[(int16_t)knowledge_type],
                perception_names[*(int16_t *)(prop + 0x30)],
                awareness_names[new_awareness]);
          if (*(float *)0x2533c0 < awareness_delta &&
              awareness_delta < *(float *)0x2533c8) {
            error(2,
                  "  awareness delta: %.2f (current awareness %.2f -> time "
                  "%.2fsec)",
                  (double)awareness_delta, (double)*(float *)(prop + 0x2c),
                  (double)((*(float *)0x2533c8 - *(float *)(prop + 0x2c)) /
                           (awareness_delta * *(float *)0x253394)));
          }
        }

        *(float *)(prop + 0x2c) = awareness_delta + *(float *)(prop + 0x2c);
        if (*(float *)(prop + 0x2c) < *(float *)0x2533c8)
          goto check_new_state;
        new_state = 3;
        if (*(char *)(prop + 0x12e) != 0 &&
            (*(uint16_t *)(debug_awareness_cache + 0x6578) = 0xffff,
             *(char *)0x5aca61 != 0)) {
          ai_debug_describe_actor(actor_handle, ((actor_t *)actor)->field_018,
                                  (char)0xff, debug_desc_a, 0x100);
          error(2, "%s: become aware!", debug_desc_a);
        }
      }
      goto apply_new_state;

    case 2:
      if (*(int16_t *)(prop + 0x30) < 1) {
        if (*(int16_t *)(prop + 0x4c) != 0) {
          delta_x = *(float *)(prop + 0xbc) - *(float *)(prop + 0x80);
          delta_y = *(float *)(prop + 0xc0) - *(float *)(prop + 0x84);
          if (delta_y * delta_y + delta_x * delta_x <= *(float *)0x2533c8)
            break;
        }
        scratch_10 = *(char *)(prop + 0x127);
        refresh_position = *(char *)(prop + 0x60);
        scratch_c = *(char *)(prop + 0x12e);
        other_actor_handle = *(int *)(prop + 0x1c);
        distance_squared = *(float *)(prop + 0x11c) * *(float *)(prop + 0x11c);
        new_prop_handle = -1;
        datum_get(actor_data, actor_handle);
        if (other_actor_handle == -1)
          other_actor = NULL;
        else
          other_actor = (char *)datum_get(actor_data, other_actor_handle);

        if (refresh_position != 0 && scratch_10 == 0 &&
            (scratch_c != 0 ||
             ((other_actor == NULL || (*(char *)(other_actor + 8) != 0 &&
                                       *(char *)(other_actor + 0x13) == 0)) &&
              distance_squared <= *(float *)0x255fe0))) {
          prop_position_refresh(actor_handle, iter[0], position_data_b, 0, 0);
          actor_perception_find_prop_pathfinding_location(actor_handle,
                                                          iter[0]);
          new_prop_handle = prop_orphan_transition(actor_handle, iter[0]);
        }
        actor_switch_props(actor_handle, iter[0], new_prop_handle);
        new_state = 0;
      } else {
        new_state = 3;
      }

    apply_new_state:
      if ((int16_t)new_state == *(int16_t *)(prop + 0x24)) {
        display_assert("new_state!=prop->state",
                       "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0x2b6, true);
        system_exit(-1);
      }
      switch ((int16_t)new_state) {
      case 0:
      case 5:
        *(char *)(prop + 0xb8) = 0;
        *(int *)(prop + 0xb4) = -1;
        break;
      case 1:
        break;
      case 2:
        *(uint16_t *)(prop + 0x4c) =
          (uint16_t)(((*(int16_t *)(prop + 0x32) < 2) - 1 & 0x32) + 10);
        break;
      case 3:
        become_acknowledged_result = actor_perception_become_acknowledged(
          actor_handle, iter[0], (int)&acknowledge_out);
        /* Dead store in the original too ([EBP-0x18] is never read back). */
        acknowledged_object = *(int *)(prop + 8);
        (void)acknowledged_object;
        break;
      case 4:
        display_assert(NULL, "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0x2d1,
                       true);
        system_exit(-1);
      default:
        display_assert(NULL, "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0x2da,
                       true);
        system_exit(-1);
      }
      *(int16_t *)(prop + 0x24) = (int16_t)new_state;
      *(char *)(prop + 0xa4) =
        (char)actor_compute_prop_unopposable(actor_handle, iter[0]);
      *(float *)(prop + 0x50) =
        actor_compute_prop_target_weight(actor_handle, iter[0]);

    check_orphan_retire:
      if (orphan_expired == 0)
        break;
      if (*(int *)(prop + 0xc) == -1) {
        display_assert("prop->parent_prop_index != NONE",
                       "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0x2ea, true);
        system_exit(-1);
      }
      parent_prop =
        (char *)datum_get(*(data_t **)0x5ab23c, *(int *)(prop + 0xc));
      if (*(int *)(parent_prop + 0xc) != iter[0]) {
        display_assert("parent_prop->orphan_prop_index == iterator.index",
                       "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0x2ef, true);
        system_exit(-1);
      }
      *(int *)(parent_prop + 0xc) = -1;
      actor_switch_props(actor_handle, iter[0], -1);
      prop_delete(actor_handle, iter[0]);
      goto tally_prop;

    case 3:
      if (*(int16_t *)(prop + 0x30) == 0) {
        scratch_c = *(char *)(prop + 0x127);
        scratch_10 = *(char *)(prop + 0x12e);
        is_visible = *(char *)(prop + 0x60);
        other_actor_handle = *(int *)(prop + 0x1c);
        distance_squared = *(float *)(prop + 0x11c) * *(float *)(prop + 0x11c);
        datum_get(actor_data, actor_handle);
        if (other_actor_handle == -1)
          other_actor = NULL;
        else
          other_actor = (char *)datum_get(actor_data, other_actor_handle);

        if (is_visible == 0 || scratch_c != 0 ||
            (scratch_10 == 0 &&
             ((other_actor != NULL && (*(char *)(other_actor + 8) == 0 ||
                                       *(char *)(other_actor + 0x13) != 0)) ||
              *(float *)0x255fe0 < distance_squared))) {
          actor_switch_props(actor_handle, iter[0], -1);
          new_state = 0;
        } else {
          new_state = 2;
        }
        goto apply_new_state;
      }
      break;

    case 4:
    case 5:
      if (*(int16_t *)(prop + 0x24) == 4) {
        retire_threshold =
          (int16_t)((-(uint16_t)(((actor_t *)actor)->field_162 != 0) & 0xff) +
                    0x2d);
        if (*(int16_t *)(prop + 0x32) > 1 ||
            (((actor_t *)actor)->control_current_fire_target_type ==
               _actor_fire_target_prop &&
             ((actor_t *)actor)->control_current_fire_target_prop_index ==
               iter[0] &&
             game_time_get() % 3 == 0)) {
          *(int16_t *)(prop + 0x3c) = (int16_t)(*(int16_t *)(prop + 0x3c) + 1);
          if (*(int16_t *)(prop + 0x3c) >= retire_threshold)
            new_state = 5;
        }
      }
      if (iter[0] == ((actor_t *)actor)->field_3ac ||
          (((actor_t *)actor)->state_action == 4 &&
           ((actor_t *)actor)->field_0b8 == iter[0])) {
        awareness_penalty = 0;
      } else if (iter[0] == ((actor_t *)actor)->target_target_prop_index) {
        awareness_penalty = (int16_t)(*(char *)(prop + 0xbb) != 0);
      } else if (iter[0] == ((actor_t *)actor)->field_054) {
        awareness_penalty =
          (int16_t)((((((actor_t *)actor)->field_06e < 4) - 1) & 5) + 1);
      } else {
        awareness_penalty = 10;
      }
      *(int16_t *)(prop + 0x3a) =
        (int16_t)(*(int16_t *)(prop + 0x3a) - awareness_penalty);
      if (*(int16_t *)(prop + 0x3a) < 0)
        orphan_expired = 1;

    check_new_state:
      if ((int16_t)new_state != -1)
        goto apply_new_state;
      goto check_orphan_retire;
    }

    /* Post state-machine: event generation and prop tallies. */
    if (*(char *)(prop + 0x64) == 0 || *(int16_t *)(prop + 0x24) < 2 ||
        *(int16_t *)(prop + 0x24) > 3) {
      if (*(int16_t *)(prop + 0x24) > 3 && *(int16_t *)(prop + 0x24) < 6 &&
          *(float *)(prop + 0x11c) < best_weight) {
        best_prop = iter[0];
        best_weight = *(float *)(prop + 0x11c);
      }

    tally_prop:
      if (*(char *)(prop + 0x127) != 0)
        goto tally_dead_prop;
      prop_state = *(int16_t *)(prop + 0x24);
      if (*(char *)(prop + 0x60) == 0) {
        if (prop_state >= 2 && prop_state <= 3) {
          *(int16_t *)0x5ac2a4 = (int16_t)(*(int16_t *)0x5ac2a4 + 1);
        } else if (prop_state >= 4 && prop_state <= 5) {
          *(int16_t *)0x5ac32c = (int16_t)(*(int16_t *)0x5ac32c + 1);
        } else if (prop_state >= 0 && prop_state <= 1) {
          *(int16_t *)0x5ac3b4 = (int16_t)(*(int16_t *)0x5ac3b4 + 1);
        }
      } else if (prop_state >= 2 && prop_state <= 3) {
        *(int16_t *)0x5ac10c = (int16_t)(*(int16_t *)0x5ac10c + 1);
      } else if (prop_state >= 4 && prop_state <= 5) {
        *(int16_t *)0x5ac194 = (int16_t)(*(int16_t *)0x5ac194 + 1);
      } else if (prop_state >= 0 && prop_state <= 1) {
        *(int16_t *)0x5ac21c = (int16_t)(*(int16_t *)0x5ac21c + 1);
      }
    } else {
      if (*(char *)(prop + 0x129) != 0) {
        actor_stimulus_prop_just_killed(actor_handle, iter[0]);
        *(char *)(prop + 0x129) = 0;
      }
      if (*(char *)(prop + 0x12a) != 0 ||
          (become_acknowledged_result != 0 && *(int16_t *)(prop + 0x32) > 0)) {
        if (become_acknowledged_result == 0) {
        clear_acknowledge_flag:
          acknowledge_flag = 0;
        } else {
          acknowledge_flag = 1;
          if (acknowledge_out != 0)
            goto clear_acknowledge_flag;
        }
        actor_stimulus_prop_sighted(actor_handle, iter[0], acknowledge_flag);
        *(char *)(prop + 0x12a) = 0;
      }

      if (((actor_t *)actor)->field_377 == 0 && *(char *)(prop + 0x60) == 0 &&
          *(char *)(prop + 0x12e) != 0 && *(int16_t *)(prop + 0x32) > 1 &&
          *(char *)(prop + 0x122) < 3 &&
          *(float *)(prop + 0x11c) < *(float *)0x2548f4) {
        ((actor_t *)actor)->field_377 = 1;
        ai_communication_event(0x19, ((actor_t *)actor)->field_018,
                               *(int *)(prop + 0x18), 2, -1, -1, 0);
        actor_stimulus_prop_sighted(actor_handle, iter[0], 0);
      }

      if (((actor_t *)actor)->field_018 != -1 && *(char *)(prop + 0x127) == 0 &&
          *(char *)(prop + 0x61) != 0 && *(char *)(prop + 0x62) != 0) {
        is_friendly = (char)game_allegiance_get_team_is_friendly(
          ((actor_t *)actor)->field_03e, *(int16_t *)(prop + 0x12));
        if (is_friendly != 0)
          event_threshold = *(float *)0x254cc0;
        else if (*(char *)(prop + 0x122) < 3)
          event_threshold = *(float *)0x253f34;
        else
          event_threshold = *(float *)0x254644;
        in_event_range = (char)(event_threshold > *(float *)(prop + 0x11c));

        if ((is_friendly != 0 && *(char *)(prop + 0x74) != 0) ||
            in_event_range != 0) {
          team_info.prop_team = *(int16_t *)(prop + 0x12);
          team_info.actor_team = ((actor_t *)actor)->field_03e;
          team_info.is_friendly = is_friendly;
          ai_communication_event(8, ((actor_t *)actor)->field_018,
                                 *(int *)(prop + 0x18),
                                 (is_friendly != 0) * 2 + 2, -1, 1,
                                 (ai_information_data_t *)&team_info);
        }
      }

      if (((actor_t *)actor)->field_06a < 3) {
        if (*(char *)(prop + 0x127) != 0) {
          if (*(char *)(prop + 0x60) != 0)
            goto notify_departed;
          FUN_00036a90(actor_handle, iter[0]);
          goto after_notify;
        }
        if (*(char *)(prop + 0x60) != 0) {
        notify_departed:
          actor_stimulus_enter_combat_perceived_prop(actor_handle, iter[0]);
          goto after_notify;
        }
      } else {
      after_notify:
        if (*(char *)(prop + 0x60) != 0)
          goto tally_prop;
      }

      if (*(char *)(prop + 0x127) == 0) {
        if (*(char *)(prop + 0x12e) == 0) {
          if (((actor_t *)actor)->target_target_prop_index == -1 ||
              (((actor_t *)actor)->field_278 != -1 &&
               ((actor_t *)actor)->field_278 < 0xb4)) {
            if (*(int *)(actor + 0x34) != -1) {
              encounter =
                (char *)datum_get(*(data_t **)0x5ab270, *(int *)(actor + 0x34));
              if (*(int *)(encounter + 0x50) == -1 ||
                  (*(int *)(encounter + 0x50) > 0xb3 &&
                   *(char *)(encounter + 0x44) != 0))
                goto emit_contact_event;
            }
          } else {
          emit_contact_event:
            if (((actor_t *)actor)->field_018 != -1) {
              if (((actor_t *)actor)->field_06a < 3) {
                if (*(char *)(prop + 0x12c) != 0) {
                  ai_communication_event(0xf, *(int *)(prop + 0x18),
                                         ((actor_t *)actor)->field_018, 2, -1,
                                         2, 0);
                }
              } else if (actor_in_combat(actor_handle) != 0 &&
                         actor_is_fighting(actor_handle) == 0 &&
                         *(char *)(prop + 0x12b) != 0 &&
                         *(int16_t *)(prop + 0x32) > 1) {
                ai_communication_event(0xf, ((actor_t *)actor)->field_018,
                                       *(int *)(prop + 0x18), 2, -1, 2, 0);
              }
            }
          }
        }
        goto tally_prop;
      }

    tally_dead_prop:
      prop_state = *(int16_t *)(prop + 0x24);
      if (prop_state >= 2 && prop_state <= 3) {
        *(int16_t *)0x5abf74 = (int16_t)(*(int16_t *)0x5abf74 + 1);
      } else if (prop_state >= 4 && prop_state <= 5) {
        *(int16_t *)0x5abffc = (int16_t)(*(int16_t *)0x5abffc + 1);
      } else if (prop_state >= 0 && prop_state <= 1) {
        *(int16_t *)0x5ac084 = (int16_t)(*(int16_t *)0x5ac084 + 1);
      }
    }

    prop = (char *)prop_iterator_next(iter);
  }

  if (((actor_t *)actor)->target_target_prop_index != -1) {
    parent_prop = (char *)datum_get(
      *(data_t **)0x5ab23c, ((actor_t *)actor)->target_target_prop_index);
    if (*(int16_t *)(parent_prop + 0x24) > 3 &&
        *(int16_t *)(parent_prop + 0x24) < 6)
      best_prop = -1;
  }
  if (((actor_t *)actor)->target_target_type > 5)
    ((actor_t *)actor)->field_274 = 1;
  if (((actor_t *)actor)->target_target_type > 9) {
    ((actor_t *)actor)->field_278 = 0;
    ((actor_t *)actor)->field_04e = awareness_slot;
    ((actor_t *)actor)->field_054 = best_prop;
    return;
  }
  if (((actor_t *)actor)->field_1c8 != 0) {
    ((actor_t *)actor)->field_278 = -1;
    ((actor_t *)actor)->field_04e = awareness_slot;
    ((actor_t *)actor)->field_054 = best_prop;
    return;
  }
  if (((actor_t *)actor)->field_278 != -1) {
    ((actor_t *)actor)->field_278 = ((actor_t *)actor)->field_278 + 1;
    ((actor_t *)actor)->field_04e = awareness_slot;
    ((actor_t *)actor)->field_054 = best_prop;
    return;
  }
  ((actor_t *)actor)->field_04e = awareness_slot;
  ((actor_t *)actor)->field_054 = best_prop;
}
