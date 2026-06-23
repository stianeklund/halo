#include "x87_math.h"

/* 0x20f80 — Check whether an actor meets conditions for a given combat mode.
 * mode 1: actor.field_60c==1 and field_268>=8
 * mode 2: actor.field_60c==0, field_268>=5, field_27c!=0, field_278>=0x4b
 * mode 3: mode 1 conditions plus field_161!=0 */
int actor_combat_check_mode(int actor_handle /* @<eax> */, short mode)
{
  char *actor = (char *)datum_get(*(void **)0x6325a4, actor_handle);

  switch (mode) {
  case 1:
    if (*(short *)(actor + 0x60c) == 1 && *(short *)(actor + 0x268) >= 8)
      return 1;
    break;
  case 2:
    if (*(short *)(actor + 0x60c) == 0 && *(short *)(actor + 0x268) >= 5 &&
        *(char *)(actor + 0x27c) != 0 && *(int *)(actor + 0x278) >= 0x4b)
      return 1;
    break;
  case 3:
    if (*(short *)(actor + 0x60c) == 1 && *(short *)(actor + 0x268) >= 8 &&
        *(char *)(actor + 0x161) != 0)
      return 1;
    break;
  }
  return 0;
}

/* 0x21010 — Begin a fixed-duration ("type 4") firing state for an actor.
 * Sets the fire_state enum (actor+0x5f2) to 4 and stores the supplied
 * duration in ticks (actor+0x5f4). cdecl: actor_handle in arg1 (EDI at the
 * call site), ticks is the truncated float result the caller pushes (arg2).
 * The field at +0x5f4 is a short, so the duration is narrowed. */
void FUN_00021010(int actor_handle, int ticks)
{
  char *actor = (char *)datum_get(*(void **)0x6325a4, actor_handle);

  *(short *)(actor + 0x5f2) = 4;
  *(short *)(actor + 0x5f4) = (short)ticks;
}

/* 0x21040 — Raise an actor's "hold burst" timer (actor+0x5f6) to at least
 * the requested number of ticks: field_5f6 = max(field_5f6, ticks).
 * The original compares the requested value against the current short and,
 * when the request is smaller, leaves the field unchanged (a no-op
 * self-assignment in the original codegen); otherwise it stores the new
 * value (narrowed to short). */
void FUN_00021040(int actor_handle, int ticks)
{
  char *actor = (char *)datum_get(*(void **)0x6325a4, actor_handle);

  if (ticks < *(short *)(actor + 0x5f6))
    *(short *)(actor + 0x5f6) = *(short *)(actor + 0x5f6);
  else
    *(short *)(actor + 0x5f6) = (short)ticks;
}

/* 0x21130 — Get the weapon/aim direction for an actor.
 * If the actor is in a vehicle with flag 0x100 set in the vehicle tag,
 * copies the vehicle's position (offset 0x24). Otherwise falls back to
 * unit aiming functions. */
void actor_combat_get_weapon_vector(int actor_handle /* @<eax> */,
                                    float *weapon_vector /* @<ebx> */)
{
  char *actor = (char *)datum_get(*(void **)0x6325a4, actor_handle);
  int handle = *(int *)(actor + 0x18);

  assert_halt(weapon_vector != NULL);

  if (*(char *)(actor + 0x161) != 0) {
    int *vehicle_obj =
      (int *)object_get_and_verify_type(*(int *)(actor + 0x158), 2);
    char *vehi_tag = (char *)tag_get(0x76656869, *vehicle_obj);
    handle = *(int *)(actor + 0x158);
    if ((*(unsigned int *)(vehi_tag + 0x2f0) & 0x100) != 0) {
      weapon_vector[0] = *(float *)((char *)vehicle_obj + 0x24);
      weapon_vector[1] = *(float *)((char *)vehicle_obj + 0x28);
      weapon_vector[2] = *(float *)((char *)vehicle_obj + 0x2c);
      return;
    }
  }

  object_get_and_verify_type(handle, 3);
  unit_scripting_unit_driver(handle, weapon_vector);
  unit_clip_to_aiming_bounds(handle, weapon_vector, 1);
}

char *actor_combat_get_firing_variant_definition(int actor_handle)
{
  char *actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
  char *actv = (char *)tag_get(0x61637476, *(int *)(actor + 0x5c));
  int weapon_handle = actor_attacking_target(actor_handle);
  if (weapon_handle != -1) {
    int *obj = (int *)object_get_and_verify_type(weapon_handle, 4);
    char *weap = (char *)tag_get(0x77656170, *obj);
    if (weap != 0 && *(int *)(weap + 0x3c8) != -1) {
      actv = (char *)tag_get(0x61637476, *(int *)(weap + 0x3c8));
    }
  }
  return actv;
}

/* 0x21270 — Look up burst and firing rate parameters from the actv tag.
 * Selects firing modifier based on actor state flags (field_378/600/601). */
void actor_combat_get_burst_parameters(int actor_handle /* @<eax> */,
                                       void *actv /* @<ecx> */,
                                       void **burst_ref, void **firing_ref)
{
  char *actor = (char *)datum_get(*(void **)0x6325a4, actor_handle);
  char *tag = (char *)actv;
  void *burst = tag + 0xcc;
  void *firing = NULL;

  if (*(char *)(actor + 0x378) != 0)
    firing = tag + 0x130;
  else if (*(char *)(actor + 0x600) != 0)
    firing = tag + 0x100;
  else if (*(char *)(actor + 0x601) != 0)
    firing = tag + 0x118;

  assert_halt(burst_ref != NULL && firing_ref != NULL);
  *burst_ref = burst;
  *firing_ref = firing;
}

/* 0x21350 — Round a float to the nearest integer using the FPU's current
 * rounding mode (FLD; FISTP). cdecl helper, single float argument, returns
 * the rounded value in EAX. */
int FUN_00021350(float value)
{
  return x87_round_to_int(value);
}

/* 0x21590 — Compute and set the fire delay timer for an actor.
 * Uses burst/firing rate data from the actv tag, random timing,
 * and combat property scaling. Result stored in actor.field_5f4.
 *
 * VC71 72.1% (61/61 insns) is a STRUCTURAL ceiling, not a defect: the @<esi>
 * actor_handle is register-received by the original, but VC71 compiles our
 * annotation as cdecl (adds a [ebp+8] load); burst_ref/firing_ref are
 * address-taken out-params forced onto the stack here while the original keeps
 * them in registers. Residual diffs are FPU store/compare ordering. No recovery
 * lever remains. Verified 2026-06-23 [[project_sub80_vc71_audit_2026-06-23]]. */
void actor_combat_set_fire_timer(int actor_handle /* @<esi> */)
{
  char *actor = (char *)datum_get(*(void **)0x6325a4, actor_handle);
  void *burst_ref;
  void *firing_ref;
  float random_time;
  float result;

  char *actv = actor_combat_get_firing_variant_definition(actor_handle);
  actor_combat_get_burst_parameters(actor_handle, actv, &burst_ref,
                                    &firing_ref);

  {
    float min_time = *(float *)((char *)burst_ref + 0x1c);
    float max_time = *(float *)((char *)burst_ref + 0x20);
    int *seed = get_global_random_seed_address();
    random_time = random_real_range(seed, min_time, max_time);
  }

  result =
    FUN_000b55b0(0xe, (int)*(unsigned short *)(actor + 0x3e)) * random_time;

  if (firing_ref != NULL) {
    if (*(float *)((char *)firing_ref + 4) != 0.0f)
      result *= *(float *)((char *)firing_ref + 4);
  }

  if (*(char *)(actor + 0x1ca) != 0)
    result *= *(float *)0x254970;

  result *= TICKS_PER_SECOND;
  *(short *)(actor + 0x5f4) = (short)(int)result;
}

/* 0x21640 — Evaluate whether the actor should fire and compute delay.
 * Returns false if encounter is in retreat state (field_24==4 or 5)
 * or if actor.field_457 is set. Otherwise computes a random delay
 * from timing_data and returns true.
 *
 * VC71 78.1% (58/70 insns) is a STRUCTURAL ceiling: @<eax>/@<edi> args are
 * register-received by the original (VC71 adds a cdecl [ebp+8] load), and the
 * bool returns compile to branchy movb/xorb here vs the original's `sete %al`.
 * Field offsets (0x3bc/0x457/0x5f4/0x60c) and control flow match exactly — no
 * dropped branch. Verified 2026-06-23 [[project_sub80_vc71_audit_2026-06-23]]. */
bool actor_combat_evaluate_firing(int actor_handle /* @<eax> */,
                                  void *timing_data /* @<edi> */)
{
  char *actor = (char *)datum_get(*(void **)0x6325a4, actor_handle);
  char flag = *(char *)(actor + 0x457);

  if (*(short *)(actor + 0x60c) == 1) {
    char *encounter =
      (char *)datum_get(*(void **)0x5ab23c, *(int *)(actor + 0x610));
    short enc_state = *(short *)(encounter + 0x24);
    if (enc_state >= 4 && enc_state <= 5) {
      *(char *)(actor + 0x3bc) = 1;
      *(short *)(actor + 0x5f4) = 0;
      return 0;
    }
  }

  if (flag != 0) {
    *(short *)(actor + 0x5f4) = 0;
    return 0;
  }

  {
    float min_time = *(float *)((char *)timing_data + 0x80);
    float max_time = *(float *)((char *)timing_data + 0x84);
    int *seed = get_global_random_seed_address();
    float delay = random_real_range(seed, min_time, max_time);
    *(short *)(actor + 0x5f4) = (short)(int)(delay * TICKS_PER_SECOND);
  }

  return 1;
}

/* 0x21710 — Compute a ballistic firing solution toward the actor's current
 * impact point and validate the line of fire.
 *
 * Looks up the actor's firing-variant projectile definition (actv tag
 * +0x180 -> globals weapon block element +0x40 -> 'proj' tag), aborts via
 * projectile_aim to get the aim direction, aim speed, target handle and a
 * gating flag. Rejects the shot when the planar aim magnitude is zero or the
 * forward alignment (dir . actor-facing) falls at/below cos(30deg) = 0.866.
 * On success it scales the direction by the aim speed into a desired-impact
 * delta, runs ai_test_ballistic_line_of_fire, and on a positive result caches
 * the aim direction at actor+0x6bc and the aim speed at actor+0x6c8.
 * Returns true when a valid firing solution was produced. */
bool actor_combat_compute_ballistic_solution(int actor_handle, int param_2)
{
  char *actor = (char *)datum_get(*(void **)0x6325a4, actor_handle);
  char *actv = (char *)tag_get(0x61637476 /* 'actv' */, *(int *)(actor + 0x5c));
  short proj_index = *(short *)(actv + 0x180);
  void *globals = game_globals_get();
  int *weapon_elem =
    (int *)tag_block_get_element((char *)globals + 0x128, proj_index, 0x44);
  int projectile_tag = 0;

  float dir[3];     /* [ebp-0x20..-0x18]  aim direction (projectile_aim out) */
  float aim_speed;  /* [ebp-8]            aim speed (projectile_aim out)     */
  int target;       /* [ebp-0x14]         target handle (projectile_aim out) */
  char gate;        /* [ebp-1]            gating flag (projectile_aim out)   */
  float impact[3];  /* [ebp-0x2c..-0x24]  desired-impact delta              */
  float mag_vec[3]; /* [ebp-0x10..-8]     planar dir + speed scratch        */
  float accel;      /* [ebp-0xc]          ballistic acceleration            */

  if (weapon_elem != 0 && weapon_elem[0x40 / 4] != -1)
    projectile_tag = (int)tag_get(0x70726f6a /* 'proj' */, weapon_elem[0x40 / 4]);

  if (projectile_tag == 0) {
    display_assert("projectile_definition",
                   "c:\\halo\\SOURCE\\ai\\actor_combat.c", 0x6c3, 1);
    system_exit(-1);
  }

  if (!projectile_aim(projectile_tag, param_2,
                      (int)(actor + 0x6a8), 0, 0, 0,
                      (int)(actor + 0x6c8), *(unsigned char *)(actor + 0x6a1),
                      (int)&dir[0], (int)&aim_speed, (int)&target, 0,
                      (void *)&gate))
    return 0;

  mag_vec[0] = dir[0];
  mag_vec[1] = dir[1];
  mag_vec[2] = aim_speed;
  if (magnitude3d(mag_vec) <= 0.0f)
    return 0;

  if (dir[1] * *(float *)(actor + 0x178) + dir[0] * *(float *)(actor + 0x174)
        <= 0.8660254f)
    return 0;

  impact[0] = dir[0] * aim_speed;
  impact[1] = dir[1] * aim_speed;
  impact[2] = dir[2] * aim_speed;

  if (gate == 0)
    accel = 0.0f;
  else
    accel = projectile_get_ballistic_acceleration(projectile_tag);

  if (!ai_test_ballistic_line_of_fire(actor_handle, param_2, target, impact,
                                      accel, *(int *)(actor + 0x6b8),
                                      *(int *)(actor + 0x158) != -1))
    return 0;

  *(float *)(actor + 0x6bc) = dir[0];
  *(float *)(actor + 0x6c0) = dir[1];
  *(float *)(actor + 0x6c4) = dir[2];
  *(float *)(actor + 0x6c8) = aim_speed;
  return 1;
}

/* 0x219e0 — Find a grenade aim target from the actor's current encounter.
 *
 * cdecl: actor_handle (EDI) plus three output pointers. When the actor has a
 * live encounter (actor+0x270 != -1) whose datum is active (+0x60 set) and
 * not suppressed (+0x127 clear), and whose type (+0x24) is 2..4, and whose
 * range value (+0x11c) lies within the actv's grenade band
 * (actv+0x194 < range < actv+0x198), it copies the encounter's target point
 * (+0xbc/+0xc0/+0xc4) into out_pos with a fixed Z bias (*0x2549d4), reports
 * the encounter handle in out_handle and the target prop index (+0x110) in
 * out_extra, and returns 1. If the actor is flagged for aim refinement
 * (actor+0x1ca), it nudges out_pos toward the actor via FUN_00021430 with a
 * 1.5 weight. Returns 0 when no suitable target exists. */
char actor_combat_find_grenade_target(int actor_handle, float *out_pos,
                                      int *out_handle, int *out_extra)
{
  char *actor = (char *)datum_get(*(void **)0x6325a4, actor_handle);
  char *actv = (char *)tag_get(0x61637476 /* 'actv' */, *(int *)(actor + 0x5c));
  char result = 0;
  char *enc;
  short type;

  if (*(int *)(actor + 0x270) != -1) {
    enc = (char *)datum_get(*(void **)0x5ab23c, *(int *)(actor + 0x270));
    if (*(char *)(enc + 0x60) != 0 && *(char *)(enc + 0x127) == 0) {
      type = *(short *)(enc + 0x24);
      if ((type > 1 && type < 4) || type == 4) {
        if (*(float *)(actv + 0x194) < *(float *)(enc + 0x11c) &&
            *(float *)(enc + 0x11c) < *(float *)(actv + 0x198)) {
          out_pos[0] = *(float *)(enc + 0xbc);
          out_pos[1] = *(float *)(enc + 0xc0);
          out_pos[2] = *(float *)(enc + 0xc4);
          result = 1;
          out_pos[2] = out_pos[2] + *(float *)0x2549d4;
          *out_handle = *(int *)(actor + 0x270);
          *out_extra = *(int *)(enc + 0x110);
          if (*(char *)(actor + 0x1ca) != 0)
            FUN_00021430(out_pos, 1.5f);
        }
      }
    }
  }
  return result;
}

/* 0x22010 — Check whether the current fire target is still valid.
 * Only applies when mode==3 (prop targeting). Checks encounter data
 * and falls back to FUN_00021ae0 distance-based search. */
int actor_combat_check_fire_target(int actor_handle /* @<edi> */, short mode)
{
  char *actor = (char *)datum_get(*(void **)0x6325a4, actor_handle);
  char *encounter;

  if (mode != 3)
    return 1;

  if (*(short *)(actor + 0x60c) != 1) {
    display_assert(
      "actor->control.current_fire_target_type == _actor_fire_target_prop",
      "c:\\halo\\SOURCE\\ai\\actor_combat.c", 0x3ca, 1);
    system_exit(-1);
  }

  encounter =
    (char *)datum_get(*(void **)0x5ab23c, *(int *)(actor + 0x610));

  if (*(int *)(encounter + 0x110) != -1)
    return 1;

  if (*(char *)(encounter + 0x12e) != 0)
    return 0;

  {
    short result = 0;
    FUN_00021ae0(actor_handle, 6.0f, 0.0f, (float *)(encounter + 0xbc), &result);
    return result >= 3;
  }
}

/* FUN_00022390 (0x22390) — Update actor combat aiming state each tick.
 * Checks/clears fire-ok flag, determines moving and in-combat status,
 * computes fire timer from burst parameters, rate-of-fire modifier from
 * weapon damage, applies encounter suppression, then calculates the aim
 * error vectors (primary lateral + secondary elevation) using trig rotation
 * of a perpendicular vector with random angular offsets. Dispatches a
 * combat vocalization sound when the actor's rank is high enough.
 *
 * VC71 79.6% (590/613 insns) is a STRUCTURAL ceiling: a 613-insn body whose
 * cos/sin intrinsification (below) is already applied; the residual is many
 * small MSVC<->clang idiom diffs (branch encodings, register allocation)
 * accumulated over the function's length, not one defect. Verified 2026-06-23
 * [[project_sub80_vc71_audit_2026-06-23]]. */
void FUN_00022390(int actor_handle)
{
  char *actor;
  char *actv;
  char suppress_flag;
  char is_moving;
  void *burst_ref;
  void *firing_ref;
  float target_pos[3];
  float perp[3];
  float primary_err[3];
  float secondary_err[3];
  float error_primary, error_secondary;
  float yaw_angle, combined_angle;
  float cos_yaw_v, sin_yaw_v, cos_comb, sin_comb;
  float delay, rate_of_fire, rof_modifier;
  float projectile_damage, max_range, damage_per_second;
  float cone_angle, cone_radius, extended_radius;
  float dx, dy, dz;
  float combat_prop, scaled_val;
  float fvar;
  int *seed;
  uint16_t random_val;
  short fire_timer;
  int *vehicle_obj;
  int weapon_handle;
  int *weapon_obj;
  char *encounter;
  short enc_state;
  char enc_flag;
  int enc_handle;
  int sound_type;

  actor = (char *)datum_get(*(void **)0x6325a4, actor_handle);
  actv = actor_combat_get_firing_variant_definition(actor_handle);
  suppress_flag = 0;

  if (*(char *)(actor + 0x604) != 0) {
    if (actor_combat_check_fire_target(actor_handle,
            *(int16_t *)(actv + 0x156)) == 0) {
      *(char *)(actor + 0x604) = 0;
    }
  }
  *(char *)(actor + 0x603) = *(char *)(actor + 0x604);
  *(char *)(actor + 0x604) = 0;

  if (*(int *)(actor + 0x158) != -1) {
    vehicle_obj = (int *)object_get_and_verify_type(*(int *)(actor + 0x158), 2);
    fvar = *(float *)((char *)vehicle_obj + 0x18);
    dy = *(float *)((char *)vehicle_obj + 0x1c);
    dz = *(float *)((char *)vehicle_obj + 0x20);
    if (fvar * fvar + dy * dy + dz * dz > *(float *)0x2533c8) {
      is_moving = 1;
    } else {
      is_moving = 0;
    }
  } else {
    if (*(char *)(actor + 0x15c) != 0 || *(char *)(actor + 0x504) != 0) {
      is_moving = 1;
    } else {
      is_moving = 0;
    }
  }
  *(char *)(actor + 0x601) = is_moving;

  combat_prop = FUN_000b55b0(0xd, (int)*(unsigned short *)(actor + 0x3e));
  scaled_val = combat_prop * *(float *)(actv + 0x88) * TICKS_PER_SECOND;
  *(char *)(actor + 0x600) =
      (float)*(int *)(actor + 0x61c) < scaled_val ? 1 : 0;

  actor_combat_get_burst_parameters(actor_handle, actv, &burst_ref, &firing_ref);

  if (*(float *)(actor + 0x458) > *(float *)0x2533c0) {
    delay = *(float *)(actor + 0x458);
  } else {
    {
      float burst_min = *(float *)((char *)burst_ref + 0x14);
      float burst_max = *(float *)((char *)burst_ref + 0x18);
      seed = get_global_random_seed_address();
      delay = random_real_range(seed, burst_min, burst_max);
    }
    if (firing_ref != NULL && *(float *)firing_ref != *(float *)0x2533c0) {
      delay *= *(float *)firing_ref;
    }
  }
  if (*(char *)(actor + 0x1ca) != 0) {
    delay *= *(float *)0x253f3c;
  }
  delay *= TICKS_PER_SECOND;
  *(short *)(actor + 0x5f4) = (short)(int)delay;

  rate_of_fire =
      FUN_000b55b0(0xb, (int)*(unsigned short *)(actor + 0x3e)) *
      *(float *)(actv + 0x7c);
  if (firing_ref != NULL &&
      *(float *)((char *)firing_ref + 0xc) != *(float *)0x2533c0) {
    rate_of_fire *= *(float *)((char *)firing_ref + 0xc);
  }
  if (*(char *)(actor + 0x1ca) != 0) {
    rate_of_fire = rate_of_fire + rate_of_fire + *(float *)0x253d4c;
  }
  *(float *)(actor + 0x698) = rate_of_fire;
  *(int *)(actor + 0x69c) = 0;

  if (*(float *)(actv + 0xc4) > *(float *)0x2533c0) {
    rof_modifier = *(float *)(actv + 0xc4);
    *(float *)(actor + 0x69c) = rof_modifier;
    if (*(char *)0x5aca5c != 0) {
      console_printf(0, "%s: manual damage modifier %.2f",
          tag_name_strip_path(tag_get_name(*(int *)(actor + 0x5c))),
          (double)rof_modifier);
    }
  } else if (*(float *)(actv + 0xc8) > *(float *)0x2533c0) {
    weapon_handle = actor_attacking_target(actor_handle);
    if (weapon_handle != -1) {
      weapon_obj = (int *)object_get_and_verify_type(weapon_handle, 4);
      projectile_damage = FUN_000fac20(*weapon_obj, &max_range);
      if (*(float *)(actv + 0x78) > *(float *)0x2533c0 &&
          max_range > *(float *)(actv + 0x78)) {
        max_range = *(float *)(actv + 0x78);
      }
      damage_per_second = max_range * projectile_damage;
      if (damage_per_second > *(float *)0x2533c0) {
        rof_modifier = *(float *)(actv + 0xc8) / damage_per_second;
        *(float *)(actor + 0x69c) = rof_modifier;
        if (*(char *)0x5aca5c != 0) {
          console_printf(0,
              "%s: proj %.1f rof %.1f dmg/s %.1f -> to get %.1f mod= %.2f",
              tag_name_strip_path(tag_get_name(*(int *)(actor + 0x5c))),
              (double)projectile_damage, (double)max_range,
              (double)damage_per_second, (double)*(float *)(actv + 0xc8),
              (double)rof_modifier);
        }
      }
    }
  }

  if (*(char *)(actor + 0x603) != 0 || *(char *)(actor + 0x602) != 0) {
    if (*(float *)(actv + 0xf8) > *(float *)0x2533c0) {
      *(float *)(actor + 0x69c) *= *(float *)(actv + 0xf8);
    }
    *(float *)(actor + 0x698) += *(float *)(actv + 0xfc);
  }

  if (*(float *)(actv + 0x14c) > *(float *)0x2533c0 &&
      *(short *)(actor + 0x60c) == 1) {
    encounter = (char *)datum_get(*(void **)0x5ab23c,
                                  *(int *)(actor + 0x610));
    enc_state = *(short *)(encounter + 0x24);
    if (enc_state < 2 || enc_state > 3 || *(short *)(encounter + 0x32) == 0) {
      suppress_flag = 1;
    }
  }

  target_pos[0] = *(float *)(actor + 0x62c);
  target_pos[1] = *(float *)(actor + 0x630);
  target_pos[2] = *(float *)(actor + 0x634);

  if (suppress_flag != 0) {
    FUN_00021430(target_pos, *(float *)(actv + 0x14c));
  }

  dx = target_pos[0] - *(float *)(actor + 0x120);
  dy = target_pos[1] - *(float *)(actor + 0x124);
  dz = target_pos[2] - *(float *)(actor + 0x128);
  perp[0] = dy - dz * 0.0f;
  perp[1] = dz * 0.0f - dx;
  perp[2] = dx * 0.0f - dy * 0.0f;
  normalize3d(perp);

  seed = get_global_random_seed_address();
  random_val = random_seed_step((unsigned int *)seed);
  if (random_val > 0x8000) {
    perp[0] = -perp[0];
    perp[1] = -perp[1];
    perp[2] = -perp[2];
  }

  {
    float yaw_max = *(float *)((char *)burst_ref + 4);
    seed = get_global_random_seed_address();
    yaw_angle = random_real_range(seed, -yaw_max, yaw_max);
  }
  {
    float pitch_max = *(float *)((char *)burst_ref + 0x10);
    seed = get_global_random_seed_address();
    combined_angle = random_real_range(seed, -pitch_max, pitch_max) + yaw_angle;
  }

  error_primary = FUN_000b55b0(0xc, (int)*(unsigned short *)(actor + 0x3e)) *
                  *(float *)burst_ref;
  {
    float sec_min = *(float *)((char *)burst_ref + 8);
    float sec_max = *(float *)((char *)burst_ref + 0xc);
    seed = get_global_random_seed_address();
    error_secondary = FUN_000b55b0(0xc,
        (int)*(unsigned short *)(actor + 0x3e)) *
        random_real_range(seed, sec_min, sec_max);
  }
  if (*(char *)(actor + 0x1ca) != 0) {
    error_primary = error_primary + error_primary;
    error_secondary = error_secondary + error_secondary;
  }

  fire_timer = *(short *)(actor + 0x5f4);
  if (fire_timer > 0 &&
      *(float *)((char *)burst_ref + 0x24) > *(float *)0x2533c0) {
    float fire_timer_f = (float)(int)fire_timer;
    cone_angle = fire_timer_f * *(float *)((char *)burst_ref + 0x24) *
                 *(float *)0x2546a4;
    if (cone_angle > *(float *)0x254a58) {
      cone_angle = *(float *)0x254a58;
    }
    cone_radius = x87_fptan(cone_angle) * *(float *)(actor + 0x638);
    if (error_primary > cone_radius) {
      extended_radius = cone_radius * *(float *)0x2533ec;
      if (error_primary < extended_radius) {
        *(short *)(actor + 0x5f4) =
            (short)(int)(error_primary / cone_radius * fire_timer_f);
        error_secondary = (extended_radius / error_primary) * error_secondary;
        error_primary = extended_radius;
      } else {
        *(short *)(actor + 0x5f4) =
            (short)(int)(fire_timer_f * *(float *)0x2533ec);
        error_secondary = (extended_radius / error_primary) * error_secondary;
        error_primary = extended_radius;
      }
    }
  }

#if defined(_MSC_VER) && !defined(__clang__)
  /* VC71 /Oi inlines cos/sin as FCOS/FSIN sharing ST0, matching the original
   * codegen; the clang runtime build keeps the explicit x87 helpers. */
  cos_yaw_v = (float)cos((double)yaw_angle);
  sin_yaw_v = (float)sin((double)yaw_angle);
  cos_comb = (float)cos((double)combined_angle);
  sin_comb = (float)sin((double)combined_angle);
#else
  cos_yaw_v = x87_fcos(yaw_angle);
  sin_yaw_v = x87_fsin(yaw_angle);
  cos_comb = x87_fcos(combined_angle);
  sin_comb = x87_fsin(combined_angle);
#endif

  primary_err[0] =
      (perp[0] * cos_yaw_v + 0.0f * sin_yaw_v) * error_primary;
  primary_err[1] =
      (perp[1] * cos_yaw_v + 0.0f * sin_yaw_v) * error_primary;
  primary_err[2] =
      (perp[2] * cos_yaw_v + sin_yaw_v) * error_primary;
  secondary_err[0] =
      -((perp[0] * cos_comb + sin_comb * 0.0f) * error_secondary);
  secondary_err[1] =
      -((perp[1] * cos_comb + sin_comb * 0.0f) * error_secondary);
  secondary_err[2] =
      -((perp[2] * cos_comb + sin_comb) * error_secondary);

  fire_timer = *(short *)(actor + 0x5f4);
  if (fire_timer > 0) {
    fvar = *(float *)0x2533c8 / (float)(int)fire_timer;
    secondary_err[0] *= fvar;
    secondary_err[1] *= fvar;
    secondary_err[2] *= fvar;
  }

  *(float *)(actor + 0x64c) = target_pos[0];
  *(float *)(actor + 0x650) = target_pos[1];
  *(float *)(actor + 0x654) = target_pos[2];
  *(float *)(actor + 0x664) = primary_err[0];
  *(float *)(actor + 0x668) = primary_err[1];
  *(float *)(actor + 0x66c) = primary_err[2];
  *(float *)(actor + 0x670) = secondary_err[0];
  *(float *)(actor + 0x674) = secondary_err[1];
  *(float *)(actor + 0x678) = secondary_err[2];
  *(float *)(actor + 0x67c) =
      *(float *)(actor + 0x64c) + *(float *)(actor + 0x664);
  *(float *)(actor + 0x680) =
      *(float *)(actor + 0x650) + *(float *)(actor + 0x668);
  *(float *)(actor + 0x684) =
      *(float *)(actor + 0x654) + *(float *)(actor + 0x66c);

  if (*(short *)(actor + 0x6e) > 6) {
    enc_flag = 0;
    enc_handle = -1;
    if (*(short *)(actor + 0x60c) == 1) {
      encounter = (char *)datum_get(*(void **)0x5ab23c,
                                    *(int *)(actor + 0x610));
      enc_flag = *(char *)(encounter + 0x61);
      enc_handle = *(int *)(encounter + 0x18);
    }
    if (*(char *)(actor + 0x378) != 0) {
      sound_type = 0x1c;
    } else if (enc_flag != 0) {
      sound_type = 0x1e;
    } else if (*(char *)(actor + 0x1f8) >= 5) {
      sound_type = 0x1d;
    } else {
      sound_type = (*(char *)(actor + 0x161) != 0) ? 0x1b : 0x1a;
    }
    FUN_00046f10(sound_type, *(int *)(actor + 0x18), enc_handle, 3, -1, -1, 0);
  }
}

/* 0x22ba0 — Compute an actor's grenade-throw aim vector.
 *
 * Looks up the actor (0x6325a4). If the actor belongs to an encounter
 * (actor+0x6b4 != -1), reads that encounter (0x5ab23c): when its type
 * (enc+0x24) is 2 or 3 the encounter datum (enc+0x18) becomes the return
 * value, and when the type is outside [0,1] a seed aim point is built from
 * enc+0xbc/0xc0/0xc4 (with a Z bias of *0x2549d4) and fed to the helper
 * FUN_00022b40 (actor_handle in EBX, &aim point in ESI).
 *
 * It then runs the ballistic firing solution (actor_combat_compute_ballistic_
 * solution), and if the actor currently has no live grenade target
 * (actor+0x158 == -1), derives the throw direction from the cached aim
 * (actor+0x6bc/0x6c0/0x6c4): it normalizes the planar (x,y) part, and when
 * that planar direction points far enough off the actor facing
 * (actor+0x174/0x178), it rotates the actor-facing normal by a fixed cos
 * (0x3f5db3d7) and a +/- sin (*0x253398, sign from the planar cross sign),
 * rescales by the original planar magnitude, asserts the result is a valid
 * real normal, and adopts it as the new aim vector.
 *
 * Finally the chosen vector is scaled by the throw speed (actor+0x6c8) and
 * written to out_aim_vector. Returns the encounter datum (or -1).
 *
 * VC71 75.0% (168/176 insns) is a STRUCTURAL ceiling: x87 op scheduling
 * (faddp/fsqrt order), register allocation (ebx vs edi for the arg, reg-vs-stack
 * -1 init), and byte/dword compare idioms (jle/jl, testl/testb). The
 * display_assert(...,0x749,1) is present and aligned; no FPU operand swap.
 * Verified 2026-06-23 [[project_sub80_vc71_audit_2026-06-23]]. */
int actor_aim_grenade(int actor_handle, void *aim_params, float *out_aim_vector)
{
  char *actor = (char *)datum_get(*(void **)0x6325a4, actor_handle);
  float aim_x, aim_y, aim_z;   /* [ebp-0x24/-0x20/-0x1c] chosen aim vector  */
  float nrm_x, nrm_y, nrm_z;   /* [ebp-0x18/-0x14/-0x10] rotated facing nrm  */
  float planar[2];             /* [ebp-0xc/-0x8] planar dir for normalize    */
  int result;                  /* [ebp-0x4] encounter datum / -1             */
  char *enc;
  short enc_type;
  float aim_vec[3];            /* contiguous buffer for FUN_00022b40 (ESI)   */
  float speed;
  float planar_mag;
  float t;
  int sign;
  float sin_a;

  result = -1;
  if (*(int *)(actor + 0x6b4) != -1) {
    enc = (char *)datum_get(*(void **)0x5ab23c, *(int *)(actor + 0x6b4));
    enc_type = *(short *)(enc + 0x24);
    if (enc_type > 1 && enc_type < 4)
      result = *(int *)(enc + 0x18);
    if (enc_type < 0 || enc_type > 1) {
      aim_vec[0] = *(float *)(enc + 0xbc);
      aim_vec[1] = *(float *)(enc + 0xc0);
      aim_vec[2] = *(float *)(enc + 0xc4) + *(float *)0x2549d4;
      FUN_00022b40(actor_handle, aim_vec);
    }
  }

  actor_combat_compute_ballistic_solution(actor_handle, (int)aim_params);

  aim_x = *(float *)(actor + 0x6bc);
  aim_y = *(float *)(actor + 0x6c0);
  aim_z = *(float *)(actor + 0x6c4);
  if (*(int *)(actor + 0x158) == -1) {
    planar[0] = aim_x;
    planar[1] = aim_y;
    if (magnitude3d(planar) > *(float *)0x2533c0 &&
        planar[0] * *(float *)(actor + 0x174) +
            planar[1] * *(float *)(actor + 0x178) < *(float *)0x2533dc) {
      nrm_x = *(float *)(actor + 0x174);
      nrm_y = *(float *)(actor + 0x178);
      nrm_z = *(float *)(actor + 0x17c);
      t = planar[1] * *(float *)(actor + 0x174) -
          planar[0] * *(float *)(actor + 0x178);
      sign = (t > *(float *)0x2533c0) ? 1 : -1;
      sin_a = (float)sign * *(float *)0x253398;
      nrm_z = aim_z;
      rotate_vector3d_by_sincos(&nrm_x, *(float **)0x31fc44, sin_a,
                                0.857651889f /* 0x3f5db3d7 */);
      planar_mag = (float)x87_sqrt(aim_x * aim_x + aim_y * aim_y);
      nrm_x = nrm_x * planar_mag;
      nrm_y = planar_mag * nrm_y;
      if (!valid_real_normal3d(&nrm_x)) {
        csprintf((char *)0x5ab100,
                 "%s: assert_valid_real_normal3d(%f, %f, %f)", "&new_aim_vector",
                 (double)nrm_x, (double)nrm_y, (double)aim_z);
        display_assert((char *)0x5ab100, "c:\\halo\\SOURCE\\ai\\actor_combat.c",
                       0x749, 1);
        system_exit(-1);
      }
      aim_x = nrm_x;
      aim_y = nrm_y;
      aim_z = nrm_z;
    }
  }

  speed = *(float *)(actor + 0x6c8);
  out_aim_vector[0] = aim_x * speed;
  out_aim_vector[1] = aim_y * speed;
  out_aim_vector[2] = speed * aim_z;
  return result;
}
