/* actor_looking.c — AI actor looking/targeting subsystem.
 *
 * Corresponds to actor_looking.obj (or the actor_looking portion of the
 * compiled AI module).
 * Assertion path: c:\halo\SOURCE\ai\actor_looking.c
 */

#include "../../common.h"
#include "../../x87_math.h"

/* Cross-object callee declarations */
extern float distance_squared3d(const float *a, const float *b);
extern float actor_destination_tolerance(int actor_handle);
extern char *actor_combat_get_firing_variant_definition(int actor_handle);
extern void actor_find_pathfinding_location(int actor_handle);

__declspec(noinline) static int
s_actor_charge_estimate_target(char *actor, float *target_pos)
{
  if (!*(char *)(actor + 0x4a8))
    return 0;
  unit_estimate_position(*(int *)(actor + 0x18), 1,
                         (vector3_t *)(actor + 0x4ac), NULL, NULL,
                         (vector3_t *)target_pos);
  return 1;
}

/* FUN_00013dd0 (0x13dd0)
 * Tests whether a clear collision path exists from source_pos to the actor's
 * charge target. If actor->flag_0x484 is set, reads the target position from
 * actor+0x120. If actor->flag_0x4a8 is set, estimates it via
 * unit_estimate_position. Otherwise returns 0 (no target).
 * Pushes collision user ID 5, fires a line test, then pops.
 * Returns 1 always when a target exists; 0 when no valid target.
 * Confirmed: @eax=actor_handle, @esi=source_pos (weapon_datum+0xc8 per call
 * site at 0x141ec). PUSH EDX before call is an unused extra arg; callee never
 * reads [EBP+8]. assert: c:\halo\SOURCE\ai\action_charge.c, 0x379/0x381. */
int FUN_00013dd0(int actor_handle, float *source_pos)
{
  char *actor;
  float target_pos[3];
  float dir[3];
  int16_t collision_result[40];
  int depth;

  actor = (char *)datum_get(actor_data, actor_handle);
  if (*(char *)(actor + 0x484)) {
    target_pos[0] = *(float *)(actor + 0x120);
    target_pos[1] = *(float *)(actor + 0x124);
    target_pos[2] = *(float *)(actor + 0x128);
  } else if (!s_actor_charge_estimate_target(actor, target_pos)) {
    return 0;
  }

  if (*(volatile int16_t *)0x4761d8 >= 0x20) {
    display_assert("global_current_collision_user_depth < "
                   "MAXIMUM_COLLISION_USER_STACK_DEPTH",
                   "c:\\halo\\SOURCE\\ai\\action_charge.c", 0x379, 1);
    system_exit(-1);
  }
  depth = *(volatile short *)0x4761d8;
  *(int16_t *)(0x5a8c80 + (int)depth * 2) = 5;
  *(volatile short *)0x4761d8 = depth + 1;

  dir[0] = source_pos[0] - target_pos[0];
  dir[1] = source_pos[1] - target_pos[1];
  dir[2] = source_pos[2] - target_pos[2];
  FUN_0014df70(0x33, target_pos, dir, -1, collision_result);

  if (*(volatile short *)0x4761d8 <= 1) {
    display_assert("global_current_collision_user_depth > 1",
                   "c:\\halo\\SOURCE\\ai\\action_charge.c", 0x381, 1);
    system_exit(-1);
  }
  {
    int d = *(volatile short *)0x4761d8;
    *(volatile short *)0x4761d8 = (short)(d - 1);
  }
  return 1;
}

/* FUN_00013ef0 (0x13ef0)
 * Actor charge action initializer (action_charge.c, 0x2f).
 * Validates charge preconditions, computes melee timing and speed, then arms
 * actor_move_to_prop and tests the line-of-sight path via FUN_00013dd0.
 * Returns 1 if the charge can proceed, 0 otherwise; sets actor_state+0x190
 * to a fail-reason code (0=ok, 2–9 = specific block reasons).
 * Confirmed: 3 cdecl stack args; actor_state = *(char**)0x331f58 + slot*0x657c;
 * encounter = datum_get(*(data_t**)0x5ab23c, actor+0x270);
 * FUN_00012ad0 @ebx=actor_handle @esi=action_type, stack=charge_state. */
char FUN_00013ef0(int actor_handle, int action_type, void *charge_state)
{
  char *actor;
  char *actor_state;
  char *actr_tag;
  char *encounter;
  char return_flag;
  int is_secondary;
  int melee_tick_count;
  float attack_time;
  short tick_count_short;
  float damage_time;
  float speed;
  float rand_val;
  char *actv_tag;
  float min_speed;

  actor = (char *)datum_get(actor_data, actor_handle);
  actr_tag = (char *)tag_get(0x61637472, *(int *)(actor + 0x58));
  return_flag = 1;
  actor_state = *(char **)0x331f58 + (actor_handle & 0xffff) * 0x657c;
  *(int *)(actor_state + 0x18c) = game_time_get();

  if (!charge_state) {
    display_assert("state_data", "c:\\halo\\SOURCE\\ai\\action_charge.c", 0x2f,
                   1);
    system_exit(-1);
  }
  csmemset(charge_state, 0, 0x38);
  *(int *)charge_state = game_time_get();

  if ((short)action_type == 5 || (short)action_type == 4) {
    return_flag = (char)(*(int16_t *)(actor + 0x15e) > 1);
    *(int16_t *)(actor_state + 0x190) =
      (int16_t)(*(int16_t *)(actor + 0x15e) <= 1);
    goto done;
  }
  if ((short)action_type != 2) {
    return_flag = 0;
    if ((short)action_type == 0 && (*(int *)actr_tag & 0x20000) &&
        *(int16_t *)(actor + 0x6e) >= 5 && !*(char *)(actor + 0x378)) {
      action_type = 1;
      *(int16_t *)(actor_state + 0x190) = 8;
    } else {
      *(int16_t *)(actor_state + 0x190) = 9;
    }
    goto done;
  }

  /* action_type == 2: melee charge */
  if (*(char *)(actor + 0x6)) {
    return_flag = 0;
    *(int16_t *)(actor_state + 0x190) = 2;
    goto done;
  }
  if (*(int8_t *)((char *)object_get_and_verify_type(*(int *)(actor + 0x18),
                                                     3) +
                  0xb6) < 0) {
    return_flag = 0;
    *(int16_t *)(actor_state + 0x190) = 3;
    goto done;
  }
  if (*(int *)(actor + 0x270) == -1) {
    return_flag = 0;
    *(int16_t *)(actor_state + 0x190) = 4;
    goto done;
  }

  encounter = (char *)datum_get(*(data_t **)0x5ab23c, *(int *)(actor + 0x270));

  /* determine is_secondary (lunge vs normal melee) */
  if (*(float *)(actr_tag + 0x388) == *(float *)0x2533c0 ||
      *(float *)(actr_tag + 0x390) == *(float *)0x2533c0) {
    *(char *)((char *)charge_state + 0xa) = 0;
    is_secondary = 0;
  } else if (*(char *)(encounter + 0x130) == 0 &&
             *(int16_t *)(encounter + 0x9c) <= 0) {
    rand_val =
      random_math_real((unsigned int *)get_global_random_seed_address());
    is_secondary = 1;
    if (rand_val >= *(float *)(actr_tag + 0x390))
      is_secondary = 0;
    *(char *)((char *)charge_state + 0xa) = (char)is_secondary;
    if (*(float *)(encounter + 0x11c) < *(float *)(actr_tag + 0x384)) {
      is_secondary = 0;
    } else {
      if (is_secondary)
        action_type = 3;
    }
  } else {
    *(char *)((char *)charge_state + 0xa) = 1;
    is_secondary = 1;
    action_type = 3;
  }
  *(char *)(actor_state + 0x198) = is_secondary;

  if (!unit_get_melee_range_and_ticks(*(int *)(actor + 0x18), is_secondary,
                                      &melee_tick_count, &attack_time,
                                      &tick_count_short, &damage_time)) {
    *(int16_t *)(actor_state + 0x190) = 5;
    goto done;
  }

  if (*(int *)actr_tag & 0x8000000) {
    /* dual-wield: use weapon tick count directly */
    *(int16_t *)((char *)charge_state + 0x32) = tick_count_short;
    *(int *)((char *)charge_state + 0x34) = 0;
    *(char *)((char *)charge_state + 0x30) = 1;
  } else {
    if (!melee_tick_count) {
      actv_tag = (char *)tag_get(0x61637476, *(int *)(actor + 0x5c));
      if (*(int *)(actv_tag + 0x8)) {
        error(2, "actor %s melee animation has no damage keyframe",
              *(int *)(actv_tag + 0x8));
      }
      melee_tick_count = (int16_t)tick_count_short / 2;
      attack_time = damage_time * *(float *)0x253398;
    }
    *(int16_t *)((char *)charge_state + 0x32) = (int16_t)melee_tick_count;
    *(float *)((char *)charge_state + 0x34) = damage_time - attack_time;
  }

  speed = FUN_00012ad0(actor_handle, action_type, charge_state);
  *(float *)((char *)charge_state + 0x2c) = speed;

  min_speed =
    ((short)action_type == 3) ? *(float *)0x2533d8 : *(float *)0x2533ec;
  if (speed < min_speed)
    speed = min_speed;

  if (actor_move_to_prop(actor_handle, *(int *)(actor + 0x270), speed)) {
    FUN_0002a330(actor_handle);
    if (FUN_00013dd0(actor_handle, (float *)(encounter + 0xc8))) {
      return_flag = 1;
      *(int16_t *)(actor_state + 0x190) = 7;
      goto done;
    }
  }
  *(int16_t *)(actor_state + 0x190) = 6;
  *(float *)(actor_state + 0x194) = speed;

done:
  *(int16_t *)((char *)charge_state + 0x4) = (int16_t)action_type;
  return return_flag;
}

/* FUN_000142a0 (0x142a0)
 * Conversation action initializer (action_converse.c, line 0x21).
 * Validates actor, looks up the conversation datum via 0x6324ec, fetches the
 * scenario conversation entry at scenario+0x468[*(short*)(action_ref+2)],
 * and populates state_data (20 bytes): [0]=action_handle, float@+8 from
 * tag+0x28, [3]=speaker_handle (if float==REAL_NONE) else -1, [4]=-1,
 * byte@+5=0. Returns 1 always.
 *
 * Confirmed: first datum_get(actor_data, actor_handle) result is discarded.
 * Confirmed: FCOMP+TEST AH,0x44+JP at 0x1432f — JP taken when PF=1 (NOT
 * equal/NaN) → speaker handle; fall-through (equal) → -1. */
char FUN_000142a0(int actor_handle, int action_handle, int *state_data)
{
  char *action_ref;
  char *tag_elem;
  float fVar1;
  int speaker;

  datum_get(actor_data, actor_handle);
  action_ref = (char *)datum_get(*(data_t **)0x6324ec, action_handle);
  tag_elem =
    (char *)tag_block_get_element((char *)global_scenario_get() + 0x468,
                                  (int)*(int16_t *)(action_ref + 2), 0x74);
  if (state_data == (int *)0) {
    display_assert("state_data", "c:\\halo\\SOURCE\\ai\\action_converse.c",
                   0x21, 1);
    system_exit(-1);
  }
  csmemset(state_data, 0, 0x14);
  *state_data = action_handle;
  fVar1 = *(float *)(tag_elem + 0x28);
  *(float *)((char *)state_data + 8) = fVar1;
  if (fVar1 == *(float *)0x2533c0) {
    speaker = -1;
  } else {
    speaker = *(int *)(action_ref + 0x10);
  }
  state_data[3] = speaker;
  state_data[4] = -1;
  *(char *)((char *)state_data + 5) = 0;
  return 1;
}

/* actor_update_prop_desire (0x14360)
 * Update actor's desire to reach its prop target.
 *
 * If the actor has no current prop (field_ac == -1) but has a follow-prop
 * handle (field_a8 != -1), acquires a prop near the follow target via
 * FUN_00064b40.  Then, if a prop is held, checks whether the prop's type
 * qualifies (field_32 >= 2) and its distance (field_11c) is within the
 * actor's tolerance (field_a4), or the prop distance is less than 0.7f; if
 * so, marks the actor as ready (field_a1 = 1).  When ready, calls
 * FUN_0002f1a0 (perception acknowledge) and returns the current state byte
 * (field_a0).  Otherwise tries to move toward the prop via
 * actor_move_to_prop; sets field_a0 = 1 if no prop at all, or if
 * actor_move_to_prop returns 0.
 *
 * Confirmed: EBX = 1 set at 0x1438c; used as literal arg to FUN_00064b40
 *   and as byte written to field_a1 at 0x14408 and field_a0 at 0x14447.
 * Confirmed: actor_move_to_prop pushes [ESI+0xa4] raw (float bits) as 3rd arg.
 * Confirmed: field_32 comparison is signed short CMP vs 2 (JL 0x143f5).
 * Confirmed: float threshold at 0x2533c4 = 0.7f (0x3f333333).
 * Confirmed: early return at 0x1441e returns field_a0 without setting it. */
char actor_update_prop_desire(int actor_handle)
{
  char *actor;
  char *prop;
  char a1_flag;

  actor = (char *)datum_get(actor_data, actor_handle);
  if (*(char *)(actor + 0x4c) == '\0') {
    return *(char *)(actor + 0xa0);
  }
  if ((*(int *)(actor + 0xac) == -1) && (*(int *)(actor + 0xa8) != -1)) {
    *(int *)(actor + 0xac) =
      FUN_00064b40(actor_handle, *(int *)(actor + 0xa8), 1, 1);
  }
  if (*(int *)(actor + 0xac) == -1) {
    *(char *)(actor + 0xa0) = 1;
    return *(char *)(actor + 0xa0);
  }
  a1_flag = *(char *)(actor + 0xa1);
  if (a1_flag == '\0') {
    prop = (char *)datum_get(prop_data, *(int *)(actor + 0xac));
    if ((*(short *)(prop + 0x32) >= 2 &&
         *(float *)(prop + 0x11c) < *(float *)(actor + 0xa4)) ||
        (*(float *)(prop + 0x11c) < *(float *)0x2533c4)) {
      *(char *)(actor + 0xa1) = 1;
    }
  }
  if (*(char *)(actor + 0xa1) != '\0') {
    FUN_0002f1a0(actor_handle);
    return *(char *)(actor + 0xa0);
  }
  if (actor_move_to_prop(actor_handle, *(int *)(actor + 0xac),
                         *(float *)(actor + 0xa4)) == '\0') {
    *(char *)(actor + 0xa0) = 1;
  }
  return *(char *)(actor + 0xa0);
}

/* FUN_00014460 (0x14460)
 * Validate actor handle by performing a datum lookup (result discarded). */
void FUN_00014460(int actor_handle)
{
  datum_get(actor_data, actor_handle);
}

/* FUN_00014480 (0x14480)
 * Set up an actor's prop-look reference from its conversation or existing prop.
 *
 * Looks up the actor's conversation handle (actor+0x9c).  If a conversation
 * is active, fetches the conversation record and, if the actor has no current
 * prop (actor+0xac == -1), looks up the active prop for the conversation's
 * unit (conversation+0x10) via prop_get_active_by_unit_index.  Then marks
 * actor+0x3fc = 1 (look-spec active).  If a prop was resolved, sets the
 * look-type word at actor+0x3e8 = 3, the look-active flag at actor+0x3ec = 1,
 * and stores the prop handle at actor+0x3f0.
 *
 * Confirmed: datum_get(actor_data, actor_handle); ESI = actor record.
 * Confirmed: CMP dword [ESI+0x9c],-0x1; JZ skip; PUSH [ESI+0x9c]; MOV
 *   ECX,[0x6324ec]; PUSH ECX; CALL datum_get → EAX = conversation ptr.
 * Confirmed: MOV ECX,[ESI+0xac]; CMP ECX,-0x1; JZ prop_none;
 *   MOV EDI,ECX.
 * Confirmed: TEST EAX,EAX; JZ done; MOV EAX,[EAX+0x10]; CMP EAX,-0x1;
 *   JZ done; PUSH EAX; PUSH EBX; CALL 0x64ab0 → prop_get_active_by_unit_index.
 * Confirmed: MOV word [ESI+0x3fc],1; if EDI!=-1: MOV word [ESI+0x3e8],3;
 *   MOV word [ESI+0x3ec],1; MOV dword [ESI+0x3f0],EDI. */
void FUN_00014480(int actor_handle)
{
  char *actor;
  char *conversation;
  int prop_handle;

  actor = (char *)datum_get(actor_data, actor_handle);
  conversation = NULL;
  if (*(int *)(actor + 0x9c) != -1)
    conversation =
      (char *)datum_get(*(data_t **)0x6324ec, *(int *)(actor + 0x9c));
  prop_handle = *(int *)(actor + 0xac);
  if (prop_handle == -1 && conversation != NULL &&
      *(int *)(conversation + 0x10) != -1)
    prop_handle = prop_get_active_by_unit_index(actor_handle,
                                                *(int *)(conversation + 0x10));
  *(int16_t *)(actor + 0x3fc) = 1;
  if (prop_handle != -1) {
    *(int16_t *)(actor + 0x3e8) = 3;
    *(int16_t *)(actor + 0x3ec) = 1;
    *(int *)(actor + 0x3f0) = prop_handle;
  }
}

/* actor_set_prop_if_match (0x14510)
 * Conditionally replace an actor's prop handle if it matches old_prop.
 *
 * If the actor's current prop handle at actor+0xac equals old_prop, replace
 * it with new_prop.  Used to safely swap out a prop reference without
 * touching actors that already moved to a different prop.
 *
 * Confirmed: datum_get(actor_data, actor_handle) from decompile.
 * Confirmed: compare *(int *)(actor+0xac) == old_prop; store new_prop on
 *   match. */
void actor_set_prop_if_match(int actor_handle, int old_prop, int new_prop)
{
  char *actor;
  actor = (char *)datum_get(actor_data, actor_handle);
  if (*(int *)(actor + 0xac) == old_prop) {
    *(int *)(actor + 0xac) = new_prop;
  }
}

/* FUN_00014540 (0x14540)
 * Initialize actor looking state from the scripted look target at activation.
 *
 * Called during actor activation (FUN_0003ec80) after prop and movement init.
 * Reads the actor's scripted-look target handle at actor+0x1dc.  If set (not
 * -1) and the actor also has a secondary-look object handle at actor+0x1e0,
 * it attempts to find an existing look-at entry for that object via
 * prop_get_active_by_unit_index.  If one is found it is passed as an
 * object-handle look target (type 1); otherwise the object's position is
 * fetched via unit_get_head_position and a position look target (type 3) is
 * issued via FUN_00027a60.
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x14552.
 * Confirmed: guard on [actor+0x1dc] != -1 AND [actor+0x1e0] != -1 at
 *   0x14559/0x14567.
 * Confirmed: prop_get_active_by_unit_index(actor_handle, [actor+0x1e0]) at
 * 0x14574. Confirmed: branch on return == -1 at 0x1457c/0x1457f. Confirmed:
 * type=1 path: MOV word [EBP-0x10],1; MOV [EBP-0xc],EAX at 0x14581/0x14587.
 * Confirmed: type=3 path: MOV word [EBP-0x10],3 at 0x14597; CALL 0x1a9200
 *   with args ([actor+0x1e0], &buf[1]) at 0x1459d.
 * Confirmed: FUN_00027a60(actor_handle, 8, 5, &buf) at 0x145ae.
 * Confirmed: cdecl: ADD ESP,0x10 after FUN_00027a60; ADD ESP,0x8 after
 *   prop_get_active_by_unit_index and unit_get_head_position. */
void FUN_00014540(int actor_handle)
{
  char *actor;
  int look_object; /* [actor+0x1e0]: handle of scripted look object */
  int look_entry; /* return from prop_get_active_by_unit_index: existing look
                     entry or -1 */

  /* Buffer passed to FUN_00027a60: { int16_t type; int16_t pad; int data[3]; }
   * type=1 (object handle look): data[0] = look_entry handle
   * type=3 (position look):      data[0..2] = xyz position from
   * unit_get_head_position Total: 2 + 2(pad) + 12 = 16 bytes (0x10), matching
   * SUB ESP,0x10 */
  short look_buf[8]; /* 16 bytes on stack: [0]=type word, [2..7]=data dwords */

  actor = (char *)datum_get(actor_data, actor_handle);

  if (*(int *)(actor + 0x1dc) == -1)
    return;
  if (*(int *)(actor + 0x1e0) == -1)
    return;

  look_object = *(int *)(actor + 0x1e0);

  look_entry = prop_get_active_by_unit_index(actor_handle, look_object);
  if (look_entry != -1) {
    /* Existing look entry found: use it as an object-handle target (type 1) */
    look_buf[0] = 1;
    *(int *)&look_buf[2] = look_entry;
  } else {
    /* No existing look entry: use object's world position (type 3) */
    look_buf[0] = 3;
    unit_get_head_position(look_object, (float *)&look_buf[2]);
  }

  FUN_00027a60(actor_handle, 8, 5, look_buf);
}

/* FUN_000145c0 (0x145c0)
 * Stop any active conversation for this actor.
 *
 * Reads the actor's conversation handle at actor+0x1dc.  If it is not -1
 * (i.e. there is an active conversation), calls ai_conversation_finish with
 * that handle, no advance (0), and no explicit end (0).
 *
 * Confirmed: datum_get(actor_data, actor_handle) pattern from disassembly.
 * Confirmed: guard on [actor+0x1dc] != -1 before call.
 * Confirmed: ADD ESP,0xC after call — 3 cdecl args.
 * Confirmed: ai_conversation_finish(*(int *)(actor+0x1dc), 0, 0). */
void FUN_000145c0(int actor_handle)
{
  char *actor;
  actor = (char *)datum_get(actor_data, actor_handle);
  if (*(int *)(actor + 0x1dc) != -1) {
    ai_conversation_finish(*(int *)(actor + 0x1dc), 0, 0);
  }
}

/* FUN_000145f0 (0x145f0)
 * Stop any active conversation for this actor (second variant).
 *
 * Structurally identical to FUN_000145c0.  Two separate functions exist in
 * the binary at distinct addresses; both read actor+0x1dc and call
 * ai_conversation_finish if a conversation is active.
 *
 * Confirmed: identical decompile to FUN_000145c0 at 0x145f0.
 * Inferred: two entry points probably correspond to different calling
 *   contexts (e.g. voluntary vs forced conversation stop). */
void FUN_000145f0(int actor_handle)
{
  char *actor;
  actor = (char *)datum_get(actor_data, actor_handle);
  if (*(int *)(actor + 0x1dc) != -1) {
    ai_conversation_finish(*(int *)(actor + 0x1dc), 0, 0);
  }
}

/* FUN_00014620 (0x14620)
 * Initialize action fight state: asserts non-null state pointer, zeroes 4
 * bytes, returns true.
 *
 * Confirmed: cdecl, two args: [EBP+0x8]=actor_handle (unused),
 * [EBP+0xc]=state_data. Confirmed: TEST ESI,ESI at 0x14627;
 * csmemset(state_data,0,4) at 0x1464d. Confirmed: MOV AL,0x1 at 0x14655 (byte
 * return). */
char FUN_00014620(int actor_handle, void *state_data)
{
  if (state_data == NULL) {
    display_assert("state_data", "c:\\halo\\SOURCE\\ai\\action_fight.c", 0x1e,
                   1);
    system_exit(-1);
  }
  csmemset(state_data, 0, 4);
  return 1;
}

/* FUN_00014680 (0x14680)
 * Countdown timer for actor action state: decrements actor+0x9c when flag
 * actor+0x484 is set; when the counter hits zero and a pending action state
 * exists (actor+0x3b8 != -1) with no transition in progress (actor+0x3ba == 0),
 * dispatches the state via FUN_00024be0.
 */
void FUN_00014680(int actor_handle)
{
  char *actor;
  short cnt;
  short state;

  actor = (char *)datum_get(actor_data, actor_handle);
  if (*(short *)(actor + 0x9c) > 0 && *(char *)(actor + 0x484) != '\0') {
    cnt = *(short *)(actor + 0x9c) - 1;
    *(short *)(actor + 0x9c) = cnt;
    if (cnt == 0) {
      state = *(short *)(actor + 0x3b8);
      if (state != -1 && *(char *)(actor + 0x3ba) == '\0') {
        FUN_00024be0(actor_handle, state, 0);
      }
    }
  }
}

/* FUN_000146f0 (0x146f0)
 * Initialize actor scripted-look state (type 5 target, scripted look mode).
 *
 * Copies the scripted-look timer seed from actor+0x358 to actor+0x426.
 * Sets look priority (actor+0x3e8) to 5, look-spec type (actor+0x3ec) to 2,
 * another look field (actor+0x3fc) to 4, and clears four flag bytes starting
 * at actor+0x424.
 *
 * If the actor's behavior type (actor+0x15e) is not 4 AND the actor's team
 * type (actor+0x6e) is > 4, also sets actor+0x454 = 1 and raises the look
 * priority to 7.
 *
 * Confirmed: datum_get(actor_data, actor_handle) from decompile.
 * Inferred: actor+0x3e8 = look priority (int16_t); actor+0x3ec = look spec
 *   type (int16_t); actor+0x3fc = look frame type (int16_t).
 * Inferred: actor+0x424..0x428 = look state flags (char[5]).
 * Inferred: actor+0x15e = behavior type (int16_t); actor+0x6e = team (int16_t).
 * Inferred: actor+0x454 = scripted look override flag (char). */
void FUN_000146f0(int actor_handle)
{
  char *actor;
  actor = (char *)datum_get(actor_data, actor_handle);
  *(char *)(actor + 0x426) = *(char *)(actor + 0x358);
  *(short *)(actor + 1000) = 5; /* actor+0x3e8 = look priority */
  *(short *)(actor + 0x3ec) = 2; /* look spec type */
  *(short *)(actor + 0x3fc) = 4; /* look frame type */
  *(char *)(actor + 0x427) = 0;
  *(char *)(actor + 0x428) = 0;
  *(char *)(actor + 0x424) = 0;
  *(char *)(actor + 0x425) = 0;
  if ((*(short *)(actor + 0x15e) != 4) && (*(short *)(actor + 0x6e) >= 4)) {
    *(char *)(actor + 0x454) = 1;
    *(short *)(actor + 1000) = 7; /* raise look priority */
  }
}

/* FUN_00014770 (0x14770) — Combat fight-action state evaluator.
 *
 * Evaluates the actor's combat targeting state. Two execution paths:
 *   PATH A (field_358 && tag_bit5): quick check via FUN_00025a00.
 *     Success → encounter checks + optional firing position reset → tail.
 *     Failure → fall through to PATH B.
 *   PATH B: evaluation pipeline (csmemset → FUN_00027090 → FUN_000272d0),
 *     then randomized delay timer from tag range and weapon rate-of-fire.
 *   TAIL: if actor->field_268 >= 7, prop unreachable check with ranged weapon.
 *
 * Confirmed: cdecl, single stack arg (actor_handle).
 * Confirmed: _chkstk(0x1474c) at entry.
 * Confirmed: datum_get(actor_data, actor_handle) at 0x1478a.
 * Confirmed: assert !actor->meta.swarm at 0x14797.
 * Confirmed: guard on actor->field_4c at 0x147bb.
 * Confirmed: tag_get(actr) at 0x147cc; FUN_000211f0 at 0x147d7.
 * Confirmed: field_160 (suppressed) guard at 0x147e8.
 * Confirmed: field_358+tag_bit5 branch at 0x147f0-0x14801.
 * Confirmed: FUN_0003bc90 at 0x14808; FUN_00025a00 at 0x1481e.
 * Confirmed: encounter/fp distance check at 0x1484f-0x1489e.
 * Confirmed: field_504/1fc prop check at 0x148a2-0x148ee.
 * Confirmed: evaluation pipeline at 0x14912-0x1496a.
 * Confirmed: random timer with weapon rate cap at 0x1498f-0x14a1f.
 * Confirmed: tail check at 0x14a26 with actor_has_ranged_weapon (0x14a51)
 *   and actor_perception_unreachable at 0x14b27.
 * Confirmed: XOR AL,AL return at 0x14b31 (always returns 0). */
unsigned int FUN_00014770(int actor_handle)
{
  char *actor;
  char *tag;
  char *variant;
  char *prop;
  char *encounter;
  char *firing_pos;
  short saved_fp;
  int within_range;
  float f_threshold;
  float dist_sq;
  float timer;
  short result;
  static char large_buf[0x670];
  static char huge_buf[0x1474c];
  int local_14;
  int local_50[12]; /* FUN_00027090 writes 12 ints (48 bytes); was float[3]=12
                       bytes causing overflow */
  char local_c;

  actor = (char *)datum_get(actor_data, actor_handle);

  if (*(char *)(actor + 6) != '\0') {
    display_assert("!actor->meta.swarm", "c:\\halo\\SOURCE\\ai\\action_fight.c",
                   0x37, 1);
    system_exit(-1);
  }

  if (*(char *)(actor + 0x4c) == '\0')
    return 0;

  tag = (char *)tag_get(0x61637472, *(int *)(actor + 0x58));
  variant = actor_combat_get_firing_variant_definition(actor_handle);

  if (*(char *)(actor + 0x160) != '\0')
    goto tail;

  if (*(char *)(actor + 0x358) != '\0' && (*tag & 0x20)) {
    actor_find_pathfinding_location(actor_handle);

    if (FUN_00025a00(actor_handle, (int *)(actor + 0x168),
                     *(int *)(actor + 0x164), 0) != '\0') {
      within_range = 0;

      if (*(int *)(actor + 0x34) != -1 && *(short *)(actor + 0x3b8) != -1) {
        encounter = (char *)tag_block_get_element(
          (char *)global_scenario_get() + 0x42c,
          *(unsigned int *)(actor + 0x34) & 0xffff, 0xb0);
        firing_pos = (char *)tag_block_get_element(
          encounter + 0x98, (int)*(short *)(actor + 0x3b8), 0x18);

        f_threshold = actor_destination_tolerance(actor_handle);
        dist_sq =
          distance_squared3d((float *)(actor + 0x12c), (float *)firing_pos);

        if (dist_sq <= f_threshold * f_threshold)
          within_range = 1;
      }

      if (*(char *)(actor + 0x504) != '\0' &&
          *(char *)(actor + 0x1fc) == '\0') {
        if (*(int *)(actor + 0x270) != -1) {
          prop = (char *)datum_get(prop_data, *(int *)(actor + 0x270));
          if (*(float *)(prop + 0x11c) < *(float *)(variant + 0xa0))
            goto reset_fp;
        }
        goto tail;
      }

      if (within_range == 0)
        goto reset_fp;

      goto tail;
    }
  }

  saved_fp = *(short *)(actor + 0x3b8);
  csmemset(large_buf, 0, 0x670);
  *(short *)(large_buf + 4) = 0;

  result = FUN_00027090(actor_handle, large_buf, local_50, &local_14, huge_buf,
                        &local_c);
  result = FUN_000272d0(actor_handle, (short)result, (short)result, local_14,
                        (unsigned int)(int)huge_buf, local_c);

  if (result == -1) {
    *(short *)(actor + 0x9c) = 0;
    goto tail;
  }

  if (result != saved_fp) {
    timer = random_real_range(get_global_random_seed_address(),
                              *(float *)(tag + 0x3c0), *(float *)(tag + 0x3c4));

    if (*(short *)(actor + 0x15e) > 0) {
      char *weap_tag;
      weap_tag = (char *)tag_get(
        0x76656869, *(unsigned int *)(char *)object_get_and_verify_type(
                      *(int *)(actor + 0x158), 3));
      if (*(float *)(weap_tag + 0x3a8) > *(float *)0x2533c0 &&
          timer > *(float *)(weap_tag + 0x3a8)) {
        timer = *(float *)(weap_tag + 0x3a8);
      }
    }

    *(short *)(actor + 0x9c) = (short)(int)(timer * *(float *)0x253394);
  }
  goto tail;

reset_fp:
  *(short *)(actor + 0x3b8) = -1;
  FUN_0002f1a0(actor_handle);

tail:
  if (*(short *)(actor + 0x268) < 7)
    return 0;

  prop = (char *)datum_get(prop_data, *(int *)(actor + 0x270));
  within_range = 1;

  if (actor_has_ranged_weapon(actor_handle) != '\0') {
    if (*(float *)(prop + 0x11c) < *(float *)(actor + 0x608)) {
      within_range = 0;
    } else if (*(int *)(actor + 0x34) != -1 &&
               *(short *)(actor + 0x3b8) != -1) {
      encounter = (char *)tag_block_get_element(
        (char *)global_scenario_get() + 0x42c,
        *(unsigned int *)(actor + 0x34) & 0xffff, 0xb0);
      firing_pos = (char *)tag_block_get_element(
        encounter + 0x98, (int)*(short *)(actor + 0x3b8), 0x18);
      f_threshold = actor_destination_tolerance(actor_handle);
      dist_sq =
        distance_squared3d((float *)(actor + 0x12c), (float *)firing_pos);
      if (dist_sq < f_threshold * f_threshold) {
        dist_sq =
          distance_squared3d((float *)(prop + 0xbc), (float *)firing_pos);
        if (dist_sq <= *(float *)(actor + 0x608) * *(float *)(actor + 0x608))
          within_range = 0;
      }
    }
  }

  actor_perception_unreachable(actor_handle, *(int *)(actor + 0x270),
                               (char)within_range);
  return 0;
}

/* FUN_00014b40 (0x14b40)
 * Stop the actor's host unit from running blindly.
 *
 * Reads the unit handle at actor+0x18.  If valid (not -1), calls
 * unit_stop_running_blindly with that unit handle, clearing the blindly-running
 * flag on the unit.
 *
 * Confirmed: datum_get(actor_data, actor_handle) from decompile.
 * Confirmed: guard on [actor+0x18] != -1.
 * Confirmed: unit_stop_running_blindly(*(int *)(actor+0x18)) — single cdecl
 * arg. Inferred: actor+0x18 = unit datum handle. */
void FUN_00014b40(int actor_handle)
{
  char *actor;
  actor = (char *)datum_get(actor_data, actor_handle);
  if (*(int *)(actor + 0x18) != -1) {
    unit_stop_running_blindly(*(int *)(actor + 0x18));
  }
}

/* FUN_00014b70 (0x14b70)
 * Mark actor as controlled (scripted look override active).
 *
 * Sets the actor's control word at actor+0xa4 to 0xffff (all bits set) and
 * sets the controlled flag byte at actor+0xa2 to 1.
 *
 * Confirmed: datum_get(actor_data, actor_handle) from decompile.
 * Inferred: actor+0xa4 = control/override handle (int16_t, 0xffff = all).
 * Inferred: actor+0xa2 = controlled flag (char). */
void FUN_00014b70(int actor_handle)
{
  char *actor;
  actor = (char *)datum_get(actor_data, actor_handle);
  *(short *)(actor + 0xa4) = (short)0xffff;
  *(char *)(actor + 0xa2) = 1;
}

/* FUN_00014ba0 (0x14ba0)
 * Select an actor look-direction weight vector based on look state.
 *
 * If the actor's look-count at actor+0xa8 (int16_t) is positive, copies the
 * 16-byte vector from the global pointer at 0x2ee6e0; otherwise copies from
 * the global pointer at 0x2ee6d4.
 *
 * Confirmed: datum_get(actor_data, actor_handle) → actor.
 * Confirmed: CMP WORD PTR [EAX+0xc], 0 after ADD EAX,0x9c → actor+0xa8.
 * Confirmed: 16-byte (4 dword) copy from dereferenced global pointer. */
void FUN_00014ba0(int actor_handle, int *param_2)
{
  char *actor;
  char *looking;
  char *src;

  actor = (char *)datum_get(actor_data, actor_handle);
  looking = actor + 0x9c;
  if (*(short *)(looking + 0xc) > 0) {
    src = *(char **)0x2ee6e0;
    *param_2 = *(int *)src;
    param_2[1] = *(int *)(src + 4);
    param_2[2] = *(int *)(src + 8);
    param_2[3] = *(int *)(src + 0xc);
    return;
  }
  src = *(char **)0x2ee6d4;
  *param_2 = *(int *)src;
  param_2[1] = *(int *)(src + 4);
  param_2[2] = *(int *)(src + 8);
  param_2[3] = *(int *)(src + 0xc);
}

/* FUN_00014c10 (0x14c10) — Recompute the actor's firing-position target.
 *
 * Register args: actor_handle@<ebx>, look_state@<esi>. One cdecl stack arg
 * (param_1) is pushed by both callers (0 from FUN_00015040, 1 from
 * FUN_00015520) but is never read by the body. The body clobbers EBX/ESI;
 * the callers preserve them.
 *
 * Builds a firing-position request in a 0x670-byte scratch buffer, runs the
 * firing-position evaluation pipeline (actor_get_firing_position_group,
 * FUN_00025c10, FUN_000272d0) to pick a firing position, stores it into
 * look_state+0x8 / +0xa, then (if a position and a prop were found) builds a
 * path-state to it and tests whether an approach point is reachable, storing
 * the result into look_state+0x20.
 *
 * Confirmed: _chkstk(0x28820) at 0x14c18.
 * Confirmed: datum_get(actor_data, actor_handle) -> actor (EDI) at 0x14c25.
 * Confirmed: tag_get('actr', actor->field_58) at 0x14c38.
 * Confirmed: assert !actor->meta.timeslice (actor+0x6) at 0x14c40-0x14c5b.
 * Confirmed: csmemset(large_buf, 0, 0x670) at 0x14c78.
 * Confirmed: branch on signed look_state+0xc vs 0 at 0x14c83 (JLE).
 *   look_state+0xc > 0 path: buf+0x4=1; if look_state+0x0>0 then buf+0x14=1,
 *   buf+0x15=1; buf+0x36=1, buf+0x38=10.0f, buf+0x3c=6.0f.
 *   else path: buf+0x8=look_state+0x5; buf+0x4=2; FCOMP [tag+0x320] vs 0.0f
 *   (TEST AH,0x41 JNZ at 0x14cf0): if tag+0x320>0 buf+0x1c=tag+0x320 else 6.0f.
 *   Always: buf+0x41 = look_state+0x4.
 * Confirmed: actor_get_firing_position_group(actor_handle, buf+0x4, 0) at
 *   0x14d14; result stored at buf+0x0; FUN_00025c10 at 0x14d3a;
 *   FUN_000272d0 at 0x14d58. Result (AX) stored to look_state+0x8 at 0x14d54
 *   then again at 0x14d64.
 * Confirmed: look_state+0xa = (result != -1 && local_5 == 0) at
 * 0x14d6a-0x14d7e. Confirmed: look_state+0x20 = 0 at 0x14d81. Confirmed: if
 * result != -1 && look_state+0x1c != -1: datum_get(prop_data =DAT_005ab23c,
 * look_state+0x1c) -> prop (EDI) at 0x14d9e. if prop+0x24 in [2,3]:
 * actor_perception_find_prop_pathfinding_location( actor_handle,
 * look_state+0x1c) at 0x14dbd. src = prop+0x110 != -1 ? prop+0x110 : prop+0x18
 * at 0x14dc5-0x14dd0. path_input_new(path_input, tag+0x8c, 0, src) at 0x14de7.
 *   path_input_set_start(path_input, prop+0xf0, prop+0xec) at 0x14e01.
 *   paths_dispose(path_input, actor+0x18) at 0x14e14.
 *   path_state_new(path_input, path_state, 0) at 0x14e29.
 *   FUN_0005e0d0(path_state, *local_50, (*local_50)+0x14, 0) at 0x14e3f.
 *   if FUN_0005ff70(path_state) != 0: look_state+0x20 =
 *     path_state_approach_point(path_state, *local_50, (*local_50)+0x14,
 *     &local_1, look_state+0x24) at 0x14e71.
 * Confirmed: look_state+0x6 = 0 at 0x14e7c (always, on all paths).
 * Inferred: large_buf is the firing-position-query struct (same 0x670 layout
 *   and pipeline as FUN_000163d0). huge_buf is FUN_00025c10/272d0 scratch.
 * Uncertain: exact field meanings inside large_buf/path buffers (opaque). */
void FUN_00014c10(int actor_handle, char *look_state, int param_1)
{
  char *actor;
  char *tag;
#if defined(_MSC_VER) && !defined(__clang__)
  char large_buf[0x670];
  char path_input[0x98];
  char path_state[0x1408c];
  char huge_buf[0x1408c];
#else
  static char large_buf[0x670];
  static char path_input[0x98];
  static char path_state[0x1408c];
  static char huge_buf[0x1408c];
#endif
  int local_50[12]; /* FUN_00025c10 out1 buffer (first dword = pos ptr) */
  int local_14;
  char local_5;
  char local_1;
  short result;
  char looking_active;

  (void)param_1;

  actor = (char *)datum_get(actor_data, actor_handle);
  tag = (char *)tag_get(0x61637472, *(int *)(actor + 0x58));

  if (*(char *)(actor + 0x6) != '\0') {
    display_assert("!actor->meta.timeslice",
                   "c:\\halo\\SOURCE\\ai\\actor_looking.c", 0x20a, 1);
    system_exit(-1);
  }

  csmemset(large_buf, 0, 0x670);

  if (*(short *)(look_state + 0xc) > 0) {
    *(short *)(large_buf + 4) = 1;
    if (*(short *)look_state > 0) {
      large_buf[0x14] = 1;
      large_buf[0x15] = 1;
    }
    large_buf[0x36] = 1;
    *(float *)(large_buf + 0x38) = 10.0f;
    *(float *)(large_buf + 0x3c) = 6.0f;
  } else {
    large_buf[8] = look_state[5];
    *(short *)(large_buf + 4) = 2;
    if (*(float *)(tag + 0x320) > 0.0f) {
      *(float *)(large_buf + 0x1c) = *(float *)(tag + 0x320);
    } else {
      *(float *)(large_buf + 0x1c) = 6.0f;
    }
  }
  large_buf[0x41] = look_state[4];

  *(int *)large_buf =
    actor_get_firing_position_group(actor_handle, *(short *)(large_buf + 4), 0);

  result = (short)FUN_00025c10(actor_handle, large_buf, local_50, &local_14,
                               huge_buf, (int *)&local_5);
  *(short *)(look_state + 8) = result;
  result = FUN_000272d0(actor_handle, result, result, local_14,
                        (unsigned int)(int)huge_buf, local_5);
  *(short *)(look_state + 8) = result;

  if (result != -1 && local_5 == '\0') {
    looking_active = 1;
  } else {
    looking_active = 0;
  }
  look_state[0xa] = looking_active;
  look_state[0x20] = 0;

  if (result != -1 && *(int *)(look_state + 0x1c) != -1) {
    char *prop;
    short prop_type;
    int src;

    prop = (char *)datum_get(*(data_t **)0x5ab23c, *(int *)(look_state + 0x1c));
    prop_type = *(short *)(prop + 0x24);
    if (prop_type >= 2 && prop_type <= 3) {
      actor_perception_find_prop_pathfinding_location(
        actor_handle, *(int *)(look_state + 0x1c));
    }

    src = *(int *)(prop + 0x110);
    if (src == -1) {
      src = *(int *)(prop + 0x18);
    }
    path_input_new(path_input, *(unsigned int *)(tag + 0x8c), 0, src);
    path_input_set_start(path_input, (float *)(prop + 0xf0),
                         *(int *)(prop + 0xec));
    paths_dispose(path_input, *(int *)(actor + 0x18));
    path_state_new(path_input, path_state, 0);
    FUN_0005e0d0(path_state, (float *)local_50[0], *(int *)(local_50[0] + 0x14),
                 0);
    if (FUN_0005ff70((unsigned int *)path_state) != '\0') {
      look_state[0x20] = path_state_approach_point(
        (int)path_state, local_50[0], *(int *)(local_50[0] + 0x14),
        (unsigned char *)&local_1, (int *)(look_state + 0x24));
    }
  }

  look_state[6] = 0;
}

/* FUN_00014e90 (0x14e90)
 * Flee action: evaluate whether the actor should flee to a firing position.
 *
 * EAX = actor_handle (register arg), stack = state_data pointer.
 * Original caller (FUN_00015520 at 0x15697): MOV EAX,[EBP+8]; PUSH ESI;
 * CALL 0x14e90 — ESI = actor+0x9c (flee action state block).
 *
 * Reads state_data+0x1c (prop handle), state_data+0x8 (firing position idx).
 * If valid target, encounter, and firing position, estimates target position,
 * tests LOS, and copies target position to state_data+0x24 if LOS blocked.
 * Returns 1 if flee target is in LOS state [2,3], else 0.
 *
 * Confirmed: PUSH EAX at 0x14e9f; CALL datum_get → EAX is actor_handle.
 * Confirmed: MOV ESI,[EBP+0x8] at 0x14ed7 → stack param is state_data.
 * Confirmed: assert "!actor->meta.swarm" at action_flee.c line 0x265. */
char FUN_00014e90(int actor_handle, char *state_data)
{
  char *actor;
  short sVar1;
  char *prop;
  char *encounter_elem;
  char *fp_elem;
  float local_14[4];
  char local_5;

  actor = (char *)datum_get(actor_data, actor_handle);
  local_5 = 0;
  if (*(char *)(actor + 6) != '\0') {
    display_assert("!actor->meta.swarm", "c:\\halo\\SOURCE\\ai\\action_flee.c",
                   0x265, 1);
    system_exit(-1);
  }
  if (*(int *)(state_data + 0x1c) != -1 &&
      *(unsigned int *)(actor + 0x34) != 0xffffffff &&
      *(short *)(state_data + 8) != -1) {
    encounter_elem = (char *)tag_block_get_element(
      (char *)global_scenario_get() + 0x42c,
      *(unsigned int *)(actor + 0x34) & 0xffff, 0xb0);
    prop = (char *)datum_get(*(data_t **)0x5ab23c, *(int *)(state_data + 0x1c));
    fp_elem = (char *)tag_block_get_element(
      encounter_elem + 0x98, (int)*(short *)(state_data + 8), 0x18);
    unit_estimate_position(*(int *)(actor + 0x18), 2, (void *)fp_elem, 0, 0,
                           (void *)local_14);
    sVar1 = ai_test_line_of_sight(
      local_14, (int)*(unsigned short *)(fp_elem + 0xe),
      (float *)(prop + 0x104), (int)*(unsigned short *)(prop + 0x100), 1, 0,
      *(int *)(prop + 0x110), *(int *)(actor + 0x158) != -1);
    if (*(short *)(prop + 0x24) >= 2 && *(short *)(prop + 0x24) <= 3) {
      local_5 = sVar1 == 0;
    }
    if (sVar1 == 0 || sVar1 == 3) {
      *(char *)(state_data + 0x20) = 1;
      *(int *)(state_data + 0x24) = *(int *)(prop + 0xbc);
      *(int *)(state_data + 0x28) = *(int *)(prop + 0xc0);
      *(int *)(state_data + 0x2c) = *(int *)(prop + 0xc4);
    }
    return local_5;
  }
  return 0;
}

/* FUN_00014ff0 (0x14ff0)
 * Conditionally update the actor's look-frame field.
 *
 * If the current look-frame value at actor+0xb8 equals param_2, replaces
 * it with param_3.  This acts as a guarded transition — only fires if the
 * actor is in the expected look state.
 *
 * Confirmed: datum_get(actor_data, actor_handle) from decompile.
 * Confirmed: compare [actor+0xb8] == param_2 before write.
 * Inferred: actor+0xb8 = look frame / look state id (int). */
void FUN_00014ff0(int actor_handle, int param_2, int param_3)
{
  char *actor;
  actor = (char *)datum_get(actor_data, actor_handle);
  if (*(int *)(actor + 0xb8) == param_2) {
    *(int *)(actor + 0xb8) = param_3;
  }
}

/* FUN_00015020 (0x15020)
 * Test whether a look-state value is in the scripted-look priority range
 * [9,12].
 *
 * Returns 1 (true) if param_1 is in the half-open interval (8, 0xD), i.e.
 * 9 <= param_1 <= 12, else returns 0.
 *
 * Confirmed: 16-bit load via MOV AX,WORD PTR[EBP+8] at 0x15023 — param is
 *   a signed 16-bit short pushed as a dword.
 * Confirmed: CMP AX,9 / JL; CMP AX,0xC / JG; return 1 else return 0. */
int FUN_00015020(short param_1)
{
  if (param_1 >= 9 && param_1 <= 0xc) {
    return 1;
  }
  return 0;
}

/* FUN_00015040 (0x15040)
 * Initialize flee action state for an actor.
 *
 * Validates state_data pointer, zeroes 0x30 bytes, populates fields from
 * params. If a valid target is provided, calls actor_situation_try_new_target.
 * For scripted look types [9,12], rolls a random chance against 0x253524 and
 * may set a short timer. Otherwise dispatches FUN_00014c10(0) to find a
 * firing position.
 *
 * Confirmed: standard cdecl, 7 stack params.
 * Confirmed: NEG CL; SBB ECX,ECX; AND ECX,0xb4 → condition ? 0xb4 : 0. */
char FUN_00015040(int actor_handle, short param_2, int param_3, char param_4,
                  char param_5, char param_6, short *param_7)
{
  char *actor;

  actor = (char *)datum_get(actor_data, actor_handle);
  if (*(char *)(actor + 0x160) != '\0') {
    return 0;
  }
  if (param_7 == (short *)0) {
    display_assert("state_data", "c:\\halo\\SOURCE\\ai\\action_flee.c", 0x2c,
                   1);
    system_exit(-1);
  }
  csmemset(param_7, 0, 0x30);
  *((char *)param_7 + 5) = param_6;
  param_7[4] = (short)0xffff;
  param_7[6] = param_2;
  *(char *)(param_7 + 2) = param_5;
  *(int *)(param_7 + 0xe) = param_3;
  *param_7 = param_4 != '\0' ? (short)0xb4 : 0;
  if (param_3 != -1) {
    actor_situation_try_new_target(actor_handle, param_3);
  }
  if ((short)param_2 >= 9 && (short)param_2 <= 0xc) {
    if (random_math_real((unsigned int *)get_global_random_seed_address()) <
        *(float *)0x253524) {
      param_7[1] = 0x2d;
      return 1;
    }
  }
  if (*(char *)(actor + 6) == '\0') {
    FUN_00014c10(actor_handle, (char *)param_7, 0);
    if (param_7[4] != (short)0xffff) {
      return 1;
    }
    *(char *)(param_7 + 7) = 0;
  }
  return 0;
}

/* FUN_00015150 (0x15150)
 * Reset actor look-frame and conditionally start the unit running blindly.
 *
 * Clears actor+0xb4 (look frame id) to 0.  If actor+0xa8 > 0, clears the
 * byte at actor+0x98.  If actor+0x9e == 0 AND the actor has a unit
 * (actor+0x18 != -1) AND actor+0xa8 is in the scripted-look range [9, 12],
 * calls unit_start_running_blindly with the actor's unit handle.
 *
 * Confirmed: XOR ECX,ECX; MOV dword ptr [EAX+0xb4],ECX at 0x15163/0x1516f.
 * Confirmed: CMP word ptr [EAX+0xa8],CX / JLE at 0x15168/0x15175 — clears
 *   byte [EAX+0x98] if > 0.
 * Confirmed: CMP word ptr [EAX+0x9e],CX / JNZ at 0x1517d/0x15184.
 * Confirmed: CMP ECX,-0x1 / JZ at 0x15189/0x1518c (unit handle guard).
 * Confirmed: MOV AX,word ptr [EAX+0xa8]; CMP AX,0x9 / JL; CMP AX,0xC / JG
 *   at 0x1518e-0x1519f — range [9, 12].
 * Confirmed: PUSH ECX; CALL 0x1ac450; ADD ESP,0x4 at 0x151a1.
 * Inferred: actor+0xa8 = look animation type (int16_t).
 * Inferred: actor+0x9e = scripted-look countdown (int16_t, 0 = expired).
 * Inferred: actor+0x98 = look active flag (char). */
void FUN_00015150(int actor_handle)
{
  char *actor;
  actor = (char *)datum_get(actor_data, actor_handle);
  *(int *)(actor + 0xb4) = 0;
  if (*(short *)(actor + 0xa8) > 0) {
    *(char *)(actor + 0x98) = 0;
  }
  if (*(short *)(actor + 0x9e) == 0 && *(int *)(actor + 0x18) != -1 &&
      *(short *)(actor + 0xa8) >= 9 && *(short *)(actor + 0xa8) <= 0xc) {
    unit_start_running_blindly(*(int *)(actor + 0x18));
  }
}

/* FUN_000151b0 (0x151b0)
 * Advance actor look timers and conditionally start the unit running blindly.
 *
 * Increments actor+0xb4 (look frame count).  Decrements actor+0x9c if > 0.
 * Decrements actor+0x9e if > 0; when it reaches exactly 0 AND the actor has
 * a unit (actor+0x18 != -1) AND actor+0xa8 is in [9, 12], calls
 * unit_start_running_blindly.  If actor+0xa8 > 0, sets actor+0x39c to
 * game_time_get() + 0x2ee.
 *
 * Confirmed: INC EDX; MOV dword ptr [ESI+0xb4],EDX at 0x151d8/0x151dc.
 * Confirmed: TEST AX,AX / JLE; DEC AX; MOV word ptr [ESI+0x9c],AX at
 *   0x151d9-0x151e5 — decrement 0x9c if > 0.
 * Confirmed: MOV AX,[ESI+0x9e] / TEST / JLE; DEC AX; TEST AX,AX / JNZ at
 *   0x151ee-0x15205 — decrement 0x9e, proceed only if result == 0.
 * Confirmed: CMP AX,0x9 / JL; CMP AX,0xC / JG at 0x15216-0x15220.
 * Confirmed: CALL 0xb5aa0 (game_time_get); ADD EAX,0x2ee;
 *   MOV dword ptr [ESI+0x39c],EAX at 0x15235-0x1523f.
 * Inferred: actor+0x9c = secondary look cooldown (int16_t).
 * Inferred: actor+0x39c = next-look-allowed time (int). */
void FUN_000151b0(int actor_handle)
{
  short counter;
  int timer;
  char *actor;
  actor = (char *)datum_get(actor_data, actor_handle);
  *(int *)(actor + 0xb4) = *(int *)(actor + 0xb4) + 1;
  if (*(short *)(actor + 0x9c) > 0) {
    *(short *)(actor + 0x9c) = *(short *)(actor + 0x9c) - 1;
  }
  if (*(short *)(actor + 0x9e) > 0) {
    counter = *(short *)(actor + 0x9e) - 1;
    *(short *)(actor + 0x9e) = counter;
    if (counter == 0 && *(int *)(actor + 0x18) != -1 &&
        *(short *)(actor + 0xa8) >= 9 && *(short *)(actor + 0xa8) <= 0xc) {
      unit_start_running_blindly(*(int *)(actor + 0x18));
    }
  }
  if (*(short *)(actor + 0xa8) > 0) {
    timer = game_time_get();
    *(int *)(actor + 0x39c) = timer + 0x2ee;
  }
}

/* FUN_00015250 (0x15250)
 * Classify the actor's current looking state and configure the firing-position
 * state block fields from live actor data.
 *
 * First, selects a look-mode value written to actor+0x3e8 (the enum field at
 * offset 1000).  Priority order:
 *   1. If actor+0xa8 >= 1 (has props? look-state > 0):
 *        mode=6, actor+0x3ec=0, actor+0x456=1.
 *   2. Else if actor+0x270 != -1 and prop_data[actor+0x270]+0x32 > 0:
 *        mode=7, actor+0x3ec=2, actor+0x454=1.  (forward goto to shared tail)
 *   3. Else if actor+0xb8 == -1:
 *        mode=0.
 *   4. Else:
 *        mode=3, actor+0x3ec=1, actor+0x3f0 = actor+0xb8.
 *
 * Shared tail (always executed):
 *   - actor+0x3fc = 4
 *   - actor+0x428 = (actor+0xa8 > 0) (bool byte)
 *   - actor+0x429 = (actor+0xa8 in [9,12])
 *   - actor+0x426 = 1, actor+0x427 = 0
 *   - actor+0x424 = 1, actor+0x425 = 0
 *
 * Then dispatches the look target:
 *   - If actor+0xa4 == -1: call FUN_0002f1a0 and return.
 *   - If actor+0x4c != 0: try actor_move_to_firing_position.
 *     On success: copy actor+0xa4..0xa6 into actor+0x3b8..0x3ba and return.
 *     On failure: if actor+0x3b8 != -1, dispatch FUN_00024be0 + FUN_0002f1a0
 *                 and clear 0x3b8.  Then set actor+0xa4=-1, actor+0xa2=1.
 *
 * Note: The original has a goto from branch 2 to the shared tail, bypassing
 * branches 3 and 4.  Restructured here with a `handled` flag (no C89 goto).
 * This produces TEST+JNZ instead of a JMP in VC71, which is a known structural
 * ceiling.  Disassembly cross-check not performed (Ghidra MCP unavailable at
 * lift time).
 *
 * Confirmed (decompilation): datum_get(actor_data, actor_handle) at entry;
 *   prop_data lookup at actor+0x270; sentinel comparisons -1 on actor+0xb8,
 *   actor+0xa4, actor+0x3b8.
 * Inferred: actor+0xa8 = look state (int16_t); actor+0x270 = prop handle (int);
 *   actor+0x4c = is_vehicle/prop flag (byte); actor+0x3b8 = cached look target;
 *   actor+0x3ec, 0x3fc = look-mode sub-fields; 0x424-0x429 = look flags. */
void FUN_00015250(int actor_handle)
{
  char *actor;
  char *prop;
  char move_result;

  actor = (char *)datum_get(actor_data, actor_handle);

  if (*(short *)(actor + 0xa8) > 0) {
    *(short *)(actor + 0x3e8) = 6;
    *(short *)(actor + 0x3ec) = 0;
    *(char *)(actor + 0x456) = 1;
  } else {
    if (*(int *)(actor + 0x270) != -1) {
      prop = (char *)datum_get(prop_data, *(int *)(actor + 0x270));
      if (*(short *)(prop + 0x32) > 0) {
        *(short *)(actor + 0x3e8) = 7;
        *(short *)(actor + 0x3ec) = 2;
        *(char *)(actor + 0x454) = 1;
        goto FUN_00015250_tail;
      }
    }
    if (*(int *)(actor + 0xb8) != -1) {
      *(short *)(actor + 0x3e8) = 3;
      *(short *)(actor + 0x3ec) = 1;
      *(int *)(actor + 0x3f0) = *(int *)(actor + 0xb8);
    } else {
      *(short *)(actor + 0x3e8) = 0;
    }
  }
FUN_00015250_tail:

  *(short *)(actor + 0x3fc) = 4;
  *(char *)(actor + 0x428) = *(short *)(actor + 0xa8) > 0;
  if (*(short *)(actor + 0xa8) >= 9 && *(short *)(actor + 0xa8) <= 0xc) {
    *(char *)(actor + 0x429) = 1;
  } else {
    *(char *)(actor + 0x429) = 0;
  }
  *(char *)(actor + 0x426) = 1;
  *(char *)(actor + 0x427) = 0;
  *(char *)(actor + 0x424) = 1;
  *(char *)(actor + 0x425) = 0;

  if (*(short *)(actor + 0xa4) == -1) {
    FUN_0002f1a0(actor_handle);
    return;
  }
  if (*(char *)(actor + 0x4c) != 0) {
    move_result =
      actor_move_to_firing_position(actor_handle, *(short *)(actor + 0xa4), 0);
    if (move_result != 0) {
      *(short *)(actor + 0x3b8) = *(short *)(actor + 0xa4);
      *(char *)(actor + 0x3ba) = *(char *)(actor + 0xa6);
      return;
    }
    if (*(short *)(actor + 0x3b8) != -1) {
      FUN_00024be0(actor_handle, *(short *)(actor + 0x3b8), 0);
      FUN_0002f1a0(actor_handle);
      *(short *)(actor + 0x3b8) = -1;
    }
    *(short *)(actor + 0xa4) = -1;
    *(char *)(actor + 0xa2) = 1;
  }
}

/* FUN_000153e0 (0x153e0)
 * Tests whether the actor's flee target position has been reached.
 * Looks up the firing position from the encounter block using the actor's
 * prop encounter handle and cached look target index, computes the squared
 * distance to the actor's current position, and compares against
 * actor_destination_tolerance.  If within range and the prop at the target
 * has a seat other than 0 or 1 (driver/passenger), the target is reached.
 * Returns false if no valid encounter or firing position, or if a prop
 * with seat 0/1 blocks the position.
 *
 * Confirmed: datum_get(actor_data, actor_handle) at entry; assert on
 *   !actor->meta.swarm (line 0x1d6 in action_flee.c).
 * Confirmed: global_scenario_get + tag_block_get_element chain to resolve
 *   the encounter block and firing position.
 * Confirmed: float subtraction chain at 0x15498-0x154af computes dx/dy/dz;
 *   squared-distance comparison at 0x154b5-0x154c9 (FPU: FMUL/FADDP/FCOMPP).
 * Confirmed: prop seat check (0x38) at 0x154fa-0x15503; seat 0 or 1 returns
 *   false (vehicle driver/passenger seats block the flee target).
 * Confirmed: FUN_0002a3f0 short-circuit — if the actor is already fleeing
 *   with the same target index (0x46c==3 && 0x470==0x3b8), returns true. */
bool FUN_000153e0(int actor_handle)
{
  char *actor;
  char *encounter;
  float *firing_pos;
  float threshold;
  float dx;
  float dy;
  float dz;
  float dist_sq;
  int prop_handle;
  char *prop;

  actor = (char *)datum_get(actor_data, actor_handle);
  if (*(char *)(actor + 0x6) != 0) {
    display_assert("!actor->meta.swarm", "c:\\halo\\SOURCE\\ai\\action_flee.c",
                   0x1d6, 1);
    system_exit(-1);
  }
  if (*(int *)(actor + 0x34) == -1 || *(short *)(actor + 0x3b8) == -1)
    return false;

  encounter =
    (char *)tag_block_get_element((char *)global_scenario_get() + 0x42c,
                                  *(int *)(actor + 0x34) & 0xffff, 0xb0);
  firing_pos = (float *)tag_block_get_element(
    encounter + 0x98, (int)*(short *)(actor + 0x3b8), 0x18);

  if (FUN_0002a3f0(actor_handle) != 0 && *(short *)(actor + 0x46c) == 3 &&
      *(short *)(actor + 0x470) == *(short *)(actor + 0x3b8))
    return true;

  threshold = actor_destination_tolerance(actor_handle);

  dx = firing_pos[0] - *(float *)(actor + 0x12c);
  dy = firing_pos[1] - *(float *)(actor + 0x130);
  dz = firing_pos[2] - *(float *)(actor + 0x134);
  dist_sq = dx * dx + dy * dy + dz * dz;

  if (dist_sq < threshold * threshold) {
    prop_handle = *(int *)(actor + 0xb8);
    if (prop_handle != -1) {
      prop = (char *)datum_get(prop_data, prop_handle);
      if (*(short *)(prop + 0x38) == 0 || *(short *)(prop + 0x38) == 1)
        return false;
      return true;
    }
    return true;
  }
  return false;
}

/* FUN_00015520 (0x15520)
 * action_flee per-tick update.  Manages the actor's flee firing-position
 * target (the "look state" block at actor+0x9c) and drives the appropriate
 * scream/vocalization based on the looking-action type at actor+0xa8.
 *
 * Returns 1 when the actor is unable to flee (actor+0xaa) or has finished
 * fleeing (actor+0xab), 0 otherwise (still fleeing).
 *
 * Confirmed: prototype is cdecl with the actor index at [EBP+8]; EAX holds
 *   the 0/1 return (XOR EAX,EAX / MOV EAX,1 at the two RET sites).
 * Confirmed: datum_get(actor_data, actor_index) at 0x15530; the flee look
 *   state lives at actor+0x9c (ESI base).
 * Confirmed: FUN_000153e0 (target-reached test) is called with the actor
 *   index passed in a register (no PUSH at 0x1559a; EBX = actor index).
 * Confirmed: FUN_00014e90 takes the actor index in EAX (MOV EAX,[EBP+8] at
 *   0x15697) and the look-state pointer pushed.
 * Confirmed: FUN_00014c10 takes the actor index in EBX (0x156c6), the
 *   look-state pointer in ESI, and a stack arg of 1.
 * Confirmed: actor_situation_update_target_status (0x300b0) /
 *   actor_situation_combat_status_update (0x302b0) each take the actor index
 *   as a single stack arg (PUSH EBX at 0x1562f / 0x1563f); decl was (void)
 *   but both consume param_1 as the datum_get handle inside the callee.
 * Confirmed: the looking-action switch (cases 9-12) uses the jump table at
 *   0x15864; the scream dispatch at the tail maps action types 9/10 -> 1
 *   and 11/12 -> 2 for FUN_001a74d0.
 * Confirmed: prop-status writes at 0x1560d-0x15634 target the prop datum
 *   (datum_get(prop_data, actor+0xb8)): +0x32=0, +0x30=max(+0x34,+0x36),
 *   +0x74=0, +0x38=2.
 * Confirmed: assert strings + line numbers (0x98, 0x12e) from action_flee.c.
 */
int FUN_00015520(int actor_index)
{
  char *actor;
  char *look_state;
  char *prop;
  short max_burst;
  int now;
  int scream_unit;
  int scream_type;
  char reached;

  actor = (char *)datum_get(actor_data, actor_index);
  look_state = actor + 0x9c;

  if (*(char *)(actor + 0x6) == 0) {
    if (*(short *)(actor + 0xa8) > 8 && *(short *)(actor + 0xa8) < 0xd)
      *(short *)look_state = 0xb4;

    if (*(short *)(actor + 0x9e) < 1) {
      if (*(short *)(actor + 0xa4) == -1) {
        *(char *)(actor + 0xa2) = 1;
      } else if (*(short *)(actor + 0x3b8) == -1) {
        *(short *)(actor + 0xa4) = (short)0xffff;
        *(char *)(actor + 0xa2) = 1;
      } else {
        reached = FUN_000153e0(actor_index);
        if (reached != 0) {
          if (*(short *)(actor + 0x3b8) == -1) {
            display_assert(
              "actor->firing_positions.current_position_index != NONE",
              "c:\\halo\\SOURCE\\ai\\action_flee.c", 0x98, 1);
            system_exit(-1);
          }
          if (*(short *)look_state == 0) {
            *(short *)(actor + 0xa4) = *(short *)(actor + 0x3b8);
            *(char *)(actor + 0xa6) = *(char *)(actor + 0x3ba);
            *(char *)(actor + 0xab) = 1;
            *(char *)(actor + 0xa2) = 0;
            if (*(int *)(actor + 0xb8) != -1) {
              prop = (char *)datum_get(prop_data, *(int *)(actor + 0xb8));
              *(short *)(prop + 0x32) = 0;
              max_burst = *(short *)(prop + 0x34);
              if (*(short *)(prop + 0x34) <= *(short *)(prop + 0x36))
                max_burst = *(short *)(prop + 0x36);
              *(char *)(prop + 0x74) = 0;
              *(short *)(prop + 0x30) = max_burst;
              *(short *)(prop + 0x38) = 2;
              actor_situation_update_target_status(actor_index);
              actor_situation_combat_status_update(actor_index);
            }
          } else {
            *(char *)(actor + 0xa2) = 1;
          }
        }
      }
    } else {
      *(short *)(actor + 0xa4) = (short)0xffff;
    }

    switch (*(short *)(actor + 0xa8)) {
    case 9:
    case 10:
      if (*(int *)(actor + 0x1b0) == -1)
        *(char *)(actor + 0xab) = 1;
      break;
    case 0xb:
      if (*(char *)(actor + 0x1b4) == 0)
        *(char *)(actor + 0xab) = 1;
      break;
    case 0xc:
      if (*(char *)(actor + 0x1b5) == 0)
        *(char *)(actor + 0xab) = 1;
      break;
    default:
      break;
    }

    if (*(char *)(actor + 0x4c) != 0 && *(char *)(actor + 0xab) == 0) {
      if (*(short *)(actor + 0xa4) != -1 && *(short *)look_state == 0 &&
          FUN_00014e90(actor_index, look_state) != 0) {
        *(short *)(actor + 0xa4) = (short)0xffff;
        *(char *)(actor + 0xa2) = 1;
      }
      if (*(char *)(actor + 0x160) == 0) {
        if (*(char *)(actor + 0xa2) == 0)
          goto flee_post;
        FUN_00014c10(actor_index, look_state, 1);
        if (*(short *)(actor + 0xa4) != -1)
          goto flee_post;
      } else {
        *(char *)(actor + 0xa2) = 0;
      }
      *(char *)(actor + 0xaa) = 1;
      *(int *)(actor + 0x398) = game_time_get();
    }
  }

flee_post:
  if (*(short *)(actor + 0xa8) > 8 && *(short *)(actor + 0xa8) < 0xd &&
      *(int *)(actor + 0x18) != -1 &&
      FUN_001a6bc0(*(int *)(actor + 0x18)) == 0) {
    *(char *)(actor + 0xac) = 0;
  }

  if (*(short *)(actor + 0xa8) < 1 || *(short *)(actor + 0xa4) == -1 ||
      *(char *)(actor + 0xaa) != 0 || *(int *)(actor + 0x18) == -1 ||
      (now = game_time_get(),
       *(char *)(actor + 0xac) != 0 && *(int *)(actor + 0xb0) + 0x3c < now))
    goto flee_skip;

  if (*(short *)(actor + 0xa8) == 0xc || *(short *)(actor + 0xa8) == 0xb) {
    FUN_001a74d0(*(int *)(actor + 0x18), 2);
  } else if (*(short *)(actor + 0xa8) == 9 || *(short *)(actor + 0xa8) == 10) {
    FUN_001a74d0(*(int *)(actor + 0x18), 1);
  } else {
    scream_unit = -1;
    if (*(int *)(actor + 0xb8) != -1) {
      prop = (char *)datum_get(prop_data, *(int *)(actor + 0xb8));
      scream_unit = *(int *)(prop + 0x18);
    }
    if (*(char *)(actor + 0xac) == 0) {
      scream_type = (*(short *)(actor + 0xa8) == 8) + 0x1f;
      FUN_00046f10(scream_type, *(int *)(actor + 0x18), scream_unit, -1, -1, 4,
                   0);
      *(char *)(actor + 0xac) = 1;
    } else {
      FUN_00046f10(0x21, *(int *)(actor + 0x18), scream_unit, -1, -1, -1, 0);
    }
  }
  *(int *)(actor + 0xb0) = now;

flee_skip:

  if (*(char *)(actor + 0x6) == 0 &&
      (*(char *)(actor + 0x4c) != 0 || *(char *)(actor + 0xa2) == 0) &&
      *(short *)(actor + 0x9e) < 1 && *(short *)(actor + 0xa4) == -1) {
    if (*(char *)(actor + 0xaa) != 0)
      return 1;
    if (*(char *)(actor + 0xab) == 0) {
      display_assert(
        "(!actor->meta.timeslice && state_data->find_new_flee_position) || "
        "(state_data->flee_stationary_ticks > 0) || "
        "(state_data->flee_firing_position_index != NONE) || "
        "state_data->unable_to_flee || state_data->done_fleeing",
        "c:\\halo\\SOURCE\\ai\\action_flee.c", 0x12e, 1);
      system_exit(-1);
    }
  }

  if (*(char *)(actor + 0xaa) == 0 && *(char *)(actor + 0xab) == 0)
    return 0;
  return 1;
}

/* FUN_00015880 (0x15880)
 * Initializes a guard state block (0x44 bytes) for the given actor.
 * Copies 3 floats from actor+0x174..0x17c into state_data+0x18..0x20,
 * sets state_data+0x24 = 1 (short), state_data+0x14 = 1 (byte),
 * state_data+0x3c = -1 (sentinel), and clears the rest to zero.
 *
 * Confirmed: datum_get(actor_data, actor_handle) call at 0x15890.
 * Confirmed: assert on state_data != NULL (line 0x72 in action_guard.c).
 * Confirmed: csmemset(state_data, 0, 0x44) at 0x158be.
 * Confirmed: all field offsets from raw disassembly MOV stores.
 */
int FUN_00015880(int actor_handle, char *state_data)
{
  char *actor;
  actor = (char *)datum_get(actor_data, actor_handle);
  if (state_data == NULL) {
    display_assert("state_data", "c:\\halo\\SOURCE\\ai\\action_guard.c", 0x72,
                   1);
    system_exit(-1);
  }
  csmemset(state_data, 0, 0x44);
  *(short *)(state_data + 0x24) = 1;
  *(char *)(state_data + 0x14) = 1;
  *(int *)(state_data + 0x18) = *(int *)(actor + 0x174);
  *(int *)(state_data + 0x1c) = *(int *)(actor + 0x178);
  *(int *)(state_data + 0x20) = *(int *)(actor + 0x17c);
  *(int *)(state_data + 0x3c) = -1;
  return 1;
}

/* FUN_00015900 (0x15900)
 * Initializes a guard state block (0x44 bytes) for the given actor,
 * conditionally populating fields based on actor flags and param_2.
 *
 * If actor+0x160 != 0 OR actor+6 != 0 (actor already has guard/prop state):
 *   sets state_data+0x24 = 1, state_data+0x3c = -1 and returns 1.
 * Otherwise writes param_2 to *state_data (short), then:
 *   - if param_2 == 0: sets state_data+0xe = 1, state_data+0x24 = 0, +0x3c =
 * -1.
 *   - if param_2 != 0: sets state_data+0x24 = 1, state_data+0x14 = 1, copies
 *     actor+0x174..0x17c into state_data+0x18..0x20, state_data+0x3c = -1.
 *
 * Confirmed: datum_get(actor_data, actor_handle) call at 0x15911.
 * Confirmed: assert on state_data != NULL (line 0x86 in action_guard.c).
 * Confirmed: csmemset(state_data, 0, 0x44) at 0x1594b.
 * Confirmed: all branch conditions and field offsets from raw disassembly.
 */
int FUN_00015900(int actor_handle, short param_2, char *state_data)
{
  char *actor;
  actor = (char *)datum_get(actor_data, actor_handle);
  if (state_data == NULL) {
    display_assert("state_data", "c:\\halo\\SOURCE\\ai\\action_guard.c", 0x86,
                   1);
    system_exit(-1);
  }
  csmemset(state_data, 0, 0x44);
  if ((*(char *)(actor + 0x160) == '\0') && (*(char *)(actor + 6) == '\0')) {
    *(short *)state_data = param_2;
    if (param_2 == 0) {
      *(char *)(state_data + 0xe) = 1;
      *(short *)(state_data + 0x24) = 0;
      *(int *)(state_data + 0x3c) = -1;
      return 1;
    }
    *(short *)(state_data + 0x24) = 1;
    *(char *)(state_data + 0x14) = 1;
    *(int *)(state_data + 0x18) = *(int *)(actor + 0x174);
    *(int *)(state_data + 0x1c) = *(int *)(actor + 0x178);
    *(int *)(state_data + 0x20) = *(int *)(actor + 0x17c);
    *(int *)(state_data + 0x3c) = -1;
    return 1;
  }
  *(short *)(state_data + 0x24) = 1;
  *(int *)(state_data + 0x3c) = -1;
  return 1;
}

/* FUN_000159d0 (0x159d0)
 * Initialize a guard action state block (0x44 bytes) for guard behavior.
 * Returns 1 on success, 0 on early-out (behavior already terminal).
 * Sets mode=0x78, flag=1, prop=-1 as defaults; populates prop data if
 * actor has a prop (actor+0x1e8 != -1) and is not suppressed/swarmed.
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x159dc.
 * Confirmed: assert on state_data != NULL (action_guard.c:0xac).
 * Confirmed: switch on *(short*)(actor+0x1e4)-6 for velocity scale. */
char FUN_000159d0(int actor_handle, short *state_data)
{
  char *actor;
  char *prop;
  int prop_handle;
  int behavior;

  actor = (char *)datum_get(actor_data, actor_handle);
  if (state_data == NULL) {
    display_assert("state_data", "c:\\halo\\SOURCE\\ai\\action_guard.c", 0xac,
                   1);
    system_exit(-1);
  }
  csmemset(state_data, 0, 0x44);
  *state_data = 0x78;
  state_data[0x12] = 1;
  *(char *)((char *)state_data + 5) = 1;
  *(int *)((char *)state_data + 0x3c) = -1;
  if (*(short *)(actor + 0x15e) == 4) {
    return 0;
  }
  if ((*(char *)(actor + 0x160) == '\0') && (*(char *)(actor + 6) == '\0') &&
      ((prop_handle = *(int *)(actor + 0x1e8)) != -1)) {
    prop = (char *)datum_get(prop_data, prop_handle);
    *(int *)((char *)state_data + 0x3c) = prop_handle;
    state_data[1] = 0x78;
    *(char *)((char *)state_data + 0x40) = 1;
    behavior = *(short *)(actor + 0x1e4) - 6;
    switch (behavior) {
    case 0:
      *(float *)((char *)state_data + 0x38) = 2.0f;
      break;
    case 1:
    case 2:
      *(float *)((char *)state_data + 0x38) = 1.0f;
      break;
    case 3:
      *(float *)((char *)state_data + 0x38) = 1.5f;
      break;
    default:
      return 1;
    }
    actor_perception_find_prop_pathfinding_location(actor_handle, prop_handle);
    state_data[0x12] = 2;
    *(int *)((char *)state_data + 0x28) = *(int *)(prop + 0xf0);
    *(int *)((char *)state_data + 0x2c) = *(int *)(prop + 0xf4);
    *(int *)((char *)state_data + 0x30) = *(int *)(prop + 0xf8);
    *(int *)((char *)state_data + 0x34) = *(int *)(prop + 0xec);
  }
  return 1;
}

/* FUN_00015b30 (0x15b30) */
void FUN_00015b30(int actor_handle)
{
  char *actor;
  actor = (char *)datum_get(actor_data, actor_handle);
  actor_perception_forget_recent_damage(actor_handle);
  *(char *)(actor + 0x98) = 0;
  if (*(char *)(actor + 0xa6) != '\0') {
    actor_perception_retreat_successful(actor_handle);
  }
}

/* actor_clear_guard_state (0x15b70)
 * Clears the actor's prop/guard encounter state when the prop-ready flag is
 * set.
 *
 * Confirmed: datum_get(actor_data, actor_handle) from decompile.
 * Confirmed: branch on *(char *)(actor+0xa1) != 0 from decompile.
 * Confirmed: *(int16_t *)(actor+0x1e4) = 0; *(int *)(actor+0x1e8) = -1 from
 * decompile. Inferred: actor+0xa1 = prop-ready flag (set by
 * actor_update_prop_desire at 0x14360). Inferred: actor+0x1e4 = guard/prop
 * state index (int16_t). Inferred: actor+0x1e8 = guard/prop encounter handle,
 * reset to invalid (-1). */
void actor_clear_guard_state(int actor_handle)
{
  char *actor;
  actor = (char *)datum_get(actor_data, actor_handle);
  if (*(char *)(actor + 0xa1) != '\0') {
    *(int16_t *)(actor + 0x1e4) = 0;
    *(int *)(actor + 0x1e8) = -1;
  }
}

/* FUN_00015cf0 (0x15cf0)
 * Per-tick guard-state look countdown: decrements guard state counters and
 * calls FUN_00015bb0 (actor handle in EAX) when the main counter expires.
 * Also handles fleet-vehicle leave and target-following logic.
 *
 * Confirmed: datum_get at 0x15cfd. MOV EAX,EDI / CALL FUN_00015bb0 at 0x15d4b.
 * Confirmed: CMP/DEC word [ESI+0x9c/0x9e] at 0x15d1d/0x15d7b.
 * Confirmed: FUN_0003ca40 at 0x15e0c. FUN_0003b380+FUN_00046f10 at 0x15e3a. */
void FUN_00015cf0(int actor_handle)
{
  char *actor;
  short decval;
  int uVar3;

  actor = (char *)datum_get(actor_data, actor_handle);
  if ((*(char *)(actor + 0x13) == '\0') && (*(char *)(actor + 0x484) != '\0') &&
      (*(short *)(actor + 0x9c) > 0)) {
    decval = *(short *)(actor + 0x9c) - 1;
    *(short *)(actor + 0x9c) = decval;
    if ((decval == 0) && (*(char *)(actor + 0x160) == '\0') &&
        (*(char *)(actor + 6) == '\0')) {
      if (*(char *)(actor + 0xa1) != '\0') {
        FUN_00015bb0(actor_handle);
        *(short *)(actor + 0x1e4) = 0;
        *(int *)(actor + 0x1e8) = -1;
        *(char *)(actor + 0xa1) = 0;
        *(char *)(actor + 0xa3) = 0;
        *(int *)(actor + 0xd8) = -1;
      }
      *(char *)(actor + 0xaa) = 1;
    }
  }
  if ((*(short *)(actor + 0x9e) > 0) && ((*(char *)(actor + 0xdc) == '\0') ||
                                         (*(char *)(actor + 0x484) != '\0'))) {
    decval = *(short *)(actor + 0x9e) - 1;
    *(short *)(actor + 0x9e) = decval;
    if (decval == 0) {
      *(int *)(actor + 0xd8) = -1;
    }
  }
  if ((*(char *)(actor + 0xa0) == '\0') || (*(char *)(actor + 0x484) == '\0')) {
    return;
  }
  if (*(char *)(actor + 0xa6) != '\0') {
    *(char *)(actor + 0xa6) = (char)(*(short *)(actor + 0x3a8) > 0);
    if (*(char *)(actor + 0xa6) != '\0') {
      return;
    }
    goto wake;
  }
  if (*(short *)(actor + 0xa8) < 1) {
    return;
  }
  decval = *(short *)(actor + 0xa8) - 1;
  *(short *)(actor + 0xa8) = decval;
  if (decval != 0) {
    return;
  }
wake:
  actor_set_dormant(actor_handle, 0);
  *(char *)(actor + 0xa4) = 0;
  *(char *)(actor + 0xa5) = 0;
  *(char *)(actor + 0xa6) = 0;
  *(short *)(actor + 0xa8) = 0;
  if (*(short *)(actor + 0x6e) >= 2 && *(int *)(actor + 0x18) != -1) {
    uVar3 = actor_target_unit_index(actor_handle);
    FUN_00046f10(0x23, *(int *)(actor + 0x18), uVar3, -1, -1, -1, 0);
  }
  FUN_00024be0(actor_handle, *(short *)(actor + 0xc4), 0);
  *(short *)(actor + 0x3b8) = -1;
  *(short *)(actor + 0xc4) = -1;
  if (*(char *)(actor + 0x160) != '\0') {
    *(short *)(actor + 0xc0) = 1;
    return;
  }
  *(short *)(actor + 0xc0) = 0;
  *(char *)(actor + 0xaa) = 1;
}

/* actor_reset_action_state (0x15eb0)
 * If actor is active (actor+0xa4 != 0) and in state 3, clears the prop flags
 * at a4, a6, a8. If state==3 or (state==1 and not suppressed at 0x160),
 * resets action state: c0=0 (idle), c4=0xffff, aa=1.
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x15ebe.
 * Confirmed: field offsets a4, a6, a8, aa, c0, c4, 160 from disassembly.
 * Inferred: state 3 = fleeing/post-action; state 1 = normal; 0x160 = suppressed
 * flag. */
void actor_reset_action_state(int actor_handle)
{
  char *actor;
  actor = (char *)datum_get(actor_data, actor_handle);
  if (*(char *)(actor + 0xa4) != '\0' && *(int16_t *)(actor + 0xc0) == 3) {
    *(char *)(actor + 0xa4) = 0;
    *(int16_t *)(actor + 0xa8) = 0;
    *(char *)(actor + 0xa6) = 0;
  }
  if (*(int16_t *)(actor + 0xc0) == 3 ||
      (*(int16_t *)(actor + 0xc0) == 1 && *(char *)(actor + 0x160) == '\0')) {
    *(int16_t *)(actor + 0xc0) = 0;
    *(int16_t *)(actor + 0xc4) = (int16_t)0xffff;
    *(char *)(actor + 0xaa) = 1;
  }
}

/* actor_clear_flee_target (0x15f30)
 * If the actor is in flee state (action state 2 at actor+0xc0), clears the
 * flee target handle at actor+0xd0 to -1 (invalid datum).
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x15f3e.
 * Confirmed: cmp word ptr [eax+0xc0], 2 and mov dword ptr [eax+0xd0], -1 from
 * disassembly. Inferred: actor+0xc0 = action state enum; actor+0xd0 =
 * flee-target datum handle. */
void actor_clear_flee_target(int actor_handle)
{
  char *actor;
  actor = (char *)datum_get(actor_data, actor_handle);
  if (*(int16_t *)(actor + 0xc0) == 2) {
    *(int *)(actor + 0xd0) = -1;
  }
}

/* FUN_00015f60 (0x15f60)
 * Copy 16-byte look-target vector from one of three global pointers based
 * on actor's look-suppress/phase flags (actor+0xa4, 0xa5, 0xa6).
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x15f6c.
 * Confirmed: PTR_DAT_002ee6f4=default, 002ee6e8=phase1, 002ee704=phase2. */
void FUN_00015f60(int actor_handle, int *param_2)
{
  int *src;
  char *actor;
  actor = (char *)datum_get(actor_data, actor_handle);
  src = *(int **)0x2ee700;
  if (*(char *)(actor + 0xa4) != '\0') {
    if (*(char *)(actor + 0xa6) != '\0') {
      src = *(int **)0x2ee704;
      param_2[0] = src[0];
      param_2[1] = src[1];
      param_2[2] = src[2];
      param_2[3] = src[3];
      return;
    }
    src = *(int **)0x2ee6f4;
    if (*(char *)(actor + 0xa5) != '\0') {
      src = *(int **)0x2ee6e8;
      param_2[0] = src[0];
      param_2[1] = src[1];
      param_2[2] = src[2];
      param_2[3] = src[3];
      return;
    }
  }
  param_2[0] = src[0];
  param_2[1] = src[1];
  param_2[2] = src[2];
  param_2[3] = src[3];
}

/* actor_replace_prop_handle (0x16000)
 * Replace all references to old_handle in actor prop fields with new_handle.
 *
 * Checks actor+0xd8 (prop field) and actor+0xac (scripted-look prop handle).
 * If new_handle is -1 (invalid datum), also clears the byte flag at actor+0xab.
 *
 * Confirmed: cdecl, three stack args at [EBP+0x8], [EBP+0xC], [EBP+0x10].
 * Confirmed: datum_get(actor_data=DAT_006325a4, actor_handle) at 0x1600e.
 * Confirmed: ADD EAX,0x9c at 0x1601c gives base; [EAX+0x3c]=actor+0xd8,
 *   [EAX+0x10]=actor+0xac, byte [EAX+0x0f]=actor+0xab.
 * Confirmed: CMP [EAX+0x3c],EDX / MOV [EAX+0x3c],ECX at 0x16024-0x1602b.
 * Confirmed: CMP [EAX+0x10],EDX / MOV [EAX+0x10],ECX / CMP ECX,-1 /
 *   MOV byte [EAX+0xf],0 at 0x1602e-0x1603b.
 * Inferred: actor+0xd8 = prop datum handle; actor+0xac = scripted-look prop
 * handle; actor+0xab = scripted-look prop valid flag. */
void actor_replace_prop_handle(int actor_handle, int old_handle, int new_handle)
{
  char *actor;
  actor = (char *)datum_get(actor_data, actor_handle);
  if (*(int *)(actor + 0xd8) == old_handle) {
    *(int *)(actor + 0xd8) = new_handle;
  }
  if (*(int *)(actor + 0xac) == old_handle) {
    *(int *)(actor + 0xac) = new_handle;
    if (new_handle == -1) {
      *(char *)(actor + 0xab) = 0;
    }
  }
}

/* FUN_00016050 (0x16050)
 * Initialize guard-mode action state.
 *
 * Populates a 0x44-byte state buffer: copies actor combat transition info,
 * checks for direct-fire position, and fills look/aim/target data from the
 * actor record. Returns 1 always.
 *
 * Confirmed: datum_get(actor_data, actor_handle) → actor.
 * Confirmed: csmemset(param_2, 0, 0x44).
 * Confirmed: assertion path "c:\halo\SOURCE\ai\action_guard.c". */
int FUN_00016050(int actor_handle, short *param_2)
{
  char *actor;
  int iVar3;
  char cVar1;

  actor = (char *)datum_get(actor_data, actor_handle);
  if (param_2 == (short *)0) {
    display_assert("state_data", "c:\\halo\\SOURCE\\ai\\action_guard.c", 0x20,
                   1);
    system_exit(-1);
  }
  csmemset(param_2, 0, 0x44);
  *param_2 = *(short *)(actor + 0x33c);
  if (*(char *)(actor + 0x160) == '\0' && *(char *)(actor + 6) == '\0') {
    if (*(short *)(actor + 0x312) == 0) {
      display_assert("actor->stimuli.combat_transition",
                     "c:\\halo\\SOURCE\\ai\\action_guard.c", 0x2d, 1);
      system_exit(-1);
    }
    if (*(char *)(actor + 0x314) != '\0') {
      cVar1 = FUN_00025a00(actor_handle, (int *)(actor + 0x318),
                           *(int *)(actor + 0x324), 1);
      if (cVar1 != '\0') {
        param_2[0x12] = 2;
        *(int *)(param_2 + 0x14) = *(int *)(actor + 0x318);
        *(int *)(param_2 + 0x16) = *(int *)(actor + 0x31c);
        *(int *)(param_2 + 0x18) = *(int *)(actor + 0x320);
        *(int *)(param_2 + 0x1a) = *(int *)(actor + 0x324);
        *(int *)(param_2 + 0x1c) = *(int *)(actor + 0x328);
        goto LAB_done;
      }
    }
    if (*param_2 < 1) {
      param_2[0x12] = 0;
      *(char *)(param_2 + 7) = 1;
    } else {
      param_2[0x12] = 1;
      cVar1 = *(char *)(actor + 0x32c);
      *(char *)(param_2 + 10) = cVar1;
      *((char *)param_2 + 0x15) = 0;
      if (cVar1 != '\0') {
        *(int *)(param_2 + 0xc) = *(int *)(actor + 0x330);
        *(int *)(param_2 + 0xe) = *(int *)(actor + 0x334);
        *(int *)(param_2 + 0x10) = *(int *)(actor + 0x338);
        if (normalize3d((float *)(param_2 + 0xc)) == *(float *)0x2533c0) {
          *(char *)(param_2 + 10) = 0;
        }
      }
    }
  } else {
    param_2[0x12] = 1;
  }
LAB_done:
  if (*(short *)(actor + 0x312) == 2) {
    if (*(int *)(actor + 0x340) == -1) {
      display_assert("actor->stimuli.combat_transition_prop_index != NONE",
                     "c:\\halo\\SOURCE\\ai\\action_guard.c", 0x5a, 1);
      system_exit(-1);
    }
    *((char *)param_2 + 0xf) = 1;
    *(int *)(param_2 + 8) = *(int *)(actor + 0x340);
  }
  iVar3 = *(int *)(actor + 0x340);
  *(int *)(param_2 + 0x1e) = iVar3;
  if (iVar3 != -1) {
    param_2[1] = *(short *)(actor + 0x344);
    *(char *)(param_2 + 0x20) = *(char *)(actor + 0x348);
  }
  return 1;
}

/* FUN_00016210 (0x16210)
 * Initialize guard-mode action state (post-combat variant).
 *
 * Similar to FUN_00016050 but adds actor-type tag lookup and random-range
 * timer for pre-combat delay. Handles combat-transition type 2 prop
 * assignment.
 *
 * Confirmed: datum_get(actor_data, actor_handle) → actor.
 * Confirmed: tag_get(0x61637472, actor+0x58) → actor tag.
 * Confirmed: assert path "c:\halo\SOURCE\ai\action_guard.c" at 0xed. */
char FUN_00016210(int actor_handle, int param_2, short *param_3)
{
  char *actor;
  char *tag;
  char cVar1;
  int lo;
  int hi;

  actor = (char *)datum_get(actor_data, actor_handle);
  tag = (char *)tag_get(0x61637472, *(int *)(actor + 0x58));
  if (param_3 == (short *)0) {
    display_assert("state_data", "c:\\halo\\SOURCE\\ai\\action_guard.c", 0xed,
                   1);
    system_exit(-1);
  }
  csmemset(param_3, 0, 0x44);
  if (*(char *)(actor + 0x160) == '\0' && *(char *)(actor + 6) == '\0') {
    *param_3 = 0;
    *(char *)(param_3 + 4) = 1;
    param_3[6] = 0;
    *(char *)((char *)param_3 + 9) = 0 < *(short *)(param_2 + 0xc);
    if (*(short *)(actor + 0x3a8) <= 0 ||
        *(char *)((char *)param_3 + 9) != '\0') {
      cVar1 = '\0';
    } else {
      cVar1 = '\x01';
    }
    *(char *)(param_3 + 5) = cVar1;
    if (cVar1 == '\0') {
      if (*(char *)((char *)param_3 + 9) == '\0') {
        lo = *(int *)(tag + 0x2d0);
        hi = *(int *)(tag + 0x2d4);
      } else {
        lo = *(int *)(tag + 0x298);
        hi = *(int *)(tag + 0x29c);
      }
      param_3[6] = (short)(int)random_real_range(
        get_global_random_seed_address(), *(float *)&lo, *(float *)&hi);
    }
    if (*(short *)(param_2 + 8) == -1) {
      *(char *)(param_3 + 7) = 1;
      param_3[0x12] = 0;
      *(int *)(param_3 + 0x1e) = -1;
      return 1;
    }
    *(char *)(param_3 + 7) = 0;
    param_3[0x12] = 3;
    param_3[0x14] = *(short *)(param_2 + 8);
    if (*(char *)(param_2 + 0x20) != '\0') {
      *(char *)(param_3 + 10) = 1;
      *((char *)param_3 + 0x15) = 1;
      *(float *)(param_3 + 0xc) =
        *(float *)(param_2 + 0x24) - *(float *)(actor + 0x12c);
      *(float *)(param_3 + 0xe) =
        *(float *)(param_2 + 0x28) - *(float *)(actor + 0x130);
      *(float *)(param_3 + 0x10) =
        *(float *)(param_2 + 0x2c) - *(float *)(actor + 0x134);
      if (normalize3d((float *)(param_3 + 0xc)) == *(float *)0x2533c0) {
        *(char *)(param_3 + 10) = 0;
        *(int *)(param_3 + 0x1e) = -1;
        return 1;
      }
    }
  } else {
    param_3[0x12] = 1;
  }
  *(int *)(param_3 + 0x1e) = -1;
  return 1;
}

/* FUN_000163d0 (0x163d0) — Firing-position combat state evaluator.
 *
 * Evaluates the actor's firing-position combat state and advances the
 * state machine. Three early-exit conditions (swarm, field_160, reset).
 * Main path: if field_4c is active and field_aa is flagged, runs
 * the firing-position evaluation pipeline (FUN_00024be0, FUN_00024a60,
 * FUN_00025c10, FUN_000272d0), then selects a randomized delay timer
 * from the actor tag's min/max range, scaled by 30.0 Hz.
 *
 * Confirmed: cdecl, single stack arg (actor_handle).
 * Confirmed: _chkstk with 0x14748 (83784 bytes) at 0x163d3/0x163d8.
 * Confirmed: datum_get(actor_data=DAT_006325a4, actor_handle) at
 * 0x163e4/0x163e9. Confirmed: tag_get('actr', actor->field_58) at
 * 0x163f0/0x163f9. Confirmed: three early-return paths for
 * swarm/field_160/reset at 0x16407/0x16422/0x16443. Confirmed: main evaluation
 * pipeline: FUN_00024be0 at 0x1649b, csmemset(0x670) at 0x164b1, FUN_00024a60
 * at 0x164c4, FUN_00025c10 at 0x164f1, FUN_000272d0 at 0x1650b. Confirmed:
 * random timer: FUN_0010b270(seed, tag_min, tag_max) at 0x16563, FMUL
 * [0x253394]=30.0f at 0x16568, _ftol2 at 0x16571, store to actor->field_9c at
 * 0x16576. Inferred: actor+0xc0 = firing-position action state (int16_t).
 *   actor+0xc4 = firing-position target (int16_t).
 *   actor+0x9c = combat timer (int16_t). */
unsigned int FUN_000163d0(int actor_handle)
{
  char *actor;
  char *tag;
#if defined(_MSC_VER) && !defined(__clang__)
  char large_buf[0x670];
  char huge_buf[0x1474c];
#else
  static char large_buf[0x670];
  static char huge_buf[0x1474c];
#endif
  short result;
  int seed_ret;
  float timer;

  actor = (char *)datum_get(actor_data, actor_handle);
  tag = (char *)tag_get(0x61637472, *(int *)(actor + 0x58));

  if (*(char *)(actor + 0x6) != '\0') {
    *(short *)(actor + 0xc0) = 1;
    return 0;
  }

  if (*(char *)(actor + 0x160) != '\0') {
    *(short *)(actor + 0xc0) = 1;
    *(char *)(actor + 0xaa) = 1;
    return 0;
  }

  if (*(short *)(actor + 0xc0) == 3 && *(short *)(actor + 0x3b8) == -1) {
    *(short *)(actor + 0xc0) = 0;
    *(char *)(actor + 0xaa) = 1;
  }

  if (*(char *)(actor + 0x4c) != '\0' && *(char *)(actor + 0xaa) != '\0') {
    short saved_3b8;
    int ret_24a60;
    int local_10;
    int local_c;
    int local_50[12]; /* FUN_00025c10 writes 12 ints (48 bytes) */
    int ret_25c10;

    if (*(short *)(actor + 0xc0) == 3 &&
        (saved_3b8 = *(short *)(actor + 0x3b8)) != -1) {
      FUN_00024be0(actor_handle, (int)saved_3b8, 0);
    }

    csmemset(large_buf, 0, 0x670);
    *(short *)(large_buf + 4) = 4;
    ret_24a60 = actor_get_firing_position_group(actor_handle, 4, 0);
    *(int *)large_buf = ret_24a60;
    large_buf[0x19] = 1;

    ret_25c10 = (int)FUN_00025c10(actor_handle, large_buf, local_50, &local_c,
                                  huge_buf, &local_10);
    result = FUN_000272d0(actor_handle, (short)ret_25c10, (short)ret_25c10,
                          local_c, (unsigned int)(int)huge_buf, (char)local_10);

    *(char *)(actor + 0xaa) = 0;
    *(char *)(actor + 0xb0) = 0;

    if (result == -1) {
      *(short *)(actor + 0xc0) = 1;
    } else {
      *(short *)(actor + 0xc0) = 3;
      *(short *)(actor + 0xc4) = result;
    }

    seed_ret = (int)get_global_random_seed_address();
    timer = random_real_range((int *)seed_ret, *(float *)(tag + 0x3b8),
                              *(float *)(tag + 0x3bc));
    return (unsigned int)((int)(timer * *(float *)0x253394) & 0xffffff00);
  }

  return 0;
}

/* FUN_00016590 (0x16590)
 * Update guard-mode look flags and dispatch navigation target based on
 * current action state (actor+0xc0).
 *
 * Sets look-flag bytes actor+0x426/427/428/424/425 from actr tag bits and
 * actor stance (a4/a6). Dispatches:
 *   0/1 → retreat (FUN_0002f1a0)
 *   2   → approach position (FUN_0002d720 or retreat)
 *   3   → move to firing position (actor_move_to_firing_position)
 * On approach success: fires FUN_00015bb0 if prop-ready (actor+0xa1) and
 * guard state (a3) not set. Then sets nav output (3e8/3ec/3f0/3fc).
 *
 * Confirmed: datum_get at 0x165a0. tag_get(actr) at 0x165b0.
 * Confirmed: CALL FUN_00015bb0 at 0x167f7 with MOV EAX,EDI beforehand.
 * Confirmed: SETGE+LEA for 3fc at 0x16934-0x16942. */
void FUN_00016590(int actor_handle)
{
  char *actor;
  char *tag_data;
  char *prop;
  float *fwd;
  char bVar2;
  int prop_unit;
  float fsq;
  float thresh;
  short sVar1;

  actor = (char *)datum_get(actor_data, actor_handle);
  tag_data = (char *)tag_get(0x61637472, *(int *)(actor + 0x58));
  if ((*tag_data & 0x40) != 0 && *(short *)(actor + 0x6e) == 0) {
    *(char *)(actor + 0x426) = 1;
    *(char *)(actor + 0x427) = 1;
  } else {
    *(char *)(actor + 0x427) = 0;
    if (*(char *)(actor + 0xa4) == '\0') {
      if ((*tag_data & 0x80) != 0 && *(short *)(actor + 0x6e) > 0) {
        *(char *)(actor + 0x426) = 1;
      } else {
        *(char *)(actor + 0x426) = 0;
      }
    } else if (*(char *)(actor + 0xa6) == '\0') {
      *(char *)(actor + 0x426) = 1;
    } else {
      *(char *)(actor + 0x426) = (char)((*tag_data >> 0x17) & 1);
    }
  }
  *(char *)(actor + 0x428) = 0;
  *(char *)(actor + 0x424) = 0;
  *(char *)(actor + 0x425) = 0;
  if (*(char *)(actor + 0x4c) == '\0' || *(char *)(actor + 6) != '\0') {
    goto output;
  }
  bVar2 = 0;
  switch (*(short *)(actor + 0xc0)) {
  case 0:
  case 1:
    FUN_0002f1a0(actor_handle);
    bVar2 = 1;
    break;
  case 2:
    fsq = distance_squared3d((float *)(actor + 0x12c), (float *)(actor + 0xc4));
    thresh = *(float *)(actor + 0xd4);
    if (thresh * thresh <= fsq) {
      actor_move_to_point(actor_handle, (float *)(actor + 0xc4),
                          *(int *)(actor + 0xd0), -1);
    } else {
      FUN_0002f1a0(actor_handle);
    }
    if (fsq >= *(float *)0x2536cc) {
      bVar2 = 0;
      break;
    }
    bVar2 = 1;
    break;
  case 3:
    sVar1 = *(short *)(actor + 0xc4);
    if (sVar1 != -1) {
      *(short *)(actor + 0x3b8) = sVar1;
      *(char *)(actor + 0x3ba) = 0;
      if (actor_move_to_firing_position(actor_handle, sVar1, 0) == '\0') {
        FUN_00024be0(actor_handle, *(short *)(actor + 0xc4), 0);
        *(short *)(actor + 0x3b8) = -1;
      }
    }
    if (*(char *)(actor + 0x4a8) == '\0' ||
        distance_squared3d((float *)(actor + 0x4ac), (float *)(actor + 0x12c)) <
          *(float *)0x2536cc) {
      bVar2 = 1;
    } else {
      bVar2 = 0;
    }
    break;
  default:
    display_assert(0, "c:\\halo\\SOURCE\\ai\\action_guard.c", 0x2a9, 1);
    system_exit(-1);
    break;
  }
  *(char *)(actor + 0xa0) = 1;
  if (bVar2) {
    if (*(char *)(actor + 0xab) != '\0') {
      prop = (char *)datum_get(prop_data, *(int *)(actor + 0xac));
      *(char *)(actor + 0xab) = 0;
      *(int *)(actor + 0xac) = -1;
      FUN_000369c0(actor_handle, 2, 600);
      FUN_00046f10(7, *(int *)(actor + 0x18), *(int *)(prop + 0x18), -1, -1, 2,
                   0);
    }
    if (*(char *)(actor + 0xa1) != '\0') {
      FUN_00015bb0(actor_handle);
      if (*(short *)(actor + 0x1e4) == 9 && *(short *)(actor + 0xc0) == 2) {
        *(char *)(actor + 0xa3) = 1;
      }
    }
  }
output:
  if (*(char *)(actor + 0xa3) != '\0') {
    *(short *)(actor + 0x3e8) = 7;
    *(short *)(actor + 0x3ec) = 2;
    *(char *)(actor + 0x454) = 1;
    *(char *)(actor + 0x45d) = 1;
    fwd = *(float **)0x31fc44;
    *(float *)(actor + 0x468) =
      fwd[2] * *(float *)0x2533e8 + *(float *)(actor + 0xcc);
    *(float *)(actor + 0x464) =
      fwd[1] * *(float *)0x2533e8 + *(float *)(actor + 0xc8);
    *(float *)(actor + 0x460) =
      fwd[0] * *(float *)0x2533e8 + *(float *)(actor + 0xc4);
  } else {
    prop_unit = *(int *)(actor + 0xd8);
    if (prop_unit != -1) {
      *(short *)(actor + 0x3e8) = 5;
      *(short *)(actor + 0x3ec) = 1;
      *(int *)(actor + 0x3f0) = prop_unit;
    } else if (*(char *)(actor + 0xb0) != '\0') {
      *(short *)(actor + 0x3ec) = 4;
      *(short *)(actor + 0x3e8) =
        (short)(3 + (*(char *)(actor + 0xb1) != '\0') * 2);
      *(int *)(actor + 0x3f0) = *(int *)(actor + 0xb4);
      *(int *)(actor + 0x3f4) = *(int *)(actor + 0xb8);
      *(int *)(actor + 0x3f8) = *(int *)(actor + 0xbc);
    } else if (*(short *)(actor + 0x6e) > 0 && *(int *)(actor + 0x270) != -1) {
      *(short *)(actor + 0x3e8) = 3;
      *(short *)(actor + 0x3ec) = 1;
      *(int *)(actor + 0x3f0) = *(int *)(actor + 0x270);
    } else {
      *(short *)(actor + 0x3e8) = 0;
    }
  }
  *(short *)(actor + 0x3fc) = (short)(2 + (*(short *)(actor + 0x6e) >= 4) * 2);
}

/* FUN_00016960 (0x16960)
 * Three-way float comparator: -1 if *a < *b, 1 if *a > *b, 0 if equal.
 *
 * Confirmed: FLD [ECX] / FCOMP [EDX] for both comparisons (param_1=ECX,
 * param_2=EDX). First: TEST AH,0x5; JP (jump if not-less). Second: TEST
 * AH,0x41; JNZ (jump if equal-or-less → return 0). Fall-through → return 1.
 * Confirmed: MOV EAX,0xffffffff / MOV EAX,0x1 / XOR EAX,EAX returns. */
int FUN_00016960(float *param_1, float *param_2)
{
  if (*param_1 < *param_2) {
    return -1;
  }
  if (*param_1 > *param_2) {
    return 1;
  }
  return 0;
}

/* FUN_000169a0 (0x169a0) — Command-list step "stop/cleanup" dispatcher.
 *
 * Given a command-list execution-state struct (passed in ESI), looks up the
 * current command element in the scenario's command_list tag block
 * (global_scenario + 0x438) and dispatches on the element's opcode word
 * (element[0]) to perform the appropriate stop/cleanup action for the
 * command type. Bounds-checks state->cmd_index against the element count
 * before fetching the element.
 *
 * Register arg: state @<esi> — pointer to the per-command-list run state.
 *   state[0]      = current command index (byte)
 *   state[1]      = loop counter (byte)
 *   state[4]      = flag byte (bits manipulated by case 0x14)
 *   state[5]      = flag byte (bits cleared by cases 3/0x16 and 0xa/0xb)
 *   word[state+8] = sub-state word (set to 0xffff / 0 by those cases)
 *
 * Confirmed (disasm 0x169a0-0x16b88):
 *  - datum_get(actor_data, actor_handle) -> actor (iVar2), uses actor+0x18.
 *  - cmd_list = tag_block_get_element(global_scenario_get()+0x438, cmd_type,
 * 0x60).
 *  - if state[0] >= cmd_list[0x30] (element count): return.
 *  - element = tag_block_get_element(cmd_list+0x30, state[0], 0x20).
 *  - switch (element[0]) — opcode word — via jump table at 0x16b8c.
 *  - ESI is passed in register by both callers (FUN_00016cf0, FUN_00019110),
 *    each loaded from their own [EBP+0x14] (param_4 = state ptr).
 *  - case 0xd uses object_try_and_get_and_verify_type at 0x13d640
 *    (NOT the caller's 0x13d680). */
void FUN_000169a0(int actor_handle, int object_handle, short cmd_type,
                  int out_state, char *out_index, unsigned char *state)
{
  char buf[512];
  int actor;
  int cmd_list;
  unsigned short *element;
  unsigned char b;

  actor = (int)datum_get(actor_data, actor_handle);
  cmd_list = (int)tag_block_get_element((char *)global_scenario_get() + 0x438,
                                        (int)cmd_type, 0x60);
  if ((int)(unsigned int)*state < *(int *)(cmd_list + 0x30)) {
    element = (unsigned short *)tag_block_get_element(
      (void *)(cmd_list + 0x30), (unsigned int)*state, 0x20);
    switch (*element) {
    case 1:
    case 2:
      if (object_handle == *(int *)(actor + 0x18)) {
        FUN_0002f1a0(actor_handle);
      }
      if (out_state != 0) {
        *(unsigned char *)(out_state + 4) = 0;
        *(unsigned char *)(out_state + 0x18) = 0;
        return;
      }
      break;
    case 3:
    case 0x16:
      state[5] = state[5] & 0xfe;
      *(unsigned short *)(state + 8) = 0xffff;
      return;
    case 7:
      if (out_state != 0) {
        *(unsigned char *)(out_state + 0x36) = 0;
        return;
      }
      break;
    case 10:
    case 0xb:
      state[5] = state[5] & 0xfb;
      *(unsigned short *)(state + 8) = 0;
      return;
    case 0x14:
      if (element[1] == 1) {
        b = state[4];
        state[4] = b & 0xf7;
        if ((~(b >> 3) & 1) == 0) {
          state[4] = b & 0xe7;
          return;
        }
        state[4] = (b & 0xf7) | 0x10;
      }
      if (element[0xb] == (unsigned short)*state) {
        ai_debug_describe_actor(actor_handle, -1, 1, buf, 0x200);
        error(2, "%s: command list %s entry #%d tried to loop to itself", buf,
              cmd_list, *state);
        return;
      }
      if (9 < state[1]) {
        ai_debug_describe_actor(actor_handle, -1, 1, buf, 0x200);
        error(2, "%s: command list %s is stuck looping (aborting on loop #%d)",
              buf, cmd_list, *state);
        return;
      }
      *out_index = *(char *)((char *)element + 0x16);
      state[1] = state[1] + 1;
      return;
    case 0xd:
      actor = (int)object_try_and_get_and_verify_type(object_handle, 1);
      if (actor != 0) {
        *(unsigned int *)(actor + 0x424) =
          *(unsigned int *)(actor + 0x424) & 0xfffffff3;
        return;
      }
      break;
    case 4:
    case 0x17:
    case 0x18:
    case 0x19:
      if (object_handle == *(int *)(actor + 0x18)) {
        FUN_00027870(actor_handle);
      }
      break;
    }
  }
  return;
}

/* FUN_00016bd0 / actor_look_secondary_stop (0x16bd0)
 * Command-list initialization callback for prop interest dispatch.
 *
 * Clears 0x24 bytes of state_data, sets first byte to 0xff. Asserts
 * targeting_reference is non-null. Sets/clears bit 0 of state_data[4]
 * based on targeting_reference[0]. Optionally initializes targeting_info
 * buffer (0x58 bytes, sets +2 to 0xffff).
 *
 * Confirmed: csmemset(param_4, 0, 0x24); MOV byte [ESI],0xff.
 * Confirmed: assert "targeting_reference" at line 0x550 of action_obey.c. */
void actor_look_secondary_stop(int param_1, int param_2, int param_3,
                               char *param_4, int param_5, char *param_6)
{
  csmemset(param_4, 0, 0x24);
  *param_4 = (char)0xff;
  if (param_6 == (char *)0) {
    display_assert("targeting_reference", "c:\\halo\\SOURCE\\ai\\action_obey.c",
                   0x550, 1);
    system_exit(-1);
  }
  if (*param_6 != '\0') {
    param_4[4] = param_4[4] | 1;
  } else {
    param_4[4] = param_4[4] & (char)0xfe;
  }
  if (param_5 != 0) {
    csmemset((char *)param_5, 0, 0x58);
    *(short *)(param_5 + 2) = (short)0xffff;
  }
}

/* FUN_00016c40 (0x16c40) */
void FUN_00016c40(int param_1, int param_2, short param_3, char *param_4)
{
  int iVar1;
  iVar1 = (int)tag_block_get_element((char *)global_scenario_get() + 0x438,
                                     (int)param_3, 0x60);
  if ((int)(unsigned char)*param_4 >= *(int *)(iVar1 + 0x30)) {
    *param_4 = (char)0xff;
  }
}

/* FUN_00016c80 (0x16c80) — Scenario encounter guard-zone boundary callback.
 * Checks if the encounter element at param_3 has the 0x10 flag set at +0x20.
 * If so, sets bit 0x1000 in the object's flags at +0x1b4. */
void FUN_00016c80(int param_1, int param_2, short param_3)
{
  char *elem;
  char *obj;
  elem = (char *)tag_block_get_element((char *)global_scenario_get() + 0x438,
                                       (int)param_3, 0x60);
  if ((*(unsigned char *)(elem + 0x20) & 0x10) != 0) {
    obj = (char *)object_get_and_verify_type(param_2, 3);
    *(unsigned int *)(obj + 0x1b4) = *(unsigned int *)(obj + 0x1b4) | 0x1000;
  }
}

/* FUN_00016cd0 (0x16cd0) — Prop-interest reset callback.
 * Sets bit 3 (0x08) and clears bit 4 (0x10) in byte at param_4+4.
 * Called by actor_look_compute_prop_interest as the reset callback.
 * Dispatcher passes: (actor_handle, object_handle, index, state_data_ptr, ...)
 */
void FUN_00016cd0(int param_1, int param_2, int param_3, char *param_4)
{
  param_4[4] = (param_4[4] & (char)0xef) | 8;
}

/* FUN_00016cf0 (0x16cf0)
 * Execute command-list step: get unit, optionally dispatch FUN_000169a0,
 * then clear bit 12 (0x1000) from unit+0x1b4.
 *
 * Confirmed: object_get_and_verify_type(param_2, 3) → unit.
 * Confirmed: TEST AL,0x2 on param_4[4]; if clear, call FUN_000169a0.
 * Confirmed: LEA EAX,[EBP+0x17] → address of high byte of param_4 on stack.
 * Confirmed: AND [EDI+0x1b4],0xffffefff at end. */
void FUN_00016cf0(int param_1, int param_2, short param_3, int param_4,
                  int param_5)
{
  char *unit;

  unit = (char *)object_get_and_verify_type(param_2, 3);
  if ((*(unsigned char *)(param_4 + 4) & 2) == 0) {
    FUN_000169a0(param_1, param_2, param_3, param_5,
                 (char *)((int)&param_4 + 3), (unsigned char *)param_4);
  }
  *(int *)(unit + 0x1b4) = *(int *)(unit + 0x1b4) & 0xffffefff;
}

/* actor_look_compute_prop_interest (0x16d40)
 * Iterate an actor's prop list (or each swarm unit's list for swarm actors)
 * and invoke the callback for each active entry.
 *
 * For swarm actors (actor+6 != 0): iterates swarm units via swarm_data and
 * swarm_component_data. For each unit with the "active" bit set, calls
 * callback(actor_handle, unit_handle, *prop_state, component+0x1c, 0, param_5).
 * For non-swarm: calls callback once with the actor's own unit handle.
 * If param_2 != 0: resets each component's state (csmemset + set bit 3).
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x16d4e.
 * Confirmed: CALL 0x16d40 from FUN_00017090 at 0x170b2.
 * Confirmed: swarm_data=DAT_006325a0, swarm_component_data=DAT_0063259c. */
void actor_look_compute_prop_interest(int actor_handle, int param_2,
                                      short *param_3, void (*callback)(void),
                                      int param_5)
{
  char *actor;
  char *swarm;
  char *component;
  short i;

  actor = (char *)datum_get(actor_data, actor_handle);
  if (*(char *)(actor + 6) != '\0') {
    if (*(int *)(actor + 0x28) == -1) {
      display_assert(
        "!actor->meta.swarm || (actor->meta.swarm_cache_index != NONE)",
        "c:\\halo\\SOURCE\\ai\\action_obey.c", 0x611, 1);
      system_exit(-1);
    }
    swarm = (char *)datum_get(swarm_data, *(int *)(actor + 0x28));
    if (*(short *)(swarm + 2) < 1) {
      return;
    }
    for (i = 0; i < *(short *)(swarm + 2); i++) {
      component =
        (char *)datum_get(swarm_component_data, *(int *)(swarm + 0x58 + i * 4));
      if (param_2 != 0) {
        csmemset(component + 0x1c, 0, 0x24);
        *(unsigned short *)(component + 2) =
          (*(unsigned short *)(component + 2) & ~4u) | 8;
      }
      if ((*(unsigned char *)(component + 2) & 8) != 0) {
        ((void (*)(int, int, int, char *, int, int))callback)(
          actor_handle, *(int *)(swarm + 0x18 + i * 4), (int)*param_3,
          component + 0x1c, 0, param_5);
      }
    }
    return;
  }
  ((void (*)(int, int, int, short *, short *, int))callback)(
    actor_handle, *(int *)(actor + 0x18), (int)*param_3, param_3 + 4,
    param_3 + 0x16, param_5);
}

/* FUN_00016e70 (0x16e70)
 * Initialize a command-list execution state for an actor.
 *
 * Validates the command-list index against the scenario encounter table,
 * checks BSP compatibility, extracts command flags, optionally stops
 * scripted look, and dispatches initialization via
 * actor_look_compute_prop_interest with the FUN_00016bd0 callback.
 *
 * Confirmed: datum_get(actor_data, actor_handle) → actor.
 * Confirmed: global_scenario_get, tag_block_get_element(scenario+0x438,
 * cmd_idx, 0x60). Confirmed: command flags extracted from iVar3[0x20] (bits
 * 0-3). */
int FUN_00016e70(int actor_handle, short param_2, char *param_3)
{
  char *actor;
  char *scenario;
  char *cmd_entry;
  unsigned int flags;
  char bit0;
  char bit1;
  char stop_look;
  char bit3;
  char local_10c[256];

  actor = (char *)datum_get(actor_data, actor_handle);
  scenario = (char *)global_scenario_get();
  if (param_3 == (char *)0) {
    display_assert("state_data", "c:\\halo\\SOURCE\\ai\\action_obey.c", 0x63d,
                   1);
    system_exit(-1);
  }
  csmemset(param_3, 0, 0x84);
  if (param_2 >= 0) {
    if ((int)param_2 < *(int *)(scenario + 0x438)) {
      cmd_entry =
        (char *)tag_block_get_element(scenario + 0x438, (int)param_2, 0x60);
      if (*(char *)(actor + 6) == '\0' || *(int *)(actor + 0x28) != -1) {
        if (*(short *)(cmd_entry + 0x2e) != -1 &&
            *(short *)(cmd_entry + 0x2e) != *(short *)0x326a0c) {
          error(2, "wrong structure bsp, cannot execute command list %s",
                (int)cmd_entry);
          return 0;
        }
        *(short *)param_3 = param_2;
        flags = *(unsigned int *)(cmd_entry + 0x20);
        bit0 = *(char *)(cmd_entry + 0x20) & 1;
        bit1 = (char)((flags >> 1) & 1);
        stop_look = ~(char)(flags >> 2) & 1;
        bit3 = ~(char)(flags >> 3) & 1;
        if (stop_look == 0) {
          FUN_00027870(actor_handle);
        }
        *(char *)(param_3 + 2) = bit0;
        param_3[3] = stop_look;
        *(char *)(param_3 + 4) = bit3;
        actor_look_compute_prop_interest(
          actor_handle, 1, (short *)param_3,
          (void (*)(void))actor_look_secondary_stop, (int)&bit1);
        return 1;
      }
      if (*(char *)(actor + 8) == '\0') {
        *(short *)(actor + 0x90) = param_2;
        return 0;
      }
      ai_debug_describe_actor(actor_handle, -1, 1, local_10c, 0x100);
      error(
        2,
        "swarm actor %s cannot execute command list, ran out of swarm caches",
        (int)local_10c);
    }
  }
  return 0;
}

/* FUN_00016ff0 (0x16ff0)
 * Update actor scripted-look prop interest, or invalidate if out of range.
 *
 * Fetches actor record; prop_state = (short*)(actor+0x9c).  If the prop
 * index (*prop_state) is valid (>= 0) and within the scenario prop count
 * (*(int*)(scenario+0x438)), delegates to actor_look_compute_prop_interest
 * with reset=0, callback=FUN_00016c40, param_5=0.  Otherwise marks the
 * prop as invalid: *prop_state = -1, actor+0xa1 = 1, then calls
 * actor_action_change(actor_handle, 0, 0).
 *
 * Confirmed: cdecl, single stack arg (actor_handle).
 * Confirmed: datum_get(actor_data, actor_handle); ADD ESI,0x9c.
 * Confirmed: MOV CX,[ESI]; TEST CX,CX; JL else; MOV EDX,[EAX+0x438].
 * Confirmed: MOV word [ESI],0xffff; MOV byte [ESI+0x5],0x1 in else-branch.
 * Confirmed: CALL actor_action_change(actor_handle, 0, 0) in else-branch.
 * Inferred: actor+0x9c = scripted-look prop state (short index);
 *   actor+0xa1 = scripted-look valid flag; scenario+0x438 = prop count. */
void FUN_00016ff0(int actor_handle)
{
  int scenario;
  char *actor;
  short *prop_state;
  actor = (char *)datum_get(actor_data, actor_handle);
  prop_state = (short *)(actor + 0x9c);
  scenario = (int)global_scenario_get();
  if ((*prop_state >= 0) && ((int)*prop_state < *(int *)(scenario + 0x438))) {
    actor_look_compute_prop_interest(actor_handle, 0, prop_state,
                                     (void (*)(void))FUN_00016c40, 0);
    return;
  }
  *prop_state = -1;
  *(char *)(actor + 0xa1) = 1;
  actor_action_change(actor_handle, 0, 0);
}

/* actor_clear_aim_target (0x17060)
 * If the actor's aiming-active flag (actor+0xcc) is set, resets the aim
 * target handle (actor+0xdc) to the -1 sentinel.
 *
 * Confirmed: datum_get(DAT_006325a4, param_1) from decompile.
 * Confirmed: actor+0xcc flag check (char), actor+0xdc reset to 0xffffffff. */
void actor_clear_aim_target(int actor_handle)
{
  char *actor;
  actor = (char *)datum_get(actor_data, actor_handle);
  if (*(char *)(actor + 0xcc) != '\0') {
    *(int *)(actor + 0xdc) = -1;
  }
}

/* FUN_00017090 (0x17090)
 * Compute actor prop-interest for the prop list at actor+0x9c.
 *
 * Fetches the actor record, then calls actor_look_compute_prop_interest with
 * reset=0, prop_state=actor+0x9c, callback=FUN_00016cd0, param_5=0.
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x1709e.
 * Confirmed: PUSH 0x0; PUSH 0x16cd0; ADD EAX,0x9c; PUSH EAX; PUSH 0x0;
 *   PUSH ESI; CALL 0x16d40; ADD ESP,0x1c at 0x170a3-0x170b8.
 * Inferred: actor+0x9c = scripted-look prop state array. */
void FUN_00017090(int actor_handle)
{
  char *actor;
  actor = (char *)datum_get(actor_data, actor_handle);
  actor_look_compute_prop_interest(actor_handle, 0, (short *)(actor + 0x9c),
                                   (void (*)(void))FUN_00016cd0, 0);
}

/* FUN_000170c0 (0x170c0)
 * Compute actor prop-interest for the prop list at actor+0x9c using the
 * guard-zone boundary callback (FUN_00016c80).
 *
 * Same pattern as FUN_00017090 but selects the guard-specific callback.
 *
 * Confirmed: datum_get(actor_data, actor_handle);
 * actor_look_compute_prop_interest with callback=FUN_00016c80, reset=0,
 * prop_state=actor+0x9c, param_5=0. */
void FUN_000170c0(int actor_handle)
{
  char *actor;
  actor = (char *)datum_get(actor_data, actor_handle);
  actor_look_compute_prop_interest(actor_handle, 0, (short *)(actor + 0x9c),
                                   (void (*)(void))FUN_00016c80, 0);
}

/* FUN_000170f0 (0x170f0)
 * Compute actor prop-interest for the prop list at actor+0x9c using the
 * danger-zone update callback (FUN_00016cf0).
 *
 * Same pattern as FUN_00017090 but selects the danger-update callback.
 *
 * Confirmed: datum_get(actor_data, actor_handle);
 * actor_look_compute_prop_interest with callback=FUN_00016cf0, reset=0,
 * prop_state=actor+0x9c, param_5=0. */
void FUN_000170f0(int actor_handle)
{
  char *actor;
  actor = (char *)datum_get(actor_data, actor_handle);
  actor_look_compute_prop_interest(actor_handle, 0, (short *)(actor + 0x9c),
                                   (void (*)(void))FUN_00016cf0, 0);
}

/* FUN_00017120 (0x17120)
 * Debug-print formatter for command-point action elements.
 * Converts a command-point element (short[16]) into a human-readable
 * description string via snprintf.  Called from the obey-action debug
 * path and from the command-point debug dump.
 *
 * Each case formats the element fields into string tables (inlined
 * on the stack) and produces a single snprintf call with the
 * appropriate format string and parameters.
 *
 * Confirmed: 4 cdecl stack args — scenario_data, cmd_element, out_buf,
 *   out_size.  Jump table at 0x17838 covering 0x1c cases.
 * Confirmed: snprintf family for string building; tag_block_get_element
 *   for name lookups from scenario tag blocks.
 * Confirmed: 320 bytes of locals (SUB ESP,0x140). */
void FUN_00017120(void *scenario_data, short *cmd, char *out_buf, int out_size)
{
  int cmd_type;
  int sub_type;

  cmd_type = cmd[0];
  sub_type = cmd[1];

  if (cmd_type > 0x1b) {
    snprintf(out_buf, out_size, "<unknown %d>", cmd_type);
    return;
  }

  switch (cmd_type) {
  case 0:
    snprintf(out_buf, out_size, "pause %.1f", *(float *)(cmd + 2));
    return;
  case 1: {
    const char *go_mode[] = { "idle aim weapon", "idle turn around",
                              "idle look with head", "forced exact facing",
                              "forced aim weapon" };
    snprintf(out_buf, out_size, "go to (p%d) %s", cmd[6], go_mode[sub_type]);
    return;
  }
  case 2:
    snprintf(out_buf, out_size, "go to (p%d) and face (p%d)", cmd[6], cmd[7]);
    return;
  case 3: {
    const char *move_dir[] = { "forwards", "left", "right", "backwards",
                               "any-facing" };
    if (cmd[6] == -1) {
      snprintf(out_buf, out_size, "move %s along angle %.1f, dist %.2f",
               move_dir[sub_type], *(float *)(cmd + 4), *(float *)(cmd + 2));
      return;
    }
    snprintf(out_buf, out_size, "move %s towards (p%d), dist %.2f",
             move_dir[sub_type], cmd[6], *(float *)(cmd + 2));
    return;
  }
  case 4: {
    const char *look_mode[] = { "idle aim weapon", "idle turn around",
                                "idle look with head", "forced exact facing",
                                "forced aim weapon" };
    snprintf(out_buf, out_size, "look %s at (p%d) for %.1f",
             look_mode[sub_type], cmd[6], *(float *)(cmd + 2));
    return;
  }
  case 5: {
    const char *anim_mode[] = { "idle aim weapon", "noncombat", "asleep",
                                "combat", "panic" };
    snprintf(out_buf, out_size, "animation mode %s", anim_mode[sub_type + 1]);
    return;
  }
  case 6: {
    const char *crouch[] = { "disable", "enable" };
    snprintf(out_buf, out_size, "crouch %s", crouch[sub_type]);
    return;
  }
  case 7:
    snprintf(out_buf, out_size, "shoot at (p%d) for %.1f", cmd[6],
             *(float *)(cmd + 2));
    return;
  case 8:
    snprintf(out_buf, out_size, "throw grenade at (p%d)", cmd[6]);
    return;
  case 9: {
    const char *seat[] = { "any-non-driver", "gunner", "passenger", "driver",
                           "any-seat" };
    snprintf(out_buf, out_size, "enter vehicle as %s if within %.1f",
             seat[sub_type], *(float *)(cmd + 2));
    return;
  }
  case 10:
    snprintf(out_buf, out_size, "exit vehicle");
    return;
  case 11:
    snprintf(out_buf, out_size, "targeted jump (%.2fh, %.2fv)",
             *(float *)(cmd + 2), *(float *)(cmd + 4));
    return;
  case 12: {
    const char *script_mode[] = { "wait-for-finish", "wake-and-continue" };
    char *name;
    name = "<error>";
    if (cmd[9] == -1)
      name = "NONE";
    else if (cmd[9] >= 0 && cmd[9] < *(int *)((char *)scenario_data + 0x450))
      name = (char *)tag_block_get_element((char *)scenario_data + 0x450,
                                           cmd[9], 0x28);
    snprintf(out_buf, out_size, "script %s %s", name, script_mode[sub_type]);
    return;
  }
  case 13: {
    char *name;
    name = "<error>";
    if (cmd[8] != -1) {
      if (cmd[8] >= 0 && cmd[8] < *(int *)((char *)scenario_data + 0x444))
        name = (char *)tag_block_get_element((char *)scenario_data + 0x444,
                                             cmd[8], 0x3c);
      snprintf(out_buf, out_size, "animate %s", name);
    } else
      snprintf(out_buf, out_size, "animate %s", "NONE");
    return;
  }
  case 14: {
    char *name;
    name = "<error>";
    if (cmd[10] != -1) {
      if (cmd[10] >= 0 && cmd[10] < *(int *)((char *)scenario_data + 0x45c))
        name = (char *)tag_block_get_element((char *)scenario_data + 0x45c,
                                             cmd[10], 0x28);
      snprintf(out_buf, out_size, "play recording %s", name);
    } else
      snprintf(out_buf, out_size, "play recording %s", "NONE");
    return;
  }
  case 15: {
    const char *action[] = {
      "berserk",     "surprise-front", "surprise-back", "evade-left",
      "evade-right", "dive-fwd",       "dive-back",     "dive-left",
      "dive-right",  "vehicle-woohoo", "vehicle-scared"
    };
    snprintf(out_buf, out_size, "action %s", action[sub_type]);
    return;
  }
  case 16:
    snprintf(out_buf, out_size, "vocalize %s", FUN_001a67b0(sub_type, 0));
    return;
  case 17: {
    const char *on_off[] = { "enable", "disable" };
    snprintf(out_buf, out_size, "targeting %s", on_off[sub_type]);
    return;
  }
  case 18: {
    const char *on_off[] = { "enable", "disable" };
    snprintf(out_buf, out_size, "initiative %s", on_off[sub_type]);
    return;
  }
  case 19: {
    const char *wait_mode[] = { "until alerted", "until visible enemy",
                                "until told to advance" };
    snprintf(out_buf, out_size, "wait %s", wait_mode[sub_type]);
    return;
  }
  case 20: {
    const char *loop_mode[] = { "always", "only until told to advance" };
    if (cmd[11] == -1) {
      snprintf(out_buf, out_size, "loop to <none> %s", loop_mode[sub_type]);
    } else {
      char local_buf[256];
      csstrcpy(local_buf, "");
      if (cmd[7] != -1)
        crt_sprintf(local_buf, " (p%d)", cmd[7]);
      snprintf(out_buf, out_size, "loop to (p%d)%s %s", cmd[6], local_buf,
               loop_mode[sub_type]);
    }
    return;
  }
  case 21:
    snprintf(out_buf, out_size, "pause in loop %.1f", *(float *)(cmd + 2));
    return;
  case 22: {
    const char *move_dir[] = { "forwards", "left", "right", "backwards" };
    snprintf(out_buf, out_size, "move %s for %.1f sec", move_dir[sub_type],
             *(float *)(cmd + 2));
    return;
  }
  case 23: {
    const char *look_mode[] = { "idle aim weapon", "idle turn around",
                                "idle look with head", "forced exact facing",
                                "forced aim weapon" };
    snprintf(out_buf, out_size,
             "look %s at random one of (p%d-p%d) for %.1f-%.1f",
             look_mode[sub_type], cmd[6], cmd[7], *(float *)(cmd + 2),
             *(float *)(cmd + 4));
    return;
  }
  case 24: {
    const char *look_mode[] = { "idle aim weapon", "idle turn around",
                                "idle look with head", "forced exact facing",
                                "forced aim weapon" };
    snprintf(out_buf, out_size, "look %s at player for %.1f",
             look_mode[sub_type], *(float *)(cmd + 2));
    return;
  }
  case 25: {
    const char *look_mode[] = { "idle aim weapon", "idle turn around",
                                "idle look with head", "forced exact facing",
                                "forced aim weapon" };
    char *elem_name;
    elem_name = "<error>";
    if (cmd[12] >= 0 && cmd[12] < *(int *)((char *)scenario_data + 0x204))
      elem_name = (char *)tag_block_get_element((char *)scenario_data + 0x204,
                                                cmd[12], 0x24);
    snprintf(out_buf, out_size, "look %s at %s for %.1f", look_mode[sub_type],
             elem_name, *(float *)(cmd + 2));
    return;
  }
  case 26:
    snprintf(out_buf, out_size, "set radius %.2f", *(float *)(cmd + 2));
    return;
  case 27:
    snprintf(out_buf, out_size, "continue after melee");
    return;
  }

  snprintf(out_buf, out_size, "<unknown %d>", cmd_type);
}

/* FUN_000178b0 (0x178b0)
 * Subtract two 2D vectors: result = b - a.
 * Confirmed: cdecl, 3 stack params. FLD [ECX]/FSUB [EDX]/FSTP [EAX] twice. */
void FUN_000178b0(float *a, float *b, float *result)
{
  result[0] = b[0] - a[0];
  result[1] = b[1] - a[1];
}

/* Compute the cross product of two 3D vectors: out = a x b.
 *
 * Ref: z computed first, then y, then x; all three FPU results held on the
 * x87 stack before the first FSTP (aliasing safe when b==out). */
void cross_product3d(float *a, float *b, float *out)
{
  float z = a[0] * b[1] - a[1] * b[0];
  float y = a[2] * b[0] - a[0] * b[2];
  float x = a[1] * b[2] - a[2] * b[1];
  out[0] = x;
  out[1] = y;
  out[2] = z;
}

/* FUN_00017910 (0x17910)
 * Negate a 3D vector: result = -a.
 * Confirmed: cdecl, 2 stack params. FCHS on each component. */
void FUN_00017910(float *a, float *result)
{
  result[0] = -a[0];
  result[1] = -a[1];
  result[2] = -a[2];
}

/* FUN_00017940 (0x17940)
 * Draw a random int16_t in [min, max] using the global random seed.
 *
 * Calls get_global_random_seed_address() to obtain a pointer to the global
 * RNG seed, then passes it together with min and max to random_range.
 *
 * Note: MSVC pre-pushed min/max before calling get_global_random_seed_address
 * (which is void), reusing that stack space.  The seed pointer (EAX) is then
 * pushed last, making it the first C argument to random_range.
 *
 * Confirmed: PUSH EAX (max); PUSH ECX (min); CALL 0x10b0d0
 *   (get_global_random_seed_address, takes no params); PUSH EAX (seed);
 *   CALL 0x10b2d0 (random_range); ADD ESP,0xc. */
int16_t FUN_00017940(int16_t min, int16_t max)
{
  return random_range((unsigned int *)get_global_random_seed_address(), min,
                      max);
}

/* FUN_00018b90 (0x18b90) — Action-obey command validator.
 *
 * Validates whether an action-obey command atom should execute based on
 * the current game state. Dispatches through a 28-case switch on the
 * command atom type (local_8[0]), checking actor state, threat distance,
 * facing alignment, and various behavioral flags.
 *
 * Register args: @eax=unit_handle (copied to EDI).
 * Stack args: actor_handle, scenario_index (int16_t), output_byte (byte*),
 *   command (void*).
 *
 * Returns true if the command should be skipped/blocked, false if it
 * should proceed.
 *
 * Confirmed: 28-case jump table at 0x18c02.
 * Confirmed: cases fall-through to default returning true (keep-checking).
 * Confirmed: early-return false for validated commands.
 * Confirmed: case 0/4/0x16-0x19 returns output[2]==0.
 * Confirmed: FPU dot-product checks in case 1/2 (blind/not-blind).
 * Confirmed: case 3 (unit position check).
 * Confirmed: case 7 (burst-fire check with tag weapon rate-of-fire).
 * Confirmed: case 8 (board-vehicle / move-to-line-of-sight).
 * Confirmed: case 10/11 (berserk/melee interrupt check).
 * Confirmed: case 13 (threat state check).
 * Confirmed: case 14 (game-engine status check).
 * Confirmed: case 16 (damage aftermath check).
 * Confirmed: case 19 (stance/vitality sub-cases). */
bool FUN_00018b90(int unit_handle, int actor_handle, short scenario_index,
                  char *output, void *command)
{
  char *actor;
  char *scenario;
  char *atom_table;
  short *atom;
  char check_result;
  short command_priority;
  float range;
  float dot;
  float diff_x, diff_y, diff_z;
  float local_vec[3];
  float threat_position[3];
  float actor_position[3];

  actor = (char *)datum_get(actor_data, actor_handle);
  scenario = (char *)global_scenario_get();
  atom_table = (char *)tag_block_get_element(scenario + 0x438, 0, 0);

  if ((int)(unsigned char)*output >= *(int *)(atom_table + 0x30))
    return 1;

  atom = (short *)tag_block_get_element(
    atom_table + 0x30, (unsigned int)(unsigned char)*output, 0x20);

  switch (*atom) {
  case 0:
  case 4:
  case 0x16:
  case 0x17:
  case 0x18:
  case 0x19:
    return *(short *)(output + 2) == 0;

  case 1:
  case 2:
    if (unit_handle == *(int *)(actor + 0x18) && command != NULL) {
      check_result = (char)FUN_0002a3f0(actor_handle);
      if (check_result == 0 && *(char *)((char *)command + 5) != '\0' &&
          *(char *)((char *)command + 4) != '\0') {
        range = actor_destination_tolerance(actor_handle);
        FUN_00012140((float *)(actor + 0x12c), (float *)((char *)command + 8),
                     local_vec);
        dot = FUN_00012170(local_vec);
        if (dot < range * range ||
            (dot < (range + 0.5f) * (range + 0.5f) &&
               object_get_and_verify_type(*(int *)(actor + 0x18), 3),
             local_vec[0] * *(float *)((char *)object_get_and_verify_type(
                                         *(int *)(actor + 0x18), 3) +
                                       0x18) +
                 local_vec[2] * *(float *)((char *)object_get_and_verify_type(
                                             *(int *)(actor + 0x18), 3) +
                                           0x20) +
                 local_vec[1] * *(float *)((char *)object_get_and_verify_type(
                                             *(int *)(actor + 0x18), 3) +
                                           0x1c) <
               0.0f)) {
          check_result = 1;
        }
      }
      if (*atom == 2 || atom[1] == 0) {
        if (*(char *)(actor + 0x504) != '\0') {
          *(short *)(output + 2) = 10;
          *(char *)(output + 3) = 0;
        }
        if (check_result == 0)
          return 0;
        if (*(short *)(output + 2) != 0)
          return 0;
        check_result = 1;
      } else if (check_result == 0) {
        return 0;
      }
      if (*(char *)((char *)command + 0x18) != '\0') {
        if (*atom != 2) {
          display_assert(
            "current_command->atom_type == _ai_atom_go_to_and_face",
            "c:\\halo\\SOURCE\\ai\\action_obey.c", 0x3d7, 1);
          system_exit(-1);
        }
        if (*(char *)(actor + 0x99) == '\0') {
          diff_x =
            *(float *)((char *)command + 0x1c) - *(float *)(actor + 0x12c);
          diff_y =
            *(float *)((char *)command + 0x20) - *(float *)(actor + 0x130);
          diff_z = 0.0f;
          (void)diff_y;
          (void)diff_z;
          if (magnitude3d(&diff_x) > 0.0f) {
            range = diff_y * *(float *)(actor + 0x178);
            local_vec[0] = diff_x;
          }
        } else {
          FUN_00012140((float *)(actor + 0x12c),
                       (float *)((char *)command + 0x1c), local_vec);
          if (magnitude3d(local_vec) > 0.0f) {
            range = local_vec[2] * *(float *)(actor + 0x17c) +
                    local_vec[1] * *(float *)(actor + 0x178);
          }
        }
        if (local_vec[0] * *(float *)(actor + 0x174) + range <
            *(float *)0x253d54) {
          return 0;
        }
      }
      FUN_0002f1a0(actor_handle);
      return check_result != 0;
    }
    break;

  case 3:
    if (unit_handle == *(int *)(actor + 0x18)) {
      actor_position[0] = *(float *)(actor + 0x12c);
      actor_position[1] = *(float *)(actor + 0x130);
      actor_position[2] = *(float *)(actor + 0x134);
    } else {
      object_get_world_position(unit_handle, (vector3_t *)actor_position);
    }
    FUN_00012140((float *)(output + 0x18), actor_position, threat_position);
    if (threat_position[0] * *(float *)(output + 0xc) +
          threat_position[1] * *(float *)(output + 0x14) +
          threat_position[2] * *(float *)(output + 0x10) <=
        *(float *)(atom + 1)) {
      return 0;
    }
    break;

  case 5:
  case 6:
  case 9:
  case 0xc:
  case 0x11:
  case 0x12:
  case 0x14:
  case 0x15:
  case 0x1a:
  case 0x1b:
    break;

  case 7:
    if (unit_handle == *(int *)(actor + 0x18) && command != NULL) {
      if (*(short *)(actor + 0x60c) != 2 ||
          !(distance_squared3d((float *)((char *)command + 0x38),
                               (float *)(actor + 0x610)) >= 0.5f)) {
        char *weapon_tag;
        weapon_tag = (char *)tag_get(0x61637476, *(int *)(actor + 0x5c));
        command_priority =
          (short)(int)(*(float *)(weapon_tag + 0x84) * TICKS_PER_SECOND);
        if (command_priority < 61)
          command_priority = 60;
        *(short *)(output + 2) = command_priority;
      }
      return *(short *)(output + 2) == 0;
    }
    break;

  case 8:
    if (unit_handle == *(int *)(actor + 0x18) && command != NULL) {
      if (*(char *)((char *)command + 0x49) != '\0') {
        *(unsigned short *)(output + 2) =
          (unsigned short)(-((*(char *)((char *)object_get_and_verify_type(
                                          *(int *)(actor + 0x18), 3) +
                                        0x23d) != '\0')) &
                           0x1e);
        return *(short *)(output + 2) == 0;
      }
      check_result = unit_is_busy(unit_handle);
      if (check_result == 0) {
        threat_position[0] = *(float *)((char *)command + 0x4c);
        threat_position[1] = *(float *)((char *)command + 0x50);
        threat_position[2] = *(float *)((char *)command + 0x54);
        check_result =
          FUN_00021e50(actor_handle, *(short *)((char *)command + 0x4a),
                       threat_position, -1, -1);
        if (check_result != 0) {
          *(char *)((char *)command + 0x48) = 1;
        }
      }
      return *(short *)(output + 2) == 0;
    }
    break;

  case 10:
  case 0xb:
    if ((output[5] & 4) != 0) {
      if (unit_handle == *(int *)(actor + 0x18)) {
        check_result = *(char *)(actor + 0x15c);
      } else {
        check_result = unit_flying_through_air(unit_handle);
      }
      if ((output[5] & 8) != 0 && check_result != '\0') {
        *(short *)(output + 2) = 0;
        *(char *)(output + 3) = 0;
      }
      return *(short *)(output + 2) == 0;
    }
    break;

  case 0xd:
    return *(char *)((char *)object_get_and_verify_type(unit_handle, 3) +
                     0x253) != 0x1c;

  case 0xe:
    check_result = recorded_animation_controlling_unit(unit_handle);
    return (char)(check_result == '\0');

  case 0xf:
    if (command != NULL && *(char *)((char *)command + 0x30) != '\0') {
      return 0;
    }
    break;

  case 0x10:
    return *(short *)((char *)object_get_and_verify_type(unit_handle, 3) +
                      0x338) != 6;

  case 0x13:
    command_priority = atom[1];
    if (command_priority == 0) {
      return *(short *)(actor + 0x6e) > 0;
    }
    if (command_priority == 1) {
      return *(short *)(actor + 0x6e) > 6;
    }
    if (command_priority == 2) {
      if ((output[4] & 8) == 0) {
        output[4] = output[4] | 0x10;
        return 0;
      }
      output[4] = output[4] & 0xe7;
    }
    break;

  default:
    break;
  }

  return 1;
}

/* FUN_00019230 (0x19230)
 * Scripted-look prop-interest update callback.
 *
 * Decrements state_data timers.  If the flags byte at state_data+5 has
 * both bit 0 and bit 1 set, invokes FUN_00017960 to compute the look-at
 * direction and store it into state_data+0xc..0x14.
 *
 * Confirmed: 4 cdecl stack args — actor_handle, object_handle, unused,
 *   state_data_ptr.
 * Confirmed: state_data+0x2 = short countdown; state_data+0x5 = flags;
 *   state_data+0x8 = short secondary countdown.
 * Confirmed: FUN_00017960(state_data@<ecx>, actor_handle@<eax>,
 *   object_handle@<edi>). */
void FUN_00019230(int actor_handle, int object_handle, int unused,
                  char *state_data)
{
  short counter;
  char flags;

  counter = *(short *)(state_data + 2);
  if (counter > 0) {
    *(short *)(state_data + 2) = (short)(counter - 1);
  }

  flags = *(char *)(state_data + 5);
  if (flags & 4) {
    counter = *(short *)(state_data + 8);
    if (counter > 0) {
      *(short *)(state_data + 8) = (short)(counter - 1);
    }
  }

  if (!(flags & 1))
    return;
  if (!(flags & 2))
    return;

  FUN_00017960(state_data, actor_handle, object_handle);
}

/* FUN_00019280 (0x19280)
 * Compute actor prop-interest for the prop list at actor+0x9c using the
 * scripted-look update callback (FUN_00019230).
 *
 * Same pattern as FUN_00017090 but selects the scripted-look callback.
 *
 * Confirmed: datum_get(actor_data, actor_handle);
 * actor_look_compute_prop_interest with callback=FUN_00019230, reset=0,
 * prop_state=actor+0x9c, param_5=0. */
void FUN_00019280(int actor_handle)
{
  char *actor;
  actor = (char *)datum_get(actor_data, actor_handle);
  actor_look_compute_prop_interest(actor_handle, 0, (short *)(actor + 0x9c),
                                   (void (*)(void))FUN_00019230, 0);
}

/* FUN_000192b0 (0x192b0)
 * Execute actor command-list "obey" action update.
 *
 * Dispatches the prop-interest iteration via actor_look_compute_prop_interest
 * with the FUN_00019110 callback. If the finished-flag is set and the actor
 * hasn't posted combat yet, checks encounter flags and actor-type flags to
 * decide whether to post combat. Returns 1 if actor's current action (0x6c)
 * is 0xb and post_combat is set, else 0.
 *
 * Confirmed: datum_get(actor_data, actor_handle) → actor.
 * Confirmed: LEA ECX,[EBP-1] → address of local_finished byte.
 * Confirmed: FUN_00016d40 callback=FUN_00019110. */
int FUN_000192b0(int actor_handle)
{
  char *actor;
  char *encounter_elem;
  int *tag;
  char finished;

  actor = (char *)datum_get(actor_data, actor_handle);
  finished = 1;
  actor_look_compute_prop_interest(actor_handle, 0, (short *)(actor + 0x9c),
                                   (void (*)(void))FUN_00019110,
                                   (int)&finished);
  if (finished != '\0' && *(char *)(actor + 0xa1) == '\0') {
    encounter_elem =
      (char *)tag_block_get_element((char *)global_scenario_get() + 0x438,
                                    (int)*(short *)(actor + 0x9c), 0x60);
    if ((*(char *)(encounter_elem + 0x20) & 0x10) != 0 &&
        *(char *)(actor + 0x15c) != '\0') {
      tag = (int *)tag_get(0x61637472, *(int *)(actor + 0x58));
      if ((*tag & 0x200000) != 0) {
        goto LAB_done;
      }
    }
    *(int *)(actor + 0x94) = game_time_get();
    *(char *)(actor + 0xa1) = 1;
  }
LAB_done:
  if (*(short *)(actor + 0x6c) == 0xb && *(char *)(actor + 0xa1) != '\0') {
    return 1;
  }
  return 0;
}

/* FUN_00019370 (0x19370) — Actor action/look initialization.
 *
 * Initializes the actor's action and look state based on the current
 * combat and behavioral mode. Sets up action-priority (3e8), look-spec
 * (3ec), look-type (3fc), and various behavioral flags (454/457/45d).
 *
 * Primary branches:
 *   1. field_fe (action list active): copies x100-x10c to look-target and
 *      sets special action flags.
 *   2. field_e0 && can_move: aim-at-target mode — copies e4-ec to
 *      3f0-3f8, overrides 3f8 if not blind (field_99==0).
 *   3. field_ca == 1 or 3: retreat/fight guard-mode init.
 *   4. field_6e >= 5 && field_a8&1: swarm-aware fire-mode init.
 *   5. Default: reset action priority; set look-type from field_9f.
 *
 * Post-initialization:
 *   - Copies field_c8 to 426/427, field_ca to 42c.
 *   - If field_f8 is set and FUN_0002a360 passes: dispatches
 *     FUN_0002a7e0 and FUN_00046f10 for firing-position targets.
 *   - field_a9&1: copies field_b0/ac to 430-43c.
 *   - field_a9&4: evasion/cliff-protection init or maintenance.
 *
 * Confirmed: cdecl, single stack arg (actor_handle).
 * Confirmed: datum_get(actor_data, actor_handle) at 0x19381/0x19383.
 * Confirmed: assert !actor->swarm at 0x19390/0x193a5.
 * Confirmed: branch on field_fe at 0x193b4/0x193bc.
 * Confirmed: FUN_0002a3f0(actor_handle) at 0x19426.
 * Confirmed: magnitude3d at 0x19390 (discard result, 0x19595: FSTP ST0).
 * Confirmed: FCOMP [0x2533c0] / FNSTSW / TEST AH,0x44 at 0x19699/0x196a4
 *   for null-vector check.
 * Confirmed: FMUL [0x2533c4]=0.7f / FCOMP at 0x196c2/0x196ce. */
void FUN_00019370(int actor_handle)
{
  char *actor;
  int mode;
  int tmp;
  float tmp_x, tmp_y;
  float len_sq;

  actor = (char *)datum_get(actor_data, actor_handle);

  if (*(char *)(actor + 0x6) != '\0') {
    display_assert("!actor->meta.swarm", "c:\\halo\\SOURCE\\ai\\action_obey.c",
                   0x6f1, 1);
    system_exit(-1);
  }

  if (*(char *)(actor + 0xfe) != '\0') {
    *(short *)(actor + 0x3e8) = 7;
    *(short *)(actor + 0x3ec) = 2;
    *(short *)(actor + 0x3fc) = 4;
    *(char *)(actor + 0x454) = 1;
    *(char *)(actor + 0x457) = 1;
    *(char *)(actor + 0x45d) = 1;
    *(int *)(actor + 0x460) = *(int *)(actor + 0x100);
    *(int *)(actor + 0x464) = *(int *)(actor + 0x104);
    *(int *)(actor + 0x468) = *(int *)(actor + 0x108);
    *(int *)(actor + 0x458) = *(int *)(actor + 0x10c);
    goto LAB_done;
  }

  if (*(char *)(actor + 0xe0) != '\0' && FUN_0002a3f0(actor_handle)) {
    *(short *)(actor + 0x3e8) = 4;
    *(short *)(actor + 0x3ec) = 3;
    *(int *)(actor + 0x3f0) = *(int *)(actor + 0xe4);
    *(int *)(actor + 0x3f4) = *(int *)(actor + 0xe8);
    *(int *)(actor + 0x3f8) = *(int *)(actor + 0xec);
    if (*(char *)(actor + 0x99) == '\0') {
      *(int *)(actor + 0x3f8) = *(int *)(actor + 0x128);
    }
    mode = 4;
    if (*(short *)(actor + 0x6a) < 3)
      mode = 1;
    goto LAB_set_fc;
  }

  if (*(short *)(actor + 0xca) == 3 || *(short *)(actor + 0xca) == 1) {
    mode = 0;
    *(short *)(actor + 0x3e8) = 7;
    *(short *)(actor + 0x3ec) = 0;
  } else {
    if (*(short *)(actor + 0x6e) >= 5 && (*(char *)(actor + 0xa8) & 1) != 0) {
      *(short *)(actor + 0x3e8) = 7;
      *(short *)(actor + 0x3ec) = 2;
      *(short *)(actor + 0x3fc) = 4;
      *(char *)(actor + 0x454) = 1;
      goto LAB_done;
    }
    mode = 0;
    *(short *)(actor + 0x3e8) = 0;
    if (*(char *)(actor + 0x9f) != '\0') {
      tmp = 4;
      if (*(short *)(actor + 0x6a) < 3)
        tmp = 1;
      mode = tmp;
      goto LAB_set_fc;
    }
  }

LAB_set_fc:
  *(short *)(actor + 0x3fc) = (short)mode;

LAB_done:
  if (*(char *)(actor + 0x110) != '\0') {
    *(char *)(actor + 0x45c) = 1;
    *(char *)(actor + 0x110) = 0;
  }

  *(char *)(actor + 0x426) = *(char *)(actor + 0xc8);
  *(char *)(actor + 0x427) = *(char *)(actor + 0xc8);
  *(short *)(actor + 0x42c) = *(short *)(actor + 0xca);

  if (*(char *)(actor + 0xf8) != '\0' && !FUN_0002a360(actor_handle)) {
    if (*(short *)(actor + 0xfa) != -1) {
      tmp_y = *(float *)(actor + 0x5a8);
      tmp_x = *(float *)(actor + 0x5a4);
      magnitude3d(&tmp_x);
      (void)actor_move_animation_impulse(actor_handle, *(short *)(actor + 0xfa),
                                         (int *)&tmp_x);
    }
    if (*(short *)(actor + 0xfc) != -1) {
      (void)FUN_00046f10(*(short *)(actor + 0xfc), *(int *)(actor + 0x18), -1,
                         -1, -1, -1, 0);
    }
    *(char *)(actor + 0xf8) = 0;
  }

  if ((*(char *)(actor + 0xa9) & 1) != 0) {
    *(char *)(actor + 0x430) = 1;
    *(int *)(actor + 0x434) = *(int *)(actor + 0xb0);
    *(int *)(actor + 0x438) = *(int *)(actor + 0xb4);
    *(int *)(actor + 0x43c) = *(int *)(actor + 0xb8);
    *(short *)(actor + 0x42e) = *(short *)(actor + 0xac);
  }

  if ((*(char *)(actor + 0xa9) & 4) != 0) {
    if ((*(char *)(actor + 0xa9) & 8) == 0) {
      if (*(short *)(actor + 0xac) == 0 && *(char *)(actor + 0x15c) == '\0' &&
          !unit_is_busy(*(int *)(actor + 0x18))) {
        tmp_x = *(float *)(actor + 0x174);
        tmp_y = *(float *)(actor + 0x178);
        len_sq = magnitude3d(&tmp_x);
        if (len_sq == *(float *)0x2533c0) {
          tmp_x = **(float **)0x31fc0c;
          tmp_y = *(float *)(*(int *)0x31fc0c + 4);
        }
        *(char *)(actor + 0x440) = 1;
        *(char *)(actor + 0x441) =
          (char)(*(float *)(actor + 0xb4) <
                 *(float *)(actor + 0xb0) * *(float *)0x2533c4);
        *(char *)(actor + 0x442) =
          (char)((*(unsigned char *)(actor + 0xa9) >> 4) & 1);
        *(float *)(actor + 0x444) = tmp_x;
        *(float *)(actor + 0x448) = tmp_y;
        *(float *)(actor + 0x44c) = *(float *)(actor + 0xb0);
        *(float *)(actor + 0x450) = *(float *)(actor + 0xb4);
        *(unsigned char *)(actor + 0xa9) |= 8;
        if ((*(unsigned char *)(actor + 0xa9) & 0x10) != 0) {
          return;
        }
        *(short *)(actor + 0xac) = 15;
        return;
      }
    } else {
      if (*(short *)(actor + 0xac) < 1) {
        return;
      }
      *(int *)(actor + 0x434) = *(int *)(actor + 0x174);
      *(int *)(actor + 0x438) = *(int *)(actor + 0x178);
      *(char *)(actor + 0x430) = 1;
      *(int *)(actor + 0x43c) = *(int *)(actor + 0x17c);
      *(short *)(actor + 0x42e) = 0;
      return;
    }
  }
}

/* FUN_00019750 (0x19750)
 * Initialize action_search state for a non-retreating actor (type 0).
 *
 * Validates state_data != NULL, zeros the 0x2c-byte buffer, then if the
 * actor is not in retreat (actor+0x160 == 0) fills in the initial state:
 * type=0 at state_data+8, param flag at state_data+5, and marks actor
 * as active (actor+0x98 = 1). Returns 1 on success, 0 if retreating.
 *
 * Confirmed: display_assert "state_data", action_search.c line 0x21.
 * Confirmed: csmemset(state_data, 0, 0x2c); actor+0x160 branch. */
int FUN_00019750(int actor_handle, char param_2, char *state_data)
{
  char *actor;
  actor = (char *)datum_get(actor_data, actor_handle);
  if (state_data == NULL) {
    display_assert("state_data", "c:\\halo\\SOURCE\\ai\\action_search.c", 0x21,
                   1);
    system_exit(-1);
  }
  csmemset(state_data, 0, 0x2c);
  if (*(char *)(actor + 0x160) == '\0') {
    *(short *)(state_data + 8) = 0;
    *(char *)(state_data + 5) = param_2;
    *(char *)(actor + 0x98) = 1;
    return 1;
  }
  return 0;
}

/* FUN_000197d0 (0x197d0)
 * Initialize search action state from a firing-position record (type 1).
 *
 * Similar to FUN_0001a100 (uncover variant) but uses a 0x2c-byte buffer
 * and stores param_3 (byte flag) to state_data+4 instead of state_data+3.
 * Validates actor conditions, looks up the firing position, fills state.
 *
 * Confirmed: 4-arg cdecl: actor_handle, param_2 (short), param_3 (byte),
 *   state_data (ptr). datum_get at 0x197e1. assert line 0x38.
 * Confirmed: [EBP+0x10] → MOV [ESI+4],CL (state_data[4] = param_3). */
int FUN_000197d0(int actor_handle, short param_2, char param_3,
                 char *state_data)
{
  char *actor;
  char *enc;
  int *pos;

  actor = (char *)datum_get(actor_data, actor_handle);
  if (state_data == NULL) {
    display_assert("state_data", "c:\\halo\\SOURCE\\ai\\action_search.c", 0x38,
                   1);
    system_exit(-1);
  }
  csmemset(state_data, 0, 0x2c);
  if ((*(char *)(actor + 0x160) == '\0') && (*(char *)(actor + 6) == '\0') &&
      (*(int *)(actor + 0x34) != -1)) {
    if (param_2 != -1) {
      enc = (char *)tag_block_get_element(
        (char *)global_scenario_get() + 0x42c,
        *(unsigned int *)(actor + 0x34) & 0xffff, 0xb0);
      pos = (int *)tag_block_get_element(enc + 0x98, (int)param_2, 0x18);
      *(char *)(state_data + 4) = param_3;
      *(short *)(state_data + 10) = param_2;
      *(short *)(state_data + 8) = 1;
      *(int *)(state_data + 0x14) = pos[0];
      *(int *)(state_data + 0x18) = pos[1];
      *(int *)(state_data + 0x1c) = pos[2];
      *(int *)(state_data + 0x10) = pos[5];
      *(short *)(state_data + 0xc) = *(short *)((char *)pos + 0xe);
      *(char *)(actor + 0x98) = 1;
      return 1;
    }
    return 0;
  }
  return 0;
}

/* FUN_000198d0 (0x198d0)
 * Initialize action_search state for a berserk actor (type 2).
 *
 * Validates state_data != NULL, zeros the 0x2c-byte buffer, then if the
 * actor is berserk (actor+6 != 0) sets state type=2 at state_data+8 and
 * marks actor as active (actor+0x98 = 1). Returns 1 on success, 0 if
 * not berserk.
 *
 * Confirmed: display_assert "state_data", action_search.c line 0x57.
 * Confirmed: csmemset(state_data, 0, 0x2c); actor+6 branch; type=2. */
int FUN_000198d0(int actor_handle, int param_2, char *state_data)
{
  char *actor;
  actor = (char *)datum_get(actor_data, actor_handle);
  if (state_data == NULL) {
    display_assert("state_data", "c:\\halo\\SOURCE\\ai\\action_search.c", 0x57,
                   1);
    system_exit(-1);
  }
  csmemset(state_data, 0, 0x2c);
  if (*(char *)(actor + 6) != '\0') {
    *(short *)(state_data + 8) = 2;
    *(char *)(actor + 0x98) = 1;
    return 1;
  }
  return 0;
}

/* FUN_00019940 (0x19940)
 * Per-tick update for action_search: checks if the search has been found
 * or timed out, increments/decrements counters, fires look-event if needed.
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x19948.
 * Confirmed: tag_get(0x61637472, actor+0x58).
 * Confirmed: actor+0x9c = search done; actor+0x9e/0xbc/0xc0 = tick counters. */
void FUN_00019940(int actor_handle)
{
  char *actor;
  char *tag_data;
  char *prop;
  int remain;

  actor = (char *)datum_get(actor_data, actor_handle);
  if (*(char *)(actor + 0x9c) != '\0') {
    return;
  }
  tag_data = (char *)tag_get(0x61637472, *(int *)(actor + 0x58));
  *(char *)(actor + 0x9f) = 0;
  if ((*(short *)(tag_data + 0x2f8) == 4) ||
      ((*(tag_data) & 2) != 0 && *(short *)(actor + 0xa4) == 0 &&
       *(short *)(actor + 0x268) == 5)) {
    if (*(short *)(tag_data + 0x2f8) != 4) {
      prop = (char *)datum_get(prop_data, *(int *)(actor + 0x270));
      if (*(char *)(prop + 0x121) >= 3) {
        goto skip_flag;
      }
    }
    *(char *)(actor + 0x9f) = 1;
  }
skip_flag:
  if (*(char *)(actor + 0x9e) == '\0') {
    if ((*(char *)(actor + 0x504) == '\0') && (*(char *)(actor + 6) == '\0')) {
      *(int *)(actor + 0xc4) = *(int *)(actor + 0xc4) + 1;
      if (0x78 < *(int *)(actor + 0xc4)) {
        *(char *)(actor + 0x9d) = 1;
        *(char *)(actor + 0x9c) = 1;
      }
    }
  } else {
    if (0 < *(int *)(actor + 0xc0)) {
      *(int *)(actor + 0xc0) = *(int *)(actor + 0xc0) - 1;
    }
    remain = *(int *)(actor + 0xc0);
    if (remain == 0) {
      *(char *)(actor + 0x9c) = 1;
    }
    if (*(int *)(actor + 0x18) != -1) {
      if (*(short *)(actor + 0xa4) == 0) {
        if ((*(char *)(actor + 0x3bd) == '\0') &&
            (*(char *)(actor + 0x9c) != '\0' ||
             (remain + 0x5a < *(int *)(actor + 0xbc)))) {
          *(int *)(actor + 0x3bd) = 1;
          FUN_00046f10(0xd, *(int *)(actor + 0x18),
                       actor_target_unit_index(actor_handle), -1, -1, -1, 0);
          *(char *)(actor + 0x3bd) = 1;
          return;
        }
      } else if (remain == 0) {
        FUN_00046f10(0x12, *(int *)(actor + 0x18),
                     actor_target_unit_index(actor_handle), -1, -1, -1, 0);
        return;
      }
    }
  }
}

/* FUN_00019ac0 (0x19ac0)
 * Mark actor look-state as interrupted (target type 1 path).
 *
 * If the look-target type word at actor+0xa4 equals 1, clears the
 * target-acquired index at actor+0xa6 (set to 0xffff = none) and
 * sets the look-state byte at actor+0x9c to 1.
 *
 * Confirmed: ADD EAX,0x9c after datum_get; CMP word [EAX+0x8],0x1;
 *   MOV word [EAX+0xa],0xffff; MOV byte [EAX],0x1. */
void FUN_00019ac0(int actor_handle)
{
  char *actor;
  actor = (char *)datum_get(actor_data, actor_handle);
  if (*(short *)(actor + 0xa4) == 1) {
    *(short *)(actor + 0xa6) = (short)0xffff;
    *(char *)(actor + 0x9c) = 1;
  }
}

/* FUN_00019af0 (0x19af0)
 * Reset actor look-target handles to invalid (-1).
 *
 * Unconditionally clears actor+0xa8 (int16_t) and actor+0xac (int32_t)
 * to -1 (all-bits-set via OR ECX,0xffffffff).
 *
 * Confirmed: ADD EAX,0x9c after datum_get; OR ECX,0xffffffff;
 *   MOV word [EAX+0xc],CX (actor+0xa8); MOV dword [EAX+0x10],ECX (actor+0xac).
 */
void FUN_00019af0(int actor_handle)
{
  char *actor;
  actor = (char *)datum_get(actor_data, actor_handle);
  *(short *)(actor + 0xa8) = -1;
  *(int *)(actor + 0xac) = -1;
}

/* FUN_00019b20 (0x19b20)
 * Set up search-mode look output fields for current actor.
 * Chooses move-type based on timer elapsed vs threshold (timer/3 min 0x5a).
 * Sets nav-type based on search stance (actor+0xa4). Sets look-speed=3.
 * Sets actor+0x454 (sneak flag) from actr tag bit 0x10 and actor+0x268.
 *
 * Confirmed: datum_get → tag_get(actr). IMUL 0x55555556 for signed /3.
 * Confirmed: CMP [ESI+0xa4],0; SETGE for sneak flag.
 * Confirmed: if (tag[0] & 0x10): check >=5, else check >=6. */
void FUN_00019b20(int actor_handle)
{
  char *actor;
  char *tag;
  int timer;
  int remain;
  int threshold;

  actor = (char *)datum_get(actor_data, actor_handle);
  tag = (char *)tag_get(0x61637472, *(int *)(actor + 0x58));
  if (*(char *)(actor + 0x504) != '\0') {
    *(short *)(actor + 0x3e8) = 3;
    *(short *)(actor + 0x3ec) = 0;
  } else {
    timer = *(int *)(actor + 0xbc);
    threshold = timer / 3;
    if (threshold <= 0x5a)
      threshold = 0x5a;
    remain = *(int *)(actor + 0xc0);
    if (timer - remain < threshold) {
      if (*(short *)(actor + 0xa4) == 0) {
        *(short *)(actor + 0x3e8) = 3;
        *(short *)(actor + 0x3ec) = 2;
      } else if (*(short *)(actor + 0xa4) == 1) {
        *(short *)(actor + 0x3e8) = 3;
        *(short *)(actor + 0x3ec) = 3;
        *(int *)(actor + 0x3f0) = *(int *)(actor + 0xb0);
        *(int *)(actor + 0x3f4) = *(int *)(actor + 0xb4);
        *(int *)(actor + 0x3f8) = *(int *)(actor + 0xb8);
      } else {
        *(short *)(actor + 0x3e8) = 1;
      }
    } else {
      *(short *)(actor + 0x3e8) = 1;
    }
  }
  *(short *)(actor + 0x3fc) = 3;
  if (*(short *)(actor + 0xa4) == 0) {
    if ((*tag & 0x10) != 0) {
      *(char *)(actor + 0x454) = (char)(*(short *)(actor + 0x268) >= 5);
    } else {
      *(char *)(actor + 0x454) = (char)(*(short *)(actor + 0x268) >= 6);
    }
  }
  *(char *)(actor + 0x426) = *(char *)(actor + 0x9f);
  *(char *)(actor + 0x427) = *(char *)(actor + 0x9f);
  *(char *)(actor + 0x428) = 0;
  *(char *)(actor + 0x424) = 0;
  *(char *)(actor + 0x425) = 1;
}

/* FUN_00019c70 (0x19c70)
 * Randomize actor look timers from actor-type tag ranges.
 *
 * Selects min/max from tag+0x344/0x348 (look_type==0) or
 * tag+0x34c/0x350 (look_type!=0). Generates a random float in
 * [min,max], multiplies by 30 (ticks/sec), converts to int, and
 * stores in both actor+0xbc and actor+0xc0.
 *
 * Confirmed: CMP WORD [ESI+0xa4],0 selects range pair.
 * Confirmed: FMUL [0x253394] → multiply by 30.0f.
 * Confirmed: CALL _ftol2 → (int) cast. */
void FUN_00019c70(int actor_handle)
{
  char *actor;
  char *tag;
  float lo;
  float hi;
  int ticks;

  actor = (char *)datum_get(actor_data, actor_handle);
  tag = (char *)tag_get(0x61637472, *(int *)(actor + 0x58));
  if (*(short *)(actor + 0xa4) == 0) {
    hi = *(float *)(tag + 0x348);
    lo = *(float *)(tag + 0x344);
  } else {
    hi = *(float *)(tag + 0x350);
    lo = *(float *)(tag + 0x34c);
  }
  ticks = (int)(random_real_range(get_global_random_seed_address(), lo, hi) *
                TICKS_PER_SECOND);
  *(int *)(actor + 0xbc) = ticks;
  *(int *)(actor + 0xc0) = ticks;
}

/* actor_look_secondary (0x19d00) — Secondary actor aim/fire target evaluation.
 *
 * Evaluates whether the actor should engage a secondary aim target when the
 * primary look-target condition (field_9e) is not met.  Branches:
 *   1. field_a4==0 && field_270!=-1: distance check against threat obj.
 *   2. field_a4==1 && field_a6!=-1: distance/window check with LOS test.
 *   3. field_9e==0 && field_a1==0: iterate nearby actors (field_24 in [2,3])
 *      and count/assess proximity for retreat/support decisions.
 *   4. Fallback: FUN_0002d9b0 for type-0, FUN_0002d900 for type-1.
 *
 * Confirmed: cdecl, single stack arg (actor_handle).
 * Confirmed: datum_get(actor_data, actor_handle) at 0x19d10/0x19d12.
 * Confirmed: check swarm/field_4c/field_9c early-exit at
 * 0x19d1f/0x19d2c/0x19d38. Confirmed: field_a4==0 branch: datum_get(prop_data,
 * field_270) at 0x19d5f/0x19d67, squared-distance comparison at
 * 0x19d91-0x19d9f. Confirmed: field_a4==1 branch: squared-distance to field_b0
 * at 0x19ddf/0x19de4, FCOM [0x253dd0] / FCOMP [0x253dcc] at 0x19de4/0x19dff,
 *   unit_estimate_position + ai_test_line_of_sight at 0x19e24/0x19e59.
 * Confirmed: actor iteration with FUN_00064540/FUN_00064570 at 0x19e97/0x19ea0,
 *   loop head at 0x19eb2, FUN_00020140 at 0x19ee0, inner datum_get at 0x19ef7.
 * Confirmed: encounter support FUN_0005b5e0 at 0x19fac.
 * Confirmed: fallback FUN_0002d9b0(actor_handle, field_270, 3.0f) at 0x19feb.
 *   or FUN_0002d900(actor_handle, field_a6, 0) at 0x19fa2a.
 * Confirmed: FUN_0002f1a0(actor_handle) call at 0x1a035. */
int actor_look_secondary(int actor_handle)
{
  char *actor;
  char *threat;
  float dist_sq_threshold;
  float offset_radius[3];
  int iter_ctx[2];
  char *iter_actor;
  short los_result;
  int found_count;
  int nearby_count;
  int enc_handle;
  int ret;

  actor = (char *)datum_get(actor_data, actor_handle);

  if (*(char *)(actor + 6) != '\0')
    return *(int *)(actor + 0x9c);
  if (*(char *)(actor + 0x4c) == '\0')
    return *(int *)(actor + 0x9c);
  if (*(char *)(actor + 0x9c) != '\0')
    return *(int *)(actor + 0x9c);

  *(char *)(actor + 0x9e) = 1;

  if (*(short *)(actor + 0xa4) == 0) {
    if (*(int *)(actor + 0x270) != -1) {
      threat = (char *)datum_get(*(data_t **)0x5ab23c, *(int *)(actor + 0x270));
      dist_sq_threshold = 1.7f;
      if (*(short *)(threat + 0x38) != 0) {
        dist_sq_threshold = 0.7f;
      }
      if (distance_squared3d((float *)(threat + 0xbc),
                             (float *)(actor + 0x12c)) <
          dist_sq_threshold * dist_sq_threshold) {
        *(char *)(actor + 0x9e) = 1;
      } else {
        *(char *)(actor + 0x9e) = 0;
      }
    }
  } else if (*(short *)(actor + 0xa4) == 1) {
    if (*(short *)(actor + 0xa6) != -1) {
      if (distance_squared3d((float *)(actor + 0xb0),
                             (float *)(actor + 0x12c)) < *(float *)0x253dd0) {
        *(char *)(actor + 0x9e) = 1;
      } else if (distance_squared3d((float *)(actor + 0xb0),
                                    (float *)(actor + 0x12c)) <
                 *(float *)0x253dcc) {
        unit_estimate_position(*(int *)(actor + 0x18), 1,
                               (vector3_t *)(actor + 0xb0), NULL, NULL,
                               (vector3_t *)offset_radius);
        los_result = (short)ai_test_line_of_sight(
          (float *)(actor + 0x120), (int)*(uint16_t *)(actor + 0x148),
          offset_radius, (int)*(uint16_t *)(actor + 0xa8), 0, 0, -1,
          (char)(*(int *)(actor + 0x158) != -1));
        *(char *)(actor + 0x9e) = (char)(los_result == 0);
      } else {
        *(char *)(actor + 0x9e) = 0;
      }
    }
  }

  if (*(char *)(actor + 0x9e) == '\0' && *(char *)(actor + 0xa1) == '\0') {
    found_count = 0;
    nearby_count = 0;
    FUN_00064540((void *)iter_ctx, actor_handle);
    iter_actor = (char *)FUN_00064570((void *)iter_ctx);
    while (iter_actor != NULL) {
      if (*(short *)(iter_actor + 0x24) >= 2 &&
          *(short *)(iter_actor + 0x24) <= 3 &&
          *(char *)(iter_actor + 0x60) == '\0' &&
          *(char *)(iter_actor + 0x127) == '\0' &&
          *(int *)(iter_actor + 0x1c) != -1 &&
          actors_searching_same_position(actor_handle,
                                         *(int *)(iter_actor + 0x1c))) {
        iter_actor = (char *)datum_get(actor_data, *(int *)(iter_actor + 0x1c));
        found_count = found_count + 1;
        if (*(char *)(iter_actor + 6) == '\0' &&
            *(char *)(iter_actor + 0x504) == '\0' &&
            distance_squared3d((float *)(actor + 0x12c),
                               (float *)(iter_actor + 0x12c)) <
              *(float *)0x253dc8) {
          nearby_count = nearby_count + 1;
        }
      }
      iter_actor = (char *)FUN_00064570((void *)iter_ctx);
    }

    if (*(char *)(actor + 0xa0) == '\0' &&
        found_count >= (short)((*(short *)(actor + 0xa4) != 1) * 2 + 2)) {
      *(char *)(actor + 0x9c) = 1;
      enc_handle = -1;
      if (*(int *)(actor + 0x270) != -1) {
        threat =
          (char *)datum_get(*(data_t **)0x5ab23c, *(int *)(actor + 0x270));
        enc_handle = *(int *)(threat + 0x7c);
      }
      if (*(int *)(actor + 0x34) != -1) {
        encounter_mark_examined_pursuit_position(
          *(int *)(actor + 0x34), actor_handle, *(short *)(actor + 0xa6),
          enc_handle);
      }
    } else if (nearby_count > 0) {
      *(char *)(actor + 0x9e) = 1;
    }
  }

  if (*(char *)(actor + 0x9e) == '\0') {
    if (*(short *)(actor + 0xa4) == 0) {
      ret = actor_move_to_prop(actor_handle, *(int *)(actor + 0x270), 3.0f);
      if (!ret) {
        *(char *)(actor + 0x9c) = 1;
        *(char *)(actor + 0x9d) = 1;
        return *(int *)(actor + 0x9c);
      }
    } else if (*(short *)(actor + 0xa4) == 1) {
      *(short *)(actor + 0x3b8) = -1;
      ret = actor_move_to_firing_position(actor_handle,
                                          *(short *)(actor + 0xa6), 0);
      if (!ret) {
        *(char *)(actor + 0x9c) = 1;
        *(char *)(actor + 0x9d) = 1;
        return *(int *)(actor + 0x9c);
      }
    }
  }

  FUN_0002f1a0(actor_handle);
  return *(int *)(actor + 0x9c);
}

/* FUN_0001a050 (0x1a050)
 * Clear the look-spec type word at actor+0x3fc.
 *
 * Sets actor+0x3fc to 0 (int16_t write, zero-extending).
 *
 * Confirmed: CALL datum_get; ADD ESP,0x8;
 *   MOV word [EAX+0x3fc],0x0. */
void FUN_0001a050(int actor_handle)
{
  char *actor;
  actor = (char *)datum_get(actor_data, actor_handle);
  *(short *)(actor + 0x3fc) = 0;
}

/* FUN_0001a080 (0x1a080)
 * Initialize action_uncover state data for a non-retreating, non-berserk actor.
 *
 * Validates state_data != NULL, zeros the 0x34-byte buffer, then checks
 * two actor flags: if the actor is not retreating (actor+0x160 == 0) AND
 * not berserk (actor+6 == 0), fills in the initial state: type=0 at
 * state_data+8, param_2 at state_data+3, and returns 1.  Returns 0 if
 * either flag is set.
 *
 * Confirmed: display_assert "state_data", action_uncover.c line 0x22.
 * Confirmed: csmemset(state_data, 0, 0x34); MOV word [state_data+8],0x0.
 * Confirmed: MOV byte [state_data+3],param_2; actor+0x160 and actor+6 checks.
 * Confirmed: MOV AL,0x1 (success path); MOV AL,BL (BL=0, failure path). */
int FUN_0001a080(int actor_handle, char param_2, char *state_data)
{
  char *actor;
  actor = (char *)datum_get(actor_data, actor_handle);
  if (state_data == NULL) {
    display_assert("state_data", "c:\\halo\\SOURCE\\ai\\action_uncover.c", 0x22,
                   1);
    system_exit(-1);
  }
  csmemset(state_data, 0, 0x34);
  if ((*(char *)(actor + 0x160) == '\0') && (*(char *)(actor + 6) == '\0')) {
    *(short *)(state_data + 8) = 0;
    *(char *)(state_data + 3) = param_2;
    return 1;
  }
  return 0;
}

/* FUN_0001a100 (0x1a100)
 * Initialize uncover action state from a firing-position record.
 *
 * Validates state_data, zeros it, then if actor is not suppressed/berserk and
 * has an encounter (actor+0x34 != -1) AND param_2 != -1: looks up the firing
 * position in the scenario tag, fills state_data with the position data and
 * marks actor+0x98 as "new firing position chosen". Returns 1 on success, 0 if
 * actor has no encounter or param_2 == -1.
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x1a108.
 * Confirmed: assert on state_data (action_uncover.c:0x38).
 * Confirmed: csmemset 0x34 bytes. tag_block_get_element stride 0x18.
 * Confirmed: pos[5] = *(int*)((char*)pos+0x14) stored at state_data+0x10. */
int FUN_0001a100(int actor_handle, short param_2, char *state_data)
{
  char *actor;
  char *enc;
  int *pos;

  actor = (char *)datum_get(actor_data, actor_handle);
  if (state_data == NULL) {
    display_assert("state_data", "c:\\halo\\SOURCE\\ai\\action_uncover.c", 0x38,
                   1);
    system_exit(-1);
  }
  csmemset(state_data, 0, 0x34);
  if ((*(char *)(actor + 0x160) == '\0') && (*(char *)(actor + 6) == '\0') &&
      (*(unsigned int *)(actor + 0x34) != 0xffffffff)) {
    if (param_2 != -1) {
      enc = (char *)tag_block_get_element(
        (char *)global_scenario_get() + 0x42c,
        *(unsigned int *)(actor + 0x34) & 0xffff, 0xb0);
      pos = (int *)tag_block_get_element(enc + 0x98, (int)param_2, 0x18);
      *(short *)(state_data + 10) = param_2;
      *(short *)(state_data + 8) = 1;
      *(int *)(state_data + 0x14) = pos[0];
      *(int *)(state_data + 0x18) = pos[1];
      *(int *)(state_data + 0x1c) = pos[2];
      *(int *)(state_data + 0x10) = pos[5];
      *(short *)(state_data + 0xc) = *(short *)((char *)pos + 0xe);
      *(char *)(state_data + 0x20) = 0;
      *(char *)(state_data + 3) = 1;
      *(char *)(actor + 0x98) = 1;
      return 1;
    }
    return 0;
  }
  return 0;
}

/* FUN_0001a420 (0x1a420)
 * Set up uncover-mode look output fields (movement/navigation type).
 *
 * Based on actor+0xa4 (search stance), sets move-type and nav-target:
 * If actor has a prop (actor+0x270 != -1):
 *   - Determines sneak threshold based on prop type and actor level (0x268).
 *   - If stance 0: sets move-type 7 or 3 based on threat level ≥ 6/5.
 *   - If stance 1: sets move-type 7 or 2 based on prop engagement type.
 * Sets speed (0x3fc=3), look flags, and approach flags.
 *
 * Confirmed: datum_get(actor_data) at 0x1a428; tag_get(actr) at 0x1a430.
 * Confirmed: SBORROW2(*(short*)(actor+0x268), 6) → (actor+0x268 >= 6). */
void FUN_0001a420(int actor_handle)
{
  char *actor;
  char *tag_data;
  char *prop;
  int prop_handle;

  actor = (char *)datum_get(actor_data, actor_handle);
  tag_data = (char *)tag_get(0x61637472, *(int *)(actor + 0x58));
  prop_handle = *(int *)(actor + 0x270);
  if (prop_handle != -1) {
    prop = (char *)datum_get(prop_data, prop_handle);
    if (*(short *)(actor + 0xa4) == 0) {
      if (*(char *)(actor + 0x162) == '\0') {
        if ((*tag_data & 0x10) == 0) {
          *(char *)(actor + 0x454) = (char)(*(short *)(actor + 0x268) >= 6);
        } else {
          *(char *)(actor + 0x454) = (char)(*(short *)(actor + 0x268) >= 5);
        }
      } else {
        *(char *)(actor + 0x454) = 1;
        *(char *)(actor + 0x455) = 1;
      }
    }
    if ((*(char *)(actor + 0x454) != '\0' &&
         (*(short *)(prop + 0x38) == 0 || *(short *)(prop + 0x38) == 1)) ||
        (*(char *)(actor + 0x162) != '\0')) {
      *(short *)(actor + 0x3e8) = 7;
    } else if (*(short *)(actor + 0x268) < 5) {
      *(short *)(actor + 0x3e8) = 3;
    } else if (*(short *)(prop + 0x38) == 2 || *(short *)(prop + 0x38) == 4) {
      *(short *)(actor + 0x3e8) = 2;
    } else {
      *(short *)(actor + 0x3e8) = 5;
    }
    if (*(short *)(actor + 0xa4) == 0) {
      *(short *)(actor + 0x3ec) = 2;
    } else if (*(short *)(actor + 0xa4) == 1) {
      *(short *)(actor + 0x3ec) = 3;
      *(int *)(actor + 0x3f0) = *(int *)(actor + 0xb0);
      *(int *)(actor + 0x3f4) = *(int *)(actor + 0xb4);
      *(int *)(actor + 0x3f8) = *(int *)(actor + 0xb8);
    }
  }
  *(short *)(actor + 0x3fc) = 3;
  *(char *)(actor + 0x426) = *(char *)(actor + 0x9c);
  *(char *)(actor + 0x427) = *(char *)(actor + 0x9c);
  *(char *)(actor + 0x428) = 0;
  *(char *)(actor + 0x424) = 0;
  *(char *)(actor + 0x425) = 1;
}

/* FUN_0001a590 (0x1a590)
 * Mark actor look-state as interrupted (target type 1 path, byte +0x9d).
 *
 * If the look-target type word at actor+0xa4 equals 1, clears the
 * target-acquired index at actor+0xa6 (set to 0xffff = none) and
 * sets the look-state byte at actor+0x9d to 1.  Differs from FUN_00019ac0
 * only in the byte offset written: 0x9d vs 0x9c.
 *
 * Confirmed: ADD EAX,0x9c; MOV ECX,0x1; CMP word [EAX+0x8],CX;
 *   MOV word [EAX+0xa],0xffff; MOV byte [EAX+0x1],CL. */
void FUN_0001a590(int actor_handle)
{
  char *actor;
  actor = (char *)datum_get(actor_data, actor_handle);
  if (*(short *)(actor + 0xa4) == 1) {
    *(short *)(actor + 0xa6) = (short)0xffff;
    *(char *)(actor + 0x9d) = 1;
  }
}

/* FUN_0001a5d0 (0x1a5d0)
 * Reset actor look-target handles to invalid (-1).
 *
 * Identical body to FUN_00019af0: clears actor+0xa8 (int16_t) and
 * actor+0xac (int32_t) to -1.  Compiled as a separate function at a
 * different address.
 *
 * Confirmed: ADD EAX,0x9c; OR ECX,0xffffffff;
 *   MOV word [EAX+0xc],CX (actor+0xa8); MOV dword [EAX+0x10],ECX (actor+0xac).
 */
void FUN_0001a5d0(int actor_handle)
{
  char *actor;
  actor = (char *)datum_get(actor_data, actor_handle);
  *(short *)(actor + 0xa8) = -1;
  *(int *)(actor + 0xac) = -1;
}

/* FUN_0001a600 (0x1a600)
 * Select actor look-direction weights based on uncover state.
 *
 * If actor+0x9c (char, uncover active flag) is non-zero, copies the 16-byte
 * vector from the global pointer at 0x2ee6d8; otherwise copies from 0x2ee6ec.
 *
 * Confirmed: datum_get(actor_data, actor_handle) → actor.
 * Confirmed: TEST CL,CL after MOV CL,[EAX+0x9c] → char comparison.
 * Confirmed: 16-byte (4 dword) copy from dereferenced global pointer. */
void FUN_0001a600(int actor_handle, int *param_2)
{
  char *actor;
  char *looking;
  char *src;

  actor = (char *)datum_get(actor_data, actor_handle);
  looking = actor + 0x9c;
  if (*looking != '\0') {
    src = *(char **)0x2ee6d8;
    *param_2 = *(int *)src;
    param_2[1] = *(int *)(src + 4);
    param_2[2] = *(int *)(src + 8);
    param_2[3] = *(int *)(src + 0xc);
    return;
  }
  src = *(char **)0x2ee6ec;
  *param_2 = *(int *)src;
  param_2[1] = *(int *)(src + 4);
  param_2[2] = *(int *)(src + 8);
  param_2[3] = *(int *)(src + 0xc);
}

/* FUN_0001a670 (0x1a670)
 * Initialize uncover timer and optionally emit a communication vocalization.
 *
 * Selects min/max ranges from actor tag based on actor+0x9f (search-able flag).
 * Generates a random timer, multiplied by 30 ticks/sec, stores in actor+0xc4
 * and actor+0xc8. If AI debug is enabled, logs the timer params. If the actor's
 * look-type is 0 (target) and has a prop with combat_status < 3, emits a
 * vocalization (type 0x15).
 *
 * Confirmed: standard cdecl, SUB ESP,0x10c for locals.
 * Confirmed: FMUL [0x253394] = multiply by 30.0f, then _ftol2 cast. */
void FUN_0001a670(int actor_handle)
{
  char *actor;
  char *tag;
  char *prop;
  float lo;
  float hi;
  float random_val;
  int ticks;
  char local_110[256];

  actor = (char *)datum_get(actor_data, actor_handle);
  tag = (char *)tag_get(0x61637472, *(int *)(actor + 0x58));
  hi = *(float *)(tag + 0x340);
  lo = *(float *)(tag + 0x33c);
  if (*(char *)(actor + 0x9f) == '\0') {
    if (lo <= *(float *)(tag + 0x344)) {
      lo = *(float *)(tag + 0x344);
    }
    if (hi <= *(float *)(tag + 0x348)) {
      hi = *(float *)(tag + 0x348);
    }
  }
  random_val = random_real_range(get_global_random_seed_address(), lo, hi);
  ticks = (int)(random_val * TICKS_PER_SECOND);
  *(int *)(actor + 0xc4) = ticks;
  *(int *)(actor + 0xc8) = ticks;
  if (*(char *)0x5aca64 != '\0') {
    ai_debug_describe_actor(actor_handle, -1, 1, local_110, 0x100);
    error(2, "%s: begin %s uncover for [%.1f-%.1f] = %.1f (%sable to search)",
          (int)local_110,
          *(short *)(actor + 0xa4) != 0 ? (int)"pursuit" : (int)"target",
          (double)lo, (double)hi, (double)random_val,
          *(char *)(actor + 0x9f) == '\0' ? (int)"un" : (int)"");
  }
  if (*(short *)(actor + 0xa4) == 0 && *(int *)(actor + 0x270) != -1 &&
      *(short *)(actor + 0x6e) < 3) {
    prop = (char *)datum_get(*(data_t **)0x5ab23c, *(int *)(actor + 0x270));
    FUN_00046f10(0x15, *(int *)(actor + 0x18), *(int *)(prop + 0x18), -1, -1,
                 -1, 0);
  }
}

/* FUN_0001a7e0 (0x1a7e0) — Update action_uncover state and timers.
 *
 * Manages the actor's "uncover" behavioral state: determines whether
 * the actor should mark a location as uncovered based on actor-tag
 * flags, threat distance, and persistent/timer-based conditions.
 * Handles debug-log output via FUN_00049ac0 and FUN_0008f390.
 *
 * Confirmed: cdecl, single stack arg (actor_handle).
 * Confirmed: datum_get at 0x1a7e0, tag_get('actr') at 0x1a7e0+0x48.
 * Confirmed: early return on field_9d at 0x1a7e0+0x13.
 * Confirmed: field_a4==0 branch: tag->field_2f8==4 or threat-combat check.
 * Confirmed: field_a4==1 branch: tag->field_2f8==4 or distance check.
 * Confirmed: field_504==0 timer management at 0x1a8bf.
 * Confirmed: field_3b8==-1 timer decrement at 0x1a958.
 * Confirmed: debug-log FUN_00049ac0 at 0x1a9af, FUN_0008f390 at 0x1aa70. */
void FUN_0001a7e0(int actor_handle)
{
  char *actor;
  char *tag;
  char *threat;
  char result;
  char debug_buf[256];
  char debug_msg[259];
  char done;

  actor = (char *)datum_get(actor_data, actor_handle);
  if (*(char *)(actor + 0x9d) != '\0')
    return;

  tag = (char *)tag_get(0x61637472, *(int *)(actor + 0x58));
  result = 1;
  done = 0;
  *(char *)(actor + 0x9c) = 0;

  if (*(short *)(actor + 0xa4) == 0) {
    if (*(short *)(tag + 0x2f8) == 4) {
      *(char *)(actor + 0x9c) = (char)(*(short *)(actor + 0x268) != 6);
    } else if (((*(char *)tag & 2) != 0) && (*(short *)(actor + 0x268) == 5)) {
      threat = (char *)datum_get(*(data_t **)0x5ab23c, *(int *)(actor + 0x270));
      if (*(char *)(threat + 0x121) < 3)
        *(char *)(actor + 0x9c) = 1;
    }
  } else if (*(short *)(actor + 0xa4) == 1) {
    if (*(short *)(tag + 0x2f8) == 4 ||
        ((*(char *)tag & 4) != 0 &&
         distance_squared3d((float *)(actor + 0x12c), (float *)(actor + 0xb0)) <
           *(float *)0x253f00)) {
      *(char *)(actor + 0x9c) = 1;
    }
  }

  if (*(char *)(actor + 0x504) == '\0') {
    *(int *)(actor + 0xc0) = *(int *)(actor + 0xc0) + 1;
    if (*(short *)(actor + 0xa4) == 0) {
      if (*(int *)(actor + 0xc0) > 29)
        FUN_00024be0(actor_handle, *(short *)(actor + 0x3b8), 0);
      goto LAB_timer_check;
    }
    result = (*(char *)(actor + 0xbc) == '\0');
  } else {
    *(int *)(actor + 0xc0) = 0;
  LAB_timer_check:
    if (*(short *)(actor + 0xa4) != 0)
      goto LAB_timer_check_done;
    if (*(int *)(actor + 0x270) != -1) {
      threat = (char *)datum_get(*(data_t **)0x5ab23c, *(int *)(actor + 0x270));
      done = (char)(*(short *)(threat + 0x32) > 0);
      if (done && *(short *)(actor + 0x268) < 5)
        result = 0;
      else
        result = 1;
    }
  }

LAB_timer_check_done:
  if (*(short *)(actor + 0x3b8) == -1) {
    *(char *)(actor + 0x9e) = 1;
    if (*(int *)(actor + 0xc8) > 0)
      *(int *)(actor + 0xc8) = *(int *)(actor + 0xc8) - 1;
    *(int *)(actor + 0xcc) = *(int *)(actor + 0xcc) + 1;
  } else {
    if (!result || (*(char *)(actor + 0x162) == '\0' && done == '\0' &&
                    *(char *)(actor + 0x504) == '\0')) {
      *(char *)(actor + 0x9e) = 1;
      if (*(int *)(actor + 0xc8) > 0)
        *(int *)(actor + 0xc8) = *(int *)(actor + 0xc8) - 1;
      *(int *)(actor + 0xcc) = *(int *)(actor + 0xcc) + 1;
    } else {
      *(int *)(actor + 0xc8) = *(int *)(actor + 0xc4);
    }
  }

  if (*(int *)(actor + 0xc8) == 0 || *(int *)(actor + 0xcc) > 359) {
    *(char *)(actor + 0x9d) = 1;
  } else {
    *(char *)(actor + 0x9d) = 0;
  }

  if (*(short *)(actor + 0xa4) == 1 && *(char *)(actor + 0xbc) != '\0') {
    *(char *)(actor + 0x9d) = 1;
  }

  if (*(char *)(actor + 0x9d) != '\0' && *(char *)0x5aca64 != '\0') {
    ai_debug_describe_actor(actor_handle, -1, 1, debug_buf, 0x100);
    if (*(int *)(actor + 0xc8) == 0) {
      csprintf(debug_msg, "timer %d finished", *(int *)(actor + 0xc4));
    } else if (*(int *)(actor + 0xcc) < 360) {
      if (*(short *)(actor + 0xa4) == 1 && *(char *)(actor + 0xbc) != '\0') {
        csprintf(debug_msg, "location inspected");
      } else {
        csprintf(debug_msg, "<unknown reason>");
      }
    } else {
      csprintf(debug_msg, "persistent timer %d", *(int *)(actor + 0xcc));
    }
    console_printf(2, "%s: %s uncover done: %s", debug_buf,
                   (*(short *)(actor + 0xa4) != 0) ? "pursuit" : "target",
                   debug_msg);
  }
}

/* FUN_0001aae0 (0x1aae0)
 * Get an object's bounding sphere (center and radius).
 *
 * Resolves the object via object_get_and_verify_type with type_mask=0xffffffff
 * (all types accepted).  Asserts that center and radius pointers are non-NULL.
 * Copies the three-float center position from object+0x50..0x58 and the scalar
 * radius from object+0x5c.
 *
 * Confirmed: PUSH -0x1; PUSH param_1; CALL 0x13d680
 * (object_get_and_verify_type). Confirmed: MOV ESI,[EBP+0xc] (center); TEST
 * ESI,ESI; JNZ ok; display_assert("center","..\\objects\\objects.h",0x217,1);
 * system_exit(-1). Confirmed: MOV EBX,[EBP+0x10] (radius); TEST EBX,EBX; JNZ
 * ok; display_assert("radius","..\\objects\\objects.h",0x218,1);
 * system_exit(-1). Confirmed: LEA ECX,[EDI+0x50]; MOV EDX,[ECX]; MOV [ESI],EDX;
 *   MOV EAX,[ECX+0x4]; MOV [ESI+0x4],EAX; MOV ECX,[ECX+0x8]; MOV [ESI+0x8],ECX;
 *   MOV EDX,[EDI+0x5c]; MOV [EBX],EDX. */
void FUN_0001aae0(int object_handle, float *center, float *radius)
{
  char *obj;
  obj = (char *)object_get_and_verify_type(object_handle, 0xffffffff);
  if (center == NULL) {
    display_assert("center", "..\\objects\\objects.h", 0x217, 1);
    system_exit(-1);
  }
  if (radius == NULL) {
    display_assert("radius", "..\\objects\\objects.h", 0x218, 1);
    system_exit(-1);
  }
  center[0] = *(float *)(obj + 0x50);
  center[1] = *(float *)(obj + 0x54);
  center[2] = *(float *)(obj + 0x58);
  *radius = *(float *)(obj + 0x5c);
}

/* FUN_0001ab70 (0x1ab70) — Initialize actor look-at snapshot.
 * Clears the look-at timer (actor+0xaa), records current game time at
 * actor+0xac, and copies the 3-float vector at actor+0x12c into actor+0xb0. */
void FUN_0001ab70(int actor_handle)
{
  char *actor;
  char *src;
  int t;
  int v;
  actor = (char *)datum_get(actor_data, actor_handle);
  *(short *)(actor + 0xaa) = 0;
  t = game_time_get();
  *(int *)(actor + 0xac) = t;
  src = actor + 0x12c;
  v = *(int *)src;
  actor += 0xb0;
  *(int *)actor = v;
  *(int *)(actor + 4) = *(int *)(src + 4);
  *(int *)(actor + 8) = *(int *)(src + 8);
}

/* FUN_0001abd0 (0x1abd0)
 * Clear actor look-at target: set the 32-bit field at actor+0xe4 to -1
 * (null/invalid handle sentinel).
 */
void FUN_0001abd0(int actor_handle)
{
  char *actor;
  actor = (char *)datum_get(actor_data, actor_handle);
  *(int *)(actor + 0xe4) = -1;
}

/* FUN_0001ac00 (0x1ac00)
 * Set up wander/idle look output block in actor record.
 * Selects move-type=3/nav=0 if mounted, or move-type=0 only if not. In flee
 * mode (actor+0xc8 set), sets move-type=4/nav=4 and copies flee target pos.
 * Clears look-suppression flags; sets look-speed=4.
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x1ac08.
 * Confirmed: CMP byte [EAX+0xc8],0; FUN_0002a3d0(actor_handle) for mount check.
 */
void FUN_0001ac00(int actor_handle)
{
  char *actor;
  int *src;
  int *dst;
  int v;
  actor = (char *)datum_get(actor_data, actor_handle);
  if (*(char *)(actor + 0xc8) != '\0') {
    v = 4;
    *(short *)(actor + 0x3e8) = (short)v;
    *(short *)(actor + 0x3ec) = (short)v;
    src = (int *)(actor + 0xd8);
    dst = (int *)(actor + 0x3f0);
    *dst = *src;
    dst[1] = src[1];
    dst[2] = src[2];
  } else {
    if (FUN_0002a3d0(actor_handle) != '\0') {
      *(short *)(actor + 0x3e8) = 3;
      *(short *)(actor + 0x3ec) = 0;
    } else {
      *(short *)(actor + 0x3e8) = 0;
    }
  }
  *(char *)(actor + 0x454) = 0;
  *(char *)(actor + 0x426) = 0;
  *(char *)(actor + 0x427) = 0;
  *(char *)(actor + 0x428) = 0;
  *(char *)(actor + 0x424) = 0;
  *(char *)(actor + 0x425) = 0;
  *(short *)(actor + 0x3fc) = 4;
}

/* FUN_00024ca0 (0x24ca0)
 * Check if a firing position index is in the actor's active set.
 *
 * Searches the actor's 4-entry firing position array at actor+0x3ca
 * (int16_t stride 4) for param_2. Returns 1 if found, 0 if not found or
 * if param_2 == -1.
 *
 * Confirmed: CMP SI,-1; JZ (short-circuit on NONE).
 * Confirmed: MOVSX EDI,CX; CMP SI,[EDX+EDI*4+0x3ca] loop with CX < 4. */
char FUN_00024ca0(int actor_handle, short param_2)
{
  char *actor;
  short i;

  actor = (char *)datum_get(actor_data, actor_handle);
  if (param_2 == -1) {
    return 0;
  }
  for (i = 0; i < 4; i++) {
    if (param_2 == *(short *)(actor + 0x3ca + (int)i * 4)) {
      return 1;
    }
  }
  return 0;
}

/* FUN_00025340 (0x25340) — Firing-position ideal-range scorer.
 *
 * Iterates over a firing-position array and scores each active position
 * based on its range relative to the context's ideal range. If the
 * context has a target, also applies weapon-optimal-range and
 * facing-alignment bonuses.
 *
 * Confirmed: cdecl, 4 stack args.
 * Confirmed: loop at 0x25340+0xd, count >= 0 check.
 * Confirmed: display_assert guards on eval thresholds. */
void FUN_00025340(int actor_handle, void *ctx, unsigned short count,
                  void *positions)
{
  char *pos;
  float eval;
  float range;
  unsigned int n;

  if (0 >= (short)count)
    return;

  n = (unsigned int)count;
  pos = (char *)positions + 8;
  do {
    if (*(char *)(pos + 10) != '\0') {
      eval = 0.0f;
      range = *(float *)((char *)ctx + 0x18) * 0.5f;
      if (range <= *(float *)pos) {
        if (*(float *)pos < *(float *)((char *)ctx + 0x18)) {
          eval = (2.0f / range) *
                 (*(float *)((char *)ctx + 0x18) - *(float *)pos) * 10.0f;
          if (eval < 0.0f || eval >= 1000.0f) {
            display_assert("(evaluation >= 0.0f) && (evaluation < 1e+03f)",
                           "c:\\halo\\SOURCE\\ai\\actor_firing_position.c",
                           0x81, 1);
            system_exit(-1);
          }
        }
      } else {
        eval = 5.0f;
      }
      *(float *)(pos + 0xc) = eval + *(float *)(pos + 0xc);

      if (*(char *)((char *)ctx + 0x5fc) != '\0') {
        if (*(float *)(pos + 0x10) < *(float *)0x254cd0) {
          range = (*(float *)0x254cd0 - *(float *)(pos + 0x10)) * 0.5f;
          if (range < 0.0f || range >= 1000.0f) {
            display_assert("(evaluation >= 0.0f) && (evaluation < 1e+03f)",
                           "c:\\halo\\SOURCE\\ai\\actor_firing_position.c",
                           0x81, 1);
            system_exit(-1);
          }
          *(float *)(pos + 0xc) = range + *(float *)(pos + 0xc);
        }
        if (*(char *)((char *)ctx + 0x43) != '\0' &&
            *(char *)((char *)ctx + 0x648) != '\0') {
          float dot;
          float score;
          dot = *(float *)(pos + 0x18) * *(float *)((char *)ctx + 0x64c) +
                *(float *)((char *)ctx + 0x650) * *(float *)(pos + 0x1c) +
                *(float *)(pos + 0x20) * *(float *)((char *)ctx + 0x654);
          if (dot <= 0.0f) {
            score = dot * (-1.0f);
            if (score < 0.0f)
              score = 0.0f;
            score = score * 10.0f;
          } else {
            score = 10.0f;
          }
          FUN_00024000(ctx, score, 3);
        }
      }
    }
    pos = pos + 0x3c;
    n = n - 1;
  } while (n != 0);
}

/* FUN_00025510 (0x25510) — Firing-position occlusion/long-range scorer.
 *
 * Iterates firing positions and scores each by its distance from the
 * context's desired range. Penalizes positions out of range
 * (too close to ideal, or ambiguous dot-product), and awards
 * 'in-view' bonuses. Clamps evaluations via display_assert guards.
 *
 * Confirmed: cdecl, 4 stack args.
 * Confirmed: loop structure at 0x25510, position stride 0x3c.
 * Confirmed: display_assert guards on evaluation. */
void FUN_00025510(int actor_handle, void *ctx, unsigned short count,
                  void *positions)
{
  char *pos;
  float eval;
  unsigned int n;

  if (0 >= (short)count)
    return;

  n = (unsigned int)count;
  pos = (char *)positions + 8;
  do {
    if (*(char *)(pos + 0x28) != '\0') {
      if (*(float *)0x2533d8 <= *(float *)pos) {
        eval = 0.0f;
        if (*(float *)0x253f78 <= *(float *)pos) {
          if (*(float *)pos < *(float *)((char *)ctx + 0x18)) {
            eval = ((*(float *)((char *)ctx + 0x18) - *(float *)pos) *
                    *(float *)0x253f78) /
                   (*(float *)((char *)ctx + 0x18) - *(float *)0x253f78);
            goto LAB_eval_guard;
          }
        } else {
          eval = *(float *)pos - *(float *)0x2533d8;
          eval = eval + eval;
        LAB_eval_guard:
          if (eval < 0.0f || eval >= 1000.0f) {
            display_assert("(evaluation >= 0.0f) && (evaluation < 1e+03f)",
                           "c:\\halo\\SOURCE\\ai\\actor_firing_position.c",
                           0x81, 1);
            system_exit(-1);
          }
        }
        *(float *)(pos + 0x30) = eval + *(float *)(pos + 0x30);
      } else {
        pos[0x29] = 1;
        if (*(char *)((char *)ctx + 0x14) == '\0') {
          *(char *)(pos + 0x28) = 0;
          goto LAB_next;
        }
      }
      if (*(char *)((char *)ctx + 0x5fc) != '\0') {
        if (*(float *)0x254e74 <= *(float *)(pos + 0x24) ||
            (*(char *)(pos + 0x29) = 1,
             *(char *)((char *)ctx + 0x14) != '\0')) {
          eval = 0.0f;
          if (*(float *)0x254e74 <= *(float *)(pos + 0x24) &&
              (eval = *(float *)0x253f34,
               *(float *)(pos + 0x24) < *(float *)0x254e70 &&
                 (eval =
                    (xbox_sqrtf(*(float *)(pos + 0x24)) - *(float *)0x2533d8) *
                    *(float *)0x254e6c,
                  (eval < 0.0f || eval >= 1000.0f)))) {
            display_assert("(evaluation >= 0.0f) && (evaluation < 1e+03f)",
                           "c:\\halo\\SOURCE\\ai\\actor_firing_position.c",
                           0x81, 1);
            system_exit(-1);
          }
          *(float *)(pos + 0x30) = eval + *(float *)(pos + 0x30);
          if ((*(float *)(pos + 0x14) < *(float *)0x2548fc &&
               0.0f < *(float *)((char *)ctx + 0x600) &&
               *(float *)((char *)ctx + 0x600) < *(float *)0x2548fc)) {
            eval = *(float *)0x2533c8 -
                   *(float *)(pos + 0x14) /
                     (*(float *)((char *)ctx + 0x600) * *(float *)0x2533f0);
            if (eval <= 0.5f ||
                !(pos[0x29] = 1, *(char *)((char *)ctx + 0x14) == '\0')) {
              float bonus;
              bonus = 0.0f;
              if (0.0f <= eval) {
                bonus = eval;
                if (*(float *)0x2533c8 < eval)
                  bonus = *(float *)0x2533c8;
              }
              FUN_00024000(
                ctx, (*(float *)0x2533c8 - bonus) * *(float *)0x253f78, 10);
            } else {
              *(char *)(pos + 0x28) = 0;
            }
          }
        } else {
          *(char *)(pos + 0x28) = 0;
        }
      }
    }
  LAB_next:
    pos = pos + 0x3c;
    n = n - 1;
  } while (n != 0);
}

/* FUN_000257a0 (0x257a0) — Firing-position evaluation per-position updater.
 *
 * Evaluates aim/LoS targeting for a single firing position within the
 * actor's firing-position evaluation. Two branches:
 *   1. mode==5 (burst check): checks state[+8] against a threshold,
 *      applies a short-duration offset-radius, and tests LOS.
 *   2. Other modes: determines type/position/facing args, estimates
 *      unit position, and tests LOS with varying parameters.
 *
 * Register args: @eax=actor_handle, @edi=state, @ecx=actor.
 * Calls datum_get, unit_estimate_position, ai_test_line_of_sight,
 * magnitude3d, display_assert, system_exit.
 *
 * Confirmed: PUSH at 0x257b0/0x257b1 → datum_get(actor_data, actor_handle).
 * Confirmed: CMP word [ESI+4],5 / JNZ at 0x257ea/0x257ee.
 * Confirmed: FCOMP [0x254640] / FNSTSW / TEST at 0x257f3/0x257fb.
 * Confirmed: 2x CALL 001a93e0 (unit_estimate_position) and 2x CALL 000416e0
 *   (ai_test_line_of_sight).
 * Confirmed: MOV word [EDI+6],AX at 0x2584d/0x25857/0x2595e. */
void FUN_000257a0(int actor_handle, void *state, char *actor)
{
  char *d;
  short mode;
  float diff[3];
  float len_sq;
  float *position_ptr;
  void *direction_ptr;
  int type;
  int flag;
  short los_result;
  void *state_pos;
  float offset_radius[3];
  float offset_radius2[3];

  position_ptr = NULL;
  direction_ptr = NULL;

  d = (char *)datum_get(actor_data, actor_handle);

  if (*(char *)(actor + 0x5fc) == '\0') {
    display_assert("has_target",
                   "c:\\halo\\SOURCE\\ai\\actor_firing_position.c", 0x33c, 1);
    system_exit(-1);
  }

  mode = *(short *)(actor + 4);

  if (mode == 5) {
    state_pos = *(void **)state;
    if (*(float *)((char *)state + 8) < *(float *)0x254640) {
      unit_estimate_position(*(int *)(d + 0x18), 1, (vector3_t *)state_pos,
                             NULL, NULL, (vector3_t *)offset_radius);
      los_result = (short)ai_test_line_of_sight(
        (float *)(d + 0x120), (int)*(uint16_t *)(d + 0x148), offset_radius,
        (int)*(uint16_t *)((char *)state_pos + 0xe), 0, 0, -1,
        (char)(*(int *)(d + 0x158) != -1));
      *(short *)((char *)state + 6) = los_result;
      return;
    }
    *(short *)((char *)state + 6) = 4;
    return;
  }

  flag = 0;
  direction_ptr = NULL;

  if (mode == 1 || mode == 2) {
    type = 2;
  } else if (*(char *)(actor + 0x5dc) != '\0') {
    state_pos = *(void **)state;
    diff[0] = *(float *)(actor + 0x610) - *(float *)state_pos;
    diff[1] = *(float *)(actor + 0x614) - *(float *)((char *)state_pos + 4);
    diff[2] = *(float *)(actor + 0x618) - *(float *)((char *)state_pos + 8);
    type = 3;
    direction_ptr = (void *)(actor + 0x5e0);
    len_sq = magnitude3d(diff);
    if (len_sq <= *(float *)0x2533c0) {
      position_ptr = (float *)(d + 0x174);
    } else {
      diff[2] = 0.0f;
      position_ptr = diff;
    }
  } else {
    type = 1;
    position_ptr = NULL;
  }

  state_pos = *(void **)state;
  unit_estimate_position(*(int *)(d + 0x18), type, (vector3_t *)state_pos,
                         (vector3_t *)position_ptr, (vector3_t *)direction_ptr,
                         (vector3_t *)offset_radius2);

  if (mode >= 1 && mode <= 3)
    flag = 1;

  los_result = (short)ai_test_line_of_sight(
    offset_radius2, (int)*(uint16_t *)((char *)state_pos + 0xe),
    (float *)(actor + 0x61c), (int)*(uint16_t *)(actor + 0x640), flag, 1,
    *(int *)(actor + 0x62c), (char)(*(int *)(d + 0x158) != -1));

  *(short *)((char *)state + 6) = los_result;
}

/* FUN_00025970 (0x25970) — Firing-position evaluation state updater.
 * Advances the per-position evaluation state machine.
 *
 * Confirmed: EAX=state_ptr@<eax>, ESI=actor_ptr, EBP+8=actor_handle.
 *   Calls FUN_00024850, FUN_000257a0, FUN_00024890.
 * Confirmed: state+0x30/0x31/0x34/0x38 accessed; actor+0x668/0x66a/0x66c
 *   debug counters. */
char FUN_00025970(void *state, int actor_handle, char *actor)
{
  char result;

  *(int *)((char *)state + 0x38) = 0;
  *(int *)((char *)state + 0x34) = 0;
  *(char *)((char *)state + 0x31) = 0;
  *(char *)((char *)state + 0x30) = 1;

  FUN_00024850(actor_handle, 1);

  if (*(char *)((char *)state + 0x30) != 0)
    *(short *)(actor + 0x668) = *(short *)(actor + 0x668) + 1;

  if (*(char *)((char *)state + 0x31) == 0)
    *(short *)(actor + 0x66a) = *(short *)(actor + 0x66a) + 1;

  if (*(char *)((char *)state + 0x30) != 0) {
    if (*(char *)(actor + 0x5fc) != 0)
      FUN_000257a0(actor_handle, state, actor);
    *(int *)((char *)state + 0x34) = *(int *)((char *)state + 0x38);
    result = FUN_00024890(actor_handle, state);
    *(char *)((char *)state + 0x30) = result;
    *(short *)(actor + 0x66c) = *(short *)(actor + 0x66c) + 1;
  }

  return *(char *)((char *)state + 0x30);
}

/* FUN_000272d0 (0x272d0)
 * Assign a firing position to an actor, evicting any previous occupant.
 *
 * If param_2 == -1: clears actor firing position via FUN_0002f1a0, sets
 * actor+0x3b8 = -1. Else: validates encounter, displaces any current
 * holder of the slot (param_4), sets actor+0x3b8 = param_2, updates the
 * platform prop if needed, and calls FUN_0005b370.
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x272e3.
 * Confirmed: assert on actor->meta.encounter_index != NONE at 0x27305.
 * Confirmed: FUN_0002d900 = attempt move to position. */
short FUN_000272d0(int actor_handle, short param_2, short param_3, int param_4,
                   unsigned int param_5, char param_6)
{
  char *actor;
  char *prev_actor;
  char cancel;

  actor = (char *)datum_get(actor_data, actor_handle);
  if ((short)param_2 == -1) {
    FUN_0002f1a0(actor_handle);
  } else {
    if (*(int *)(actor + 0x34) == -1) {
      display_assert("actor->meta.encounter_index != NONE",
                     "c:\\halo\\SOURCE\\ai\\actor_firing_position.c", 0x97b, 1);
      system_exit(-1);
    }
    if (*(short *)(actor + 0x3b8) != -1 &&
        *(short *)(actor + 0x3b8) != param_2) {
      FUN_00024be0(actor_handle, *(short *)(actor + 0x3b8), 1);
    }
    if (param_4 != -1) {
      prev_actor = (char *)datum_get(actor_data, param_4);
      if (actor_handle == param_4) {
        display_assert("actor_index != previous_owner",
                       "c:\\halo\\SOURCE\\ai\\actor_firing_position.c", 0x988,
                       1);
        system_exit(-1);
      }
      FUN_0002f1a0(param_4);
      *(short *)(prev_actor + 0x3b8) = -1;
    }
    if (*(short *)(actor + 0x3b8) != (short)param_2) {
      *(short *)(actor + 0x3b8) = (short)param_2;
      *(char *)(actor + 0x3ba) = (char)(param_6 == '\0');
      *(char *)(actor + 0x3bb) = 0;
      cancel = (char)actor_move_to_firing_position(
        actor_handle, param_2,
        (void *)(-(unsigned int)(param_6 != '\0') & param_5));
      if (cancel != '\0') {
        goto done;
      }
    } else {
      goto done;
    }
  }
  *(short *)(actor + 0x3b8) = -1;
done:
  if (*(int *)(actor + 0x34) != -1) {
    encounter_verify_firing_position_owner_actor_indices(
      *(int *)(actor + 0x34));
  }
  return *(short *)(actor + 0x3b8);
}

/* FUN_00027870 (0x27870)
 * Stop scripted look: log debug message and clear the scripted-look fields.
 *
 * If the scripted-look counter at actor+0x544 is > 0 AND the AI debug display
 * flag (0x5aca5d) is non-zero, describes the actor via ai_debug_describe_actor
 * and prints a console "look-stop" line.  Then clears actor+0x544, actor+0x546,
 * and actor+0x548 (scripted-look timer/type/state words).
 *
 * Confirmed: CMP word ptr [ESI+0x544],0x0 / JLE at 0x27889/0x27891 — signed.
 * Confirmed: MOV AL,[0x5aca5d] / TEST AL,AL / JZ at 0x27893/0x27898/0x2789a.
 * Confirmed: PUSH 0x100; PUSH 0x5ab100 (error_string_buffer); PUSH 0x0;
 *   PUSH -0x1; PUSH EDI; CALL 0x49ac0 at 0x2789c.
 * Confirmed: PUSH EAX (return ptr from ai_debug_describe_actor);
 *   PUSH 0x255144 ("%s: look-stop"); PUSH 0x0; CALL 0xff4d0 at 0x278b0.
 * Confirmed: MOV word [ESI+0x546],0x0; MOV word [ESI+0x544],0x0;
 *   MOV word [ESI+0x548],0x0 at 0x278c0-0x278d3.
 * Inferred: actor+0x544 = scripted-look state counter (int16_t).
 * Inferred: actor+0x546, actor+0x548 = scripted-look type/state words. */
void FUN_00027870(int actor_handle)
{
  char *actor;
  char *desc;
  actor = (char *)datum_get(actor_data, actor_handle);
  if (*(short *)(actor + 0x544) > 0 && *(char *)0x5aca5d != '\0') {
    desc =
      ai_debug_describe_actor(actor_handle, -1, 0, error_string_buffer, 0x100);
    console_printf(0, "%s: look-stop", desc);
  }
  *(short *)(actor + 0x546) = 0;
  *(short *)(actor + 0x544) = 0;
  *(short *)(actor + 0x548) = 0;
}

/* FUN_000278e0 (0x278e0)
 * Compute a prop's threat importance score for actor look targeting.
 *
 * Evaluates an actor-prop pair and returns a float score based on the
 * prop's combat state, awareness, visibility, vehicle ownership, and
 * facing status. Score modifiers stack additively from baseline.
 *
 * Confirmed: datum_get(actor_data, actor_handle) and datum_get(prop_data,
 * prop_handle). Confirmed: switch on prop[0x123] (1=half,2=identity,3=double
 * awareness modifier). */
float FUN_000278e0(int actor_handle, int prop_handle)
{
  char *actor;
  char *prop;
  float importance;
  float base;
  short combat;

  actor = (char *)datum_get(actor_data, actor_handle);
  prop = (char *)datum_get(*(data_t **)0x5ab23c, prop_handle);
  importance = *(float *)0x2533c0;
  combat = *(short *)(prop + 0x24);
  if (combat >= 2 && combat <= 3) {
    if (*(char *)(prop + 0x127) != '\0') {
      if (*(short *)(prop + 0x76) < 0xd2) {
        importance = *(float *)0x255154;
      } else {
        importance = *(float *)0x253524;
      }
    } else {
      if (*(char *)(prop + 0x60) != '\0') {
        importance = *(float *)0x253f40;
      } else {
        importance = *(float *)0x2533c8;
      }
    }
  } else {
    if (*(char *)(prop + 0x60) != '\0' && combat >= 4 && combat <= 5) {
      importance = *(float *)0x2533ec;
    }
  }
  if (*(int *)(prop + 0x18) == *(int *)(actor + 0x158) ||
      *(int *)(prop + 0x110) == *(int *)(actor + 0x158)) {
    importance = *(float *)0x2533c0;
  }
  if (*(int *)(prop + 0x110) != -1) {
    base = *(float *)0x2533ec;
  } else {
    base = *(float *)0x2533c8;
  }
  switch (*(char *)(prop + 0x123)) {
  case 1:
    importance = importance + base * *(float *)0x253398;
    break;
  case 2:
    importance = importance + base;
    break;
  case 3:
    importance = importance + base + base;
    break;
  default:
    break;
  }
  if (*(char *)(prop + 0x12f) != '\0') {
    importance = importance + base + base;
  }
  switch (*(char *)(prop + 0x121)) {
  case 1:
    importance = importance * *(float *)0x253f3c;
    break;
  case 3:
    return importance * *(float *)0x253524;
  case 4:
    return importance * *(float *)0x2549d4;
  default:
    break;
  }
  return importance;
}

/* FUN_00027a10 (0x27a10)
 * Select the actor tag's look-angle pair based on actor look-type.
 *
 * Returns a pointer to the look yaw/pitch pair in the actor tag:
 *   look_type == 2 → tag+0xf4
 *   look_type == 3 or 4 → tag+0x10c
 *   otherwise → tag+0xdc
 *
 * Confirmed: @eax register arg (no prologue; MOV ECX,[0x6325a4] first).
 * Confirmed: MOVSX ECX,[ESI+0x3fc]; CMP ECX,2; JZ/JLE/CMP 4/JG. */
int FUN_00027a10(int actor_handle)
{
  char *actor;
  char *tag;
  int look_type;

  actor = (char *)datum_get(actor_data, actor_handle);
  tag = (char *)tag_get(0x61637472, *(int *)(actor + 0x58));
  look_type = (int)*(short *)(actor + 0x3fc);
  if (look_type == 2) {
    return (int)(tag + 0xf4);
  }
  if (look_type <= 2 || look_type > 4) {
    return (int)(tag + 0xdc);
  }
  return (int)(tag + 0x10c);
}

/* FUN_00027dd0 (0x27dd0)
 * Returns true if the dot product of a normalized 2D direction vector
 * and a comparison direction exceeds a threshold.
 *
 * Confirmed: EAX=dir (float* 2D), EDX=vec2 (float* 2D), stack=threshold.
 *   Leaf function, no callee calls. FSQRT/FDIV/FMUL/FADDP/FCOMPP chain. */
bool FUN_00027dd0(float *dir, float *vec2, float threshold)
{
  float dx;
  float dy;
  float len;

  dx = dir[0];
  dy = dir[1];
  len = xbox_sqrtf(dx * dx + dy * dy);

  if (!(len >= 0.0001f))
    return 0;

  dx = (1.0f / len) * dx;
  dy = (1.0f / len) * dy;

  if (!(len > 0.0f))
    return 0;

  if (!(dx * vec2[0] + dy * vec2[1] > threshold))
    return 0;

  return 1;
}

/* FUN_00027e50 (0x27e50)
 * Vector normalization + dot/cross comparison against two thresholds.
 *
 * Confirmed: EAX=dir, ECX=vec2, EDX=limit, EBP+8=threshold,
 *   EBP+0xC=output.  FSQRT/FDIV/FMUL/FCOMPP + magnitude3d. */
bool FUN_00027e50(float *dir, float *vec2, float *limit, float threshold,
                  float *output)
{
  float dx;
  float dy;
  float len;
  float vx;
  float vy;
  float mag;
  float cross;
  float dot;
  int idx;

  dx = dir[0];
  dy = dir[1];
  vx = vec2[0];
  vy = vec2[1];

  len = xbox_sqrtf(dx * dx + dy * dy);

  if (xbox_fabsf(len) < 0.0001f)
    return 0;

  dx = (1.0f / len) * dx;
  dy = (1.0f / len) * dy;

  if (!(dx * limit[0] + dy * limit[1] > threshold))
    return 0;

  {
    float tmp[3];
    tmp[0] = vx;
    tmp[1] = vy;
    tmp[2] = dx;
    mag = magnitude3d(tmp);
  }
  if (!(mag > 0.0f))
    return 0;

  cross = vy * dx - vx * dy;
  idx = 0;
  if (cross > 0.0f)
    idx = 1;

  dot = dx * vx + dy * vy;
  if (!(dot > output[idx]))
    return 0;

  return 1;
}

/* FUN_00027f40 (0x27f40) — Actor look-at angle constraint evaluator.
 * Gets actor tag, computes pitch/yaw cosines based on combat state,
 * and invokes FUN_00027dd0/FUN_00027e50 to fill two output flags.
 *
 * Confirmed: EDI=actor, EBX=dir_ptr, out1/out2 at EBP+0x10/0x14. */
void FUN_00027f40(int actor_handle, void *dir_ptr, void *out1, void *out2)
{
  char *actor;
  char *tag_data;
  float cos_angles[2];

  actor = (char *)datum_get(actor_data, actor_handle);
  tag_data = (char *)tag_get(0x61637472, *(int *)(actor + 0x58));

  *((char *)out1) = (char)FUN_00027dd0(
    (float *)dir_ptr, (float *)(actor + 0x174), *(float *)(tag_data + 0x12c));

  if (*(short *)(actor + 0x6a) == 3) {
    cos_angles[0] = x87_fcos(*(float *)(tag_data + 0xbc));
    cos_angles[1] = *(float *)(tag_data + 0xc0);
  } else {
    cos_angles[0] = x87_fcos(*(float *)(tag_data + 0xb4));
    cos_angles[1] = *(float *)(tag_data + 0xb8);
  }
  cos_angles[1] = x87_fcos(cos_angles[1]);

  *((char *)out2) = (char)FUN_00027e50(
    (float *)dir_ptr, (float *)(actor + 0x180), (float *)(actor + 0x174),
    *(float *)(tag_data + 0x134), cos_angles);
}

/* FUN_00027ff0 (0x27ff0)
 * Evaluates firing positions for an actor's look-at target.
 * Iterates over firing positions via object iterator, scores each
 * by recency and distance, checks direction constraints via
 * FUN_00027dd0/FUN_00027e50, and returns the best candidate.
 *
 * Confirmed: param_1=actor_handle, param_2/param_3=direction flags,
 *   param_4=output struct (short result, int index), param_5=out_flag.
 *   Returns true if a suitable firing position was found. */
char FUN_00027ff0(int actor_handle, char param_2, char param_3, short *output,
                  char *out_flag)
{
  char *actor;
  char *tag_data;
  int current_tick;
  float tag_threshold;
  float tag_fov;
  float cos_yaw;
  float cos_pitch;
  int iter[2];
  char *item;
  float best_score;
  float item_score;
  int best_index;
  char *best_item;
  char best_flag;
  char item_flag;
  int item_timestamp;
  char check_result;
  (void)check_result;
  (void)cos_pitch;

  actor = (char *)datum_get(actor_data, actor_handle);
  tag_data = (char *)tag_get('actr', *(int *)(actor + 0x58));
  best_index = -1;
  best_score = 0.0f;
  best_item = 0;
  best_flag = 0;
  current_tick = game_time_get();
  tag_threshold = *(float *)(tag_data + 0x12c);
  tag_fov = *(float *)(tag_data + 0x134);

  if (*(short *)(actor + 0x6a) == 3) {
    cos_yaw = x87_fcos(*(float *)(tag_data + 0xbc));
    cos_pitch = x87_fcos(*(float *)(tag_data + 0xc0));
  } else {
    cos_yaw = x87_fcos(*(float *)(tag_data + 0xb4));
    cos_pitch = x87_fcos(*(float *)(tag_data + 0xb8));
  }

  if (actor_handle == *(int *)0x5ac9f8) {
    ai_debug_idle_look_clear(actor_handle);
  }

  FUN_00064540(iter, actor_handle);
  item = (char *)FUN_00064570(iter);

  if (item != 0) {
    do {
      item_flag = 0;
      item_score = 0.0f;

      if (*(short *)(item + 0x24) < 2 || *(short *)(item + 0x24) > 3 ||
          *(short *)(item + 0x32) == 0) {
        *(float *)(item + 0x58) = 0.0f;
      } else if (*(float *)(item + 0x54) > 0.0f) {
        item_timestamp = *(int *)(item + 0x5c);
        if (item_timestamp == -1) {
          item_score = 1.0f;
        } else {
          item_score =
            ((float)current_tick - (float)item_timestamp) * (1.0f / 600.0f) -
            1.0f;
        }
        item_score = (*(float *)(item + 0x54) - *(float *)(item + 0x58)) /
                       *(float *)(item + 0x54) +
                     item_score;
        if (item_score > 1.0f) {
          item_score = 1.0f;
        }
        item_score = item_score * *(float *)(item + 0x54);
        if (*(float *)(item + 0x54) <= *(float *)(item + 0x58)) {
          item_flag = 0;
        } else {
          item_flag = 1;
        }
      }

      if (actor_handle == *(int *)0x5ac9f8) {
        ai_debug_idle_look_addprop(iter[0], item_score);
      }

      if (item_score > 0.0f) {
        check_result = 0;
        if (param_3 == 0) {
          check_result =
            (char)FUN_00027e50((float *)(item + 0xe0), (float *)(actor + 0x5b0),
                               (float *)(actor + 0x5a4), tag_fov, &cos_yaw);
        } else if (param_2 == 0 || item_flag == 0) {
          check_result = (char)FUN_00027dd0(
            (float *)(item + 0xe0), (float *)(actor + 0x5a4), tag_threshold);
        }

        if (check_result != 0 && item_score > best_score) {
          best_index = iter[0];
          best_item = item;
          best_score = item_score;
          best_flag = item_flag;
        }
      }

      item = (char *)FUN_00064570(iter);
    } while (item != 0);

    if (best_index != -1) {
      *(int *)(best_item + 0x5c) = current_tick;
      *(float *)(best_item + 0x58) = *(float *)(best_item + 0x54);
      output[0] = 1;
      *(int *)(output + 2) = best_index;
      *out_flag = best_flag;
      return 1;
    }
  }

  return 0;
}

/* actor_look_update (0x29040)
 * Per-tick update of an actor's look/aim/facing output vectors.
 * Resolves primary/secondary look modes from the look-spec tables,
 * manages idle-major and idle-minor timers, applies snapping constraints,
 * and writes the final facing/aiming/looking vectors to actor output fields.
 *
 * Confirmed: all 13 FUN_00027dd0 call sites traced from disasm
 *   (EAX=dir, EDX=vec2 at each CALL); FUN_00027e50 takes 2 extra cdecl args
 *   (constrain_range, cos_limits); FUN_00028250/28cc0/28ed0 all take
 *   actor_handle via register (@<esi>, @<eax>, @<eax> respectively).
 *   primary_vec/secondary_vec/cos_angles are contiguous stack buffers
 *   passed by address to callees — lift-learnings #2 applies. */
void actor_look_update(int actor_handle)
{
  char *actor;
  int tag_data;
  float *desired_facing;
  float *desired_aiming;
  float *desired_looking;
  float primary_vec[3];
  float secondary_vec[3];
  float cos_angles[2];
  short look_spec_type;
  float constrain_range;
  int aim_threshold;
  int look_data_mode;
  int look_mode;
  int secondary_mode;
  char *actor_save;
  char is_attacking;
  char snap_flag;
  char use_aim;
  char has_primary;
  char want_secondary;
  char transient_aim;
  char in_range;
  char no_timing_window;
  char strict_look;
  char idle_major_active;
  short look_type;
  int look_data = 0;
  char *pfVar14;
  char cVar5;
  short sVar8;
  int iVar10;
  int iVar13;
  char cVar7;
  char bVar15;

  actor = (char *)datum_get(actor_data, actor_handle);
  actor_save = actor;
  tag_data = (int)tag_get(0x61637472, *(int *)(actor + 0x58));

  desired_facing = (float *)(actor + 0x5a4);
  desired_aiming = (float *)(actor + 0x5b0);
  desired_looking = (float *)(actor + 0x5bc);
  strict_look = 0;

  if (!valid_real_normal3d(desired_facing)) {
    display_assert("&actor->control.desired_facing_vector",
                   "c:\\halo\\SOURCE\\ai\\actor_looking.c", 0x44f, 1);
    system_exit(-1);
  }
  if (!valid_real_normal3d(desired_aiming)) {
    display_assert("&actor->control.desired_aiming_vector",
                   "c:\\halo\\SOURCE\\ai\\actor_looking.c", 0x450, 1);
    system_exit(-1);
  }
  if (!valid_real_normal3d(desired_looking)) {
    display_assert("&actor->control.desired_looking_vector",
                   "c:\\halo\\SOURCE\\ai\\actor_looking.c", 0x451, 1);
    system_exit(-1);
  }

  look_type = *(short *)(actor + 0x6dc);

  if (look_type == 1) {
    desired_aiming[0] = desired_facing[0];
    desired_aiming[1] = desired_facing[1];
    desired_aiming[2] = desired_facing[2];
    desired_looking[0] = desired_facing[0];
    desired_looking[1] = desired_facing[1];
    desired_looking[2] = desired_facing[2];
    pfVar14 = (char *)desired_aiming;
    goto LAB_00029e6d;
  }

  /* Determine is_attacking */
  if (*(char *)(actor + 0x161)) {
    is_attacking = 1;
  } else if (look_type == 0 || look_type == 2) {
    iVar10 = actor_attacking_target(actor_handle);
    is_attacking = (char)(iVar10 != -1);
  } else {
    is_attacking = 0;
  }

  snap_flag = *(char *)(actor + 0x58d);
  transient_aim = *(char *)(actor + 0x58e);
  use_aim = is_attacking;
  aim_threshold = *(int *)(tag_data + 0x12c);
  constrain_range = *(float *)(tag_data + 0x134);
  has_primary = 1;
  no_timing_window = 0;
  want_secondary = 0;

  /* Compute cos of aim angles */
  if (*(short *)(actor + 0x6a) == 3) {
    cos_angles[0] = x87_fcos(*(float *)(tag_data + 0xbc));
    cos_angles[1] = x87_fcos(*(float *)(tag_data + 0xc0));
  } else {
    cos_angles[0] = x87_fcos(*(float *)(tag_data + 0xb4));
    cos_angles[1] = x87_fcos(*(float *)(tag_data + 0xb8));
  }

  /* Determine primary look mode */
  if (FUN_000210b0(actor_handle) && !*(char *)(actor + 0x456)) {
    look_spec_type = 2;
    if (FUN_00028660(actor_handle, &look_spec_type, primary_vec)) {
      look_mode = 7;
      strict_look = 1;
    } else {
      goto LAB_look_mode_from_actor;
    }
  } else {
  LAB_look_mode_from_actor:
    look_mode = (int)(unsigned short)(*(unsigned short *)(actor + 0x3e8));
    if (look_mode != 0 && look_mode != 1) {
      if (FUN_00028660(actor_handle, (short *)(actor + 0x3ec), primary_vec)) {
        strict_look = (char)(*(short *)(actor + 0x3ec) == 2);
      } else {
        look_mode = 0;
      }
    }
  }

  /* Secondary look mode */
  secondary_mode = 0;
  if (*(short *)(actor + 0x544) >= 0 && *(short *)(actor + 0x548) > 0) {
    if (FUN_00028660(actor_handle, (short *)(actor + 0x54c), secondary_vec)) {
      secondary_mode = (int)(*(short *)(actor + 0x546));
    }
  }

  /* Panic overrides secondary_mode up to 5 */
  if (*(char *)(actor + 0x504)) {
    sVar8 = actor_action_try_to_panic(actor_handle);
    if (sVar8 == 2) {
      iVar10 = secondary_mode;
      secondary_mode = 5;
      if ((short)iVar10 < 6)
        secondary_mode = iVar10;
    }
  }

  /* Validate primary_vec and secondary_vec */
  if ((short)look_mode > 1 && !valid_real_normal3d(primary_vec)) {
    display_assert("&primary_vector", "c:\\halo\\SOURCE\\ai\\actor_looking.c",
                   0x4ca, 1);
    system_exit(-1);
  }
  if ((short)secondary_mode != 0 && !valid_real_normal3d(secondary_vec)) {
    display_assert("&secondary_vector", "c:\\halo\\SOURCE\\ai\\actor_looking.c",
                   0x4ce, 1);
    system_exit(-1);
  }

  /* Decrement look timer */
  if (*(short *)(actor + 0x548) > 0) {
    sVar8 = *(short *)(actor + 0x548) - 1;
    *(short *)(actor + 0x548) = sVar8;
    if (sVar8 == 0) {
      if (*(char *)0x5aca5d) {
        console_printf(0, "%s: look timer expire",
                       ai_debug_describe_actor(actor_handle, -1, 0,
                                               (char *)0x5ab100, 0x100));
      }
      *(short *)(actor + 0x544) = 0;
      *(short *)(actor + 0x546) = 0;
    }
  }

  *(char *)(actor + 0x58c) = 0;

  /* Apply primary_vec if look_mode >= 2 */
  if ((short)look_mode >= 2) {
    if ((short)look_mode < 5 ||
        (snap_flag == 0 &&
         !FUN_00027dd0(primary_vec, desired_facing, aim_threshold))) {
      if ((short)look_mode > 2 && transient_aim) {
        transient_aim = 0;
        snap_flag = 1;
        goto LAB_000294dc;
      }
    } else {
    LAB_000294dc:
      desired_aiming[0] = primary_vec[0];
      desired_aiming[1] = primary_vec[1];
      desired_aiming[2] = primary_vec[2];
      has_primary = 0;
      use_aim = is_attacking;
      if ((short)look_mode > 6 && !want_secondary && is_attacking) {
        want_secondary = 1;
        desired_looking[0] = primary_vec[0];
        desired_looking[1] = primary_vec[1];
        desired_looking[2] = primary_vec[2];
      }
    }
    if ((short)look_mode == 2)
      look_mode = (int)(-(char)(snap_flag != 0) & 5);
    if (snap_flag) {
      desired_facing[0] = primary_vec[0];
      desired_facing[1] = primary_vec[1];
      desired_facing[2] = primary_vec[2];
      snap_flag = 0;
      transient_aim = 0;
      *(char *)(actor + 0x591) |= (char)((short)look_mode == 4);
    }
    if ((*(char *)(actor + 0x58d) == 0 && *(char *)(actor + 0x58e) == 0) ||
        (no_timing_window = 0, (short)look_mode > 5)) {
      no_timing_window = 1;
    }
  }

  pfVar14 = (char *)desired_facing;

  /* Secondary mode switch */
  switch ((short)secondary_mode) {
  case 2:
  case 3:
  case 4:
  case 5:
  case 6:
    in_range = FUN_00027dd0(secondary_vec, desired_facing, aim_threshold);
    if (!no_timing_window) {
      if (want_secondary)
        goto LAB_000296af;
      if ((short)secondary_mode >= 6 && !actor_move_force_stop(actor_handle))
        goto LAB_0002961a;
      if ((short)secondary_mode >= 5 &&
          (transient_aim != 0 || *(char *)(actor + 0x58d) != 0))
        goto LAB_0002961a;
      if ((short)secondary_mode < 4)
        goto LAB_0002966b;
      if (in_range != 0)
        goto LAB_00029672;
      if (!snap_flag || !has_primary)
        goto LAB_000296af;
    LAB_0002961a:
      if (!*(char *)(actor + 0x591) || !in_range) {
        desired_facing[0] = secondary_vec[0];
        desired_facing[1] = secondary_vec[1];
        desired_facing[2] = secondary_vec[2];
        *(char *)(actor + 0x591) = 0;
      }
      desired_aiming[0] = secondary_vec[0];
      desired_aiming[1] = secondary_vec[1];
      desired_aiming[2] = secondary_vec[2];
      desired_looking[0] = secondary_vec[0];
      desired_looking[1] = secondary_vec[1];
      desired_looking[2] = secondary_vec[2];
      goto LAB_000297b4;
    }
    if (!want_secondary) {
    LAB_0002966b:
      if (!in_range)
        goto LAB_000296af;
    LAB_00029672:
      if ((short)secondary_mode < 5 &&
          ((short)secondary_mode < 3 || !has_primary))
        goto LAB_000296af;
      desired_aiming[0] = secondary_vec[0];
      desired_aiming[1] = secondary_vec[1];
      desired_aiming[2] = secondary_vec[2];
      desired_looking[0] = secondary_vec[0];
      desired_looking[1] = secondary_vec[1];
      desired_looking[2] = secondary_vec[2];
      use_aim = is_attacking;
      goto LAB_000297b8;
    }
  LAB_000296af:
    if (use_aim && FUN_00027e50(secondary_vec, desired_aiming, desired_facing,
                                constrain_range, cos_angles)) {
      desired_looking[0] = secondary_vec[0];
      desired_looking[1] = secondary_vec[1];
      desired_looking[2] = secondary_vec[2];
      use_aim = 0;
      goto LAB_000297c7;
    }
    if (has_primary && in_range) {
      desired_aiming[0] = secondary_vec[0];
      desired_aiming[1] = secondary_vec[1];
      desired_aiming[2] = secondary_vec[2];
      desired_looking[0] = secondary_vec[0];
      desired_looking[1] = secondary_vec[1];
      desired_looking[2] = secondary_vec[2];
      use_aim = 1;
      goto LAB_000297c3;
    }
    break;
  case 7:
  case 8:
    idle_major_active = (char)((short)secondary_mode == 8);
    no_timing_window = idle_major_active;
    if (!*(char *)(actor + 0x58d)) {
      if (!FUN_00027dd0(secondary_vec, desired_facing, aim_threshold)) {
        if (!actor_move_force_stop(actor_handle))
          goto LAB_000297c7;
        no_timing_window = 1;
        goto LAB_0002977e;
      }
      if (!idle_major_active)
        goto LAB_0002977e;
    }
  LAB_0002977e:
    desired_facing[0] = secondary_vec[0];
    desired_facing[1] = secondary_vec[1];
    desired_facing[2] = secondary_vec[2];
    *(char *)(actor + 0x591) = no_timing_window;
    desired_aiming[0] = secondary_vec[0];
    desired_aiming[1] = secondary_vec[1];
    desired_aiming[2] = secondary_vec[2];
    desired_looking[0] = secondary_vec[0];
    desired_looking[1] = secondary_vec[1];
    desired_looking[2] = secondary_vec[2];
  LAB_000297b4:
    snap_flag = 0;
    use_aim = is_attacking;
  LAB_000297b8:
    *(char *)(actor + 0x58c) = 1;
    want_secondary = 0;
    is_attacking = use_aim;
  LAB_000297c3:
    has_primary = 0;
    break;
  default:
    break;
  }
LAB_000297c7:

  /* look_mode==2 snap check */
  if ((short)look_mode == 2 && has_primary &&
      FUN_00027dd0(primary_vec, desired_facing, aim_threshold)) {
    desired_aiming[0] = primary_vec[0];
    desired_aiming[1] = primary_vec[1];
    desired_aiming[2] = primary_vec[2];
    if (use_aim) {
      desired_looking[0] = primary_vec[0];
      desired_looking[1] = primary_vec[1];
      desired_looking[2] = primary_vec[2];
    }
    *(char *)(actor + 0x58c) = 0;
    has_primary = 0;
  }

  /* Get look timing data */
  iVar13 = FUN_00027a10(actor_handle);
  bVar15 = (char)(*(float *)(iVar13 + 4) <= *(float *)0x2533c0);
  no_timing_window = !bVar15;
  in_range = (char)(*(float *)0x2533c0 < *(float *)(iVar13 + 0xc));
  transient_aim = (char)(*(float *)0x2533c0 < *(float *)(iVar13 + 0x14));
  look_data_mode = iVar13;

  /* Gate on look_timer */
  if (*(short *)(actor + 0x3fc) < 1 || want_secondary ||
      (!has_primary && !use_aim) || (bVar15 && !in_range && !transient_aim)) {
    *(char *)(actor + 0x55c) = 0;
    *(char *)(actor + 0x55e) = 0;
    goto LAB_00029da2;
  }

  idle_major_active = 0;
  want_secondary = 0;

  /* Force-snap flag for look_mode==1 */
  if (!bVar15 && snap_flag && (short)look_mode == 1 && *(int *)(actor + 0x560))
    look_mode = (look_mode & 0xffffff00) | 1;
  else
    look_mode = look_mode & 0xffffff00;

  if (*(int *)(actor + 0x560) > 0)
    *(int *)(actor + 0x560) -= 1;

  /* Idle major timer branch */
  if (!*(char *)(actor + 0x55c)) {
  LAB_00029971: {
    float *pfv;
    char uVar11;
    bVar15 = 1;
    look_data = (look_data & 0xffffff00) | 1;
    if (!has_primary || !in_range) {
      bVar15 = 0;
      look_data = look_data & 0xffffff00;
      if (!use_aim || !transient_aim)
        goto LAB_00029a12;
    } else if (!use_aim || !transient_aim) {
    LAB_00029998:
      pfv = bVar15 ? desired_facing : (float *)desired_aiming;
      uVar11 = (char)(use_aim && transient_aim ? 1 : 0);
      *(char *)(actor + 0x55e) =
        FUN_00028cc0((float *)iVar13, pfv, (char)look_mode,
                     (char)(int)look_data, uVar11, actor_handle);
      if (*(char *)(actor + 0x55c) && *(int *)(actor + 0x564) < 1) {
        display_assert("!actor->control.idle_major_active || "
                       "(actor->control.idle_major_timer > 0)",
                       "c:\\halo\\SOURCE\\ai\\actor_looking.c", 0x5dd, 1);
        system_exit(-1);
      }
      idle_major_active = 1;
      goto LAB_00029a12;
    } else {
      goto LAB_00029998;
    }
    if (use_aim && transient_aim)
      goto LAB_00029998;
  LAB_00029a12:;
  }
  } else {
    if (*(char *)(actor + 0x55d) && !has_primary) {
      *(char *)(actor + 0x55c) = 1;
      *(int *)(actor + 0x564) =
        FUN_00028250((float *)iVar13, 1, actor_handle, 2);
      *(short *)(actor + 0x56c) = 4;
      *(float *)(actor + 0x570) = desired_aiming[0];
      *(float *)(actor + 0x574) = desired_aiming[1];
      *(float *)(actor + 0x578) = desired_aiming[2];
      actor = actor_save;
    }
    if (!*(char *)(actor + 0x55c) || !*(int *)(actor + 0x564))
      goto LAB_00029971;
  }

  /* Idle major countdown */
  if (!*(char *)(actor + 0x55c))
    goto LAB_00029ccc;

  *(int *)(actor + 0x564) -= 1;

  if (!FUN_00028660(actor_handle, (short *)(actor + 0x56c), primary_vec))
    goto LAB_00029ccc;

  if (!valid_real_normal3d(primary_vec)) {
    display_assert("&idle_major_vector",
                   "c:\\halo\\SOURCE\\ai\\actor_looking.c", 0x5e8, 1);
    system_exit(-1);
  }

  if (!has_primary) {
    if (!FUN_00027e50(primary_vec, desired_aiming, desired_facing,
                      constrain_range, cos_angles))
      goto LAB_00029ccc;
    desired_looking[0] = primary_vec[0];
    desired_looking[1] = primary_vec[1];
    desired_looking[2] = primary_vec[2];
  } else {
    if (snap_flag && no_timing_window && *(char *)(actor + 0x99)) {
      look_mode = (look_mode & 0xffffff00) | 1;
      want_secondary = 1;
    }
    if ((char)look_mode ||
        (snap_flag && no_timing_window && *(char *)(actor + 0x99))) {
      desired_facing[0] = primary_vec[0];
      desired_facing[1] = primary_vec[1];
      desired_facing[2] = primary_vec[2];
      desired_aiming[0] = primary_vec[0];
      desired_aiming[1] = primary_vec[1];
      desired_aiming[2] = primary_vec[2];
      *(char *)(actor + 0x58c) = 1;
    } else {
      if (!FUN_00027dd0(primary_vec, desired_facing, aim_threshold))
        goto LAB_00029ccc;
      desired_aiming[0] = primary_vec[0];
      desired_aiming[1] = primary_vec[1];
      desired_aiming[2] = primary_vec[2];
      *(char *)(actor + 0x58c) = 1;
    }
  }
  goto LAB_00029b75;

LAB_00029ccc:
  primary_vec[0] = desired_aiming[0];
  primary_vec[1] = desired_aiming[1];
  primary_vec[2] = desired_aiming[2];
  *(char *)(actor + 0x55c) = 0;

LAB_00029b75:
  /* Idle minor timer */
  if (idle_major_active && transient_aim) {
    *(char *)(actor + 0x55f) = 1;
    *(int *)(actor + 0x568) = FUN_00028250(
      (float *)look_data_mode, *(char *)(actor + 0x55e), actor_handle, 2);
    *(int *)(actor + 0x57c) = *(int *)(actor + 0x56c);
    *(int *)(actor + 0x580) = *(int *)(actor + 0x570);
    *(int *)(actor + 0x584) = *(int *)(actor + 0x574);
    *(int *)(actor + 0x588) = *(int *)(actor + 0x578);
    if ((char)look_mode) {
      *(int *)(actor + 0x560) = FUN_00028250(
        (float *)look_data_mode, *(char *)(actor + 0x55e), actor_handle, 0);
    }
  }

  /* Idle minor active */
  if (!has_primary ||
      ((!use_aim || !transient_aim) && (!want_secondary || !in_range))) {
    *(char *)(actor + 0x55f) = 0;
  } else {
    if (!*(int *)(actor + 0x568)) {
      FUN_00028ed0((float *)look_data_mode, primary_vec, actor_handle);
      if (*(int *)(actor + 0x568) < 1) {
        display_assert("actor->control.idle_minor_timer > 0",
                       "c:\\halo\\SOURCE\\ai\\actor_looking.c", 0x62d, 1);
        system_exit(-1);
      }
    }
    *(int *)(actor + 0x568) -= 1;
    if (*(char *)(actor + 0x55f)) {
      cVar7 = FUN_00028660(actor_handle, (short *)(actor + 0x57c), primary_vec);
      cVar5 = want_secondary;
      if (cVar7) {
        if (!want_secondary)
          cVar7 = FUN_00027e50(primary_vec, desired_aiming, desired_facing,
                               constrain_range, cos_angles);
        else
          cVar7 = FUN_00027dd0(primary_vec, desired_facing, aim_threshold);
        if (cVar7) {
          if (!valid_real_normal3d(primary_vec)) {
            display_assert("&idle_minor_vector",
                           "c:\\halo\\SOURCE\\ai\\actor_looking.c", 0x645, 1);
            system_exit(-1);
          }
          if (cVar5) {
            desired_aiming[0] = primary_vec[0];
            desired_aiming[1] = primary_vec[1];
            desired_aiming[2] = primary_vec[2];
          }
          desired_looking[0] = primary_vec[0];
          desired_looking[1] = primary_vec[1];
          desired_looking[2] = primary_vec[2];
          pfVar14 = (char *)desired_aiming;
          goto LAB_00029dac;
        }
      }
    LAB_00029da2:
      *(char *)(actor + 0x55f) = 0;
      pfVar14 = (char *)desired_aiming;
    } else {
      goto LAB_00029dac;
    }
  }
  goto LAB_00029dac;

LAB_00029dac:
  /* Snap-to-original */
  if (!*(char *)(actor + 0x504) && !*(char *)(actor + 0x505) &&
      !unit_is_busy(*(int *)(actor + 0x18)) && *(int *)(actor + 0x158) == -1) {
    if (!FUN_00027dd0(desired_aiming, (float *)(actor + 0x174),
                      aim_threshold) ||
        !FUN_00027dd0(desired_aiming, desired_facing, aim_threshold)) {
      if (is_attacking) {
        if (FUN_00027e50(desired_looking, desired_aiming, desired_facing,
                         constrain_range, cos_angles) &&
            !FUN_00027e50(desired_looking, desired_aiming,
                          (float *)(actor + 0x174), constrain_range,
                          cos_angles))
          goto LAB_00029e51;
      }
      goto LAB_00029e58;
    }
  LAB_00029e51:
    *(char *)(actor + 0x591) = 1;
  }
  if (!is_attacking) {
  LAB_00029e58:
    desired_looking[0] = ((float *)pfVar14)[0];
    desired_looking[1] = ((float *)pfVar14)[1];
    desired_looking[2] = ((float *)pfVar14)[2];
  }

LAB_00029e6d:
  /* Zero pitch if needed */
  if (!*(char *)(actor + 0x99)) {
    float pitch_abs = *(float *)(actor + 0x5ac);
    if (pitch_abs < 0.0f)
      pitch_abs = -pitch_abs;
    if (*(double *)0x2533d0 <= (double)pitch_abs) {
      *(float *)(actor + 0x5ac) = 0.0f;
      if (magnitude3d(desired_facing) == *(float *)0x2533c0) {
        desired_facing[0] = *(float *)(actor + 0x174);
        desired_facing[1] = *(float *)(actor + 0x178);
        desired_facing[2] = *(float *)(actor + 0x17c);
      }
    }
    if (!valid_real_normal2d((float *)(actor + 0x6fc))) {
      display_assert("(real_vector2d *) &actor->output.facing_vector",
                     "c:\\halo\\SOURCE\\ai\\actor_looking.c", 0x689, 1);
      system_exit(-1);
    }
  }

  /* Snap-to-look-target */
  if (!*(char *)(actor + 0x58f)) {
    *(char *)(actor + 0x590) = 0;
    goto LAB_0002a0c3;
  }
  if (!*(char *)(actor + 0x590)) {
    if (!*(char *)(actor + 0x504)) {
      float dot = *(float *)(actor + 0x180) * ((float *)pfVar14)[0] +
                  *(float *)(actor + 0x184) * ((float *)pfVar14)[1] +
                  *(float *)(actor + 0x188) * ((float *)pfVar14)[2];
      if (dot > *(float *)0x2555d0) {
        *(float *)(actor + 0x598) = ((float *)pfVar14)[0];
        *(float *)(actor + 0x59c) = ((float *)pfVar14)[1];
        *(float *)(actor + 0x5a0) = ((float *)pfVar14)[2];
        *(char *)(actor + 0x590) = 1;
      }
    }
    goto LAB_0002a0c3;
  }
  {
    float snap_cos;
    float *snap_stored;
    float *pfv;
    snap_stored = (float *)(actor + 0x598);
    pfv = (float *)pfVar14;
    if (*(float *)(tag_data + 0x330) <= *(float *)0x2533c0)
      goto LAB_0002a0c3;
    snap_cos = x87_fcos(*(float *)(tag_data + 0x330));
    if (!*(char *)(actor + 0x99)) {
      float sv[2];
      float dv[2];
      float ss[2];
      sv[0] = pfv[0];
      sv[1] = pfv[1];
      dv[0] = desired_facing[0];
      dv[1] = desired_facing[1];
      ss[0] = snap_stored[0];
      ss[1] = snap_stored[1];
      if (magnitude3d(sv) == *(float *)0x2533c0)
        goto LAB_0002a0a7;
      if (magnitude3d(dv) == *(float *)0x2533c0)
        goto LAB_0002a0a7;
      if (magnitude3d(ss) == *(float *)0x2533c0)
        goto LAB_0002a0a7;
      if (ss[1] * sv[1] + ss[0] * sv[0] > snap_cos) {
        if (dv[1] * ss[1] + dv[0] * ss[0] <= snap_cos)
          goto LAB_0002a0c3;
      }
    } else {
      float dot3 = pfv[0] * snap_stored[0] + pfv[1] * snap_stored[1] +
                   pfv[2] * snap_stored[2];
      if (dot3 > snap_cos) {
        float dot4 = FUN_00013070(desired_facing, snap_stored);
        if (dot4 <= snap_cos)
          goto LAB_0002a0c3;
      }
    }
  LAB_0002a0a7:
    *(char *)(actor + 0x590) = 0;
    FUN_00036e50(actor_handle);
  }

LAB_0002a0c3:
  *(float *)(actor + 0x6fc) = ((float *)pfVar14)[0];
  *(float *)(actor + 0x700) = ((float *)pfVar14)[1];
  *(float *)(actor + 0x704) = ((float *)pfVar14)[2];
  *(float *)(actor + 0x708) = desired_aiming[0];
  *(float *)(actor + 0x70c) = desired_aiming[1];
  *(float *)(actor + 0x710) = desired_aiming[2];
  *(float *)(actor + 0x714) = desired_looking[0];
  *(float *)(actor + 0x718) = desired_looking[1];
  *(float *)(actor + 0x71c) = desired_looking[2];

  actor_unit_control_exact_facing(actor_handle, *(char *)(actor + 0x591));

  if (!valid_real_normal3d((float *)(actor + 0x6fc))) {
    display_assert("&actor->output.facing_vector",
                   "c:\\halo\\SOURCE\\ai\\actor_looking.c", 0x6c8, 1);
    system_exit(-1);
  }
  if (!valid_real_normal3d((float *)(actor + 0x708))) {
    display_assert("&actor->output.aiming_vector",
                   "c:\\halo\\SOURCE\\ai\\actor_looking.c", 0x6c9, 1);
    system_exit(-1);
  }
  if (!valid_real_normal3d((float *)(actor + 0x714))) {
    display_assert("&actor->output.looking_vector",
                   "c:\\halo\\SOURCE\\ai\\actor_looking.c", 0x6ca, 1);
    system_exit(-1);
  }

  /* Output flag */
  if (!strict_look && *(short *)(actor + 0x3fc) != 4) {
    switch (*(short *)(actor + 0x544)) {
    case 3:
    case 6:
    case 10:
    case 11:
    case 12:
      break;
    default:
      *(short *)(actor + 0x6f8) = 1;
      return;
    }
  }
  *(short *)(actor + 0x6f8) = 0;
}

/* FUN_0002a2b0 (0x2a2b0)
 * Update the actor's look-direction validity flag (actor+0x505).
 *
 * Reads the actor record and the look-specification slot at actor+0x3ec.
 * If the spec type word (actor+0x3ec) is 0 (no look command active) and the
 * actor is not in a vehicle (FUN_0002a3d0 returns 0), the look-priority word
 * at actor+0x3e8 is reset to 0.
 *
 * If the look-priority (actor+0x3e8) is > 2 AND a look spec is active
 * (actor+0x3ec != 0), FUN_00028660 is called with EBX=&actor[0x3ec] and
 * EDI=&actor[0x524] (the look-direction float[3] output buffer).  On success
 * actor+0x505 is set to 1; on failure or when no look spec is active it is
 * set to 0.
 *
 * Confirmed: cdecl, single stack arg (actor_handle).
 * Confirmed: datum_get(actor_data=DAT_006325a4, actor_handle) at 0x2a2c0.
 * Confirmed: EBX = &actor[0x3ec] via LEA EBX,[ESI+0x3ec] at 0x2a2c7.
 * Confirmed: CMP word [EBX],0x0 at 0x2a2d0; JNZ 0x2a2ec.
 * Confirmed: CALL FUN_0002a3d0(actor_handle) at 0x2a2d7; TEST AL,AL.
 * Confirmed: MOV word [ESI+0x3e8],0x0 at 0x2a2e3 when spec==0 && not mounted.
 * Confirmed: CMP word [ESI+0x3e8],0x3 (JL skip) at 0x2a2ec; CMP [EBX],0x0 (JZ
 * skip). Confirmed: LEA EDI,[ESI+0x524] at 0x2a2ff; PUSH actor_handle; CALL
 * 0x28660. Confirmed: FUN_00028660 register args: EBX=short*look_spec,
 * EDI=float*direction. Confirmed: MOV byte [ESI+0x505],0x1 on success at
 * 0x2a313; RET. Confirmed: MOV byte [ESI+0x505],0x0 on fallthrough at 0x2a31f;
 * RET. Inferred: actor+0x3e8 = look priority (int16_t); actor+0x3ec = look spec
 * type (int16_t). Inferred: actor+0x505 = look-direction valid flag (char).
 * Inferred: actor+0x524 = computed look direction (float[3]). */
void FUN_0002a2b0(int actor_handle)
{
  char *actor = (char *)datum_get(actor_data, actor_handle);
  short *look_spec = (short *)(actor + 0x3ec);

  if (*look_spec == 0) {
    if (!FUN_0002a3d0(actor_handle))
      *(short *)(actor + 0x3e8) = 0;
  }
  if (*(short *)(actor + 0x3e8) >= 3 && *look_spec != 0) {
    if (FUN_00028660(actor_handle, look_spec, (float *)(actor + 0x524))) {
      actor[0x505] = 1;
      return;
    }
  }
  actor[0x505] = 0;
}

/* FUN_0002a330 (0x2a330) — Set actor 'looking' active flags.
 * Sets the byte at actor+0x402 and actor+0x46e both to 1. */
void FUN_0002a330(int actor_handle)
{
  char *actor;
  actor = (char *)datum_get(actor_data, actor_handle);
  *(char *)(actor + 0x402) = 1;
  *(char *)(actor + 0x46e) = 1;
}

/* FUN_0002a3d0 (0x2a3d0)
 * Return the in-vehicle / mounted flag byte for the actor.
 *
 * Looks up the actor record via datum_get(actor_data, actor_handle) and
 * returns the byte at actor+0x4a8.  Non-zero means the actor is currently
 * mounted in or on a vehicle.
 *
 * Confirmed: cdecl, single stack arg (actor_handle).
 * Confirmed: datum_get(actor_data=DAT_006325a4, actor_handle) at 0x2a3de.
 * Confirmed: MOV AL,byte ptr [EAX+0x4a8] at 0x2a3e3; ADD ESP,0x8; RET. */
char FUN_0002a3d0(int actor_handle)
{
  char *actor = (char *)datum_get(actor_data, actor_handle);
  return actor[0x4a8];
}

/* FUN_0002a3f0 (0x2a3f0) — Check if actor can move (not on active path and
 * is_moving). Returns 0 if path_active (+0x4a8) is set AND is_moving (+0x484)
 * is clear, else 1. */
int FUN_0002a3f0(int actor_handle)
{
  char *actor;

  actor = (char *)datum_get(actor_data, actor_handle);
  if (*(char *)(actor + 0x4a8) != 0 && *(char *)(actor + 0x484) == 0)
    return 0;
  return 1;
}

/* FUN_0002a430 (0x2a430) — Get actor activation value if activation state is 3.
 * Returns actor[+0x470] (short) if actor[+0x46c] == 3, else -1. */
short FUN_0002a430(int actor_handle)
{
  char *actor;

  actor = (char *)datum_get(actor_data, actor_handle);
  if (*(short *)(actor + 0x46c) == 3)
    return *(short *)(actor + 0x470);
  return -1;
}
