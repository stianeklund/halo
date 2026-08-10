/* action_vehicle.c — AI actor "enter vehicle" action setup.
 *
 * Corresponds to action_vehicle.obj.
 * Assertion path: c:\halo\SOURCE\ai\action_vehicle.c
 *
 * Recovered by lifting FUN_0001b750 from cachebeta.xbe (v01.10.12.2276).
 * This function was mis-filed under real_math.obj by whole-object address-range
 * grouping; its behaviour (actor datum lookup, vehicle-seat attach-point query,
 * actor_move_to_point) and the embedded assert path place it in
 * ai/action_vehicle.c.
 */

#include "../../common.h"

/* FUN_0001b750 (0x1b750) — Build the state buffer for an actor's "enter
 * vehicle" action and start the actor moving toward the entry point.
 *
 * Zeroes the 0x4c-byte action state buffer, then (only when the actor is not
 * already assigned to a vehicle: actor+0x158 == -1 and actor+0x6 == 0) verifies
 * the target object is a vehicle whose speed (object+0x38) is at or above the
 * global threshold at 0x253398 and whose object+0xb6 bit 2 is clear. On success
 * it records the vehicle handle / seat index into the state buffer, computes
 * the seat attach transform (FUN_0001aeb0 -> FUN_0001b280), and issues an
 * actor_move_to_point toward the computed entry position.
 *
 * Returns 1 only when every check passes and actor_move_to_point succeeds;
 * otherwise 0.
 *
 * Confirmed (delinked disasm 0x1b750-0x1b897):
 *   datum pool = actor_data (*0x6325a4); actor+0x18 = unit_handle.
 *   object_get_and_verify_type(vehicle_handle, 3); object+0x38 float speed.
 *   FPU: continue iff speed >= *(float*)0x253398 (fld speed; fcomp threshold).
 *   object+0xb6 is a byte; continue iff (~(b>>2)) & 1 (bit 2 clear).
 *   state buffer: +0 = vehicle handle (dword), +4 = seat index (word), +6 = 0.
 *   FUN_0001b280: ECX = actor_handle, EAX = vehicle_handle, out at
 * buf+0x30/+0x48. Note: the second datum_get(actor_data, actor_handle) is
 * present in the original; its result is immediately overwritten by
 * object_get_and_verify_type and never used (preserved here for fidelity).
 */
char FUN_0001b750(int actor_handle, int vehicle_handle, int16_t seat_index,
                  short *state_data)
{
  char *actor;
  char *object;
  volatile long ok; /* volatile: forces the memory store of ok=0, matching
                     * original VC71 codegen (permuter, 86.7% -> 90.8%). */
  float attach[9];

  actor = (char *)datum_get(actor_data, actor_handle);
  ok = 0;
  assert_halt(state_data != 0);
  csmemset(state_data, 0, 0x4c);
  if (((actor_t *)actor)->field_158 == -1 && *(char *)(actor + 6) == 0) {
    (void)datum_get(actor_data, actor_handle);
    object = (char *)object_get_and_verify_type(vehicle_handle, 3);
    if (*(float *)(object + 0x38) >= *(float *)0x253398 &&
        (~(*(unsigned char *)(object + 0xb6) >> 2) & 1) != 0) {
      *(int *)state_data = vehicle_handle;
      *(int16_t *)((char *)state_data + 4) = seat_index;
      *((char *)state_data + 6) = 0;
      if (unit_has_animation_to_enter_seat(((actor_t *)actor)->field_018,
                                           vehicle_handle,
                                           seat_index) != '\0' &&
          FUN_0001aeb0(actor_handle, vehicle_handle, seat_index, 1, &attach[0],
                       &attach[3], &attach[6], 0, 0, 0, 0) != '\0' &&
          FUN_0001b280(actor_handle, vehicle_handle, &attach[0], &attach[3],
                       &attach[6], 0, (float *)((char *)state_data + 0x30),
                       (int *)((char *)state_data + 0x48)) != '\0' &&
          actor_move_to_point(
            actor_handle, (float *)((char *)state_data + 0x30),
            *(int *)((char *)state_data + 0x48), vehicle_handle) != '\0') {
        return 1;
      }
    }
  }
  return ok;
}

/* action_vehicle_perform (0x1b8a0) — Per-tick update of an actor's "enter
 * vehicle" action.
 *
 * Re-resolves the actor's pending vehicle handle (actor+0x9c) and drives the
 * approach: it periodically re-latches the vehicle's current position, asks
 * FUN_0001ada0 whether the approach is still viable, recomputes the seat
 * attach transform (FUN_0001aeb0), and either steers the actor toward the
 * entry point (FUN_0001b280 -> actor_move_to_point), stops
 * (FUN_0002f1a0), or boards the vehicle (unit_board_vehicle).
 *
 * Returns non-zero once the action has resolved — either "already seated /
 * abandoned" (actor+0xa5) or "give up" (actor+0xa6).
 *
 * Confirmed (delinked disasm 0x1b8a0-0x1bb9e):
 *   Frame: PUSH EBP; MOV EBP,ESP; SUB ESP,0x28 (no _chkstk). EBX = the single
 *   stack parameter [EBP+8] = actor_handle; EDI = actor base for the body.
 *   Returns via two exits: XOR EAX,EAX (0) and MOV EAX,1 — a full-EAX int,
 *   selected by `MOV AL,[EDI+0xa5]; TEST; JNZ` then the same on +0xa6.
 *   Locals: one 9-float scratch block at EBP-0x28 handed to FUN_0001aeb0 as
 *   three vec3 out-params (&[3], &[6], &[0], in that argument order — verified
 *   from the reverse PUSH sequence at 0x1ba25, NOT copied from the sibling
 *   FUN_0001b750 which uses a different order), plus three byte out-flags at
 *   EBP-3/-2/-1 (flag_7/flag_6/flag_5 below).
 *   Assert tail at 0x1b8ea is `PUSH -1; CALL 0x8e2f0` = system_exit(-1)
 *   (the decompiler wrongly showed halt_and_catch_fire); original message was
 *   "!actor->meta.swarm", file line 0xa1.
 *   FUN_0001ada0 (0x1ada0) takes actor_handle in EAX (MOV EAX,EBX immediately
 *   before the CALL) plus 5 pushed args (ADD ESP,0x14) and returns AL.
 *   FPU: both compares are FCOMP/FNSTSW/TEST AH,0x5/JP, i.e. the "<" form.
 *   0x1b9aa takes the re-latch branch when dist2 >= *(float*)0x253f74;
 *   0x1bb46 yields 1 when dist2 < *(float*)0x2533c8 (== 1.0f).
 *   Widths: actor+0xa0/0xa8/0xaa/0xc6 are 16-bit; +0x6/0x4c/0xa2/0xa4/0xa5/
 *   0xa6/0xc4/0xc5/0xc8 are bytes; +0x18/0x9c/0xac/0xe4/0x158 are dwords.
 *   The retry limit is `NEG CL; SBB ECX,ECX; AND ECX,0xffffffd3; ADD ECX,0x32`
 *   = 5 when actor+0xa2 is set, else 0x32.
 *
 * Inferred: actor+0xb0..0xb8 is a latched copy of the vehicle position at
 *   actor+0x12c..0x134 (copied as three dwords, matching the integer MOV pair
 *   per component in the original); actor+0xaa counts consecutive ticks the
 *   vehicle failed to move far enough, and gives up at 8.
 *
 * Uncertain: the three byte out-flags of FUN_0001aeb0 keep mechanical names —
 *   only their control-flow effect is proven, not their meaning. flag_6 and
 *   flag_5 are echoed to actor+0xc4/+0xc5.
 */
int action_vehicle_perform(int actor_handle)
{
  char *actor;
  void *vehicle;
  int now;
  int in_range;
  int16_t limit;
  float dist2;
  char flag_5; /* [EBP-1], FUN_0001aeb0 out-flag -> actor+0xc5 */
  char flag_6; /* [EBP-2], FUN_0001aeb0 out-flag -> actor+0xc4 */
  char flag_7; /* [EBP-3], FUN_0001aeb0 out-flag, gates the +0xc6 counter */
  float attach[9]; /* [EBP-0x28]: three vec3 out-params, [3..5]/[6..8]/[0..2] */

  actor = (char *)datum_get(actor_data, actor_handle);
  vehicle = object_try_and_get_and_verify_type(*(int *)(actor + 0x9c), 2);
  assert_halt_at("c:\\halo\\SOURCE\\ai\\action_vehicle.c", 0xa1,
                 *(char *)(actor + 6) == 0);
  if (((actor_t *)actor)->field_158 != -1) {
    *(actor + 0xa5) = 1;
    goto done;
  }
  if (((actor_t *)actor)->field_0a4 != 0) {
    goto done;
  }
  if (vehicle == 0) {
    *(int *)(actor + 0x9c) = -1;
    goto give_up;
  }
  if (FUN_0001ada0(actor_handle, *(int *)(actor + 0x9c),
                   ((actor_t *)actor)->field_0a2 == 0,
                   *(float *)(actor + 0xbc), *(float *)(actor + 0xc0), 0,
                   1) == 0) {
    goto give_up;
  }
  now = game_time_get();
  if (now >= *(int *)(actor + 0xac) + 0x96) {
    *(int *)(actor + 0xac) = now;
    dist2 =
      distance_squared3d((float *)(actor + 0xb0), (float *)(actor + 0x12c));
    if (dist2 < *(float *)0x253f74) {
      *(int16_t *)(actor + 0xaa) = (int16_t)(*(int16_t *)(actor + 0xaa) + 1);
    } else {
      int *latched = (int *)(actor + 0xb0);
      const int *current = (const int *)(actor + 0x12c);
      *(int16_t *)(actor + 0xaa) = 0;
      latched[0] = current[0];
      latched[1] = current[1];
      latched[2] = current[2];
    }
  }
  if (*(int16_t *)(actor + 0xaa) >= 8) {
    goto give_up;
  }
  if (FUN_0001aeb0(actor_handle, *(int *)(actor + 0x9c),
                   *(unsigned short *)(actor + 0xa0),
                   ((actor_t *)actor)->field_0a2 == 0, &attach[3], &attach[6],
                   &attach[0], 0, &flag_6, &flag_5, &flag_7) == 0) {
    goto give_up;
  }
  if (flag_7 != 0) {
    ((actor_t *)actor)->field_0c6 =
      (int16_t)(((actor_t *)actor)->field_0c6 + 1);
    if (((actor_t *)actor)->field_0c6 >= 0x1e) {
      flag_5 = 1;
      flag_6 = 1;
      goto board;
    }
  } else {
    ((actor_t *)actor)->field_0c6 = 0;
  }
  if (flag_6 != 0) {
    if (flag_5 != 0) {
    board:
      unit_board_vehicle(((actor_t *)actor)->field_018, *(int *)(actor + 0x9c),
                         *(unsigned short *)(actor + 0xa0));
      *(actor + 0xa4) = 1;
    } else {
      FUN_0002f1a0(actor_handle);
    }
  } else {
    if (((actor_t *)actor)->field_04c != 0) {
      if (FUN_0001b280(actor_handle, *(int *)(actor + 0x9c), &attach[3],
                       &attach[6], &attach[0], actor + 0xa3,
                       (float *)(actor + 0xcc), (int *)(actor + 0xe4)) == 0 ||
          actor_move_to_point(actor_handle, (float *)(actor + 0xcc),
                              ((actor_t *)actor)->field_0e4,
                              *(int *)(actor + 0x9c)) == 0) {
        ((actor_t *)actor)->field_0a8 =
          (int16_t)(((actor_t *)actor)->field_0a8 + 1);
        limit = (int16_t)(((actor_t *)actor)->field_0a2 != 0 ? 5 : 0x32);
        if (((actor_t *)actor)->field_0a8 > limit) {
          *(actor + 0xa6) = 1;
        }
      } else {
        ((actor_t *)actor)->field_0a8 = 0;
      }
    }
  }
  dist2 = distance_squared3d((float *)(actor + 0x12c), &attach[3]);
  in_range = (dist2 < *(float *)0x2533c8);
  *(float *)(actor + 0xd8) = attach[6];
  *(actor + 0xc8) = (char)in_range;
  ((actor_t *)actor)->field_0dc = *(int *)&attach[7];
  *(int *)(actor + 0xe0) = *(int *)&attach[8];
  *(actor + 0xc5) = flag_5;
  *(actor + 0xc4) = flag_6;
  goto done;

give_up:
  *(actor + 0xa6) = 1;
done:
  return (((actor_t *)actor)->field_0a5 != 0 ||
          ((actor_t *)actor)->field_0a6 != 0);
}
