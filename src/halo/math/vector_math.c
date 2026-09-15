#include "x87_math.h"

/* 0x12090 — action_alert: raise alert/engage flags on an actor.
 *
 * Confirmed: cdecl, one stack arg at [EBP+0x8] (Ghidra: in_stack_00000004).
 *   The kb decl was `(void)`; the disassembly reads [EBP+0x8] and pushes it.
 * Confirmed: PUSH EAX([EBP+8]) / PUSH ECX(*0x6325a4) / CALL 0x119320
 *   -> datum_get(actors_data, actor_handle); result kept in ESI.
 * Confirmed: PUSH EDX([ESI+0x58]) / PUSH 0x61637472 ('actr') / CALL 0x1ba140
 *   -> tag_get(group_tag='actr', tag_index=*(int *)(actor + 0x58)).
 *   ADD ESP,0x10 cleans both cdecl calls (2 + 2 dword args).
 * Confirmed: MOV ECX,1 / MOV word ptr [ESI+0x3fc],CX — 16-bit store of 1.
 * Confirmed: MOV DL,byte ptr [EAX] / TEST DL,0x40 — tests bit 6 of byte 0 of
 *   the 'actr' tag definition; when set, MOV byte [ESI+0x426],CL and
 *   MOV byte [ESI+0x427],CL — two 8-bit stores of 1. */
void FUN_00012090(int actor_handle)
{
  int actor;
  unsigned char *definition;

  actor = (int)datum_get(*(data_t **)0x6325a4, actor_handle);
  definition = (unsigned char *)tag_get(0x61637472, *(int *)(actor + 0x58));
  *(unsigned short *)(actor + 0x3fc) = 1;
  if ((definition[0] & 0x40) != 0) {
    *(unsigned char *)(actor + 0x426) = 1;
    *(unsigned char *)(actor + 0x427) = 1;
  }
}

/* 0x120e0 — action_alert: clear alert state on actor.
 * Sets actor->state_data1 (0xa2) and state_data2 (0xa4) to 0xffff. */
void FUN_000120e0(int actor_handle)
{
  char *actor;

  actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle) + 0x9c;
  *(short *)(actor + 0x6) = -1;
  *(short *)(actor + 0x8) = -1;
}

/* 0x12110 — action_alert: clear another alert/avoid state.
 * Sets actor->field_d0 (short) to 0xffff and field_f4 (int) to -1. */
void FUN_00012110(int actor_handle)
{
  char *actor;

  actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle) + 0x9c;
  *(short *)(actor + 0x34) = -1;
  *(int *)(actor + 0x58) = -1;
}


/* FUN_00012140 (0x12140) — Subtract two 3D vectors: result = b - a.
 * Confirmed: cdecl, 3 pointer args. Pure FPU leaf.
 * Confirmed: arg1=a [EBP+0x8], arg2=b [EBP+0xc], arg3=result [EBP+0x10].
 * Confirmed: FLD [ECX] / FSUB [EDX] / FSTP [EAX] where ECX=b, EDX=a,
 * EAX=result. */
void FUN_00012140(float *a, float *b, float *result)
{
  result[0] = b[0] - a[0];
  result[1] = b[1] - a[1];
  result[2] = b[2] - a[2];
}

/* 0x12170 — FUN_00012170: squared magnitude of a 3D vector.
 *
 * Computes vector[0]^2 + vector[1]^2 + vector[2]^2 and returns it.
 *
 * Confirmed: loads three floats from [arg0+0], [arg0+4], and [arg0+8].
 * Confirmed: x87 stack sequence squares each component and accumulates with
 *   FADDP; no globals or calls.
 * Confirmed: returns the accumulated sum in ST0.
 */
float FUN_00012170(float *vector)
{
  return vector[0] * vector[0] + vector[1] * vector[1] + vector[2] * vector[2];
}

/* 0x121a0 — distance_squared3d: squared distance between two 3D points.
 *
 * Computes (b[0]-a[0])^2 + (b[1]-a[1])^2 + (b[2]-a[2])^2 and returns it.
 *
 * Confirmed: loads from [arg1+0/4/8] and subtracts [arg0+0/4/8].
 * Confirmed: x87 sequence squares each component delta and sums with FADDP.
 * Confirmed: returns in ST0; no globals or calls.
 */
float distance_squared3d(const float *a, const float *b)
{
  float dx = b[0] - a[0];
  float dy = b[1] - a[1];
  float dz = b[2] - a[2];
  float sum = dz * dz + dx * dx;

  return sum + dy * dy;
}

float FUN_000121e0(float min, float max)
{
  int *seed = get_global_random_seed_address();
  return random_real_range(seed, min, max);
}

/* action_alert_perform (0x12660)
 * One tick of the alert action: (1) if the actor has an alert command list
 * and no command in flight, optionally retire the arrival check and pick the
 * next position; (2) if the actor is timesliced, awake and holding a command
 * index, look the command up in the scenario squad's command block, copy it
 * into the actor state and hand it to the move-position selector.
 *
 * Confirmed: cdecl, one stack arg at [EBP+0x8] kept in ESI (0x1266b) and
 *   re-read at 0x1286b; the kb decl was `(void)`.
 * Confirmed: XOR AL,AL at 0x128ad on the single epilogue — byte return,
 *   always false (the 0x12881 JNZ early exit lands on the same XOR).
 * Confirmed: PUSH ESI / PUSH EAX(=[0x6325a4]) / CALL 0x119320 -> datum_get;
 *   result kept in EBX. EDI holds the -1 sentinel (OR EDI,0xffffffff).
 * Confirmed guards: CMP word [EBX+0x9c],0 / JZ; CMP word [EBX+0xa4],DI / JNZ.
 * Confirmed: MOV AL,[EBX+6] -> display_assert("!actor->meta.swarm",
 *   "c:\halo\SOURCE\ai\action_alert.c", 0x47, 1) then PUSH EDI(-1) /
 *   CALL system_exit (noreturn); the second copy at 0x127a8 uses line 0x78.
 * Confirmed: PUSH ECX(EBX+0xa8) / PUSH EDX(EBX+0x12c) / CALL 0x121a0 ->
 *   distance_squared3d(actor+0x12c, actor+0xa8); FSTP [EBP-4] holds it while
 *   PUSH ESI / CALL 0x3bd50 -> actor_destination_tolerance(actor_handle).
 *   ADD ESP,0xc cleans both (2 + 1 dword args).
 * Confirmed: FCOM [0x253398] / FNSTSW / TEST AH,0x41 / JZ skips the reload,
 *   so tolerance <= *(float *)0x253398 clamps up to that constant; then
 *   FLD ST0 / FMUL ST1 (tolerance squared) / FLD [EBP-4] / FCOMPP /
 *   TEST AH,0x41 / JZ 0x12770 — leave when dist2 > tolerance*tolerance.
 * Confirmed: CMP word [EBX+0x9e],0 / JG and MOV AL,[EBX+0xa6] / TEST / JNZ
 *   gate the next-position pick; object_get_and_verify_type([EBX+0x18], 3)
 *   then CMP byte [EAX+0x253],0x1c / JZ skips it too.
 * Confirmed action_alert_next_position pushes (last-to-first, 0x1275d..
 *   0x12760): EBX+0xa0, MOVZX word [EBX+0xa2], MOVZX word [EBX+0x9c],
 *   actor_handle; ADD ESP,0x10 (4 dword args, cdecl) and MOV word
 *   [EBX+0xa4],AX — 16-bit return.  Its kb decl was `void (void)`.
 * Confirmed: PUSH 0xb0 / PUSH (EAX & 0xffff) / global_scenario_get() + 0x42c
 *   / tag_block_get_element, then +0x80 with MOVSX [EBX+0x3a] and 0xe8;
 *   ADD ESP,0x18 cleans both 3-arg calls.
 * Confirmed: TEST CX,CX / JL and CMP ECX,[EAX+0xc4] / JGE bound the command
 *   index against the block count at squad+0xc4; the element block base is
 *   squad+0xc4 with element size 0x50.
 * Confirmed: MOV EDX,[ESI+0x18] / MOV EAX,[ESI+0x14] / PUSH EDX / PUSH EAX /
 *   CALL 0x121e0 -> FUN_000121e0(command[5], command[6]) — raw dword pushes
 *   of two float fields; FMUL [0x253394] (TICKS_PER_SECOND) follows, and the
 *   product stays on the x87 stack across the copy until CALL _ftol2
 *   (0x1d9068) whose AX is stored 16-bit to [EBX+0x9e].
 * Confirmed: MOVSD.REP with ECX=0x14, EDI=EBX+0xa8, ESI=command — an inline
 *   0x50-byte copy of the command element into the actor state.
 * Confirmed: MOV byte [EBX+0xa6],1 then PUSH EDX(MOVZX word [EBX+0xa2]) /
 *   PUSH EAX([EBP+8]) / CALL 0x2d850 -> actor_move_to_move_position;
 *   TEST AL,AL / JNZ 0x128ab returns without the reset block.
 * Unknown: field meanings at 0x9c/0x9e/0xa0/0xa2/0xa4/0xa6/0xa8 and the
 *   0x1c object-state constant — no string evidence in this function. */
bool action_alert_perform(int actor_handle)
{
  char *actor;
  void *unit;
  void *encounter;
  void *squad;
  int *command;
  float dist2;
  float tolerance;
  float ticks;
  short current;

  actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
  if (*(short *)(actor + 0x9c) != 0 && *(short *)(actor + 0xa4) == -1) {
    if (*(char *)(actor + 6) != '\0') {
      display_assert("!actor->meta.swarm",
                     "c:\\halo\\SOURCE\\ai\\action_alert.c", 0x47, 1);
      system_exit(-1);
    }
    if (*(short *)(actor + 0xa2) != -1 && actor_path_has_path(actor_handle) != '\0') {
      dist2 = distance_squared3d((const float *)(actor + 0x12c),
                                 (const float *)(actor + 0xa8));
      tolerance = actor_destination_tolerance(actor_handle);
      if (tolerance <= *(float *)0x253398) {
        tolerance = *(float *)0x253398;
      }
      if (tolerance * tolerance < dist2) {
        goto update_command;
      }
    }
    if (*(short *)(actor + 0x9e) <= 0 && *(char *)(actor + 0xa6) == '\0') {
      unit = object_get_and_verify_type(*(int *)(actor + 0x18), 3);
      if (*(char *)((char *)unit + 0x253) != 0x1c) {
        *(short *)(actor + 0xa4) = action_alert_next_position(
          actor_handle, *(unsigned short *)(actor + 0x9c),
          *(unsigned short *)(actor + 0xa2), actor + 0xa0);
      }
    }
  }

update_command:
  if (*(char *)(actor + 0x4c) != '\0' && *(char *)(actor + 0x13) == '\0' &&
      *(short *)(actor + 0xa4) != -1) {
    if (*(char *)(actor + 6) != '\0') {
      display_assert("!actor->meta.swarm",
                     "c:\\halo\\SOURCE\\ai\\action_alert.c", 0x78, 1);
      system_exit(-1);
    }
    if (*(int *)(actor + 0x34) != -1) {
      encounter = tag_block_get_element((char *)global_scenario_get() + 0x42c,
                                        *(int *)(actor + 0x34) & 0xffff, 0xb0);
      squad = tag_block_get_element((char *)encounter + 0x80,
                                    *(short *)(actor + 0x3a), 0xe8);
      current = *(short *)(actor + 0xa4);
      if (current >= 0 && (int)current < *(int *)((char *)squad + 0xc4)) {
        command = (int *)tag_block_get_element((char *)squad + 0xc4,
                                               (int)current, 0x50);
        ticks = FUN_000121e0(*(float *)(command + 5), *(float *)(command + 6)) *
                TICKS_PER_SECOND;
        *(short *)(actor + 0xa2) = *(short *)(actor + 0xa4);
        *(short *)(actor + 0xa4) = -1;
        qmemcpy(actor + 0xa8, command, 0x50);
        *(short *)(actor + 0x9e) = (short)(int)ticks;
        *(char *)(actor + 0xa6) = 1;
        if (actor_move_to_move_position(
              actor_handle, *(unsigned short *)(actor + 0xa2)) != '\0') {
          return 0;
        }
      }
    }
    *(short *)(actor + 0xa2) = *(short *)(actor + 0xa4);
    *(short *)(actor + 0xa4) = -1;
    *(short *)(actor + 0x9e) = 0;
    *(char *)(actor + 0xa6) = 0;
  }
  return 0;
}

/* action_avoid_setup (0x128c0)
 * Initialize action avoid state: asserts non-null state pointer, zeroes 4
 * bytes, returns true.
 *
 * Confirmed: cdecl, two stack args. MOV ESI,[EBP+0xc] at 0x128c4 is the
 *   asserted/zeroed pointer, so state_data is the SECOND arg; [EBP+0x8] is
 *   never read by this function (unknown type; named actor_handle after the
 *   identical action_fight_setup twin action_fight_setup / 0x14620).
 * Confirmed: TEST ESI,ESI / JNZ 0x128e8 at 0x128c7 — assert on NULL only.
 * Confirmed assert args (pushed last-to-first): PUSH 1 (halt), PUSH 0x1e
 *   (line 30), PUSH 0x25339c ("c:\halo\SOURCE\ai\action_avoid.c"),
 *   PUSH 0x25334c ("state_data") / CALL display_assert; then PUSH -1 /
 *   CALL system_exit (noreturn — no ADD ESP on that path).
 * Confirmed: PUSH 4 / PUSH 0 / PUSH ESI / CALL csmemset / ADD ESP,0xc.
 * Confirmed: MOV AL,0x1 at 0x128f5 — byte return, always true. */
char action_avoid_setup(int actor_handle, void *state_data)
{
  if (state_data == NULL) {
    display_assert("state_data", "c:\\halo\\SOURCE\\ai\\action_avoid.c", 0x1e,
                   1);
    system_exit(-1);
  }
  csmemset(state_data, 0, 4);
  return 1;
}

/* action_avoid_perform (0x12920)
 * Run one avoid-action tick: assert the actor is not a swarm actor, and when
 * its timeslice byte is set, evaluate a look target (actor_active_select_firing_position) and hand
 * the result to the firing-position selector (actor_change_firing_position).  Returns true
 * when actor+0x280 (short) is zero, on both paths.
 *
 * Confirmed: cdecl, one stack arg at [EBP+0x8] kept in EDI (0x12934); the kb
 *   decl was `(void)` but the arg is pushed to all three callees.
 * Confirmed: MOV EAX,0x14740 / CALL _chkstk at 0x12923 — 0x14740-byte frame.
 *   Slots: big_buf 0x1408c @EBP-0x14740, state_buf 0x670 @EBP-0x6b4,
 *   local_48 0x3c @EBP-0x44, local_8 @EBP-0x8, local_4 @EBP-0x4
 *   (4+4+0x3c+0x670+0x1408c == 0x14740).
 * Confirmed: PUSH EDI / PUSH EAX(=[0x6325a4]) / CALL 0x119320 -> datum_get;
 *   result kept in ESI.
 * Confirmed: MOV AL,byte ptr [ESI+0x6] / TEST AL,AL -> display_assert(
 *   "!actor->meta.swarm" @0x253380, "c:\halo\SOURCE\ai\action_avoid.c"
 *   @0x25339c, 0x37, 1) then PUSH -1 / CALL system_exit (noreturn).
 * Confirmed: MOV AL,byte ptr [ESI+0x4c] / TEST AL,AL / JZ 0x129c7 guards the
 *   body.
 * Confirmed: PUSH 0x670 / PUSH 0 / PUSH ECX(EBP-0x6b4) / CALL csmemset, then
 *   MOV word ptr [EBP-0x6b0],0x6 — a 16-bit 6 at state_buf+4.
 * Confirmed actor_active_select_firing_position pushes (last-to-first, 0x12981..0x1299b): &local_4,
 *   big_buf, &local_8, local_48, state_buf, actor_handle.
 * Confirmed actor_change_firing_position pushes (last-to-first, 0x129aa..0x129be): local_4,
 *   big_buf, local_8, local_48, EAX (actor_active_select_firing_position result), actor_handle.
 *   ADD ESP,0x3c at 0x129c4 cleans csmemset (0xc) + both 6-arg calls (0x18
 *   each); the actor_change_firing_position return value is discarded.
 * Confirmed: XOR EAX,EAX / CMP word ptr [ESI+0x280],AX / SETZ AL — byte
 *   return. */
bool action_avoid_perform(int actor_handle)
{
  char *actor;
  char state_buf[0x670];
  char big_buf[0x1408c];
  char local_48[0x3c];
  int local_4;
  int local_8;
  short result;

  actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
  if (*(char *)(actor + 6) != '\0') {
    display_assert("!actor->meta.swarm", "c:\\halo\\SOURCE\\ai\\action_avoid.c",
                   0x37, 1);
    system_exit(-1);
  }
  if (*(char *)(actor + 0x4c) != '\0') {
    csmemset(state_buf, 0, 0x670);
    *(short *)(state_buf + 4) = 6;
    result = actor_active_select_firing_position(actor_handle, state_buf, local_48, &local_8, big_buf,
                          &local_4);
    actor_change_firing_position(actor_handle, result, local_48, local_8, (unsigned int)big_buf,
                 (char)local_4);
  }
  return *(short *)(actor + 0x280) == 0;
}

/* 0x12a80 — action_alert: decrement squad-vehicle-passenger counter
 * if actor is in state 4 (vehicle) and target's state is 3. */
void FUN_00012a80(int actor_handle)
{
  int actor;
  int other_actor;

  actor = (int)datum_get(*(data_t **)0x6325a4, actor_handle);
  if (*(short *)(actor + 0xa0) == 4) {
    other_actor = (int)actor_combat_get_firing_variant_definition(actor_handle);
    if (*(short *)(other_actor + 0x156) == 3 && 0 < *(short *)(actor + 0x5fe)) {
      *(short *)(actor + 0x5fe) = *(short *)(actor + 0x5fe) + -1;
    }
  }
}

/* 0x12be0 — FUN_00012be0: bump the short counter at actor+0xaa when the actor
 * is in state 3 (same state constant guarded by FUN_00012e50) and three gate
 * bytes agree.
 *
 * Confirmed: cdecl, one stack arg at [EBP+0x8] (0x12be3 MOV EAX,[EBP+0x8]);
 *   the kb decl was `(void)` but the dword is pushed to datum_get.
 * Confirmed: MOV ECX,[0x6325a4] / PUSH EAX / PUSH ECX / CALL 0x119320 ->
 *   datum_get(actor_data, actor_handle); ADD ESP,0x8 (cdecl, 2 args).
 * Confirmed: guards are all against the datum_get result in EAX, no base bias:
 *   CMP word ptr [EAX+0xa0],0x3 / JNZ; MOV CL,[EAX+0xa7] / TEST / JZ;
 *   MOV CL,[EAX+0xa2] / TEST / JNZ; MOV CL,[EAX+0x15c] / TEST / JNZ.
 * Confirmed: INC word ptr [EAX+0xaa] — 16-bit increment, no clamp, no return.
 * Unknown: the meanings of the 0xa2, 0x15c gate bytes and the 0xaa counter;
 *   no string or assert evidence in this function, so no semantic names. */
void FUN_00012be0(int actor_handle)
{
  char *actor;

  actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
  if (*(short *)(actor + 0xa0) == 3 && *(char *)(actor + 0xa7) != '\0' &&
      *(char *)(actor + 0xa2) == '\0' && *(char *)(actor + 0x15c) == '\0') {
    *(short *)(actor + 0xaa) = *(short *)(actor + 0xaa) + 1;
  }
}

/* 0x12c30 — FUN_00012c30: recompute an actor's action-selection state
 * (move class, look-lock flags, and target-acquire snapshot) after its
 * per-tick evaluation.
 *
 * Confirmed: cdecl, one stack arg [EBP+0x8] (actor_handle); Ghidra's
 *   in_stack_00000004 pattern (same shape as FUN_00012090/FUN_00012be0).
 * Confirmed: PUSH EDI([EBP+8]) / PUSH EAX(=[0x6325a4]) / CALL 0x119320 ->
 *   datum_get(actors_data, actor_handle); the single ADD ESP,0x10 at
 *   0x12c71 cleans this call together with the tag_get call below (both
 *   cdecl, 2 args each — same combined-cleanup shape as FUN_00012090).
 * Confirmed: PUSH ECX([ESI+0x58]) / PUSH 0x61637472('actr') / CALL 0x1ba140
 *   -> tag_get(group_tag='actr', tag_index=*(int*)(actor+0x58)); result
 *   held at [EBP-0x4] and reread twice below for two separate flag tests.
 * Confirmed: MOV word[ESI+0x3ec],2 / MOV word[ESI+0x3fc],4 — 16-bit stores.
 * Confirmed: first branch tests actor+0xa0(short)==2||3, actor+0xa5(char)
 *   !=0, actor+0x504(char)==0, and actor_path_has_path(actor_handle)==0 (PUSH
 *   EDI/CALL 0x2a3d0/ADD ESP,4/TEST AL,AL — actor_handle forwarded
 *   unchanged); on all true, stores 4 at actor+0x3e8(short).
 * Confirmed: else-if actor+0x6e(short)<5 or actor+0xa0==1, stores 5 at
 *   actor+0x3e8 (MOV EAX,5 at 0x12cac feeds both the JL/JZ-taken path via
 *   AX and the fallthrough compare — same literal 5), else stores 7.
 * Confirmed: if actor+0xa0==1, actor+0x426/+0x427(char) := (actor+0xc1==0);
 *   else if actor+0x428(char)==0 && (tag_def[0]&0x10000)!=0, both :=
 *   actor+0x358(char); else both := 0.
 * Confirmed: if actor+0xa8(byte — MOV AL,byte[ESI+0xa8]) != 0:
 *   actor+0x440=1; actor+0x441 := (actor+0xbc < actor+0xb8*[0x2533c4]) via
 *   FLD[+0xb8]/FMUL[0x2533c4]/FCOMP[+0xbc]/FNSTSW/TEST AH,0x41 — the same
 *   confirmed 0.7f-constant idiom as actor_looking.c:4656 (0x2533c4=0.7f);
 *   actor+0x442=1; actor+0x444/0x448/0x44c/0x450(float, dword copies) :=
 *   actor+0xb0/0xb4/0xb8/0xbc; actor+0xa7=1; actor+0xa8=0; then
 *   CALL 0x000b5aa0 -> game_time_get() stored to actor+0xac(int);
 *   actor+0xaa(word)=0.
 * Confirmed: if (tag_def[0]&0x100000)!=0 && (actor+0x378(char)!=0 ||
 *   actor+0xa0==2 || actor+0xa0==3): actor+0x428(char) :=
 *   (actor+0xc4!=0 && actor+0x427==0).
 * Confirmed: unconditional tail — actor+0x42a=1; actor+0x424=0;
 *   actor+0x425=0; actor+0x454(bool) := actor+0xa0 != 1.
 * Permuter (95.8% LCS vs 88.6% baseline, audit=OK — semantics unchanged):
 *   an extra `char **new_var = &actor` indirection level at some sites
 *   (still the same actor pointer, one more load) matches the reference's
 *   register/stack scheduling more closely than reading through `actor`
 *   everywhere; kept exactly as found, only reformatted to house style. */
void FUN_00012c30(int actor_handle)
{
  char *actor;
  unsigned int *tag_def;
  char **new_var;

  actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
  new_var = &actor;
  tag_def = (unsigned int *)tag_get(0x61637472, *(int *)(*new_var + 0x58));
  *(short *)(actor + 0x3ec) = 2;
  *(short *)(actor + 0x3fc) = 4;

  if ((*(short *)(*new_var + 0xa0) == 2 || *(short *)(actor + 0xa0) == 3) &&
      *(char *)(*new_var + 0xa5) != '\0' &&
      (*(char *)(actor + 0x504) == '\0' &&
       actor_path_has_path(actor_handle) == '\0')) {
    *(short *)(*new_var + 0x3e8) = 4;
  } else if (*(short *)(*new_var + 0x6e) < 5 ||
             *(short *)(*new_var + 0xa0) == 1) {
    *(short *)(*new_var + 0x3e8) = 5;
  } else {
    *(short *)(actor + 0x3e8) = 7;
  }

  if (*(short *)(actor + 0xa0) == 1) {
    *(unsigned char *)(actor + 0x426) = *(char *)(*new_var + 0xc1) == '\0';
    *(unsigned char *)(actor + 0x427) = *(char *)(*new_var + 0xc1) == '\0';
  } else if (*(char *)(actor + 0x428) == '\0' && (*tag_def & 0x10000) != 0) {
    *(unsigned char *)(*new_var + 0x426) = *(unsigned char *)(*new_var + 0x358);
    *(unsigned char *)(*new_var + 0x427) = *(unsigned char *)(*new_var + 0x358);
  } else {
    *(unsigned char *)(actor + 0x426) = 0;
    *(unsigned char *)(*new_var + 0x427) = 0;
  }

  if (*(char *)(actor + 0xa8) != '\0') {
    *(unsigned char *)(*new_var + 0x440) = 1;
    *(unsigned char *)(*new_var + 0x441) =
      (unsigned char)(*(float *)(actor + 0xbc) <
                      *(float *)(actor + 0xb8) * *(float *)0x2533c4);
    *(unsigned char *)(actor + 0x442) = 1;
    *(float *)(actor + 0x444) = *(float *)(*new_var + 0xb0);
    *(float *)(actor + 0x448) = *(float *)(actor + 0xb4);
    *(float *)(*new_var + 0x44c) = *(float *)(actor + 0xb8);
    *(float *)(actor + 0x450) = *(float *)(*new_var + 0xbc);
    *(unsigned char *)(*new_var + 0xa7) = 1;
    *(unsigned char *)(*new_var + 0xa8) = 0;
    *(int *)(actor + 0xac) = game_time_get();
    *(short *)(*new_var + 0xaa) = 0;
  }

  if ((*tag_def & 0x100000) != 0 &&
      (*(char *)(actor + 0x378) != '\0' || *(short *)(actor + 0xa0) == 2 ||
       *(short *)(*new_var + 0xa0) == 3)) {
    *(unsigned char *)(*new_var + 0x428) =
      (unsigned char)(*(char *)(actor + 0xc4) != '\0' &&
                      *(char *)(actor + 0x427) == '\0');
  }

  *(unsigned char *)(*new_var + 0x42a) = 1;
  *(unsigned char *)(actor + 0x424) = 0;
  *(unsigned char *)(actor + 0x425) = 0;
  *(bool *)(*new_var + 0x454) = *(short *)(*new_var + 0xa0) != 1;
}

/* 0x12e50 — FUN_00012e50: check if actor is in a valid 'swarm flying' state
 * and its state timer has not yet expired.
 *
 * Looks up the actor record via actor_data (0x6325a4) and checks:
 *   actor+0xa0 (short): must equal 3 (swarm flying state)
 *   actor+0xa7 (char):  must be non-zero (active flag)
 * If both conditions hold, reads actor+0xac (int, state end time) and
 * returns true iff game_time_get() <= actor+0xac + 0x1e.
 *
 * Confirmed: cdecl, 1 stack arg (actor_handle). Returns bool.
 * Confirmed: ESI = datum_get result + 0x9c; offsets relative to ESI.
 * Confirmed: SETGE AL after CMP EDX,EAX (EDX=*(int*)(ESI+0x10)+0x1e,
 *   AX=game_time_get()), so bVar1 = (EDX >= EAX). */
bool FUN_00012e50(int actor_handle)
{
  char *actor;
  bool result;

  actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle) + 0x9c;
  result = false;
  if (*(short *)(actor + 4) == 3 && *(char *)(actor + 0xb) != '\0') {
    result = *(int *)(actor + 0x10) + 0x1e >= game_time_get();
  }
  return result;
}

/* 0x12ea0 — sqrtf wrapper. */
float FUN_00012ea0(float x)
{
  return sqrtf(x);
}

/* 0x12eb0 — Scale a 2D vector: out = scale * in. */
void FUN_00012eb0(float *in, float scale, float *out)
{
  out[0] = scale * in[0];
  out[1] = scale * in[1];
}

/* 0x12ed0 — Squared magnitude of a 2D vector. */
float FUN_00012ed0(float *v)
{
  return v[0] * v[0] + v[1] * v[1];
}

/* 0x12ef0 — Magnitude of a 2D vector. */
float FUN_00012ef0(float *v)
{
  return sqrtf(v[0] * v[0] + v[1] * v[1]);
}

/* 0x12f10 — Normalize a 2D vector in-place and return its magnitude.
 * Despite the kb.json name "magnitude3d", only operates on v[0] and v[1].
 * If magnitude exceeds epsilon, divides each component by it so v becomes
 * a unit vector. Returns the original magnitude, or 0.0f if too small. */
float magnitude3d(float *v)
{
  float mag;
  float scale;

  mag = sqrtf(v[0] * v[0] + v[1] * v[1]);
  if (!(x87_fabs(mag) < *(double *)0x2533d0)) {
    scale = 1.0f / mag;
    v[0] = v[0] * scale;
    v[1] = v[1] * scale;
    return mag;
  }
  return 0.0f;
}

/* 0x12f60 — Dot product of two 2D vectors. */
float FUN_00012f60(float *a, float *b)
{
  return a[0] * b[0] + a[1] * b[1];
}

/* 0x12f80 — Compute out = base + scale * direction (3-component). */
float *vector3d_scale_add(float *base, float *direction, float scale,
                          float *out)
{
  out[0] = scale * direction[0] + base[0];
  out[1] = scale * direction[1] + base[1];
  out[2] = scale * direction[2] + base[2];
  return out;
}

/* 0x12fb0 — Scale a 3D vector: out = scale * in. */
void FUN_00012fb0(float *in, float scale, float *out)
{
  out[0] = scale * in[0];
  out[1] = scale * in[1];
  out[2] = scale * in[2];
}

/* 0x12fe0 — Magnitude of a 3D vector. */
float FUN_00012fe0(float *v)
{
  return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

/* Normalize a 3D vector in-place.
 * Computes the magnitude (Euclidean length) of v, and if it exceeds a
 * small epsilon threshold (~0.0001), divides each component by the
 * magnitude so v becomes a unit vector. Returns the original magnitude,
 * or 0.0f if the vector was too small to normalize. */
float normalize3d(float *v)
{
  float mag;
  float scale;

  mag = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  /* Original (0x13010): FCOMP ABS(mag) against the *double* 0.0001 at 0x2533d0
     and normalize only when |mag| >= 0.0001, otherwise return 0.0 (0x2533c0)
     leaving v unchanged.  A prior lift mis-read 0x2533d0 as a float
     (-3.69e19) — making the threshold always-true — and substituted a
     `mag == 0.0f` guard.  That let denormalized / near-zero (but nonzero)
     vectors be divided into a non-unit result that later tripped
     assert_valid_real_normal3d (actor_looking.c:529 via actor_look_decode_direction).  Read
     the threshold as a double to restore the original early-out. */
  if (fabsf(mag) >= *(double *)0x2533d0) {
    scale = 1.0f / mag;
    v[0] = v[0] * scale;
    v[1] = v[1] * scale;
    v[2] = v[2] * scale;
    return mag;
  }
  return 0.0f;
}

/* FUN_00013070 (0x13070) — Calculate the dot product of two 3D vectors.
 * Confirmed: cdecl calling convention with two pointer arguments.
 * Confirmed: FPU leaf function.
 * The calculation order is a.z*b.z + a.y*b.y + a.x*b.x. */
float FUN_00013070(float *a, float *b)
{
  /* Keep this calculation order. It is necessary for lockstep determinism.
     The original function at 0x13070 calculates ((z*z' + y*y') + x*x').
     The instruction sequence is: FLD z, FMUL, FLD y, FMUL, FADDP,
     FLD x, FMUL, FADDP.

     Do not write the expression as x+y+z. Clang can then calculate
     ((x+y)+z). This can produce a different floating-point rounding result.
     A difference of one ULP can change a movement or facing threshold on a
     different tick. This can cause a system-link game to lose synchronization.

     cl.exe reassociates the expression and matches the original instruction
     order. Therefore, this problem is not visible in the VC71 build.
     The shipped build uses Clang, so the explicit order is necessary. */
  return a[2] * b[2] + a[1] * b[1] + a[0] * b[0];
}

/* 0x13090 — Subtract two 3D vectors: out = a - b. */
void FUN_00013090(float *a, float *b, float *out)
{
  out[0] = a[0] - b[0];
  out[1] = a[1] - b[1];
  out[2] = a[2] - b[2];
}

/* 0x130d0 — Ray-cast between two points. Computes the direction vector
 * (point_b - point_a) and delegates to FUN_0014df70 for the actual
 * collision test along that direction from point_a. */
bool FUN_000130d0(uint32_t collision_flags, float *point_a, float *point_b,
                  int max_distance, int16_t *collision_result)
{
  float direction[3];

  direction[0] = point_b[0] - point_a[0];
  direction[1] = point_b[1] - point_a[1];
  direction[2] = point_b[2] - point_a[2];
  return FUN_0014df70(collision_flags, point_a, direction, max_distance,
                      collision_result);
}

/* 0x21370 — Sine of a float (x87 FSIN). */
float FUN_00021370(float x)
{
#if defined(_MSC_VER) && !defined(__clang__)
  return (float)sin(
    (double)x); /* VC71 /Oi inlines as FSIN (matches original) */
#else
  return x87_fsin(x);
#endif
}

/* 0x21380 — Cosine of a float (x87 FCOS). */
float FUN_00021380(float x)
{
#if defined(_MSC_VER) && !defined(__clang__)
  return (float)cos(
    (double)x); /* VC71 /Oi inlines as FCOS (matches original) */
#else
  return x87_fcos(x);
#endif
}

/* 0x21390 — Tangent of a float (x87 FPTAN). */
float FUN_00021390(float x)
{
#if defined(_MSC_VER) && !defined(__clang__)
  return (float)tan(
    (double)x); /* VC71 /Oi inlines as FPTAN (matches original) */
#else
  return x87_fsin(x) / x87_fcos(x);
#endif
}

/* 0x213a0 — 2D cross product (z-component): a[0]*b[1] - a[1]*b[0]. */
float FUN_000213a0(float *a, float *b)
{
  return b[1] * a[0] - a[1] * b[0];
}

/* 0x213c0 — Compute out = a + b (3-component). */
void vector3d_add(float *a, float *b, float *out)
{
  out[0] = a[0] + b[0];
  out[1] = a[1] + b[1];
  out[2] = a[2] + b[2];
}

/* 0x21410 — Check if a float is valid (not NaN/Inf). */
int FUN_00021410(uint32_t bits)
{
  return (bits & 0x7f800000) != 0x7f800000;
}

/* 0x21e50 — Build and validate a grenade throwing solution for an actor.
 *
 * Confirmed: PUSH EAX([EBP+8]) / PUSH ECX(*0x6325a4) / CALL 0x119320
 *   -> datum_get(actors_data, actor_handle); actor kept in ESI.
 * Confirmed: PUSH EDX([ESI+0x5c]) / PUSH 0x61637476 ('actv') / CALL 0x1ba140
 *   -> tag_get('actv', actor+0x5c); the definition pointer stays in EAX and is
 *   read at +0x180 (MOVSX word) and +0x190.
 * Confirmed: LEA ECX,[ESI+0x120] then three MOV dword copies into [EBP-0x18].
 * Confirmed: CALL 0x218d0 takes 9 stack args plus two register args:
 *   EAX = &position ([EBP-0x18]) and EBX = &aim_direction ([EBP-0x24]).
 *   The grouped ADD ESP,0x34 also cleans the datum_get/tag_get arg slots
 *   (9 + 2 + 2 = 13 dwords), so it is not a 13-argument call.
 * Confirmed: the last pushed argument is LEA EDX,[EBP+0x10] — the address of
 *   the param_3 parameter slot. The callee overwrites that slot, and the new
 *   value is re-read at 0x21ed7/0x21eeb and passed as the `accel` argument of
 *   ai_test_ballistic_line_of_fire (same argument position as the sibling
 *   0x21710, which passes a ballistic acceleration there). The incoming
 *   pointer value is cached in EDI before the call and is what the final
 *   stores dereference, so both uses are reproduced explicitly here.
 * Confirmed: SETNZ CL from CMP [ESI+0x158],-1 is the last argument of
 *   ai_test_ballistic_line_of_fire.
 * Confirmed: on success the three dwords at [EDI] land at actor+0x6a8..0x6b0,
 *   param_4 at +0x6b4, param_5 (EBX from [EBP+0x18]) at +0x6b8, the callee's
 *   aim direction at +0x6bc..0x6c4, the callee's scalar at +0x6c8, and
 *   byte [ESI+0x6a1] is cleared; AL = 1. Both failure exits return the
 *   [EBP-0x1] byte, which is only ever set to 0.
 * Unknown: param_2 ([EBP+0xc]) is never read by this function. */
char FUN_00021e50(volatile int actor_handle, short param_2, float *param_3,
                  int param_4, int param_5)
{
  char *actor;
  char *definition;
  float impact_point[3];
  float aim_direction[3];
  float position[3];
  float speed;
  int target;
  char result;
  float *point;

  actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
  definition = (char *)tag_get(0x61637476 /* 'actv' */, *(int *)(actor + 0x5c));
  point = param_3;
  *(uint32_t *)&position[0] = *(uint32_t *)(actor + 0x120);
  *(uint32_t *)&position[1] = *(uint32_t *)(actor + 0x124);
  *(uint32_t *)&position[2] = *(uint32_t *)(actor + 0x128);
  result = 0;

  if (actor_combat_build_grenade_trajectory(
        position, aim_direction, (int)*(short *)(definition + 0x180),
        *(int *)(definition + 0x190), param_3, 0, 0, &speed, &target,
        impact_point, (float *)&param_3) != '\0') {
    /* param_3's stack slot now holds the scalar written by the callee; the
     * original pointer survives in `point`. */
    if (ai_test_ballistic_line_of_fire(
          actor_handle, (int)position, target, impact_point, *(float *)&param_3,
          param_5, (char)(*(int *)(actor + 0x158) != -1)) != '\0') {
      *(uint32_t *)(actor + 0x6a8) = *(uint32_t *)&point[0];
      *(uint32_t *)(actor + 0x6ac) = *(uint32_t *)&point[1];
      *(uint32_t *)(actor + 0x6b0) = *(uint32_t *)&point[2];
      *(int *)(actor + 0x6b4) = param_4;
      *(uint32_t *)(actor + 0x6bc) = *(uint32_t *)&aim_direction[0];
      *(uint32_t *)(actor + 0x6c0) = *(uint32_t *)&aim_direction[1];
      *(int *)(actor + 0x6b8) = param_5;
      *(uint32_t *)(actor + 0x6c8) = *(uint32_t *)&speed;
      *(unsigned char *)(actor + 0x6a1) = 0;
      *(uint32_t *)(actor + 0x6c4) = *(uint32_t *)&aim_direction[2];
      return 1;
    }
  }
  return result;
}

/* 0x21f70 — Float approximate equality check within epsilon. */
int FUN_00021f70(float a, float b)
{
  float diff = a - b;
  if ((*(uint32_t *)&diff & 0x7f800000) == 0x7f800000)
    return 0;
  if (fabsf(diff) < *(double *)0x2549d8)
    return 1;
  return 0;
}

/* 0x21fb0 — valid_real_normal3d: check whether a 3D vector is a valid
 * unit normal (length within epsilon of 1.0).
 *
 * Computes squared_length = dot(v, v) and returns true if
 * |squared_length - 1.0f| < 0.001f.
 *
 * Also rejects NaN/infinity by testing the exponent bits.
 *
 * Confirmed: FLD / FMUL / FADDP computes dot(v, v) on x87 stack.
 * Confirmed: FSUB [0x2533c8] subtracts 1.0f.
 * Confirmed: FABS / FCOMP double ptr [0x2549d8] compares against
 * (double)0.001f. */
bool valid_real_normal3d(float *v)
{
  float sq_len = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
  float diff = sq_len - 1.0f;

  if ((*(unsigned int *)&diff & 0x7f800000) == 0x7f800000) {
    return 0;
  }

  return fabsf(diff) < 0.001f;
}

/* 0x28610 — Validate that a 2D vector is a unit normal.
 * Checks that x²+y² is within epsilon of 1.0 and not NaN/Inf. */
int valid_real_normal2d(float *v)
{
  float diff = (v[0] * v[0] + v[1] * v[1]) - 1.0f;
  if ((*(uint32_t *)&diff & 0x7f800000) == 0x7f800000)
    return 0;
  if (fabsf(diff) < *(double *)0x2549d8)
    return 1;
  return 0;
}
