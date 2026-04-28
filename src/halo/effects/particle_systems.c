void particle_systems_initialize(void)
{
  particle_system_header_data =
    game_state_data_new("particle systems", 0x40, 0x158);
  particle_system_data =
    game_state_data_new("particle system particles", 0x200, 0x80);
}

void particle_systems_initialize_for_new_map(void)
{
  data_delete_all(particle_system_header_data);
  data_delete_all(particle_system_data);
}

void particle_system_delete(int particle_system_handle)
{
  char *entry, *tag, *particle_entry;
  int particle_index, next_particle;
  int i, instance_count;

  entry =
    (char *)datum_get(particle_system_header_data, particle_system_handle);
  tag = (char *)tag_get(0x7063746c, *(int *)(entry + 8));
  instance_count = *(int *)(tag + 0x5c);
  for (i = 0; i < instance_count; i++) {
    particle_index = *(int *)(entry + 0x94 + i * 0x40);
    while (particle_index != NONE) {
      particle_entry = (char *)datum_get(particle_system_data, particle_index);
      next_particle = *(int *)(particle_entry + 4);
      datum_delete(particle_system_data, particle_index);
      particle_index = next_particle;
    }
  }
  datum_delete(particle_system_header_data, particle_system_handle);
}

void particle_systems_dispose(void)
{
}

void particle_systems_dispose_from_old_map(void)
{
  int particle_system_index;

  if (particle_system_header_data && particle_system_header_data->valid) {
    for (particle_system_index =
           data_next_index(particle_system_header_data, NONE);
         particle_system_index != NONE;
         particle_system_index = data_next_index(particle_system_header_data,
                                                 particle_system_index)) {
      particle_system_delete(particle_system_index);
    }
    data_make_invalid(particle_system_header_data);
    data_make_invalid(particle_system_data);
  }
}

/* Initialize particle system type instances from the pctl tag (0xa0fd0).
 * For each particle type in the tag definition:
 *   - If the type has no particle states, returns false (setup failed).
 *   - Otherwise, initializes the instance state: sets the current state
 *     index to 0, next state to NONE, marks as initialized, clears
 *     particle count and first-particle handle, and picks a random
 *     duration from the first particle state's bounds.
 * Resolves the BSP location from the system's position, sets the
 * "location resolved" flag, then runs an initial 0.001s update tick
 * via FUN_000a0180 if all types were valid. */
char FUN_000a0fd0(int particle_handle)
{
  char *entry;
  char *tag;
  int *tag_block_ptr;
  char *type_def;
  char *state_elem;
  char *instance;
  short i;
  int idx;
  char result;
  float duration;

  entry = (char *)datum_get(particle_system_header_data, particle_handle);
  tag = (char *)tag_get(0x7063746c, *(int *)(entry + 8));
  result = 1;
  scenario_location_from_point(entry + 0x18, entry + 0x20);
  tag_block_ptr = (int *)(tag + 0x5c);
  *(unsigned int *)(entry + 4) |= 2;
  idx = 0;
  i = 0;
  if (*tag_block_ptr < 1) {
    result = 1;
  } else {
    do {
      instance = entry + 0x58 + idx * 0x40;
      type_def = (char *)tag_block_get_element(tag_block_ptr, idx, 0x80);
      if (*(int *)(type_def + 0x68) == 0) {
        result = 0;
      } else {
        *(short *)(instance + 0x00) = 0;
        *(short *)(instance + 0x02) = (short)NONE;
        *(char *)(instance + 0x38) = 1;
        *(short *)(instance + 0x3a) = 0;
        *(int *)(instance + 0x3c) = NONE;
        if (*(int *)(type_def + 0x68) > 0) {
          state_elem =
            (char *)tag_block_get_element((int *)(type_def + 0x68), 0, 0xc0);
          duration = random_real_range(
            (int *)random_math_get_local_seed_address(),
            *(float *)(state_elem + 0x20), *(float *)(state_elem + 0x24));
          *(float *)(instance + 0x04) = duration;
          *(float *)(instance + 0x08) = duration;
        }
      }
      i = i + 1;
      idx = (int)i;
    } while (idx < *tag_block_ptr);
    if (result == 0) {
      return 0;
    }
  }
  FUN_000a0180(0.001f, particle_handle);
  return result;
}

void particle_systems_update(float dt)
{
  int particle_system_index;

  assert_halt(particle_system_header_data &&
              particle_system_header_data->valid);
  for (particle_system_index =
         data_next_index(particle_system_header_data, NONE);
       particle_system_index != NONE;
       particle_system_index =
         data_next_index(particle_system_header_data, particle_system_index)) {
    FUN_000a0180(dt, particle_system_index);
  }
}
