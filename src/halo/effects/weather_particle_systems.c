void weather_particle_systems_initialize(void)
{
  weather_particle_system_data = data_new("weather particles", 0x200, 0x54);
  if (weather_particle_system_data == 0)
    error(0, "couldn't allocate weather particle system globals.");
}

void weather_particle_systems_initialize_for_new_map(void)
{
  int16_t i;
  int *entry;

  i = 0;
  entry = (int *)0x4557f4;
  do {
    assert_halt(i >= 0 && i < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
    *entry = NONE;
    i++;
    entry = (int *)((char *)entry + 0x9c);
  } while (i < 4);
  *(int16_t *)0x4557f0 = 0;
  data_delete_all(weather_particle_system_data);
}

void weather_particle_systems_dispose_from_old_map(void)
{
  if (weather_particle_system_data && weather_particle_system_data->valid)
    data_make_invalid(weather_particle_system_data);
}

void weather_particle_systems_dispose(void)
{
  if (weather_particle_system_data) {
    data_dispose(weather_particle_system_data);
    weather_particle_system_data = 0;
  }
}
