/* actor_looking.c — AI actor looking/targeting subsystem.
 *
 * Corresponds to actor_looking.obj (or the actor_looking portion of the
 * compiled AI module).
 * Assertion path: c:\halo\SOURCE\ai\actor_looking.c
 */

#include "../../common.h"

/* FUN_000142a0 (0x142a0)
 * Conversation action initializer (action_converse.c, line 0x21).
 * Validates actor, looks up the conversation datum via 0x6324ec, fetches the
 * scenario conversation entry at scenario+0x468[*(short*)(action_ref+2)],
 * and populates state_data (20 bytes): [0]=action_handle, float@+8 from
 * tag+0x28, [3]=speaker_handle (if float==REAL_NONE) else -1, [4]=-1,
 * byte@+5=0. Returns 1 always.
 *
 * Confirmed: first datum_get(actor_data, actor_handle) result is discarded.
 * Confirmed: FCOMP+TEST AH,0x44+JP at 0x1432f — JP taken when PF=1 (NOT equal/NaN)
 *   → speaker handle; fall-through (equal) → -1. */
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
  if (look_entry == -1) {
    /* No existing look entry: use object's world position (type 3) */
    look_buf[0] = 3;
    unit_get_head_position(look_object, (float *)&look_buf[2]);
  } else {
    /* Existing look entry found: use it as an object-handle target (type 1) */
    look_buf[0] = 1;
    *(int *)&look_buf[2] = look_entry;
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
    display_assert("state_data", "c:\\halo\\SOURCE\\ai\\action_guard.c",
                   0xac, 1);
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
      param_2[0] = src[0]; param_2[1] = src[1];
      param_2[2] = src[2]; param_2[3] = src[3];
      return;
    }
    src = *(int **)0x2ee6f4;
    if (*(char *)(actor + 0xa5) != '\0') {
      src = *(int **)0x2ee6e8;
      param_2[0] = src[0]; param_2[1] = src[1];
      param_2[2] = src[2]; param_2[3] = src[3];
      return;
    }
  }
  param_2[0] = src[0];
  param_2[1] = src[1];
  param_2[2] = src[2];
  param_2[3] = src[3];
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
      FUN_00046f10(7, *(int *)(actor + 0x18), *(int *)(prop + 0x18),
                   -1, -1, 2, 0);
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
    *(float *)(actor + 0x468) = fwd[2] * *(float *)0x2533e8 + *(float *)(actor + 0xcc);
    *(float *)(actor + 0x464) = fwd[1] * *(float *)0x2533e8 + *(float *)(actor + 0xc8);
    *(float *)(actor + 0x460) = fwd[0] * *(float *)0x2533e8 + *(float *)(actor + 0xc4);
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
  *(short *)(actor + 0x3fc) =
      (short)(2 + (*(short *)(actor + 0x6e) >= 4) * 2);
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
                                       short *param_3,
                                       void (*callback)(void), int param_5)
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
      component = (char *)datum_get(swarm_component_data,
                                    *(int *)(swarm + 0x58 + i * 4));
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
      actor_handle, *(int *)(actor + 0x18), (int)*param_3,
      param_3 + 4, param_3 + 0x16, param_5);
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

/* FUN_000178b0 (0x178b0)
 * Subtract two 2D vectors: result = b - a.
 * Confirmed: cdecl, 3 stack params. FLD [ECX]/FSUB [EDX]/FSTP [EAX] twice. */
void FUN_000178b0(float *a, float *b, float *result)
{
  result[0] = b[0] - a[0];
  result[1] = b[1] - a[1];
}

/* Compute the cross product of two 3D vectors.
 *
 * out = a × b
 *
 * Confirmed: pure x87 arithmetic, no assertions, no globals.
 * Confirmed: FLD [ECX] / FMUL [EAX+0x4] / FLD [ECX+0x4] / FMUL [EAX] / FSUBP
 *   computes out[2] = a[0]*b[1] - a[1]*b[0] (z component).
 * Confirmed: three-component store at [EAX], [EAX+0x4], [EAX+0x8].
 */
void cross_product3d(float *a, float *b, float *out)
{
  /* Load all inputs before any store so b==out (in-place) is safe.
   * The binary (0x178d0) holds all three FPU results on the x87 stack
   * before the first FSTP, giving the same aliasing guarantee. */
  float a0 = a[0], a1 = a[1], a2 = a[2];
  float b0 = b[0], b1 = b[1], b2 = b[2];
  out[0] = a1 * b2 - a2 * b1;
  out[1] = a2 * b0 - a0 * b2;
  out[2] = a0 * b1 - a1 * b0;
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
    display_assert("state_data", "c:\\halo\\SOURCE\\ai\\action_search.c",
                   0x38, 1);
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
            (*(char *)(actor + 0x9c) != '\0' || (remain + 0x5a < *(int *)(actor + 0xbc)))) {
          *(int *)(actor + 0x3bd) = 1;
          FUN_00046f10(0xd, *(int *)(actor + 0x18),
                       actor_target_unit_index(actor_handle),
                       -1, -1, -1, 0);
          *(char *)(actor + 0x3bd) = 1;
          return;
        }
      } else if (remain == 0) {
        FUN_00046f10(0x12, *(int *)(actor + 0x18),
                     actor_target_unit_index(actor_handle),
                     -1, -1, -1, 0);
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
    display_assert("state_data", "c:\\halo\\SOURCE\\ai\\action_uncover.c",
                   0x38, 1);
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
short FUN_000272d0(int actor_handle, short param_2, short param_3,
                   int param_4, unsigned int param_5, char param_6)
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
                     "c:\\halo\\SOURCE\\ai\\actor_firing_position.c",
                     0x97b, 1);
      system_exit(-1);
    }
    if (*(short *)(actor + 0x3b8) != -1 && *(short *)(actor + 0x3b8) != param_2) {
      FUN_00024be0(actor_handle, *(short *)(actor + 0x3b8), 1);
    }
    if (param_4 != -1) {
      prev_actor = (char *)datum_get(actor_data, param_4);
      if (actor_handle == param_4) {
        display_assert("actor_index != previous_owner",
                       "c:\\halo\\SOURCE\\ai\\actor_firing_position.c",
                       0x988, 1);
        system_exit(-1);
      }
      FUN_0002f1a0(param_4);
      *(short *)(prev_actor + 0x3b8) = -1;
    }
    if (*(short *)(actor + 0x3b8) != (short)param_2) {
      *(short *)(actor + 0x3b8) = (short)param_2;
      *(char *)(actor + 0x3ba) = (char)(param_6 == '\0');
      *(char *)(actor + 0x3bb) = 0;
      cancel = (char)actor_move_to_firing_position(actor_handle, param_2,
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
    encounter_verify_firing_position_owner_actor_indices();
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
  if (*(short *)(actor + 0x3e8) > 2 && *look_spec != 0) {
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
