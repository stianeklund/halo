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
    ((void (*)(float, int))0xa0180)(dt, particle_system_index);
  }
}
