#include "x87_math.h"

/* 0x20f80 — Check whether an actor meets conditions for a given combat mode.
 * mode 1: actor.field_60c==1 and field_268>=8
 * mode 2: actor.field_60c==0, field_268>=5, field_27c!=0, field_278>=0x4b
 * mode 3: mode 1 conditions plus field_161!=0 */
int actor_combat_check_mode(int actor_handle /* @<eax> */, short mode)
{
  char *actor = (char *)datum_get(*(void **)0x6325a4, actor_handle);

  if (mode == 1) {
    if (*(short *)(actor + 0x60c) == 1 && *(short *)(actor + 0x268) >= 8)
      return 1;
  } else if (mode == 2) {
    if (*(short *)(actor + 0x60c) == 0 && *(short *)(actor + 0x268) >= 5 &&
        *(char *)(actor + 0x27c) != 0 && *(int *)(actor + 0x278) >= 0x4b)
      return 1;
  } else if (mode == 3) {
    if (*(short *)(actor + 0x60c) == 1 && *(short *)(actor + 0x268) >= 8 &&
        *(char *)(actor + 0x161) != 0)
      return 1;
  }
  return 0;
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

/* 0x21590 — Compute and set the fire delay timer for an actor.
 * Uses burst/firing rate data from the actv tag, random timing,
 * and combat property scaling. Result stored in actor.field_5f4. */
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
    float mod = *(float *)((char *)firing_ref + 4);
    if (mod != 0.0f)
      result *= mod;
  }

  if (*(char *)(actor + 0x1ca) != 0)
    result *= *(float *)0x254970;

  result *= TICKS_PER_SECOND;
  *(short *)(actor + 0x5f4) = (short)(int)result;
}

/* 0x21640 — Evaluate whether the actor should fire and compute delay.
 * Returns false if encounter is in retreat state (field_24==4 or 5)
 * or if actor.field_457 is set. Otherwise computes a random delay
 * from timing_data and returns true. */
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

/* 0x22010 — Check whether the current fire target is still valid.
 * Only applies when mode==3 (prop targeting). Checks encounter data
 * and falls back to FUN_00021ae0 distance-based search. */
int actor_combat_check_fire_target(int actor_handle /* @<edi> */, short mode)
{
  char *actor = (char *)datum_get(*(void **)0x6325a4, actor_handle);

  if (mode != 3)
    return 1;

  if (*(short *)(actor + 0x60c) != 1) {
    display_assert(
      "actor->control.current_fire_target_type == _actor_fire_target_prop",
      "c:\\halo\\SOURCE\\ai\\actor_combat.c", 0x3ca, 1);
    system_exit(-1);
  }

  char *encounter =
    (char *)datum_get(*(void **)0x5ab23c, *(int *)(actor + 0x610));

  if (*(int *)(encounter + 0x110) != -1)
    return 1;

  if (*(char *)(encounter + 0x12e) != 0)
    return 0;

  {
    short result = 0;
    FUN_00021ae0(actor_handle, 6.0f, 0, encounter + 0xbc, &result);
    return result >= 3;
  }
}

/* FUN_00022390 (0x22390) — Update actor combat aiming state each tick.
 * Checks/clears fire-ok flag, determines moving and in-combat status,
 * computes fire timer from burst parameters, rate-of-fire modifier from
 * weapon damage, applies encounter suppression, then calculates the aim
 * error vectors (primary lateral + secondary elevation) using trig rotation
 * of a perpendicular vector with random angular offsets. Dispatches a
 * combat vocalization sound when the actor's rank is high enough. */
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

  cos_yaw_v = x87_fcos(yaw_angle);
  sin_yaw_v = x87_fsin(yaw_angle);
  cos_comb = x87_fcos(combined_angle);
  sin_comb = x87_fsin(combined_angle);

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
