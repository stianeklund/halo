void effects_initialize(void)
{
  effect_data = game_state_data_new("effect", 0x100, 0xfc);
  effect_location_data = game_state_data_new("effect location", 0x200, 0x3c);
  if (!effect_data || !effect_location_data)
    error(0, "couldn't allocate effect globals");
}

void effects_initialize_for_new_map(void)
{
  data_delete_all(effect_data);
  data_delete_all(effect_location_data);
}

void effects_dispose_from_old_map(void)
{
  data_make_invalid(effect_data);
  data_make_invalid(effect_location_data);
}

void effects_dispose(void)
{
  if (effect_data)
    effect_data = 0;
  if (effect_location_data)
    effect_location_data = 0;
}

/* Delete an effect and all its per-event datum chains (0x9c750).
 * For each event in the effect tag, walks the linked list of event datums
 * starting at effect+0x5c+i*4 and deletes each from the event datum pool
 * (0x5aa8ac). Then deletes the effect datum from the effects pool (0x5aa8b0).
 */
void FUN_0009c750(int effect_handle)
{
  char *effect = (char *)(int)datum_absolute_index_to_index(
    *(data_t **)0x5aa8b0, effect_handle);
  if (!effect)
    return;

  char *tag_data = (char *)tag_get(0x65666665, *(int *)(effect + 4));
  int count = *(int *)(tag_data + 0x28);

  int16_t i = 0;
  while ((int)i < count) {
    int cursor = *(int *)(effect + 0x5c + (int)i * 4);
    while (cursor != -1) {
      char *datum = (char *)datum_get(*(data_t **)0x5aa8ac, cursor);
      datum_delete(*(data_t **)0x5aa8ac, cursor);
      cursor = *(int *)(datum + 4);
    }
    i++;
  }

  datum_delete(*(data_t **)0x5aa8b0, effect_handle);
}

/* Effect part volume filter (0x9caf0). Tests whether an effect part should
 * spawn based on its creation type and the effect's trigger/kill volume.
 * type 0=always, 1=outside volume, 2=inside volume, 3=never. */
bool FUN_0009caf0(int16_t part_type, void *position, void *effect_volumes)
{
  bool inside;
  switch (part_type) {
  case 0:
    return true;
  case 1:
    inside = FUN_0018f3e0(effect_volumes, position, 0);
    return !inside;
  case 2:
    return FUN_0018f3e0(effect_volumes, position, 0);
  case 3:
    return false;
  default:
    display_assert(0, "c:\\halo\\SOURCE\\effects\\effects.c", 0x378, 1);
    system_exit(-1);
    return false;
  }
}

/* Set the current event on an effect instance and randomize its duration
 * (0x9cb90). Clears the "event complete" flag, records the event index, resets
 * the elapsed-time counter, and picks a random duration from the event's
 * min/max range. Uses the global or local random seed depending on the
 * effect tag's "non-deterministic" flag (bit 1). */
void FUN_0009cb90(int effect_handle, int event_index)
{
  char *effect = (char *)(int)datum_absolute_index_to_index(
    *(data_t **)0x5aa8b0, effect_handle);
  if (!effect)
    return;

  char *tag_data = (char *)tag_get(0x65666665, *(int *)(effect + 4));
  if (event_index < 0)
    return;
  if (event_index >= *(int *)(tag_data + 0x34))
    return;

  char *event =
    (char *)tag_block_get_element(tag_data + 0x34, event_index, 0x44);
  *(uint8_t *)(effect + 2) &= ~1;
  *(int16_t *)(effect + 0x4e) = (int16_t)event_index;
  *(int *)(effect + 0x50) = 0;

  char *tag_flags = (char *)tag_get(0x65666665, *(int *)(effect + 4));
  int *seed;
  if (*(uint8_t *)tag_flags & 2)
    seed = get_global_random_seed_address();
  else
    seed = (int *)random_math_get_local_seed_address();

  *(float *)(effect + 0x54) =
    random_real_range(seed, *(float *)(event + 8), *(float *)(event + 0xc));
}

/* Walk to the next effect location datum, filtering by node attachment
 * type (0x9cca0). For part types 1 or (3 with single-player + attached
 * object), skips locations whose node_index is -1 or non-negative — i.e.
 * looks for negative marker-resolved indices. For other types, skips
 * locations with negative (non -1) node indices. Recursive. */
void *FUN_0009cca0(void *effect, int *location_handle, int part_type)
{
  char *loc;

  if (!effect) {
    display_assert("effect", "c:\\halo\\SOURCE\\effects\\effects.c", 0x6e6, 1);
    system_exit(-1);
  }
  if (!location_handle) {
    display_assert("location_datum_index",
                   "c:\\halo\\SOURCE\\effects\\effects.c", 0x6e7, 1);
    system_exit(-1);
  }
  if (*location_handle == -1)
    return 0;

  loc = (char *)datum_get(*(data_t **)0x5aa8ac, *location_handle);
  *location_handle = *(int *)(loc + 4);

  if ((int16_t)part_type == 1 ||
      ((int16_t)part_type == 3 && *(int16_t *)((char *)effect + 0x4c) != -1 &&
       local_player_count() == 1)) {
    if (*(int16_t *)(loc + 2) == -1 || *(int16_t *)(loc + 2) >= 0)
      return FUN_0009cca0(effect, location_handle, part_type);
  } else {
    if (*(int16_t *)(loc + 2) != -1 && *(int16_t *)(loc + 2) < 0)
      return FUN_0009cca0(effect, location_handle, part_type);
  }
  return loc;
}

/* Effect transition function / easing curve (0x9cdd0). Maps a normalized
 * time value through one of several curves: constant, step, linear,
 * quadratic, inverse quadratic, or cubic hermite (smoothstep). Returns
 * 0 when t == -1 (sentinel for "no duration"). */
float FUN_0009cdd0(uint16_t transition_type, float t)
{
  if (t == -1.0f)
    return 0.0f;
  switch (transition_type) {
  case 0:
    return 1.0f;
  case 1:
    if (t < 1.0f)
      return 0.0f;
    return 1.0f;
  case 2:
    return t;
  case 3:
    return t * t;
  case 4:
    return (2.0f - t) * t;
  case 5:
    return (3.0f - 2.0f * t) * t * t;
  default:
    display_assert(0, "c:\\halo\\SOURCE\\effects\\effects.c", 0x75e, 1);
    system_exit(-1);
    return t;
  }
}

/* Check if any active effect with a nonzero danger radius is close enough
 * to any player to be considered dangerous. Iterates all effects, skips
 * those with flag bit 3 set or zero danger radius, then for each player
 * walks the effect's location list and computes squared distance to the
 * player's bounding sphere. Returns true as soon as one location falls
 * within (object_radius + danger_radius). */
bool dangerous_effects_near_player(void)
{
  int effect_index;
  void *effect;
  void *effect_tag;
  data_iter_t iter;
  void *player;
  void *player_object;
  int location_index;
  void *location;
  float position[3];

  for (effect_index = data_next_index(effect_data, NONE); effect_index != NONE;
       effect_index = data_next_index(effect_data, effect_index)) {
    effect = datum_get(effect_data, effect_index);
    effect_tag = tag_get(0x65666665, *(int *)((char *)effect + 4));

    /* skip effects with "completed" flag or zero danger radius */
    if ((*(uint8_t *)((char *)effect + 2) & 8) != 0)
      continue;
    if (*(float *)((char *)effect_tag + 8) == 0.0f)
      continue;

    data_iterator_new(&iter, player_data);
    for (player = data_iterator_next(&iter); player != NULL;
         player = data_iterator_next(&iter)) {
      if (*(int *)((char *)player + 0x34) == NONE)
        continue;

      player_object =
        object_get_and_verify_type(*(int *)((char *)player + 0x34), NONE);

      /* walk the effect's location linked list */
      location_index = *(int *)((char *)effect + 0x5c);
      while (location_index != NONE) {
        uint16_t node_index;
        float radius_sum, dx, dy, dz, dist_sq;

        location = datum_get(effect_location_data, location_index);
        location_index = *(int *)((char *)location + 4);

        /* negative (but not -1) node index → resolve via helper */
        if (*(int16_t *)((char *)location + 2) != -1 &&
            *(int16_t *)((char *)location + 2) < 0) {
          location = ((void *(*)(void *, int *, int))0x9cca0)(
            effect, &location_index, 0);
        }
        if (location == NULL)
          break;

        /* resolve world-space position from node or direct coords */
        node_index = *(uint16_t *)((char *)location + 2);
        if (node_index == 0xffff) {
          /* no node attachment — use position directly */
          position[0] = *(float *)((char *)location + 0x30);
          position[1] = *(float *)((char *)location + 0x34);
          position[2] = *(float *)((char *)location + 0x38);
        } else {
          void *matrix;
          if ((int16_t)node_index < 0) {
            matrix = FUN_000dd410(*(uint16_t *)((char *)effect + 0x4c),
                                  node_index & 0x7fff);
          } else {
            matrix = ((void *(*)(int, int))0x140eb0)(
              *(int *)((char *)effect + 0x3c), node_index & 0x7fff);
          }
          /* transform local position to world space via node matrix */
          ((void (*)(void *, void *, float *))0x109590)(
            matrix, (char *)location + 0x30, position);
        }

        /* squared-distance check against player bounding sphere */
        radius_sum = *(float *)((char *)player_object + 0x5c) +
                     *(float *)((char *)effect_tag + 8);
        dx = position[0] - *(float *)((char *)player_object + 0x50);
        dy = position[1] - *(float *)((char *)player_object + 0x54);
        dz = position[2] - *(float *)((char *)player_object + 0x58);
        dist_sq = dy * dy + dx * dx + dz * dz;
        if (radius_sum * radius_sum >= dist_sq)
          return true;
      }
    }
  }

  return false;
}

/* Compute a random direction and velocity for a newly spawned effect part
 * (0x9d1f0). Picks a random speed from a scaled (min,max) range, optionally
 * rotates direction_in by a random cone angle, and writes direction_out and
 * velocity_out = speed * direction_out. Inlines effect_compute_scale logic
 * for speed (bit 0/1) and cone angle (bit 2/3). */
void FUN_0009d1f0(void *effect, unsigned int *seed, float *direction_in,
                  float *direction_out, float *velocity_out, float min_speed,
                  float max_speed, float cone_angle, int flags_lo, int flags_hi)
{
  float scale_a = *(float *)((char *)effect + 0x44);
  float scale_b = *(float *)((char *)effect + 0x48);

  /* compute speed: inline effect_compute_scale with bit_index=0 */
  float base = min_speed;
  if (flags_lo & 1)
    base *= scale_a;
  if (flags_hi & 1)
    base *= scale_b;
  float range = max_speed - min_speed;
  if (flags_lo & 2)
    range *= scale_a;
  if (flags_hi & 2)
    range *= scale_b;
  float speed = random_real_range((int *)seed, 0.0f, range) + base;

  /* compute cone spread angle */
  float cone = cone_angle;
  if (flags_lo & 4)
    cone *= scale_a;
  if (flags_hi & 4)
    cone *= scale_b;
  float angle = random_math_real(seed) * cone;

  /* copy direction_in to direction_out */
  direction_out[0] = direction_in[0];
  direction_out[1] = direction_in[1];
  direction_out[2] = direction_in[2];

  /* rotate by cone angle if nonzero */
  if (angle != 0.0f) {
    float cos_a, sin_a;
#ifdef XDK_BUILD
    __asm fld angle
    __asm fsincos
    __asm fstp cos_a
    __asm fstp sin_a
#else
    __asm__ volatile("fsincos" : "=t"(cos_a), "=u"(sin_a) : "0"(angle));
#endif
    float axis[3];
    random_seed_get_direction3d(seed, axis);
    rotate_vector3d_by_sincos(direction_out, axis, sin_a, cos_a);
  }

  /* velocity = speed * direction */
  velocity_out[0] = speed * direction_out[0];
  velocity_out[1] = speed * direction_out[1];
  velocity_out[2] = speed * direction_out[2];
}

/* Effect parts spawner (0x9d590). For each part in the current event,
 * computes how many new instances to create this frame via the transition
 * function, then walks the location chain spawning particles/effects at
 * each location with randomized position, direction, velocity, size, and
 * color. Handles node-matrix transforms for attached effects. */
void FUN_0009d590(void *effect)
{
  char *ef = (char *)effect;
  char *tag_data;
  char *event;
  char *part;
  float prev_t;
  float t;
  int *parts_block;
  int part_counter;
  short part_index;

  tag_data = (char *)tag_get(0x65666665, *(int *)(ef + 4));
  event = (char *)tag_block_get_element(tag_data + 0x34,
                                        (int)*(int16_t *)(ef + 0x4e), 0x44);

  prev_t = *(float *)(ef + 0x58);

  if (*(float *)(ef + 0x54) <= 0.0f) {
    t = 1.0f;
  } else {
    t = *(float *)(ef + 0x50) / *(float *)(ef + 0x54);
  }

  parts_block = (int *)(event + 0x38);
  part_counter = 0;
  part_index = 0;

  if (*parts_block < 1) {
    *(float *)(ef + 0x58) = t;
    return;
  }

  do {
    part = (char *)tag_block_get_element(parts_block, (int)part_index, 0xe8);

    if (*(int16_t *)(part + 8) >= 0 &&
        (int)*(int16_t *)(part + 8) < *(int *)(tag_data + 0x28)) {
      bool skip;
      if ((*(uint8_t *)(ef + 2) >> 6) & 1) {
        skip = (*(int16_t *)(part + 2) == 1);
      } else {
        skip = (*(int16_t *)(part + 2) == 2);
      }

      if (!skip) {
        unsigned int *seed;
        int count;
        float trans_prev, trans_cur;
        int ftol_prev, ftol_cur;
        int16_t count_delta;

        trans_prev = FUN_0009cdd0(*(uint16_t *)(part + 0x68), prev_t);
        count = (int)*(uint8_t *)(ef + 0xdc + (int)part_index);
        ftol_prev = (int)(trans_prev * (float)count);

        trans_cur = FUN_0009cdd0(*(uint16_t *)(part + 0x68), t);
        count = (int)*(uint8_t *)(ef + 0xdc + (int)part_index);
        ftol_cur = (int)(trans_cur * (float)count);

        count_delta = (int16_t)((uint16_t)ftol_cur - ftol_prev);

        if (count_delta < 0) {
          display_assert("count_delta>=0",
                         "c:\\halo\\SOURCE\\effects\\effects.c", 0x583, 1);
          system_exit(-1);
        }

        if (count_delta > 0) {
          int location_handle;
          char *location;
          int part_type;
          uint32_t total_count;

          location_handle =
            *(int *)(ef + 0x5c + (int)*(int16_t *)(part + 8) * 4);
          part_type = (int)*(uint16_t *)(part + 4);

          location = (char *)FUN_0009cca0(effect, &location_handle, part_type);

          if (location != NULL) {
            total_count = (uint32_t)(uint16_t)count_delta;

            do {
              uint32_t spawn_count = total_count;

              do {
                float dist_lo, dist_hi, dist_base, dist_range;
                uint32_t flags_lo, flags_hi;
                float scale_a, scale_b;
                float random_distance;
                float random_dir[3];
                float scaled_dir[3];
                float position[3];
                float direction_out[3];
                float velocity_out[3];
                float world_pos[3];
                float dir_world[3];
                float vel_world[3];
                float up_vector[3];
                char spawn_params[0x5c];

                flags_hi = *(uint32_t *)(part + 0xe4);
                dist_hi = *(float *)(part + 0x74);
                dist_lo = *(float *)(part + 0x70);
                flags_lo = *(uint32_t *)(part + 0xe0);
                scale_a = *(float *)(ef + 0x44);
                scale_b = *(float *)(ef + 0x48);

                seed = random_math_get_local_seed_address();

                dist_base = dist_lo;
                if ((int8_t)(flags_lo & 0xff) < 0)
                  dist_base *= scale_a;
                if ((int8_t)(flags_hi & 0xff) < 0)
                  dist_base *= scale_b;

                dist_range = dist_hi - dist_lo;
                if (flags_lo & 0x100)
                  dist_range *= scale_a;
                if (flags_hi & 0x100)
                  dist_range *= scale_b;

                random_distance =
                  random_real_range((int *)seed, 0.0f, dist_range) + dist_base;

                seed = random_math_get_local_seed_address();
                random_seed_get_direction3d(seed, random_dir);

                matrix_scale_transform_vector(
                  (float *)(location + 8), (float *)(part + 0x14), scaled_dir);

                position[0] = random_dir[0] * random_distance +
                              *(float *)(location + 0x30) + scaled_dir[0];
                position[1] = random_dir[1] * random_distance +
                              *(float *)(location + 0x34) + scaled_dir[1];
                position[2] = random_dir[2] * random_distance +
                              *(float *)(location + 0x38) + scaled_dir[2];

                seed = random_math_get_local_seed_address();
                FUN_0009d1f0(effect, seed, (float *)(part + 0x20),
                             direction_out, velocity_out,
                             *(float *)(part + 0x84), *(float *)(part + 0x88),
                             *(float *)(part + 0x8c), (int)flags_lo,
                             (int)flags_hi);

                matrix_transform_vector((float *)(location + 8), direction_out,
                                        direction_out);
                matrix_scale_transform_vector((float *)(location + 8),
                                              velocity_out, velocity_out);

                {
                  uint16_t node_idx = *(uint16_t *)(location + 2);
                  if (node_idx == 0xffff) {
                    world_pos[0] = position[0];
                    world_pos[1] = position[1];
                    world_pos[2] = position[2];
                    dir_world[0] = direction_out[0];
                    dir_world[1] = direction_out[1];
                    dir_world[2] = direction_out[2];
                    vel_world[0] = velocity_out[0];
                    vel_world[1] = velocity_out[1];
                    vel_world[2] = velocity_out[2];
                  } else {
                    float *node_matrix;
                    if ((int16_t)node_idx < 0) {
                      node_matrix =
                        (float *)FUN_000dd410((int)*(uint16_t *)(ef + 0x4c),
                                              (int)(node_idx & 0x7fff));
                    } else {
                      node_matrix = (float *)object_get_node_matrix(
                        *(int *)(ef + 0x3c), (int16_t)(node_idx & 0x7fff));
                    }
                    matrix_transform_point(node_matrix, position, world_pos);
                    matrix_transform_vector(node_matrix, direction_out,
                                            dir_world);
                    matrix_scale_transform_vector(node_matrix, velocity_out,
                                                  vel_world);
                  }
                }

                if (!FUN_0009caf0(*(int16_t *)part, world_pos,
                                  (char *)effect + 0x10))
                  goto next_count;

                *(int *)(spawn_params + 0x00) = *(int *)(part + 0x60);

                if (*(uint8_t *)(part + 0x64) & 1) {
                  *(int *)(spawn_params + 0x04) = *(int *)(ef + 0x3c);
                  {
                    uint16_t loc_node = *(uint16_t *)(location + 2);
                    if (loc_node == 0xffff) {
                      *(uint16_t *)(spawn_params + 0x08) = 0xffff;
                    } else {
                      *(uint16_t *)(spawn_params + 0x08) = loc_node & 0x7fff;
                    }
                  }
                  {
                    float *zero_vec = *(float **)0x31fc38;
                    up_vector[0] = zero_vec[0];
                    up_vector[1] = zero_vec[1];
                    up_vector[2] = zero_vec[2];
                  }
                } else {
                  if (*(void **)(ef + 0x34) != NULL) {
                    ((void (*)(float *, float *, int)) * (void **)(ef + 0x34))(
                      up_vector, world_pos, *(int *)(ef + 0x30));
                  } else {
                    float *zero_vec = *(float **)0x31fc38;
                    up_vector[0] = zero_vec[0];
                    up_vector[1] = zero_vec[1];
                    up_vector[2] = zero_vec[2];
                  }

                  *(float *)(spawn_params + 0x10) = world_pos[0];
                  *(float *)(spawn_params + 0x14) = world_pos[1];
                  *(float *)(spawn_params + 0x18) = world_pos[2];
                  *(float *)(spawn_params + 0x1c) = dir_world[0];
                  *(float *)(spawn_params + 0x20) = dir_world[1];
                  *(float *)(spawn_params + 0x24) = dir_world[2];
                  *(float *)(spawn_params + 0x28) =
                    *(float *)(ef + 0x24) * *(float *)0x253394 + vel_world[0];
                  *(float *)(spawn_params + 0x2c) =
                    *(float *)(ef + 0x28) * *(float *)0x253394 + vel_world[1];
                  *(float *)(spawn_params + 0x30) =
                    *(float *)(ef + 0x2c) * *(float *)0x253394 + vel_world[2];
                  *(int *)(spawn_params + 0x04) = -1;
                }

                {
                  float size_lo, size_hi, size_base, size_range;

                  size_hi = *(float *)(part + 0xa4);
                  size_lo = *(float *)(part + 0xa0);

                  seed = random_math_get_local_seed_address();

                  size_base = size_lo;
                  if (flags_lo & 0x200)
                    size_base *= scale_a;
                  if (flags_hi & 0x200)
                    size_base *= scale_b;

                  size_range = size_hi - size_lo;
                  if (flags_lo & 0x400)
                    size_range *= scale_a;
                  if (flags_hi & 0x400)
                    size_range *= scale_b;

                  *(float *)(spawn_params + 0x48) =
                    random_real_range((int *)seed, 0.0f, size_range) +
                    size_base;
                }

                {
                  float scl_lo, scl_hi, scl_base, scl_range;

                  scl_hi = *(float *)(part + 0x94);
                  scl_lo = *(float *)(part + 0x90);

                  seed = random_math_get_local_seed_address();

                  scl_base = scl_lo;
                  if (flags_lo & 0x8)
                    scl_base *= scale_a;
                  if (flags_hi & 0x8)
                    scl_base *= scale_b;

                  scl_range = scl_hi - scl_lo;
                  if (flags_lo & 0x10)
                    scl_range *= scale_a;
                  if (flags_hi & 0x10)
                    scl_range *= scale_b;

                  *(float *)(spawn_params + 0x44) =
                    random_real_range((int *)seed, 0.0f, scl_range) + scl_base;
                }

                if (*(uint8_t *)(part + 0x64) & 2) {
                  seed = random_math_get_local_seed_address();
                  *(float *)(spawn_params + 0x40) =
                    random_real_range((int *)seed, 0.0f, 6.2831855f);
                } else {
                  *(float *)(spawn_params + 0x40) = 0.0f;
                }

                {
                  float blend;
                  uint32_t fl = *(uint32_t *)(part + 0xe0);
                  uint32_t fh = *(uint32_t *)(part + 0xe4);

                  if (!(fl & 0x800) && !(fh & 0x800)) {
                    seed = random_math_get_local_seed_address();
                    blend = random_math_real(seed);
                  } else {
                    blend = 1.0f;
                    if (fl & 0x800)
                      blend = *(float *)(ef + 0x44);
                    if (fh & 0x800)
                      blend *= *(float *)(ef + 0x48);
                  }

                  FUN_0007c270(
                    (float *)(spawn_params + 0x50),
                    (uint32_t)((*(uint32_t *)(part + 0x64) >> 3) & 3),
                    (float *)(part + 0xb4), (float *)(part + 0xc4), blend);

                  *(float *)(spawn_params + 0x4c) =
                    (1.0f - blend) * *(float *)(part + 0xb0) +
                    blend * *(float *)(part + 0xc0);

                  if (*(uint8_t *)(part + 0x64) & 4) {
                    *(float *)(spawn_params + 0x50) *= *(float *)(ef + 0x18);
                    *(float *)(spawn_params + 0x54) *= *(float *)(ef + 0x1c);
                    *(float *)(spawn_params + 0x58) *= *(float *)(ef + 0x20);
                  }
                }

                *(uint16_t *)(spawn_params + 0x0a) = *(uint16_t *)(ef + 0x4c);

                {
                  uint16_t loc_node = *(uint16_t *)(location + 2);
                  if (loc_node == 0xffff || (int16_t)loc_node >= 0) {
                    *(uint8_t *)(spawn_params + 0x0c) = 0;
                  } else {
                    *(uint8_t *)(spawn_params + 0x0c) = 1;
                  }
                }

                *(uint8_t *)(spawn_params + 0x0d) =
                  (*(int16_t *)(part + 4) == 2) ? 1 : 0;
                *(uint8_t *)(spawn_params + 0x0e) =
                  (*(int16_t *)(part + 4) == 1) ? 1 : 0;

                *(float *)(spawn_params + 0x34) = up_vector[0];
                *(float *)(spawn_params + 0x38) = up_vector[1];
                *(float *)(spawn_params + 0x3c) = up_vector[2];

                FUN_000a1fd0(spawn_params);

              next_count:
                spawn_count--;
              } while (spawn_count != 0);

              location =
                (char *)FUN_0009cca0(effect, &location_handle, part_type);
            } while (location != NULL);
          }
        }
      }
    }

    part_counter++;
    part_index = (int16_t)part_counter;
  } while ((int)part_index < *parts_block);

  *(float *)(ef + 0x58) = t;
}

void FUN_0009e310(void *effect)
{
  char *ef = (char *)effect;
  char *tag_data;
  char *event;
  int *locations_block;
  int loc_counter;

  tag_data = (char *)tag_get(0x65666665, *(int *)(ef + 4));
  event = (char *)tag_block_get_element(tag_data + 0x34,
                                        (int)*(int16_t *)(ef + 0x4e), 0x44);
  locations_block = (int *)(event + 0x2c);
  loc_counter = 0;

  if (*locations_block < 1)
    return;

  do {
    char *loc_entry;
    int16_t ref_index;

    loc_entry = (char *)tag_block_get_element(locations_block,
                                              (int)(int16_t)loc_counter, 0x68);
    ref_index = *(int16_t *)(loc_entry + 4);

    if (ref_index >= 0 && (int)ref_index < *(int *)(tag_data + 0x28) &&
        *(int *)(loc_entry + 0x24) != -1) {

      bool skip;
      if ((*(uint8_t *)(ef + 2) >> 6) & 1)
        skip = (*(int16_t *)(loc_entry + 2) == 1);
      else
        skip = (*(int16_t *)(loc_entry + 2) == 2);

      if (!skip) {
        int location_handle = *(int *)(ef + 0x5c + (int)ref_index * 4);

        while (location_handle != -1) {
          char *location;
          float position[3];
          float forward[3];
          float up[3];

          location =
            (char *)datum_get(*(data_t **)0x5aa8ac, location_handle);
          location_handle = *(int *)(location + 4);

          if (*(int16_t *)(location + 2) != (int16_t)0xffff &&
              *(int16_t *)(location + 2) < 0) {
            location =
              (char *)FUN_0009cca0(effect, &location_handle, 0);
          }

          if (location == NULL)
            break;

          {
            uint16_t node_idx = *(uint16_t *)(location + 2);
            if (node_idx == 0xffff) {
              position[0] = *(float *)(location + 0x30);
              position[1] = *(float *)(location + 0x34);
              position[2] = *(float *)(location + 0x38);
              forward[0] = *(float *)(location + 0xc);
              forward[1] = *(float *)(location + 0x10);
              forward[2] = *(float *)(location + 0x14);
              up[0] = *(float *)(location + 0x24);
              up[1] = *(float *)(location + 0x28);
              up[2] = *(float *)(location + 0x2c);
            } else {
              float *node_matrix;
              if ((int16_t)node_idx < 0) {
                node_matrix =
                  (float *)FUN_000dd410((int)*(uint16_t *)(ef + 0x4c),
                                        (int)(node_idx & 0x7fff));
              } else {
                node_matrix = (float *)object_get_node_matrix(
                  *(int *)(ef + 0x3c), (int16_t)(node_idx & 0x7fff));
              }
              matrix_transform_point(node_matrix,
                                     (float *)(location + 0x30), position);
              matrix_transform_vector(node_matrix,
                                      (float *)(location + 0xc), forward);
              matrix_transform_vector(node_matrix,
                                      (float *)(location + 0x24), up);
            }
          }

          if (*(uint8_t *)(loc_entry + 6) & 1) {
            float *default_fwd = *(float **)0x31fc50;
            float *default_up = *(float **)0x31fc3c;
            forward[0] = default_fwd[0];
            forward[1] = default_fwd[1];
            forward[2] = default_fwd[2];
            up[0] = default_up[0];
            up[1] = default_up[1];
            up[2] = default_up[2];
          }

          if (!FUN_0009caf0(*(int16_t *)loc_entry, position, ef + 0x10))
            continue;

          {
            float scale = 1.0f;
            uint32_t tag_class;

            if (*(uint8_t *)(loc_entry + 0x60) & 0x20)
              scale = *(float *)(ef + 0x44);
            if (*(uint8_t *)(loc_entry + 0x64) & 0x20)
              scale *= *(float *)(ef + 0x48);

            tag_class = *(uint32_t *)(loc_entry + 0x14);

            if (tag_class == 0x6f626a65) {
              char placement[0x88];
              float direction_scratch[3];
              unsigned int *seed;

              FUN_0013fc20(placement, *(int *)(loc_entry + 0x24),
                           *(int *)(ef + 0x40));

              *(float *)(placement + 0x18) = position[0];
              *(float *)(placement + 0x1c) = position[1];
              *(float *)(placement + 0x20) = position[2];
              *(float *)(placement + 0x34) = forward[0];
              *(float *)(placement + 0x38) = forward[1];
              *(float *)(placement + 0x3c) = forward[2];
              *(float *)(placement + 0x40) = up[0];
              *(float *)(placement + 0x44) = up[1];
              *(float *)(placement + 0x48) = up[2];

              seed = (unsigned int *)get_global_random_seed_address();
              FUN_0009d1f0(effect, seed, forward, direction_scratch,
                           (float *)(placement + 0x28),
                           *(float *)(loc_entry + 0x40),
                           *(float *)(loc_entry + 0x44),
                           *(float *)(loc_entry + 0x48),
                           (int)*(uint32_t *)(loc_entry + 0x60),
                           (int)*(uint32_t *)(loc_entry + 0x64));

              *(float *)(placement + 0x28) += *(float *)(ef + 0x24);
              *(float *)(placement + 0x2c) += *(float *)(ef + 0x28);
              *(float *)(placement + 0x30) += *(float *)(ef + 0x2c);

              seed = (unsigned int *)get_global_random_seed_address();
              {
                float sa = *(float *)(ef + 0x44);
                float sb = *(float *)(ef + 0x48);
                uint32_t fl = *(uint32_t *)(loc_entry + 0x60);
                uint32_t fh = *(uint32_t *)(loc_entry + 0x64);
                float lo = *(float *)(loc_entry + 0x4c);
                float hi = *(float *)(loc_entry + 0x50);
                float *vel = (float *)(placement + 0x4c);
                float base, range, spd;

                base = lo;
                if (fl & 0x8) base *= sa;
                if (fh & 0x8) base *= sb;
                range = hi - lo;
                if (fl & 0x10) range *= sa;
                if (fh & 0x10) range *= sb;
                spd = random_real_range((int *)seed, 0.0f, range) + base;

                if (spd != 0.0f) {
                  random_seed_get_direction3d(seed, vel);
                  vel[0] *= spd;
                  vel[1] *= spd;
                  vel[2] *= spd;
                } else {
                  float *zv = *(float **)0x31fc38;
                  vel[0] = zv[0];
                  vel[1] = zv[1];
                  vel[2] = zv[2];
                }
              }

              FUN_00143c80(placement);

            } else if (tag_class == 0x64656361) {
              float direction[3];
              float dir_scratch[3];
              unsigned int *seed;
              float decal_scale;

              seed = random_math_get_local_seed_address();
              FUN_0009d1f0(effect, seed, forward, dir_scratch, direction,
                           *(float *)(loc_entry + 0x40),
                           *(float *)(loc_entry + 0x44),
                           *(float *)(loc_entry + 0x48),
                           (int)*(uint32_t *)(loc_entry + 0x60),
                           (int)*(uint32_t *)(loc_entry + 0x64));

              seed = random_math_get_local_seed_address();
              decal_scale =
                random_real_range((int *)seed, *(float *)(loc_entry + 0x54),
                                  *(float *)(loc_entry + 0x58));

              FUN_0009c4b0(*(int *)(loc_entry + 0x24), position, direction,
                           decal_scale, false, -1, 0);

            } else if (tag_class == 0x6a707421) {
              char damage_params[0x44];
              void *object;

              object =
                object_try_and_get_and_verify_type(*(int *)(ef + 0x40), -1);
              FUN_00136750(damage_params, *(int *)(loc_entry + 0x24));

              if (object != NULL) {
                *(int *)(damage_params + 0x08) =
                  *(int *)((char *)object + 0x70);
                *(int *)(damage_params + 0x0c) = *(int *)(ef + 0x40);
                *(int16_t *)(damage_params + 0x10) =
                  *(int16_t *)((char *)object + 0x68);
              }

              *(int *)(damage_params + 0x14) = *(int *)(ef + 0x10);
              *(int *)(damage_params + 0x18) = *(int *)(ef + 0x14);
              *(float *)(damage_params + 0x1c) = position[0];
              *(float *)(damage_params + 0x20) = position[1];
              *(float *)(damage_params + 0x24) = position[2];
              *(float *)(damage_params + 0x28) = position[0];
              *(float *)(damage_params + 0x2c) = position[1];
              *(float *)(damage_params + 0x30) = position[2];
              *(float *)(damage_params + 0x34) = forward[0];
              *(float *)(damage_params + 0x38) = forward[1];
              *(float *)(damage_params + 0x3c) = forward[2];
              *(float *)(damage_params + 0x40) = scale;

              FUN_00138e30(damage_params, -1);

            } else if (tag_class == 0x6c696768) {
              uint16_t loc_node = *(uint16_t *)(location + 2);
              int marker;

              if (loc_node == 0xffff)
                marker = -1;
              else
                marker = (int)(loc_node & 0x7fff);

              FUN_0013b290(*(int *)(loc_entry + 0x24), *(int *)(ef + 0x3c),
                           marker, (float *)(location + 0x30),
                           (float *)(location + 0xc), scale);

            } else if (tag_class == 0x7063746c) {
              float velocity[3];
              float dir_scratch[3];
              unsigned int *seed;
              struct {
                float scale_factor;
                float trans_vel[3];
              } particle_data;

              particle_data.scale_factor = 1.0f;
              particle_data.trans_vel[0] = *(float *)(ef + 0x18);
              particle_data.trans_vel[1] = *(float *)(ef + 0x1c);
              particle_data.trans_vel[2] = *(float *)(ef + 0x20);

              seed = random_math_get_local_seed_address();
              FUN_0009d1f0(effect, seed, forward, dir_scratch, velocity,
                           *(float *)(loc_entry + 0x40),
                           *(float *)(loc_entry + 0x44),
                           *(float *)(loc_entry + 0x48),
                           (int)*(uint32_t *)(loc_entry + 0x60),
                           (int)*(uint32_t *)(loc_entry + 0x64));

              velocity[0] += *(float *)(ef + 0x24);
              velocity[1] += *(float *)(ef + 0x28);
              velocity[2] += *(float *)(ef + 0x2c);

              FUN_000a1210(*(int *)(loc_entry + 0x24), position, velocity,
                           &particle_data, scale);

            } else if (tag_class == 0x736e6421) {
              if (*(int *)(ef + 0x3c) != -1) {
                uint16_t loc_node = *(uint16_t *)(location + 2);
                int marker;

                if (loc_node == 0xffff)
                  marker = -1;
                else
                  marker = (int)(loc_node & 0x7fff);

                FUN_001c7e70(*(int *)(ef + 0x3c),
                             *(int *)(loc_entry + 0x24), (int16_t)marker,
                             (float *)(location + 0x30),
                             (float *)(location + 0xc), scale);
              } else {
                struct {
                  float pos[3];
                  float fwd[3];
                  float zero_vec[3];
                  int field_24;
                  int field_28;
                } sound_params;
                float *zero_vec = *(float **)0x31fc38;

                sound_params.pos[0] = position[0];
                sound_params.pos[1] = position[1];
                sound_params.pos[2] = position[2];
                sound_params.fwd[0] = forward[0];
                sound_params.fwd[1] = forward[1];
                sound_params.fwd[2] = forward[2];
                sound_params.zero_vec[0] = zero_vec[0];
                sound_params.zero_vec[1] = zero_vec[1];
                sound_params.zero_vec[2] = zero_vec[2];
                sound_params.field_24 = *(int *)(ef + 0x10);
                sound_params.field_28 = *(int *)(ef + 0x14);

                FUN_001c73d0(*(int *)(loc_entry + 0x24), &sound_params,
                             scale);
              }

            } else {
              static char err_buf[256];
              const char *effect_name = tag_get_name(*(int *)(ef + 4));
              csprintf(err_buf, "effect %s has a bad part %s", effect_name,
                       *(const char **)(loc_entry + 0x1c));
              display_assert(err_buf,
                             "c:\\halo\\SOURCE\\effects\\effects.c", 0x6d9, 1);
              system_exit(-1);
            }
          }
        }
      }
    }

    loc_counter++;
  } while ((int)(int16_t)loc_counter < *locations_block);
}

/* Per-frame update for a single effect instance. Handles: attached object
 * tracking (marker resolution, color inheritance), trigger/kill volume
 * testing, and the event timing state machine. Each effect steps through
 * its tag's event list; when an event completes, the next event is chosen
 * (with a random skip-fraction check). New events spawn particles and
 * sub-effects via the parts array. Up to 8 event transitions can occur
 * per frame to handle very short events. */
void effect_update(int effect_index, float elapsed)
{
  void *effect;
  void *effect_tag;
  void *object;
  short loop_count;
  bool transitioned;

  effect = datum_get(effect_data, effect_index);
  effect_tag = tag_get(0x65666665, *(int *)((char *)effect + 4));

  /* ---- attached object tracking ---- */
  if (*(int *)((char *)effect + 0x3c) != NONE) {
    void *parent_obj;

    /* if object was deleted, delete the effect too */
    object =
      ((void *(*)(int, int))0x13d640)(*(int *)((char *)effect + 0x3c), NONE);
    if (object == NULL)
      goto delete_effect;

    /* copy position/orientation from the root object's node matrices */
    {
      int parent_handle =
        ((int (*)(int))0x13d7f0)(*(int *)((char *)effect + 0x3c));
      parent_obj = object_get_and_verify_type(parent_handle, NONE);
    }
    if ((*(uint32_t *)((char *)parent_obj + 4) & 0x800) == 0) {
      *(int16_t *)((char *)effect + 0x14) = NONE;
    } else {
      *(int *)((char *)effect + 0x10) = *(int *)((char *)parent_obj + 0x48);
      *(int *)((char *)effect + 0x14) = *(int *)((char *)parent_obj + 0x4c);
      *(int *)((char *)effect + 0x24) = *(int *)((char *)parent_obj + 0x18);
      *(int *)((char *)effect + 0x28) = *(int *)((char *)parent_obj + 0x1c);
      *(int *)((char *)effect + 0x2c) = *(int *)((char *)parent_obj + 0x20);
    }

    if ((*(uint8_t *)((char *)effect + 2) & 2) == 0)
      goto check_location;

    /* resolve marker A on the attached object */
    {
      char marker_found = ((char (*)(int, int, void *))0x1403a0)(
        *(int *)((char *)effect + 0x3c),
        (int)*(uint16_t *)((char *)effect + 0x8), (char *)effect + 0x44);

      if (!marker_found) {
        if ((*(uint8_t *)effect_tag & 1) != 0) {
          /* tag says "deleted when attachment deactivates": clear slot */
          void *obj_tag = tag_get(0x6f626a65, *(int *)object);
          int count = *(int *)((char *)obj_tag + 0x140);
          short i;
          for (i = 0; (int)i < count; i++) {
            if (*(int *)((char *)object + (int)i * 4 + 0xfc) == effect_index) {
              *(int *)((char *)object + (int)i * 4 + 0xfc) = NONE;
              goto delete_effect;
            }
          }
          goto delete_effect;
        }
        if ((*(uint8_t *)((char *)effect + 2) & 0xc) == 0)
          ((void (*)(int, int))0x9ceb0)(effect_index, 0);
      } else {
        /* marker found — check if effect was waiting to restart */
        uint16_t ef = *(uint16_t *)((char *)effect + 2);
        if ((ef & 8) != 0) {
          if ((ef & 0x20) != 0) {
            ((void (*)(int))0x9c750)(effect_index);
          } else {
            /* clear completed flag and restart from event 0 */
            *(uint16_t *)((char *)effect + 2) = ef & 0xfff7;
            FUN_0009cb90(effect_index, 0);
          }
        }
      }

      /* resolve marker B */
      ((void (*)(int, int, void *))0x1403a0)(
        *(int *)((char *)effect + 0x3c),
        (int)*(uint16_t *)((char *)effect + 0xa), (char *)effect + 0x48);

      /* inherit color from object node */
      if (*(int16_t *)((char *)effect + 0xc) != NONE) {
        int node_idx = (int)*(int16_t *)((char *)effect + 0xc);
        float *src = (float *)((char *)object + (node_idx + 0x1e) * 12);
        float *color = (float *)((char *)effect + 0x18);
        color[0] = src[0];
        color[1] = src[1];
        color[2] = src[2];
        if (!((bool (*)(float *))0x7b020)(color)) {
          error(2, "%s: assert_valid_real_rgb_color(%f, %f, %f)",
                "&effect->color", (double)color[0], (double)color[1],
                (double)color[2], "c:\\halo\\SOURCE\\effects\\effects.c", 0x4cb,
                1);
          system_exit(NONE);
        }
      }
    }
  }

check_location:
  /* ---- trigger/kill volume test ---- */
  if (*(int16_t *)((char *)effect + 0x14) == NONE)
    goto check_flags;
  {
    bool in_vol;
    if ((*(uint8_t *)effect_tag & 2) != 0)
      in_vol = ((bool (*)(void *))0x18e9b0)((char *)effect + 0x10);
    else
      in_vol = ((bool (*)(void *))0x18e910)((char *)effect + 0x10);
    if (!in_vol)
      goto check_flags;
    {
      uint16_t f = *(uint16_t *)((char *)effect + 2);
      if ((f & 0x10) == 0)
        goto event_loop;
      *(uint16_t *)((char *)effect + 2) = f & 0xffef;
      goto event_loop;
    }
  }

check_flags: {
  uint16_t f = *(uint16_t *)((char *)effect + 2);
  if (f & 0x10)
    goto event_loop;
  if (!(f & 2))
    goto delete_effect;
  *(uint16_t *)((char *)effect + 2) = f | 0x10;
}

event_loop:
  /* ---- main event timing loop (up to 8 transitions per frame) ---- */
  loop_count = 0;
  if (elapsed < 0.0f)
    return;

  for (;;) {
    uint16_t ef_flags;
    float time_remaining;

    ef_flags = *(uint16_t *)((char *)effect + 2);
    if (ef_flags & 8)
      return;
    if (loop_count >= 8)
      return;

    time_remaining =
      *(float *)((char *)effect + 0x54) - *(float *)((char *)effect + 0x50);
    if (time_remaining > elapsed) {
      /* event continues — consume elapsed and stop */
      transitioned = false;
      *(float *)((char *)effect + 0x50) += elapsed;
      elapsed = -1.0f;
    } else {
      /* event completed — advance and keep remaining time */
      transitioned = true;
      elapsed = elapsed - time_remaining;
      *(float *)((char *)effect + 0x50) = *(float *)((char *)effect + 0x54);
    }

    if (ef_flags & 1) {
      /* ---- have active event: update it ---- */
      if ((ef_flags & 0x10) == 0) {
        FUN_0009d590(effect);
      }

      if (transitioned) {
        /* determine next event index */
        short next_event;
        if ((*(uint8_t *)((char *)effect + 2) & 2) != 0 &&
            *(int16_t *)((char *)effect + 0x4e) ==
              *(int16_t *)((char *)effect_tag + 6) &&
            (next_event = *(int16_t *)((char *)effect_tag + 4),
             next_event != NONE)) {
          /* looping effect: jump back to loop start event */
        } else {
          next_event = *(int16_t *)((char *)effect + 0x4e) + 1;
        }

        /* skip events whose random roll exceeds skip_fraction */
        while ((int)(int16_t)next_event < *(int *)((char *)effect_tag + 0x34)) {
          int *seed;
          float rnd;
          void *evt;
          void *tag2 = tag_get(0x65666665, *(int *)((char *)effect + 4));
          if (*(uint8_t *)tag2 & 2)
            seed = get_global_random_seed_address();
          else
            seed = ((int *(*)(void))0x10b120)();
          rnd = ((float (*)(int *))0x10b240)(seed);
          evt = tag_block_get_element((char *)effect_tag + 0x34,
                                      (int)(int16_t)next_event, 0x44);
          if (rnd >= *(float *)((char *)evt + 4))
            break;
          next_event++;
        }

        if ((int)(int16_t)next_event >= *(int *)((char *)effect_tag + 0x34)) {
          /* past all events */
          if (*(uint16_t *)((char *)effect + 2) & 2) {
            *(uint16_t *)((char *)effect + 2) |= 8;
            return;
          }
          ((void (*)(int))0x9c750)(effect_index);
          return;
        }

        /* start the chosen event */
        {
          FUN_0009cb90(effect_index, (int)(int16_t)next_event);
        }
      }
    } else {
      /* ---- no active event yet: initialize it ---- */
      if (transitioned) {
        void *event_elem;
        int *seed;
        void *tag2;
        int parts_count;
        int *parts_block;
        short part_i;
        int part_idx;

        event_elem =
          tag_block_get_element((char *)effect_tag + 0x34,
                                (int)*(int16_t *)((char *)effect + 0x4e), 0x44);
        *(uint8_t *)((char *)effect + 2) |= 1;
        *(int *)((char *)effect + 0x50) = 0;
        *(int *)((char *)effect + 0x58) = 0xbf800000; /* -1.0f */

        tag2 = tag_get(0x65666665, *(int *)((char *)effect + 4));
        if (*(uint8_t *)tag2 & 2)
          seed = get_global_random_seed_address();
        else
          seed = ((int *(*)(void))0x10b120)();

        /* set event duration from random range */
        *(float *)((char *)effect + 0x54) =
          ((float (*)(int *, float, float))0x10b270)(
            seed, *(float *)((char *)event_elem + 0x10),
            *(float *)((char *)event_elem + 0x14));

        /* create particles/effects for each part in the event */
        parts_count = *(int *)((char *)event_elem + 0x38);
        parts_block = (int *)((char *)event_elem + 0x38);
        for (part_i = 0, part_idx = 0; part_idx < parts_count;
             part_i++, part_idx = (int)(int16_t)part_i) {
          void *part;
          float lo, hi, base, range, scale_a, scale_b;
          int flags_a, flags_b, mask;
          int *seed2;
          float result;
          uint8_t byte_val;

          part = tag_block_get_element(parts_block, part_idx, 0xe8);
          lo = (float)(int)*(int16_t *)((char *)part + 0x6c);
          hi = (float)(int)*(int16_t *)((char *)part + 0x6e);
          seed2 = ((int *(*)(void))0x10b120)();
          flags_a = *(int *)((char *)part + 0xe0);
          flags_b = *(int *)((char *)part + 0xe4);
          scale_a = *(float *)((char *)effect + 0x44);
          scale_b = *(float *)((char *)effect + 0x48);

          /* inline effect_compute_scale: scale lo/hi by effect
           * A/B scale factors based on per-part flag bits */
          base = lo;
          mask = 1 << 5;
          if (flags_a & mask)
            base *= scale_a;
          if (flags_b & mask)
            base *= scale_b;
          range = hi - lo;
          mask = 1 << 6;
          if (flags_a & mask)
            range *= scale_a;
          if (flags_b & mask)
            range *= scale_b;
          result =
            ((float (*)(int *, float, float))0x10b270)(seed2, 0.0f, range) +
            base;

          /* __ftol2: convert float to byte count */
          byte_val = (uint8_t)(int)result;
          *((uint8_t *)effect + 0xdc + part_idx) = byte_val;

          /* remap values > 6 based on local player count */
          if (byte_val > 6) {
            float temp = (float)(int)byte_val - *(float *)0x254640;
            int16_t count = local_player_count();
            temp = temp / (float)(int)count + *(float *)0x254640;
            *((uint8_t *)effect + 0xdc + part_idx) = (uint8_t)(int)temp;
          }
        }

        /* create effect locations for the new event */
        if ((*(uint8_t *)((char *)effect + 2) & 0x10) == 0) {
          FUN_0009e310(effect);
        }
      }
    }

    /* advance loop counter and check if elapsed time remains */
    loop_count++;
    if (elapsed < 0.0f)
      return;
  }

delete_effect:
  ((void (*)(int))0x9c750)(effect_index);
}

bool effects_update(float elapsed)
{
  int effect_index;

  if (profile_global_enable && *(bool *)0x2eebf0)
    profile_enter_private((void *)0x2eebe8);

  for (effect_index = data_next_index(effect_data, NONE); effect_index != NONE;
       effect_index = data_next_index(effect_data, effect_index)) {
    effect_update(effect_index, elapsed);
  }

  if (profile_global_enable && *(bool *)0x2eebf0)
    profile_exit_private((void *)0x2eebe8);

  return false;
}
