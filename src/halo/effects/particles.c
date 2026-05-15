/* Allocate a new particle system header and initialize it (0xa1210).
 * Copies position, velocity, and tint/orientation data into the datum,
 * resolves lighting color via FUN_00139480, then runs setup (FUN_000a0fd0).
 * Returns the datum handle, or -1 on failure. */
int FUN_000a1210(int tag_index, float *position, float *velocity,
                 void *ext_data, float scale)
{
  int handle;
  char *datum;
  char local_buf[12];

  handle = data_new_at_index(particle_system_header_data);
  if (handle != -1) {
    datum = (char *)datum_get(particle_system_header_data, handle);
    *(int *)(datum + 0x08) = tag_index;
    *(int *)(datum + 0x0c) = -1;

    *(float *)(datum + 0x20) = position[0];
    *(float *)(datum + 0x24) = position[1];
    *(float *)(datum + 0x28) = position[2];

    *(float *)(datum + 0x2c) = velocity[0];
    *(float *)(datum + 0x30) = velocity[1];
    *(float *)(datum + 0x34) = velocity[2];

    *(float *)(datum + 0x38) = ((float *)ext_data)[0];
    *(float *)(datum + 0x3c) = ((float *)ext_data)[1];
    *(float *)(datum + 0x40) = ((float *)ext_data)[2];
    *(float *)(datum + 0x44) = ((float *)ext_data)[3];

    *(float *)(datum + 0x14) = scale;
    *(uint32_t *)(datum + 0x04) |= 1;

    FUN_00139480((void *)(datum + 0x20), (void *)(datum + 0x48), local_buf, 0);

    if (!FUN_000a0fd0(handle)) {
      datum_delete(particle_system_header_data, handle);
      return -1;
    }
  }
  return handle;
}

void particles_initialize(void)
{
  particle_data = game_state_data_new("particle", 0x400, 0x70);
  if (!particle_data)
    error(0, "couldn't allocate particle globals");
}

void particles_initialize_for_new_map(void)
{
  data_delete_all(particle_data);
}

void particles_dispose_from_old_map(void)
{
  data_make_invalid(particle_data);
}

void particles_dispose(void)
{
  if (particle_data)
    particle_data = 0;
}

/* Delete particle datum from the particle pool (0xa14f0). */
void particle_delete(int datum_handle)
{
  datum_delete(particle_data, datum_handle);
}

/* Delete all particles owned by a local player that have an attached
   object (flag 0x40 set and object handle != -1). */
void FUN_000a1510(int local_player_index)
{
  int handle;
  char *datum;

  for (handle = data_next_index(particle_data, -1); handle != -1;
       handle = data_next_index(particle_data, handle)) {
    datum = (char *)datum_get(particle_data, handle);
    if (*(uint8_t *)(datum + 0xf) == (uint8_t)local_player_index &&
        (*(uint8_t *)(datum + 2) & 0x40) != 0 && *(int *)(datum + 8) != -1) {
      datum_delete(particle_data, handle);
    }
  }
}

/* Compute the particle's current visual size (0xa1670).
 * Interpolates between the tag's min/max size based on the ratio of
 * elapsed time to total lifetime, then scales by the particle's
 * individual size factor. Returns the interpolated size. */
float particle_get_radius(int datum_handle)
{
  char *datum;
  char *tag;

  datum = (char *)datum_get(particle_data, datum_handle);
  tag = (char *)tag_get(0x70617274, *(int *)(datum + 0x04));

  return (((*(float *)(tag + 0x78) - *(float *)(tag + 0x74)) *
           (*(float *)(datum + 0x14) / *(float *)(datum + 0x18))) +
          *(float *)(tag + 0x74)) *
         *(float *)(datum + 0x5c);
}

/* Check whether a 3D point has all finite float components (0x0a16b0).
 * Returns true if none of the three components are NaN or infinity
 * (IEEE 754 exponent field != 0x7f800000). */
bool valid_real_point3d(float *point)
{
  uint32_t *p = (uint32_t *)point;
  if ((p[0] & 0x7f800000) == 0x7f800000)
    return false;
  if ((p[1] & 0x7f800000) == 0x7f800000)
    return false;
  if ((p[2] & 0x7f800000) == 0x7f800000)
    return false;
  return true;
}

/* Validate an ARGB color (0xa1710).
 * The alpha component (color[0]) must be finite, in [0.0, 1.0],
 * and the RGB components (color[1..3]) must each be finite and valid.
 * Returns true if the color is valid. */
bool valid_real_argb_color(float *color)
{
  /* Check alpha is not NaN/Inf */
  if ((*(uint32_t *)color & 0x7f800000) == 0x7f800000)
    return false;

  /* Check alpha >= 0.0 */
  if (*color < 0.0f)
    return false;

  /* Check alpha <= 1.0 */
  if (*color > 1.0f)
    return false;

  /* Validate RGB components */
  if (!valid_real_rgb_color(color + 1))
    return false;

  return true;
}

/* Particle physics cleanup dispatch (0xa1770).
 * Called when deleting a particle that has a secondary physics tag.
 * Dispatches to effect creation ('effe') or sound playback ('snd!'). */
void FUN_000a1770(int particle, int tag_group, int physics_tag, int param)
{
  float velocity[3];
  float *default_fwd;

  velocity[0] = *(float *)(particle + 0x48) * *(float *)0x2546a4;
  velocity[1] = *(float *)(particle + 0x4c) * *(float *)0x2546a4;
  velocity[2] = *(float *)(particle + 0x50) * *(float *)0x2546a4;

  if (tag_group == 0x65666665) {
    float marker_points[6];
    float marker_forwards[6];

    marker_points[0] = *(float *)(particle + 0x30);
    marker_points[1] = *(float *)(particle + 0x34);
    marker_points[2] = *(float *)(particle + 0x38);
    marker_points[3] = *(float *)(particle + 0x30);
    marker_points[4] = *(float *)(particle + 0x34);
    marker_points[5] = *(float *)(particle + 0x38);

    marker_forwards[0] = *(float *)(particle + 0x3c);
    marker_forwards[1] = *(float *)(particle + 0x40);
    marker_forwards[2] = *(float *)(particle + 0x44);
    normalize3d(marker_forwards);

    default_fwd = *(float **)0x31fc50;
    marker_forwards[3] = default_fwd[0];
    marker_forwards[4] = default_fwd[1];
    marker_forwards[5] = default_fwd[2];

    effect_new_unattached_from_markers(
      physics_tag, -1, velocity, 2, (void *)0x2ef7d8, marker_points,
      marker_forwards, *(float *)&param, 0.0f, 0.0f, 0.0f, 0.0f);
    return;
  }

  if (tag_group == 0x736e6421) {
    float location[11];

    location[0] = *(float *)(particle + 0x30);
    location[1] = *(float *)(particle + 0x34);
    location[2] = *(float *)(particle + 0x38);

    default_fwd = *(float **)0x31fc3c;
    location[3] = default_fwd[0];
    location[4] = default_fwd[1];
    location[5] = default_fwd[2];

    location[6] = velocity[0];
    location[7] = velocity[1];
    location[8] = velocity[2];

    *(int *)&location[9] = *(int *)(particle + 0x28);
    *(int *)&location[10] = *(int *)(particle + 0x2c);

    unattached_impulse_sound_new(physics_tag, location, *(float *)&param);
    return;
  }

  display_assert(0, "c:\\halo\\SOURCE\\effects\\particles.c", 799, 1);
  system_exit(-1);
}

/* Delete a particle (0xa18c0).
 * Checks the particle tag for a secondary physics resource; if present,
 * dispatches cleanup via FUN_000a1770. Then removes the particle datum. */
void FUN_000a18c0(int datum_handle)
{
  char *datum;
  char *tag;

  datum = (char *)datum_get(particle_data, datum_handle);
  tag = (char *)tag_get(0x70617274, *(int *)(datum + 0x04));
  if (*(int *)(tag + 0x64) != -1) {
    FUN_000a1770((int)datum, *(int *)(tag + 0x58), *(int *)(tag + 0x64), 0);
  }
  datum_delete(particle_data, datum_handle);
}

/* TODO: particle_step and particle_move temporarily reverted to original
 * binary due to a stale datum handle crash during gameplay. These need
 * debugging — the implementations are preserved in git history (commit
 * 08bf664). The issue manifests as "particle index #X is unused or changed"
 * in datum_get after loading a campaign level. */

/* Set up the particle's bitmap sequence index (0xa1910).
 * Walks through up to 4 animation phases (creation, attached, detached,
 * fading) and picks a random frame range for each phase based on the
 * particle tag's sequence counts at tag offsets 0x98..0x9e.
 * Each phase accumulates the total frame offset from prior phases.
 * If the final frame index is valid within the bitmap tag's sequence
 * array, clamps it and returns true. Otherwise calls particle_delete
 * (FUN_000a18c0) and returns false. */
bool FUN_000a1910(int datum_handle)
{
  char *datum;
  char *tag;
  char *bitmap_tag;
  int16_t frame_index;
  int16_t rval;
  unsigned int *seed;

  datum = (char *)datum_get(particle_data, datum_handle);
  tag = (char *)tag_get(0x70617274, *(int *)(datum + 0x04));
  bitmap_tag = (char *)tag_get(0x6269746d, *(int *)(tag + 0x10));

  frame_index = -1;
  *(int16_t *)(datum + 0x24) = frame_index;

  /* Phase 0 → 1: creation sequence */
  if (*(char *)(datum + 0x0e) == 0) {
    if (*(int16_t *)(tag + 0x9a) > 0) {
      seed = random_math_get_local_seed_address();
      rval = random_range(seed, 0, *(int16_t *)(tag + 0x9a));
      *(int16_t *)(datum + 0x24) = *(int16_t *)(tag + 0x98) + rval;
    }
    (*(char *)(datum + 0x0e))++;
  }

  /* Check if we need to skip to phase 2 */
  if (*(int16_t *)(datum + 0x24) == -1 && *(char *)(datum + 0x0e) == 1) {
    *(char *)(datum + 0x0e) = 2;
  } else if (*(char *)(datum + 0x0e) != 2) {
    goto skip_phase2;
  }

  /* Phase 2: attached/detached transition sequence */
  if (*(float *)(datum + 0x14) >= *(float *)(datum + 0x18) ||
      *(int16_t *)(tag + 0x9c) < 1) {
    (*(char *)(datum + 0x0e))++;
  } else {
    seed = random_math_get_local_seed_address();
    rval = random_range(seed, 0, *(int16_t *)(tag + 0x9c));
    *(int16_t *)(datum + 0x24) =
      *(int16_t *)(tag + 0x9a) + *(int16_t *)(tag + 0x98) + rval;
  }

skip_phase2:
  /* Phase 3 → 4: fading sequence */
  if (*(int16_t *)(datum + 0x24) == -1 && *(char *)(datum + 0x0e) == 3) {
    if (*(int16_t *)(tag + 0x9e) > 0) {
      seed = random_math_get_local_seed_address();
      rval = random_range(seed, 0, *(int16_t *)(tag + 0x9e));
      *(int16_t *)(datum + 0x24) = *(int16_t *)(tag + 0x9c) +
                                   *(int16_t *)(tag + 0x9a) +
                                   *(int16_t *)(tag + 0x98) + rval;
    }
    (*(char *)(datum + 0x0e))++;
  }

  /* Clamp frame_index and validate against bitmap sequence count */
  frame_index = *(int16_t *)(datum + 0x24);
  if (frame_index != -1 && *(int *)(bitmap_tag + 0x54) != 0) {
    if (frame_index < 0) {
      *(int16_t *)(datum + 0x24) = 0;
      return true;
    }
    {
      int max_index = *(int *)(bitmap_tag + 0x54) - 1;
      int idx = (int)frame_index;
      if (idx > max_index)
        idx = max_index;
      *(int16_t *)(datum + 0x24) = (int16_t)idx;
    }
    return true;
  }

  /* No valid frame — delete particle */
  FUN_000a18c0(datum_handle);
  return false;
}

/* Advance particle bitmap frame counter by one step (0xa1a90).
 * Selects forward or backward animation based on datum flag bit 0x1.
 * When the last/first frame is reached, calls FUN_000a1910 to pick the
 * next sequence. Returns true while the animation is still alive. */
bool FUN_000a1a90(int datum_handle)
{
  char *datum;
  char *part_tag;
  char *bitm_tag;
  char *seq_elem;
  int16_t frame_counter;
  int frame_count;
  bool result;

  datum = (char *)datum_get(particle_data, datum_handle);
  part_tag = (char *)tag_get(0x70617274, *(int *)(datum + 0x04));
  bitm_tag = (char *)tag_get(0x6269746d, *(int *)(part_tag + 0x10));
  *(uint32_t *)(datum + 0x1c) = 0;
  if ((*(uint8_t *)(datum + 0x2) & 0x1) != 0) {
    /* Backward: decrement toward zero */
    frame_counter = *(int16_t *)(datum + 0x26);
    if (frame_counter > 0) {
      *(int16_t *)(datum + 0x26) = frame_counter - 1;
      return 1;
    }
    result = FUN_000a1910(datum_handle);
    if (result) {
      seq_elem = (char *)tag_block_get_element(
        bitm_tag + 0x54, (int)(*(int16_t *)(datum + 0x24)), 0x40);
      *(int16_t *)(datum + 0x26) = *(int16_t *)(seq_elem + 0x34) - 1;
    }
    return result;
  }
  /* Forward: increment toward frame_count */
  seq_elem = (char *)tag_block_get_element(
    bitm_tag + 0x54, (int)(*(int16_t *)(datum + 0x24)), 0x40);
  frame_count = *(int32_t *)(seq_elem + 0x34);
  frame_counter = *(int16_t *)(datum + 0x26);
  if ((int)(frame_counter + 1) < frame_count) {
    *(int16_t *)(datum + 0x26) = frame_counter + 1;
    return 1;
  }
  result = FUN_000a1910(datum_handle);
  *(int16_t *)(datum + 0x26) = 0;
  return result;
}

/* Advance particle frame timer by delta_time (0xa1b60).
 * datum_handle via @edi. Handles three animation modes via tag flags:
 *   bit 1+2: early-exit (both must be set),
 *   bit 3: random-start (advance once if delta_time != 0),
 *   default: accumulate time and advance frames in a loop.
 * Returns true while the particle is still alive. */
bool FUN_000a1b60(int datum_handle, float delta_time)
{
  char *datum;
  uint32_t flags;
  float frame_remainder;
  bool result;

  datum = (char *)datum_get(particle_data, datum_handle);
  flags = *(uint32_t *)tag_get(0x70617274, *(int *)(datum + 0x04));
  result = 1;
  if ((flags & 0x2) != 0 && (*(uint8_t *)(datum + 0x2) & 0x2) != 0)
    return result;
  if (flags & 0x8) {
    if (delta_time != 0.0f)
      return (bool)FUN_000a1a90(datum_handle);
  } else {
    if (*(uint32_t *)(datum + 0x1c) == 0xbf800000u) {
      result = (bool)FUN_000a1a90(datum_handle);
      *(uint32_t *)(datum + 0x1c) = 0;
    }
    if (delta_time > 0.0f && result) {
      while (result) {
        frame_remainder = *(float *)(datum + 0x20) - *(float *)(datum + 0x1c);
        if (frame_remainder > delta_time) {
          *(float *)(datum + 0x1c) += delta_time;
          return result;
        }
        result = (bool)FUN_000a1a90(datum_handle);
        delta_time -= frame_remainder;
        if (delta_time <= 0.0f)
          return result;
      }
    }
  }
  return result;
}

/* Create a single particle from spawn parameters (0xa1fd0).
 * Validates
 * velocity, position, and color vectors. Resolves the spawn
 * position through
 * the parent object's node matrix (or first-person weapon
 * node matrix) when
 * attached to an object. Allocates a particle datum,
 * fills it with tag data
 * (lifetime, animation rate, sprite index), copies
 * position/velocity/color
 * from spawn_params, optionally applies physics
 * velocity, samples lighting,
 * and sets up the initial bitmap sequence. */
void particle_new(void *spawn_params)
{
  char *sp = (char *)spawn_params;
  float *velocity = (float *)(sp + 0x28);
  float *position = (float *)(sp + 0x10);
  float *color = (float *)(sp + 0x4c);
  float local_position[3];
  char location[8];
  int datum_handle;
  char *datum;
  uint32_t *tag;
  float light[3];
  float diffuse[3];

  /* assert velocity is valid */
  if (!real_vector3d_valid(velocity)) {
    csprintf((char *)0x5ab100, "%s: assert_valid_real_vector2d(%f, %f, %f)",
             "&data->velocity", (double)velocity[0], (double)velocity[1],
             (double)velocity[2]);
    display_assert((char *)0x5ab100, "c:\\halo\\SOURCE\\effects\\particles.c",
                   0x6d, 1);
    system_exit(-1);
  }

  /* assert position is valid */
  if (!valid_real_point3d(position)) {
    csprintf((char *)0x5ab100, "%s: assert_valid_real_point3d(%f, %f, %f)",
             "&data->position", (double)position[0], (double)position[1],
             (double)position[2]);
    display_assert((char *)0x5ab100, "c:\\halo\\SOURCE\\effects\\particles.c",
                   0x6e, 1);
    system_exit(-1);
  }

  /* assert color is valid */
  if (!valid_real_argb_color(color)) {
    csprintf((char *)0x5ab100,
             "%s: assert_valid_real_argb_color(%f, %f, %f, %f)", "&data->color",
             (double)color[0], (double)color[1], (double)color[2],
             (double)color[3]);
    display_assert((char *)0x5ab100, "c:\\halo\\SOURCE\\effects\\particles.c",
                   0x6f, 1);
    system_exit(-1);
  }

  if (*(int *)(sp + 0x00) == -1)
    return;

  tag = (uint32_t *)tag_get(0x70617274, *(int *)(sp + 0x00));

  if (*(int *)(sp + 0x04) == -1) {
    /* no parent object — use position directly */
    local_position[0] = position[0];
    local_position[1] = position[1];
    local_position[2] = position[2];
  } else if (*(uint8_t *)(sp + 0x0c) == 0) {
    /* attached to object — transform through object node matrix */
    float *matrix = (float *)object_get_node_matrix(*(int *)(sp + 0x04),
                                                    *(uint16_t *)(sp + 0x08));
    matrix_transform_point(matrix, position, local_position);
  } else {
    /* first-person weapon — transform through FP weapon node matrix */
    float *matrix = (float *)first_person_weapon_get_node_matrix(
      *(uint16_t *)(sp + 0x0a), *(uint16_t *)(sp + 0x08));
    matrix_transform_point(matrix, position, local_position);
  }

  scenario_location_from_point(location, local_position);

  if (*(int *)location == -1)
    return;

  if (!scenario_location_potentially_visible_local(location))
    return;

  datum_handle = data_new_at_index(particle_data);
  if (datum_handle == -1)
    return;

  datum = (char *)datum_get(particle_data, datum_handle);

  /* flags word */
  *(uint16_t *)(datum + 0x02) = 0;
  if (*tag & 0x1) {
    uint16_t rval = FUN_0008e7c0();
    *(uint16_t *)(datum + 0x02) |= rval & 1;
  }
  if (*tag & 0x400) {
    uint16_t rval = FUN_0008e7c0();
    *(uint16_t *)(datum + 0x02) |= rval & 4;
  }
  if (*tag & 0x800) {
    uint16_t rval = FUN_0008e7c0();
    *(uint16_t *)(datum + 0x02) |= rval & 8;
  }

  if (*(uint8_t *)(sp + 0x0d) != 0)
    *(uint8_t *)(datum + 0x02) |= 0x10;
  else
    *(uint8_t *)(datum + 0x02) &= ~0x10;

  if (*(uint8_t *)(sp + 0x0e) != 0)
    *(uint8_t *)(datum + 0x02) |= 0x20;
  else
    *(uint8_t *)(datum + 0x02) &= ~0x20;

  if (*(uint8_t *)(sp + 0x0c) != 0)
    *(uint8_t *)(datum + 0x02) |= 0x40;
  else
    *(uint8_t *)(datum + 0x02) &= ~0x40;

  /* tag index, marker, object handle, node, animation phase */
  *(int *)(datum + 0x04) = *(int *)(sp + 0x00);
  *(uint8_t *)(datum + 0x0f) = *(uint8_t *)(sp + 0x0a);
  *(int *)(datum + 0x08) = *(int *)(sp + 0x04);
  *(int16_t *)(datum + 0x0c) = (int16_t) * (uint16_t *)(sp + 0x08);
  *(uint8_t *)(datum + 0x0e) = 0;
  *(int *)(datum + 0x10) = render;

  /* lifetime: random range from tag fields [0x38..0x3c] */
  {
    float lifetime;
    unsigned int *seed = random_math_get_local_seed_address();
    lifetime = random_real_range((int *)seed, *(float *)((char *)tag + 0x38),
                                 *(float *)((char *)tag + 0x3c));
    *(float *)(datum + 0x18) = lifetime;
    if (lifetime > 0.7f) {
      float excess = lifetime - 0.7f;
      int16_t player_count = local_player_count();
      *(float *)(datum + 0x18) = excess / (float)(int)player_count + 0.7f;
    }
  }

  /* animation rate */
  if (*(float *)((char *)tag + 0x84) == 0.0f) {
    *(float *)(datum + 0x20) = *(float *)0x2548fc; /* FLT_MAX */
  } else {
    float anim_rate = FUN_000849f0(*(float *)((char *)tag + 0x80),
                                   *(float *)((char *)tag + 0x84));
    *(float *)(datum + 0x20) = 1.0f / anim_rate;
  }

  *(float *)(datum + 0x1c) = -1.0f;

  /* scenario location */
  *(int *)(datum + 0x28) = *(int *)(location + 0);
  *(int *)(datum + 0x2c) = *(int *)(location + 4);

  /* position (from spawn_params) */
  *(float *)(datum + 0x30) = *(float *)(sp + 0x10);
  *(float *)(datum + 0x34) = *(float *)(sp + 0x14);
  *(float *)(datum + 0x38) = *(float *)(sp + 0x18);

  /* direction (from spawn_params) */
  *(float *)(datum + 0x3c) = *(float *)(sp + 0x1c);
  *(float *)(datum + 0x40) = *(float *)(sp + 0x20);
  *(float *)(datum + 0x44) = *(float *)(sp + 0x24);

  /* rotation */
  *(float *)(datum + 0x54) = *(float *)(sp + 0x40);

  /* size */
  *(float *)(datum + 0x5c) = *(float *)(sp + 0x48);

  /* color (argb from spawn_params) */
  *(float *)(datum + 0x60) = *(float *)(sp + 0x4c);
  *(float *)(datum + 0x64) = *(float *)(sp + 0x50);
  *(float *)(datum + 0x68) = *(float *)(sp + 0x54);
  *(float *)(datum + 0x6c) = *(float *)(sp + 0x58);

  /* velocity (from spawn_params) */
  *(float *)(datum + 0x48) = *(float *)(sp + 0x28);
  *(float *)(datum + 0x4c) = *(float *)(sp + 0x2c);
  *(float *)(datum + 0x50) = *(float *)(sp + 0x30);

  /* apply physics velocity if no parent object */
  if (*(int *)(datum + 0x08) == -1) {
    float phys_scale = particle_get_radius(datum_handle);
    float *phys_tag =
      (float *)tag_get(0x70706879, *(int *)((char *)tag + 0x20));
    float phys_vel =
      point_physics_definition_get_mass((int)phys_tag, phys_scale);
    *(float *)(datum + 0x48) += phys_vel * *(float *)(sp + 0x34);
    *(float *)(datum + 0x4c) += phys_vel * *(float *)(sp + 0x38);
    *(float *)(datum + 0x50) += phys_vel * *(float *)(sp + 0x3c);
  }

  /* scale */
  *(float *)(datum + 0x58) = *(float *)(sp + 0x44);

  /* sample lighting and apply to color channels */
  if ((*tag & 0x200) == 0 || (*tag & 0x40) != 0) {
    FUN_00139480(local_position, light, diffuse, 0);

    if (!valid_real_rgb_color(light)) {
      csprintf((char *)0x5ab100, "%s: assert_valid_real_rgb_color(%f, %f, %f)",
               "&light", (double)light[0], (double)light[1], (double)light[2]);
      display_assert((char *)0x5ab100, "c:\\halo\\SOURCE\\effects\\particles.c",
                     0xcf, 1);
      system_exit(-1);
    }

    if (!valid_real_rgb_color(diffuse)) {
      csprintf((char *)0x5ab100, "%s: assert_valid_real_rgb_color(%f, %f, %f)",
               "&diffuse", (double)diffuse[0], (double)diffuse[1],
               (double)diffuse[2]);
      display_assert((char *)0x5ab100, "c:\\halo\\SOURCE\\effects\\particles.c",
                     0xd0, 1);
      system_exit(-1);
    }

    if ((*tag & 0x200) == 0) {
      *(float *)(datum + 0x64) *= light[0];
      *(float *)(datum + 0x68) *= light[1];
      *(float *)(datum + 0x6c) *= light[2];
    }

    if ((*tag & 0x40) != 0) {
      *(float *)(datum + 0x64) *= diffuse[0];
      *(float *)(datum + 0x68) *= diffuse[1];
      *(float *)(datum + 0x6c) *= diffuse[2];
    }
  }

  /* bitmap sprite setup */
  if (FUN_000a1910(datum_handle)) {
    char *bitmap_tag =
      (char *)tag_get(0x6269746d, *(int *)((char *)tag + 0x10));
    char *seq_element = (char *)tag_block_get_element(
      bitmap_tag + 0x54, (int)*(int16_t *)(datum + 0x24), 0x40);

    if (*tag & 0x4) {
      /* randomized animated sprite */
      int16_t frame_count =
        local_random_range(0, *(uint16_t *)(seq_element + 0x34));
      int16_t direction = (*(uint8_t *)(datum + 0x02) & 1) ? 1 : -1;
      *(int16_t *)(datum + 0x26) = frame_count + direction;
      return;
    }

    if (*(uint8_t *)(datum + 0x02) & 1) {
      *(int16_t *)(datum + 0x26) = *(int16_t *)(seq_element + 0x34);
      return;
    }

    *(int16_t *)(datum + 0x26) = -1;
  }
}

void particles_update(float delta_time)
{
  int datum_handle;
  char *datum;
  char *tag;
  bool just_created;
  float new_lifetime;

  if (profile_global_enable && *(char *)0x2ef1e8)
    profile_enter_private((void *)0x2ef1e0);

  for (datum_handle = data_next_index(particle_data, -1); datum_handle != -1;
       datum_handle = data_next_index(particle_data, datum_handle)) {
    datum = (char *)datum_get(particle_data, datum_handle);
    tag = (char *)tag_get(0x70617274, *(int *)(datum + 4));
    just_created = *(float *)(datum + 0x14) == *(float *)0x2533c0;
    if (render - *(int *)(datum + 0x10) < 0x10) {
      new_lifetime = delta_time + *(float *)(datum + 0x14);
      *(float *)(datum + 0x14) = new_lifetime;
      if (new_lifetime < *(float *)(datum + 0x18) || just_created ||
          *(int16_t *)(tag + 0x9e) != 0) {
        if (FUN_000a1b60(datum_handle, delta_time))
          FUN_000a1c30(datum_handle, delta_time);
      } else {
        FUN_000a18c0(datum_handle);
      }
    } else {
      datum_delete(particle_data, datum_handle);
    }
  }

  if (profile_global_enable && *(char *)0x2ef1e8)
    profile_exit_private((void *)0x2ef1e0);
}
