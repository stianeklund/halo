/* units.c — unit lifecycle and query helpers.
 *
 * Corresponds to units.obj. Functions sorted by XBE address.
 */

#include "../../common.h"

/* unit_delete (0x1a7fc0)
 *
 * Marks a unit object for deletion by setting bit 5 (0x20) of the
 * object flags byte at object_data_t.unk_183 (offset 0xB7). The
 * actual object destruction happens later during the object system's
 * garbage-collection pass. Resolves handle via
 * object_get_and_verify_type with type mask 3 (biped | vehicle).
 */
void unit_delete(int datum_handle)
{
  object_data_t *obj;

  obj = (object_data_t *)object_get_and_verify_type(datum_handle, 3);
  obj->unk_183 |= 0x20;
}

/* units_update (0x1a7ff0)
 *
 * Per-tick global update for the units subsystem. Reads the units globals
 * pointer at 0x4e4cf8 (a small {int16_t, int16_t, uint8_t} struct) and
 * rotates: copies the "max ticks this frame" (offset +2) into "current
 * ticks" (offset +0), then zeroes the max-ticks field (+2) and the
 * ready-flag byte (+4). Called once per game tick from the main update loop.
 */
void units_update(void)
{
  int16_t *p = *(int16_t **)0x4e4cf8;

  p[0] = p[1];
  p[1] = 0;
  *(uint8_t *)&p[2] = 0;
}

/* unit_find_nearby_seat (0x1a8ce0)
 *
 * Searches the child object chain of target_unit for a unit that occupies the
 * given seat_index, or that belongs to a friendly team (via game_allegiance).
 *
 * Walks the linked list starting at target_unit's first_child_object (offset
 * 0xC8). For each child that is a biped or vehicle (type 0 or 1):
 *   - If the child unit's seat tag index (offset 0x2A0) matches seat_index,
 *     records the child's handle in *out_unit_handle.
 *   - Otherwise, if the searching unit has an active rider/owner (offset 0x1C8
 *     != NONE) and the child's team is friendly with the searching unit's team,
 *     clears the "empty" flag but does not update the output handle.
 *
 * Returns true only if unit_handle != target_unit_handle AND no matching/
 * friendly child was found. If out_unit_handle is non-NULL, writes the handle
 * of the last seat-matching child found (or NONE if none matched).
 */
bool unit_find_nearby_seat(int unit_handle, int target_unit_handle,
                           int16_t seat_index, int *out_unit_handle)
{
  unit_data_t *unit_data;
  object_data_t *target_obj;
  object_data_t *child_obj;
  unit_data_t *child_unit;
  int child_handle;
  int result_handle;
  bool not_found;

  unit_data = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);
  target_obj =
    (object_data_t *)object_get_and_verify_type(target_unit_handle, 3);

  not_found = (unit_handle != target_unit_handle);
  result_handle = -1;

  child_handle = target_obj->unk_200.value;
  while (child_handle != -1) {
    child_obj = (object_data_t *)object_get_and_verify_type(child_handle, -1);

    /* check if child is biped (type 0) or vehicle (type 1) */
    if ((1 << (*(uint8_t *)&child_obj->type & 0x1f)) & 3) {
      child_unit = (unit_data_t *)object_get_and_verify_type(child_handle, 3);

      if (child_unit->unk_672 == seat_index) {
        /* child occupies the requested seat */
        result_handle = child_handle;
        not_found = false;
      } else if (unit_data->unk_456.value != -1 &&
                 game_allegiance_get_team_is_friendly(
                   (int16_t)unit_data->object.unk_104,
                   (uint16_t)child_obj->unk_104)) {
        /* child is on a friendly team; seat is occupied/blocked */
        not_found = false;
      }
    }

    child_handle = child_obj->next_object_index.value;
  }

  if (out_unit_handle != NULL) {
    *out_unit_handle = result_handle;
  }
  return not_found;
}

/* unit_is_alive (0x1a9a30)
 *
 * Returns whether the given unit handle refers to a unit that is currently
 * alive. Resolves the handle via object_get_and_verify_type with type mask
 * 0x3 (bit 0 = biped, bit 1 = vehicle — accepts any unit object; asserts
 * otherwise) and tests bit 6 of the unit's flag word at offset 0x1B4
 * (unit_data_t.unk_436).
 */
bool unit_is_alive(int unit_handle)
{
  unit_data_t *unit;

  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);
  return (unit->unk_436 >> 6) & 1;
}

/* any_unit_is_dangerous (0x1aa3c0)
 *
 * Iterates all unit objects (type_mask=3: bipeds + vehicles) and returns true
 * if any unit is considered "dangerous" — i.e. in a combat-relevant state
 * that should block saving or other safe-state transitions.
 *
 * A unit is dangerous when:
 *   - unk_595 (offset 0x253) == 0x21 and unk_573 (offset 0x23D) != 3
 *     (unit in some active/aggressive state, not in state 3)
 *   - unk_595 == 0x19 or 0x18, and bit 2 of unk_584 (offset 0x248) is clear
 *     (unit in alert/combat stance without a particular suppression flag)
 *
 * Called by game_all_quiet, game_safe_to_save, and players_respawn_coop.
 */
bool any_unit_is_dangerous(void)
{
  int iter_buf[4];
  unit_data_t *unit;
  uint8_t state;

  ((void (*)(void *, int, uint8_t))0x13d6f0)(iter_buf, 3, 1);
  unit = (unit_data_t *)((void *(*)(void *))0x13d730)(iter_buf);

  while (unit != NULL) {
    state = unit->unk_595;
    if (state == 0x21 && unit->unk_573 != 3)
      return true;
    if ((state == 0x19 || state == 0x18) && (unit->unk_584 & 4) == 0)
      return true;
    unit = (unit_data_t *)((void *(*)(void *))0x13d730)(iter_buf);
  }

  return false;
}

/* unit_pickup_equipment (0x1aab20)
 *
 * Attempts to pick up an equipment object for a unit. Validates that the
 * equipment's powerup_type is neither _equipment_powerup_none (0) nor
 * _equipment_powerup_grenade (6). If the unit already holds equipment
 * (unit+0x2C8 != NONE) and flag==1, deletes the existing equipment first.
 * If the equipment slot is free, detaches the equipment from the map,
 * hides it, attaches it to the unit, and triggers any pickup sound effect
 * if the unit has a controlling player. Returns true on success. */
bool unit_pickup_equipment(int unit_handle, int equipment_handle, short flag)
{
  int *equipment_obj;
  int equipment_def;
  char *unit_obj;
  int player_handle;
  char *player;

  equipment_obj = (int *)object_get_and_verify_type(equipment_handle, 8);
  equipment_def = (int)tag_get(0x65716970, *equipment_obj);
  unit_obj = (char *)object_get_and_verify_type(unit_handle, 3);

  if (*(short *)(equipment_def + 0x308) == 0) {
    display_assert(
      "equipment_definition->equipment.powerup_type!=_equipment_powerup_none",
      "c:\\halo\\SOURCE\\units\\units.c", 0x1ca1, 1);
    system_exit(NONE);
  }
  if (*(short *)(equipment_def + 0x308) == 6) {
    display_assert("equipment_definition->equipment.powerup_type!=_equipment_"
                   "powerup_grenade",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x1ca2, 1);
    system_exit(NONE);
  }

  if (*(int *)(unit_obj + 0x2c8) != NONE && flag == 1) {
    object_delete(*(int *)(unit_obj + 0x2c8));
    *(int *)(unit_obj + 0x2c8) = NONE;
  }

  if (*(int *)(unit_obj + 0x2c8) == NONE) {
    /* detach equipment from map and hide it */
    object_disconnect_from_map(equipment_handle);
    object_set_garbage(equipment_handle, 0);

    /* if the unit has a controlling player, trigger the pickup sound */
    player_handle = player_index_from_unit_index(unit_handle);
    if (player_handle != NONE) {
      player_handle = player_index_from_unit_index(unit_handle);
      player = (char *)datum_get(player_data, player_handle);
      if (*(short *)(player + 0x2) != NONE) {
        item_activate_equipment_effect(equipment_handle);
      }
    }

    /* attach equipment to unit */
    item_attach_to_unit(equipment_handle, unit_handle);
    *(int *)(unit_obj + 0x2c8) = equipment_handle;
    return true;
  }

  return false;
}

/* unit_clear_seat_tag (0x1aac40)
 *
 * Clears the unit's equipment/seat tag handle at offset 0x2C8
 * (unit_data_t.unk_712). If the current value is not NONE (-1), it calls
 * object_delete (0x140cc0) on that handle to destroy the associated object,
 * then sets the field to NONE. Resolves the unit via
 * object_get_and_verify_type with type mask 3 (biped | vehicle).
 */
void unit_clear_seat_tag(int unit_handle)
{
  unit_data_t *unit;

  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);
  if (unit->unk_712.value != -1) {
    object_delete(unit->unk_712.value);
    unit->unk_712.value = -1;
  }
}

/* unit_count_weapons (0x1aad90)
 *
 * Counts the number of "countable" weapons held by a unit. Iterates all 4
 * weapon slots (unit_data_t.unk_680, offset 0x2A8). For each non-NONE slot,
 * resolves the weapon object, looks up its weapon tag via tag_get("weap"),
 * and checks byte at tag+0x308 bit 0x10. Weapons without that bit set are
 * counted. Returns the count as int16_t.
 */
int16_t unit_count_weapons(int unit_handle)
{
  unit_data_t *unit;
  int count;
  int weapon_handle;
  int *weapon_obj;
  char *weapon_tag;
  int i;

  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);
  count = 0;

  for (i = 0; i < MAXIMUM_WEAPONS_PER_UNIT; i++) {
    weapon_handle = unit->unk_680[i].value;
    if (weapon_handle != -1) {
      weapon_obj = (int *)object_get_and_verify_type(weapon_handle, 4);
      weapon_tag = (char *)tag_get(0x77656170, *weapon_obj);
      if ((*(uint8_t *)(weapon_tag + 0x308) & 0x10) == 0) {
        count++;
      }
    }
  }

  return (int16_t)count;
}

/* unit_weapon_is_new (0x1aae00)
 *
 * Returns true if the given weapon is "new" to the unit — i.e. no existing
 * weapon in the unit's 4 weapon slots shares the same tag definition (first
 * dword of the weapon object data, the tag_index). Resolves the target weapon
 * via object_get_and_verify_type with type mask 4 (weapon), then iterates all
 * weapon slots comparing tag indices. If any match is found, returns false;
 * otherwise returns true.
 */
bool unit_weapon_is_new(int unit_handle, int weapon_unit_handle)
{
  unit_data_t *unit;
  int *target_weapon_obj;
  int *slot_weapon_obj;
  bool is_new;
  int weapon_handle;
  int i;

  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);
  target_weapon_obj = (int *)object_get_and_verify_type(weapon_unit_handle, 4);
  tag_get(0x77656170, *target_weapon_obj);
  is_new = true;

  for (i = 0; i < MAXIMUM_WEAPONS_PER_UNIT; i++) {
    weapon_handle = unit->unk_680[i].value;
    if (weapon_handle != -1) {
      slot_weapon_obj = (int *)object_get_and_verify_type(weapon_handle, 4);
      if (*target_weapon_obj == *slot_weapon_obj) {
        is_new = false;
      }
    }
  }

  return is_new;
}

/* unit_get_weapon (0x1adeb0)
 *
 * Returns the weapon datum handle stored in the unit's weapon slot array
 * (unit_data_t.unk_680, offset 0x2A8) at the given weapon_index. If
 * weapon_index is NONE (-1), returns NONE. Asserts that the index is in
 * range [0, MAXIMUM_WEAPONS_PER_UNIT=4). Resolves the unit via
 * object_get_and_verify_type with type mask 3 (biped | vehicle).
 */
int unit_get_weapon(int unit_handle, int16_t weapon_index)
{
  unit_data_t *unit;
  int result;

  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);
  result = -1;
  if (weapon_index != -1) {
    if (weapon_index < 0 || weapon_index >= MAXIMUM_WEAPONS_PER_UNIT) {
      display_assert("index>=0 && index<MAXIMUM_WEAPONS_PER_UNIT",
                     "c:\\halo\\SOURCE\\units\\units.c", 0x20ac, 1);
      system_exit(-1);
    }
    result = unit->unk_680[weapon_index].value;
  }
  return result;
}

/* unit_clear_seat_equipment (0x1ae330)
 *
 * Clears the unit's seat equipment handle at offset 0x2C8
 * (unit_data_t.unk_712). If the current value is not NONE (-1), calls the
 * dual-register function at 0x1ab990 (EDI=unit_handle, ESI=equipment_handle)
 * to detach/remove the equipment, then sets the field to NONE.
 */
void unit_clear_seat_equipment(int unit_handle)
{
  unit_data_t *unit;
  int equipment_handle;

  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);
  equipment_handle = unit->unk_712.value;
  if (equipment_handle != -1) {
    /* 0x1ab990: dual-register call, EDI=unit_handle, ESI=equipment_handle */
    {
      int _edi = unit_handle;
      int _esi = equipment_handle;
      __asm__ __volatile__("call *%[fn]"
                           : "+D"(_edi), "+S"(_esi)
                           : [fn] "r"((void *)0x1ab990)
                           : "eax", "ecx", "edx", "ebx", "memory", "cc");
    }
    unit->unk_712.value = -1;
  }
}

/* unit_next_weapon_index (0x1ae490)
 *
 * Scans the unit's weapon slots circularly in the given direction to find the
 * next valid/usable weapon. Uses unit->unk_680[] (offset 0x2A8) as the weapon
 * handle array and unit->unk_696[] (offset 0x2B8) as a priority/ordering array.
 *
 * If weapon_index is NONE (-1), starts scanning from slot 0. If direction is 0,
 * picks the weapon with the lowest priority value; if direction is nonzero,
 * picks the first valid weapon found. Returns the index of the best weapon
 * found, or NONE (-1) if no valid weapon exists.
 *
 * Callees (by address, not in kb.json):
 *   0x1ae290 — get unit animation tag pointer (EAX=unit_handle@<eax>)
 *   0xfae80  — get weapon tag info pointer (1 stack arg: weapon_handle)
 *   0x1acd70 — check unit can use weapon (EAX=unit_handle@<eax>, 3 stack args)
 *   0xa8b30  — weapon usability callback (2 stack args)
 *   0xfb090  — weapon has must-be-readied flag (1 stack arg)
 */
int16_t unit_next_weapon_index(int unit_handle, int16_t weapon_index,
                               int16_t direction)
{
  unit_data_t *unit;
  int current_index;
  int best_index;
  int iter_index;
  int weapon_handle;
  int anim_tag;
  int weapon_tag;
  char can_use;
  char usable;
  char must_be_readied;

  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);
  best_index = -1;

  if (weapon_index == (int16_t)-1) {
    weapon_index = 0;
  } else if (weapon_index < 0 || weapon_index >= MAXIMUM_WEAPONS_PER_UNIT) {
    display_assert("current_index>=0 && current_index<MAXIMUM_WEAPONS_PER_UNIT",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x1e40, 1);
    system_exit(-1);
  }

  current_index = weapon_index;

  do {
    iter_index = (int)(int16_t)current_index;
    weapon_handle = unit->unk_680[iter_index].value;

    if (weapon_handle != -1) {
      /* Validate both the unit and weapon objects */
      object_get_and_verify_type(unit_handle, 3);
      object_get_and_verify_type(weapon_handle, 4);

      /* 0x1ae290: get unit animation tag pointer, EAX=unit_handle */
      {
        int _eax = unit_handle;
        asm volatile("call *%[fn]"
                     : "+a"(_eax)
                     : [fn] "r"((void *)0x1ae290)
                     : "ecx", "edx", "memory", "cc");
        anim_tag = _eax;
      }

      /* 0xfae80: get weapon tag info pointer, 1 cdecl arg */
      weapon_tag = ((int (*)(int))0xfae80)(weapon_handle);

      /* 0x1acd70: check unit can use weapon, EAX=unit_handle, 3 stack args */
      {
        int _eax = unit_handle;
        int args[3];
        args[0] = anim_tag;
        args[1] = weapon_tag;
        args[2] = 0;
        asm volatile("pushl %[a2]\n\t"
                     "pushl %[a1]\n\t"
                     "pushl %[a0]\n\t"
                     "call *%[fn]\n\t"
                     "addl $12, %%esp"
                     : "+a"(_eax)
                     : [fn] "r"((void *)0x1acd70), [a0] "r"(args[0]),
                       [a1] "r"(args[1]), [a2] "r"(args[2])
                     : "ecx", "edx", "memory", "cc");
        can_use = (char)_eax;
      }

      if (can_use != 0) {
        /* 0xa8b30: weapon usability callback, 2 cdecl args */
        usable = (char)((int (*)(int, int))0xa8b30)(unit_handle, weapon_handle);
        if (usable != 0) {
          /* direction != 0: pick first valid; direction == 0: pick lowest
           * priority */
          if (direction != 0) {
            best_index = current_index;
          } else {
            if ((int16_t)best_index == (int16_t)-1 ||
                unit->unk_696[(int)(int16_t)best_index].value <
                  unit->unk_696[iter_index].value) {
              best_index = current_index;
            }
          }

          /* 0xfb090: check weapon must-be-readied flag */
          must_be_readied =
            (char)((int (*)(int))0xfb090)(unit->unk_680[iter_index].value);
          if (must_be_readied != 0)
            return (int16_t)best_index;

          if ((int16_t)current_index != weapon_index)
            return (int16_t)best_index;
        }
      }
    }

    /* Advance to next slot, wrapping around */
    if (direction < 0) {
      if ((int16_t)current_index == 0)
        current_index = 3;
      else
        current_index = iter_index - 1;
    } else {
      if ((int16_t)current_index == 3)
        current_index = 0;
      else
        current_index = iter_index + 1;
    }
  } while ((int16_t)current_index != weapon_index);

  return (int16_t)best_index;
}

/* unit_set_in_vehicle (0x1ae600)
 *
 * Attempts to stow/put the unit's current weapon into a vehicle slot.
 * Returns true (1) if the weapon was successfully placed, false (0) otherwise.
 *
 * Steps:
 * 1. Gets the unit tag definition via tag_get("unit", unit->tag_index).
 * 2. Looks up the current weapon handle via unit_get_weapon.
 * 3. Calls FUN_001ae490 to compute the next weapon index.
 * 4. Skips if weapon is NONE, or if next index equals current and flag is
 * false.
 * 5. Checks the weapon object's flags byte (bit 0 must be clear).
 * 6. Calls FUN_000fd360(weapon_handle, flag) to attempt the placement.
 * 7. On success: fires unit event 0xd, calls FUN_001ab990, clears the weapon
 *    slot, resets current/next weapon indices, and optionally deletes the
 *    weapon object if FUN_000faf50 returns false.
 */
bool unit_set_in_vehicle(int unit_handle, bool flag)
{
  unit_data_t *unit;
  int weapon_handle;
  int16_t new_index;
  object_data_t *weapon_obj;
  int16_t cur_index;

  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);
  tag_get(0x756e6974, *(int *)unit);
  (void)object_get_and_verify_type(unit_handle, 3);
  weapon_handle = unit_get_weapon(unit_handle, unit->unk_674);
  new_index = unit_next_weapon_index(unit_handle, unit->unk_674, 1);

  if (weapon_handle == -1)
    return false;
  if (new_index == unit->unk_674 && !flag)
    return false;

  weapon_obj = (object_data_t *)object_get_and_verify_type(weapon_handle, -1);
  if (weapon_obj->flags & 1)
    return false;

  if (!((bool (*)(int, bool))0xfd360)(weapon_handle, flag))
    return false;

  ((void (*)(int, int))0xde360)(unit_handle, 0xd);

  /* 0x1ab990 takes EDI=unit_handle, ESI=weapon_handle as register args
   * (two register args, can't use kb.json single-reg thunk) */
  {
    int _edi = unit_handle;
    int _esi = weapon_handle;
    asm volatile("call *%[fn]"
                 : "+D"(_edi), "+S"(_esi)
                 : [fn] "r"((void *)0x1ab990)
                 : "eax", "ecx", "edx", "ebx", "memory", "cc");
  }

  cur_index = (int16_t)unit->unk_674;
  unit->unk_680[cur_index].value = -1;
  unit->unk_674 = (uint16_t)-1;
  new_index = unit_next_weapon_index(unit_handle, -1, 0);
  unit->unk_676 = (uint16_t)new_index;

  if (!((bool (*)(int))0xfaf50)(weapon_handle))
    object_delete(weapon_handle);

  return true;
}

/* unit_control_trace (0x1af6b0)
 *
 * Validates the unit's internal vectors (facing, aiming, looking, forward, up)
 * via an internal verify function at 0x1af620. If any vector is invalid, dumps
 * the unit's position, orientation, facing/aiming/looking vectors both as
 * float and as raw hex to the error log, then retries verification. If it
 * still fails, fires a fatal assert ("unit_verify_vectors FAILURE").
 *
 * Register arg: unit_handle passed in EDI.
 * Stack arg: label string (e.g. "unit-control") used in the error header.
 *
 * The verify function at 0x1af620 takes unit_handle in EAX, calls
 * assert_valid_real_normal3d on each direction vector and returns true
 * if all are valid.
 *
 * The function at 0x49ac0 builds a textual location string for the unit
 * (using the actor or swarm actor index) into a caller-provided buffer.
 */
void unit_control_trace(int unit_handle, const char *label)
{
  unit_data_t *unit;
  int32_t actor;
  char location_buf[512];

  /* 0x1af6b9: MOV EAX, EDI; CALL 0x1af620 — verify vectors with
   * unit_handle in EAX. Returns true if all vectors are valid normals. */
  {
    int _eax = unit_handle;
    __asm__ __volatile__("call *%[fn]"
                         : "+a"(_eax)
                         : [fn] "r"((void *)0x1af620)
                         : "ecx", "edx", "memory", "cc");
    if ((bool)_eax)
      return;
  }

  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);

  /* 0x1af6d3: choose actor_index; fall back to swarm_actor_index */
  actor = unit->actor_index.value;
  if (actor == -1)
    actor = unit->swarm_actor_index.value;

  /* 0x1af6f7: build location string into stack buffer */
  ((void (*)(int, int, int, char *, int))0x49ac0)(actor, unit_handle, 1,
                                                  location_buf, 512);

  /* header: "unit_verify_vectors: problems with %s at location %s" */
  error(2, "**** unit_verify_vectors: problems with %s at location %s",
        location_buf, label);

  /* dump object position, forward, up as floats */
  error(2, "  object: pos %f %f %f, fwd %f %f %f, up %f %f %f",
        (double)unit->object.unk_12.x, (double)unit->object.unk_12.y,
        (double)unit->object.unk_12.z, (double)unit->object.unk_36.x,
        (double)unit->object.unk_36.y, (double)unit->object.unk_36.z,
        (double)unit->object.unk_48.x, (double)unit->object.unk_48.y,
        (double)unit->object.unk_48.z);

  /* dump desired facing, aiming, looking as floats */
  error(
    2, "  desired facing %f %f %f, aiming %f %f %f, looking %f %f %f",
    (double)unit->unk_468.x, (double)unit->unk_468.y, (double)unit->unk_468.z,
    (double)unit->unk_480.x, (double)unit->unk_480.y, (double)unit->unk_480.z,
    (double)unit->unk_516.x, (double)unit->unk_516.y, (double)unit->unk_516.z);

  /* dump aiming vector + velocity as floats */
  error(2, "  aiming vector %f %f %f velocity %f %f %f",
        (double)unit->unk_492.x, (double)unit->unk_492.y,
        (double)unit->unk_492.z, (double)unit->unk_504.x,
        (double)unit->unk_504.y, (double)unit->unk_504.z);

  /* dump looking vector + velocity as floats */
  error(2, "  looking vector %f %f %f velocity %f %f %f",
        (double)unit->unk_528.x, (double)unit->unk_528.y,
        (double)unit->unk_528.z, (double)unit->unk_540.x,
        (double)unit->unk_540.y, (double)unit->unk_540.z);

  error(2, "  warning, hex dump follows...");

  /* dump object position, forward, up as hex (raw dword reinterpret) */
  error(
    2, "  object: pos %08X %08X %08X, fwd %08X %08X %08X, up %08X %08X %08X",
    *(uint32_t *)&unit->object.unk_12.x, *(uint32_t *)&unit->object.unk_12.y,
    *(uint32_t *)&unit->object.unk_12.z, *(uint32_t *)&unit->object.unk_36.x,
    *(uint32_t *)&unit->object.unk_36.y, *(uint32_t *)&unit->object.unk_36.z,
    *(uint32_t *)&unit->object.unk_48.x, *(uint32_t *)&unit->object.unk_48.y,
    *(uint32_t *)&unit->object.unk_48.z);

  /* dump desired facing, aiming, looking as hex */
  error(2,
        "  desired facing %08X %08X %08X, aiming %08X %08X %08X, looking %08X "
        "%08X %08X",
        *(uint32_t *)&unit->unk_468.x, *(uint32_t *)&unit->unk_468.y,
        *(uint32_t *)&unit->unk_468.z, *(uint32_t *)&unit->unk_480.x,
        *(uint32_t *)&unit->unk_480.y, *(uint32_t *)&unit->unk_480.z,
        *(uint32_t *)&unit->unk_516.x, *(uint32_t *)&unit->unk_516.y,
        *(uint32_t *)&unit->unk_516.z);

  /* dump aiming vector + velocity as hex */
  error(2, "  aiming vector %08X %08X %08X velocity %08X %08X %08X",
        *(uint32_t *)&unit->unk_492.x, *(uint32_t *)&unit->unk_492.y,
        *(uint32_t *)&unit->unk_492.z, *(uint32_t *)&unit->unk_504.x,
        *(uint32_t *)&unit->unk_504.y, *(uint32_t *)&unit->unk_504.z);

  /* dump looking vector + velocity as hex */
  error(2, "  looking vector %08X %08X %08X velocity %08X %08X %08X",
        *(uint32_t *)&unit->unk_528.x, *(uint32_t *)&unit->unk_528.y,
        *(uint32_t *)&unit->unk_528.z, *(uint32_t *)&unit->unk_540.x,
        *(uint32_t *)&unit->unk_540.y, *(uint32_t *)&unit->unk_540.z);

  /* retry verification */
  {
    int _eax = unit_handle;
    __asm__ __volatile__("call *%[fn]"
                         : "+a"(_eax)
                         : [fn] "r"((void *)0x1af620)
                         : "ecx", "edx", "memory", "cc");
    if ((bool)_eax)
      return;
  }

  /* fatal assert if vectors are still broken */
  display_assert("unit_verify_vectors FAILURE, see above for details",
                 "c:\\halo\\SOURCE\\units\\units.c", 0x252, true);
  system_exit(-1);
}

/* unit_set_control (0x1af990)
 *
 * Validates and applies a unit_control block to the given unit. The control
 * data (param_2) is a 0x40-byte struct containing animation state, aiming
 * speed, control flags, weapon/grenade/zoom indices, throttle vector,
 * primary trigger, facing/aiming/looking vectors.
 *
 * Validation asserts (with original line numbers):
 *   - throttle magnitude <= 3.0f (line 0x5e1)
 *   - animation_state in [0,7) (line 0x5e2)
 *   - aiming_speed in [0,2) (line 0x5e3)
 *   - control_flags valid (bit 7 clear) (line 0x5e4)
 *   - facing/aiming/looking vectors are valid normals (lines 0x5e5-0x5e7)
 *   - weapon_index NONE or in [0,4) (line 0x5e8)
 *   - grenade_index NONE or in [0,2) (line 0x5e9)
 *   - zoom_level NONE or >= 0 (line 0x5ea)
 *   - primary_trigger is not NaN/Inf (line 0x5eb)
 *
 * After validation, copies fields from control_data into unit_data_t at
 * offsets documented in the store-offset table below.
 *
 * Store-offset table (unit <- control_data):
 *   +0x228 <- +0x0C  throttle (vector3)
 *   +0x234 <- +0x18  primary_trigger (float)
 *   +0x238 <- +0x01  aiming_speed (byte)
 *   +0x2A4 <- +0x04  weapon_index (word, if not NONE)
 *   +0x2CD <- +0x06  grenade_index (byte, if not NONE)
 *   +0x2D1 <- +0x08  zoom_level (byte)
 *   +0x1B8 <- +0x02  control_flags (zext word -> dword)
 *   +0x204 <- +0x34  looking_vector (vector3)
 *   +0x1E0 <- +0x28  aiming_vector (vector3)
 *   +0x1D4 <- +0x1C  facing_vector (vector3)
 *   +0x256 <- +0x00  animation_state (byte)
 */
void unit_set_control(int unit_handle, void *unit_control)
{
  unit_data_t *unit;
  char *cd;
  float mag;
  float *looking;

  cd = (char *)unit_control;
  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);

  /* validate throttle magnitude <= 3.0f */
  mag = sqrtf(*(float *)(cd + 0x14) * *(float *)(cd + 0x14) +
              *(float *)(cd + 0x10) * *(float *)(cd + 0x10) +
              *(float *)(cd + 0x0c) * *(float *)(cd + 0x0c));
  if (!(mag <= *(float *)0x254644)) {
    display_assert("magnitude3d(&control_data->throttle)<=3.0f",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x5e1, 1);
    system_exit(-1);
  }

  /* validate animation_state in [0, 7) */
  if (cd[0] < 0 || cd[0] >= 7) {
    display_assert("control_data->animation_state>=0 && control_data->"
                   "animation_state<NUMBER_OF_UNIT_ANIMATION_STATES",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x5e2, 1);
    system_exit(-1);
  }

  /* validate aiming_speed in [0, 2) */
  if (cd[1] < 0 || cd[1] >= 2) {
    display_assert("control_data->aiming_speed>=0 && control_data->"
                   "aiming_speed<NUMBER_OF_UNIT_AIMING_SPEEDS",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x5e3, 1);
    system_exit(-1);
  }

  /* validate control_flags (bit 7 must be clear) */
  if (cd[3] & 0x80) {
    display_assert("VALID_FLAGS(control_data->control_flags, "
                   "NUMBER_OF_UNIT_CONTROL_FLAGS)",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x5e4, 1);
    system_exit(-1);
  }

  /* validate facing_vector is a valid normal */
  if (!((bool (*)(float *))0x21fb0)((float *)(cd + 0x1c))) {
    display_assert(
      csprintf(error_string_buffer,
               "%s: assert_valid_real_normal3d(%f, %f, %f)",
               "&control_data->facing_vector", (double)*(float *)(cd + 0x1c),
               (double)*(float *)(cd + 0x20), (double)*(float *)(cd + 0x24)),
      "c:\\halo\\SOURCE\\units\\units.c", 0x5e5, 1);
    system_exit(-1);
  }

  /* validate aiming_vector is a valid normal */
  if (!((bool (*)(float *))0x21fb0)((float *)(cd + 0x28))) {
    display_assert(
      csprintf(error_string_buffer,
               "%s: assert_valid_real_normal3d(%f, %f, %f)",
               "&control_data->aiming_vector", (double)*(float *)(cd + 0x28),
               (double)*(float *)(cd + 0x2c), (double)*(float *)(cd + 0x30)),
      "c:\\halo\\SOURCE\\units\\units.c", 0x5e6, 1);
    system_exit(-1);
  }

  /* validate looking_vector is a valid normal */
  looking = (float *)(cd + 0x34);
  if (!((bool (*)(float *))0x21fb0)(looking)) {
    display_assert(csprintf(error_string_buffer,
                            "%s: assert_valid_real_normal3d(%f, %f, %f)",
                            "&control_data->looking_vector", (double)looking[0],
                            (double)*(float *)(cd + 0x38),
                            (double)*(float *)(cd + 0x3c)),
                   "c:\\halo\\SOURCE\\units\\units.c", 0x5e7, 1);
    system_exit(-1);
  }

  /* validate weapon_index: NONE or in [0, MAXIMUM_WEAPONS_PER_UNIT) */
  if (*(int16_t *)(cd + 4) != -1 &&
      (*(int16_t *)(cd + 4) < 0 ||
       *(int16_t *)(cd + 4) >= MAXIMUM_WEAPONS_PER_UNIT)) {
    display_assert("control_data->weapon_index==NONE || (control_data->"
                   "weapon_index>=0 && control_data->"
                   "weapon_index<MAXIMUM_WEAPONS_PER_UNIT)",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x5e8, 1);
    system_exit(-1);
  }

  /* validate grenade_index: NONE or in [0, NUMBER_OF_UNIT_GRENADE_TYPES) */
  if (*(int16_t *)(cd + 6) != -1 &&
      (*(int16_t *)(cd + 6) < 0 ||
       *(int16_t *)(cd + 6) >= NUMBER_OF_UNIT_GRENADE_TYPES)) {
    display_assert("control_data->grenade_index==NONE || (control_data->"
                   "grenade_index>=0 && control_data->"
                   "grenade_index<NUMBER_OF_UNIT_GRENADE_TYPES)",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x5e9, 1);
    system_exit(-1);
  }

  /* validate zoom_level: NONE or >= 0 */
  if (*(int16_t *)(cd + 8) != -1 && *(int16_t *)(cd + 8) < 0) {
    display_assert("control_data->zoom_level==NONE || (control_data->"
                   "zoom_level>=0)",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x5ea, 1);
    system_exit(-1);
  }

  /* validate primary_trigger is not NaN/Inf */
  if ((*(uint32_t *)(cd + 0x18) & 0x7f800000) == 0x7f800000) {
    display_assert(
      csprintf(error_string_buffer, "%s: assert_valid_real(0x%08X %f)",
               "control_data->primary_trigger", *(uint32_t *)(cd + 0x18),
               (double)*(float *)(cd + 0x18)),
      "c:\\halo\\SOURCE\\units\\units.c", 0x5eb, 1);
    system_exit(-1);
  }

  /* copy throttle vector (control +0x0C -> unit +0x228) */
  unit->unk_552.x = *(float *)(cd + 0x0c);
  unit->unk_552.y = *(float *)(cd + 0x10);
  unit->unk_552.z = *(float *)(cd + 0x14);

  /* copy primary trigger (control +0x18 -> unit +0x234) */
  unit->unk_564 = *(float *)(cd + 0x18);

  /* copy aiming speed (control +0x01 -> unit +0x238) */
  unit->unk_568 = cd[1];

  /* copy weapon index if not NONE (control +0x04 -> unit +0x2A4) */
  if (*(int16_t *)(cd + 4) != -1)
    unit->unk_676 = *(uint16_t *)(cd + 4);

  /* copy grenade index if not NONE (control +0x06 -> unit +0x2CD) */
  if (*(int16_t *)(cd + 6) != -1)
    unit->unk_717 = cd[6];

  /* copy zoom level (control +0x08 -> unit +0x2D1) */
  unit->unk_721 = cd[8];

  /* copy control flags (control +0x02 zext word -> unit +0x1B8 dword) */
  unit->unk_440 = (uint32_t) * (uint16_t *)(cd + 2);

  /* copy looking vector (control +0x34 -> unit +0x204) */
  unit->unk_516.x = looking[0];
  unit->unk_516.y = looking[1];
  unit->unk_516.z = looking[2];

  /* copy aiming vector (control +0x28 -> unit +0x1E0) */
  unit->unk_480.x = *(float *)(cd + 0x28);
  unit->unk_480.y = *(float *)(cd + 0x2c);
  unit->unk_480.z = *(float *)(cd + 0x30);

  /* copy facing vector (control +0x1C -> unit +0x1D4) */
  unit->unk_468.x = *(float *)(cd + 0x1c);
  unit->unk_468.y = *(float *)(cd + 0x20);
  unit->unk_468.z = *(float *)(cd + 0x24);

  /* copy animation state (control +0x00 -> unit +0x256) */
  unit->unk_598 = cd[0];

  /* trace/profile call */
  unit_control_trace(unit_handle, "unit-control");
}

/* unit_enter_seat (0x1b1db0)
 *
 * Attempts to place a unit into a weapon/item seat. Validates that the seat
 * object (type 4) has flag bit 0x800 set and has no parent (parent_object_index
 * == -1). Then checks unit_can_enter_seat and game_engine_unit_can_enter_seat.
 *
 * If flag == 2, clears all existing weapons first. Finds an empty weapon slot
 * via an internal helper (0x1aad60, EAX reg-arg). If a slot is found:
 *   - disconnects the seat object from the map
 *   - disables garbage collection on it
 *   - attaches it to the unit
 *   - stores the seat object handle in unk_680[slot]
 *   - clears unk_696[slot]
 *
 * Based on the flag value:
 *   flag 0: sets unk_676 via unit_next_weapon_index(unit, unk_674, 0)
 *   flag 1: if control_flags bit 0x800 is clear, calls
 *           player_control_set_unit_seat, then sets unk_676 = slot
 *   flag 2: sets unk_676 = slot
 *   default: returns true without changing unk_676
 *
 * Returns true if the seat was entered, false otherwise.
 */
bool unit_enter_seat(int unit_handle, int seat_object_handle, int16_t flag)
{
  object_data_t *seat_obj;
  unit_data_t *unit;
  int16_t seat_index;
  int _eax;

  seat_obj = (object_data_t *)object_get_and_verify_type(seat_object_handle, 4);
  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);

  if (!(seat_obj->flags & 0x800))
    return false;
  if (seat_obj->parent_object_index.value != -1)
    return false;
  if (!unit_can_enter_seat(unit_handle, seat_object_handle))
    return false;
  if (!game_engine_unit_can_enter_seat(unit_handle, seat_object_handle))
    return false;

  if (flag == 2)
    unit_clear_weapons(unit_handle);

  /* FUN_001aad60: finds first empty weapon slot. EAX = unit_handle (reg arg),
   * returns int16_t seat index in AX, or -1 if no slot available. */
  _eax = unit_handle;
  __asm__ __volatile__("call *%[fn]"
                       : "+a"(_eax)
                       : [fn] "r"((void *)0x1aad60)
                       : "ecx", "edx", "memory", "cc");
  seat_index = (int16_t)_eax;

  if (seat_index == -1)
    return false;

  object_disconnect_from_map(seat_object_handle);
  object_set_garbage(seat_object_handle, 0);
  item_attach_to_unit(seat_object_handle, unit_handle);

  unit->unk_680[(int16_t)seat_index].value = seat_object_handle;
  unit->unk_696[(int16_t)seat_index].value = 0;

  switch (flag) {
  case 0:
    unit->unk_676 = unit_next_weapon_index(unit_handle, unit->unk_674, 0);
    return true;
  case 1:
    if (!(unit->unk_440 & 0x800))
      player_control_set_unit_seat(unit_handle, seat_index);
    /* fall through */
  case 2:
    unit->unk_676 = seat_index;
    return true;
  default:
    return true;
  }
}
