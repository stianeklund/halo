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
    ((void (*)(int))0x140cc0)(unit->unk_712.value);
    unit->unk_712.value = -1;
  }
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
  new_index = ((int16_t(*)(int16_t, int))0x1ae490)(unit->unk_674, 1);

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
  ((void (*)(void))0x1ab990)();

  cur_index = (int16_t)unit->unk_674;
  unit->unk_680[cur_index].value = -1;
  unit->unk_674 = (uint16_t)-1;
  new_index = ((int16_t(*)(int16_t, int))0x1ae490)(-1, 0);
  unit->unk_676 = (uint16_t)new_index;

  if (!((bool (*)(int))0xfaf50)(weapon_handle))
    ((void (*)(int))0x140cc0)(weapon_handle);

  return true;
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
