#include "x87_math.h"

/* 0x2a3a0 — Reset actor path/movement state. Clears the path-active flag,
 * sets is_moving to 1, and zeroes the path step counter. */
void actor_path_clear(int actor_handle)
{
  actor_t *actor;

  actor = (actor_t *)datum_get(halo_actor_data_global, actor_handle);
  actor->field_4a8 = 0;
  actor->field_484 = 1;
  actor->field_4a0 = 0;
}

/* actor_path_input_new (0x2a470) — Populate nav state for actor movement.
 *
 * Gets the actor's actr tag speed value (tag+0x8c) as the base speed.
 * If the actor is in a vehicle (actor->vehicle_count at +0x15e > 0):
 *   - Gets the vehicle object via object_get_and_verify_type(actor[0x158], 2)
 *   - Gets the vehi tag via tag_get('vehi', vehicle[0])
 *   - Overrides unit_handle with the vehicle handle (actor[0x158])
 *   - If the vehicle tag's movement scalar at +0x38c exceeds the default, it overrides the actor's base speed.
 * Calls actor_find_pathfinding_location(actor_handle), then fills nav_state_out
 * via path_input_new and path_input_set_start.
 *
 * Confirmed: datum_get + tag_get('actr', actor[0x58]) at 0x2a481-0x2a491.
 * Confirmed: tag[0x8c] → speed; actor[0x18] → unit_handle default.
 * Confirmed: object_get_and_verify_type(actor[0x158], 2) → vehicle at 0x2a4b8.
 * Confirmed: tag_get('vehi', vehicle[0]) at 0x2a4c5.
 * Confirmed: unit_handle = actor[0x158] at 0x2a4ca.
 * Confirmed: FPU FCOMP [0x2533c0] with TEST AH,0x41 at 0x2a4db.
 * Confirmed: actor_find_pathfinding_location(actor_handle) at 0x2a4f2.
 * Confirmed: path_input_new(nav, speed, actor[0x376], unit) at 0x2a509.
 * Confirmed: path_input_set_start(nav, actor+0x168, actor[0x164]) at 0x2a51d.
 */
void actor_path_input_new(int actor_handle, char *nav_state_out)
{
  char *actor;
  char *p;
  float speed;
  int unit_handle;

  actor = (char *)datum_get(halo_actor_data_global_void, actor_handle);
  p = (char *)tag_get(0x61637472, ((actor_t *)actor)->field_058);
  speed = *(float *)(p + 0x8c);
  unit_handle = ((actor_t *)actor)->field_018;
  if (((actor_t *)actor)->field_15e > 0) {
    p = (char *)object_get_and_verify_type(((actor_t *)actor)->field_158, 2);
    p = (char *)tag_get(0x76656869, *(int *)p);
    unit_handle = ((actor_t *)actor)->field_158;
    if (*(float *)(p + 0x38c) > *(float *)0x2533c0) {
      speed = *(float *)(p + 0x38c);
    }
  }
  actor_find_pathfinding_location(actor_handle);
  path_input_new(nav_state_out, *(uint32_t *)&speed,
                 ((actor_t *)actor)->field_376, unit_handle);
  path_input_set_start(nav_state_out, (float *)(actor + 0x168),
                       ((actor_t *)actor)->field_164);
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
#if defined(_MSC_VER) && !defined(__clang__)
  double acos(double x);
  return (float)acos((double)x);
#else
  return acosf(x);
#endif
}

/* midpoint3d (0x2a540) — Compute the midpoint of two 3D vectors.
 * out[i] = (a[i] + b[i]) * 0.5f for i in {0,1,2}. */
float *midpoint3d(float *a, float *b, float *out)
{
  out[0] = (a[0] + b[0]) * *(const float *)0x253398;
  out[1] = (a[1] + b[1]) * *(const float *)0x253398;
  out[2] = (a[2] + b[2]) * *(const float *)0x253398;
  return out;
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

  actor = (char *)datum_get(halo_actor_data_global, actor_handle);
  if (((actor_t *)actor)->field_46c == 0 ||
      ((actor_t *)actor)->field_46c == 1) {
    ((actor_t *)actor)->field_484 = 1;
  } else {
    tol = actor_destination_tolerance(actor_handle);
    dx = ((actor_t *)actor)->field_488 - ((actor_t *)actor)->field_12c;
    dy = ((actor_t *)actor)->field_48c - ((actor_t *)actor)->field_130;
    dz = ((actor_t *)actor)->field_490 - ((actor_t *)actor)->field_134;
    if ((dx * dx + dz * dz) + dy * dy < tol * tol) {
      ((actor_t *)actor)->field_484 = 1;
    }
  }
  return ((actor_t *)actor)->field_484;
}

static __inline float dot_product3d(const real_vector3d *a, const real_vector3d *b)
{
  return a->i * b->i + a->j * b->j + a->k * b->k;
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

  actor = (char *)datum_get(halo_actor_data_global, actor_handle);
  max_speed = *(float *)0x255960;
  current_speed = 0.0f;
  turn_decel = 0.016666668f;
  brake_decel = 0.026666667f;

  vehicle_handle = ((actor_t *)actor)->field_158;
  if (vehicle_handle != -1) {
    vehicle_count = (int)((actor_t *)actor)->field_15e;
    if (vehicle_count >= 2 && vehicle_count <= 3) {
      /* seated in vehicle: object_get_and_verify_type asserts, no NULL check */
      obj = (char *)object_get_and_verify_type(vehicle_handle, 2);
      tag = (char *)tag_get(0x76656869, *(int *)obj);
      /* disasm 0x2a688: FLD [ESI+0x20]*[ESI+0x2c], FLD [ESI+0x1c]*[ESI+0x28],
       * FADDP, FLD [ESI+0x18]*[ESI+0x24], FADDP */
      current_speed = dot_product3d((real_vector3d *)(obj + 0x18),
                                    (real_vector3d *)(obj + 0x24));
      max_speed = *(float *)(tag + 0x2f8);
      turn_decel = *(float *)(tag + 0x300);
      brake_decel = turn_decel;
    }
  } else {
    /* no vehicle: check unit handle */
    unit_handle = ((actor_t *)actor)->field_018;
    if (unit_handle != -1) {
      obj = (char *)object_try_and_get_and_verify_type(unit_handle, 1);
      if (obj != NULL) {
        /* tag_get called before dot product in disasm, ECX=tag at 0x2a6f1 */
        tag = (char *)tag_get(0x62697064, *(int *)obj);
        /* dot product: velocity [+0x18,+0x1c,+0x20] . facing
         * [+0x24,+0x28,+0x2c] disasm at 0x2a6eb: FLD [ESI+0x20]*[ESI+0x2c], FLD
         * [ESI+0x1c]*[ESI+0x28], FADDP, FLD [ESI+0x24]*[ESI+0x18], FADDP */
        current_speed = dot_product3d((real_vector3d *)(obj + 0x18),
                                      (real_vector3d *)(obj + 0x24));
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
bool actor_move_animation_impulse(int actor_handle, int16_t param_2,
                                  int *param_3)
{
  char *actor;
  char *actor2;
  char result;

  actor = (char *)datum_get(halo_actor_data_global, actor_handle);
  result = 0;
  actor_set_dormant(actor_handle, 0);
  actor2 = (char *)datum_get(halo_actor_data_global, actor_handle);
  if (*(int16_t *)(actor2 + 0x418) == -1) {
    if (*(int *)(actor2 + 0x18) == -1 ||
        !unit_is_busy(*(int *)(actor2 + 0x18))) {
      ((actor_t *)actor)->field_418 = param_2;
      ((actor_t *)actor)->field_41c = *param_3;
      ((actor_t *)actor)->field_420 = param_3[1];
      result = 1;
    }
  }
  return result;
}

/* 0x2a860 — Clear actor destination and trigger flee movement.
 * Returns 0 if actor has a goal slot, is in a flying vehicle, or
 * actor_action_deny_transition returns true. Otherwise zeroes the swarm flag,
 * copies 12 bytes from the global pointer at 0x31fc38, calls
 * actor_unit_control_stop_animation_impulse, and returns 1. */
bool actor_move_force_stop(int actor_handle)
{
  char *actor;
  char *actor2;
  bool result;

  actor = (char *)datum_get(halo_actor_data_global, actor_handle);
  result = 0;
  actor2 = (char *)datum_get(halo_actor_data_global, actor_handle);
  if (*(int16_t *)(actor2 + 0x418) == -1) {
    if (*(int *)(actor2 + 0x18) == -1 ||
        !unit_is_busy(*(int *)(actor2 + 0x18))) {
      if (!actor_action_deny_transition(actor_handle)) {
        ((actor_t *)actor)->field_504 = 0;
        *(real_vector3d *)(actor + 0x6e0) =
            *(const real_vector3d *)halo_global_zero_vector_ptr;
        actor_unit_control_stop_animation_impulse(actor_handle);
        result = 1;
      }
    }
  }
  return result;
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
 *   out_flag       = EBP+0x18 optional char* out (set to used_bsp_test)
 *   result         = EBP+0x1c >=28-byte collision/path result buffer
 *                    (asserted non-NULL; structure_test_pill2d fills 7 dwords)
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
 * Confirmed: 9-arg structure_test_pill2d(scenario_get(), (u8)actor[0x376],
 * (float*)actor[0x168], actor[0x164], &local_target, -1, actr_tag[0x8c], 0,
 * result) at 0x2a9bf — Ghidra mis-grouped the 8 pushes onto the inner
 * zero-arg scenario_get(); cleanup ADD ESP,0x24=36=9 cdecl args proves it.
 * Confirmed: when that returns 0, set found=1; height_delta=result[3]-actor[0x134];
 * keep found=1 iff (height_delta <= scale*0.5) && (param_4 != 0.0 ||
 * scale*-0.5 <= height_delta) (FCOMP polarity at 0x2a9d3-0x2aa1d).
 * Confirmed: clearance fallback only when param_4 > 0.0 (0x2aa1f-0x2aa2d).
 *   origin   = ((actor[0x120]+actor[0x12c])*0.5, (actor[0x124]+actor[0x130])
 *               *0.5, (actor[0x128]+actor[0x134])*0.5)
 *   direction= (scale*evasion[0], scale*evasion[1], 0.0)
 * Confirmed: collision_bsp_test_vector(3, global_collision_bsp_get(), 0, 0,
 * &origin, &direction, 0x7f7fffff(FLT_MAX), col_result) at 0x2aaad.
 * Miss (0) => found=1,
 * used_bsp_test=1; if param_4 < FLT_MAX, build a half-clearance segment via
 * vector3d_scale_add(&origin, &direction, 1.0, seg_a) and
 * FUN_00012fb0(*(float**)0x31fc50, param_4, seg_b), retest; a hit there
 * clears found back to 0 (0x2aab9-0x2ab24).
 * Confirmed: out_flag NULL-checked at 0x2ab2b; *out_flag=used_bsp_test. */
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
  collision_bsp_test_vector_result_t col_result;
  float height_delta;
  char found;
  char used_bsp_test;

  actor = (char *)datum_get(halo_actor_data_global, actor_handle);
  actr_tag = (char *)tag_get(0x61637472, ((actor_t *)actor)->field_058);
  found = 0;
  used_bsp_test = 0;

  if (evasion_vector == (float *)0x0 || result == (void *)0x0) {
    display_assert("evasion_vector && result",
                   "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0x484, 1);
    system_exit(-1);
  }

  if (((actor_t *)actor)->field_099 == '\0') {
    local_target[0] = scale * evasion_vector[0] + ((actor_t *)actor)->field_12c;
    local_target[1] = scale * evasion_vector[1] + ((actor_t *)actor)->field_130;
    actor_find_pathfinding_location(actor_handle);
    if (structure_test_pill2d((int)scenario_get(), ((actor_t *)actor)->field_376,
                     (float *)(actor + 0x168), ((actor_t *)actor)->field_164,
                     local_target, 0xffffffff, *(float *)(actr_tag + 0x8c), 0,
                     (unsigned int *)result) == '\0') {
      found = 1;
      height_delta = *(float *)((char *)result + 0xc) - ((actor_t *)actor)->field_134;
      if (height_delta > scale * *(float *)0x253398) {
        found = 0;
      } else if (param_4 != *(float *)0x2533c0 ||
                 !(height_delta < scale * *(float *)0x255964)) {
        goto done;
      } else {
        found = 0;
      }
    }
    if (param_4 > *(float *)0x2533c0) {
      bsp = (int)global_collision_bsp_get();
      origin[0] =
        (((actor_t *)actor)->field_120 + ((actor_t *)actor)->field_12c) *
        *(float *)0x253398;
      origin[1] =
        (((actor_t *)actor)->field_124 + ((actor_t *)actor)->field_130) *
        *(float *)0x253398;
      origin[2] =
        (((actor_t *)actor)->field_128 + ((actor_t *)actor)->field_134) *
        *(float *)0x253398;
      direction[0] = scale * evasion_vector[0];
      direction[1] = scale * evasion_vector[1];
      direction[2] = 0.0f;
      if (collision_bsp_test_vector(3, bsp, 0, 0, (int)origin, (int)direction,
                                    3.4028235e+38f, (float *)&col_result) == '\0') {
        found = 1;
        used_bsp_test = 1;
        if (param_4 < *(float *)0x2548fc) {
          vector3d_scale_add(origin, direction, 1.0f, seg_a);
          FUN_00012fb0(*(float **)0x31fc50, param_4, seg_b);
          if (collision_bsp_test_vector(3, bsp, 0, 0, (int)seg_a, (int)seg_b,
                                        3.4028235e+38f, (float *)&col_result) == '\0') {
            found = 0;
          }
        }
      }
    }
  }

done:
  if (out_flag != (char *)0x0) {
    *out_flag = used_bsp_test;
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
  volatile short count;
  short attempt;
  short index;

  datum_get(halo_actor_data_global, actor_handle);

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
    *(int *)&evade_dir[1] = *(int *)&alignment_vector[0];
    evade_dir[0] = -alignment_vector[1];
    break;
  case 1:
    *(int *)&evade_dir[0] = *(int *)&alignment_vector[1];
    evade_dir[1] = -alignment_vector[0];
    break;
  case 2:
    evade_dir[1] = alignment_vector[1];
    *(int *)&evade_dir[0] = *(int *)&alignment_vector[0];
    break;
  case 3:
    evade_dir[0] = -alignment_vector[0];
    evade_dir[1] = -alignment_vector[1];
    break;
  case 4:
    if (random_seed_step((unsigned int *)get_global_random_seed_address()) >
        0x8000) {
      *(int *)&evade_dir[1] = *(int *)&alignment_vector[0];
      evade_dir[0] = -alignment_vector[1];
      index = 0;
      count = 2;
    } else {
      *(int *)&evade_dir[0] = *(int *)&alignment_vector[1];
      evade_dir[1] = -alignment_vector[0];
      index = 1;
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
                                        param_5, out_flag, result) != 0) {
        *evade_direction_reference = index;
        return 1;
      }
      attempt++;
      evade_dir[0] = -evade_dir[0];
      index ^= 1;
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
 * actor_type_swarm_aim_jump and clears actor[0x530]. If not mounted but jump-aim is
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
 * Confirmed: datum_get(halo_actor_data_global, actor_handle) at 0x2acf0.
 * Confirmed: CMP [ESI+0x158],-1 at 0x2acf7-0x2ad03.
 * Confirmed: TEST [ESI+0x6],AL / JZ at 0x2ad09-0x2ad0e.
 * Confirmed: actor_type_swarm_aim_jump(actor_handle, a2, param_4, param_5) at 0x2ad1d.
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
bool actor_aim_jump(int actor_handle, int a2, char param_3, float param_4,
                    float *param_5)
{
  char *actor;
  char cVar3;
  float magnitude;
  float p0, p1, p2;

  actor = (char *)datum_get(halo_actor_data_global, actor_handle);
  if (((actor_t *)actor)->field_158 == -1) {
    if (((actor_t *)actor)->field_006 != 0) {
      actor_type_swarm_aim_jump(actor_handle, a2, param_4, param_5);
      ((actor_t *)actor)->field_530 = 0;
      return 1;
    }
    if (((actor_t *)actor)->field_530 != 0) {
      if (((actor_t *)actor)->state_action == 10 &&
          *(int16_t *)(actor + 0xa0) == 3) {
        cVar3 = 1;
      } else {
        cVar3 = param_3;
      }
      p2 = *(float *)(actor + 0x540);
      p1 = *(float *)(actor + 0x538) * *(float *)(actor + 0x53c);
      p0 = *(float *)(actor + 0x534) * *(float *)(actor + 0x53c);
      param_5[0] = p0;
      param_5[1] = p1;
      param_5[2] = p2;
      magnitude = x87_sqrt(param_5[0] * param_5[0] + p1 * p1 + p2 * p2);
      if (cVar3 == 0) {
        if (magnitude > param_4) {
          FUN_00012fb0(param_5, param_4 / magnitude,
                       param_5); /* dup-args-ok: in-place scale matches
                                    confirmed call above. */
        }
      }
    }
  }
  ((actor_t *)actor)->field_530 = 0;
  return 1;
}

/* 0x2ade0 — FUN_0002ade0: scan nearby objects and record an
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
 * Confirmed: object_get_bounding_sphere writes center[3] (&[EBP-0x20]) + obj_radius
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
void actor_move_avoidance_setup(int actor_handle)
{
  int handles[2048];
  float world_matrix[13];
  float out[3];
  int object_handle;
  float center[3];
  int16_t sphere_index;
  float dx, dy;
  float obj_radius;
  int left;
  int *scan;
  float maxdist;
  void *obj;
  void *obj_tag;
  int *coll_count;
  int16_t found;
  float scale_term;
  float kbase;
  void *matrix;
  unsigned short *marker;
  int16_t record_count;
  vector_avoidance_data_t *avoidance_data;

  avoidance_data = (vector_avoidance_data_t *)actor_handle;
  obj = object_get_and_verify_type(avoidance_data->object_index, -1);

  /* disasm 0x2adfc: FLD [0x2557f0]; FCOMP [0x255778]; JNZ loads 778 — i.e.
   * kbase = max(7f0, 778).  Written with the > leaf taken (load 7f0) so VC71
   * lays the branch out as FCOMP+JNE rather than the <= min-idiom JP. */
  if (*(float *)0x2557f0 > *(float *)0x255778) {
    kbase = *(float *)0x2557f0;
  } else {
    kbase = *(float *)0x255778;
  }
  found = object_find_in_radius(1, 0xc2, (char *)obj + 0x48,
                                (float *)&avoidance_data->origin,
                                kbase * avoidance_data->avoid_distance,
                                handles, 0x800);
  avoidance_data->avoidance_object_count = 0;

  /* 0x2ae5e: movzx count; cursor walks handles[]; dec/jnz, not an index. */
  if (found > 0) {
    scan = handles;
    left = (int)(unsigned short)found;
    do {
      object_handle = *scan;
      obj = object_get_and_verify_type(object_handle, -1);
      if (object_handle != -1 &&
          object_handle != avoidance_data->object_index) {
        obj_tag = tag_get(0x6f626a65, *(int *)obj); /* 'obje' */
        coll_count =
            (int *)((char *)tag_get(0x636f6c6c,
                                    *(int *)((char *)obj_tag + 0x7c)) +
                    0x280); /* 'coll' */
        if (*coll_count > 0) {
          /* 0x2aecc: maxdist home is written 0 before the sphere call. */
          maxdist = 0.0f;
          object_get_bounding_sphere(object_handle, center, &obj_radius);
          object_get_world_matrix(object_handle, world_matrix);

          /* disasm 0x2af81-0x2af8a: sphere_index is 16-bit (INC EAX, then
           * MOVSX EAX,AX before the compare against *coll_count). */
          for (sphere_index = 0; sphere_index < *coll_count; sphere_index++) {
            marker =
                (unsigned short *)tag_block_get_element(coll_count,
                                                        sphere_index, 0x20);
            /* disasm 0x2af07: CMP AX,0xffff; JZ — node!=0xffff is fall-through. */
            if (*marker != 0xffff) {
              matrix = object_get_node_matrix(object_handle, (int16_t)*marker);
              matrix_transform_point((float *)matrix,
                                     (float *)((char *)marker + 0x10), out);
              scale_term = *(float *)((char *)marker + 0x1c) * *(float *)matrix;
            } else {
              matrix_transform_point(world_matrix,
                                     (float *)((char *)marker + 0x10), out);
              scale_term = world_matrix[0] * *(float *)((char *)marker + 0x1c);
            }
            dx = out[0] - center[0];
            dy = out[1] - center[1];
            scale_term = sqrtf(dx * dx + dy * dy) + scale_term;
            /* disasm 0x2af69: FLD maxdist; FCOMP scale_term; JZ keeps maxdist. */
            if (maxdist > scale_term) {
            } else {
              maxdist = scale_term;
            }
          }

          record_count = avoidance_data->avoidance_object_count;
          if (record_count < MAXIMUM_NUMBER_OF_AVOIDANCE_OBJECTS) {
            avoidance_data->avoidance_object_count = record_count + 1;
            avoidance_data->avoidance_objects[record_count].width = maxdist;
            avoidance_data->avoidance_objects[record_count].base =
                *(real_point3d *)center;
            avoidance_data->avoidance_objects[record_count].object_index =
                object_handle;
            avoidance_data->avoidance_objects[record_count].base.z -=
                obj_radius - maxdist;
            scale_term = (obj_radius + obj_radius) - (maxdist + maxdist);
            /* disasm 0x2afe4: FLD [0x2533c0]; FCOMP scale_term; JNZ keeps
             * scale_term — i.e. max(const, scale_term). */
            avoidance_data->avoidance_objects[record_count].height =
                0.0f > scale_term ? 0.0f : scale_term;
          }
        }
      }
      scan += 1;
      left -= 1;
    } while (left != 0);
  }
}

/* 0x2b020 — FUN_0002b020: transform an avoidance ray by the
 * avoidance_data's per-instance world matrix, then cast it against the BSP and
 * a list of obstacle spheres to find the nearest collision time.
 *
 * Register args (confirmed from caller actor_move_vector_avoidance @ 0x2c01f-0x2c02b and
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
 * transformed direction (local origin pheight_delta=avoidance_data+0xc), second the
 * world-space ray_origin/ray_direction.  Ghidra truncates the 2nd call to 4
 * args; disasm 0x2b256-0x2b265 shows the full 8.
 * Confirmed: obstacle loop calls pill_test_vector3d (truncated to 3 args by Ghidra;
 * disasm 0x2b29d-0x2b2b3 shows 7) writing the per-obstacle collision time into
 * a scratch float (EBP-0x4) that is compared against *collision_t.  This
 * scratch is distinct from avoidance_ray[6], which Ghidra aliases as the same
 * slot. */
static __forceinline real_vector3d *vector_from_points3d(
    const real_point3d *a,
    const real_point3d *b,
    real_vector3d *result)
{
  result->i = b->x - a->x;
  result->j = b->y - a->y;
  result->k = b->z - a->z;
  return result;
}

static __forceinline void actor_move_transform_avoidance_vector_inline(
    const vector_avoidance_data_t *avoidance_data,
    const real_vector3d *avoidance_vector,
    real_vector3d *direction_vector)
{
  float component;

  *direction_vector = *(const real_vector3d *)halo_global_zero_vector_ptr;

  component = avoidance_vector->i;
  direction_vector->i += component * avoidance_data->forward.i;
  direction_vector->j += component * avoidance_data->forward.j;
  direction_vector->k += component * avoidance_data->forward.k;

  component = avoidance_vector->j;
  direction_vector->i += component * avoidance_data->left.i;
  direction_vector->j += component * avoidance_data->left.j;
  direction_vector->k += component * avoidance_data->left.k;

  component = avoidance_vector->k;
  direction_vector->i += component * avoidance_data->up.i;
  direction_vector->j += component * avoidance_data->up.j;
  direction_vector->k += component * avoidance_data->up.k;
}

/* 0x2b020 — actor_move_test_avoidance_vector: test a candidate avoidance ray against
 * the collision BSP and nearby vehicle avoidance obstacles.
 * Grounded against PAL source (source/ai/actor_moving.c:827).
 */
short actor_move_test_avoidance_vector(float *avoidance_ray_raw, float *ray_origin_raw, int avoidance_data_raw,
                                       float *ray_direction_raw, float *collision_t, char *param_3)
{
  collision_bsp_test_vector_result_t collision;
  real_vector3d offset;
  real_vector3d divergence;
  float scale;
  float object_t;
  short result;
  short object_index;
  const vector_avoidance_ray_t *avoidance_ray;
  real_point3d *ray_origin;
  vector_avoidance_data_t *avoidance_data;
  real_vector3d *ray_direction;
  uint8_t *collision_timer;

  result = 0; /* _actor_vector_avoidance_clear */
  avoidance_ray = (const vector_avoidance_ray_t *)avoidance_ray_raw;
  ray_origin = (real_point3d *)ray_origin_raw;
  avoidance_data = (vector_avoidance_data_t *)avoidance_data_raw;
  ray_direction = (real_vector3d *)ray_direction_raw;
  collision_timer = (uint8_t *)param_3;

  if (avoidance_data == NULL || avoidance_ray == NULL) {
    display_assert("avoidance_data && avoidance_ray",
                   "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0x7f7, 1);
    system_exit(-1);
  }
  if (ray_origin == NULL || ray_direction == NULL) {
    display_assert("ray_origin && ray_direction",
                   "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0x7f8, 1);
    system_exit(-1);
  }
  if (collision_t == NULL) {
    display_assert("collision_t", "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0x7f9, 1);
    system_exit(-1);
  }

  *collision_t = 3.4028235e+38f;

  actor_move_transform_avoidance_vector_inline(avoidance_data, &avoidance_ray->offset, &offset);
  actor_move_transform_avoidance_vector_inline(avoidance_data, &avoidance_ray->divergence, &divergence);

  ray_origin->x = offset.i * avoidance_data->avoid_width + avoidance_data->origin.x;
  ray_origin->y = offset.j * avoidance_data->avoid_width + avoidance_data->origin.y;
  ray_origin->z = offset.k * avoidance_data->avoid_width + avoidance_data->origin.z;

  scale = avoidance_data->avoid_distance * avoidance_ray->length;
  ray_direction->i = divergence.i * scale;
  ray_direction->j = divergence.j * scale;
  ray_direction->k = divergence.k * scale;

  vector_from_points3d(&avoidance_data->origin, ray_origin, &offset);
  if (collision_bsp_test_vector(3, (int)avoidance_data->bsp, 0, 0,
                                (int)&avoidance_data->origin, (int)&offset, 1.0f,
                                (float *)&collision)) {
    result = 2; /* _actor_vector_avoidance_obstructed_structure */
    *collision_t = 0.0f;
  } else if (collision_bsp_test_vector(3, (int)avoidance_data->bsp, 0, 0,
                                       (int)ray_origin, (int)ray_direction, 1.0f,
                                       (float *)&collision)) {
    result = 2; /* _actor_vector_avoidance_obstructed_structure */
    *collision_t = collision.t;
  }

  for (object_index = 0; object_index < avoidance_data->avoidance_object_count; object_index++) {
    vehicle_avoidance_cylinder_t *cylinder;

    cylinder = &avoidance_data->avoidance_objects[object_index];
    if (pill_test_vector3d((float *)&cylinder->base, cylinder->height, cylinder->width,
                           (float *)ray_origin, (float *)ray_direction, &object_t, (float *)&offset) &&
        object_t < *collision_t) {
      result = 1; /* _actor_vector_avoidance_obstructed_object */
      *collision_t = object_t;
    }
  }

  if (collision_timer != NULL) {
    if (result > 0) {
      *collision_timer = 0;
    } else if (*collision_timer < 255) {
      *collision_timer = (uint8_t)(*collision_timer + 1);
    }
  }

  return result;
}

/* 0x2b310 — FUN_0002b310: locate the angular sector of the
 * query direction within a fan of direction records and linearly interpolate a
 * fractional index plus an associated value.
 *
 * Register args (confirmed from caller actor_move_vector_avoidance @ 0x2c4c7-0x2c4cf and
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
char actor_move_vector_avoidance_find_direction(float *direction, short count, int records, float *values,
                  float *out_index, float *out_value)
{
  const real_vector3d *directions;
  const real_vector3d *direction_vector;
  short previous_index;
  float previous_cross;
  short direction_index;
  float cross;
  int denom_idx;

  directions = (const real_vector3d *)records;
  direction_vector = (const real_vector3d *)direction;
  previous_index = count - 1;
  previous_cross =
    directions[previous_index].j * direction_vector->k -
    directions[previous_index].k * direction_vector->j;

  for (direction_index = 0; direction_index < count; direction_index++) {
    cross =
      directions[direction_index].j * direction_vector->k -
      directions[direction_index].k * direction_vector->j;

    if (previous_cross * cross <= 0.0f &&
        directions[direction_index].k * direction_vector->k +
        directions[direction_index].j * direction_vector->j +
        direction_vector->i * directions[direction_index].i > 0.0f) {
      if (direction_index == 0) {
        denom_idx = (int)count;
      } else {
        denom_idx = (int)direction_index;
      }
      *out_index =
        ((float)(int)previous_index * cross - (float)denom_idx * previous_cross) /
        (cross - previous_cross);
      *out_value =
        (cross * values[previous_index] - previous_cross * values[direction_index]) /
        (cross - previous_cross);
      return 1;
    }

    previous_index = direction_index;
    previous_cross = cross;
  }

  return 0;
}

/* 0x2b400 — actor_move_transform_avoidance_vector: transform a local-space
 * direction vector (in_vec) by the avoidance_data's per-instance 3x3 rotation
 * (forward/left/up rows at +0x18/+0x24/+0x30), starting from the vector read
 * through the global pointer at *0x31fc38 (halo_global_zero_vector_ptr).
 *
 * cdecl, all stack args (confirmed: PUSH EBP / RET, no RET N; caller
 * ai_debug_render_actor @ 0x4d39c-0x4d39d pushes EAX=[EDI+0x1a0] last).
 *   matrix   : per-instance struct, 3x3 rotation rows at +0x18..+0x38
 *   in_vec   : float[3] local-space direction
 *   out_vec  : float[3] world-space output
 *
 * Confirmed (disasm 0x2b406-0x2b41c): out_vec is seeded by a 12-byte copy
 * through the once-dereferenced global pointer at *0x31fc38, NOT the raw
 * global address.  Confirmed three interleaved read-modify-write passes (one
 * per in_vec component) with operand order in_vec[c] * mtx[col] + out[r]; the
 * body is the shared actor_move_transform_avoidance_vector_inline.
 */
void actor_move_transform_avoidance_vector(int matrix, float *in_vec,
                                           float *out_vec)
{
  actor_move_transform_avoidance_vector_inline(
      (const vector_avoidance_data_t *)matrix,
      (const real_vector3d *)in_vec, (real_vector3d *)out_vec);
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
 * ai_debug_render_actor @ 0x4d26a-0x4d26b and 0x4d2d7-0x4d2d8).
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
  short sector;
  float frac;
  float angle0;
  float angle1;
  volatile float angle;
  float vec[3];

  /* Clamp the index into the valid [0, 8) range. */
  if (dir_index < 0.0f || dir_index >= 8.0f) {
    dir_index = 0.0f;
  }

  for (sector = 0; sector < 8; sector++) {
    if ((float)sector + 1.0f > dir_index) {
      frac = dir_index - (float)sector;
      angle0 = ((const float *)0x2557f4)[sector];
      angle1 = (sector == 7) ? *(const float *)0x2557f4 : ((const float *)0x2557f8)[sector];
      angle = angle0 * (1.0f - frac) + angle1 * frac;
      break;
    }
  }

  if (sector == 8 || angle == *(const float *)0x2548fc) {
    angle = 0.0f;
    error(2,
          "warning: actor_move_get_avoidance_vector couldn't find out-of-bounds "
          "direction %.4f",
          (double)dir_index);
  } else {
    if (!(angle >= 0.0f && angle < *(const float *)0x255a54)) {
      display_assert("(angle >= 0.0f) && (angle < _full_circle)",
                     "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0xab7, 1);
      system_exit(-1);
    }
  }

  vec[0] = 0.0f;
#if defined(_MSC_VER) && !defined(__clang__)
  vec[1] = (float)cos((double)angle);
  vec[2] = (float)sin((double)angle);
#else
  vec[1] = x87_fcos(angle);
  vec[2] = x87_fsin(angle);
#endif
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

  const float *scale_table_9 = (const float *)0x2557a8;

  const float *outer_angles = (const float *)0x25581c;
  const float *outer_scales = (const float *)0x255814;
  const float *inner_angles;

  int row;
  int col;
  float sin_angle, cos_angle, scaled_angle, sin_scaled, scaled_len;
  float sin_outer, cos_outer, row_scale;
  float cos_inner, sin_inner;

  scale_table_9 = (const float *)0x2557a8;
  row = 9;
  do {
    sin_angle = x87_fsin(scale_table_9[9]);
    cos_angle = x87_fcos(scale_table_9[9]);
    scaled_angle = *(const float *)0x255780 * scale_table_9[0];
    sin_scaled = x87_fsin(scaled_angle);
    scaled_len = *(const float *)0x25577c * scale_table_9[-9];

    p_a[0] = *(const float *)0x255778;
    p_a[1] = 0.0f;
    p_a[2] = scaled_len * cos_angle;
    p_a[3] = scaled_len * sin_angle;
    p_a[4] = x87_fcos(scaled_angle);
    p_a[5] = sin_scaled * cos_angle;
    p_a[6] = sin_scaled * sin_angle;
    p_a += 7;
    scale_table_9++;
    row = row - 1;
  } while (row != 0);

  row = 2;
  do {
    sin_outer = x87_fsin(outer_angles[0]);
    cos_outer = x87_fcos(outer_angles[0]);
    row_scale = outer_scales[0];
    outer_angles++;
    outer_scales++;

    basis = (float *)0x632780;
    p_b = outer_b;
    inner_angles = (const float *)0x2557f4;

    for (col = 8; col != 0; col--) {
      inner_angles++;
      cos_inner = x87_fcos(inner_angles[-1]);
      sin_inner = x87_fsin(inner_angles[-1]);

      basis[0] = 0.0f;
      basis[1] = cos_inner;
      basis[2] = sin_inner;

      /* table_b entry base = p_b + 3 floats:
       *   [+0] k_inner_base
       *   [+0x10] row_scale * basis[0..2]
       *   [+0x1c] sin_outer * basis[0..2], with [+0x1c] then overwritten by
       *           cos_outer (the sin_outer*basis[0] product is dead). */
      p_b[-3] = *(const float *)0x2557f0;
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
    row = row - 1;
  } while (row != 0);
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
 * Confirmed: vehi[0x388] → speed at 0x2b76f.
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
  float speed;
  float delta[3];
  char result;

  actor = (char *)datum_get(halo_actor_data_global_void, actor_handle);
  speed = 0.0f;
  result = 1;
  if (((actor_t *)actor)->field_15e == 4) {
    vehi = (char *)object_get_and_verify_type(((actor_t *)actor)->field_158, 2);
    vehi = (char *)tag_get(0x76656869, *(int *)vehi);
    speed = *(float *)(vehi + 0x388);
    if (speed > *(float *)0x2533c0 &&
        *(float *)(actor + 0x5ec) > *(float *)0x2555d0) {
      delta[0] = dest_pos[0] - ((actor_t *)actor)->field_12c;
      delta[1] = dest_pos[1] - ((actor_t *)actor)->field_130;
      delta[2] = dest_pos[2] - ((actor_t *)actor)->field_134;
      if (normalize3d(delta) > *(float *)0x2533c0 &&
          delta[0] * ((actor_t *)actor)->input_facing_vector[0] +
              delta[1] * ((actor_t *)actor)->input_facing_vector[1] +
              delta[2] * ((actor_t *)actor)->input_facing_vector[2] >
            *(float *)0x253d54) {
        result = 0;
      }
    }
  }
  if (dist_out != (float *)0) {
    *dist_out = speed;
  }
  return result;
}

/* 0x2b830 — actor_move_calculate_controlled_by_aiming: choose a desired facing vector.
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
 * Register args confirmed from caller actor_move_compute_facing @
 * 0x2dd9a-0x2ddb4: ECX = facing_basis (this+0x174), EAX = in_vec (the desired
 * vector), EDI = weight_vec (this+0x524); cdecl stack: use_3d, out_vector,
 * out_index (caller cleans 0xc bytes). */
void actor_move_calculate_controlled_by_aiming(float *facing_basis /* @<ecx> */, char use_3d,
                  float *out_vector, short *out_index,
                  float *in_vec /* @<eax> */, float *weight_vec /* @<edi> */)
{
  real_vector3d directions[4];
  short best_direction;
  float best_aim_dot;
  float best_facing_dot;
  short direction;
  float *desired_facing_vector;

  if (use_3d != 0) {
    directions[0] = *(const real_vector3d *)in_vec;
    if (normalize3d((float *)&directions[0]) == 0.0f) {
      directions[0] = *(const real_vector3d *)facing_basis;
    }
    directions[2] = *(const real_vector3d *)halo_global_zero_vector_ptr;
  } else {
    directions[0] = *(const real_vector3d *)in_vec;
    directions[0].k = 0.0f;
    if (normalize3d((float *)&directions[0]) == 0.0f) {
      directions[0] = *(const real_vector3d *)facing_basis;
    }
    directions[2].i = -directions[0].j;
    directions[2].j = directions[0].i;
    directions[2].k = 0.0f;
  }

  directions[1].i = -directions[0].i;
  directions[1].j = -directions[0].j;
  directions[1].k = -directions[0].k;
  directions[3].i = -directions[2].i;
  directions[3].j = -directions[2].j;
  directions[3].k = -directions[2].k;

  best_direction = -1;
  for (direction = 0; direction < 4; direction++) {
    const real_vector3d *direction_vector = &directions[direction];
    float aim_dot;
    float facing_dot;

    if (use_3d != 0) {
      aim_dot = (weight_vec[2] * direction_vector->k +
                 direction_vector->j * weight_vec[1]) +
                weight_vec[0] * direction_vector->i;
      facing_dot = (facing_basis[2] * direction_vector->k +
                    facing_basis[0] * direction_vector->i) +
                   direction_vector->j * facing_basis[1];
    } else {
      aim_dot = direction_vector->j * weight_vec[1] +
                weight_vec[0] * direction_vector->i;
      facing_dot = facing_basis[0] * direction_vector->i +
                   direction_vector->j * facing_basis[1];
    }

    if (best_direction == -1 ||
        (aim_dot > best_aim_dot ?
          (facing_dot > best_facing_dot || best_facing_dot < 0.5f) :
          (facing_dot > best_facing_dot && aim_dot > 0.5f))) {
      best_direction = direction;
      best_aim_dot = aim_dot;
      best_facing_dot = facing_dot;
    }
  }

  *out_index = best_direction;
  desired_facing_vector = out_vector;
  desired_facing_vector[0] = directions[best_direction].i;
  desired_facing_vector[1] = directions[best_direction].j;
  desired_facing_vector[2] = directions[best_direction].k;

  {
    float err;
    err = (desired_facing_vector[2] * desired_facing_vector[2] +
           desired_facing_vector[1] * desired_facing_vector[1] +
           desired_facing_vector[0] * desired_facing_vector[0]) -
          1.0f;
    if (((*(unsigned int *)&err & 0x7f800000) == 0x7f800000) ||
        (fabsf(err) >= *(double *)0x2549d8)) {
      display_assert(csprintf((char *)0x5ab100,
                              "%s: assert_valid_real_normal3d(%f, %f, %f)",
                              "desired_facing_vector", (double)desired_facing_vector[0],
                              (double)desired_facing_vector[1], (double)desired_facing_vector[2]),
                     "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0x764, 1);
      system_exit(-1);
    }
  }
}

/* 0x2bab0 — FUN_0002bab0: express the movement
 * direction in the local frame of the facing direction, writing the result
 * into the caller's output vector.
 *
 * Register args (confirmed from caller actor_move_compute_facing @
 * 0x2dbeb-0x2dbf7): use_3d              @<al>   : 0 selects the 2D (planar)
 * path, nonzero the full 3D path. movement_direction  @<esi>  : float[3] unit
 * movement direction. facing_direction    @<edi>  : float[3] unit facing
 * direction. out                 @<ebx>  : float[3] result frame coordinates.
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
static __inline int valid_real(float n)
{
  return (*(const unsigned long *)&n & 0x7F800000) != 0x7F800000;
}

static __inline int valid_realcmp(float a, float b)
{
  float y = a - b;
  return valid_real(y) && fabs(y) < 0.001f;
}

void actor_move_calculate_free(char use_3d /* @<al> */,
                               float *movement_direction /* @<esi> */,
                               float *facing_direction /* @<edi> */,
                               float *out /* @<ebx> */)
{
  const real_vector3d *movement;
  const real_vector3d *facing;
  real_vector3d *throttle;
  real_vector3d left;
  real_vector3d up;
  real_vector2d perpendicular;

  movement = (const real_vector3d *)movement_direction;
  facing = (const real_vector3d *)facing_direction;
  throttle = (real_vector3d *)out;

  if (use_3d != 0) {
    if (valid_real_normal3d(movement_direction) == 0) {
      display_assert(
        csprintf(error_string_buffer, "%s: assert_valid_real_normal3d(%f, %f, %f)",
                 "movement_direction", (double)movement->i,
                 (double)movement->j, (double)movement->k),
        "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0x775, 1);
      system_exit(-1);
    }
    if (valid_real_normal3d(facing_direction) == 0) {
      display_assert(
        csprintf(error_string_buffer, "%s: assert_valid_real_normal3d(%f, %f, %f)",
                 "facing_direction", (double)facing->i,
                 (double)facing->j, (double)facing->k),
        "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0x776, 1);
      system_exit(-1);
    }
    biped_build_flying_axes(facing_direction, (float *)&left, (float *)&up);
    throttle->i = dot_product3d(movement, facing);
    throttle->j = (left.j * movement->j + left.k * movement->k) + left.i * movement->i;
    throttle->k = (up.j * movement->j + up.k * movement->k) + up.i * movement->i;
    normalize3d(out);
    return;
  }

  if (valid_real_normal2d(movement_direction) == 0) {
    display_assert(
      csprintf(error_string_buffer, "%s: assert_valid_real_normal2d(%f, %f)",
               "(real_vector2d *) movement_direction",
               movement->i, movement->j),
      "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0x785, 1);
      system_exit(-1);
  }
  if (valid_real_normal2d(facing_direction) == 0) {
    display_assert(
      csprintf(error_string_buffer, "%s: assert_valid_real_normal2d(%f, %f)",
               "(real_vector2d *) facing_direction",
               facing->i, facing->j),
      "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0x786, 1);
    system_exit(-1);
  }
  if (!valid_realcmp(movement->k, 0.0f)) {
    display_assert(
      csprintf(error_string_buffer,
               "%s, %s: assert_valid_realcmp(%f, %f)",
               "movement_direction->k", "0.0f",
               movement->k, 0.0f),
      "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0x787, 1);
    system_exit(-1);
  }
  if (!valid_realcmp(facing->k, 0.0f)) {
    display_assert(
      csprintf(error_string_buffer,
               "%s, %s: assert_valid_realcmp(%f, %f)",
               "facing_direction->k", "0.0f",
               facing->k, 0.0f),
      "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0x788, 1);
    system_exit(-1);
  }
  perpendicular.i = -facing->j;
  perpendicular.j = facing->i;
  throttle->i = movement->i * facing->i + movement->j * facing->j;
  throttle->j = movement->i * perpendicular.i + movement->j * perpendicular.j;
  throttle->k = 0.0f;
  normalize3d(out);
}

/* 0x2bd80 — FUN_0002bd80: the per-tick obstacle-avoidance and
 * movement-vector resolver.  Given the actor's desired facing (avoidance
 * rotation), it builds a local avoidance state, samples avoidance rays in 8
 * directions, scores each direction, picks the best, and produces an output
 * velocity direction (vel_out) and emergency turn amount (speed_out).
 *
 * ABI (confirmed from caller actor_move_update @ 0x2e705-0x2e710):
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
#define VECTOR_AVOIDANCE_NUMBER_OF_DIRECTIONS 8
#define actor_debug_array (*(actor_debug_info_t **)0x00331f58)
#define avoidance_directions ((const real_vector3d *)0x632780)
#define sense_rays ((const vector_avoidance_ray_t *)0x6327e0)
#define avoidance_rays ((const vector_avoidance_ray_t *)0x6325c0)
#define sense_ray_avoidance_weights ((const float *)0x255828)
#define avoid_ray_avoidance_weights ((const float *)0x255948)
#define avoid_ray_adjacent_fractions ((const float *)0x255954)
#define avoid_ray_clear_bias_time 75
#define angle_between_vectors3d(a, b) FUN_0010c510((float *)(a), (float *)(b))

#define MIN_FLOAT(a, b) (((a) < (b)) ? (a) : (b))
#define MAX_FLOAT(a, b) (((a) > (b)) ? (a) : (b))
#define PIN_FLOAT(v, min_val, max_val) (((v) < (min_val)) ? (min_val) : (((v) > (max_val)) ? (max_val) : (v)))

static __inline void cross_product3d_inline(const real_vector3d *a, const real_vector3d *b, real_vector3d *out)
{
  out->i = a->j * b->k - a->k * b->j;
  out->j = a->k * b->i - a->i * b->k;
  out->k = a->i * b->j - a->j * b->i;
}

static __inline float magnitude_squared3d(const real_vector3d *v)
{
  return v->i * v->i + v->j * v->j + v->k * v->k;
}

static __inline float magnitude3d(const real_vector3d *v)
{
  return x87_sqrt(magnitude_squared3d(v));
}

static __forceinline float normalize3d_inline(real_vector3d *v)
{
  float mag = magnitude3d(v);
  if (x87_fabs(mag) >= *(const double *)0x2533d0) {
    float inv = 1.0f / mag;
    v->i *= inv;
    v->j *= inv;
    v->k *= inv;
  } else {
    mag = 0.0f;
  }
  return mag;
}

void actor_move_vector_avoidance(int actor_handle /* @<ecx> */, float *facing, float *vel_out,
                  float *speed_out)
{
  actor_t *actor;
  real_vector3d rotation;
  float emergency;
  int unit_handle;

  actor = (actor_t *)datum_get((data_t *)halo_actor_data_global_int, actor_handle);
  emergency = 0.0f;
  rotation.i = (halo_global_zero_vector_ptr)[0];
  rotation.j = (halo_global_zero_vector_ptr)[1];
  rotation.k = (halo_global_zero_vector_ptr)[2];

  unit_handle = actor->vehicle_index;
  if (unit_handle == -1) {
    unit_handle = actor->meta_unit_index;
  }

  if (vel_out == (float *)0 || speed_out == (float *)0) {
    display_assert("avoidance_rotation && emergency_amount",
                   "c:\\halo\\SOURCE\\ai\\actor_moving.c", 2183, 1);
    system_exit(-1);
  }

  if (unit_handle != -1) {
    object_datum_t *object;
    actor_debug_info_t *debug_info;
    vector_avoidance_data_t avoidance_data;
    float weights[VECTOR_AVOIDANCE_NUMBER_OF_DIRECTIONS];
    real_vector3d movement_vector;
    real_vector3d local_movement_direction;
    float maximum_sense_emergency;
    float angular_speed;
    float best_weight;
    float movement_direction_approximation;
    float movement_approximate_weight;
    float forward_dot;
    float weight_difference;
    float emergency_scale;
    short best_avoidance_direction;
    short direction_index;
    short ray_index;
    char sharp_turn;
    char direction_chosen;

    object = (object_datum_t *)object_get_and_verify_type(unit_handle, -1);
    direction_chosen = 0;
    debug_info = &actor_debug_array[actor_handle & 0xffff];
    sharp_turn = 0;

    debug_info->timestamp = game_time_get();
    avoidance_data.structure = scenario_get();
    avoidance_data.bsp = global_collision_bsp_get();
    avoidance_data.object_index = unit_handle;
    object_get_world_position(unit_handle, (vector3_t *)&avoidance_data.origin);
    avoidance_data.forward = object->forward;
    avoidance_data.up = object->up;
    cross_product3d_inline(&object->up, &object->forward, &avoidance_data.left);
    avoidance_data.avoid_distance = 12.0f;
    avoidance_data.avoid_width = 1.0f;
    actor_move_avoidance_setup((int)&avoidance_data);

    maximum_sense_emergency = 0.0f;
    csmemset(weights, 0, sizeof(weights));

    {
      short current_direction = actor->control_vector_avoidance_current_direction;

      if (current_direction >= 0 && current_direction < VECTOR_AVOIDANCE_NUMBER_OF_DIRECTIONS) {
        short next_direction = (current_direction + 1) % VECTOR_AVOIDANCE_NUMBER_OF_DIRECTIONS;
        short second_next_direction = (current_direction + 2) % VECTOR_AVOIDANCE_NUMBER_OF_DIRECTIONS;
        short previous_direction = (current_direction + VECTOR_AVOIDANCE_NUMBER_OF_DIRECTIONS - 1) % VECTOR_AVOIDANCE_NUMBER_OF_DIRECTIONS;
        short second_previous_direction = (current_direction + VECTOR_AVOIDANCE_NUMBER_OF_DIRECTIONS - 2) % VECTOR_AVOIDANCE_NUMBER_OF_DIRECTIONS;

        weights[current_direction] += 0.4f;
        weights[next_direction] += avoid_ray_adjacent_fractions[0] * 0.4f;
        weights[second_next_direction] += avoid_ray_adjacent_fractions[1] * 0.4f;
        weights[previous_direction] += avoid_ray_adjacent_fractions[0] * 0.4f;
        weights[second_previous_direction] += avoid_ray_adjacent_fractions[1] * 0.4f;
      }
    }

    for (ray_index = 0; ray_index < 9; ray_index++) {
      real_point3d ray_origin;
      real_vector3d ray_direction;
      float collision_t;
      short avoidance_type = (short)actor_move_test_avoidance_vector(
        (float *)&sense_rays[ray_index],
        (float *)&ray_origin,
        (int)&avoidance_data,
        (float *)&ray_direction,
        &collision_t,
        NULL);

      debug_info->ray_origin[ray_index] = ray_origin;
      debug_info->ray_direction[ray_index] = ray_direction;
      debug_info->avoidance_type[ray_index] = avoidance_type;
      debug_info->collision_t[ray_index] = collision_t;
      if (avoidance_type > 0) {
        float sense_emergency = 1.0f - collision_t;
        float sense_weight = 2.0f * sense_emergency;

        for (direction_index = 0;
             direction_index < VECTOR_AVOIDANCE_NUMBER_OF_DIRECTIONS;
             direction_index++) {
          weights[direction_index] +=
            sense_ray_avoidance_weights[ray_index * VECTOR_AVOIDANCE_NUMBER_OF_DIRECTIONS + direction_index] *
            MIN_FLOAT(sense_weight, 1.0f);
        }
        maximum_sense_emergency = MAX_FLOAT(maximum_sense_emergency, sense_emergency);
      }
    }

    for (direction_index = 0;
         direction_index < VECTOR_AVOIDANCE_NUMBER_OF_DIRECTIONS;
         direction_index++) {
      short avoidance_types[2];
      float avoidance_t[2];
      float direction_weight;
      char obstructed;

      for (ray_index = 0; ray_index < 2; ray_index++) {
        real_point3d ray_origin;
        real_vector3d ray_direction;

        avoidance_types[ray_index] = (short)actor_move_test_avoidance_vector(
          (float *)&avoidance_rays[direction_index * 2 + ray_index],
          (float *)&ray_origin,
          (int)&avoidance_data,
          (float *)&ray_direction,
          &avoidance_t[ray_index],
          (char *)&actor->control_vector_avoidance_clear_times[direction_index][ray_index]);
        debug_info->probe_origin[direction_index][ray_index] = ray_origin;
        debug_info->probe_dir[direction_index][ray_index] = ray_direction;
        debug_info->avoidance_types_2[direction_index][ray_index] = avoidance_types[ray_index];
        debug_info->avoid_t[direction_index][ray_index] = avoidance_t[ray_index];
      }

      direction_weight = 0.0f;
      obstructed = 0;

      for (ray_index = 2 - 1; ray_index >= 0; ray_index--) {
        if (avoidance_types[ray_index] == 0) {
          float clear_fraction = 1.0f;

          if (!obstructed) {
            unsigned char clear_time = actor->control_vector_avoidance_clear_times[direction_index][ray_index];

            if (clear_time < avoid_ray_clear_bias_time) {
              clear_fraction = 0.0f;
            } else {
              clear_fraction = PIN_FLOAT(
                1.0f - (float)avoid_ray_clear_bias_time / (float)clear_time,
                0.0f,
                1.0f);
            }
          }
          direction_weight += avoid_ray_avoidance_weights[ray_index] * clear_fraction;
        } else {
          direction_weight -=
            avoid_ray_avoidance_weights[ray_index] * MIN_FLOAT(2.0f * (1.0f - avoidance_t[ray_index]), 1.0f);
          obstructed = 1;
        }
      }

      {
        short next_direction = (direction_index + 1) % VECTOR_AVOIDANCE_NUMBER_OF_DIRECTIONS;
        short second_next_direction = (direction_index + 2) % VECTOR_AVOIDANCE_NUMBER_OF_DIRECTIONS;
        short previous_direction = (direction_index + VECTOR_AVOIDANCE_NUMBER_OF_DIRECTIONS - 1) % VECTOR_AVOIDANCE_NUMBER_OF_DIRECTIONS;
        short second_previous_direction = (direction_index + VECTOR_AVOIDANCE_NUMBER_OF_DIRECTIONS - 2) % VECTOR_AVOIDANCE_NUMBER_OF_DIRECTIONS;

        weights[direction_index] += direction_weight;
        weights[next_direction] += avoid_ray_adjacent_fractions[0] * direction_weight;
        weights[second_next_direction] += avoid_ray_adjacent_fractions[1] * direction_weight;
        weights[previous_direction] += avoid_ray_adjacent_fractions[0] * direction_weight;
        weights[second_previous_direction] += avoid_ray_adjacent_fractions[1] * direction_weight;
      }
    }

    angular_speed = magnitude3d(&object->angular_velocity);
    debug_info->has_emergency_velocity = 0;
    if (angular_speed > 0.02f) {
      float velocity_weight = MIN_FLOAT((angular_speed - 0.02f) * 12.5f, 1.0f) * 0.8f;
      real_vector3d velocity_direction;
      float velocity_approximate_direction;
      float velocity_approximate_weight = 0.0f;

      velocity_direction.i = 0.0f;
      velocity_direction.j = dot_product3d(&avoidance_data.up, &object->angular_velocity);
      velocity_direction.k = -dot_product3d(&avoidance_data.left, &object->angular_velocity);
      if (normalize3d_inline(&velocity_direction) > 0.0f &&
          actor_move_vector_avoidance_find_direction(
            (float *)&velocity_direction,
            VECTOR_AVOIDANCE_NUMBER_OF_DIRECTIONS,
            (int)avoidance_directions,
            weights,
            &velocity_approximate_direction,
            &velocity_approximate_weight) &&
          velocity_approximate_weight > 0.5f) {
        for (direction_index = 0;
             direction_index < VECTOR_AVOIDANCE_NUMBER_OF_DIRECTIONS;
             direction_index++) {
          float velocity_dot =
            dot_product3d(&avoidance_directions[direction_index], &velocity_direction);

          if (velocity_dot < 0.0f) {
            weights[direction_index] += velocity_dot * velocity_weight;
          }
        }
      } else {
        velocity_weight = 0.0f;
      }

      debug_info->velocity_weight = velocity_weight;
      debug_info->angular_speed = angular_speed;
      debug_info->has_emergency_velocity = 1;
      debug_info->avoidance_vector = velocity_direction;
      debug_info->velocity_approximate_weight = velocity_approximate_weight;
    }

    best_weight = *(const float *)0x255c98;
    best_avoidance_direction = -1;
    for (direction_index = 0;
         direction_index < VECTOR_AVOIDANCE_NUMBER_OF_DIRECTIONS;
         direction_index++) {
      if (weights[direction_index] > best_weight) {
        best_weight = weights[direction_index];
        best_avoidance_direction = direction_index;
      }
    }
    if (best_avoidance_direction < 0 || best_avoidance_direction >= VECTOR_AVOIDANCE_NUMBER_OF_DIRECTIONS) {
      display_assert(
        "(best_avoidance_direction >= 0) && (best_avoidance_direction < VECTOR_AVOIDANCE_NUMBER_OF_DIRECTIONS)",
        "c:\\halo\\SOURCE\\ai\\actor_moving.c", 2435, 1);
      system_exit(-1);
    }
    csmemcpy(debug_info->weights, weights, sizeof(weights));

    movement_vector = *(const real_vector3d *)facing;
    forward_dot = 1.0f;
    local_movement_direction.i = 0.0f;
    local_movement_direction.j = 0.0f;
    local_movement_direction.k = 0.0f;
    movement_direction_approximation = 0.0f;
    movement_approximate_weight = 0.0f;
    if (normalize3d_inline(&movement_vector) > 0.0f) {
      local_movement_direction.i = 0.0f;
      forward_dot = dot_product3d(&avoidance_data.forward, &movement_vector);
      local_movement_direction.j = dot_product3d(&avoidance_data.left, &movement_vector);
      local_movement_direction.k = dot_product3d(&avoidance_data.up, &movement_vector);
      if (normalize3d_inline(&local_movement_direction) > 0.0f) {
        actor_move_vector_avoidance_find_direction(
          (float *)&local_movement_direction,
          VECTOR_AVOIDANCE_NUMBER_OF_DIRECTIONS,
          (int)avoidance_directions,
          weights,
          &movement_direction_approximation,
          &movement_approximate_weight);
        if (movement_direction_approximation >= 0.0f &&
            movement_direction_approximation <= *(const float *)0x253f78) {
        } else {
          display_assert(
            "(movement_direction_approximation >= 0) && (movement_direction_approximation <= ((real) VECTOR_AVOIDANCE_NUMBER_OF_DIRECTIONS))",
            "c:\\halo\\SOURCE\\ai\\actor_moving.c", 2466, 1);
          system_exit(-1);
        }
      }
    }

    weight_difference = best_weight - movement_approximate_weight;
    debug_info->forward = avoidance_data.forward;
    debug_info->requested_facing = *(const real_vector3d *)facing;
    debug_info->movement_direction_approximation = movement_direction_approximation;
    debug_info->best_weight = best_weight;
    debug_info->best_avoidance_direction = best_avoidance_direction;
    debug_info->movement_approximate_weight = movement_approximate_weight;
    if (maximum_sense_emergency > 0.6f) {
      emergency_scale = 1.0f + MIN_FLOAT(1.0f, (maximum_sense_emergency - 0.6f) / (1.0f - 0.6f));
    } else {
      emergency_scale = MIN_FLOAT(1.0f, maximum_sense_emergency / 0.3f);
    }
    debug_info->forward_dot = forward_dot;
    debug_info->sign_no_danger = weight_difference;

    if (forward_dot < -0.2f) {
      if (actor->control_vector_avoidance_sharp_turn_timer != -1 &&
          actor->control_vector_avoidance_sharp_turn_timer < 90) {
        debug_info->debug_mode = 7;
        sharp_turn = 1;
      } else if (magnitude_squared3d(&object->angular_velocity) > 0.05f * 0.05f) {
        if (weight_difference > 2.0f && best_weight > 2.0f) {
          debug_info->debug_mode = 6;
          sharp_turn = 1;
        }
      } else if (emergency_scale > 0.5f) {
        debug_info->debug_mode = 5;
        sharp_turn = 1;
      }
    }

    if (sharp_turn) {
      real_vector3d best_direction;
      real_vector3d rotation_axis;

      if (actor->control_vector_avoidance_sharp_turn_timer == -1) {
        actor->control_vector_avoidance_sharp_turn_timer = 0;
      } else {
        actor->control_vector_avoidance_sharp_turn_timer++;
      }

      actor_move_transform_avoidance_vector(
        (int)&avoidance_data,
        (float *)&avoidance_directions[best_avoidance_direction],
        (float *)&best_direction);
      rotation_axis.i = best_direction.k * ((const real_vector3d *)facing)->j - best_direction.j * ((const real_vector3d *)facing)->k;
      rotation_axis.j = best_direction.i * ((const real_vector3d *)facing)->k - best_direction.k * ((const real_vector3d *)facing)->i;
      rotation_axis.k = best_direction.j * ((const real_vector3d *)facing)->i - best_direction.i * ((const real_vector3d *)facing)->j;
      if (normalize3d_inline(&rotation_axis) > 0.0f) {
        float rotation_angle = angle_between_vectors3d(facing, &best_direction);

        rotation.i = rotation_axis.i * rotation_angle;
        rotation.j = rotation_axis.j * rotation_angle;
        rotation.k = rotation_axis.k * rotation_angle;
      }

      emergency = PIN_FLOAT((2.0f - movement_approximate_weight) * 0.5f - 0.5f, 0.0f, 1.0f);
      emergency = MAX_FLOAT(emergency, emergency_scale);
      direction_chosen = 1;
    } else {
      actor->control_vector_avoidance_sharp_turn_timer = -1;
      if (forward_dot < 0.5f) {
        if (weight_difference > 1.3f) {
          float direction_dot = dot_product3d(
            &avoidance_directions[best_avoidance_direction],
            &local_movement_direction);

          if (direction_dot > 0.5f) {
            float rotation_angle;

            emergency = PIN_FLOAT(weight_difference / 1.3f - 0.5f, 0.0f, 1.0f);
            emergency = MAX_FLOAT(emergency, emergency_scale);
            rotation_angle = emergency * (3.14159265f / 3.0f);
            if (local_movement_direction.k * avoidance_directions[best_avoidance_direction].j -
                local_movement_direction.j * avoidance_directions[best_avoidance_direction].k > 0.0f) {
              rotation_angle = -rotation_angle;
            }
            rotation.i = avoidance_data.forward.i * rotation_angle;
            rotation.j = avoidance_data.forward.j * rotation_angle;
            rotation.k = avoidance_data.forward.k * rotation_angle;
            direction_chosen = 1;
            debug_info->debug_mode = 4;
            debug_info->sign_rotated = rotation_angle;
          } else {
            debug_info->sign_too_far_cosangle = direction_dot;
            debug_info->debug_mode = 3;
          }
        } else {
          debug_info->debug_mode = 2;
        }
      } else if (maximum_sense_emergency > 0.0f) {
        real_vector3d perpendicular;
        float rotation_angle = 0.0f;

        emergency = emergency_scale;

        rotation.i = 0.0f;
        rotation.j = 0.0f;
        rotation.k = 0.0f;
        perpendicular.j = -avoidance_directions[best_avoidance_direction].k;
        rotation.i += perpendicular.j * avoidance_data.left.i;
        rotation.j += perpendicular.j * avoidance_data.left.j;
        rotation.k += perpendicular.j * avoidance_data.left.k;
        perpendicular.k = avoidance_directions[best_avoidance_direction].j;
        rotation.i += perpendicular.k * avoidance_data.up.i;
        rotation.j += perpendicular.k * avoidance_data.up.j;
        rotation.k += perpendicular.k * avoidance_data.up.k;
        if (normalize3d_inline(&rotation) > 0.0f) {
          rotation_angle = emergency * (3.14159265f / 3.0f);
          rotation.i *= rotation_angle;
          rotation.j *= rotation_angle;
          rotation.k *= rotation_angle;
        }
        debug_info->rotation_angle = rotation_angle;
        direction_chosen = 1;
        debug_info->debug_mode = 1;
        debug_info->maximum_sense_emergency = maximum_sense_emergency;
      } else {
        debug_info->debug_mode = 0;
      }
    }

    debug_info->direction_chosen = direction_chosen;
    if (direction_chosen) {
      actor->control_vector_avoidance_current_direction = best_avoidance_direction;
    } else {
      actor->control_vector_avoidance_current_direction = -1;
    }
    {
      /* Dword copy, not struct assignment: clang lowers a 0x6048-byte struct
       * assignment to a memcpy call the original does not make (VC71 emits
       * rep movsd for either form). */
      const uint32_t *src;
      uint32_t *dst;
      int count;

      src = (const uint32_t *)&avoidance_data;
      dst = (uint32_t *)&debug_info->avoidance_data;
      for (count = sizeof(avoidance_data) / sizeof(uint32_t); count != 0; count--) {
        *dst = *src;
        src++;
        dst++;
      }
    }
    debug_info->emergency = emergency;
    debug_info->rotation = rotation;
  }

  vel_out[0] = rotation.i;
  vel_out[1] = rotation.j;
  vel_out[2] = rotation.k;
  *speed_out = emergency;
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
  char
    local_nav[0x48]; /* [EBP-0x60]: nav-state struct (waypoint init output).
                      * MUST be >= 0x48: actor_path_input_new -> path_input_new
                      * does csmemset(buf, 0, 0x48). Original reserved
                      * -0x60..-0x18 (0x48 bytes); a too-small [44] overflowed
                      * and zeroed the cached actor+0x4a8 nav_state_out pointer
                      * -> NULL write in path_state_build_path -> PoA campaign
                      * access-violation. */
  char
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
  actor = (char *)datum_get(halo_actor_data_global, actor_handle);
  move_src = ((actor_t *)actor)->field_46c;
  had_path = 0;

  /* If move_src != 0 and != 1, save old destination and set had_path. */
  if (move_src != 0 && move_src != 1) {
    saved_pos[0] = ((actor_t *)actor)->field_488;
    saved_pos[1] = ((actor_t *)actor)->field_48c;
    saved_pos[2] = ((actor_t *)actor)->field_490;
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
  if (((actor_t *)actor)->field_160 != '\0' || move_src == 0 || move_src == 1 ||
      (move_src == 3 && ((actor_t *)actor)->field_3bb != '\0')) {
    /* Second datum_get at 0x0002d305 */
    actor = (char *)datum_get(halo_actor_data_global, actor_handle);
    *(int *)(actor + 0x4a0) = 0;
    ((actor_t *)actor)->field_4a8 = 0;
    ((actor_t *)actor)->field_484 = 1;
    return '\x01';
  }

  /* Clear navigation state fields for this tick. */
  ((actor_t *)actor)->field_4a8 = 0;
  ((actor_t *)actor)->field_484 = 0;
  *(int *)(actor + 0x4a0) = 0;
  ((actor_t *)actor)->field_506 = 0;

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
    if (((actor_t *)actor)->field_034 == 0xffffffff) {
      goto LAB_fail;
    }
    ai_idx = (int)(((actor_t *)actor)->field_034 & 0xffff);
    scenario = (int)global_scenario_get();
    squad_elem =
      (int)tag_block_get_element((void *)(scenario + 0x42c), ai_idx, 0xb0);
    order_elem = (int)tag_block_get_element(
      (void *)(squad_elem + 0x98), (int)(short)((actor_t *)actor)->field_470,
      0x18);
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
    if (((actor_t *)actor)->field_034 == 0xffffffff) {
      goto LAB_fail;
    }
    ai_idx = (int)(((actor_t *)actor)->field_034 & 0xffff);
    scenario = (int)global_scenario_get();
    squad_elem =
      (int)tag_block_get_element((void *)(scenario + 0x42c), ai_idx, 0xb0);
    order_elem = (int)tag_block_get_element(
      (void *)(squad_elem + 0x80), (int)(short)((actor_t *)actor)->field_03a,
      0xe8);
    order_idx = ((actor_t *)actor)->field_470;
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
    prop = (int)datum_get(prop_data, *(int *)(actor + 0x470));
    if ((*(short *)(prop + 0x24) < 4) || (*(short *)(prop + 0x24) > 5)) {
      /* Prop state invalid: notify and continue (don't abort). */
      actor_perception_find_prop_pathfinding_location(actor_handle,
                                                      *(int *)(actor + 0x470));
    }
    if (((actor_t *)actor)->field_099 != '\0') {
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
  if (((actor_t *)actor)->field_099 != '\0') {
    path_found =
      actor_path_3d_available(actor_handle, (float *)(actor + 0x488), &dist);
    if (path_found == '\0') {
      goto LAB_fail;
    }
  } else {
    if (*(float *)(actor + 0x498) == 0.0f) {
      path_found = (char)(((actor_t *)actor)->field_494 != -1);
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
     *   CALL distance_squared3d  (distance_squared3d = 0x000121a0)
     *   FCOMP [0x255d1c]; FNSTSW AX; TEST AH,0x41; JNZ 0x2d32a (return 1)
     * JNZ taken when: AH & 0x41 != 0 → C3|C0 set → FPU flags for <=
     *   So jump to return-1 when dist_sq <= threshold.
     *   Fall through (full repath) when dist_sq > threshold.
     */
    dist_sq_saved =
      (float)distance_squared3d(saved_pos, (float *)(actor + 0x488));
    if (!(dist_sq_saved > *(float *)0x255d1c)) {
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
  tag = (char *)tag_get(0x61637472, ((actor_t *)actor)->field_058);
  dist =
    (float)FUN_0001ad60((float *)(actor + 0x12c), (float *)(actor + 0x488));
  actor_handle_u = (unsigned int)actor_handle;
  game_tick = game_time_get();
  *(int *)((actor_handle_u & 0xffff) * 0x657c + *(int *)0x331f58 + 4) =
    game_tick;

  /* Select pathfinding mode: mounted (vehicle) vs on-foot vs override. */
  if (((actor_t *)actor)->field_099 != '\0') {
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
     *   FUN_0005e0d0(override_path, &actor[0x488], actor[0x494], actor[0x498])
     *   path_state_build_path(override_path, &actor[0x4a8])
     * Confirmed at 0x0002d194: MOV ECX,[ESI+0x498]; MOV EDX,[ESI+0x494];
     *   PUSH ECX; PUSH EDX; PUSH EDI(&actor[0x488]); PUSH EBX(override_path);
     *   CALL FUN_0005e0d0 -> args (override_path, &actor[0x488], actor[0x494],
     *   actor[0x498]) — same destination/facing setup as the on-foot branch,
     *   only the path-build buffer differs (override_path vs large_buf). The
     *   prior lift passed (override_path, &actor[0x494], actor[0x498], 0),
     *   which seeded the override path-build state with the FACING field as
     *   the destination -> path_state_build_path failed -> scripted a10 door
     *   grunts could not advance to their firing positions.
     */
    if (((actor_t *)actor)
          ->control_path_destination_orders_ignore_target_object_index != -1) {
      display_assert("actor->control.path.destination_orders."
                     "ignore_target_object_index == NONE",
                     "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0xbbc, 1);
      system_exit(-1);
    }
    FUN_0005e0d0(override_path, (float *)(actor + 0x488),
                 ((actor_t *)actor)->field_494, *(int *)(actor + 0x498));
    path_found = path_state_build_path((unsigned int)override_path,
                                       (unsigned int *)(actor + 0x4a8));
  } else {
    /*
     * Normal on-foot pathfinding pipeline:
     *  1. actor_path_input_new(actor_handle, local_nav): initialize nav-state
     * struct (actor position, facing, vehicle info, etc.).
     *  2. path_input_set_target_object(local_nav, actor[0x480]): if ignore_object!=-1,
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
     *  7. FUN_0005ff70(large_buf): init heap_count=1, seed start node, traverse.
     *     Original CALL at 0x2d266 is 0x5ff70, not path_state_traverse
     *     (0x5f740) — that skip leaves heap_count==0 (path.c:1394).
     *  8. path_state_build_path(large_buf, &actor[0x4a8]): extract waypoint
     * result into actor nav-control struct. Returns 1 if path is usable.
     *
     * Disasm confirmed:
     *   local_nav at [EBP-0x60] (44 bytes)
     *   large_buf at [EBP+0xfffebf14] (82060 bytes = 0x1408c)
     */
    actor_path_input_new(actor_handle, local_nav);
    if (((actor_t *)actor)
          ->control_path_destination_orders_ignore_target_object_index != -1) {
      path_input_set_target_object(
        local_nav,
        ((actor_t *)actor)
          ->control_path_destination_orders_ignore_target_object_index);
    }
    if ((((actor_t *)actor)->danger_zone_danger_type > 0) &&
        (((actor_t *)actor)->field_28a == '\0') &&
        ((*(unsigned char *)(tag + 4) & 0x10) == 0)) {
      path_input_set_attractor(
        local_nav, (float *)(actor + 0x2b0), ((actor_t *)actor)->field_294,
        *(unsigned int *)(actor + 0x28c),
        10.0f); /* original PUSHes bits 0x41200000 = 10.0f attractor weight.
                   param_5 is float: an (unsigned int) cast here would do an
                   int->float NUMERIC conversion (1.09e9), not a
                   bit-reinterpret, corrupting the A* attractor weight ->
                   path.c:1005 cost assert. */
    }
    path_state = ai_debug_get_path_storage(actor_handle);
    path_state_new(local_nav, large_buf, path_state);
    FUN_0005e0d0(large_buf, (float *)(actor + 0x488),
                 ((actor_t *)actor)->field_494, *(int *)(actor + 0x498));
    path_found = FUN_0005ff70((unsigned int *)large_buf);
    if (path_found != '\0') {
      path_found2 = path_state_build_path((unsigned int)large_buf,
                                          (unsigned int *)(actor + 0x4a8));
      path_found = path_found2 ? '\x01' : '\0';
    }
  }

  /* Mark path-computation attempted this tick. */
  ((actor_t *)actor)->field_4a4 = 1;
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
     *   CALL actor_path_clear(actor_handle)
     */
    if ((((actor_t *)actor)->field_4bc > 0.0f) &&
        (dist < *(float *)(actor + 0x498)) &&
        (dist - ((actor_t *)actor)->field_4bc < *(float *)0x253398)) {
      actor_path_clear(actor_handle);
    }
    return path_found;
  }

LAB_fail:
  actor_path_clear(actor_handle);
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
  char step_idx;
  bool is_facing;
  char step_reached;
  float *cur;
  float *nxt;
  real_vector2d to_step;
  real_vector2d step_vector;
  float dot_seg_to_cur, dot_seg_facing;
  double t;
  float perp_x, perp_y, perp_sq;
  float dist_sq;
  char name_buf[0x200];
  real_point3d *position;
  real_point3d *target;
  real_vector3d *delta;
  float step;

  actor = (char *)datum_get(halo_actor_data_global, actor_handle);

  if (((actor_t *)actor)->field_04c != '\0' &&
      ((actor_t *)actor)->field_4a4 == '\0' &&
      ((actor_t *)actor)->field_013 == '\0') {
    actor_path_refresh(actor_handle, 0, 0);
  }

  actor_test_destination(actor_handle);

  path_ctl = actor + 0x4a8;
  if (((actor_t *)actor)->field_4a8 != '\0') {
    exhausted = '\0';

    do {
      step_idx = path_ctl[0x1a];
      step_reached = '\0';

      if (step_idx + 1 < (int)(signed char)path_ctl[0x19]) {
        cur = (float *)(path_ctl + (step_idx + 2) * 0x10);
        nxt = (float *)(path_ctl + (step_idx + 3) * 0x10);

        to_step.i = cur[0] - ((actor_t *)actor)->field_12c;
        to_step.j = cur[1] - ((actor_t *)actor)->field_130;

        step_vector.i = nxt[0] - cur[0];
        step_vector.j = nxt[1] - cur[1];

        if (((actor_t *)actor)->field_506 != '\0') {
          step_reached = '\x01';
        } else if (((actor_t *)actor)->field_504 != '\0' &&
                   ((actor_t *)actor)->field_507 != '\0') {
          /* Segment projection test: advance only when facing along the
           * step (dot > 0), already past the current node (dot < 0), and
           * within 0.25 units of the segment line (perp_sq < 0.0625f).
           * Compares are unordered-taken (FCOMP + TEST AH,0x41/0x5). */
          dot_seg_to_cur = step_vector.j * to_step.j + step_vector.i * to_step.i;
          dot_seg_facing =
            step_vector.j * ((actor_t *)actor)->input_facing_vector[1] +
            step_vector.i * ((actor_t *)actor)->input_facing_vector[0];

          if (dot_seg_facing > *(const float *)0x2533c0 &&
              dot_seg_to_cur < *(const float *)0x2533c0) {
            t = -dot_seg_to_cur;
            perp_x = step_vector.i * t + to_step.i;
            perp_y = step_vector.j * t + to_step.j;
            perp_sq = perp_x * perp_x + perp_y * perp_y;
            step_reached = perp_sq < *(const float *)0x255d90;
          }
        } else {
          /* Simple 2D distance-to-current-node check (0.15^2 = 0.0225f). */
          dist_sq = to_step.j * to_step.j + to_step.i * to_step.i;
          step_reached = dist_sq < *(const float *)0x255d8c;
        }

        if (step_reached) {
          path_ctl[0x1a] = step_idx + 1;
          ((actor_t *)actor)->field_506 = '\0';
        }
      } else {
        exhausted = '\x01';
      }
    } while (step_reached);

    /* Handle path-exhausted or final-step state. */
    if (((actor_t *)actor)->field_506 != '\0') {
      if (exhausted == '\0') {
        /* Reached the final step but loop says we shouldn't be here. */
        display_assert("final_step", "c:\\halo\\SOURCE\\ai\\actor_moving.c",
                       0xb4, 1);
        system_exit(-1);
      }

      if (((actor_t *)actor)->field_4c0 != '\0') {
        /* Path has a loop/done handler — call actor_path_stop. */
        actor_path_clear(actor_handle);
      } else if (*(char *)0x5aca62 != '\0') {
        /* Debug: log "fell off end of unfinished path".
         * ai_debug_describe_actor: actor_describe_name(actor_handle, -1, 1,
         * buf, 0x200) Disasm 0x2d518-0x2d529: PUSH 0x200; PUSH EDX(local_218);
         * PUSH 1; PUSH -1; PUSH EBX
         */
        ai_debug_describe_actor(actor_handle, -1, 1, name_buf, 0x200);
        error(2, "%s: fell off end of unfinished path %d/%d", name_buf,
              (int)((actor_t *)actor)->field_4c1, 4);
      }
    }

    /* If path_active and (has_destination or not is_moving), set the
     * current target position from the path node at step_index. */
    if (*path_ctl != '\0' && (((actor_t *)actor)->field_504 != '\0' ||
                              ((actor_t *)actor)->field_484 == '\0')) {
      ((actor_t *)actor)->field_504 = '\x01';

      /* Copy node position: actor[0x4c8 + step_index*0x10] → actor[0x50c].
       * Disasm 0x2d574-0x2d5a1: MOVSX EDX,byte[ESI+0x4c2]; SHL EDX,4;
       *   LEA ECX,[EDX+ESI+0x4c8]; copy 3 dwords to [ESI+0x50c].
       */
      target = (real_point3d *)(actor + 0x50c);
      *target = *(real_point3d *)(actor + 0x4c8 +
                                  ((actor_t *)actor)->field_4c2 * 0x10);

      /* Compute vector from actor to target. */
      position = (real_point3d *)(actor + 0x12c);
      delta = (real_vector3d *)(actor + 0x518);
      delta->i = target->x - position->x;
      delta->j = target->y - position->y;
      delta->k = target->z - position->z;

      /* "Tau ceti" guard: the distance (not its square) to the target is
       * compared against 1,000,000 world units (0x255d50).
       * FCOMP [0x255d50]; TEST AH,0x1; JNE => !(dist >= 1e6) (unordered-taken). */
      if (!(sqrtf(delta->i * delta->i + delta->j * delta->j +
                  delta->k * delta->k) >= *(const float *)0x255d50)) {
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
  if (((actor_t *)actor)->field_15e == 4) {
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
    is_facing = *(float *)(actor + 0x5ec) > *(const float *)0x2555d0;
    ((actor_t *)actor)->field_504 = '\x01';
    ((actor_t *)actor)->field_506 = '\0';
    step = (float)(is_facing ? -1 : 1) * 3.0f;

    delta = (real_vector3d *)(actor + 0x518);
    delta->i = step * ((actor_t *)actor)->input_facing_vector[0];
    delta->j = step * ((actor_t *)actor)->input_facing_vector[1];
    delta->k = step * ((actor_t *)actor)->input_facing_vector[2];

    position = (real_point3d *)(actor + 0x12c);
    target = (real_point3d *)(actor + 0x50c);
    target->x = position->x + delta->i;
    target->y = position->y + delta->j;
    target->z = position->z + delta->k;
  } else {
    /* Not far-movement: reset path destination and target state. */
    ((actor_t *)actor)->field_504 = '\0';
    ((actor_t *)actor)->field_506 = '\0';
    ((actor_t *)actor)->field_484 = '\x01';

    /* Re-fetch actor (second datum_get call in this branch, confirmed at
     * 0x2d6ea). */
    actor = (char *)datum_get(halo_actor_data_global, actor_handle);
    ((actor_t *)actor)->field_4a8 = '\0';
    ((actor_t *)actor)->field_484 = '\x01';
    *(int *)(actor + 0x4a0) = 0;
  }
}

/* 0x2d720 — actor_move_to_point: Set actor movement to point-movement mode
 * (move_type=2, dest=destination xyz, dest_node=param_3, override=param_4).
 *
 * Clears the actor's +0x3b8 movement flag (0xffff), wakes the actor via
 * actor_set_dormant, then checks if the actor is already in mode 2 heading to
 * the same destination node (+0x47c == param_3) and within the close-enough
 * threshold (squared distance from active dest at +0x470..+0x478 to the
 * requested destination <= *0x255d1c). If so and the actor is still active
 * (+0x4c) and not sleeping (+0x4a4), refreshes the path (store_distance=0);
 * otherwise returns 1. If not already at the target, sets up the movement
 * block at +0x400 (mode=2, dest xyz at +0x404, dest_node at +0x410,
 * override at +0x414), copies the 24-byte block to the active slot at +0x46c,
 * and kicks off a path refresh (store_distance=1).
 *
 * Confirmed: datum_get(*0x6325a4, actor_handle) at 0x2d730.
 * Confirmed: NULL-destination assert("destination", file, 0x3b7, 1) +
 *            system_exit(-1) at 0x2d752-0x2d759.
 * Confirmed: MOV [ESI+0x3b8],0xffff at 0x2d764.
 * Confirmed: actor_set_dormant(actor_handle, 0) at 0x2d76d.
 * Confirmed: CMP [ESI+0x46c],2 / CMP [ESI+0x47c],param_3 at 0x2d77b-0x2d787.
 * Confirmed: FLD/FSUB squared-distance vs FCOMP [0x255d1c]; TEST AH,0x41 =
 *            dist_sq <= threshold at 0x2d78c-0x2d7c7.
 * Confirmed: setup stores at 0x2d7f2-0x2d820 (offsets 0x402,0x400,0x404,
 *            0x408,0x40c,0x410,0x414).
 * Confirmed: REP MOVSD ECX=6 from actor+0x400 to actor+0x46c at 0x2d837.
 * Confirmed: actor_path_refresh(actor_handle,1,0) at 0x2d839.
 * Confirmed: actor_path_refresh(actor_handle,0,0) at 0x2d7df.
 * Confirmed: return 1 via MOV AL,1 at 0x2d848.
 */
char actor_move_to_point(int actor_handle, float *destination, int param_3,
                         int param_4)
{
  actor_t *actor;
  float dx;
  float dy;
  float dz;
  char result;

  actor = (actor_t *)datum_get(halo_actor_data_global, actor_handle);
  if (!destination) {
    display_assert("destination", "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0x3b7, 1);
    system_exit(-1);
  }
  actor->firing_positions_current_position_index = -1;
  actor_set_dormant(actor_handle, 0);
  result = 1;
  if (actor->active_destination.mode != 2 ||
      actor->active_destination.raw.surface_index != param_3 ||
      ((dx = actor->active_destination.raw.point.x - destination[0]),
       (dy = actor->active_destination.raw.point.y - destination[1]),
       (dz = actor->active_destination.raw.point.z - destination[2]),
       (dx * dx + dy * dy + dz * dz) > 0.01f)) {
    actor->pending_destination.field_02 = 0;
    actor->pending_destination.mode = 2;
    actor->pending_destination.raw.point = *(const real_point3d *)destination;
    actor->pending_destination.raw.surface_index = param_3;
    actor->pending_destination.orders_ignore_target_object_index = param_4;
    actor->active_destination = actor->pending_destination;
    result = actor_path_refresh(actor_handle, 1, 0);
  } else if (actor->field_04c && !actor->field_4a4) {
    result = actor_path_refresh(actor_handle, 0, 0);
  }
  return result;
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
  actor_t *actor;
  char result;

  actor = (actor_t *)datum_get(halo_actor_data_global, actor_handle);
  result = 1;
  actor->firing_positions_current_position_index = -1;
  actor_set_dormant(actor_handle, 0);
  if (actor->active_destination.mode != 4 ||
      actor->active_destination.position_index != param_2) {
    actor->pending_destination.mode = 4;
    actor->pending_destination.field_02 = 0;
    actor->pending_destination.position_index = param_2;
    actor->pending_destination.orders_ignore_target_object_index = -1;
    actor->active_destination = actor->pending_destination;
    result = actor_path_refresh(actor_handle, 1, 0);
  } else if (actor->field_04c && !actor->field_4a4) {
    result = actor_path_refresh(actor_handle, 0, 0);
  }
  return result;
}

/* 0x2d900 — actor_move_to_firing_position: Set actor movement to firing-point
 * mode (move_type=3, dest=param_2).
 *
 * Wakes the actor (actor_set_dormant), then checks if it is already in mode 3
 * heading to the same firing-point target (+0x470 == param_2). If so, either
 * refreshes the path (store_distance=0, override=param_3) when the actor is
 * active (+0x4c) and not sleeping (+0x4a4), or returns 1. Otherwise sets up
 * the movement block at +0x400 (mode=3, target at +0x404, override slot
 * +0x414 = -1, clears +0x402 and +0x3bb), copies the 24-byte block to the
 * active slot at +0x46c, and kicks off a path refresh (store_distance=1,
 * override=param_3).
 *
 * Unlike actor_move_to_point/move_position, this function does NOT clear the
 * +0x3b8 movement flag, and it threads param_3 through as the path-refresh
 * override argument in both refresh branches.
 *
 * Confirmed: datum_get(*0x6325a4, actor_handle) at 0x2d910.
 * Confirmed: actor_set_dormant(actor_handle, 0) at 0x2d91a.
 * Confirmed: CMP [ESI+0x46c],3 / CMP [ESI+0x470],CX (param_2) at
 *            0x2d92c-0x2d932.
 * Confirmed: fast-path actor_path_refresh(actor_handle,0,param_3) at 0x2d953.
 * Confirmed: setup stores MOV [ESI+0x404],CX; MOV [ESI+0x402],0;
 *            MOV [ESI+0x414],-1; MOV [ESI+0x3bb],0; MOV [ESI+0x400],3 at
 *            0x2d96a-0x2d98d.
 * Confirmed: REP MOVSD ECX=6 from actor+0x400 to actor+0x46c at 0x2d998.
 * Confirmed: setup actor_path_refresh(actor_handle,1,param_3) at 0x2d99a.
 * Confirmed: return 1 via MOV AL,1 at 0x2d9a9.
 */
char actor_move_to_firing_position(int actor_handle, int16_t param_2,
                                   void *param_3)
{
  actor_t *actor;
  char result;

  actor = (actor_t *)datum_get(halo_actor_data_global, actor_handle);
  result = 1;
  actor_set_dormant(actor_handle, 0);
  if (actor->active_destination.mode != 3 ||
      actor->active_destination.position_index != param_2) {
    actor->pending_destination.mode = 3;
    actor->pending_destination.field_02 = 0;
    actor->pending_destination.position_index = param_2;
    actor->pending_destination.orders_ignore_target_object_index = -1;
    actor->field_3bb = 0;
    actor->active_destination = actor->pending_destination;
    result = actor_path_refresh(actor_handle, 1, param_3);
  } else if (actor->field_04c && !actor->field_4a4) {
    result = actor_path_refresh(actor_handle, 0, param_3);
  }
  return result;
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
  actor_t *actor;
  prop_t *prop;
  char result;

  actor = (actor_t *)datum_get(halo_actor_data_global, actor_handle);
  actor->firing_positions_current_position_index = -1;
  actor_set_dormant(actor_handle, 0);
  result = 1;
  if (actor->active_destination.mode != 5 ||
      actor->active_destination.prop.prop_index != encounter_handle ||
      actor->active_destination.prop.accept_radius != distance) {
    prop = (prop_t *)datum_get(prop_data, encounter_handle);
    actor->pending_destination.mode = 5;
    actor->pending_destination.field_02 = 0;
    actor->pending_destination.prop.prop_index = encounter_handle;
    actor->pending_destination.prop.accept_radius = distance;
    actor->pending_destination.orders_ignore_target_object_index =
      prop->vehicle_index == -1 ? prop->unit_index : prop->vehicle_index;
    actor->active_destination = actor->pending_destination;
    result = actor_path_refresh(actor_handle, 1, 0);
  } else if (actor->field_04c && !actor->field_4a4) {
    result = actor_path_refresh(actor_handle, 0, 0);
  }
  return result;
}


/* 0x2daa0 — actor_move_compute_facing: Resolve the actor's desired facing
 * vector, movement-direction index, and steering speed for this update.
 *
 * Register args (confirmed from sole caller actor_move_update @
 * 0x2ed71-0x2edbf): ECX = move_dir  (actor's facing-direction selector,
 * actor[0x42e]); AL  = want_facing (avoidance/facing flag, caller used_bsp_test).
 * cdecl stack args (caller cleans 0x3c = 15 dwords) — see below.
 *
 * Confirmed: datum_get(*0x6325a4, actor_handle) at 0x2dab8; tag_get('actr',
 *            actor[0x58]) at 0x2dacb (actr_tag, used only via actr_tag[0xa0]).
 * Confirmed: 4-way move_dir switch (jump table 0x2e524) at 0x2db64; second
 *            4-way move_type switch (table 0x2e534) at 0x2dedf; third
 *            move_type switch (table 0x2e544) at 0x2e110.
 * Confirmed: actor_move_calculate_free(use_z@al [MOV AL,[EBP+0xc]], mvdir@esi, fdir@edi,
 *            avoid_vec@ebx) at 0x2dbf7, and actor_move_calculate_controlled_by_aiming(...) at 0x2ddb4
 *            (both reg-arg). NB: @al here is use_z (stack [EBP+0xc]), not the
 *            incoming want_facing@<al> register arg.
 * Confirmed: actor_get_stopping_distances(actor_handle, &min_dist, &slow_dist)
 *            at 0x2e02a writes floats into the arg3 ([EBP+0x14]) and slow_dist
 *            ([EBP+0x2c], the dead movement-ptr slot) — modelled with fresh
 *            typed locals here, never recycling parameters or float<->ptr
 * casts. Confirmed: FUN_000639e0(scenario_get(), actor[0x376], actor+0x12c,
 *            node_handle, &scratch2, -1, &bsp_scratch) at 0x2df9c — the inner
 *            scenario_get() return is its arg0 (Ghidra mis-grouped the pushes).
 * Confirmed: out-stores at 0x2e4ec-0x2e519: *out_dir = move_type (EDI reloaded
 *            from [EBP-0x8]); *out_facing = facing; *out_vec2 = steer_vec.
 * Uncertain: many actor field meanings are by-offset (0x42a, 0x591, 0x505,
 *            0x46e, 0x594, 0x6dc); named only where prior lifts established. */
void actor_move_compute_facing(char want_facing /* @<al> */,
                               short move_dir /* @<ecx> */, int actor_handle,
                               char use_z, float max_speed_sq, char path_gate,
                               float arg4, float arg5, float arg6, float arg7,
                               float maximum_throttle, float *movement,
                               float *out_facing, short *out_dir,
                               float *out_vec2, char *out_byte, char *out_bool)
{
  actor_t *actor;
  char *definition;
  real_vector3d facing_vector;
  real_vector3d throttle;
  real_vector3d free_throttle;
  float minimum_facing_dot;
  float facing_dot;
  float movement_distance_squared;
  float current_stopping_distance;
  float maximum_stopping_distance;
  short facing_direction;
  char face_actor_facing;
  char facing_allows_movement;
  const real_vector3d *desired_movement_vector;

  actor = (actor_t *)datum_get(halo_actor_data_global, actor_handle);
  definition = (char *)tag_get(0x61637472, actor->field_058);
  minimum_facing_dot = 0.8660254f;
  facing_direction = -1;
  desired_movement_vector = (const real_vector3d *)movement;

  if (actor->field_42a) {
    actor->field_591 = 1;
  }

  if (move_dir >= 0 && move_dir <= 3) {
    real_vector3d movement_vector;

    facing_direction = move_dir;
    movement_vector = *desired_movement_vector;
    if (!use_z) {
      movement_vector.k = 0.0f;
    }
    if (normalize3d((float *)&movement_vector) == 0.0f) {
      movement_vector = *(const real_vector3d *)actor->input_facing_vector;
    }

    switch (move_dir) {
    case 0:
      facing_vector = movement_vector;
      break;
    case 1:
      facing_vector.i = -movement_vector.i;
      facing_vector.j = -movement_vector.j;
      facing_vector.k = movement_vector.k;
      break;
    case 2:
      facing_vector.i = -movement_vector.j;
      facing_vector.j = movement_vector.i;
      facing_vector.k = movement_vector.k;
      break;
    case 3:
      facing_vector.i = movement_vector.j;
      facing_vector.j = -movement_vector.i;
      facing_vector.k = movement_vector.k;
      break;
    default:
      display_assert("!\"unreachable\"", "c:\\halo\\SOURCE\\ai\\actor_moving.c",
                     0x598, 1);
      system_exit(-1);
      break;
    }

    if (want_facing) {
      actor_move_calculate_free(use_z, (float *)&movement_vector, (float *)&facing_vector, (float *)&free_throttle);
      facing_direction = 4;
    }
  } else {
    movement_distance_squared = desired_movement_vector->i * desired_movement_vector->i +
                                desired_movement_vector->j * desired_movement_vector->j +
                                desired_movement_vector->k * desired_movement_vector->k;
    if (movement_distance_squared > 0.64f) {
      minimum_facing_dot = *(float *)(definition + 0xa0);
    }

    if (want_facing && movement_distance_squared < max_speed_sq) {
      real_vector3d movement_vector;
      real_vector3d actor_facing_vector;
      const real_vector3d *free_facing_vector;

      movement_vector = *desired_movement_vector;
      face_actor_facing = 0;

      if (actor->field_505) {
        facing_vector = actor->control_moving_forced_aim_direction;
        if (actor->field_15e > 0) {
          face_actor_facing = 1;
        }
      } else {
        facing_vector = *(const real_vector3d *)actor->input_facing_vector;
      }

      if (!use_z) {
        movement_vector.k = 0.0f;
        facing_vector.k = 0.0f;
      }
      if (normalize3d((float *)&facing_vector) == 0.0f) {
        facing_vector = *(const real_vector3d *)actor->input_facing_vector;
      }
      if (normalize3d((float *)&movement_vector) == 0.0f) {
        movement_vector = facing_vector;
      }

      if (face_actor_facing) {
        actor_facing_vector = *(const real_vector3d *)actor->input_facing_vector;
        if (!use_z) {
          actor_facing_vector.k = 0.0f;
        }
        if (normalize3d((float *)&actor_facing_vector) == 0.0f) {
          actor_facing_vector = facing_vector;
        }
        free_facing_vector = &actor_facing_vector;
      } else {
        free_facing_vector = &facing_vector;
      }
      actor_move_calculate_free(use_z, (float *)&movement_vector, (float *)free_facing_vector, (float *)&free_throttle);
      facing_direction = 4;
    } else if (actor->field_505) {
      actor_move_calculate_controlled_by_aiming(
        actor->input_facing_vector,
        use_z,
        (float *)&facing_vector,
        &facing_direction,
        movement,
        (float *)&actor->control_moving_forced_aim_direction);
    } else {
      facing_vector = *desired_movement_vector;
      if (!use_z) {
        facing_vector.k = 0.0f;
      }
      if (normalize3d((float *)&facing_vector) == 0.0f) {
        facing_vector = *(const real_vector3d *)actor->input_facing_vector;
      }
      facing_direction = 0;
    }
  }

  if (!valid_real_normal3d((float *)&facing_vector)) {
    display_assert(csprintf(error_string_buffer,
                            "%s: assert_valid_real_normal3d(%f, %f, %f)",
                            "&facing_vector", (double)facing_vector.i,
                            (double)facing_vector.j, (double)facing_vector.k),
                   "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0x5fc, 1);
    system_exit(-1);
  }

  facing_dot = (facing_vector.j * actor->input_facing_vector[1] +
                facing_vector.k * actor->input_facing_vector[2]) +
               facing_vector.i * actor->input_facing_vector[0];

  if (path_gate || actor->field_6dc == 4) {
    facing_allows_movement = 1;
  } else {
    if (!actor->field_099) {
      int pathfinding_surface_index;

      actor_find_pathfinding_location(actor_handle);
      pathfinding_surface_index = actor->field_164;
      if (pathfinding_surface_index != -1) {
        real_vector3d movement_direction;
        char test_movement_direction = 1;

        switch (facing_direction) {
        case 0:
          movement_direction = *(const real_vector3d *)actor->input_facing_vector;
          break;
        case 1:
          movement_direction.i = -actor->input_facing_vector[0];
          movement_direction.j = -actor->input_facing_vector[1];
          movement_direction.k = actor->input_facing_vector[2];
          break;
        case 2:
          movement_direction.i = actor->input_facing_vector[1];
          movement_direction.j = -actor->input_facing_vector[0];
          movement_direction.k = actor->input_facing_vector[2];
          break;
        case 3:
          movement_direction.i = -actor->input_facing_vector[1];
          movement_direction.j = actor->input_facing_vector[0];
          movement_direction.k = actor->input_facing_vector[2];
          break;
        default:
          test_movement_direction = 0;
          break;
        }

        if (test_movement_direction) {
          if (normalize2d((float *)&movement_direction) > 0.0f) {
            char collision[28];
            real_point3d test_point;

            movement_direction.k = 0.0f;
            vector3d_scale_add((float *)&actor->body_position, (float *)&movement_direction, 0.4f, (float *)&test_point);
            if (FUN_000639e0((int)scenario_get(), actor->field_376,
                             (float *)&actor->body_position, pathfinding_surface_index,
                             (float *)&test_point, -1, collision)) {
              if (minimum_facing_dot < 0.95f) {
                minimum_facing_dot = 0.95f;
              }
            }
          }
        }
      }
    }
    facing_allows_movement = facing_dot > minimum_facing_dot;
  }

  {
    float destination_tolerance = actor_destination_tolerance(actor_handle);

    movement_distance_squared = desired_movement_vector->i * desired_movement_vector->i +
                                desired_movement_vector->j * desired_movement_vector->j +
                                desired_movement_vector->k * desired_movement_vector->k;
    *out_bool = movement_distance_squared < destination_tolerance * destination_tolerance;
  }

  actor_get_stopping_distances(actor_handle, &current_stopping_distance, &maximum_stopping_distance);
  if (!actor->active_destination.field_02 &&
      movement_distance_squared < current_stopping_distance * current_stopping_distance) {
    float movement_distance = sqrtf(movement_distance_squared);

    if (movement_distance > maximum_stopping_distance + 0.05f &&
        current_stopping_distance > maximum_stopping_distance) {
      float cap = (movement_distance - maximum_stopping_distance) /
                  (current_stopping_distance - maximum_stopping_distance);
      if (cap < maximum_throttle) {
        maximum_throttle = cap;
      }
    } else {
      maximum_throttle = 0.0f;
    }
    if (maximum_throttle < 0.0f || maximum_throttle > 1.0f) {
      display_assert("(maximum_throttle >= 0.0f) && (maximum_throttle <= 1.0f)",
                     "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0x662, 1);
      system_exit(-1);
    }
  }

  throttle = *(const real_vector3d *)global_zero_vector_ptr;
  if (facing_allows_movement) {
    switch (facing_direction) {
    case 0:
      throttle.i = 1.0f;
      break;
    case 1:
      throttle.i = -1.0f;
      break;
    case 2:
      throttle.j = -1.0f;
      break;
    case 3:
      throttle.j = 1.0f;
      break;
    case 4:
      throttle = free_throttle;
      break;
    default:
      display_assert(0, "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0x671, 1);
      system_exit(-1);
      break;
    }
    throttle.i *= maximum_throttle;
    *out_byte = 0;
    throttle.j *= maximum_throttle;
    throttle.k *= maximum_throttle;
  } else {
    actor->field_591 = 1;
    *out_byte = 1;
  }

  if (game_connection() == 0 && *(char *)0x5ac9d1 != '\0') {
    arg6 = 0.0f;
    arg5 = 0.0f;
  }

  if (arg4 > 0.0f || arg6 > 0.0f) {
    float angle;
    float steering_angle;
    float angle_adjustment;

    if (facing_dot >= 1.0f) {
      angle = 0.0f;
    } else if (facing_dot <= -1.0f) {
      angle = 3.1415927f;
    } else {
      angle = acosf(facing_dot);
    }

    steering_angle = angle;
    if (arg4 > 0.0f) {
      float minimum_steering_angle = arg4 * arg7;
      float maximum_steering_angle = arg4;

      if (arg7 > 1.0f) {
        float factor = arg7 < 1.5f ? arg7 : 1.5f;
        maximum_steering_angle = factor * arg4;
      }
      if (angle * 3.0f < minimum_steering_angle) {
        minimum_steering_angle = angle * 3.0f;
      }
      if (steering_angle < minimum_steering_angle) {
        steering_angle = minimum_steering_angle;
      } else if (steering_angle > maximum_steering_angle) {
        steering_angle = maximum_steering_angle;
      }
    }

    if (steering_angle > actor->control_face_exactly_oversteer_angle) {
      if (actor->field_591 && steering_angle > arg5) {
        actor->control_face_exactly_oversteer_angle = steering_angle < arg6 ? steering_angle : arg6;
        if (*(char *)0x5aca5e != '\0') {
          console_printf(0, "steer %.4f (set oversteer %.4f)",
                         (double)steering_angle,
                         (double)actor->control_face_exactly_oversteer_angle);
        }
      } else if (*(char *)0x5aca5e != '\0') {
        console_printf(0, "steer %.4f", (double)steering_angle);
      }
    } else if (actor->control_face_exactly_oversteer_angle > 0.0f) {
      if (steering_angle < arg5) {
        if (*(char *)0x5aca5e != '\0') {
          console_printf(0, "steer %.4f < %.4f - clear oversteer %.4f",
                         (double)steering_angle,
                         (double)arg5,
                         (double)actor->control_face_exactly_oversteer_angle);
        }
        actor->control_face_exactly_oversteer_angle = 0.0f;
      } else {
        if (*(char *)0x5aca5e != '\0') {
          console_printf(0, "steer %.4f - oversteer to %.4f",
                         (double)steering_angle,
                         (double)actor->control_face_exactly_oversteer_angle);
        }
        steering_angle = actor->control_face_exactly_oversteer_angle;
      }
    }

    angle_adjustment = steering_angle - angle;
    if (fabs(angle_adjustment) > 0.00001) {
      real_vector3d rotation_axis;

      rotation_axis.i = facing_vector.k * actor->input_facing_vector[1] - facing_vector.j * actor->input_facing_vector[2];
      rotation_axis.j = facing_vector.i * actor->input_facing_vector[2] - facing_vector.k * actor->input_facing_vector[0];
      rotation_axis.k = facing_vector.j * actor->input_facing_vector[0] - facing_vector.i * actor->input_facing_vector[1];
      if (normalize3d((float *)&rotation_axis) > 0.0f) {
        rotate_vector3d_by_sincos(
          (float *)&facing_vector,
          (float *)&rotation_axis,
          x87_fsin(angle_adjustment),
          x87_fcos(angle_adjustment));
        if (*(char *)0x5aca5e != '\0') {
          console_printf(0, "adjust angle %.4f -> %.4f (%.4f)",
                         (double)angle,
                         (double)steering_angle,
                         (double)angle_adjustment);
        }
      }
    }
  }

  if (!actor->field_505 && actor->field_42e == -1) {
    if (facing_direction != 0 && facing_direction != 4) {
      display_assert("(facing_direction == _actor_facing_forward) || "
                     "(facing_direction == _actor_facing_free)",
                     "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0x6f1, 1);
      system_exit(-1);
    }
  }

  *out_dir = facing_direction;
  *(real_vector3d *)out_facing = facing_vector;
  *(real_vector3d *)out_vec2 = throttle;
}

/* 0x2e560 — actor_move_update: Top-level per-tick actor movement dispatcher.
 * Re-evaluates the actor's facing seed, picks the movement state from the
 * pending move-type (actor[0x15e]) and vehicle/path context, then calls
 * actor_move_compute_facing to resolve the final facing/throttle and applies
 * crouch/jump unit control.
 *
 * cdecl, sole arg actor_handle (confirmed: caller actor_update @ 0x3ed88,
 * prologue MOV EDI,[EBP+0x8] is the only stack arg, RET with MOV ESP,EBP).
 *
 * Confirmed: datum_get(*0x6325a4, actor_handle) at 0x2e573; tag_get('actr',
 *            actor[0x58]) at 0x2e583 -> actr_tag (uint*, control flags).
 * Confirmed: assert_valid_real_normal3d(actor+0x174) at 0x2e5d4 then seed
 *            desired_facing actor[0x5a4] = actor[0x174] at 0x2e62f.
 * Confirmed: actor[0x430] branch (0x2e64d) copies actor[0x434] vec into
 *            actor[0x518] and resets the smoothing state (0x5dc..0x5ec).
 * Confirmed: vehicle smoothing path when actor[0x15e]==4 (0x2e6bf) via
 *            actor_move_vector_avoidance(actor+0x518 or scaled, &slerp, &weight)@<ecx>.
 * Confirmed: facing-direction resolution from actor[0x42c]/0x429/0x428/0x6a
 *            at 0x2e8b7 -> actor[0x6dc].
 * Confirmed: big move-state switch on actor[0x15e] (vehicle / >0 path) at
 *            0x2e958 and the no-vehicle branch at 0x2eb77.
 * Confirmed: actor_move_compute_facing(...) call at 0x2edbf (al=used_bsp_test,
 *            ecx=actor[0x42e]).
 * Confirmed: actor_unit_control_crouch(actor_handle, crouch) at 0x2ef32,
 *            actor_unit_control_jump at 0x2f04b/0x2f123, and the leap/anim
 *            impulse path (0x2efa4-0x2f039 and 0x2f083-0x2f166). */
void actor_move_update(int actor_handle)
{
  char *actor;
  unsigned int *actr_tag;
  float vec_scratch[3]; /* [EBP-0x44..-0x3c] */
  float slerp[3]; /* [EBP-0x38..-0x30] */
  short facing_dir; /* [EBP-0x30] (resolved facing-direction selector) */
  float arg4; /* [EBP-0x28] */
  float arg5; /* [EBP-0x24] */
  float arg6; /* [EBP-0x20] */
  float arg7; /* [EBP-0x1c] */
  float maximum_throttle; /* [EBP-0x18] */
  char use_z; /* [EBP-0x14] -> compute_facing arg1 */
  float max_speed_sq; /* [EBP-0x10] */
  float weight; /* [EBP-0x8] (actor_move_vector_avoidance speed out) */
  char want_facing; /* [EBP-0x1]  -> compute_facing want_facing@al */
  char need_jump; /* [EBP-0x2]  seed-fallback gate */
  char leap_jump; /* [EBP-0x3]  unit-control-jump gate */
  char clear_firing; /* [EBP-0x4]  discarded-firing-position gate */
  char path_gate; /* [EBP-0x2c] -> compute_facing arg3 */
  char crouch; /* low byte of [EBP-0x8] crouch flag (bVar14/BL) */
  char *src;
  char *vehicle;
  char *vehicle_tag;
  short pending;
  short submode;
  int handle;
  float fade;
  float keep;
  float len_sq;
  float inv_len;
  float forward[3]; /* [EBP-0x34..-0x30] cross-edge scratch */

  actor = (char *)datum_get(halo_actor_data_global, actor_handle);
  actr_tag = (unsigned int *)tag_get(0x61637472, ((actor_t *)actor)->field_058);
  want_facing = 0;
  use_z = 0;
  path_gate = 0;
  clear_firing = 0;
  need_jump = 0;
  leap_jump = 0;
  crouch = 0;
  max_speed_sq = 0.0f;
  arg4 = 0.0f;
  arg6 = 0.0f;
  arg5 = 0.0f;
  arg7 = 0.0f;
  maximum_throttle = 1.0f;

  if (valid_real_normal3d((float *)(actor + 0x174)) == 0) {
    display_assert(csprintf((char *)0x5ab100,
                            "%s: assert_valid_real_normal3d(%f, %f, %f)",
                            (char *)0x255efc,
                            (double)((actor_t *)actor)->input_facing_vector[0],
                            (double)((actor_t *)actor)->input_facing_vector[1],
                            (double)((actor_t *)actor)->input_facing_vector[2]),
                   "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0x11e, 1);
    system_exit(-1);
  }
  ((actor_t *)actor)->control_desired_facing_vector[0] =
    ((actor_t *)actor)->input_facing_vector[0];
  *(int *)(actor + 0x5a8) = *(int *)(actor + 0x178);
  *(int *)(actor + 0x5ac) = *(int *)(actor + 0x17c);
  ((actor_t *)actor)->field_591 = 0;
  ((actor_t *)actor)->field_58d = 1;
  ((actor_t *)actor)->field_58e = 1;

  if (((actor_t *)actor)->field_430 != '\0') {
    ((actor_t *)actor)->field_518 = ((actor_t *)actor)->field_434;
    ((actor_t *)actor)->field_51c = ((actor_t *)actor)->field_438;
    ((actor_t *)actor)->field_520 = ((actor_t *)actor)->field_43c;
    ((actor_t *)actor)->field_504 = 1;
    ((actor_t *)actor)->field_58d = 0;
    *(int *)(actor + 0x5dc) = *(int *)halo_global_zero_vector_ptr_int;
    *(int *)(actor + 0x5e0) = ((int *)halo_global_zero_vector_ptr_int)[1];
    *(int *)(actor + 0x5e4) = ((int *)halo_global_zero_vector_ptr_int)[2];
    *(int *)(actor + 0x5e8) = 0;
    *(int *)(actor + 0x5ec) = 0;
  } else if (((actor_t *)actor)->field_15e == 4) {
    if (((actor_t *)actor)->field_504 == '\0') {
      vec_scratch[0] =
        ((actor_t *)actor)->input_facing_vector[0] * *(float *)0x254644;
      vec_scratch[1] =
        ((actor_t *)actor)->input_facing_vector[1] * *(float *)0x254644;
      vec_scratch[2] =
        ((actor_t *)actor)->input_facing_vector[2] * *(float *)0x254644;
      src = (char *)vec_scratch;
    } else {
      src = actor + 0x518;
    }
    actor_move_vector_avoidance(actor_handle, (float *)src, slerp, &weight);
    fade = *(float *)0x2533e8;
    if (*(float *)(actor + 0x5e4) * *(float *)(actor + 0x5e4) +
          *(float *)(actor + 0x5e0) * *(float *)(actor + 0x5e0) +
          *(float *)(actor + 0x5dc) * *(float *)(actor + 0x5dc) <
        slerp[2] * slerp[2] + slerp[1] * slerp[1] + slerp[0] * slerp[0]) {
      fade = *(float *)0x2533e4;
    }
    keep = *(float *)0x2533c8 - fade;
    *(float *)(actor + 0x5dc) = keep * *(float *)(actor + 0x5dc);
    *(float *)(actor + 0x5e0) = keep * *(float *)(actor + 0x5e0);
    *(float *)(actor + 0x5e4) = keep * *(float *)(actor + 0x5e4);
    *(float *)(actor + 0x5dc) = slerp[0] * fade + *(float *)(actor + 0x5dc);
    *(float *)(actor + 0x5e0) = slerp[1] * fade + *(float *)(actor + 0x5e0);
    *(float *)(actor + 0x5e4) = slerp[2] * fade + *(float *)(actor + 0x5e4);
    if (*(float *)(actor + 0x5e4) * *(float *)(actor + 0x5e4) +
          *(float *)(actor + 0x5e0) * *(float *)(actor + 0x5e0) +
          *(float *)(actor + 0x5dc) * *(float *)(actor + 0x5dc) <
        *(float *)0x253f44) {
      *(int *)(actor + 0x5dc) = *(int *)halo_global_zero_vector_ptr_int;
      *(int *)(actor + 0x5e0) = ((int *)halo_global_zero_vector_ptr_int)[1];
      *(int *)(actor + 0x5e4) = ((int *)halo_global_zero_vector_ptr_int)[2];
    }
    *(float *)(actor + 0x5ec) = weight;
    fade = fade * weight + keep * *(float *)(actor + 0x5e8);
    *(float *)(actor + 0x5e8) = fade;
    if (fade < *(float *)0x255ef8) {
      *(int *)(actor + 0x5e8) = 0;
    }
    if (((actor_t *)actor)->field_504 != '\0') {
      slerp[0] = *(float *)(actor + 0x5dc);
      slerp[1] = *(float *)(actor + 0x5e0);
      slerp[2] = *(float *)(actor + 0x5e4);
      len_sq = slerp[1] * slerp[1] + slerp[2] * slerp[2] + slerp[0] * slerp[0];
      if (*(float *)0x253f44 < len_sq) {
        inv_len = *(float *)0x2533c8 / sqrtf(len_sq);
        slerp[0] = slerp[0] * inv_len;
        slerp[1] = slerp[1] * inv_len;
        slerp[2] = slerp[2] * inv_len;
        rotate_vector3d_by_sincos((float *)(actor + 0x518), slerp,
                                  x87_fsin(sqrtf(len_sq)),
                                  x87_fcos(sqrtf(len_sq)));
      }
      arg7 = *(float *)(actor + 0x5e8);
    }
  }

  pending = ((actor_t *)actor)->field_42c;
  if (pending == -1) {
    pending = 2;
    if (((actor_t *)actor)->field_429 != '\0') {
      pending = 4;
    } else if (((actor_t *)actor)->field_428 != '\0') {
      pending = 3;
    } else {
      submode = ((actor_t *)actor)->field_06a;
      if (submode == 1) {
        pending = 1;
      } else if (submode == 2) {
        pending = 0;
      } else if (submode == 3) {
        pending = 2;
      }
    }
  }
  ((actor_t *)actor)->field_6dc = pending;
  facing_dir = ((actor_t *)actor)->field_42e;

  actor = (char *)datum_get(halo_actor_data_global, actor_handle);
  if (((actor_t *)actor)->field_4a8 == '\0' ||
      *(float *)(actor + 0x4a0) < ((float *)actr_tag)[0x25]) {
    crouch = ((actor_t *)actor)->field_426;
  } else {
    crouch = ((actor_t *)actor)->field_427;
  }

  if (((actor_t *)actor)->field_15e < 1) {
    if (((actor_t *)actor)->field_160 == '\0') {
      if (((actor_t *)actor)->field_418 == -1) {
        pending = ((actor_t *)actor)->field_6dc;
        if (pending == 1) {
          crouch = 0;
          ((actor_t *)actor)->field_504 = 0;
          ((actor_t *)actor)->field_58d = 0;
          ((actor_t *)actor)->field_58e = 0;
          need_jump = 1;
        } else if (((actor_t *)actor)->field_15c == '\0' ||
                   ((actor_t *)actor)->field_099 != '\0') {
          if (((actor_t *)actor)->field_6a0 == '\0') {
            if (((actor_t *)actor)->field_360 < 1) {
              clear_firing = 1;
              if (pending != 2 || (crouch == 0 ? (*actr_tag & 0x4000) != 0 :
                                                 (char)(*actr_tag >> 8) < 0)) {
                ((actor_t *)actor)->field_505 = 0;
              }
              if (pending == 4) {
                need_jump = 1;
              }
              if ((*actr_tag & 0x200000) != 0) {
                max_speed_sq =
                  ((float *)actr_tag)[0x26] * ((float *)actr_tag)[0x26];
                use_z = 1;
                want_facing = 1;
                if (((actor_t *)actor)->field_505 != '\0') {
                  max_speed_sq = max_speed_sq * *(float *)0x2533d8;
                }
              }
            } else {
              ((actor_t *)actor)->field_504 = 0;
              ((actor_t *)actor)->field_58d = 1;
              crouch = (char)(*actr_tag >> 0x1e) & 1;
            }
          } else {
            ((actor_t *)actor)->field_504 = 0;
            FUN_00012140((float *)(actor + 0x12c), (float *)(actor + 0x6a8),
                         vec_scratch);
            crouch = 0;
            if (normalize3d(vec_scratch) == *(float *)0x2533c0) {
              ((actor_t *)actor)->field_58d = 1;
            } else {
              ((actor_t *)actor)->control_desired_facing_vector[0] =
                vec_scratch[0];
              ((actor_t *)actor)->control_desired_facing_vector[1] =
                vec_scratch[1];
              ((actor_t *)actor)->control_desired_facing_vector[2] =
                vec_scratch[2];
              ((actor_t *)actor)->field_58d = 0;
              ((actor_t *)actor)->field_58e = 0;
              ((actor_t *)actor)->field_591 = 1;
            }
          }
        } else {
          crouch = 0;
          ((actor_t *)actor)->field_504 = 0;
          ((actor_t *)actor)->field_58d = 1;
        }
      } else {
        crouch = 0;
        ((actor_t *)actor)->field_504 = 0;
        ((actor_t *)actor)->field_58d = 0;
        ((actor_t *)actor)->field_58e = 0;
      }
    } else {
      ((actor_t *)actor)->field_504 = 0;
      ((actor_t *)actor)->field_50a = 0;
      if (*(int16_t *)(actor + 4) == 0xf ||
          ((actor_t *)actor)->field_161 != '\0') {
        ((actor_t *)actor)->field_58d = 1;
      } else {
        ((actor_t *)actor)->field_58d = 0;
      }
      crouch = 0;
      ((actor_t *)actor)->field_58e = 0;
    }
  } else {
    vehicle =
      (char *)object_get_and_verify_type(((actor_t *)actor)->field_158, 2);
    vehicle_tag = (char *)tag_get(0x76656869, *(int *)vehicle);
    arg4 = *(float *)(vehicle_tag + 0x3a0);
    if (*(float *)0x2533c0 < *(float *)(vehicle_tag + 0x3a4)) {
      maximum_throttle = *(float *)(vehicle_tag + 0x3a4);
    }
    arg5 = *(float *)(vehicle_tag + 0x398);
    arg6 = *(float *)(vehicle_tag + 0x39c);
    pending = ((actor_t *)actor)->field_15e;
    if (pending == 2) {
      if (*(char *)(vehicle + 0x428) == '\0') {
        if (*(float *)(vehicle + 0x444) < *(float *)0x2533c4 &&
            (leap_jump = 1, *(float *)(vehicle + 0x38) < *(float *)0x2533f0)) {
          vec_scratch[0] = *(float *)(vehicle + 0x30);
          vec_scratch[1] = *(float *)(vehicle + 0x34);
          vec_scratch[2] = 0.0f;
          if (*(float *)0x2533c0 < normalize2d(vec_scratch)) {
            ((actor_t *)actor)->field_504 = 1;
            crouch = 0;
            *(float *)(actor + 0x518) = vec_scratch[0] * *(float *)0x254644;
            *(float *)(actor + 0x51c) = vec_scratch[1] * *(float *)0x254644;
            *(float *)(actor + 0x520) = vec_scratch[2] * *(float *)0x254644;
          } else {
            crouch = 0;
            ((actor_t *)actor)->field_504 = 0;
          }
        } else {
          goto length_seed;
        }
      } else {
        crouch = 0;
        leap_jump = 1;
        ((actor_t *)actor)->field_504 = 0;
        ((actor_t *)actor)->field_58d = 1;
      }
    } else if (pending == 3) {
    length_seed:
      crouch = 0;
      max_speed_sq =
        *(float *)(vehicle_tag + 0x380) * *(float *)(vehicle_tag + 0x380);
      want_facing = 1;
    } else if (pending == 4) {
      if (vehicle_stuck(((actor_t *)actor)->field_158, vec_scratch) == '\0') {
        crouch = 0;
        facing_dir = 0;
        path_gate = 1;
        use_z = 1;
      } else {
        ((actor_t *)actor)->field_504 = 0;
        ((actor_t *)actor)->field_58d = 0;
        ((actor_t *)actor)->field_58e = 0;
        ((actor_t *)actor)->control_desired_facing_vector[0] = -vec_scratch[0];
        crouch = 0;
        ((actor_t *)actor)->control_desired_facing_vector[1] = -vec_scratch[1];
        ((actor_t *)actor)->control_desired_facing_vector[2] = -vec_scratch[2];
      }
    } else {
      ((actor_t *)actor)->field_504 = 0;
      ((actor_t *)actor)->field_50a = 0;
      if (*(int16_t *)(actor + 4) == 0xf ||
          ((actor_t *)actor)->field_161 != '\0') {
        ((actor_t *)actor)->field_58d = 1;
      } else {
        ((actor_t *)actor)->field_58d = 0;
      }
      crouch = 0;
    }
  }

  if (((actor_t *)actor)->field_504 == '\0') {
    goto seed_fallback;
  } else {
    if (((actor_t *)actor)->field_506 == '\0') {
      actor_move_compute_facing(
        want_facing, facing_dir, actor_handle, use_z, max_speed_sq, path_gate,
        arg4, arg5, arg6, arg7, maximum_throttle, (float *)(actor + 0x518),
        (float *)(actor + 0x5a4), (short *)(actor + 0x50a),
        (float *)(actor + 0x6e0), (char *)(actor + 0x507),
        (char *)(actor + 0x506));
      if (((actor_t *)actor)->field_506 != '\0') {
        ((actor_t *)actor)->field_504 = 0;
      }
    }
    if (((actor_t *)actor)->field_504 == '\0') {
      goto seed_fallback;
    }
    ((actor_t *)actor)->field_58e = 0;
    goto clear_active;
  }

seed_fallback:
  if (need_jump != '\0') {
    *(int *)(actor + 0x5a4) = *(int *)(actor + 0x174);
    *(int *)(actor + 0x5a8) = *(int *)(actor + 0x178);
    *(int *)(actor + 0x5ac) = *(int *)(actor + 0x17c);
    ((actor_t *)actor)->field_58e = 0;
    goto clear_pending;
  }
  if (((actor_t *)actor)->field_590 != '\0') {
    *(int *)(actor + 0x5a4) = *(int *)(actor + 0x598);
    *(int *)(actor + 0x5a8) = *(int *)(actor + 0x59c);
    *(int *)(actor + 0x5ac) = *(int *)(actor + 0x5a0);
    ((actor_t *)actor)->field_58e = 1;
  clear_pending:
    ((actor_t *)actor)->field_50a = 0;
  clear_active:
    ((actor_t *)actor)->field_58d = 0;
  }

  if (valid_real_normal3d((float *)(actor + 0x5a4)) == 0) {
    display_assert(
      csprintf((char *)0x5ab100, "%s: assert_valid_real_normal3d(%f, %f, %f)",
               "&actor->control.desired_facing_vector",
               (double)((actor_t *)actor)->control_desired_facing_vector[0],
               (double)((actor_t *)actor)->control_desired_facing_vector[1],
               (double)((actor_t *)actor)->control_desired_facing_vector[2]),
      "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0x28f, 1);
    system_exit(-1);
  }
  if (clear_firing != '\0' && ((actor_t *)actor)->field_504 == '\0') {
    actor_clear_discarded_firing_positions(actor_handle, 1);
  }
  if (((actor_t *)actor)->field_504 != '\0' && (*actr_tag & 0x10000000) != 0) {
    crouch = 0;
  }
  if (game_connection() == 0 && *(char *)0x5ac9ce != '\0') {
    crouch = 1;
  }
  ((actor_t *)actor)->field_58f = 0;
  if (crouch != 0 && (*actr_tag & 0x20000000) != 0) {
    ((actor_t *)actor)->field_58f = 1;
  }
  *(char *)(actor + 0x508) = crouch;
  actor_unit_control_crouch(actor_handle, crouch);

  actor = (char *)datum_get(halo_actor_data_global, actor_handle);
  if (((actor_t *)actor)->field_418 == -1 &&
      (((actor_t *)actor)->field_018 == -1 ||
       unit_is_busy(((actor_t *)actor)->field_018) == 0) &&
      ((actor_t *)actor)->field_158 == -1 &&
      ((actor_t *)actor)->field_15c == '\0' &&
      ((actor_t *)actor)->field_378 != '\0' &&
      ((actor_t *)actor)->field_379 == '\0') {
    forward[0] = ((actor_t *)actor)->input_facing_vector[0];
    forward[1] = ((actor_t *)actor)->input_facing_vector[1];
    handle = -1;
    if (((actor_t *)actor)->target_target_prop_index != -1) {
      src = (char *)datum_get(prop_data,
                              ((actor_t *)actor)->target_target_prop_index);
      forward[0] = *(float *)(src + 0xe0);
      forward[1] = *(float *)(src + 0xe4);
      handle = *(int *)(src + 0x18);
      if (normalize2d(forward) == *(float *)0x2533c0) {
        forward[0] = ((actor_t *)actor)->input_facing_vector[0];
        forward[1] = ((actor_t *)actor)->input_facing_vector[1];
      }
    }
    actor_move_animation_impulse(actor_handle, 0, (int *)forward);
    ai_communication_event(0x2a, ((actor_t *)actor)->field_018, handle, 3, -1, -1, 0);
    ((actor_t *)actor)->field_379 = 1;
  }

  if (leap_jump != '\0') {
    actor_unit_control_jump(actor_handle);
    goto store_prev;
  }
  if (((actor_t *)actor)->field_15c != '\0' ||
      ((actor_t *)actor)->field_158 != -1) {
    ((actor_t *)actor)->field_530 = 0;
    goto store_prev;
  }
  if (FUN_0002a360(actor_handle) != '\0' ||
      ((actor_t *)actor)->field_440 == '\0') {
    goto store_prev;
  }
  if (((actor_t *)actor)->field_441 == '\0') {
    actor_unit_control_jump(actor_handle);
  } else {
    if (((actor_t *)actor)->field_442 == '\0') {
      forward[0] = ((actor_t *)actor)->input_facing_vector[0];
      forward[1] = ((actor_t *)actor)->input_facing_vector[1];
      if (normalize2d(forward) == *(float *)0x2533c0) {
        forward[0] = *(float *)*(int *)0x31fc0c;
        forward[1] = ((float *)*(int *)0x31fc0c)[1];
      }
    } else {
      forward[0] = ((actor_t *)actor)->field_444;
      forward[1] = ((actor_t *)actor)->field_448;
    }
    if (unit_leap_begin(((actor_t *)actor)->field_018, forward) == '\0') {
      actor_unit_control_jump(actor_handle);
    } else {
      ai_communication_event(0x2f, ((actor_t *)actor)->field_018, -1, -1, -1, -1, 0);
    }
  }
  if (((actor_t *)actor)->field_442 != '\0') {
    *(int *)(actor + 0x534) = *(int *)(actor + 0x444);
    ((actor_t *)actor)->field_530 = 1;
    *(int *)(actor + 0x538) = *(int *)(actor + 0x448);
    *(int *)(actor + 0x53c) = *(int *)(actor + 0x44c);
    *(int *)(actor + 0x540) = *(int *)(actor + 0x450);
  }

store_prev:
  ((actor_t *)actor)->field_6ec = *(int *)(actor + 0x418);
  ((actor_t *)actor)->field_6f0 = ((actor_t *)actor)->field_41c;
  ((actor_t *)actor)->field_6f4 = ((actor_t *)actor)->field_420;
}
