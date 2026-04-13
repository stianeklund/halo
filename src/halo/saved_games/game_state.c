void game_state_dispose(void)
{
  xbox_game_state_dispose_buffer();
  xbox_game_state_close_file();
}

void game_state_dispose_from_old_map(void)
{
}

void *game_state_malloc(const char *name, const char *group_name, int size)
{
  void *result;

  assert_halt(!(size & 3));
  assert_halt(!game_state_globals.locked);
  assert_halt(game_state_globals.cpu_allocation_size + size <=
              GAME_STATE_CPU_SIZE);

  result =
    game_state_globals.base_address + game_state_globals.cpu_allocation_size;
  game_state_globals.cpu_allocation_size += size;
  crc_checksum_buffer(&game_state_globals.checksum, &size, 4);
  return result;
}

data_t *game_state_data_new(char *name, __int16 maximum_count, __int16 size)
{
  data_t *data; // esi
  int allocation_size; // [esp-Ch] [ebp-18h]

  allocation_size = data_allocation_size(maximum_count, size);
  data = (data_t *)game_state_malloc(name, "data array", allocation_size);
  data_initialize(data, name, maximum_count, size);
  return data;
}
