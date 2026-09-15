/* actor_perception.c — AI actor perception and prop evaluation.
 *
 * Corresponds to actor_perception.obj.
 * Assertion path: c:\halo\SOURCE\ai\actor_perception.c
 */

#include "../../common.h"

/* FUN_0002f1a0: set actor movement destination or refresh path.
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
void FUN_0002f1a0(int actor_handle)
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

/* FUN_0002f230 (0x2f230): refresh actor path or dispatch to move/firing
 * position.
 *
 * If actor is NOT in move-to-point mode (field_15e != 4):
 *   copies 6-dword block from +0x400 to +0x46c (if not already done),
 *   then calls actor_path_refresh(actor_handle, 1, NULL).
 * If in move-to-point mode and field_3b8 != -1:
 *   calls actor_move_to_firing_position.
 * Otherwise falls through to FUN_0002f1a0. */
void FUN_0002f230(int actor_handle)
{
  char *actor;

  actor = (char *)datum_get(actor_data, actor_handle);

  if (((actor_t *)actor)->field_15e == 4) {
    if (((actor_t *)actor)->firing_positions_current_position_index == -1) {
      FUN_0002f1a0(actor_handle);
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

/* FUN_0002f380 (0x2f380)
 * Returns the engagement level (0-3) for a prop relative to actor.
 * 3 = actively targeting/seen; 2/3 = based on orphan state; 0/1/2 = based
 * on actor awareness level when no prop or no orphan.
 */
uint16_t FUN_0002f380(int actor_handle, int prop_handle)
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
    if ((*(short *)(prop + 0x24) >= 2 && *(short *)(prop + 0x24) <= 3) ||
        *(short *)(prop + 0x66) == 1 || *(short *)(prop + 0x66) == 2 ||
        (*(char *)(prop + 0x60) == 0 && (*(char *)(prop + 0x127) == 0 ||
                                         ((actor_t *)actor)->field_06a >= 3))) {
      return 3;
    }
    if (*(int *)(prop + 0xc) != -1) {
      orphan = (char *)datum_get(*(data_t **)0x5ab23c, *(int *)(prop + 0xc));
      r = (uint16_t)((*(char *)(orphan + 0xb8) != 0) + 2);
      if (r != 0xffff) {
        return r;
      }
    }
  }
  if (((actor_t *)actor)->field_06e >= 2)
    return 2;
  return (uint16_t)(((actor_t *)actor)->field_06a >= 3);
}

/* FUN_0002f5b0 (0x2f5b0)
 * Compare two prop-like structs by their float[2] field (offset +8).
 * Returns -1, 0, or 1 (strcmp-style).
 */
int FUN_0002f5b0(int param_1, int param_2)
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

/* FUN_0002f5f0 (0x2f5f0)
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
bool FUN_0002f5f0(int actor_handle /* @<eax> */, int object_handle /* @<edi> */,
                  float param_3, float param_4, char param_5, char param_6)
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
  int unit_handle;

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
    unit_handle = *(int *)(prop + 0x18);
    if (object_try_and_get_and_verify_type(unit_handle, 1) != NULL) {
      *(int *)(prop + 0xec) = biped_find_pathfinding_surface_index(
        *(int *)(prop + 0x18), (vector3_t *)(prop + 0xf0));
    }
  }
}

/* actor_perception_find_killer_prop_index (0x2f9b0)
 * Find the highest-scoring active damaging prop visible to the unit that owns
 * the given prop. Similar to actor_get_best_damaging_prop but uses the prop's
 * owning unit as the source of weapon slots.
 * flag: when non-zero, require prop visibility; when 0, accept any.
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
  int count;
  short prop_type;

  prop_rec = (char *)datum_get(*(data_t **)0x5ab23c, prop_handle);
  unit = (char *)object_get_and_verify_type(*(int *)(prop_rec + 0x18), 3);
  best_handle = -1;
  best_score = 0;
  slot = (int *)(unit + 0x3e8);
  count = 4;
  do {
    score = slot[-2];
    responsible = ai_get_responsible_unit((unsigned int)*slot, 1);
    if (responsible != -1) {
      cand_handle = prop_get_active_by_unit_index(actor_handle, responsible);
      if (cand_handle != -1) {
        cand_prop = (char *)datum_get(*(data_t **)0x5ab23c, cand_handle);
        prop_type = *(short *)(cand_prop + 0x24);
        if (prop_type >= 2 && prop_type <= 3) {
          if (*(char *)(cand_prop + 0x60) != '\0' || flag == '\0') {
            if (best_score < score) {
              best_handle = cand_handle;
              best_score = score;
            }
          }
        }
      }
    }
    slot += 4;
    count--;
  } while (count != 0);
  return best_handle;
}

/* actor_get_best_damaging_prop (0x2fa70)
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
int actor_get_best_damaging_prop(int actor_handle, char prefer_visible)
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
  int iter;

  unit_handle = *(int *)((char *)datum_get(actor_data, actor_handle) + 0x18);
  damaging_prop_index = -1;
  if (unit_handle != -1) {
    unit = (char *)object_get_and_verify_type(unit_handle, 3);
    best_score = 0;
    slot = (unsigned int *)(unit + 0x3e0);
    iter = 4;
    do {
      unit_result = ai_get_responsible_unit(slot[2], 1);
      if (unit_result != -1) {
        prop_handle = prop_get_active_by_unit_index(actor_handle, unit_result);
        if (prop_handle != -1) {
          prop_rec = (char *)datum_get(*(data_t **)0x5ab23c, prop_handle);
          prop_type = *(short *)(prop_rec + 0x24);
          if (prop_type >= 2 && prop_type <= 3) {
            if (*(char *)(prop_rec + 0x60) != '\0' || prefer_visible == '\0') {
              if (*slot > best_score) {
                best_score = *slot;
                damaging_prop_index = prop_handle;
              }
            }
          }
        }
      }
      slot += 4;
      iter--;
    } while (iter != 0);

    if (damaging_prop_index == 0) {
      display_assert("damaging_prop_index != 0x00000000",
                     "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0xe8e, 1);
      system_exit(-1);
    }
    return damaging_prop_index;
  }
  return damaging_prop_index;
}

/* actor_perception_forget_recent_damage (0x2fb70) — Clear the recent-damage
 * tracking for all props visible to this actor. Resets field +0x74 to 0 and
 * field +0x6c to -1 for each prop in the iterator. */
__declspec(noinline) void
actor_perception_forget_recent_damage(int actor_handle)
{
  int iter[2];
  char *prop;

  FUN_00064540(iter, actor_handle);
  prop = (char *)FUN_00064570(iter);
  while (prop != NULL) {
    *(char *)(prop + 0x74) = 0;
    *(int16_t *)(prop + 0x6c) = -1;
    prop = (char *)FUN_00064570(iter);
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
  FUN_00064540(iter, actor_handle);
  prop = (char *)FUN_00064570(iter);
  while (prop != NULL) {
    *(int16_t *)(prop + 0xaa) = 0;
    *(int16_t *)(prop + 0xae) = 0;
    *(int16_t *)(prop + 0xac) = 0;
    prop = (char *)FUN_00064570(iter);
  }
}

/* actor_get_perception_knowledge (0x2fc20)
 * Evaluate whether an actor should engage a prop. Checks prop type,
 * visibility flags, and actor state to determine engagement eligibility.
 * Side effects: clears prop tracking fields when engagement drops,
 * and clears actor pursuit fields when target is lost. */
bool actor_get_perception_knowledge(int actor_handle, int prop_handle)
{
  char *actor;
  char *prop;
  short type;
  char result;

  actor = (char *)datum_get(actor_data, actor_handle);
  prop = (char *)datum_get(*(data_t **)0x5ab23c, prop_handle);
  type = *(short *)(prop + 0x24);
  result = 0;

  if (type > 1 && type < 4 && *(char *)(prop + 0x60) != 0 &&
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

  if (type > 1 && type < 4 && result == 0 &&
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
  char *prop;
  char *actr_tag;
  char *actv_tag;
  short vision_level; /* EDI in the binary */
  short awareness; /* EAX in the binary */
  short bonus_flag; /* [EBP-0x14], 0 or 1 */
  short extra_flag; /* [EBP-0x10], 0 or 2 */
  float local_c; /* [EBP-0xc], bonus addend (0.0f or 3.0f) */
  float actv_threshold;
  int sum;

  actor = (char *)datum_get(actor_data, actor_handle);
  prop = (char *)datum_get(*(data_t **)0x5ab23c, clump_item_handle);

  /* Early-exit conditions: return 0.0f */
  if (*(char *)(prop + 0x133) != 0)
    return 0.0f;
  if (*(char *)(prop + 0x60) == 0)
    return 0.0f;
  if (*(short *)(prop + 0x24) >= 0 && *(short *)(prop + 0x24) <= 1)
    return 0.0f;
  if (*(char *)(prop + 0x127) != 0 && *(short *)(prop + 0x76) >= 0x96)
    return 0.0f;
  if (*(short *)(prop + 0x10) == 0xf)
    return 0.0f;

  actr_tag = (char *)tag_get(0x61637472, ((actor_t *)actor)->field_058);
  actv_tag = (char *)tag_get(0x61637476, ((actor_t *)actor)->field_05c);

  bonus_flag = 0;
  extra_flag = 0;
  local_c = 0.0f;

  /* Compute vision_level (cVar4 / EDI) */
  if (*(char *)(actor + 6) != 0) {
    vision_level = 0;
  } else if (*(short *)(prop + 0x9c) > 0) {
    vision_level = 0;
  } else {
    if (actor_has_ranged_weapon(actor_handle) == 0) {
      /* Actor does not have a weapon in hand */
      if (((actor_t *)actor)->field_378 != 0) {
        actv_threshold = *(float *)(actv_tag + 0x160);
      } else {
        actv_threshold = *(float *)(actv_tag + 0x170);
      }
      if (*(float *)(prop + 0x11c) < *(float *)0x253f40) {
        /* prop distance < 2.0f */
        vision_level = 5;
        if (*(short *)(prop + 0x24) != 5)
          goto done_vision;
      }
      /* prop distance >= 2.0f (or prop type == 5) */
      if (*(int *)(prop + 0x110) != -1) {
        vision_level = 0;
      } else if (*(char *)(prop + 0x130) != 0 &&
                 *(float *)(actr_tag + 0x38c) == 0.0f) {
        vision_level = 0;
      } else if (*(char *)(prop + 0x118) != ((actor_t *)actor)->field_15d) {
        vision_level = 1;
      } else if (*(float *)(prop + 0x11c) < actv_threshold) {
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
          *(float *)(prop + 0x11c) >= *(float *)(weapon_tag + 0x40c)) {
        /* prop distance >= weapon range (or no weapon tag) */
        if (*(char *)(prop + 0x118) != ((actor_t *)actor)->field_15d) {
          vision_level = 2;
        } else if (*(float *)(prop + 0x11c) < *(float *)0x253f40) {
          /* prop distance < 2.0f */
          vision_level = 5;
          if (*(short *)(prop + 0x24) != 5)
            goto done_vision;
        } else if (*(float *)(prop + 0x11c) < *(float *)(actv_tag2 + 0xa0)) {
          vision_level = 3;
        } else {
          vision_level = 2;
          if (*(float *)(prop + 0x11c) >= *(float *)(actv_tag2 + 0x74)) {
            vision_level = 1;
          }
        }
      } else {
        vision_level = 2;
      }
    }
  }
done_vision:

  /* Compute awareness (cVar5 / EAX) */
  if (*(char *)(prop + 0x127) != 0) {
    awareness = 1;
  } else if (*(char *)(actor + 6) == 0 && *(char *)(prop + 0x74) != 0 &&
             *(short *)(prop + 0x9c) == 0) {
    awareness = 6;
  } else {
    short prop_type = *(short *)(prop + 0x24);
    if (prop_type >= 2 && prop_type <= 3) {
      if (*(char *)(actor + 6) != 0) {
        awareness = 4;
      } else if (*(short *)(prop + 0x9c) > 0) {
        awareness = 3;
      } else if (*(short *)(prop + 0x38) != 0 && *(short *)(prop + 0x38) != 1) {
        awareness = 3;
      } else if (*(char *)(prop + 0x12f) != 0 && *(char *)(prop + 0x122) <= 1) {
        awareness = 5;
      } else {
        awareness = 4;
      }
    } else {
      if (prop_type < 4 || prop_type > 5) {
        assert_halt_msg(0, "prop_orphaned(prop)");
      }
      if (*(char *)(prop + 0xb8) != 0) {
        awareness = 3;
      } else {
        awareness = (*(short *)(prop + 0x24) == 4) + 1;
      }
    }
  }

  /* Bonus computations */
  if (((actor_t *)actor)->target_target_prop_index == -1) {
    if (*(char *)(prop + 0x12e) != 0 ||
        clump_item_handle == ((actor_t *)actor)->field_054) {
      local_c = 3.0f;
    }
  } else if (clump_item_handle ==
               ((actor_t *)actor)->target_target_prop_index &&
             ((actor_t *)actor)->field_06e > 2) {
    bonus_flag = 1;
  }

  if (*(char *)(prop + 0x134) != 0) {
    extra_flag = 2;
  }

  /* Final score computation:
   * score = (int)(bonus_flag + extra_flag + vision_level + awareness) * 10.0f
   *       + 5.0f / (prop->field_11c * 0.1f + 1.0f)
   *       + local_c
   */
  sum = (int)extra_flag + (int)bonus_flag + (int)vision_level + (int)awareness;
  return (float)sum * 10.0f + 5.0f / (*(float *)(prop + 0x11c) * 0.1f + 1.0f) +
         local_c;
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
  char *prop;
  char *object;
  short status;

  actor = (actor_t *)datum_get(actor_data, actor_handle);
  if (actor->target_target_prop_index == -1) {
    actor->target_target_type = 0;
    actor->field_26c = -1;
    actor->field_27c = 0;
    return;
  }

  prop = (char *)datum_get(prop_data, actor->target_target_prop_index);
  object = (char *)object_get_and_verify_type(*(int *)(prop + 0x18), 3);
  if (*(char *)(prop + 0x60) == 0) {
    display_assert("target_prop->enemy",
                   "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0x10c3, true);
    system_exit(-1);
  }

  switch (*(short *)(prop + 0x24)) {
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
    if (*(char *)(prop + 0x127) != 0) {
      status = 2;
    } else if (*(char *)(prop + 0x74) != 0) {
      status = 11;
    } else if (*(short *)(prop + 0x32) >= 2) {
      status = 10;
    } else if (*(short *)(prop + 0x38) != 0 && *(short *)(prop + 0x38) != 1) {
      status = 7;
    } else if (*(char *)(prop + 0x122) > 2 ||
               *(float *)(prop + 0x11c) >= *(float *)0x254640) {
      status = 8;
    } else {
      status = 9;
    }
    break;
  case 4:
    status = (short)(5 + (*(char *)(prop + 0xb8) != 0));
    break;
  case 5:
    if (*(char *)(prop + 0x127) != 0) {
      status = 2;
    } else {
      status = (short)(4 - (*(char *)(prop + 0xbb) != 0));
    }
    break;
  default:
    display_assert((const char *)0, "c:\\halo\\SOURCE\\ai\\actor_perception.c",
                   0x110a, true);
    system_exit(-1);
    status = 0;
    break;
  }

  actor->target_target_type = status;
  if (*(short *)(prop + 0x24) >= 2 && *(short *)(prop + 0x24) <= 3) {
    actor->field_27c = (char)(*(char *)(prop + 0x127) == 0);
    if (*(short *)(prop + 0x32) > 0) {
      actor->field_26c = *(int *)(prop + 0x8c);
      return;
    }
  } else {
    actor->field_27c = (char)(~(*(unsigned char *)(object + 0xb6) >> 2) & 1);
  }
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
  if (weight <= *(float *)0x2533c0) {
    return false;
  }

  if (*(char *)(new_prop + 0x60) == 0) {
    display_assert("new_prop->enemy",
                   "c:\\halo\\SOURCE\\ai\\actor_perception.c", 0x124d, true);
    system_exit(-1);
  }

  if (old_prop != (char *)0 &&
      *(float *)(new_prop + 0x50) < *(float *)(old_prop + 0x50)) {
    return false;
  }

  actor->target_target_type = 0;
  actor->target_target_prop_index = target;
  actor->field_26c = -1;
  actor_situation_update_target_status(actor_handle);
  actor_situation_combat_status_update(actor_handle);
  return true;
}

/* FUN_00030e60 (0x30e60): find-or-append a 0x1c-byte record in a caller-owned
 * array, keyed by the dword at record+0x8.
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
short FUN_00030e60(void *records /* @<eax> */, int key /* @<edi> */,
                   int param_1, short *p_count, short max_count)
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
    (char)actor_get_perception_knowledge(actor_handle, leader_handle);
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

/* actor_perception_become_acknowledged (0x33330): promote a prop to the
 * "acknowledged" state (prop+0x24 == 3).
 *
 * Does nothing when the prop is already in state 2 or 3.  Otherwise it asks
 * actor_expected_acknowledgement whether the acknowledgement was expected,
 * and — when the prop still has a parent prop (prop+0xc != NONE) — folds the
 * parent's target weight block (+0x50..+0x5c) and its acknowledgement
 * bookkeeping (+0x9c, +0xa0, +0xa4, +0xa6, +0xa8) into this prop, retires the
 * parent link through actor_switch_props/prop_iterator_next, and clears prop+0xc.
 *
 * Returns 1 when the promotion ran, 0 when the prop was already in state 2/3.
 * out_acknowledged (optional) receives the actor_expected_acknowledgement
 * result, or 0 on the skipped path.
 *
 * ADD ESP,0x1c at 0x33409 coalesces three cdecl cleanups: datum_get (8) +
 * actor_switch_props (12) + prop_iterator_next (8) = 28.  A cleanup=7 ARG_COUNT
 * hazard on prop_iterator_next is that coalescing, not a real arg mismatch.
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
      prop_iterator_next(actor_handle, *(int *)(prop + 0xc));
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
    ((actor_t *)actor)->field_287 = 1;
    ramp_ready = (char)(((actor_t *)actor)->field_284 > 0);
    ((actor_t *)actor)->field_284 = 0;
  ramp_gate:
    if (ramp_ready) {
    ramp_run:
      if (alertness == 1) {
      ramp_promote:
        ((actor_t *)actor)->field_287 = 1;
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

      if (((actor_t *)actor)->field_287 != 0) {
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
        actor_stimulus_noticed_danger_zone(actor_handle, *(uint16_t *)(actor + 0x280),
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
      ((actor_t *)actor)->field_287 = 1;
      ((actor_t *)actor)->field_288 = 0;
    }
  }

iterate_props:
  FUN_00064540(iter, actor_handle);
  prop = (char *)FUN_00064570(iter);
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
        knowledge_type = FUN_0002f380(actor_handle, iter[0]);
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
        (char)actor_get_perception_knowledge(actor_handle, iter[0]);
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
      prop_iterator_next(actor_handle, iter[0]);
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
          ai_communication_event(
            8, ((actor_t *)actor)->field_018, *(int *)(prop + 0x18),
            (is_friendly != 0) * 2 + 2, -1, 1, (int)&team_info);
        }
      }

      if (((actor_t *)actor)->field_06a < 3) {
        if (*(char *)(prop + 0x127) != 0) {
          if (*(char *)(prop + 0x60) != 0)
            goto notify_departed;
          actor_stimulus_enter_combat_found_body(actor_handle, iter[0]);
          goto after_notify;
        }
        if (*(char *)(prop + 0x60) != 0) {
        notify_departed:
          actor_stimulus_enter_combat_perceived_enemy(actor_handle, iter[0]);
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

    prop = (char *)FUN_00064570(iter);
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
