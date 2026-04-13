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
