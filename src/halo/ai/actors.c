void *FUN_0003a600(short actor_type /* @<ax> */)
{
  void **actor_type_definitions = (void **)0x2c86a8;

  if (actor_type < 0 || actor_type > 0xf) {
    display_assert("actor_type>=0 && actor_type<NUMBER_OF_ACTOR_TYPES",
                   "c:\\halo\\source\\ai\\actor_type_definitions.h", 0x2e, 1);
    system_exit(-1);
  }
  if (actor_type_definitions[actor_type] == 0) {
    display_assert("actor_type_definitions[actor_type]",
                   "c:\\halo\\source\\ai\\actor_type_definitions.h", 0x2f, 1);
    system_exit(-1);
  }
  char *def = (char *)actor_type_definitions[actor_type];
  if (*(int *)(def + 0) == 0) {
    display_assert("actor_type_definitions[actor_type]->name",
                   "c:\\halo\\source\\ai\\actor_type_definitions.h", 0x32, 1);
    system_exit(-1);
  }
  if (*(int *)(def + 0x14) == 0) {
    display_assert("actor_type_definitions[actor_type]->decide_action",
                   "c:\\halo\\source\\ai\\actor_type_definitions.h", 0x33, 1);
    system_exit(-1);
  }
  if (*(short *)(def + 6) > 2) {
    display_assert("actor_type_definitions[actor_type]->when_to_search_at_"
                   "target < NUMBER_OF_ACTOR_PURSUIT_SETTINGS",
                   "c:\\halo\\source\\ai\\actor_type_definitions.h", 0x35, 1);
    system_exit(-1);
  }
  if (*(short *)(def + 8) > 2) {
    display_assert("actor_type_definitions[actor_type]->when_to_pursue < "
                   "NUMBER_OF_ACTOR_PURSUIT_SETTINGS",
                   "c:\\halo\\source\\ai\\actor_type_definitions.h", 0x35, 1);
    system_exit(-1);
  }
  if (*(short *)(def + 10) > 2) {
    display_assert("actor_type_definitions[actor_type]->when_to_search_pursuit "
                   "< NUMBER_OF_ACTOR_PURSUIT_SETTINGS",
                   "c:\\halo\\source\\ai\\actor_type_definitions.h", 0x37, 1);
    system_exit(-1);
  }
  return actor_type_definitions[actor_type];
}

void FUN_0003a740(void)
{
  short i;
  for (i = 0; i < 0x10; i++) {
    FUN_0003a600(i);
  }
}

/* actors.c — AI actor/swarm data lifecycle.
 *
 * Corresponds to actors.obj (XBE address range ~0x3a990–0x3aab7).
 * Implements actors_dispose_from_old_map. Binary strings at
 * FUN_0003a990 confirm the source path "c:\halo\SOURCE\ai\actors.c"
 * and the three global data tables: actor_data (name "actor",
 * 0x100 entries * 0x724 bytes), swarm_data (name "swarm", 0x20 *
 * 0x98), and swarm_component_data (name "swarm component", 0x100 *
 * 0x40). actors_dispose (0x3aa50) is a single-RET stub in the
 * original binary — empty function, no operation.
 */

/* FUN_0003a990 (0x3a990)
 * Allocate actor_data, swarm_data, and swarm_component_data tables
 * via game_state_data_new. Asserts each allocation succeeded. */
void FUN_0003a990(void)
{
  FUN_0003a740();
  actor_data = game_state_data_new("actor", 0x100, 0x724);
  assert_halt(actor_data);
  swarm_data = game_state_data_new("swarm", 0x20, 0x98);
  assert_halt(swarm_data);
  swarm_component_data = game_state_data_new("swarm component", 0x100, 0x40);
  assert_halt(swarm_component_data);
}

/* actors_dispose: empty stub in the original binary (single RET).
 * Confirmed: 0x3aa50 contains only a RET instruction. */
void actors_dispose(void)
{
}

/* FUN_0003aa60 (0x3aa60)
 * Delete all entries from the three actor data tables. */
void FUN_0003aa60(void)
{
  data_delete_all(actor_data);
  data_delete_all(swarm_data);
  data_delete_all(swarm_component_data);
}

/* actors_dispose_from_old_map: invalidate the three actor data
 * tables when leaving a map. Calls data_make_invalid on
 * actor_data, swarm_data, swarm_component_data in that order.
 * Confirmed: three MOV r32,[global] / PUSH r32 / CALL 0x119550
 * sequences followed by ADD ESP,0xC / RET. */
void actors_dispose_from_old_map(void)
{
  data_make_invalid(actor_data);
  data_make_invalid(swarm_data);
  data_make_invalid(swarm_component_data);
}

/* FUN_0003b270 (0x3b270)
 * Get the current weapon handle for an actor. First checks if the actor has
 * a held-weapon unit (offset 0x158, guarded by byte at 0x161). If that path
 * yields a weapon, return it immediately. Otherwise falls back to the actor's
 * own unit (offset 0x18), checking the actor variant tag ('actv' at offset
 * 0x5c) for flag 0x40 — if clear, returns the weapon from that unit.
 * Returns -1 if no weapon found.
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x3b280.
 * Confirmed: object_get_and_verify_type with type_mask 3 at 0x3b2a5, 0x3b2ee.
 * Confirmed: unit_get_weapon at 0x3b2bb, 0x3b2ff.
 * Confirmed: tag_get(0x61637476, actor+0x5c) at 0x3b2d9.
 * Confirmed: first path uses XOR EDX,EDX; MOV DX (zero-extend) for
 * weapon_index. Confirmed: second path uses MOVSX (sign-extend) for
 * weapon_index. */
int FUN_0003b270(int actor_handle)
{
  char *actor;
  char *unit;
  int result;

  actor = (char *)datum_get(actor_data, actor_handle);
  result = -1;

  if (*(char *)(actor + 0x161) != 0 && *(int *)(actor + 0x158) != -1) {
    unit = (char *)object_get_and_verify_type(*(int *)(actor + 0x158), 3);
    result = unit_get_weapon(*(int *)(actor + 0x158),
                             (int)(uint16_t)(*(int16_t *)(unit + 0x2a2)));
    if (result != -1) {
      return result;
    }
  }

  if (*(int *)(actor + 0x18) != -1) {
    char *actv = (char *)tag_get(0x61637476, *(int *)(actor + 0x5c));
    if ((*(unsigned char *)actv & 0x40) == 0) {
      unit = (char *)object_get_and_verify_type(*(int *)(actor + 0x18), 3);
      return unit_get_weapon(*(int *)(actor + 0x18),
                             (int)(*(int16_t *)(unit + 0x2a2)));
    }
  }

  return result;
}

bool FUN_0003b320(int actor_handle)
{
  char *actor = (char *)datum_get(actor_data, actor_handle);
  int weapon_handle = FUN_0003b270(actor_handle);
  bool has_weapon = (weapon_handle != -1);
  if (has_weapon && *(int *)(actor + 0x18) != -1) {
    char *unit = (char *)object_get_and_verify_type(*(int *)(actor + 0x18), 3);
    if (*(unsigned char *)(unit + 0xb7) & 1) {
      return false;
    }
  }
  return has_weapon;
}

/* FUN_0003b7e0 (0x3b7e0)
 * Reset a unit's control state. Builds a default unit_control_t (0x40 bytes):
 * animation_state=1, aiming_speed=1, control_flags=0, weapon/grenade/zoom=-1,
 * throttle from global zero vector, then fills facing/aiming/looking vectors
 * from the unit's current state. Applies via unit_set_control and FUN_001adf10.
 *
 * Confirmed: csmemset(&control, 0, 0x40) at 0x3b7ee.
 * Confirmed: global zero vector ptr at [0x31fc38] copied to throttle.
 * Confirmed: ESI = unit_handle (register arg), stack param = actor_handle
 * (unused). Confirmed: FUN_001a9960(ESI, &facing) at 0x3b82c. Confirmed:
 * FUN_001a9900(ESI, &aiming) at 0x3b836. Confirmed: FUN_001a9930(ESI, &looking)
 * at 0x3b840. Confirmed: unit_set_control(ESI, &control) at 0x3b84a. Confirmed:
 * FUN_001adf10(ESI, 0) at 0x3b852. */
void FUN_0003b7e0(int actor_handle, int unit_handle /* @<esi> */)
{
  char control[0x40];
  float *global_origin;

  csmemset(control, 0, 0x40);

  global_origin = *(float **)0x31fc38;

  /* animation_state = 1, aiming_speed = 1 */
  *(char *)(control + 0x00) = 1;
  *(char *)(control + 0x01) = 1;

  /* control_flags = 0 */
  *(int16_t *)(control + 0x02) = 0;

  /* weapon_index = -1, grenade_index = -1, zoom_level = -1 */
  *(int16_t *)(control + 0x04) = -1;
  *(int16_t *)(control + 0x06) = -1;
  *(int16_t *)(control + 0x08) = -1;

  /* throttle = global zero vector */
  *(float *)(control + 0x0c) = global_origin[0];
  *(float *)(control + 0x10) = global_origin[1];
  *(float *)(control + 0x14) = global_origin[2];

  /* Fill facing, aiming, looking vectors from unit's current state */
  FUN_001a9960(unit_handle, control + 0x1c);
  FUN_001a9900(unit_handle, control + 0x28);
  FUN_001a9930(unit_handle, control + 0x34);

  /* Apply the control and update weapon state */
  unit_set_control(unit_handle, control);
  FUN_001adf10(unit_handle, 0);
}

/* FUN_0003b860 (0x3b860)
 * Reset control state for an actor's unit(s). For non-swarm actors (byte at
 * actor+6 == 0), resets the single unit at actor+0x18. For swarm actors,
 * iterates over swarm components and resets each unit. Sets actor+7 = 1
 * when done (marks as control-reset).
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x3b870.
 * Confirmed: actor+6 test at 0x3b87d (JZ to simple path).
 * Confirmed: datum_get(swarm_data, actor+0x28) at 0x3b895.
 * Confirmed: loop counter is 16-bit (DI), compared against word at swarm+2.
 * Confirmed: FUN_0003b7e0 called with ESI=unit_handle, stack=actor_handle.
 * Confirmed: actor+7 set to 1 at all exit paths. */
void FUN_0003b860(int actor_handle)
{
  char *actor;
  char *swarm;
  short i;

  actor = (char *)datum_get(actor_data, actor_handle);

  if (*(char *)(actor + 6) == 0) {
    /* Non-swarm: reset the single unit */
    FUN_0003b7e0(actor_handle, *(int *)(actor + 0x18));
  } else if (*(int *)(actor + 0x28) != -1) {
    /* Swarm: iterate over swarm components */
    swarm = (char *)datum_get(swarm_data, *(int *)(actor + 0x28));
    for (i = 0; i < *(short *)(swarm + 2); i++) {
      FUN_0003b7e0(actor_handle, *(int *)(swarm + 0x18 + (int)i * 4));
    }
  }

  *(char *)(actor + 7) = 1;
}

void FUN_0003b900(void)
{
  char iter[0x1c];
  FUN_00059b10(iter, 1);
  while (FUN_00059b50(iter)) {
    FUN_0003b860(*(int *)(iter + 0x14));
  }
}

/* FUN_0003b940 (0x3b940)
 * Idle-update a unit's control state. Builds a default unit_control_t (0x40
 * bytes): animation_state=3, aiming_speed=1, control_flags=0,
 * weapon/grenade/zoom=-1, throttle from global forward vector. Gets the
 * unit's current facing via FUN_001a9960, then rotates it 30 degrees around
 * the up axis via rotate_vector3d_by_sincos. Copies the rotated facing to
 * aiming and looking. Every 5th tick (based on game_time_get() +
 * unit_object_index mod 5), sets control_flags |= 0x0800 and
 * primary_trigger = 1.0f. Applies via unit_set_control and FUN_001adf10.
 *
 * Confirmed: csmemset(&control, 0, 0x40) at 0x3b94e.
 * Confirmed: global forward vector ptr at [0x31fc3c] copied to throttle.
 * Confirmed: ESI = unit_object_index (register arg), stack param = actor_handle
 * (unused). Confirmed: FUN_001a9960(ESI, &facing) at 0x3b98c. Confirmed:
 * rotate_vector3d_by_sincos(&facing, *(float**)0x31fc44, 0.5f, 0.866f) at
 * 0x3b9a5. Confirmed: facing copied to aiming and looking at 0x3b9b6-0x3b9c5.
 * Confirmed: game_time_get() at 0x3b9c8, (result+ESI)%5==0 triggers flag set.
 * Confirmed: unit_set_control(ESI, &control) at 0x3b9eb. Confirmed:
 * FUN_001adf10(ESI, 0) at 0x3b9f3. */
void FUN_0003b940(int actor_handle, int unit_object_index /* @<esi> */)
{
  char control[0x40];
  float *global_forward;
  float *up_axis;

  csmemset(control, 0, 0x40);

  global_forward = *(float **)0x31fc3c;

  /* animation_state = 3, aiming_speed = 1 */
  *(char *)(control + 0x00) = 3;
  *(char *)(control + 0x01) = 1;

  /* control_flags = 0 */
  *(int16_t *)(control + 0x02) = 0;

  /* weapon_index = -1, grenade_index = -1, zoom_level = -1 */
  *(int16_t *)(control + 0x04) = -1;
  *(int16_t *)(control + 0x06) = -1;
  *(int16_t *)(control + 0x08) = -1;

  /* throttle = global forward vector */
  *(float *)(control + 0x0c) = global_forward[0];
  *(float *)(control + 0x10) = global_forward[1];
  *(float *)(control + 0x14) = global_forward[2];

  /* Get unit's current facing vector */
  FUN_001a9960(unit_object_index, control + 0x1c);

  /* Rotate facing 30 degrees around the up axis */
  up_axis = *(float **)0x31fc44;
  rotate_vector3d_by_sincos((float *)(control + 0x1c), up_axis, 0.5f,
                            0.866025388f);

  /* Copy rotated facing to aiming and looking */
  *(float *)(control + 0x28) = *(float *)(control + 0x1c);
  *(float *)(control + 0x2c) = *(float *)(control + 0x20);
  *(float *)(control + 0x30) = *(float *)(control + 0x24);
  *(float *)(control + 0x34) = *(float *)(control + 0x1c);
  *(float *)(control + 0x38) = *(float *)(control + 0x20);
  *(float *)(control + 0x3c) = *(float *)(control + 0x24);

  /* Every 5th tick, fire the weapon */
  if ((game_time_get() + unit_object_index) % 5 == 0) {
    *(int16_t *)(control + 0x02) |= 0x0800;
    *(float *)(control + 0x18) = 1.0f;
  }

  /* Apply the control and update weapon state */
  unit_set_control(unit_object_index, control);
  FUN_001adf10(unit_object_index, 0);
}

/* FUN_0003ba00 (0x3ba00) — actors_idle_update
 *
 * Iterates all active actors via the standard iterator
 * (FUN_00059b10/FUN_00059b50). For each actor record:
 *   - If record->field_6 == 0 (non-swarm): calls FUN_0003b940 once with the
 *     actor's unit object index from record->field_18.
 *   - If record->field_6 != 0 and record->field_28 (swarm handle) is valid:
 *     looks up the swarm via datum_get(swarm_data, swarm_handle), then iterates
 *     over swarm->count (field_2) entries, calling FUN_0003b940 for each
 *     component's unit object index from the swarm array at offset 0x18.
 *
 * Confirmed: called only from ai_update (0x41180) with no arguments.
 * Confirmed: iter+0x14 ([EBP-0x8]) holds the actor datum handle.
 * Confirmed: ESI is set before each call to FUN_0003b940 (register arg).
 * Confirmed: FUN_0003b940 stack arg is iter+0x14 (actor datum handle).
 * Confirmed: inner loop counter is 16-bit (DI), compared against word at
 * swarm+2.
 */
void FUN_0003ba00(void)
{
  char iter[0x1c];
  char *record;
  int actor_handle;

  FUN_00059b10(iter, 1);
  record = (char *)FUN_00059b50(iter);
  while (record != NULL) {
    actor_handle = *(int *)(iter + 0x14);
    if (*(char *)(record + 6) == 0) {
      /* Non-swarm actor: single unit object index at record+0x18 */
      FUN_0003b940(actor_handle, *(int *)(record + 0x18));
    } else if (*(int *)(record + 0x28) != -1) {
      /* Swarm actor: iterate over swarm component entries */
      char *swarm = (char *)datum_get(swarm_data, *(int *)(record + 0x28));
      short i;
      for (i = 0; i < *(short *)(swarm + 2); i++) {
        FUN_0003b940(actor_handle, *(int *)(swarm + 0x18 + i * 4));
      }
    }
    record = (char *)FUN_00059b50(iter);
  }
}

/* FUN_0003d950 (0x3d950) — actor_erase_units
 *
 * Erase all units owned by an actor. For swarm actors (byte at actor+6 != 0),
 * iterates the linked-list of unit handles starting at actor+0x24, detaching
 * each unit via FUN_0003ae60 and deleting it. For non-swarm actors, handles
 * the single unit at actor+0x18 via FUN_0003cff0. The flag parameter controls
 * whether units are deleted with object_delete (flag=0) or FUN_00144b30
 * (flag!=0, a softer detach-and-delete path).
 *
 * Classification evidence: callee FUN_0003ae60 references actors.c asserts
 * at 0x3aeab/0x3af05/0x3af32/0x3af6d. Callee FUN_0003cc10 references actors.c
 * assert at 0x3cc40. Callee FUN_0003cff0 calls FUN_0003cc10. Caller
 * FUN_0003d9f0 references actors.c string at 0x3da76. All confirm actors.c TU.
 *
 * Confirmed: cdecl, two stack args (actor_handle, flag).
 * Confirmed: datum_get(actor_data, actor_handle) at 0x3d95f.
 * Confirmed: swarm check at actor+6 (0x3d966), branch at 0x3d96e.
 * Confirmed: loop reads actor+0x24 each iteration (0x3d971, 0x3d99e).
 * Confirmed: FUN_0003ae60(actor_handle, unit_handle) at 0x3d982.
 * Confirmed: flag test at 0x3d98d selects object_delete vs FUN_00144b30.
 * Confirmed: FUN_0003cc10(actor_handle, 1) at 0x3d9ac.
 * Confirmed: non-swarm path loads actor+0x18 into EDI at 0x3d9b9.
 * Confirmed: FUN_0003cff0(actor_handle) at 0x3d9bd. */
void FUN_0003d950(int actor_handle, char flag)
{
  char *actor;
  int unit_handle;

  actor = (char *)datum_get(actor_data, actor_handle);

  if (*(char *)(actor + 0x6) != 0) {
    /* Swarm actor: detach and delete all units in linked list */
    unit_handle = *(int *)(actor + 0x24);
    while (unit_handle != -1) {
      FUN_0003ae60(actor_handle, unit_handle);
      if (flag != 0) {
        FUN_00144b30(unit_handle);
      } else {
        object_delete(unit_handle);
      }
      unit_handle = *(int *)(actor + 0x24);
    }
    FUN_0003cc10(actor_handle, 1);
    return;
  }

  /* Non-swarm actor: handle single unit */
  unit_handle = *(int *)(actor + 0x18);
  FUN_0003cff0(actor_handle);
  if (flag != 0) {
    FUN_00144b30(unit_handle);
  } else {
    object_delete(unit_handle);
  }
}
