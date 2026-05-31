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

  if (*(short *)(actor + 0x15e) == 4 && *(char *)(actor + 0x504) != '\0') {
    actor_move_to_point(actor_handle, (float *)(actor + 0x12c),
                        *(int *)(actor + 0x164), -1);
    return;
  }

  *(short *)(actor + 0x3b8) = -1;

  if (*(short *)(actor + 0x46c) != 1) {
    *(short *)(actor + 0x400) = 1;
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

  if (*(short *)(actor + 0x15e) == 4) {
    if (*(short *)(actor + 0x3b8) == -1) {
      FUN_0002f1a0(actor_handle);
      return;
    }
    actor_move_to_firing_position(actor_handle, *(short *)(actor + 0x3b8), 0);
    return;
  }

  if (*(short *)(actor + 0x46c) != 1) {
    *(short *)(actor + 0x400) = 1;
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

  FUN_00036f20(actor_handle, prop_handle, param_3, param_4);
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
        (*(char *)(prop + 0x60) == 0 &&
         (*(char *)(prop + 0x127) == 0 || *(short *)(actor + 0x6a) >= 3))) {
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
  if (*(short *)(actor + 0x6e) >= 2)
    return 2;
  return (uint16_t)(*(short *)(actor + 0x6a) >= 3);
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
  if (f2 < f1)
    return 1;
  return 0;
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
        unit_handle, (vector3_t *)(prop + 0xf0));
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
__declspec(noinline) void actor_perception_forget_recent_damage(int actor_handle)
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
        (*(int *)(actor + 0x270) == prop_handle ||
         *(char *)(actor + 0x1ed) == 0)) {
      result = 1;
    } else if ((*(char *)(prop + 0x135) != 0 || *(char *)(prop + 0x136) != 0) &&
               *(char *)(actor + 0x161) == 0 && *(char *)(actor + 0x202) == 0) {
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

  if (type > 1 && type < 4 && result == 0 && *(short *)(actor + 0x3a8) > 0 &&
      *(int *)(actor + 0x3ac) == prop_handle) {
    *(uint16_t *)(actor + 0x3a8) = 0;
    *(int *)(actor + 0x3ac) = -1;
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

  actr_tag = (char *)tag_get(0x61637472, *(int *)(actor + 0x58));
  actv_tag = (char *)tag_get(0x61637476, *(int *)(actor + 0x5c));

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
      if (*(char *)(actor + 0x378) != 0) {
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
      } else if (*(char *)(prop + 0x118) != *(char *)(actor + 0x15d)) {
        vision_level = 1;
      } else if (*(float *)(prop + 0x11c) < actv_threshold) {
        vision_level = 3;
      } else {
        vision_level = 2;
      }
    } else {
      /* Actor has a weapon in hand */
      char *weapon_tag = FUN_000210f0(actor_handle);
      char *actv_tag2 =
        actor_combat_get_firing_variant_definition(actor_handle);

      if (weapon_tag == 0 ||
          *(float *)(prop + 0x11c) >= *(float *)(weapon_tag + 0x40c)) {
        /* prop distance >= weapon range (or no weapon tag) */
        if (*(char *)(prop + 0x118) != *(char *)(actor + 0x15d)) {
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
  if (*(int *)(actor + 0x270) == -1) {
    if (*(char *)(prop + 0x12e) != 0 ||
        clump_item_handle == *(int *)(actor + 0x54)) {
      local_c = 3.0f;
    }
  } else if (clump_item_handle == *(int *)(actor + 0x270) &&
             *(short *)(actor + 0x6e) > 2) {
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