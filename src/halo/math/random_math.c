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

/* Returns the address of the thread-local random seed.
 * Used by callers that need a local (non-global) RNG state
 * to avoid contention with the global seed. */
unsigned int *random_math_get_local_seed_address(void)
{
  return (unsigned int *)0x46e3f8;
}

/* Empty in retail; presumably logged seed state to debug output. */
void random_seed_debug_log(bool a1)
{
}

/* Generate a random int16 in [min, max) using a linear congruential
 * generator.  Advances *seed with the classic Numerical Recipes
 * LCG (a=0x19660d, c=0x3c6ef35f), then maps the upper 16 bits of
 * the new seed into the requested range. */
int16_t random_range(unsigned int *seed, int16_t min, int16_t max)
{
  unsigned int s;

  s = *seed * 0x19660d + 0x3c6ef35f;
  *seed = s;
  return (int16_t)(((int)(max - min) * (int)(s >> 16) >> 16) + (int)min);
}
