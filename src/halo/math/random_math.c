void lock_global_random_seed(void)
{
  *(int *)0x46e3f0 = *(int *)0x46e3f0 + 1;
}

void unlock_global_random_seed(void)
{
  assert_halt(*(int *)0x46e3f0 > 0);
  *(int *)0x46e3f0 = *(int *)0x46e3f0 - 1;
}

int *get_global_random_seed_address(void)
{
  if (game_engine_running() && *(int *)0x46e3f0 != 0) {
    display_assert(
      "you should not be using global random(); use local random() instead",
      "c:\\halo\\SOURCE\\math\\random_math.c", 0x38, 1);
    system_exit(-1);
  }
  return (int *)0x46e3f4;
}

void random_seed_debug_log(bool a1)
{
}
