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
  int enc_handle;

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

  if ((short)action_type == 5 || (short)action_type == 4)
    goto case_invade_retreat;
  if ((short)action_type != 2)
    goto case_default;

  /* action_type == 2: melee charge */
  if (*(char *)(actor + 0x6)) {
    return_flag = 0;
    *(int16_t *)(actor_state + 0x190) = 2;
    goto done;
  }
  /* Original zeroes return_flag unconditionally here (0x13fe4: MOV byte
   * [EBP-0x1],0 before the JNS) — the rest of the melee path is pessimistic:
   * fail reasons 3-6 all return 0; only reason 7 re-sets 1. */
  return_flag = 0;
  if (*(int8_t *)((char *)object_get_and_verify_type(*(int *)(actor + 0x18),
                                                     3) +
                  0xb6) < 0) {
    return_flag = 0;
    *(int16_t *)(actor_state + 0x190) = 3;
    goto done;
  }
  enc_handle = *(int *)(actor + 0x270);
  if (enc_handle == -1) {
    return_flag = 0;
    *(int16_t *)(actor_state + 0x190) = 4;
    goto done;
  }

  encounter = (char *)datum_get(*(data_t **)0x5ab23c, enc_handle);

  /* determine is_secondary (lunge vs normal melee) */
  if (*(float *)(actr_tag + 0x388) == *(float *)0x2533c0 ||
      *(float *)(actr_tag + 0x390) == *(float *)0x2533c0) {
    *(char *)((char *)charge_state + 0xa) = 0;
    is_secondary = 0;
  } else if (*(char *)(encounter + 0x130) == 0 &&
             *(int16_t *)(encounter + 0x9c) <= 0) {
    rand_val =
      random_math_real((unsigned int *)get_global_random_seed_address());
    if (rand_val < *(float *)(actr_tag + 0x390))
      is_secondary = 1;
    else
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
    if (!(int16_t)melee_tick_count) {
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
  goto done;

case_default:
  if ((short)action_type == 0 && (*(int *)actr_tag & 0x20000) &&
      *(int16_t *)(actor + 0x6e) >= 5 && !*(char *)(actor + 0x378)) {
    action_type = 1;
    *(int16_t *)(actor_state + 0x190) = 8;
  } else {
    *(int16_t *)(actor_state + 0x190) = 9;
  }
  goto done;

case_invade_retreat:
  return_flag = (char)(*(int16_t *)(actor + 0x15e) > 1);
  *(int16_t *)(actor_state + 0x190) = (int16_t)(return_flag == '\0');

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
  int16_t *move_type;
  int prop_handle_init;
  int prop_handle;

  actor = (char *)datum_get(actor_data, actor_handle);
  conversation = NULL;
  prop_handle_init = *(int *)(actor + 0xac);
  if (*(int *)(actor + 0x9c) != -1)
    conversation =
      (char *)datum_get(*(data_t **)0x6324ec, *(int *)(actor + 0x9c));
  prop_handle = prop_handle_init;
  if (prop_handle == -1 && conversation != NULL &&
      *(int *)(conversation + 0x10) != -1)
    prop_handle = prop_get_active_by_unit_index(actor_handle,
                                                *(int *)(conversation + 0x10));
  *(int16_t *)(actor + 0x3fc) = 1;
  if (prop_handle != -1) {
    move_type = (int16_t *)(actor + 0x3e8);
    *move_type = 3;
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

    if (actor_has_accessible_firing_position(
          actor_handle, (float *)(actor + 0x168), *(int *)(actor + 0x164), 0) !=
        '\0') {
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
  result = FUN_000272d0(actor_handle, (short)result, local_50, local_14,
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
  struct look_vec4 {
    int v0, v1, v2, v3;
  };
  char *actor;
  char *looking;

  actor = (char *)datum_get(actor_data, actor_handle);
  looking = actor + 0x9c;
  if (*(short *)(looking + 0xc) > 0) {
    *(struct look_vec4 *)param_2 = **(struct look_vec4 **)0x2ee6e0;
    return;
  }
  *(struct look_vec4 *)param_2 = **(struct look_vec4 **)0x2ee6d4;
}

/* FUN_00014c10 (0x14c10)
 * Firing-position state update for look/fight actor actions.
 *
 * Initialises state_data (zeroes it, fills the FP query buffer, queries
 * the best firing position via FUN_00025c10 + FUN_000272d0), then if a
 * valid FP and secondary target exist evaluates path accessibility via
 * the path_input / path_state pipeline.
 *
 * Register args:
 *   EBX = actor_handle
 *   ESI = state_data (output block, written throughout)
 * Stack arg:
 *   param_3 (int) DEAD: no [EBP+8] access in disasm.
 *
 * state_data field writes (all confirmed from disasm):
 *   +0x8  (short): fp_index; FUN_00025c10 ret written first (0x14d54),
 *                  then overwritten by FUN_000272d0 return (0x14d64)
 *   +0xa  (bool):  1 iff fp_index != -1 && (char)out3_int == 0 (0x14d7e)
 *   +0x20 (char):  0 always (0x14d81), conditionally path_state_approach_point
 *                  return (0x14e79)
 *   +0x6  (char):  always 0 at exit (0x14e7c)
 *
 * Stack-local layout (_chkstk EAX=0x28820 = 165920-byte frame):
 *   EBP-0x1    local_byte (char output from path_state_approach_point)
 *   EBP-0x5    out3_int (int written by FUN_00025c10 *out3; low byte used)
 *   EBP-0xc    tag_ptr
 *   EBP-0x10   actor_ptr
 *   EBP-0x14   out2 (int written by FUN_00025c10 *out2)
 *   EBP-0x50   fp_result_ptr (void* output from FUN_00025c10 *out1;
 *                used as struct ptr via [EBP-0x50]+0x14 accesses)
 *   EBP-0x98   path_buf (0x98 bytes)
 *   EBP-0x708  query_buf (0x670 bytes, csmemset confirmed)
 *   EBP-0x14794 path_state_buf (up to 0x146fc bytes)
 *   EBP-0x28820 huge_buf (0x1408c bytes raw FP candidate data)
 *
 * Confirmed: swarm assert action_flee.c:0x20a (522).
 * Confirmed: PUSH EDI at 0x14c22 is callee-save, not a datum_get arg.
 * Confirmed: datum_get(actor_data, actor_handle) global at 0x6325a4.
 * Confirmed: secondary target: datum_get(prop_data, state_data->0x1c).
 * Confirmed: fp_result_ptr is pointer output from FUN_00025c10; disasm
 *   0x14e2e MOV EAX,[EBP-0x50] then 0x14e31 MOV ECX,[EAX+0x14] proves it.
 * Confirmed: FUN_000272d0 param_3 = LEA [EBP-0x50] = &fp_result_ptr.
 * Confirmed: FUN_000272d0 param_5 = LEA [EBP-0x28820] = (uint)addr of huge_buf.
 */
void FUN_00014c10(int actor_handle, void *state_data, int param_3)
{
  char *actor;
  char *tag;
  char *sec_target;
  void *fp_result_ptr;
  short fp_index_tmp;
  short fp_index;
  int out2;
  int out3_int;
  char local_byte;
  int target_pos;
  unsigned char valid;
  static char path_buf[0x98];
  static char path_state_buf[0x146fc];
  static char huge_buf[0x1408c];
  char query_buf[0x670];
  short group_type;

  (void)param_3; /* DEAD: no [EBP+8] access in disasm */

  actor = (char *)datum_get(actor_data, actor_handle);
  tag = (char *)tag_get(0x61637472, *(int *)(actor + 0x58));

  /* assert(!actor->meta.swarm) action_flee.c:0x20a */
  if (*(char *)(actor + 0x6) != 0) {
    display_assert("!actor->meta.swarm", "c:\\halo\\SOURCE\\ai\\action_flee.c",
                   0x20a, 1);
    system_exit(-1);
  }

  csmemset(query_buf, 0, 0x670);

  /* query_buf[0x41] = state_data+4; set before branch (0x14c7d, 0x14c88) */
  query_buf[0x41] = *(char *)((char *)state_data + 0x4);

  if (*(short *)((char *)state_data + 0xc) > 0) {
    /* Swarm actor: group_type = 1 */
    group_type = 1;
    if (*(short *)((char *)state_data) > 0) {
      query_buf[0x14] = 1;
      query_buf[0x15] = 1;
    }
    query_buf[0x36] = 1;
    *(float *)(query_buf + 0x38) = 10.0f;
    *(float *)(query_buf + 0x3c) = 6.0f;
  } else {
    /* Normal actor: group_type = 2 */
    query_buf[0x8] = *(char *)((char *)state_data + 0x5);
    group_type = 2;
    /* FPU: tag->0x320 > 0.0f (FCOMP [0x2533c0]=0.0; TEST AH,0x41; JNZ=6.0) */
    if (*(float *)(tag + 0x320) > 0.0f) {
      *(float *)(query_buf + 0x1c) = *(float *)(tag + 0x320);
    } else {
      *(float *)(query_buf + 0x1c) = 6.0f;
    }
  }

  *(short *)(query_buf + 0x4) = group_type;

  /* Store group handle at query_buf+0 (0x14d24), search firing positions */
  *(int *)query_buf =
    actor_get_firing_position_group(actor_handle, (int)group_type, 0);

  /*
   * FUN_00025c10 returns a short (AX = fp candidate count/index).
   * Writes pointer to fp results into fp_result_ptr via *out1 (EBP-0x50).
   * Writes out2 int via *out2 (EBP-0x14).
   * Writes out3 int via *out3 (EBP-0x5); low byte = exclusion flag.
   */
  fp_index_tmp = (short)FUN_00025c10(
    actor_handle, query_buf, (int *)&fp_result_ptr, &out2, huge_buf, &out3_int);

  /* Temporary write (0x14d54); overwritten by FUN_000272d0 return */
  *(short *)((char *)state_data + 0x8) = fp_index_tmp;

  /*
   * FUN_000272d0:
   *   param_3 = &fp_result_ptr (address of pointer storage, void **)
   *   param_5 = (unsigned int) address of huge_buf (pointer as uint)
   */
  fp_index = FUN_000272d0(actor_handle, fp_index_tmp, &fp_result_ptr, out2,
                          (unsigned int)(int)huge_buf, (char)out3_int);
  *(short *)((char *)state_data + 0x8) = fp_index;

  /* valid = 1 iff fp_index != -1 AND out3 low byte == 0 */
  valid = (fp_index != (short)-1) && ((char)out3_int == 0) ? 1 : 0;
  *(char *)((char *)state_data + 0xa) = (char)valid;
  *(char *)((char *)state_data + 0x20) = 0;

  if (fp_index != (short)-1 && *(int *)((char *)state_data + 0x1c) != -1) {
    sec_target =
      (char *)datum_get(prop_data, *(int *)((char *)state_data + 0x1c));

    /* If sec_target type (field_0x24) is 2 or 3: find prop pathfinding loc */
    if (*(short *)(sec_target + 0x24) >= 2 &&
        *(short *)(sec_target + 0x24) <= 3) {
      actor_perception_find_prop_pathfinding_location(
        actor_handle, *(int *)((char *)state_data + 0x1c));
    }

    /* Resolve target pos: sec_target->0x110 if not -1, else ->0x18 */
    if (*(int *)(sec_target + 0x110) != -1) {
      target_pos = *(int *)(sec_target + 0x110);
    } else {
      target_pos = *(int *)(sec_target + 0x18);
    }

    /* Path accessibility pipeline for the firing position */
    path_input_new(path_buf, *(unsigned int *)(tag + 0x8c), 0, target_pos);
    path_input_set_start(path_buf, (float *)(sec_target + 0xf0),
                         *(int *)(sec_target + 0xec));
    paths_dispose(path_buf, *(int *)(actor + 0x18));
    path_state_new(path_buf, path_state_buf, 0);
    /* fp_result_ptr->field_0x14 = *(int*)((char*)fp_result_ptr + 0x14) */
    FUN_0005e0d0(path_state_buf, (float *)fp_result_ptr,
                 *(int *)((char *)fp_result_ptr + 0x14), 0);
    if (FUN_0005ff70((unsigned int *)path_state_buf)) {
      *(char *)((char *)state_data + 0x20) =
        path_state_approach_point(path_state_buf, (float *)fp_result_ptr,
                                  *(int *)((char *)fp_result_ptr + 0x14),
                                  &local_byte, (char *)state_data + 0x24);
    }
  }

  *(char *)((char *)state_data + 0x6) = 0;
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
 * may set a short timer. Otherwise dispatches FUN_00014c10(actor_handle,
 * state_data, 0) to find a firing position.
 *
 * Confirmed: standard cdecl, 7 stack params.
 * Confirmed: FUN_00014c10 takes actor_handle @<ebx> + state_data @<esi> +
 * 1 stack arg — original 0x1504c loads EBX=[EBP+8] (handle), 0x15071 loads
 * ESI=[EBP+0x20] (param_7 state_data, still live at the call), 0x15121
 * PUSH 0, 0x15123 CALL. Ghidra showed only FUN_00014c10(0) (register-arg
 * blindness); the missing args caused a datum_get assert (garbage EBX) and
 * then an AV at 0x14c7d reading [ESI+4] with leftover ESI=-1.
 * Confirmed: NEG CL; SBB ECX,ECX; AND ECX,0xb4 → condition ? 0xb4 : 0. */
char FUN_00015040(int actor_handle, short param_2, int param_3, char param_4,
                  char param_5, char param_6, short *param_7)
{
  char *actor;
  volatile char result;

  actor = (char *)datum_get(actor_data, actor_handle);
  result = 0;
  if (*(char *)(actor + 0x160) == '\0') {
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
      FUN_00014c10(actor_handle, param_7, 0);
      if (param_7[4] != (short)0xffff) {
        return 1;
      }
      *(char *)(param_7 + 7) = 0;
    }
  }
  return result;
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
 * Per-tick update for actor flee state (action_flee.c).
 * Manages flee target selection (firing positions), flee timer, look
 * animation state, and flee-related vocal/sound cues. Returns 1 if flee is
 * still active, 0 if done (neither flee_unable nor done_fleeing is set).
 *
 * State block layout (actor+0x9c base = ESI):
 *   +0x00 = look_type (int16_t)
 *   +0x02 = 9e = flee_ticks_remaining (int16_t)
 *   +0x04 = a0 (unused in this fn)
 *   +0x06 = a2 = find_new_position flag (char)
 *   +0x08 = a4 = current_firing_position_index (int16_t)
 *   +0x0a = a6 = firing_position_byte (char)
 *   +0x0c = a8 = look_anim_type (int16_t)
 *   +0x0e = aa = unable_to_flee (char)
 *   +0x0f = ab = done_fleeing (char)
 *   +0x10 = ac = flee_sound_played (char)
 *   +0x14 = b0 = last_update_tick (int)
 *   +0x1c = b8 = encounter_handle (int)
 *
 * Confirmed: EBX=actor_handle, EDI=actor*, ESI=actor+0x9c.
 * Confirmed: FUN_000153e0 @<ebx>=actor_handle (no stack args).
 * Confirmed: FUN_00014e90 @<eax>=actor_handle, stack=state_block_ptr.
 * Confirmed: FUN_00014c10(actor_handle @<ebx>, state_data @<esi>, int) —
 *   original 0x156c6: MOV EBX,[EBP+8]; PUSH 1; CALL 0x14c10, with ESI =
 *   state block (LEA ESI,[EDI+0x9c] at 0x1553f still live). 14c10 reads
 *   [ESI], [ESI+4], [ESI+0xc] before any ESI write (first use 0x14c7d).
 *   Ghidra missed both register args.
 * Confirmed: FUN_000300b0/302b0 cdecl 1 arg.
 * Confirmed: switch table at 0x15864; cases 9/10 share case 0xb/0xc bodies.
 * Confirmed: assert at action_flee.c:0x12e, 0x98. */
int FUN_00015520(int actor_handle)
{
  char *actor;
  char *state;
  short look_anim;
  short sVar;
  int encounter_handle;
  char *encounter;
  int cur_tick;
  int enc_val;
  int mode;

  actor = (char *)datum_get(actor_data, actor_handle);
  state = actor + 0x9c;

  if (*(char *)(actor + 0x6) == '\0') {
    /* Update look_type counter if we are in animated flee range [9,12] */
    look_anim = *(int16_t *)(state + 0xc);
    if (look_anim > 8 && look_anim < 0xd) {
      *(int16_t *)(state + 0x0) = 0xb4;
    }

    /* Manage firing position selection */
    if (*(int16_t *)(state + 0x2) < 1) {
      if (*(int16_t *)(state + 0x8) == -1) {
        /* No current target, need new one */
        *(char *)(state + 0x6) = 1;
      } else if (*(int16_t *)(actor + 0x3b8) == -1) {
        /* No firing positions available */
        *(int16_t *)(state + 0x8) = (short)0xffff;
        *(char *)(state + 0x6) = 1;
      } else {
        /* Check if current firing position is valid */
        if (FUN_000153e0(actor_handle)) {
          if (*(int16_t *)(actor + 0x3b8) == -1) {
            display_assert(
              "actor->firing_positions.current_position_index != NONE",
              "c:\\halo\\SOURCE\\ai\\action_flee.c", 0x98, 1);
            system_exit(-1);
          }
          if (*(int16_t *)(state + 0x0) == 0) {
            /* Adopt new firing position */
            *(int16_t *)(state + 0x8) = *(int16_t *)(actor + 0x3b8);
            *(char *)(state + 0xa) = *(char *)(actor + 0x3ba);
            *(char *)(state + 0xf) = 1;
            *(char *)(state + 0x6) = 0;
            /* Update encounter state if actor has encounter */
            encounter_handle = *(int *)(state + 0x1c);
            if (encounter_handle != -1) {
              encounter =
                (char *)datum_get(*(data_t **)0x5ab23c, encounter_handle);
              *(int16_t *)(encounter + 0x32) = 0;
              sVar = *(int16_t *)(encounter + 0x34);
              if (*(int16_t *)(encounter + 0x34) <=
                  *(int16_t *)(encounter + 0x36)) {
                sVar = *(int16_t *)(encounter + 0x36);
              }
              *(char *)(encounter + 0x74) = 0;
              *(int16_t *)(encounter + 0x30) = sVar;
              *(int16_t *)(encounter + 0x38) = 2;
              actor_situation_update_target_status(actor_handle);
              actor_situation_combat_status_update(actor_handle);
            }
          } else {
            *(char *)(state + 0x6) = 1;
          }
        }
      }
    } else {
      /* Flee timer expired */
      *(int16_t *)(state + 0x8) = (short)0xffff;
    }

    /* Switch on look animation type */
    look_anim = *(int16_t *)(state + 0xc);
    switch (look_anim) {
    case 9:
    case 10:
      if (*(int *)(actor + 0x1b0) == -1)
        *(char *)(state + 0xf) = 1;
      break;
    case 0xb:
      if (*(char *)(actor + 0x1b4) == '\0')
        *(char *)(state + 0xf) = 1;
      break;
    case 0xc:
      if (*(char *)(actor + 0x1b5) == '\0')
        *(char *)(state + 0xf) = 1;
      break;
    default:
      break;
    }

    /* Check if actor is actively fleeing and can find a position */
    if (*(char *)(actor + 0x4c) != '\0' && *(char *)(state + 0xf) == '\0') {
      if (*(int16_t *)(state + 0x8) != -1 && *(int16_t *)(state + 0x0) == 0 &&
          FUN_00014e90(actor_handle, state)) {
        *(int16_t *)(state + 0x8) = (short)0xffff;
        *(char *)(state + 0x6) = 1;
      }
      if (*(char *)(actor + 0x160) == '\0') {
        if (*(char *)(state + 0x6) == '\0' ||
            (FUN_00014c10(actor_handle, state, 1),
             *(int16_t *)(state + 0x8) != -1)) {
          goto skip_mark_unable;
        }
      } else {
        *(char *)(state + 0x6) = 0;
      }
      *(char *)(state + 0xe) = 1;
      *(int *)(actor + 0x398) = game_time_get();
    }
  }

skip_mark_unable:
  /* Clear flee_sound_played if still in flee anim range and unit not talking */
  look_anim = *(int16_t *)(state + 0xc);
  if (look_anim > 8 && look_anim < 0xd) {
    if (*(int *)(actor + 0x18) != -1) {
      if ((char)FUN_001a6bc0(*(int *)(actor + 0x18)) == 0) {
        *(char *)(state + 0x10) = 0;
      }
    }
  }

  /* Check if we should play flee sound/vocal */
  if (*(int16_t *)(state + 0xc) <= 0 || *(int16_t *)(state + 0x8) == -1 ||
      *(char *)(state + 0xe) != '\0' || *(int *)(actor + 0x18) == -1) {
    goto skip_flee_vocal;
  }
  cur_tick = game_time_get();
  if (*(char *)(state + 0x10) != '\0' &&
      *(int *)(state + 0x14) + 0x3c < cur_tick) {
    goto skip_flee_vocal;
  }

  /* Dispatch flee vocal/sound based on look_anim type */
  look_anim = *(int16_t *)(state + 0xc);
  if (look_anim == 0xc || look_anim == 0xb) {
    FUN_001a74d0(*(int *)(actor + 0x18), 2);
  } else if (look_anim == 9 || look_anim == 0xa) {
    FUN_001a74d0(*(int *)(actor + 0x18), 1);
  } else {
    /* Encounter-based flee sound */
    enc_val = -1;
    encounter_handle = *(int *)(state + 0x1c);
    if (encounter_handle != -1) {
      encounter = (char *)datum_get(*(data_t **)0x5ab23c, encounter_handle);
      enc_val = *(int *)(encounter + 0x18);
    }
    if (*(char *)(state + 0x10) == '\0') {
      mode = (look_anim == 8) ? 0x20 : 0x1f;
      FUN_00046f10(mode, *(int *)(actor + 0x18), enc_val, -1, -1, 4, 0);
      *(char *)(state + 0x10) = 1;
    } else {
      FUN_00046f10(0x21, *(int *)(actor + 0x18), enc_val, -1, -1, -1, 0);
    }
  }
  *(int *)(state + 0x14) = cur_tick;

skip_flee_vocal:
  /* Check if flee is complete or asserted */
  if (*(char *)(actor + 0x6) == '\0') {
    if ((*(char *)(actor + 0x4c) != '\0' || *(char *)(state + 0x6) == '\0') &&
        *(int16_t *)(state + 0x2) < 1 && *(int16_t *)(state + 0x8) == -1) {
      if (*(char *)(state + 0xe) != '\0') {
        return 1;
      }
      if (*(char *)(state + 0xf) == '\0') {
        display_assert(
          "(!actor->meta.timeslice && state_data->find_new_flee_position)"
          " || (state_data->flee_stationary_ticks > 0)"
          " || (state_data->flee_firing_position_index != NONE)"
          " || state_data->unable_to_flee || state_data->done_fleeing",
          "c:\\halo\\SOURCE\\ai\\action_flee.c", 0x12e, 1);
        system_exit(-1);
      }
    }
  }
  if (*(char *)(state + 0xe) == '\0' && *(char *)(state + 0xf) == '\0') {
    return 0;
  }
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
  prop_handle = *(int *)(actor + 0x1e8);
  if ((*(char *)(actor + 0x160) == '\0') && (*(char *)(actor + 6) == '\0') &&
      prop_handle != -1) {
    prop = (char *)datum_get(prop_data, prop_handle);
    *(int *)((char *)state_data + 0x3c) = *(int *)(actor + 0x1e8);
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
    actor_perception_find_prop_pathfinding_location(actor_handle,
                                                    *(int *)(actor + 0x1e8));
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
      param_2[1] = src[1];
      param_2[0] = src[0];
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
      cVar1 = actor_has_accessible_firing_position(
        actor_handle, (float *)(actor + 0x318), *(int *)(actor + 0x324), 1);
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
    result = FUN_000272d0(actor_handle, (short)ret_25c10, local_50, local_c,
                          (unsigned int)(int)huge_buf, (char)local_10);

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
  int *src;
  int *dst;
  char bVar2;
  int prop_unit;
  int look_flag;
  float fsq;
  float thresh;
  unsigned short sVar1;

  actor = (char *)datum_get(actor_data, actor_handle);
  tag_data = (char *)tag_get(0x61637472, *(int *)(actor + 0x58));
  if ((*tag_data & 0x40) != 0 && *(short *)(actor + 0x6e) == 0) {
    *(char *)(actor + 0x426) = 1;
    *(char *)(actor + 0x427) = 1;
  } else {
    *(char *)(actor + 0x427) = 0;
    if (*(char *)(actor + 0xa4) != '\0') {
      if (*(char *)(actor + 0xa6) != '\0') {
        *(char *)(actor + 0x426) =
          (char)((*(unsigned int *)tag_data >> 0x17) & 1);
      } else {
        *(char *)(actor + 0x426) = 1;
      }
    } else {
      if (*tag_data < 0 && *(short *)(actor + 0x6e) > 0) {
        look_flag = 1;
      } else {
        look_flag = 0;
      }
      *(char *)(actor + 0x426) = (char)look_flag;
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
    if (fsq < thresh * thresh) {
      FUN_0002f1a0(actor_handle);
    } else {
      actor_move_to_point(actor_handle, (float *)(actor + 0xc4),
                          *(int *)(actor + 0xd0), -1);
    }
    if (fsq < *(float *)0x2536cc) {
      bVar2 = 1;
    } else {
      bVar2 = 0;
    }
    break;
  case 3:
    sVar1 = *(unsigned short *)(actor + 0xc4);
    if (sVar1 != 0xffff) {
      *(short *)(actor + 0x3b8) = (short)sVar1;
      *(char *)(actor + 0x3ba) = 0;
      if (actor_move_to_firing_position(actor_handle, sVar1, 0) == '\0') {
        FUN_00024be0(actor_handle, *(unsigned short *)(actor + 0xc4), 0);
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
    *(float *)(actor + 0x460) =
      fwd[0] * *(float *)0x2533e8 + *(float *)(actor + 0xc4);
    *(float *)(actor + 0x464) =
      fwd[1] * *(float *)0x2533e8 + *(float *)(actor + 0xc8);
    *(float *)(actor + 0x468) =
      fwd[2] * *(float *)0x2533e8 + *(float *)(actor + 0xcc);
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
      src = (int *)(actor + 0xb4);
      dst = (int *)(actor + 0x3f0);
      dst[0] = src[0];
      dst[1] = src[1];
      dst[2] = src[2];
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

/* FUN_000169a0 (0x169a0)
 * Command-list step execution callback for actor scripted-look behavior.
 * Dispatches on the command type (*cmd_entry) to apply the appropriate
 * state change. Called with @<esi>=state_ptr (the actor's look state block).
 *
 * State block (ESI) layout:
 *   ESI[0]  = command_index (byte, current entry index in the command list)
 *   ESI[1]  = loop_count (byte, incremented on each loop)
 *   ESI[4]  = flags (byte: bit 0=?, bit 3=?, bit 4=?)
 *   ESI[5]  = flags2 (byte: bit 0=active?, bit 2=?)
 *   ESI[8:9] = int16 index
 *
 * Confirmed: ESI not in stack frame; reads [ESI],[ESI+1],[ESI+4],[ESI+5],
 *   [ESI+8] from raw disasm.
 * Confirmed: switch table at 0x16bac (byte lookup) + 0x16b8c (DWORD jumps).
 * Confirmed: assert strings "c:\\halo\\SOURCE\\ai\\action_obey.c". */
void FUN_000169a0(int actor_handle, int unit_handle, short scenario_idx,
                  int param_4, char *out_index, void *state_ptr /* @<esi> */)
{
  char *actor;
  char *scenario;
  char *cmd_entry;
  short cmd_type;
  int idx;
  char *obj;
  char local_buf[512];
  char *sp;

  actor = (char *)datum_get(actor_data, actor_handle);
  scenario = (char *)global_scenario_get();
  cmd_entry =
    (char *)tag_block_get_element(scenario + 0x438, (int)scenario_idx, 0x60);

  sp = (char *)state_ptr; /* ESI: state block */

  idx = (int)(unsigned char)sp[0];
  if (idx >= *(int *)(cmd_entry + 0x30)) {
    return;
  }
  cmd_entry = (char *)tag_block_get_element(cmd_entry + 0x30, idx, 0x20);

  cmd_type = *(short *)cmd_entry - 1;
  if ((unsigned short)cmd_type > 0x18) {
    return;
  }

  switch ((int)(unsigned char)cmd_type) {
  case 0: /* case 1 */
  case 1: /* case 2 */
    if (unit_handle == *(int *)(actor + 0x18)) {
      FUN_0002f1a0(actor_handle);
    }
    if (param_4 != 0) {
      *(char *)(param_4 + 4) = 0;
      *(char *)(param_4 + 0x18) = 0;
      return;
    }
    break;
  case 2: /* case 3 */
  case 0x15: /* case 0x16 */
    sp[5] = sp[5] & (char)0xfe;
    *(short *)(sp + 8) = (short)0xffff;
    return;
  case 3: /* case 4 */
  case 0x16: /* case 0x17 */
  case 0x17: /* case 0x18 */
  case 0x18: /* case 0x19 */
    if (unit_handle == *(int *)(actor + 0x18)) {
      FUN_00027870(actor_handle);
    }
    break;
  case 6: /* case 7 */
    if (param_4 != 0) {
      *(char *)(param_4 + 0x36) = 0;
      return;
    }
    break;
  case 9: /* case 10 */
  case 0xa: /* case 0xb */
    sp[5] = sp[5] & (char)0xfb;
    *(short *)(sp + 8) = 0;
    return;
  case 0xc: /* case 0xd */
    obj = (char *)object_try_and_get_and_verify_type(unit_handle, 1);
    if (obj != NULL) {
      *(unsigned int *)(obj + 0x424) &= 0xfffffff3;
      return;
    }
    break;
  case 0x13: /* case 0x14 */
    if (*(short *)(cmd_entry + 2) == 1) {
      char flags = sp[4];
      sp[4] = flags & (char)0xf7;
      if ((~(flags >> 3) & 1) == 0) {
        sp[4] = flags & (char)0xe7;
        return;
      }
      sp[4] = (flags & (char)0xf7) | (char)0x10;
    }
    if (*(short *)(cmd_entry + 0x16) == (short)(unsigned char)sp[0]) {
      ai_debug_describe_actor(actor_handle, -1, 1, local_buf, 0x200);
      error(2, "%s: command list %s entry #%d tried to loop to itself",
            (int)local_buf, (int)cmd_entry, (int)(unsigned char)sp[0]);
      return;
    }
    if ((unsigned char)sp[1] > 9) {
      ai_debug_describe_actor(actor_handle, -1, 1, local_buf, 0x200);
      error(2, "%s: command list %s is stuck looping (aborting on loop #%d)",
            (int)local_buf, (int)cmd_entry, (int)(unsigned char)sp[0]);
      return;
    }
    if (out_index != NULL)
      *out_index = *(char *)(cmd_entry + 0x16);
    sp[1]++;
    return;
  default:
    break;
  }
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
  if ((*(char *)(param_4 + 4) & 2) == 0) {
    FUN_000169a0(param_1, param_2, param_3, param_5,
                 (char *)((int)&param_4 + 3), (void *)param_4);
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

/* FUN_00017960 (0x17960) — Resolve look-direction vector into state_data.
 *
 * Writes a normalized look-direction vec3 to state_data+0xc..+0x14 based
 * on the look_type field at state_data+8.
 *
 * look_type 0: copy actor's current facing (actor+0x174..0x17c)
 * look_type 1: negate the actor's current facing
 * look_type 2/3: cross product of world forward vector (*(float**)0x31fc44)
 *   with the actor facing; if zero-length, fallback to object forward
 *   (object+0x30); if still zero, use world default (*(float**)0x31fc3c).
 *   look_type 2 stores positive, look_type 3 stores negative.
 *
 * Register args: ECX=state_data, EAX=actor_handle, EDI=object_handle.
 * Confirmed: ESI=state_data; actor+0x18=object_handle check. */
void FUN_00017960(char *state_data, int actor_handle, int object_handle)
{
  char *actor;
  float facing[3];
  float cross[3];
  char *obj;

  actor = (char *)datum_get(actor_data, actor_handle);
  if (object_handle == *(int *)(actor + 0x18)) {
    facing[0] = *(float *)(actor + 0x174);
    facing[1] = *(float *)(actor + 0x178);
    facing[2] = *(float *)(actor + 0x17c);
  } else {
    units_debug_get_closest_unit(object_handle, &facing[0]);
  }

  switch (*(short *)(state_data + 8)) {
  case 0:
    *(float *)(state_data + 0xc) = facing[0];
    *(float *)(state_data + 0x10) = facing[1];
    *(float *)(state_data + 0x14) = facing[2];
    return;
  case 1:
    *(float *)(state_data + 0xc) = -facing[0];
    *(float *)(state_data + 0x10) = -facing[1];
    *(float *)(state_data + 0x14) = -facing[2];
    return;
  case 2:
  case 3:
    cross_product3d(*(float **)0x31fc44, &facing[0], &cross[0]);
    if (normalize3d(&cross[0]) == 0.0f) {
      obj = (char *)object_get_and_verify_type(object_handle, 0x3);
      cross_product3d((float *)(obj + 0x30), &facing[0], &cross[0]);
      if (normalize3d(&cross[0]) == 0.0f) {
        cross[0] = *(float *)(*(int *)0x31fc3c);
        cross[1] = *(float *)(*(int *)0x31fc3c + 4);
        cross[2] = *(float *)(*(int *)0x31fc3c + 8);
      }
    }
    if (*(short *)(state_data + 8) == 2) {
      *(float *)(state_data + 0xc) = cross[0];
      *(float *)(state_data + 0x10) = cross[1];
      *(float *)(state_data + 0x14) = cross[2];
      return;
    }
    *(float *)(state_data + 0xc) = -cross[0];
    *(float *)(state_data + 0x10) = -cross[1];
    *(float *)(state_data + 0x14) = -cross[2];
    return;
  default:
    return;
  }
}

/* FUN_00017ab0 (0x17ab0) — Execute one command-list atom for an actor.
 *
 * Dispatches on the atom type (28 cases, jump table at 0x18adc) from the
 * scenario command list block (scenario+0x438, stride 0x60; atoms at
 * cmd_list+0x30, stride 0x20; points at cmd_list+0x3c, stride 0x14).
 * Returns 1 when the atom executed/queued successfully, 0 otherwise.
 * When the AI command-debug flag (0x5aca5b) is set, prints
 * "<encounter/squad>: <list> #<n>[ FAILED]: <describe>" via error().
 *
 * Confirmed: cdecl(actor_handle, scenario_idx, state_data) plus
 *   cmd_param@<eax>, unit_handle@<ecx>; frame SUB ESP,0x174.
 * Confirmed: misgrouped Ghidra calls are really
 *   tag_block_get_element(scenario+0x438, scenario_idx, 0x60),
 *   tag_block_get_element(scenario+0x450/0x444/0x45c, idx, stride) and
 *   FUN_00017120(scenario, atom, 0x5ab100, 0x100).
 * Confirmed: FUN_00017960(state_data@<ecx>, actor_handle@<eax>,
 *   unit_handle@<edi>) at 0x18113 (EDI still holds entry ECX).
 * Confirmed: qsort(near_list, count, 8, FUN_00016960) at 0x185ea.
 * Confirmed: case 0x10 reuses the state_data arg slot for the seat short
 *   and case 0x1b reuses the scenario_idx slot for the point element
 *   (separate locals here).
 * Confirmed: case 0x1b non-flying branch zeroes the actr-definition local
 *   (dead store kept for parity, decomp local_1c = 0). */
bool FUN_00017ab0(int actor_handle, short scenario_idx, char *state_data,
                  int cmd_param, int unit_handle)
{
  char *actor;
  char *actr_def;
  char *actv_def;
  char *cmd_list;
  short *atom;
  int *elem;
  int *elem2;
  char *obj;
  char *bipd;
  char *ent;
  char *enc;
  int bipd_null;
  char *sq;
  const char *status;
  int prop;
  short cmd_type;
  short mode;
  short mode2;
  short anim_idx;
  short count;
  short i;
  short seat_count;
  unsigned short umode;
  int atom_seq;
  int fp_index;
  int prop_handle;
  int object_index;
  int style;
  int anim_tag;
  int do_flag;
  int vmode;
  int seat_slot;
  int unit_out;
  int t;
  char loop_flag;
  char aim_flag;
  char flag;
  char result;
  int elem_z;
  float radius;
  float best;
  float d;
  int prop_iter[2];
  int obj_iter[4];
  data_iter_t iter;
  float pos[3];
  float near_list[32];
  char look_target[0x10];
  char comm[0x30];
  char name_buf[128];

  actor = (char *)datum_get(actor_data, actor_handle);
  actr_def = (char *)tag_get(0x61637472, *(int *)(actor + 0x58));
  actv_def = (char *)tag_get(0x61637476, *(int *)(actor + 0x5c));
  cmd_list = (char *)tag_block_get_element(
    (char *)global_scenario_get() + 0x438, (int)scenario_idx, 0x60);
  result = 0;
  if ((int)(unsigned char)*state_data >= *(int *)(cmd_list + 0x30)) {
    return 0;
  }
  atom = (short *)tag_block_get_element(cmd_list + 0x30,
                                        (int)(unsigned char)*state_data, 0x20);
  atom_seq = (int)(unsigned char)*state_data + 1;
  cmd_type = *atom;

  switch (cmd_type) {
  case 0:
    *(short *)(state_data + 2) =
      (short)(int)(*(float *)(atom + 2) * *(float *)0x253394);
    result = 1;
    break;

  case 1:
  case 2:
    if (cmd_param != 0 && (mode = atom[6], mode >= 0)) {
      if ((int)mode < *(int *)(cmd_list + 0x3c)) {
        elem = (int *)tag_block_get_element(cmd_list + 0x3c, (int)mode, 0x14);
        *(short *)(state_data + 2) = 0;
        *(char *)(cmd_param + 4) = 1;
        elem_z = elem[2];
        *(char *)(cmd_param + 5) = (atom[1] == 1);
        *(int *)(cmd_param + 8) = elem[0];
        *(int *)(cmd_param + 0xc) = elem[1];
        *(int *)(cmd_param + 0x10) = elem_z;
        *(int *)(cmd_param + 0x14) = elem[3];
        result = actor_move_to_point(actor_handle, (float *)(cmd_param + 8),
                                     elem[3], -1);
        if (result != 0) {
          if (*(char *)(cmd_param + 5) != 0) {
            FUN_0002a330(actor_handle);
          }
          if (*atom == 2 && (mode = atom[7], mode >= 0)) {
            if ((int)mode < *(int *)(cmd_list + 0x3c)) {
              elem =
                (int *)tag_block_get_element(cmd_list + 0x3c, (int)mode, 0x14);
              *(char *)(cmd_param + 0x18) = 1;
              *(int *)(cmd_param + 0x1c) = elem[0];
              *(int *)(cmd_param + 0x20) = elem[1];
              *(int *)(cmd_param + 0x24) = elem[2];
            }
          }
        }
      }
    }
    break;

  case 4:
  case 0x17:
  case 0x18:
  case 0x19:
    if (cmd_param == 0) {
      break;
    }
    radius = *(float *)(atom + 2);
    fp_index = -1;
    prop_handle = -1;
    object_index = -1;
    if (cmd_type == 4) {
      mode = atom[6];
      if (mode >= 0 && (int)mode < *(int *)(cmd_list + 0x3c)) {
        radius = *(float *)(atom + 2);
        fp_index = mode;
      }
    } else if (cmd_type == 0x17) {
      mode = atom[6];
      if (mode >= 0 && (int)mode < *(int *)(cmd_list + 0x3c)) {
        mode2 = atom[7];
        if (mode2 >= 0 && (int)mode2 < *(int *)(cmd_list + 0x3c)) {
          fp_index = FUN_00017940(mode, (short)(mode2 + 1));
          if (*(float *)(atom + 2) == *(float *)0x2533c0 &&
              *(float *)(atom + 4) == *(float *)0x2533c0) {
            radius = FUN_000121e0(*(float *)(actr_def + 0xec),
                                  *(float *)(actr_def + 0xf0));
          } else {
            radius = FUN_000121e0(*(float *)(atom + 2), *(float *)(atom + 4));
          }
        }
      }
    } else if (cmd_type == 0x18) {
      best = 3.4028235e+38f;
      FUN_00064540(prop_iter, actor_handle);
      prop = FUN_00064570(prop_iter);
      if (prop != 0) {
        do {
          if (*(short *)(prop + 0x24) >= 2 && *(short *)(prop + 0x24) <= 3 &&
              *(char *)(prop + 0x12e) != 0 && *(float *)(prop + 0x11c) < best) {
            best = *(float *)(prop + 0x11c);
            prop_handle = prop_iter[0];
          }
          prop = FUN_00064570(prop_iter);
        } while (prop != 0);
        if (prop_handle != -1) {
          goto LAB_look_merge;
        }
      }
      best = 3.4028235e+38f;
      data_iterator_new(&iter, player_data);
      ent = (char *)data_iterator_next(&iter);
      while (ent != 0) {
        if (*(int *)(ent + 0x34) != -1) {
          unit_get_head_position(*(int *)(ent + 0x34), pos);
          d = distance_squared3d(pos, (float *)(actor + 0x120));
          if (d < best) {
            object_index = *(int *)(ent + 0x34);
            best = d;
          }
        }
        ent = (char *)data_iterator_next(&iter);
      }
    } else { /* 0x19 */
      mode = atom[0xc];
      if (mode >= 0 &&
          (int)mode < *(int *)((char *)global_scenario_get() + 0x204)) {
        t = object_name_list_get_handle(mode);
        if (object_try_and_get_and_verify_type(t, 3) != 0) {
          prop_handle = prop_get_active_by_unit_index(actor_handle, t);
          object_index = t;
        }
      }
    }
  LAB_look_merge:
    if (radius > *(float *)0x2533c0 &&
        (prop_handle != -1 || object_index != -1 ||
         ((short)fp_index >= 0 &&
          (int)(short)fp_index < *(int *)(cmd_list + 0x3c)))) {
      mode = atom[1];
      style = 1;
      if (mode == 1) {
        style = 5;
      } else if (mode == 2) {
        style = 2;
      } else if (mode == 4) {
        style = 7;
      } else if (mode == 3) {
        style = 8;
      }
      if (prop_handle == -1) {
        if (object_index == -1) {
          elem = (int *)tag_block_get_element(cmd_list + 0x3c,
                                              (int)(short)fp_index, 0x14);
          *(short *)look_target = 3;
          *(int *)(look_target + 4) = elem[0];
          *(int *)(look_target + 8) = elem[1];
          *(int *)(look_target + 0xc) = elem[2];
        } else {
          *(short *)look_target = 3;
          unit_get_head_position(object_index, (float *)(look_target + 4));
        }
      } else {
        *(short *)look_target = 1;
        *(int *)(look_target + 4) = prop_handle;
      }
      FUN_00027a60(actor_handle, 0xd, (short)style, (short *)look_target);
      *(short *)(state_data + 2) = (short)(int)(radius * *(float *)0x253394);
      result = 1;
    }
    break;

  case 3:
    if (unit_handle == *(int *)(actor + 0x18)) {
      *(int *)(state_data + 0x18) = *(int *)(actor + 0x12c);
      *(int *)(state_data + 0x1c) = *(int *)(actor + 0x130);
      *(int *)(state_data + 0x20) = *(int *)(actor + 0x134);
    } else {
      object_get_world_position(unit_handle, (vector3_t *)(state_data + 0x18));
    }
    mode = atom[6];
    if (mode >= 0) {
      if ((int)mode >= *(int *)(cmd_list + 0x3c)) {
        goto LAB_move_angle;
      }
      elem = (int *)tag_block_get_element(cmd_list + 0x3c, (int)mode, 0x14);
      FUN_00012140((float *)(state_data + 0x18), (float *)elem,
                   (float *)(state_data + 0xc));
      if (!(normalize3d((float *)(state_data + 0xc)) > *(float *)0x2533c0)) {
        result = 0;
        break;
      }
    } else {
    LAB_move_angle:
      if (*(float *)(atom + 4) < *(float *)0x2533c0 ||
          *(float *)0x253d50 <= *(float *)(atom + 4)) {
        break;
      }
      vector3d_from_angle((float *)(state_data + 0xc),
                          *(float *)(atom + 4) * *(float *)0x253d4c);
    }
    mode = atom[1];
    result = 1;
    if (mode < 0 || mode > 3) {
      *(short *)(state_data + 8) = -1;
    } else {
      *(short *)(state_data + 8) = mode;
    }
    if (unit_handle == *(int *)(actor + 0x18)) {
      FUN_0002f1a0(actor_handle);
    }
    state_data[5] = (char)((state_data[5] & 0xfd) | 1);
    break;

  case 0x16:
    mode = atom[1];
    if (mode < 0 || mode > 3) {
      *(short *)(state_data + 8) = 0;
    } else {
      *(short *)(state_data + 8) = mode;
    }
    FUN_00017960(state_data, actor_handle, unit_handle);
    *(short *)(state_data + 2) =
      (short)(int)(*(float *)(atom + 2) * *(float *)0x253394);
    state_data[5] = state_data[5] | 3;
    result = 1;
    break;

  case 10:
    if (unit_handle == *(int *)(actor + 0x18) &&
        *(int *)(actor + 0x158) != -1) {
      break;
    }
    state_data[5] = (char)((state_data[5] & 0xe7) | 4);
    if (unit_handle == *(int *)(actor + 0x18)) {
      if (*(char *)(actor + 0x504) == 0) {
        flag = 0;
      } else {
        flag = (*(short *)(actor + 0x50a) == 0);
      }
    } else {
      obj = (char *)object_get_and_verify_type(unit_handle, 3);
      if (*(int *)(obj + 0xcc) == -1) {
        flag = (FUN_00013070((float *)(obj + 0x18), (float *)(obj + 0x24)) >
                *(float *)0x253d48);
      } else {
        flag = 0;
      }
    }
    *(short *)(state_data + 2) = 0x3c;
    *(unsigned short *)(state_data + 8) =
      (unsigned short)(((flag != 0) - 1) & 10);
    result = 1;
    break;

  case 0xb:
    if (unit_handle == *(int *)(actor + 0x18) &&
        *(int *)(actor + 0x158) != -1) {
      break;
    }
    state_data[5] = (char)((state_data[5] & 0xf7) | 0x14);
    *(short *)(state_data + 8) = 0;
    *(int *)(state_data + 0xc) = *(int *)(atom + 2);
    *(int *)(state_data + 0x10) = *(int *)(atom + 4);
    *(short *)(state_data + 2) = 0x3c;
    result = 1;
    break;

  case 5:
    if (cmd_param == 0 || (mode = atom[1], mode < 0) || mode > 3) {
      break;
    }
    *(short *)(cmd_param + 2) = mode;
    result = 1;
    break;

  case 6:
    if (cmd_param != 0) {
      result = 1;
      *(char *)cmd_param = (atom[1] == 1);
    }
    break;

  case 0x11:
    if (atom[1] == 0) {
      state_data[4] = state_data[4] | 1;
      result = 1;
    } else {
      state_data[4] = (char)(state_data[4] & 0xfe);
      result = 1;
    }
    break;

  case 0x12:
    if (unit_handle != *(int *)(actor + 0x18)) {
      break;
    }
    *(char *)(actor + 0x9e) = (atom[1] == 0);
    result = 1;
    break;

  case 0x13:
    result = 1;
    break;

  case 0x1a:
    if (cmd_param != 0 && *(float *)(atom + 2) > *(float *)0x2533c0) {
      *(char *)(cmd_param + 0x28) = 1;
      *(int *)(cmd_param + 0x2c) = *(int *)(atom + 2);
      result = 1;
    }
    break;

  case 0xf:
    if (cmd_param == 0) {
      break;
    }
    *(char *)(cmd_param + 0x30) = 0;
    switch (atom[1]) {
    case 0:
      *(char *)(cmd_param + 0x30) = 1;
      result = *(char *)(cmd_param + 0x30);
      *(short *)(cmd_param + 0x32) = 0;
      *(short *)(cmd_param + 0x34) = 0x2a;
      break;
    case 1:
      *(char *)(cmd_param + 0x30) = 1;
      result = *(char *)(cmd_param + 0x30);
      *(short *)(cmd_param + 0x32) = 4;
      *(short *)(cmd_param + 0x34) = 0x29;
      break;
    case 2:
      *(char *)(cmd_param + 0x30) = 1;
      result = *(char *)(cmd_param + 0x30);
      *(short *)(cmd_param + 0x32) = 5;
      *(short *)(cmd_param + 0x34) = 0x29;
      break;
    case 3:
      *(char *)(cmd_param + 0x30) = 1;
      result = *(char *)(cmd_param + 0x30);
      *(short *)(cmd_param + 0x32) = 6;
      *(short *)(cmd_param + 0x34) = -1;
      break;
    case 4:
      *(char *)(cmd_param + 0x30) = 1;
      result = *(char *)(cmd_param + 0x30);
      *(short *)(cmd_param + 0x32) = 7;
      *(short *)(cmd_param + 0x34) = -1;
      break;
    case 5:
      *(char *)(cmd_param + 0x30) = 1;
      result = *(char *)(cmd_param + 0x30);
      *(short *)(cmd_param + 0x32) = 8;
      *(short *)(cmd_param + 0x34) = 0x2c;
      break;
    case 6:
      *(char *)(cmd_param + 0x30) = 1;
      result = *(char *)(cmd_param + 0x30);
      *(short *)(cmd_param + 0x32) = 9;
      *(short *)(cmd_param + 0x34) = 0x2c;
      break;
    case 7:
      *(char *)(cmd_param + 0x30) = 1;
      result = *(char *)(cmd_param + 0x30);
      *(short *)(cmd_param + 0x32) = 10;
      *(short *)(cmd_param + 0x34) = 0x2c;
      break;
    case 8:
      *(char *)(cmd_param + 0x30) = 1;
      result = *(char *)(cmd_param + 0x30);
      *(short *)(cmd_param + 0x32) = 0xb;
      *(short *)(cmd_param + 0x34) = 0x2c;
      break;
    case 9:
      *(short *)(cmd_param + 0x34) = 0x26;
      goto LAB_cue_common;
    case 10:
      *(short *)(cmd_param + 0x34) = 0x27;
    LAB_cue_common:
      *(short *)(cmd_param + 0x32) = -1;
      *(char *)(cmd_param + 0x30) = 1;
      /* FALLTHROUGH */
    default:
      result = *(char *)(cmd_param + 0x30);
      break;
    }
    break;

  case 7:
    if (cmd_param != 0 && (mode = atom[6], mode >= 0)) {
      if ((int)mode < *(int *)(cmd_list + 0x3c)) {
        elem = (int *)tag_block_get_element(cmd_list + 0x3c, (int)mode, 0x14);
        *(char *)(cmd_param + 0x36) = 1;
        *(int *)(cmd_param + 0x38) = elem[0];
        *(int *)(cmd_param + 0x3c) = elem[1];
        *(int *)(cmd_param + 0x40) = elem[2];
        *(int *)(cmd_param + 0x44) = *(int *)(atom + 2);
        result = 1;
      }
    }
    break;

  case 8:
    if (cmd_param == 0 || *(short *)(actv_def + 0x180) == -1 ||
        (mode = atom[6], mode < 0)) {
      break;
    }
    if ((int)mode >= *(int *)(cmd_list + 0x3c)) {
      break;
    }
    elem = (int *)tag_block_get_element(cmd_list + 0x3c, (int)mode, 0x14);
    unit_set_grenade_count(*(int *)(actor + 0x18),
                           *(unsigned short *)(actv_def + 0x180), 1);
    *(char *)(cmd_param + 0x49) = 0;
    *(char *)(cmd_param + 0x48) = 0;
    *(int *)(cmd_param + 0x4c) = elem[0];
    *(int *)(cmd_param + 0x50) = elem[1];
    *(int *)(cmd_param + 0x54) = elem[2];
    *(short *)(cmd_param + 0x4a) = 0;
    mode = atom[1];
    if (mode >= 0 && mode < 3) {
      *(short *)(cmd_param + 0x4a) = mode;
    }
    *(short *)(state_data + 2) = 0x3c;
    result = 1;
    break;

  case 9:
    if (unit_handle == *(int *)(actor + 0x18)) {
      count = 0;
      vmode = -1;
      object_iterator_new(obj_iter, 2, 0);
      if (object_iterator_next(obj_iter) != 0) {
        do {
          object_get_world_position(obj_iter[2], (vector3_t *)pos);
          d = distance_squared3d((float *)(actor + 0x12c), pos);
          if (*(float *)(atom + 2) == *(float *)0x2533c0 ||
              d < *(float *)(atom + 2) * *(float *)(atom + 2)) {
            near_list[count * 2] = d;
            *(int *)&near_list[count * 2 + 1] = obj_iter[2];
            count = (short)(count + 1);
            if (count >= 16) {
              break;
            }
          }
        } while (object_iterator_next(obj_iter) != 0);
        if (count > 1) {
          qsort(near_list, (int)count, 8,
                (int (*)(const void *, const void *))FUN_00016960);
        }
      }
      umode = *(unsigned short *)(atom + 1);
      if ((short)umode >= 0 && (short)umode < 5) {
        vmode = umode;
      }
      i = 0;
      if (count > 0) {
        do {
          if (actor_action_try_to_enter_vehicle(actor_handle,
                                                *(int *)&near_list[i * 2 + 1],
                                                (int)"", vmode, 0, 0) != 0) {
            state_data[4] = state_data[4] | 4;
            result = 1;
            goto LAB_done;
          }
          i = (short)(i + 1);
        } while (i < count);
      }
    }
    break;

  case 0xd:
    mode = atom[8];
    if (mode == -1) {
      break;
    }
    elem = (int *)tag_block_get_element((char *)global_scenario_get() + 0x444,
                                        (int)mode, 0x3c);
    anim_tag = elem[0xb];
    loop_flag = 0;
    aim_flag = 0;
    do_flag = 1;
    if (anim_tag == -1) {
      obj = (char *)object_get_and_verify_type(unit_handle, 3);
      anim_tag = *(int *)((char *)tag_get(0x756e6974, *(int *)obj) + 0x44);
    }
    switch (atom[1]) {
    case 1:
      loop_flag = 1;
      break;
    case 2:
      aim_flag = 1;
      loop_flag = 1;
      break;
    case 3:
      do_flag = 0;
      break;
    case 4:
      do_flag = 0;
      loop_flag = 1;
      break;
    case 5:
      do_flag = 0;
      aim_flag = 1;
      loop_flag = 1;
      break;
    default:
      break;
    }
    if (FUN_001ac180(unit_handle, anim_tag, elem, do_flag) == 0) {
      break;
    }
    obj = (char *)object_try_and_get_and_verify_type(unit_handle, 1);
    if (obj != 0) {
      if (loop_flag != 0) {
        *(unsigned int *)(obj + 0x424) = *(unsigned int *)(obj + 0x424) | 4;
      } else {
        *(unsigned int *)(obj + 0x424) =
          *(unsigned int *)(obj + 0x424) & 0xfffffffb;
      }
      if (aim_flag != 0) {
        *(unsigned int *)(obj + 0x424) = *(unsigned int *)(obj + 0x424) | 8;
      } else {
        *(unsigned int *)(obj + 0x424) =
          *(unsigned int *)(obj + 0x424) & 0xfffffff7;
      }
    }
    result = 1;
    break;

  case 0xe:
    if (atom[10] >= 0) {
      mode = atom[10];
      if ((int)mode < *(int *)((char *)global_scenario_get() + 0x45c)) {
        anim_idx = FUN_000936b0(
          global_scenario_get(),
          tag_block_get_element((char *)global_scenario_get() + 0x45c,
                                (int)mode, 0x28));
        if (anim_idx != -1) {
          result = recorded_animation_play(unit_handle, anim_idx);
        }
      }
    }
    break;

  case 0xc:
    if (atom[9] >= 0) {
      mode = atom[9];
      if ((int)mode < *(int *)((char *)global_scenario_get() + 0x450)) {
        result = hs_wake_by_name(tag_block_get_element(
          (char *)global_scenario_get() + 0x450, (int)mode, 0x28));
      }
    }
    break;

  case 0x14:
    mode = atom[0xb];
    if (mode < 0 || (int)mode >= *(int *)(cmd_list + 0x30) ||
        (int)mode == (int)(unsigned char)*state_data) {
      break;
    }
    result = 1;
    break;

  case 0x15:
    obj = (char *)object_get_and_verify_type(unit_handle, 3);
    if (atom[1] == 1) {
      *(unsigned char *)(obj + 0xb6) = *(unsigned char *)(obj + 0xb6) | 0x40;
      result = 1;
    } else {
      *(unsigned char *)(obj + 0xb6) = *(unsigned char *)(obj + 0xb6) | 0x20;
      result = 1;
    }
    break;

  case 0x10:
    seat_slot = *(unsigned short *)(atom + 1);
    unit_out = -1;
    seat_count =
      FUN_001a68d0(unit_handle, 6, 1, 1, 0, (short *)&seat_slot, &unit_out);
    if (seat_count < 1) {
      break;
    }
    csmemset(comm, 0, 0x30);
    *(short *)(comm + 2) = (short)seat_slot;
    *(int *)(comm + 4) = unit_out;
    *(short *)comm = 6;
    ai_communication_packet_new(comm + 0x10);
    FUN_001a6ef0(unit_handle, seat_count, comm);
    result = 1;
    break;

  case 0x1b:
    mode = atom[6];
    if (mode < 0) {
      break;
    }
    if ((int)mode >= *(int *)(cmd_list + 0x3c)) {
      break;
    }
    elem = (int *)tag_block_get_element(cmd_list + 0x3c, (int)mode, 0x14);
    units_debug_get_closest_unit(unit_handle, pos);
    mode = atom[7];
    if (mode >= 0 && (int)mode < *(int *)(cmd_list + 0x3c)) {
      elem2 = (int *)tag_block_get_element(cmd_list + 0x3c, (int)mode, 0x14);
      obj = (char *)object_try_and_get_and_verify_type(unit_handle, 1);
      if (obj == 0) {
        bipd = 0;
      } else {
        bipd = (char *)tag_get(0x62697064, *(int *)obj);
      }
      bipd_null = (bipd == 0);
      FUN_00012140((float *)elem, (float *)elem2, pos);
      if (bipd_null || (*(unsigned char *)(bipd + 0x2f4) & 0x44) == 0) {
        actr_def = 0;
        if (magnitude3d(pos) == *(float *)0x2533c0) {
          goto LAB_teleport_face;
        }
      } else {
        if (normalize3d(pos) == *(float *)0x2533c0) {
        LAB_teleport_face:
          units_debug_get_closest_unit(unit_handle, pos);
        }
      }
    }
    object_set_position(unit_handle, (float *)elem, pos, 0);
    object_reset(unit_handle);
    object_update_children_recursive(unit_handle);
    if (unit_handle == *(int *)(actor + 0x18)) {
      FUN_0003bde0(actor_handle, *(int *)(actor + 0x18), actor + 0x120);
      FUN_0002f1a0(actor_handle);
    }
    result = 1;
    break;

  default:
    break;
  }

LAB_done:
  if (*(char *)0x5aca5b == 0) {
    return result;
  }
  if (*(unsigned int *)(actor + 0x34) == 0xffffffff) {
    csstrcpy(name_buf, "<no encounter>");
  } else {
    enc = (char *)tag_block_get_element(
      (char *)global_scenario_get() + 0x42c,
      (int)(*(unsigned int *)(actor + 0x34) & 0xffff), 0xb0);
    sq = (char *)tag_block_get_element(enc + 0x80,
                                       (int)*(short *)(actor + 0x3a), 0xe8);
    crt_sprintf(name_buf, "%s/%s", enc, sq);
  }
  FUN_00017120(global_scenario_get(), atom, (char *)0x5ab100, 0x100);
  status = "";
  if (result == 0) {
    status = " FAILED";
  }
  error(2, "%s: %s #%d%s: %s", name_buf, cmd_list, (int)(short)atom_seq, status,
        (char *)0x5ab100);
  return result;
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
  /* Command-list entry for THIS list (scenario_index selects the list,
   * entry size 0x60). Confirmed vs original: MOVSX EDX,[EBP+0xC]; PUSH 0x60;
   * PUSH EDX at 0x18bab-0x18bb4. A prior lift passed (0, 0) here — classic
   * cdecl arg-misgroup (args pushed before the nested global_scenario_get
   * call were misattributed by Ghidra), which made every atom validate
   * against command list 0 and instantly completed go_to atoms. */
  atom_table =
    (char *)tag_block_get_element(scenario + 0x438, (int)scenario_index, 0x60);

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

/* FUN_00019110 (0x19110)
 * Action-obey command-list step callback (prop-interest update).
 *
 * Called by actor_look_compute_prop_interest via FUN_000192b0.
 * Steps through atoms in the encounter's command list:
 *   - Validates the current atom via FUN_00018b90 (if cVar5 set).
 *   - If validated, advances the atom index and calls FUN_000169a0 to execute
 *     the atom step.
 *   - Sets the exhausted flag (state_data[4] | 0x2) when the index reaches
 *     the atom count.
 *   - When the loop exits without exhausting, writes 0 to *finished_ref.
 *     Asserts if finished_ref is NULL.
 *
 * Confirmed: param_1=actor_handle (EDI), param_2=unit_handle (EAX@<eax> of
 *   18b90/17ab0 call via [EBP+0xc]), param_3=scenario_idx (EBX, sign-extended
 *   short), param_4=state_data (ESI, loaded at 0x19142), param_5=cmd_param
 *   ([EBP+0x18]), param_6=finished_ref ([EBP+0x1c]).
 * Confirmed: FUN_00018b90 @<eax>=unit_handle, cdecl(actor_handle,
 *   scenario_idx, state_data, cmd_param).
 * Confirmed: FUN_000169a0 @<esi>=state_data, cdecl(actor_handle, unit_handle,
 *   scenario_idx, cmd_param, &out_index).
 * Confirmed: FUN_00017ab0 @<eax>=cmd_param, @<ecx>=unit_handle,
 *   cdecl(actor_handle, scenario_idx, state_data).
 * Confirmed: out_index local at [EBP+0x17]; cVar5 (loop-continue) stored at
 *   [EBP+0x13]; loop exits on (state_data[4] & 4). */
void FUN_00019110(int actor_handle, int unit_handle, short scenario_idx,
                  char *state_data, int cmd_param, char *finished_ref)
{
  char *atom_table;
  char cVar5;
  char out_index;
  bool validated;

  atom_table = (char *)tag_block_get_element(
    (char *)global_scenario_get() + 0x438, (int)scenario_idx, 0x60);

  if ((state_data[4] & 2) != 0)
    goto LAB_check_finished;

  cVar5 = (int)(unsigned char)state_data[0] < *(int *)(atom_table + 0x30);
  state_data[1] = 0;

  do {
    if (cVar5 != 0) {
      validated = FUN_00018b90(unit_handle, actor_handle, scenario_idx,
                               state_data, (void *)cmd_param);
      if (validated == 0)
        break;
    }

    if (state_data[0] == (char)0xff) {
      out_index = 0;
    } else {
      out_index = state_data[0] + 1;
    }

    if (cVar5 != 0) {
      FUN_000169a0(actor_handle, unit_handle, scenario_idx, cmd_param,
                   &out_index, state_data);
    }

    if (*(int *)(atom_table + 0x30) <= (int)(unsigned char)out_index) {
      state_data[4] = state_data[4] | 2;
      break;
    }

    state_data[0] = out_index;
    cVar5 = FUN_00017ab0(actor_handle, scenario_idx, state_data, cmd_param,
                         unit_handle);
  } while ((state_data[4] & 4) == 0);

LAB_check_finished:
  if ((state_data[4] & 2) != 0)
    return;

  if (finished_ref == NULL) {
    display_assert("finished_reference", "c:\\halo\\SOURCE\\ai\\action_obey.c",
                   0x595, 1);
    system_exit(-1);
  }
  *finished_ref = 0;
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
      (void)actor_move_animation_impulse(
        actor_handle, *(short *)(actor + 0xfa),
        (int *)&tmp_x); /* hazard-ok: intentional-discard (output via pointer
                           param; return val = success bool not needed here) */
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
  volatile int pos5_idx;

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
      pos5_idx = 5;
      *(int *)(state_data + 0x10) = pos[pos5_idx];
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
  if (*(char *)(actor + 0x9e) != '\0') {
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
  } else {
    if ((*(char *)(actor + 0x504) == '\0') && (*(char *)(actor + 6) == '\0')) {
      *(int *)(actor + 0xc4) = *(int *)(actor + 0xc4) + 1;
      if (*(int *)(actor + 0xc4) >= 0x78) {
        *(char *)(actor + 0x9d) = 1;
        *(char *)(actor + 0x9c) = 1;
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

/* FUN_0001a200 (0x1a200) — Run actor uncover-action look-target search.
 *
 * Source file: c:\halo\SOURCE\ai\action_uncover.c (assert line 0x84).
 *
 * Checks three gating flags on the actor record (swarm-assert, flag+0x4c,
 * flag+0x160, flag+0x9d), then builds a 0x670-byte look-state buffer and
 * calls FUN_00027090 to find a look target.  If a target is found:
 *   - If actor+0xa4==0 and look-result (local_48+6) is not 0 or 1:
 *       sets actor+0xa0=1 (with optional ai_debug log when +0xa0 was 0).
 *   - If actor+0xa4==1 and look-result==0:
 *       if distance (local_48+8) < actor_destination_tolerance:
 *         sets actor+0xbc=1 (with optional ai_debug log when +0xbc was 0).
 * Then always calls FUN_000272d0 to find firing position.
 * If firing position == -1: sets actor+0x9e=1.
 * Always returns 0.
 *
 * Confirmed: datum_get(actor_data, actor_handle) stores ESI at 0x1a21e.
 * Confirmed: swarm assert at action_uncover.c:0x84 (flag at actor+6).
 * Confirmed: csmemset 0x670 bytes at 0x1a271-0x1a284.
 * Confirmed: look_type = 3 at state_buf+0x4 (short).
 * Confirmed: store offsets from disasm (buf base EBP-0x7b4).
 * Confirmed: XOR AL,AL at 0x1a40e — always returns 0.
 */
char FUN_0001a200(int actor_handle)
{
  char *actor;
  static char state_buf[0x670];
  static char big_buf[0x14840];
  char local_48[0x44];
  int local_4;
  int local_8;
  short result;
  short look_result;
  float dist;
  float tol;
  char desc_buf[0x100];
  short fire_result;

  actor = (char *)datum_get(actor_data, actor_handle);
  if (*(char *)(actor + 6) != '\0') {
    display_assert("!actor->meta.swarm",
                   "c:\\halo\\SOURCE\\ai\\action_uncover.c", 0x84, 1);
    system_exit(-1);
  }
  if (*(char *)(actor + 0x4c) == '\0')
    return 0;
  if (*(char *)(actor + 0x160) != '\0')
    return 0;
  if (*(char *)(actor + 0x9d) != '\0')
    return 0;

  csmemset(state_buf, 0, 0x670);
  *(short *)(state_buf + 0x4) = 3;

  if (*(short *)(actor + 0xa4) == 1) {
    *(char *)(state_buf + 0x20) = 1;
    *(int *)(state_buf + 0x24) = *(int *)(actor + 0xb0);
    *(int *)(state_buf + 0x28) = *(int *)(actor + 0xb4);
    *(int *)(state_buf + 0x2c) = *(int *)(actor + 0xb8);
    *(int *)(state_buf + 0x30) = *(int *)(actor + 0xac);
    *(short *)(state_buf + 0x34) = *(short *)(actor + 0xa8);
  } else {
    *(char *)(state_buf + 0x41) = *(char *)(actor + 0xa0);
  }

  result = (short)FUN_00027090(actor_handle, state_buf, local_48, &local_8,
                               big_buf, &local_4);
  if (result != -1) {
    if (*(short *)(actor + 0xa4) == 0) {
      look_result = *(short *)(local_48 + 6);
      if ((look_result != 0) && (look_result != 1)) {
        if ((*(char *)(actor + 0xa0) == '\0') && (*(char *)0x5aca64 != '\0')) {
          ai_debug_describe_actor(actor_handle, -1, 1, desc_buf, 0x100);
          error(2, "%s: unable to see target's current location", desc_buf);
        }
        *(char *)(actor + 0xa0) = 1;
      }
    } else {
      if (*(short *)(local_48 + 6) == 0) {
        dist = *(float *)(local_48 + 8);
        tol = actor_destination_tolerance(actor_handle);
        if (dist < tol) {
          if ((*(char *)(actor + 0xbc) == '\0') &&
              (*(char *)0x5aca64 != '\0')) {
            ai_debug_describe_actor(actor_handle, -1, 1, desc_buf, 0x100);
            error(2, "%s: inspected pursuit location", desc_buf);
          }
          *(char *)(actor + 0xbc) = 1;
        }
      }
    }
  }

  fire_result = (short)FUN_000272d0(actor_handle, result, local_48, local_8,
                                    (unsigned int)big_buf, (char)local_4);
  if (fire_result == -1) {
    *(char *)(actor + 0x9e) = 1;
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
  int *src;
  int *dst;
  char force7;
  short pursuit;
  int prop_handle;

  actor = (char *)datum_get(actor_data, actor_handle);
  tag_data = (char *)tag_get(0x61637472, *(int *)(actor + 0x58));
  prop_handle = *(int *)(actor + 0x270);
  if (prop_handle != -1) {
    prop = (char *)datum_get(prop_data, prop_handle);
    force7 = 0;
    if (*(short *)(actor + 0xa4) == 0) {
      if (*(char *)(actor + 0x162) != '\0') {
        *(char *)(actor + 0x454) = 1;
        *(char *)(actor + 0x455) = 1;
        force7 = 1;
      } else if ((*tag_data & 0x10) != 0) {
        *(char *)(actor + 0x454) = (char)(*(short *)(actor + 0x268) >= 5);
      } else {
        *(char *)(actor + 0x454) = (char)(*(short *)(actor + 0x268) >= 6);
      }
    }
    if ((*(char *)(actor + 0x454) != '\0' &&
         (*(short *)(prop + 0x38) == 0 || *(short *)(prop + 0x38) == 1)) ||
        force7 != '\0') {
      *(short *)(actor + 0x3e8) = 7;
    } else if (*(short *)(actor + 0x268) < 5) {
      *(short *)(actor + 0x3e8) = 3;
    } else if (*(short *)(prop + 0x38) == 2 || *(short *)(prop + 0x38) == 4) {
      *(short *)(actor + 0x3e8) = 2;
    } else {
      *(short *)(actor + 0x3e8) = 5;
    }
    pursuit = *(short *)(actor + 0xa4);
    if (pursuit == 0) {
      *(short *)(actor + 0x3ec) = 2;
    } else if (pursuit == 1) {
      *(short *)(actor + 0x3ec) = 3;
      src = (int *)(actor + 0xb0);
      dst = (int *)(actor + 0x3f0);
      dst[0] = src[0];
      dst[1] = src[1];
      dst[2] = src[2];
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
  int *p;

  actor = (char *)datum_get(actor_data, actor_handle);
  looking = actor + 0x9c;
  if (*looking != '\0') {
    src = *(char **)0x2ee6d8;
    p = (int *)src;
    *param_2 = *p;
    param_2[1] = *(int *)(src + 4);
    param_2[2] = *(int *)(src + 8);
    param_2[3] = *(int *)(src + 0xc);
    return;
  }
  *param_2 = *(int *)*(char **)0x2ee6ec;
  param_2[1] = *(int *)(*(char **)0x2ee6ec + 4);
  param_2[2] = *(int *)(*(char **)0x2ee6ec + 8);
  param_2[3] = *(int *)(*(char **)0x2ee6ec + 0xc);
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
 * Confirmed: a4!=0 paths converge on result=(field_bc==0) at 0x1a958
 *   (both from the increment block and the shared merge at 0x1a912).
 * Confirmed: field_9e=1 written ONLY on the field_3b8==-1 path (0x1a96d);
 *   the else path's je jumps past it to the shared decrement at 0x1a974.
 * Confirmed: field_9d receives the flag register once at 0x1aad0
 *   (single MOV [ESI+0x9d],BL at function exit).
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
      if (*(char *)(threat + 0x121) <= 2)
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

  if (*(char *)(actor + 0x504) != '\0') {
    *(int *)(actor + 0xc0) = 0;
  } else {
    *(int *)(actor + 0xc0) = *(int *)(actor + 0xc0) + 1;
    if (*(short *)(actor + 0xa4) != 0)
      goto LAB_result_inspected;
    if (*(int *)(actor + 0xc0) >= 30)
      FUN_00024be0(actor_handle, *(unsigned short *)(actor + 0x3b8), 0);
  }

  if (*(short *)(actor + 0xa4) == 0) {
    if (*(int *)(actor + 0x270) != -1) {
      threat = (char *)datum_get(*(data_t **)0x5ab23c, *(int *)(actor + 0x270));
      done = (char)(*(short *)(threat + 0x32) > 0);
      if (done && *(short *)(actor + 0x268) < 5)
        result = 0;
      else
        result = 1;
    }
  } else {
  LAB_result_inspected:
    result = (*(char *)(actor + 0xbc) == '\0');
  }

  if (*(short *)(actor + 0x3b8) == -1) {
    *(char *)(actor + 0x9e) = 1;
  LAB_decrement:
    if (*(int *)(actor + 0xc8) > 0)
      *(int *)(actor + 0xc8) = *(int *)(actor + 0xc8) - 1;
    *(int *)(actor + 0xcc) = *(int *)(actor + 0xcc) + 1;
  LAB_expiry:
    if (*(int *)(actor + 0xc8) == 0 || *(int *)(actor + 0xcc) >= 360)
      result = 1;
    else
      result = 0;
  } else {
    if (!result || (*(char *)(actor + 0x162) == '\0' && done == '\0' &&
                    *(char *)(actor + 0x504) == '\0'))
      goto LAB_decrement;
    *(int *)(actor + 0xc8) = *(int *)(actor + 0xc4);
    goto LAB_expiry;
  }

  if (*(short *)(actor + 0xa4) == 1 && *(char *)(actor + 0xbc) != '\0')
    result = 1;
  else if (result == '\0')
    goto LAB_store;

  if (*(char *)0x5aca64 != '\0') {
    ai_debug_describe_actor(actor_handle, -1, 1, debug_buf, 0x100);
    if (*(int *)(actor + 0xc8) == 0) {
      csprintf(debug_msg, "timer %d finished", *(int *)(actor + 0xc4));
    } else if (*(int *)(actor + 0xcc) >= 360) {
      csprintf(debug_msg, "persistent timer %d", *(int *)(actor + 0xcc));
    } else if (*(short *)(actor + 0xa4) == 1 &&
               *(char *)(actor + 0xbc) != '\0') {
      csprintf(debug_msg, "location inspected");
    } else {
      csprintf(debug_msg, "<unknown reason>");
    }
    console_printf(2, "%s: %s uncover done: %s", debug_buf,
                   (*(short *)(actor + 0xa4) == 0) ? "target" : "pursuit",
                   debug_msg);
  }
LAB_store:
  *(char *)(actor + 0x9d) = result;
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
  char result;
  short i;

  actor = (char *)datum_get(actor_data, actor_handle);
  result = 0;
  if (param_2 != -1) {
    for (i = 0; i < 4; i++) {
      if (param_2 == *(short *)(actor + 0x3ca + (int)i * 4)) {
        result = 1;
        break;
      }
    }
  }
  return result;
}

/* FUN_00024cf0 (0x24cf0)
 * Evaluate and score firing positions for an actor.
 *
 * First pass (for each fp in fp_array[0..fp_count)):
 *   Stride = 0x3c bytes. fp_ptr = fp_array+0x14+i*0x3c.
 *   fp_pos_ptr = *(int*)(fp_ptr-0x14)  external 3-float position pointer.
 *   fp_type    = *(short*)(fp_ptr-0x10).
 *   fp->valid  = *(char*)(fp_ptr+0x1c).
 *   fp->score  = *(float*)(fp_ptr+0x24).
 *
 *   FUN_00024ca0: 1 if fp_type in actor's active slot set.
 *   result==1: mark fp[0x1d]=1; invalidate (fp[0x1c]=0) if not locked.
 *   result==0: skip marks, fall through to scoring body.
 *
 *   Danger zone (eval[0x40] set):
 *     Trajectory dir = actor[0x2c8..0x2d0] - actor[0x2b0..0x2b8].
 *     Sphere (r=actor[0x2d8]+2.5) around actor[0x2dc]: if fp inside:
 *       FUN_0010cd40 dist from fp to trajectory line (dist_sq).
 *       if dist_sq <= range^2: mark fp[0x1d]=1; invalidate if unlocked.
 *       if dist_sq <= (range+2.5)^2: score=(sqrt(dist_sq)-range)*8.0 ->
 * FUN_00024000. else: score=20.0 -> FUN_00024000. Sphere (r=actor[0x2d8]+3.0)
 * around actor[0x12c] eye: if eye inside AND actor[0x2d4]>range: FUN_0010cd40
 * from eye to trajectory; if eye_dist>range^2: scaled fp direction =
 * fp_ptr[-8..0]*3.0; if norm>1e-4: vector_to_line_distance_squared3d; if
 * dist<range^2: invalidate if unlocked.
 *
 *   Faction bonus: bit (1<<fp_faction) in eval[0x48] -> fp->score +=
 * eval[0x4c]. Threat proximity: eval[0x50] threats; stride 0x10; weight at
 * +0x54, pos at +0x58. min_ratio = min of dist_sq/(w^2); fp->score +=
 * (min<1)?sqrt(min)*10:10.
 *
 * Second pass (eval[0x45] && actor[0x158] != -1):
 *   fp2 = fp_array+0x30+i*0x3c; fp2->valid at fp2[0]; fp2->score at fp2[8].
 *   fp2_pos_ptr = *(int*)(fp2-0x30).
 *   cos_angle = dot(fp2_pos-target_pos, target_facing) / dist.
 *   dist<8m AND cos<0.707 AND dir non-degen: invalidate if unlocked.
 *   Else: los_score = f(cos_angle) in [0..15]; fp2->score += los_score.
 *
 * Confirmed: datum_get at 0x24d04; CALL FUN_00024ca0 at 0x24d46.
 * Confirmed: stride 0x3c at 0x25138; fp_ptr saved at [EBP-4] (0x24d24).
 * Confirmed: FUN_0010cd40 at 0x24e33, 0x24f3a; FUN_0010ce10 at 0x24fb8.
 * Confirmed: object_get_and_verify_type(target,2) at 0x25168.
 * Confirmed: fp->score = fp_ptr+0x24 = fp2_ptr+8 (element_base+0x38).
 * Confirmed: threat stride i*0x10 (SHL EAX,4 at 0x25076). */
void FUN_00024cf0(int actor_handle, char *eval_state, unsigned short fp_count,
                  char *fp_array)
{
  char *actor;
  char *fp_ptr;
  char *fp_pos_ptr;
  char *fp_base_ptr;
  char *actor_traj_base;
  float local_buf[3];
  float dir_vec[3];
  float faction_bonus;
  float local_1c_range;
  float local_14_score;
  float local_10_dsq;
  float local_8_min;
  float local_c_score;
  int target_handle;
  char *target_obj;
  unsigned int fp_idx;
  int threat_count;
  int threat_i;
  float dist_sq;
  float norm_sq;
  float radius;
  float range;
  float dx, dy, dz;
  float w;
  float ratio;
  short fp_type;
  unsigned int faction_mask;
  unsigned int faction_bit;
  char *fp2_ptr;
  char *fp2_pos_ptr;
  unsigned int fp2_idx;
  float dot_val;
  float dist;
  float cos_angle;
  float los_score;
  float tfx, tfy, tfz;

  actor = (char *)datum_get(actor_data, actor_handle);

  /* ======================================================
   * First pass: evaluate each firing position
   * ====================================================== */
  if ((short)fp_count > 0) {
    fp_ptr = fp_array + 0x14;
    fp_idx = (unsigned int)fp_count;
    do {
      if (*(char *)(fp_ptr + 0x1c) != 0) {
        fp_type = *(short *)(fp_ptr - 0x10);
        if (FUN_00024ca0(actor_handle, fp_type) != 0) {
          *(char *)(fp_ptr + 0x1d) = 1;
          if (*(char *)(eval_state + 0x14) == 0) {
            *(char *)(fp_ptr + 0x1c) = 0;
            goto LAB_24cf0_fp_next;
          }
        }

        if (*(char *)(eval_state + 0x40) != 0) {
          if (*(short *)(actor + 0x280) <= 0 || *(char *)(actor + 0x287) == 0) {
            display_assert("actor->danger_zone.position != NONE",
                           "c:\\halo\\SOURCE\\ai\\actor_firing_position.c",
                           0xba, 1);
            system_exit(-1);
          }

          fp_pos_ptr = (char *)*(int *)(fp_ptr - 0x14);
          actor_traj_base = actor + 0x2b0;
          local_buf[0] = *(float *)(actor + 0x2c8) - *(float *)(actor + 0x2b0);
          local_buf[1] = *(float *)(actor + 0x2cc) - *(float *)(actor + 0x2b4);
          local_buf[2] = *(float *)(actor + 0x2d0) - *(float *)(actor + 0x2b8);
          fp_base_ptr = fp_ptr - 0x14;

          dx = *(float *)fp_pos_ptr - *(float *)(actor + 0x2dc);
          dy = *(float *)(fp_pos_ptr + 4) - *(float *)(actor + 0x2e0);
          dz = *(float *)(fp_pos_ptr + 8) - *(float *)(actor + 0x2e4);
          dist_sq = dx * dx + dy * dy + dz * dz;
          radius = *(float *)(actor + 0x2d8) + 2.5f;

          if (dist_sq <= radius * radius) {
            local_10_dsq = FUN_0010cd40((float *)fp_pos_ptr,
                                        (float *)actor_traj_base, local_buf);
            range = *(float *)(actor + 0x294);
            local_14_score = 0.0f;

            if (local_10_dsq <= range * range) {
              *(char *)(fp_ptr + 0x1d) = 1;
              if (*(char *)(eval_state + 0x14) == 0) {
                *(char *)(fp_ptr + 0x1c) = 0;
                goto LAB_24cf0_fp_next;
              }
            } else {
              if (local_10_dsq <= (range + 2.5f) * (range + 2.5f)) {
                local_14_score = (xbox_sqrtf(local_10_dsq) - range) * 8.0f;
              } else {
                local_14_score = 20.0f;
              }
              FUN_00024000(eval_state, local_14_score, 0x17, fp_base_ptr);
            }
          }

          dx = *(float *)(actor + 0x12c) - *(float *)(actor + 0x2dc);
          dy = *(float *)(actor + 0x130) - *(float *)(actor + 0x2e0);
          dz = *(float *)(actor + 0x134) - *(float *)(actor + 0x2e4);
          dist_sq = dx * dx + dy * dy + dz * dz;
          radius = *(float *)(actor + 0x2d8) + 3.0f;

          if (dist_sq <= radius * radius) {
            if (*(float *)(actor + 0x2d4) > *(float *)(actor + 0x294)) {
              local_10_dsq = FUN_0010cd40((float *)(actor + 0x12c),
                                          (float *)actor_traj_base, local_buf);
              range = *(float *)(actor + 0x294);
              if (local_10_dsq > range * range) {
                dir_vec[0] = *(float *)(fp_ptr - 8) * 3.0f;
                dir_vec[1] = *(float *)(fp_ptr - 4) * 3.0f;
                dir_vec[2] = *(float *)(fp_ptr + 0) * 3.0f;
                norm_sq = dir_vec[0] * dir_vec[0] + dir_vec[1] * dir_vec[1] +
                          dir_vec[2] * dir_vec[2];
                if (norm_sq > 1e-4f) {
                  local_1c_range = *(float *)(actor + 0x294);
                  local_10_dsq = vector_to_line_distance_squared3d(
                    (float *)(actor + 0x12c), dir_vec, (float *)actor_traj_base,
                    local_buf);
                  if (local_10_dsq < local_1c_range * local_1c_range) {
                    *(char *)(fp_ptr + 0x1d) = 1;
                    if (*(char *)(eval_state + 0x14) == 0) {
                      *(char *)(fp_ptr + 0x1c) = 0;
                      goto LAB_24cf0_fp_next;
                    }
                  }
                }
              }
            }
          }
        }

        fp_pos_ptr = (char *)*(int *)(fp_ptr - 0x14);
        faction_mask = *(unsigned int *)(eval_state + 0x48);
        faction_bit = 1u << (*(unsigned char *)(fp_pos_ptr + 0xc) & 0x1f);
        if (faction_mask & faction_bit) {
          faction_bonus = *(float *)(eval_state + 0x4c);
          if (faction_bonus < 0.0f || faction_bonus >= 1000.0f) {
            display_assert("score >= 0.f && score < REAL_MAX",
                           "c:\\halo\\SOURCE\\ai\\actor_firing_position.c",
                           0x81, 1);
            system_exit(-1);
          }
          *(float *)(fp_ptr + 0x24) += faction_bonus;
        }

        threat_count = *(int *)(eval_state + 0x50);
        if (threat_count > 0) {
          fp_pos_ptr = (char *)*(int *)(fp_ptr - 0x14);
          local_8_min = 1.0f;
          for (threat_i = 0; threat_i < threat_count; threat_i++) {
            int off = threat_i * 0x10;
            w = *(float *)(eval_state + off + 0x54);
            dx = *(float *)(eval_state + off + 0x58) - *(float *)fp_pos_ptr;
            dy =
              *(float *)(eval_state + off + 0x5c) - *(float *)(fp_pos_ptr + 4);
            dz =
              *(float *)(eval_state + off + 0x60) - *(float *)(fp_pos_ptr + 8);
            dist_sq = dx * dx + dy * dy + dz * dz;
            ratio = dist_sq / (w * w);
            if (ratio < local_8_min)
              local_8_min = ratio;
          }
          local_c_score = 10.0f;
          if (local_8_min < 1.0f) {
            local_c_score = xbox_sqrtf(local_8_min) * 10.0f;
            if (local_c_score < 0.0f || local_c_score >= 1000.0f) {
              display_assert("score >= 0.f && score < REAL_MAX",
                             "c:\\halo\\SOURCE\\ai\\actor_firing_position.c",
                             0x81, 1);
              system_exit(-1);
            }
          }
          *(float *)(fp_ptr + 0x24) += local_c_score;
        }
      }

    LAB_24cf0_fp_next:
      fp_ptr += 0x3c;
      fp_idx--;
    } while (fp_idx != 0);
  }

  /* ======================================================
   * Second pass: LOS scoring against locked target
   * ====================================================== */
  if (*(char *)(eval_state + 0x45) != 0) {
    target_handle = *(int *)(actor + 0x158);
    if (target_handle != -1) {
      target_obj = (char *)object_get_and_verify_type(target_handle, 2);
      object_get_world_position(target_handle, (vector3_t *)local_buf);

      if ((short)fp_count > 0) {
        fp2_ptr = fp_array + 0x30;
        fp2_idx = (unsigned int)fp_count;
        do {
          if (*(char *)fp2_ptr != 0) {
            fp2_pos_ptr = (char *)*(int *)(fp2_ptr - 0x30);
            dx = *(float *)fp2_pos_ptr - local_buf[0];
            dy = *(float *)(fp2_pos_ptr + 4) - local_buf[1];
            dz = *(float *)(fp2_pos_ptr + 8) - local_buf[2];
            dist_sq = dx * dx + dy * dy + dz * dz;

            if ((double)(dist_sq < 0.0f ? -dist_sq : dist_sq) > 1e-4 &&
                dist_sq < 900.0f) {
              dot_val = dz * *(float *)(target_obj + 0x2c);
              dot_val += dy * *(float *)(target_obj + 0x28);
              dot_val += dx * *(float *)(target_obj + 0x24);
              dist = xbox_sqrtf(dist_sq);
              cos_angle = dot_val / dist;

              if (*(char *)(eval_state + 0x46) == 0) {
                tfx = *(float *)(target_obj + 0x18);
                tfy = *(float *)(target_obj + 0x1c);
                tfz = *(float *)(target_obj + 0x20);
                norm_sq = tfx * tfx + tfy * tfy + tfz * tfz;
                if (norm_sq <= 1.0f / 144.0f)
                  goto LAB_24cf0_fp2_score;
              }

              if (dist_sq < 64.0f && cos_angle < 0.7071068f) {
                *(char *)(fp2_ptr + 1) = 1;
                if (*(char *)(eval_state + 0x14) == 0) {
                  *(char *)fp2_ptr = 0;
                  goto LAB_24cf0_fp2_next;
                }
              }

            LAB_24cf0_fp2_score:
              los_score = 15.0f;
              if (cos_angle >= 0.866025f) {
                /* max score; no assertion needed */
              } else if (cos_angle < 0.0f) {
                los_score = 15.0f - cos_angle * -15.0f;
                if (los_score < 0.0f || los_score >= 1000.0f) {
                  display_assert(
                    "score >= 0.f && score < REAL_MAX",
                    "c:\\halo\\SOURCE\\ai\\actor_firing_position.c", 0x81, 1);
                  system_exit(-1);
                }
              } else {
                los_score = (cos_angle - 0.866025f) * 111.9619f + 15.0f;
                if (los_score < 0.0f || los_score >= 1000.0f) {
                  display_assert(
                    "score >= 0.f && score < REAL_MAX",
                    "c:\\halo\\SOURCE\\ai\\actor_firing_position.c", 0x81, 1);
                  system_exit(-1);
                }
              }
              *(float *)(fp2_ptr + 8) += los_score;
            }
          }
        LAB_24cf0_fp2_next:
          fp2_ptr += 0x3c;
          fp2_idx--;
        } while (fp2_idx != 0);
      }
    }
  }
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
          /* @<esi> = position element base (original 0x254ed LEA ESI,[EDI-8]);
           * 24000 accumulates score into [esi+0x38] = pos+0x30. */
          FUN_00024000(ctx, score, 3, pos - 8);
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
              /* @<esi> = position element base (original 0x2576b). */
              FUN_00024000(ctx,
                           (*(float *)0x2533c8 - bonus) * *(float *)0x253f78,
                           10, pos - 8);
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
 * Confirmed: EAX=state_ptr@<eax>, ESI=actor_ptr@<esi>, EBP+8=actor_handle.
 *   (actor was wrongly a stack param before 2026-06-10 — original callers
 *   at 0x26d2f/0x2728f push only the handle; the rvthunk fed garbage.)
 *   Calls FUN_00024850, FUN_000257a0, FUN_00024890.
 * Confirmed: FUN_00024850 takes actor@<edi> + state@<ebx> pass-throughs —
 *   original 0x25988 MOV EDI,ESI and 0x25974 MOV EBX,EAX before the call;
 *   24850/24890 gate hook tables (0x254bf8/0x254c30) on 1<<[actor+4]
 *   (actor type index) and forward (handle, actor, state) to each hook.
 * Confirmed: state+0x30/0x31/0x34/0x38 accessed; actor+0x668/0x66a/0x66c
 *   debug counters. */
char FUN_00025970(void *state, int actor_handle, char *actor)
{
  char result;

  *(int *)((char *)state + 0x38) = 0;
  *(int *)((char *)state + 0x34) = 0;
  *(char *)((char *)state + 0x31) = 0;
  *(char *)((char *)state + 0x30) = 1;

  FUN_00024850(actor_handle, 1, actor, state);

  if (*(char *)((char *)state + 0x30) != 0)
    *(short *)(actor + 0x668) = *(short *)(actor + 0x668) + 1;

  if (*(char *)((char *)state + 0x31) == 0)
    *(short *)(actor + 0x66a) = *(short *)(actor + 0x66a) + 1;

  if (*(char *)((char *)state + 0x30) != 0) {
    if (*(char *)(actor + 0x5fc) != 0)
      FUN_000257a0(actor_handle, state, actor);
    *(int *)((char *)state + 0x34) = *(int *)((char *)state + 0x38);
    result = FUN_00024890(actor_handle, state, actor);
    *(char *)((char *)state + 0x30) = result;
    *(short *)(actor + 0x66c) = *(short *)(actor + 0x66c) + 1;
  }

  return *(char *)((char *)state + 0x30);
}

/* FUN_00025a00 (0x25a00) — actor_has_accessible_firing_position
 * Tests whether the given position is an accessible firing position for the
 * actor.  Checks that the position belongs to the actor's encounter's firing-
 * position list, is within range, and (for non-swarm actors) is reachable via
 * a path-state ray cast.
 *
 * Confirmed: param_1=actor_handle, param_2=position (float *),
 *   param_3=surface_index (-1 = none), param_4=group_mask (bitfield).
 * Confirmed: actor+0x99 = is_swarm flag; actor+0x34 = encounter handle;
 *   actor+0x3a = firing_position_index (short); actor+0x58 = tag_index.
 * Confirmed: FUN_00024a60 = actor_get_firing_position_group (3 args).
 * Confirmed: FUN_0005f550 = path_state_estimated_distance (6 args cdecl,
 *   ADD ESP,0x18 at 0x25bb6/all sites; output via arg4 float*, args 5/6 are
 *   optional out-pointers — this site shares two PUSH 0 with the 0x5e830
 *   branch, so it passes NULL/NULL); NOT an ESI-output function — unaff_ESI
 *   in Ghidra is an artifact of inlining; result is at [EBP-0x8] after the
 *   call.  arg3 is a raw int load (MOV ECX,[EDX+0x14]), not a float-to-int
 *   conversion.
 * Confirmed: threshold at 0x2533d8 = 4.0f; dist_sq threshold at 0x254e74 =
 *   16.0f.
 * Confirmed: huge path-state buffer at EBP-0x140e0 (82080 bytes); ray-init
 *   struct at EBP-0x54 (84 bytes).
 * Source file string: c:\halo\SOURCE\ai\actor_firing_position.c */
char actor_has_accessible_firing_position(int actor_handle, float *position,
                                          int surface_index, int group_mask)
{
  /* C89: all declarations at top */
  char *actor;
  char *tag;
  void *bsp;
  char *encounter_elem;
  unsigned int mask;
  char *fp_block;
  float *fp_elem;
  float dx;
  float dy;
  float dz;
  float dist_sq;
  float hit_dist;
  int i;
  int fp_count;
  char vis;
  unsigned char huge_buf[0x140e0];
  unsigned char ray_init[0x54];

  actor = (char *)datum_get(actor_data, actor_handle);
  tag = (char *)tag_get(0x61637472, *(int *)(actor + 0x58));

  if (*(char *)(actor + 0x99) == 0) {
    /* not a swarm: validate surface_index */
    if (surface_index != -1) {
      bsp = global_collision_bsp_get();
      if (surface_index < 0 || surface_index >= *(int *)((char *)bsp + 0x3c)) {
        display_assert("(test_surface_index >= 0) && (test_surface_index < "
                       "collision_bsp->surfaces.count)",
                       "c:\\halo\\SOURCE\\ai\\actor_firing_position.c", 0x59a,
                       1);
        system_exit(-1);
      }
    }
    if (*(char *)(actor + 0x99) == 0 && surface_index == -1) {
      return 0;
    }
  }

  if (*(int *)(actor + 0x34) == -1) {
    return 0;
  }

  /* actor has an encounter — get firing-position group mask */
  encounter_elem = (char *)tag_block_get_element(
    (char *)global_scenario_get() + 0x42c,
    *(unsigned int *)(actor + 0x34) & 0xffff, 0xb0);
  mask =
    (unsigned int)actor_get_firing_position_group(actor_handle, 0, group_mask);

  /* for non-swarm: set up path-state ray */
  if (*(char *)(actor + 0x99) == 0) {
    path_input_new((void *)ray_init, *(unsigned int *)((char *)tag + 0x8c), 1,
                   -1);
    path_input_set_start((void *)ray_init, position, surface_index);
    path_input_set_search_bounds((void *)ray_init, 0x40800000);
    path_state_new((void *)ray_init, (void *)huge_buf, (void *)0);
    FUN_0005ff70((unsigned int *)huge_buf);
  }

  /* iterate over encounter's firing positions */
  fp_block = encounter_elem + 0x98;
  fp_count = *(int *)(encounter_elem + 0x98);
  i = 0;
  if (fp_count <= 0) {
    return 0;
  }
  do {
    fp_elem = (float *)tag_block_get_element((void *)fp_block, i, 0x18);

    /* check group bit */
    if (mask & (1u << (*(unsigned char *)((char *)fp_elem + 0xc) & 0x1f))) {
      /* check squared distance */
      dx = fp_elem[0] - position[0];
      dy = fp_elem[1] - position[1];
      dz = fp_elem[2] - position[2];
      dist_sq = dx * dx + dy * dy + dz * dz;

      if (dist_sq < 16.0f) {
        if (*(char *)(actor + 0x99) == 0) {
          /* non-swarm: ray-cast check */
          hit_dist = 0.0f;
          path_state_estimated_distance((void *)huge_buf, (void *)fp_elem,
                                        *(int *)(fp_elem + 5), &hit_dist,
                                        (float *)0, (float *)0);
          if (hit_dist < 4.0f) {
            return 1;
          }
        } else {
          /* swarm: simple 3D visibility check */
          vis =
            path_3d_available((int)scenario_get(), (int *)position, 0,
                              (int *)fp_elem, (unsigned char *)0, (float *)0);
          if (vis != 0) {
            return 1;
          }
        }
      }
    }

    i++;
  } while (i < fp_count);

  return 0;
}

/* FUN_00025c10 (0x25c10) — actor_firing_position evaluate/select.
 *
 * Main entry point for choosing an actor's firing position within its
 * encounter.  Populates the caller-supplied evaluation context (eval_ctx),
 * gathers candidate firing-position records, refines them against the
 * current target, runs the registered scorer table, sorts the survivors,
 * and selects the best valid record.
 *
 *   actor_handle  — actor datum handle (EBP+0x8).
 *   eval_ctx      — evaluation context / request buffer (EBP+0xc, EBX).
 *   out_record    — if non-NULL receives a 0x3c-byte copy of the selected
 *                   record (15 dwords) (EBP+0x10).
 *   out_owner     — if non-NULL receives the selected firing-position's
 *                   owner actor index (EBP+0x14).
 *   huge_buf      — caller-provided path-state scratch (EBP+0x18).
 *   out_found     — byte out: 1 when the actor path-state is valid
 *                   (EBP+0x1c).
 *
 * Returns the selected firing-position index (AX), or -1.
 *
 * Confirmed: cdecl 6 stack args; _chkstk frame 0x1c91c at 0x25c13.
 * Confirmed: EDI=actor, EBX=eval_ctx, [EBP-0x28]=actr tag, [EBP-0xc]=
 *   encounter element (scenario+0x42c stride 0xb0), [EBP-0x24]=best score
 *   (init 0), [EBP-0x30]=selected (init -1).
 * Confirmed: debug gate compares DAT_005ac9f8 against ACTOR_HANDLE
 *   (CMP EDX,ESI at 0x25c63; ESI=[EBP+8]).
 * Confirmed: candidate records are a 0x3c-stride array (max 0x200) at
 *   EBP-0x8890: pos-ptr +0x0, fp-index +0x4 (u16), +0x6 u16 0, dist +0x8,
 *   dir +0xc..0x14, clearance +0x18, refine-dir +0x20..0x28, target-dist²
 *   +0x2c, active +0x30, skip +0x31, score-copy +0x34, score +0x38.
 *   Init zeroes +0x2c/+0x34/+0x38 (0x2677f-0x2678d).
 * Confirmed: prop-scan gate flags read the ACTR TAG (+0 sign bit, +4 bit0)
 *   via [EBP-0x28] (0x262dc-0x262fa), and the seed weight at 0x261ee is
 *   actor_tag+0x3c8 (ESI=[EBP-0x28] set at 0x26018).
 * Confirmed: prop loop second branch enters when
 *   (prop+0xa4 && (prop+0x12e || prop+0x12f)) || (actor_tag[4] & 1)
 *   (0x26447-0x26471).
 * Confirmed: actor_perception_friend_prop_is_attacking(actor_handle,
 *   iter[0], out_dir) — pushes EAX(dir) ECX(iter) EDX(actor) at 0x263b4.
 * Confirmed: scorer table at 0x254bf8 (8-byte entries: mask word at -4,
 *   fn at +0): fn(actor_handle, ctx, rec_count, records) — pushes
 *   ECX(records) EDX(count) EBX(ctx) EAX(actor) at 0x26da0.
 * Confirmed: sort FUN_00091ef0(order, count, FUN_00024950) with record
 *   base/count published to 0x331f04/0x331f00 (0x26de8/0x26def).
 * Confirmed: path_state_estimated_distance is 6-arg cdecl (ADD ESP,0x18):
 *   main loop (huge_buf, pos, pos[0x14], rec+0x8, rec+0x1c,
 *   ctx[0x40] ? rec+0xc : 0) at 0x26c50; refine loop (path_eval, pos,
 *   pos[0x14], rec+0x18, 0, ctx[0x43] ? rec+0x20 : 0) at 0x269fb.
 * Confirmed: discard test uses <= (TEST AH,0x41; JP at 0x26e78):
 *   rec[0x38]+ctx[0x660] <= best.
 * Confirmed: "too many firing positions" error passes the encounter
 *   element for %s and *(enc+0x98) for %d (pushes at 0x267b7-0x267c3).
 * Confirmed: fp-eval debug name table = {fight, panic, cover, uncover,
 *   guard, pursue} built at EBP-0x48..-0x34 (0x26fe7-0x2700a).
 * Confirmed: local path-eval buffer at EBP-0x1c91c (0x1408c bytes,
 *   LEA at 0x269f4) is distinct from the huge_buf parameter.
 * Confirmed: returns AX=[EBP-0x30]; on success AX=rec[+0x4] and
 *   *out_owner=owner_indices[fp_idx] (0x27058-0x27072). */
short FUN_00025c10(int actor_handle, void *eval_ctx, int *out_record,
                   int *out_owner, void *huge_buf, int *out_found)
{
  char path_eval_scratch[0x1408c]; /* EBP-0x1c91c local path-eval state */
  char records[0x200 * 0x3c]; /* EBP-0x8890 candidate records */
  int order[0x200]; /* EBP-0x1090 sort order indices */
  int owner_indices[0x200]; /* EBP-0x890 fp owner actor indices */
  char path_input[0x44]; /* EBP-0x90 path-input scratch */
  int prop_iter[2]; /* EBP-0x14 prop iterator */
  float vtmp[3]; /* EBP-0x3c scratch vector */
  float dir[3]; /* EBP-0x18 aim direction */
  const char *names[6]; /* EBP-0x48 fp-eval debug names */
  char *ctx; /* EBX = eval_ctx */
  char *actor; /* EDI */
  char *actor_tag; /* EBP-0x28 ('actr' definition) */
  char *encounter_atoms; /* EBP-0xc encounter element */
  void *fp;
  char *unit;
  int selected; /* EBP-0x30 chosen record index */
  float best; /* EBP-0x24 running best score */
  float sel; /* facing-evaluation weight */
  char debug_eval; /* EBP-0x1 */
  char debug_overflow; /* EBP-0x19 */
  char range_flag; /* EBP-0x2 */
  int rec_count; /* EBP-0x8 */
  int fp_index; /* EBP-0x10 */
  int mode;
  int i;
  int j;

  ctx = (char *)eval_ctx;
  actor = (char *)datum_get(actor_data, actor_handle);
  selected = -1;
  best = 0.0f;
  debug_eval = 0;
  if (*(int *)(actor + 0x34) == *(int *)0x5ac9f4 &&
      (*(int *)0x5ac9f8 == -1 || *(int *)0x5ac9f8 == actor_handle)) {
    char dbg;
    dbg = *(char *)0x5aca83;
    if (dbg != 0 && *(short *)(ctx + 0x4) != 5)
      debug_eval = 1;
    dbg = *(char *)0x5aca84;
    if (dbg != 0 && *(short *)(ctx + 0x4) == 5)
      debug_eval = 1;
  }

  if (*(int *)(actor + 0x34) == -1)
    return (short)selected;

  actor_tag = (char *)tag_get(0x61637472 /*'actr'*/, *(int *)(actor + 0x58));
  tag_get(0x61637476 /*'actv'*/, *(int *)(actor + 0x5c));
  encounter_atoms =
    (char *)tag_block_get_element((char *)global_scenario_get() + 0x42c,
                                  *(int *)(actor + 0x34) & 0xffff, 0xb0);

  if (*(char *)(actor + 0x160) != '\0') {
    display_assert("!actor->input.vehicle_passenger",
                   "c:\\halo\\SOURCE\\ai\\actor_firing_position.c", 0x610, 1);
    system_exit(-1);
  }

  ++*(short *)0x5ac904;
  encounter_build_firing_position_owner_actor_indices(*(int *)(actor + 0x34),
                                                      owner_indices);
  if (*(short *)(actor + 0x3b8) != -1)
    owner_indices[*(short *)(actor + 0x3b8)] = -1;

  if (*(short *)(actor + 0x15e) == 0)
    sel = *(float *)0x254cc0;
  else
    sel = *(float *)0x254fe4;
  *(float *)(ctx + 0x18) = sel;
  if (*(float *)(ctx + 0x1c) == *(float *)0x2533c0)
    *(float *)(ctx + 0x1c) = sel;

  *(char *)(ctx + 0x42) = (*(short *)(ctx + 0x4) == 5);
  *(char *)(ctx + 0x5fc) = 0;

  if (*(char *)(ctx + 0x20) != '\0') {
    float dx, dy, dz;
    *(vector3_t *)(ctx + 0x604) = *(vector3_t *)(ctx + 0x24);
    *(vector3_t *)(ctx + 0x634) = *(vector3_t *)(ctx + 0x24);
    *(short *)(ctx + 0x640) = *(short *)(ctx + 0x34);
    *(char *)(ctx + 0x5fc) = 1;
    *(int *)(ctx + 0x630) = *(int *)(ctx + 0x30);
    dx = *(float *)(ctx + 0x604) - *(float *)(actor + 0x12c);
    dy = *(float *)(ctx + 0x608) - *(float *)(actor + 0x130);
    dz = *(float *)(ctx + 0x60c) - *(float *)(actor + 0x134);
    *(int *)(ctx + 0x644) = -1;
    *(int *)(ctx + 0x62c) = -1;
    *(int *)(ctx + 0x658) = 0;
    *(float *)(ctx + 0x600) = xbox_sqrtf(dz * dz + dy * dy + dx * dx);
    /* estimate_mode=1 confirmed: PUSH 0x1 at 0x25e34; args 4/5 are both
     * genuine NULLs (PUSH 0 x2 at 0x25e07/0x25e0c). */
    unit_estimate_position(*(int *)(actor + 0x18), 1,
                           (vector3_t *)(ctx + 0x604), (vector3_t *)0,
                           (vector3_t *)0, (vector3_t *)(ctx + 0x610));
    *(vector3_t *)(ctx + 0x61c) = *(vector3_t *)(ctx + 0x610);
  } else {
    int guard;
    if (((*(short *)(actor + 0x6c) == 4 &&
          (guard = *(int *)(actor + 0xb8)) != -1) ||
         (guard = *(int *)(actor + 0x270)) != -1) &&
        guard != -1) {
      unit = (char *)datum_get(*(data_t **)0x5ab23c, guard);
      if (*(char *)(ctx + 0x42) != '\0' && *(short *)(unit + 0x24) >= 2 &&
          *(short *)(unit + 0x24) <= 3)
        actor_perception_find_prop_pathfinding_location(actor_handle, guard);
      *(char *)(ctx + 0x5fc) = 1;
      *(vector3_t *)(ctx + 0x604) = *(vector3_t *)(unit + 0xbc);
      *(vector3_t *)(ctx + 0x634) = *(vector3_t *)(unit + 0xf0);
      *(int *)(ctx + 0x630) = *(int *)(unit + 0xec);
      *(short *)(ctx + 0x640) = *(short *)(unit + 0x100);
      *(int *)(ctx + 0x600) = *(int *)(unit + 0x11c);
      *(int *)(ctx + 0x644) = guard;
      *(vector3_t *)(ctx + 0x610) = *(vector3_t *)(unit + 0x104);
      *(int *)(ctx + 0x62c) = *(int *)(unit + 0x110);
      *(int *)(ctx + 0x658) = *(int *)(unit + 0x20);
      if (*(char *)(ctx + 0x41) == '\0' || *(int *)(unit + 0x8c) == -1) {
        *(vector3_t *)(ctx + 0x61c) = *(vector3_t *)(unit + 0x104);
      } else {
        *(vector3_t *)(ctx + 0x61c) = *(vector3_t *)(unit + 0x90);
      }
      if (*(short *)(unit + 0x24) >= 4 && *(short *)(unit + 0x24) <= 5) {
        *(char *)(ctx + 0x648) = 1;
        *(vector3_t *)(ctx + 0x64c) = *(vector3_t *)(unit + 0x40);
      }
      if (*(short *)(ctx + 0x4) == 4 || *(short *)(ctx + 0x4) == 6)
        *(char *)(ctx + 0x628) = 1;
      else
        *(char *)(ctx + 0x628) = 0;
    }
  }

  /* aim / forward axis validity (ctx+0x5dc / +0x5ec) from the actr tag. */
  if (*(float *)(actor_tag + 0xac) * *(float *)(actor_tag + 0xac) +
        *(float *)(actor_tag + 0xa8) * *(float *)(actor_tag + 0xa8) +
        *(float *)(actor_tag + 0xa4) * *(float *)(actor_tag + 0xa4) >
      *(float *)0x253f44) {
    *(char *)(ctx + 0x5dc) = 1;
    *(int *)(ctx + 0x5e0) = *(int *)(actor_tag + 0xa4);
    *(int *)(ctx + 0x5e4) = *(int *)(actor_tag + 0xa8);
    *(int *)(ctx + 0x5e8) = *(int *)(actor_tag + 0xac);
  } else if (*(float *)(actor_tag + 0x3c) * *(float *)(actor_tag + 0x3c) +
               *(float *)(actor_tag + 0x38) * *(float *)(actor_tag + 0x38) +
               *(float *)(actor_tag + 0x34) * *(float *)(actor_tag + 0x34) >
             *(float *)0x253f44) {
    *(char *)(ctx + 0x5dc) = 1;
    *(int *)(ctx + 0x5e0) = *(int *)(actor_tag + 0x34);
    *(int *)(ctx + 0x5e4) = *(int *)(actor_tag + 0x38);
    *(int *)(ctx + 0x5e8) = *(int *)(actor_tag + 0x3c);
  } else {
    *(char *)(ctx + 0x5dc) = 0;
  }

  if (*(float *)(actor_tag + 0xb8) * *(float *)(actor_tag + 0xb8) +
        *(float *)(actor_tag + 0xb4) * *(float *)(actor_tag + 0xb4) +
        *(float *)(actor_tag + 0xb0) * *(float *)(actor_tag + 0xb0) >
      *(float *)0x253f44) {
    *(char *)(ctx + 0x5ec) = 1;
    *(int *)(ctx + 0x5f0) = *(int *)(actor_tag + 0xb0);
    *(int *)(ctx + 0x5f4) = *(int *)(actor_tag + 0xb4);
    *(int *)(ctx + 0x5f8) = *(int *)(actor_tag + 0xb8);
  } else if (*(float *)(actor_tag + 0x48) * *(float *)(actor_tag + 0x48) +
               *(float *)(actor_tag + 0x44) * *(float *)(actor_tag + 0x44) +
               *(float *)(actor_tag + 0x40) * *(float *)(actor_tag + 0x40) >
             *(float *)0x253f44) {
    *(char *)(ctx + 0x5ec) = 1;
    *(int *)(ctx + 0x5f0) = *(int *)(actor_tag + 0x40);
    *(int *)(ctx + 0x5f4) = *(int *)(actor_tag + 0x44);
    *(int *)(ctx + 0x5f8) = *(int *)(actor_tag + 0x48);
  } else {
    *(char *)(ctx + 0x5ec) = 0;
  }

  if (*(short *)(actor + 0x280) > 0 && *(char *)(actor + 0x287) != '\0' &&
      *(float *)(actor + 0x2d4) <
        *(float *)(actor + 0x2d8) + *(float *)0x254644)
    *(char *)(ctx + 0x40) = 1;
  *(char *)(ctx + 0x44) = *(char *)(actor + 0x99);
  if (*(short *)(actor + 0x15e) == 4) {
    *(char *)(ctx + 0x45) = 1;
    *(char *)(ctx + 0x46) = 1;
  }

  *(int *)(ctx + 0x50) = 0;
  if (*(char *)(actor + 0x3d8) != '\0') {
    *(int *)(ctx + 0x58) = *(int *)(actor + 0x3dc);
    *(int *)(ctx + 0x5c) = *(int *)(actor + 0x3e0);
    *(int *)(ctx + 0x60) = *(int *)(actor + 0x3e4);
    *(int *)(ctx + (*(int *)(ctx + 0x50) << 4) + 0x54) =
      *(int *)(actor_tag + 0x3c8);
    *(int *)(ctx + 0x50) = *(int *)(ctx + 0x50) + 1;
  }

  /* group-scan: pull nearby allied prop positions into the ctx+0x54 list. */
  if (*(float *)(actor_tag + 0x3cc) > *(float *)0x2533c0 &&
      (mode = *(short *)(ctx + 0x4), mode == 0 || mode == 3 || mode == 6)) {
    FUN_00064540(prop_iter, actor_handle);
    while (*(int *)(ctx + 0x50) < 0x20) {
      int it = FUN_00064570(prop_iter);
      if (it == 0)
        break;
      if (*(short *)(it + 0x24) >= 2 && *(short *)(it + 0x24) <= 3 &&
          *(char *)(it + 0x60) == '\0' && *(char *)(it + 0x127) == '\0' &&
          *(char *)(it + 0x12e) == '\0') {
        *(vector3_t *)(ctx + (*(int *)(ctx + 0x50) << 4) + 0x58) =
          *(vector3_t *)(it + 0xbc);
        *(int *)(ctx + (*(int *)(ctx + 0x50) << 4) + 0x54) =
          *(int *)(actor_tag + 0x3cc);
        *(int *)(ctx + 0x50) = *(int *)(ctx + 0x50) + 1;
      }
    }
  }

  *(short *)(ctx + 0x254) = 0;
  *(short *)(ctx + 0x256) = 0;
  *(short *)(ctx + 0x258) = 0;

  {
    char prop_scan = 0;
    if (*(int *)actor_tag < 0 && *(short *)(actor + 0x6e) >= 3 &&
        *(char *)(actor + 0x200) > '\0')
      prop_scan = 1;
    if ((*(unsigned int *)(actor_tag + 4) & 1) != 0 &&
        *(short *)(actor + 0x6e) >= 3)
      prop_scan = 1;

    switch ((int)*(short *)(ctx + 0x4)) {
    case 0:
    case 2:
    case 3:
    case 6:
      if (prop_scan) {
        FUN_00064540(prop_iter, actor_handle);
        while (*(short *)(ctx + 0x254) < 0x20) {
          int it = FUN_00064570(prop_iter);
          if (it == 0)
            break;
          if (*(short *)(it + 0x24) >= 2 && *(short *)(it + 0x24) <= 3 &&
              *(char *)(it + 0x127) == '\0') {
            if (*(char *)(it + 0x60) == '\0') {
              if (*(char *)(it + 0x12e) != '\0' || *(int *)(it + 0x110) == -1) {
                if (actor_perception_friend_prop_is_attacking(
                      actor_handle, prop_iter[0], vtmp) != '\0') {
                  *(short *)(ctx + *(short *)(ctx + 0x254) * 0x1c + 0x25c) =
                    (unsigned short)(*(char *)(it + 0x12e) != '\0');
                  *(vector3_t *)(ctx + *(short *)(ctx + 0x254) * 0x1c + 0x260) =
                    *(vector3_t *)(it + 0xbc);
                  *(vector3_t *)(ctx + *(short *)(ctx + 0x254) * 0x1c + 0x26c) =
                    *(vector3_t *)vtmp;
                  *(short *)(ctx + 0x254) = *(short *)(ctx + 0x254) + 1;
                  *(short *)(ctx + 0x256) = *(short *)(ctx + 0x256) + 1;
                }
              }
              if (*(char *)(it + 0x60) == '\0')
                goto prop_scan_next;
            }
            if ((*(char *)(it + 0xa4) != '\0' &&
                 (*(char *)(it + 0x12e) != '\0' ||
                  *(char *)(it + 0x12f) != '\0')) ||
                (*(unsigned int *)(actor_tag + 4) & 1) != 0) {
              int aim;
              aim = *(int *)(it + 0x110);
              if (aim == -1)
                aim = *(int *)(it + 0x18);
              *(short *)(ctx + *(short *)(ctx + 0x254) * 0x1c + 0x25c) = 2;
              *(vector3_t *)(ctx + *(short *)(ctx + 0x254) * 0x1c + 0x260) =
                *(vector3_t *)(it + 0xbc);
              unit_scripting_unit_driver(
                aim, ctx + *(short *)(ctx + 0x254) * 0x1c + 0x26c);
              *(short *)(ctx + 0x254) = *(short *)(ctx + 0x254) + 1;
              *(short *)(ctx + 0x258) = *(short *)(ctx + 0x258) + 1;
            }
          }
        prop_scan_next:;
        }
      }
      break;
    default:
      break;
    }
  }

  *(short *)(ctx + 0x66e) = 0;
  *(short *)(ctx + 0x66c) = 0;
  *(short *)(ctx + 0x66a) = 0;
  *(short *)(ctx + 0x668) = 0;
  *(short *)(ctx + 0x666) = 0;
  *(short *)(ctx + 0x664) = 0;

  if (debug_eval) {
    *(char *)0x629d40 = 1;
    memcpy((void *)0x629d44, ctx, 0x19c * 4);
  }

  if (huge_buf == (void *)0) {
    display_assert("area_path_state_valid",
                   "c:\\halo\\SOURCE\\ai\\actor_firing_position.c", 0x726, 1);
    system_exit(-1);
  }
  *(char *)out_found = 0;

  /* walk the encounter firing positions and build candidate records. */
  debug_overflow = 0;
  rec_count = 0;
  fp_index = 0;
  if (*(int *)(encounter_atoms + 0x98) > 0) {
    do {
      fp = tag_block_get_element(encounter_atoms + 0x98, fp_index, 0x18);
      if (debug_eval)
        *(char *)(0x62a3b5 + fp_index * 0x40) = 0;
      if ((*(unsigned int *)ctx &
           (1u << (*(unsigned char *)((char *)fp + 0xc) & 0x1f))) != 0) {
        if (*(int *)(actor + 0x34) == *(int *)0x5ac9f4)
          *(char *)(0x62a3b4 + fp_index * 0x40) = (*(short *)(ctx + 0x4) == 5);
        if (*(char *)(ctx + 0x44) != '\0' ||
            *(int *)((char *)fp + 0x14) != -1) {
          int keep = 1;
          if (*(short *)(ctx + 0x4) == 5) {
            if (actor_has_accessible_firing_position(
                  actor_handle, (float *)fp, *(int *)((char *)fp + 0x14), 1) ==
                '\0')
              keep = 0;
          }
          if (keep) {
            if (owner_indices[fp_index] != -1) {
              float dx, dy, dz, downer, dself;
              if (*(short *)(ctx + 0x4) == 4 && *(int *)(ctx + 0x50) < 0x20) {
                *(vector3_t *)(ctx + (*(int *)(ctx + 0x50) << 4) + 0x58) =
                  *(vector3_t *)fp;
                *(int *)(ctx + (*(int *)(ctx + 0x50) << 4) + 0x54) = 0x40800000;
                *(int *)(ctx + 0x50) = *(int *)(ctx + 0x50) + 1;
              }
              {
                char *owner =
                  (char *)datum_get(actor_data, owner_indices[fp_index]);
                dx = *(float *)fp - *(float *)(owner + 0x12c);
                dy = *(float *)((char *)fp + 4) - *(float *)(owner + 0x130);
                dz = *(float *)((char *)fp + 8) - *(float *)(owner + 0x134);
              }
              downer = xbox_sqrtf(dx * dx + dy * dy + dz * dz);
              dx = *(float *)fp - *(float *)(actor + 0x12c);
              dy = *(float *)((char *)fp + 4) - *(float *)(actor + 0x130);
              dz = *(float *)((char *)fp + 8) - *(float *)(actor + 0x134);
              if (downer < *(float *)0x2533c8)
                goto fp_next;
              dself = xbox_sqrtf(dx * dx + dy * dy + dz * dz);
              if (downer < dself + dself)
                goto fp_next;
            }
            if ((short)rec_count < 0x200) {
              char *fwd = *(char **)0x31fc38;
              int ri = (short)rec_count * 0x3c;
              *(short *)(records + ri + 0x4) = (unsigned short)fp_index;
              *(int *)(records + ri + 0x0) = (int)fp;
              *(short *)(records + ri + 0x6) = 0;
              *(int *)(records + ri + 0x8) = 0x7f7fffff;
              *(vector3_t *)(records + ri + 0xc) = *(vector3_t *)fwd;
              *(int *)(records + ri + 0x18) = 0x7f7fffff;
              *(int *)(records + ri + 0x1c) = 0x7f7fffff;
              *(vector3_t *)(records + ri + 0x20) = *(vector3_t *)fwd;
              *(int *)(records + ri + 0x2c) = 0;
              *(int *)(records + ri + 0x34) = 0;
              *(int *)(records + ri + 0x38) = 0;
              *(char *)(records + ri + 0x30) = 1;
              *(char *)(records + ri + 0x31) = 0;
              rec_count++;
            } else if (debug_overflow == '\0') {
              error(2, "encounter %s has too many firing positions (%d > %d)",
                    encounter_atoms, *(int *)(encounter_atoms + 0x98), 0x200);
              debug_overflow = 1;
            }
          }
        }
      }
    fp_next:
      fp_index++;
      fp_index = (short)fp_index;
    } while (fp_index < *(int *)(encounter_atoms + 0x98));
  }

  *(short *)(ctx + 0x664) = *(short *)(encounter_atoms + 0x98);
  *(short *)(ctx + 0x666) = (short)rec_count;

  if ((short)rec_count == 0) {
    char *a = (char *)datum_get(actor_data, actor_handle);
    short *p;
    *(short *)(a + 0x3c6) = 0;
    p = (short *)(a + 0x3ca);
    for (j = 4; j != 0; j--) {
      *p = (short)0xffff;
      p += 2;
    }
    if (*(char *)(a + 0x3d8) != '\0')
      *(char *)(a + 0x3d8) = 0;
    return (short)selected;
  }

  /* refine candidate clearance against the target. */
  if (*(char *)(ctx + 0x5fc) != '\0' && *(char *)(ctx + 0x42) != '\0') {
    if (*(char *)(ctx + 0x44) == '\0') {
      if (*(int *)(ctx + 0x630) != -1) {
        path_input_new(path_input, *(int *)(actor_tag + 0x8c),
                       *(unsigned char *)(actor + 0x376), -1);
        path_input_set_start(path_input, (float *)(ctx + 0x634),
                             *(int *)(ctx + 0x630));
        path_input_set_search_bounds(path_input, 0x41a00000);
        path_state_new(path_input, path_eval_scratch, (void *)0);
        FUN_0005ff70((unsigned int *)path_eval_scratch);
        if ((short)rec_count > 0) {
          char *rec = records;
          unsigned int n = (unsigned int)(unsigned short)(short)rec_count;
          do {
            float *clr;
            float *pos = *(float **)rec;
            if (*(char *)(ctx + 0x43) == '\0')
              clr = (float *)0;
            else
              clr = (float *)(rec + 0x20);
            path_state_estimated_distance(
              path_eval_scratch, pos, *(int *)((char *)pos + 0x14),
              (float *)(rec + 0x18), (float *)0, clr);
            rec += 0x3c;
            n--;
          } while (n != 0);
        }
      }
    } else {
      void *scn = scenario_get();
      if ((short)rec_count > 0) {
        char *rec = records;
        unsigned int n = (unsigned int)(unsigned short)(short)rec_count;
        do {
          float *pos = *(float **)rec;
          vtmp[0] = pos[0] - *(float *)(ctx + 0x604);
          vtmp[1] = pos[1] - *(float *)(ctx + 0x608);
          vtmp[2] = pos[2] - *(float *)(ctx + 0x60c);
          if (vtmp[1] * vtmp[1] + vtmp[2] * vtmp[2] + vtmp[0] * vtmp[0] <
              *(float *)0x254f90) {
            if (path_3d_available((int)scn, (int *)(ctx + 0x604), 0, (int *)pos,
                                  (unsigned char *)0, (float *)0) != '\0') {
              *(float *)(rec + 0x18) = normalize3d(vtmp);
              if (*(char *)(ctx + 0x43) != '\0') {
                *(int *)(rec + 0x20) = *(int *)&vtmp[0];
                *(int *)(rec + 0x24) = *(int *)&vtmp[1];
                *(int *)(rec + 0x28) = *(int *)&vtmp[2];
              }
            }
          }
          rec += 0x3c;
          n--;
        } while (n != 0);
      }
    }
  }

  /* build the actor path input, then clearance-test every candidate. */
  if (*(char *)(ctx + 0x44) == '\0') {
    actor_path_input_new(actor_handle, path_input);
    path_input_set_search_bounds(path_input, *(int *)(ctx + 0x1c));
    if (*(char *)(ctx + 0x36) != '\0' && *(char *)(ctx + 0x5fc) != '\0') {
      int aim;
      aim = -1;
      if (*(int *)(ctx + 0x644) != -1) {
        char *u2 =
          (char *)datum_get(*(data_t **)0x5ab23c, *(int *)(ctx + 0x644));
        aim = *(int *)(u2 + 0x18);
      }
      path_input_set_attractor(path_input, (float *)(ctx + 0x604),
                               *(float *)(ctx + 0x3c), aim,
                               *(float *)(ctx + 0x38));
    } else {
      if (*(short *)(actor + 0x280) > 0 &&
          (*(unsigned int *)(actor_tag + 4) & 0x10) == 0)
        path_input_set_attractor(path_input, (float *)(actor + 0x2b0),
                                 *(float *)(actor + 0x294),
                                 *(unsigned int *)(actor + 0x28c), 10.0f);
    }
    path_state_new(path_input, huge_buf,
                   ai_debug_get_path_storage(actor_handle));
    if (FUN_0005ff70((unsigned int *)huge_buf) != '\0')
      *(char *)out_found = 1;
  }

  /* re-score each candidate against actor position / path clearance. */
  range_flag = 0;
  if ((short)rec_count > 0) {
    char *rec = records;
    unsigned int n = (unsigned int)(unsigned short)(short)rec_count;
    do {
      float *pos = *(float **)rec;
      if (*(char *)(ctx + 0x5fc) != '\0') {
        float tx = pos[0] - *(float *)(ctx + 0x604);
        float ty = pos[1] - *(float *)(ctx + 0x608);
        float tz = pos[2] - *(float *)(ctx + 0x60c);
        *(float *)(rec + 0x2c) = tz * tz + ty * ty + tx * tx;
      }
      dir[0] = pos[0] - *(float *)(actor + 0x12c);
      dir[1] = pos[1] - *(float *)(actor + 0x130);
      dir[2] = pos[2] - *(float *)(actor + 0x134);
      if (dir[2] * dir[2] + dir[1] * dir[1] + dir[0] * dir[0] <
          *(float *)(ctx + 0x1c) * *(float *)(ctx + 0x1c)) {
        if (*(char *)(ctx + 0x44) == '\0') {
          path_state_estimated_distance(
            huge_buf, pos, *(int *)((char *)pos + 0x14), (float *)(rec + 0x8),
            (float *)(rec + 0x1c),
            (*(char *)(ctx + 0x40) != '\0') ? (float *)(rec + 0xc) :
                                              (float *)0);
        } else {
          float mag;
          float mag_kept;
          *(float *)(rec + 0x1c) = xbox_sqrtf(FUN_0010cd40(
            (float *)(ctx + 0x604), (float *)(actor + 0x12c), dir));
          mag = xbox_sqrtf(dir[2] * dir[2] + dir[1] * dir[1] + dir[0] * dir[0]);
          mag_kept = *(float *)0x2533c0;
          if ((double)xbox_fabsf(mag) >= *(double *)0x2533d0) {
            float inv = *(float *)0x2533c8 / mag;
            dir[0] = dir[0] * inv;
            dir[1] = dir[1] * inv;
            dir[2] = dir[2] * inv;
            mag_kept = mag;
          }
          *(float *)(rec + 0x8) = mag_kept;
          if (*(char *)(ctx + 0x40) != '\0') {
            *(int *)(rec + 0xc) = *(int *)&dir[0];
            *(int *)(rec + 0x10) = *(int *)&dir[1];
            *(int *)(rec + 0x14) = *(int *)&dir[2];
          }
        }
      }
      if (*(float *)(rec + 0x8) < *(float *)(ctx + 0x1c))
        range_flag = 1;
      else
        *(char *)(rec + 0x30) = 0;
      rec += 0x3c;
      n--;
    } while (n != 0);
  }

  if (*(char *)(ctx + 0x15) != '\0' && range_flag == 0) {
    /* nothing in range and the actor must move: pick a random fallback. */
    int rec_idx;
    char *rec;
    if ((short)rec_count < 1) {
      display_assert("firing_position_count > 0",
                     "c:\\halo\\SOURCE\\ai\\actor_firing_position.c", 0x83c, 1);
      system_exit(-1);
    }
    rec_idx = random_range((unsigned int *)get_global_random_seed_address(), 0,
                           (short)rec_count);
    *(char *)out_found = 0;
    selected = rec_idx;
    {
      char *a = (char *)datum_get(actor_data, actor_handle);
      short *p;
      *(short *)(a + 0x3c6) = 0;
      p = (short *)(a + 0x3ca);
      for (j = 4; j != 0; j--) {
        *p = (short)0xffff;
        p += 2;
      }
      if (*(char *)(a + 0x3d8) != '\0')
        *(char *)(a + 0x3d8) = 0;
    }
    rec = records + (short)rec_idx * 0x3c;
    if (FUN_00025970(rec, actor_handle, ctx) == '\0')
      selected = -1;
    if (debug_eval) {
      short ridx = *(short *)(rec + 0x4);
      *(char *)(0x62a3b5 + ridx * 0x40) = 1;
      memcpy((void *)(0x62a3b8 + ridx * 0x40), rec, 0xf * 4);
    }
  } else {
    /* normal path: run scorers, sort, select the best valid record. */
    char too_far;
    too_far = 0;
    {
      void **tbl = (void **)0x254bf8;
      if (*tbl != (void *)0) {
        do {
          if (((int)*(short *)((char *)tbl - 4) &
               (1 << (*(unsigned char *)(ctx + 4) & 0x1f))) != 0) {
            ((void (*)(int, void *, int, void *)) * tbl)(actor_handle, ctx,
                                                         rec_count, records);
          }
          tbl += 2;
        } while (*tbl != (void *)0);
      }
    }
    if ((short)rec_count > 0) {
      unsigned int n = (unsigned int)(unsigned short)(short)rec_count;
      i = 0;
      do {
        order[i] = i;
        i++;
        n--;
      } while (n != 0);
    }
    *(short *)0x331f00 = (short)rec_count;
    *(int *)0x331f04 = (int)records;
    FUN_00091ef0(order, (short)rec_count, FUN_00024950);
    *(char *)(ctx + 0x65c) = FUN_00024900(actor_handle, ctx);

    i = 0;
    if ((short)rec_count > 0) {
      do {
        int ridx = (short)*(unsigned short *)&order[(short)i];
        char *rec = records + ridx * 0x3c;
        if (*(char *)(rec + 0x30) != '\0')
          *(short *)(ctx + 0x668) = *(short *)(ctx + 0x668) + 1;
        if (*(char *)(rec + 0x31) == '\0')
          *(short *)(ctx + 0x66a) = *(short *)(ctx + 0x66a) + 1;
        if (*(char *)(rec + 0x30) == '\0' ||
            (*(char *)(ctx + 0x65c) != '\0' &&
             *(float *)(rec + 0x38) + *(float *)(ctx + 0x660) <= best)) {
          if (game_connection() != 0 || *(char *)0x5ac9c5 == '\0') {
            if ((short)i < (short)rec_count) {
              int *o = &order[(short)i];
              unsigned int m = (unsigned int)((rec_count - i) & 0xffff);
              do {
                if (*(char *)(records + (*o) * 0x3c + 0x30) != '\0') {
                  *(short *)(ctx + 0x668) = *(short *)(ctx + 0x668) + 1;
                  *(short *)(ctx + 0x66e) = *(short *)(ctx + 0x66e) + 1;
                }
                o++;
                m--;
              } while (m != 0);
            }
            break;
          }
          too_far = 1;
        }
        if (*(char *)(rec + 0x30) != '\0') {
          if (*(char *)(ctx + 0x5fc) != '\0')
            FUN_000257a0(actor_handle, rec, ctx);
          *(short *)(ctx + 0x66c) = *(short *)(ctx + 0x66c) + 1;
          *(int *)(rec + 0x34) = *(int *)(rec + 0x38);
          if (FUN_00024890(actor_handle, rec, ctx) != '\0' &&
              best < *(float *)(rec + 0x38)) {
            if (too_far) {
              display_assert("!expected_to_discard",
                             "c:\\halo\\SOURCE\\ai\\actor_firing_position.c",
                             0x8c3, 1);
              system_exit(-1);
            }
            selected = ridx;
            best = *(float *)(rec + 0x38);
          }
        }
        i++;
      } while ((short)i < (short)rec_count);
    }

    if (debug_eval && (short)rec_count > 0) {
      char *rec = records;
      unsigned int n = (unsigned int)(unsigned short)(short)rec_count;
      do {
        short ridx = *(short *)(rec + 0x4);
        *(char *)(0x62a3b5 + ridx * 0x40) = 1;
        memcpy((void *)(0x62a3b8 + ridx * 0x40), rec, 0xf * 4);
        rec += 0x3c;
        n--;
      } while (n != 0);
    }

    if (*(char *)0x5aca4e != '\0') {
      names[0] = "fight";
      names[1] = "panic";
      names[2] = "cover";
      names[3] = "uncover";
      names[4] = "guard";
      names[5] = "pursue";
      error(2,
            "fp-eval %s: encounter %3d consider %3d valid %3d nonrejected"
            " %3d post-eval %3d skipped %3d",
            names[*(short *)(ctx + 0x4)], (int)*(short *)(ctx + 0x664),
            (int)*(short *)(ctx + 0x666), (int)*(short *)(ctx + 0x668),
            (int)*(short *)(ctx + 0x66a), (int)*(short *)(ctx + 0x66c),
            (int)*(short *)(ctx + 0x66e));
    }
  }

  /* publish the selection. */
  if ((short)selected != -1) {
    char *rec = records + (short)selected * 0x3c;
    short fp_idx;
    if (out_record != (int *)0)
      memcpy(out_record, rec, 0xf * 4);
    fp_idx = *(short *)(rec + 0x4);
    if (out_owner != (int *)0)
      *out_owner = owner_indices[fp_idx];
    return fp_idx;
  }
  return (short)selected;
}

/* FUN_00027090 (0x27090) — Select firing position for actor.
 * Sets up eval_ctx group bitmasks, calls FUN_00025c10; falls back to
 * cached actor fp_index on miss.
 * Confirmed: allowed_groups from actor_get_firing_position_group 0/2/1.
 * Confirmed: ctx[0]=total, ctx[0x12]=allowed, ctx[0x13]=0x41000000. */
short FUN_00027090(int actor_handle, void *param_2, void *param_3,
                   void *param_4, void *param_5, void *param_6)
{
  char *actor;
  unsigned int allowed;
  unsigned int gb;
  unsigned int total;
  int one;
  short fp_idx;
  short existing_fp;
  int iVar8;
  float *def;

  actor = (char *)datum_get(actor_data, actor_handle);
  one = 1;
  if (*(int *)(actor + 0x34) != -1) {
    allowed = (unsigned int)actor_get_firing_position_group(
      actor_handle, (short)(((unsigned int *)param_2)[one]), 0);
    gb = (unsigned int)actor_get_firing_position_group(
      actor_handle, (short)(((unsigned int *)param_2)[one]), 2);
    total = (unsigned int)actor_get_firing_position_group(
      actor_handle, (short)(((unsigned int *)param_2)[1]), 1);
    total = total | gb;

    if (allowed < total) {
      ((unsigned int *)param_2)[0x12] = allowed;
      ((unsigned int *)param_2)[0x13] = 0x41000000u;
      if ((total & allowed) != allowed) {
        display_assert("(total_groups & currently_allowed_groups) == "
                       "currently_allowed_groups",
                       "c:\\halo\\SOURCE\\ai\\actor_firing_position.c", 0x907,
                       one);
        system_exit(-one);
      }
      ((unsigned int *)param_2)[0] = total;
    } else {
      ((unsigned int *)param_2)[0] = allowed;
    }

    if (*(short *)(actor + 0x3b8) == -1 || *(char *)(actor + 0x3ba) == '\0') {
      *(char *)((char *)param_2 + 0x15) = 1;
    } else {
      *(char *)((char *)param_2 + 0x15) = 0;
    }
    *(char *)((char *)param_2 + 0x14) = (char)one;

    fp_idx =
      FUN_00025c10(actor_handle, param_2, param_3, param_4, param_5, param_6);

    if (fp_idx != -1) {
      if ((allowed &
           (1u << (*(unsigned char *)(*(int *)param_3 + 0xc) & 0x1f))) == 0) {
        actor = (char *)datum_get(actor_data, actor_handle);
        *(char *)(actor + 0x98) = (*(char *)(actor + 0x98) == '\0');
      }
      return fp_idx;
    }

    existing_fp = *(short *)(actor + 0x3b8);
    if (existing_fp != -1 && *(char *)(actor + 0x3ba) != '\0') {
      iVar8 = (int)tag_block_get_element(
        (char *)global_scenario_get() + 0x42c,
        *(unsigned int *)(actor + 0x34) & 0xffff, 0xb0);
      iVar8 = (int)tag_block_get_element((char *)iVar8 + 0x98, (int)existing_fp,
                                         0x18);
      *(int *)param_3 = iVar8;
      *(short *)((char *)param_3 + 4) = existing_fp;
      *(unsigned short *)((char *)param_3 + 6) = 0;
      ((int *)param_3)[2] = 0x7f7fffff;
      ((int *)param_3)[6] = 0x7f7fffff;
      ((int *)param_3)[7] = 0x7f7fffff;
      def = *(float **)0x31fc38;
      ((int *)param_3)[3] = *(int *)(def + 0);
      ((int *)param_3)[4] = *(int *)(def + 1);
      ((int *)param_3)[5] = *(int *)(def + 2);
      def = *(float **)0x31fc38;
      ((int *)param_3)[8] = *(int *)(def + 0);
      ((int *)param_3)[9] = *(int *)(def + 1);
      ((int *)param_3)[10] = *(int *)(def + 2);
      if (*(char *)((char *)param_2 + 0x5fc) == '\0') {
        ((int *)param_3)[0xb] = 0;
      } else {
        ((int *)param_3)[0xb] = (int)distance_squared3d(
          (const float *)((char *)param_2 + 0x604), (const float *)iVar8);
      }
      if (!FUN_00025970(param_2, actor_handle, actor)) {
        existing_fp = -1;
        *(short *)(actor + 0x3b8) = -1;
      }
      *(int *)param_4 = -1;
      *(char *)param_6 = 0;
      return existing_fp;
    }
    return fp_idx;
  }
  return -one;
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
short FUN_000272d0(int actor_handle, short param_2, void *param_3, int param_4,
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

/* FUN_00027410 (0x27410) — Firing-position preferred-range and attack-vector
 * scorer.
 *
 * Source file: c:\halo\SOURCE\ai\actor_firing_position.c
 *
 * Iterates fp_count active firing positions (stride 0x3c). Entry layout:
 *   +0x00: float* position_ptr  +0x2c: float range_sq
 *   +0x30: char active_flag     +0x38: float score accumulator
 *
 * Pass 1 (ctx+0x5fc gate): weapon optimal-range score + actor preferred-range
 *   score via FUN_00024000 (type=0xd). Preferred-range min floored by
 *   ctx+0x658 and combat+0x40c; gap = min(max-range, range-min).
 * Pass 2 (ctx+0x258 > 0 gate): finds closest type-2 attack vector, projects
 *   position onto its plane (dot>0 only), scores by perpendicular distance.
 *
 * Confirmed: datum_get at 0x27421; actor_combat_get_firing_variant_definition
 *   at 0x2742a; position ptr at entry+0x0 (MOV EAX,[ECX-0x38] at 0x27734).
 * Confirmed: cdecl 4 stack args; entry stride ADD ECX,0x3c at 0x27852.
 * Confirmed: attack-vector fields at ctx+i*0x1c+{0x25c type, 0x260 origin,
 * 0x26c normal}. */
void FUN_00027410(int actor_handle, void *ctx, unsigned short fp_count,
                  void *fp_array)
{
  char *actor;
  char *variant_def;
  char *combat;
  float range;
  float score;
  float preferred_range_max;
  float preferred_range_min;
  float gap;
  float pref_score;
  float min_dist_sq;
  float attack_score;
  float dx;
  float dy;
  float dz;
  float len_sq_m1;
  unsigned int n;
  short i;

  actor = (char *)datum_get(actor_data, actor_handle);
  variant_def = actor_combat_get_firing_variant_definition(actor_handle);

  if ((short)fp_count <= 0)
    return;

  n = (unsigned int)fp_count;
  fp_array = (char *)fp_array + 0x38; /* walk fp_array as score-field ptr */

  do {
    if (*(char *)((char *)fp_array - 0x8) != '\0') { /* active at +0x30 */

      /* Pass 1: weapon preferred-range scoring */
      if (*(char *)((char *)ctx + 0x5fc) != '\0') {
        range = xbox_sqrtf(*(float *)((char *)fp_array - 0xc));

        if (0.0f < *(float *)(variant_def + 0x74)) {
          if (*(float *)(variant_def + 0x74) * *(float *)0x2533f0 <= range) {
            score =
              (*(float *)(variant_def + 0x74) * *(float *)0x2533f0 / range) *
              *(float *)0x253f34;
            if (score < 0.0f || score >= 1000.0f) {
              display_assert("(evaluation >= 0.0f) && (evaluation < 1e+03f)",
                             "c:\\halo\\SOURCE\\ai\\actor_firing_position.c",
                             0x81, 1);
              system_exit(-1);
            }
          } else {
            score = 10.0f;
          }
          *(float *)fp_array += score;
        }

        if (0.0f < *(float *)(variant_def + 0xa0) &&
            range < *(float *)(variant_def + 0xa0)) {
          if (*(char *)(actor + 0x378) != '\0') {
            preferred_range_min = *(float *)(variant_def + 0x168);
            preferred_range_max = *(float *)(variant_def + 0x16c);
          } else {
            preferred_range_min = *(float *)(variant_def + 0x9c);
            preferred_range_max = *(float *)(variant_def + 0xa0);
          }
          if (preferred_range_min <= *(float *)((char *)ctx + 0x658))
            preferred_range_min = *(float *)((char *)ctx + 0x658);

          combat = FUN_000210f0(actor_handle);
          if (combat != 0 && 0.0f < *(float *)(combat + 0x40c) &&
              preferred_range_min <= *(float *)(combat + 0x40c))
            preferred_range_min = *(float *)(combat + 0x40c);

          gap = preferred_range_max - range;
          if (0.0f < preferred_range_min && range - preferred_range_min < gap)
            gap = range - preferred_range_min;

          pref_score = 0.0f;
          if (gap > *(float *)0x253f40) {
            pref_score = 20.0f;
          } else if (0.0f < gap) {
            pref_score = gap * *(float *)0x253398 * *(float *)0x254cd0;
          }
          FUN_00024000(ctx, pref_score, 0xd, (char *)fp_array - 0x38);
        }
      }

      /* Pass 2: attack-vector perpendicular distance scoring */
      if (0 < *(short *)((char *)ctx + 0x258)) {
        min_dist_sq = 3.4028235e+38f;
        i = 0;
        if (0 < *(short *)((char *)ctx + 0x254)) {
          do {
            if (*(short *)((char *)ctx + i * 0x1c + 0x25c) == 2) {
              char *av;
              float *pos;
              float dot;
              av = (char *)ctx + i * 0x1c;
              len_sq_m1 = *(float *)(av + 0x26c) * *(float *)(av + 0x26c) +
                          *(float *)(av + 0x270) * *(float *)(av + 0x270) +
                          *(float *)(av + 0x274) * *(float *)(av + 0x274) -
                          *(float *)0x2533c8;
              if (((*(unsigned int *)&len_sq_m1 & 0x7f800000) == 0x7f800000) ||
                  *(float *)0x2549d8 <= fabsf(len_sq_m1)) {
                display_assert(
                  csprintf((char *)0x5ab100,
                           "%s: assert_valid_real_normal3d(%f, %f, %f)",
                           "&evaluation_context->attack_vectors"
                           "[vector_index].vector",
                           (double)*(float *)(av + 0x26c),
                           (double)*(float *)(av + 0x270),
                           (double)*(float *)(av + 0x274)),
                  "c:\\halo\\SOURCE\\ai\\actor_firing_position.c", 0x291, 1);
                system_exit(-1);
              }
              pos = *(float **)((char *)fp_array - 0x38);
              dx = pos[0] - *(float *)(av + 0x260);
              dy = pos[1] - *(float *)(av + 0x264);
              dz = pos[2] - *(float *)(av + 0x268);
              dot = dx * *(float *)(av + 0x26c) + dz * *(float *)(av + 0x274) +
                    dy * *(float *)(av + 0x270);
              if (0.0f < dot) {
                float neg_dot, px, py, pz, perp_sq;
                neg_dot = -dot;
                px = neg_dot * *(float *)(av + 0x26c) + dx;
                py = neg_dot * *(float *)(av + 0x270) + dy;
                pz = neg_dot * *(float *)(av + 0x274) + dz;
                perp_sq = px * px + pz * pz + py * py;
                if (perp_sq < min_dist_sq)
                  min_dist_sq = perp_sq;
              }
            }
            i++;
          } while (i < *(short *)((char *)ctx + 0x254));
        }
        attack_score = 6.0f;
        if (min_dist_sq < *(float *)0x255094) {
          attack_score = xbox_sqrtf(min_dist_sq) * *(float *)0x25337c;
          if (attack_score < 0.0f || attack_score >= 1000.0f) {
            display_assert("(evaluation >= 0.0f) && (evaluation < 1e+03f)",
                           "c:\\halo\\SOURCE\\ai\\actor_firing_position.c",
                           0x81, 1);
            system_exit(-1);
          }
        }
        *(float *)fp_array += attack_score;
      }
    }

    fp_array = (char *)fp_array + 0x3c;
    n--;
  } while (n != 0);
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

/* FUN_00027a60 (0x27a60)
 * Set an actor's secondary scripted-look target.
 *
 * Validates look_type (0-13), checks priority against current look state,
 * optionally selects a prop object and applies cooldown/speed limits, then
 * writes the look spec (type, priority, tick-count, look_buf) to actor+0x544.
 * Returns 1 on success, 0 if blocked by priority or state conditions.
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x27a74.
 * Confirmed: tag_get(0x61637472, actor+0x58) at 0x27a7f.
 * Confirmed: assert (type >= 0) && (type < 14) at line 0x87 actor_looking.c.
 * Confirmed: actor+0x3e8 >= 7 check (SETGE) stored to local_5 at 0x27ae6.
 * Confirmed: MOV word [ESI+0x546],DI; MOV word [ESI+0x548],BX;
 *   MOV word [ESI+0x544],DX at 0x27d8b-0x27d99.
 * Confirmed: 16-byte copy from param_4 to actor+0x54c (4 dwords) at
 * 0x27da0-0x27db9. Confirmed: debug type_names[14] at EBP-0x64, prio_names[9]
 * at EBP-0x34; literal XBE addresses 0x2551e0..0x255178 (type) and
 * 0x255244..0x2551ec (prio). */
int FUN_00027a60(int actor_handle, short look_type, short priority,
                 short *look_buf)
{
  char *actor;
  char *tag;
  char *prop;
  float scale;
  float rng_min;
  float rng_max;
  int tick_count;
  char is_high_level;
  int *seed;
  char *type_names[12];
  char *prio_names[9];

  actor = (char *)datum_get(actor_data, actor_handle);
  tag = (char *)tag_get(0x61637472, *(int *)(actor + 0x58));

  if (look_type < 0 || look_type >= 0xe) {
    display_assert("(type >= 0) && (type < NUMBER_OF_SECONDARY_LOOK_TYPES)",
                   "c:\\halo\\SOURCE\\ai\\actor_looking.c", 0x87, 1);
    system_exit(-1);
  }

  /* Priority gate: skip if look_type is low and actor already has
   * higher-priority look */
  if (*(short *)(actor + 0x6a) <= 1 && look_type < 0xd) {
    return 0;
  }
  if (*(short *)(actor + 0x544) > look_type) {
    return 0;
  }

  is_high_level = (char)(*(short *)(actor + 0x3e8) >= 7);

  /* Additional gate for types < 13: special actor state checks */
  if (look_type < 0xd) {
    if (*(short *)(actor + 0x6c) == 0xb && *(char *)(actor + 0x9f) == '\0') {
      return 0;
    }
  }

  if (is_high_level && look_type < 4) {
    return 0;
  }

  /* If look_buf[0] == 1: prop-based look — check prop state and cooldown */
  if (*(short *)look_buf == 1) {
    prop = (char *)datum_absolute_index_to_index(*(data_t **)0x5ab23c,
                                                 *(int *)(look_buf + 2));
    if (prop == (char *)0) {
      return 0;
    }
    if (look_type < 8) {
      /* Visibility/awareness gate: must have flags+awareness to proceed */
      if (*(char *)(prop + 0x60) != '\0' || *(char *)(prop + 0x127) != '\0') {
        if (*(char *)(prop + 0x127) == '\0') {
          goto after_prop_check;
        }
        if (*(short *)(actor + 0x6a) < 3) {
          goto after_prop_check;
        }
      }
      /* Cooldown check */
      tick_count = game_time_get();
      if (is_high_level) {
        return 0;
      }
      if (*(char *)(prop + 0x12e) != '\0' && look_type >= 4) {
        goto do_update_timer;
      }
      if (*(int *)(prop + 0x5c) == -1 ||
          *(int *)(prop + 0x5c) + 600 <= tick_count) {
        goto do_update_timer;
      }
      return 0;

    do_update_timer:
      *(int *)(prop + 0x5c) = tick_count;
      *(float *)(prop + 0x58) =
        *(float *)(prop + 0x58) > *(float *)(prop + 0x54) ?
          *(float *)(prop + 0x58) :
          *(float *)(prop + 0x54);
    }
  }

after_prop_check:
  /* Load base scale from table by look_type */
  scale = *(float *)((char *)0x25510c + (int)look_type * 4);

  /* Double scale if actor awareness < 3 or actor+0x6e == 0 */
  if (*(short *)(actor + 0x6a) < 3 || *(short *)(actor + 0x6e) == 0) {
    scale = scale + scale;
  }

  /* Apply random scale from tag if tag+0xd4 or tag+0xd8 is non-zero */
  if (*(float *)(tag + 0xd4) != *(float *)0x2533c0 ||
      *(float *)(tag + 0xd8) != *(float *)0x2533c0) {
    /* Floor min at 0.5f */
    if (*(float *)(tag + 0xd4) > *(float *)0x253398) {
      rng_min = *(float *)(tag + 0xd4);
    } else {
      rng_min = 0.5f;
    }
    /* Ceiling max at 2.0f */
    if (*(float *)(tag + 0xd8) > *(float *)0x253f40) {
      rng_max = 2.0f;
    } else {
      rng_max = *(float *)(tag + 0xd8);
    }
    seed = get_global_random_seed_address();
    scale = scale * random_real_range(seed, rng_min, rng_max);
  }

  /* Convert scale to tick count (int), capped at 0x7fff */
  rng_min = scale * *(float *)0x253394;
  tick_count = (int)rng_min;
  if (tick_count > 0x7fff) {
    tick_count = 0x7fff;
  }

  /* Remap priority=1 from table indexed by [look_type, actor+0x6e>=4] */
  if (priority == 1) {
    priority = *(short *)((char *)0x2550d4 +
                          ((int)(unsigned char)(*(short *)(actor + 0x6e) >= 4) +
                           (int)look_type * 2) *
                            2);
  }

  /* Debug output */
  if (*(char *)0x5aca5d != '\0') {
    type_names[0] = (char *)0x254384;
    type_names[1] = (char *)0x2551e0;
    type_names[2] = (char *)0x2551d4;
    type_names[3] = (char *)0x2551cc;
    type_names[4] = (char *)0x2551c0;
    type_names[5] = (char *)0x2551b0;
    type_names[6] = (char *)0x2551a4;
    type_names[7] = (char *)0x255194;
    type_names[8] = (char *)0x255188;
    type_names[9] = (char *)0x255180;
    type_names[10] = (char *)0x255178;
    type_names[11] = (char *)0x25516c;

    prio_names[0] = (char *)0x254384;
    prio_names[1] = (char *)0x255244;
    prio_names[2] = (char *)0x255238;
    prio_names[3] = (char *)0x25522c;
    prio_names[4] = (char *)0x255228;
    prio_names[5] = (char *)0x255218;
    prio_names[6] = (char *)0x255208;
    prio_names[7] = (char *)0x2551fc;
    prio_names[8] = (char *)0x2551ec;

    console_printf(
      0, (char *)0x255158,
      ai_debug_describe_actor(actor_handle, -1, 0, (char *)0x5ab100, 0x100),
      type_names[(int)look_type], prio_names[(int)priority],
      (int)(short)tick_count);
  }

  /* Write look spec to actor */
  *(short *)(actor + 0x546) = priority;
  *(short *)(actor + 0x548) = (short)tick_count;
  *(short *)(actor + 0x544) = look_type;
  *(int *)(actor + 0x54c) = *(int *)look_buf;
  *(int *)(actor + 0x550) = *(int *)(look_buf + 2);
  *(int *)(actor + 0x554) = *(int *)(look_buf + 4);
  *(int *)(actor + 0x558) = *(int *)(look_buf + 6);
  return 1;
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

/* FUN_00028cc0 (0x28cc0)
 * Primary look update: evaluates whether the actor should be looking/aiming
 * at a target within angular constraints, then writes the constrained direction
 * to actor+0x570 and updates the look-spec via FUN_00028250.
 *
 * Confirmed: EAX = actor_handle (register arg, invisible to Ghidra);
 *   look_vectors = param_1 (float*); dir_vec = param_2 (float*);
 *   look_mode = param_3 (char); is_aim = param_4 (char);
 *   is_secondary = param_5 (char).
 *   The is_secondary param slot [EBP+0x18] is REUSED as az_range float storage
 *   by MSVC — local az_range variable avoids the slot reuse in C89.
 *   FUN_000283b0 called with @<eax>=local_dir (copy of *dir_vec);
 *   FUN_00028250 called with @<esi>=actor_handle (preserved ESI),
 * @<edi>=look_type. look_type = is_aim ? 1 : 2 (computed via SETZ+INC in disasm
 * at 0x28e8a-0x28e92). Original branch layout: is_aim!=0 (combat) is
 * fallthrough at 0x28d3b; is_aim==0 (idle) is the taken JZ branch to 0x28dd7.
 *   actor+0x55c = primary look valid flag; actor+0x55d = is_aim copy;
 *   actor+0x56c = 4 (short, look type slot set before FUN_000283b0 call);
 *   actor+0x564 = FUN_00028250 result; actor+0x570 = output look direction
 * vec3. Returns 1 (char) if FUN_00028250 found a valid look target, 0
 * otherwise. */
char FUN_00028cc0(float *look_vectors, float *dir_vec, char look_mode,
                  char is_aim, char is_secondary, int actor_handle)
{
  char *actor;
  int tag_data;
  float local_dir[3];
  float el_min;
  char local_c;
  float el_max;
  float az_range;
  int look_type;
  int fp_result;

  actor = (char *)datum_get(actor_data, actor_handle);
  tag_data = (int)tag_get('actr', *(int *)(actor + 0x58));
  local_c = 0;
  *(char *)(actor + 0x55c) = 0;

  if (is_secondary != '\0' ||
      FUN_00027ff0(actor_handle, look_mode, is_aim, (short *)(actor + 0x56c),
                   &local_c) == '\0') {
    local_dir[0] = dir_vec[0];
    local_dir[1] = dir_vec[1];
    local_dir[2] = dir_vec[2];

    /* Original branch layout: is_aim != 0 (combat) is fallthrough (JZ to idle
     * path taken at 0x28d3b), is_aim == 0 (idle) is the JZ-taken branch */
    if (is_aim != '\0') {
      /* Combat/aim angular constraints from tag */
      if (look_mode != '\0') {
        /* Full 180-degree horizontal range */
        az_range = 3.1415927f;
      } else {
        if (*(float *)(tag_data + 0xa4) <= *(float *)(tag_data + 0xc4)) {
          az_range = *(float *)(tag_data + 0xa4);
        } else {
          az_range = *(float *)(tag_data + 0xc4);
        }
      }
      if (*(float *)(tag_data + 0xa8) <= *(float *)(tag_data + 0xc8)) {
        el_max = *(float *)(tag_data + 0xa8);
      } else {
        el_max = *(float *)(tag_data + 0xc8);
      }
      local_dir[2] = 0.0f;
      if (normalize3d(local_dir) == 0.0f) {
        local_dir[0] = *(float *)(*(int *)0x0031fc3c);
        local_dir[1] = *(float *)(*(int *)0x0031fc3c + 4);
        local_dir[2] = *(float *)(*(int *)0x0031fc3c + 8);
      }
    } else {
      /* Idle angular constraints from tag */
      if (*(float *)(tag_data + 0xac) <= *(float *)(tag_data + 0xcc)) {
        az_range = *(float *)(tag_data + 0xac);
      } else {
        az_range = *(float *)(tag_data + 0xcc);
      }
      if (*(float *)(tag_data + 0xb0) <= *(float *)(tag_data + 0xd0)) {
        el_max = *(float *)(tag_data + 0xb0);
      } else {
        el_max = *(float *)(tag_data + 0xd0);
      }
    }

    el_min = -el_max;
    if (*(char *)(actor + 0x161) != '\0') {
      el_min = el_min * *(float *)0x00253398;
    }
    *(short *)(actor + 0x56c) = 4;
    if (!FUN_000283b0((float *)(actor + 0x120), 1, -az_range, az_range, el_min,
                      el_max, (float *)(actor + 0x570), local_dir)) {
      return local_c;
    }
  }

  look_type = is_aim ? 1 : 2;
  fp_result = FUN_00028250(look_vectors, local_c, actor_handle, look_type);
  *(int *)(actor + 0x564) = fp_result;
  if (fp_result == 0) {
    return local_c;
  }
  *(char *)(actor + 0x55c) = 1;
  *(char *)(actor + 0x55d) = is_aim;
  return local_c;
}

/* FUN_00028ed0 (0x28ed0)
 * Idle-minor look update: selects a randomised look direction within the
 * actor's angular constraints and writes a new idle-minor look-spec if one
 * is found, then evaluates the current look state via FUN_00028250.
 *
 * Called from actor_look_update when the idle-minor timer has expired.
 *
 * Confirmed: EAX = actor_handle (register arg); param_1 = look_vectors
 * (float*); param_2 = idle_direction (float*, passed as @<eax> to
 * FUN_000283b0); FUN_000283b0 called with 7 cdecl + 1 @<eax> = idle_direction;
 *   FUN_00028250 called with ESI=actor_handle (preserved), EDI=2, cdecl
 *   (look_vectors, flag_byte). Output vec3 written to actor+0x580/584/588. */
void FUN_00028ed0(float *look_vectors, float *idle_direction, int actor_handle)
{
  char *actor;
  int tag_data;
  float az_range;
  float el_range;
  float *pfVar4;
  float az_min;
  float az_max;
  float out_vec3[3];
  char flag_byte;
  char look_found;

  actor = (char *)datum_get(actor_data, actor_handle);
  tag_data = (int)tag_get('actr', *(int *)(actor + 0x58));
  flag_byte = 0;
  *(char *)(actor + 0x55f) = 0;

  look_found =
    FUN_00027ff0(actor_handle, 0, 0, (short *)(actor + 0x57c), &flag_byte);
  if (!look_found) {
    /* az_range = min(tag[0xac], tag[0xcc]) */
    if (*(float *)(tag_data + 0xac) <= *(float *)(tag_data + 0xcc)) {
      az_range = *(float *)(tag_data + 0xac);
    } else {
      az_range = *(float *)(tag_data + 0xcc);
    }
    /* el_range = min(tag[0xb0], tag[0xd0]) */
    if (*(float *)(tag_data + 0xb0) <= *(float *)(tag_data + 0xd0)) {
      el_range = *(float *)(tag_data + 0xb0);
    } else {
      el_range = *(float *)(tag_data + 0xd0);
    }
    /* select azimuth limit table: crouching (state==3) vs standing */
    if (*(short *)(actor + 0x6a) == 3) {
      pfVar4 = (float *)(tag_data + 0xbc);
    } else {
      pfVar4 = (float *)(tag_data + 0xb4);
    }
    /* az_min = max(-az_range, -pfVar4[0]) */
    az_min = -*pfVar4;
    if (az_min < -az_range) {
      az_min = -az_range;
    }
    /* az_max = min(az_range, pfVar4[1]) */
    az_max = az_range;
    if (pfVar4[1] < az_range) {
      az_max = pfVar4[1];
    }
    if (FUN_000283b0((float *)(actor + 0x120), 0, az_min, az_max, -el_range,
                     el_range, out_vec3, idle_direction)) {
      *(float *)(actor + 0x580) = out_vec3[0];
      *(float *)(actor + 0x584) = out_vec3[1];
      *(float *)(actor + 0x588) = out_vec3[2];
      *(short *)(actor + 0x57c) = 4;
      flag_byte = 0;
    }
  }

  *(int *)(actor + 0x568) =
    FUN_00028250(look_vectors, flag_byte, actor_handle, 2);
  if (*(int *)(actor + 0x568) != 0) {
    *(char *)(actor + 0x55f) = 1;
  }
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
