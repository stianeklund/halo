/* actor_looking.c — AI actor looking/targeting subsystem.
 *
 * Corresponds to actor_looking.obj (or the actor_looking portion of the
 * compiled AI module).
 * Assertion path: c:\halo\SOURCE\ai\actor_looking.c
 */

#include "../../common.h"

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
                                   FUN_00016cd0, 0);
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
