#include "x87_math.h"

/* 0x12000 — action_alert: initialise a 0x5c-byte alert state block.
 *
 * Confirmed: cdecl, four stack args [EBP+0x8..0x14]; byte return (MOV AL,1).
 * Confirmed: PUSH EAX([EBP+8]) / PUSH ECX(*0x6325a4) / CALL 0x119320
 *   -> datum_get(actors_data, actor_handle), result kept in EDI; the call
 *   precedes the state_data check.
 * Confirmed: TEST ESI([EBP+0x14]) / JNZ -> display_assert("state_data"
 *   @0x25334c, "c:\halo\SOURCE\ai\action_alert.c" @0x253358, 0x23, 1),
 *   then PUSH -1 / CALL system_exit (noreturn).
 * Confirmed: PUSH 0x5c / PUSH 0 / PUSH ESI / CALL csmemset.
 * Confirmed: byte [actor+6] nonzero -> 0, else low word of param_2; stored
 *   as word [state+0]. Then word [state+6]=0xffff, word [state+8]=low word
 *   of param_3, byte [state+4]=1, word [state+2]=0, byte [state+0xa]=0.
 *   Field meanings are unknown. */
char FUN_00012000(int actor_handle, int param_2, int param_3, int state_data)
{
  int actor;
  int value;

  actor = (int)datum_get(*(data_t **)0x6325a4, actor_handle);
  if (state_data == 0) {
    display_assert("state_data", "c:\\halo\\SOURCE\\ai\\action_alert.c", 0x23,
                   1);
    system_exit(-1);
  }
  csmemset((void *)state_data, 0, 0x5c);
  value = param_2;
  if (*(char *)(actor + 6) != 0) {
    value = 0;
  }
  *(short *)state_data = (short)value;
  *(unsigned short *)(state_data + 6) = 0xffff;
  *(short *)(state_data + 8) = (short)param_3;
  *(char *)(state_data + 4) = 1;
  *(short *)(state_data + 2) = 0;
  *(char *)(state_data + 10) = 0;
  return 1;
}

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
float *FUN_00012140(float *a, float *b, float *result)
{
  result[0] = b[0] - a[0];
  result[1] = b[1] - a[1];
  result[2] = b[2] - a[2];
  return result;
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

/* action_alert_update (0x12200) — per-tick arrival check for the alert action.
 *
 * If the actor is awake and holds an alert state index, measure the squared
 * distance from its body position to the stored alert position; when the
 * pathfinder reports arrival, or that distance is below the global arrival
 * threshold, tick the initiative countdown down and, if the arrival flag is
 * still set, hand the pending scenario command to the animation starter and
 * clear the flag.
 *
 * Confirmed: cdecl, one stack arg at [EBP+0x8] kept in EDI (0x1220b). The kb
 *   decl was `(void)`; the disassembly reads and pushes [EBP+0x8].
 * Confirmed: PUSH EDI(actor_handle) / PUSH EAX(*0x6325a4) / CALL 0x119320 ->
 *   datum_get(actors_data, actor_handle); result kept in ESI; ADD ESP,0x8.
 * Confirmed: TEST byte [ESI+0x13] (dormant) and CMP word [ESI+0xa2],-1 are two
 *   independent exits to 0x1233d.
 * Confirmed: FLD [ESI+0xa8] / FSUB [ESI+0x12c] (and 0xac/0x130, 0xb0/0x134) —
 *   delta = alert_position - body_position, in that subtraction order.
 * Confirmed: x87 accumulation order is dz*dz + dx*dx then + dy*dy (FLD ST0 /
 *   FMUL ST1 / FLD ST3 / FMUL ST4 / FADDP / FLD ST2 / FMUL ST3 / FADDP),
 *   the same shape distance_squared3d (0x121a0) emits, inlined here.
 * Confirmed: display_assert("!actor->meta.swarm", "c:\\halo\\SOURCE\\ai\\
 *   action_alert.c", 0xae, true) / system_exit(-1) guarded by TEST byte
 * [ESI+6]. Confirmed: PUSH EDI / CALL 0x2a3f0 ->
 * actor_path_at_destination(actor_handle); TEST AL,AL / JNZ enters the body,
 * otherwise FCOMP float [0x25337c] with FNSTSW/TEST AH,0x5/JP — i.e. `||
 * distance_sq < *(float *)0x25337c`. Confirmed: word [ESI+0x9e] is decremented
 * only while > 0 (TEST AX,AX / JLE). Confirmed: MOV AX,[ESI+0xc4] / CMP
 * AX,0xffff / MOVSX ECX,AX — signed short command index; PUSH 0x3c / PUSH ECX /
 * CALL 0x18e380 / ADD EAX,0x444 / PUSH EAX / CALL 0x19b210 ->
 * tag_block_get_element(scenario+0x444, index, 0x3c); element kept in EDI.
 * Confirmed: when *(int *)(element+0x2c) == -1 the animation comes from
 *   object_get_and_verify_type(*(int *)(actor+0x18), 3) -> *(int *) deref ->
 *   tag_get('unit', tag_index) -> *(int *)(definition + 0x44).
 * Confirmed: PUSH 1 / PUSH EDI / PUSH EAX / PUSH ECX([ESI+0x18]) /
 *   CALL 0x1ac180 -> FUN_001ac180(unit_handle, animation, element, 1).
 * Confirmed: MOV byte [ESI+0xa6],0 clears the arrival flag on both paths that
 *   reach 0x12336. */
void action_alert_update(int actor_handle)
{
  char *actor;
  float distance_sq;
  short initiative;
  short command_index;
  int animation;
  void *command;
  int *object;
  unsigned char *definition;

  actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
  if (*(char *)(actor + 0x13) != 0) {
    return;
  }
  if (*(short *)(actor + 0xa2) == -1) {
    return;
  }

  distance_sq =
    distance_squared3d((const float *)&((actor_t *)actor)->body_position,
                       (const float *)(actor + 0xa8));

  assert_halt_msg_at("!actor->meta.swarm",
                     "c:\\halo\\SOURCE\\ai\\action_alert.c", 0xae,
                     *(char *)(actor + 0x6) == 0);

  if ((char)actor_path_at_destination(actor_handle) != '\0' ||
      distance_sq < *(float *)0x25337c) {
    initiative = *(short *)(actor + 0x9e);
    if (initiative > 0) {
      *(short *)(actor + 0x9e) = initiative - 1;
    }
    if (*(char *)(actor + 0xa6) != 0) {
      command_index = *(short *)(actor + 0xc4);
      if (command_index != -1) {
        command = tag_block_get_element((char *)global_scenario_get() + 0x444,
                                        command_index, 0x3c);
        animation = *(int *)((char *)command + 0x2c);
        if (animation == -1) {
          object = (int *)object_get_and_verify_type(
            ((actor_t *)actor)->meta_unit_index, 3);
          definition = (unsigned char *)tag_get(0x756e6974, *object);
          animation = *(int *)(definition + 0x44);
        }
        FUN_001ac180(((actor_t *)actor)->meta_unit_index, animation, command,
                     1);
      }
      *(unsigned char *)(actor + 0xa6) = 0;
    }
  }
}

/* action_alert_next_position (0x12350) — pick the next alert position index
 * in the actor's squad position block.
 *
 * Confirmed: cdecl, four stack args [EBP+0x8..0x14]; param_2/param_3 are read
 *   as 16-bit words (MOV BX,[EBP+0xc] / CMP AX,0xffff on [EBP+0x10]);
 *   16-bit return in AX. Early exits return -1 (OR EAX,-1), except
 *   param_2 == 1 with param_3 != -1, which returns param_3 (0x12401..0x12408).
 * Confirmed: datum_get(*0x6325a4, actor_handle); exits on byte [actor+0x160],
 *   word param_2 == 0, dword [actor+0x34] == -1.
 * Confirmed: tag_block_get_element(scenario+0x42c, [actor+0x34] & 0xffff,
 *   0xb0) then (+0x80, MOVSX [actor+0x3a], 0xe8) -> squad; assert
 *   "!actor->meta.swarm" line 0x113 on byte [actor+6].
 * Confirmed: csmemset(&flags, 0, 4); position block at squad+0xc4 (count
 *   dword, elements 0x50 bytes). Per position: usable = index != param_3;
 *   cleared when param_3 != -1 and |pos - actor[0x12c..0x134]|^2 < [0x25337c]
 *   (x87 order dx*dx + dy*dy, + dz*dz), or byte pos[0x1e] is nonzero and
 *   differs from byte [actor+0x68]; any prop from prop_iterator_new/next with
 *   word [prop+0x24] in 2..3 within [0x25337c] of pos (order dz*dz + dy*dy,
 *   + dx*dx) marks it occupied without clearing any_usable.
 * Confirmed: mode 5 -> choose_random_array_element(*(squad+0xc8), 0x50,
 *   word count, 0x10, &flags). Otherwise walk from param_3 (clamped to 0):
 *   mode 3 reverses at the last index / forwards at 0 / else follows *param_4;
 *   mode 4 uses game_time_get() & 1; other modes forward. The direction byte
 *   is stored to *param_4 when non-NULL. Returns the first index whose flag
 *   bit is clear.
 * Unknown: field meanings of actor+0x160/0x68, pos+0x1e, prop+0x24/0xbc. */
short action_alert_next_position(int actor_handle, short param_2, short param_3,
                                 void *param_4)
{
  char *actor;
  void *squad;
  int *positions;
  float *position;
  int prop;
  int iterator[2];
  uint32_t used_flags[1];
  short index;
  short next;
  short result;
  char usable;
  char any_usable;
  char forward;
  float dx;
  float dy;
  float dz;

  actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
  result = -1;
  if (*(char *)(actor + 0x160) == 0 && (short)param_2 != 0 &&
      *(int *)(actor + 0x34) != -1) {
    squad = tag_block_get_element((char *)global_scenario_get() + 0x42c,
                                  *(int *)(actor + 0x34) & 0xffff, 0xb0);
    squad = tag_block_get_element((char *)squad + 0x80,
                                  *(short *)(actor + 0x3a), 0xe8);
    if (*(char *)(actor + 6) != 0) {
      display_assert("!actor->meta.swarm",
                     "c:\\halo\\SOURCE\\ai\\action_alert.c", 0x113, 1);
      system_exit(-1);
    }
    if ((short)param_2 == 1 && (short)param_3 != -1) {
      result = (short)param_3;
    } else {
      any_usable = 0;
      csmemset(used_flags, 0, 4);
      positions = (int *)((char *)squad + 0xc4);
      index = 0;
      while (index < *positions) {
        position = (float *)tag_block_get_element(positions, index, 0x50);
        usable = 1;
        if (index == (short)param_3) {
          usable = 0;
        }
        if ((short)param_3 != -1) {
          dx = position[0] - *(float *)(actor + 0x12c);
          dy = position[1] - *(float *)(actor + 0x130);
          dz = position[2] - *(float *)(actor + 0x134);
          if (dx * dx + dy * dy + dz * dz < *(float *)0x25337c) {
            usable = 0;
          }
        }
        if (*((char *)position + 0x1e) != 0 &&
            *((char *)position + 0x1e) != *(char *)(actor + 0x68)) {
          usable = 0;
        }
        prop_iterator_new(iterator, actor_handle);
        prop = prop_iterator_next(iterator);
        while (prop != 0) {
          if (*(short *)(prop + 0x24) >= 2 && *(short *)(prop + 0x24) <= 3) {
            dx = position[0] - *(float *)(prop + 0xbc);
            dy = position[1] - *(float *)(prop + 0xc0);
            dz = position[2] - *(float *)(prop + 0xc4);
            if (dz * dz + dy * dy + dx * dx < *(float *)0x25337c) {
              usable = 0;
              break;
            }
          }
          prop = prop_iterator_next(iterator);
        }
        if (usable) {
          any_usable = 1;
        } else {
          used_flags[index >> 5] |= 1 << (index & 0x1f);
        }
        index++;
      }
      if (!any_usable) {
        result = -1;
      } else {
        if ((short)param_2 == 5) {
          result =
            choose_random_array_element(*(void **)((char *)squad + 0xc8), 0x50,
                                        (short)*positions, 0x10, used_flags);
        } else {
          next = (short)param_3;
          if (next < 0 || next >= *positions) {
            next = 0;
          }
          do {
            forward = 1;
            switch ((short)param_2) {
            case 2:
              forward = 1;
              break;
            case 3:
              if (next == 0) {
                forward = 1;
              } else if (next == *positions - 1) {
                forward = 0;
              } else if (param_4 != NULL) {
                forward = *(char *)param_4;
              } else {
                forward = 1;
              }
              break;
            case 4:
              forward = (char)(game_time_get() & 1);
              break;
            }
            if (param_4 != NULL) {
              *(char *)param_4 = forward;
            }
            if (forward) {
              next++;
              if (next >= *positions) {
                next = 0;
              }
            } else {
              next--;
              if (next < 0) {
                next = (short)*positions - 1;
              }
            }
          } while (used_flags[next >> 5] & (1 << (next & 0x1f)));
          result = next;
        }
      }
    }
  }
  return result;
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
    if (*(short *)(actor + 0xa2) != -1 &&
        actor_path_has_path(actor_handle) != '\0') {
      dist2 =
        distance_squared3d((const float *)&((actor_t *)actor)->body_position,
                           (const float *)(actor + 0xa8));
      tolerance = actor_destination_tolerance(actor_handle);
      if (!(tolerance > *(float *)0x253398)) {
        tolerance = *(float *)0x253398;
      }
      if (dist2 > tolerance * tolerance) {
        goto update_command;
      }
    }
    if (*(short *)(actor + 0x9e) <= 0 && *(char *)(actor + 0xa6) == '\0') {
      unit = object_get_and_verify_type(((actor_t *)actor)->meta_unit_index, 3);
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
 * its timeslice byte is set, evaluate a look target
 * (actor_active_select_firing_position) and hand the result to the
 * firing-position selector (actor_change_firing_position).  Returns true when
 * actor+0x280 (short) is zero, on both paths.
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
 * Confirmed actor_active_select_firing_position pushes (last-to-first,
 * 0x12981..0x1299b): &local_4, big_buf, &local_8, local_48, state_buf,
 * actor_handle. Confirmed actor_change_firing_position pushes (last-to-first,
 * 0x129aa..0x129be): local_4, big_buf, local_8, local_48, EAX
 * (actor_active_select_firing_position result), actor_handle. ADD ESP,0x3c at
 * 0x129c4 cleans csmemset (0xc) + both 6-arg calls (0x18 each); the
 * actor_change_firing_position return value is discarded. Confirmed: XOR
 * EAX,EAX / CMP word ptr [ESI+0x280],AX / SETZ AL — byte return. */
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
    result = actor_active_select_firing_position(
      actor_handle, state_buf, local_48, &local_8, big_buf, &local_4);
    actor_change_firing_position(actor_handle, result, local_48, local_8,
                                 (unsigned int)big_buf, (char)local_4);
  }
  return *(short *)(actor + 0x280) == 0;
}

/* action_avoid_control (0x129f0)
 * Fill in the actor's control/output block for the avoid action: pick the
 * 0x3e8 / 0x3ec mode pair from the actor's target type and danger-zone type,
 * then stamp the common tail (0x3fc = 4, 0x426 copied from 0x358, four zero
 * bytes).
 *
 * Confirmed: cdecl, one stack arg at [EBP+0x8] (0x129f3 MOV EAX,[EBP+0x8]);
 *   the kb decl was `(void)` but the dword is pushed to datum_get.
 * Confirmed: MOV ECX,[0x6325a4] / PUSH EAX / PUSH ECX / CALL 0x119320 ->
 *   datum_get(actors_data, actor_handle); ADD ESP,0x8 (cdecl, 2 args).
 *   All later accesses are against the datum_get result in EAX, no base bias.
 * Confirmed: MOV ECX,0x5 / XOR EDX,EDX hold the two literals; CMP word ptr
 *   [EAX+0x268],CX / JL 0x12a5e — signed 16-bit compare of
 *   actor->target.target_type against 5.
 * Confirmed >=5 path (0x12a16): MOV byte [EAX+0x454],1 / MOV word
 *   [EAX+0x3e8],7 / MOV word [EAX+0x3ec],2.
 * Confirmed <5 path (0x12a5e): CMP word [EAX+0x280],DX / MOV word
 *   [EAX+0x3e8],CX(5) / JLE 0x12a26 — the 0x3e8 store happens on both
 *   sub-paths; >0 takes MOV word [EAX+0x3ec],CX(5), otherwise the shared
 *   0x3ec = 2 store at 0x12a26.
 * Confirmed tail (0x12a2f): MOV CL,[EAX+0x358] / MOV word [EAX+0x3fc],4 /
 *   MOV byte [EAX+0x426],CL / [0x427],[0x428],[0x424],[0x425] = DL (0).
 *   No return value (POP EBP / RET).
 * Unknown: meanings of 0x358, 0x3e8, 0x3ec, 0x3fc, 0x424-0x428 and 0x454; no
 *   string or assert evidence in this function, so the fields keep their
 *   field_<hex> names. */
void action_avoid_control(int actor_handle)
{
  actor_t *actor;

  actor = (actor_t *)datum_get(*(data_t **)0x6325a4, actor_handle);
  if (actor->target_target_type >= 5) {
    actor->field_454 = 1;
    actor->field_3e8 = 7;
  } else {
    actor->field_3e8 = 5;
    if (actor->danger_zone_danger_type > 0) {
      actor->field_3ec = 5;
      goto tail;
    }
  }
  actor->field_3ec = 2;
tail:
  actor->field_3fc = 4;
  actor->field_426 = actor->field_358;
  actor->field_427 = 0;
  actor->field_428 = 0;
  actor->field_424 = 0;
  actor->field_425 = 0;
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

/* 0x12ad0 — charge range for an actor: action_type is compared as a 16-bit
 * value (CMP SI); 0x2533c0 is 0.0f. */
float FUN_00012ad0(int actor_handle, int action_type, void *charge_state)
{
  char *actor;
  char *definition;
  float range;
  float limit;

  actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
  range = 0.0f;
  if ((short)action_type == 2 || (short)action_type == 3) {
    definition = (char *)tag_get(0x61637472, *(int *)(actor + 0x58));
    if ((short)action_type == 3 && !(0.0f > *(float *)(definition + 0x388))) {
      range = *(float *)(definition + 0x388);
    }
    if (*(char *)((char *)charge_state + 0x30) != '\0') {
      if (!(range > *(float *)(definition + 0x37c))) {
        range = *(float *)(definition + 0x37c);
      }
      return range;
    }
    limit = *(float *)(definition + 0x37c) +
            *(float *)((char *)charge_state + 0x34);
    if (!(range > limit)) {
      range = limit;
    }
    return range;
  }
  if ((short)action_type == 4 || (short)action_type == 0) {
    if (actor_has_ranged_weapon(actor_handle) &&
        *(short *)(actor + 0x268) >= 7 &&
        !(0.0f > *(float *)(actor + 0x608))) {
      range = *(float *)(actor + 0x608);
    }
  }
  return range;
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
float *FUN_00012eb0(float *in, float scale, float *out)
{
  out[0] = scale * in[0];
  out[1] = scale * in[1];
  return out;
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
 * Name from PAL 2342 (real_math.c normalize2d); only operates on v[0] and v[1].
 * If magnitude exceeds epsilon, divides each component by it so v becomes
 * a unit vector. Returns the original magnitude, or 0.0f if too small. */
float normalize2d(float *v)
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
float *FUN_00012fb0(float *in, float scale, float *out)
{
  out[0] = scale * in[0];
  out[1] = scale * in[1];
  out[2] = scale * in[2];
  return out;
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
     assert_valid_real_normal3d (actor_looking.c:529 via
     actor_look_decode_direction).  Read the threshold as a double to restore
     the original early-out. */
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
float *FUN_00013090(float *a, float *b, float *out)
{
  out[0] = a[0] - b[0];
  out[1] = a[1] - b[1];
  out[2] = a[2] - b[2];
  return out;
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


/* action_charge_perform (0x13120)
 * Per-tick evaluation of an actor's charge action: decides whether to keep
 * advancing on the target prop, handles melee / leaping-melee / stalking /
 * close-range goals, launches melee attacks and leaps, keeps the actor
 * moving toward the prop, and returns whether the action is finished.
 *
 * Confirmed TU: asserts "!actor->meta.swarm" with
 *   "c:\halo\SOURCE\ai\action_charge.c" (lines 0x120 and 0x1e7). The kb
 *   object here (vector_math.obj) lumps the action_* code of 0x12000-0x13dce.
 * Confirmed: cdecl, one stack arg [EBP+0x8] (actor index; EBX), result in
 *   AL from the byte at [EBP-3] (bool, not void as previously declared).
 * Confirmed: EDI = actor + 0x9c is the charge state block; all state
 *   offsets below are relative to it.
 * Confirmed: callee FUN_00012ad0 takes the actor index in EBX and the goal
 *   in ESI (kb @<reg>), third arg pushed = the state block.
 * Unknown: most actor/prop/definition field meanings; kept as raw offsets
 *   like the rest of this TU. */
bool action_charge_perform(int actor_index)
{
  char *actor;
  unsigned int *definition;
  char *variant_definition;
  char *firing_variant_definition;
  char *state;
  char *prop;
  bool result;

  actor = (char *)datum_get(*(data_t **)0x6325a4, actor_index);
  definition = (unsigned int *)tag_get(0x61637472, *(int *)(actor + 0x58));
  variant_definition = (char *)tag_get(0x61637476, *(int *)(actor + 0x5c));
  firing_variant_definition =
    actor_combat_get_firing_variant_definition(actor_index);
  state = actor + 0x9c;
  prop = NULL;
  result = false;

  if (*(int *)(actor + 0x270) != -1) {
    prop = (char *)datum_get(*(data_t **)0x5ab23c, *(int *)(actor + 0x270));

    if (*(int *)(actor + 0x1b0) != -1) {
      *(char *)(state + 0x28) = 1;
    } else if (*(short *)(state + 4) == 5 || *(short *)(state + 4) == 4) {
      *(char *)(state + 0x28) = 1;
    } else if (*(short *)(state + 4) == 2 || *(short *)(state + 4) == 3) {
      float abort_range = 3.4028235e38f;
      char check_range = 1;
      char berserk_ranges = *(char *)(actor + 0x378);

      if (!actor_has_ranged_weapon(actor_index)) {
        berserk_ranges = 1;
      }

      if (*(char *)(actor + 6)) {
        display_assert("!actor->meta.swarm",
                       "c:\\halo\\SOURCE\\ai\\action_charge.c", 0x120, true);
        system_exit(-1);
      }

      if (*(char *)(state + 6) || *(char *)(state + 0xb) ||
          *(char *)(state + 0xc)) {
        check_range = 0;
      } else if (*(char *)(actor + 0x378) ||
                 !actor_has_ranged_weapon(actor_index)) {
        abort_range = berserk_ranges ? *(float *)(variant_definition + 0x174) :
                                       *(float *)(variant_definition + 0x164);
      }

      if (*(char *)(actor + 0x1cb)) {
        float maximum_abort_range =
          (0.0f > *(float *)&definition[0xdf] ? 0.0f :
                                                *(float *)&definition[0xdf]) +
          0.8f;

        abort_range =
          abort_range < maximum_abort_range ? abort_range : maximum_abort_range;
      }

      if (check_range && *(float *)(prop + 0x11c) > abort_range) {
        *(char *)(state + 8) = 1;
      } else {
        *(int *)(actor + 0x380) = game_time_get();
        *(char *)(state + 0x28) = 1;

        if (check_range) {
          if (*(short *)(state + 4) == 2) {
            if (*(float *)&definition[0xe2] == 0.0f ||
                *(float *)&definition[0xe4] == 0.0f) {
              *(char *)(state + 0xa) = 0;
            } else if (*(char *)(prop + 0x130) || *(short *)(prop + 0x9c) > 0) {
              *(char *)(state + 0xa) = 1;
            }

            if (*(char *)(state + 0xa) &&
                (*(short *)(prop + 0x9c) > 0 ||
                 *(float *)(prop + 0x11c) >
                   *(float *)&definition[0xe1] * 1.5f)) {
              *(short *)(state + 4) = 3;
            }
          } else if (*(float *)(prop + 0x11c) < *(float *)&definition[0xe1]) {
            *(short *)(state + 4) = 2;
            *(char *)(state + 0xa) = 1;
          }
        }
      }
    } else {
      char stalking = (definition[0] & 0x20000) &&
                      *(short *)(actor + 0x6e) >= 5 &&
                      !*(char *)(actor + 0x378);

      *(short *)(state + 4) = stalking ? 1 : 0;
      if (*(short *)(state + 4) == 1) {
        *(char *)(state + 0x24) =
          (*(short *)(prop + 0x38) == 0 || *(short *)(prop + 0x38) == 1) &&
          *(signed char *)(prop + 0x122) <= 2;
        if (*(char *)(state + 0x24) && (definition[0] & 0x40000)) {
          *(char *)(state + 0x28) = 0;
        } else {
          *(char *)(state + 0x28) = 1;
        }
        if (*(char *)(state + 0x24)) {
          (*(short *)(state + 0x26))++;
        }

        *(char *)(state + 0x25) = 0;
        if (!*(char *)(state + 0x24) && *(signed char *)(prop + 0x124) <= 1) {
          *(char *)(state + 0x25) = 1;
        } else if (*(float *)&definition[0xcb] > 0.0f &&
                   *(float *)(prop + 0x11c) >= *(float *)&definition[0xcb]) {
          *(char *)(state + 0x25) = 1;
        }
      } else if (!actor_has_ranged_weapon(actor_index) ||
                 *(char *)(actor + 0x15d)) {
        *(char *)(state + 0x28) = 1;
      } else {
        float minimum_range;
        float maximum_range;
        char *weapon;

        if (*(char *)(actor + 0x378)) {
          minimum_range = *(float *)(firing_variant_definition + 0x168);
          maximum_range = *(float *)(firing_variant_definition + 0x16c);
        } else {
          minimum_range = *(float *)(firing_variant_definition + 0x9c);
          maximum_range = *(float *)(firing_variant_definition + 0xa0);
        }

        weapon = actor_get_weapon_definition(actor_index);
        if (weapon && *(float *)(weapon + 0x40c) > 0.0f) {
          minimum_range = minimum_range > *(float *)(weapon + 0x40c) ?
                            minimum_range :
                            *(float *)(weapon + 0x40c);
        }

        if (*(char *)(state + 0x28)) {
          if (*(float *)(prop + 0x11c) < minimum_range) {
            *(char *)(state + 0x28) = 0;
          }
        } else {
          if (*(float *)(prop + 0x11c) > maximum_range) {
            *(char *)(state + 0x28) = 1;
          }
        }

        if (*(float *)(prop + 0x11c) > 0.7f && *(short *)(prop + 0x38) != 0 &&
            *(short *)(prop + 0x38) != 1) {
          *(char *)(state + 0x28) = 1;
        }
      }
    }
  } else {
    *(char *)(state + 0x28) = 0;
  }

  if (*(char *)(state + 6)) {
    int unit_index = *(int *)(actor + 0x18);

    *(char *)(state + 7) = unit_index == -1 || !unit_is_busy(unit_index);
  } else if (!*(char *)(state + 0xc) &&
             (*(short *)(state + 4) == 2 || *(short *)(state + 4) == 3) &&
             prop) {
    char melee = 0;
    char *debug_info =
      (char *)(*(int *)0x331f58 + (actor_index & 0xffff) * 0x657c);
    real_vector3d direction;
    float distance;

    *(int *)(debug_info + 0x104) = game_time_get();
    vector3d_scale_add((float *)(actor + 0x12c), *(float **)0x31fc44, 0.05f,
                       (float *)(debug_info + 0x108));
    *(real_vector3d *)(debug_info + 0x114) = *(real_vector3d *)(actor + 0x174);
    vector3d_scale_add((float *)(prop + 0xbc), *(float **)0x31fc44, 0.05f,
                       (float *)(debug_info + 0x120));

    if (*(char *)(actor + 6)) {
      display_assert("!actor->meta.swarm",
                     "c:\\halo\\SOURCE\\ai\\action_charge.c", 0x1e7, true);
      system_exit(-1);
    }

    if (*(float *)(prop + 0x11c) < 0.8f) {
      *(char *)(debug_info + 0x139) = 1;
      direction = *(real_vector3d *)(prop + 0xe0);
      melee = 1;
    } else {
      char *unit =
        (char *)object_get_and_verify_type(*(int *)(actor + 0x18), 3);
      float lead_fraction;
      float speed;
      real_vector3d target_point;

      *(char *)(debug_info + 0x139) = 0;
      lead_fraction = 0.0f;
      speed = FUN_00012fe0((float *)(prop + 0xd4));
      if (speed > 0.0f) {
        lead_fraction =
          (FUN_00013070((float *)(prop + 0xd4), (float *)(prop + 0xe0)) /
             speed +
           1.0f) *
          0.5f;
      }

      vector3d_scale_add((float *)(prop + 0xbc), (float *)(prop + 0xd4),
                         lead_fraction * *(short *)(state + 0x32),
                         &target_point.i);
      direction.i = target_point.i - *(float *)(actor + 0x12c);
      direction.j = target_point.j - *(float *)(actor + 0x130);
      direction.k = target_point.k - *(float *)(actor + 0x134);
      vector3d_scale_add(&target_point.i, *(float **)0x31fc44, 0.05f,
                         (float *)(debug_info + 0x13c));

      if (direction.i * *(float *)(prop + 0xe0) +
            direction.j * *(float *)(prop + 0xe4) +
            direction.k * *(float *)(prop + 0xe8) <
          0.0f) {
        distance = 0.0f;
        direction = *(real_vector3d *)(prop + 0xe0);
      } else {
        distance = normalize3d(&direction.i);
        if (distance == 0.0f) {
          direction = *(real_vector3d *)(prop + 0xe0);
        }
      }

      if (*(short *)(state + 4) == 3 && !*(char *)(state + 0xb)) {
        *(float *)(debug_info + 0x148) = *(float *)&definition[0xe1];
        *(float *)(debug_info + 0x14c) = *(float *)&definition[0xe2];

        if (distance < *(float *)&definition[0xe1] &&
            *(short *)(prop + 0x9c) == 0 && !*(char *)(prop + 0x130)) {
          *(char *)(state + 8) = 1;
          *(int *)(actor + 0x380) = -1;
        } else if (distance < *(float *)&definition[0xe2]) {
          float target_velocity_minimum = *(float *)&definition[0xe3] * 0.3f;
          real_vector3d aim_vector;
          float horizontal_velocity;
          float vertical_velocity;

          if (projectile_aim_ballistic(
                *(float *)&definition[0xe3], 1.0f, (float *)(actor + 0x12c),
                (float *)(prop + 0xbc), (int)&target_velocity_minimum,
                (float *)&definition[0xe5], NULL, 0, &aim_vector.i, NULL, NULL,
                NULL, &vertical_velocity, &horizontal_velocity)) {
            if (normalize2d(&aim_vector.i) == 0.0f) {
              aim_vector = *(real_vector3d *)(actor + 0x174);
              if (normalize2d(&aim_vector.i) == 0.0f) {
                aim_vector = **(real_vector3d **)0x31fc3c;
              }
            }

            *(char *)(state + 0xc) = 1;
            *(float *)(state + 0x14) = aim_vector.i;
            *(float *)(state + 0x18) = aim_vector.j;
            *(float *)(state + 0x1c) = horizontal_velocity;
            *(float *)(state + 0x20) = vertical_velocity;
          }
        }
      } else if (*(char *)(state + 0x30)) {
        *(float *)(debug_info + 0x148) = 0.0f;
        *(float *)(debug_info + 0x14c) = *(float *)&definition[0xdf];

        if (distance < *(float *)&definition[0xdf]) {
          melee = 1;
        } else if (distance < *(float *)&definition[0xe9]) {
          real_vector3d relative_velocity;

          FUN_00013090((float *)(prop + 0xd4), (float *)(unit + 0x18),
                       &relative_velocity.i);
          if (relative_velocity.i * direction.i +
                relative_velocity.j * direction.j +
                relative_velocity.k * direction.k >
              0.023333333f) {
            melee = 1;
          }
        }
      } else {
        if (*(short *)(state + 4) == 3 && *(char *)(state + 0xb)) {
          distance -= (direction.i * *(float *)(unit + 0x18) +
                       direction.j * *(float *)(unit + 0x1c) +
                       direction.k * *(float *)(unit + 0x20)) *
                      *(short *)(state + 0x32);
        }

        *(float *)(debug_info + 0x148) = *(float *)(state + 0x34);
        *(float *)(debug_info + 0x14c) =
          *(float *)(state + 0x34) + *(float *)&definition[0xdf];
        if (distance < *(float *)(state + 0x34) + *(float *)&definition[0xdf]) {
          melee = 1;
        }
      }
    }

    if (*(char *)(state + 0xc) || (melee && !*(char *)(state + 0x30))) {
      float facing_direction[2];

      facing_direction[0] = direction.i;
      facing_direction[1] = direction.j;
      if (normalize2d(facing_direction) > 0.0f) {
        float minimum_alignment = *(char *)(state + 0xb) ? 0.0f : 0.8660254f;

        if (FUN_00012f60(facing_direction, (float *)(actor + 0x174)) <
            minimum_alignment) {
          melee = 0;
          *(char *)(state + 0xc) = 0;
          *(char *)(state + 9) = 1;
        }
      }
    }

    *(real_vector3d *)(debug_info + 0x12c) = direction;
    *(char *)(debug_info + 0x138) = melee;

    if (melee) {
      float melee_direction[2];

      melee_direction[0] = direction.i;
      melee_direction[1] = direction.j;
      if (normalize2d(melee_direction) == 0.0f) {
        melee_direction[0] = *(float *)(actor + 0x174);
        melee_direction[1] = *(float *)(actor + 0x178);
      }

      if (unit_melee_attack_begin(*(int *)(actor + 0x18), 0,
                                  (int)melee_direction)) {
        ai_communication_event(0x2b, *(int *)(actor + 0x18),
                               *(int *)(prop + 0x18), 3, -1, -1, NULL);
        *(char *)(state + 6) = 1;
      }
    }
  }

  {
    int time = game_time_get();
    float target_range;

    if ((*(short *)(state + 4) == 2 || *(short *)(state + 4) == 3) &&
        !*(char *)(state + 6) && !*(char *)(state + 0xc)) {
      if (*(char *)(state + 0xb)) {
        if (*(short *)(state + 0xe) > 15) {
          *(char *)(state + 8) = 1;
        }
      } else if (*(float *)&definition[0xe0] > 0.0f &&
                 time >= *(int *)state + *(float *)&definition[0xe0] * 30.0f) {
        *(char *)(state + 8) = 1;
      }
    }

    if (*(short *)(state + 4) == 4 || *(short *)(state + 4) == 5) {
      *(int *)(actor + 0x388) = time;
    }

    target_range = FUN_00012ad0(actor_index, *(short *)(state + 4), state);
    *(float *)(state + 0x2c) = target_range;

    if (!*(char *)(actor + 6) && *(char *)(actor + 0x4c)) {
      char moving = 0;

      *(char *)(state + 0x29) = 0;
      if (!*(char *)(state + 6) && !*(char *)(state + 0xb) &&
          !*(char *)(state + 0xc) && *(char *)(state + 0x28)) {
        float minimum_move_range = *(short *)(state + 4) == 3 ? 4.0f : 1.5f;
        float move_range =
          minimum_move_range > target_range ? minimum_move_range : target_range;

        if (actor_move_to_prop(actor_index, *(int *)(actor + 0x270),
                               move_range)) {
          actor_move_keep_moving_past_destination(actor_index);
          moving = 1;
        } else {
          *(char *)(state + 0x29) = 1;
          *(char *)(state + 0x28) = 0;
        }
      }

      if (!moving) {
        actor_move_halt(actor_index);
      }

      if (*(short *)(actor + 0x268) >= 7) {
        char *target_prop =
          (char *)datum_get(*(data_t **)0x5ab23c, *(int *)(actor + 0x270));
        char unreachable = 0;
        char out_of_range = 0;

        if (*(float *)(target_prop + 0x11c) > *(float *)(state + 0x2c)) {
          out_of_range = 1;
        }

        if ((*(short *)(state + 4) == 2 || *(short *)(state + 4) == 3) &&
            (*(char *)(state + 0xb) || *(char *)(state + 0xc) ||
             *(char *)(state + 6))) {
          out_of_range = 0;
        }

        if (out_of_range) {
          if (*(char *)(state + 0x29) || !actor_path_has_path(actor_index)) {
            unreachable = 1;
          } else if (*(float *)(actor + 0x4bc) > *(float *)(state + 0x2c)) {
            unreachable = 1;
          }
        }

        actor_perception_unreachable(actor_index, *(int *)(actor + 0x270),
                                     unreachable);
      }
    }
  }

  if (*(short *)(state + 4) == 2 || *(short *)(state + 4) == 3) {
    result =
      *(char *)(state + 8) || *(char *)(state + 7) || *(char *)(state + 0x29);
  } else if (*(short *)(state + 4) == 4 || *(short *)(state + 4) == 5) {
    result = *(char *)(state + 0x29);
  }

  return result;
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
float *vector3d_add(float *a, float *b, float *out)
{
  out[0] = a[0] + b[0];
  out[1] = a[1] + b[1];
  out[2] = a[2] + b[2];
  return out;
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
