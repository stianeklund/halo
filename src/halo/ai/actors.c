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

/* FUN_0003a8a0 (0x3a8a0) — actor_swarm_control_dispatch
 *
 * Dispatch the actor-type-specific swarm control function for a given actor.
 * Retrieves the actor datum, reads its actor_type (int16_t at offset 4),
 * looks up the actor type definition, asserts the definition describes a
 * swarm actor (byte at +0xd) and that the swarm_control function pointer
 * (at +0x18) is non-null, then calls it with actor_handle.
 *
 * Assert strings confirm source: c:\halo\SOURCE\ai\actor_types.c, lines
 * 0x8d–0x8e.
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x3a8af.
 * Confirmed: MOV AX,[EAX+4] loads actor_type for @<ax> register call at
 * 0x3a8b4. Confirmed: FUN_0003a600(@<ax>) returns type_def pointer in EAX ->
 * ESI at 0x3a8bb. Confirmed: type_def->swarm (byte at +0xd) tested at
 * 0x3a8c2-0x3a8c7. Confirmed: type_def->swarm_control (int * at +0x18) tested
 * at 0x3a8e9-0x3a8ee. Confirmed: CALL dword ptr [ESI+0x18] dispatches
 * swarm_control(actor_handle) at 0x3a911. */
void FUN_0003a8a0(int actor_handle)
{
  char *actor;
  void *type_def;

  actor = (char *)datum_get(actor_data, actor_handle);
  type_def = FUN_0003a600(*(short *)(actor + 4));

  if (*(char *)((char *)type_def + 0xd) == 0) {
    display_assert("actor_type_definition->swarm",
                   "c:\\halo\\SOURCE\\ai\\actor_types.c", 0x8d, 1);
    system_exit(-1);
  }
  if (*(int *)((char *)type_def + 0x18) == 0) {
    display_assert("actor_type_definition->swarm_control",
                   "c:\\halo\\SOURCE\\ai\\actor_types.c", 0x8e, 1);
    system_exit(-1);
  }

  (*(void (*)(int)) * (int *)((char *)type_def + 0x18))(actor_handle);
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

/* FUN_0003ad80 (0x3ad80) — actor_clear_unit
 *
 * Clear the unit reference from an actor. Sets unit flags, clears the unit's
 * actor_index, decrements the encounter's unique_leader_count if the actor
 * was a unique leader, and finally clears actor->unit_handle and the unique
 * leader flag.
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x3ad8f.
 * Confirmed: actor+0x18 (unit_handle) checked against -1 at 0x3ad9c.
 * Confirmed: object_get_and_verify_type(unit_handle, 3) at 0x3ada9.
 * Confirmed: FUN_0013ff50(unit_handle, 1) at 0x3adb6.
 * Confirmed: FUN_001adf10(unit_handle, 0) at 0x3adc1.
 * Confirmed: assert unit+0x1a4 == actor_handle at 0x3adcf.
 * Confirmed: unit+0x1a4 = -1 at 0x3adf6.
 * Confirmed: actor+0x1c (unique leader flag) checked at 0x3adfc.
 * Confirmed: datum_get(encounter_data, actor+0x34) at 0x3ae11.
 * Confirmed: assert encounter+0x1c > 0 at 0x3ae1b.
 * Confirmed: encounter+0x1c decremented at 0x3ae41.
 * Confirmed: actor+0x18 = -1, actor+0x1c = 0 at 0x3ae45-0x3ae48. */
void FUN_0003ad80(int actor_handle)
{
  char *actor;
  char *unit;
  char *encounter;

  actor = (char *)datum_get(actor_data, actor_handle);

  if (*(int *)(actor + 0x18) == -1) {
    return;
  }

  unit = (char *)object_get_and_verify_type(*(int *)(actor + 0x18), 3);
  FUN_0013ff50(*(int *)(actor + 0x18), 1);
  FUN_001adf10(*(int *)(actor + 0x18), 0);

  /* Assert unit's actor_index matches */
  if (*(int *)(unit + 0x1a4) != actor_handle) {
    display_assert("unit->unit.actor_index == actor_index",
                   "c:\\halo\\SOURCE\\ai\\actors.c", 0x55e, 1);
    system_exit(-1);
  }

  *(int *)(unit + 0x1a4) = -1;

  /* If actor was a unique leader, decrement encounter's count */
  if (*(char *)(actor + 0x1c) != 0 && *(int *)(actor + 0x34) != -1) {
    encounter = (char *)datum_get(*(data_t **)0x5ab270, *(int *)(actor + 0x34));
    if (*(short *)(encounter + 0x1c) <= 0) {
      display_assert("encounter->unique_leader_count > 0",
                     "c:\\halo\\SOURCE\\ai\\actors.c", 0x565, 1);
      system_exit(-1);
    }
    *(short *)(encounter + 0x1c) = *(short *)(encounter + 0x1c) - 1;
  }

  *(int *)(actor + 0x18) = -1;
  *(char *)(actor + 0x1c) = 0;
}

/* FUN_0003ae60 (0x3ae60) — actor_detach_unit
 *
 * Detach a unit from an actor. This removes the unit from the actor's unit
 * list, updates the unit linked-list pointers (unit+0x1ac/0x1b0), and if the
 * actor is a swarm, removes the unit from the swarm's component arrays.
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x3ae71.
 * Confirmed: object_get_and_verify_type(unit_handle, 3) at 0x3ae7f.
 * Confirmed: unit+0x1a8 == actor_handle check at 0x3ae8f.
 * Confirmed: assert actor+0x1e > 0 (swarm_unit_count) at 0x3aea2.
 * Confirmed: FUN_0013ff50(unit_handle, 1) at 0x3aec7.
 * Confirmed: FUN_001adf10(unit_handle, 0) at 0x3aecf.
 * Confirmed: swarm lookup via datum_get(swarm_data, actor+0x28) at 0x3aeed.
 * Confirmed: swarm+0x18 array holds unit handles, swarm+0x58 holds component
 * handles. Confirmed: datum_delete(swarm_component_data, component_handle) at
 * 0x3afdd. Confirmed: linked list update via unit+0x1ac (prev) and unit+0x1b0
 * (next). Confirmed: unit+0x1a8 = -1 at end, actor+0x1e decremented. */
void FUN_0003ae60(int actor_handle, int unit_handle)
{
  char *actor;
  char *unit;
  char *swarm;
  short i;
  int component_handle;
  short unit_count;

  actor = (char *)datum_get(actor_data, actor_handle);
  unit = (char *)object_get_and_verify_type(unit_handle, 3);

  /* Only proceed if this unit belongs to this actor */
  if (*(int *)(unit + 0x1a8) != actor_handle) {
    return;
  }

  /* Assert swarm_unit_count > 0 */
  if (*(short *)(actor + 0x1e) <= 0) {
    display_assert("actor->meta.swarm_unit_count > 0",
                   "c:\\halo\\SOURCE\\ai\\actors.c", 0x579, 1);
    system_exit(-1);
  }

  /* Set unit flags and update weapon state */
  FUN_0013ff50(unit_handle, 1);
  FUN_001adf10(unit_handle, 0);

  /* If actor has a swarm, remove unit from swarm arrays */
  if (*(int *)(actor + 0x28) != -1) {
    swarm = (char *)datum_get(swarm_data, *(int *)(actor + 0x28));

    /* Assert swarm->actor_index == actor_index */
    if (*(int *)(swarm + 4) != actor_handle) {
      display_assert("swarm->actor_index == actor_index",
                     "c:\\halo\\SOURCE\\ai\\actors.c", 0x585, 1);
      system_exit(-1);
    }

    /* Assert swarm->unit_count == actor->meta.swarm_unit_count */
    if (*(short *)(swarm + 2) != *(short *)(actor + 0x1e)) {
      display_assert("swarm->unit_count == actor->meta.swarm_unit_count",
                     "c:\\halo\\SOURCE\\ai\\actors.c", 0x586, 1);
      system_exit(-1);
    }

    /* Search for unit in swarm array and remove it */
    for (i = 0; i < *(short *)(swarm + 2); i++) {
      if (*(int *)(swarm + 0x18 + i * 4) == unit_handle) {
        /* Save component handle before removing */
        component_handle = *(int *)(swarm + 0x58 + i * 4);

        /* Decrement count */
        unit_count = *(short *)(swarm + 2) - 1;
        *(short *)(swarm + 2) = unit_count;

        /* Compact arrays if not last element */
        if (i < unit_count) {
          *(int *)(swarm + 0x18 + i * 4) =
            *(int *)(swarm + 0x18 + unit_count * 4);
          *(int *)(swarm + 0x58 + i * 4) =
            *(int *)(swarm + 0x58 + *(short *)(swarm + 2) * 4);
        }

        /* Delete the swarm component */
        datum_delete(swarm_component_data, component_handle);
        goto update_linked_list;
      }
    }

    /* Unit not found in swarm - assert */
    display_assert("found", "c:\\halo\\SOURCE\\ai\\actors.c", 0x59c, 1);
    system_exit(-1);
  }

update_linked_list:
  /* Update linked list of units */
  if (*(int *)(unit + 0x1b0) == -1) {
    /* No next unit - update actor's first unit pointer */
    *(int *)(actor + 0x24) = *(int *)(unit + 0x1ac);
  } else {
    /* Update next unit's prev pointer */
    char *next_unit =
      (char *)object_get_and_verify_type(*(int *)(unit + 0x1b0), 3);
    *(int *)(next_unit + 0x1ac) = *(int *)(unit + 0x1ac);
  }

  if (*(int *)(unit + 0x1ac) != -1) {
    /* Update prev unit's next pointer */
    char *prev_unit =
      (char *)object_get_and_verify_type(*(int *)(unit + 0x1ac), 3);
    *(int *)(prev_unit + 0x1b0) = *(int *)(unit + 0x1b0);
  }

  /* Clear unit's actor reference and decrement count */
  *(int *)(unit + 0x1a8) = -1;
  *(short *)(actor + 0x1e) = *(short *)(actor + 0x1e) - 1;
}

/* FUN_0003b030 (0x3b030) — actor_delete_swarm
 *
 * Delete swarm data for an actor. Iterates all swarm components and deletes
 * them from swarm_component_data, then deletes the swarm from swarm_data and
 * clears actor+0x28.
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x3b03f.
 * Confirmed: actor+0x28 (swarm_handle) checked against -1 at 0x3b04c.
 * Confirmed: datum_get(swarm_data, swarm_handle) at 0x3b05b.
 * Confirmed: loop over swarm+2 (count), deleting swarm+0x58[i] components.
 * Confirmed: datum_delete(swarm_component_data, component) at 0x3b07f.
 * Confirmed: datum_delete(swarm_data, actor+0x28) at 0x3b099.
 * Confirmed: actor+0x28 = -1 at 0x3b0a2. */
void FUN_0003b030(int actor_handle)
{
  char *actor;
  char *swarm;
  short i;

  actor = (char *)datum_get(actor_data, actor_handle);

  if (*(int *)(actor + 0x28) == -1) {
    return;
  }

  swarm = (char *)datum_get(swarm_data, *(int *)(actor + 0x28));

  /* Delete all swarm components */
  for (i = 0; i < *(short *)(swarm + 2); i++) {
    datum_delete(swarm_component_data, *(int *)(swarm + 0x58 + i * 4));
  }

  /* Delete the swarm itself */
  datum_delete(swarm_data, *(int *)(actor + 0x28));
  *(int *)(actor + 0x28) = -1;
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

/* FUN_0003b410 (0x3b410) — actor_replace_prop_reference
 *
 * Replace all references to old_prop with new_prop in actor fields. Updates
 * multiple prop reference fields at various offsets in the actor structure.
 * Also updates swarm component prop references if the actor is a swarm.
 * Finally calls FUN_0001c450 to dispatch to action-specific prop replacement.
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x3b421.
 * Confirmed: actor+0x270 prop reference with special clear of +0x268 if new=-1.
 * Confirmed: actor+0x610 prop with special clear of +0x60c if new=-1 and
 * +0x60c==1. Confirmed: actor+0x6b4, +0x2f4, +0x30c, +0x340, +0x3ac, +0x1d0,
 * +0x1e8 prop refs. Confirmed: actor+0x3a8 cleared when +0x3ac matched and
 * new=-1. Confirmed: actor+0x1e4 cleared when +0x1e8 matched and new=-1.
 * Confirmed: actor+0x46c (action state 5) with +0x470 prop, clears +0x480 if
 * new=-1. Confirmed: actor+0x54c, +0x56c, +0x57c (action state 1) with
 * +0x550/0x570/0x580. Confirmed: swarm component +0x14 prop updated for each
 * component. Confirmed: FUN_0001c450(actor_handle, old_prop, new_prop) at
 * 0x3b5c4. */
void FUN_0003b410(int actor_handle, int old_prop, int new_prop)
{
  char *actor;
  char *swarm;
  char *component;
  short i;

  actor = (char *)datum_get(actor_data, actor_handle);

  /* Update prop reference at +0x270 */
  if (*(int *)(actor + 0x270) == old_prop) {
    *(int *)(actor + 0x270) = new_prop;
    if (new_prop == -1) {
      *(short *)(actor + 0x268) = 0;
    }
  }

  /* Update prop reference at +0x610 (only if +0x60c == 1) */
  if (*(short *)(actor + 0x60c) == 1 && *(int *)(actor + 0x610) == old_prop) {
    *(int *)(actor + 0x610) = new_prop;
    if (new_prop == -1) {
      *(short *)(actor + 0x60c) = 0;
    }
  }

  /* Update simple prop references */
  if (*(int *)(actor + 0x6b4) == old_prop) {
    *(int *)(actor + 0x6b4) = new_prop;
  }
  if (*(int *)(actor + 0x2f4) == old_prop) {
    *(int *)(actor + 0x2f4) = new_prop;
  }
  if (*(int *)(actor + 0x30c) == old_prop) {
    *(int *)(actor + 0x30c) = new_prop;
  }
  if (*(int *)(actor + 0x340) == old_prop) {
    *(int *)(actor + 0x340) = new_prop;
  }

  /* Update prop reference at +0x3ac with special +0x3a8 clear */
  if (*(int *)(actor + 0x3ac) == old_prop) {
    if (new_prop == -1) {
      *(short *)(actor + 0x3a8) = 0;
    }
    *(int *)(actor + 0x3ac) = new_prop;
  }

  /* Update prop reference at +0x1d0 */
  if (*(int *)(actor + 0x1d0) == old_prop) {
    *(int *)(actor + 0x1d0) = new_prop;
  }

  /* Update prop reference at +0x1e8 with special +0x1e4 clear */
  if (*(int *)(actor + 0x1e8) == old_prop) {
    *(int *)(actor + 0x1e8) = new_prop;
    if (new_prop == -1) {
      *(short *)(actor + 0x1e4) = 0;
    }
  }

  /* Update prop reference at +0x470 (only if action state +0x46c == 5) */
  if (*(short *)(actor + 0x46c) == 5 && *(int *)(actor + 0x470) == old_prop) {
    if (new_prop == -1) {
      *(short *)(actor + 0x46c) = 0;
      *(int *)(actor + 0x480) = -1;
    } else {
      *(int *)(actor + 0x470) = new_prop;
    }
  }

  /* Update prop references at +0x550, +0x570, +0x580 (if action state == 1) */
  if (*(short *)(actor + 0x54c) == 1 && *(int *)(actor + 0x550) == old_prop) {
    *(int *)(actor + 0x550) = new_prop;
  }
  if (*(short *)(actor + 0x56c) == 1 && *(int *)(actor + 0x570) == old_prop) {
    *(int *)(actor + 0x570) = new_prop;
  }
  if (*(short *)(actor + 0x57c) == 1 && *(int *)(actor + 0x580) == old_prop) {
    *(int *)(actor + 0x580) = new_prop;
  }

  /* Update swarm component prop references if this is a swarm actor */
  if (*(char *)(actor + 6) != 0 && *(int *)(actor + 0x28) != -1) {
    swarm = (char *)datum_get(swarm_data, *(int *)(actor + 0x28));
    for (i = 0; i < *(short *)(swarm + 2); i++) {
      component =
        (char *)datum_get(swarm_component_data, *(int *)(swarm + 0x58 + i * 4));
      if (*(int *)(component + 0x14) == old_prop) {
        *(int *)(component + 0x14) = new_prop;
      }
    }
  }

  /* Dispatch to action-specific prop replacement */
  FUN_0001c450(actor_handle, old_prop, new_prop);
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

/* FUN_0003cbc0 (0x3cbc0) — actor_clean_props
 *
 * Clean up all props associated with an actor. Iterates actor+0x50 linked list,
 * calling FUN_0003b410 to clear prop references and FUN_00064a80 to delete
 * each prop, until the list is empty.
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x3cbcf.
 * Confirmed: actor+0x50 (prop list head) checked against -1 at 0x3cbdc.
 * Confirmed: FUN_0003b410(actor_handle, prop, -1) at 0x3cbe5.
 * Confirmed: FUN_00064a80(actor_handle, actor+0x50) at 0x3cbef.
 * Confirmed: loop continues while actor+0x50 != -1 at 0x3cbfd. */
void FUN_0003cbc0(int actor_handle)
{
  char *actor;
  int prop_handle;

  actor = (char *)datum_get(actor_data, actor_handle);
  prop_handle = *(int *)(actor + 0x50);

  while (prop_handle != -1) {
    FUN_0003b410(actor_handle, prop_handle, -1);
    FUN_00064a80(actor_handle, *(int *)(actor + 0x50));
    prop_handle = *(int *)(actor + 0x50);
  }
}

/* FUN_0003cc10 (0x3cc10) — actor_delete
 *
 * Delete an actor and clean up all references. Asserts the actor is not the
 * currently updating actor. Clears global references if they match, removes
 * the actor from encounter or encounterless list, detaches all units, cleans
 * up props, clears prop actor references, and finally deletes the actor datum.
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x3cc23.
 * Confirmed: assert actor_index != global_updating_actor_index at 0x3cc37.
 * Confirmed: DAT_005ac9f8 and DAT_006323b4 cleared if equal to actor_handle.
 * Confirmed: actor+9 flag selects FUN_000597f0 (encounterless) vs FUN_00059480.
 * Confirmed: actor+6 flag selects FUN_0003b030+loop vs FUN_0003ad80 path.
 * Confirmed: FUN_0003cbc0 at 0x3cccc, data_iterator on DAT_005ab23c at 0x3ccdc.
 * Confirmed: FUN_00049080 at 0x3cd0a, FUN_00044590 at 0x3cd10.
 * Confirmed: datum_delete(actor_data, actor_handle) at 0x3cd1c. */
void FUN_0003cc10(int actor_handle, int flag)
{
  char *actor;
  char iter[0x10];
  char *prop;
  int unit_handle;

  actor = (char *)datum_get(actor_data, actor_handle);

  /* Assert not deleting the currently updating actor */
  if (actor_handle == *(int *)0x2c8728) {
    display_assert("actor_index != global_updating_actor_index",
                   "c:\\halo\\SOURCE\\ai\\actors.c", 0x5d0, 1);
    system_exit(-1);
  }

  /* Clear global references if they match */
  if (*(int *)0x5ac9f8 == actor_handle) {
    *(int *)0x5ac9f8 = -1;
  }
  if (*(int *)0x6323b4 == actor_handle) {
    *(int *)0x6323b4 = -1;
  }

  /* Remove from encounter or encounterless list */
  if (*(char *)(actor + 9) == 0) {
    FUN_00059480(actor_handle, (char)flag);
  } else {
    FUN_000597f0(actor_handle);
  }

  /* Detach units */
  if (*(char *)(actor + 6) == 0) {
    /* Non-swarm: single unit detach */
    FUN_0003ad80(actor_handle);
  } else {
    /* Swarm: delete swarm data and detach all units */
    FUN_0003b030(actor_handle);
    unit_handle = *(int *)(actor + 0x24);
    while (unit_handle != -1) {
      FUN_0003ae60(actor_handle, unit_handle);
      unit_handle = *(int *)(actor + 0x24);
    }
  }

  /* Clean up props */
  FUN_0003cbc0(actor_handle);

  /* Clear prop actor references */
  data_iterator_new((data_iter_t *)iter, *(data_t **)0x5ab23c);
  prop = (char *)data_iterator_next((data_iter_t *)iter);
  while (prop != NULL) {
    if (*(int *)(prop + 0x1c) == actor_handle) {
      *(int *)(prop + 0x1c) = -1;
    }
    prop = (char *)data_iterator_next((data_iter_t *)iter);
  }

  /* Final cleanup */
  FUN_00049080(actor_handle);
  FUN_00044590(actor_handle);

  /* Delete the actor record */
  datum_delete(actor_data, actor_handle);
}

/* FUN_0003cff0 (0x3cff0) — actor_update_weapon_state
 *
 * Update weapon firing state for an actor. If the actor is in combat state 3
 * (attacking) with burst count > 1, performs accuracy-based random firing
 * check. Also handles weapon ammo distribution and magazine rounds for the
 * actor's weapon. Calls FUN_0003cc10 to mark actor as needing cleanup, and if
 * actor has an encounter, calls FUN_0005d420 to update encounter state.
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x3d004.
 * Confirmed: tag_get(0x61637476, actor+0x5c) at 0x3d014.
 * Confirmed: actor+0x34 stored for encounter_handle check at end.
 * Confirmed: actor+0x6a == 3 and actor+0x6e > 1 condition at 0x3d021-0x3d034.
 * Confirmed: object_get_and_verify_type(actor+0x18, 3) at 0x3d040, 0x3d064.
 * Confirmed: unit_is_alive(actor+0x18) at 0x3d04e.
 * Confirmed: unit_get_weapon(actor+0x18, unit+0x2a2) at 0x3d077.
 * Confirmed: accuracy clamping to [0.1f, 0.6f] at 0x3d096-0x3d0d7.
 * Confirmed: accuracy boost by 1.5x under certain conditions at
 * 0x3d0fe-0x3d12d. Confirmed: random check against accuracy at 0x3d12f-0x3d145.
 * Confirmed: burst duration from tag+0x98, clamped to [0.05f, 2.0f], * 30.0f.
 * Confirmed: FUN_001a8190(unit, ticks, 0x800) at 0x3d1c8.
 * Confirmed: FUN_0003cc10(actor_handle, 1) at 0x3d304.
 * Confirmed: FUN_0005d420(encounter_handle) at 0x3d318 if actor+0x34 != -1. */
void FUN_0003cff0(int actor_handle)
{
  char *actor;
  char *tag;
  char *unit;
  int encounter_handle;
  int weapon_handle;
  float accuracy;
  float boosted;
  float burst_duration;
  int ticks;
  int *seed;

  actor = (char *)datum_get(actor_data, actor_handle);
  tag = (char *)tag_get(0x61637476, *(int *)(actor + 0x5c));
  encounter_handle = *(int *)(actor + 0x34);

  /* Combat state 3 with burst count > 1: perform firing logic */
  if (*(short *)(actor + 0x6a) == 3 && *(short *)(actor + 0x6e) > 1) {
    unit = (char *)object_get_and_verify_type(*(int *)(actor + 0x18), 3);

    if (unit_is_alive(*(int *)(actor + 0x18))) {
      char *unit2 =
        (char *)object_get_and_verify_type(*(int *)(actor + 0x18), 3);
      weapon_handle = unit_get_weapon(
        *(int *)(actor + 0x18), (int)(uint16_t)(*(int16_t *)(unit2 + 0x2a2)));

      if (weapon_handle != -1 && *(char *)(unit + 0x23c) > 0) {
        /* Clamp accuracy to [0.1f, 0.6f] */
        if (*(float *)(tag + 0x94) < 0.1f) {
          accuracy = 0.1f;
        } else if (*(float *)(tag + 0x94) > 0.6f) {
          accuracy = 0.6f;
        } else {
          accuracy = *(float *)(tag + 0x94);
        }

        /* Accuracy boost under certain conditions */
        if (*(char *)(actor + 0x378) != 0 ||
            (*(short *)(actor + 0x60c) > 0 &&
             *(float *)(actor + 0x648) < *(float *)0x254644)) {
          boosted = accuracy * 1.5f;
          if (boosted > 0.6f) {
            boosted = 0.6f;
          }
          if (boosted > accuracy) {
            accuracy = boosted;
          }
        }

        /* Random check against accuracy */
        seed = get_global_random_seed_address();
        if (random_math_real((unsigned int *)seed) < accuracy) {
          /* Calculate burst duration in ticks */
          if (*(float *)(tag + 0x98) == 0.0f) {
            FUN_000121e0(0.8f, 1.3f);
          }

          /* Clamp burst_seconds to [0.05f, 2.0f] */
          if (*(float *)(tag + 0x98) < 0.05f) {
            burst_duration = 0.05f;
          } else if (*(float *)(tag + 0x98) > 2.0f) {
            burst_duration = 2.0f;
          } else {
            burst_duration = *(float *)(tag + 0x98);
          }

          /* Convert to ticks (30 ticks per second) */
          ticks = (int)(burst_duration * 30.0f);
          FUN_001a8190(*(int *)(actor + 0x18), ticks, 0x800);
          *(char *)(unit + 0x23c) = (char)ticks;
        }
      }
    }
  }

  /* Update weapon ammo state */
  unit = (char *)object_get_and_verify_type(*(int *)(actor + 0x18), 3);

  seed = get_global_random_seed_address();
  float random_val = random_math_real((unsigned int *)seed);

  char *unit2 = (char *)object_get_and_verify_type(*(int *)(actor + 0x18), 3);
  weapon_handle =
    unit_get_weapon(*(int *)(actor + 0x18), (int)(*(int16_t *)(unit2 + 0x2a2)));

  /* Check global flag for clearing weapon state */
  char *ai_globals = *(char **)0x632574;
  if (*(char *)(ai_globals + 0x3b4) == 0 ||
      random_val < *(float *)(tag + 0x1d4)) {
    csmemset(unit + 0x2ce, 0, 2);
  }

  if (weapon_handle != -1) {
    /* Set weapon ammo fraction if tag defines it */
    if (*(float *)(tag + 0x1d8) > 0.0f || *(float *)(tag + 0x1dc) > 0.0f) {
      seed = get_global_random_seed_address();
      float ammo_fraction = random_real_range(seed, *(float *)(tag + 0x1d8),
                                              *(float *)(tag + 0x1dc));
      FUN_000fd180(weapon_handle, ammo_fraction);
    }

    /* Set magazine rounds if tag defines it */
    if (*(short *)(tag + 0x1e0) > 0 || *(short *)(tag + 0x1e2) > 0) {
      int16_t rounds = 0;
      seed = get_global_random_seed_address();
      rounds = random_range((unsigned int *)seed, *(short *)(tag + 0x1e0),
                            *(short *)(tag + 0x1e2) + 1);
      FUN_000fbbd0(weapon_handle, &rounds);
    }
  }

  FUN_0003cc10(actor_handle, 1);

  if (encounter_handle != -1) {
    FUN_0005d420(encounter_handle);
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

/* FUN_0003ec80 (0x3ec80) — actor_activate (full AI init sequence for one actor)
 *
 * Called from FUN_0003f5f0 (ai.obj) when actor+0x6a > 0 (activation counter
 * exhausted) and the actor has not yet been activated. Runs all per-actor
 * AI subsystem initialization in sequence.
 *
 * Classification evidence: caller FUN_0003f5f0 is in ai.obj; all callees
 * (FUN_0003d9f0, FUN_0003dc20, FUN_0003bb50, FUN_0003bbf0, FUN_0003be90,
 * FUN_0003e7a0) live in the actors.obj address range (~0x3b000-0x3e9aa) and
 * operate exclusively on actor_data. Function is placed at the end of
 * actors.obj (follows FUN_0003d950 at 0x3d950).
 *
 * Confirmed: actor_handle passed in ESI (register arg @<esi>).
 *   MOV ESI,[EBP-8]; CALL 0x3ec80 at caller 0x3f652/0x3f655.
 * Confirmed: first datum_get(actor_data, actor_handle) at 0x3ec8b.
 *   Result stored in [EBP-4] for use as iVar2/actor record.
 * Confirmed: DAT_002c8728 = actor_handle at 0x3ecaa (before any branch).
 * Confirmed: debug block byte[0x5ac9c0] cleared if actor_handle == [0x5ac9f8]
 *   at 0x3ecb2/0x3ecba.
 * Confirmed: FUN_0003d9f0(actor_handle) cdecl at 0x3ecc3; returns bool/char.
 *   ADD ESP,4 at 0x3ecc8. Return tested; JZ 0x3edae → early out.
 * Confirmed: FUN_0003bb50(actor_handle@<eax>) at 0x3ecd6 (MOV EAX,ESI).
 * Confirmed: FUN_0003dc20(actor_handle) cdecl at 0x3ecdc.
 * Confirmed: FUN_0003355f0(actor_handle) cdecl at 0x3ece2.
 * Confirmed: FUN_000303f0(actor_handle) cdecl at 0x3ece8.
 * Confirmed: FUN_00032cb0(actor_handle) cdecl at 0x3ecee.
 * Confirmed: second datum_get(actor_data, actor_handle) at 0x3ecfb; result
 *   in EDI, used as iVar3/actor record for memset+field init.
 * Confirmed: csmemset(actor+0x3e8, 0, 0x84) at 0x3ed10.
 * Confirmed: word[actor+0x418]=0xffff, [0x42c]=0xffff, [0x42e]=0xffff at
 *   0x3ed19/0x3ed20/0x3ed27. EBX = 0xffffffff set by OR EBX,0xffffffff.
 * Confirmed: FUN_0003be90(actor_handle) cdecl at 0x3ed2e (PUSH ESI at 0x3ed18).
 * Confirmed: FUN_0001c370(actor_handle) cdecl at 0x3ed34.
 * Confirmed: ADD ESP,0x2c at 0x3ed3f cleans 11 cdecl args.
 * Confirmed: iVar2/actor+0x13 checked at 0x3ed3c; JNZ → skip subsystem init.
 * Confirmed: iVar2/actor+6 checked at 0x3ed47; JNZ (swarm actor) → call
 *   FUN_0003a8a0(actor_handle) then return.
 * Confirmed: FUN_0003bbf0(actor_handle@<eax>) at 0x3ed64 (MOV EAX,ESI).
 * Confirmed: 8 cdecl calls follow (0x1c3e0, 0x43db0, 0x14540, 0x2d350,
 *   0x2a2b0, 0x2e560, 0x29040, 0x22dc0); ADD ESP,0x20 at 0x3ed99.
 * Confirmed: FUN_0003e7a0(actor_handle@<eax>) at 0x3ed9e (MOV EAX,ESI).
 * Confirmed: DAT_002c8728 = EBX (0xffffffff) at 0x3eda3 (normal exit).
 * Confirmed: DAT_002c8728 = 0xffffffff at 0x3edae (early-out path, literal).
 * Inferred: DAT_002c8728 holds the "currently activating actor" handle;
 *   reset to -1 on all exit paths including early bail-out.
 * Inferred: byte[0x5ac9c0] is an AI debug focused-actor flag (see ai_debug.c).
 *   Cleared here to prevent stale display after actor is re-activated.
 * Inferred: actor+0x13 is a "don't initialize" or dormant flag;
 *   non-zero skips all subsystem init and just resets DAT_002c8728.
 * Inferred: actor+6 distinguishes swarm vs. normal actor type; swarm actors
 *   take a shortened init path via FUN_0003a8a0. */
void FUN_0003ec80(int actor_handle /* @<esi> */)
{
  char *actor;
  char *actor2;
  char ok;

  /* Record first datum_get result (iVar2) for field checks below */
  actor = (char *)datum_get(actor_data, actor_handle);
  /* Unused second call result discarded by compiler — both use same handle */
  datum_get(actor_data, actor_handle);

  /* Track which actor is being activated */
  *(int *)0x2c8728 = actor_handle;

  /* Clear AI debug focus flag if it points at this actor */
  if (*(char *)0x5ac9c0 != 0 && *(int *)0x5ac9f8 == actor_handle) {
    *(char *)0x5ac9c0 = 0;
  }

  /* Run actor validation/pre-init; bail if not ready */
  ok = FUN_0003d9f0(actor_handle);
  if (ok == 0) {
    *(int *)0x2c8728 = -1;
    return;
  }

  /* --- actor is ready for activation --- */

  /* Subsystem pre-init */
  FUN_0003bb50(actor_handle);
  FUN_0003dc20(actor_handle);
  FUN_000355f0(actor_handle);
  FUN_000303f0(actor_handle);
  FUN_00032cb0(actor_handle);

  /* Reload actor record (EDI path for memset+field writes) */
  actor2 = (char *)datum_get(actor_data, actor_handle);

  /* Zero 0x84 bytes of actor state starting at offset 0x3e8 */
  csmemset(actor2 + 0x3e8, 0, 0x84);

  /* Initialize handle sentinel fields to 0xffff */
  *(short *)(actor2 + 0x418) = (short)0xffff;
  *(short *)(actor2 + 0x42c) = (short)0xffff;
  *(short *)(actor2 + 0x42e) = (short)0xffff;

  /* More subsystem init */
  FUN_0003be90(actor_handle);
  FUN_0001c370(actor_handle);

  /* Check dormant/don't-activate flag at actor+0x13 */
  if (*(char *)(actor + 0x13) != 0) {
    *(int *)0x2c8728 = -1;
    return;
  }

  /* Swarm actor: shortened init path */
  if (*(char *)(actor + 0x6) != 0) {
    FUN_0003a8a0(actor_handle);
    *(int *)0x2c8728 = -1;
    return;
  }

  /* Normal actor: full subsystem init sequence */
  FUN_0003bbf0(actor_handle);
  FUN_0001c3e0(actor_handle);
  FUN_00043db0(actor_handle);
  FUN_00014540(actor_handle);
  FUN_0002d350(actor_handle);
  FUN_0002a2b0(actor_handle);
  FUN_0002e560(actor_handle);
  FUN_00029040(actor_handle);
  FUN_00022dc0(actor_handle);
  FUN_0003e7a0(actor_handle);

  *(int *)0x2c8728 = -1;
}
