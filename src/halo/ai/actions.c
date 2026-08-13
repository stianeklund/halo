#include "../../common.h"

/* actions.c — AI actor action dispatch.
 *
 * Corresponds to actions.obj.
 * Assertion path: c:\halo\SOURCE\ai\actions.c
 */


/* FUN_0001bba0 (0x1bba0) — Scan a vehicle unit's seats and select the seat
 * whose attach transform yields the greatest score for an actor.
 *
 * Resolves the vehicle object (object_get_and_verify_type(vehicle_handle, 3)),
 * reads its unit definition tag (tag_get('unit', object[0])), then iterates
 * seat indices in [0, unit_def->seat_count @ +0x2e4). For each seat, calls
 * FUN_0001aeb0 to compute three attach vec3s plus a scalar score into scratch
 * buffers; tracks the seat with the maximum score (update only when the new
 * score exceeds the running best). For each non-NULL output pointer, the best
 * seat's corresponding attach vec3 is written on exit. Returns the winning
 * seat index, or -1 if no seat produced a qualifying score.
 *
 * The seat index is compared as a 16-bit value (movsx si before cmp),
 * preserving the original truncating loop bound. The best-vector scratch is
 * deliberately left uninitialized when no seat qualifies, matching the
 * original codegen (garbage is written to non-NULL outputs, which the caller
 * ignores when the returned index is -1).
 *
 * Confirmed: object_get_and_verify_type(vehicle_handle, 3) at 0x1bba0+0xe.
 * Confirmed: tag_get('unit', object[0]) at 0x1bba0+0x17.
 * Confirmed: seat count at unit_def+0x2e4; FCOMP greater-than update. */
int FUN_0001bba0(int actor_handle, int vehicle_handle, float *out_attach0,
                 float *out_attach1, float *out_attach2)
{
  int *object;
  char *unit_def;
  int i;
  int best_index;
  float best_score;
  float score;
  float cand0[3];
  float cand1[3];
  float cand2[3];
  float best0[3];
  float best1[3];
  float best2[3];

  object = (int *)object_get_and_verify_type(vehicle_handle, 3);
  unit_def = (char *)tag_get(0x756e6974 /* 'unit' */, *object);
  best_index = -1;
  best_score = 0.0f;
  for (i = 0; (short)i < *(int *)(unit_def + 0x2e4); i++) {
    if (FUN_0001aeb0(actor_handle, vehicle_handle, (short)i, 0, &cand0[0],
                     &cand1[0], &cand2[0], (int)&score, 0, 0, 0) != '\0') {
      if (best_score < score) {
        best_score = score;
        best0[0] = cand0[0];
        best0[1] = cand0[1];
        best0[2] = cand0[2];
        best1[0] = cand1[0];
        best1[1] = cand1[1];
        best1[2] = cand1[2];
        best2[0] = cand2[0];
        best2[1] = cand2[1];
        best2[2] = cand2[2];
        best_index = i;
      }
    }
  }
  if (out_attach0 != NULL) {
    out_attach0[0] = best0[0];
    out_attach0[1] = best0[1];
    out_attach0[2] = best0[2];
  }
  if (out_attach1 != NULL) {
    out_attach1[0] = best1[0];
    out_attach1[1] = best1[1];
    out_attach1[2] = best1[2];
  }
  if (out_attach2 != NULL) {
    out_attach2[0] = best2[0];
    out_attach2[1] = best2[1];
    out_attach2[2] = best2[2];
  }
  return best_index;
}

/* action_vehicle_setup_impromptu (0x1bcd0) — Build an "impromptu vehicle entry"
 * action state block for an actor and a candidate vehicle, returning whether
 * the actor actually committed to a path toward a seat.
 *
 * The caller-supplied state block is 0x4c bytes (csmemset 0x4c) and is filled
 * as: +0x00 vehicle handle (int), +0x04 chosen seat index (int16), +0x06 flag
 * byte, +0x20/+0x24 the two radii passed in, +0x30 destination vec3 and +0x48
 * an opaque handle (both written by FUN_0001b280).
 *
 * Early rejections, in binary order: actor+0x158 must be -1 (no pending
 * vehicle), actor+0x6 must be clear (actor not suppressed), and actor+0x6c
 * (current action) must not already be 9 (the vehicle action).
 *
 * The vehicle is then qualified. It is disqualified outright when object+0xb6
 * bit 2 (0x4) is set. Otherwise the vehicle's world position minus the actor's
 * position at actor+0x12c is taken, and the vehicle qualifies only when that
 * squared distance is below radius_b squared AND FUN_00012170(object+0x18)
 * (a magnitude/dot over the object's orientation vector) is at or below
 * *0x253f2c. Independently, object+0x38 must be at or above *0x253398.
 *
 * When qualified, FUN_0001bba0 picks the best seat (truncated to int16). A
 * valid seat sets the +0x06 flag, then the actor's unit must have an entry
 * animation for that seat, FUN_0001b280 must produce a destination, and
 * actor_move_to_point must accept it — only then is 1 returned.
 *
 * Confirmed: cdecl, five stack args; ESI holds the state block (advanced by
 * 0x30 before the last two calls), EDI the vehicle handle, BL the qualified
 * flag. Confirmed: datum_get(actor_data, actor_handle) is called TWICE (0x1bce3
 * and 0x1bd5d); the first result feeds actor+0x18 at the animation check, the
 * second feeds the position deltas — not CSE'd in the original.
 * Confirmed: radius_a/radius_b are floats stored as plain dwords to
 * +0x20/+0x24. Confirmed: the assert tail is display_assert(...,1) followed by
 * system_exit(-1) (CALL 0x8e2f0), reason "state_data", line 0x38 of
 * c:\halo\SOURCE\ai\action_vehicle.c.
 * Confirmed: the delta is (vehicle world position - actor position), FSUB in
 * that order; the squared sum accumulates dy*dy + dz*dz then + dx*dx.
 * Confirmed FPU directions: FCOMPP + TEST AH,0x41 / JNZ skips when
 * radius_b*radius_b <= dist^2; FCOMP [0x253f2c] + TEST AH,0x41 / JNZ keeps the
 * qualified flag when the scalar is <= the constant; FCOMP [0x253398] +
 * TEST AH,0x5 / JNP takes the body when object+0x38 >= the constant.
 * Confirmed: the return byte lives at EBP-1 and is loaded into AL before the
 * single RET at 0x1be8c — Ghidra rendered this function as void(void). */
char action_vehicle_setup_impromptu(int actor_handle, int vehicle_handle,
                                    float radius_a, float radius_b,
                                    void *out_action_data)
{
  char *actor;
  char *actor_pos;
  char *object;
  char *state;
  short seat;
  char qualified;
  char result;
  float attach0[3];
  float attach1[3];
  float delta[3];

  result = 0;
  actor = (char *)datum_get(actor_data, actor_handle);
  if (out_action_data == NULL) {
    display_assert("state_data", "c:\\halo\\SOURCE\\ai\\action_vehicle.c", 0x38,
                   1);
    system_exit(-1);
  }
  state = (char *)out_action_data;
  csmemset(state, 0, 0x4c);
  *(float *)(state + 0x20) = radius_a;
  *(float *)(state + 0x24) = radius_b;
  if (((actor_t *)actor)->field_158 != -1) {
    return result;
  }
  if (((actor_t *)actor)->field_006 != '\0') {
    return result;
  }
  if (((actor_t *)actor)->state_action == _actor_action_vehicle) {
    return result;
  }

  actor_pos = (char *)datum_get(actor_data, actor_handle);
  object = (char *)object_get_and_verify_type(vehicle_handle, 3);
  if ((*(unsigned char *)(object + 0xb6) & 4) == 0) {
    object_get_world_position(vehicle_handle, (vector3_t *)delta);
    delta[0] = delta[0] - *(float *)(actor_pos + 0x12c);
    delta[1] = delta[1] - *(float *)(actor_pos + 0x130);
    delta[2] = delta[2] - *(float *)(actor_pos + 0x134);
    if (radius_b * radius_b >
        delta[1] * delta[1] + delta[2] * delta[2] + delta[0] * delta[0]) {
      qualified = 1;
      if (FUN_00012170((float *)(object + 0x18)) <= *(const float *)0x253f2c) {
        goto qualified_resolved;
      }
    }
  }
  /* Single shared XOR BL,BL join in the original, reached from the flag-set
   * else, the out-of-range test, and the fall-through when the scalar exceeds
   * *0x253f2c. */
  qualified = 0;
qualified_resolved:
  if (*(float *)(object + 0x38) >= *(const float *)0x253398 &&
      qualified != '\0') {
    *(int *)state = vehicle_handle;
    seat = (short)FUN_0001bba0(actor_handle, vehicle_handle, &attach0[0],
                               &attach1[0], delta);
    *(short *)(state + 0x4) = seat;
    if (seat != -1) {
      *(unsigned char *)(state + 0x6) = 1;
      if (unit_has_animation_to_enter_seat(((actor_t *)actor)->field_018,
                                           vehicle_handle, seat) != '\0') {
        if (FUN_0001b280(actor_handle, vehicle_handle, &attach0[0], &attach1[0],
                         delta, NULL, (float *)(state + 0x30),
                         (int *)(state + 0x48)) != '\0') {
          if (actor_move_to_point(actor_handle, (float *)(state + 0x30),
                                  *(int *)(state + 0x48),
                                  vehicle_handle) != '\0') {
            result = 1;
          }
        }
      }
    }
  }
  return result;
}

/* FUN_0001beb0 (0x1beb0) — Update an actor's pursuit/follow state and, when the
 * actor is not suppressed (actor+0x6 clear), either drive it toward its pursuit
 * prop or run the no-pursuit path.
 *
 * Refreshes the nearby-actor scan via actor_pursuit_find_nearby_actors(handle,
 * actor+0x1cc) which republishes the chosen prop handle at actor+0x1d0, then
 * decides two output flags: actor+0x9f ("move toward the prop") and actor+0x9c
 * (the returned state byte).
 *
 * When actor+0x9d is set: with no prop (handle == -1) it seeds the countdown at
 * actor+0xaa to 150 if it is zero; otherwise it sets actor+0x9c once
 * game_time_get() has reached actor+0xa4 + 0xa8c (2700 ticks).
 *
 * When actor+0x9d is clear: actor+0x9c is set, and with a valid prop the prop
 * record is resolved and its scalar at prop+0x11c compared against two .rdata
 * constants. prop+0x32 (signed int16) below 2, or prop+0x11c at/above
 * *0x253f78, keeps actor+0x9c set; otherwise prop+0x11c above *0x253f30 (and
 * not already blocked via actor+0xa0) selects the move-to-prop path
 * (actor+0x9f = 1, actor+0x9c = 0), and anything else clears both.
 *
 * Confirmed: cdecl, one stack arg at EBP+8 held in EDI for the whole body;
 * ESI = datum_get(actor_data, actor_handle). Confirmed: the function returns
 * actor+0x9c in AL (MOV AL,[ESI+0x9c] at 0x1bf65 and the shared 0x1c025 load
 * before the single RET at 0x1c02b) — kb.json previously declared it
 * void(void). Confirmed field widths: actor+0xaa and prop+0x32 are int16
 * (CMPW/MOVW), actor+0xa4 and actor+0x1d0 int32, the rest bytes. Confirmed:
 * FUN_00020280's flag arg is zero-extended (XOR ECX,ECX; MOV CL,[ESI+0x1cc]).
 * Confirmed: the distance passed to actor_move_to_prop is the immediate
 * 0x41000000 = 8.0f. Confirmed FPU directions: FCOMPS [0x253f78] + TEST AH,5 /
 * JP takes the branch when prop+0x11c >= the constant; FCOMPS [0x253f30] +
 * TEST AH,0x41 / JNE takes the else when prop+0x11c <= the constant. */
char FUN_0001beb0(int actor_handle)
{
  int actor;
  int prop;

  actor = (int)datum_get(actor_data, actor_handle);
  if (((actor_t *)actor)->field_04c == '\0') {
    return *(char *)(actor + 0x9c);
  }
  ((actor_t *)actor)->field_09f = 0;
  actor_pursuit_find_nearby_actors(actor_handle, ((actor_t *)actor)->field_1cc);
  if (((actor_t *)actor)->field_09d != '\0') {
    if (((actor_t *)actor)->field_1d0 == -1) {
      if (*(short *)(actor + 0xaa) == 0) {
        *(short *)(actor + 0xaa) = 0x96;
      }
      goto LAB_0001bf35;
    }
    if (game_time_get() < *(int *)(actor + 0xa4) + 0xa8c) {
      goto LAB_0001bf35;
    }
  LAB_0001bf2e:
    *(char *)(actor + 0x9c) = 1;
    goto LAB_0001bf35;
  }
  *(char *)(actor + 0x9c) = 1;
  if (((actor_t *)actor)->field_1d0 == -1) {
    goto LAB_0001bf35;
  }
  prop = (int)datum_get(prop_data, ((actor_t *)actor)->field_1d0);
  if (((actor_t *)actor)->field_09e == '\0' ||
      ((actor_t *)actor)->field_0a0 != '\0') {
    if (*(short *)(prop + 0x32) < 2 ||
        *(const float *)0x253f78 <= *(float *)(prop + 0x11c)) {
      goto LAB_0001bf2e;
    }
    if (((actor_t *)actor)->field_0a0 != '\0') {
      goto LAB_0001c009;
    }
  }
  /* LAB_0001bfdf */
  if (*(float *)(prop + 0x11c) > *(const float *)0x253f30) {
    ((actor_t *)actor)->field_09f = 1;
    *(char *)(actor + 0x9c) = 0;
    goto LAB_0001bf35;
  }
  prop = 0;
LAB_0001c009:
  ((actor_t *)actor)->field_09f = prop;
  *(char *)(actor + 0x9c) = prop;
LAB_0001bf35:
  if (*(char *)(actor + 6) == '\0') {
    if (((actor_t *)actor)->field_09f != '\0') {
      if (actor_move_to_prop(actor_handle, ((actor_t *)actor)->field_1d0,
                             8.0f) == '\0') {
        ((actor_t *)actor)->field_0a0 = 1;
      }
    } else {
      FUN_0002f1a0(actor_handle);
    }
  }
  return *(char *)(actor + 0x9c);
}

/* FUN_0001c030 (0x1c030) — Initialize actor guard state based on combat status.
 * Sets guard mode (0x3e8) to 3/5/1 depending on whether the actor is a
 * designated combatant, has a valid encounter with positive attack count,
 * or is in a default state. Also sets 0x3fc=3 and clears flags at
 * 0x424-0x428, 0x454. */
void FUN_0001c030(int actor_handle)
{
  char *actor;

  actor = (char *)datum_get(actor_data, actor_handle);
  if (((actor_t *)actor)->field_504 != '\0') {
    ((actor_t *)actor)->field_3e8 = 3;
    ((actor_t *)actor)->field_3ec = 0;
  } else if (((actor_t *)actor)->field_1cc == '\0' &&
             ((actor_t *)actor)->field_1d0 != -1 &&
             ((actor_t *)actor)->field_0a8 > 0) {
    ((actor_t *)actor)->field_3e8 = 5;
    ((actor_t *)actor)->field_3ec = 1;
    ((actor_t *)actor)->field_3f0 = ((actor_t *)actor)->field_1d0;
  } else {
    ((actor_t *)actor)->field_3e8 = 1;
  }
  ((actor_t *)actor)->field_3fc = 3;
  ((actor_t *)actor)->field_454 = 0;
  ((actor_t *)actor)->field_426 = 0;
  ((actor_t *)actor)->field_427 = 0;
  ((actor_t *)actor)->field_428 = 0;
  ((actor_t *)actor)->field_424 = 0;
  ((actor_t *)actor)->field_425 = 0;
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
  if (((actor_t *)actor)->field_160 != '\0') {
    return 0;
  }
  *(char *)(state_data + 1) = ((actor_t *)actor)->field_1cc;
  *(char *)(state_data + 2) = param_2;
  *(int *)(state_data + 8) = game_time_get();
  *(int16_t *)(state_data + 0xe) = 0;
  *(int16_t *)(state_data + 0xc) = 0x78;
  *(char *)(state_data + 3) = 1;
  *(int16_t *)(state_data + 0x10) =
    random_range((unsigned int *)get_global_random_seed_address(), 300, 600);
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
  if (((actor_t *)actor)->field_0ac > 0) {
    sVar1 = ((actor_t *)actor)->field_0ac - 1;
    ((actor_t *)actor)->field_0ac = sVar1;
    if (sVar1 == 0) {
      if (((actor_t *)actor)->field_018 != -1) {
        FUN_00046f10(0x11, ((actor_t *)actor)->field_018, -1, -1, -1, -1, 0);
      }
      ((actor_t *)actor)->field_0ac = random_range(
        (unsigned int *)get_global_random_seed_address(), 300, 600);
    }
  }
  if (*(int16_t *)(actor + 0xaa) > 0) {
    sVar1 = *(int16_t *)(actor + 0xaa) - 1;
    *(int16_t *)(actor + 0xaa) = sVar1;
    if (sVar1 == 0) {
      if (((actor_t *)actor)->field_09d != '\0' &&
          ((actor_t *)actor)->field_018 != -1) {
        FUN_00046f10(0x14, ((actor_t *)actor)->field_018, -1, -1, -1, -1, 0);
      }
      *(char *)(actor + 0x9c) = 1;
    }
  }
  if (((actor_t *)actor)->field_09f == '\0' &&
      ((actor_t *)actor)->field_0a8 > 0) {
    ((actor_t *)actor)->field_0a8 = ((actor_t *)actor)->field_0a8 - 1;
  }
}

/* 0x1c270 — encounter_get_squad: return pointer to a squad record by index.
 *
 * Validates squad_index is in [0, MAXIMUM_SQUADS_PER_ENCOUNTER) and also
 * less than encounter->squad_count (at encounter+0x6).  Computes the
 * absolute squad index as encounter->squad_base (encounter+0x4) +
 * squad_index, validates it is in [0, MAXIMUM_SQUADS_PER_MAP), then
 * returns squad_array + absolute_index * 0x20.
 *
 * __FILE__ = c:\halo\source\ai\encounters.h (inline helper defined there,
 * compiled as an out-of-line instance here).
 * MAXIMUM_SQUADS_PER_ENCOUNTER = 0x40; MAXIMUM_SQUADS_PER_MAP = 0x400.
 * squad_array (0x5ab278) is a flat game_state_malloc block of 0x8000 bytes
 * (0x400 * 0x20), initialized by encounters_initialize (0x58eb0).
 *
 * Confirmed: encounter+0x4 = squad_base (int16_t), encounter+0x6 =
 *   squad_count (int16_t).  Each squad record is 0x20 bytes.
 * Confirmed: squad_array pointer at [0x5ab278] (MOVSX EAX,SI; SHL EAX,5;
 *   ADD EAX,[0x5ab278]).
 */
char *encounter_get_squad(char *encounter, int16_t squad_index)
{
  int16_t squad_absolute;

  if (squad_index < 0 || squad_index >= 0x40 ||
      squad_index >= *(int16_t *)(encounter + 6)) {
    display_assert(
      "squad_index>=0 && squad_index<MAXIMUM_SQUADS_PER_ENCOUNTER && "
      "squad_index<encounter->squad_count",
      "c:\\halo\\source\\ai\\encounters.h", 0xdc, 1);
    system_exit(-1);
  }
  squad_absolute = *(int16_t *)(encounter + 4) + squad_index;
  if (squad_absolute < 0 || squad_absolute >= 0x400) {
    display_assert(
      "squad_absolute_index>=0 && squad_absolute_index<MAXIMUM_SQUADS_PER_MAP",
      "c:\\halo\\source\\ai\\encounters.h", 0xdf, 1);
    system_exit(-1);
  }
  return *(char **)0x5ab278 + (int16_t)squad_absolute * 0x20;
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
  action = ((actor_t *)actor)->state_action;

  assert_halt(action >= 0 && action < NUMBER_OF_ACTOR_ACTIONS);

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
  action = ((actor_t *)actor)->state_action;

  assert_halt(action >= 0 && action < NUMBER_OF_ACTOR_ACTIONS);

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
  action = ((actor_t *)actor)->state_action;

  assert_halt(action >= 0 && action < NUMBER_OF_ACTOR_ACTIONS);

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
  action = ((actor_t *)actor)->state_action;

  assert_halt(action >= 0 && action < NUMBER_OF_ACTOR_ACTIONS);

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
  action = ((actor_t *)actor)->state_action;

  assert_halt(action >= 0 && action < NUMBER_OF_ACTOR_ACTIONS);

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
  action = ((actor_t *)actor)->state_action;

  assert_halt(action >= 0 && action < NUMBER_OF_ACTOR_ACTIONS);

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
  actr_tag = (int *)tag_get(0x61637472, ((actor_t *)actor)->field_058);
  result = 0;
  if ((((actor_t *)actor)->field_2f0 != '\0') &&
      ((*(unsigned int *)actr_tag & 0x400) != 0)) {
    panic_type = ((actor_t *)actor)->stimuli_panic_type;
    if ((panic_type == 0) ||
        (((actor_t *)actor)->stimuli_panic_prop_index == -1)) {
      ((actor_t *)actor)->stimuli_panic_prop_index =
        ((actor_t *)actor)->field_2f4;
    }
    if (panic_type < 8) {
      panic_type = 7;
    }
    ((actor_t *)actor)->stimuli_panic_type = panic_type;
    ((actor_t *)actor)->field_2f0 = 0;
    result = 1;
  }
  assert_halt(((actor_t *)actor)->stimuli_panic_type == 0 ||
              ((actor_t *)actor)->stimuli_panic_prop_index != 0);
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
 * Confirmed: actor_get_best_damaging_prop(actor_handle, 1) = get best target
 * prop. Confirmed: assert at line 0x228 checks panic state consistency.
 */
char actor_action_handle_panic_from_damage(int actor_handle)
{
  char *actor;
  int actr_tag;
  short panic_type;
  int result;

  actor = (char *)datum_get(actor_data, actor_handle);
  actr_tag = (int)tag_get(0x61637472, ((actor_t *)actor)->field_058);
  result = 0;
  if (((actor_t *)actor)->field_2ec != '\0') {
    if ((game_connection() != 0) || (*(char *)0x5ac9c8 == '\0')) {
      if (*(float *)(actor + 0x1c0) <= *(float *)(actr_tag + 0x2ac))
        goto check_assert_damage;
    }
    panic_type = ((actor_t *)actor)->stimuli_panic_type;
    if ((panic_type == 0) ||
        (((actor_t *)actor)->stimuli_panic_prop_index == -1)) {
      ((actor_t *)actor)->stimuli_panic_prop_index =
        actor_get_best_damaging_prop(actor_handle, 1);
    }
    if (((actor_t *)actor)->stimuli_panic_type < 2) {
      ((actor_t *)actor)->stimuli_panic_type = 1;
    }
    ((actor_t *)actor)->field_2ec = 0;
    result = 1;
  }
check_assert_damage:
  assert_halt(((actor_t *)actor)->stimuli_panic_type == 0 ||
              ((actor_t *)actor)->stimuli_panic_prop_index != 0);
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
  if (((actor_t *)actor)->field_1b5 != '\0') {
    prop_handle = -1;
    if (((actor_t *)actor)->field_018 != -1) {
      unit = (int)object_get_and_verify_type(((actor_t *)actor)->field_018, 3);
      responsible = ai_get_responsible_unit(*(int *)(unit + 0x3c0), 1);
      if (responsible != -1) {
        prop_handle = prop_get_active_by_unit_index(actor_handle, responsible);
      }
    }
    panic_type = ((actor_t *)actor)->stimuli_panic_type;
    if ((panic_type == 0) ||
        (((actor_t *)actor)->stimuli_panic_prop_index == -1)) {
      ((actor_t *)actor)->stimuli_panic_prop_index = prop_handle;
    }
    if (panic_type < 0xd) {
      panic_type = 0xc;
    }
    ((actor_t *)actor)->stimuli_panic_type = panic_type;
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
  if (((actor_t *)actor)->field_1b0 != -1) {
    projectile = (int)object_try_and_get_and_verify_type(
      ((actor_t *)actor)->field_1b0, 0xffffffff);
    prop_handle = -1;
    if (projectile != 0) {
      responsible = ai_get_responsible_unit(*(int *)(projectile + 0x74), 1);
      if (responsible != -1) {
        prop_handle = prop_get_active_by_unit_index(actor_handle, responsible);
      }
    }
    panic_type = ((actor_t *)actor)->stimuli_panic_type;
    if ((panic_type == 0) ||
        (((actor_t *)actor)->stimuli_panic_prop_index == -1)) {
      ((actor_t *)actor)->stimuli_panic_prop_index = prop_handle;
    }
    if (panic_type < 0xb) {
      panic_type = 10;
    }
    ((actor_t *)actor)->stimuli_panic_type = panic_type;
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
  if (((actor_t *)actor)->field_1b4 != '\0') {
    panic_type = ((actor_t *)actor)->stimuli_panic_type;
    if ((int)panic_type < 0xc) {
      panic_type = 0xb;
    }
    ((actor_t *)actor)->stimuli_panic_type = panic_type;
    ((actor_t *)actor)->stimuli_panic_prop_index = -1;
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
  actr_tag = (int *)tag_get(0x61637472, ((actor_t *)actor)->field_058);
  result = 0;
  if (((*(unsigned int *)actr_tag & 0x80000) != 0) &&
      (((actor_t *)actor)->field_1c9 == '\0') &&
      (((actor_t *)actor)->field_06e >= 5)) {
    berserk_state = ((actor_t *)actor)->field_310;
    if (berserk_state < 2) {
      berserk_state = 1;
    }
    ((actor_t *)actor)->field_310 = berserk_state;
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
  actr_tag = (char *)tag_get(0x61637472, ((actor_t *)actor)->field_058);
  if (4 < ((actor_t *)actor)->field_06e) {
    prop = (char *)datum_get(*(data_t **)0x5ab23c,
                             ((actor_t *)actor)->target_target_prop_index);
    prop_handle = ((actor_t *)actor)->target_target_prop_index;
    assert_halt(prop_handle != -1);
    dist = *(float *)(prop + 0x11c);
    threshold = *(float *)(actr_tag + 0x3a0);
    if (dist < threshold) {
      berserk_state = ((actor_t *)actor)->field_310;
      if (berserk_state < 3) {
        berserk_state = 2;
      }
      ((actor_t *)actor)->field_310 = berserk_state;
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
  actr_tag = (int)tag_get(0x61637472, ((actor_t *)actor)->field_058);
  result = 0;
  if (((actor_t *)actor)->field_2ec != '\0') {
    if (*(float *)(actor + 0x1c0) > *(float *)(actr_tag + 0x398)) {
      if (*(float *)(actor + 0x1b8) < *(float *)(actr_tag + 0x39c)) {
        berserk_state = ((actor_t *)actor)->field_310;
        if (berserk_state < 4) {
          berserk_state = 3;
        }
        ((actor_t *)actor)->field_310 = berserk_state;
        ((actor_t *)actor)->field_2ec = 0;
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
  if ((((actor_t *)actor)->field_090 != -1) &&
      (0 < ((actor_t *)actor)->field_092)) {
    result = 1;
  }
  if (*(int *)(actor + 0x34) != -1) {
    squad = encounter_get_squad(
      datum_get(*(data_t **)0x5ab270, *(int *)(actor + 0x34)),
      ((actor_t *)actor)->field_03a);
    if (0 < *(short *)(squad + 0x12)) {
      if (((actor_t *)actor)->field_06e >= 5) {
        encounter_squad_timer_expire(*(int *)(actor + 0x34),
                                     ((actor_t *)actor)->field_03a);
      } else {
        result = 1;
      }
    }
  }
  if (((actor_t *)actor)->state_action == _actor_action_obey) {
    if ((((actor_t *)actor)->field_09e == '\0') &&
        (((actor_t *)actor)->field_0a1 == '\0')) {
      return 1;
    }
  }
  return result;
}

/* FUN_0001cb30 (0x1cb30) — Per-record action cooldown / dedup gate.
 * Returns 0 (deny) only when record_index equals the record's stored id at
 * +0x390 AND the game clock has not yet reached the deadline at +0x394;
 * otherwise returns 1 (allow). The record is fetched via
 * datum_get(actor_data, datum_handle), where datum_handle arrives in EAX.
 * Confirmed: PUSH EAX (@eax) then PUSH actor_data before CALL datum_get at
 * 0x1cb3d; game_time_get() at 0x1cb52; fields +0x390 (int, equality via JNZ)
 * and +0x394 (int, game-time deadline via JGE). Return is a bool in AL. */
char FUN_0001cb30(int record_index, int datum_handle /* @<eax> */)
{
  char *record;

  record = (char *)datum_get(actor_data, datum_handle);
  if (record_index == *(int *)(record + 0x390)) {
    if (game_time_get() < *(int *)(record + 0x394)) {
      return 0;
    }
  }
  return 1;
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
  if (((actor_t *)actor)->field_158 == -1) {
    goto done_clear;
  }
  berserk_nearby = 0;
  local_5 = 0;
  FUN_00064540((int *)iter_buf, actor_handle);
  prop = FUN_00064570((int *)iter_buf);
  while (prop != 0) {
    if (((1 < *(short *)(prop + 0x24)) && (*(short *)(prop + 0x24) < 4)) &&
        (*(char *)(prop + 0x12e) != '\0') && (*(char *)(prop + 0x60) != '\0') &&
        (*(int *)(prop + 0x110) == ((actor_t *)actor)->field_158)) {
      berserk_nearby = 1;
      local_5 = 1;
      break;
    }
    prop = FUN_00064570((int *)iter_buf);
  }
  if (((actor_t *)actor)->field_2ed != '\0') {
    berserk_nearby = 1;
  }
  if ((((actor_t *)actor)->field_160 == '\0') ||
      ((((actor_t *)actor)->field_1b0 == -1) &&
       ((((actor_t *)actor)->danger_zone_danger_type != 2 ||
         (((actor_t *)actor)->field_28a == '\0'))))) {
    if (!berserk_nearby) {
      goto done_clear;
    }
  } else {
    local_5 = 1;
  }
  ((actor_t *)actor)->field_38c = local_5;
  exit_ok = unit_try_and_exit_seat(((actor_t *)actor)->field_018);
  if (exit_ok != '\0') {
    ((actor_t *)actor)->field_390 = ((actor_t *)actor)->field_158;
    t = game_time_get();
    ((actor_t *)actor)->field_394 = t + 0xb4;
    ((actor_t *)actor)->field_38c = 0;
    ((actor_t *)actor)->field_2ed = 0;
    return 1;
  }
  ((actor_t *)actor)->field_38c = 0;
  ((actor_t *)actor)->field_2ed = 0;
  return result;
done_clear:
  ((actor_t *)actor)->field_2ed = 0;
  return result;
}

/* actor_action_allow_cover_seeking (0x1ccc0)
 * Gating predicate: may the actor begin seeking cover this frame? Result starts
 * true. Only when param_2 == 0 AND the actor+0x1ca suppress byte is clear, two
 * actr-tag cooldown/threshold checks may clear it. Two unconditional overrides
 * follow: actor+0x378 forces deny; actor+0x160 hard-denies (returns 0) ignoring
 * everything above.
 * Confirmed: datum_get(actor_data, actor_handle) at 0x1ccd6.
 * Confirmed: tag_get(0x61637472, actor+0x58) loads the actr tag.
 *   actr_tag+0x2d8 = cover cooldown in seconds (*30 -> ticks @30Hz).
 *   actr_tag+0x324 = threat threshold (float).
 *   actor+0x6e     = short state counter; >= 7 denies.
 *   actor+0x26c    = last cover-seek game time (-1 = none).
 *   actor+0x1c0    = current threat scalar (float).
 * Note: the ftol2 result is spilled and reloaded via MOVSX word (0x1cd9a), so
 * the cooldown tick count is truncated to int16 before being added to
 * actor+0x26c. The (short) cast preserves that. */
char actor_action_allow_cover_seeking(int actor_handle, char param_2)
{
  char *actor;
  int actr_tag;
  short cooldown_ticks;
  char result;

  actor = (char *)datum_get(actor_data, actor_handle);
  actr_tag = (int)tag_get(0x61637472, ((actor_t *)actor)->field_058);
  result = 1;
  if ((param_2 == '\0') && (((actor_t *)actor)->field_1ca == '\0')) {
    if (*(float *)(actr_tag + 0x2d8) > 0.0f) {
      if (((actor_t *)actor)->field_06e >= 7) {
        result = 0;
      } else {
        cooldown_ticks = (short)(int)(*(float *)(actr_tag + 0x2d8) * 30.0f);
        if (((actor_t *)actor)->field_26c != -1) {
          if (game_time_get() <
              (int)cooldown_ticks + ((actor_t *)actor)->field_26c) {
            result = 0;
          }
        }
      }
    }
    if (*(float *)(actr_tag + 0x324) > 0.0f) {
      if (*(float *)(actor + 0x1c0) < *(float *)(actr_tag + 0x324)) {
        result = 0;
      }
    }
  }
  if (((actor_t *)actor)->field_378 != '\0') {
    result = 0;
  }
  if (((actor_t *)actor)->field_160 != '\0') {
    return 0;
  }
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
  assert_halt(((actor_t *)actor)->state_action == _actor_action_guard);
  if (*(char *)(actor + 0xa4) != '\0') {
    return max_state <= ((actor_t *)actor)->field_06e;
  }
  if (((0 < *(short *)(actor + 0x9c)) &&
       (((actor_t *)actor)->field_06e < min_state)) &&
      ((((actor_t *)actor)->field_1e4 < 1 ||
        (((actor_t *)actor)->field_0a1 != '\0')))) {
    return 0;
  }
  return 1;
}

/* actor_action_can_stop_conversing (0x1cfa0) — Check whether an actor may stop
 * its current conversation. Returns 1 if not in a conversation, or if the
 * conversation's flags permit stopping based on the actor's state.
 *
 * Returns char, not int: every return path in the original writes only AL
 * (MOV AL,0x1 @0001cfc2 and @0001d002, XOR AL,AL @0001d023), leaving the
 * upper 24 bits of EAX as whatever the preceding datum_get left there. The
 * adjacent sibling actor_action_can_stop_guarding (0x1cf10) has the same
 * shape and is likewise declared char; both feed an `int` local at the call
 * sites in actors.c, which widens the value implicitly. */
char actor_action_can_stop_conversing(int actor_handle, int flag)
{
  char *actor;
  char *conv;
  char *elem;
  int16_t flags;
  char can_stop;

  (void)flag;

  /* The `if (1)` is a codegen-shaping construct, not RE speculation, and it
   * is what takes this function from 87.8% to an exact 100% byte match. MSVC
   * 7.1 folds the constant condition away (no TEST/Jcc is emitted for it) but
   * still opens a basic block for the body, which lands the shared MOV AL,0x1
   * epilogue at the offset the original uses. Measured alternatives, same
   * reference (delinked/actions_FUN_0001cfa0.obj), all rejected:
   *   - plain nested block `{ ... }` instead of `if (1)`  -> 87.8% (baseline)
   *   - natural early `if (conv_index == -1) return 1;`   -> 86.6%
   *   - casting flags to unsigned char at the bit tests   -> 87.8% (no-op)
   * Only this form reaches 100%, so the compiled bytes are now identical to
   * the original and the layout question is settled by the binary itself. */
  if (1) {
    actor = (char *)datum_get(actor_data, actor_handle);
    /* can_stop is seeded to 1 BEFORE the -1 test and the "not conversing" path
     * falls through to the shared epilogue: the original does MOV AL,0x1
     * @0001cfc2 and then JZ 0x0001d025 @0001cfc4, i.e. it sets the result first
     * and branches straight to POP ESI/POP EBP/RET. Writing this as an early
     * `return 1` instead costs the shared exit and flips the branch polarity.
     */
    can_stop = 1;
    if (((actor_t *)actor)->field_1dc != -1) {
      conv =
        (char *)datum_get(*(data_t **)0x6324ec, ((actor_t *)actor)->field_1dc);
      elem =
        (char *)tag_block_get_element((char *)global_scenario_get() + 0x468,
                                      (int)*(int16_t *)(conv + 2), 0x74);
      flags = *(int16_t *)(elem + 0x20);
      /* Three separate `return 1` statements, not one combined ||-chain: MSVC
       * tail-merges them into the single MOV AL,0x1 epilogue at 0x0001d002, so
       * the second and third tests reach it as BACKWARD jumps
       * (JGE 0x0001d002 @0001d013 and @0001d021). A combined
       * `if (A || B || C) return 1;` places the success block after all three
       * tests instead, which inverts every branch to a forward jne/jl.
       *
       * The comparisons are written >= 9 and >= 6 to match CMP 0x9/JGE and
       * CMP 0x6/JGE rather than the equivalent > 8 / > 5, which MSVC encodes
       * as CMP 0x8/JG. */
      if ((flags & 2) != 0 && ((actor_t *)actor)->field_1f6 != '\0') {
        return 1;
      }
      if ((flags & 4) != 0 && ((actor_t *)actor)->target_target_type >= 9) {
        return 1;
      }
      if ((flags & 8) != 0 && ((actor_t *)actor)->target_target_type >= 6) {
        return 1;
      }
      can_stop = 0;
    }
    return can_stop;
  }
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

  assert_halt(new_action_type >= 0 &&
              new_action_type < NUMBER_OF_ACTOR_ACTIONS);

  table_offset = new_action_type * 0x38;

  assert_halt(*(int *)(0x253fa0 + table_offset) == new_action_type);

  assert_halt(((actor_t *)actor)->state_action >= 0 &&
              ((actor_t *)actor)->state_action < NUMBER_OF_ACTOR_ACTIONS);

  handler = *(action_handler_fn_t *)(0x253fc4 +
                                     ((actor_t *)actor)->state_action * 0x38);
  if (handler != NULL) {
    handler(actor_handle);
  }

  if (*(short *)(0x253fb0 + table_offset) == 0) {
    if (((actor_t *)actor)->field_06a > 2) {
      ((actor_t *)actor)->field_06a = 2;
    }
  } else {
    if (((actor_t *)actor)->field_06a < 3) {
      ((actor_t *)actor)->field_06a = 3;
    }
  }

  actor_clear_discarded_firing_positions(actor_handle, 0);

  if (*(unsigned int *)(0x253fac + table_offset) != 0 && param_3 != 0) {
    csmemcpy(actor + 0x9c, (void *)param_3, *(int *)(0x253fac + table_offset));
  }

  ((actor_t *)actor)->state_action = (short)new_action_type;
  ((actor_t *)actor)->field_070 = 1;

  handler = *(action_handler_fn_t *)(0x253fb4 + (short)new_action_type * 0x38);
  if (handler != NULL) {
    handler(actor_handle);
  }
}

/* actor_action_test_grenade (0x1d180) — Decide whether the actor may throw a
 * grenade this tick, recording the outcome code in its AI-state slot.
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x1d193.
 * Confirmed: tag_get(0x61637476 'actv', actor+0x5c) at 0x1d1a3.
 * Confirmed: game_time_get() stored to slot+0x150 (last-test timestamp).
 * Confirmed: slot = (actor_handle & 0xffff) * 0x657c + *0x331f58.
 * Confirmed: encounter pool datum_get(*0x5ab270, actor+0x34) at 0x1d1fa.
 * Confirmed: FPU chain 0x1d219..0x1d233 that the decompiler drops —
 *            FUN_000b55b0 result is multiplied by actv+0x1a8 (saved to the
 *            EBP-0x4 scratch BEFORE the call), doubled via FLD ST0/FADDP when
 *            actor+0x1ca != 0, then scaled by TICKS_PER_SECOND (*0x253394)
 *            before _ftol2 truncates it to a signed short (MOVSX EDI,AX).
 * Confirmed: the out-short for FUN_00021ae0 is the upper half of the incoming
 *            actor_handle stack slot (LEA EAX,[EBP+0xa] at 0x1d29e), which is
 *            why the handle is copied to a register before that call.
 * Confirmed: CMP AX,[EBX+0x186] / JL is a SIGNED 16-bit compare; the +0x156
 *            store is 16-bit arithmetic (ADD AX,[EDX+0x5c] at 0x1d25a).
 * Slot fields: +0x150 int last test time, +0x154 short result code,
 *   +0x156 short retry delay, +0x158 short count, +0x15a short count required.
 * Result codes at +0x154: 0 already performing an action, 5 encounter grenade
 *   cooldown still running, 6 no target found, 7 not enough grenades,
 *   8 no throw solution, 9 throw rejected, 10 throw accepted (only path that
 *   returns 1). */
char actor_action_test_grenade(int actor_handle)
{
  char *actor;
  char *actv;
  int slot;
  int handle_copy;
  short delay;
  short grenade_count;
  char ok;
  /* Stack frame is exactly SUB ESP,0x18 — these four locals fill it. */
  float cooldown_scale; /* EBP-0x04 */
  int encounter; /* EBP-0x08: encounter ptr, then out_handle */
  int extra; /* EBP-0x0c: game time, then out_extra */
  float grenade_pos[3]; /* EBP-0x18 */
  float delay_f;

  actor = (char *)datum_get(actor_data, actor_handle);
  actv =
    (char *)tag_get(0x61637476 /* 'actv' */, ((actor_t *)actor)->field_05c);
  extra = game_time_get();
  slot = (actor_handle & 0xffff) * 0x657c + *(int *)0x331f58;
  *(int *)(slot + 0x150) = extra;

  if (((actor_t *)actor)->field_158 != -1) {
    *(short *)(slot + 0x154) = 0;
    return 0;
  }

  if (*(int *)(actor + 0x34) != -1) {
    encounter = (int)datum_get(*(data_t **)0x5ab270, *(int *)(actor + 0x34));
    cooldown_scale = *(float *)(actv + 0x1a8);
    delay_f = FUN_000b55b0(0x18, (int)*(unsigned short *)(encounter + 2)) *
              cooldown_scale;
    if (((actor_t *)actor)->field_1ca != '\0') {
      delay_f = delay_f + delay_f;
    }
    delay = (short)(int)(delay_f * TICKS_PER_SECOND);
    if (*(int *)(encounter + 0x5c) != -1 &&
        (int)delay + *(int *)(encounter + 0x5c) > extra) {
      *(short *)(slot + 0x154) = 5;
      *(short *)(slot + 0x156) =
        (short)((delay - (short)extra) + *(short *)(encounter + 0x5c));
      return 0;
    }
  }

  handle_copy = actor_handle;
  /* Nested rather than early-return: VC71 lays the four cold failure blocks
   * out-of-line at the tail in unwind order 9, 7, 8, 6, matching the original
   * (LAB_0001d2f3 / 0x1d305 / 0x1d32c / 0x1d33e). */
  if (actor_combat_find_grenade_target(actor_handle, grenade_pos, &encounter,
                                       &extra) != '\0') {
    /* The out-short aliases the upper half of the actor_handle param slot. */
    ok = FUN_00021ae0(handle_copy, *(float *)(actv + 0x188),
                      *(float *)(actv + 0x19c), grenade_pos,
                      (short *)((char *)&actor_handle + 2));
    if (ok != '\0') {
      grenade_count = *(short *)((char *)&actor_handle + 2);
      if (grenade_count >= *(short *)(actv + 0x186)) {
        if (FUN_00021e50(handle_copy, *(unsigned short *)(actv + 0x182),
                         grenade_pos, encounter, extra) != '\0') {
          *(short *)(slot + 0x154) = 10;
          return 1;
        }
        *(short *)(slot + 0x154) = 9;
        return 0;
      }
      *(short *)(slot + 0x158) = grenade_count;
      *(short *)(slot + 0x154) = 7;
      *(short *)(slot + 0x15a) = *(short *)(actv + 0x186);
      return 0;
    }
    *(short *)(slot + 0x154) = 8;
    return 0;
  }
  *(short *)(slot + 0x154) = 6;
  return 0;
}

/* actor_action_try_to_seek_cover (0x1d350) — Attempt to make the actor seek
 * cover. Gets actor+0x270 as param_2 for FUN_00015040, then calls
 * actor_action_change with action 4 if successful. */
char actor_action_try_to_seek_cover(int actor_handle, char param_2,
                                    char param_3)
{
  char *actor;
  char cVar1;
  short local_88[66];

  actor = (char *)datum_get(actor_data, actor_handle);
  cVar1 =
    FUN_00015040(actor_handle, 0, ((actor_t *)actor)->target_target_prop_index,
                 0, param_2, param_3, local_88);
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
char actor_action_try_to_enter_vehicle(int actor_handle, int param_2,
                                       int param_3, int param_4,
                                       int16_t param_5, int16_t *param_6)
{
  char *actor;
  int16_t i;
  int16_t seat_index;
  int16_t local_seats[16];
  short action_buf[66];

  actor = (char *)datum_get(actor_data, actor_handle);
  if (param_6 == NULL) {
    param_6 = local_seats;
    param_5 = vehicle_scripting_find_available_seats(param_2, param_3, param_4,
                                                     local_seats, 0x10);
  }
  for (i = 0; i < param_5; i++) {
    seat_index = param_6[i];
    if (seat_index != -1 &&
        unit_has_animation_to_enter_seat(((actor_t *)actor)->field_018, param_2,
                                         seat_index) != '\0' &&
        FUN_0001b750(actor_handle, param_2, seat_index, action_buf) != '\0') {
      actor_action_change(actor_handle, 9, (int)action_buf);
      param_6[i] = (int16_t)0xffff;
      return 1;
    }
  }
  return 0;
}

/* actor_get_pursuit_location (0x1d4f0) — Returns the address of the actor's
 * pursuit-location sub-record (actor+0xa4) when the actor's mode word
 * (field_6c) is 7 or 5; otherwise returns NULL.
 *
 * Confirmed: datum_get(actor_data, actor_handle) — MOV EAX,[EBP+8];
 *   MOV ECX,[0x6325a4]; PUSH EAX; PUSH ECX; CALL 0x119320; ADD ESP,0x8.
 *   First PUSH is the last arg, so the pool is arg1 and the handle is arg2.
 * Confirmed: MOV CX,word ptr [EAX+0x6c] — 16-bit field, loaded ONCE and
 *   compared twice (CMP CX,7 / JZ; CMP CX,5 / JNZ). Read before the ADD ESP.
 * Confirmed: hit path is ADD EAX,0xa4 — 0xa4 is used as an ADDRESS (base of a
 *   sub-record), not dereferenced as a value here (contrast the byte read of
 *   field_a4 in FUN_0001d530).
 * Confirmed: miss path is XOR EDX,EDX / MOV EAX,EDX — returns NULL. Ghidra
 *   typed this function `void`, which silently drops the EAX return.
 * Confirmed: the datum_get result is NOT NULL-checked before the +0x6c load;
 *   preserved verbatim (no added guard).
 * Uncertain: the pointed-to type. The name suggests a location (likely
 *   real_point3d) but this function alone gives no typed evidence, so the
 *   return stays void *.
 *
 * MATCH-SENSITIVE SHAPE: the single-return accumulator form below (result
 * pre-set to NULL, assigned only on the hit path) is what reproduces the
 * original's hoisted XOR EDX,EDX / CMP 7 / JZ hit / CMP 5 / JNZ miss layout
 * with MOV EAX,EDX on the out-of-line miss epilogue. Both equivalent
 * early-return forms (`if (mode != 7 && mode != 5) return NULL;` and the
 * ternary) instead compile to XOR EAX,EAX with the null path as fallthrough
 * — verified 76.9% vs 100% under VC71. Do not "simplify" to an early return. */
void *actor_get_pursuit_location(int actor_handle)
{
  char *actor;
  int16_t mode;
  char *result;

  actor = (char *)datum_get(actor_data, actor_handle);
  result = NULL;
  mode = ((actor_t *)actor)->state_action;
  if (mode == 7 || mode == 5) {
    result = actor + 0xa4;
  }
  return result;
}

/* FUN_0001d530 (0x1d530) — Predicate: is the actor at a given absolute index
 * an eligible target of a differing category. Validates actor_handle (@<eax>)
 * via datum_get(actor_data, ...) with the result discarded (validation only),
 * resolves the actor record from actor_index, then gates on a bounded type
 * field (field_6e in [2,4)) and a mode field (field_6c). On a qualifying mode
 * it maps the actor's type word (field_4) through FUN_0003a7f0 and returns 1
 * when the mapped category differs from param_1; otherwise returns 0.
 *
 * Confirmed: datum_get(actor_data, actor_handle@<eax>) at 0x1d53c; two pushes
 *   for datum_get + two for datum_absolute_index_to_index cleaned by one
 *   ADD ESP,0x10. Return value discarded.
 * Confirmed: datum_absolute_index_to_index(actor_data, actor_index) at 0x1d54e;
 *   result used as the actor record base pointer.
 * Confirmed: field_6e bounded (1 < field_6e < 4), word.
 * Confirmed: field_6c mode set {7,5, 8 iff param_1==0, 6 iff field_a4==0 &&
 *   field_9c>0}, word (field_a4 byte, field_9c word).
 * Confirmed: FUN_0003a7f0(*(int16_t *)(actor + 4)) compared to param_1;
 *   MOV AL,1 / MOV BL,AL byte-only return -> char. */
char FUN_0001d530(int actor_handle, char param_1, int actor_index)
{
  char *actor;
  short mode;

  datum_get(actor_data, actor_handle);
  actor = (char *)datum_absolute_index_to_index(actor_data, actor_index);
  if (actor != NULL && 1 < ((actor_t *)actor)->field_06e &&
      ((actor_t *)actor)->field_06e < 4) {
    mode = ((actor_t *)actor)->state_action;
    if (mode == 7 || mode == 5 || (param_1 == '\0' && mode == 8) ||
        (mode == 6 && *(char *)(actor + 0xa4) == '\0' &&
         0 < *(short *)(actor + 0x9c))) {
      if ((char)FUN_0003a7f0(*(int16_t *)(actor + 4)) != param_1) {
        return 1;
      }
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
  if (action_type >= 0 && action_type < NUMBER_OF_ACTOR_ACTIONS) {
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

/* actor_action_debug_color (0x1d620) — Compute the debug-visualization color
 * for an actor's current action.
 *
 * Copies a default color (4 dwords) from the global default-color pointer at
 * 0x2ee6cc into the shared scratch color buffer at 0x6328e0. If the actor's
 * action index (actor+0x6c, signed short) is in range [0, 14), overrides the
 * color from the action_definitions entry's color field (0x253fa8, stride
 * 0x38); that field is a double-indirect pointer to the 4-dword color. Then,
 * if the entry's callback field (0x253fc8, stride 0x38) is non-null, invokes
 * it as (actor_handle, &scratch_color) so the action type may adjust the
 * color. Always returns a pointer to the scratch color buffer.
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x1d62e (pool 0x6325a4);
 *   result is NOT asserted.
 * Confirmed: default color = *(uint32_t **)0x2ee6cc, 4 dwords (pointer deref,
 *   0x1d633: mov ecx,[0x2ee6cc]; mov [ecx+k]).
 * Confirmed: actor+0x6c signed int16 action index; range [0,14) via signed
 *   TEST/JL + CMP 0xe/JGE at 0x1d663 — silent skip (no assert), unlike the
 *   sibling dispatchers.
 * Confirmed: color field 0x253fa8+action*0x38 is DOUBLE-indirect
 *   (0x1d674 mov ecx,[edx+0x253fa8]; 0x1d67a mov edx,[ecx]; 0x1d67c mov
 * ecx,[edx]). Confirmed: callback field 0x253fc8+action*0x38 (action re-read
 * via movsx at 0x1d69f); cdecl (actor_handle, &color), caller ADD ESP,8.
 * Confirmed: returns &scratch (0x6328e0) unconditionally (0x1d6bb mov
 * eax,0x6328e0). Confirmed: __FILE__ "c:\halo\SOURCE\ai\actions.c". */
void *actor_action_debug_color(int actor_handle)
{
  typedef void (*action_debug_color_fn_t)(int, void *);

  char *actor;
  int16_t action;
  uint32_t *color;
  action_debug_color_fn_t callback;

  actor = (char *)datum_get(actor_data, actor_handle);

  color = *(uint32_t **)0x2ee6cc;
  *(uint32_t *)0x6328e0 = color[0];
  *(uint32_t *)0x6328e4 = color[1];
  *(uint32_t *)0x6328e8 = color[2];
  *(uint32_t *)0x6328ec = color[3];

  action = ((actor_t *)actor)->state_action;
  if (action >= 0 && action < NUMBER_OF_ACTOR_ACTIONS) {
    color = *(uint32_t **)(*(int *)(0x253fa8 + action * 0x38));
    *(uint32_t *)0x6328e0 = color[0];
    *(uint32_t *)0x6328e4 = color[1];
    *(uint32_t *)0x6328e8 = color[2];
    *(uint32_t *)0x6328ec = color[3];

    callback =
      *(action_debug_color_fn_t *)(0x253fc8 +
                                   ((actor_t *)actor)->state_action * 0x38);
    if (callback != NULL) {
      callback(actor_handle, (void *)0x6328e0);
    }
  }

  return (void *)0x6328e0;
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
  action = ((actor_t *)actor)->state_action;

  assert_halt(action >= 0 && action < NUMBER_OF_ACTOR_ACTIONS);

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

/* set_real_vector3d (0x1d780) — Store three float values into a 3D vector
 * output pointer. */
void set_real_vector3d(float *out, float x, float y, float z)
{
  out[0] = x;
  out[1] = y;
  out[2] = z;
}

/* 0x1d7a0 (point_to_line_distance3d) — Distance from a point to a 3D line
 * segment. cdecl, 3 pointer args (p1=point, p2=segment start, p3=segment
 * direction). Thin wrapper: passes args straight through to the squared
 * point-to-segment helper FUN_0010cd40, then applies FSQRT (sqrtf). */
float point_to_line_distance3d(float *p1, float *p2, float *p3)
{
  return sqrtf(FUN_0010cd40(p1, p2, p3));
}

/* actor_action_set_default_state (0x1d7c0) — Transition an actor to a default
 * action state. If state == -1 (0xffff), uses actor+0x60 or actor+0x62 as
 * a fallback state index. Dispatches through a 12-entry switch:
 *   cases 0,2-7: lookup table at 0x2542e8 maps state to an action type,
 *     then calls FUN_00012000 to build action data and actor_action_change(2).
 *   case 1: set actor+0x6a = 1, call actor_action_change(1, 0).
 *   case 8: call FUN_00015880, then actor_action_change(6).
 *   case 9: if already in action 6, set actor+0xaa = 1; else try FUN_00015900
 *     and actor_action_change(6).
 *   case 10: set panic state fields, try actor_action_handle_lost_contact,
 *     fallback to FUN_00015880 + actor_action_change(6).
 *   case 11: try FUN_00015040(0xd, ...), then FUN_00015880 fallback.
 * Falls through to a final idle check: if action==0, try FUN_00012000(0, -1)
 * and actor_action_change(2).
 *
 * Confirmed: jump table at 0x1da74. Confirmed: lookup table at 0x2542e8
 * = {0, 0, 0, 1, 2, 3, 4, 5, 0, 0, 0, 0}.
 * Confirmed: game_time_get() throttle with +0x2d cooldown at actor+0x64.
 *
 * noinline (VC71 verification only): the original build emits this as a real
 * out-of-line function at 0x1d7c0 and CALLs it -- the delinked reference for
 * actor_action_handle_lost_contact carries a reloc to FUN_0001d7c0.  Our TU has
 * the body in scope, so cl.exe inlines all ~224 instructions of it into that
 * caller (its 12-way `jmp *` switch, the 0x2542e8 lookup table and the
 * actor+0x60/0x62/0x64/0x6c/0x9c field accesses all appear in the caller's
 * codegen, none of which the reference contains).  That alone held
 * actor_action_handle_lost_contact at 70.3% (830 insns vs the reference's 606)
 * and produced four spurious [LOADW-WARN] hits on the inlined callee's fields.
 *
 * The guard is `_MSC_VER && !__clang__` because our clang build targets
 * i386-pc-win32 and therefore also defines _MSC_VER; this must apply to cl.exe
 * ONLY and must never change the shipped binary's codegen.
 */
#if defined(_MSC_VER) && !defined(__clang__)
__declspec(noinline)
#endif
char actor_action_set_default_state(int actor_handle, short state)
{
  char *actor;
  int game_time;
  int switch_val;
  short local_88[66];
  char result;

  actor = (char *)datum_get(actor_data, actor_handle);
  game_time = game_time_get();
  result = 0;

  /* Throttle: if state == -1 and we have a valid timestamp, skip if within
   * cooldown */
  if (state == (short)-1 && ((actor_t *)actor)->field_064 != -1 &&
      ((actor_t *)actor)->field_064 + 0x2d >= game_time) {
    return 0;
  }

  ((actor_t *)actor)->field_064 = game_time;

  if (state != (short)-1) {
    /* state already specified, skip fallback resolution */
  } else {
    /* Resolve fallback state from actor fields */
    if (*(unsigned short *)(actor + 0x60) != 0xffff) {
      state = ((actor_t *)actor)->field_060;
      ((actor_t *)actor)->field_060 = (short)0xffff;
    } else {
      if (*(unsigned short *)(actor + 0x62) == 0xffff)
        state = 0;
      else
        state = ((actor_t *)actor)->field_062;
    }
  }

  switch_val = (int)state;

  switch (switch_val) {
  case 0:
  case 2:
  case 3:
  case 4:
  case 5:
  case 6:
  case 7:
    if (((actor_t *)actor)->state_action == _actor_action_alert &&
        *(short *)(actor + 0x9c) == *(short *)(0x2542e8 + switch_val * 2))
      break;
    if (FUN_00012000(actor_handle, (int)*(short *)(0x2542e8 + switch_val * 2),
                     -1, (int)local_88))
      goto action_change_2;
    break;
  case 1:
    if (((actor_t *)actor)->field_06a != 1) {
      ((actor_t *)actor)->field_06a = 1;
      actor_action_change(actor_handle, 1, 0);
      result = 1;
      return result;
    }
    break;
  case 8:
    if ((((actor_t *)actor)->state_action != _actor_action_guard ||
         ((actor_t *)actor)->field_0c0 != 1) &&
        FUN_00015880(actor_handle, (char *)local_88)) {
      actor_action_change(actor_handle, 6, (int)local_88);
      result = 1;
      return result;
    }
    break;
  case 9:
    if (((actor_t *)actor)->state_action == _actor_action_guard) {
      if (((actor_t *)actor)->field_0c0 != 3)
        ((actor_t *)actor)->field_0aa = 1;
    } else {
      if (FUN_00015900(actor_handle, 0, (char *)local_88)) {
        actor_action_change(actor_handle, 6, (int)local_88);
        result = 1;
        return result;
      }
    }
    break;
  case 10:
    if (actor_action_try_to_panic(actor_handle) != 3) {
      ((actor_t *)actor)->field_06a = 3;
      ((actor_t *)actor)->field_072 = 2;
      ((actor_t *)actor)->field_06e = 2;
      if (!actor_action_handle_lost_contact(actor_handle) &&
          FUN_00015880(actor_handle, (char *)local_88)) {
        actor_action_change(actor_handle, 6, (int)local_88);
        result = 1;
        return result;
      }
    }
    break;
  case 11:
    if (((actor_t *)actor)->state_action != _actor_action_flee) {
      if (FUN_00015040(actor_handle, 0xd, -1, 1, 0, 0, (short *)local_88)) {
        actor_action_change(actor_handle, 4, (int)local_88);
        result = 1;
        return result;
      }
      if (((actor_t *)actor)->state_action != _actor_action_guard &&
          FUN_00015880(actor_handle, (char *)local_88)) {
        actor_action_change(actor_handle, 6, (int)local_88);
        result = 1;
        return result;
      }
    }
    break;
  }

  /* Final idle fallback: if actor is in action 0, try alert action */
  if (((actor_t *)actor)->state_action == _actor_action_none &&
      FUN_00012000(actor_handle, 0, -1, (int)local_88)) {
  action_change_2:
    actor_action_change(actor_handle, 2, (int)local_88);
    result = 1;
  }

  return result;
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
  if ((((actor_t *)actor)->state_action == _actor_action_none) &&
      (((actor_t *)actor)->field_06a != 0)) {
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
  if (((actor_t *)actor)->field_090 == -1) {
    return 0;
  }
  if (((actor_t *)actor)->field_08e != '\0') {
    goto do_action;
  }
  if (((actor_t *)actor)->field_06a == 0) {
    return 0;
  }
  cVar1 = actor_action_deny_transition(actor_handle);
  if (cVar1 != '\0') {
    return 0;
  }
do_action:
  cVar1 = FUN_00016e70(actor_handle, ((actor_t *)actor)->field_090, action_buf);
  if (cVar1 != '\0') {
    actor_action_change(actor_handle, 0xb, (int)action_buf);
    result = 1;
  }
  ((actor_t *)actor)->field_08e = 0;
  ((actor_t *)actor)->field_090 = -1;
  return result;
}

/* actor_action_handle_surprise (0x1db90) — Process an actor's surprise
 * reaction. If the actor is not dead (field_160 == 0) and the surprise level
 * (field_2ee) is at least as large as the requested type, computes a flee
 * direction, triggers a surprise animation impulse (type 4 = forward, type 5 =
 * backward), fires a sound event (0x29), and optionally queues wild fire and a
 * new combat target. Always clears the surprise level (field_2ee = 0) before
 * returning. */
char actor_action_handle_surprise(int actor_handle, short type)
{
  char *actor;
  char *actv_tag;
  char *prop;
  float direction[2];
  float dot;
  int anim_type;
  int weapon_trigger_index;
  int weapon_state;
  int prop_handle;

  actor = (char *)datum_get(actor_data, actor_handle);
  actv_tag = (char *)tag_get(0x61637476, ((actor_t *)actor)->field_05c);

  if (((actor_t *)actor)->field_160 != '\0') {
    ((actor_t *)actor)->field_2ee = 0;
    return 0;
  }
  if (((actor_t *)actor)->field_2ee < type) {
    ((actor_t *)actor)->field_2ee = 0;
    return 0;
  }

  if (((actor_t *)actor)->field_2f8 != '\0') {
    direction[0] = *(float *)(actor + 0x2fc);
    direction[1] = *(float *)(actor + 0x300);
    magnitude3d(direction);
    dot = direction[1] * ((actor_t *)actor)->control_desired_facing_vector[1] +
          direction[0] * ((actor_t *)actor)->control_desired_facing_vector[0];
    if (dot < 0.0f) {
      direction[0] = -direction[0];
      direction[1] = -direction[1];
      anim_type = 5;
    } else {
      anim_type = 4;
    }
  } else {
    direction[0] = ((actor_t *)actor)->input_facing_vector[0];
    direction[1] = ((actor_t *)actor)->input_facing_vector[1];
    magnitude3d(direction);
    anim_type = 4;
  }

  actor_move_animation_impulse(actor_handle, (short)anim_type,
                               (int *)direction);

  prop_handle = ((actor_t *)actor)->field_2f4;
  weapon_trigger_index = -1;
  weapon_state = 0;
  if (prop_handle != -1) {
    prop = (char *)datum_get(prop_data, prop_handle);
    weapon_trigger_index = *(int *)(prop + 0x18);
    weapon_state = (*(char *)(prop + 0x60) != '\0') + 2;
  }

  FUN_00046f10(0x29, ((actor_t *)actor)->field_018, weapon_trigger_index,
               weapon_state, -1, -1, 0);

  if (*(float *)(actv_tag + 0x90) > 0.0f) {
    FUN_00021010(actor_handle, (int)(*(float *)(actv_tag + 0x90) * 30.0f));
  }

  if (*(float *)(actv_tag + 0x8c) > 0.0f) {
    FUN_00021040(actor_handle, (int)(*(float *)(actv_tag + 0x8c) * 30.0f));
  }

  FUN_00036da0(actor_handle);

  prop_handle = ((actor_t *)actor)->field_2f4;
  if (prop_handle != -1) {
    actor_situation_try_new_target(actor_handle, prop_handle);
  }

  ((actor_t *)actor)->field_2ee = 0;
  return 1;
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
char actor_action_handle_panic_transition(int actor_handle, short param_2,
                                          char param_3, short param_4)
{
  actor_t *actor = (actor_t *)datum_get(actor_data, actor_handle);
  short panic_level;
  short shield_value;
  int iVar5;
  char bVar3;
  volatile char result;

  panic_level = actor->stimuli_panic_type;
  result = 0;
  if (param_2 <= panic_level && actor->field_160 == '\0') {
    if (actor->state_action == _actor_action_flee &&
        (shield_value = actor->field_0a8, shield_value > 0)) {
      if (panic_level < shield_value) {
        actor->field_0a8 = shield_value;
        actor->stimuli_panic_type = 0;
        return 0;
      }
      actor->field_0a8 = panic_level;
      actor->stimuli_panic_type = 0;
      return result;
    }
    if (actor->field_398 != -1) {
      iVar5 = game_time_get();
      if (iVar5 <= actor->field_398 + 7) {
        goto done;
      }
    }
    bVar3 = actor->stimuli_panic_type >= param_4;
    if (actor->stimuli_panic_prop_index == 0) {
      display_assert("actor->stimuli.panic_prop_index != 0x00000000",
                     "c:\\halo\\SOURCE\\ai\\actions.c", 0x295, 1);
      system_exit(-1);
    }
    if (param_3 != '\0' && !bVar3) {
      FUN_00046f10(0x22, actor->field_018, -1, -1, -1, -1, 0);
      actor->stimuli_panic_type = 0;
      return result;
    }
    result = FUN_0001d3c0(actor_handle, actor->stimuli_panic_type,
                          actor->stimuli_panic_prop_index, bVar3);
  }
done:
  actor->stimuli_panic_type = 0;
  return result;
}

/* actor_action_handle_combat_targeting (0x1dea0)
 * If the actor has a valid target (actor+0x1b0 != NONE) and its current action
 * index (actor+0x6e) is greater than 4, checks whether the target prop's field
 * at +0x11c is below the firing-variant definition's field at +0x16c. If so,
 * rolls a random real against the 'actr' tag probability at +0x3a8 and, on
 * success, clamps the retry field at actor+0x310 to a minimum of 4 and returns
 * 1. Returns 0 otherwise. Return value is discarded by both callers.
 * Confirmed: datum_get(actor_data, actor_handle); tag_get(0x61637472,
 * actor+0x58); actor_combat_get_firing_variant_definition(actor_handle);
 * datum_get(prop_data, actor+0x270). Assert path display_assert +
 * system_exit(-1) at 0x1de08, line 0x2c8, __FILE__
 * "c:\halo\SOURCE\ai\actions.c". */
char actor_action_handle_combat_targeting(int actor_handle)
{
  char *actor;
  int actr_tag;
  char *firing_variant;
  char *prop;
  int retry;

  actor = (char *)datum_get(actor_data, actor_handle);
  actr_tag = (int)tag_get(0x61637472, ((actor_t *)actor)->field_058);
  firing_variant = actor_combat_get_firing_variant_definition(actor_handle);
  if ((*(unsigned int *)(actor + 0x1b0) != 0xffffffff) &&
      (4 < ((actor_t *)actor)->field_06e)) {
    prop = (char *)datum_get(prop_data,
                             ((actor_t *)actor)->target_target_prop_index);
    if (((actor_t *)actor)->target_target_prop_index == -1) {
      display_assert("actor->target.target_prop_index != NONE",
                     "c:\\halo\\SOURCE\\ai\\actions.c", 0x2c8, 1);
      system_exit(-1);
    }
    if (*(float *)(prop + 0x11c) < *(float *)(firing_variant + 0x16c)) {
      if (random_math_real((unsigned int *)get_global_random_seed_address()) <
          *(float *)(actr_tag + 0x3a8)) {
        retry = ((actor_t *)actor)->field_310;
        if (((actor_t *)actor)->field_310 < 5) {
          retry = 4;
        }
        ((actor_t *)actor)->field_310 = (short)retry;
        return 1;
      }
    }
  }
  return 0;
}

/* actor_action_handle_vehicle_entry (0x1dfa0) — Periodic scan for a vehicle
 * the actor should board. Throttled to once per 0x2d ticks via the stamp at
 * actor+0x384, and skipped entirely while the actor is in action state 4 with
 * actor+0xa8 > 0, or in action state 0xb.
 *
 * Two candidate sources, tried in order:
 * 1. Allies — only when the 'actr' definition flag 0x1000 is set. Walks the
 *    clump-actor iterator (FUN_00064540/FUN_00064570) and accepts an ally
 *    record of type 2..3 with +0x12e set, +0x60 clear, a valid handle at
 *    +0x110 that passes FUN_0001cb30, resolves as an object of type mask 2,
 *    and whose object+0x2d4 equals ally+0x18. The candidate must be within
 *    100.0 squared units of actor+0x12c and closer than the best so far; the
 *    recorded distance becomes (ally+0x11c)^2 and the two radii 8.0 / 10.0.
 * 2. The AI-globals impromptu-vehicle table (*(char **)0x632574 + 0x3b8,
 *    int16 count at +0x3b6, stride 0x28) — only when actor+0x84 >= 0x3c and
 *    no ally candidate was found. Entries are filtered by object type mask 2,
 *    FUN_0001cb30, distance against the entry+0x4 radius (skipped when that
 *    radius is the FLT_MAX sentinel), an int16 bitmask at entry+0x8 tested
 *    against actor+0x3e, an int16 bitmask at entry+0xa tested against
 *    actor+0x4, and an optional array of entry+0xc identifiers at entry+0x10
 *    matched on the low 16 bits of actor+0x34 plus a 2-bit tag in bits 30..31
 *    selecting actor+0x3c (tag 1) or actor+0x3a (tag 2). Accepted entries
 *    record radii entry+0x4 + 3.0 and entry+0x4 + 6.0.
 *
 * Confirmed: returns bool in AL — the three early-outs land at 0x1e351 and
 * return DL (zeroed at 0x1dfd9), the no-candidate epilogue at 0x1e348 does
 * XOR AL,AL, and only the committed path at 0x1e341 does MOV AL,0x1.
 * Confirmed: action_vehicle_setup_impromptu (0x1bcd0) takes 5 stack args
 * (ADD ESP,0x14 at 0x1e326) and returns char (TEST AL,AL at 0x1e329); the
 * two float radii are passed as raw dword MOV/PUSH at 0x1e310-0x1e31e.
 * Confirmed: distance_squared3d operand order differs between the two loops —
 * (actor+0x12c, pos) at 0x1e0ee, (pos, actor+0x12c) at 0x1e1cf.
 * Confirmed: 132-byte action buffer at EBP-0xb0 (frame SUB ESP,0xb0, next
 * local up is the position vector at EBP-0x2c), matching the other action
 * builders in this TU.
 * Confirmed: the table base is re-read from 0x632574 at the bottom of every
 * iteration (0x1e2f0) and the index is compared as int16 (0x1e2f7). */
char actor_action_handle_vehicle_entry(int actor_handle)
{
  char *actor;
  int *actr_tag;
  int now;
  int ally;
  char *ai_globals;
  char *entry;
  void *object;
  int best_handle;
  float best_dist2;
  float radius_a;
  float radius_b;
  float dist2;
  int index;
  int identifier;
  short id_count;
  short i;
  char matched;
  char result;
  int iter[2];
  vector3_t pos;
  short action_buf[66];

  actor = (char *)datum_get(actor_data, actor_handle);
  actr_tag = (int *)tag_get(0x61637472, ((actor_t *)actor)->field_058);
  now = game_time_get();
  result = 0;
  if (((actor_t *)actor)->state_action == _actor_action_flee &&
      ((actor_t *)actor)->field_0a8 > 0) {
    return result;
  }
  if (((actor_t *)actor)->state_action == _actor_action_obey) {
    return result;
  }
  if (((actor_t *)actor)->field_384 != -1 &&
      ((actor_t *)actor)->field_384 + 0x2d >= now) {
    return result;
  }
  ((actor_t *)actor)->field_384 = now;

  best_dist2 = 3.4028235e+38f;
  radius_a = 3.4028235e+38f;
  radius_b = 3.4028235e+38f;
  best_handle = -1;

  /* Source 1: allied actors already heading for / owning a vehicle. */
  if ((*actr_tag & 0x1000) != 0) {
    FUN_00064540(iter, actor_handle);
    ally = FUN_00064570(iter);
    if (ally != 0) {
      do {
        if (*(short *)(ally + 0x24) >= 2 && *(short *)(ally + 0x24) <= 3 &&
            *(char *)(ally + 0x12e) != '\0' && *(char *)(ally + 0x60) == '\0' &&
            *(int *)(ally + 0x110) != -1 &&
            FUN_0001cb30(*(int *)(ally + 0x110), actor_handle) != '\0') {
          object =
            object_try_and_get_and_verify_type(*(int *)(ally + 0x110), 2);
          if (object != NULL &&
              *(int *)((char *)object + 0x2d4) == *(int *)(ally + 0x18)) {
            object_get_world_position(*(int *)(ally + 0x110), &pos);
            dist2 = distance_squared3d((float *)(actor + 0x12c), (float *)&pos);
            if (dist2 < 100.0f && dist2 < best_dist2) {
              best_handle = *(int *)(ally + 0x110);
              best_dist2 = *(float *)(ally + 0x11c) * *(float *)(ally + 0x11c);
              radius_a = 8.0f;
              radius_b = 10.0f;
            }
          }
        }
        ally = FUN_00064570(iter);
      } while (ally != 0);
      if (best_handle != -1) {
        goto commit;
      }
    }
  }

  /* Source 2: the AI-globals impromptu vehicle table. */
  if (((actor_t *)actor)->field_084 < 0x3c) {
    return 0;
  }
  ai_globals = *(char **)0x632574;
  index = 0;
  if (*(short *)(ai_globals + 0x3b6) <= 0) {
    return 0;
  }
  do {
    entry = ai_globals + 0x3b8 + (short)index * 0x28;
    object = object_try_and_get_and_verify_type(*(int *)entry, 2);
    if (object != NULL && FUN_0001cb30(*(int *)entry, actor_handle) != '\0') {
      object_get_world_position(*(int *)entry, &pos);
      dist2 = distance_squared3d((float *)&pos, (float *)(actor + 0x12c));
      if (dist2 < best_dist2 &&
          (*(unsigned int *)(entry + 4) == 0x7f7fffff ||
           dist2 <= *(float *)(entry + 4) * *(float *)(entry + 4)) &&
          (*(short *)(entry + 8) <= 0 ||
           (((actor_t *)actor)->field_03e != -1 &&
            ((int)*(short *)(entry + 8) &
             (1 << ((actor_t *)actor)->field_03e)) != 0)) &&
          (*(short *)(entry + 0xa) <= 0 ||
           ((int)*(short *)(entry + 0xa) &
            (1 << *(unsigned char *)(actor + 4))) != 0)) {
        id_count = *(short *)(entry + 0xc);
        matched = 1;
        if (id_count > 0) {
          matched = 0;
          for (i = 0; i < id_count; i++) {
            identifier = *(int *)(entry + 0x10 + i * 4);
            if (identifier != -1) {
              /* 0x1e289 NEG/SBB/INC materializes this compare into a byte
               * before it is branched on — keep it as an assignment. */
              matched = (char)(((((actor_t *)actor)->field_034 ^
                                 (unsigned int)identifier) &
                                0xffff) == 0);
              if (matched != 0) {
                switch ((unsigned int)identifier >> 0x1e) {
                case 1:
                  matched =
                    (char)(*(unsigned short *)(actor + 0x3c) ==
                           (unsigned short)(((unsigned int)identifier >> 0x10) &
                                            0xff));
                  break;
                case 2:
                  matched =
                    (char)(*(unsigned short *)(actor + 0x3a) ==
                           (unsigned short)(((unsigned int)identifier >> 0x10) &
                                            0xff));
                  break;
                }
                if (matched != 0) {
                  break;
                }
              }
            }
          }
        }
        if (matched != 0) {
          best_handle = *(int *)entry;
          best_dist2 = dist2;
          radius_a = *(float *)(entry + 4) + 3.0f;
          radius_b = *(float *)(entry + 4) + 6.0f;
        }
      }
    }
    ai_globals = *(char **)0x632574;
    index++;
  } while ((short)index < *(short *)(ai_globals + 0x3b6));
  if (best_handle == -1) {
    return 0;
  }

commit:
  if (action_vehicle_setup_impromptu(actor_handle, best_handle, radius_a,
                                     radius_b, action_buf) == '\0') {
    return 0;
  }
  actor_action_change(actor_handle, 9, (int)action_buf);
  return 1;
}

/* actor_action_handle_active_cover_seeking (0x1e700) — When the actor's
 * active-cover gate flag (actor+0x4c) is set, evaluate whether the actor should
 * panic and seek cover. If the actor's stress field (actor+0x1bc) is at or
 * below the 'actr' tag threshold (tag+0x2dc), call actor_action_try_to_panic;
 * for a panic result of 3 or 4 with the suppress flag (actor+0x378) clear and
 * the action counter (actor+0x6e) greater than 1, throttle on a 0x1e-tick
 * cooldown (actor+0x370). On a fresh cooldown, gate on
 * actor_action_allow_cover_seeking then try actor_action_try_to_seek_cover;
 * failing that (and only when param2 is set) try FUN_0001d3c0 with the actor's
 * cover target (actor+0x270). Returns 1 if a cover-seek action was started, 0
 * otherwise.
 *
 * The per-actor state-trace record (base *(int*)0x331f58 +
 * (handle&0xffff)*0x657c) fields +0xb8/+0xba/+0xbc/+0xc0 are debug telemetry
 * only; they do not affect control flow. Confirmed: datum_get(actor_data,
 * actor_handle); tag_get(0x61637472, actor+0x58); game_time_get();
 * actor_action_try_to_panic(actor_handle);
 * actor_action_allow_cover_seeking(actor_handle, 0);
 * actor_action_try_to_seek_cover(actor_handle, 1, 0);
 * FUN_0001d3c0(actor_handle, 4, actor+0x270, param3). FPU: FLD actor+0x1bc;
 * FCOMP tag+0x2dc; TEST AH,0x41; JP => (actor+0x1bc <= tag+0x2dc). */
char actor_action_handle_active_cover_seeking(int actor_handle, char param2,
                                              int param3)
{
  char *actor;
  char *trace;
  int actr_tag;
  int elapsed;
  short panic;
  int now;
  char cVar1;
  char result;

  result = 0;
  actor = (char *)datum_get(actor_data, actor_handle);
  trace = (char *)((actor_handle & 0xffff) * 0x657c + *(int *)0x331f58);
  actr_tag = (int)tag_get(0x61637472, ((actor_t *)actor)->field_058);
  if (((actor_t *)actor)->field_04c != '\0') {
    *(char *)(trace + 0xb8) = 1;
    *(int16_t *)(trace + 0xba) = 4;
    if (((actor_t *)actor)->field_26c == -1) {
      elapsed = 1000;
    } else {
      elapsed = game_time_get() - ((actor_t *)actor)->field_26c;
    }
    *(int16_t *)(trace + 0xbc) = (int16_t)elapsed;
    *(int *)(trace + 0xc0) = *(int *)(actor + 0x1bc);
    if (*(float *)(actor + 0x1bc) <= *(float *)(actr_tag + 0x2dc)) {
      panic = actor_action_try_to_panic(actor_handle);
      *(int16_t *)(trace + 0xba) = 0;
      if (((actor_t *)actor)->field_378 == '\0' && (panic == 4 || panic == 3)) {
        *(int16_t *)(trace + 0xba) = 1;
        if (((actor_t *)actor)->field_06e >= 2) {
          now = game_time_get();
          *(int16_t *)(trace + 0xba) = 2;
          if (((actor_t *)actor)->field_370 == -1 ||
              ((actor_t *)actor)->field_370 + 0x1e <= now) {
            *(int16_t *)(trace + 0xba) = 3;
            ((actor_t *)actor)->field_370 = now;
            cVar1 = actor_action_allow_cover_seeking(actor_handle, 0);
            if (cVar1 != '\0') {
              *(int16_t *)(trace + 0xba) = 5;
              cVar1 = actor_action_try_to_seek_cover(actor_handle, 1, 0);
              if (cVar1 != '\0') {
                *(int16_t *)(trace + 0xba) = 6;
                return 1;
              }
              if (param2 != '\0') {
                cVar1 = FUN_0001d3c0(
                  actor_handle, 4, ((actor_t *)actor)->target_target_prop_index,
                  param3);
                if (cVar1 != '\0') {
                  *(int16_t *)(trace + 0xba) = 7;
                  result = 1;
                }
              }
            }
          }
        }
      }
    }
  }
  return result;
}

/* actor_action_handle_combat_selection (0x1e8a0) — Master combat action
 * selector. Resolves the actor's 'actr'/'actv' tags, the current firing
 * variant, and the target prop (actor+0x270; distance at prop+0x11c).
 * When a target prop exists, three stages run in order:
 * 1. Berserk gate (action 10, state 1): if prop+0x74 is set, or prop+0x12f
 *    with prop+0x121 <= 1, or the 'actr' timer tag+0x328 (seconds) satisfies
 *    (short)(tag+0x328 * 30) <= actor+0xc2, berserk immediately when
 *    distance <= variant+0xa0, else berserk only after
 *    actor_action_try_to_seek_cover fails.
 * 2. Melee charge attempt: gated on ranged-weapon/target flags, a 10-tick
 *    retry window (actor+0x37c), the 'actv' range (actv+0x170 when melee is
 *    preferred, else actv+0x160), a minimum distance 0.8f + max(0,
 *    tag+0x37c) when actor+0x1cb is set, and a randomized delay window past
 *    actor+0x380; on success builds action data via FUN_00013ef0(2) and
 *    switches to action 10.
 * 3. Vehicle charge attempt: when actor+0x15e == 4 and not already charging,
 *    honors a cooldown (vehi+0x390 seconds past actor+0x388) and requires
 *    distance > variant+0x160 with prop+0x38 clear; builds action data via
 *    FUN_00013ef0(4).
 * Otherwise decides between charging (want/force flags from actor+0x375,
 * 'actr' flag 0x1000000, action-10 sub-state 2/3/4/5 flags, and for state 4
 * the vehicle ram distances vehi+0x394 / FUN_00013070 >= 0.5f) and falling
 * back to fight (FUN_00014620 + actor_action_change(3)). Ends with a
 * state-consistency assert ladder (actions.c lines 0x87b-0x892).
 * FPU compares verified against disassembly: berserk timer uses
 * tag+0x328 > 0.0f; the delay window fires when
 * delay*30 + last(actor+0x380) < game_time (TEST AH,0x41; JNP at 0x1eb50);
 * the vehicle cooldown blocks while game_time <= until. */
char actor_action_handle_combat_selection(int actor_handle)
{
  char *actor;
  char *prop;
  int actr_tag;
  int actv_tag;
  char *variant;
  int *veh_object;
  int vehi_tag;
  int now;
  int last_fire;
  int last_charge;
  float distance;
  float delay;
  float threshold;
  float min_throw;
  float charge_until;
  short state;
  char want_charge;
  char result;
  char melee_preferred;
  char force_charge;
  char action_buf[132];

  actor = (char *)datum_get(actor_data, actor_handle);
  actr_tag = (int)tag_get(0x61637472, ((actor_t *)actor)->field_058);
  actv_tag = (int)tag_get(0x61637476, ((actor_t *)actor)->field_05c);
  variant = actor_combat_get_firing_variant_definition(actor_handle);
  result = 0;
  prop = 0;
  distance = 3.4028235e+38f;
  if (((actor_t *)actor)->target_target_prop_index != -1) {
    prop = (char *)datum_get(prop_data,
                             ((actor_t *)actor)->target_target_prop_index);
    distance = *(float *)(prop + 0x11c);
    if (((actor_t *)actor)->state_action == _actor_action_charge &&
        *(short *)(actor + 0xa0) == 1) {
      if ((*(char *)(prop + 0x74) != '\0' ||
           (*(char *)(prop + 0x12f) != '\0' && *(char *)(prop + 0x121) <= 1)) ||
          (*(float *)(actr_tag + 0x328) > *(float *)0x2533c0 &&
           ((actor_t *)actor)->field_0c2 >=
             (short)(int)(*(float *)(actr_tag + 0x328) * TICKS_PER_SECOND))) {
        if (!(distance > *(float *)(variant + 0xa0)) ||
            (result = actor_action_try_to_seek_cover(actor_handle, 0, 0)) ==
              '\0')
          actor_berserk(actor_handle, 1);
      }
    }
    if ((!actor_has_ranged_weapon(actor_handle) ||
         (*(int *)(prop + 0x110) == -1 && *(char *)(prop + 0x14) == '\0')) &&
        (((actor_t *)actor)->state_action != _actor_action_charge ||
         (*(short *)(actor + 0xa0) != 2 && *(short *)(actor + 0xa0) != 3)) &&
        result == '\0' && *(char *)(actor + 6) == '\0' &&
        ((actor_t *)actor)->field_158 == -1 &&
        ((actor_t *)actor)->control_fire_state != 2) {
      /* result discarded in the original; the call performs the
       * handle-verify assert side effect */
      object_get_and_verify_type(((actor_t *)actor)->field_018, 3);
      now = game_time_get();
      melee_preferred = ((actor_t *)actor)->field_378;
      if (!actor_has_ranged_weapon(actor_handle) &&
          (*(int *)actr_tag & 0x20000) == 0)
        melee_preferred = 1;
      if (((actor_t *)actor)->field_378 != '\0')
        delay = 0.0f;
      else
        delay = *(float *)(actr_tag + 0x378);
      delay = FUN_000b5590(0x15) * delay;
      delay = FUN_000b5590(0x14) + delay;
      threshold = melee_preferred != '\0' ? *(float *)(actv_tag + 0x170) :
                                            *(float *)(actv_tag + 0x160);
      if ((((actor_t *)actor)->field_37c == -1 ||
           ((actor_t *)actor)->field_37c + 10 < now) &&
          !(distance > threshold)) {
        if (((actor_t *)actor)->field_1cb != '\0') {
          if (*(float *)0x2533c0 > *(float *)(actr_tag + 0x37c))
            min_throw = *(float *)0x2533c0;
          else
            min_throw = *(float *)(actr_tag + 0x37c);
          if (distance > *(float *)0x2533f0 + min_throw)
            goto handle_vehicle_charge;
        }
        last_fire = ((actor_t *)actor)->field_380;
        if (last_fire != -1 &&
            (float)now <= delay * TICKS_PER_SECOND + (float)last_fire)
          goto handle_vehicle_charge;
        /* result discarded in the original */
        actor_has_ranged_weapon(actor_handle);
        ((actor_t *)actor)->field_37c = now;
        if (FUN_00013ef0(actor_handle, 2, action_buf) != '\0') {
          actor_action_change(actor_handle, 10, (int)action_buf);
          result = 1;
        }
      }
    }
  handle_vehicle_charge:
    if (((actor_t *)actor)->state_action != _actor_action_charge &&
        ((actor_t *)actor)->field_1cb == '\0') {
      if (result != '\0')
        return result;
      if (((actor_t *)actor)->field_15e > 0) {
        veh_object =
          (int *)object_get_and_verify_type(((actor_t *)actor)->field_158, 2);
        vehi_tag = (int)tag_get(0x76656869, *veh_object);
        last_charge = ((actor_t *)actor)->field_388;
        if (last_charge != -1) {
          charge_until = *(float *)(vehi_tag + 0x390) * TICKS_PER_SECOND +
                         (float)last_charge;
          now = game_time_get();
          if (!((float)now > charge_until))
            goto decide_charge;
        }
        if (((actor_t *)actor)->field_15e == 4 &&
            distance > *(float *)(variant + 0x160) &&
            *(short *)(prop + 0x38) == 0 &&
            FUN_00013ef0(actor_handle, 4, action_buf) != '\0') {
          actor_action_change(actor_handle, 10, (int)action_buf);
          result = 1;
          return result;
        }
      }
    } else {
      if (result != '\0')
        return result;
    }
  }
decide_charge:
  if (*(char *)(actor + 0x375) == '\0') {
    want_charge = 0;
  } else {
    want_charge = 1;
    if (((actor_t *)actor)->field_1cb != '\0')
      want_charge = 0;
  }
  force_charge = 0;
  if (((actor_t *)actor)->field_1cb == '\0' &&
      !actor_has_ranged_weapon(actor_handle) &&
      (*(int *)actr_tag & 0x1000000) != 0)
    want_charge = 1;
  if (((actor_t *)actor)->state_action == _actor_action_charge) {
    state = *(short *)(actor + 0xa0);
    if (state == 2 || state == 3) {
      if (((actor_t *)actor)->field_0a3 == '\0' &&
          *(char *)(actor + 0xa4) == '\0' &&
          ((actor_t *)actor)->field_0c5 == '\0') {
        want_charge = 1;
        goto maybe_start_charge;
      }
      goto force_charge_transition;
    }
    if (((actor_t *)actor)->field_1cb != '\0') {
      want_charge = 0;
      goto try_fight;
    }
    if (state != 4 && state != 5)
      goto check_charge_flags;
    if (((actor_t *)actor)->field_0c5 != '\0')
      goto force_charge_transition;
    if (((actor_t *)actor)->field_15e <= 1)
      goto force_charge_transition;
    want_charge = 1;
    if (state != 4)
      goto maybe_start_charge;
    veh_object =
      (int *)object_get_and_verify_type(((actor_t *)actor)->field_158, 2);
    vehi_tag = (int)tag_get(0x76656869, *veh_object);
    if (((actor_t *)actor)->field_484 != '\0' &&
        ((actor_t *)actor)->field_46c == 5 &&
        *(int *)(actor + 0x470) ==
          ((actor_t *)actor)->target_target_prop_index) {
      want_charge = 0;
      goto try_fight;
    }
    if (distance < *(float *)(vehi_tag + 0x394)) {
      want_charge = 0;
      goto try_fight;
    }
    if (!(*(float *)(vehi_tag + 0x394) + *(float *)(vehi_tag + 0x394) >
          distance))
      goto maybe_start_charge;
    if (!(FUN_00013070((float *)(actor + 0x174), (float *)(prop + 0xe0)) <
          *(float *)0x253398))
      goto maybe_start_charge;
  abandon_charge:
    want_charge = 0;
    goto try_fight;
  force_charge_transition:
    force_charge = 1;
  }
check_charge_flags:
  if (want_charge == '\0')
    goto try_fight;
  if (force_charge == '\0') {
  maybe_start_charge:
    if (((actor_t *)actor)->state_action == _actor_action_charge)
      goto charge_started;
  }
  if (FUN_00013ef0(actor_handle, 0, action_buf) == '\0')
    goto abandon_charge;
  actor_action_change(actor_handle, 10, (int)action_buf);
  result = 1;
charge_started:
  if (want_charge != '\0') {
    if (result != '\0')
      return result;
    goto verify_action_state;
  }
  if (result != '\0')
    return result;
try_fight:
  if (((actor_t *)actor)->state_action != _actor_action_fight) {
    if (FUN_00014620(actor_handle, action_buf) == '\0') {
      display_assert("success", "c:\\halo\\SOURCE\\ai\\actions.c", 0x87b, 1);
      system_exit(-1);
    }
    actor_action_change(actor_handle, 3, (int)action_buf);
    result = 1;
    return result;
  }
verify_action_state:
  if (want_charge != '\0') {
    if (((actor_t *)actor)->state_action != _actor_action_charge) {
      display_assert("actor->state.action == _actor_action_charge",
                     "c:\\halo\\SOURCE\\ai\\actions.c", 0x886, 1);
      system_exit(-1);
    }
    state = *(short *)(actor + 0xa0);
    if (state == 2 || state == 3) {
      if (((actor_t *)actor)->field_0a3 == '\0' &&
          *(char *)(actor + 0xa4) == '\0' &&
          ((actor_t *)actor)->field_0c5 == '\0')
        return result;
      display_assert("!actor->state.action_data.charge.finished_melee_attack "
                     "&& !actor->state.action_data.charge.aborted_melee_attack"
                     " && !actor->state.action_data.charge.unable_to_advance",
                     "c:\\halo\\SOURCE\\ai\\actions.c", 0x889, 1);
      system_exit(-1);
    } else if (state == 4 || state == 5) {
      if (((actor_t *)actor)->field_0c5 == '\0')
        return result;
      display_assert("!actor->state.action_data.charge.unable_to_advance",
                     "c:\\halo\\SOURCE\\ai\\actions.c", 0x88d, 1);
      system_exit(-1);
    }
  } else {
    if (((actor_t *)actor)->state_action != _actor_action_fight) {
      display_assert("actor->state.action == _actor_action_fight",
                     "c:\\halo\\SOURCE\\ai\\actions.c", 0x892, 1);
      system_exit(-1);
    }
  }
  return result;
}

/* actor_action_handle_lost_contact (0x1ef90) — Decides what an actor does when
 * it has lost contact with its target: retry an uncover/search behaviour, pick
 * a new firing position to pursue, fall back to the encounter's default state,
 * or give up and go to action type 6 ("done"). Returns non-zero when an action
 * change was committed.
 *
 * Confirmed (disassembly 0x1ef90-0x1f6d1, delinked ref 0001ef90.obj):
 *  - _chkstk frame is 0x147f4: search_scratch[0x1408c] at EBP-0x147f4,
 *    eval_ctx[0x670] at EBP-0x768, out_record[15] at EBP-0xf8,
 *    action_buf[0x84] at EBP-0xbc, scalars in EBP-0x38..EBP-0x1.
 *  - ESI holds the actor datum for the whole body; EDI holds the ENCOUNTER
 *    datum in the first half and is reused for the prop datum / firing
 *    position later. The encounter pointer is still live at the actions.c:2629
 *    assert (only reached from paths that never touch the pursuit block).
 *  - The three assert tails are display_assert(...,1) + system_exit(-1)
 *    (CALL 0x8e2f0) — NOT halt_and_catch_fire, which the decompiler claimed.
 *  - encounter_mark_examined_pursuit_position returns a bool in AL
 *    (TEST AL,AL at 0x1f4dd); kb.json previously declared it void.
 *  - encounter_modify_pursuit_desires takes 8 stack args (ADD ESP,0x20 at
 *    0x1f163), encounter_determine_pursuit_availability 12 (ADD ESP,0x30),
 *    FUN_0001cda0 11 stack args plus EAX/ECX/EDX register args (the three
 *    loads at 0x1f1e2/0x1f1e6/0x1f1dc are consumed by no PUSH). All three
 *    were declared (void) in kb.json before this lift.
 *  - The pursuit-eligibility threshold read from the 'actr' tag (+0x354 /
 *    +0x356) is a SEPARATE int16 from the firing-position index; the
 *    decompiler merged both into one variable.
 *  - The 0xd sound event is FUN_00046f10(0xd, actor+0x18,
 *    actor_target_unit_index(...), -1, -1, -1, 0): the pushes for the trailing
 *    constants precede CALL 0x3b380 (cdecl arg mis-grouping), and the two
 *    cleanups are merged into one ADD ESP,0x1c.
 *  - actor_get_firing_position_group's 3-arg cleanup is merged with
 *    FUN_00025c10's 6-arg cleanup into ADD ESP,0x24 at 0x1f446.
 *
 * Actor fields: +0x4 actor type (int16), +0x6 int8 gate, +0x18 unit handle,
 * +0x34 encounter index, +0x3a int16 passed to the desire query, +0x58 actor
 * definition tag index, +0x6a/+0x6c/+0x6e int16 state, +0x72/+0x74 int16
 * counters, +0x98/+0x9d/+0xa1 int8 flags, +0xa4/+0xa6 int16 pending firing
 * position, +0x160 int8 "already searching", +0x1d0 int32 handle, +0x1e4
 * int16 counter, +0x270 current prop handle, +0x375 int8, +0x3bc/+0x3bd int8
 * "tried uncover/search" latches, +0x3c0 prop the pursuit was started against,
 * +0x3c4 int16 examined-position count. Prop field +0x7c is the threatening
 * encounter index; encounter field +0x42 is the "search allowed" flag.
 *
 * Uncertain: the four int slots exchanged with the pursuit-desire helpers
 * (EBP-0x24/-0x28/-0x30/-0x34) are typed int because the binary only ever
 * moves them as raw dwords; some may be floats. */
char actor_action_handle_lost_contact(int actor_handle)
{
  char *actor;
  char *encounter;
  char *prop;
  void *actor_tag;
  char result;
  char can_search;
  char flag_b;
  char flag_a;
  char have_pos;
  char flag_6;
  char flag_a2;
  char flag_e;
  char flag_12;
  char found;
  char flag_2c;
  char flag_38;
  short val_24;
  short val_28;
  short val_30;
  short val_34;
  int threat;
  short firing_pos;
  short threshold;
  short delay;
  short action;
  short action_buf[66];
  int out_record[15];
  char eval_ctx[0x670];
  char search_scratch[0x1408c];

  actor = (char *)datum_get(actor_data, actor_handle);
  actor_tag = tag_get(0x61637472 /* 'actr' */, ((actor_t *)actor)->field_058);
  if (*(int *)(actor + 0x34) == -1)
    encounter = (char *)0;
  else
    encounter = (char *)datum_get(*(data_t **)0x5ab270, *(int *)(actor + 0x34));

  result = 0;
  can_search = 0;
  flag_a = 0;
  if (encounter != (char *)0 && *(char *)(encounter + 0x42) != '\0' &&
      ((actor_t *)actor)->field_06e < 3 && ((actor_t *)actor)->field_072 == 0 &&
      ((actor_t *)actor)->field_074 == 0)
    can_search = 1;
  if (((actor_t *)actor)->field_1e4 > 0 && ((actor_t *)actor)->field_06e < 3 &&
      ((actor_t *)actor)->field_074 == 0)
    flag_a = 1;
  if (((actor_t *)actor)->field_06a < 3) {
    if (actor_action_try_to_panic(actor_handle) == 0) {
      result = 1;
      return result;
    }
  }

  if (((actor_t *)actor)->field_160 == '\0' && flag_a == '\0' &&
      can_search == '\0' && ((actor_t *)actor)->field_06e > 1) {
    /* ---- pursuit block: pick / re-pick a position to search ---- */
    if (((actor_t *)actor)->target_target_prop_index == -1)
      prop = (char *)0;
    else
      prop = (char *)datum_get(prop_data,
                               ((actor_t *)actor)->target_target_prop_index);

    flag_12 = 0;
    flag_6 = 0;
    flag_a = 0;
    flag_a2 = 0;
    flag_e = 0;
    flag_b = 0;
    can_search = 0;
    if (prop == (char *)0 || *(char *)(prop + 0xbb) == '\0') {
      val_30 = FUN_0003a790(*(short *)(actor + 4));
      val_24 = FUN_0003a7b0(*(short *)(actor + 4));
      val_34 = FUN_0003a7d0(*(short *)(actor + 4));
      flag_38 = (char)FUN_0003a7f0(*(short *)(actor + 4));
      val_28 = 0;
      have_pos = 0;
      flag_2c = 0;
      if (prop != (char *)0) {
        flag_a = 1;
        flag_6 = 1;
      }
      flag_e = 1;
      flag_a2 = 1;
      flag_b = 1;
      if (*(int *)(actor + 0x34) != -1) {
        encounter_modify_pursuit_desires(
          *(int *)(actor + 0x34), *(unsigned short *)(actor + 0x3a), &flag_12,
          &val_28, &flag_38, &val_30, &val_24, &val_34);
        if (*(char *)(actor + 6) == '\0') {
          encounter_determine_pursuit_availability(
            *(int *)(actor + 0x34), actor_handle, val_28, flag_38, &flag_6,
            &flag_a, &flag_a2, &flag_e, &flag_b, &have_pos, &flag_2c,
            &can_search);
        } else {
          flag_a2 = 1;
          flag_a = 1;
          flag_6 = 1;
          flag_b = 1;
          flag_e = 1;
        }
      }
      FUN_0001cda0(have_pos, val_24, val_30, actor_handle, val_34, flag_2c, 0,
                   *(unsigned char *)(actor + 0x375), &flag_6, &flag_a,
                   &flag_a2, &flag_e, &flag_b, &can_search);
    }

    if (((actor_t *)actor)->field_3c0 !=
        ((actor_t *)actor)->target_target_prop_index) {
      ((actor_t *)actor)->field_3c4 = 0;
      ((actor_t *)actor)->field_3c0 =
        ((actor_t *)actor)->target_target_prop_index;
      ((actor_t *)actor)->field_3bc = 0;
      ((actor_t *)actor)->field_3bd = 0;
    }
    if (flag_6 != '\0' &&
        (char)FUN_0001a080(actor_handle, flag_a2, (char *)action_buf) != '\0') {
      actor_action_change(actor_handle, 5, (int)action_buf);
      result = 1;
      return result;
    }
    actor_perception_tried_to_uncover(
      actor_handle, ((actor_t *)actor)->target_target_prop_index);
    if (flag_a2 != '\0' &&
        (char)FUN_00019750(actor_handle, *(unsigned char *)(actor + 0x375),
                           (char *)action_buf) != '\0') {
      actor_action_change(actor_handle, 7, (int)action_buf);
      result = 1;
      return result;
    }
    actor_perception_tried_to_search(
      actor_handle, ((actor_t *)actor)->target_target_prop_index);
    if (((actor_t *)actor)->field_3bc != '\0' &&
        ((actor_t *)actor)->field_3bd == '\0') {
      FUN_00046f10(0xd, ((actor_t *)actor)->field_018,
                   actor_target_unit_index(actor_handle), -1, -1, -1, 0);
      ((actor_t *)actor)->field_3bd = 1;
    }
    if (flag_e == '\0')
      goto pursuit_failed;

    firing_pos = -1;
    have_pos = 0;
    ((actor_t *)actor)->field_098 = 1;
    if (*(char *)(actor + 6) != '\0') {
      if (flag_b == '\0')
        goto pursuit_failed;
      if ((char)FUN_000198d0(actor_handle, flag_12, (char *)action_buf) == '\0')
        goto pursuit_failed;
      action = 7;
      goto commit_action;
    }

    if (((actor_t *)actor)->state_action == _actor_action_uncover &&
        flag_b != '\0' && *(short *)(actor + 0xa4) == 1) {
      firing_pos = *(short *)(actor + 0xa6);
      have_pos = 1;
      if (firing_pos != -1)
        goto try_pursuit_move;
    }

    if (((actor_t *)actor)->field_1d0 == -1)
      threshold = *(short *)((char *)actor_tag + 0x356);
    else
      threshold = *(short *)((char *)actor_tag + 0x354);
    if (flag_12 == '\0' &&
        ((actor_t *)actor)->field_3c0 ==
          ((actor_t *)actor)->target_target_prop_index &&
        ((actor_t *)actor)->field_3c4 >= threshold)
      goto pursuit_failed;

    csmemset(eval_ctx, 0, 0x670);
    *(short *)(eval_ctx + 4) = 5;
    *(int *)(eval_ctx + 8) = ((actor_t *)actor)->target_target_prop_index;
    if (prop == (char *)0)
      *(int *)(eval_ctx + 0xc) = -1;
    else
      *(int *)(eval_ctx + 0xc) = *(int *)(prop + 0x7c);
    *(char *)(eval_ctx + 0x10) = flag_12;
    *(char *)(eval_ctx + 0x43) =
      (char)(((actor_t *)actor)->target_target_prop_index != -1);
    *(int *)eval_ctx = actor_get_firing_position_group(actor_handle, 5, 0);
    *(float *)(eval_ctx + 0x1c) = 20.0f;
    /* &found is a single byte at EBP-0x1d in the original frame; FUN_00025c10's
     * kb.json prototype types the slot as int * (its definition lives in
     * actor_looking.c), so the address is cast here rather than widened. */
    firing_pos = FUN_00025c10(actor_handle, eval_ctx, out_record,
                              (int *)&actor_tag, search_scratch, (int *)&found);
    if (firing_pos == -1)
      goto pursuit_failed;
    if (have_pos == '\0' && (char)FUN_0001a100(actor_handle, firing_pos,
                                               (char *)action_buf) != '\0') {
      action = 5;
      goto commit_action;
    }

  try_pursuit_move:
    if (flag_b == '\0')
      goto pursuit_failed;
    if ((char)FUN_000197d0(actor_handle, firing_pos, flag_12,
                           (char *)action_buf) == '\0')
      goto pursuit_failed;
    action = 7;

  commit_action:
    actor_action_change(actor_handle, action, (int)action_buf);
    result = 1;
    if (prop == (char *)0)
      threat = -1;
    else
      threat = *(int *)(prop + 0x7c);
    if (*(int *)(actor + 0x34) == -1)
      return result;
    if (encounter_mark_examined_pursuit_position(
          *(int *)(actor + 0x34), actor_handle, firing_pos, threat) == '\0')
      return result;
    if (((actor_t *)actor)->field_3c4 == 0)
      FUN_00046f10(0x10, ((actor_t *)actor)->field_018, -1, -1, -1, -1, 0);
    ((actor_t *)actor)->field_3c4 += 1;
    return result;

    if (*(char *)(actor + 6) == '\0' && can_search != '\0' &&
        FUN_0001c0e0(actor_handle, flag_e, (int)action_buf) != '\0') {
      actor_action_change(actor_handle, 8, (int)action_buf);
      result = 1;
      return result;
    }
  } else {
    if (flag_a != '\0') {
      if (((actor_t *)actor)->state_action == _actor_action_guard &&
          ((actor_t *)actor)->field_0a1 != '\0') {
        result = 1;
        return result;
      }
      if (FUN_000159d0(actor_handle, action_buf) != '\0')
        goto change_to_done;
    }
    if (can_search != '\0') {
      if (encounter == (char *)0) {
      pursuit_failed:
        if (((actor_t *)actor)->field_3c4 > 0 &&
            ((actor_t *)actor)->field_018 != -1)
          FUN_00046f10(0x13, ((actor_t *)actor)->field_018, -1, -1, -1, -1, 0);
        display_assert("encounter", "c:\\halo\\SOURCE\\ai\\actions.c", 0xa45,
                       1);
        system_exit(-1);
      }
      result = actor_action_set_default_state(actor_handle, -1);
      if (result != '\0')
        return result;
    }
  }

  if (actor_action_try_to_panic(actor_handle) == 1)
    goto assert_handled;
  delay = 0;
  action = ((actor_t *)actor)->state_action;
  if ((action != 7 || ((actor_t *)actor)->field_09d != '\0') && action != 8)
    delay = 0x5a;
  actor_perception_abandoned_search(
    actor_handle, ((actor_t *)actor)->target_target_prop_index);
  if ((char)FUN_00015900(actor_handle, delay, (char *)action_buf) == '\0') {
    display_assert("success", "c:\\halo\\SOURCE\\ai\\actions.c", 0xa62, 1);
    system_exit(-1);
  }
change_to_done:
  actor_action_change(actor_handle, 6, (int)action_buf);
  result = 1;
  return result;

assert_handled:
  if (actor_action_try_to_panic(actor_handle) != 1) {
    display_assert("handled || (actor_action_class(actor_index) == "
                   "_action_class_passive)",
                   "c:\\halo\\SOURCE\\ai\\actions.c", 0xa67, 1);
    system_exit(-1);
  }
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
  if (((actor_t *)actor)->state_action != _actor_action_flee) {
    return 0;
  }
  if (((actor_t *)actor)->field_0ab == '\0') {
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

/* actor_action_handle_combat_status (0x1f770) — Central "should the actor
 * change action?" arbiter. Asks actor_action_try_to_panic (0x1d6d0) for a
 * panic/urgency code in [0,4] and dispatches through a 5-entry jump table
 * (table at 0x1f900); anything outside that range trips the actions.c:2841
 * assert.
 *
 * param2/param3 are read as bytes only (byte [EBP+0xc] / byte [EBP+0x10]).
 * param3 both forces the internal gate on and, on the no-transition tail,
 * forces a final actor_action_handle_lost_contact retry.
 *
 * Actor fields consulted: 0x6c (current action type, int16), 0x6e (threat /
 * combat level, int16), 0xa4 (action timer, int16), 0x1c8 (int8 flag),
 * 0x1e4 (int16 counter), 0x270 (current prop handle), 0x3c0 (prop handle the
 * action was started against). Prop fields 0xb9 / 0xba are int8 flags.
 *
 * Returns the sub-handler's result, or 0 when no transition happened. */
char actor_action_handle_combat_status(int actor_handle, int param2, int param3)
{
  char *actor;
  char *prop;
  short panic_code;
  short level;
  short action_type;
  int prop_handle;
  char result;
  char gate;

  actor = (char *)datum_get(actor_data, actor_handle);
  panic_code = actor_action_try_to_panic(actor_handle);
  result = 0;
  gate = ((char)param3 != 0) ? (char)1 : (char)param2;

  switch (panic_code) {
  case 0:
    goto no_transition;

  case 1:
  case 2:
    if (gate == 0)
      goto no_transition;
    level = ((actor_t *)actor)->field_06e;
    if (level >= 5) {
      result = actor_action_handle_combat_selection(actor_handle);
      goto check_result;
    } else {
      if (level < 2 &&
          ((actor_t *)actor)->state_action != _actor_action_alert) {
        if (level != 0)
          goto no_transition;
        if (((actor_t *)actor)->field_1c8 == 0 &&
            ((actor_t *)actor)->field_1e4 <= 0)
          goto no_transition;
      }
      goto lost_contact;
    }

  case 4:
    if (((actor_t *)actor)->field_06e < 4)
      goto lost_contact;
    result = actor_action_handle_combat_selection(actor_handle);
    goto check_result;

  case 3:
    if (gate != 0 && ((actor_t *)actor)->field_06e >= 4) {
      result = actor_action_handle_combat_selection(actor_handle);
      goto check_result;
    }
    if (((actor_t *)actor)->field_06e >= 2) {
      prop_handle = ((actor_t *)actor)->target_target_prop_index;
      if (prop_handle == -1)
        goto no_transition;
      prop = (char *)datum_get(prop_data, prop_handle);
      if (((actor_t *)actor)->target_target_prop_index ==
          ((actor_t *)actor)->field_3c0) {
        if (*(char *)(prop + 0xb9) != 0 ||
            (((actor_t *)actor)->state_action == _actor_action_uncover &&
             *(short *)(actor + 0xa4) == 0)) {
          if (*(char *)(prop + 0xba) != 0)
            goto no_transition;
          action_type = ((actor_t *)actor)->state_action;
          if (action_type == 5 && *(short *)(actor + 0xa4) == 0)
            goto no_transition;
          if (action_type != 7)
            goto lost_contact;
          if (*(short *)(actor + 0xa4) == 0)
            goto no_transition;
        }
      }
    }
    goto lost_contact;

  default:
    display_assert((char *)0, "c:\\halo\\SOURCE\\ai\\actions.c", 0xb19, 1);
    system_exit(-1);
  }

lost_contact:
  result = actor_action_handle_lost_contact(actor_handle);

check_result:
  if (result != 0)
    return result;

no_transition:
  if ((char)param3 == 0)
    return result;
  return actor_action_handle_lost_contact(actor_handle);
}

/* actor_action_handle_combat_failure (0x1f920) — Checks if the actor's current
 * action (offset 0x6c) is type 10 and handles combat failure based on the
 * actor's state (offset 0xa0). For states 2/3, if flags at 0xa3 or 0xa4 are
 * set, or 0xc5 is set, delegates to actor_action_handle_combat_selection.
 * For states 4/5, checks 0xc5 directly. Returns the result of combat
 * selection, or 0 if no transition occurred. */
char actor_action_handle_combat_failure(int actor_handle)
{
  char *actor;
  short sVar2;

  actor = (char *)datum_get(actor_data, actor_handle);
  if (((actor_t *)actor)->state_action == _actor_action_charge) {
    sVar2 = *(short *)(actor + 0xa0);
    if ((sVar2 == 2) || (sVar2 == 3)) {
      if ((((actor_t *)actor)->field_0a3 != '\0') ||
          (*(char *)(actor + 0xa4) != '\0'))
        goto do_combat_selection;
    } else if ((sVar2 != 4) && (sVar2 != 5)) {
      return 0;
    }
    if (((actor_t *)actor)->field_0c5 != '\0') {
    do_combat_selection:
      return actor_action_handle_combat_selection(actor_handle);
    }
  }
  return 0;
}

/* actor_action_handle_exit_pursuit (0x1f9a0) — Handles exit from pursuit-type
 * actions (guard=5, vehicle_patrol=7, vehicle=8). For guard actions, checks
 * offset 0x9d; for vehicle actions, checks 0x9c. If the flag is set and
 * the timer at 0xa4 is zero, calls the appropriate perception notification
 * (tried_to_uncover, tried_to_search, or abandoned_search) with the actor's
 * prop handle (0x270). Then delegates to actor_action_handle_lost_contact
 * to handle the actual transition. Returns 0 if no transition occurred. */
char actor_action_handle_exit_pursuit(int actor_handle)
{
  char *actor;
  short action_type;

  actor = (char *)datum_get(actor_data, actor_handle);
  action_type = ((actor_t *)actor)->state_action;

  switch (action_type) {
  case 5:
    if (((actor_t *)actor)->field_09d == '\0')
      return 0;
    if (*(short *)(actor + 0xa4) == 0)
      actor_perception_tried_to_uncover(
        actor_handle, ((actor_t *)actor)->target_target_prop_index);
    return actor_action_handle_lost_contact(actor_handle);
  case 7:
    if (*(char *)(actor + 0x9c) == '\0')
      return 0;
    if (*(short *)(actor + 0xa4) == 0) {
      actor_perception_tried_to_search(
        actor_handle, ((actor_t *)actor)->target_target_prop_index);
      return actor_action_handle_lost_contact(actor_handle);
    }
    return actor_action_handle_lost_contact(actor_handle);
  case 8:
    if (*(char *)(actor + 0x9c) == '\0')
      return 0;
    actor_perception_abandoned_search(
      actor_handle, ((actor_t *)actor)->target_target_prop_index);
    return actor_action_handle_lost_contact(actor_handle);
  default:
    return 0;
  }
}

/* actor_action_try_to_throw_grenade (0x1fa60) — Attempts to commit the actor
 * to a grenade throw this tick. Returns 1 if the throw was committed, 0
 * otherwise.
 *
 * Resolves the actor and its unit object (actor+0x18, type 3). Bails
 * (returns 0) if the unit is busy (unit_is_busy) or the object's throw
 * cooldown timer (object+0x9c) has not expired (i.e. is > 0.0). When flag==0,
 * re-tests grenade viability via actor_action_test_grenade and clears the
 * pending flag (actor+0x6a0) if that test fails. If the pending flag is set,
 * computes the horizontal offset from self position (0x12c/0x130) to the
 * grenade target (0x6a8/0x6ac), normalizes it in place (magnitude3d, 2 floats),
 * and requires a nonzero magnitude. Then checks the target lies within the
 * throw arc: the normalized direction dotted with the actor facing
 * (0x174/0x178) must be >= *(float*)0x2533dc (cos(30 deg) = 0.866025388;
 * referenced as the exact global constant, NOT a rounded literal). On success
 * sets the throw-commit flag (actor+0x45c), clears the pending flag, and — if
 * the actor has a valid encounter handle (actor+0x34 != NONE) — stamps that
 * encounter record (encounter+0x5c) with the current game time. */
char actor_action_try_to_throw_grenade(int actor_handle, char flag)
{
  char *actor;
  char *object;
  char *encounter;
  float delta[2];

  actor = (char *)datum_get(actor_data, actor_handle);
  object = (char *)object_get_and_verify_type(((actor_t *)actor)->field_018, 3);
  if (unit_is_busy(((actor_t *)actor)->field_018) == 0) {
    if (*(float *)(object + 0x9c) <= *(float *)0x2533c0) {
      if (flag == '\0') {
        if (actor_action_test_grenade(actor_handle) == '\0') {
          ((actor_t *)actor)->field_6a0 = 0;
        }
      }
      if (((actor_t *)actor)->field_6a0 != '\0') {
        delta[0] =
          ((actor_t *)actor)->field_6a8 - ((actor_t *)actor)->field_12c;
        delta[1] =
          ((actor_t *)actor)->field_6ac - ((actor_t *)actor)->field_130;
        if (*(float *)0x2533c0 < magnitude3d(delta)) {
          if (*(float *)0x2533dc <=
              delta[0] * ((actor_t *)actor)->input_facing_vector[0] +
                delta[1] * ((actor_t *)actor)->input_facing_vector[1]) {
            ((actor_t *)actor)->field_45c = 1;
            ((actor_t *)actor)->field_6a0 = 0;
            if (*(int *)(actor + 0x34) != -1) {
              encounter =
                (char *)datum_get(*(data_t **)0x5ab270, *(int *)(actor + 0x34));
              *(int *)(encounter + 0x5c) = game_time_get();
            }
            return 1;
          }
        }
      }
    }
  }
  return 0;
}

/* actor_action_consider_grenade (0x1fb80) — Probabilistically decides whether
 * the actor should begin a grenade throw this tick. Gated by: the "already
 * considering" flag (actor+0x6a0), the global AI grenade-enable flag
 * (*(char**)0x632574 + 0x3b4), and the actv tag having both grenade type
 * fields present (tag+0x180 and tag+0x182 != -1). Enforces a cooldown: if the
 * last-throw timestamp (actor+0x6a4) is valid (!= -1) and the cooldown window
 * (tag+0x1a4 * 30.0f + stamp) has not yet elapsed relative to game_time, bail.
 * Otherwise re-stamps the cooldown, rolls a random value against
 * (team-scaled multiplier * tag throw-chance at 0x1a0), and if the roll
 * succeeds and actor_action_test_grenade passes, sets the consider flag and
 * kicks off the throw. Returns 1 while inside the consider window, else 0. */
char actor_action_consider_grenade(int actor_handle)
{
  char *actor;
  char *actv_tag;
  int current_time;
  int *seed;
  float expiry;
  float throw_chance;
  float team_mult;
  float roll;

  actor = (char *)datum_get(actor_data, actor_handle);
  actv_tag = (char *)tag_get(0x61637476, ((actor_t *)actor)->field_05c);
  if (((actor_t *)actor)->field_6a0 == '\0') {
    if (*(char *)(*(char **)0x632574 + 0x3b4) == '\0')
      return 0;
    if (*(short *)(actv_tag + 0x180) == -1)
      return 0;
    if (*(short *)(actv_tag + 0x182) == -1)
      return 0;
    current_time = game_time_get();
    if (((actor_t *)actor)->field_6a4 != -1) {
      expiry = *(float *)(actv_tag + 0x1a4) * *(float *)0x253394 +
               (float)((actor_t *)actor)->field_6a4;
      if (expiry > (float)current_time)
        return 0;
    }
    throw_chance = *(float *)(actv_tag + 0x1a0);
    team_mult = FUN_000b55b0(0x17, ((actor_t *)actor)->field_03e);
    ((actor_t *)actor)->field_6a4 = current_time;
    seed = get_global_random_seed_address();
    roll = random_math_real((unsigned int *)seed);
    if (team_mult * throw_chance <= roll ||
        actor_action_test_grenade(actor_handle) == '\0')
      return 0;
    ((actor_t *)actor)->field_6a0 = 1;
    actor_action_try_to_throw_grenade(actor_handle, 1);
  }
  return 1;
}

/* actor_action_try_to_evade (0x1fca0) - Attempt to start an evade (sidestep)
 * action away from the actor's current target/attractor.
 *
 * Pre-screen guards (all fall through to a false return):
 *   1. actor+0x158 (swarm element handle) must be NONE (-1).
 *   2. FUN_0002a360(actor_handle) must be false (some blocking condition).
 *   3. actor+0x504 (a boolean flag) must be clear.
 *   4. actor+0x270 (target prop/attractor datum handle) must be valid (!= -1).
 *   5. unit_tag+0x234 (evade-enable / max-evade scalar) must be > 0.0f.
 *
 * Reads the attractor direction (2D vector at prop+0xe0) and measures its
 * alignment with the actor's facing (actor+0x174/0x178). When actr_tag flag
 * 0x200000 is set the alignment is a 3-component dot (FUN_00013070); otherwise
 * the attractor vector is copied, normalized (magnitude3d), and dotted in 2D.
 * A zero-length attractor vector skips the alignment gate. The alignment must
 * exceed 0.4f (0x253524) to proceed.
 *
 * Builds the alignment vector (normalized copy of prop+0xe0), requests a
 * feasible evade direction from actor_move_try_evasion_direction with the
 * "random" reference mode (4). On success the returned direction index selects
 * an animation impulse (1 -> 7, 0 -> 6; anything else asserts), which is
 * validated via unit_test_animation_impulse before being committed with
 * actor_move_animation_impulse.
 *
 * Confirmed: datum_get(actor_data, actor_handle); tag_get('actr', actor+0x58);
 * object_get_and_verify_type(actor+0x18, 3); tag_get('unit', *object);
 * datum_get(prop_data 0x5ab23c, actor+0x270). Confirmed float constants
 * 0x2533c0 = 0.0f, 0x253524 = 0.4f. Confirmed FUN_0002ab40 result buffer is a
 * pathfinding-location scratch (28-byte frame reservation, callee reads +0xc).
 * Confirmed default-case assert "evade_direction == _actor_evade_left",
 * line 0xce3, + system_exit(-1). Returns bool in AL. */
char actor_action_try_to_evade(int actor_handle)
{
  actor_t *actor = (actor_t *)datum_get(actor_data, actor_handle);
  int *actr_tag;
  char *unit_tag;
  char *prop;
  float *attractor_vec;
  float scratch[2];
  float alignment_vec[2];
  float dot;
  int evade_dir_ref;
  char out_flag;
  int impulse;
  char path_result[0x1c];

  if (actor->field_158 != -1) return 0;
  if (FUN_0002a360(actor_handle) != 0) return 0;
  if (actor->field_504 != '\0') return 0;
  if (actor->target_target_prop_index == -1) return 0;

  actr_tag = (int *)tag_get(0x61637472, actor->field_058);
  unit_tag = (char *)tag_get(0x756e6974, *(int *)object_get_and_verify_type(actor->field_018, 3));
  prop = (char *)datum_get(*(data_t **)0x5ab23c, actor->target_target_prop_index);
  if (!(*(float *)(unit_tag + 0x234) > *(float *)0x2533c0)) return 0;

  attractor_vec = (float *)(prop + 0xe0);
  if ((*actr_tag & 0x200000) != 0) {
    dot = FUN_00013070(attractor_vec, (float *)((char *)actor + 0x174));
  } else {
    scratch[0] = attractor_vec[0];
    scratch[1] = attractor_vec[1];
    if (!(magnitude3d(scratch) > *(float *)0x2533c0)) {
      goto do_evade;
    }
    dot = scratch[1] * actor->input_facing_vector[1] +
          scratch[0] * actor->input_facing_vector[0];
  }
  if (!(dot > *(float *)0x253524)) return 0;

do_evade:
  alignment_vec[0] = attractor_vec[0];
  alignment_vec[1] = attractor_vec[1];
  evade_dir_ref = 4;
  magnitude3d(alignment_vec);
  if (actor_move_try_evasion_direction(actor_handle, alignment_vec,
                                       *(float *)(unit_tag + 0x234),
                                       (unsigned short *)&evade_dir_ref, 0.0f,
                                       &out_flag, path_result) != '\0') {
    if ((unsigned short)evade_dir_ref == 1) {
      impulse = 7;
    } else if ((unsigned short)evade_dir_ref == 0) {
      impulse = 6;
    } else {
      display_assert("evade_direction == _actor_evade_left",
                     "c:\\halo\\SOURCE\\ai\\actions.c", 0xce3, 1);
      system_exit(-1);
    }
    if (unit_test_animation_impulse(actor->field_018, impulse) != 0) {
      return (char)actor_move_animation_impulse(
        actor_handle, (int16_t)impulse, (int *)alignment_vec);
    }
  }
  return 0;
}

/* actor_action_try_to_dive (0x1fe70) - Attempt to start a dive/dodge action in
 * a lateral direction chosen relative to the actor's facing frame.
 *
 * On entry the actor's avoidance-state record (base *(int*)0x331f58, stride
 * 0x657c, indexed by the low 16 bits of the handle) is stamped at +0x184 with
 * the current game time; the final outcome is written at +0x188:
 *   1 = pre-screen / evasion-direction rejection, 2 = no feasible dive
 *   possibility, 3 = animation impulse rejected, 4 = success.
 *
 * Pre-screen (outcome 1): the actor must not be in a vehicle (actor+0x158 ==
 * NONE) and actor_move_try_evasion_direction must accept the requested
 * direction. direction_ref is an IN/OUT reference-mode word: on success the
 * callee overwrites it with the chosen reference direction (0..3), which then
 * selects how the input 2D vector is rotated into (dive_x, dive_y).
 *
 * (dive_x, dive_y) is projected onto the actor's facing axes (actor+0x174 /
 * +0x178) to build four candidate alignment scores. The static dive-possibility
 * table at 0x2542b2 is walked; each 8-byte entry carries {impulse index (-2),
 * animation_direction (0), score bias (+2, float)}. A possibility wins when its
 * biased score beats the running best (seeded to -0.5f) and
 * unit_test_animation_impulse accepts its impulse on the actor's object. If a
 * winner is found the output direction is re-derived from the winning
 * animation_direction and committed via actor_move_animation_impulse; on
 * success a 0x2c event is fired (FUN_00046f10) and the impulse result is
 * returned.
 *
 * Confirmed: datum_get(actor_data, actor_handle); avoidance record
 * *(int*)0x331f58 + (handle & 0xffff)*0x657c; game_time_get() stamp at +0x184;
 * actor_move_try_evasion_direction 7-arg call (float scalars arg3/arg5, IN/OUT
 * unsigned-short ref arg4, out flag + 28-byte path scratch); asserts at
 * actions.c 0xd24 / 0xd3e / 0xd69 each followed by system_exit(-1); float
 * constant 0xbf000000 = -0.5f. cases 2 and 3 are byte-identical in both
 * switches (separate jump-table slots). Returns bool in AL. */
char actor_action_try_to_dive(int actor_handle, short direction_ref,
                              float param_3, float *direction, float param_5)
{
  actor_t *actor = (actor_t *)datum_get(actor_data, actor_handle);
  int record = (actor_handle & 0xffff) * 0x657c + *(int *)0x331f58;
  char path_result[0x1c];
  float dive_x;
  float dive_y;
  float scores[4];
  short best_index;
  short anim_dir;
  float best_score;
  float out_vec[2];
  unsigned short *poss;
  char out_flag;
  char result;

  *(int *)(record + 0x184) = game_time_get();
  out_flag = 0;

  if (actor->field_158 != -1 ||
      actor_move_try_evasion_direction(
        actor_handle, direction, param_3, (unsigned short *)&direction_ref,
        param_5, &out_flag, path_result) == '\0') {
    *(short *)(record + 0x188) = 1;
    return '\0';
  }

  switch (direction_ref) {
  case 0:
    dive_x = *direction;
    dive_y = -direction[1];
    break;
  case 1:
    dive_y = direction[1];
    dive_x = -*direction;
    break;
  case 2:
  case 3:
    dive_x = direction[1];
    dive_y = *direction;
    break;
  default:
    display_assert(0, "c:\\halo\\SOURCE\\ai\\actions.c", 0xd24, 1);
    system_exit(-1);
  }

  best_index = -1;
  anim_dir = -1;
  best_score = -0.5f;
  scores[2] = dive_y * actor->input_facing_vector[0] +
              dive_x * actor->input_facing_vector[1];
  scores[0] = actor->input_facing_vector[0] * dive_x +
              -actor->input_facing_vector[1] * dive_y;
  scores[3] = -scores[2];
  scores[1] = -scores[0];

  poss = (unsigned short *)0x2542b2;
  do {
    if ((short)poss[0] < 0 || (short)poss[0] >= 4) {
      display_assert("(possibility->animation_direction >= 0) && "
                     "(possibility->animation_direction < 4)",
                     "c:\\halo\\SOURCE\\ai\\actions.c", 0xd3e, 1);
      system_exit(-1);
    }
    if (best_score < scores[(short)poss[0]] + *(float *)(poss + 1) &&
        unit_test_animation_impulse(actor->field_018, poss[-1]) != 0) {
      best_index = poss[-1];
      anim_dir = poss[0];
      best_score = scores[(short)poss[0]] + *(float *)(poss + 1);
    }
    poss = poss + 4;
  } while (poss[-1] != 0xffff);

  if (best_index == -1) {
    *(short *)(record + 0x188) = 2;
    return '\0';
  }

  switch (anim_dir) {
  case 0:
    out_vec[1] = -dive_y;
    out_vec[0] = dive_x;
    break;
  case 1:
    out_vec[0] = -dive_x;
    out_vec[1] = dive_y;
    break;
  case 2:
  case 3:
    out_vec[0] = dive_y;
    out_vec[1] = dive_x;
    break;
  default:
    display_assert(0, "c:\\halo\\SOURCE\\ai\\actions.c", 0xd69, 1);
    system_exit(-1);
  }

  result = (char)actor_move_animation_impulse(actor_handle, best_index,
                                              (int *)out_vec);
  if (result == '\0') {
    *(short *)(record + 0x188) = 3;
    return result;
  }
  FUN_00046f10(0x2c, ((actor_t *)actor)->field_018, -1, -1, -1, -1, 0);
  *(short *)(record + 0x188) = 4;
  return result;
}

/* actors_searching_same_position (0x20140) — Returns true when two actors are
 * searching/investigating the same position. Each actor's search record is the
 * sub-struct at actor+0xa4, but only consulted when the actor's search-type tag
 * at actor+0x6c is 5 or 7 (otherwise its record pointer stays NULL and the
 * function returns false). The record's first word (offset 0) is a "position
 * kind":
 *   kind 0 (world position): resolve each actor's referenced object
 *     (actor+0x270 is an absolute index into object pool 0x5ab23c) and compare
 *     the squared distance between the two object positions (object+0xbc)
 *     against the threshold at 0x253dd0 (~0.49 = 0.7^2); same only if closer.
 *   kind 1 (index/waypoint): equal only if the index word at record+2
 *     (actor+0xa6) matches.
 *   kind 2: always treated as the same position.
 * Mismatched kinds return false. */
bool actors_searching_same_position(int actor_handle, int param_2)
{
  int rec1;
  int rec2;
  int rec;
  short *search1;
  short *search2;
  short kind1;
  bool result;

  rec1 = (int)datum_get(actor_data, actor_handle);
  rec2 = (int)datum_get(actor_data, param_2);

  rec = (int)datum_get(actor_data, actor_handle);
  search1 = NULL;
  if (*(short *)(rec + 0x6c) == 7 || *(short *)(rec + 0x6c) == 5) {
    search1 = (short *)(rec + 0xa4);
  }

  rec = (int)datum_get(actor_data, param_2);
  search2 = NULL;
  if (*(short *)(rec + 0x6c) == 7 || *(short *)(rec + 0x6c) == 5) {
    search2 = (short *)(rec + 0xa4);
  }

  result = 0;
  if (search1 != NULL && search2 != NULL) {
    kind1 = *search1;
    if (kind1 == 0 && *search2 == 0) {
      rec1 = datum_absolute_index_to_index(*(data_t **)0x5ab23c,
                                           *(int *)(rec1 + 0x270));
      rec2 = datum_absolute_index_to_index(*(data_t **)0x5ab23c,
                                           *(int *)(rec2 + 0x270));
      if (rec1 == 0) {
        return 0;
      }
      if (rec2 == 0) {
        return 0;
      }
      if (*(float *)0x253dd0 <=
          distance_squared3d((float *)(rec1 + 0xbc), (float *)(rec2 + 0xbc))) {
        return 0;
      }
    } else {
      if (kind1 == 1 && *search2 == 1) {
        return search1[1] == search2[1];
      }
      if (kind1 != 2) {
        return 0;
      }
      if (*search2 != 2) {
        return 0;
      }
    }
    result = 1;
  }
  return result;
}

/* actor_pursuit_find_nearby_actors (0x20280) — Scans the actor's own clump and
 * (when still under-satisfied) its encounter for eligible pursuit targets,
 * counts the qualifiers, records the nearest one's index at actor+0x1d0, and
 * returns the qualifier count.
 *
 * Pass 1 walks the clump-actor iterator (FUN_00064540/FUN_00064570): each
 * record must have flag bytes at +0x60 and +0x127 clear, a valid unit index at
 * +0x1c, and (when flag != 0) a type word at +0x24 in [2,4). FUN_0001d530
 * (actor_handle in EAX) is the category-differs predicate. Each qualifier
 * increments the count; the record with the smallest key at +0x11c becomes the
 * best, its index taken from the iterator cursor iter1[0].
 *
 * threshold = (flag != 0) + 1  (1 or 2). If the count is still below threshold
 * and the actor's encounter handle (+0x34) is valid, Pass 2 walks the encounter
 * iterator (encounter_actor_iterator_new/next). For each element with a valid
 * unit index at +0x18 passing FUN_0001d530, it resolves an active-prop index
 * via prop_get_active_by_unit_index, falling back to FUN_00064b40(...,1,0); on
 * a valid index it counts the element and, when the 3D distance (record
 * +0x12c..) to the actor's own position is closer, records that index. Stops
 * once the count reaches threshold.
 *
 * Confirmed: EDI = actor_handle preserved as the @<eax> arg to both
 * FUN_0001d530 sites (MOV EAX,EDI at 0x20309 and 0x203a2); cdecl pushes are
 * (char)flag then the actor index, one ADD ESP,0x8 each. Confirmed: FLT_MAX
 * seed 0x7f7fffff at 0x202b9; both distance compares are '<' (TEST AH,5;JP).
 * Pass-2 magnitude is FSQRT over +0x12c/+0x130/+0x134 deltas. Confirmed: iter1
 * = 8-byte clump iterator (FUN_00064540 writes +4, FUN_00064570 walks +0/+4);
 * iter2 = 12-byte encounter iterator, current handle at iter2[1] (EBP-0x20).
 * Final store ((actor_t *)actor)->field_1d0 = best_index at 0x2045d. */
int actor_pursuit_find_nearby_actors(int actor_handle, char flag)
{
  int actor;
  int rec;
  int count;
  int best_index;
  float best_dist;
  int threshold;
  int threshold_raw;
  int mapped;
  float dx;
  float dy;
  float dz;
  float dist;
  int iter1[2];
  int iter2[3];

  actor = (int)datum_get(actor_data, actor_handle);
  count = 0;
  best_index = -1;
  best_dist = 3.4028235e+38f;
  FUN_00064540(iter1, actor_handle);
  threshold_raw = (flag != '\0') + 1;
  rec = FUN_00064570(iter1);
  while (rec != 0) {
    if (*(char *)(rec + 0x60) == '\0' && *(char *)(rec + 0x127) == '\0' &&
        *(int *)(rec + 0x1c) != -1 &&
        (flag == '\0' ||
         (1 < *(short *)(rec + 0x24) && *(short *)(rec + 0x24) < 4)) &&
        FUN_0001d530(actor_handle, flag, *(int *)(rec + 0x1c)) != '\0') {
      count++;
      if (*(float *)(rec + 0x11c) < best_dist) {
        best_dist = *(float *)(rec + 0x11c);
        best_index = iter1[0];
      }
    }
    rec = FUN_00064570(iter1);
  }
  threshold = (short)threshold_raw;
  if (count < threshold && *(int *)(actor + 0x34) != -1) {
    encounter_actor_iterator_new(iter2, *(int *)(actor + 0x34));
    rec = encounter_actor_iterator_next(iter2);
    while (rec != 0) {
      if (*(int *)(rec + 0x18) != -1 &&
          FUN_0001d530(actor_handle, flag, iter2[1]) != '\0') {
        mapped =
          prop_get_active_by_unit_index(actor_handle, *(int *)(rec + 0x18));
        if (mapped == -1) {
          mapped = FUN_00064b40(actor_handle, *(int *)(rec + 0x18), 1, 0);
        }
        if (mapped != -1) {
          count++;
          dx = *(float *)(rec + 0x12c) - ((actor_t *)actor)->field_12c;
          dy = *(float *)(rec + 0x130) - ((actor_t *)actor)->field_130;
          dz = *(float *)(rec + 0x134) - ((actor_t *)actor)->field_134;
          dist = sqrtf(dx * dx + dy * dy + dz * dz);
          if (dist < best_dist) {
            best_index = mapped;
            best_dist = dist;
          }
          if (threshold <= count) {
            break;
          }
        }
      }
      rec = encounter_actor_iterator_next(iter2);
    }
  }
  ((actor_t *)actor)->field_1d0 = best_index;
  return count;
}

/* actor_action_handle_berserk_transition (0x20470) — Handles berserk state
 * transition. If the actor's berserk timer (offset 0x310) has reached the
 * threshold and the actor is not already berserking (0x378), calls
 * actor_berserk. If the actor's action priority (0x6e) is >= 4 (i.e. > 3),
 * delegates to combat selection. Always clears the berserk timer. Returns
 * the result of combat selection, or 0 otherwise. */
char actor_action_handle_berserk_transition(int actor_handle, short param_2)
{
  char *actor;

  actor = (char *)datum_get(actor_data, actor_handle);
  if (((actor_t *)actor)->field_310 < param_2 ||
      ((actor_t *)actor)->field_378 != '\0') {
    ((actor_t *)actor)->field_310 = 0;
    return 0;
  }
  actor_berserk(actor_handle, 1);
  if (((actor_t *)actor)->field_06e > 3) {
    ((actor_t *)actor)->field_310 = 0;
    return actor_action_handle_combat_selection(actor_handle);
  }
  ((actor_t *)actor)->field_310 = 0;
  return 0;
}

/* actor_action_handle_combat_transition (0x204f0) — Handles transition into
 * combat. If the actor's disposition (0x6a) is below 3 and the combat
 * transition flag (0x312) is nonzero, sets disposition to 3 and attempts to
 * build a new action buffer. If successful, changes to action type 6;
 * otherwise delegates to combat selection. If disposition is already 3 and
 * action priority (0x6e) is 0, tries to panic. Returns 1 if the transition
 * was performed, 0 otherwise. */
char actor_action_handle_combat_transition(int actor_handle)
{
  char *actor;
  char cVar1;
  short action_buf[66];

  actor = (char *)datum_get(actor_data, actor_handle);
  if (((actor_t *)actor)->field_06a < 3 && ((actor_t *)actor)->field_312 != 0) {
    ((actor_t *)actor)->field_06a = 3;
    cVar1 = FUN_00016050(actor_handle, action_buf);
    if (cVar1 != '\0') {
      actor_action_change(actor_handle, 6, (int)action_buf);
    } else {
      actor_action_handle_combat_selection(actor_handle);
    }
    ((actor_t *)actor)->field_312 = 0;
    return 1;
  }
  if (((actor_t *)actor)->field_06a == 3 &&
      ((actor_t *)actor)->field_06e == 0) {
    actor_action_try_to_panic(actor_handle);
  }
  return 0;
}

/* actor_action_handle_grenade_throwing (0x205a0) — Evaluates whether the actor
 * should throw a grenade. If grenade timer (0x268) < 5, or the actor is in
 * action 4 with positive count at 0xa8, clears the grenade flag (0x6a0) and
 * returns 0. Otherwise, checks the actor's tag definition (actv) grenade type
 * at offset 0x184: type 1 requires action priority >= 5; type 2 requires the
 * prop flag at 0x14 to be set (or action 4 with nonzero 0xa8). If conditions
 * pass, calls actor_action_consider_grenade. Finally, if the grenade flag
 * (0x6a0) is set, calls actor_action_try_to_throw_grenade. */
char actor_action_handle_grenade_throwing(int actor_handle)
{
  actor_t *actor = (actor_t *)datum_get(actor_data, actor_handle);
  char *actv_tag = (char *)tag_get(0x61637476, actor->field_05c);
  char *prop;
  short mode;
  char result;

  result = 0;
  if (actor->target_target_type < 5 ||
      (actor->state_action == _actor_action_flee && actor->field_0a8 > 0)) {
    actor->field_6a0 = 0;
    return 0;
  }

  prop = (char *)datum_get(prop_data, actor->target_target_prop_index);
  mode = *(short *)(actv_tag + 0x184);

  switch (mode) {
  case 1:
    if (actor->field_06e >= 5) {
      result = actor_action_consider_grenade(actor_handle);
    }
    break;
  case 2:
    if (*(char *)(prop + 0x14) != '\0' ||
        (actor->state_action == _actor_action_flee && actor->field_0a8 != 0)) {
      result = actor_action_consider_grenade(actor_handle);
    }
    break;
  }

  if (actor->field_6a0 != '\0') {
    actor_action_try_to_throw_grenade(actor_handle, 0);
  }
  return result;
}

/* actor_action_handle_evasion (0x20670) — Decides whether the actor should
 * take an evasive action (dodge a thrown grenade, seek cover, or side-step)
 * this tick, and starts it. Returns 1 if an evasion action was started, 0
 * otherwise.
 *
 * Resolves the actor object plus its 'actv' (actor variant, actor+0x5c) and
 * 'actr' (actor, actor+0x58) tag blocks, and the current game time.
 *
 * 1. Grenade-avoidance dodge: only when the actor has a pending
 *    grenade-avoid target (short actor+0x3a8 > 0), no vehicle (actor+0x158 ==
 *    NONE), the target prop's flag (prop+0xa4) is set and its state
 *    (prop+0x38) is 0 or 1, and the 0x1e-tick dodge cooldown (actor+0x36c)
 *    has expired. Refreshes the cooldown, then gates on
 *    actor_action_allow_cover_seeking(actor,1); attempts
 *    actor_action_try_to_seek_cover, else (when the 'actr' flag 0x400000 is
 *    set) FUN_0001d3c0 toward the grenade prop.
 * 2. Selects an evasion radius from the 'actr' tag: tag+0x314 when actor+0x374
 *    is set and actor+0x378 is clear, otherwise tag+0x310. When actor+0x1ca is
 *    set and tag+0x318 exceeds *(float*)0x2533c0, the radius is clamped down to
 *    *(float*)0x253f38. If actor+0x354 <= radius the actor is outside evasion
 *    range and nothing is done.
 * 3. When actor+0x504 is clear, one RNG draw is consumed (result discarded) to
 *    keep the random stream aligned.
 * 4. If the 'actv' grenade type (tag+0x184) is 2 and
 *    actor_action_consider_grenade succeeds, clears actor+0x354 and marks the
 *    action as taken.
 * 5. Otherwise (action not yet taken): a retreat gate (b_retreat, from
 *    actor+0x358 / 'actr' flag 0x20 / the target prop's +0x122,+0x121 fields)
 *    may start a seek-cover + fire-retreat-event sequence (FUN_00046f10 event
 *    0x18); or, when the side-step gate (b_sidestep, cleared when actor+0x6c
 *    == 10 with actor+0xa0 in {2,3}) is open and actor+0x368 == 0, calls
 *    actor_action_try_to_evade and stores a timer at actor+0x368.
 *
 * Verified against delinked reference (delinked/actions.obj). Both datum_get
 * lookups (actor+0x3ac and actor+0x270) use prop_data. FUN_001d9068 at the tail
 * is the _ftol2 intrinsic, written here as (short)(int)(...). */
char actor_action_handle_evasion(int actor_handle)
{
  char *actor;
  char *actv_tag;
  char *actr_tag;
  char *prop;
  char *other_prop;
  int now;
  char status;
  char b_retreat;
  char b_sidestep;
  float radius;

  actor = (char *)datum_get(actor_data, actor_handle);
  actv_tag = (char *)tag_get(0x61637476, ((actor_t *)actor)->field_05c);
  actr_tag = (char *)tag_get(0x61637472, ((actor_t *)actor)->field_058);
  now = game_time_get();
  status = 0;

  if (((actor_t *)actor)->field_3a8 > 0 &&
      ((actor_t *)actor)->field_158 == -1) {
    prop = (char *)datum_get(prop_data, ((actor_t *)actor)->field_3ac);
    if (*(char *)(prop + 0xa4) != '\0' &&
        (*(short *)(prop + 0x38) == 0 || *(short *)(prop + 0x38) == 1) &&
        (((actor_t *)actor)->field_36c == -1 ||
         ((actor_t *)actor)->field_36c + 0x1e <= now)) {
      ((actor_t *)actor)->field_36c = now;
      if (actor_action_allow_cover_seeking(actor_handle, 1)) {
        if (actor_action_try_to_seek_cover(actor_handle, 0, 1)) {
          return 1;
        }
        if ((*(unsigned int *)actr_tag & 0x400000) &&
            FUN_0001d3c0(actor_handle, 5, ((actor_t *)actor)->field_3ac, 0)) {
          return 1;
        }
      }
    }
  }

  if (((actor_t *)actor)->field_374 != '\0' &&
      ((actor_t *)actor)->field_378 == '\0') {
    radius = *(float *)(actr_tag + 0x314);
  } else {
    radius = *(float *)(actr_tag + 0x310);
  }
  if (((actor_t *)actor)->field_1ca != '\0') {
    if (*(float *)(actr_tag + 0x318) > *(float *)0x2533c0 &&
        radius > *(float *)0x253f38) {
      radius = *(float *)0x253f38;
    }
  }
  if (((actor_t *)actor)->field_354 <= radius) {
    return 0;
  }

  if (((actor_t *)actor)->field_504 == '\0') {
    random_math_real((unsigned int *)get_global_random_seed_address());
  }

  if (*(short *)(actv_tag + 0x184) == 2 &&
      actor_action_consider_grenade(actor_handle)) {
    *(int *)(actor + 0x354) = 0;
    status = 1;
  }

  b_retreat = 1;
  b_sidestep = 1;
  if (((actor_t *)actor)->field_358 != '\0' &&
      (*(unsigned int *)actr_tag & 0x20)) {
    b_retreat = 0;
    if (((actor_t *)actor)->target_target_prop_index != -1) {
      other_prop = (char *)datum_get(
        prop_data, ((actor_t *)actor)->target_target_prop_index);
      if (*(char *)(other_prop + 0x122) <= 2 &&
          *(char *)(other_prop + 0x121) <= 1) {
        b_retreat = 1;
      }
    }
  }
  if (((actor_t *)actor)->state_action == _actor_action_charge &&
      (*(short *)(actor + 0xa0) == 2 || *(short *)(actor + 0xa0) == 3)) {
    b_sidestep = 0;
  }

  if (status == 0) {
    if (b_retreat != '\0' && (((actor_t *)actor)->field_36c == -1 ||
                              ((actor_t *)actor)->field_36c + 0x1e <= now)) {
      ((actor_t *)actor)->field_36c = now;
      if (actor_action_allow_cover_seeking(actor_handle, 0)) {
        if (random_math_real((unsigned int *)get_global_random_seed_address()) <
            *(float *)(actr_tag + 0x318)) {
          if (actor_action_try_to_seek_cover(actor_handle, 0, 1)) {
            FUN_00046f10(0x18, ((actor_t *)actor)->field_018,
                         actor_target_unit_index(actor_handle), -1, -1, -1, 0);
            *(int *)(actor + 0x354) = 0;
            return 1;
          }
        }
      }
    }
    if (b_sidestep == '\0') {
      return status;
    }
    if (((actor_t *)actor)->field_368 != 0) {
      return status;
    }
    if (actor_action_try_to_evade(actor_handle)) {
      *(int *)(actor + 0x354) = 0;
      ((actor_t *)actor)->field_368 =
        (short)(int)(*(float *)(actr_tag + 0x31c) * *(float *)0x253394);
      ((actor_t *)actor)->field_3bb = 1;
      return 1;
    }
    return status;
  }
  return status;
}

/* FUN_00021080 (0x21080) — Returns non-zero if the actor's fire_state
 * enum (actor+0x5f2) equals 4. Paired with FUN_00021040 (actor_combat.c),
 * which sets fire_state to 4. */
char FUN_00021080(int actor_handle)
{
  char *actor;

  actor = (char *)datum_get(actor_data, actor_handle);
  return ((actor_t *)actor)->control_fire_state == 4;
}

/* FUN_000210b0 (0x210b0) — Primary-look eligibility predicate. Resolves the
 * actor via datum_get(actor_data, actor_handle) and returns true only when the
 * signed 16-bit control.current_fire_target_type at actor+0x60c is positive
 * (> 0; signed CMP/JLE in the original) AND the fire_state enum at actor+0x5f2
 * (same field read by sibling FUN_00021080) equals 2. Positive here means "a
 * fire target is set" — the named values are _actor_fire_target_prop (1) and
 * _actor_fire_target_manual_point (2); 0 is never named by an assert string.
 * When no fire target is set it returns false without inspecting fire_state.
 * Called by actor_looking to gate primary look-mode selection. */
bool FUN_000210b0(int actor_handle)
{
  char *actor;
  bool result;

  actor = (char *)datum_get(actor_data, actor_handle);
  result = 0;
  if (((actor_t *)actor)->control_current_fire_target_type > 0) {
    result = ((actor_t *)actor)->control_fire_state == 2;
  }
  return result;
}

char *FUN_000210f0(int actor_handle)
{
  int weapon_handle = actor_attacking_target(actor_handle);
  if (weapon_handle != -1) {
    int *obj = (int *)object_get_and_verify_type(weapon_handle, 4);
    return (char *)tag_get(0x77656170, *obj);
  }
  return 0;
}
