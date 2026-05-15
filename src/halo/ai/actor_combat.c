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

  result *= 30.0f;
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
    *(short *)(actor + 0x5f4) = (short)(int)(delay * 30.0f);
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
