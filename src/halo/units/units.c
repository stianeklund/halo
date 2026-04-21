/* units.c — unit lifecycle and query helpers.
 *
 * Corresponds to units.obj. Functions sorted by XBE address.
 */

#include "../../common.h"

#define NUMBER_OF_UNIT_BASE_SEATS 6

/* unit_set_actively_controlled_flag (0x1a7f80)
 *
 * Sets bit 5 (0x20) of the byte at object_data_t+0xb6 (offset 182,
 * the byte just before unk_183) on the resolved unit object.
 * Distinct from unit_delete (0x1a7fc0) which sets the same bit at
 * offset 0xb7 (unk_183).
 *
 * Confirmed: CALL 0x13d680 with type_mask=3 (biped|vehicle).
 * Confirmed: OR byte ptr [EAX+0xb6],0x20.
 */
void unit_set_actively_controlled_flag(int unit_handle)
{
  object_data_t *obj;

  obj = (object_data_t *)object_get_and_verify_type(unit_handle, 3);
  *(uint8_t *)((char *)obj + 0xb6) |= 0x20;
}

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

  object_iterator_new(iter_buf, 3, 1);
  unit = (unit_data_t *)object_iterator_next(iter_buf);

  while (unit != NULL) {
    state = unit->unk_595;
    if (state == 0x21 && unit->unk_573 != 3)
      return true;
    if ((state == 0x19 || state == 0x18) && (unit->unk_584 & 4) == 0)
      return true;
    unit = (unit_data_t *)object_iterator_next(iter_buf);
  }

  return false;
}

/* unit_update_seat_occupancy (0x1aa890)
 *
 * Walks the child object chain of a unit (starting at unk_200, offset 0xC8)
 * and updates two seat-occupancy handles: unk_724 (offset 0x2D4) for seats
 * with flag bit 2 (0x4) set, and unk_728 (offset 0x2D8) for seats with flag
 * bit 3 (0x8) set. For each child that is a biped or vehicle with a valid
 * seat tag index, retrieves the seat definition flags from the unit tag.
 *
 * Bit 2 (0x4): writes child handle to unk_724, but only if the unit's
 * unk_436 bit 0 is clear AND unk_724 is currently NONE.
 * Bit 3 (0x8): writes child handle to unk_728, but only if unk_728 is
 * currently NONE or equals unk_724.
 *
 * Register arg: unit_handle passed in EAX (vehicle_handle).
 *
 * Confirmed: PUSH 0x3 / PUSH EAX -> object_get_and_verify_type.
 * Confirmed: child chain via [ESI+0xC8] -> [EBX+0xC4].
 * Confirmed: seat flags from tag_block_get_element(tag+0x2e4, index, 0x11c).
 * Confirmed: TEST CL,0x4 and TEST CL,0x8 for seat flag bits.
 * Confirmed: stores to [ESI+0x2D4] and [ESI+0x2D8].
 */
void unit_update_seat_occupancy(int vehicle_handle)
{
  char *unit;
  char *unit_tag;
  int child_handle;
  char *child_obj;
  int *seat_element;
  int seat_flags;

  unit = (char *)object_get_and_verify_type(vehicle_handle, 3);
  unit_tag = (char *)tag_get(0x756e6974, *(int *)unit);
  child_handle = *(int *)(unit + 0xc8);

  while (child_handle != -1) {
    child_obj = (char *)object_get_and_verify_type(child_handle, -1);

    if (((1 << (*(uint8_t *)(child_obj + 0x64) & 0x1f)) & 3) &&
        *(int16_t *)(child_obj + 0x2a0) != -1) {
      seat_element = (int *)tag_block_get_element(
        unit_tag + 0x2e4, (int)*(int16_t *)(child_obj + 0x2a0), 0x11c);
      seat_flags = *seat_element;

      if ((seat_flags & 4) == 0 || (*(uint8_t *)(unit + 0x1b4) & 1) ||
          *(int *)(unit + 0x2d4) != -1) {
        /* bit 2 not set, or unit already has unk_724 occupied —
         * check bit 3 only */
        if (seat_flags & 8) {
          if (*(int *)(unit + 0x2d8) == -1 ||
              *(int *)(unit + 0x2d8) == *(int *)(unit + 0x2d4)) {
            *(int *)(unit + 0x2d8) = child_handle;
          }
        }
      } else {
        /* bit 2 set and unk_724 is NONE and unk_436 bit 0 clear */
        *(int *)(unit + 0x2d4) = child_handle;
        if (seat_flags & 8) {
          if (*(int *)(unit + 0x2d8) == -1) {
            *(int *)(unit + 0x2d8) = child_handle;
          }
        }
      }
    }

    child_handle = *(int *)(child_obj + 0xc4);
  }
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

/* unit_clear_weapons (0x1aac80)
 *
 * Deletes all weapons from a unit's weapon slots EXCEPT the one at the
 * current weapon index (unk_674, offset 0x2A2). For each of the 4 weapon
 * slots: if the slot is occupied (handle != NONE) and the slot index does
 * not match the current weapon index, deletes the weapon object and clears
 * the slot handle to NONE. Also resets the next-weapon index (unk_676,
 * offset 0x2A4) or current-weapon index (unk_674) to NONE if they matched
 * the cleared slot. Called by unit_enter_seat when flag==2 to strip all
 * secondary weapons before seating.
 */
void unit_clear_weapons(int unit_handle)
{
  unit_data_t *unit;
  int16_t i;

  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);

  for (i = 0; i < MAXIMUM_WEAPONS_PER_UNIT; i++) {
    if (unit->unk_680[i].value != -1 && i != (int16_t)unit->unk_674) {
      object_delete(unit->unk_680[i].value);
      unit->unk_680[i].value = -1;
      if (i == (int16_t)unit->unk_676) {
        unit->unk_676 = (uint16_t)-1;
      }
      if (i == (int16_t)unit->unk_674) {
        unit->unk_674 = (uint16_t)-1;
      }
    }
  }
}

/* unit_find_empty_weapon_slot (0x1aad60)
 *
 * Scans the unit's 4 weapon slots (unit_data_t.unk_680, offset 0x2A8)
 * for the first slot containing NONE (-1) and returns its index (0-3).
 * Returns -1 if all slots are occupied.
 *
 * Register arg: unit_handle passed in EAX.
 *
 * Confirmed: PUSH 0x3 / PUSH EAX -> object_get_and_verify_type(handle, 3).
 * Confirmed: CMP dword ptr [EAX + ESI*4 + 0x2a8], -1 — weapon slot check.
 * Confirmed: CMP CX, 0x4 — loop bound is 4 (MAXIMUM_WEAPONS_PER_UNIT).
 * Confirmed: Returns int16_t (MOV AX,CX / MOV AX,DX).
 */
int16_t unit_find_empty_weapon_slot(int unit_handle)
{
  unit_data_t *unit;
  int16_t i;

  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);

  for (i = 0; i < MAXIMUM_WEAPONS_PER_UNIT; i++) {
    if (unit->unk_680[i].value == -1)
      return i;
  }
  return -1;
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

/* unit_set_animation (0x1ab7c0)
 *
 * Sets the current animation on a unit object. Writes the animation graph
 * tag index to offset 0x7c, the animation index (int16_t) to offset 0x80,
 * and zeroes the animation frame counter at offset 0x82. When the debug
 * flag at 0x5054fc is set, logs the unit name and animation name to the
 * console via console_printf, optionally filtered by the debug unit handle
 * at 0x5ac9f8.
 */
void unit_set_animation(int unit_handle, int anim_graph_tag_index,
                        int16_t animation_index)
{
  int *unit;
  const char *anim_name;
  int debug_filter;
  void *tag_data;

  unit = (int *)object_get_and_verify_type(unit_handle, 3);

  /* Set animation graph tag index, animation index, and zero frame counter */
  *(int *)((char *)unit + 0x7c) = anim_graph_tag_index;
  *(int16_t *)((char *)unit + 0x80) = animation_index;
  *(int16_t *)((char *)unit + 0x82) = 0;

  /* Debug logging path */
  if (*(char *)0x5054fc != 0) {
    anim_name = "<none>";
    if (anim_graph_tag_index != -1) {
      tag_data = tag_get(0x616e7472, anim_graph_tag_index);
      if (animation_index != -1) {
        anim_name = (const char *)tag_block_get_element(
          (char *)tag_data + 0x74, (int)animation_index, 0xb4);
      }
    }

    debug_filter = *(int *)0x5ac9f8;
    if (debug_filter == -1 || *(int *)((char *)unit + 0x1a4) == debug_filter ||
        *(int *)((char *)unit + 0x1a8) == debug_filter) {
      console_printf(0, "%s: animation %s",
                     tag_name_strip_path(tag_get_name(*(int *)unit)),
                     anim_name);
    }
  }
}

/* unit_detach_weapon (0x1ab990)
 *
 * Detaches a weapon/item from a unit. If the item has no parent, connects it
 * to the map and attaches it at the unit's "left hand" marker. If it has a
 * parent, asserts that the parent is the unit. Then detaches the item from
 * the unit, clears its velocity, generates a random direction offset based on
 * the unit's unk_492 facing vector with a small random angle and scale, adds
 * that to the root position, and attempts to place the item at the new
 * position. If placement fails and the game engine is not running, deletes
 * the item. Also deletes if the unit has flag bit 0x100000 set in unk_436.
 *
 * Register args: unit_handle in EDI, weapon_handle in ESI.
 *
 * Confirmed: PUSH 0x3 / PUSH EDI -> object_get_and_verify_type(unit, 3).
 * Confirmed: PUSH 0x1c / PUSH ESI -> object_get_and_verify_type(weapon, 0x1c).
 * Confirmed: parent check at [EBX+0xCC] against -1 and EDI.
 * Confirmed: object_attach_to_marker(edi, "left hand", esi, "").
 * Confirmed: item_attach_to_unit(esi, -1) to detach.
 * Confirmed: global zero vector copied from [0x31fc38].
 * Confirmed: assert "item->object.parent_object_index==unit_index" at 0x20c5.
 * Confirmed: random_direction3d with angle 0x3ec90fdb, scale range [0x3cda740e,
 * 0x3d23d70b]. Confirmed: [EBX+0x1b0] = EDI (weapon remembers its detaching
 * unit). Confirmed: flag check at unit+0x1b4 bit 0x100000.
 */
void unit_detach_weapon(int unit_handle, int weapon_handle)
{
  char *unit;
  char *weapon;
  int parent;
  float *global_origin;
  float direction[3];
  float position[3];
  int *seed;
  float scale;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  weapon = (char *)object_get_and_verify_type(weapon_handle, 0x1c);
  parent = *(int *)(weapon + 0xcc);

  if (parent == -1) {
    /* Weapon has no parent — connect to map and attach at marker */
    object_connect_to_map(weapon_handle, 0);
    object_set_garbage(weapon_handle, 1);
    object_attach_to_marker(unit_handle, (void *)0x2b6d2c, weapon_handle,
                            (void *)0x25386f);
  } else if (parent != unit_handle) {
    display_assert("item->object.parent_object_index==unit_index",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x20c5, 1);
    system_exit(-1);
  }

  /* Detach weapon from unit and parent */
  item_attach_to_unit(weapon_handle, -1);
  object_detach_from_parent(weapon_handle);

  /* Zero the weapon's velocity fields at +0x18 and +0x3c */
  global_origin = *(float **)0x31fc38;
  *(float *)(weapon + 0x18) = global_origin[0];
  *(float *)(weapon + 0x1c) = global_origin[1];
  *(float *)(weapon + 0x20) = global_origin[2];

  global_origin = *(float **)0x31fc38;
  *(float *)(weapon + 0x3c) = global_origin[0];
  *(float *)(weapon + 0x40) = global_origin[1];
  *(float *)(weapon + 0x44) = global_origin[2];

  /* Generate random throw direction from unit's facing vector (unk_492) */
  seed = get_global_random_seed_address();
  random_direction3d(seed, (float *)(unit + 0x1ec), 0.0f, 0.39269909f,
                     direction);

  /* Scale direction by random amount */
  seed = get_global_random_seed_address();
  scale = random_real_range(seed, 0.02666667f, 0.04f);
  direction[0] *= scale;
  direction[1] *= scale;
  direction[2] *= scale;

  /* Get root parent position and offset by scaled direction */
  object_get_root_location(unit_handle, position, 0);
  direction[0] = position[0] + direction[0];
  direction[1] = position[1] + direction[1];
  direction[2] = position[2] + direction[2];

  /* Record the detaching unit on the weapon */
  *(int *)(weapon + 0x1b0) = unit_handle;

  /* Set weapon position and try to place it */
  item_set_position(weapon_handle, direction, 0);
  unit_set_seat_state(unit_handle, position);

  if (!object_try_place(weapon_handle, position)) {
    if (!game_engine_running()) {
      object_delete(weapon_handle);
    }
  }

  /* If unit has death/despawn flag, force-delete the weapon */
  if (*(uint32_t *)(unit + 0x1b4) & 0x100000) {
    object_delete(weapon_handle);
  }
}

/* unit_has_weapon_with_flag (0x1ac3f0)
 *
 * Returns true if any of the unit's equipped weapons has the given flag
 * bit set in its flags field (weapon_data+0x1dc).
 *
 * Walks the 4-slot weapon handle array at unit+0x2a8; for each slot that
 * is not NONE (-1), resolves the weapon object with type_mask=4 and tests
 * bit (1 << flag_index) against the 32-bit flags at weapon_data+0x1dc.
 *
 * Confirmed: LEA ESI,[EAX+0x2a8] — weapon slot array at unit+0x2a8.
 * Confirmed: PUSH 0x4 for weapon type_mask in inner object_get_and_verify_type.
 * Confirmed: MOV ECX,BX; SHL EDX,CL — flag_index used as shift count (byte).
 * Confirmed: CMP EAX,-0x1 / JZ skip — slot NONE guard.
 * Confirmed: TEST EDX,ECX at [EAX+0x1dc] — flags field at +0x1dc.
 */
bool unit_has_weapon_with_flag(int unit_handle, int flag_index)
{
  int *unit;
  int *weapon_slots;
  int i;

  unit = (int *)object_get_and_verify_type(unit_handle, 3);
  weapon_slots = (int *)((char *)unit + 0x2a8);

  for (i = 0; i < 4; i++) {
    if (weapon_slots[i] != NONE) {
      int *weapon = (int *)object_get_and_verify_type(weapon_slots[i], 4);
      if ((1 << (flag_index & 0x1f)) & *(uint32_t *)((char *)weapon + 0x1dc))
        return 1;
    }
  }
  return 0;
}

/* unit_try_animation_state (0x1acd70)
 *
 * Searches the unit's animation graph for a matching animation mode and
 * weapon label. The animation graph is resolved via: unit tag -> antr tag
 * at offset +0x44. The antr tag's animation modes block starts at tag+0xc.
 *
 * For each mode:
 *   - If seat_label is non-NULL, compares it (case-insensitive) against the
 *     mode's name string; skips non-matching modes.
 *   - Within the mode, iterates sub-animations at mode+0x58 (size 0xBC each).
 *   - Within each sub-animation, iterates weapon labels at sub_anim+0xB0
 *     (size 0x3C each).
 *   - Matches weapon_label: NULL matches anything; "unarmed" matches empty
 *     strings; otherwise case-insensitive compare.
 *
 * If reset_flag is 0, returns true on first match without updating state.
 * If reset_flag is non-zero, updates the unit's animation state fields:
 *   - unk_592 (0x250) = mode index
 *   - unk_593 (0x251) = sub-animation index
 *   - unk_594 (0x252) = weapon label index
 *   - base_seat_index (0x257) = matched base seat label index (-1 if none)
 *   - unk_595 (0x253) = 0xff if previously != 0x1c
 *   - unk_584 (0x248) bit 1: set if mode has multi-weapon animation channels
 *
 * Register arg: unit_handle passed in EAX.
 *
 * Confirmed: PUSH 0x3 / PUSH EAX -> object_get_and_verify_type.
 * Confirmed: tag_get('unit', *unit) then tag_get('antr', unit_tag+0x44).
 * Confirmed: stricmp via CALL 0x1dd801.
 * Confirmed: csstrcmp via CALL 0x8dcb0 for "unarmed" check.
 * Confirmed: stores to offsets 0x250, 0x251, 0x252, 0x253, 0x257, 0x248.
 * Confirmed: base_seat_labels table at 0x32e484, 6 entries.
 */
bool unit_try_animation_state(int unit_handle, int seat_label, int weapon_label,
                              int reset_flag)
{
  char *unit;
  char *unit_tag;
  char *antr_tag;
  int *anim_block;
  int mode_count;
  int mode_index;
  char *mode;
  int sub_count;
  int16_t sub_index;
  char *sub_anim;
  int *weapon_block;
  int weapon_count;
  int16_t weapon_index;
  char *weapon_name;
  bool found;
  bool has_multi_weapon;
  int16_t base_seat;
  int16_t si;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  unit_tag = (char *)tag_get(0x756e6974, *(int *)unit);
  antr_tag = (char *)tag_get(0x616e7472, *(int *)(unit_tag + 0x44));
  anim_block = (int *)(antr_tag + 0xc);
  mode_count = *anim_block;
  found = false;

  if (mode_count < 1)
    return false;

  mode_index = 0;
  while (1) {
    mode = (char *)tag_block_get_element(anim_block, mode_index, 0x64);

    if (seat_label != 0 && crt_stricmp((const char *)seat_label, mode) != 0) {
      goto next_mode;
    }

    sub_count = *(int *)(mode + 0x58);
    sub_index = 0;
    if (sub_count < 1)
      goto next_mode;

    while (1) {
      sub_anim =
        (char *)tag_block_get_element(mode + 0x58, (int)sub_index, 0xbc);
      weapon_block = (int *)(sub_anim + 0xb0);
      weapon_count = *weapon_block;
      weapon_index = 0;

      if (weapon_count < 1)
        goto next_sub;

      while (1) {
        weapon_name =
          (char *)tag_block_get_element(weapon_block, (int)weapon_index, 0x3c);

        if (weapon_label == 0)
          goto matched;
        if (csstrcmp((const char *)weapon_label, "unarmed") == 0 &&
            *weapon_name == '\0')
          goto matched;
        if (crt_stricmp((const char *)weapon_label, weapon_name) == 0)
          goto matched;

        weapon_index++;
        if ((int)(int16_t)weapon_index >= weapon_count)
          goto next_sub;
        continue;

      matched:
        if (reset_flag == 0)
          goto found_match;

        /* Check if mode has multi-weapon animation channels */
        {
          int num_key_types = *(int *)(mode + 0x40);
          int *key_data = *(int **)(mode + 0x44);
          has_multi_weapon = false;

          if ((num_key_types >= 3 && *(int16_t *)(key_data + 1) != -1) ||
              (num_key_types >= 4 &&
               *(int16_t *)((char *)key_data + 6) != -1) ||
              (num_key_types >= 5 &&
               *(int16_t *)((char *)key_data + 8) != -1)) {
            has_multi_weapon = true;
          }
        }

        if (*(uint8_t *)(unit + 0x253) != 0x1c)
          *(uint8_t *)(unit + 0x253) = 0xff;

        *(uint8_t *)(unit + 0x250) = (uint8_t)mode_index;

        /* Find base_seat_index by matching seat_label against table */
        base_seat = -1;
        for (si = 0; si < NUMBER_OF_UNIT_BASE_SEATS; si++) {
          if (crt_stricmp((const char *)seat_label,
                          *(const char **)(0x32e484 + si * 4)) == 0) {
            base_seat = si;
            break;
          }
        }

        *(uint8_t *)(unit + 0x252) = (uint8_t)weapon_index;
        *(int8_t *)(unit + 0x257) = (int8_t)base_seat;
        *(uint8_t *)(unit + 0x251) = (uint8_t)sub_index;

        if (has_multi_weapon) {
          *(uint8_t *)(unit + 0x248) |= 0x2;
        } else {
          *(uint8_t *)(unit + 0x248) &= ~0x2;
        }

      found_match:
        found = true;
        goto next_sub;
      }

    next_sub:
      sub_index++;
      if ((int)(int16_t)sub_index >= *(int *)(mode + 0x58))
        goto next_mode;
    }

  next_mode:
    mode_index++;
    if ((int)(int16_t)mode_index >= *anim_block)
      break;
  }

  return found;
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

/* unit_get_seat_label (0x1ae290)
 *
 * Returns the seat label string for a unit. If the unit has a parent object
 * (offset 0xCC != NONE) and a valid seat tag index (offset 0x2A0 != -1),
 * retrieves the label from the parent's unit tag seat block at offset +4
 * within the 0x11C-sized seat element. Otherwise falls back to a base seat
 * label from the global table at 0x32e484 indexed by base_seat_index
 * (offset 0x257), asserting it is in range [0, 6).
 *
 * Register arg: unit_handle passed in EAX.
 *
 * Confirmed: PUSH 0x3 / PUSH EAX -> object_get_and_verify_type.
 * Confirmed: parent_object_index at [ESI + 0xCC].
 * Confirmed: seat tag index at [ESI + 0x2A0].
 * Confirmed: tag_get(0x756e6974, ...) for 'unit' tag.
 * Confirmed: tag_block_get_element(tag+0x2e4, seat_index, 0x11c).
 * Confirmed: assert "base_seat_index>=0 &&
 * base_seat_index<NUMBER_OF_UNIT_BASE_SEATS" at line 0x200f. Confirmed: global
 * table at 0x32e484, indexed by MOVSX of byte at +0x257.
 */
int unit_get_seat_label(int unit_handle)
{
  char *unit;
  int parent_handle;
  int16_t seat_tag_index;
  int *parent_obj;
  char *unit_tag;
  char *seat_element;
  int16_t base_seat;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);

  parent_handle = *(int *)(unit + 0xcc);
  if (parent_handle != -1 && *(int16_t *)(unit + 0x2a0) != -1) {
    parent_obj = (int *)object_get_and_verify_type(parent_handle, 3);
    unit_tag = (char *)tag_get(0x756e6974, *parent_obj);
    seat_tag_index = *(int16_t *)(unit + 0x2a0);
    seat_element = (char *)tag_block_get_element(unit_tag + 0x2e4,
                                                 (int)seat_tag_index, 0x11c);
    return (int)(seat_element + 4);
  }

  base_seat = (int16_t) * (int8_t *)(unit + 0x257);
  if (base_seat < 0 || base_seat >= NUMBER_OF_UNIT_BASE_SEATS) {
    display_assert(
      "base_seat_index>=0 && base_seat_index<NUMBER_OF_UNIT_BASE_SEATS",
      "c:\\halo\\SOURCE\\units\\units.c", 0x200f, 1);
    system_exit(-1);
  }
  return *(int *)(0x32e484 + base_seat * 4);
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
    unit_detach_weapon(unit_handle, equipment_handle);
    unit->unk_712.value = -1;
  }
}

/* unit_can_enter_seat (0x1ae370)
 *
 * Checks whether a unit can enter a given seat object. Verifies both object
 * types (unit=3, seat=4), retrieves the unit's seat label string via 0x1ae290
 * and the seat object's weapon label string via 0xfae80, then calls 0x1acd70
 * to attempt seat matching. If the match succeeds, dispatches through the game
 * engine vtable (current_game_engine->vtable[0x58]) for engine-specific
 * validation.
 *
 * Returns: true if the seat can be entered, false otherwise.
 */
bool unit_can_enter_seat(int unit_handle, int seat_object_handle)
{
  int seat_label;
  int weapon_label;
  char can_enter;

  object_get_and_verify_type(unit_handle, 3);
  object_get_and_verify_type(seat_object_handle, 4);

  seat_label = unit_get_seat_label(unit_handle);
  weapon_label = (int)weapon_get_label(seat_object_handle);
  can_enter =
    (char)unit_try_animation_state(unit_handle, seat_label, weapon_label, 0);

  if (can_enter != 0) {
    /* 0xa8b30: game engine vtable dispatch */
    can_enter =
      (char)game_engine_allow_weapon_pick_up(unit_handle, seat_object_handle);
  }
  return can_enter != 0;
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

      anim_tag = unit_get_seat_label(unit_handle);
      weapon_tag = (int)weapon_get_label(weapon_handle);
      can_use =
        (char)unit_try_animation_state(unit_handle, anim_tag, weapon_tag, 0);

      if (can_use != 0) {
        /* 0xa8b30: weapon usability callback */
        usable =
          (char)game_engine_allow_weapon_pick_up(unit_handle, weapon_handle);
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

  unit_detach_weapon(unit_handle, weapon_handle);

  cur_index = (int16_t)unit->unk_674;
  unit->unk_680[cur_index].value = -1;
  unit->unk_674 = (uint16_t)-1;
  new_index = unit_next_weapon_index(unit_handle, -1, 0);
  unit->unk_676 = (uint16_t)new_index;

  if (!((bool (*)(int))0xfaf50)(weapon_handle))
    object_delete(weapon_handle);

  return true;
}

/* unit_verify_vectors (0x1af620)
 *
 * Validates 6 directional vectors stored on a unit object:
 *   - unk_468 (offset 0x1D4) — facing vector
 *   - unk_480 (offset 0x1E0) — aiming vector
 *   - unk_516 (offset 0x204) — looking vector
 *   - unk_36/unk_48 (offsets 0x24/0x30) — forward/up from object_data_t
 *   - unk_492 (offset 0x1EC) — additional vector
 *   - unk_528 (offset 0x210) — additional vector
 *
 * Each vector is checked via valid_real_normal3d; additionally the
 * forward/up pair at 0x24/0x30 is checked for perpendicularity via
 * valid_real_normal3d_perpendicular. Returns true only if all checks pass.
 *
 * Register arg: unit_handle passed in EAX.
 *
 * Confirmed: PUSH 0x3 / PUSH EAX -> object_get_and_verify_type.
 * Confirmed: LEA offsets 0x1d4, 0x1e0, 0x204, 0x24, 0x30, 0x1ec, 0x210.
 * Confirmed: CALL 0x21fb0 (valid_real_normal3d) and 0x84a70 (perpendicular
 * check). Confirmed: returns bool (MOV AL,1 / XOR AL,AL).
 */
bool unit_verify_vectors(int unit_handle)
{
  char *obj;

  obj = (char *)object_get_and_verify_type(unit_handle, 3);

  if (!valid_real_normal3d((float *)(obj + 0x1d4)))
    return false;
  if (!valid_real_normal3d((float *)(obj + 0x1e0)))
    return false;
  if (!valid_real_normal3d((float *)(obj + 0x204)))
    return false;
  if (!valid_real_normal3d_perpendicular((float *)(obj + 0x24),
                                         (float *)(obj + 0x30)))
    return false;
  if (!valid_real_normal3d((float *)(obj + 0x1ec)))
    return false;
  if (!valid_real_normal3d((float *)(obj + 0x210)))
    return false;

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

  /* Verify vectors — returns true if all vectors are valid normals. */
  if (unit_verify_vectors(unit_handle))
    return;

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
  if (unit_verify_vectors(unit_handle))
    return;

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

/* unit_reset_weapon_state (0x1b1290)
 *
 * Resets the unit's weapon zoom/ready state. If the unit has an associated
 * player and that player is valid, and the unit's zoom_level is not 0xFF,
 * retrieves the unit's current weapon and plays its zoom-deactivation sound
 * (weapon tag +0x4bc) at scale 1.0. Then clears zoom_level and unk_721 to
 * 0xFF and zeroes unk_760. Finally calls player_clear_aim_assist.
 */
void unit_reset_weapon_state(int unit_handle)
{
  unit_data_t *unit;
  int player_index;
  char *player;
  int weapon_handle;
  weapon_data_t *weapon;
  void *weapon_tag;
  int sound_tag_index;

  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);
  player_index = player_index_from_unit_index(unit_handle);
  if (player_index != -1) {
    player =
      (char *)datum_get(player_data, player_index_from_unit_index(unit_handle));
    if (*(short *)(player + 2) != -1 && unit->zoom_level != 0xFF) {
      unit_data_t *unit2 =
        (unit_data_t *)object_get_and_verify_type(unit_handle, 3);
      weapon_handle = unit_get_weapon(unit_handle, unit2->unk_674);
      if (weapon_handle != -1) {
        weapon = (weapon_data_t *)object_get_and_verify_type(weapon_handle, 4);
        weapon_tag = tag_get(0x77656170, weapon->item.object.tag_index);
        sound_tag_index = *(int *)((char *)weapon_tag + 0x4bc);
        if (sound_tag_index != -1) {
          sound_impulse_start(sound_tag_index, 1.0f);
        }
      }
    }
  }
  unit->zoom_level = 0xFF;
  unit->unk_721 = 0xFF;
  unit->unk_760 = 0;
  player_clear_aim_assist(unit_handle);
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

  seat_index = unit_find_empty_weapon_slot(unit_handle);

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

/* unit_update_weapon_readiness (0x1b1ee0)
 *
 * Transitions the unit's active weapon based on its "next weapon" index
 * (unk_676, offset 0x2A4). If the unit currently holds a weapon (unk_674,
 * offset 0x2A2), attempts to place/stow it via weapon_try_place. On
 * success, detaches the current weapon from the parent, disconnects it
 * from the map, marks it as garbage, re-attaches it to the unit via
 * item_attach_to_unit, and clears unk_674.
 *
 * When unk_674 becomes -1 (no active weapon), looks up the "next" weapon
 * (EBX). If a next weapon exists, resolves its label, looks up the
 * animation state, connects the weapon to the map, attaches it at the
 * appropriate marker, copies unk_676 to unk_674, records game_time in
 * unk_696[weapon_index], and activates the weapon. If no next weapon,
 * uses the "unarmed" animation state and sets unk_674 to -1.
 *
 * Always calls unit_reset_weapon_state at the end.
 *
 * Register arg: unit_handle passed in ESI.
 *
 * Confirmed: PUSH 0x3 / PUSH ESI -> object_get_and_verify_type.
 * Confirmed: XOR ECX,ECX; MOV CX,[EAX+0x2a4] — unk_676.
 * Confirmed: XOR EDX,EDX; MOV DX,[EAX+0x2a2] — unk_674.
 * Confirmed: weapon_try_place at 0xfd360, object_detach_from_parent at
 * 0x1411c0. Confirmed: object_disconnect_from_map at 0x13fd00, FUN_0x13fb30 at
 * 0x13fb30. Confirmed: object_set_garbage at 0x13ffc0, item_attach_to_unit at
 * 0xf69c0. Confirmed: weapon_get_label at 0xfae80, unit_get_seat_label at
 * 0x1ae290. Confirmed: unit_try_animation_state at 0x1acd70. Confirmed:
 * object_connect_to_map at 0x140ce0, object_attach_to_marker at 0x144860.
 * Confirmed: weapon_activate at 0xfd2e0, unit_reset_weapon_state at 0x1b1290.
 * Confirmed: game_time_get at 0xb5aa0.
 * Confirmed: "unarmed" string at 0x2b6e68.
 */
void unit_update_weapon_readiness(int unit_handle, int flag)
{
  char *unit;
  char *unit_tag;
  int next_weapon_handle;
  int cur_weapon_handle;
  int seat_label;
  int weapon_label;
  char *antr_tag;
  char *mode_element;
  char *sub_anim;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  unit_tag = (char *)tag_get(0x756e6974, *(int *)unit);

  /* Resolve next weapon from unk_676 */
  {
    char *u2 = (char *)object_get_and_verify_type(unit_handle, 3);
    uint16_t next_idx = *(uint16_t *)(u2 + 0x2a4);
    next_weapon_handle = unit_get_weapon(unit_handle, (int16_t)next_idx);
  }

  /* Resolve current weapon from unk_674 */
  {
    char *u3 = (char *)object_get_and_verify_type(unit_handle, 3);
    uint16_t cur_idx = *(uint16_t *)(u3 + 0x2a2);
    cur_weapon_handle = unit_get_weapon(unit_handle, (int16_t)cur_idx);
  }

  /* Try to place/stow the current weapon */
  if (cur_weapon_handle != -1) {
    if (weapon_try_place(cur_weapon_handle, flag)) {
      object_detach_from_parent(cur_weapon_handle);
      object_disconnect_from_map(cur_weapon_handle);
      object_activate(cur_weapon_handle);
      object_set_garbage(cur_weapon_handle, 0);
      item_attach_to_unit(cur_weapon_handle, unit_handle);
      *(uint16_t *)(unit + 0x2a2) = (uint16_t)-1;
    }
  }

  /* If no active weapon, transition to next or unarmed */
  if (*(int16_t *)(unit + 0x2a2) == -1) {
    if (next_weapon_handle != -1) {
      weapon_label = (int)weapon_get_label(next_weapon_handle);
      seat_label = unit_get_seat_label(unit_handle);
      unit_try_animation_state(unit_handle, seat_label, weapon_label, 1);

      /* Look up animation sub-element for weapon attachment markers */
      antr_tag = (char *)tag_get(0x616e7472, *(int *)(unit_tag + 0x44));
      mode_element = (char *)tag_block_get_element(
        antr_tag + 0xc, (int)*(int8_t *)(unit + 0x250), 0x64);
      sub_anim = (char *)tag_block_get_element(
        mode_element + 0x58, (int)*(int8_t *)(unit + 0x251), 0xbc);

      /* Connect weapon to map and attach at markers */
      object_connect_to_map(next_weapon_handle, 0);
      object_set_garbage(next_weapon_handle, 1);
      object_attach_to_marker(unit_handle, sub_anim + 0x40, next_weapon_handle,
                              sub_anim + 0x20);

      /* Copy next weapon index to current */
      {
        uint16_t next_idx = *(uint16_t *)(unit + 0x2a4);
        *(uint16_t *)(unit + 0x2a2) = next_idx;
        if (next_idx != (uint16_t)-1) {
          int16_t cur = *(int16_t *)(unit + 0x2a2);
          ((int *)(unit + 0x2b8))[(int)cur] = game_time_get();
        }
      }

      weapon_activate(next_weapon_handle);
      unit_reset_weapon_state(unit_handle);
      return;
    }

    /* No next weapon — use "unarmed" animation */
    seat_label = unit_get_seat_label(unit_handle);
    unit_try_animation_state(unit_handle, seat_label, (int)"unarmed", 1);
    *(uint16_t *)(unit + 0x2a2) = (uint16_t)-1;
  }

  unit_reset_weapon_state(unit_handle);
}

/* unit_board_vehicle (0x1b2b80)
 *
 * Handles a unit boarding a vehicle at a specific seat. Validates the seat via
 * unit_find_nearby_seat, attaches the unit to the vehicle at the seat's marker,
 * sets up weapon state and animations for the boarding sequence. Asserts that
 * the unit is not already parented. If the unit's tag has a boarding animation
 * (animation graph entry index > 7 and valid boarding animation), plays it.
 * Returns true if the boarding succeeds, false if the seat check fails.
 */
bool unit_board_vehicle(int unit_handle, int vehicle_handle, int16_t seat_index)
{
  unit_data_t *unit;
  unit_data_t *vehicle_unit;
  void *unit_tag;
  void *seat_def;
  void *marker_name;
  vector3_t unit_pos;
  char markers[0x90]; /* marker output buffer */
  vector3_t delta;
  int weapon_handle;
  char *weapon_label;
  void *anim_tag;
  void *anim_entry;
  int16_t boarding_anim_index;
  int anim_result;

  if (!unit_find_nearby_seat(unit_handle, vehicle_handle, seat_index, 0))
    return false;

  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);
  vehicle_unit = (unit_data_t *)object_get_and_verify_type(vehicle_handle, 3);

  unit_tag = tag_get(0x756e6974, vehicle_unit->object.tag_index);
  seat_def =
    tag_block_get_element((char *)unit_tag + 0x2e4, (int)seat_index, 0x11c);

  if (unit->object.parent_object_index.value != -1) {
    display_assert("unit->object.parent_object_index==NONE",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x1095, 1);
    system_exit(-1);
  }

  /* Get unit world position */
  object_get_world_position(unit_handle, &unit_pos);

  /* Find the seat marker on the vehicle */
  marker_name = (char *)seat_def + 0x24;
  object_get_markers_by_string_id(vehicle_handle, marker_name, markers, 1);

  /* Compute position delta: unit_pos - marker_pos */
  /* Marker position is at offset 0x60 in the marker output buffer */
  delta.x = unit_pos.x - *(float *)(markers + 0x60);
  delta.y = unit_pos.y - *(float *)(markers + 0x64);
  delta.z = unit_pos.z - *(float *)(markers + 0x68);

  /* Transform delta through marker's rotation matrix (at offset 0x38) */
  real_matrix3x3_transform_vector(markers + 0x38, &delta, &delta);

  /* Attach unit to vehicle at seat marker */
  object_attach_to_marker(vehicle_handle, marker_name, unit_handle,
                          (void *)0x25386f);

  /* Set seat index and parent */
  unit->unk_672 = seat_index;
  unit->object.parent_object_index.value = vehicle_handle;

  /* Update unit seat occupancy tracking. */
  unit_update_seat_occupancy(vehicle_handle);

  /* Re-fetch unit data after potential reallocation */
  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);

  /* Set next weapon index */
  unit->unk_676 = unit_next_weapon_index(unit_handle, unit->unk_674, 0);

  unit_update_weapon_readiness(unit_handle, 1);

  /* Get current weapon */
  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);
  weapon_handle = unit_get_weapon(unit_handle, (int16_t)unit->unk_674);

  if (weapon_handle == -1)
    weapon_label = "unarmed";
  else
    weapon_label = weapon_get_label(weapon_handle);

  if (!unit_try_animation_state(unit_handle, (int)((char *)seat_def + 4),
                                (int)weapon_label, 1)) {
    /* Retry with NULL weapon label */
    unit_try_animation_state(unit_handle, (int)((char *)seat_def + 4), 0, 1);
  }

  /* Check for boarding animation in the unit's animation graph */
  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);
  unit_tag = tag_get(0x756e6974, unit->object.tag_index);
  anim_tag = tag_get(0x616e7472, *(uint32_t *)((char *)unit_tag + 0x44));
  anim_entry = tag_block_get_element((char *)anim_tag + 0xc,
                                     (int)(int8_t)unit->unk_592, 100);

  if (*(int *)((char *)anim_entry + 0x40) > 7) {
    boarding_anim_index =
      *(int16_t *)(*(int *)((char *)anim_entry + 0x44) + 0xe);
    if (boarding_anim_index != -1) {
      int anim_graph_tag_index;

      /* Set interpolation */
      object_set_region_count(unit_handle, 6);

      /* Choose random boarding animation */
      anim_graph_tag_index = *(uint32_t *)((char *)unit_tag + 0x44);
      anim_result = model_animation_choose_random(1, anim_graph_tag_index,
                                                  boarding_anim_index);

      /* Set unit animation graph, index, and reset frame counter */
      unit_set_animation(unit_handle, anim_graph_tag_index,
                         (int16_t)anim_result);

      /* Set animation state byte */
      *((uint8_t *)unit + 0x253) = 0x1a;

      /* Adjust interpolation position with the delta */
      object_adjust_interpolation_position(unit_handle, &delta);

      /* Recursively update child object positions */
      object_update_children_recursive(unit_handle);
    }
  }

  unit_vehicle_board_notify(unit_handle, vehicle_handle);
  unit_reset_weapon_state(unit_handle);
  return true;
}
