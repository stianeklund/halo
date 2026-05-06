/* Check if an actor has a swarm component or its unit is in a vehicle seat. */
int FUN_0002a360(int actor_handle)
{
  char *actor = (char *)datum_get(actor_data, actor_handle);
  if (*(short *)(actor + 0x418) != -1)
    return 1;
  if (*(int *)(actor + 0x18) != -1 && FUN_001a9ad0(*(int *)(actor + 0x18)))
    return 1;
  return 0;
}

/* Clear 100 bytes of actor state at offset 0x2ec (action decision state). */
void FUN_00036860(int actor_handle)
{
  char *actor = (char *)datum_get(actor_data, actor_handle);
  csmemset(actor + 0x2ec, 0, 0x64);
}

/* FUN_00036e30 (0x36e30)
 * Mark an actor as having an active approach. Looks up the actor
 * record in actor_data and sets the byte flag at offset +0x2ed to 1.
 * Called from ai_handle_unit_approach when a non-friendly unit is
 * within approach range and the caller's flag parameter is set.
 * Confirmed: 1 cdecl arg (ADD ESP,4 at call site), void return,
 * single datum_get call followed by byte store. */
void FUN_00036e30(int ai_handle)
{
  char *actor;

  actor = (char *)datum_get(actor_data, ai_handle);
  *(char *)(actor + 0x2ed) = 1;
}

void *FUN_0003a600(short actor_type /* @<ax> */)
{
  void **actor_type_definitions = (void **)0x2c86a8;
  char *def;

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
  def = (char *)actor_type_definitions[actor_type];
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

/* Return the name string for an actor type definition. */
const char *FUN_0003a760(int16_t actor_type)
{
  return *(const char **)FUN_0003a600(actor_type);
}

/* FUN_0003a800 (0x3a800) — actor_type_is_swarm
 * Returns the swarm flag byte (offset 0xd) from the actor type definition
 * for the given actor_type. Used to test whether an actor type uses swarm
 * control before dispatching swarm callbacks. */
int FUN_0003a800(int16_t actor_type)
{
  char *type_def;
  type_def = (char *)FUN_0003a600(actor_type);
  return (int)(unsigned char)type_def[0xd];
}

/* FUN_0003a810 (0x3a810) — actor_type_init_dispatch
 * Looks up the actor datum by handle, reads the actor_type field (int16_t at
 * offset 4), retrieves the actor type definition, and calls the type-specific
 * init callback (function pointer at type_def+0x10) if it is non-null.
 * Called at the end of actor_new (FUN_0003c410) to perform per-type
 * initialization of a newly allocated actor. */
void FUN_0003a810(int actor_handle)
{
  char *actor;
  char *type_def;
  void (*init_cb)(int);

  actor = (char *)datum_get(actor_data, actor_handle);
  type_def = (char *)FUN_0003a600(*(short *)(actor + 0x4));
  init_cb = *(void (**)(int))(type_def + 0x10);
  if (init_cb != NULL) {
    init_cb(actor_handle);
  }
}

/* Dispatch the actor-type-specific decide_action function for a given actor. */
void FUN_0003a840(int actor_handle)
{
  char *actor;
  void *type_def;

  actor = (char *)datum_get(actor_data, actor_handle);
  type_def = FUN_0003a600(*(short *)(actor + 4));

  if (*(int *)((char *)type_def + 0x14) == 0) {
    display_assert("actor_type_definition->decide_action",
                   "c:\\halo\\SOURCE\\ai\\actor_types.c", 0x81, 1);
    system_exit(-1);
  }
  (*(void (**)(int))((char *)type_def + 0x14))(actor_handle);
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

/* FUN_0003ac20 (0x3ac20) — actor_check_unit_activation_logic
 *
 * Validates that a unit's activation state is consistent with the actor's
 * dormancy flag. For top-level objects only (parent_object_index == -1): if
 * the unit was deactivated more than 30 ticks ago and its active-flag differs
 * from what the actor expects, fires an activation-logic error.
 *
 * Parameters:
 *   actor_handle — cdecl stack arg: actor datum handle
 *   reason       — cdecl stack arg: string label used in the error message
 *   obj_handle   — @<eax> register arg: handle of the unit/object to check
 *
 * Confirmed: MOV ESI,EAX at 0x3ac25 captures register arg.
 * Confirmed: datum_get(*(data_t**)0x5a8d50, obj_handle) at 0x3ac2f → header.
 * Confirmed: object_get_and_verify_type(obj_handle, 3) at 0x3ac39 → obj ptr.
 * Confirmed: datum_get(actor_data, actor_handle) at 0x3ac4b → actor ptr.
 * Confirmed: [obj+0xcc] == -1 guard (parent_object_index) at 0x3ac5b.
 * Confirmed: game_time_get() at 0x3ac60; [obj+0x2dc]+0x1e compared at 0x3ac6e.
 * Confirmed: [actor+0x13] vs (~[header+0x2])&1 mismatch check at 0x3ac7f.
 * Confirmed: error(2, "%s unit activation logic error", reason) at 0x3ac8e. */
void FUN_0003ac20(int actor_handle, const char *reason,
                  int obj_handle /* @<eax> */)
{
  char *header;
  char *obj;
  char *actor;
  int now;

  /* Resolve object header and full object pointer */
  header = (char *)datum_get(*(data_t **)0x5a8d50, obj_handle);
  obj = (char *)object_get_and_verify_type(obj_handle, 3);
  actor = (char *)datum_get(actor_data, actor_handle);

  /* Only check top-level objects (no parent) */
  if (*(int *)(obj + 0xcc) != -1) {
    return;
  }

  /* Only flag if the unit has been deactivated long enough */
  now = game_time_get();
  if (*(int *)(obj + 0x2dc) + 0x1e >= now) {
    return;
  }

  /* Check: actor's active-flag must match what the object header reports.
   * header->unk_2 bit0 == 0 means active; actor+0x13 == 0 means dormant.
   * If (~header->unk_2 & 1) != actor->active then it's a logic error. */
  if (*(unsigned char *)(actor + 0x13) !=
      ((~*(unsigned char *)(header + 0x2)) & 1u)) {
    error(2, "%s unit activation logic error", reason);
  }
}

/* FUN_0003aca0 (0x3aca0) — actor_check_dormancy_logic
 *
 * Validates dormancy/activation consistency for all units controlled by an
 * actor. Asserts at least one of the two dormancy flags (actor+0x8 = has_unit,
 * actor+0x13 = active) is set. Then dispatches to FUN_0003ac20 for each
 * unit depending on actor type:
 *
 *   Non-swarm (actor+0x6 == 0): checks the single unit at actor+0x18.
 *   Swarm with no handle (actor+0x28 == -1): walks the singly-linked list
 *     starting at actor+0x24, following obj+0x1ac, checking each object.
 *   Swarm with handle (actor+0x28 != -1): looks up the swarm record via
 *     swarm_data and checks each member handle in swarm->members[].
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x3acb0.
 * Confirmed: actor+0x8 and actor+0x13 both-zero assert at 0x3acbf/0x3acc6.
 * Confirmed: actor+0x6 branch at 0x3acda.
 * Confirmed: actor+0x18 (unit handle, individual path) checked at 0x3ad5d.
 * Confirmed: actor+0x28 (swarm handle) checked at 0x3ace4.
 * Confirmed: datum_get(swarm_data, actor+0x28) at 0x3acee (active swarm).
 * Confirmed: swarm->count at [swarm+0x2] (int16_t); member handles at
 *   [swarm+0x18+i*4]; loop counter ESI is int16 (CMP SI, word ptr).
 * Confirmed: linked-list path: actor+0x24 head, obj+0x1ac next ptr.
 * Confirmed: object_get_and_verify_type(handle, 3) before FUN_0003ac20 on
 *   linked list path (EDI = obj ptr used to read next ptr at 0x3ad47).
 * Confirmed: FUN_0003ac20 called with @EAX=obj_handle, all three paths. */
void FUN_0003aca0(int actor_handle)
{
  char *actor;
  char *swarm;
  char *obj;
  int obj_handle;
  int16_t i;

  actor = (char *)datum_get(actor_data, actor_handle);

  /* At least one of has_unit or active must be set */
  if (*(char *)(actor + 0x8) == 0 && *(char *)(actor + 0x13) == 0) {
    error(2, "actor dormancy logic error");
  }

  if (*(char *)(actor + 0x6) == 0) {
    /* Non-swarm: single unit */
    if (*(int *)(actor + 0x18) != -1) {
      /* @<eax> = unit handle */
      int unit_handle = *(int *)(actor + 0x18);
      FUN_0003ac20(actor_handle, "individual", unit_handle /* @<eax> */);
    }
  } else if (*(int *)(actor + 0x28) == -1) {
    /* Swarm with no swarm-data handle: walk linked list from actor+0x24 */
    obj_handle = *(int *)(actor + 0x24);
    while (obj_handle != -1) {
      obj = (char *)object_get_and_verify_type(obj_handle, 3);
      FUN_0003ac20(actor_handle, "inactive swarm", obj_handle /* @<eax> */);
      obj_handle = *(int *)(obj + 0x1ac);
    }
  } else {
    /* Swarm with swarm-data handle: iterate members array */
    swarm = (char *)datum_get(swarm_data, *(int *)(actor + 0x28));
    for (i = 0; i < *(int16_t *)(swarm + 0x2); i++) {
      int member = *(int *)(swarm + 0x18 + (int)i * 4);
      FUN_0003ac20(actor_handle, "active swarm", member /* @<eax> */);
    }
  }
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

/* FUN_0003b5e0 (0x3b5e0) — actor_reset_action_state
 *
 * Resets the actor's action-related state and dispatches to the current
 * action's update function. Unconditionally clears the word at actor+0x3b8
 * to 0xffff (a "no-action" or invalid sentinel). If the current action state
 * (actor+0x46c, int16_t) is 3 or 4, resets it to 0 and sets actor+0x480
 * (action timer/handle) to -1. Finally calls FUN_0001c4c0 to dispatch to the
 * action-specific handler indexed by actor+0x6c (state.action).
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x3b5ee.
 * Confirmed: MOV CX,[EAX+0x46c] — int16_t compare at 0x3b5f3.
 * Confirmed: OR EDX,-1 then MOV word [EAX+0x3b8],DX — unconditional store
 *   of 0xffff at 0x3b604 (before the branch, not inside it).
 * Confirmed: MOV word [EAX+0x46c],0x0 and MOV dword [EAX+0x480],EDX at
 *   0x3b613/0x3b61c — conditional on CX==3||CX==4.
 * Confirmed: FUN_0001c4c0(actor_handle) at 0x3b623 (cdecl, 1 arg). */
void FUN_0003b5e0(int actor_handle)
{
  char *actor;
  int16_t action_state;

  actor = (char *)datum_get(actor_data, actor_handle);

  /* Unconditional: clear action sentinel (0xffff = no action pending) */
  *(int16_t *)(actor + 0x3b8) = (int16_t)0xffff;

  /* If action state is 3 or 4, reset it and clear the action handle */
  action_state = *(int16_t *)(actor + 0x46c);
  if (action_state == 3 || action_state == 4) {
    *(int16_t *)(actor + 0x46c) = 0;
    *(int *)(actor + 0x480) = -1;
  }

  /* Dispatch to the current action's update function */
  FUN_0001c4c0(actor_handle);
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

/* Reassign an actor to a new encounter/squad, detaching from the old one. */
void FUN_0003baa0(int actor_handle, int encounter_handle, int16_t squad_index)
{
  char *actor = (char *)datum_get(actor_data, actor_handle);
  FUN_0003b5e0(actor_handle);

  if (*(char *)(actor + 9) != 0) {
    FUN_000597f0(actor_handle);
  } else {
    if (*(int *)(actor + 0x34) != -1) {
      FUN_00059480(actor_handle, 0);
    }
  }

  if (encounter_handle == -1) {
    FUN_00059740(actor_handle);
    return;
  }
  FUN_0005d200(actor_handle, encounter_handle, squad_index, 1);
}

/* FUN_0003bb50 (0x3bb50) — actor_update_cognition_score
 *
 * Updates a per-actor cognition score (field +0x4a) and compares it against
 * thresholds stored in the AI globals struct at 0x632574. If the threshold is
 * exceeded, resets the score to zero and sets alarm flags; otherwise tracks
 * the running maximum.
 *
 * Algorithm:
 *   increment = 1 if actor->field_0x6c != 10 or field_0xa0 not in {2,3,4,5}
 *               3 if actor->field_0x6c == 10 AND field_0xa0 in {2,3,4,5}
 *   actor->field_0x4a += increment
 *   score = actor->field_0x4a
 *   if (ai_globals[3] == 0 && score > ai_globals[+4] && score > 15):
 *       actor->field_0x4a = 0
 *       ai_globals[+3] = 1
 *       actor->field_0x4c = 1     (alarm triggered)
 *       return
 *   if (score > ai_globals[+6]):
 *       ai_globals[+6] = score    (update running max)
 *   actor->field_0x4c = 0         (no alarm)
 *
 * actor->field_0x6c: int16 — actor mode/state (10 = some firing mode)
 * actor->field_0xa0: int16 — some sub-state (2–5 = active sub-states)
 * actor->field_0x4a: int16 — cognition score accumulator
 * actor->field_0x4c: byte  — alarm flag
 * ai_globals+3:      byte  — global alarm triggered flag
 * ai_globals+4:      int16 — score threshold
 * ai_globals+6:      int16 — running max score
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x3bb59.
 * Confirmed: CMP [ECX+0x6c], 0xa; then MOV SI,[ECX+0xa0]; CMP SI,{2,3,4,5} at
 *   0x3bb67–0x3bb8d.
 * Confirmed: LEA EBX,[EBX+EBX*1+1] computes increment (1 or 3) at 0x3bb97.
 * Confirmed: ADD [ECX+0x4a], BX at 0x3bb9b.
 * Confirmed: MOV ESI,[0x632574] loads AI globals ptr at 0x3bb9f.
 * Confirmed: BL = ai_globals[+3] at 0x3bba5; TEST BL,BL / JNZ at
 * 0x3bbac–0x3bbaf. Confirmed: CMP DX,[ESI+4] / CMP DX,0xf at 0x3bbb1–0x3bbb7.
 * Confirmed: reset path: [ECX+0x4a]=0; [EDX+3]=1; [ECX+0x4c]=1 at
 * 0x3bbbd–0x3bbce. Confirmed: max-tracking: CMP DX,[ESI+6] / MOV [ESI+6],DX at
 * 0x3bbd3–0x3bbd9. Confirmed: [ECX+0x4c]=0 (AL=0 from XOR AL,AL at 0x3bb63, not
 * reassigned) at 0x3bbdd. */
void FUN_0003bb50(int actor_handle /* @<eax> */)
{
  char *actor;
  char *ai_globals;
  short score;
  int increment;

  actor = (char *)datum_get(actor_data, actor_handle);
  ai_globals = *(char **)0x632574;

  increment = 1;
  if (*(short *)(actor + 0x6c) == 10) {
    short sub = *(short *)(actor + 0xa0);
    if (sub == 2 || sub == 3 || sub == 4 || sub == 5) {
      increment = 3;
    }
  }

  *(short *)(actor + 0x4a) += (short)increment;
  score = *(short *)(actor + 0x4a);

  if (*(char *)(ai_globals + 3) == 0 && score > *(short *)(ai_globals + 4) &&
      score > 0xf) {
    *(short *)(actor + 0x4a) = 0;
    *(char *)(*(char **)0x632574 + 3) = 1;
    *(char *)(actor + 0x4c) = 1;
    return;
  }

  if (score > *(short *)(ai_globals + 6)) {
    *(short *)(ai_globals + 6) = score;
  }
  *(char *)(actor + 0x4c) = 0;
}

/* FUN_0003bbf0 (0x3bbf0)
 *
 * Initializes actor perception/tracking fields on re-activation. Copies
 * three vector3_t values from actor+0x174..0x18c into actor+0x6fc..0x714,
 * zeroes actor+0x6d0 and actor+0x720, copies the global zero vector
 * (*(float**)0x31fc38) into actor+0x6e0, and sets actor+0x6ec = 0xffff.
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x3bbf9 (EAX=handle,
 * ECX=actor_data). Confirmed: three 12-byte copies actor+0x174→+0x6fc,
 * +0x180→+0x708, +0x18c→+0x714 at 0x3bbfe–0x3bc4f. Confirmed: actor+0x6d0
 * zeroed at 0x3bc54, actor+0x720 zeroed at 0x3bc5a. Confirmed: global zero
 * vector *(float**)0x31fc38 copied 12 bytes → actor+0x6e0 at 0x3bc60–0x3bc7c.
 * Confirmed: actor+0x6ec = 0xffff at 0x3bc7f.
 * Confirmed: called with MOV EAX,ESI / CALL 0x3bbf0 from FUN_0003ec80 at
 * 0x3ed62–0x3ed64. */
void FUN_0003bbf0(int actor_handle /* @<eax> */)
{
  char *actor;
  float *zero_vec;

  actor = (char *)datum_get(actor_data, actor_handle);

  /* copy three facing/aiming/look vectors into tracking slots */
  *(vector3_t *)(actor + 0x6fc) = *(vector3_t *)(actor + 0x174);
  *(vector3_t *)(actor + 0x708) = *(vector3_t *)(actor + 0x180);
  *(vector3_t *)(actor + 0x714) = *(vector3_t *)(actor + 0x18c);

  *(int *)(actor + 0x6d0) = 0;
  *(int *)(actor + 0x720) = 0;

  zero_vec = *(float **)0x31fc38;
  *(float *)(actor + 0x6e0) = zero_vec[0];
  *(float *)(actor + 0x6e4) = zero_vec[1];
  *(float *)(actor + 0x6e8) = zero_vec[2];

  *(short *)(actor + 0x6ec) = (short)0xffff;
}

/* FUN_0003bde0 (0x3bde0) — actor_fill_unit_input_block
 *
 * Populates an input block structure with the unit's world position,
 * velocity vector from object+0x24, physics position via FUN_001a9200,
 * root location, and root parent's object+0x48/0x4c fields.
 *
 * Confirmed: object_get_and_verify_type(unit_handle, 3) at 0x3bdec.
 * Confirmed: object_get_world_position(unit_handle, input_block+0xc) at
 * 0x3bdfb. Confirmed: 12-byte copy from obj+0x24 to input_block+0x18 at
 * 0x3be03. Confirmed: FUN_001a9200(unit_handle, input_block) at 0x3be18.
 * Confirmed: object_get_root_location(unit_handle, input_block+0x2c, 0) at
 * 0x3be24. Confirmed: object_get_root_parent(unit_handle) at 0x3be2a.
 * Confirmed: object_get_and_verify_type(root, -1) at 0x3be32.
 * Confirmed: root_obj+0x48/0x4c copied to input_block+0x24/0x28. */
void FUN_0003bde0(int actor_handle, int unit_handle, char *input_block)
{
  char *unit_obj;
  char *root_obj;
  int root_handle;

  unit_obj = (char *)object_get_and_verify_type(unit_handle, 3);

  object_get_world_position(unit_handle, (vector3_t *)(input_block + 0xc));

  *(int *)(input_block + 0x18) = *(int *)(unit_obj + 0x24);
  *(int *)(input_block + 0x1c) = *(int *)(unit_obj + 0x28);
  *(int *)(input_block + 0x20) = *(int *)(unit_obj + 0x2c);

  FUN_001a9200(unit_handle, (float *)input_block);

  object_get_root_location(unit_handle, (float *)(input_block + 0x2c), 0);

  root_handle = object_get_root_parent(unit_handle);
  root_obj = (char *)object_get_and_verify_type(root_handle, -1);

  *(int *)(input_block + 0x24) = *(int *)(root_obj + 0x48);
  *(int *)(input_block + 0x28) = *(int *)(root_obj + 0x4c);
}

/* FUN_0003be90 (0x3be90) — actor run internal logic / infinite-loop watchdog
 *
 * Runs the actor's decision loop up to 10 times, recording the last 5 action
 * indices in a ring buffer. Each iteration: stores actor->state.action in the
 * ring, increments a counter, advances the ring index mod 5, clears the
 * action-changed flag (actor+0x70), dispatches the actor-type decide_action
 * callback (FUN_0003a840), then clears perception state (FUN_00036860).
 *
 * Loop exit paths:
 *   (a) Normal: BL (previous action-executed result) != 0 AND actor+0x70 == 0
 *       → action completed without requesting a new action.
 *   (b) Hard limit: counter >= 10 → break (error reported below).
 *   (c) Normal: FUN_0001c300 returns 0 AND actor+0x70 == 0 → clean return.
 *
 * After loop: if counter < 10, logs "actor-type %s internal logic error (%s)"
 *   with the current action name and encounter/squad path.
 * If counter >= 10, logs each of the last 5 ring-buffer actions plus an
 *   "infinite decision loop" message.
 * Both paths call display_assert at line 0xd6c, then error() at priority 2,
 * then FUN_0001d030(actor_handle, 0, 0) to force-set action 0 (recover).
 *
 * Confirmed: SUB ESP,0x510 → 1296-byte frame; local_514[1024] at EBP-0x510,
 *   local_114[256] at EBP-0x110, short local_14[5] at EBP-0x10, int local_8
 *   (counter) at EBP-0x4.
 * Confirmed: csmemset(local_14, 0xff, 10) at 0x3bebd (pre-fills ring with -1).
 * Confirmed: ESI = datum_get result (actor record ptr); EDI = ring index (mod
 * 5). Confirmed: BL = result of FUN_0001c300; XOR BL,BL at 0x3beb8 → BL starts
 * 0. Confirmed: loop stored at EBP-0x4 (local_8), incremented at 0x3bedc.
 * Confirmed: MOV EDI,EDX at 0x3beef sets new ring index from IDIV remainder.
 * Confirmed: break-on-BL-nonzero test at 0x3bf06–0x3bf0f.
 * Confirmed: break-on-count>=10 test at 0x3bf11–0x3bf15.
 * Confirmed: FUN_0001c300 called at 0x3bf1b; result into BL at 0x3bf20.
 * Confirmed: early-return (BL==0 && actor[0x70]==0) at 0x3bf25–0x3bf36.
 * Confirmed: global_scenario_get() takes 0 args; PUSH 0xb0, PUSH encounter_idx
 *   at 0x3bf5a–0x3bf5f remain on stack for tag_block_get_element call at
 * 0x3bf6b. Confirmed: ADD ESP,0x28 at 0x3bf9b cleans 10 dwords from
 * encounter-path calls. Confirmed: infinite-loop ring-dump loop: ESI=EDI (start
 * index), advances mod 5, terminates when ESI wraps back to EDI. Confirmed:
 * FUN_0001d030(actor_handle, 0, 0) at 0x3c0a5 with PUSH 0,0,EAX. Confirmed: ADD
 * ESP,0x24 at 0x3c0aa cleans display_assert(4) + error(2) + action_set(3).
 * Inferred: actor+0x6c = state.action (short); actor+0x70 = action-changed flag
 * (byte). Inferred: actor+0x34 = encounter handle (int); actor+0x3a = squad
 * index (short). Inferred: actor+0x4 = actor type index (short). Inferred:
 * FUN_0001c300 = actor_execute_current_action (dispatches via action table).
 * Inferred: FUN_0003a840 = actor_type_decide_action (calls type->decide_action
 * fn ptr). Inferred: FUN_00036860 = actor_clear_perception_state (csmemset
 * actor+0x2ec, 0, 100). Inferred: FUN_0001d030 = actor_set_action (sets action
 * to param_2, clears changed flag). Inferred: FUN_0003a760 =
 * actor_type_get_name (returns actor type name string). Inferred: FUN_0001d5c0
 * = actor_action_get_name (returns action name string). */
void FUN_0003be90(int actor_handle)
{
  char *actor;
  short local_14[5]; /* ring buffer of last 5 action indices */
  int local_8; /* loop counter */
  char local_114[256]; /* encounter name buffer */
  char local_514[1024]; /* error message buffer */
  int edi; /* ring buffer index (mod 5) */
  char bl; /* result of FUN_0001c300 */
  int encounter_idx;
  void *encounter_elem;
  void *squad_elem;
  const char *actor_type_name;
  const char *action_name;
  int i;

  actor = (char *)datum_get(actor_data, actor_handle);
  edi = 0;
  bl = 0;
  local_8 = 0;
  csmemset(local_14, 0xff, 10);

  /* Decision loop: run until action settles or limit hit */
  for (;;) {
    local_14[edi] = *(short *)(actor + 0x6c);
    local_8++;
    edi = (edi + 1) % 5;
    *(char *)(actor + 0x70) = 0;
    FUN_0003a840(actor_handle);
    FUN_00036860(actor_handle);

    /* (a) Previous action completed without requesting change */
    if (bl != 0 && *(char *)(actor + 0x70) == 0) {
      break;
    }
    /* (b) Hard iteration limit */
    if (local_8 >= 10) {
      break;
    }
    /* Execute current action; check if it changed state */
    bl = (char)FUN_0001c300(actor_handle);
    /* (c) No action ran and no change requested → clean return */
    if (bl == 0 && *(char *)(actor + 0x70) == 0) {
      return;
    }
  }

  /* --- Error reporting: build encounter/squad path string --- */
  if (*(int *)(actor + 0x34) == -1) {
    csstrcpy(local_114, "<no encounter>");
  } else {
    encounter_idx = (int)(*(unsigned int *)(actor + 0x34) & 0xffff);
    encounter_elem = tag_block_get_element(
      (char *)global_scenario_get() + 0x42c, encounter_idx, 0xb0);
    squad_elem = tag_block_get_element((char *)encounter_elem + 0x80,
                                       (int)*(short *)(actor + 0x3a), 0xe8);
    crt_sprintf(local_114, "%s/%s", encounter_elem, squad_elem);
  }

  if (local_8 < 10) {
    /* Logic error: action did not converge */
    action_name = (const char *)FUN_0001d5c0(*(short *)(actor + 0x6c));
    actor_type_name = (const char *)FUN_0003a760(*(short *)(actor + 0x4));
    crt_sprintf(local_514, "actor-type %s %s internal logic error (%s)",
                actor_type_name, action_name, local_114);
  } else {
    /* Infinite decision loop: dump ring buffer */
    actor_type_name = (const char *)FUN_0003a760(*(short *)(actor + 0x4));
    crt_sprintf(local_514, "actor-type %s ", actor_type_name);
    i = edi;
    do {
      if (local_14[i] != (short)-1) {
        action_name = (const char *)FUN_0001d5c0(local_14[i]);
        FUN_0008dc30(local_514, action_name);
        FUN_0008dc30(local_514, (const char *)0x256ec8);
      }
      i = (i + 1) % 5;
    } while (i != edi);
    crt_sprintf((char *)0x5ab100, " infinite decision loop (%s)", local_114);
    FUN_0008dc30(local_514, (const char *)0x5ab100);
  }

  display_assert(local_514, "c:\\halo\\SOURCE\\ai\\actors.c", 0xd6c, 0);
  error(2, "AI error condition detected, attempting to recover (please tell "
           "butcher)...");
  FUN_0001d030(actor_handle, 0, 0);
}

/* Set or clear bit 0x800 in actor flags at +0x6d0, and store target at +0x720.
 */
void FUN_0003c2d0(int actor_handle, char flag, int target)
{
  char *actor = (char *)datum_get(actor_data, actor_handle);
  if (flag) {
    *(uint32_t *)(actor + 0x6d0) |= 0x800;
  } else {
    *(uint32_t *)(actor + 0x6d0) &= ~0x800u;
  }
  *(int *)(actor + 0x720) = target;
}

/* Set or clear bit 0x1000 in actor flags at +0x6d0. */
void FUN_0003c330(int actor_handle, char flag)
{
  char *actor = (char *)datum_get(actor_data, actor_handle);
  if (flag) {
    *(uint32_t *)(actor + 0x6d0) |= 0x1000;
  } else {
    *(uint32_t *)(actor + 0x6d0) &= ~0x1000u;
  }
}

/* Set bit 0x2000 in actor flags at +0x6d0. */
void FUN_0003c370(int actor_handle)
{
  char *actor = (char *)datum_get(actor_data, actor_handle);
  *(uint32_t *)(actor + 0x6d0) |= 0x2000;
}

/* FUN_0003c410 (0x3c410) — actor_new
 * Allocate and minimally initialize a new actor datum from the actor_data pool.
 * Looks up the actv (actor variant) tag by actv_tag_index, then the actr
 * (actor) tag it references at offset +0x10. Calls data_new_at_index to
 * reserve a slot, then datum_get to get the record pointer. Writes the actv
 * and actr tag indices into the record (offsets +0x5c, +0x58), copies the
 * actor's type tag word and a swarm flag bit, and zeroes/sentinels every
 * other field (handles to -1, counts to 0, flag bytes to 0 or 1). Zeroes
 * three struct sub-ranges with csmemset. Initializes the actor's per-slot
 * AI state array entry (at DAT_00331f58 + slot*0x657c) with csmemset and
 * sentinel -1 handles. Copies the default facing vector (from
 * *PTR_DAT_0031fc3c) into actor fields +0x5a4, +0x5b0, +0x5bc. If actr[0x90]
 * (never_dormant_chance) exceeds *(float*)0x2533c0, rolls a random float and
 * sets actor+0x376 to 1 if the roll is less than the chance threshold. Calls
 * FUN_0003a810 to dispatch the actor-type init callback. Returns the new actor
 * handle, or -1 on failure (invalid actv, invalid actr tag, or allocation
 * failure).
 *
 * Confirmed: PUSH EAX(param_1)/PUSH 0x61637476 → tag_get at 0x3c42c.
 * Confirmed: actr_tag_index from *(int*)(actv+0x10) at 0x3c431.
 * Confirmed: data_new_at_index(actor_data) at 0x3c453; ADD ESP,0xc cleans
 *   both tag_get calls and data_new_at_index in one batch (0x3c458).
 * Confirmed: datum_get(actor_data, handle) at 0x3c46f; ADD ESP,0x14 at
 *   0x3c56b cleans datum_get(2) + csmemset(0x350)(3) in one batch.
 * Confirmed: actr_tag stored at actor+0x5c at 0x3c484; actr_idx at +0x58.
 * Confirmed: swarm bit = (actr[0] >> 0x1a) & 1 stored at actor+0x6.
 * Confirmed: type word = *(short*)(actr+0x14) stored at actor+0x4.
 * Confirmed: csmemset(actor+0x350, 0, 0x68) at 0x3c566.
 * Confirmed: csmemset(actor+0x4a8, 0, 0x5c) at 0x3c63a.
 * Confirmed: csmemset(actor+0x5c8, -1, 0x10) at 0x3c692.
 * Confirmed: FPU compare at 0x3c5cf: actr[0x90] vs *(float*)0x2533c0.
 * Confirmed: random_math_real result compared with actr[0x90] at 0x3c5f0.
 * Confirmed: state slot = (handle & 0xffff) * 0x657c + *(char**)0x331f58.
 * Confirmed: csmemset(state, 0, 0x657c) at 0x3c75c.
 * Confirmed: FUN_0003a810(handle) at 0x3c79c; ADD ESP,0x30 cleans 12 pushes.
 * Confirmed: return value = ESI = handle (MOV EAX,ESI at 0x3c7a4).
 * Inferred: *(float**)0x31fc3c points to a {1,0,0} default facing vec3.
 * Inferred: 0x2533c0 is a float threshold (0.0f at load, runtime-set later). */
int FUN_0003c410(int actv_tag_index)
{
  char *actv_data;
  char *actr_data;
  int new_handle;
  char *actor;
  int actr_tag_index;
  float *default_facing;
  char *state;
  unsigned int actr_flags;
  float rand_val;
  int *seed;

  if (actv_tag_index == -1) {
    return -1;
  }

  actv_data = (char *)tag_get(0x61637476, actv_tag_index);
  actr_tag_index = *(int *)(actv_data + 0x10);
  if (actr_tag_index == -1) {
    return -1;
  }

  actr_data = (char *)tag_get(0x61637472, actr_tag_index);

  new_handle = data_new_at_index(actor_data);
  if (new_handle == -1) {
    return -1;
  }

  actor = (char *)datum_get(actor_data, new_handle);

  /* Copy actv/actr tag indices and actor-type fields into the actor record. */
  actr_flags = *(unsigned int *)actr_data;
  *(int *)(actor + 0x5c) = actv_tag_index;
  *(char *)(actor + 0x6) = (char)((actr_flags >> 0x1a) & 1);
  *(int *)(actor + 0x58) = actr_tag_index;
  *(short *)(actor + 0x4) = *(short *)(actr_data + 0x14);

  /* Initialize handle/index sentinels and zero-fields. */
  *(int *)(actor + 0x18) = -1;
  *(char *)(actor + 0x1c) = 0;
  *(int *)(actor + 0x34) = -1;
  *(short *)(actor + 0x3a) = (short)-1;
  *(short *)(actor + 0x3c) = (short)-1;
  *(char *)(actor + 0x9) = 0;
  *(int *)(actor + 0x30) = -1;
  *(short *)(actor + 0x38) = (short)-1;
  *(short *)(actor + 0x1e) = 0;
  *(short *)(actor + 0x20) = 0;
  *(int *)(actor + 0x24) = -1;
  *(int *)(actor + 0x28) = -1;
  *(char *)(actor + 0x7) = 1;
  *(char *)(actor + 0x8) = 0;
  *(int *)(actor + 0xc) = -1;
  *(char *)(actor + 0x13) = 1;
  *(char *)(actor + 0x12) = 1;
  *(short *)(actor + 0x4a) = 0;
  *(int *)(actor + 0x50) = -1;
  *(int *)(actor + 0x54) = -1;
  *(short *)(actor + 0x3b8) = (short)-1;
  *(short *)(actor + 0x60) = (short)-1;
  *(short *)(actor + 0x62) = (short)-1;
  *(int *)(actor + 0x64) = -1;
  *(char *)(actor + 0x8e) = 0;
  *(short *)(actor + 0x90) = (short)-1;
  *(int *)(actor + 0x94) = -1;
  *(short *)(actor + 0x6c) = 0;
  *(short *)(actor + 0x6a) = 2;
  *(short *)(actor + 0x6e) = 0;
  *(short *)(actor + 0x72) = 0;
  *(short *)(actor + 0x74) = 0;
  *(int *)(actor + 0x88) = -1;
  *(char *)(actor + 0x98) = 0;

  /* Swarm-component flag: bit 0x15 of actr_flags (re-read after possible
   * aliasing). */
  *(char *)(actor + 0x99) = (char)((*(unsigned int *)actr_data >> 0x15) & 1);

  *(int *)(actor + 0x164) = -1;
  *(int *)(actor + 0x158) = -1;
  *(char *)(actor + 0x1c9) = 0;
  *(char *)(actor + 0x1cc) = 0;
  *(int *)(actor + 0x1d0) = -1;
  *(short *)(actor + 0x1d4) = 0;
  *(int *)(actor + 0x1dc) = -1;

  /* Zero sub-range 0x350..0x3b7 (0x68 bytes). */
  csmemset(actor + 0x350, 0, 0x68);

  /* Initialize fields in 0x350..0x3ff range (after csmemset). */
  *(int *)(actor + 0x370) = -1;
  *(int *)(actor + 0x37c) = -1;
  *(int *)(actor + 0x380) = -1;
  *(int *)(actor + 0x36c) = -1;
  *(int *)(actor + 0x384) = -1;
  *(int *)(actor + 0x388) = -1;
  *(int *)(actor + 0x398) = -1;
  *(int *)(actor + 0x3a0) = -1;
  *(int *)(actor + 0x3a4) = -1;
  *(int *)(actor + 0x3ac) = -1;
  *(int *)(actor + 0x3b0) = -1;
  *(float *)(actor + 0x3b4) = 1.0f;
  *(int *)(actor + 0x390) = -1;
  *(int *)(actor + 0x394) = -1;
  *(int *)(actor + 0x39c) = -1;

  /* Roll a random dormancy check if actr never_dormant_chance > threshold. */
  if (*(float *)0x2533c0 < *(float *)(actr_data + 0x90)) {
    seed = get_global_random_seed_address();
    rand_val = random_math_real((unsigned int *)seed);
    *(char *)(actor + 0x376) = (char)(rand_val < *(float *)(actr_data + 0x90));
  }

  /* Zero actor sub-ranges 0x3e8..0x503 (0x5c bytes) and misc field inits. */
  *(short *)(actor + 0x3e8) = 0;
  *(short *)(actor + 0x400) = 0;
  *(short *)(actor + 0x46c) = 0;
  *(int *)(actor + 0x480) = -1;
  *(int *)(actor + 0x494) = -1;
  csmemset(actor + 0x4a8, 0, 0x5c);

  /* Zero then sentinel-fill 0x5c8..0x5d7 (0x10 bytes with 0xff). */
  *(char *)(actor + 0x504) = 0;
  *(char *)(actor + 0x505) = 0;
  *(short *)(actor + 0x5f2) = 1;
  *(short *)(actor + 0x5f4) = 0;
  *(short *)(actor + 0x5f6) = 0;
  *(short *)(actor + 0x5f8) = 0;
  *(short *)(actor + 0x5fa) = 0;
  *(int *)(actor + 0x61c) = 0;
  *(int *)(actor + 0x610) = -1;
  *(int *)(actor + 0x6a4) = -1;
  *(int *)(actor + 0x6b4) = -1;
  csmemset(actor + 0x5c8, -1, 0x10);

  /* Copy default facing vector {1,0,0} to three actor orientation fields. */
  default_facing = *(float **)0x31fc3c;
  *(float *)(actor + 0x5b0) = default_facing[0];
  *(float *)(actor + 0x5b4) = default_facing[1];
  *(float *)(actor + 0x5b8) = default_facing[2];
  *(float *)(actor + 0x5a4) = default_facing[0];
  *(float *)(actor + 0x5a8) = default_facing[1];
  *(float *)(actor + 0x5ac) = default_facing[2];
  *(float *)(actor + 0x5bc) = default_facing[0];
  *(float *)(actor + 0x5c0) = default_facing[1];
  *(float *)(actor + 0x5c4) = default_facing[2];

  *(short *)(actor + 0x5d8) = (short)-1;
  *(short *)(actor + 0x5f0) = (short)-1;
  *(short *)(actor + 0x544) = 0;
  *(short *)(actor + 0x548) = 0;

  *(char *)(actor + 0x6cc) = 0;
  *(short *)(actor + 0x6ce) = 0x1e;
  *(short *)(actor + 0x268) = 0;
  *(int *)(actor + 0x270) = -1;
  *(int *)(actor + 0x26c) = -1;
  *(int *)(actor + 0x278) = -1;

  FUN_00024b80(new_handle, 0);

  *(int *)(actor + 0x3c0) = -1;

  /* Initialize the per-slot AI state array entry for this actor. */
  state = *(char **)0x331f58 + (new_handle & 0xffff) * 0x657c;
  csmemset(state, 0, 0x657c);
  *(int *)(state + 0x4) = -1;
  *(int *)(state + 0x5c) = -1;
  *(int *)(state + 0xc4) = -1;
  *(int *)(state + 0x104) = -1;
  *(int *)(state + 0x150) = -1;
  *(int *)(state + 0x168) = -1;
  *(int *)(state + 0x18c) = -1;
  *(int *)(state + 0x19c) = -1;
  *(int *)(state + 0x656c) = -1;
  *(short *)(state + 0x6578) = (short)-1;

  /* Dispatch actor-type init callback. */
  FUN_0003a810(new_handle);

  return new_handle;
}

/* FUN_0003c7c0 (0x3c7c0) — actor_variant_setup_unit
 * Initializes a newly created unit from its actor variant (actv) tag data.
 * Sets perception ranges, grenade type, change colors, initial weapon,
 * grenade count, initial equipment, and deaf/blind flags. */
void FUN_0003c7c0(int actv_tag_index, int unit_index)
{
  char *actv_data;
  char *actr_data;
  char *unit_data;
  char *element;
  char *eqip_data;
  char placement[136];
  int object_handle;
  int i;
  int count;
  int *seed;
  float blend;
  float *out_color;
  float *copy_dest;

  actv_data = (char *)tag_get(0x61637476, actv_tag_index);
  actr_data = (char *)tag_get(0x61637472, *(int *)(actv_data + 0x10));
  unit_data = (char *)object_get_and_verify_type(unit_index, 3);

  if (*(float *)(actv_data + 0x200) > 0.0f ||
      *(float *)(actv_data + 0x204) > 0.0f) {
    FUN_001365d0(unit_index, (int)(actv_data + 0x200),
                 (int)(actv_data + 0x204));
  }

  if (*(short *)(actv_data + 0x20c) != 0) {
    *(short *)(unit_data + 0x126) = *(short *)(actv_data + 0x20c);
  }

  count = 0;
  for (i = 0; (short)i < *(int *)(actv_data + 0x22c); i = (int)(short)(count)) {
    element = (char *)tag_block_get_element(actv_data + 0x22c, i, 0x20);
    if ((short)count < 4) {
      out_color = (float *)(unit_data + (i * 3 + 0x4e) * 4);
      seed = get_global_random_seed_address();
      blend = random_math_real((unsigned int *)seed);
      FUN_0007c270(out_color, 1, (float *)element, (float *)(element + 0xc),
                   blend);
      copy_dest = (float *)(unit_data + (i * 3 + 0x5a) * 4);
      copy_dest[0] = out_color[0];
      copy_dest[1] = out_color[1];
      copy_dest[2] = out_color[2];
    }
    count = count + 1;
  }

  if (*(int *)(actv_data + 0x70) != -1) {
    FUN_0013fc20(placement, *(int *)(actv_data + 0x70), unit_index);
    object_handle = FUN_00143c80(placement);
    if (object_handle != -1) {
      if (!unit_enter_seat(unit_index, object_handle, 2)) {
        object_delete(object_handle);
      }
    }
  }

  if (*(short *)(actv_data + 0x180) != -1) {
    seed = get_global_random_seed_address();
    unit_set_grenade_count(unit_index, *(short *)(actv_data + 0x180),
                           random_range((unsigned int *)seed,
                                        *(short *)(actv_data + 0x1d0),
                                        *(short *)(actv_data + 0x1d2) + 1));
  }

  if (*(int *)(actv_data + 0x1cc) != -1) {
    eqip_data = (char *)tag_get(0x65716970, *(int *)(actv_data + 0x1cc));
    if (*(short *)(eqip_data + 0x308) == 0 ||
        *(short *)(eqip_data + 0x308) == 6) {
      error(2, "cannot add grenades or non-powerups to an actor's inventory "
               "as equipment... try using the 'grenade' fields maybe?");
    } else {
      FUN_0013fc20(placement, *(int *)(actv_data + 0x1cc), unit_index);
      object_handle = FUN_00143c80(placement);
      if (object_handle != -1) {
        if (!unit_pickup_equipment(unit_index, object_handle, 1)) {
          object_delete(object_handle);
        }
      }
    }
  }

  if ((*(unsigned char *)actv_data & 0x30) != 0) {
    if ((*(unsigned char *)actv_data & 0x20) != 0) {
      *(unsigned int *)(unit_data + 0x1b4) |= 0x20;
    }
    *(unsigned int *)(unit_data + 0x1b4) |= 0x10;
    *(float *)(unit_data + 0x32c) = 1.0f;
    if ((*(unsigned char *)actr_data & 0x20) != 0) {
      *(float *)(unit_data + 0x330) = 1.0f;
      return;
    }
    *(float *)(unit_data + 0x330) = 0.0f;
  }
}

/* FUN_0003ca40 (0x3ca40) — actor_set_object_activation
 *
 * Sets activation state on the actor's associated objects. Depending on
 * actor type (single-object vs swarm leader), activates/deactivates one
 * object, a linked list of objects, or all swarm member objects.
 * Always calls FUN_0003aca0 at the end regardless of path taken.
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x3ca50.
 * Confirmed: actor+0x8 != 0 guard at 0x3ca5d.
 * Confirmed: actor+0x13 != flag guard at 0x3ca6b.
 * Confirmed: actor+0x6 branches single vs multi at 0x3ca72.
 * Confirmed: actor+0x18 = unit handle (single path) at 0x3cb23.
 * Confirmed: actor+0x24 = object list head, linked via obj+0x1ac.
 * Confirmed: actor+0x28 = swarm handle; datum_get(swarm_data, ...) at 0x3ca8b.
 * Confirmed: swarm+0x2 = count (int16); swarm+0x18[i*4] = handles.
 * Confirmed: object_activate (0x13fb30) when flag==0; FUN_0013fb80 when
 * flag!=0. Confirmed: actor+0x13 = flag written at 0x3cad0. Confirmed:
 * actor+0x14 = 0 (int16) when flag==0 at 0x3cad5. Confirmed:
 * FUN_0003aca0(actor_handle) always called at 0x3cae0. */
void FUN_0003ca40(int actor_handle, char flag)
{
  char *actor;
  char *swarm;
  char *obj;
  int obj_handle;
  int16_t i;

  actor = (char *)datum_get(actor_data, actor_handle);

  if (*(char *)(actor + 0x8) != 0 && *(char *)(actor + 0x13) != flag) {
    if (*(char *)(actor + 0x6) == 0) {
      if (*(int *)(actor + 0x18) != -1) {
        if (flag == 0) {
          object_activate(*(int *)(actor + 0x18));
        } else {
          FUN_0013fb80(*(int *)(actor + 0x18));
        }
      }
    } else if (*(int *)(actor + 0x28) == -1) {
      obj_handle = *(int *)(actor + 0x24);
      while (obj_handle != -1) {
        obj = (char *)object_get_and_verify_type(obj_handle, 3);
        if (flag == 0) {
          object_activate(obj_handle);
        } else {
          FUN_0013fb80(obj_handle);
        }
        obj_handle = *(int *)(obj + 0x1ac);
      }
    } else {
      swarm = (char *)datum_get(swarm_data, *(int *)(actor + 0x28));
      i = 0;
      while (i < *(int16_t *)(swarm + 0x2)) {
        if (flag == 0) {
          object_activate(*(int *)(swarm + 0x18 + (int)i * 4));
        } else {
          FUN_0013fb80(*(int *)(swarm + 0x18 + (int)i * 4));
        }
        i++;
      }
    }
    *(char *)(actor + 0x13) = flag;
    if (flag == 0) {
      *(int16_t *)(actor + 0x14) = 0;
    }
  }

  FUN_0003aca0(actor_handle);
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
  char *unit2;
  char *ai_globals;
  int encounter_handle;
  int weapon_handle;
  float accuracy;
  float boosted;
  float burst_duration;
  float random_val;
  float ammo_fraction;
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
  random_val = random_math_real((unsigned int *)seed);

  unit2 = (char *)object_get_and_verify_type(*(int *)(actor + 0x18), 3);
  weapon_handle =
    unit_get_weapon(*(int *)(actor + 0x18), (int)(*(int16_t *)(unit2 + 0x2a2)));

  /* Check global flag for clearing weapon state */
  ai_globals = *(char **)0x632574;
  if (*(char *)(ai_globals + 0x3b4) == 0 ||
      random_val < *(float *)(tag + 0x1d4)) {
    csmemset(unit + 0x2ce, 0, 2);
  }

  if (weapon_handle != -1) {
    /* Set weapon ammo fraction if tag defines it */
    if (*(float *)(tag + 0x1d8) > 0.0f || *(float *)(tag + 0x1dc) > 0.0f) {
      seed = get_global_random_seed_address();
      ammo_fraction = random_real_range(seed, *(float *)(tag + 0x1d8),
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

/* FUN_0003d6c0 (0x3d6c0) — actor_link_swarm_unit
 *
 * Link a unit into an actor's swarm unit list. If the unit is already linked
 * to this actor as its swarm actor, returns true immediately (no-op).
 * Otherwise, detaches any existing swarm/actor linkage on the unit, validates
 * preconditions, inserts the unit at the head of the actor's swarm-unit linked
 * list, allocates a swarm component if the actor belongs to an encounter swarm,
 * updates encounter bookkeeping, assigns team affiliation, and activates the
 * unit object.
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x3d6d4.
 * Confirmed: object_get_and_verify_type(unit_index, 3) at 0x3d6e1.
 * Confirmed: early-out if unit->swarm_actor_index == actor_handle at
 * 0x3d6f1-0x3d6fe. Confirmed: data_new_at_index(swarm_component_data) if
 * actor->swarm_index != -1 at 0x3d719. Confirmed: error(2, "unable to create
 * any more swarm components...", 0x100) at 0x3d73d. Confirmed:
 * FUN_0003ae60(unit->swarm_actor_index, unit_index) to detach old swarm at
 * 0x3d75c. Confirmed: FUN_0003cc10(unit->actor_index, 0) to detach old actor at
 * 0x3d772. Confirmed: FUN_0003ad80(actor_handle) to detach actor's existing
 * unit at 0x3d784. Confirmed: assert checks (actor->meta.swarm byte at +6, unit
 * counts) at 0x3d78c-0x3d84f. Confirmed: unit->swarm_actor_index = actor_handle
 * at 0x3d855. Confirmed: unit->swarm_next_unit = actor->first_unit at 0x3d85e
 * (unit+0x1ac = actor+0x24). Confirmed: unit->swarm_prev_unit = -1 at 0x3d864
 * (unit+0x1b0). Confirmed: first_unit->swarm_prev_unit = unit_index if
 * first_unit != -1 at 0x3d879-0x3d8b1. Confirmed: actor->first_unit =
 * unit_index at 0x3d8bf (ESI+0x24 = EBX). Confirmed:
 * FUN_0003cb50(actor->swarm_index@eax, new_sc@edi, unit_index@ebx) at 0x3d8c7.
 * Confirmed: actor->swarm_unit_count (short at +0x1e) incremented at 0x3d8d2.
 * Confirmed: short at actor+0x20 incremented at 0x3d8d9.
 * Confirmed: encounter FUN_00059630(actor->encounter_index, unit_index) at
 * 0x3d8f9. Confirmed: unit->team (word at unit+0x68) set from encounter biped
 * data+2 at 0x3d8fe/0x3d908. Confirmed: actor->team (word at actor+0x3e) =
 * unit->team at 0x3d913. Confirmed: FUN_0013ff50(unit_index, 0) at 0x3d917.
 * Confirmed: object_activate(unit_index) or FUN_0013fb80(unit_index) at
 * 0x3d92e/0x3d927. Confirmed: FUN_001adf10(unit_index, 1) at 0x3d939.
 */
int FUN_0003d6c0(int actor_handle, int unit_index)
{
  char *actor;
  char *unit;
  int swarm_component_handle;
  char *first_unit;
  char *biped;
  char result;

  actor = (char *)datum_get(actor_data, actor_handle);
  unit = (char *)object_get_and_verify_type(unit_index, 3);

  /* Early-out: unit is already linked to this actor as swarm actor. */
  if (*(int *)(unit + 0x1a8) == actor_handle) {
    return 1;
  }

  swarm_component_handle = -1;

  /* Allocate swarm component if actor belongs to an encounter swarm. */
  if (*(int *)(actor + 0x28) != -1) {
    swarm_component_handle = data_new_at_index(*(data_t **)0x63259c);
    result = (swarm_component_handle != -1);
    if (!result) {
      error(2, "unable to create any more swarm components (max %d)", 0x100);
      return result;
    }
  }

  /* Detach unit from its old swarm actor if it had one. */
  if (*(int *)(unit + 0x1a8) != -1) {
    FUN_0003ae60(*(int *)(unit + 0x1a8), unit_index);
  }

  /* Detach unit from its old actor if it had one. */
  if (*(int *)(unit + 0x1a4) != -1) {
    FUN_0003cc10(*(int *)(unit + 0x1a4), 0);
  }

  /* Detach actor's current unit if it had one. */
  if (*(int *)(actor + 0x18) != -1) {
    FUN_0003ad80(actor_handle);
  }

  /* Precondition assertions (source line 0x513-0x519). */
  if (*(char *)(actor + 6) == 0) {
    display_assert("actor->meta.swarm", "c:\\halo\\SOURCE\\ai\\actors.c", 0x513,
                   1);
    system_exit(-1);
  }
  if (*(int *)(actor + 0x18) != -1) {
    display_assert("actor->meta.unit_index == NONE",
                   "c:\\halo\\SOURCE\\ai\\actors.c", 0x514, 1);
    system_exit(-1);
  }
  if (*(int *)(unit + 0x1a4) != -1) {
    display_assert("unit->unit.actor_index == NONE",
                   "c:\\halo\\SOURCE\\ai\\actors.c", 0x515, 1);
    system_exit(-1);
  }
  if (*(int *)(unit + 0x1a8) != -1) {
    display_assert("unit->unit.swarm_actor_index == NONE",
                   "c:\\halo\\SOURCE\\ai\\actors.c", 0x516, 1);
    system_exit(-1);
  }
  if (*(short *)(actor + 0x1e) >= 0x10) {
    display_assert(
      "actor->meta.swarm_unit_count < MAXIMUM_NUMBER_OF_UNITS_PER_SWARM",
      "c:\\halo\\SOURCE\\ai\\actors.c", 0x519, 1);
    system_exit(-1);
  }

  /* Link unit into actor's swarm list at the head. */
  *(int *)(unit + 0x1a8) = actor_handle;
  *(int *)(unit + 0x1ac) =
    *(int *)(actor + 0x24); /* unit->swarm_next = old first */
  *(int *)(unit + 0x1b0) = -1; /* unit->swarm_prev = NONE */

  /* If there was a previous first unit, point its prev back to this unit. */
  if (*(int *)(actor + 0x24) != -1) {
    first_unit = (char *)object_get_and_verify_type(*(int *)(actor + 0x24), 3);
    if (*(int *)(first_unit + 0x1b0) != -1) {
      display_assert("swarm_first_unit->unit.swarm_prev_unit_index == NONE",
                     "c:\\halo\\SOURCE\\ai\\actors.c", 0x524, 1);
      system_exit(-1);
    }
    *(int *)(first_unit + 0x1b0) = unit_index;
  }

  /* Update actor's first unit pointer. */
  *(int *)(actor + 0x24) = unit_index;

  /* Register swarm component if allocated. */
  if (*(int *)(actor + 0x28) != -1) {
    FUN_0003cb50(*(int *)(actor + 0x28), swarm_component_handle, unit_index);
  }

  /* Increment swarm unit counts. */
  *(short *)(actor + 0x1e) += 1;
  *(short *)(actor + 0x20) += 1;

  /* Sync encounter data and set team affiliation. */
  if (*(int *)(actor + 0x34) != -1) {
    biped = (char *)datum_get(*(data_t **)0x5ab270, *(int *)(actor + 0x34));
    FUN_00059630(*(int *)(actor + 0x34), unit_index);
    *(short *)(unit + 0x68) = *(short *)(biped + 2);
  }
  *(short *)(actor + 0x3e) = *(short *)(unit + 0x68);

  FUN_0013ff50(unit_index, 0);

  /* Activate unit: if actor is in "active" mode, use deferred activation. */
  if (*(char *)(actor + 0x13) != 0) {
    FUN_0013fb80(unit_index);
  } else {
    object_activate(unit_index);
  }

  FUN_001adf10(unit_index, 1);

  return 1;
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

/* FUN_0003d9f0 (0x3d9f0) — actor_pre_activate_check
 *
 * Validates an actor before activation and updates per-tick AI counters.
 * Returns 1 if the actor may proceed to full activation, 0 if it was erased.
 *
 * Per-tick counter updates (always, before any early-outs):
 *   - word[0x5abc44]++ : total actor count increment
 *   - if actor+0x13 == 0: word[0x5abccc]++  (non-dormant actor count)
 *   - if actor+6 == 0: word[0x5abddc]++ (non-swarm count), else add
 *     short[actor+0x1e] (swarm_unit_count) to word[0x5abddc]
 *   - same conditional logic for word[0x5abe64] gated on actor+0x13==0
 *
 * Error path (swarm actor without swarm cache at actor+0x28 == -1):
 *   - Fires csprintf assert at actors.c line 0xaad (2733).
 *   - Calls FUN_0003d950(actor_handle, 0) to erase units.
 *   - Returns 0.
 *
 * Counter reset block (executed before dormancy/activation checks):
 *   - byte[actor+0x4a4] = 0
 *   - if int[actor+0x78] > 0: decrement; if reaches 0, clear word[actor+0x74]
 *   - if short[actor+0x92] > 0: decrement
 *
 * Activation readiness checks (return 1 to allow activation):
 *   - If actor+0x12 == 0 (no player-presence?) OR combined flags != 0:
 *       call FUN_0003ca40(actor_handle, 0); return 1.
 *   - If actor+0x13 != 0 (dormant): return 1 (dormant actors always pass).
 *   - If FUN_0001d6d0(actor_handle) returns 2 (action already in flight):
 * return 1.
 *   - Encounter validity check: if actor+0x270 != -1:
 *       datum_get(DAT_005ab23c, actor+0x270); check +0x12e, +0x60, +0x127;
 *       if valid encounter and action type in [2,3] → return 1;
 *       if action type in [4,5] and FUN_0001d6d0 returned 3 → return 1.
 *   - FUN_0002a3d0(actor_handle) checks byte at actor+0x4a8 (non-zero =
 * vehicle?): if mode==3 and actor+0x6c==6 and biped+0x62==1 → return 1. if
 * mode==5 and encounter+0x12e!=0 → return 1.
 *   - Increment word[actor+0x14] (idle ticks); if > 0x3b (59): deactivate and
 * return 1.
 *
 * Classification evidence: references actors.c string at 0x3da76 (line 0xaad).
 *   Called by FUN_0003ec80 (actor_activate) at 0x3ecc3; result tested with
 *   TEST AL,AL; JZ 0x3edae.
 *
 * Confirmed: cdecl, single stack arg actor_handle. Return via AL.
 * Confirmed: [EBP-1] initialised to 1 at 0x3da16; set to 0 at 0x3daa3 only.
 *   All exits load AL from [EBP-1], so default return is 1.
 * Confirmed: ESI = datum_get result (actor record pointer) throughout.
 * Confirmed: EDI = actor_handle (from [EBP+0x8]) at 0x3d9fb; preserved until
 *   overwritten by FUN_0001d6d0 return at 0x3db2a, then restored at 0x3db92.
 * Confirmed: encounter data table at DAT_005ab23c (0x5ab23c).
 * Confirmed: FUN_0003ca40(actor_handle, flag) cdecl 2 args — ADD ESP,0x8.
 * Confirmed: FUN_0001d6d0(actor_handle) cdecl 1 arg → short action type in AX.
 *   Return stored in DI; compared as 16-bit (CMP DI,0x2 / CMP DI,0x3).
 * Confirmed: FUN_0002a3d0(actor_handle) cdecl 1 arg → byte at actor+0x4a8.
 * Confirmed: mode==3 path: CMP word[ESI+0x6c],6; CMP word[EBX+0x62],1 (biped
 * rec). EBX = DAT_005ab270 datum_get result (biped record), set at 0x3daf7.
 * Confirmed: mode==5 path: datum_get(DAT_005ab23c, actor+0x470) → check +0x12e.
 * Confirmed: ADD ESP,0x18 at 0x3da9c cleans csprintf(3)+display_assert(1)+
 *   FUN_0003d950(2) = 6 dwords after partial ADD ESP,0xc at 0x3da8b.
 * Inferred: actor+0x13 = dormant flag (byte); actor+6 = swarm flag (byte).
 * Inferred: actor+0x28 = swarm cache handle (int); -1 = no cache.
 * Inferred: actor+0x1e = swarm unit count (short).
 * Inferred: actor+0x78 = timer/countdown int; actor+0x74 = associated mode
 * word. Inferred: actor+0x92 = secondary tick countdown (short). Inferred:
 * actor+0x34 = biped handle (int); DAT_005ab270 = biped data table. Inferred:
 * actor+0xa = actor flags byte; biped+0xc = biped flags byte. Inferred:
 * actor+0x12 = player-proximity or targeting flag (byte). Inferred: actor+0x14
 * = idle tick counter (short); threshold 0x3b (59 ticks). Inferred: actor+0x270
 * = encounter handle (int). Inferred: encounter+0x12e = scripted flag (char);
 * encounter+0x60 = active (char); encounter+0x127 = some exclusion flag (char);
 * encounter+0x24 = type/state short. Inferred: actor+0x4a8 = in-vehicle or
 * mounted flag (byte, read by FUN_0002a3d0). Inferred: actor+0x46c = activation
 * mode (short); 3=biped-ride, 5=encounter-board. Inferred: actor+0x470 =
 * secondary encounter handle (int) used with mode==5. */
char FUN_0003d9f0(int actor_handle)
{
  char *actor;
  char *biped;
  char *encounter;
  char flags;
  short action_type;
  char in_vehicle;
  char ret;

  actor = (char *)datum_get(actor_data, actor_handle);

  ret = 1;

  /* Per-tick counter updates */
  (*(short *)0x5abc44)++;
  if (*(char *)(actor + 0x13) == 0) {
    (*(short *)0x5abccc)++;
  }
  if (*(char *)(actor + 0x6) == 0) {
    (*(short *)0x5abddc)++;
    if (*(char *)(actor + 0x13) == 0) {
      (*(short *)0x5abe64)++;
    }
  } else {
    *(short *)0x5abddc += *(short *)(actor + 0x1e);
    if (*(char *)(actor + 0x13) == 0) {
      *(short *)0x5abe64 += *(short *)(actor + 0x1e);
    }
  }

  /* Swarm actor without a swarm cache: error, erase and bail.
   * NOTE: The binary shares filepath/lineno/halt args across csprintf and
   * display_assert via a partial-cleanup trick (ADD ESP,0xc after csprintf
   * leaves 3 args on stack; PUSH EAX adds reason; CALL display_assert sees 4).
   * In C we write both calls explicitly; the compiler may or may not fold them.
   */
  if (*(char *)(actor + 0x6) != 0 && *(int *)(actor + 0x28) == -1) {
    csprintf(
      (char *)0x5ab100,
      "tried to update a swarm actor without a swarm cache, erasing %d units",
      (int)*(short *)(actor + 0x1e));
    display_assert((char *)0x5ab100, "c:\\halo\\SOURCE\\ai\\actors.c", 0xaad,
                   0);
    FUN_0003d950(actor_handle, 0);
    ret = 0;
    return ret;
  }

  /* Reset per-tick counters on actor */
  *(char *)(actor + 0x4a4) = 0;
  if (*(int *)(actor + 0x78) > 0) {
    *(int *)(actor + 0x78) -= 1;
    if (*(int *)(actor + 0x78) == 0) {
      *(short *)(actor + 0x74) = 0;
    }
  }
  if (*(short *)(actor + 0x92) > 0) {
    *(short *)(actor + 0x92) -= 1;
  }

  /* Resolve biped record if actor has a biped handle */
  biped = 0;
  if (*(int *)(actor + 0x34) != -1) {
    biped = (char *)datum_get(*(data_t **)0x5ab270, *(int *)(actor + 0x34));
  }

  /* Combined flags: actor own flags OR biped flags */
  flags = *(char *)(actor + 0xa);
  if (biped != 0) {
    flags |= *(char *)(biped + 0xc);
  }

  /* Deactivate if no player present or combined flags set */
  if (*(char *)(actor + 0x12) == 0 || flags != 0) {
    FUN_0003ca40(actor_handle, 0);
    return ret;
  }

  /* Dormant actors pass immediately */
  if (*(char *)(actor + 0x13) != 0) {
    return ret;
  }

  /* Check current action type */
  action_type = (short)FUN_0001d6d0(actor_handle);
  if (action_type == 2) {
    return ret;
  }

  /* Encounter validity check */
  if (*(int *)(actor + 0x270) != -1) {
    encounter =
      (char *)datum_get(*(data_t **)0x5ab23c, *(int *)(actor + 0x270));
    if (*(char *)(encounter + 0x12e) != 0 && *(char *)(encounter + 0x60) != 0 &&
        *(char *)(encounter + 0x127) == 0) {
      short enc_state = *(short *)(encounter + 0x24);
      if (enc_state >= 2 && enc_state <= 3) {
        return ret;
      }
      if (enc_state >= 4 && enc_state <= 5 && action_type == 3) {
        return ret;
      }
    }
  }

  /* Check in-vehicle / mounted flag */
  in_vehicle = FUN_0002a3d0(actor_handle);
  if (in_vehicle != 0) {
    if (*(short *)(actor + 0x46c) == 3) {
      /* Biped-ride mode: check biped action state */
      if (*(short *)(actor + 0x6c) == 6 && *(short *)(biped + 0x62) == 1) {
        return ret;
      }
    } else if (*(short *)(actor + 0x46c) == 5) {
      /* Encounter-board mode: check encounter scripted flag */
      encounter =
        (char *)datum_get(*(data_t **)0x5ab23c, *(int *)(actor + 0x470));
      if (*(char *)(encounter + 0x12e) != 0) {
        return ret;
      }
    }
  }

  /* Idle tick counter: if exceeded threshold, force deactivation */
  *(short *)(actor + 0x14) += 1;
  if (*(short *)(actor + 0x14) > 0x3b) {
    FUN_0003ca40(actor_handle, 1);
    return ret;
  }

  return ret;
}

/* FUN_0003dc20 (0x3dc20) — actor_input_update
 *
 * Populates the actor's "input" block (actor+0x120..0x1c4) which describes
 * the actor's perceived threat, orientation vectors, and motion state.
 * This is the per-activation update of the actor's sensory/targeting input.
 *
 * Two major code paths depending on actor+0x6 (swarm flag):
 *
 * SWARM PATH (actor+0x6 != 0):
 *   Computes the centroid of all swarm component positions by:
 *   1. Initing a running sum at swarm+0xc from the PTR_DAT_0031fc1c constant.
 *   2. Iterating over swarm components: for each, datum_get from
 *      swarm_component_data, get the component object, copy its
 *      position relative info, accumulate position into the centroid.
 *   3. Divide by component count to get the average.
 *   4. memset actor+0x120 (0xa8 bytes) to zero.
 *   5. Write sentinel -1 to actor+0x158 and actor+0x164.
 *   6. If actor+0x24 != -1, call FUN_0003bde0 to fill input block.
 *
 * NORMAL PATH (actor+0x6 == 0):
 *   1. object_get_and_verify_type(actor+0x18, 3) to get the biped object.
 *   2. Check biped+0xcc for parent object handle.
 *   3. FUN_0003bde0 to populate actor+0x120 block.
 *   4. FUN_001a9520 to get biped world position into local_pos[3].
 *   5. FUN_0018f3e0 to resolve BSP location; result → actor+0x15d.
 *   6. Extract player-proximity flag from tag: (tag[0] >> 0x15) & 1 →
 * actor+0x99.
 *   7. If parent object is a vehicle (type==1, biped+0x64==1):
 *      - tag_get('vehi', vehicle[0]) → vehicle tag
 *      - set actor+0x158 = parent_handle, actor+0x161=0, actor+0x162=0,
 * +0x15e=0
 *      - if vehicle+0x2d4 == actor+0x18 (driver seat): set +0x15e=1, check
 *        tag flags at +0x2f0 for vehicle type (banshee/warthog bits)
 *      - if vehicle+0x2d8 == actor+0x18 (passenger): set +0x161=1, check speed
 *        via FUN_000211f0
 *      - compute actor+0x160 = (actor+0x15e < 2)
 *      - if vehicle+0x2e4 (preferred_seat_index) != -1: check encounter seat
 *        matching, potentially call FUN_0003baa0
 *   8. If no vehicle: clear actor+0x158=-1, +0x15e=0, +0x160=0, +0x161=0
 *      and call FUN_0003baa0 if sticky_burst active (actor+0x40).
 *   9. Player proximity counter (actor+0x99, scenario player record +0x657a).
 *  10. Walk object child chain (biped+0xc8) to fill actor+0x1b4 (has_weapon)
 *      and actor+0x1b0 (active_grenade_handle).
 *  11. Resolve actor+0x164 (preferred_weapon) from biped if no vehicle.
 *  12. FUN_001a9960 to fill actor+0x174 (facing_vector 3D).
 *  13. If not in vehicle: clamp facing_vector to 0 if zero-length; set
 * +0x17c=0.
 *  14. Compute aiming_vector from object+0x1ec..0x1f4 (or vehicle aiming pos).
 *  15. Compute looking_vector from object+0x210..0x218.
 *  16. Compute up_vector = cross-like product of looking and world_up constant.
 *  17. Compute right_vector = cross product of looking and up.
 *  18. Assert validity of facing, aiming, looking vectors.
 *  19. Copy actor+0x1b8..0x1c4 from biped object offsets 0x90/0x94/0xa8/0xa4.
 *
 * Confirmed: stdcall-like — single param pushed; all callees show stack
 * cleanup. Confirmed: actor_data at [0x6325a4]; swarm_data at [0x6325a0];
 *   swarm_component_data at [0x63259c].
 * Confirmed: tag_get('actr', actor+0x58) at 0x3dc43.
 * Confirmed: tag_get('vehi', vehicle[0]) at 0x3de45–0x3de50.
 * Confirmed: encounter data (player table) at *(data_t**)0x5ab270.
 * Confirmed: scenario player table base via *(int*)0x331f58 * 0x657c stride.
 * Confirmed: global_scenario_get at 0x3e057; tag_block_get_element at 0x3e062.
 * Confirmed: FUN_001ba1f0 = tag_get_for_object (two calls at 0x3e0b2/0x3e0cf).
 * Confirmed: FUN_0008f390 = error_display_string (two calls at
 * 0x3e0c2/0x3e0e0). Confirmed: FUN_0001c270 = encounter_get_squad at
 * 0x3df6d/0x3df83. Confirmed: FUN_000211f0 = actor_get_unit_speed_record at
 * 0x3ded2. Confirmed: FUN_00021fb0 = assert_valid_real_normal3d (3 calls at
 * 0x3e380..0x3e447). Confirmed: FUN_00028610 = assert_valid_real_normal2d at
 * 0x3e4ac. Confirmed: FUN_000a7a30 = object_is_in_team at 0x3e145. Confirmed:
 * FUN_00012f10 = real_vector3d_length at 0x3e22e. Confirmed: FUN_00013010 =
 * real_vector3d_normalize (in-place) at 0x3e33a. Confirmed: world_up constant
 * pointer at *(float**)0x31fc44 (x,y,z). Confirmed: zero-vector pointer at
 * *(float**)0x31fc1c. Confirmed: forward-vector pointer at *(float**)0x31fc3c.
 * Confirmed: float 1.0 at [0x2533c8] (averaging divisor).
 * Confirmed: float 0.0 at [0x2533c0] (length threshold).
 * Confirmed: facing.k assert epsilon at [0x2533d0] (absolute value threshold).
 * Inferred: actor+0x120 block is the actor_input_t (size 0xa8).
 * Inferred: actor+0x158 = vehicle_handle (or -1).
 * Inferred: actor+0x15d = BSP cluster index (byte).
 * Inferred: actor+0x15e = seat_type (short:
 * 0=none,1=driver,2=gunner,4=passenger). Inferred: actor+0x15c = has_flashlight
 * flag. Inferred: actor+0x160 = is_in_open_seat (not shooting seat). Inferred:
 * actor+0x161 = is_passenger flag. Inferred: actor+0x162 = vehicle_moving fast
 * flag. Inferred: actor+0x164 = preferred_weapon_handle (or -1). Inferred:
 * actor+0x174 = facing_vector (real_vector3d). Inferred: actor+0x180 =
 * aiming_vector (real_vector3d). Inferred: actor+0x18c = looking_vector
 * (real_vector3d). Inferred: actor+0x198 = up_vector (real_vector3d). Inferred:
 * actor+0x1a4 = right_vector (real_vector3d). Inferred: actor+0x1b0 =
 * active_grenade_handle (or -1). Inferred: actor+0x1b4 = has_weapon_in_team
 * flag. Inferred: actor+0x1b5 = crouching flag from biped+0x23b. Inferred:
 * actor+0x1b8..0x1c4 = speed/velocity/motion fields from biped. Uncertain:
 * exact semantics of FUN_0008f390 args (priority/event type). Uncertain: player
 * record stride 0x657c and field +0x657a (proximity counter). */
void FUN_0003dc20(int actor_handle)
{
  char *actor;
  char *actr_tag;
  char *swarm;
  char *swarm_comp;
  char *biped;
  char *parent_obj;
  char *obj;
  char *encounter;
  char *squad;
  char *squad2;
  char *dat;
  char *speed_rec;
  char *scenario_base;
  char *scenario_elem;
  float *centroid;
  float *world_up;
  float *zero_vec;
  float *fwd_vec;
  float lx, ly, lz;
  float ux, uy, uz;
  float ax, ay, az;
  int parent_handle;
  int no_return;
  int in_vehicle;
  int bit5;
  char *vehi_tag_data;
  int player_base;
  int tag_flags;
  short prox_ctr;
  short comp_count;
  int i;

  actor = (char *)datum_get(actor_data, actor_handle);
  actr_tag = (char *)tag_get(0x61637472, *(int *)(actor + 0x58));

  if (*(char *)(actor + 0x6) != 0) {
    /* SWARM PATH: compute centroid of all swarm components.
     * Confirmed: swarm record from swarm_data at [0x6325a0].
     * Confirmed: centroid at swarm+0xc (3 floats, init from zero-vector ptr).
     * Confirmed: component count at swarm+2 (short).
     * Confirmed: comp handle array at swarm+0x58 (4 bytes each).
     * Confirmed: component object handle array at swarm+0x18 (4 bytes each).
     * Confirmed: datum_get(swarm_component_data, comp_handle) called TWICE per
     *   iteration (faithful transcription of binary; first result = swarm_comp
     *   for centroid accumulation, second = dat for FUN_001412f0 / weapon
     * handle). Confirmed: FUN_001412f0(obj_h, dat+4) at 0x3dd01. Confirmed:
     * dat+0x10 = no_return (preferred weapon handle or -1). Confirmed: centroid
     * += swarm_comp+4/+8/+0xc (FPU loads from ECX). Confirmed: averaging: 1.0f
     * / count at [0x2533c8] / FILD count. Confirmed: csmemset(actor+0x120, 0,
     * 0xa8) at 0x3dd74–0x3dd82. Confirmed: actor+0x158 = actor+0x164 = -1 at
     * 0x3dd87–0x3dd90. Confirmed: early return if actor+0x24 == -1 at 0x3dd9e.
     * Confirmed: FUN_0003bde0(actor_handle, actor+0x24, actor+0x120) at
     * 0x3dda7. */
    swarm = (char *)datum_get(swarm_data, *(int *)(actor + 0x28));
    zero_vec = *(float **)0x31fc1c;
    centroid = (float *)(swarm + 0xc);
    centroid[0] = zero_vec[0];
    centroid[1] = zero_vec[1];
    centroid[2] = zero_vec[2];

    comp_count = *(short *)(swarm + 2);
    if (comp_count > 0) {
      for (i = 0; (short)i < comp_count; i++) {
        int comp_handle = *(int *)(swarm + 0x58 + (short)i * 4);
        int obj_h = *(int *)(swarm + 0x18 + (short)i * 4);
        /* First datum_get: for centroid accumulation */
        swarm_comp = (char *)datum_get(swarm_component_data, comp_handle);
        biped = (char *)object_get_and_verify_type(obj_h, 3);
        /* Second datum_get on same handle (faithful; see binary
         * 0x3dcd0–0x3dce7) */
        dat = (char *)datum_get(swarm_component_data, comp_handle);
        no_return = -1;
        if (*(short *)(biped + 0x64) == 0) {
          no_return = *(int *)(biped + 0x430);
        }
        object_get_world_position(obj_h, (vector3_t *)(dat + 4));
        *(int *)(dat + 0x10) = no_return;
        /* Accumulate component position (swarm_comp+4/+8/+0xc) into centroid */
        centroid[0] += *(float *)(swarm_comp + 4);
        centroid[1] += *(float *)(swarm_comp + 8);
        centroid[2] += *(float *)(swarm_comp + 0xc);
      }
    }

    comp_count = *(short *)(swarm + 2);
    if (comp_count > 0) {
      float inv = *(float *)0x2533c8 / (float)(int)(short)comp_count;
      centroid[0] *= inv;
      centroid[1] *= inv;
      centroid[2] *= inv;
    }

    csmemset(actor + 0x120, 0, 0xa8);
    *(int *)(actor + 0x158) = -1;
    *(int *)(actor + 0x164) = -1;
    if (*(int *)(actor + 0x24) == -1) {
      return;
    }
    FUN_0003bde0(actor_handle, *(int *)(actor + 0x24), actor + 0x120);
    return;
  }

  /* NORMAL PATH */
  biped = (char *)object_get_and_verify_type(*(int *)(actor + 0x18), 3);
  parent_handle = *(int *)(biped + 0xcc);
  if (parent_handle == -1) {
    parent_obj = (char *)0;
  } else {
    parent_obj = (char *)object_get_and_verify_type(parent_handle, -1);
  }

  FUN_0003bde0(actor_handle, *(int *)(actor + 0x18), actor + 0x120);

  /* Get world position of the biped into local stack buffer (12 bytes).
   * FUN_001a9520(unit_handle, out_pos[3]) writes 3 floats to local_pos.
   * FUN_0018f3e0(actor+0x144, local_pos, NULL) = scenario_location_from_point:
   *   returns byte outdoor flag; cluster stored at actor+0x15d. */
  {
    float local_pos[3];
    FUN_001a9520(*(int *)(actor + 0x18), local_pos);
    *(char *)(actor + 0x15d) =
      FUN_0018f3e0(actor + 0x144, (void *)local_pos, (int16_t *)0);
  }

  /* Extract player-proximity tag bit: (actr_tag[0] >> 0x15) & 1 → actor+0x99 */
  *(char *)(actor + 0x99) = (char)((*(unsigned int *)actr_tag >> 0x15) & 1);

  if (parent_obj == (char *)0 || *(short *)(parent_obj + 0x64) != 1) {
    /* No parent vehicle */
    *(int *)(actor + 0x158) = -1;
    *(short *)(actor + 0x15e) = 0;
    *(char *)(actor + 0x160) = 0;
    *(char *)(actor + 0x161) = 0;
    if (*(char *)(actor + 0x40) != 0) {
      FUN_0003baa0(actor_handle, *(int *)(actor + 0x44),
                   *(short *)(actor + 0x48));
      *(char *)(actor + 0x40) = 0;
    }
  } else {
    /* Parent is a vehicle (parent_obj+0x64 == 1) */
    vehi_tag_data = (char *)tag_get(0x76656869, *(int *)parent_obj);
    *(int *)(actor + 0x158) = parent_handle;
    *(char *)(actor + 0x161) = 0;
    *(char *)(actor + 0x162) = 0;
    *(short *)(actor + 0x15e) = 0;

    /* Check if actor's biped is in the driver seat (vehicle+0x2d4) */
    if (*(int *)(parent_obj + 0x2d4) == *(int *)(actor + 0x18)) {
      *(short *)(actor + 0x15e) = 1;
      tag_flags = *(unsigned int *)(vehi_tag_data + 0x2f0);
      if ((tag_flags & 0x800) != 0) {
        if ((tag_flags & 0x1000) != 0) {
          *(short *)(actor + 0x15e) = 4;
          *(char *)(actor + 0x99) = 1;
        } else if ((tag_flags & 0x2000) != 0) {
          *(short *)(actor + 0x15e) = (short)((~(tag_flags >> 0xe) & 1) | 2);
        }
      }
    }

    /* Check if actor's biped is in the passenger seat (vehicle+0x2d8) */
    if (*(int *)(parent_obj + 0x2d8) == *(int *)(actor + 0x18)) {
      *(char *)(actor + 0x161) = 1;
      speed_rec = (char *)FUN_000211f0(actor_handle);
      *(char *)(actor + 0x162) =
        (*(float *)(speed_rec + 0x14c) > *(float *)0x2533c0) ? 1 : 0;
    }

    *(char *)(actor + 0x160) = (*(short *)(actor + 0x15e) < 2) ? 1 : 0;

    /* Seat preference matching: check encounter vehicle+seat vs. preferred */
    if (*(short *)(parent_obj + 0x2e4) != (short)-1) {
      if ((*(unsigned int *)(actor + 0x34) & 0xffff) ==
          (unsigned int)(int)(short)*(short *)(parent_obj + 0x2e4)) {
        if (*(short *)(parent_obj + 0x2e6) == (short)-1 ||
            *(short *)(actor + 0x3a) == *(short *)(parent_obj + 0x2e6)) {
          goto LAB_3e02c;
        }
        encounter = (char *)datum_get(*(data_t **)0x5ab270,
                                      *(unsigned int *)(actor + 0x34));
        if (*(short *)(encounter + 0x62) > 0) {
          squad = (char *)FUN_0001c270(encounter, *(short *)(actor + 0x3a));
          squad2 = (char *)FUN_0001c270(
            encounter, (int)(short)*(short *)(parent_obj + 0x2e6));
          if (*(char *)(squad + 0x10) != 0 && *(char *)(squad2 + 0x10) != 0) {
            goto LAB_3e02c;
          }
        }
      }

      if (*(char *)(actor + 0x40) == 0) {
        int unit_h = *(int *)(actor + 0x34);
        *(int *)(actor + 0x44) = unit_h;
        *(short *)(actor + 0x48) = *(short *)(actor + 0x3a);
        *(char *)(actor + 0x40) = 1;
        if (unit_h != -1) {
          char *enc2 = (char *)datum_get(*(data_t **)0x5ab270, unit_h);
          *(char *)(enc2 + 0x1e) = 1;
        }
      }
      FUN_0003baa0(actor_handle, (int)(short)*(short *)(parent_obj + 0x2e4),
                   (short)*(short *)(parent_obj + 0x2e6));
    }
  }

LAB_3e02c:
  /* Player proximity counter update.
   * Confirmed: player_base = (actor_handle & 0xffff) * 0x657c +
   * *(int*)0x331f58. Confirmed: global_scenario_get() takes 0 args (0x3e057);
   * encounter_idx and 0xb0 remain on stack as args to tag_block_get_element
   * (0x3e062). Confirmed: proximity counter at player_base + 0x657a (short).
   * Confirmed: FUN_001ba1f0(actor+0x58, scenario_elem) at 0x3e0b2 / 0x3e0cf.
   *   ADD ESP,4 after call cleans 1 arg; scenario_elem stays on stack for
   *   FUN_0008f390(2, string, tag, scenario_elem) ADD ESP,0x10 cleanup.
   * Confirmed: 0x3e074 JZ: vehicle case (0x99!=0) increments when bit 5 clear;
   *   on-foot case (0x99==0) increments when bit 5 set. */
  if (*(unsigned int *)(actor + 0x34) != 0xffffffff) {
    int encounter_idx = (int)(*(unsigned int *)(actor + 0x34) & 0xffff);
    player_base = (actor_handle & 0xffff) * 0x657c + *(int *)0x331f58;
    scenario_base = (char *)global_scenario_get();
    scenario_elem =
      (char *)tag_block_get_element(scenario_base + 0x42c, encounter_idx, 0xb0);
    in_vehicle = *(char *)(actor + 0x99);
    bit5 = (*(unsigned char *)(scenario_elem + 0x20) & 0x20) != 0;
    /* Increment when vehicle && bit clear, or on-foot && bit set */
    if (in_vehicle ? !bit5 : bit5) {
      prox_ctr = *(short *)(player_base + 0x657a);
      if (prox_ctr < 0x96) {
        prox_ctr++;
        *(short *)(player_base + 0x657a) = prox_ctr;
        if (prox_ctr == 0x96) {
          /* Proximity threshold reached — fire notification */
          /* tag_get_name(actor_tag_handle) — 1 arg; scenario_elem is stack
           * residue from earlier push, acts as extra variadic arg to error().
           */
          const char *tag_name = tag_get_name(*(int *)(actor + 0x58));
          if (in_vehicle) {
            error(2, (const char *)0x257300, tag_name, scenario_elem);
          } else {
            error(2, (const char *)0x2572b0, tag_name, scenario_elem);
          }
        }
      }
    } else {
      *(short *)(player_base + 0x657a) = 0;
    }
  }

  /* Crouching/grenade flags from biped */
  *(char *)(actor + 0x1b5) = (*(unsigned char *)(biped + 0x23b) > 0) ? 1 : 0;
  *(char *)(actor + 0x1b4) = 0;
  *(int *)(actor + 0x1b0) = -1;

  /* Walk child object chain to find equipped weapon and grenade.
   * Confirmed: chain starts at biped+0xc8; next ptr at obj+0xc4.
   * Confirmed: obj+0x64 == 0 → weapon; obj+0x64 == 5 → equipment/grenade.
   * Confirmed: FUN_000a7a30(actor+0x3e, obj+0x68) for team membership check.
   * Confirmed: grenade condition: obj+0x1dc < 0 OR (actor+0x280==2 AND
   *   child == actor+0x28c). */
  {
    int child = *(int *)(biped + 0xc8);
    while (child != -1) {
      obj = (char *)object_get_and_verify_type(child, -1);
      if (*(short *)(obj + 0x64) == 0) {
        if (game_allegiance_get_team_is_friendly(*(short *)(actor + 0x3e),
                                                 *(short *)(obj + 0x68))) {
          *(char *)(actor + 0x1b4) = 1;
        }
      } else if (*(short *)(obj + 0x64) == 5) {
        if (*(char *)(obj + 0x1dc) < 0 || (*(short *)(actor + 0x280) == 2 &&
                                           child == *(int *)(actor + 0x28c))) {
          *(int *)(actor + 0x1b0) = child;
        }
      }
      child = *(int *)(obj + 0xc4);
    }
    /* After loop, biped ptr (pfVar2/EBX=[EBP-8]) used for biped+0x64 test below
     */
  }

  /* Preferred weapon / flashlight from biped unit record (if on foot, no
   * vehicle). Confirmed: object_get_and_verify_type(actor+0x18, 1) at 0x3e1b6.
   * Confirmed: unit+0x459 > 5 → flashlight on (actor+0x15c = 1).
   * Confirmed: actor+0x164..+0x170 from unit+0x434..+0x440. */
  *(char *)(actor + 0x15c) = 0;
  *(int *)(actor + 0x164) = -1;
  if (*(short *)(biped + 0x64) == 0 && *(int *)(actor + 0x158) == -1) {
    char *unit_obj =
      (char *)object_get_and_verify_type(*(int *)(actor + 0x18), 1);
    if ((unsigned char)*(char *)(unit_obj + 0x459) > 5) {
      *(char *)(actor + 0x15c) = 1;
    }
    *(int *)(actor + 0x164) = *(int *)(unit_obj + 0x434);
    *(int *)(actor + 0x168) = *(int *)(unit_obj + 0x438);
    *(int *)(actor + 0x16c) = *(int *)(unit_obj + 0x43c);
    *(int *)(actor + 0x170) = *(int *)(unit_obj + 0x440);
  }

  /* Facing vector: from vehicle if riding, otherwise from own biped.
   * Confirmed: CMP word[ESI+0x15e],0 at 0x3e1f7; if >= 1 use actor+0x158,
   *   else use actor+0x18. FUN_001a9960(handle, actor+0x174) at 0x3e215. */
  {
    int facing_src;
    if (*(short *)(actor + 0x15e) >= 1) {
      facing_src = *(int *)(actor + 0x158);
    } else {
      facing_src = *(int *)(actor + 0x18);
    }
    FUN_001a9960(facing_src, actor + 0x174);
  }

  /* On foot: if facing vector is zero-length, use default forward vector.
   * Otherwise clear facing_vector.z (force 2D). Vehicle: skip.
   * Confirmed: FUN_00012f10(actor+0x174) length check at 0x3e22e.
   * Confirmed: FCOMP [0x2533c0] (0.0f) at 0x3e233; if length <= 0 copy fwd_vec.
   * Confirmed: else case: MOV dword[ESI+0x17c],0 at 0x3e243 (clears .z). */
  if (*(char *)(actor + 0x99) == 0) {
    if (magnitude3d((float *)(actor + 0x174)) <= *(float *)0x2533c0) {
      fwd_vec = *(float **)0x31fc3c;
      *(float *)(actor + 0x174) = fwd_vec[0];
      *(float *)(actor + 0x178) = fwd_vec[1];
      *(float *)(actor + 0x17c) = fwd_vec[2];
    } else {
      *(float *)(actor + 0x17c) = 0.0f;
    }
  }

  /* Aiming vector.
   * Passenger (actor+0x161 != 0): get from vehicle or own position.
   * Otherwise: copy from biped+0x1ec..0x1f4.
   * Confirmed: object_get_and_verify_type(actor+0x158, 2) at 0x3e277.
   * Confirmed: tag_get('vehi', vehicle[0]) for flag check at 0x3e280/0x3e286.
   * Confirmed: flag bit 0x100 at tag+0x2f0 → if set, use FUN_001a9960 for
   *   aiming from own position; else copy vehicle+0x1ec..0x1f4.
   * Confirmed: biped+0x1ec..0x1f4 for on-foot path (0x3e2ae/0x3e2c6).
   * Confirmed: biped+500 (0x1f4) used as int (500 == 0x1f4). */
  if (*(char *)(actor + 0x161) == 0) {
    /* On foot or driver: use biped aiming vector */
    *(int *)(actor + 0x180) = *(int *)(biped + 0x1ec);
    *(int *)(actor + 0x184) = *(int *)(biped + 0x1f0);
    *(int *)(actor + 0x188) = *(int *)(biped + 0x1f4);
  } else {
    /* Passenger: use vehicle aiming vector (or self position if flag set) */
    char *vehi_obj =
      (char *)object_get_and_verify_type(*(int *)(actor + 0x158), 2);
    char *vehi_tag2 = (char *)tag_get(0x76656869, *(int *)vehi_obj);
    if ((*(unsigned int *)(vehi_tag2 + 0x2f0) & 0x100) == 0) {
      *(int *)(actor + 0x180) = *(int *)(vehi_obj + 0x1ec);
      *(int *)(actor + 0x184) = *(int *)(vehi_obj + 0x1f0);
      *(int *)(actor + 0x188) = *(int *)(vehi_obj + 0x1f4);
    } else {
      FUN_001a9960(*(int *)(actor + 0x18), actor + 0x180);
    }
  }

  /* Looking vector from biped+0x210..0x218 */
  *(int *)(actor + 0x18c) = *(int *)(biped + 0x210);
  *(int *)(actor + 0x190) = *(int *)(biped + 0x214);
  *(int *)(actor + 0x194) = *(int *)(biped + 0x218);

  /* Compute up-vector = cross-like product of looking x world_up
   * (directly transcribed FPU sequence; see disassembly 0x3e300-0x3e337)
   * world_up = *(float**)0x31fc44 (pointer to {wx,wy,wz} constant)
   * looking  = actor+0x18c {lx,ly,lz}
   *
   * up.x = ly*wx - wy*lx
   * up.y = wz*lx - lz*wx
   * up.z = lz*wy - wz*ly
   */
  {
    world_up = *(float **)0x31fc44;
    lx = *(float *)(actor + 0x18c);
    ly = *(float *)(actor + 0x190);
    lz = *(float *)(actor + 0x194);
    ux = ly * world_up[0] - world_up[1] * lx;
    uy = world_up[2] * lx - lz * world_up[0];
    uz = lz * world_up[1] - world_up[2] * ly;
    *(float *)(actor + 0x198) = ux;
    *(float *)(actor + 0x19c) = uy;
    *(float *)(actor + 0x1a0) = uz;
    normalize3d((float *)(actor + 0x198));
  }

  /* Compute right-vector = cross(looking, up)
   * (directly transcribed FPU sequence; see disassembly 0x3e341-0x3e37a)
   * right.x = lx*uy - ly*ux
   * right.y = ux*lz - lx*uz
   * right.z = ly*uz - lz*uy
   * (stored after normalize call on up, using post-normalize ux/uy/uz)
   */
  {
    ux = *(float *)(actor + 0x198);
    uy = *(float *)(actor + 0x19c);
    uz = *(float *)(actor + 0x1a0);
    ax = lx * uy - ly * ux;
    ay = ux * lz - lx * uz;
    az = ly * uz - lz * uy;
    *(float *)(actor + 0x1a4) = ax;
    *(float *)(actor + 0x1a8) = ay;
    *(float *)(actor + 0x1ac) = az;
  }

  /* Assert vector validity.
   * Pattern: csprintf fills error_string_buffer with the formatted message,
   * then display_assert(buffer, file, line, halt) is called as FUN_0008d9f0.
   * The file/line/halt args are pre-pushed before csprintf; csprintf result
   * (pointer to buffer) is then pushed as arg1. See disasm 0x3e38c–0x3e3d8.
   * Confirmed: FUN_00021fb0 = assert_valid_real_normal3d (3-component).
   * Confirmed: FUN_00028610 = assert_valid_real_normal2d (2-component). */
  if (!valid_real_normal3d((float *)(actor + 0x174))) {
    csprintf(error_string_buffer, "%s: assert_valid_real_normal3d(%f, %f, %f)",
             "&actor->input.facing_vector", (double)*(float *)(actor + 0x174),
             (double)*(float *)(actor + 0x178),
             (double)*(float *)(actor + 0x17c));
    display_assert(error_string_buffer, "c:\\halo\\SOURCE\\ai\\actors.c", 0xcf1,
                   1);
    system_exit(-1);
  }
  if (!valid_real_normal3d((float *)(actor + 0x180))) {
    csprintf(error_string_buffer, "%s: assert_valid_real_normal3d(%f, %f, %f)",
             "&actor->input.aiming_vector", (double)*(float *)(actor + 0x180),
             (double)*(float *)(actor + 0x184),
             (double)*(float *)(actor + 0x188));
    display_assert(error_string_buffer, "c:\\halo\\SOURCE\\ai\\actors.c", 0xcf2,
                   1);
    system_exit(-1);
  }
  if (!valid_real_normal3d((float *)(actor + 0x18c))) {
    csprintf(error_string_buffer, "%s: assert_valid_real_normal3d(%f, %f, %f)",
             "&actor->input.looking_vector", (double)*(float *)(actor + 0x18c),
             (double)*(float *)(actor + 0x190),
             (double)*(float *)(actor + 0x194));
    display_assert(error_string_buffer, "c:\\halo\\SOURCE\\ai\\actors.c", 0xcf3,
                   1);
    system_exit(-1);
  }

  /* Additional on-foot facing vector assertions.
   * Confirmed: valid_real_normal2d checks xy-plane normal validity.
   * Confirmed: FABS + FCOMP double[0x2533d0] at 0x3e503/0x3e505; uses double.
   */
  if (*(char *)(actor + 0x99) == 0) {
    if (!valid_real_normal2d((float *)(actor + 0x174))) {
      csprintf(error_string_buffer, "%s: assert_valid_real_normal2d(%f, %f)",
               "(real_vector2d *) &actor->input.facing_vector",
               (double)*(float *)(actor + 0x174),
               (double)*(float *)(actor + 0x178));
      display_assert(error_string_buffer, "c:\\halo\\SOURCE\\ai\\actors.c",
                     0xcf6, 1);
      system_exit(-1);
    }
    if (fabsf(*(float *)(actor + 0x17c)) >= (float)*(double *)0x2533d0) {
      display_assert("realcmp(actor->input.facing_vector.k, 0.0f)",
                     "c:\\halo\\SOURCE\\ai\\actors.c", 0xcf7, 1);
      system_exit(-1);
    }
  }

  /* Copy motion/velocity fields from biped */
  *(int *)(actor + 0x1b8) = *(int *)(biped + 0x90);
  *(int *)(actor + 0x1bc) = *(int *)(biped + 0x94);
  *(int *)(actor + 0x1c0) = *(int *)(biped + 0xa8);
  *(int *)(actor + 0x1c4) = *(int *)(biped + 0xa4);
}

/* FUN_0003e7a0 (0x3e7a0) — actor_apply_control_data
 *
 * Applies a pre-computed AI control snapshot (actor+0x6d0..0x720 range) to the
 * unit owned by the actor, performing vector validity assertions first. Called
 * as the final step of actor_activate (FUN_0003ec80) after all AI subsystems
 * have been initialized.
 *
 * Confirmed: actor_handle passed in EAX (@<eax>, regparm). MOV EAX,ESI at
 *   caller 0x3ed9c before CALL 0x3e7a0 — ESI = actor_handle throughout caller.
 * Confirmed: datum_get(actor_data, actor_handle) at 0x3e7b1; result → ESI.
 * Confirmed: object_get_and_verify_type(actor+0x18, 3) at 0x3e7be; result → EBX
 *   (unit type tag record, used for actor_type->0x1c8 check).
 * Confirmed: animation_state byte = byte[actor+0x6dc*2 + 0x256c94] at 0x3e7c5.
 *   MOVSX EAX,word[ESI+0x6dc]; MOV CL,byte[EAX*2+0x256c94].
 * Confirmed: control_flags word at actor+0x6d0 → struct+0x02 at 0x3e7d6.
 * Confirmed: primary_trigger dword at actor+0x720 → struct+0x18 at 0x3e7e1.
 * Confirmed: throttle xyz from actor+0x6e0..0x6e8 → struct+0x0c..0x14 via LEA
 *   ECX,[ESI+0x6e0] at 0x3e7ea; stores to [EBP-0x34/0x30/0x2c] (= +0x0c/10/14).
 * Confirmed: aiming_speed byte at actor+0x6f8 → struct+0x01 at 0x3e801.
 * Confirmed: facing_vector from actor+0x6fc..0x704 → struct+0x1c..0x24 at
 *   0x3e80a (LEA EAX,[ESI+0x6fc]).
 * Confirmed: aiming_vector from actor+0x708..0x710 → struct+0x28..0x30 at
 *   0x3e821 (LEA ECX,[ESI+0x708]).
 * Confirmed: looking_vector from actor+0x714..0x71c → struct+0x34..0x3c at
 *   0x3e838 (LEA EDX,[ESI+0x714]).
 * Confirmed: weapon/grenade/zoom_level set to 0xffff via OR EDI,0xffffffff at
 *   0x3e846; three word stores [EBP-0x3c/0x3a/0x38] (= struct+0x04/06/08).
 * Confirmed: actor+0x99 byte check (0 = on-foot) gates valid_real_normal2d
 *   check at 0x3e85e/0x3e867/0x3e869.
 * Confirmed: valid_real_normal2d(&facing) at 0x3e86f; string
 *   "(real_vector2d *) &control_data.facing_vector", line 0xf06.
 * Confirmed: valid_real_normal3d(&facing) at 0x3e8c1; string
 *   "&control_data.facing_vector", line 0xf08.
 * Confirmed: valid_real_normal3d(&aiming) at 0x3e91a; string
 *   "&control_data.aiming_vector", line 0xf09.
 * Confirmed: valid_real_normal3d(&looking) at 0x3e973; string
 *   "&control_data.looking_vector", line 0xf0a.
 * Confirmed: throttle FABS+FCOMP against double[0x2573d8]=1.0 at
 *   0x3e9c8..0x3e9fc; JP branches: outer condition fires if ANY |throttle|
 * > 1.0. Assert string: "(fabs...)...", line 0xf0b; display_assert without
 * csprintf. Confirmed: [EBX+0x1c8] != -1 test at 0x3ea1d; if non-(-1), call
 *   player_input_enabled(); return early if returns non-zero at 0x3ea2a.
 * Confirmed: actor+7 != 0 → FUN_001adf10(actor+0x18, 1) + clear actor+7 at
 *   0x3ea2e..0x3ea43.
 * Confirmed: unit_set_control(actor+0x18, &control) at 0x3ea4f.
 * Confirmed: actor+0x6ec != -1 → FUN_001b1a20(actor+0x18,
 * (int)(uint16)(actor+0x6ec), actor+0x6f0) at 0x3ea65; XOR EAX,EAX; MOV
 * AX,word[ESI+0x6ec] = zero-extend. Confirmed: actor+0x6d4 > 0 →
 * FUN_001a8190(actor+0x18, MOVSX(actor+0x6d4), actor+0x6d8) at 0x3ea85; MOVSX
 * EDX,AX sign-extends the short. Inferred: actor+0x6dc = animation_state_index
 * (maps through 0x256c94 table). Inferred: actor+0x6f0 = pointer/data block
 * passed as 3rd arg to FUN_001b1a20. Inferred: actor+0x6d4 =
 * animation_tick_count (short); actor+0x6d8 = animation control flags dword for
 * FUN_001a8190. Uncertain: exact semantics of FUN_001b1a20's 2nd arg
 * (zero-extended index).
 */
void FUN_0003e7a0(int actor_handle /* @<eax> */)
{
  char *actor;
  char *unit;
  char control[0x40];

  actor = (char *)datum_get(actor_data, actor_handle);
  unit = (char *)object_get_and_verify_type(*(int *)(actor + 0x18), 3);

  /* Build the unit control struct from actor's control data fields */
  *(uint8_t *)(control + 0x00) =
    ((uint8_t *)0x256c94)[*(int16_t *)(actor + 0x6dc) * 2];
  *(uint8_t *)(control + 0x01) = *(uint8_t *)(actor + 0x6f8);
  *(uint16_t *)(control + 0x02) = *(uint16_t *)(actor + 0x6d0);
  *(uint16_t *)(control + 0x04) = (uint16_t)0xffff;
  *(uint16_t *)(control + 0x06) = (uint16_t)0xffff;
  *(uint16_t *)(control + 0x08) = (uint16_t)0xffff;
  *(float *)(control + 0x0c) = *(float *)(actor + 0x6e0);
  *(float *)(control + 0x10) = *(float *)(actor + 0x6e4);
  *(float *)(control + 0x14) = *(float *)(actor + 0x6e8);
  *(uint32_t *)(control + 0x18) = *(uint32_t *)(actor + 0x720);
  *(float *)(control + 0x1c) = *(float *)(actor + 0x6fc);
  *(float *)(control + 0x20) = *(float *)(actor + 0x700);
  *(float *)(control + 0x24) = *(float *)(actor + 0x704);
  *(float *)(control + 0x28) = *(float *)(actor + 0x708);
  *(float *)(control + 0x2c) = *(float *)(actor + 0x70c);
  *(float *)(control + 0x30) = *(float *)(actor + 0x710);
  *(float *)(control + 0x34) = *(float *)(actor + 0x714);
  *(float *)(control + 0x38) = *(float *)(actor + 0x718);
  *(float *)(control + 0x3c) = *(float *)(actor + 0x71c);

  /* Validate facing vector: 2D check only when on-foot (actor+0x99 == 0) */
  if (*(char *)(actor + 0x99) == 0) {
    if (!valid_real_normal2d((float *)(control + 0x1c))) {
      csprintf(error_string_buffer, "%s: assert_valid_real_normal2d(%f, %f)",
               "(real_vector2d *) &control_data.facing_vector",
               (double)*(float *)(control + 0x1c),
               (double)*(float *)(control + 0x20));
      display_assert(error_string_buffer, "c:\\halo\\SOURCE\\ai\\actors.c",
                     0xf06, 1);
      system_exit(-1);
    }
  }

  /* Validate facing, aiming, looking vectors as 3D normals */
  if (!valid_real_normal3d((float *)(control + 0x1c))) {
    csprintf(error_string_buffer, "%s: assert_valid_real_normal3d(%f, %f, %f)",
             "&control_data.facing_vector", (double)*(float *)(control + 0x1c),
             (double)*(float *)(control + 0x20),
             (double)*(float *)(control + 0x24));
    display_assert(error_string_buffer, "c:\\halo\\SOURCE\\ai\\actors.c", 0xf08,
                   1);
    system_exit(-1);
  }
  if (!valid_real_normal3d((float *)(control + 0x28))) {
    csprintf(error_string_buffer, "%s: assert_valid_real_normal3d(%f, %f, %f)",
             "&control_data.aiming_vector", (double)*(float *)(control + 0x28),
             (double)*(float *)(control + 0x2c),
             (double)*(float *)(control + 0x30));
    display_assert(error_string_buffer, "c:\\halo\\SOURCE\\ai\\actors.c", 0xf09,
                   1);
    system_exit(-1);
  }
  if (!valid_real_normal3d((float *)(control + 0x34))) {
    csprintf(error_string_buffer, "%s: assert_valid_real_normal3d(%f, %f, %f)",
             "&control_data.looking_vector", (double)*(float *)(control + 0x34),
             (double)*(float *)(control + 0x38),
             (double)*(float *)(control + 0x3c));
    display_assert(error_string_buffer, "c:\\halo\\SOURCE\\ai\\actors.c", 0xf0a,
                   1);
    system_exit(-1);
  }

  /* Validate throttle components are all <= 1.0 */
  if (fabsf(*(float *)(control + 0x0c)) > (float)*(double *)0x2573d8 ||
      fabsf(*(float *)(control + 0x10)) > (float)*(double *)0x2573d8 ||
      fabsf(*(float *)(control + 0x14)) > (float)*(double *)0x2573d8) {
    display_assert("(fabs(control_data.throttle.i) <= 1.0f) && "
                   "(fabs(control_data.throttle.j) <= 1.0f) && "
                   "(fabs(control_data.throttle.k) <= 1.0f)",
                   "c:\\halo\\SOURCE\\ai\\actors.c", 0xf0b, 1);
    system_exit(-1);
  }

  /* If unit has a controlling player (actor_type at unit+0x1c8 != -1) and
   * player input is enabled, skip applying AI control */
  if (*(int *)(unit + 0x1c8) != -1 && player_input_enabled()) {
    return;
  }

  /* If actor was flagged for control reset (actor+7 != 0), apply reset
   * input to unit and clear the flag */
  if (*(char *)(actor + 7) != 0) {
    FUN_001adf10(*(int *)(actor + 0x18), 1);
    *(char *)(actor + 7) = 0;
  }

  /* Apply the control snapshot to the unit */
  unit_set_control(*(int *)(actor + 0x18), control);

  /* If an animation is pending (actor+0x6ec != -1), apply it */
  if ((uint16_t) * (uint16_t *)(actor + 0x6ec) != 0xffff) {
    unit_apply_animation_impulse(*(int *)(actor + 0x18),
                                 (int)(uint16_t) * (uint16_t *)(actor + 0x6ec),
                                 actor + 0x6f0);
  }

  /* If animation ticks are pending (actor+0x6d4 > 0), advance animation */
  if ((int16_t) * (int16_t *)(actor + 0x6d4) > 0) {
    FUN_001a8190(*(int *)(actor + 0x18),
                 (int)(int16_t) * (int16_t *)(actor + 0x6d4),
                 *(int *)(actor + 0x6d8));
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

/* FUN_0003edc0 (0x3edc0) — allocate and initialize an actor record, then link
 * it to its unit.
 *
 * For individual (non-swarm) actors (flags==0): verifies the unit can accept an
 * actor (object_try_and_get_and_verify_type succeeds and bit 2 at +0xb6 is
 * clear). For swarm actors (flags!=0): iterates the encounter's actor list
 * looking for an existing swarm actor that has the same actv_tag, is not at the
 * exclude handle, has fewer than 16 swarm units, and (if param6==0) matches the
 * squad index.
 *
 * Then creates a fresh actor datum via FUN_0003c410 (which allocates from
 * actor_data and initializes all fields). Sets encounter/squad assignment via
 * FUN_00059740 (no encounter) or FUN_0005d200 (with encounter). Sets the
 * encounter_flag, squad starting location index, squad position index,
 * swarm-flag, and marker byte on the actor record.
 *
 * Validates that the actor variant's swarm flag matches the actor type's swarm
 * flag (from the actor_type field at actor+4 via FUN_0003a800). On mismatch,
 * prints a warning and destroys the allocated actor.
 *
 * Finally links the unit to the actor: FUN_0003eab0 for individual,
 * FUN_0003d6c0 for swarm. On swarm link failure, destroys the actor if it has
 * no swarm units.
 *
 * Returns actor handle, or -1 on failure.
 *
 * Confirmed: 12 cdecl args (ADD ESP,0x30 at 0x3f2a1 in FUN_0003f030).
 * Confirmed: iter[3] at [EBP-0xc]: FUN_00059a00 writes iter[0..2], FUN_00059a50
 *   returns datum_get(actor_data,iter[1]) and advances iter[2] to next handle.
 * Confirmed: actor_data (DAT_006325a4) at 0x3ee88, encounter_data (0x5ab270) at
 *   0x3eeaa. Confirmed: handle-tag construction (MOVSX+SHL+OR) at
 * 0x3eec2-0x3eece. Confirmed: FUN_0003a800 takes int16_t actor_type, returns
 * char swarm flag. Confirmed: strings "swarm" at 0x256cd4, "individual" at
 * 0x256d2c, format string at 0x257468. */
int FUN_0003edc0(char flags, int unit_index, int actv_tag_index,
                 int encounter_index, int squad_index, char param6,
                 int exclude_actor_handle, char encounter_flag,
                 short starting_location_index, short squad_position_index,
                 unsigned short param11, char param12)
{
  char *actor;
  char actor_is_swarm;
  char type_is_swarm;
  char *encounter_ptr;
  int actor_handle;
  int iter[3];
  int current_actor;
  short default_pos;
  const char *type_str;
  const char *variant_name;

  if (unit_index == -1 || actv_tag_index == -1) {
    return -1;
  }

  if (flags == 0) {
    /* Individual actor: check the unit is valid and can accept an actor. */
    actor = (char *)object_try_and_get_and_verify_type(unit_index, 1);
    if (actor == NULL || (*(unsigned char *)(actor + 0xb6) & 4) != 0) {
      return -1;
    }
  } else {
    /* Swarm actor: search existing encounter actors for a matching swarm actor
     * to attach to. */
    FUN_00059a00(iter, encounter_index);
    actor = (char *)FUN_00059a50(iter);
    current_actor = iter[1];
    while (actor != 0) {
      if (*(char *)(actor + 0x6) != 0 &&
          current_actor != exclude_actor_handle &&
          *(short *)(actor + 0x1e) < 0x10 &&
          *(int *)(actor + 0x5c) == actv_tag_index &&
          (param6 != 0 || *(short *)(actor + 0x3a) == (short)squad_index)) {
        if (current_actor != -1) {
          actor_handle = current_actor;
          goto actor_found;
        }
        break;
      }
      actor = (char *)FUN_00059a50(iter);
      current_actor = iter[1];
    }
  }

  /* Allocate a new actor datum from actor_data. */
  actor_handle = FUN_0003c410(actv_tag_index);
  if (actor_handle == -1) {
    return -1;
  }
  actor = (char *)datum_get(actor_data, actor_handle);

  /* Assign encounter/squad. */
  if (encounter_index == -1) {
    FUN_00059740(actor_handle);
  } else {
    encounter_ptr = (char *)datum_get(*(data_t **)0x5ab270, encounter_index);
    if ((encounter_index & 0xffff0000) == 0) {
      encounter_index =
        (encounter_index & 0xffff) | ((int)*(short *)encounter_ptr << 0x10);
    }
    FUN_0005d200(actor_handle, encounter_index, squad_index, 0);
  }

  /* Set encounter membership flag and handle special init for encounter actors.
   */
  if (encounter_flag != 0) {
    *(short *)(actor + 0x6a) = 0;
    if (*(char *)(actor + 8) != 0) {
      FUN_0003ca40(actor_handle, 0);
    }
  } else {
    *(short *)(actor + 0x6a) = 2;
  }

  /* Set squad starting location and position indices. */
  *(short *)(actor + 0x60) = starting_location_index;
  *(short *)(actor + 0x62) = squad_position_index;
  if (squad_position_index == -1 || squad_position_index == 0) {
    default_pos = (short)FUN_0001d730(starting_location_index);
    *(short *)(actor + 0x62) = default_pos;
  }

  /* Store remaining actor fields. */
  actor_is_swarm = *(char *)(actor + 0x6);
  *(char *)(actor + 0x8e) = 0;
  *(short *)(actor + 0x92) = 2;
  *(short *)(actor + 0x90) = (short)param11;
  *(char *)(actor + 0x68) = param12;

  /* Validate swarm flag matches actor type. */
  type_is_swarm = FUN_0003a800((int16_t) * (short *)(actor + 0x4));
  if (actor_is_swarm != type_is_swarm) {
    type_str = "swarm";
    if (actor_is_swarm == 0) {
      type_str = "individual";
    }
    variant_name = tag_name_strip_path(tag_get_name(actv_tag_index));
    error(2,
          "%s actor variant %s cannot have type %s (swarm flag does not match)",
          type_str, variant_name);
    FUN_0003cc10(actor_handle, 0);
    return -1;
  }

actor_found:
  /* Link unit to actor. */
  if (flags == 0) {
    FUN_0003eab0(actor_handle, unit_index);
  } else {
    if (FUN_0003d6c0(actor_handle, unit_index) == 0) {
      actor = (char *)datum_get(actor_data, actor_handle);
      if (*(short *)(actor + 0x1e) == 0) {
        FUN_0003cc10(actor_handle, 0);
      }
      FUN_0003aca0(-1);
      return -1;
    }
  }
  FUN_0003aca0(actor_handle);
  return actor_handle;
}

/* FUN_0003f030 (0x3f030) — create an AI actor with its associated unit object.
 * Resolves actor variant tag, initializes object placement, spawns the unit,
 * then creates the actor record. Returns actor handle or -1 on failure.
 *
 * Confirmed: 6 cdecl args (ADD ESP,0x2c at caller in ai.c).
 * Confirmed: tag_get('actv', actv_tag_index) at 0x3f071.
 * Confirmed: use_major_variant selects actv+0x30 alternate variant at 0x3f082.
 * Confirmed: tag_get('actr', [actv+0x10]) at 0x3f0c5 for actor definition
 * flags. Confirmed: FUN_0013fc20(placement, [actv+0x20], -1) at 0x3f0d9.
 * Confirmed: position copied from starting_location[0..8] to placement+0x18 at
 *   0x3f0e0-0x3f0f6.
 * Confirmed: FUN_0010cc70(placement+0x34, starting_location+0xC) at 0x3f0f9.
 * Confirmed: FUN_00143c80(placement) at 0x3f10d creates the unit.
 * Confirmed: FUN_0003c7c0(actv_tag_index, unit_index) at 0x3f202 (2 cdecl args,
 *   ADD ESP,0x8).
 * Confirmed: FUN_0003edc0 with 12 cdecl args at 0x3f2a1 (ADD ESP,0x30).
 * Confirmed: FUN_0003aca0(actor_handle) at 0x3f33c on success.
 * Confirmed: object_delete(unit_index) at 0x3f32a on actor creation failure.
 * Confirmed: FUN_00054220(combined_idx, scenario, buf, 256) at 0x3f16b/0x3f2f8
 *   with pre-pushed args from global_scenario_get (ADD ESP,0x10 cleans 4 args).
 * Confirmed: error(2, format, tag_name, encounter_name) at 0x3f194/0x3f321
 *   with pre-pushed encounter_name from stack (ADD ESP,0x10/0x14). */
int FUN_0003f030(int actv_tag_index, int encounter_index, int squad_index,
                 void *starting_location, char use_major_variant, int16_t team)
{
  char *actv_data;
  char *actr_data;
  char placement[0x88];
  char name_buffer[256];
  int unit_index;
  int actor_index;
  char *encounter_elem;
  char *squad_elem;
  char actor_flag;
  char encounter_flag;
  short sVar1;
  short sVar7;
  const char *tag_name;
  unsigned int combined_index;
  char *scenario_ptr;

  if (starting_location == NULL) {
    display_assert("starting_location", "c:\\halo\\SOURCE\\ai\\actors.c", 0x25b,
                   1);
    system_exit(-1);
  }

  FUN_00144b50();

  actv_data = (char *)tag_get(0x61637476, actv_tag_index);

  if (use_major_variant != 0) {
    actv_tag_index = *(int *)(actv_data + 0x30);
    if (actv_tag_index == -1) {
      display_assert("actor_variant_definition_index != NONE",
                     "c:\\halo\\SOURCE\\ai\\actors.c", 0x265, 1);
      system_exit(-1);
    }
    actv_data = (char *)tag_get(0x61637476, actv_tag_index);
  }

  actr_data = (char *)tag_get(0x61637472, *(int *)(actv_data + 0x10));

  FUN_0013fc20(placement, *(int *)(actv_data + 0x20), -1);

  *(int *)(placement + 0x18) = *(int *)((char *)starting_location);
  *(int *)(placement + 0x1C) = *(int *)((char *)starting_location + 4);
  *(int *)(placement + 0x20) = *(int *)((char *)starting_location + 8);

  FUN_0010cc70((float *)(placement + 0x34),
               *(float *)((char *)starting_location + 0xC));

  *(int16_t *)(placement + 0x16) = team;

  unit_index = FUN_00143c80(placement);

  if (unit_index == -1) {
    if (encounter_index == -1) {
      csstrcpy(name_buffer, "<encounterless>");
    } else {
      scenario_ptr = (char *)global_scenario_get();
      combined_index =
        (((unsigned int)(squad_index & 0xff) | 0xffff8000u) << 16) |
        ((unsigned int)encounter_index & 0xffff);
      FUN_00054220(combined_index, scenario_ptr, name_buffer, 256);
    }
    tag_name = tag_name_strip_path(tag_get_name(actv_tag_index));
    error(2, "WARNING: cannot create unit for actor %s %s", tag_name,
          name_buffer);
    return -1;
  }

  if (encounter_index != -1) {
    encounter_elem = (char *)tag_block_get_element(
      (char *)global_scenario_get() + 0x42c, encounter_index & 0xffff, 0xb0);
    if (encounter_elem != NULL) {
      tag_block_get_element(encounter_elem + 0x80, squad_index, 0xe8);
    }
  }

  actor_flag = (char)((*(unsigned int *)actr_data >> 26) & 1);
  encounter_flag = 0;
  sVar1 = 0;
  sVar7 = 0;

  FUN_0003c7c0(actv_tag_index, unit_index);

  if (encounter_index != -1) {
    encounter_elem = (char *)tag_block_get_element(
      (char *)global_scenario_get() + 0x42c, encounter_index & 0xffff, 0xb0);
    squad_elem =
      (char *)tag_block_get_element(encounter_elem + 0x80, squad_index, 0xe8);
    sVar1 = *(short *)(squad_elem + 0x24);
    sVar7 = *(short *)(squad_elem + 0x26);
    encounter_flag =
      (char)((*(unsigned int *)(encounter_elem + 0x20) >> 4) & 1);
  }

  if (*(short *)((char *)starting_location + 0x16) > 0) {
    sVar1 = *(short *)((char *)starting_location + 0x16);
  }
  if (*(short *)((char *)starting_location + 0x14) > 0) {
    sVar7 = *(short *)((char *)starting_location + 0x14);
  }

  actor_index =
    FUN_0003edc0(actor_flag, unit_index, actv_tag_index, encounter_index,
                 squad_index, 0, -1, encounter_flag, sVar1, sVar7,
                 *(unsigned short *)((char *)starting_location + 0x1a),
                 (short)*(signed char *)((char *)starting_location + 0x12));

  if (actor_index == -1) {
    if (encounter_index == -1) {
      csstrcpy(name_buffer, "<encounterless>");
    } else {
      scenario_ptr = (char *)global_scenario_get();
      combined_index =
        (((unsigned int)(squad_index & 0xff) | 0xffff8000u) << 16) |
        ((unsigned int)encounter_index & 0xffff);
      FUN_00054220(combined_index, scenario_ptr, name_buffer, 256);
    }
    tag_name = tag_name_strip_path(tag_get_name(actv_tag_index));
    error(2, "WARNING: cannot create actor %s %s", tag_name, name_buffer);
    object_delete(unit_index);
    return -1;
  }

  FUN_0003aca0(actor_index);
  return actor_index;
}
