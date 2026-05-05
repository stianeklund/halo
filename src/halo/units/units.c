/* units.c — unit lifecycle and query helpers.
 *
 * Corresponds to units.obj. Functions sorted by XBE address.
 */

#include "../../common.h"

#define NUMBER_OF_UNIT_BASE_SEATS 6

char *FUN_0008dc30(char *destination, const char *source)
{
  const char *source_cursor;
  char *destination_cursor;
  unsigned int source_size;

  if (destination == NULL || source == NULL) {
    display_assert("s1 && s2", "c:\\halo\\SOURCE\\cseries\\cseries.c", 0x122,
                   true);
    system_exit(-1);
  }

  source_cursor = source;
  while (*source_cursor != '\0') {
    source_cursor += 1;
  }
  source_size = (unsigned int)(source_cursor - source) + 1;

  destination_cursor = destination - 1;
  do {
    destination_cursor += 1;
  } while (*destination_cursor != '\0');

  {
    unsigned int i;
    for (i = source_size >> 2; i != 0; i -= 1) {
      *(uint32_t *)destination_cursor = *(const uint32_t *)source;
      source += 4;
      destination_cursor += 4;
    }
  }

  {
    unsigned int i;
    for (i = source_size & 3; i != 0; i -= 1) {
      *destination_cursor = *source;
      source += 1;
      destination_cursor += 1;
    }
  }

  return destination;
}

void FUN_00123470(void *mode_tag, void *animation, int animation_index,
                  void *out_matrix)
{
  uint8_t node_data[0x800];

  FUN_00121d60(mode_tag, animation, animation_index, node_data);
  FUN_001094d0(out_matrix, (float *)(node_data + 0x10), (float *)node_data);
}

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

/* FUN_001a8190 (0x1a8190)
 *
 * Sets persistent control state on a unit. Stores animation_ticks at offset
 * 0x1c0 and control_flags at offset 0x1c4. Asserts if control_flags has any
 * bits set beyond position 14 (NUMBER_OF_UNIT_CONTROL_FLAGS = 15).
 */
void FUN_001a8190(int unit_handle, int animation_ticks, int control_flags)
{
  char *unit = (char *)object_get_and_verify_type(unit_handle, 3);

  if ((control_flags & 0xffff8000) != 0) {
    display_assert(
      "VALID_FLAGS(persistent_control_flags, NUMBER_OF_UNIT_CONTROL_FLAGS)",
      "c:\\halo\\SOURCE\\units\\units.c", 0x605, 1);
    system_exit(-1);
  }

  *(int *)(unit + 0x1c4) = control_flags;
  *(int *)(unit + 0x1c0) = animation_ticks;
}

int unit_get_seat_enter_position(int unit_handle, int target_unit_handle,
                                 int16_t seat_index, float *out_pos_a,
                                 float *out_pos_b, float *out_pos_c)
{
  char marker_name[256];
  uint8_t hint_marker_data[0x6c];
  uint8_t mode_matrix[0x34];
  uint8_t enter_position_matrix[0x34];
  uint8_t seat_marker_data[0xa0];
  char *unit = (char *)object_get_and_verify_type(unit_handle, 3);
  char *unit_tag = (char *)tag_get(0x756e6974, *(int *)unit);
  void *mode_tag = (void *)tag_get(0x6d6f6465, *(int *)(unit_tag + 0x34));
  char *antr_tag = (char *)tag_get(0x616e7472, *(int *)(unit_tag + 0x44));
  char *target_unit = (char *)object_get_and_verify_type(target_unit_handle, 3);
  char *target_unit_tag = (char *)tag_get(0x756e6974, *(int *)target_unit);
  char *seat = (char *)tag_block_get_element(target_unit_tag + 0x2e4,
                                             (int)seat_index, 0x11c);
  int *mode_block = (int *)(antr_tag + 0xc);
  int16_t mode_index = 0;

  while ((int)mode_index < *mode_block) {
    char *mode =
      (char *)tag_block_get_element(mode_block, (int)mode_index, 0x64);

    if (crt_stricmp(mode, seat + 4) == 0) {
      if (*(int *)(mode + 0x40) < 8) {
        return 0;
      }

      mode_index = *(int16_t *)(*(int *)(mode + 0x44) + 0xe);
      if (mode_index == -1) {
        return 0;
      }

      mode =
        (char *)tag_block_get_element(antr_tag + 0x74, (int)mode_index, 0xb4);
      object_get_markers_by_string_id(target_unit_handle, seat + 0x24,
                                      seat_marker_data, 1);
      FUN_00123470(mode_tag, mode, 0, mode_matrix);
      matrix4x3_multiply((float *)(seat_marker_data + 0x38),
                         (float *)mode_matrix, (float *)enter_position_matrix);
      csstrcpy(marker_name, seat + 0x24);
      FUN_0008dc30(marker_name, " enter-hint");
      object_get_markers_by_string_id(target_unit_handle, marker_name,
                                      hint_marker_data, 1);

      if (out_pos_b != NULL) {
        out_pos_b[0] = *(float *)(seat_marker_data + 0x60);
        out_pos_b[1] = *(float *)(seat_marker_data + 0x64);
        out_pos_b[2] = *(float *)(seat_marker_data + 0x68);
      }

      if (out_pos_a != NULL) {
        out_pos_a[0] = *(float *)(enter_position_matrix + 0x28);
        out_pos_a[1] = *(float *)(enter_position_matrix + 0x2c);
        out_pos_a[2] = *(float *)(enter_position_matrix + 0x30);
      }

      if (out_pos_c != NULL) {
        out_pos_c[0] = *(float *)(hint_marker_data + 0x60);
        out_pos_c[1] = *(float *)(hint_marker_data + 0x64);
        out_pos_c[2] = *(float *)(hint_marker_data + 0x68);
      }

      return 1;
    }

    mode_index += 1;
  }

  return 0;
}

/* FUN_001a8990 (0x1a8990)
 *
 * Sets the unit's seated animation state by looking up an animation index from
 * the unit's animation tag hierarchy and calling model_animation_choose_random.
 *
 * When state == 0: clears unk_596 (animation state byte at 0x254) and sets
 * unk_602 (0x25a) to 0xffff (NONE), then returns.
 *
 * For state 1-9: walks the unit tag -> antr tag -> mode element -> sub-element
 * -> weapon-element hierarchy using unk_592/593/594 as indices. Then:
 *
 *   States 1-4, 8: index the animation kind table in the sub-element block
 *     (sub_element+0x98/0x9c). Kind indices: 0x15, 0x16, 0x17, 0x18, 0x14.
 *   States 5-7, 9: index the animation kind table in the weapon-element block
 *     (weapon_element+0x30/0x34). Sub-indices: 0, 1, 8, 9.
 *
 * If a valid animation index is found (DI != -1):
 *   - Calls object_set_region_count(object_handle, 6) unless state == 7.
 *   - Calls model_animation_choose_random(1, antr_tag_index, animation_index).
 *   - Stores result in unk_602 (0x25a), clears unk_604 (0x25c), sets
 *     unk_596 (0x254) to (uint8_t)state.
 *
 * Confirmed: switch jump table at 0x1a8af8 (9 entries for ECX=0-8).
 * Confirmed: MOVSX ECX,byte ptr [ESI+0x250/0x251/0x252] for tag block indices.
 * Confirmed: MOV byte ptr [ESI+0x254],CL; MOV word ptr [ESI+0x25a],AX.
 */
void FUN_001a8990(int object_handle, int16_t state)
{
  unit_data_t *unit;
  char *unit_tag;
  char *antr_tag;
  char *mode_element;
  char *sub_element;
  char *weapon_element;
  int16_t animation_index;
  int anim_kind_idx;
  int anim_sub_idx;

  unit = (unit_data_t *)object_get_and_verify_type(object_handle, 3);

  if (state == 0) {
    unit->unk_596 = 0;
    unit->unk_602 = (uint16_t)0xffff;
    return;
  }

  unit_tag = (char *)tag_get(0x756e6974, unit->object.tag_index);
  antr_tag = (char *)tag_get(0x616e7472, *(int *)(unit_tag + 0x44));
  mode_element = (char *)tag_block_get_element(
    antr_tag + 0xc, (int)(int8_t)unit->unk_592, 0x64);
  sub_element = (char *)tag_block_get_element(mode_element + 0x58,
                                              (int)(int8_t)unit->unk_593, 0xbc);
  weapon_element = (char *)tag_block_get_element(
    sub_element + 0xb0, (int)(int8_t)unit->unk_594, 0x3c);

  animation_index = (int16_t)-1;

  switch (state) {
  case 1:
    anim_kind_idx = 0x15;
    goto lookup_sub;
  case 2:
    anim_kind_idx = 0x16;
    goto lookup_sub;
  case 3:
    anim_kind_idx = 0x17;
    goto lookup_sub;
  case 4:
    anim_kind_idx = 0x18;
    goto lookup_sub;
  case 8:
    anim_kind_idx = 0x14;
    goto lookup_sub;
  lookup_sub:
    if (anim_kind_idx < *(int *)(sub_element + 0x98))
      animation_index =
        *(int16_t *)(*(int *)(sub_element + 0x9c) + anim_kind_idx * 2);
    break;

  case 5:
    anim_sub_idx = 0;
    goto lookup_weapon;
  case 6:
    anim_sub_idx = 1;
    goto lookup_weapon;
  case 7:
    anim_sub_idx = 8;
    goto lookup_weapon;
  case 9:
    anim_sub_idx = 9;
    goto lookup_weapon;
  lookup_weapon:
    if (anim_sub_idx < *(int *)(weapon_element + 0x30))
      animation_index =
        *(int16_t *)(*(int *)(weapon_element + 0x34) + anim_sub_idx * 2);
    break;
  }

  if (animation_index != (int16_t)-1) {
    if (state != 7)
      object_set_region_count(object_handle, 6);
    unit->unk_602 = (int16_t)model_animation_choose_random(
      1, *(int *)(unit_tag + 0x44), animation_index);
    unit->unk_604 = 0;
    unit->unk_596 = (uint8_t)state;
  }
}

/* FUN_001a8b20 (0x1a8b20)
 *
 * Attempts to set the current weapon animation state on a unit by looking up
 * the appropriate animation index from the unit's animation tag data and
 * calling model_animation_choose_random to select the sequence.
 *
 * Resolves the animation graph via the unit tag (group 'unit') and its nested
 * animation data (group 'antr'). Navigates through the weapon's animation mode
 * and weapon-type blocks using per-unit indices (unk_592, unk_593, unk_594).
 *
 * The incoming state code is remapped to an animation table index:
 *   1 -> 4 (fire-1), 2 -> 5 (fire-2), 3 -> 6 (charged-1), 4 -> 7 (charged-2),
 *   5 -> 2 (chamber-1), 6 -> 3 (chamber-2)
 *
 * Early-outs:
 *   - state < unit->unk_597 (already at or past this state)
 *   - unk_595 is in the set of "active" animation states (0x17–0x23, 0x27,
 * 0x29)
 *   - table index out of range or entry == -1 (animation not defined)
 *
 * On success, writes the chosen random animation index into unk_606 (0x25e),
 * clears unk_608 (0x260), and sets unk_597 (0x255) = state.
 *
 * On failure, if developer mode is enabled (DAT_005054fb != 0) and this is a
 * biped (object.type == 0) with no seat (unk_586 == -1), prints a warning:
 *   MISSING: <tag_path> '<state_name> <mode_name>'
 */
void FUN_001a8b20(int object_handle, int16_t state)
{
  unit_data_t *unit;
  int16_t current_state;
  char *unit_tag;
  char *antr_tag;
  void *mode_block;
  void *type_block;
  void *dest_block;
  int anim_table_index;
  int16_t anim_index;
  int16_t chosen;

  unit = (unit_data_t *)object_get_and_verify_type(object_handle, 3);

  /* bail if already at or past this state */
  current_state = (int16_t)(int8_t)unit->unk_597;
  if (state < current_state) {
    return;
  }

  /* bail for specific animation states that are already active */
  switch ((uint8_t)unit->unk_595) {
  case 0x17:
  case 0x18:
  case 0x19:
  case 0x1a:
  case 0x1b:
  case 0x1d:
  case 0x1e:
  case 0x1f:
  case 0x20:
  case 0x21:
  case 0x22:
  case 0x23:
  case 0x27:
  case 0x29:
    return;
  default:
    break;
  }

  /* navigate to the animation destination block for this unit's weapon type */
  unit_tag = tag_get(0x756e6974, *(int *)unit);
  antr_tag = tag_get(0x616e7472, *(int *)((char *)unit_tag + 0x44));
  mode_block = tag_block_get_element((char *)antr_tag + 0xc,
                                     (int)(int8_t)unit->unk_592, 0x64);
  type_block = tag_block_get_element((char *)mode_block + 0x58,
                                     (int)(int8_t)unit->unk_593, 0xbc);
  dest_block = tag_block_get_element((char *)type_block + 0xb0,
                                     (int)(int8_t)unit->unk_594, 0x3c);

  /* remap state code to animation table index */
  anim_table_index = -1;
  switch (state) {
  case 1:
    anim_table_index = 4;
    break;
  case 2:
    anim_table_index = 5;
    break;
  case 3:
    anim_table_index = 6;
    break;
  case 4:
    anim_table_index = 7;
    break;
  case 5:
    anim_table_index = 2;
    break;
  case 6:
    anim_table_index = 3;
    break;
  default:
    goto missing;
  }

  /* look up animation index in the destination block's array */
  if (anim_table_index < *(int *)((char *)dest_block + 0x30)) {
    anim_index =
      *(int16_t *)(*(int *)((char *)dest_block + 0x34) + anim_table_index * 2);
    if (anim_index != -1) {
      chosen = (int16_t)model_animation_choose_random(
        1, *(int *)((char *)unit_tag + 0x44), anim_index);
      unit->unk_606 = chosen;
      unit->unk_608 = 0;
      unit->unk_597 = (uint8_t)state;
      return;
    }
  }

missing:
  /* developer-mode warning: animation not defined */
  if (*(uint8_t *)0x5054fb != 0 && unit->object.type == 0 &&
      (int16_t)unit->unk_586 == -1) {
    const char *state_name = FUN_001205f0((void *)0x322148, anim_table_index);
    const char *tag_path =
      tag_name_strip_path(*(char **)((char *)unit_tag + 0x3c));
    console_warning("MISSING: %s '%s %s'", tag_path, state_name, type_block);
  }
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

/* FUN_001a8e10 (0x1a8e10)
 *
 * Dispatches a unit animation state transition based on an incoming state code.
 * Maps input state values 1-8 to either FUN_001a8b20 (which sets a unit
 * animation transition state with a remapped index) or FUN_001a8990 (which
 * initiates a seat-based animation sequence). The state remapping is:
 *   state 1 -> FUN_001a8b20 with index 1
 *   state 2 -> FUN_001a8b20 with index 2
 *   state 3 -> FUN_001a8b20 with index 5
 *   state 4 -> FUN_001a8b20 with index 6
 *   state 5 -> FUN_001a8990 with index 5
 *   state 6 -> FUN_001a8990 with index 6
 *   state 7 -> FUN_001a8b20 with index 3
 *   state 8 -> FUN_001a8b20 with index 4
 */
void FUN_001a8e10(int object_handle, int16_t state)
{
  switch (state) {
  case 1:
    FUN_001a8b20(object_handle, 1);
    return;
  case 2:
    FUN_001a8b20(object_handle, 2);
    return;
  case 3:
    FUN_001a8b20(object_handle, 5);
    return;
  case 4:
    FUN_001a8b20(object_handle, 6);
    return;
  case 5:
    FUN_001a8990(object_handle, 5);
    return;
  case 6:
    FUN_001a8990(object_handle, 6);
    return;
  case 7:
    FUN_001a8b20(object_handle, 3);
    return;
  case 8:
    FUN_001a8b20(object_handle, 4);
    return;
  }
}

/* 0x1a9200 — get world-space position of the "head" marker on a unit.
 * Thin wrapper: calls object_get_markers_by_string_id for the string at
 * 0x2909e4 ("head"), then extracts XYZ from offset 0x60 in the marker
 * output record. Identical pattern to FUN_001a9520 ("body" marker). */
void FUN_001a9200(int object_handle, float *out_position)
{
  char marker_buf[0x6c];
  object_get_markers_by_string_id(object_handle, (void *)0x2909e4, marker_buf,
                                  1);
  out_position[0] = *(float *)(marker_buf + 0x60);
  out_position[1] = *(float *)(marker_buf + 0x64);
  out_position[2] = *(float *)(marker_buf + 0x68);
}

/* unit_set_seat_state (0x1a9240)
 *
 * Computes a 3D position representing the unit's current seat state and writes
 * it into the caller-supplied float[3].
 *
 * Three major paths:
 *
 * 1. Unit has a parent (parent_object_index != -1):
 *    Copies the parent object's world position (offset 0x0C). If the parent's
 *    object type is 0 or 1 (biped/vehicle) and the unit has a valid seat
 *    definition index (unk_672 != -1), looks up the seat's marker name (at
 *    seat_def + 0x84) and resolves it on the parent via
 *    object_get_markers_by_string_id. For seat type 1, skips if the marker
 *    name byte at seat_def + 0x84 is zero.
 *
 * 2. Unit has no parent and no special flags:
 *    If unit flags byte (0xB6) bit 2 is clear AND object type is 0 (biped),
 *    delegates to FUN_001a1140 with zeroed optional parameters.
 *
 * 3. Unit has no parent but has flags/non-zero type:
 *    If unk_728 is NONE, gets the "head" marker on the unit itself. Otherwise,
 *    resolves unk_728 as a unit, reads its seat index (unk_672), looks up the
 *    seat definition's marker name (seat_def + 0x24) from the original unit's
 *    tag, and resolves it on the original unit via
 * object_get_markers_by_string_id.
 */
void unit_set_seat_state(int unit_handle, float *position)
{
  char *unit;
  char *unit_tag;
  int seat_index;
  char *seat_object;
  char *parent_unit;
  char *seat_def;
  int16_t seat_def_index;
  uint8_t seat_type;
  uint32_t type_mask;
  char marker_buf[0x6c];

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  unit_tag = (char *)tag_get(0x756e6974, *(int *)unit);
  seat_index = *(int *)(unit + 0xcc);

  if (seat_index == -1) {
    /* Unit is not in a seat */
    if (!(*(uint8_t *)(unit + 0xb6) & 0x04) && *(int16_t *)(unit + 0x64) == 0) {
      /* Simple biped with no special flags — delegate to
       * biped_estimate_position */
      biped_estimate_position(unit_handle, 0, (vector3_t *)0,
                              (vector3_t *)0, /* dup-args-ok */
                              (vector3_t *)0, (vector3_t *)position);
      return;
    }

    /* Has flags or non-zero type: check unk_728 */
    if (*(int *)(unit + 0x2d8) == -1) {
      /* No related unit — find "head" marker on this unit */
      object_get_markers_by_string_id(unit_handle, (void *)0x2909e4, marker_buf,
                                      1);
      position[0] = *(float *)(marker_buf + 0x60);
      position[1] = *(float *)(marker_buf + 0x64);
      position[2] = *(float *)(marker_buf + 0x68);
      return;
    }

    /* Related unit exists — get its seat definition */
    parent_unit = (char *)object_get_and_verify_type(*(int *)(unit + 0x2d8), 3);
    seat_def_index = *(int16_t *)(parent_unit + 0x2a0);
    seat_def = (char *)tag_block_get_element(unit_tag + 0x2e4,
                                             (int)seat_def_index, 0x11c);
    object_get_markers_by_string_id(unit_handle, seat_def + 0x24, marker_buf,
                                    1);
    position[0] = *(float *)(marker_buf + 0x60);
    position[1] = *(float *)(marker_buf + 0x64);
    position[2] = *(float *)(marker_buf + 0x68);
    return;
  }

  /* Unit IS in a seat — seat_index is the parent object handle */
  seat_object = (char *)object_get_and_verify_type(seat_index, -1);

  /* Copy seat object's world position */
  position[0] = *(float *)(seat_object + 0x0c);
  position[1] = *(float *)(seat_object + 0x10);
  position[2] = *(float *)(seat_object + 0x14);

  /* Check if seat type is biped (0) or vehicle (1) */
  seat_type = *(uint8_t *)(seat_object + 0x64);
  type_mask = 1 << seat_type;
  if (!(type_mask & 0x03))
    return;

  /* Seat type is 0 or 1 — refine position from seat marker */
  if (*(int16_t *)(unit + 0x2a0) == -1)
    return;

  /* Get the seat definition from the parent's unit tag */
  unit_tag = (char *)tag_get(0x756e6974, *(int *)seat_object);
  seat_def_index = *(int16_t *)(unit + 0x2a0);
  seat_def =
    (char *)tag_block_get_element(unit_tag + 0x2e4, (int)seat_def_index, 0x11c);

  /* For seat type 1 (vehicle), skip if marker name at +0x84 is empty */
  if (*(int16_t *)(seat_object + 0x64) == 1) {
    if (*(uint8_t *)(seat_def + 0x84) == 0)
      return;
  }

  /* Look up the seat marker on the parent object */
  object_get_markers_by_string_id(*(int *)(unit + 0xcc), seat_def + 0x84,
                                  marker_buf, 1);
  position[0] = *(float *)(marker_buf + 0x60);
  position[1] = *(float *)(marker_buf + 0x64);
  position[2] = *(float *)(marker_buf + 0x68);
}

/* unit_impulse_to_animation_kind (0x1a9560)
 *
 * Maps an animation impulse index (0-13) to an animation kind index and an
 * update_kind value. The impulse index selects an animation kind (e.g. 0x1d
 * for "impulse", 0x20 for "melee_attack_long_step", etc.) and writes 3 or 6
 * to *out_update_kind depending on whether the impulse uses a ranged or melee
 * update mode.
 *
 * If impulse_index is out of [0, 13] the function asserts and terminates.
 * If out_update_kind is NULL the write is skipped (TEST EBX,EBX gate).
 *
 * Impulse-to-kind mapping (jump table at 0x1a969c):
 *   0  -> 0x1d  (update 6)   8  -> 0x04  (update 3)
 *   1  -> 0x20  (update 6)   9  -> 0x05  (update 3)
 *   2  -> 0x21  (update 6)   10 -> 0x06  (update 3)
 *   3  -> 0x22  (update 6)   11 -> 0x07  (update 3)
 *   4  -> 0x1b  (update 3)   12 -> 0x28  (update 6)
 *   5  -> 0x1c  (update 3)   13 -> 0x29  (update 6)
 *   6  -> 0x1e  (update 6)
 *   7  -> 0x1f  (update 6)
 *
 * Register args: impulse_index @<ax>, out_update_kind @<ebx>.
 * Returns kind index in AX (int16_t).
 *
 * Confirmed: MOV DI,AX at entry; switch dispatch from 0x1a969c.
 * Confirmed: MOV word ptr [EBX],0x3 or 0x6; MOV AX,SI; RET.
 * Confirmed: assert "animation_impulse>=0 &&
 * animation_impulse<NUMBER_OF_UNIT_ANIMATION_IMPULSES" file
 * "c:\\halo\\SOURCE\\units\\units.c", line 0x14f4.
 */
int16_t unit_impulse_to_animation_kind(int16_t impulse_index,
                                       int16_t *out_update_kind)
{
  static const int16_t kind_table[14] = { 0x1d, 0x20, 0x21, 0x22, 0x1b,
                                          0x1c, 0x1e, 0x1f, 0x04, 0x05,
                                          0x06, 0x07, 0x28, 0x29 };
  /* update 3 for impulses 4,5,8,9,10,11; update 6 for all others. */
  static const int16_t update_table[14] = { 6, 6, 6, 6, 3, 3, 6,
                                            6, 3, 3, 3, 3, 6, 6 };
  int16_t kind;

  if (impulse_index < 0 || impulse_index >= 14) {
    display_assert("animation_impulse>=0 && "
                   "animation_impulse<NUMBER_OF_UNIT_ANIMATION_IMPULSES",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x14f4, 1);
    system_exit(-1);
    return -1;
  }

  kind = kind_table[impulse_index];

  if (out_update_kind != NULL)
    *out_update_kind = update_table[impulse_index];

  return kind;
}

/* unit_animation_state_allows_impulse (0x1a96f0)
 *
 * Returns true if the unit's current animation state (unk_595 at +0x253)
 * is one that permits an animation impulse to be applied.
 *
 * States in [0x17..0x29] that block impulses (switch table 0x1a97a8):
 *   0x17-0x1b, 0x1d-0x1f, 0x20-0x23, 0x27, 0x29 -> return false.
 * States outside [0x17..0x29] -> fall to parent check.
 *
 * If parent_object_index != -1 (unit is seated/mounted):
 *   - If unk_672 (seat-anim index) is -1 -> return false.
 *   - Resolves parent via object_try_and_get_and_verify_type (0x13d640).
 *   - Looks up the seat-anim entry at parent_unit_tag+0x2e4, indexed by
 *     unk_672, element size 0x11c.
 *   - If impulse_index is in [0xc, 0xd]: returns bit 8 of seat_anim[0].
 *   - If impulse_index is outside [0xc, 0xd]: returns false.
 *
 * If parent_object_index == -1 (no parent):
 *   - If impulse_index NOT in [0xc, 0xd] -> return true.
 *   - If impulse_index in [0xc, 0xd] -> return false.
 *
 * Register args: unit_handle @<eax>, impulse_index @<edi> (leaked from caller).
 * Returns bool in AL.
 *
 * Confirmed: PUSH 0x3 / PUSH EAX -> object_get_and_verify_type.
 * Confirmed: MOVSX EAX,byte[ESI+0x253]; ADD -0x17; CMP 0x12; jump table
 * 0x1a97a8. Confirmed: MOVSX ECX,DI at 0x1a976c and 0x1a9786 (DI = caller's EDI
 * = anim_index). Confirmed: CMP [ESI+0x2a0],-1 -> unk_672 check. Confirmed:
 * object_try_and_get_and_verify_type at 0x13d640 / tag_get /
 * tag_block_get_element. Confirmed: MOV EAX,[EAX]; SHR EAX,8; AND AL,1 -> bit 8
 * gate.
 */
bool unit_animation_state_allows_impulse(int unit_handle, int impulse_index)
{
  unit_data_t *unit;
  int state;

  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);
  state = (int)(int8_t)unit->unk_595;

  /* Switch on state - 0x17 for values in [0x17, 0x29] */
  if ((unsigned)(state - 0x17) <= 0x12) {
    switch (state) {
    case 0x17:
    case 0x18:
    case 0x19:
    case 0x1a:
    case 0x1b:
    case 0x1d:
    case 0x1e:
    case 0x1f:
    case 0x20:
    case 0x21:
    case 0x22:
    case 0x23:
    case 0x27:
    case 0x29:
      return false;
    default:
      break;
    }
  }

  /* Check parent (mounted) case */
  if (unit->object.parent_object_index.value != -1) {
    unit_data_t *parent;
    char *parent_tag;
    int *seat_anim;

    if ((int16_t)unit->unk_672 == -1)
      return false;

    parent = (unit_data_t *)object_try_and_get_and_verify_type(
      (int)unit->object.parent_object_index.value, 3);
    if (parent == NULL)
      return false;

    parent_tag = (char *)tag_get(0x756e6974, *(int *)parent);
    seat_anim = (int *)tag_block_get_element(
      parent_tag + 0x2e4, (int)(int16_t)unit->unk_672, 0x11c);

    /* Only impulses 0xc and 0xd can fire when mounted */
    if ((int16_t)impulse_index < 0xc || (int16_t)impulse_index > 0xd)
      return false;

    return (bool)((seat_anim[0] >> 8) & 1);
  }

  /* No parent: impulses 0xc and 0xd are blocked */
  if ((int16_t)impulse_index >= 0xc && (int16_t)impulse_index <= 0xd)
    return false;

  return true;
}

/* FUN_001a9900 (0x1a9900)
 *
 * Copies the unit's aiming vector (unk_492, offset 0x1EC) into the output
 * buffer. Resolves the unit via object_get_and_verify_type with type mask
 * 0x3 (biped | vehicle).
 */
void FUN_001a9900(int unit_handle, void *out_aiming)
{
  char *unit;
  float *out = (float *)out_aiming;

  unit = object_get_and_verify_type(unit_handle, 3);
  out[0] = *(float *)(unit + 0x1ec);
  out[1] = *(float *)(unit + 0x1f0);
  out[2] = *(float *)(unit + 0x1f4);
}

/* FUN_001a9930 (0x1a9930)
 *
 * Copies the unit's looking vector (unk_528, offset 0x210) into the output
 * buffer. Resolves the unit via object_get_and_verify_type with type mask
 * 0x3 (biped | vehicle).
 */
void FUN_001a9930(int unit_handle, void *out_looking)
{
  char *unit;
  float *out = (float *)out_looking;

  unit = object_get_and_verify_type(unit_handle, 3);
  out[0] = *(float *)(unit + 0x210);
  out[1] = *(float *)(unit + 0x214);
  out[2] = *(float *)(unit + 0x218);
}

/* FUN_001a9960 (0x1a9960)
 *
 * Gets the unit's facing vector by delegating to FUN_00141360 (an object-level
 * orientation getter in objects.c). Passes NULL for the up-vector output,
 * requesting only the forward direction.
 */
void FUN_001a9960(int unit_handle, void *out_facing)
{
  FUN_00141360(unit_handle, (float *)out_facing, 0);
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

/* unit_get_equipment (0x1aa970)
 *
 * Returns the equipment datum handle stored in the unit's equipment slot
 * (unit_data_t.unk_712, offset 0x2C8). Resolves the unit via
 * object_get_and_verify_type with type mask 3 (biped | vehicle).
 */
int unit_get_equipment(int unit_handle)
{
  unit_data_t *unit;

  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);
  return unit->unk_712.value;
}

/* unit_try_add_grenade (0x1aa990)
 *
 * Attempts to add a grenade to the unit's inventory. The equipment object
 * must be of powerup_type _equipment_powerup_grenade (6). Looks up the
 * grenade type's maximum count from the game globals tag block at offset
 * 0x128, then checks the unit's current grenade count array at offset 0x2CE.
 * If there is room, increments the count, plays an effect if a local player
 * is carrying the unit, and deletes the equipment object.
 *
 * Returns: true if the grenade was added, false if the unit is already at
 * maximum capacity for that grenade type.
 */
bool unit_try_add_grenade(int unit_handle, int equipment_handle)
{
  int *equipment_obj;
  char *equipment_tag;
  char *unit;
  int16_t grenade_type;
  int16_t max_count;
  char *game_globals;
  char current_count;
  int player_index;
  char *player;

  equipment_obj = (int *)object_get_and_verify_type(equipment_handle, 8);
  equipment_tag = (char *)tag_get(0x65716970, *equipment_obj);
  unit = (char *)object_get_and_verify_type(unit_handle, 3);

  if (*(int16_t *)(equipment_tag + 0x308) != 6) {
    display_assert("equipment_definition->equipment.powerup_type==_equipment_"
                   "powerup_grenade",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x1c72, 1);
    system_exit(-1);
  }

  grenade_type = *(int16_t *)(equipment_tag + 0x30a);
  game_globals = (char *)game_globals_get();
  max_count =
    *(int16_t *)tag_block_get_element(game_globals + 0x128, grenade_type, 0x44);

  if (max_count == 0)
    return false;

  current_count = *(char *)(unit + grenade_type + 0x2ce);
  if ((int16_t)current_count >= max_count)
    return false;

  *(char *)(unit + grenade_type + 0x2ce) = current_count + 1;

  player_index = player_index_from_unit_index(unit_handle);
  if (player_index != -1) {
    player = (char *)datum_get(player_data, player_index);
    if (*(int16_t *)(player + 2) != -1)
      item_activate_equipment_effect(equipment_handle);
  }

  object_delete(equipment_handle);
  return true;
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

/* unit_find_best_enter_seat (0x1ad800)
 *
 * Iterates over the target unit's seat definitions and finds the best seat
 * for the given unit to enter. For each seat, computes entry positions via
 * unit_get_seat_enter_position and checks distances from the unit's world
 * position. Seats are scored based on whether they are empty (state 2) or
 * occupied by a unit that can be approached (state 1).
 *
 * Returns: seat state (0 = none found, 1 = approach occupied seat,
 * 2 = enter empty seat). Writes the chosen seat index through out_seat_index.
 */
uint16_t unit_find_best_enter_seat(int unit_handle, int target_unit_handle,
                                   int16_t *out_seat_index)
{
  char *unit;
  char *target_unit;
  char *target_tag;
  int seat_count;
  int *seat_block;
  int seat_index;
  int best_seat;
  uint16_t best_state;
  float best_distance;
  float distance;
  float distance2;
  float pos_a[3];
  float pos_b[3];
  uint8_t found_flag;

  unit = object_get_and_verify_type(unit_handle, 3);
  target_unit = object_get_and_verify_type(target_unit_handle, 3);
  target_tag = tag_get(0x756e6974, *(int *)target_unit);

  best_state = 0;
  best_seat = -1;
  found_flag = 0;

  if ((*(uint8_t *)(target_unit + 0xb6) & 4) == 0 &&
      (*(uint32_t *)(target_unit + 0x1b4) & 0x10000) == 0) {
    seat_block = (int *)(target_tag + 0x2e4);
    seat_count = *seat_block;
    best_distance = 3.4028235e+38f;

    for (seat_index = 0; seat_index < seat_count; seat_index++) {
      unsigned int *seat_element =
        tag_block_get_element(seat_block, seat_index, 0x11c);

      if (!unit_get_seat_enter_position(unit_handle, target_unit_handle,
                                        (int16_t)seat_index, pos_a, pos_b,
                                        NULL))
        continue;

      distance = sqrtf((*(float *)(unit + 0x50) - pos_b[0]) *
                         (*(float *)(unit + 0x50) - pos_b[0]) +
                       (*(float *)(unit + 0x54) - pos_b[1]) *
                         (*(float *)(unit + 0x54) - pos_b[1]) +
                       (*(float *)(unit + 0x58) - pos_b[2]) *
                         (*(float *)(unit + 0x58) - pos_b[2]));

      distance2 = sqrtf((*(float *)(unit + 0x50) - pos_a[0]) *
                          (*(float *)(unit + 0x50) - pos_a[0]) +
                        (*(float *)(unit + 0x54) - pos_a[1]) *
                          (*(float *)(unit + 0x54) - pos_a[1]) +
                        (*(float *)(unit + 0x58) - pos_a[2]) *
                          (*(float *)(unit + 0x58) - pos_a[2]));

      if (distance2 < distance)
        distance = distance2;

      if (distance >= *(float *)0x2533c8)
        continue;

      if (((*seat_element & 0x200) == 0 ||
           *(int *)(target_unit + 0x2d4) != -1) &&
          *(char *)(seat_element + 1) != 0 &&
          unit_try_animation_state(unit_handle, (int)(seat_element + 1), 0,
                                   0)) {
        int blocking_unit = -1;
        uint16_t seat_state = 0;

        if (!unit_find_nearby_seat(unit_handle, target_unit_handle,
                                   (int16_t)seat_index, &blocking_unit)) {
          if (blocking_unit != -1) {
            char *blocking_obj =
              (char *)object_get_and_verify_type(blocking_unit, 3);
            if (*(int *)(blocking_obj + 0x1a4) != -1 &&
                ai_handle_unit_approach(*(int *)(blocking_obj + 0x1a4),
                                        unit_handle, 0)) {
              seat_state = 1;
            }
          }
        } else {
          seat_state = 2;
        }

        if (seat_state != 0) {
          float threshold = *(float *)0x2533c8;
          if (found_flag && ((*seat_element >> 2) & 1) == 0)
            threshold = *(float *)0x2533ec;

          if (best_seat == -1 || best_state < seat_state ||
              distance * threshold < best_distance) {
            best_distance = distance;
            best_state = seat_state;
            best_seat = seat_index;
            found_flag = ((*seat_element >> 2) & 1);
          }
        }
      }
    }
  }

  if (out_seat_index == NULL) {
    display_assert("parent_seat_index != NULL",
                   "c:\\halo\\SOURCE\\units\\units.c", 0xff8, 1);
    system_exit(-1);
  }

  *out_seat_index = (int16_t)best_seat;
  return best_state;
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

/* FUN_001adf10 (0x1adf10)
 *
 * Reattaches all weapons in the unit's weapon slots (unk_680, offset 0x2A8,
 * 4 entries) and updates the unit's alive/active flags (unk_436, offset 0x1B4).
 *
 * If the unit has an actor (offset 0x1A4), swarm actor (0x1A8), or unk_456
 * (0x1C8), param_2 is forced to 1 (the unit is always considered active).
 *
 * When the unit is active and object flag bit 2 at offset 0xB6 is clear:
 *   - Sets bit 0 (alive) and bit 6 in unk_436
 * Otherwise:
 *   - Clears bit 0 and bit 6 in unk_436
 *
 * Then iterates all 4 weapon slots and calls item_attach_to_unit for each
 * valid weapon handle, and finally tail-calls unit_update_seat_occupancy.
 */
void FUN_001adf10(int unit_handle, char param_2)
{
  char *unit;
  uint32_t flags;
  int *weapon_slots;
  int i;

  unit = object_get_and_verify_type(unit_handle, 3);

  /* Force active if the unit has an actor, swarm actor, or unk_456 */
  if (*(int *)(unit + 0x1a4) != -1 || *(int *)(unit + 0x1a8) != -1 ||
      *(int *)(unit + 0x1c8) != -1) {
    param_2 = 1;
  }

  if ((*(uint8_t *)(unit + 0xb6) & 4) == 0 && param_2 != 0) {
    flags = *(uint32_t *)(unit + 0x1b4);
    *(uint32_t *)(unit + 0x1b4) = flags | 1;
    flags = flags | 0x41;
  } else {
    flags = *(uint32_t *)(unit + 0x1b4);
    *(uint32_t *)(unit + 0x1b4) = flags & 0xfffffffe;
    flags = flags & 0xffffffbe;
  }
  *(uint32_t *)(unit + 0x1b4) = flags;

  /* Reattach all weapons in the unit's weapon slots */
  weapon_slots = (int *)(unit + 0x2a8);
  for (i = 4; i != 0; i--) {
    if (*weapon_slots != -1) {
      item_attach_to_unit(*weapon_slots, unit_handle);
    }
    weapon_slots++;
  }

  unit_update_seat_occupancy(unit_handle);
}

/* unit_current_weapon_is_busy (0x1ae1a0)
 *
 * Gets the weapon handle in the unit's current seat (via unit_get_weapon
 * with the seat's weapon_index at offset 0x2A2), then checks whether the
 * weapon object is in state 2 or 3 at offset 0x211. Returns true if the
 * weapon is in one of those states, false otherwise (including when there
 * is no current weapon).
 */
bool unit_current_weapon_is_busy(int unit_handle)
{
  char *unit;
  int weapon_handle;
  char *weapon;

  unit = object_get_and_verify_type(unit_handle, 3);
  weapon_handle = unit_get_weapon(unit_handle, *(int16_t *)(unit + 0x2a2));
  if (weapon_handle == -1)
    return false;

  weapon = object_get_and_verify_type(weapon_handle, 4);
  if (*(char *)(weapon + 0x211) == 2 || *(char *)(weapon + 0x211) == 3)
    return true;

  return false;
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

/* unit_should_swap_weapon (0x1ae3c0)
 *
 * Checks whether a unit should swap one of its current weapons for the given
 * weapon object. Iterates the unit's 4 weapon slots (offset 0x2A8). If any
 * non-current slot already holds the same weapon tag, returns false. If the
 * current slot holds the same weapon tag, returns true only when the current
 * weapon's ammo is above the threshold at 0x2533c0 AND the new weapon has
 * equal or greater ammo. Otherwise returns true (new weapon type is not
 * already held).
 */
bool unit_should_swap_weapon(int unit_handle, int weapon_handle)
{
  char *unit;
  int *weapon_obj;
  int weapon_tag;
  int current_weapon;
  int seat_index;
  int *weapon_slot;
  bool should_swap;

  unit = object_get_and_verify_type(unit_handle, 3);
  weapon_obj = (int *)object_get_and_verify_type(weapon_handle, 4);
  weapon_tag = *weapon_obj;

  tag_get(0x77656170, weapon_tag);

  current_weapon = unit_get_weapon(unit_handle, *(int16_t *)(unit + 0x2a2));
  if (current_weapon == -1)
    return false;

  should_swap = true;
  seat_index = 0;
  weapon_slot = (int *)(unit + 0x2a8);

  do {
    int slot_weapon = *weapon_slot;
    if (slot_weapon != -1) {
      int *slot_weapon_obj = (int *)object_get_and_verify_type(slot_weapon, 4);
      if (weapon_tag == *slot_weapon_obj) {
        if (seat_index == *(int16_t *)(unit + 0x2a2)) {
          float slot_ammo = *(float *)((char *)slot_weapon_obj + 0x1f0);
          if (slot_ammo > *(float *)0x2533c0) {
            float new_ammo = *(float *)((char *)weapon_obj + 0x1f0);
            if (new_ammo < slot_ammo)
              goto next_seat;
          }
        }
        should_swap = false;
      }
    }
  next_seat:
    seat_index++;
    weapon_slot++;
  } while (seat_index < 4);

  return should_swap;
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

/* unit_apply_alignment_vector (0x1af180)
 *
 * Sets the unit object's facing direction (forward and up vectors) from a
 * 2D alignment vector (x, y) representing a direction in the ground plane,
 * but only if the unit has no parent (is not seated/mounted).
 *
 * Steps:
 *   1. Resolves unit via object_get_and_verify_type (type_mask=3, @<eax>).
 *   2. If parent_object_index != -1 (unit is mounted), returns immediately.
 *   3. Asserts alignment_vector is a valid 2D normal via FUN_00028610.
 *   4. Copies alignment_vector[0] -> unit+0x24 (object forward x),
 *      alignment_vector[1] -> unit+0x28 (object forward y),
 *      0.0f               -> unit+0x2c (object forward z).
 *   5. Loads the canonical "up" vector from the pointer at 0x31fc44 and
 *      writes it to unit+0x30, +0x34, +0x38.
 *   6. Asserts the forward/up pair are valid axes via FUN_00084a70.
 *
 * Register args: unit_handle @<eax>, alignment_vector @<ecx>.
 *
 * Confirmed: PUSH 0x3 / PUSH EAX -> object_get_and_verify_type (0x13d680).
 * Confirmed: MOV EBX,ECX at entry; CMP [ESI+0xcc],-1 -> parent gate.
 * Confirmed: CALL 0x28610 (valid_real_normal2d check on EBX=alignment_vector).
 * Confirmed: MOV ECX,[EBX]; FSTP [ESI+0x28]; MOV [ESI+0x24],ECX; MOV
 * [ESI+0x2c],0. Confirmed: MOV EDX,[0x31fc44]; copies 3 floats to
 * [ESI+0x30..0x38]. Confirmed: assert string "alignment_vector" at 0x2b7234.
 * Confirmed: CALL 0x84a70 (valid_real_vector3d_axes2 check).
 */
void unit_apply_alignment_vector(int unit_handle, float *alignment_vector)
{
  unit_data_t *unit;
  float *up_vector;

  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);

  /* Only apply if the unit is a top-level object (no parent). */
  if (unit->object.parent_object_index.value != -1)
    return;

  /* Assert the 2D alignment vector is a valid normal (valid_real_normal2d). */
  if (!valid_real_normal2d(alignment_vector)) {
    display_assert("assert_valid_real_normal2d(alignment_vector)",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x2482, 1);
    system_exit(-1);
  }

  /* Copy 2D alignment direction into object forward vector (zero z).
   * Confirmed: MOV ECX,[EBX]; FSTP [ESI+0x28]; MOV [ESI+0x24],ECX; MOV
   * [ESI+0x2c],0. Note: store order in binary is y first (FSTP [ESI+0x28]) then
   * x (MOV [ESI+0x24]). Both reads from [EBX] are sourced before any store, so
   * no aliasing concern. */
  *(float *)((char *)unit + 0x24) = alignment_vector[0];
  *(float *)((char *)unit + 0x28) = alignment_vector[1];
  *(float *)((char *)unit + 0x2c) = 0.0f;

  /* Copy the canonical up vector (world up) from the global at 0x31fc44.
   * Confirmed: MOV EDX,[0x31fc44]; copies 3 dwords to [ESI+0x30,+0x34,+0x38].
   */
  up_vector = *(float **)0x31fc44;
  *(float *)((char *)unit + 0x30) = up_vector[0];
  *(float *)((char *)unit + 0x34) = up_vector[1];
  *(float *)((char *)unit + 0x38) = up_vector[2];

  /* Assert forward/up are valid orthogonal axes
   * (valid_real_normal3d_perpendicular). */
  if (!valid_real_normal3d_perpendicular((float *)((char *)unit + 0x24),
                                         (float *)((char *)unit + 0x30))) {
    display_assert("assert_valid_real_vector3d_axes2(forward, up)",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x2486, 1);
    system_exit(-1);
  }
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

/* unit_apply_animation_impulse (0x1b1a20)
 *
 * Attempts to apply an animation impulse to a unit. The impulse is an index
 * in [0, NUMBER_OF_UNIT_ANIMATION_IMPULSES) that maps to an animation kind
 * index and an update_kind via FUN_001a9560. The function:
 *
 *   1. Checks whether the unit's current animation state allows impulses
 *      (FUN_001a96f0 @<eax>=unit_handle). Returns false immediately if not.
 *   2. Resolves unit tag -> antr tag at unit_tag+0x44. Uses the unit's
 *      current mode (unk_592 at +0x250) and sub-anim (unk_593 at +0x251)
 *      to reach the sub-animation block at mode+0x58 (element size 0xbc).
 *   3. Maps the impulse index to an animation kind index (AX) and an
 *      update_kind (written to *out_update_kind) via FUN_001a9560.
 *   4. Looks up the animation kind index in the sub-anim's kind table at
 *      sub_anim+0x98 (count) / sub_anim+0x9c (int16[] ptr). Returns false
 *      if out of range or the slot is -1.
 *   5. Calls object_set_region_count(unit_handle, update_kind) to set
 *      the interpolation mode.
 *   6. Calls model_animation_choose_random(1, antr_tag_idx, kind_anim_idx)
 *      to choose an animation.
 *   7. Calls unit_set_animation(@<eax>=unit_handle, @<edi>=antr_tag_idx,
 *      @<bx>=chosen_anim).
 *   8. Sets unk_584 (0x248) bit 0 and unk_595 (0x253) = 0x1d.
 *   9. If anim_data is non-NULL, the unit type is 0 (biped), and the unit
 *      has no parent, calls FUN_001af180(@<eax>=unit_handle, @<ecx>=anim_data)
 *      to apply an alignment vector.
 *   10. Returns true on success.
 *
 * Register args: FUN_001a96f0 takes unit_handle @<eax>.
 *                FUN_001a9560 takes impulse_index @<ax>, out_update_kind
 * @<ebx>. FUN_001af180 takes unit_handle @<eax>, anim_data @<ecx>.
 *
 * Confirmed: PUSH EBX (unit_handle) / PUSH 0x3 -> object_get_and_verify_type.
 * Confirmed: MOV EAX,EBX -> CALL 0x1a96f0 (register arg).
 * Confirmed: tag_get(0x756e6974, *unit) then tag_get(0x616e7472,
 * unit_tag+0x44). Confirmed: MOVSX EDX,byte ptr [ESI+0x250] (unk_592 mode
 * index). Confirmed: tag_block_get_element(antr+0xc, unk_592, 0x64). Confirmed:
 * MOVSX ECX,byte ptr [ESI+0x251] (unk_593 sub-anim index). Confirmed:
 * tag_block_get_element(mode+0x58, unk_593, 0xbc). Confirmed: MOV
 * EAX,[EBP+0xc]; LEA EBX,[EBP-0xc]; CALL 0x1a9560 (reg args). Confirmed: CMP
 * [local_c+0x98] / ptr at [local_c+0x9c] / word table indexed by AX. Confirmed:
 * object_set_region_count(unit_handle, update_kind). Confirmed: MOV
 * EAX,[EDI+0x44]; PUSH EBX; PUSH EAX; PUSH 0x1 ->
 * model_animation_choose_random. Confirmed: MOV EDI,[EDI+0x44]; CALL 0x1ab7c0
 * (unit_set_animation @<eax>,@<edi>,@<bx>). Confirmed: OR byte ptr
 * [ESI+0x248],0x1; MOV byte ptr [ESI+0x253],0x1d. Confirmed: CMP word ptr
 * [ESI+0x64],0x0 (type field == 0); CMP [ESI+0xcc],-1 (parent_object_index).
 * Confirmed: MOV EAX,[EBP+0x8]; CALL 0x1af180 (FUN_001af180 @<eax>,@<ecx>).
 */
bool unit_apply_animation_impulse(int unit_handle, int anim_index,
                                  void *anim_data)
{
  unit_data_t *unit;
  char *unit_tag;
  char *antr_tag;
  char *mode_elem;
  char *sub_anim;
  int16_t kind_anim_index;
  int16_t update_kind;
  int16_t chosen_anim;
  int antr_tag_index;

  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);

  /* Check if the unit's animation state allows applying an impulse.
   * FUN_001a96f0 takes @<eax>=unit_handle, @<edi>=impulse_index (leaked).
   * Confirmed disassembly: MOV EDI,[EBP+0xc] at 0x1a34, MOV EAX,EBX at 0x1a3c,
   * CALL 0x1a96f0 at 0x1a42 — EDI = anim_index at call time.
   */
  if (!unit_animation_state_allows_impulse(unit_handle, anim_index))
    return false;

  unit_tag = (char *)tag_get(0x756e6974, *(int *)unit);
  antr_tag = (char *)tag_get(0x616e7472, *(int *)(unit_tag + 0x44));

  /* Locate the animation mode element for the unit's current mode (unk_592).
   * antr_tag+0x0c is the mode block; element size 0x64. */
  mode_elem = (char *)tag_block_get_element(antr_tag + 0xc,
                                            (int)(int8_t)unit->unk_592, 0x64);

  /* Locate the sub-animation element for the unit's current sub-anim (unk_593).
   * mode+0x58 is the sub-anim block; element size 0xbc. */
  sub_anim = (char *)tag_block_get_element(mode_elem + 0x58,
                                           (int)(int8_t)unit->unk_593, 0xbc);

  /* Map the impulse index to an animation kind index.
   * FUN_001a9560 takes impulse_index @<ax> and &update_kind @<ebx>;
   * writes update_kind (3 or 6) through the pointer, returns kind index in AX.
   * Confirmed: LEA EBX,[EBP-0xc]; MOV EAX,[EBP+0xc]; CALL 0x1a9560.
   */
  kind_anim_index =
    unit_impulse_to_animation_kind((int16_t)anim_index, &update_kind);

  /* Bounds-check the kind index against the sub-anim's kind table. */
  if (kind_anim_index < 0)
    return false;
  if ((int)kind_anim_index >= *(int *)(sub_anim + 0x98))
    return false;

  /* Index the kind->animation table (int16[] at sub_anim+0x9c). */
  kind_anim_index =
    *(int16_t *)(*(int *)(sub_anim + 0x9c) + (int)kind_anim_index * 2);
  if (kind_anim_index == -1)
    return false;

  /* Set interpolation mode and choose a random animation variant. */
  object_set_region_count(unit_handle, update_kind);

  antr_tag_index = *(int *)(unit_tag + 0x44);
  chosen_anim =
    (int16_t)model_animation_choose_random(1, antr_tag_index, kind_anim_index);

  /* Apply the chosen animation to the unit.
   * unit_set_animation: @<eax>=unit_handle, @<edi>=antr_tag_index,
   * @<bx>=chosen_anim. Confirmed: MOV EDI,[EDI+0x44]; MOV EAX,[EBP+0x8]; CALL
   * 0x1ab7c0.
   */
  unit_set_animation(unit_handle, antr_tag_index, chosen_anim);

  /* Mark animation impulse as active and set state to 0x1d. */
  unit->unk_584 |= 0x1;
  unit->unk_595 = 0x1d;

  /* If anim_data is provided and this is a top-level biped (type==0, no
   * parent), apply the facing alignment vector. FUN_001af180 takes unit_handle
   * @<eax>, anim_data @<ecx>. Confirmed: TEST ECX,ECX (param_3); CMP
   * [ESI+0x64],0; CMP [ESI+0xcc],-1.
   */
  if (anim_data != NULL && unit->object.type == 0 &&
      unit->object.parent_object_index.value == -1) {
    unit_apply_alignment_vector(unit_handle, (float *)anim_data);
  }

  return true;
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
  real_matrix3x3_transform_vector(markers + 0x38, &delta,
                                  &delta); /* dup-args-ok */

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

/* unit_estimate_position (0x1a93e0)
 *
 * Estimates the world-space position for a unit given a desired estimation mode
 * and optional predicted body position. Handles three major paths:
 *
 * 1. Unit has no parent AND flag byte (unit+0xb6) bit 2 is clear AND type==0
 *    (biped): delegate entirely to biped_estimate_position and return.
 *
 * 2. Unit type==0 (biped) AND has a parent AND parent type==1 (vehicle): call
 *    vehicle_get_estimated_position to get the vehicle's predicted position into
 *    a local vector, and use that as the local_body_pos for the delta.
 *    If vehicle_get_estimated_position returns -1 (failed), fall through to path 3.
 *
 * 3. Fallback: call object_get_world_position to get unit's world position as
 *    local_body_pos.
 *
 * After path 2/3: call unit_set_seat_state to compute the unit's current
 * estimated position into out_position, then add the delta
 * (body_position - local_body_pos) to out_position.
 *
 * Confirmed: assert strings "body_position && estimated_position" and
 * "(estimate_mode >= 0) && (estimate_mode < NUMBER_OF_UNIT_ESTIMATE_POSITION_MODES)"
 * reference "c:\halo\SOURCE\units\units.c", lines 0x14b1 and 0x14b2.
 * Confirmed: CMP EAX,-0x1 / JNZ pattern for parent_object_index checks.
 * Confirmed: TEST byte ptr [EDI+0xb6],0x4 for flags check.
 * Confirmed: CMP word ptr [EDI+0x64],0x0 and CMP word ptr [EAX+0x64],0x1 for type checks.
 * Confirmed: FPU tail sequence — FXCH after three FLD/FSUB pairs to reorder x/y.
 */
void unit_estimate_position(int unit_handle, int16_t estimate_mode,
                            vector3_t *body_position, vector3_t *desired_facing,
                            vector3_t *desired_gun_offset,
                            vector3_t *out_position)
{
  char *unit;
  char *parent_obj;
  int parent_handle;
  int result;
  vector3_t local_body_pos;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);

  if (body_position == NULL || out_position == NULL) {
    display_assert("body_position && estimated_position",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x14b1, true);
    system_exit(-1);
  }
  if (estimate_mode < 0 || estimate_mode >= 4) {
    display_assert("(estimate_mode >= 0) && (estimate_mode < "
                   "NUMBER_OF_UNIT_ESTIMATE_POSITION_MODES)",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x14b2, true);
    system_exit(-1);
  }

  parent_handle = *(int *)(unit + 0xcc);

  if (parent_handle == -1 && !(*(uint8_t *)(unit + 0xb6) & 0x4)) {
    /* No parent, no flag: if biped, fully delegate to biped_estimate_position */
    if (*(int16_t *)(unit + 0x64) == 0) {
      biped_estimate_position(unit_handle, estimate_mode, body_position,
                              desired_facing, desired_gun_offset, out_position);
      return;
    }
  } else if (*(int16_t *)(unit + 0x64) == 0 && parent_handle != -1) {
    /* Biped with a parent: if parent is a vehicle, try vehicle position */
    parent_obj = (char *)object_get_and_verify_type(parent_handle, -1);
    if (*(int16_t *)(parent_obj + 0x64) == 1) {
      result = vehicle_get_estimated_position(*(int *)(unit + 0xcc),
                                              &local_body_pos);
      if (result != -1)
        goto apply_delta;
    }
  }

  /* Fallback: use world position as local_body_pos reference */
  object_get_world_position(unit_handle, &local_body_pos);

apply_delta:
  /* Get the unit's current estimated seat position into out_position */
  unit_set_seat_state(unit_handle, (float *)out_position);

  /* Add (body_position - local_body_pos) delta to out_position */
  out_position->x += body_position->x - local_body_pos.x;
  out_position->y += body_position->y - local_body_pos.y;
  out_position->z += body_position->z - local_body_pos.z;
}
