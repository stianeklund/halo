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
  unsigned int *src;
  unsigned int *dst;
  int k;

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
    src = (unsigned int *)(actor + 0x400);
    dst = (unsigned int *)(actor + 0x46c);
    for (k = 6; k != 0; k--) {
      *dst++ = *src++;
    }
  }
  actor_path_refresh(actor_handle, 1, NULL);
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