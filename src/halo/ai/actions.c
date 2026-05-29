/* actions.c — AI actor action dispatch.
 *
 * Corresponds to actions.obj.
 * Assertion path: c:\halo\SOURCE\ai\actions.c
 */

#include "../../common.h"

/* FUN_0001c030 (0x1c030) — Initialize actor guard state based on combat status.
 * Sets guard mode (0x3e8) to 3/5/1 depending on whether the actor is a
 * designated combatant, has a valid encounter with positive attack count,
 * or is in a default state. Also sets 0x3fc=3 and clears flags at
 * 0x424-0x428, 0x454. */
void FUN_0001c030(int actor_handle)
{
  char *actor;

  actor = (char *)datum_get(actor_data, actor_handle);
  if (*(char *)(actor + 0x504) != '\0') {
    *(int16_t *)(actor + 0x3e8) = 3;
    *(int16_t *)(actor + 0x3ec) = 0;
  } else if (*(char *)(actor + 0x1cc) == '\0' && *(int *)(actor + 0x1d0) != -1 &&
             *(int16_t *)(actor + 0xa8) > 0) {
    *(int16_t *)(actor + 0x3e8) = 5;
    *(int16_t *)(actor + 0x3ec) = 1;
    *(int *)(actor + 0x3f0) = *(int *)(actor + 0x1d0);
  } else {
    *(int16_t *)(actor + 0x3e8) = 1;
  }
  *(int16_t *)(actor + 0x3fc) = 3;
  *(char *)(actor + 0x454) = 0;
  *(char *)(actor + 0x426) = 0;
  *(char *)(actor + 0x427) = 0;
  *(char *)(actor + 0x428) = 0;
  *(char *)(actor + 0x424) = 0;
  *(char *)(actor + 0x425) = 0;
}

/* FUN_0001c0e0 (0x1c0e0) — Initialize a wait action state buffer.
 * Clears 0x18 bytes at state_data, fills timing/mode fields, and returns 1
 * unless the actor is in a vehicle (actor+0x160 != 0). */
char FUN_0001c0e0(int actor_handle, char param_2, int state_data)
{
  char *actor;

  actor = (char *)datum_get(actor_data, actor_handle);
  assert_halt(state_data != 0);
  csmemset((void *)state_data, 0, 0x18);
  if (*(char *)(actor + 0x160) != '\0') {
    return 0;
  }
  *(char *)(state_data + 1) = *(char *)(actor + 0x1cc);
  *(char *)(state_data + 2) = param_2;
  *(int *)(state_data + 8) = game_time_get();
  *(int16_t *)(state_data + 0xe) = 0;
  *(int16_t *)(state_data + 0xc) = 0x78;
  *(char *)(state_data + 3) = 1;
  *(int16_t *)(state_data + 0x10) = random_range(
      (unsigned int *)get_global_random_seed_address(), 300, 600);
  return 1;
}

/* FUN_0001c190 (0x1c190) — Tick down actor wait/guard timers.
 * Decrements actor+0xac, 0xaa, and 0xa8 counters, triggering sound events and
 * state changes when they reach zero. */
void FUN_0001c190(int actor_handle)
{
  char *actor;
  int16_t sVar1;

  actor = (char *)datum_get(actor_data, actor_handle);
  if (*(int16_t *)(actor + 0xac) > 0) {
    sVar1 = *(int16_t *)(actor + 0xac) - 1;
    *(int16_t *)(actor + 0xac) = sVar1;
    if (sVar1 == 0) {
      if (*(int *)(actor + 0x18) != -1) {
        FUN_00046f10(0x11, *(int *)(actor + 0x18), -1, -1, -1, -1, 0);
      }
      *(int16_t *)(actor + 0xac) = random_range(
          (unsigned int *)get_global_random_seed_address(), 300, 600);
    }
  }
  if (*(int16_t *)(actor + 0xaa) > 0) {
    sVar1 = *(int16_t *)(actor + 0xaa) - 1;
    *(int16_t *)(actor + 0xaa) = sVar1;
    if (sVar1 == 0) {
      if (*(char *)(actor + 0x9d) != '\0' && *(int *)(actor + 0x18) != -1) {
        FUN_00046f10(0x14, *(int *)(actor + 0x18), -1, -1, -1, -1, 0);
      }
      *(char *)(actor + 0x9c) = 1;
    }
  }
  if (*(char *)(actor + 0x9f) == '\0' && *(int16_t *)(actor + 0xa8) > 0) {
    *(int16_t *)(actor + 0xa8) = *(int16_t *)(actor + 0xa8) - 1;
  }
}

/* actor_action_perform (0x1c300) — actor_execute_current_action
 *
 * Dispatches the current action's execute handler via the action_definitions
 * table. Returns the handler's result, or 0 if no handler is set.
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x1c310.
 * Confirmed: actor+0x6c = action index (short), asserted in [0,14).
 * Confirmed: table at 0x253fb8, stride 0x38 (execute handler at +0x14 in
 * entry). Confirmed: handler called with (actor_handle), returns int32_t.
 * Confirmed: returns 0 when handler is NULL (XOR BL,BL; MOV AL,BL at 0x1c369).
 */
int32_t actor_action_perform(int actor_handle)
{
  typedef int32_t (*action_execute_fn_t)(int);

  char *actor;
  short action;
  action_execute_fn_t handler;

  actor = (char *)datum_get(actor_data, actor_handle);
  action = *(short *)(actor + 0x6c);

  assert_halt(action >= 0 && action < 14);

  handler = *(action_execute_fn_t *)(0x253fb8 + action * 0x38);
  if (handler != NULL) {
    return handler(actor_handle);
  }
  return 0;
}

/* actor_action_update (0x1c370) — actor_action_update
 *
 * Dispatches the current action's update handler (table+0x1c) for the given
 * actor. Called each tick from the actor update loop after the decision logic
 * has run.
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x1c37f.
 * Confirmed: actor+0x6c = action index (short), asserted in [0, 14) at line
 *   0x9e (158).
 * Confirmed: table at 0x253fbc (action_definitions base 0x253fa0 + entry
 *   stride 0x38 + field offset 0x1c), stride 0x38.
 * Confirmed: handler called with (actor_handle); no return value used by
 *   caller (void dispatch).
 */
void actor_action_update(int actor_handle)
{
  typedef void (*action_update_fn_t)(int);

  char *actor;
  short action;
  action_update_fn_t handler;

  actor = (char *)datum_get(actor_data, actor_handle);
  action = *(short *)(actor + 0x6c);

  assert_halt(action >= 0 && action < 14);

  handler = *(action_update_fn_t *)(0x253fbc + action * 0x38);
  if (handler != NULL) {
    handler(actor_handle);
  }
}

/* actor_action_control (0x1c3e0) — actor_action_notify
 *
 * Dispatches the current action's notify handler (table+0x20) for the given
 * actor. Called each tick from the actor update loop after actor_action_update,
 * as part of the secondary per-tick action dispatch sequence.
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x1c3ef.
 * Confirmed: actor+0x6c = action index (short), asserted in [0, 14) at line
 *   0xad (173).
 * Confirmed: table at 0x253fc0 (action_definitions base 0x253fa0 + entry
 *   stride 0x38 + field offset 0x20), stride 0x38.
 * Confirmed: handler called with (actor_handle); no return value used by
 *   caller (void dispatch).
 * Inferred: handler name "notify" — binary only confirms it is the table+0x20
 *   slot; the semantic role is not directly evidenced.
 */
void actor_action_control(int actor_handle)
{
  typedef void (*action_notify_fn_t)(int);

  char *actor;
  short action;
  action_notify_fn_t handler;

  actor = (char *)datum_get(actor_data, actor_handle);
  action = *(short *)(actor + 0x6c);

  assert_halt(action >= 0 && action < 14);

  handler = *(action_notify_fn_t *)(0x253fc0 + action * 0x38);
  if (handler != NULL) {
    handler(actor_handle);
  }
}

/* actor_action_replace_prop (0x1c450)
 * Dispatch action-specific prop replacement for an actor.
 *
 * Looks up the actor via actor_data, validates the action index is in range,
 * then dispatches to the action's prop_replace handler (if any) through
 * the action_definitions table at 0x253fcc (stride 0x38, function pointer
 * at offset 0 of each entry).
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x1c45f.
 * Confirmed: actor+0x6c is short action index.
 * Confirmed: assert at line 0xbe (190) checks 0 <= action < 14.
 * Confirmed: table at 0x253fcc, stride 0x38, first field is function pointer.
 * Confirmed: indirect call passes (actor_handle, old_prop, new_prop).
 * Confirmed: __FILE__ = "c:\halo\SOURCE\ai\actions.c". */
void actor_action_replace_prop(int actor_handle, int old_prop, int new_prop)
{
  typedef void (*action_prop_replace_fn_t)(int, int, int);

  char *actor;
  short action;
  action_prop_replace_fn_t handler;

  actor = (char *)datum_get(actor_data, actor_handle);
  action = *(short *)(actor + 0x6c);

  assert_halt(action >= 0 && action < 14);

  handler = *(action_prop_replace_fn_t *)(0x253fcc + action * 0x38);
  if (handler != NULL) {
    handler(actor_handle, old_prop, new_prop);
  }
}

/* actor_action_flush_position_indices (0x1c4c0)
 *
 * Dispatches the current action's handler at table slot +0x30 (0x253fd0,
 * stride 0x38) for the given actor. The semantic role of this slot is not
 * directly evidenced by the binary; it is the field immediately after the
 * prop_replace handler (+0x2c) in each action_definitions entry.
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x1c4cf.
 * Confirmed: actor+0x6c = action index (short), asserted in [0, 14) at
 *   line 0xcd (205).
 * Confirmed: IMUL ECX,ECX,0x38; MOV EAX,[ECX+0x253fd0] at 0x1c50f.
 * Confirmed: handler called with (actor_handle); TEST EAX,EAX guards call.
 * Confirmed: __FILE__ "c:\halo\SOURCE\ai\actions.c".
 */
void actor_action_flush_position_indices(int actor_handle)
{
  typedef void (*action_slot30_fn_t)(int);

  char *actor;
  short action;
  action_slot30_fn_t handler;

  actor = (char *)datum_get(actor_data, actor_handle);
  action = *(short *)(actor + 0x6c);

  assert_halt(action >= 0 && action < 14);

  handler = *(action_slot30_fn_t *)(0x253fd0 + action * 0x38);
  if (handler != NULL) {
    handler(actor_handle);
  }
}

/* actor_action_flush_structure_indices (0x1c530) —
 * actor_action_flush_structure_indices
 *
 * Dispatches the current action's handler at table slot +0x34 (0x253fd4,
 * stride 0x38) for the given actor. Semantically "flush structure indices" —
 * the table slot immediately after slot+0x30.
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x1c53f.
 * Confirmed: actor+0x6c = action index (short), asserted in [0, 14) at
 *   line 0xdc (220).
 * Confirmed: IMUL EAX,EAX,0x38; MOV ECX,[EAX+0x253fd4] at 0x1c573/0x1c57a.
 * Confirmed: handler called with (actor_handle); TEST ECX,ECX guards call.
 * Confirmed: __FILE__ "c:\halo\SOURCE\ai\actions.c".
 */
void actor_action_flush_structure_indices(int actor_handle)
{
  typedef void (*action_slot34_fn_t)(int);

  char *actor;
  short action;
  action_slot34_fn_t handler;

  actor = (char *)datum_get(actor_data, actor_handle);
  action = *(short *)(actor + 0x6c);

  assert_halt(action >= 0 && action < 14);

  handler = *(action_slot34_fn_t *)(0x253fd4 + action * 0x38);
  if (handler != NULL) {
    handler(actor_handle);
  }
}

/* actor_action_handle_panic_from_surprise (0x1c5a0)
 *
 * Checks if an actor should enter a surprise-panic state. Sets the panic
 * substate (actor+0x308) and prop index (actor+0x30c) when the actor's
 * surprise-panic flag (actor+0x2f0) is set AND the actr tag allows it
 * (flag 0x400). Clears the flag after handling.
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x1c5af.
 * Confirmed: tag_get(0x61637472, actor+0x58) loads actr tag.
 * Confirmed: actor+0x2f0 = surprise-panic flag; actor+0x308 = panic substate
 *   (short), clamped min to 7; actor+0x30c = prop index; actor+0x2f4 = default
 *   prop index to use.
 * Confirmed: assert at line 0x210 checks panic state consistency.
 * Inferred: "surprise" type because flag at +0x2f0 and prop from +0x2f4.
 */
int actor_action_handle_panic_from_surprise(int actor_handle)
{
  char *actor;
  int *actr_tag;
  short panic_type;
  int result;

  actor = (char *)datum_get(actor_data, actor_handle);
  actr_tag = (int *)tag_get(0x61637472, *(int *)(actor + 0x58));
  result = 0;
  if ((*(char *)(actor + 0x2f0) != '\0') &&
      ((*(unsigned int *)actr_tag & 0x400) != 0)) {
    panic_type = *(short *)(actor + 0x308);
    if ((panic_type == 0) || (*(int *)(actor + 0x30c) == -1)) {
      *(int *)(actor + 0x30c) = *(int *)(actor + 0x2f4);
    }
    if (panic_type < 8) {
      panic_type = 7;
    }
    *(short *)(actor + 0x308) = panic_type;
    *(char *)(actor + 0x2f0) = 0;
    result = 1;
  }
  assert_halt(*(short *)(actor + 0x308) == 0 || *(int *)(actor + 0x30c) != 0);
  return result;
}

/* actor_action_handle_panic_from_damage (0x1c660)
 *
 * Checks if an actor should enter a damage-panic state. Sets panic substate
 * (actor+0x308, clamped to min 1) and prop index (actor+0x30c) when the
 * damage-panic flag (actor+0x2ec) is set, the actor is in a networked or
 * client context, and the actor's current damage pain boost exceeds the tag
 * threshold. Clears flag after handling.
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x1c66f.
 * Confirmed: tag_get(0x61637472, actor+0x58) loads actr tag.
 * Confirmed: actor+0x2ec = damage-panic flag; actor+0x308 = panic substate
 *   (short); actor+0x30c = prop index; actor+0x1c0 = pain boost (float);
 *   actr_tag+0x2ac = pain threshold (float).
 * Confirmed: game_connection() != 0 or !DAT_005ac9c8 enables the check.
 * Confirmed: FUN_0002fa70(actor_handle, 1) = get best target prop.
 * Confirmed: assert at line 0x228 checks panic state consistency.
 */
char actor_action_handle_panic_from_damage(int actor_handle)
{
  char *actor;
  int actr_tag;
  short panic_type;
  int result;

  actor = (char *)datum_get(actor_data, actor_handle);
  actr_tag = (int)tag_get(0x61637472, *(int *)(actor + 0x58));
  result = 0;
  if (*(char *)(actor + 0x2ec) != '\0') {
    if ((game_connection() != 0) || (*(char *)0x5ac9c8 == '\0')) {
      if (*(float *)(actor + 0x1c0) <= *(float *)(actr_tag + 0x2ac))
        goto check_assert_damage;
    }
    panic_type = *(short *)(actor + 0x308);
    if ((panic_type == 0) || (*(int *)(actor + 0x30c) == -1)) {
      *(int *)(actor + 0x30c) = actor_get_best_damaging_prop(actor_handle, 1);
    }
    if (*(short *)(actor + 0x308) < 2) {
      *(short *)(actor + 0x308) = 1;
    }
    *(char *)(actor + 0x2ec) = 0;
    result = 1;
  }
check_assert_damage:
  assert_halt(*(short *)(actor + 0x308) == 0 || *(int *)(actor + 0x30c) != 0);
  return result;
}

/* actor_action_handle_panic_from_burning_to_death (0x1c750)
 *
 * Checks if an actor should panic because it is burning to death. Sets panic
 * substate (actor+0x308, clamped to min 0xc) and prop index (actor+0x30c)
 * when the burning-death flag (actor+0x1b5) is set. Looks up the responsible
 * unit's vehicle/turret mount as the prop source.
 *
 * Confirmed: datum_get(actor_data, actor_handle); actor+0x1b5 = on-fire flag.
 * Confirmed: actor+0x18 = unit handle; object_get_and_verify_type(..., 3).
 * Confirmed: ai_get_responsible_unit(unit+0x3c0, 1);
 * prop_get_active_by_unit_index. Confirmed: actor+0x308 substate clamped min
 * 0xc (12). Confirmed: actor+0x30c prop index set only if substate==0 or
 * current==-1.
 */
int actor_action_handle_panic_from_burning_to_death(int actor_handle)
{
  char *actor;
  int unit;
  int responsible;
  int prop_handle;
  short panic_type;
  int result;

  actor = (char *)datum_get(actor_data, actor_handle);
  result = 0;
  if (*(char *)(actor + 0x1b5) != '\0') {
    prop_handle = -1;
    if (*(int *)(actor + 0x18) != -1) {
      unit = (int)object_get_and_verify_type(*(int *)(actor + 0x18), 3);
      responsible = ai_get_responsible_unit(*(int *)(unit + 0x3c0), 1);
      if (responsible != -1) {
        prop_handle = prop_get_active_by_unit_index(actor_handle, responsible);
      }
    }
    panic_type = *(short *)(actor + 0x308);
    if ((panic_type == 0) || (*(int *)(actor + 0x30c) == -1)) {
      *(int *)(actor + 0x30c) = prop_handle;
    }
    if (panic_type < 0xd) {
      panic_type = 0xc;
    }
    *(short *)(actor + 0x308) = panic_type;
    result = 1;
  }
  return result;
}

/* actor_action_handle_panic_from_attached_projectiles (0x1c7f0)
 *
 * Checks if an actor should panic because it has projectiles attached to it.
 * Sets panic substate (actor+0x308, clamped to min 0xa) and prop index
 * (actor+0x30c) when the attached-projectile handle (actor+0x1b0) is valid.
 *
 * Confirmed: datum_get(actor_data, actor_handle); actor+0x1b0 = projectile
 * handle. Confirmed: object_try_and_get_and_verify_type(actor+0x1b0,
 * 0xffffffff). Confirmed: ai_get_responsible_unit(obj+0x74, 1). Confirmed:
 * prop_get_active_by_unit_index for prop lookup. Confirmed: actor+0x308
 * substate clamped min 0xa (10).
 */
int actor_action_handle_panic_from_attached_projectiles(int actor_handle)
{
  char *actor;
  int projectile;
  int responsible;
  int prop_handle;
  short panic_type;
  int result;

  actor = (char *)datum_get(actor_data, actor_handle);
  result = 0;
  if (*(int *)(actor + 0x1b0) != -1) {
    projectile = (int)object_try_and_get_and_verify_type(
      *(int *)(actor + 0x1b0), 0xffffffff);
    prop_handle = -1;
    if (projectile != 0) {
      responsible = ai_get_responsible_unit(*(int *)(projectile + 0x74), 1);
      if (responsible != -1) {
        prop_handle = prop_get_active_by_unit_index(actor_handle, responsible);
      }
    }
    panic_type = *(short *)(actor + 0x308);
    if ((panic_type == 0) || (*(int *)(actor + 0x30c) == -1)) {
      *(int *)(actor + 0x30c) = prop_handle;
    }
    if (panic_type < 0xb) {
      panic_type = 10;
    }
    *(short *)(actor + 0x308) = panic_type;
    result = 1;
  }
  return result;
}

/* actor_action_handle_panic_from_attached_melee_attackers (0x1c880)
 *
 * Checks if an actor should panic due to attached melee attackers. Sets panic
 * substate (actor+0x308, clamped to min 0xb) and clears prop index
 * (actor+0x30c) when the attached-melee flag (actor+0x1b4) is set.
 *
 * Confirmed: datum_get(actor_data, actor_handle); actor+0x1b4 = attached-melee
 * flag. Confirmed: actor+0x308 substate clamped min 0xb (not to 0xc —
 * max(current,0xb)). Confirmed: actor+0x30c set to 0xffffffff (-1 = NONE).
 * Confirmed: returns 1 if triggered, 0 otherwise.
 */
char actor_action_handle_panic_from_attached_melee_attackers(int actor_handle)
{
  char *actor;
  short panic_type;
  char result;

  actor = (char *)datum_get(actor_data, actor_handle);
  result = 0;
  if (*(char *)(actor + 0x1b4) != '\0') {
    panic_type = *(short *)(actor + 0x308);
    if ((int)panic_type < 0xc) {
      panic_type = 0xb;
    }
    *(short *)(actor + 0x308) = panic_type;
    *(int *)(actor + 0x30c) = -1;
    result = 1;
  }
  return result;
}

/* actor_action_handle_berserking_from_attacking_mode (0x1c8d0)
 *
 * Checks if an actor should enter berserk mode due to being in high-aggression
 * attacking mode. Sets berserk substate (actor+0x310, clamped to min 1) when
 * the actor's aggression (actor+0x6e) > 4, berserk-from-attack flag
 * (actor+0x1c9) is clear, and the actr tag allows berserk (flag 0x80000).
 *
 * Confirmed: datum_get(actor_data, actor_handle);
 * tag_get(0x61637472,actor+0x58). Confirmed: *actr_tag & 0x80000 =
 * allows_berserk flag. Confirmed: actor+0x1c9 = berserk-from-attack
 * already-triggered flag. Confirmed: actor+0x6e = aggression level (short),
 * must be > 4. Confirmed: actor+0x310 = berserk substate, clamped min 1.
 * Confirmed: returns 1 if triggered, 0 otherwise.
 */
char actor_action_handle_berserking_from_attacking_mode(int actor_handle)
{
  char *actor;
  int *actr_tag;
  short berserk_state;
  char result;

  actor = (char *)datum_get(actor_data, actor_handle);
  actr_tag = (int *)tag_get(0x61637472, *(int *)(actor + 0x58));
  result = 0;
  if (((*(unsigned int *)actr_tag & 0x80000) != 0) &&
      (*(char *)(actor + 0x1c9) == '\0') && (*(short *)(actor + 0x6e) >= 5)) {
    berserk_state = *(short *)(actor + 0x310);
    if (berserk_state < 2) {
      berserk_state = 1;
    }
    *(short *)(actor + 0x310) = berserk_state;
    result = 1;
  }
  return result;
}

/* actor_action_handle_berserking_from_proximity (0x1c940)
 *
 * Checks if an actor should enter berserk mode because its target is too
 * close (within the tag's proximity berserk threshold). Sets berserk substate
 * (actor+0x310, clamped to min 2) when aggression (actor+0x6e) > 4 and
 * target-to-actor distance < actr_tag berserk_proximity_distance threshold.
 *
 * Confirmed: datum_get(actor_data); datum_get(props_data?, actor+0x270).
 * Confirmed: actor+0x270 = target prop index; assert != NONE.
 * Confirmed: prop+0x11c = prop distance (float); actr_tag+0x3a0 = threshold
 * (float). Confirmed: actor+0x310 = berserk substate, clamped min 2. Confirmed:
 * actor+0x6e = aggression level (short), must be > 4. Confirmed: returns 1 if
 * triggered, 0 otherwise.
 */
char actor_action_handle_berserking_from_proximity(int actor_handle)
{
  char *actor;
  char *actr_tag;
  char *prop;
  int prop_handle;
  float dist;
  float threshold;
  short berserk_state;

  actor = (char *)datum_get(actor_data, actor_handle);
  actr_tag = (char *)tag_get(0x61637472, *(int *)(actor + 0x58));
  if (4 < *(short *)(actor + 0x6e)) {
    prop = (char *)datum_get(*(data_t **)0x5ab23c, *(int *)(actor + 0x270));
    prop_handle = *(int *)(actor + 0x270);
    assert_halt(prop_handle != -1);
    dist = *(float *)(prop + 0x11c);
    threshold = *(float *)(actr_tag + 0x3a0);
    if (dist < threshold) {
      berserk_state = *(short *)(actor + 0x310);
      if (berserk_state < 3) {
        berserk_state = 2;
      }
      *(short *)(actor + 0x310) = berserk_state;
      return 1;
    }
  }
  return 0;
}

/* actor_action_handle_berserking_from_damage (0x1ca00)
 * Returns 1 if actor triggers berserk from damage: actor+0x2ec flag set,
 * health (actor+0x1c0) > actr_tag+0x398, and speed (actor+0x1b8) <
 * actr_tag+0x39c. Clamps berserk_state (actor+0x310) to min 3, clears the 0x2ec
 * flag. */
char actor_action_handle_berserking_from_damage(int actor_handle)
{
  char *actor;
  int actr_tag;
  short berserk_state;
  char result;

  actor = (char *)datum_get(actor_data, actor_handle);
  actr_tag = (int)tag_get(0x61637472, *(int *)(actor + 0x58));
  result = 0;
  if (*(char *)(actor + 0x2ec) != '\0') {
    if (*(float *)(actor + 0x1c0) > *(float *)(actr_tag + 0x398)) {
      if (*(float *)(actor + 0x1b8) < *(float *)(actr_tag + 0x39c)) {
        berserk_state = *(short *)(actor + 0x310);
        if (berserk_state < 4) {
          berserk_state = 3;
        }
        *(short *)(actor + 0x310) = berserk_state;
        *(char *)(actor + 0x2ec) = 0;
        result = 1;
      }
    }
  }
  return result;
}

/* actor_action_deny_transition (0x1ca90)
 * Returns 1 if the actor must deny an action transition: pending command list,
 * squad timer active with low state, or berserking with specific flags clear.
 */
char actor_action_deny_transition(int actor_handle)
{
  char *actor;
  char *squad;
  char result;

  actor = (char *)datum_get(actor_data, actor_handle);
  result = 0;
  if ((*(short *)(actor + 0x90) != -1) && (0 < *(short *)(actor + 0x92))) {
    result = 1;
  }
  if (*(int *)(actor + 0x34) != -1) {
    squad = encounter_get_squad(
      datum_get(*(data_t **)0x5ab270, *(int *)(actor + 0x34)),
      *(short *)(actor + 0x3a));
    if (0 < *(short *)(squad + 0x12)) {
      if (*(short *)(actor + 0x6e) >= 5) {
        encounter_squad_timer_expire(*(int *)(actor + 0x34),
                                     *(short *)(actor + 0x3a));
      } else {
        result = 1;
      }
    }
  }
  if (*(short *)(actor + 0x6c) == 0xb) {
    if ((*(char *)(actor + 0x9e) == '\0') &&
        (*(char *)(actor + 0xa1) == '\0')) {
      return 1;
    }
  }
  return result;
}

/* actor_action_handle_vehicle_exit (0x1cb70)
 * Attempts to exit the actor's current vehicle seat. Returns 1 on success.
 * Iterates nearby props to check for berserking attackers; tries
 * unit_try_and_exit_seat on the actor's unit, storing the vehicle handle and a
 * cooldown timer on success. */
char actor_action_handle_vehicle_exit(int actor_handle)
{
  char *actor;
  char berserk_nearby;
  char local_5;
  char iter_buf[12];
  int prop;
  char exit_ok;
  int t;
  char result;

  actor = (char *)datum_get(actor_data, actor_handle);
  result = 0;
  if (*(int *)(actor + 0x158) == -1) {
    goto done_clear;
  }
  berserk_nearby = 0;
  local_5 = 0;
  FUN_00064540((int *)iter_buf, actor_handle);
  prop = FUN_00064570((int *)iter_buf);
  while (prop != 0) {
    if (((1 < *(short *)(prop + 0x24)) && (*(short *)(prop + 0x24) < 4)) &&
        (*(char *)(prop + 0x12e) != '\0') && (*(char *)(prop + 0x60) != '\0') &&
        (*(int *)(prop + 0x110) == *(int *)(actor + 0x158))) {
      berserk_nearby = 1;
      local_5 = 1;
      break;
    }
    prop = FUN_00064570((int *)iter_buf);
  }
  if (*(char *)(actor + 0x2ed) != '\0') {
    berserk_nearby = 1;
  }
  if ((*(char *)(actor + 0x160) == '\0') ||
      ((*(int *)(actor + 0x1b0) == -1) &&
       ((*(short *)(actor + 0x280) != 2 ||
         (*(char *)(actor + 0x28a) == '\0'))))) {
    if (!berserk_nearby) {
      goto done_clear;
    }
  } else {
    local_5 = 1;
  }
  *(char *)(actor + 0x38c) = local_5;
  exit_ok = unit_try_and_exit_seat(*(int *)(actor + 0x18));
  if (exit_ok != '\0') {
    *(int *)(actor + 0x390) = *(int *)(actor + 0x158);
    t = game_time_get();
    *(int *)(actor + 0x394) = t + 0xb4;
    *(char *)(actor + 0x38c) = 0;
    *(char *)(actor + 0x2ed) = 0;
    return 1;
  }
  *(char *)(actor + 0x38c) = 0;
  *(char *)(actor + 0x2ed) = 0;
  return result;
done_clear:
  *(char *)(actor + 0x2ed) = 0;
  return result;
}

/* actor_action_can_stop_guarding (0x1cf10)
 * Returns 1 if the actor can stop the guard action, based on state counters and
 * flags. Asserts that the actor's current action is _actor_action_guard (6). */
char actor_action_can_stop_guarding(int actor_handle, short min_state,
                                    short max_state)
{
  char *actor;

  actor = (char *)datum_get(actor_data, actor_handle);
  assert_halt(*(short *)(actor + 0x6c) == 6);
  if (*(char *)(actor + 0xa4) != '\0') {
    return max_state <= *(short *)(actor + 0x6e);
  }
  if (((0 < *(short *)(actor + 0x9c)) &&
       (*(short *)(actor + 0x6e) < min_state)) &&
      ((*(short *)(actor + 0x1e4) < 1 || (*(char *)(actor + 0xa1) != '\0')))) {
    return 0;
  }
  return 1;
}

/* actor_action_can_stop_conversing (0x1cfa0) — Check whether an actor may stop
 * its current conversation. Returns 1 if not in a conversation, or if the
 * conversation's flags permit stopping based on the actor's state. */
int actor_action_can_stop_conversing(int actor_handle, int flag)
{
  char *actor;
  char *conv;
  char *elem;
  int16_t flags;

  (void)flag;

  actor = (char *)datum_get(actor_data, actor_handle);
  if (*(int *)(actor + 0x1dc) == -1) {
    return 1;
  }
  conv = (char *)datum_get(*(data_t **)0x6324ec, *(int *)(actor + 0x1dc));
  elem = (char *)tag_block_get_element(
      (char *)global_scenario_get() + 0x468,
      (int)*(int16_t *)(conv + 2), 0x74);
  flags = *(int16_t *)(elem + 0x20);
  if ((flags & 2) != 0 && *(char *)(actor + 0x1f6) != '\0') {
    return 1;
  }
  if ((flags & 4) != 0 && *(int16_t *)(actor + 0x268) > 8) {
    return 1;
  }
  if ((flags & 8) != 0 && *(int16_t *)(actor + 0x268) > 5) {
    return 1;
  }
  return 0;
}

/* actor_action_change (0x1d030) — actor_set_action
 *
 * Transitions an actor to a new action type: calls the old action's exit
 * handler, adjusts the actor's priority level, copies action-specific state
 * data, sets the new action index, and calls the new action's begin handler.
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x1d040.
 * Confirmed: INC word [0x5ac87c] (global action-change counter) at 0x1d04b.
 * Confirmed: assert new_action_type in [0,14) at line 0xb83.
 * Confirmed: assert table[new_action].action == new_action_type at line 0xb84.
 * Confirmed: assert actor+0x6c (old action) in [0,14) at line 0xb87.
 * Confirmed: exit handler at table+0x24 (0x253fc4) called with actor_handle.
 * Confirmed: priority adjust using table+0x10 (0x253fb0) short field.
 * Confirmed: actor_clear_discarded_firing_positions(actor_handle, 0) at
 * 0x1d122. Confirmed: csmemcpy(actor+0x9c, param_3, data_size) when data_size >
 * 0. Confirmed: actor+0x6c = (short)param_2, actor+0x70 = 1 at 0x1d153/0x1d157.
 * Confirmed: begin handler at table+0x14 (0x253fb4) called with actor_handle.
 */
void actor_action_change(int actor_handle, int new_action_type, int param_3)
{
  typedef void (*action_handler_fn_t)(int);

  char *actor;
  int table_offset;
  action_handler_fn_t handler;

  actor = (char *)datum_get(actor_data, actor_handle);

  (*(uint16_t *)0x5ac87c)++;

  assert_halt(new_action_type >= 0 && new_action_type < 14);

  table_offset = new_action_type * 0x38;

  assert_halt(*(int *)(0x253fa0 + table_offset) == new_action_type);

  assert_halt(*(short *)(actor + 0x6c) >= 0 && *(short *)(actor + 0x6c) < 14);

  handler =
    *(action_handler_fn_t *)(0x253fc4 + *(short *)(actor + 0x6c) * 0x38);
  if (handler != NULL) {
    handler(actor_handle);
  }

  if (*(short *)(0x253fb0 + table_offset) == 0) {
    if (*(short *)(actor + 0x6a) > 2) {
      *(short *)(actor + 0x6a) = 2;
    }
  } else {
    if (*(short *)(actor + 0x6a) < 3) {
      *(short *)(actor + 0x6a) = 3;
    }
  }

  actor_clear_discarded_firing_positions(actor_handle, 0);

  if (*(unsigned int *)(0x253fac + table_offset) != 0 && param_3 != 0) {
    csmemcpy(actor + 0x9c, (void *)param_3, *(int *)(0x253fac + table_offset));
  }

  *(short *)(actor + 0x6c) = (short)new_action_type;
  *(char *)(actor + 0x70) = 1;

  handler = *(action_handler_fn_t *)(0x253fb4 + (short)new_action_type * 0x38);
  if (handler != NULL) {
    handler(actor_handle);
  }
}

/* actor_action_try_to_seek_cover (0x1d350) — Attempt to make the actor seek
 * cover. Gets actor+0x270 as param_2 for FUN_00015040, then calls
 * actor_action_change with action 4 if successful. */
char actor_action_try_to_seek_cover(int actor_handle, char param_2, char param_3)
{
  char *actor;
  char cVar1;
  short local_88[66];

  actor = (char *)datum_get(actor_data, actor_handle);
  cVar1 = FUN_00015040(actor_handle, 0, *(int *)(actor + 0x270), 0, param_2, param_3, local_88);
  if (cVar1 != '\0') {
    actor_action_change(actor_handle, 4, (int)local_88);
    return 1;
  }
  return 0;
}

/* FUN_0001d3c0 (0x1d3c0) — Attempt to make the actor seek cover with explicit
 * parameters. Calls FUN_00015040 with param_2/param_3/param_4 and no actor
 * lookup, then actor_action_change with action 4 if successful. */
char FUN_0001d3c0(int actor_handle, short param_2, int param_3, char param_4)
{
  char cVar1;
  short local_88[66];

  cVar1 = FUN_00015040(actor_handle, param_2, param_3, param_4, 0, 0, local_88);
  if (cVar1 != '\0') {
    actor_action_change(actor_handle, 4, (int)local_88);
    return 1;
  }
  return 0;
}

/* actor_action_try_to_enter_vehicle (0x1d420) — Attempt to make the actor
 * enter a vehicle. Iterates seat indices (from param_6 array, or discovered
 * via vehicle_scripting_find_available_seats if param_6 is NULL). For each
 * valid seat index, checks unit_has_animation_to_enter_seat then
 * FUN_0001b750, and on success calls actor_action_change with action type 9.
 * Marks the consumed seat as -1 in the seat array.
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x1d437.
 * Confirmed: actor+0x18 used as unit_handle for seat check at 0x1d48a.
 * Confirmed: action type 9 at 0x1d4cf.
 * Confirmed: seat marked 0xffff at 0x1d4df. */
char actor_action_try_to_enter_vehicle(int actor_handle, int param_2, int param_3,
                                       int param_4, int16_t param_5, int16_t *param_6)
{
  char *actor;
  int16_t i;
  int16_t seat_index;
  int16_t local_seats[16];
  short action_buf[66];

  actor = (char *)datum_get(actor_data, actor_handle);
  if (param_6 == NULL) {
    param_6 = local_seats;
    param_5 = vehicle_scripting_find_available_seats(param_2, param_3, param_4, local_seats, 0x10);
  }
  for (i = 0; i < param_5; i++) {
    seat_index = param_6[i];
    if (seat_index != -1 &&
        unit_has_animation_to_enter_seat(*(int *)(actor + 0x18), param_2, seat_index) != '\0' &&
        FUN_0001b750(actor_handle, param_2, seat_index, action_buf) != '\0') {
      actor_action_change(actor_handle, 9, (int)action_buf);
      param_6[i] = (int16_t)0xffff;
      return 1;
    }
  }
  return 0;
}

/* actor_action_name (0x1d5c0) — action_type_get_name
 *
 * Returns the name string for a given action type index from the
 * action_definitions table. Returns "unknown" if out of range.
 *
 * Confirmed: range check [0, 14) at 0x1d5c7/0x1d5d1.
 * Confirmed: IMUL EAX,EAX,0x38 (stride 56) at 0x1d5da.
 * Confirmed: name ptr at [EAX + 0x253fa4] (table base+0x04).
 * Confirmed: default "unknown" string at 0x254608. */
const char *actor_action_name(int16_t action_type)
{
  const char *name = (const char *)0x254608;
  if (action_type >= 0 && action_type < 0xe) {
    name = *(const char **)(0x253fa4 + action_type * 0x38);
  }
  return name;
}

/* actor_mode_name (0x1d5f0) — Returns the name string for a given actor mode
 * index. Returns "unknown" if out of range [0, 4). Lookup table at 0x2c8510. */
const char *actor_mode_name(int16_t param_1)
{
  const char *name = (const char *)0x254608;
  if (param_1 >= 0 && param_1 < 4) {
    name = *(const char **)(0x2c8510 + param_1 * 4);
  }
  return name;
}

/* actor_action_try_to_panic (0x1d6d0) — actor_get_action_priority_flag
 *
 * Returns the priority flag (short) for the actor's current action from the
 * action_definitions table. A non-zero value indicates the action raises the
 * actor's priority to the high-priority tier (>= 3); zero means normal (< 3).
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x1d6df.
 * Confirmed: actor+0x6c = action index (short), asserted in [0, 14).
 * Confirmed: IMUL EDX,EDX,0x38 at 0x1d71c; MOV AX,[EDX+0x253fb0] at 0x1d71f.
 * Confirmed: table field 0x253fb0 = priority_flag (short at +0x10 from entry
 *   base 0x253fa0, same field used in actor_set_action at 0x1d030+0x79).
 * Confirmed: assert line 0xe98, __FILE__ "c:\halo\SOURCE\ai\actions.c".
 */
int16_t actor_action_try_to_panic(int actor_handle)
{
  char *actor;
  int16_t action;

  actor = (char *)datum_get(actor_data, actor_handle);
  action = *(int16_t *)(actor + 0x6c);

  assert_halt(action >= 0 && action < 14);

  return *(int16_t *)(0x253fb0 + action * 0x38);
}

/* actor_action_get_default_state (0x1d730) — Map a starting location index to
 * an action category. Returns a short from a 12-entry lookup table at 0x254300,
 * or 0 if the index is out of range [0, 12).
 *
 * Confirmed: CMP CX,0xc bounds check, table at 0x254300 =
 * {0,2,2,3,4,5,6,7,8,9,9,8}.
 */
short actor_action_get_default_state(short param_1)
{
  if (param_1 < 0 || param_1 >= 12)
    return 0;
  return *(short *)(0x254300 + (int)param_1 * 2);
}

/* set_real_vector2d (0x1d760) — Store two float values into a 2D vector
 * output pointer. */
void set_real_vector2d(float *out, float x, float y)
{
  out[0] = x;
  out[1] = y;
}

/* actor_action_handle_initial_action (0x1dab0)
 * If the actor is in the idle action (0x6c == 0) and has a non-zero
 * default-state index (0x6a), runs actor_action_set_default_state to
 * initiate that default state. Returns 1 if the state was set, 0 otherwise.
 *
 * Confirmed: datum_get(actor_data, actor_handle); actor+0x6c = short action;
 * actor+0x6a = short default_state_index. */
char actor_action_handle_initial_action(int actor_handle)
{
  char *actor;
  char result;

  actor = (char *)datum_get(actor_data, actor_handle);
  result = 0;
  if ((*(short *)(actor + 0x6c) == 0) && (*(short *)(actor + 0x6a) != 0)) {
    result = actor_action_set_default_state(actor_handle, 0xffff);
  }
  return result;
}

/* actor_action_handle_pending_command_list (0x1daf0)
 * Handles any pending command-list action stored in actor+0x90.
 * Returns 0 if no pending command, if guarding with no default state, or
 * if the current transition is denied. Otherwise starts the command-list
 * action (type 0xb), clears the pending entry, and returns 1.
 *
 * Confirmed: datum_get(actor_data, actor_handle); actor+0x90 = short
 * pending_action; actor+0x8e = char flag; actor+0x6a = short state.
 * actor_action_deny_transition at 0x1ca90; FUN_00016e70 at 0x16e70;
 * actor_action_change(actor_handle, 0xb, buf); actor+0x8e cleared. */
char actor_action_handle_pending_command_list(int actor_handle)
{
  char *actor;
  char cVar1;
  char result;
  char action_buf[132];

  actor = (char *)datum_get(actor_data, actor_handle);
  result = 0;
  if (*(short *)(actor + 0x90) == -1) {
    return 0;
  }
  if (*(char *)(actor + 0x8e) != '\0') {
    goto do_action;
  }
  if (*(short *)(actor + 0x6a) == 0) {
    return 0;
  }
  cVar1 = actor_action_deny_transition(actor_handle);
  if (cVar1 != '\0') {
    return 0;
  }
do_action:
  cVar1 = FUN_00016e70(actor_handle, *(short *)(actor + 0x90), action_buf);
  if (cVar1 != '\0') {
    actor_action_change(actor_handle, 0xb, (int)action_buf);
    result = 1;
  }
  *(char *)(actor + 0x8e) = 0;
  *(short *)(actor + 0x90) = -1;
  return result;
}

/* actor_action_handle_panic_transition (0x1dd40) — Handles a panic-level
 * transition for an actor. If the actor's current panic level (actor+0x308)
 * meets or exceeds param_2 and the actor is not suppressed (actor+0x160),
 * evaluates the transition. In guard action (0x6c==4) with positive shield
 * value (actor+0xa8), clamps the shield to the panic level. Otherwise, if
 * enough time has passed since actor+0x398, checks whether to play a sound
 * event or attempt seek cover via FUN_0001d3c0. Clears panic level on exit.
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x1dd50.
 * Confirmed: game_time_get() at 0x1ddd6.
 * Confirmed: display_assert + system_exit pattern at 0x1de08-0x1de25.
 * Confirmed: FUN_00046f10 sound event call at 0x1de43.
 * Confirmed: FUN_0001d3c0 call at 0x1de74. */
char actor_action_handle_panic_transition(int actor_handle, short param_2, char param_3, short param_4)
{
  char *actor;
  short panic_level;
  short shield_value;
  int iVar5;
  char bVar3;
  volatile char result;

  actor = (char *)datum_get(actor_data, actor_handle);
  panic_level = *(short *)(actor + 0x308);
  result = 0;
  if (param_2 <= panic_level && *(char *)(actor + 0x160) == '\0') {
    if (*(short *)(actor + 0x6c) == 4 && (shield_value = *(short *)(actor + 0xa8), shield_value > 0)) {
      if (panic_level < shield_value) {
        *(short *)(actor + 0xa8) = shield_value;
        *(short *)(actor + 0x308) = 0;
        return result;
      }
      *(short *)(actor + 0xa8) = panic_level;
      *(short *)(actor + 0x308) = 0;
      return result;
    }
    if (*(int *)(actor + 0x398) != -1) {
      iVar5 = game_time_get();
      if (iVar5 <= *(int *)(actor + 0x398) + 7) {
        goto done;
      }
    }
    bVar3 = *(short *)(actor + 0x308) >= param_4;
    if (*(int *)(actor + 0x30c) == 0) {
      display_assert("actor->stimuli.panic_prop_index != 0x00000000",
                     "c:\\halo\\SOURCE\\ai\\actions.c", 0x295, 1);
      system_exit(-1);
    }
    if (param_3 != '\0' && !bVar3) {
      FUN_00046f10(0x22, *(int *)(actor + 0x18), -1, -1, -1, -1, 0);
      *(short *)(actor + 0x308) = 0;
      return result;
    }
    result = FUN_0001d3c0(actor_handle, *(short *)(actor + 0x308),
                          *(int *)(actor + 0x30c), bVar3);
  }
done:
  *(short *)(actor + 0x308) = 0;
  return result;
}

/* actor_action_handle_done_fleeing (0x1f6e0)
 * Handles the transition when an actor finishes fleeing (action type 4).
 * If the actor's current action is type 4 and the flag at actor+0xab is set,
 * calls FUN_00016210 to build a new action buffer from actor+0x9c, then
 * changes to action type 6. Asserts on FUN_00016210 failure. Returns 1 if
 * the transition was performed, 0 otherwise. */
char actor_action_handle_done_fleeing(int actor_handle)
{
  char *actor;
  char cVar1;
  short action_buf[66];

  actor = (char *)datum_get(actor_data, actor_handle);
  if (*(short *)(actor + 0x6c) != 4) {
    return 0;
  }
  if (*(char *)(actor + 0xab) == '\0') {
    return 0;
  }
  cVar1 = FUN_00016210(actor_handle, (int)(actor + 0x9c), action_buf);
  if (cVar1 == '\0') {
    display_assert("success", "c:\\halo\\SOURCE\\ai\\actions.c", 0xa79, 1);
    system_exit(-1);
  }
  actor_action_change(actor_handle, 6, (int)action_buf);
  return 1;
}
