#include "x87_math.h"

/* 0x2a3a0 — Reset actor path/movement state. Clears the path-active flag,
 * sets is_moving to 1, and zeroes the path step counter. */
void FUN_0002a3a0(int actor_handle)
{
  char *actor;

  actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
  *(char *)(actor + 0x4a8) = 0;
  *(char *)(actor + 0x484) = 1;
  *(int *)(actor + 0x4a0) = 0;
}

/* actor_path_input_new (0x2a470) — Populate nav state for actor movement.
 *
 * Gets the actor's actr tag speed value (tag+0x8c) as the base speed.
 * If the actor is in a vehicle (actor->vehicle_count at +0x15e > 0):
 *   - Gets the vehicle object via object_get_and_verify_type(actor[0x158], 2)
 *   - Gets the vehi tag via tag_get('vehi', vehicle[0])
 *   - Overrides unit_handle with the vehicle handle (actor[0x158])
 *   - If vehi_tag[0x38c] > constant at 0x2533c0, overrides local_8 with it
 * Calls actor_find_pathfinding_location(actor_handle), then fills nav_state_out
 * via path_input_new and path_input_set_start.
 *
 * Confirmed: datum_get + tag_get('actr', actor[0x58]) at 0x2a481-0x2a491.
 * Confirmed: tag[0x8c] → local_8; actor[0x18] → unit_handle default.
 * Confirmed: object_get_and_verify_type(actor[0x158], 2) → vehicle at 0x2a4b8.
 * Confirmed: tag_get('vehi', vehicle[0]) at 0x2a4c5.
 * Confirmed: unit_handle = actor[0x158] at 0x2a4ca.
 * Confirmed: FPU FCOMP [0x2533c0] with TEST AH,0x41 at 0x2a4db.
 * Confirmed: actor_find_pathfinding_location(actor_handle) at 0x2a4f2.
 * Confirmed: path_input_new(nav, local_8, actor[0x376], unit) at 0x2a509.
 * Confirmed: path_input_set_start(nav, actor+0x168, actor[0x164]) at 0x2a51d.
 */
void actor_path_input_new(int actor_handle, char *nav_state_out)
{
  char *actor;
  char *p;
  int local_8;
  int unit_handle;

  actor = (char *)datum_get(*(void **)0x6325a4, actor_handle);
  p = (char *)tag_get(0x61637472, *(int *)(actor + 0x58));
  local_8 = *(int *)(p + 0x8c);
  unit_handle = *(int *)(actor + 0x18);
  if (*(int16_t *)(actor + 0x15e) > 0) {
    p = (char *)object_get_and_verify_type(*(int *)(actor + 0x158), 2);
    p = (char *)tag_get(0x76656869, *(int *)p);
    unit_handle = *(int *)(actor + 0x158);
    if (*(float *)(p + 0x38c) > *(float *)0x2533c0) {
      local_8 = *(int *)(p + 0x38c);
    }
  }
  actor_find_pathfinding_location(actor_handle);
  path_input_new(nav_state_out, local_8, *(unsigned char *)(actor + 0x376),
                 unit_handle);
  path_input_set_start(nav_state_out, (float *)(actor + 0x168),
                       *(int *)(actor + 0x164));
}

/* arccosine (0x2a530) — Single-precision arc cosine.
 * The original loads the float argument into ST(0) and tail-jumps to the
 * MSVC CRT acos core at 0x1d94f0 (which stores ST(0) as a double, calls the
 * x87 acos helper at 0x1dee48, then 0x1d950d, and returns the result in
 * ST(0)). Faithful equivalent is acosf(x). Confirmed by caller 0x2daa0
 * (0x2e1e1-0x2e21b): result 0 when arg >= ~1.0, PI (0x40490fdb) when
 * arg <= ~-1.0, CALL 0x1d94f0 otherwise — the defining signature of acos. */
float arccosine(float x)
{
  return acosf(x);
}

/* midpoint3d (0x2a540) — Compute the midpoint of two 3D vectors.
 * out[i] = (a[i] + b[i]) * 0.5f for i in {0,1,2}. */
void midpoint3d(float *a, float *b, float *out)
{
  out[0] = (a[0] + b[0]) * 0.5f;
  out[1] = (a[1] + b[1]) * 0.5f;
  out[2] = (a[2] + b[2]) * 0.5f;
}

/* actor_test_destination (0x2a580) — check whether an actor has reached its
 * destination. Returns 1 if movement state is 0 or 1, or if the squared
 * distance to the destination is less than the squared tolerance. Stores the
 * result in actor[0x484]. */
char actor_test_destination(int actor_handle)
{
  char *actor;
  float tol;
  float dx, dy, dz;

  actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
  if (*(short *)(actor + 0x46c) == 0 || *(short *)(actor + 0x46c) == 1) {
    *(char *)(actor + 0x484) = 1;
  } else {
    tol = actor_destination_tolerance(actor_handle);
    dx = *(float *)(actor + 0x488) - *(float *)(actor + 0x12c);
    dy = *(float *)(actor + 0x48c) - *(float *)(actor + 0x130);
    dz = *(float *)(actor + 0x490) - *(float *)(actor + 0x134);
    if (tol * tol > dz * dz + dx * dx + dy * dy) {
      *(char *)(actor + 0x484) = 1;
    }
  }
  return *(char *)(actor + 0x484);
}

/* actor_get_stopping_distances (0x2a610) — Compute stopping distances for an
 * actor.
 *
 * Calculates two stopping-distance values based on the actor's current speed,
 * maximum speed, and deceleration sourced from the actor's biped or vehicle
 * tag.
 *
 * param_2 = out: current-speed braking distance = speed^2 / (2 * brake_decel)
 * param_3 = out: lookahead stopping distance =
 *               effective_max^2 / (2 * brake_decel)
 *             + (effective_max^2 - speed^2) / (2 * turn_decel)
 *   where effective_max = max(max_speed, current_speed)
 *
 * Confirmed: FLD float [0x255960] default max_speed; MOV [EBP-0xc]=0x3c888889
 * (turn_decel ~1/60), MOV [EBP-0x8]=0x3cda740e (brake_decel ~1/37.5).
 * Confirmed: bipd branch gated by actor[0x18]!=-1 at 0x2a6be; loads tag+0x334
 * (max), tag+0x33c (turn), tag+0x340 (brake) scaled by [0x2546a4]; conditional
 * multiplier tag+0x34c gated by actor+0x508 and FCOMP [0x2533c0] > 0.
 * Confirmed: vehicle FST/FSTP tag+0x300 stores same value to both decel slots
 * at 0x2a6b0/0x2a6b3; object_get_and_verify_type asserts, no NULL check needed.
 * Confirmed: output section NULL-checks param_2 (0x2a780) and param_3
 * (0x2a798); max(max_speed, current_speed) lives inside param_3 block only. */
void actor_get_stopping_distances(int actor_handle, float *param_2,
                                  float *param_3)
{
  char *actor;
  char *obj;
  char *tag;
  int vehicle_handle;
  int unit_handle;
  int vehicle_count;
  float max_speed;
  float turn_decel;
  float brake_decel;
  float current_speed;

  actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
  max_speed = *(float *)0x255960;
  current_speed = 0.0f;
  turn_decel = 0.016666668f;
  brake_decel = 0.026666667f;

  vehicle_handle = *(int *)(actor + 0x158);
  if (vehicle_handle != -1) {
    vehicle_count = (int)*(int16_t *)(actor + 0x15e);
    if (vehicle_count >= 2 && vehicle_count <= 3) {
      /* seated in vehicle: object_get_and_verify_type asserts, no NULL check */
      obj = (char *)object_get_and_verify_type(vehicle_handle, 2);
      tag = (char *)tag_get(0x76656869, *(int *)obj);
      /* disasm 0x2a688: FLD [ESI+0x20]*[ESI+0x2c], FLD [ESI+0x1c]*[ESI+0x28],
       * FADDP, FLD [ESI+0x18]*[ESI+0x24], FADDP */
      current_speed = *(float *)(obj + 0x20) * *(float *)(obj + 0x2c) +
                      *(float *)(obj + 0x1c) * *(float *)(obj + 0x28) +
                      *(float *)(obj + 0x18) * *(float *)(obj + 0x24);
      max_speed = *(float *)(tag + 0x2f8);
      /* FST [EBP-0xc], FSTP [EBP-0x8]: same source tag+0x300 */
      turn_decel = *(float *)(tag + 0x300);
      brake_decel = *(float *)(tag + 0x300);
    }
  } else {
    /* no vehicle: check unit handle */
    unit_handle = *(int *)(actor + 0x18);
    if (unit_handle != -1) {
      obj = (char *)object_try_and_get_and_verify_type(unit_handle, 1);
      if (obj != NULL) {
        /* tag_get called before dot product in disasm, ECX=tag at 0x2a6f1 */
        tag = (char *)tag_get(0x62697064, *(int *)obj);
        /* dot product: velocity [+0x18,+0x1c,+0x20] . facing
         * [+0x24,+0x28,+0x2c] disasm at 0x2a6eb: FLD [ESI+0x20]*[ESI+0x2c], FLD
         * [ESI+0x1c]*[ESI+0x28], FADDP, FLD [ESI+0x24]*[ESI+0x18], FADDP */
        current_speed = *(float *)(obj + 0x20) * *(float *)(obj + 0x2c) +
                        *(float *)(obj + 0x1c) * *(float *)(obj + 0x28) +
                        *(float *)(obj + 0x24) * *(float *)(obj + 0x18);
        /* bit 2 of byte at tag+0x2f4 enables custom movement params */
        if (*(unsigned char *)(tag + 0x2f4) & 4) {
          max_speed = *(float *)(tag + 0x334) * *(float *)0x2546a4;
          turn_decel = *(float *)(tag + 0x33c) * *(float *)0x2546a4;
          brake_decel = *(float *)(tag + 0x340) * *(float *)0x2546a4;
          /* conditional multiplier: only if actor[0x508] != 0 */
          if (*(unsigned char *)(actor + 0x508) != 0) {
            /* FCOMP [0x2533c0] + TEST AH,0x41: skip if multiplier <= 0 */
            if (*(float *)(tag + 0x34c) > *(float *)0x2533c0) {
              max_speed *= *(float *)(tag + 0x34c);
              turn_decel *= *(float *)(tag + 0x34c);
              brake_decel *= *(float *)(tag + 0x34c);
            }
          }
        }
      }
    }
  }

  /* param_2: braking distance from current speed; NULL-checked at 0x2a780 */
  if (param_2 != NULL) {
    *param_2 = (current_speed * current_speed) / (2.0f * brake_decel);
  }
  /* param_3: lookahead distance; NULL-checked at 0x2a798;
   * max() inside this block only (FCOMP at 0x2a79f) */
  if (param_3 != NULL) {
    if (current_speed > max_speed) {
      max_speed = current_speed;
    }
    *param_3 = (max_speed * max_speed) / (2.0f * brake_decel) +
               (max_speed * max_speed - current_speed * current_speed) /
                 (2.0f * turn_decel);
  }
}

/* 0x2a7e0 — Set actor goal destination if not already occupied.
 * Calls actor_set_dormant(actor, 0), then checks actor->goal_slot (+0x418)
 * and vehicle-in-air state. On success writes param_2 to +0x418 and
 * copies two ints from param_3 to +0x41c/+0x420. Returns 1 on success, 0 on
 * failure. */
int actor_move_animation_impulse(int actor_handle, int16_t param_2,
                                 int *param_3)
{
  char *actor;
  char *actor2;
  char result;

  result = 0;
  actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
  actor_set_dormant(actor_handle, 0);
  actor2 = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
  if (*(int16_t *)(actor2 + 0x418) == -1) {
    if (*(int *)(actor2 + 0x18) == -1 ||
        !unit_is_busy(*(int *)(actor2 + 0x18))) {
      *(int16_t *)(actor + 0x418) = param_2;
      *(int *)(actor + 0x41c) = *param_3;
      *(int *)(actor + 0x420) = param_3[1];
      result = 1;
    }
  }
  return (int)result;
}

/* 0x2a860 — Clear actor destination and trigger flee movement.
 * Returns 0 if actor has a goal slot, is in a flying vehicle, or
 * actor_action_deny_transition returns true. Otherwise zeroes the swarm flag,
 * copies 12 bytes from the global pointer at 0x31fc38, calls
 * actor_unit_control_stop_animation_impulse, and returns 1. */
int actor_move_force_stop(int actor_handle)
{
  char *actor;
  char *ptr;
  char result;

  result = 0;
  actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
  if (*(int16_t *)(actor + 0x418) == -1) {
    if (*(int *)(actor + 0x18) == -1 || !unit_is_busy(*(int *)(actor + 0x18))) {
      if (!actor_action_deny_transition(actor_handle)) {
        *(char *)(actor + 0x504) = 0;
        ptr = (char *)*(int *)0x31fc38;
        *(int *)(actor + 0x6e0) = *(int *)ptr;
        *(int *)(actor + 0x6e4) = *(int *)(ptr + 4);
        *(int *)(actor + 0x6e8) = *(int *)(ptr + 8);
        actor_unit_control_stop_animation_impulse(actor_handle);
        result = 1;
      }
    }
  }
  return (int)result;
}

/* actor_move_try_evasion_vector (0x2a8f0) — Test whether an actor can move
 * along a candidate evasion vector and, if not, whether a half-scaled
 * collision-clearance vector is usable instead.
 *
 * Args (all cdecl stack):
 *   actor_handle   = EBP+0x08 actor datum handle
 *   evasion_vector = EBP+0x0c float[2] direction (asserted non-NULL)
 *   scale          = EBP+0x10 step length applied to evasion_vector
 *   param_4        = EBP+0x14 distance/threshold control (>= 0.0)
 *   out_flag       = EBP+0x18 optional char* out (set to local_5)
 *   result         = EBP+0x1c >=28-byte collision/path result buffer
 *                    (asserted non-NULL; FUN_00063e90 fills 7 dwords)
 *
 * Returns BL (char): 1 if a usable move/evasion vector was found, else 0.
 *
 * Confirmed: datum_get(*0x6325a4, actor_handle) at 0x2a907; tag_get('actr',
 * actor[0x58]) at 0x2a917 -> actr_tag (used only for actr_tag[0x8c]).
 * Confirmed: assert "evasion_vector && result" at 0x2a946 when
 * evasion_vector==NULL || result==NULL.
 * Confirmed: gated on actor[0x99]==0 (0x2a955); when nonzero, skips to return.
 * Confirmed: target point local[0]=scale*evasion[0]+actor[0x12c],
 * local[1]=scale*evasion[1]+actor[0x130] at 0x2a963-0x2a981 (2 floats).
 * Confirmed: actor_find_pathfinding_location(actor_handle) at 0x2a984.
 * Confirmed: 9-arg FUN_00063e90(scenario_get(), (u8)actor[0x376],
 * (float*)actor[0x168], actor[0x164], &local_target, -1, actr_tag[0x8c], 0,
 * result) at 0x2a9bf — Ghidra mis-grouped the 8 pushes onto the inner
 * zero-arg scenario_get(); cleanup ADD ESP,0x24=36=9 cdecl args proves it.
 * Confirmed: when that returns 0, set found=1; fVar1=result[3]-actor[0x134];
 * keep found=1 iff (fVar1 <= scale*0.5) && (param_4 != 0.0 ||
 * scale*-0.5 <= fVar1) (FCOMP polarity at 0x2a9d3-0x2aa1d).
 * Confirmed: clearance fallback only when param_4 > 0.0 (0x2aa1f-0x2aa2d).
 *   origin   = ((actor[0x120]+actor[0x12c])*0.5, (actor[0x124]+actor[0x130])
 *               *0.5, (actor[0x128]+actor[0x134])*0.5)
 *   direction= (scale*evasion[0], scale*evasion[1], 0.0)
 * Confirmed: collision_bsp_test_vector(3, global_collision_bsp_get(), 0, 0,
 * &origin, &direction, 0x7f7fffff(FLT_MAX), col_result) at 0x2aaad.
 * Miss (0) => found=1,
 * local_5=1; if param_4 < FLT_MAX, build a half-clearance segment via
 * vector3d_scale_add(&origin, &direction, 1.0, seg_a) and
 * FUN_00012fb0(*(float**)0x31fc50, param_4, seg_b), retest; a hit there
 * clears found back to 0 (0x2aab9-0x2ab24).
 * Confirmed: out_flag NULL-checked at 0x2ab2b; *out_flag=local_5. */
char actor_move_try_evasion_vector(int actor_handle, float *evasion_vector,
                                   float scale, float param_4, char *out_flag,
                                   void *result)
{
  char *actor;
  char *actr_tag;
  float local_target[2];
  float origin[3];
  float direction[3];
  float seg_a[3];
  float seg_b[3];
  int bsp;
  float col_result[264];
  float fVar1;
  char found;
  char local_5;

  actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
  actr_tag = (char *)tag_get(0x61637472, *(int *)(actor + 0x58));
  found = 0;
  local_5 = 0;

  if (evasion_vector == (float *)0x0 || result == (void *)0x0) {
    display_assert("evasion_vector && result",
                   "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0x484, 1);
    system_exit(-1);
  }

  if (*(char *)(actor + 0x99) == '\0') {
    local_target[0] = scale * evasion_vector[0] + *(float *)(actor + 0x12c);
    local_target[1] = scale * evasion_vector[1] + *(float *)(actor + 0x130);
    actor_find_pathfinding_location(actor_handle);
    if (FUN_00063e90((int)scenario_get(), *(unsigned char *)(actor + 0x376),
                     (float *)(actor + 0x168), *(int *)(actor + 0x164),
                     local_target, 0xffffffff, *(float *)(actr_tag + 0x8c), 0,
                     (unsigned int *)result) == '\0') {
      found = 1;
      fVar1 = *(float *)((char *)result + 0xc) - *(float *)(actor + 0x134);
      if (!(fVar1 > scale * *(float *)0x253398) &&
          (param_4 != *(float *)0x2533c0 ||
           scale * *(float *)0x255964 <= fVar1)) {
        goto done;
      }
    }
    found = 0;
    if (*(float *)0x2533c0 < param_4) {
      bsp = (int)global_collision_bsp_get();
      origin[0] = (*(float *)(actor + 0x120) + *(float *)(actor + 0x12c)) *
                  *(float *)0x253398;
      origin[1] = (*(float *)(actor + 0x124) + *(float *)(actor + 0x130)) *
                  *(float *)0x253398;
      origin[2] = (*(float *)(actor + 0x128) + *(float *)(actor + 0x134)) *
                  *(float *)0x253398;
      direction[0] = scale * evasion_vector[0];
      direction[1] = scale * evasion_vector[1];
      direction[2] = 0.0f;
      if (collision_bsp_test_vector(3, bsp, 0, 0, (int)origin, (int)direction,
                                    3.4028235e+38f, col_result) == '\0') {
        found = 1;
        local_5 = 1;
        if (param_4 < *(float *)0x2548fc) {
          vector3d_scale_add(origin, direction, 1.0f, seg_a);
          FUN_00012fb0(*(float **)0x31fc50, param_4, seg_b);
          if (collision_bsp_test_vector(3, bsp, 0, 0, (int)seg_a, (int)seg_b,
                                        3.4028235e+38f, col_result) == '\0') {
            found = 0;
          }
        }
      }
    }
  }

done:
  if (out_flag != (char *)0x0) {
    *out_flag = local_5;
  }
  return found;
}

/* actor_move_try_evasion_direction (0x2ab40) — Try one perpendicular evasion
 * direction (and its mirror) derived from the alignment vector.
 *
 * Builds a 2D evade vector from alignment_vector according to the requested
 * reference mode (*evade_direction_reference), then delegates the actual
 * feasibility test to actor_move_try_evasion_vector (0x2a8f0) for each of the
 * candidate directions. On the first feasible candidate the chosen direction
 * index is written back to *evade_direction_reference and 1 is returned. If no
 * candidate is feasible, 0xffff is written back and 0 is returned.
 *
 * Confirmed: datum_get(*0x6325a4, actor_handle) at 0x2ab49-0x2ab54 (only its
 * side effect / handle validation; result unused).
 * Confirmed: assert "alignment_vector && evade_direction_reference && result"
 * at 0x2ab78-0x2ab89 when any of param_2/param_4/param_7 is NULL.
 * Confirmed: switch on (short)*evade_direction_reference (MOVSX, jump table at
 * 0x2acc4), cases 0-4, default asserts at 0x2ac3a (line 0x508).
 *   case 0: evade[0]=-v[1], evade[1]= v[0], index unchanged, count=1.
 *   case 1: evade[0]= v[1], evade[1]=-v[0], index unchanged, count=1.
 *   case 2: evade[0]= v[0], evade[1]= v[1], index unchanged, count=1.
 *   case 3: evade[0]=-v[0], evade[1]=-v[1], index unchanged, count=1.
 *   case 4: random pick of case 0 (index 0) vs case 1 (index 1), count=2;
 *           random_seed_step(seed) <= 0x8000 -> index 1 branch (0x2ac1f).
 * Confirmed: the two evade floats are a contiguous buffer at
 * [EBP-0xc]/[EBP-0x8] passed by address to actor_move_try_evasion_vector at
 * 0x2ac77. Confirmed: per-attempt both evade floats are negated (FCHS at
 * 0x2ac87/0x2ac96) and the index toggles (XOR EDI,1) at 0x2ac89. Confirmed:
 * success writes index to *evade_direction_reference (16-bit store at
 * 0x2aca5/0x2acb9); exhaustion writes 0xffff (0x2aca5). Return is char (AL). */
char actor_move_try_evasion_direction(int actor_handle, float *alignment_vector,
                                      float param_3,
                                      unsigned short *evade_direction_reference,
                                      float param_5, char *out_flag,
                                      void *result)
{
  float evade_dir[2];
  short count;
  short attempt;
  short index;

  datum_get(*(data_t **)0x6325a4, actor_handle);

  count = 1;
  if (alignment_vector == (float *)0x0 ||
      evade_direction_reference == (unsigned short *)0x0 ||
      result == (void *)0x0) {
    display_assert("alignment_vector && evade_direction_reference && result",
                   "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0x4e4, 1);
    system_exit(-1);
  }

  index = *evade_direction_reference;
  switch (index) {
  case 0:
    evade_dir[1] = alignment_vector[0];
    evade_dir[0] = -alignment_vector[1];
    break;
  case 1:
    evade_dir[0] = alignment_vector[1];
    evade_dir[1] = -alignment_vector[0];
    break;
  case 2:
    evade_dir[1] = alignment_vector[1];
    evade_dir[0] = alignment_vector[0];
    break;
  case 3:
    evade_dir[0] = -alignment_vector[0];
    evade_dir[1] = -alignment_vector[1];
    break;
  case 4:
    if (random_seed_step((unsigned int *)get_global_random_seed_address()) <
        0x8001) {
      evade_dir[0] = alignment_vector[1];
      evade_dir[1] = -alignment_vector[0];
      index = 1;
      count = 2;
    } else {
      evade_dir[1] = alignment_vector[0];
      evade_dir[0] = -alignment_vector[1];
      index = 0;
      count = 2;
    }
    break;
  default:
    display_assert((char *)0, "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0x508, 1);
    system_exit(-1);
  }

  attempt = 0;
  if (count > 0) {
    do {
      if (actor_move_try_evasion_vector(actor_handle, evade_dir, param_3,
                                        param_5, out_flag, result) != '\0') {
        *evade_direction_reference = index;
        return 1;
      }
      attempt = attempt + 1;
      evade_dir[0] = -evade_dir[0];
      index = index ^ 1;
      evade_dir[1] = -evade_dir[1];
    } while (attempt < count);
  }

  *evade_direction_reference = 0xffff;
  return 0;
}

/* actor_aim_jump (0x2ace0) — Compute and clamp the jump aim velocity vector.
 *
 * Checks actor->swarm_element (-1 at actor[0x158]) and mounted state
 * (actor[0x6] != 0). If mounted, delegates to the actor-type vtable via
 * FUN_0003a920 and clears actor[0x530]. If not mounted but jump-aim is
 * active (actor[0x530] != 0), reads the stored jump velocity:
 *   param_5[0] = actor[0x534] * actor[0x53c]
 *   param_5[1] = actor[0x538] * actor[0x53c]
 *   param_5[2] = actor[0x540]
 * Computes the magnitude. If the actor is in swarm mode (actor[0x6c]==10
 * && actor[0xa0]==3), forces clamping on (overrides param_3=0). Otherwise
 * uses param_3 to control clamping. If clamping is off and
 * param_4 < magnitude, scales the output vector to length param_4.
 * Always clears actor[0x530] and returns 1.
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x2acf0.
 * Confirmed: CMP [ESI+0x158],-1 at 0x2acf7-0x2ad03.
 * Confirmed: TEST [ESI+0x6],AL / JZ at 0x2ad09-0x2ad0e.
 * Confirmed: FUN_0003a920(actor_handle, a2, param_4, param_5) at 0x2ad1d.
 * Confirmed: TEST [ESI+0x530] at 0x2ad34-0x2ad3c.
 * Confirmed: CMP [ESI+0x6c],10 && CMP [ESI+0xa0],3 condition at
 * 0x2ad42-0x2ad55.
 * Confirmed: param_5[0]=actor[0x534]*actor[0x53c] (FSTP [ECX]) at 0x2ad7b.
 * Confirmed: param_5[1]=actor[0x538]*actor[0x53c] (FST [ECX+4]) at 0x2ad7d.
 * Confirmed: param_5[2]=actor[0x540] (FLD ST1; FSTP [ECX+8]) at
 * 0x2ad80-0x2ad82. Confirmed: sqrtf via FSQRT; FSTP [EBP-4] at 0x2ad97-0x2ad99.
 * Confirmed: FCOMP [EBP+0x14] (magnitude vs param_4) at 0x2ada7.
 * Confirmed: TEST AH,0x41; JNZ skip (param_4 >= magnitude → skip) at 0x2adac.
 * Confirmed: FUN_00012fb0(param_5, param_4/magnitude, param_5) at 0x2adbd.
 * Confirmed: actor[0x530]=0; return 1 at 0x2adc6-0x2adcf.
 */
int actor_aim_jump(int actor_handle, int a2, char param_3, float param_4,
                   float *param_5)
{
  char *actor;
  char cVar3;
  float magnitude;

  actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
  if (*(int *)(actor + 0x158) == -1) {
    if (*(char *)(actor + 0x6) != 0) {
      FUN_0003a920(actor_handle, a2, param_4, param_5);
      *(char *)(actor + 0x530) = 0;
      return 1;
    }
    if (*(char *)(actor + 0x530) != 0) {
      if (*(int16_t *)(actor + 0x6c) == 10 && *(int16_t *)(actor + 0xa0) == 3) {
        cVar3 = 1;
      } else {
        cVar3 = param_3;
      }
      param_5[0] = *(float *)(actor + 0x534) * *(float *)(actor + 0x53c);
      param_5[1] = *(float *)(actor + 0x538) * *(float *)(actor + 0x53c);
      param_5[2] = *(float *)(actor + 0x540);
      magnitude = sqrtf(param_5[0] * param_5[0] + param_5[1] * param_5[1] +
                        param_5[2] * param_5[2]);
      if (cVar3 == 0) {
        if (param_4 < magnitude) {
          FUN_00012fb0(param_5, param_4 / magnitude,
                       param_5); /* dup-args-ok: in-place scale matches
                                    confirmed call above. */
        }
      }
    }
  }
  *(char *)(actor + 0x530) = 0;
  return 1;
}

/* 0x2ade0 — actor_move_build_obstacle_list: scan nearby objects and record an
 * obstacle entry for each colliding object found within the actor's avoidance
 * radius.
 *
 * Confirmed: search radius = max(*0x2557f0, *0x255778) * actor[0x6044]
 *   (FCOMP/TEST AH,0x41 at 0x2ae0f selects the larger; FMUL at 0x2ae22).
 * Confirmed: object_find_in_radius(1, 0xc2, obj+0x48, actor+0xc, radius,
 *   handles, 0x800) at 0x2ae47; handles is a 2048-int stack buffer; the
 *   int16 return is the found count, treated unsigned (MOVZX at 0x2ae5e).
 * Confirmed: actor[0x3c] (record count) zeroed at 0x2ae52, capped at 0x400.
 * Confirmed: each object's own handle (obj+8) is skipped (0x2ae8c); -1
 *   handles are skipped (0x2ae83).
 * Confirmed: tag_get('obje', obj_tag_id) then tag_get('coll', objtag[0x7c])
 *   at 0x2ae9d/0x2aeab; collision tag block at coll[0x280] drives the per-
 *   element loop.
 * Confirmed: FUN_0001aae0 writes center[3] (&[EBP-0x20]) + obj_radius
 *   (&[EBP-0x10]); matrix_transform_point writes out[3]; node==0xffff uses
 *   world matrix, else node matrix (0x2af0b branch). scale_term =
 *   marker[0x1c] * matrix[0]. obj_radius ([EBP-0x10]) is distinct from the
 *   search radius and drives the record math at 0x2af9f/0x2afd8.
 * Confirmed: distance is 2D (X,Y only) — sqrt(dx*dx+dy*dy)+scale_term
 *   (FLD -0x30/-0x2c at 0x2af4b; out[2] written but unread).
 * Confirmed store offsets from disasm 0x2afb8-0x2affb (decompiler shifts
 *   these one slot — see store-offset table in the lift report):
 *   record[0x40]=handle, [0x44..0x4c]=center xyz, [0x4c] RMW to
 *   center[2]-(obj_radius-maxdist), [0x50]=max(2*obj_radius-2*maxdist,
 *   *0x2533c0), [0x54]=maxdist. Record stride 0x18, base actor+0x40. */
void FUN_0002ade0(int actor_handle)
{
  int handles[2048];
  float world_matrix[13];
  void *obj;
  void *obj_tag;
  void *coll_tag;
  int *coll_count;
  int object_handle;
  int16_t found;
  int16_t i;
  int j;
  float radius;
  float obj_radius;
  float maxdist;
  float scale_term;
  float center[3];
  float out[3];
  float kbase;
  void *matrix;
  unsigned short *marker;
  int16_t record_count;
  int record;

  obj = object_get_and_verify_type(*(int *)(actor_handle + 8), -1);

  /* disasm 0x2adfc: FLD [0x2557f0]; FCOMP [0x255778]; JNZ loads 778 — i.e.
   * kbase = max(7f0, 778).  Written with the > leaf taken (load 7f0) so VC71
   * lays the branch out as FCOMP+JNE rather than the <= min-idiom JP. */
  if (*(float *)0x2557f0 > *(float *)0x255778) {
    kbase = *(float *)0x2557f0;
  } else {
    kbase = *(float *)0x255778;
  }
  radius = kbase * *(float *)(actor_handle + 0x6044);

  found = object_find_in_radius(1, 0xc2, (char *)obj + 0x48,
                                (float *)(actor_handle + 0xc), radius, handles,
                                0x800);
  *(int16_t *)(actor_handle + 0x3c) = 0;

  for (i = 0; i < found; i++) {
    object_handle = handles[i];
    obj = object_get_and_verify_type(object_handle, -1);
    if (object_handle == -1 || object_handle == *(int *)(actor_handle + 8)) {
      continue;
    }

    obj_tag = tag_get(0x6f626a65, *(int *)obj); /* 'obje' */
    coll_tag =
      tag_get(0x636f6c6c, *(int *)((char *)obj_tag + 0x7c)); /* 'coll' */
    coll_count = (int *)((char *)coll_tag + 0x280);
    if (*coll_count <= 0) {
      continue;
    }

    FUN_0001aae0(object_handle, center, &obj_radius);
    object_get_world_matrix(object_handle, world_matrix);

    maxdist = 0.0f;
    /* disasm 0x2af81-0x2af8a: j incremented full-width (INC EAX), but the
     * loop-continue test compares the sign-extended low 16 bits
     * (MOVSX EAX,AX) against *coll_count. */
    j = 0;
    while ((int16_t)j < *coll_count) {
      marker = (unsigned short *)tag_block_get_element(coll_count, j, 0x20);
      /* disasm 0x2af07: CMP AX,0xffff; JZ — node!=0xffff path is fall-through.
       * Written != so VC71 lays out the node-matrix branch first (JE shape). */
      if (*marker != 0xffff) {
        matrix = object_get_node_matrix(object_handle, (int16_t)*marker);
        matrix_transform_point((float *)matrix,
                               (float *)((char *)marker + 0x10), out);
        scale_term = *(float *)((char *)marker + 0x1c) * *(float *)matrix;
      } else {
        matrix_transform_point(world_matrix, (float *)((char *)marker + 0x10),
                               out);
        scale_term = world_matrix[0] * *(float *)((char *)marker + 0x1c);
      }
      scale_term = sqrtf((out[0] - center[0]) * (out[0] - center[0]) +
                         (out[1] - center[1]) * (out[1] - center[1])) +
                   scale_term;
      /* disasm 0x2af69: FLD maxdist; FCOMP scale_term; JZ keeps maxdist.
       * Written maxdist-first with > so VC71 emits the JNE/JE shape. */
      if (maxdist > scale_term) {
        /* keep maxdist */
      } else {
        maxdist = scale_term;
      }
      j++;
    }

    record_count = *(int16_t *)(actor_handle + 0x3c);
    if (record_count < 0x400) {
      *(int16_t *)(actor_handle + 0x3c) = record_count + 1;
      record = actor_handle + record_count * 0x18;
      *(float *)(record + 0x54) = maxdist;
      *(float *)(record + 0x44) = center[0];
      *(float *)(record + 0x48) = center[1];
      *(float *)(record + 0x4c) = center[2];
      *(int *)(record + 0x40) = object_handle;
      *(float *)(record + 0x4c) =
        *(float *)(record + 0x4c) - (obj_radius - maxdist);
      scale_term = (obj_radius + obj_radius) - (maxdist + maxdist);
      /* disasm 0x2afe4: FLD [0x2533c0]; FCOMP scale_term; JNZ keeps
       * scale_term — i.e. max(const, scale_term). */
      if (*(float *)0x2533c0 > scale_term) {
        scale_term = *(float *)0x2533c0;
      }
      *(float *)(record + 0x50) = scale_term;
    }
  }
}

/* 0x2b020 — actor_move_avoidance_ray_cast: transform an avoidance ray by the
 * avoidance_data's per-instance world matrix, then cast it against the BSP and
 * a list of obstacle spheres to find the nearest collision time.
 *
 * Register args (confirmed from caller FUN_0002bd80 @ 0x2c01f-0x2c02b and
 * 0x2c166-0x2c17b):
 *   avoidance_ray  @<eax>  : packed ray data, floats [1..6] used in transform
 *   ray_origin     @<ebx>  : float[3] world-space ray origin output
 *   avoidance_data @<esi>  : per-instance struct (matrix at +0x18, bsp at +0x4,
 *                            obstacle count at +0x3c, obstacle records at
 * +0x40, scale floats at +0x6040 / +0x6044) Stack args: ray_direction :
 * float[3] world-space ray direction output collision_t            : float*
 * nearest collision time output param_3                : optional char*
 * status/counter byte (may be NULL)
 *
 * Confirmed asserts at 0x7f7/0x7f8/0x7f9 (actor_moving.c) gate the three
 * pointer-pair preconditions.  Transform reads the global world matrix pointer
 * at *0x31fc38 (translation row at +0x0/+0x4/+0x8) and the avoidance_data
 * rotation rows at +0x18.. as the 3x3.
 *
 * Confirmed: two collision_bsp_test_vector (0x149480) calls — first against the
 * transformed direction (local origin pfVar1=avoidance_data+0xc), second the
 * world-space ray_origin/ray_direction.  Ghidra truncates the 2nd call to 4
 * args; disasm 0x2b256-0x2b265 shows the full 8.
 * Confirmed: obstacle loop calls FUN_0010d4c0 (truncated to 3 args by Ghidra;
 * disasm 0x2b29d-0x2b2b3 shows 7) writing the per-obstacle collision time into
 * a scratch float (EBP-0x4) that is compared against *collision_t.  This
 * scratch is distinct from avoidance_ray[6], which Ghidra aliases as the same
 * slot. */
short FUN_0002b020(float *avoidance_ray, float *ray_origin, int avoidance_data,
                   float *ray_direction, float *collision_t, char *param_3)
{
  float *mtx;
  float obj_pos_t[3];
  float dir[3];
  float scale;
  float ray_scale;
  float dist;
  int status;
  short i;
  char hit;
  float result[262];

  status = 0;
  if (avoidance_data == 0 || avoidance_ray == (float *)0x0) {
    display_assert("avoidance_data && avoidance_ray",
                   "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0x7f7, 1);
    system_exit(-1);
  }
  if (ray_origin == (float *)0x0 || ray_direction == (float *)0x0) {
    display_assert("ray_origin && ray_direction",
                   "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0x7f8, 1);
    system_exit(-1);
  }
  if (collision_t == (float *)0x0) {
    display_assert("collision_t", "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0x7f9,
                   1);
    system_exit(-1);
  }

  *collision_t = 3.4028235e+38f;

  mtx = *(float **)0x31fc38;

  /* Transform avoidance_ray[4..6] (direction) by the rotation rows at
   * avoidance_data+0x18.. and add the world matrix translation row. */
  obj_pos_t[0] = avoidance_ray[6] * *(float *)(avoidance_data + 0x30) +
                 avoidance_ray[5] * *(float *)(avoidance_data + 0x24) +
                 avoidance_ray[4] * *(float *)(avoidance_data + 0x18) + mtx[0];
  obj_pos_t[1] = avoidance_ray[6] * *(float *)(avoidance_data + 0x34) +
                 avoidance_ray[5] * *(float *)(avoidance_data + 0x28) +
                 avoidance_ray[4] * *(float *)(avoidance_data + 0x1c) + mtx[1];
  obj_pos_t[2] = avoidance_ray[6] * *(float *)(avoidance_data + 0x38) +
                 avoidance_ray[5] * *(float *)(avoidance_data + 0x2c) +
                 avoidance_ray[4] * *(float *)(avoidance_data + 0x20) + mtx[2];

  /* ray_origin = (avoidance_ray[1..3] rotated + world translation) * scale
   *              + avoidance_data instance position (+0xc/+0x10/+0x14). */
  scale = *(float *)(avoidance_data + 0x6040);
  ray_origin[0] =
    (avoidance_ray[3] * *(float *)(avoidance_data + 0x30) +
     avoidance_ray[2] * *(float *)(avoidance_data + 0x24) +
     avoidance_ray[1] * *(float *)(avoidance_data + 0x18) + mtx[0]) *
      scale +
    *(float *)(avoidance_data + 0xc);
  ray_origin[1] =
    (avoidance_ray[3] * *(float *)(avoidance_data + 0x34) +
     avoidance_ray[2] * *(float *)(avoidance_data + 0x28) +
     avoidance_ray[1] * *(float *)(avoidance_data + 0x1c) + mtx[1]) *
      scale +
    *(float *)(avoidance_data + 0x10);
  ray_origin[2] =
    (avoidance_ray[3] * *(float *)(avoidance_data + 0x38) +
     avoidance_ray[2] * *(float *)(avoidance_data + 0x2c) +
     avoidance_ray[1] * *(float *)(avoidance_data + 0x20) + mtx[2]) *
      scale +
    *(float *)(avoidance_data + 0x14);

  /* ray_direction = transformed object position * (instance ray scale *
   * avoidance_ray[0]). */
  ray_scale = *(float *)(avoidance_data + 0x6044) * avoidance_ray[0];
  ray_direction[0] = obj_pos_t[0] * ray_scale;
  ray_direction[1] = obj_pos_t[1] * ray_scale;
  ray_direction[2] = obj_pos_t[2] * ray_scale;

  /* dir = ray_origin - instance position (+0xc/+0x10/+0x14). */
  dir[0] = ray_origin[0] - *(float *)(avoidance_data + 0xc);
  dir[1] = ray_origin[1] - *(float *)(avoidance_data + 0x10);
  dir[2] = ray_origin[2] - *(float *)(avoidance_data + 0x14);

  hit = collision_bsp_test_vector(3, *(int *)(avoidance_data + 4), 0, 0,
                                  (int)(avoidance_data + 0xc), (int)dir, 1.0f,
                                  result);
  if (hit == '\0') {
    hit = collision_bsp_test_vector(3, *(int *)(avoidance_data + 4), 0, 0,
                                    (int)ray_origin, (int)ray_direction, 1.0f,
                                    result);
    if (hit != '\0') {
      status = 2;
      *collision_t = result[0];
    }
  } else {
    status = 2;
    *collision_t = 0.0f;
  }

  if (*(short *)(avoidance_data + 0x3c) > 0) {
    i = 0;
    do {
      int rec;

      rec = avoidance_data + 0x40 + i * 0x18;
      hit = FUN_0010d4c0((float *)(rec + 4), *(float *)(rec + 0x10),
                         *(float *)(rec + 0x14), ray_origin, ray_direction,
                         &dist, dir);
      if (hit != '\0' && dist < *collision_t) {
        status = 1;
        *collision_t = dist;
      }
      i = i + 1;
    } while (i < *(short *)(avoidance_data + 0x3c));
  }

  if (param_3 != (char *)0x0) {
    if ((short)status > 0) {
      *param_3 = '\0';
      return (short)status;
    }
    if (*param_3 != -1) {
      *param_3 = *param_3 + '\x01';
    }
  }
  return (short)status;
}

/* 0x2b310 — actor_move_find_avoidance_sector: locate the angular sector of the
 * query direction within a fan of direction records and linearly interpolate a
 * fractional index plus an associated value.
 *
 * Register args (confirmed from caller FUN_0002bd80 @ 0x2c4c7-0x2c4cf and
 * 0x2c765-0x2c76d):
 *   direction @<ecx> : query direction vector (direction[0..2])
 *   count     @<ebx> : number of records (always 8 at both call sites; used as
 *                      a signed short — TEST BX,BX / CMP DX,BX / MOVSX)
 * Stack args (cdecl, ADD ESP,0x10 cleanup):
 *   records   : base of count direction records, 12-byte (float[3]) stride
 *               (= global table 0x632780)
 *   values    : parallel array of count floats, 4-byte stride
 *   out_index : fractional sector index output
 *   out_value : interpolated value output
 * Returns 1 (AL) when a bracketing sector is found, else 0.
 *
 * The function walks the fan, computing for each record the 2D cross-product
 * component (rec.y*dir.z - rec.z*dir.y) against the query direction.  When the
 * sign of consecutive cross-products differs (product <= 0) and the dot product
 * with the record is positive (forward hemisphere), the query lies between the
 * previous record (prev) and the current record (i); the fractional index and
 * value are interpolated from the two cross magnitudes.  prev starts at the
 * wrap-around predecessor count-1.
 *
 * Confirmed: cross/dot loads at 0x2b32c-0x2b338 (seed), 0x2b34c-0x2b358
 * (cross), 0x2b36c-0x2b37e (dot).  Confirmed: opposite-sign test FCOMP
 * [0x2533c0]=0.0; TEST AH,0x41; JP at 0x2b35f-0x2b36a (enters body for product
 * <= 0). Confirmed: dot test FCOMP [0x2533c0]; TEST AH,0x41; JZ at
 * 0x2b380-0x2b38b (enters success for dot > 0).  Confirmed: success-block
 * index/value interpolation FILD/FMUL/FSUBP/FDIV at 0x2b3b3-0x2b3ec. */
char FUN_0002b310(float *direction, short count, int records, float *values,
                  float *out_index, float *out_value)
{
  float prev_cross;
  float cross;
  float *rec;
  short prev;
  short i;
  short denom_idx;

  prev = count - 1;
  i = 0;
  prev_cross = *(float *)(records + prev * 0xc + 4) * direction[2] -
               *(float *)(records + prev * 0xc + 8) * direction[1];
  if (count > 0) {
    do {
      rec = (float *)(records + i * 0xc);
      cross = rec[1] * direction[2] - rec[2] * direction[1];
      if (prev_cross * cross <= 0.0f && 0.0f < direction[0] * rec[0] +
                                                 rec[1] * direction[1] +
                                                 rec[2] * direction[2]) {
        denom_idx = i;
        if (i == 0) {
          denom_idx = count;
        }
        *out_index =
          ((float)(int)prev * cross - (float)(int)denom_idx * prev_cross) /
          (cross - prev_cross);
        *out_value = (cross * values[prev] - prev_cross * values[i]) /
                     (cross - prev_cross);
        return 1;
      }
      prev = i;
      i = i + 1;
      prev_cross = cross;
    } while (i < count);
  }
  return 0;
}

/* 0x2b400 — actor_move_transform_avoidance_vector: transform a local-space
 * direction vector (in_vec) by the avoidance_data's per-instance 3x3 rotation
 * (rows at +0x18/+0x24/+0x30) and add the global world translation row read
 * from the dereferenced pointer at *0x31fc38 (translation at +0x0/+0x4/+0x8).
 *
 * cdecl, all stack args (confirmed: PUSH EBP / RET, no RET N; caller
 * FUN_0004c920 @ 0x4d39c-0x4d39d pushes EAX=[EDI+0x1a0] last).
 *   matrix   : per-instance struct, 3x3 rotation rows at +0x18..+0x38
 *   in_vec   : float[3] local-space direction
 *   out_vec  : float[3] world-space output
 *
 * Confirmed (disasm 0x2b406-0x2b41c): out_vec is seeded from the world matrix
 * translation row via the once-dereferenced global pointer at *0x31fc38, NOT
 * the raw global address.  Confirmed three interleaved read-modify-write passes
 * (one per in_vec component) with operand order in_vec[c] * mtx[col] + out[r].
 */
void actor_move_transform_avoidance_vector(int matrix, float *in_vec,
                                           float *out_vec)
{
  float *world_translation;
  float component;

  world_translation = *(float **)0x31fc38;
  out_vec[0] = world_translation[0];
  out_vec[1] = world_translation[1];
  out_vec[2] = world_translation[2];

  component = in_vec[0];
  out_vec[0] = component * *(float *)(matrix + 0x18) + out_vec[0];
  out_vec[1] = component * *(float *)(matrix + 0x1c) + out_vec[1];
  out_vec[2] = component * *(float *)(matrix + 0x20) + out_vec[2];

  component = in_vec[1];
  out_vec[0] = component * *(float *)(matrix + 0x24) + out_vec[0];
  out_vec[1] = component * *(float *)(matrix + 0x28) + out_vec[1];
  out_vec[2] = component * *(float *)(matrix + 0x2c) + out_vec[2];

  component = in_vec[2];
  out_vec[0] = component * *(float *)(matrix + 0x30) + out_vec[0];
  out_vec[1] = component * *(float *)(matrix + 0x34) + out_vec[1];
  out_vec[2] = component * *(float *)(matrix + 0x38) + out_vec[2];
}

/* 0x2b490 — actor_move_get_avoidance_vector: map an avoidance "direction index"
 * (a fractional sector index in [0,8)) into a world-space direction vector.
 *
 * The index is clamped to [0,8); a per-sector angle is linearly interpolated
 * between adjacent entries of the angle table at 0x2557f4 (the wrap entry for
 * sector 7 uses table[0]).  cos/sin of the interpolated angle form a local
 * {0, cos, sin} vector that is transformed by
 * actor_move_transform_avoidance_vector.
 *
 * cdecl, all stack args (confirmed: PUSH EBP / RET, no RET N; caller
 * FUN_0004c920 @ 0x4d26a-0x4d26b and 0x4d2d7-0x4d2d8).
 *   matrix      : per-instance struct (passed through to the transform)
 *   dir_index   : float fractional sector index in [0,8)
 *   out_vec     : float[3] world-space output
 *
 * Confirmed constants: 0.0f @0x2533c0, 1.0f @0x2533c8, 8.0f @0x253f78,
 * FLT_MAX sentinel @0x2548fc, 2*pi (_full_circle) @0x255a54.  Assert text and
 * warning text both at actor_moving.c line 0xab7.  The warning passes the
 * (possibly clamped) index as a double (FLD float; FSTP double [ESP]). */
void actor_move_get_avoidance_vector(int matrix, float dir_index,
                                     float *out_vec)
{
  const float *angles = (const float *)0x2557f4;
  short sector;
  int idx;
  float next_angle;
  float frac;
  float angle;
  float vec[3];

  /* Clamp the index into the valid [0, 8) range. */
  if (dir_index < *(const float *)0x2533c0 ||
      *(const float *)0x253f78 <= dir_index) {
    dir_index = 0.0f;
  }

  angle = 0.0f;
  sector = 0;
  do {
    if (dir_index < (float)(int)sector + *(const float *)0x2533c8) {
      idx = (int)sector;
      next_angle = *(const float *)0x2557f4;
      if (sector != 7) {
        next_angle = ((const float *)0x2557f8)[idx];
      }
      frac = dir_index - (float)idx;
      angle =
        next_angle * frac + (*(const float *)0x2533c8 - frac) * angles[idx];
      if (angle != *(const float *)0x2548fc) {
        if (angle < *(const float *)0x2533c0 ||
            *(const float *)0x255a54 <= angle) {
          display_assert("(angle >= 0.0f) && (angle < _full_circle)",
                         "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0xab7, 1);
          system_exit(-1);
        }
        goto build_vector;
      }
      break;
    }
    sector = sector + 1;
  } while (sector < 8);

  angle = 0.0f;
  error(2,
        "warning: actor_move_get_avoidance_vector couldn't find out-of-bounds "
        "direction %.4f",
        (double)dir_index);

build_vector:
  vec[0] = 0.0f;
  vec[1] = x87_fcos(angle);
  vec[2] = x87_fsin(angle);
  actor_move_transform_avoidance_vector(matrix, vec, out_vec);
}

/* 0x2b5d0 — actor_move_get_avoidance_direction: initialize trigonometric lookup
 * tables.
 *
 * Confirmed: no arguments, no calls, writes table blocks rooted at
 * 0x6327e0 and 0x6325c0 using constants/tables at 0x25577c..0x25581c.
 * Confirmed: first loop runs 9 iterations (EDX=9), stride 0x1c (7 floats).
 * Confirmed: second stage runs 2 outer iterations × 8 inner iterations,
 * with destination stride 0x38 (14 floats) per inner iteration.
 */
void actor_move_get_avoidance_direction(void)
{
  /* First-stage table: 9 rows of 7 floats, base 0x6327e0, stride 0x1c. */
  float *p_a = (float *)0x6327e0;
  /* Second-stage table rooted at 0x6325c0; the working outer pointer starts
   * at 0x6325cc (entry stores reach back via p_b[-3..0] and forward p_b[1..3]).
   * The outer pointer advances 7 floats per outer iteration; the inner pointer
   * advances 14 floats (0x38) per inner iteration. */
  float *outer_b = (float *)0x6325cc;
  /* Per-column basis vectors, reset to 0x632780 at the start of each outer
   * iteration (only 8 entries are ever live, stride 0xc). */
  float *basis;
  float *p_b;

  const float *angle_table_9 = (const float *)0x2557cc;
  const float *scale_table_9 = (const float *)0x2557a8;
  const float *length_table_9 = (const float *)0x255784;
  const float k_angle = *(const float *)0x255780;
  const float k_length = *(const float *)0x25577c;
  const float k_base = *(const float *)0x255778;

  const float *outer_angles = (const float *)0x25581c;
  const float *outer_scales = (const float *)0x255814;
  const float *inner_angles;
  const float k_inner_base = *(const float *)0x2557f0;

  int i;
  int row;
  int col;
  float angle, sin_angle, cos_angle, scaled_angle, sin_scaled, scaled_len;
  float sin_outer, cos_outer, row_scale;
  float inner, cos_inner, sin_inner;

  for (i = 0; i < 9; i++) {
    angle = angle_table_9[i];
    sin_angle = x87_fsin(angle);
    cos_angle = x87_fcos(angle);
    scaled_angle = k_angle * scale_table_9[i];
    sin_scaled = x87_fsin(scaled_angle);
    scaled_len = k_length * length_table_9[i];

    p_a[0] = k_base;
    p_a[1] = 0.0f;
    p_a[2] = scaled_len * cos_angle;
    p_a[3] = scaled_len * sin_angle;
    p_a[4] = x87_fcos(scaled_angle);
    p_a[5] = sin_scaled * cos_angle;
    p_a[6] = sin_scaled * sin_angle;
    p_a += 7;
  }

  for (row = 0; row < 2; row++) {
    sin_outer = x87_fsin(outer_angles[row]);
    cos_outer = x87_fcos(outer_angles[row]);
    row_scale = outer_scales[row];

    basis = (float *)0x632780;
    p_b = outer_b;
    inner_angles = (const float *)0x2557f4;

    for (col = 0; col < 8; col++) {
      inner = inner_angles[col];
      cos_inner = x87_fcos(inner);
      sin_inner = x87_fsin(inner);

      basis[0] = 0.0f;
      basis[1] = cos_inner;
      basis[2] = sin_inner;

      /* table_b entry base = p_b + 3 floats:
       *   [+0] k_inner_base
       *   [+0x10] row_scale * basis[0..2]
       *   [+0x1c] sin_outer * basis[0..2], with [+0x1c] then overwritten by
       *           cos_outer (the sin_outer*basis[0] product is dead). */
      p_b[-3] = k_inner_base;
      p_b[-2] = row_scale * basis[0];
      p_b[-1] = row_scale * basis[1];
      p_b[0] = row_scale * basis[2];
      p_b[1] = sin_outer * basis[0];
      p_b[2] = sin_outer * basis[1];
      p_b[3] = sin_outer * basis[2];
      p_b[1] = cos_outer;

      basis += 3;
      p_b += 14;
    }
    outer_b += 7;
  }
}

/* actor_path_3d_available (0x2b720) — Check if vehicle actor should brake.
 *
 * If actor is in a type-4 vehicle state (actor[0x15e] == 4):
 *   - Reads vehicle tag stopping distance (vehi_tag[0x388])
 *   - If stopping distance > 0 and actor's speed factor (actor[0x5ec]) >
 * threshold:
 *     - Computes delta vector from actor position to dest_pos
 *     - Normalizes delta (getting distance)
 *     - If distance > 0 and dot(normalized_delta, facing) > threshold:
 *       returns 0 (should brake)
 * Writes stopping distance to *dist_out if non-NULL.
 * Returns 1 (don't brake) by default.
 *
 * Confirmed: datum_get at 0x2b733. BL=1 default at 0x2b74c.
 * Confirmed: CMP word [ESI+0x15e],4 at 0x2b73d.
 * Confirmed: object_get_and_verify_type(actor[0x158], 2) at 0x2b75d.
 * Confirmed: tag_get('vehi', vehicle[0]) at 0x2b76a.
 * Confirmed: vehi[0x388] → local_8 at 0x2b76f.
 * Confirmed: FCOMP [0x2533c0] checks at 0x2b77e, 0x2b7d1.
 * Confirmed: FCOMP [0x2555d0] speed check at 0x2b795.
 * Confirmed: normalize3d(&delta) at 0x2b7cc.
 * Confirmed: dot product z*fz + y*fy + x*fx at 0x2b7e1-0x2b7fe.
 * Confirmed: FCOMP [0x253d54] dot threshold at 0x2b800.
 * Confirmed: dist_out write if non-NULL at 0x2b80f-0x2b819.
 */
char actor_path_3d_available(int actor_handle, float *dest_pos, float *dist_out)
{
  char *actor;
  char *vehi;
  float local_8;
  float delta[3];
  char result;

  actor = (char *)datum_get(*(void **)0x6325a4, actor_handle);
  local_8 = 0.0f;
  result = 1;
  if (*(int16_t *)(actor + 0x15e) == 4) {
    vehi = (char *)object_get_and_verify_type(*(int *)(actor + 0x158), 2);
    vehi = (char *)tag_get(0x76656869, *(int *)vehi);
    local_8 = *(float *)(vehi + 0x388);
    if (local_8 > *(float *)0x2533c0 &&
        *(float *)(actor + 0x5ec) > *(float *)0x2555d0) {
      delta[0] = dest_pos[0] - *(float *)(actor + 0x12c);
      delta[1] = dest_pos[1] - *(float *)(actor + 0x130);
      delta[2] = dest_pos[2] - *(float *)(actor + 0x134);
      if (normalize3d(delta) > *(float *)0x2533c0 &&
          delta[0] * *(float *)(actor + 0x174) +
              delta[1] * *(float *)(actor + 0x178) +
              delta[2] * *(float *)(actor + 0x17c) >
            *(float *)0x253d54) {
        result = 0;
      }
    }
  }
  if (dist_out != (float *)0) {
    *dist_out = local_8;
  }
  return result;
}

/* 0x2b830 — FUN_0002b830: choose a desired facing vector.
 *
 * Builds four candidate facing vectors in a local [4][3] array:
 *   cand0 = normalized in_vec (z zeroed first when use_3d == 0)
 *   cand1 = -cand0
 *   cand2 = world forward (*0x31fc38) when use_3d != 0, else the 2D
 *           perpendicular (-y, x, 0) of cand0
 *   cand3 = -cand2
 * If normalize3d() reports the vector is degenerate (returns the 0.0 sentinel
 * at 0x2533c0), cand0 falls back to facing_basis (param_1 / @ecx).
 *
 * Each candidate is scored by two dot products: against weight_vec (@edi) and
 * against facing_basis (@ecx). The 3D form is used when use_3d != 0, otherwise
 * the XY-plane form. The loop keeps the running best (index, score-pair) and
 * applies a 0.5 tie-break threshold (0x253398) before replacing the best.
 *
 * The chosen candidate is written to out_vector and its index to out_index,
 * then the result is validated as a unit normal (|len^2 - 1| <= ~0.001).
 *
 * Register args confirmed from caller FUN_0002daa0 @ 0x2dd9a-0x2ddb4:
 *   ECX = facing_basis (this+0x174), EAX = in_vec (the desired vector),
 *   EDI = weight_vec (this+0x524); cdecl stack: use_3d, out_vector, out_index
 *   (caller cleans 0xc bytes). */
void FUN_0002b830(float *facing_basis /* @<ecx> */, char use_3d,
                  float *out_vector, short *out_index,
                  float *in_vec /* @<eax> */, float *weight_vec /* @<edi> */)
{
  float cand[12];
  float best_weight_dot;
  float best_basis_dot;
  float cur_basis_dot;
  float cur_weight_dot;
  short best_index;
  short i;
  float *p;
  int chosen;

  /* cand0 = in_vec. */
  cand[0] = in_vec[0];
  cand[1] = in_vec[1];
  cand[2] = in_vec[2];

  if (use_3d == 0) {
    cand[2] = 0.0f;
    if (normalize3d(cand) == *(float *)0x2533c0) {
      cand[0] = facing_basis[0];
      cand[1] = facing_basis[1];
      cand[2] = facing_basis[2];
    }
    /* cand2 = perpendicular (-y, x, 0). */
    cand[6] = -cand[1];
    cand[7] = cand[0];
    cand[8] = 0.0f;
  } else {
    if (normalize3d(cand) == *(float *)0x2533c0) {
      cand[0] = facing_basis[0];
      cand[1] = facing_basis[1];
      cand[2] = facing_basis[2];
    }
    /* cand2 = world forward vector. */
    cand[6] = *(float *)*(int *)0x31fc38;
    cand[7] = ((float *)*(int *)0x31fc38)[1];
    cand[8] = ((float *)*(int *)0x31fc38)[2];
  }

  /* cand1 = -cand0, cand3 = -cand2. */
  cand[3] = -cand[0];
  cand[4] = -cand[1];
  cand[5] = -cand[2];
  cand[9] = -cand[6];
  cand[10] = -cand[7];
  cand[11] = -cand[8];

  best_index = -1;
  best_weight_dot = 0.0f;
  best_basis_dot = 0.0f;
  p = cand;
  for (i = 0; i < 4; i++) {
    if (use_3d == 0) {
      cur_weight_dot = weight_vec[0] * p[0] + weight_vec[1] * p[1];
      cur_basis_dot = facing_basis[0] * p[0];
    } else {
      cur_weight_dot =
        weight_vec[0] * p[0] + weight_vec[1] * p[1] + weight_vec[2] * p[2];
      cur_basis_dot = facing_basis[0] * p[0] + facing_basis[2] * p[2];
    }
    cur_basis_dot = p[1] * facing_basis[1] + cur_basis_dot;

    if (best_index == -1) {
      best_weight_dot = cur_weight_dot;
      best_basis_dot = cur_basis_dot;
      best_index = i;
    } else if (cur_weight_dot <= best_weight_dot) {
      if (best_basis_dot < cur_basis_dot &&
          *(float *)0x253398 < cur_weight_dot) {
        best_weight_dot = cur_weight_dot;
        best_basis_dot = cur_basis_dot;
        best_index = i;
      }
    } else if (best_basis_dot < cur_basis_dot ||
               best_basis_dot < *(float *)0x253398) {
      best_weight_dot = cur_weight_dot;
      best_basis_dot = cur_basis_dot;
      best_index = i;
    }

    p += 3;
  }

  *out_index = best_index;
  chosen = (int)best_index;
  out_vector[0] = cand[chosen * 3];
  out_vector[1] = cand[chosen * 3 + 1];
  out_vector[2] = cand[chosen * 3 + 2];

  {
    float err;
    err = (out_vector[2] * out_vector[2] + out_vector[1] * out_vector[1] +
           out_vector[0] * out_vector[0]) -
          *(float *)0x2533c8;
    if (((*(unsigned int *)&err & 0x7f800000) == 0x7f800000) ||
        (fabsf(err) >= *(double *)0x2549d8)) {
      display_assert(csprintf((char *)0x5ab100,
                              "%s: assert_valid_real_normal3d(%f, %f, %f)",
                              "desired_facing_vector", (double)out_vector[0],
                              (double)out_vector[1], (double)out_vector[2]),
                     "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0x764, 1);
      system_exit(-1);
    }
  }
}

/* 0x2bab0 — actor_move_facing_to_movement_frame: express the movement
 * direction in the local frame of the facing direction, writing the result
 * into the caller's output vector.
 *
 * Register args (confirmed from caller FUN_0002daa0 @ 0x2dbeb-0x2dbf7):
 *   use_3d              @<al>   : 0 selects the 2D (planar) path, nonzero the
 *                                 full 3D path.
 *   movement_direction  @<esi>  : float[3] unit movement direction.
 *   facing_direction    @<edi>  : float[3] unit facing direction.
 *   out                 @<ebx>  : float[3] result frame coordinates.
 *
 * 3D path: out = ( movement.facing, movement.left, movement.up ) where
 *   biped_build_flying_axes(facing, left, up) builds the orthonormal frame
 *   (left at EBP-0x10, up at EBP-0x1c).  Then normalize3d(out).
 *   Asserts: real_normal3d(movement) @0x775, real_normal3d(facing) @0x776.
 * 2D path: out[0] = movement.facing (2D dot), out[1] = the 2D cross
 *   facing[0]*movement[1] - facing[1]*movement[0], out[2] = 0; normalize3d.
 *   Asserts: real_normal2d(movement) @0x785, real_normal2d(facing) @0x786,
 *   realcmp(movement->k) @0x787, realcmp(facing->k) @0x788 (k-component must
 *   be finite and below the *0x2549d8 bound). */
void FUN_0002bab0(char use_3d /* @<al> */,
                  float *movement_direction /* @<esi> */,
                  float *facing_direction /* @<edi> */, float *out /* @<ebx> */)
{
  float left[3];
  float up[3];
  float mk;
  float fk;

  if (use_3d != 0) {
    if (valid_real_normal3d(movement_direction) == 0) {
      display_assert(
        csprintf((char *)0x5ab100, "%s: assert_valid_real_normal3d(%f, %f, %f)",
                 "movement_direction", (double)movement_direction[0],
                 (double)movement_direction[1], (double)movement_direction[2]),
        "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0x775, 1);
      system_exit(-1);
    }
    if (valid_real_normal3d(facing_direction) == 0) {
      display_assert(
        csprintf((char *)0x5ab100, "%s: assert_valid_real_normal3d(%f, %f, %f)",
                 "facing_direction", (double)facing_direction[0],
                 (double)facing_direction[1], (double)facing_direction[2]),
        "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0x776, 1);
      system_exit(-1);
    }
    biped_build_flying_axes(facing_direction, left, up);
    out[0] = movement_direction[0] * facing_direction[0] +
             facing_direction[1] * movement_direction[1] +
             facing_direction[2] * movement_direction[2];
    out[1] = left[1] * movement_direction[1] + left[2] * movement_direction[2] +
             left[0] * movement_direction[0];
    out[2] = up[1] * movement_direction[1] + up[2] * movement_direction[2] +
             up[0] * movement_direction[0];
    normalize3d(out);
    return;
  }

  if (valid_real_normal2d(movement_direction) == 0) {
    display_assert(
      csprintf((char *)0x5ab100, "%s: assert_valid_real_normal2d(%f, %f)",
               "(real_vector2d *) movement_direction",
               (double)movement_direction[0], (double)movement_direction[1]),
      "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0x785, 1);
    system_exit(-1);
  }
  if (valid_real_normal2d(facing_direction) == 0) {
    display_assert(
      csprintf((char *)0x5ab100, "%s: assert_valid_real_normal2d(%f, %f)",
               "(real_vector2d *) facing_direction",
               (double)facing_direction[0], (double)facing_direction[1]),
      "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0x786, 1);
    system_exit(-1);
  }
  mk = movement_direction[2];
  if (((*(unsigned int *)&mk & 0x7f800000) == 0x7f800000) ||
      (!(fabsf(mk) < *(double *)0x2549d8))) {
    display_assert(csprintf((char *)0x5ab100,
                            "%s, %s: assert_valid_realcmp(%f, %f)",
                            "movement_direction->k", (char *)0x255b18,
                            (double)movement_direction[2], 0, 0),
                   "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0x787, 1);
    system_exit(-1);
  }
  fk = facing_direction[2];
  if (((*(unsigned int *)&fk & 0x7f800000) == 0x7f800000) ||
      (!(fabsf(fk) < *(double *)0x2549d8))) {
    display_assert(csprintf((char *)0x5ab100,
                            "%s, %s: assert_valid_realcmp(%f, %f)",
                            "facing_direction->k", (char *)0x255b18,
                            (double)facing_direction[2], 0, 0),
                   "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0x788, 1);
    system_exit(-1);
  }
  out[0] = movement_direction[0] * facing_direction[0] +
           movement_direction[1] * facing_direction[1];
  out[1] = facing_direction[0] * movement_direction[1] +
           (-facing_direction[1]) * movement_direction[0];
  out[2] = 0.0f;
  normalize3d(out);
}

/* 0x2bd80 — actor_move_compute_avoidance: the per-tick obstacle-avoidance and
 * movement-vector resolver.  Given the actor's desired facing (avoidance
 * rotation), it builds a local avoidance state, samples avoidance rays in 8
 * directions, scores each direction, picks the best, and produces an output
 * velocity direction (vel_out) and emergency turn amount (speed_out).
 *
 * ABI (confirmed from caller FUN_0002e560 @ 0x2e705-0x2e710):
 *   actor_handle @<ecx> : actor datum handle.
 *   facing              : float[3] desired facing / avoidance rotation (read).
 *   vel_out             : float[3] output movement direction.
 *   speed_out           : float    output emergency amount.
 *
 * The avoidance state is a 0x6048-byte scratch struct, copied at the end into
 * the per-actor avoidance record (avd) at avd+0x1a0.  avd lives at
 * actor_index_low * 0x657c + *0x331f58.  Layout of avoidance_state (offsets
 * confirmed from prologue disasm 0x2be48-0x2bef7):
 *   +0x00 scenario ptr   +0x04 collision bsp ptr   +0x08 unit datum handle
 *   +0x0c world position[3]   +0x18 object forward row (obj+0x24)[3]
 *   +0x24 cross(up,forward)[3]   +0x30 object up row (obj+0x30)[3]
 *
 * Confirmed: FUN_001d90e0 is _chkstk (frame > 0x1000), not SEH.  The scattered
 * decompiler stores into local_1c/uStack_18/local_24/local_28 are chkstk/frame
 * scheduling noise and are not real stores. */
void FUN_0002bd80(int actor_handle /* @<ecx> */, float *facing, float *vel_out,
                  float *speed_out)
{
  unsigned char avoidance_state[0x6048];
  int actor;
  int obj;
  int avd;
  int unit_handle;
  float *fwd;
  float *up;
  float *state;
  float weights[8];
  float max_weight;
  short last_best_dir;
  int idx;
  int ip0, ip2, ip4, ip5, ip6, ip7;
  int i, n;
  float *wp;
  float *rec;
  int *table;
  short hit_count;
  float vel_mag;
  float emergency;
  float mag, inv;
  char have_dir;
  float best_value;
  short best_dir;
  float out[3]; /* EBP-0x24/-0x20/-0x1c: prologue=world translation, final
                   vel_out */
  float work[3]; /* EBP-0x10/-0xc/-0x8: emergency/cross working vector */
  float xform[3]; /* EBP-0x40/-0x3c/-0x38: avoidance-vector transform output */
  float move_amt, move_idx, move_value;
  float em_value, em_index, em_push;
  float out_speed;
  unsigned char *pb;

  state = (float *)avoidance_state;
  actor = (int)datum_get((data_t *)*(int *)0x6325a4, actor_handle);

  /* world-matrix translation row; also the default movement output used by the
   * unit_handle==-1 early-exit. */
  out[0] = (*(float **)0x31fc38)[0];
  out[1] = (*(float **)0x31fc38)[1];
  out[2] = (*(float **)0x31fc38)[2];
  out_speed = *(float *)0x2533c0;

  unit_handle = *(int *)(actor + 0x158);
  if (unit_handle == -1) {
    unit_handle = *(int *)(actor + 0x18);
  }

  if (vel_out == (float *)0x0 || speed_out == (float *)0x0) {
    display_assert("avoidance_rotation && emergency_amount",
                   "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0x887, 1);
    system_exit(-1);
  }

  if (unit_handle == -1) {
    goto write_outputs;
  }

  obj = (int)object_get_and_verify_type(unit_handle, -1);
  /* avd = (actor_index_low & 0xffff) * 0x657c + *0x331f58 */
  avd = (actor_handle & 0xffff) * 0x657c + *(int *)0x331f58;

  /* game_time_get() stored into avd+0x19c (timestamp). */
  *(int *)(avd + 0x19c) = game_time_get();

  *(int *)((char *)state + 0) = (int)scenario_get();
  *(int *)((char *)state + 4) = (int)global_collision_bsp_get();
  *(int *)((char *)state + 8) = unit_handle;
  object_get_world_position(unit_handle, (vector3_t *)((char *)state + 0xc));

  /* forward = obj+0x24, up = obj+0x30 */
  fwd = (float *)(obj + 0x24);
  *(float *)((char *)state + 0x18) = fwd[0];
  *(float *)((char *)state + 0x1c) = fwd[1];
  *(float *)((char *)state + 0x20) = fwd[2];
  up = (float *)(obj + 0x30);
  *(float *)((char *)state + 0x30) = up[0];
  *(float *)((char *)state + 0x34) = up[1];
  *(float *)((char *)state + 0x38) = up[2];
  /* cross(up, forward) into +0x24 */
  *(float *)((char *)state + 0x24) = up[1] * fwd[2] - up[2] * fwd[1];
  *(float *)((char *)state + 0x28) = fwd[0] * up[2] - up[0] * fwd[2];
  *(float *)((char *)state + 0x2c) = up[0] * fwd[1] - fwd[0] * up[1];

  FUN_0002ade0((int)state);

  /* zero the 8-direction weight array, reset running max. */
  csmemset(weights, 0, 0x20);
  max_weight = 0.0f;

  /* seed weights around the previously chosen best direction (actor[0x5d8]). */
  last_best_dir = *(short *)(actor + 0x5d8);
  if (last_best_dir >= 0 && last_best_dir < 8) {
    idx = (int)last_best_dir;
    ip0 = (idx + 1) % 8;
    ip2 = (idx + 2) % 8;
    ip7 = (idx + 7) % 8;
    ip6 = (idx + 6) % 8;
    weights[idx] = weights[idx] + *(float *)0x253524;
    weights[(short)ip0] =
      *(float *)0x255954 * *(float *)0x253524 + weights[(short)ip0];
    weights[(short)ip2] =
      *(float *)0x255958 * *(float *)0x253524 + weights[(short)ip2];
    weights[(short)ip7] =
      *(float *)0x255954 * *(float *)0x253524 + weights[(short)ip7];
    weights[(short)ip6] =
      *(float *)0x255958 * *(float *)0x253524 + weights[(short)ip6];
  }

  /* First avoidance ray pass: 9 rays.  Each ray is transformed and cast by
   * FUN_0002b020 (avoidance_ray@<eax>, ray_origin@<ebx>, avoidance_data@<esi>).
   * The per-ray origin/direction are recorded at avd+0x6220/avd+0x628c (stride
   * 3 floats); the hit count goes to avd+0x61e8 (stride 2, short) and the
   * collision time to avd+0x61fc (stride 4, float). */
  rec = (float *)(avd + 0x628c);
  wp = (float *)0x255828; /* per-ray blend-weight table (8 floats/ray) */
  table = (int *)0x6327e0; /* per-ray packed-ray records (7 ints/ray) */
  {
    short *count_ptr = (short *)(avd + 0x61e8);
    float *t_ptr = (float *)(avd + 0x61fc);
    float ray_dir[3];
    float ray_origin[3];
    float collision_t;
    for (n = 9; n != 0; n--) {
      hit_count = (short)FUN_0002b020((float *)table, ray_origin, (int)state,
                                      ray_dir, &collision_t, (char *)0);
      rec[-0x1b] = ray_origin[0];
      rec[-0x1a] = ray_origin[1];
      rec[-0x19] = ray_origin[2];
      rec[0] = ray_dir[0];
      rec[1] = ray_dir[1];
      rec[2] = ray_dir[2];
      *count_ptr = hit_count;
      *t_ptr = collision_t;
      if (hit_count > 0) {
        float frac = *(float *)0x2533c8 - collision_t;
        float *acc = weights;
        float *blend = wp;
        for (i = 8; i != 0; i--) {
          float bf = frac + frac;
          if (*(float *)0x2533c8 < bf) {
            bf = *(float *)0x2533c8;
          }
          *acc = bf * *blend + *acc;
          blend++;
          acc++;
        }
        if (max_weight <= frac) {
          max_weight = frac;
        }
      }
      count_ptr = count_ptr + 1;
      t_ptr = t_ptr + 1;
      table = table + 7;
      wp = wp + 8;
      rec = rec + 3;
    }
  }

  /* Second pass: for each of 8 directions, sample 2 short avoidance probes,
   * classify them, accumulate a per-direction score, and redistribute it into
   * the weight array with the same neighbor-falloff used for seeding.  Probe
   * records: hit-count shorts at avd+0x6418 (stride 2), origin/direction floats
   * at avd+0x62f8/avd+0x6318 (stride 3). */
  {
    short *probe_count = (short *)(avd + 0x6418);
    float *probe_origin = (float *)(avd + 0x62f8);
    float *probe_dir = (float *)(avd + 0x6318);
    int *probe_table = (int *)0x6325c0;
    pb = (unsigned char *)(actor + 0x5c9);
    idx = 2; /* current direction index (local_70) */
    for (n = 8; n != 0; n--) {
      short probe_hits[2];
      float probe_t[2];
      float probe_buf[2 * 3];
      float score;
      int k;
      unsigned char *pbi = pb;
      for (k = 0; k < 2; k++) {
        float pd[3];
        float po[3];
        float pt;
        probe_hits[k] = (short)FUN_0002b020(
          (float *)probe_table, po, (int)state, pd, &pt, (char *)(pbi - 1));
        probe_origin[k * 3 + 0] = po[0];
        probe_origin[k * 3 + 1] = po[1];
        probe_origin[k * 3 + 2] = po[2];
        probe_dir[k * 3 + 0] = pd[0];
        probe_dir[k * 3 + 1] = pd[1];
        probe_dir[k * 3 + 2] = pd[2];
        probe_count[k] = probe_hits[k];
        probe_buf[k * 3 + 0] = pd[0];
        probe_buf[k * 3 + 1] = pd[1];
        probe_buf[k * 3 + 2] = pd[2];
        probe_t[k] = pt;
        probe_table = probe_table + 7;
        pbi = pbi + 1;
      }

      /* classify the two probes back-to-front and accumulate score. */
      {
        int blocked = 0;
        score = *(float *)0x2533c0;
        for (k = 1; k >= 0; k--) {
          if (probe_hits[k] == 0) {
            float v;
            if (blocked) {
              v = *(float *)0x2533c8;
              score = v * *(float *)(0x25594c + k * 4) + score;
            } else {
              unsigned char b = pb[k];
              if (b < 0x4b) {
                score =
                  *(float *)0x2533c0 * *(float *)(0x25594c + k * 4) + score;
              } else {
                v = *(float *)0x2533c8 - *(float *)0x255ca4 / (float)b;
                if (*(float *)0x2533c0 <= v) {
                  if (*(float *)0x2533c8 < v) {
                    v = *(float *)0x2533c8;
                  }
                  score = v * *(float *)(0x25594c + k * 4) + score;
                } else {
                  score =
                    *(float *)0x2533c0 * *(float *)(0x25594c + k * 4) + score;
                }
              }
            }
          } else {
            float v = *(float *)0x2533c8 - probe_buf[k * 3 + 1];
            v = v + v;
            if (*(float *)0x2533c8 < v) {
              v = *(float *)0x2533c8;
            }
            blocked = 1;
            score = score - v * *(float *)(0x25594c + k * 4);
          }
        }
      }

      ip0 = (idx - 1) % 8;
      ip2 = idx % 8;
      ip5 = (idx + 5) % 8;
      ip4 = (idx + 4) % 8;
      weights[(short)((idx - 2) & 7)] = score + weights[(short)((idx - 2) & 7)];
      weights[(short)ip0] = *(float *)0x255954 * score + weights[(short)ip0];
      weights[(short)ip2] = score * *(float *)0x255958 + weights[(short)ip2];
      weights[(short)ip5] = *(float *)0x255954 * score + weights[(short)ip5];
      weights[(short)ip4] = score * *(float *)0x255958 + weights[(short)ip4];

      pb = pb + 2;
      idx = idx + 1;
    }
  }

  /* Emergency obstacle response: if the object's velocity (obj+0x3c..0x44)
   * magnitude exceeds *0x255ca0, push the avoidance weights away from the
   * velocity direction (projected into the avoidance frame, 2D). */
  *(unsigned char *)(avd + 0x6551) = 0;
  vel_mag = sqrtf(*(float *)(obj + 0x44) * *(float *)(obj + 0x44) +
                  *(float *)(obj + 0x40) * *(float *)(obj + 0x40) +
                  *(float *)(obj + 0x3c) * *(float *)(obj + 0x3c));
  if (*(float *)0x255ca0 < vel_mag) {
    float dlen, em_scl;
    em_value = 0.0f;
    em_scl = *(float *)0x2533c0;
    em_push = (vel_mag - *(float *)0x255ca0) * *(float *)0x255c9c;
    if (*(float *)0x2533c8 < em_push) {
      em_push = *(float *)0x2533c8;
    }
    em_push = em_push * *(float *)0x2533f0;
    work[0] = 0.0f;
    work[1] = *(float *)((char *)state + 0x34) * *(float *)(obj + 0x40) +
              *(float *)((char *)state + 0x38) * *(float *)(obj + 0x44) +
              *(float *)((char *)state + 0x30) * *(float *)(obj + 0x3c);
    work[2] = -(*(float *)((char *)state + 0x2c) * *(float *)(obj + 0x44) +
                *(float *)((char *)state + 0x28) * *(float *)(obj + 0x40) +
                *(float *)((char *)state + 0x24) * *(float *)(obj + 0x3c));
    dlen = sqrtf(work[1] * work[1] + work[2] * work[2]);
    if (fabsf(dlen) >= *(double *)0x2533d0) {
      inv = *(float *)0x2533c8 / dlen;
      work[0] = *(float *)0x2533c0 * inv;
      work[1] = work[1] * inv;
      work[2] = inv * work[2];
      if (*(float *)0x2533c0 < dlen) {
        char ok =
          FUN_0002b310(work, 8, 0x632780, weights, &em_index, &em_value);
        em_scl = *(float *)0x2533c0;
        if (ok != '\0' && *(float *)0x253398 < em_value) {
          int j2;
          float *w = weights;
          float *p = (float *)0x632784;
          for (j2 = 8; j2 != 0; j2--) {
            float d = work[2] * p[0] + work[0] * p[-1] + work[1] * p[1];
            if (d < *(float *)0x2533c0) {
              *w = d * em_push + *w;
            }
            em_scl = em_push;
            p = p + 3;
            w = w + 1;
          }
        }
      }
    }
    *(float *)(avd + 0x6554) = em_scl;
    *(float *)(avd + 0x6558) = vel_mag;
    *(float *)(avd + 0x655c) = work[0];
    *(float *)(avd + 0x6560) = work[1];
    *(unsigned char *)(avd + 0x6551) = 1;
    *(float *)(avd + 0x6564) = work[2];
    *(float *)(avd + 0x6568) = em_value;
  }

  /* find the best (largest) weight above *0x255c98. */
  best_value = *(float *)0x255c98;
  best_dir = -1;
  for (i = 0; (short)i < 8; i++) {
    if (best_value < weights[i]) {
      best_value = weights[i];
      best_dir = (short)i;
    }
  }
  if (best_dir < 0 || best_dir >= 8) {
    display_assert(
      "(best_avoidance_direction >= 0) && (best_avoidance_direction < "
      "VECTOR_AVOIDANCE_NUMBER_OF_DIRECTIONS)",
      "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0x983, 1);
    system_exit(-1);
  }

  /* copy the 8 weights into the avoidance record. */
  csmemcpy((void *)(avd + 0x64d8), weights, 0x20);

  /* express the requested facing in the avoidance frame; validate the
   * resulting movement-direction approximation.  xform = normalized facing;
   * move_idx = facing.forward; work = facing projected onto the lateral axes.
   */
  xform[0] = facing[0];
  xform[1] = facing[1];
  xform[2] = facing[2];
  work[0] = (*(float **)0x31fc38)[0];
  work[1] = (*(float **)0x31fc38)[1];
  work[2] = (*(float **)0x31fc38)[2];
  move_idx = 1.0f;
  move_amt = 0.0f;
  move_value = 0.0f;
  mag = sqrtf(xform[0] * xform[0] + xform[1] * xform[1] + xform[2] * xform[2]);
  if (fabsf(mag) >= *(double *)0x2533d0) {
    inv = *(float *)0x2533c8 / mag;
    xform[0] = xform[0] * inv;
    xform[1] = xform[1] * inv;
    xform[2] = xform[2] * inv;
    if (*(float *)0x2533c0 < mag) {
      move_idx = xform[0] * *(float *)((char *)state + 0x18) +
                 *(float *)((char *)state + 0x1c) * xform[1] +
                 *(float *)((char *)state + 0x20) * xform[2];
      work[1] = xform[2] * *(float *)((char *)state + 0x2c) +
                xform[1] * *(float *)((char *)state + 0x28) +
                xform[0] * *(float *)((char *)state + 0x24);
      work[2] = xform[1] * *(float *)((char *)state + 0x34) +
                xform[2] * *(float *)((char *)state + 0x38) +
                xform[0] * *(float *)((char *)state + 0x30);
      mag = sqrtf(work[1] * work[1] + work[2] * work[2]);
      if (fabsf(mag) >= *(double *)0x2533d0) {
        inv = *(float *)0x2533c8 / mag;
        work[0] = *(float *)0x2533c0 * inv;
        work[1] = work[1] * inv;
        work[2] = inv * work[2];
        if (*(float *)0x2533c0 < mag) {
          FUN_0002b310(work, 8, 0x632780, weights, &move_amt, &move_value);
          if ((move_amt < *(float *)0x2533c0) ||
              (move_amt > *(float *)0x253f78)) {
            display_assert("(movement_direction_approximation >= 0) && "
                           "(movement_direction_approximation <= ((real) "
                           "VECTOR_AVOIDANCE_NUMBER_OF_DIRECTIONS))",
                           "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0x9a2, 1);
            system_exit(-1);
          }
        }
      }
    }
  }

  /* record the forward row, facing input, best direction, and the
   * direction-approximation residual into the avoidance record. */
  *(float *)(avd + 0x6524) = *(float *)((char *)state + 0x18);
  *(float *)(avd + 0x6528) = *(float *)((char *)state + 0x1c);
  *(float *)(avd + 0x652c) = *(float *)((char *)state + 0x20);
  *(float *)(avd + 0x6530) = facing[0];
  *(float *)(avd + 0x6534) = facing[1];
  *(float *)(avd + 0x6538) = facing[2];
  *(float *)(avd + 0x6504) = move_amt;
  *(float *)(avd + 0x64fc) = best_value;
  *(short *)(avd + 0x6500) = best_dir;
  *(float *)(avd + 0x6508) = move_value;

  /* emergency-amount ramp from the running max weight (max_weight). */
  if (max_weight <= *(float *)0x253f3c) {
    emergency = (max_weight - *(float *)0x253f3c) * *(float *)0x255ba8;
    if (*(float *)0x2533c8 <= emergency) {
      emergency = *(float *)0x2533c8;
    }
    emergency = emergency + *(float *)0x2533c8;
  } else {
    emergency = max_weight * *(float *)0x254e6c;
    if (*(float *)0x2533c8 <= emergency) {
      emergency = 1.0f;
    }
  }
  *(float *)(avd + 0x650c) = best_value - move_idx;
  *(float *)(avd + 0x6510) = move_idx;

  have_dir = 0;
  if (*(float *)0x255ba4 <= move_idx) {
    goto emergency_state;
  }

  if (*(short *)(actor + 0x5f0) == -1 || *(short *)(actor + 0x5f0) >= 0x5a) {
    float velsq = *(float *)(obj + 0x44) * *(float *)(obj + 0x44) +
                  *(float *)(obj + 0x40) * *(float *)(obj + 0x40) +
                  *(float *)(obj + 0x3c) * *(float *)(obj + 0x3c);
    if (velsq <= *(float *)0x255ba0) {
      if (emergency <= *(float *)0x253398) {
        goto emergency_state;
      }
      *(short *)(avd + 0x653c) = 5;
      goto count_state;
    }
    if (*(float *)0x253f40 < (best_value - move_idx) &&
        *(float *)0x253f40 < best_value) {
      *(short *)(avd + 0x653c) = 6;
      goto count_state;
    }
  emergency_state:
    *(short *)(actor + 0x5f0) = -1;
    if (*(float *)0x253398 <= move_idx) {
      /* mode 0/1: steer directly toward the chosen avoidance direction. */
      if (max_weight <= *(float *)0x2533c0) {
        *(short *)(avd + 0x653c) = 0;
      } else {
        float bx, by, bz, blen, axis_x, neg_z;
        bz = -(*(float *)(0x632788 + best_dir * 0xc));
        neg_z = bz;
        axis_x = *(float *)(0x632784 + best_dir * 0xc);
        bx = axis_x * *(float *)((char *)state + 0x30) +
             *(float *)((char *)state + 0x2c) * neg_z +
             (*(float **)0x31fc38)[0];
        by = *(float *)((char *)state + 0x34) * axis_x +
             *(float *)((char *)state + 0x28) * neg_z +
             (*(float **)0x31fc38)[1];
        bz = *(float *)((char *)state + 0x38) * axis_x +
             *(float *)((char *)state + 0x24) * neg_z +
             (*(float **)0x31fc38)[2];
        out[0] = bx;
        out[1] = by;
        out[2] = bz;
        blen = sqrtf(bx * bx + by * by + bz * bz);
        if (fabsf(blen) < *(double *)0x2533d0) {
          blen = 0.0f;
        } else {
          float f = *(float *)0x2533c8 / blen;
          out[0] = bx * f;
          out[1] = by * f;
          out[2] = bz * f;
          if (blen <= *(float *)0x2533c0) {
            blen = 0.0f;
          } else {
            float g = emergency * *(float *)0x255b94;
            out[0] = bx * f * g;
            out[1] = by * f * g;
            out[2] = bz * f * g;
          }
        }
        *(float *)(avd + 0x6520) = blen;
        have_dir = 1;
        *(short *)(avd + 0x653c) = 1;
        *(float *)(avd + 0x651c) = max_weight;
        out_speed = emergency;
      }
    } else if ((best_value - move_idx) <= *(float *)0x255b9c) {
      *(short *)(avd + 0x653c) = 2;
    } else {
      /* mode 3/4: gauge how far the emergency vector (work) already points at
       * the chosen direction; small dot -> mode 4 lateral turn, else mode 3. */
      int bd = (int)best_dir;
      float dot = work[0] * *(float *)(0x632780 + bd * 0xc) +
                  work[1] * *(float *)(0x632784 + bd * 0xc) +
                  work[2] * *(float *)(0x632788 + bd * 0xc);
      if (*(float *)0x253398 < dot) {
        float turn =
          (best_value - move_idx) * *(float *)0x255b98 - *(float *)0x253398;
        float lat;
        move_amt = *(float *)0x2533c0;
        if (*(float *)0x2533c0 <= turn) {
          move_amt = turn;
          if (*(float *)0x2533c8 < turn) {
            move_amt = *(float *)0x2533c8;
          }
        }
        if (move_amt <= emergency) {
          move_amt = emergency;
        }
        emergency = *(float *)0x255b94 * move_amt;
        lat = work[1] * *(float *)(0x632784 + bd * 0xc) -
              work[0] * *(float *)(0x632788 + bd * 0xc);
        if (*(float *)0x2533c0 < lat) {
          emergency = -emergency;
        }
        have_dir = 1;
        out[0] = *(float *)((char *)state + 0x18) * emergency;
        out[1] = *(float *)((char *)state + 0x1c) * emergency;
        *(short *)(avd + 0x653c) = 4;
        out[2] = *(float *)((char *)state + 0x20) * emergency;
        *(float *)(avd + 0x6518) = emergency;
      } else {
        *(float *)(avd + 0x6514) = dot;
        *(short *)(avd + 0x653c) = 3;
      }
    }
  } else {
    *(short *)(avd + 0x653c) = 7;
  count_state:
    if (*(short *)(actor + 0x5f0) == -1) {
      *(short *)(actor + 0x5f0) = 0;
    } else {
      *(short *)(actor + 0x5f0) = *(short *)(actor + 0x5f0) + 1;
    }
    /* mode 5/6/7: blend toward the chosen direction's world-space vector. */
    actor_move_transform_avoidance_vector(
      (int)state, (float *)(0x632780 + best_dir * 0xc), xform);
    work[0] = xform[2] * facing[1] - xform[1] * facing[2];
    work[1] = xform[0] * facing[2] - xform[2] * facing[0];
    work[2] = xform[1] * facing[0] - xform[0] * facing[1];
    mag = sqrtf(work[0] * work[0] + work[1] * work[1] + work[2] * work[2]);
    if (fabsf(mag) >= *(double *)0x2533d0) {
      inv = *(float *)0x2533c8 / mag;
      work[0] = work[0] * inv;
      work[1] = work[1] * inv;
      work[2] = work[2] * inv;
      if (*(float *)0x2533c0 < mag) {
        float scl = FUN_0010c510(facing, xform);
        out[0] = work[0] * scl;
        out[1] = work[1] * scl;
        out[2] = work[2] * scl;
      }
    }
    move_amt = (*(float *)0x253f40 - move_value) * *(float *)0x253398 -
               *(float *)0x253398;
    out_speed = *(float *)0x2533c0;
    if (*(float *)0x2533c0 <= move_amt) {
      out_speed = move_amt;
      if (*(float *)0x2533c8 < move_amt) {
        out_speed = *(float *)0x2533c8;
      }
    }
    if (out_speed <= emergency) {
      out_speed = emergency;
    }
    have_dir = 1;
  }

  *(char *)(avd + 0x6550) = have_dir;
  if (have_dir == 0) {
    *(short *)(actor + 0x5d8) = -1;
  } else {
    *(short *)(actor + 0x5d8) = best_dir;
  }

  /* publish the assembled avoidance state into the per-actor record. */
  {
    int *src = (int *)state;
    int *dst = (int *)(avd + 0x1a0);
    for (i = 0x1812; i != 0; i--) {
      *dst = *src;
      src++;
      dst++;
    }
  }
  *(float *)(avd + 0x654c) = out_speed;
  *(float *)(avd + 0x6540) = out[0];
  *(float *)(avd + 0x6544) = out[1];
  *(float *)(avd + 0x6548) = out[2];

write_outputs:
  vel_out[0] = out[0];
  vel_out[1] = out[1];
  vel_out[2] = out[2];
  *speed_out = out_speed;
}

/*
 * 0x2cdb0 — actor_path_refresh: Compute and populate the actor's path control
 * state for the current movement mode.
 *
 * This function is the per-tick "where should I go?" resolver for actors. It
 * reads the actor's movement source type (actor[0x46c]) to determine how to
 * fill the actor's destination fields (actor[0x488..0x494]) and navigation
 * state (actor[0x4a8]). After resolving the target, it initiates pathfinding
 * and sets actor[0x4a4]=1 when successful.
 *
 * Arguments:
 *   actor_handle   — datum handle identifying the actor.
 *   store_distance — if non-zero, writes the computed 3D distance to the
 *                    destination into actor[0x4a0].
 *   override_path  — if non-NULL (and actor is not mounted), use this
 *                    pre-computed path instead of computing a new one.
 *
 * Returns 1 if pathfinding succeeded (or a target was found), 0 on failure.
 *
 * Movement source types (actor[0x46c]):
 *   0 — none / disabled (early-return, mark ready).
 *   1 — disabled variant (same early-return).
 *   2 — absolute world-space position stored in actor[0x470..0x47c].
 *   3 — AI squad order position (scenario squads block).
 *   4 — encounter squad order (scenario encounter/squad/order blocks).
 *   5 — prop (perception object) position (from prop datum at actor[0x470]).
 *
 * Confirmed: cdecl, 3 args, char return.
 * Confirmed: ESI=actor ptr, EDI=&actor[0x488] after switch cases.
 * Confirmed: BL carries the function return value (0 or 1).
 * Confirmed float constants: 0.0f at 0x2533c0, threshold at 0x255d1c,
 *   threshold2 at 0x253398.
 */
char actor_path_refresh(int actor_handle, char store_distance,
                        void *override_path)
{
  /* All C89 declarations at top of function scope. */
  char *actor;
  short move_src;
  char had_path;
  char path_found;
  char path_found2;
  float saved_pos[3]; /* [EBP-0x18..-0x10]: copy of old actor[0x488..0x490] */
  char *tag; /* [EBP-0xc]: actor tag pointer from tag_get */
  float dist; /* [EBP-0x8]: 3D distance actor→destination */
  char local_nav[44]; /* [EBP-0x60]: nav-state struct (waypoint init output) */
  static char
    large_buf[0x1408c]; /* [EBP+0xfffebf14]: path-build scratch 82060 bytes */
  void
    *path_state; /* allocated path cache slot from ai_debug_get_path_storage */
  int scenario;
  int squad_elem;
  int order_elem;
  int order_elem2;
  short order_idx;
  int prop;
  int game_tick;
  unsigned int actor_handle_u;
  int ai_idx;
  float dist_sq_saved;

  /* datum_get confirmed at 0x0002cdcb: PUSH EAX(actor_handle), PUSH
   * ECX(0x6325a4) */
  actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
  move_src = *(short *)(actor + 0x46c);
  had_path = 0;

  /* If move_src != 0 and != 1, save old destination and set had_path. */
  if (move_src != 0 && move_src != 1) {
    saved_pos[0] = *(float *)(actor + 0x488);
    saved_pos[1] = *(float *)(actor + 0x48c);
    saved_pos[2] = *(float *)(actor + 0x490);
    had_path = 1;
  }

  /*
   * Early-return conditions — actor is busy, paused, or at a terminal state:
   *   actor[0x160] != 0 (some "is_doing" flag)
   *   move_src == 0 or 1 (no movement source)
   *   move_src == 3 && actor[0x3bb] != 0 (squad-order terminal condition)
   * In all cases: re-fetch actor, clear fields, set is_moving=1, return 1.
   * Confirmed at 0x0002d2fb: second datum_get, then BL (=1) is returned.
   */
  if (*(char *)(actor + 0x160) != '\0' || move_src == 0 || move_src == 1 ||
      (move_src == 3 && *(char *)(actor + 0x3bb) != '\0')) {
    /* Second datum_get at 0x0002d305 */
    actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
    *(int *)(actor + 0x4a0) = 0;
    *(char *)(actor + 0x4a8) = 0;
    *(char *)(actor + 0x484) = 1;
    return '\x01';
  }

  /* Clear navigation state fields for this tick. */
  *(char *)(actor + 0x4a8) = 0;
  *(char *)(actor + 0x484) = 0;
  *(int *)(actor + 0x4a0) = 0;
  *(char *)(actor + 0x506) = 0;

  /* Resolve destination by movement source type. */
  switch (move_src) {
  case 2:
    /*
     * Absolute position: copy actor[0x470..0x47c] directly.
     * Confirmed at 0x0002ce6f: LEA EDI,[ESI+0x488]; copy 3 dwords from
     * [ESI+0x470]; then [ESI+0x494] = [ESI+0x47c].
     */
    *(unsigned int *)(actor + 0x488) = *(unsigned int *)(actor + 0x470);
    *(unsigned int *)(actor + 0x48c) = *(unsigned int *)(actor + 0x474);
    *(unsigned int *)(actor + 0x490) = *(unsigned int *)(actor + 0x478);
    *(unsigned int *)(actor + 0x494) = *(unsigned int *)(actor + 0x47c);
    break;

  case 3:
    /*
     * Squad order position: look up the order waypoint from the scenario
     * squads block, indexed by actor[0x34] (squad handle low word).
     *
     * Disasm 0x0002cf62: tag_block_get_element chain (batch ESP cleanup
     * at 0x0002cfbb). Sequence:
     *   global_scenario_get() -> scenario+0x42c = &squads_block
     *   tag_block_get_element(&squads_block, squad_idx, 0xb0) -> squad
     *   tag_block_get_element(squad+0x98, actor[0x470], 0x18) -> order
     *   Copy order[0..8] -> actor[0x488..0x490], order[0x14] -> actor[0x494]
     */
    if (*(unsigned int *)(actor + 0x34) == 0xffffffff) {
      goto LAB_fail;
    }
    ai_idx = (int)(*(unsigned int *)(actor + 0x34) & 0xffff);
    scenario = (int)global_scenario_get();
    squad_elem =
      (int)tag_block_get_element((void *)(scenario + 0x42c), ai_idx, 0xb0);
    order_elem = (int)tag_block_get_element(
      (void *)(squad_elem + 0x98), (int)(short)*(short *)(actor + 0x470), 0x18);
    *(unsigned int *)(actor + 0x488) = *(unsigned int *)(order_elem + 0);
    *(unsigned int *)(actor + 0x48c) = *(unsigned int *)(order_elem + 4);
    *(unsigned int *)(actor + 0x490) = *(unsigned int *)(order_elem + 8);
    *(unsigned int *)(actor + 0x494) = *(unsigned int *)(order_elem + 0x14);
    break;

  case 4:
    /*
     * Encounter order position: look up in scenario encounters ->
     * squads -> orders, indexed by actor[0x34] (encounter handle low
     * word), actor[0x3a] (squad index), actor[0x470] (order index).
     *
     * Disasm 0x0002cec7-0x0002cf5d: same ESP batch pattern.
     * actor[0x494] = order_entry[0x4c] (facing handle).
     */
    if (*(unsigned int *)(actor + 0x34) == 0xffffffff) {
      goto LAB_fail;
    }
    ai_idx = (int)(*(unsigned int *)(actor + 0x34) & 0xffff);
    scenario = (int)global_scenario_get();
    squad_elem =
      (int)tag_block_get_element((void *)(scenario + 0x42c), ai_idx, 0xb0);
    order_elem = (int)tag_block_get_element(
      (void *)(squad_elem + 0x80), (int)(short)*(short *)(actor + 0x3a), 0xe8);
    order_idx = *(short *)(actor + 0x470);
    if (order_idx < 0) {
      goto LAB_fail;
    }
    if ((int)order_idx >= *(int *)(order_elem + 0xc4)) {
      goto LAB_fail;
    }
    order_elem2 = (int)tag_block_get_element((void *)(order_elem + 0xc4),
                                             (int)order_idx, 0x50);
    *(unsigned int *)(actor + 0x488) = *(unsigned int *)(order_elem2 + 0);
    *(unsigned int *)(actor + 0x48c) = *(unsigned int *)(order_elem2 + 4);
    *(unsigned int *)(actor + 0x490) = *(unsigned int *)(order_elem2 + 8);
    *(unsigned int *)(actor + 0x494) = *(unsigned int *)(order_elem2 + 0x4c);
    break;

  case 5:
    /*
     * Prop position: actor[0x470] is a prop datum handle. Fetch the prop
     * from prop_data (DAT_005ab23c). Validate it is in a valid-prop state
     * (prop[0x24] in [4,5]), then copy position fields.
     *
     * actor[0x99] selects between two prop position fields:
     *   ==0: prop[0xf0..0xf8] (normal position)
     *   !=0: prop[0xc8..0xd0] (vehicle/mounted position)
     * actor[0x494] = prop[0xec] (velocity handle).
     * actor[0x498] = actor[0x474] (facing yaw carry-over).
     */
    prop = (int)datum_get(*(data_t **)0x5ab23c, *(int *)(actor + 0x470));
    if ((*(short *)(prop + 0x24) < 4) || (*(short *)(prop + 0x24) > 5)) {
      /* Prop state invalid: notify and continue (don't abort). */
      actor_perception_find_prop_pathfinding_location(actor_handle,
                                                      *(int *)(actor + 0x470));
    }
    if (*(char *)(actor + 0x99) != '\0') {
      *(unsigned int *)(actor + 0x488) = *(unsigned int *)(prop + 0xc8);
      *(unsigned int *)(actor + 0x48c) = *(unsigned int *)(prop + 0xcc);
      *(unsigned int *)(actor + 0x490) = *(unsigned int *)(prop + 0xd0);
    } else {
      *(unsigned int *)(actor + 0x488) = *(unsigned int *)(prop + 0xf0);
      *(unsigned int *)(actor + 0x48c) = *(unsigned int *)(prop + 0xf4);
      *(unsigned int *)(actor + 0x490) = *(unsigned int *)(prop + 0xf8);
    }
    *(unsigned int *)(actor + 0x494) = *(unsigned int *)(prop + 0xec);
    *(unsigned int *)(actor + 0x498) = *(unsigned int *)(actor + 0x474);
    goto LAB_check_dest;

  default:
    display_assert((char *)0, "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0xb7f, 1);
    system_exit(-1);
    goto LAB_fail;
  }

  /* Cases 2/3/4 fall through here; case 5 jumps to LAB_check_dest. */
  *(int *)(actor + 0x498) = 0;

LAB_check_dest:
  /*
   * Validate destination. Two branches:
   *
   * B) actor[0x99]!=0 (mounted): call actor_path_3d_available to check whether
   * the destination is accessible for a mounted actor; output dist. Confirmed
   * at 0x0002ceab-0x0002cebf: JZ skip (actor[0x99]==0) PUSH
   * LEA[EBP-0xc](&dist); PUSH EDI(&actor[0x488]); PUSH ECX CALL
   * actor_path_3d_available
   *
   * A) actor[0x99]==0 (on foot): if actor[0x498]==0.0f, check
   *    actor[0x494]!=-1. If -1, fail. If actor[0x498]!=0.0f, fall through.
   *    Confirmed at 0x0002d096-0x0002d0b3.
   */
  if (*(char *)(actor + 0x99) != '\0') {
    path_found =
      actor_path_3d_available(actor_handle, (float *)(actor + 0x488), &dist);
    if (path_found == '\0') {
      goto LAB_fail;
    }
  } else {
    if (*(float *)(actor + 0x498) == 0.0f) {
      path_found = (char)(*(int *)(actor + 0x494) != -1);
      if (path_found == '\0') {
        goto LAB_fail;
      }
    }
  }

  /* Try fast path: actor is already navigating to the same destination. */
  path_found = actor_test_destination(actor_handle);
  if (path_found != '\0') {
    if (!had_path) {
      goto LAB_path_ok;
    }
    /*
     * Had a previous destination endpoint. Compute squared distance between
     * the saved endpoint (saved_pos) and the new destination (actor[0x488]).
     * If close enough (dist_sq <= threshold at 0x255d1c), return 1 quickly.
     * If destination has changed significantly, fall through to do a full
     * re-path.
     * Confirmed at 0x0002d0d6-0x0002d0ee:
     *   LEA EDX,[EBP-0x18](saved_pos); PUSH EDI(&actor[0x488]); PUSH EDX
     *   CALL distance_squared3d  (FUN_000121a0 = 0x000121a0)
     *   FCOMP [0x255d1c]; FNSTSW AX; TEST AH,0x41; JNZ 0x2d32a (return 1)
     * JNZ taken when: AH & 0x41 != 0 → C3|C0 set → FPU flags for <=
     *   So jump to return-1 when dist_sq <= threshold.
     *   Fall through (full repath) when dist_sq > threshold.
     */
    dist_sq_saved =
      (float)distance_squared3d(saved_pos, (float *)(actor + 0x488));
    if (dist_sq_saved <= *(float *)0x255d1c) {
      goto LAB_path_ok;
    }
    /* Destination changed significantly: fall through to full pathfinding. */
  }

  /*
   * actor_test_destination failed. Compute actual 3D distance from actor
   * position to destination, allocate path cache, and run the pathfinder.
   *
   * tag_get at 0x0002d0f7: PUSH [ESI+0x58]; PUSH 0x61637472 ('rtra'='actr')
   * FUN_0001ad60 at 0x0002d10d: PUSH EDI(&actor[0x488]); PUSH &actor[0x12c]
   *   returns float in FPU; FSTP [EBP-0x8] -> dist
   * game_time_get at 0x0002d12c: no args -> current game tick
   * Confirmed at 0x0002d131: MOV [EBX+4],EAX (path slot timestamp)
   */
  tag = (char *)tag_get(0x61637472, *(int *)(actor + 0x58));
  dist =
    (float)FUN_0001ad60((float *)(actor + 0x12c), (float *)(actor + 0x488));
  actor_handle_u = (unsigned int)actor_handle;
  game_tick = game_time_get();
  *(int *)((actor_handle_u & 0xffff) * 0x657c + *(int *)0x331f58 + 4) =
    game_tick;

  /* Select pathfinding mode: mounted (vehicle) vs on-foot vs override. */
  if (*(char *)(actor + 0x99) != '\0') {
    /*
     * Mounted: use scenario-based vehicle pathfinding (path_3d_build_path).
     * Args confirmed at 0x0002d13e-0x0002d155:
     *   pre-push: &actor[0x4a8], &actor[0x488](EDI), 0, &actor[0x12c]
     *   scenario_get() -> push EAX
     *   CALL path_3d_build_path(scenario, &actor[0x12c], 0, &actor[0x488],
     *                     &actor[0x4a8])
     * ADD ESP,0x14 = 5 args.
     */
    path_found =
      path_3d_build_path((int)scenario_get(), (int *)(actor + 0x12c), 0,
                         (int *)(actor + 0x488), (char *)(actor + 0x4a8));
  } else if (override_path != (void *)0) {
    /*
     * Caller provided a pre-computed path override.
     * Assert: actor[0x480] (dest_object) must be NONE (-1).
     * Then set up override_path as the navigation state:
     *   FUN_0005e0d0(override_path, &actor[0x494], actor[0x498], 0)
     *   path_state_build_path(override_path, &actor[0x4a8])
     * Confirmed at 0x0002d164-0x0002d1bb.
     */
    if (*(int *)(actor + 0x480) != -1) {
      display_assert("actor->control.path.destination_orders."
                     "ignore_target_object_index == NONE",
                     "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0xbbc, 1);
      system_exit(-1);
    }
    FUN_0005e0d0(override_path, (float *)(actor + 0x494),
                 *(int *)(actor + 0x498), 0);
    path_found = path_state_build_path((unsigned int)override_path,
                                       (unsigned int *)(actor + 0x4a8));
  } else {
    /*
     * Normal on-foot pathfinding pipeline:
     *  1. actor_path_input_new(actor_handle, local_nav): initialize nav-state
     * struct (actor position, facing, vehicle info, etc.).
     *  2. paths_dispose(local_nav, actor[0x480]): if ignore_object!=-1,
     *     store it at local_nav+0xc.
     *  3. (Optional) path_input_set_attractor: encode movement-constraint
     * orders into local_nav when actor has standing orders (actor[0x280]>0,
     *     actor[0x28a]==0, tag flag bit 4 clear). Float arg 0x41200000=10.0f.
     *  4. ai_debug_get_path_storage(actor_handle): allocate/find path cache
     * slot.
     *  5. path_state_new(local_nav, large_buf, path_state): init path-build
     *     state in large_buf from local_nav and the cache slot.
     *  6. FUN_0005e0d0(large_buf, &actor[0x488], actor[0x494], actor[0x498]):
     *     set destination in path-build state.
     *  7. FUN_0005ff70(large_buf): run pathfinder; returns 1 on success.
     *  8. path_state_build_path(large_buf, &actor[0x4a8]): extract waypoint
     * result into actor nav-control struct. Returns 1 if path is usable.
     *
     * Disasm confirmed:
     *   local_nav at [EBP-0x60] (44 bytes)
     *   large_buf at [EBP+0xfffebf14] (82060 bytes = 0x1408c)
     */
    actor_path_input_new(actor_handle, local_nav);
    if (*(int *)(actor + 0x480) != -1) {
      paths_dispose(local_nav, *(int *)(actor + 0x480));
    }
    if ((*(short *)(actor + 0x280) > 0) && (*(char *)(actor + 0x28a) == '\0') &&
        ((*(unsigned char *)(tag + 4) & 0x10) == 0)) {
      path_input_set_attractor(
        local_nav, (float *)(actor + 0x2b0), *(float *)(actor + 0x294),
        *(unsigned int *)(actor + 0x28c),
        (unsigned int)0x41200000); /* 10.0f as bit pattern */
    }
    path_state = ai_debug_get_path_storage(actor_handle);
    path_state_new(local_nav, large_buf, path_state);
    FUN_0005e0d0(large_buf, (float *)(actor + 0x488), *(int *)(actor + 0x494),
                 *(int *)(actor + 0x498));
    path_found = FUN_0005ff70((unsigned int *)large_buf);
    if (path_found != '\0') {
      path_found2 = path_state_build_path((unsigned int)large_buf,
                                          (unsigned int *)(actor + 0x4a8));
      path_found = path_found2 ? '\x01' : '\0';
    }
  }

  /* Mark path-computation attempted this tick. */
  *(char *)(actor + 0x4a4) = 1;
  if (store_distance != '\0') {
    *(float *)(actor + 0x4a0) = dist;
  }

  if (path_found != '\0') {
    /*
     * Pathfinding succeeded. Hysteresis check: if the actor was already
     * moving (actor[0x4bc]>0.0f) and the new distance is less than the
     * expected move distance (dist < actor[0x498]) AND the delta is small
     * (dist - actor[0x4bc] < threshold), reset the path to avoid jitter.
     * Confirmed at 0x0002d2ad-0x0002d2f2:
     *   FLD [ESI+0x4bc]; FCOMP 0.0f; TEST AH,0x41; JNZ done
     *   FLD dist; FCOMP [ESI+0x498]; TEST AH,0x5; JP done
     *   FLD dist; FSUB [ESI+0x4bc]; FCOMP [0x253398]; TEST AH,0x5; JP done
     *   CALL FUN_0002a3a0(actor_handle)
     */
    if ((*(float *)(actor + 0x4bc) > 0.0f) &&
        (dist < *(float *)(actor + 0x498)) &&
        (dist - *(float *)(actor + 0x4bc) < *(float *)0x253398)) {
      FUN_0002a3a0(actor_handle);
    }
    return path_found;
  }

LAB_fail:
  FUN_0002a3a0(actor_handle);
  return '\0';

LAB_path_ok:
  /*
   * actor_test_destination fast-path success. At 0x2d32a the original simply
   * loads AL=1 and returns — NO side effects (does not touch actor[0x4a4] or
   * actor[0x4a0]). On this path `dist` is not yet computed (it is computed at
   * 0x2d10d, which is bypassed), so writing it would be UB. Just return 1.
   */
  return '\x01';
}

/* 0x2d350 — actor_destination_update: Update actor path state and compute
 * target destination.
 *
 * Called every tick for an actor. Has three main branches:
 *
 * 1. PATH ACTIVE (actor[0x4a8] != 0):
 *    Walks the actor's waypoint path. Each tick it checks whether the actor
 *    has reached the current path node (within 0.15 world units, 2D) or is
 *    close enough to the segment (within 0.25 units perpendicular). If so,
 *    it advances path_step_index. When path is exhausted (step_index+1 >=
 *    step_count), sets path_final_step and either calls actor_path_stop
 *    (if path_loop) or logs "fell off end of unfinished path" (debug).
 *    Then copies the current waypoint to actor[0x50c] as the movement target
 *    and computes actor[0x518] = target - actor_position. Validates that
 *    the distance is less than 1,000,000 world units ("tau ceti" guard).
 *
 * 2. MOVEMENT TYPE != 4 (no far-movement):
 *    Resets path state (has_destination=0, final_step=0, is_moving=1) and
 *    clears path_active. Returns.
 *
 * 3. MOVEMENT TYPE == 4 (far_movement, seek/flee):
 *    Computes a step 3.0 world units ahead (or behind) along the actor's
 *    facing vector. Direction sign: +1 if actor[0x5ec] <= 0.9f, else -1.
 *    Writes target offset and absolute position into actor[0x518..0x514].
 *
 * Confirmed: cdecl, 1 arg (actor_handle), void return.
 * Confirmed: EBX=actor_handle, ESI=actor_ptr, EDI=&actor[0x4a8].
 * Confirmed float constants: 0.0225f=near_sq(0.15), 0.0625f=seg_sq(0.25),
 *   0.0f=zero, 1000000.0f=tau_ceti_sq, 3.0f=step_dist, 0.9f=facing_thresh.
 */
void actor_destination_update(int actor_handle)
{
  char *actor;
  char *path_ctl;
  char exhausted;
  int step_idx;
  int step_cnt;
  int cur_off;
  int next_off;
  float cur_x, cur_y, next_x, next_y;
  float to_cur_x, to_cur_y;
  float seg_x, seg_y;
  float dot_seg_to_cur, dot_seg_facing;
  float t, perp_x, perp_y, perp_sq;
  float dist_sq;
  char name_buf[0x200];
  float *node;
  float dx, dy, dz, dist;
  int sign_val;
  float step;

  actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);

  if (*(char *)(actor + 0x4c) != '\0' && *(char *)(actor + 0x4a4) == '\0' &&
      *(char *)(actor + 0x13) == '\0') {
    actor_path_refresh(actor_handle, 0, 0);
  }

  actor_test_destination(actor_handle);

  path_ctl = actor + 0x4a8;
  if (*(char *)(actor + 0x4a8) != '\0') {
    exhausted = '\0';

    while (1) {
      step_idx = (int)*(signed char *)(actor + 0x4c2);
      step_cnt = (int)*(signed char *)(actor + 0x4c1);

      if (step_idx + 1 >= step_cnt) {
        exhausted = '\x01';
        break;
      }

      cur_off = (step_idx + 2) * 0x10;
      next_off = (step_idx + 3) * 0x10;

      cur_x = *(float *)(path_ctl + cur_off);
      cur_y = *(float *)(path_ctl + cur_off + 4);
      next_x = *(float *)(path_ctl + next_off);
      next_y = *(float *)(path_ctl + next_off + 4);

      to_cur_x = cur_x - *(float *)(actor + 0x12c);
      to_cur_y = cur_y - *(float *)(actor + 0x130);

      seg_x = next_x - cur_x;
      seg_y = next_y - cur_y;

      /* Load path_final_step flag (actor+0x506). */
      if (*(char *)(actor + 0x506) == '\0') {
        /* Check whether to use simple distance or projected segment test. */
        if (*(char *)(actor + 0x504) != '\0' &&
            *(char *)(actor + 0x507) != '\0') {
          /* Segment projection test.
           *
           * dot_seg_to_cur = dot(seg_dir, to_cur)
           * dot_seg_facing = dot(seg_dir, actor_facing)
           *
           * Skip advance if:
           *   - dot_seg_facing <= 0.0f (not facing toward next step), OR
           *   - dot_seg_to_cur >= 0.0f (actor already past current node)
           *
           * If both pass, compute perpendicular distance from actor to segment
           * and advance only if perp_dist_sq < 0.0625f.
           *
           * Disassembly verified operand order:
           *   0x2d425: FLD [EBP-0x10] (seg_y) FMUL [EBP-0x8] (to_cur_y)
           *   0x2d42b: FLD [EBP-0x14] (seg_x) FMUL [EBP-0xc] (to_cur_x)
           *   FADDP => dot_seg_to_cur = seg_y*to_cur_y + seg_x*to_cur_x
           *   0x2d433: FLD [EBP-0x10] (seg_y) FMUL [ESI+0x178] (facing_y)
           *   0x2d43f: FLD [EBP-0x14] (seg_x) FMUL [ESI+0x174] (facing_x)
           *   FADDP => dot_seg_facing = seg_y*facing_y + seg_x*facing_x
           */
          dot_seg_to_cur = seg_y * to_cur_y + seg_x * to_cur_x;
          dot_seg_facing = seg_y * *(float *)(actor + 0x178) +
                           seg_x * *(float *)(actor + 0x174);

          /* FCOMP [0x2533c0]=0.0f; TEST AH,0x41; JNZ => jump if <= 0 */
          if (dot_seg_facing <= 0.0f) {
            break;
          }
          /* FCOM [0x2533c0]=0.0f; TEST AH,0x5; JP => jump if >= 0 */
          if (dot_seg_to_cur >= 0.0f) {
            break;
          }

          /* Perpendicular distance from actor to segment line.
           * t = -dot_seg_to_cur (positive, since dot_seg_to_cur < 0)
           * perp = to_cur + t * seg_dir
           * perp_sq = perp.x^2 + perp.y^2
           *
           * Disasm 0x2d461-0x2d47b:
           *   FCHS   => t = -dot_seg_to_cur (ST0 now t)
           *   FLD seg_x; FMUL ST1 => seg_x * t
           *   FADD to_cur_x => perp_x = seg_x*t + to_cur_x
           *   FLD seg_y; FMUL ST2 => seg_y * t (ST2 = t)
           *   FADD to_cur_y => perp_y = seg_y*t + to_cur_y
           *   FLD ST0; FMUL ST1 => perp_y*perp_y
           *   FLD ST2; FMUL ST3 => perp_x*perp_x (ST3=perp_x)
           *   Wait: at 0x2d473: FLD ST0 = perp_y, FMUL ST1 = perp_y*perp_y
           *         0x2d477: FLD ST2 = perp_x, FMUL ST3 = perp_x*perp_y ... no
           *
           * Re-trace FPU stack at 0x2d461:
           *   ST0 = dot_seg_to_cur (the one from FADDP at 0x2d431)
           *   dot_seg_facing was computed 0x2d433-0x2d445, then FCOMP popped it
           *   So at 0x2d461: ST0 = dot_seg_to_cur
           *   FCHS => ST0 = t = -dot_seg_to_cur
           *   0x2d463: FLD seg_x (ST0=seg_x, ST1=t)
           *   0x2d466: FMUL ST1 => ST0 = seg_x*t; ST1=t
           *   0x2d468: FADD to_cur_x => ST0 = perp_x; ST1=t
           *   0x2d46b: FLD seg_y (ST0=seg_y, ST1=perp_x, ST2=t)
           *   0x2d46e: FMUL ST2 => ST0 = seg_y*t; ST1=perp_x; ST2=t
           *   0x2d470: FADD to_cur_y => ST0=perp_y; ST1=perp_x; ST2=t
           *   0x2d473: FLD ST0 => ST0=perp_y; ST1=perp_y; ST2=perp_x; ST3=t
           *   0x2d475: FMUL ST1 => ST0=perp_y*perp_y; ST1=perp_y; ST2=perp_x
           *   0x2d477: FLD ST2 => ST0=perp_x; ST1=perp_y*perp_y; ST2=perp_y;
           * ST3=perp_x 0x2d479: FMUL ST3 => ST0=perp_x*perp_x;
           * ST1=perp_y*perp_y 0x2d47b: FADDP => ST0=perp_x*perp_x+perp_y*perp_y
           * = perp_sq
           */
          t = -dot_seg_to_cur;
          perp_x = seg_x * t + to_cur_x;
          perp_y = seg_y * t + to_cur_y;
          perp_sq = perp_x * perp_x + perp_y * perp_y;

          /* FCOMP [0x255d90]=0.0625f; TEST AH,0x5; JP => jump if >= 0.0625f */
          if (perp_sq >= 0.0625f) {
            break;
          }
        } else {
          /* Simple 2D distance-to-current-node check.
           * Disasm 0x2d48b-0x2d499:
           *   FLD to_cur_y; FMUL to_cur_y  => to_cur_y^2
           *   FLD to_cur_x; FMUL to_cur_x  => to_cur_x^2
           *   FADDP => dist_sq
           *   FCOMP [0x255d8c]=0.0225f; TEST AH,0x5; JP => jump if >= 0.0225f
           */
          dist_sq = to_cur_y * to_cur_y + to_cur_x * to_cur_x;
          if (dist_sq >= 0.0225f) {
            break;
          }
        }
      }

      /* Advance to next step. */
      *(signed char *)(actor + 0x4c2) += 1;
      *(char *)(actor + 0x506) = '\0';
    }

    /* Handle path-exhausted or final-step state. */
    if (*(char *)(actor + 0x506) != '\0') {
      if (exhausted == '\0') {
        /* Reached the final step but loop says we shouldn't be here. */
        display_assert("final_step", "c:\\halo\\SOURCE\\ai\\actor_moving.c",
                       0xb4, 1);
        system_exit(-1);
      }

      if (*(char *)(actor + 0x4c0) != '\0') {
        /* Path has a loop/done handler — call actor_path_stop. */
        FUN_0002a3a0(actor_handle);
      } else if (*(char *)0x5aca62 != '\0') {
        /* Debug: log "fell off end of unfinished path".
         * ai_debug_describe_actor: actor_describe_name(actor_handle, -1, 1,
         * buf, 0x200) Disasm 0x2d518-0x2d529: PUSH 0x200; PUSH EDX(local_218);
         * PUSH 1; PUSH -1; PUSH EBX
         */
        ai_debug_describe_actor(actor_handle, -1, 1, name_buf, 0x200);
        error(2, "%s: fell off end of unfinished path %d/%d", name_buf,
              (int)*(signed char *)(actor + 0x4c1), 4);
      }
    }

    /* If path_active and (has_destination or not is_moving), set the
     * current target position from the path node at step_index. */
    if (*path_ctl != '\0' && (*(char *)(actor + 0x504) != '\0' ||
                              *(char *)(actor + 0x484) == '\0')) {
      *(char *)(actor + 0x504) = '\x01';

      /* Copy node position: actor[0x4c8 + step_index*0x10] → actor[0x50c].
       * Disasm 0x2d574-0x2d5a1: MOVSX EDX,byte[ESI+0x4c2]; SHL EDX,4;
       *   LEA ECX,[EDX+ESI+0x4c8]; copy 3 dwords to [ESI+0x50c].
       */
      step_idx = (int)*(signed char *)(actor + 0x4c2);
      node = (float *)(actor + 0x4c8 + step_idx * 0x10);

      *(float *)(actor + 0x50c) = node[0];
      *(float *)(actor + 0x510) = node[1];
      *(float *)(actor + 0x514) = node[2];

      /* Compute vector from actor to target. */
      *(float *)(actor + 0x518) =
        *(float *)(actor + 0x50c) - *(float *)(actor + 0x12c);
      *(float *)(actor + 0x51c) =
        *(float *)(actor + 0x510) - *(float *)(actor + 0x130);
      *(float *)(actor + 0x520) =
        *(float *)(actor + 0x514) - *(float *)(actor + 0x134);

      /* Sanity check: if distance^2 < 1,000,000 (i.e. < 1000 units), OK.
       * Disasm 0x2d5d0-0x2d605: FPU computes sqrt(dx^2+dy^2+dz^2), then
       *   FCOMP [0x255d50]=1000000.0f; TEST AH,0x1; JNZ => jump if < 1000000.
       * TEST AH,0x1 = test C0 (ST0 < mem). JNZ = jump if C0 set (sqrt <
       * 1000000). But FSQRT was done before, so we compare sqrt (distance)
       * against sqrt(1000000) = 1000? No — looking again at disasm: FSQRT at
       * 0x2d5f2 FSTP ST3 at 0x2d5f4 (saves result into ST3 slot, discards from
       * top) FSTP ST0 twice (discards remaining ST0, ST1) Then FCOMP [0x255d50]
       * at 0x2d5fa After FSTP ST3: the stack shrinks, so the FCOMP operand is
       * the distance value itself (the sqrt result stored into ST3 then brought
       * to top via the STPs). Actually:
       *   At 0x2d5d0: FLD dz -> FLD dy -> FLD dx -> FLD ST0 (=dx)
       *   0x2d5e4: FMUL ST1 => dx*dx; stack: dx*dx, dx, dy, dz
       *   0x2d5e6: FLD ST2 (=dy); FMUL ST3 (=dy) => dy*dy
       *   0x2d5ea: FADDP => dx*dx+dy*dy; stack: sum, dx, dy, dz
       *   0x2d5ec: FLD ST3 (=dz); FMUL ST4 (=dz) => dz*dz
       *   0x2d5f0: FADDP => sum+dz*dz; stack: dist_sq, dx, dy, dz
       *   0x2d5f2: FSQRT => dist; stack: dist, dx, dy, dz
       *   0x2d5f4: FSTP ST3 => ST3=dist, pops: stack: dx, dy, dist
       *     (FSTP ST3 stores ST0 into ST3 slot then pops ST0)
       *     After: ST0=dx, ST1=dy, ST2=dist, ST3=dz was at ST3
       *     Wait: FSTP STn stores ST0 into STn then pops. After FSQRT:
       *       ST0=dist, ST1=dx, ST2=dy, ST3=dz
       *     FSTP ST3: ST3 = dist, pop ST0: ST0=dx, ST1=dy, ST2=dz -> wait
       *     Actually FSTP ST3 sets ST3=ST0=dist, then increments stack pointer
       *     (pops ST0). So new stack: ST0=dx, ST1=dy, ST2=dz, ST3=dist
       *   0x2d5f6: FSTP ST0 => discard dx; ST0=dy, ST1=dz, ST2=dist
       *   0x2d5f8: FSTP ST0 => discard dy; ST0=dz, ST1=dist
       *   Hmm, but FCOMP at 0x2d5fa uses 1 operand and pops ST0.
       *   We need dist to be in ST0. Let me re-read disasm...
       *   0x2d5f4: FSTP ST3 => stores dist into position 3 (which is dz), pops
       * ST0 After: ST0=dx, ST1=dy, ST2=dz(overwritten=dist) 0x2d5f6: FSTP ST0
       * => pops ST0=dx, discards it After: ST0=dy, ST1=dist 0x2d5f8: FSTP ST0
       * => pops ST0=dy, discards it After: ST0=dist 0x2d5fa: FCOMP
       * [0x255d50]=1000000.0f => compares dist to 1000000.0f TEST AH,0x1 =>
       * test C0 (ST0<mem). JNZ => jump if dist < 1000000.0f
       *
       * So we compare distance (not distance^2) to 1,000,000. This is
       * "tau ceti" = 1 million world units (absurd distance).
       */
      dx = *(float *)(actor + 0x518);
      dy = *(float *)(actor + 0x51c);
      dz = *(float *)(actor + 0x520);
      dist = sqrtf(dx * dx + dy * dy + dz * dz);

      /* Jump past error if distance is sane (< 1,000,000 units). */
      if (dist < 1000000.0f) {
        return;
      }

      /* Insanely far target: log error and clear path. */
      error(2, "pathfinding is attempting to walk to tau ceti");
      *path_ctl = '\0';
      return;
    }
    /*
     * Fall through to the move-type evaluation (0x2d624). The path-active
     * block does NOT unconditionally return: at 0x2d556 (*path_ctl cleared,
     * e.g. by actor_path_stop) and at 0x2d56e (path active but
     * actor[0x504]==0 && actor[0x484]!=0) the original jumps to the shared
     * move-type tail at 0x2d624. The set-target block above returns on both
     * its exits, so only those two fall-through edges reach here.
     */
  }

  /*
   * Move-type evaluation (join at 0x2d624): reached when the path was never
   * active (entry 0x2d3ac), when an active path was cleared, or when an
   * active path yielded no target this tick.
   */
  if (*(short *)(actor + 0x15e) != 4) {
    /* Not far-movement: reset path destination and target state. */
    *(char *)(actor + 0x504) = '\0';
    *(char *)(actor + 0x506) = '\0';
    *(char *)(actor + 0x484) = '\x01';

    /* Re-fetch actor (second datum_get call in this branch, confirmed at
     * 0x2d6ea). */
    actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
    *(char *)(actor + 0x4a8) = '\0';
    *(char *)(actor + 0x484) = '\x01';
    *(int *)(actor + 0x4a0) = 0;
    return;
  }

  /* Movement type == 4: far_movement. Compute step along facing vector.
   *
   * actor[0x5ec]: if > 0.9f → sign=-1 (backward), else sign=+1 (forward).
   * Disasm 0x2d632-0x2d693:
   *   FLD [ESI+0x5ec]; FCOMP [0x2555d0]=0.9f; FNSTSW AX
   *   TEST AH,0x41; JNZ 0x2d649 (set AL=0); else: MOV AL,1
   *   XOR EDX,EDX; TEST AL,AL; SETZ DL   => DL=1 if AL==0, DL=0 if AL!=0
   *   LEA EDX,[EDX+EDX-1]                => EDX = DL*2 - 1
   *     if actor[0x5ec]>0.9: AL=1,DL=0 → EDX=-1
   *     if actor[0x5ec]<=0.9: AL=0,DL=1 → EDX=1
   *   FILD [EBP-0x8] (=EDX); FMUL [0x254644]=3.0f  => sign * 3.0
   */
  *(char *)(actor + 0x504) = '\x01';
  *(char *)(actor + 0x506) = '\0';

  /* FCOMP test: if actor[0x5ec] <= 0.9f → sign=+1, else sign=-1 */
  if (*(float *)(actor + 0x5ec) > 0.9f) {
    sign_val = -1;
  } else {
    sign_val = 1;
  }

  step = (float)sign_val * 3.0f;

  *(float *)(actor + 0x518) = step * *(float *)(actor + 0x174);
  *(float *)(actor + 0x51c) = step * *(float *)(actor + 0x178);
  *(float *)(actor + 0x520) = step * *(float *)(actor + 0x17c);

  *(float *)(actor + 0x50c) =
    *(float *)(actor + 0x12c) + *(float *)(actor + 0x518);
  *(float *)(actor + 0x510) =
    *(float *)(actor + 0x130) + *(float *)(actor + 0x51c);
  *(float *)(actor + 0x514) =
    *(float *)(actor + 0x134) + *(float *)(actor + 0x520);
}

/* 0x2d850 — Set actor movement to far-movement mode (move_type=4,
 * dest=param_2).
 *
 * Clears the actor's 3b8 (movement dormant flag), calls actor_set_dormant to
 * wake the actor, then checks if the actor is already in far-movement mode
 * heading to param_2. If not, sets up the movement block at +0x400..+0x417,
 * copies it to the active slot at +0x46c, and kicks off a path refresh
 * (store_distance=1). If already at the target, checks if the actor is still
 * active (+0x4c) and not sleeping (+0x4a4), and if so refreshes the path
 * (store_distance=0). Returns the result of actor_path_refresh, or 1 if
 * no refresh was needed.
 *
 * Confirmed: datum_get(0x6325a4, actor_handle) at 0x2d860.
 * Confirmed: OR EDI,-1 / MOV DI,[ESI+0x3b8] = 0xffff at 0x2d869/0x2d86d.
 * Confirmed: actor_set_dormant(actor_handle, 0) at 0x2d874.
 * Confirmed: CMP [ESI+0x46c], 4 / CMP [ESI+0x470], CX at 0x2d886-0x2d893.
 * Confirmed: MOV [ESI+0x404],CX; MOV [ESI+0x414],EDI=-1; MOV [ESI+0x402],0;
 *            MOV [ESI+0x400],4 at 0x2d8be-0x2d8da.
 * Confirmed: REP MOVSD ECX=6 from ESI=actor+0x400 to EDI=actor+0x46c at
 * 0x2d8e5. Confirmed: actor_path_refresh(actor_handle,1,0) at 0x2d8e7.
 * Confirmed: actor_path_refresh(actor_handle,0,0) at 0x2d8ab.
 * Confirmed: return 1 via MOV AL,1 at 0x2d8f6.
 */
char actor_move_to_move_position(int actor_handle, int16_t param_2)
{
  char *iVar1;
  int iVar3;
  int *puVar4;
  short *psVar5;

  iVar1 = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
  *(int16_t *)(iVar1 + 0x3b8) = -1;
  actor_set_dormant(actor_handle, 0);
  if ((*(int16_t *)(iVar1 + 0x46c) != 4) ||
      (*(int16_t *)(iVar1 + 0x470) != param_2)) {
    *(int16_t *)(iVar1 + 0x404) = param_2;
    *(int *)(iVar1 + 0x414) = -1;
    *(char *)(iVar1 + 0x402) = 0;
    *(int16_t *)(iVar1 + 0x400) = 4;
    puVar4 = (int *)(iVar1 + 0x400);
    psVar5 = (short *)(iVar1 + 0x46c);
    for (iVar3 = 6; iVar3 != 0; iVar3--) {
      *(int *)psVar5 = *puVar4;
      puVar4++;
      psVar5 += 2;
    }
    return actor_path_refresh(actor_handle, 1, 0);
  }
  if ((*(char *)(iVar1 + 0x4c) != '\0') && (*(char *)(iVar1 + 0x4a4) == '\0')) {
    return actor_path_refresh(actor_handle, 0, 0);
  }
  return 1;
}

/* 0x2d9b0 — Set actor movement to encounter-path mode (move_type=5,
 * dest=encounter_handle, dist=distance).
 *
 * Clears actor+0x3b8, wakes the actor, then checks if it is already in mode 5
 * with the same encounter handle and distance. If so, either refreshes the path
 * (store_distance=0, if actor is active/not-sleeping) or returns 1. Otherwise
 * sets up the movement block at +0x400: mode=5, encounter_handle at +0x404,
 * distance at +0x408, path node from encounter+0x110 (fallback +0x18) at
 * +0x414, copies the 24-byte block to the active slot at +0x46c, then calls
 * actor_path_refresh(store_distance=1).
 *
 * Confirmed: datum_get(0x6325a4, actor_handle) at 0x2d9c0.
 * Confirmed: actor_set_dormant(actor_handle, 0) at 0x2d9c7.
 * Confirmed: CMP [EDI],5 / CMP [ESI+0x470],ECX / FCOMP [EBP+0x10] at
 * 0x2d9e1-0x2d9f8. Confirmed: datum_get(0x5ab23c, encounter_handle) at 0x2da2c.
 * Confirmed: encounter+0x110 fallback to encounter+0x18 at 0x2da5c-0x2da6d.
 * Confirmed: REP MOVSD ECX=6 from actor+0x400 to actor+0x46c at 0x2da83.
 * Confirmed: actor_path_refresh(actor_handle,1,0) at 0x2da85.
 * Confirmed: actor_path_refresh(actor_handle,0,0) at 0x2da1c.
 * Confirmed: return 1 via MOV AL,1 at 0x2da94.
 */
char actor_move_to_prop(int actor_handle, int encounter_handle, float distance)
{
  char *actor;
  char *encounter;
  int node_handle;
  int *active_state;
  int *pending_state;

  actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
  *(int16_t *)(actor + 0x3b8) = -1;
  actor_set_dormant(actor_handle, 0);
  active_state = (int *)(actor + 0x46c);
  if (*(int16_t *)active_state == 5 &&
      *(int *)(actor + 0x470) == encounter_handle &&
      *(float *)(actor + 0x474) == distance) {
    if (*(char *)(actor + 0x4c) == 0) {
      return 1;
    }
    if (*(char *)(actor + 0x4a4) != 0) {
      return 1;
    }
    return actor_path_refresh(actor_handle, 0, 0);
  }
  encounter = (char *)datum_get(*(data_t **)0x5ab23c, encounter_handle);
  *(int *)(actor + 0x404) = encounter_handle;
  *(int16_t *)(actor + 0x400) = 5;
  *(char *)(actor + 0x402) = 0;
  *(float *)(actor + 0x408) = distance;
  node_handle = *(int *)(encounter + 0x110) == -1 ? *(int *)(encounter + 0x18) :
                                                    *(int *)(encounter + 0x110);
  *(int *)(actor + 0x414) = node_handle;
  pending_state = (int *)(actor + 0x400);
  for (node_handle = 6; node_handle != 0; node_handle--) {
    *active_state++ = *pending_state++;
  }
  return actor_path_refresh(actor_handle, 1, 0);
}
